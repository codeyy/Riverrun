# FastAPI Web Client for C++ Chat Server

This is a browser client for the Riverrun C++ chat server running on `localhost:8080`.

The browser talks to FastAPI on port `5000` using a WebSocket. FastAPI opens a TCP socket to the C++ server and forwards messages both ways.

## Run

```powershell
python -m pip install -r requirements.txt
python -m uvicorn app:app --host 127.0.0.1 --port 5000
```

Then open:

```text
http://127.0.0.1:5000
```

Start your C++ chat server first on `localhost:8080`.

## Protocol

- The FastAPI bridge uses the same protocol as `client.h`:
  - `LOGIN:username<MESSAGE_DELIMITER>`
  - `BROADCAST:message<MESSAGE_DELIMITER>`
  - `DM:recipient:message<MESSAGE_DELIMITER>`
  - `ONLINE<MESSAGE_DELIMITER>` when the server sends `STATUS?`

- You can change the default target with environment variables:

```powershell
$env:CHAT_SERVER_HOST="127.0.0.1"
$env:CHAT_SERVER_PORT="8080"
python app.py
```
