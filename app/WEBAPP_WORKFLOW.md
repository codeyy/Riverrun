# Riverrun FastAPI Web Client Workflow

This document explains how the current FastAPI web app works with the C++ chat server.

## Big Picture

The browser cannot directly talk to your C++ TCP socket server, so the FastAPI app acts as a bridge.

```text
Browser UI
   |
   | WebSocket messages
   v
FastAPI app on 127.0.0.1:5000
   |
   | TCP socket messages
   v
C++ chat server on 127.0.0.1:8080
```

The browser only talks to FastAPI. FastAPI opens and manages a real TCP socket connection to your C++ server.

## Important Files

```text
app.py
```

The FastAPI backend. It handles the browser WebSocket, connects to the C++ server with an async TCP socket, sends protocol messages, receives incoming TCP messages, and forwards them to the browser.

```text
templates/index.html
```

The main web page structure. It contains the connection form, message list, recipient field, message input, and buttons.

```text
static/app.js
```

The browser-side JavaScript. It opens a WebSocket to FastAPI, updates the UI, sends chat actions, and displays sent/received messages.

```text
static/styles.css
```

The UI styling for the chat layout.

```text
requirements.txt
```

Python dependency list. Currently it contains FastAPI and Uvicorn.

## C++ Server Protocol

The web app now follows the same protocol used by your C++ `client.h`.

Every message sent over the TCP socket ends with:

```text
<MESSAGE_DELIMITER>
```

The delimiter is important because TCP is a stream. One `recv()` call might contain half a message, one full message, or multiple messages together. The delimiter tells the client where each complete message ends.

## Connecting

When the user opens:

```text
http://127.0.0.1:5000
```

FastAPI serves `templates/index.html`.

The user enters:

```text
Name
Server host
Server port
```

By default:

```text
Server host: 127.0.0.1
Server port: 8080
```

When the user presses Connect, `static/app.js` opens this WebSocket:

```text
ws://127.0.0.1:5000/ws
```

Then the browser sends this first WebSocket message:

```json
{
  "username": "Alice",
  "host": "127.0.0.1",
  "port": "8080"
}
```

FastAPI then creates a `RiverrunClient` object in `app.py`.

That object opens a TCP socket to:

```text
127.0.0.1:8080
```

Then FastAPI logs in to the C++ server by sending:

```text
LOGIN:Alice<MESSAGE_DELIMITER>
```

The C++ server should reply with something like:

```text
Welcome Alice!<MESSAGE_DELIMITER>
```

FastAPI waits for that welcome message before telling the browser that connection succeeded.

## Receiving Messages

After the TCP socket connects and login succeeds, FastAPI starts an async task to read from the C++ server.

That task continuously reads complete delimiter-framed messages from the TCP socket.

```python
When a full message is found, FastAPI removes the delimiter and immediately sends JSON to the browser WebSocket:

```json
{
  "connected": true,
  "messages": [
    {
      "type": "message",
      "text": "[Bob] hello",
      "time": "10:35"
    }
  ]
}
```

Then JavaScript adds the message to the chat window.

## Sending Public Messages

When the user sends a normal message with no DM recipient, the browser sends this WebSocket JSON:

```text
{"type":"send","text":"hello everyone","recipient":""}
```

Example JSON body:

```json
{
  "text": "hello everyone",
  "recipient": ""
}
```

FastAPI converts that into the C++ server protocol:

```text
BROADCAST:hello everyone<MESSAGE_DELIMITER>
```

Your C++ server broadcasts that to every other connected user.

Important detail: your current C++ server does not echo broadcast messages back to the sender. Because of that, FastAPI sends a local `sent` event back to the browser after the TCP send succeeds.

## Sending Direct Messages

The web UI has a `DM user (optional)` field.

If the user types a recipient name there, the message becomes a direct message.

Example browser JSON:

```json
{
  "text": "secret hello",
  "recipient": "Bob"
}
```

FastAPI sends this TCP frame:

```text
DM:Bob:secret hello<MESSAGE_DELIMITER>
```

The C++ server looks up `Bob` and sends the message only to that user.

If the user does not exist, the C++ server sends an error message back, such as:

```text
User Bob not found<MESSAGE_DELIMITER>
```

The FastAPI read task receives that error and the browser displays it.

## Server Status Ping

Your C++ `client.h` has logic for a status check:

```text
STATUS?
```

If FastAPI receives this from the server:

```text
STATUS?<MESSAGE_DELIMITER>
```

FastAPI automatically replies:

```text
ONLINE<MESSAGE_DELIMITER>
```

This keeps compatibility with your C++ protocol.

## Disconnecting

When the user presses Disconnect, the browser sends a WebSocket disconnect message:

```text
{"type":"disconnect"}
```

FastAPI closes the TCP socket and marks the client as disconnected.

If the C++ server closes the socket first, FastAPI detects it in the read task and the browser WebSocket closes.

## Current API Routes

```text
GET /
```

Serves the web page.

```text
WebSocket /ws
```

Handles connect, send, receive, and disconnect messages for one browser client.

## Current Runtime Ports

```text
FastAPI web app: 127.0.0.1:5000
C++ chat server: 127.0.0.1:8080
```

The C++ server must be running before the web client can connect.

## Typical Full Workflow

1. Start the C++ server:

```text
C:\custom_programs\Riverrun\server.exe
```

2. Start the FastAPI app:

```powershell
cd C:\custom_programs\RiverrunWebClient
python -m uvicorn app:app --host 127.0.0.1 --port 5000
```

3. Open the browser:

```text
http://127.0.0.1:5000
```

4. Enter a username and press Connect.

5. FastAPI sends:

```text
LOGIN:username<MESSAGE_DELIMITER>
```

6. The C++ server replies:

```text
Welcome username!<MESSAGE_DELIMITER>
```

7. Public messages are sent as:

```text
BROADCAST:message<MESSAGE_DELIMITER>
```

8. Direct messages are sent as:

```text
DM:recipient:message<MESSAGE_DELIMITER>
```

9. Incoming messages are received by FastAPI and immediately forwarded to the browser WebSocket.

## Notes And Limitations

- The current browser uses a WebSocket, so it no longer polls `/api/messages`.
- Each browser tab/window WebSocket owns one independent TCP connection to the C++ server.
- The FastAPI app is currently meant for local development, not production hosting.
- The C++ server currently runs on port `8080`, and the FastAPI app defaults to that port.
