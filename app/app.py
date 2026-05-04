import asyncio
import os
import time
from dataclasses import dataclass
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from starlette.requests import Request


DEFAULT_CHAT_HOST = os.environ.get("CHAT_SERVER_HOST", "127.0.0.1")
DEFAULT_CHAT_PORT = int(os.environ.get("CHAT_SERVER_PORT", "8080"))
DELIM = "<MESSAGE_DELIMITER>"

app = FastAPI(title="Riverrun Web Client")
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")


def now() -> str:
    return time.strftime("%H:%M")


@dataclass
class RiverrunClient:
    username: str
    host: str
    port: int
    reader: Optional[asyncio.StreamReader] = None
    writer: Optional[asyncio.StreamWriter] = None
    connected: bool = False

    async def connect_and_login(self) -> str:
        self.reader, self.writer = await asyncio.wait_for(
            asyncio.open_connection(self.host, self.port),
            timeout=5,
        )
        self.connected = True

        # The C++ server expects this to be the first frame after TCP connect.
        await self.send_frame(f"LOGIN:{self.username}")
        reply = await self.read_frame(timeout=5)
        if not reply.startswith("Welcome"):
            await self.close()
            raise RuntimeError(reply or "The server did not accept the login.")

        return reply

    def send_frame_nowait(self, payload: str) -> None:
        if not self.connected or self.writer is None:
            raise RuntimeError("Not connected to the C++ server.")

        self.writer.write(f"{payload}{DELIM}".encode("utf-8"))

    async def send_frame(self, payload: str) -> None:
        self.send_frame_nowait(payload)
        await self.writer.drain()

    async def send_chat(self, text: str, recipient: str = "") -> None:
        clean_text = text.strip()
        if not clean_text:
            return

        clean_recipient = recipient.strip()
        if clean_recipient:
            await self.send_frame(f"DM:{clean_recipient}:{clean_text}")
        else:
            await self.send_frame(f"BROADCAST:{clean_text}")

    async def read_frame(self, timeout: Optional[float] = None) -> str:
        if self.reader is None:
            raise RuntimeError("Not connected to the C++ server.")

        data = await asyncio.wait_for(
            self.reader.readuntil(DELIM.encode("utf-8")),
            timeout=timeout,
        )
        return data.decode("utf-8", errors="replace").removesuffix(DELIM).strip()

    async def close(self) -> None:
        self.connected = False
        if self.writer is not None:
            self.writer.close()
            try:
                await self.writer.wait_closed()
            except OSError:
                pass
        self.reader = None
        self.writer = None


async def ws_send(websocket: WebSocket, message_type: str, text: str, **extra) -> None:
    await websocket.send_json(
        {
            "type": message_type,
            "text": text,
            "time": now(),
            **extra,
        }
    )


@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse(
        request,
        "index.html",
        {
            "default_host": DEFAULT_CHAT_HOST,
            "default_port": DEFAULT_CHAT_PORT,
        },
    )


@app.websocket("/ws")
async def websocket_chat(websocket: WebSocket):
    await websocket.accept()
    chat_client: Optional[RiverrunClient] = None
    receive_task: Optional[asyncio.Task] = None

    async def forward_from_cpp() -> None:
        assert chat_client is not None
        while chat_client.connected:
            frame = await chat_client.read_frame()
            if frame == "STATUS?":
                await chat_client.send_frame("ONLINE")
                continue
            if frame:
                await ws_send(websocket, "message", frame)

    try:
        first = await websocket.receive_json()
        if first.get("type") != "connect":
            await ws_send(websocket, "error", "First WebSocket message must be a connect request.")
            return

        username = (first.get("username") or "Guest").strip()[:32]
        host = (first.get("host") or DEFAULT_CHAT_HOST).strip()
        port = int(first.get("port") or DEFAULT_CHAT_PORT)

        chat_client = RiverrunClient(username=username, host=host, port=port)
        await ws_send(websocket, "system", f"Connecting to {host}:{port}...")

        welcome = await chat_client.connect_and_login()
        await ws_send(websocket, "connected", welcome, username=username)

        receive_task = asyncio.create_task(forward_from_cpp())

        while True:
            event = await websocket.receive_json()
            event_type = event.get("type")

            if event_type == "send":
                text = event.get("text", "")
                recipient = event.get("recipient", "")
                await chat_client.send_chat(text, recipient)
                await ws_send(
                    websocket,
                    "sent",
                    f"[DM to {recipient.strip()}] {text.strip()}" if recipient.strip() else text.strip(),
                )
            elif event_type == "disconnect":
                await ws_send(websocket, "system", "Disconnected")
                break

    except WebSocketDisconnect:
        pass
    except Exception as exc:
        try:
            await ws_send(websocket, "error", str(exc))
        except RuntimeError:
            pass
    finally:
        if receive_task is not None:
            receive_task.cancel()
        if chat_client is not None:
            await chat_client.close()
