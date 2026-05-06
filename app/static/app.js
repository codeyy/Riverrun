const connectForm = document.querySelector("#connect-form");
const messageForm = document.querySelector("#message-form");
const disconnectButton = document.querySelector("#disconnect-button");
const connectButton = document.querySelector("#connect-button");
const messageInput = document.querySelector("#message-input");
const sendButton = document.querySelector("#send-button");
const messagesEl = document.querySelector("#messages");
const statusDot = document.querySelector("#status-dot");
const statusText = document.querySelector("#status-text");
const roomTitle = document.querySelector("#room-title");
const recipientInput = document.querySelector("#recipient-input");

let socket = null;
let connected = false;

function setStatus(text, isOnline = connected) {
  statusDot.classList.toggle("online", isOnline);
  statusText.textContent = text;
}

function setConnected(isConnected, label = "") {
  connected = isConnected;
  setStatus(isConnected ? "Connected" : "Disconnected", isConnected);
  connectButton.disabled = isConnected;
  disconnectButton.disabled = !isConnected;
  messageInput.disabled = !isConnected;
  recipientInput.disabled = !isConnected;
  sendButton.disabled = !isConnected;
  roomTitle.textContent = isConnected ? label : "Waiting for connection";

  if (isConnected) {
    messageInput.focus();
  }
}

function showEmptyState() {
  messagesEl.innerHTML = '<div class="empty-state">Connect to the server to start chatting.</div>';
}

function appendMessage(message) {
  const empty = messagesEl.querySelector(".empty-state");
  if (empty) {
    empty.remove();
  }

  const bubble = document.createElement("div");
  bubble.className = "bubble";
  if (message.type === "system" || message.type === "error") {
    bubble.classList.add("system");
  }
  if (message.type === "sent") {
    bubble.classList.add("own");
  }

  const time = document.createElement("span");
  time.className = "bubble-time";
  time.textContent = message.time || "";

  const text = document.createElement("span");
  text.textContent = message.text;

  bubble.append(time, text);
  messagesEl.appendChild(bubble);
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

function sendSocket(payload) {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    appendMessage({ type: "error", text: "Web client is not connected.", time: "" });
    return;
  }
  socket.send(JSON.stringify(payload));
}

function connectWebSocket({ username, bridgeUrl }) {
  const scheme = window.location.protocol === "https:" ? "wss" : "ws";
  socket = new WebSocket(`${scheme}://${window.location.host}/ws`);

  socket.addEventListener("open", () => {
    sendSocket({ type: "connect", username, bridgeUrl });
  });

  socket.addEventListener("message", (event) => {
    const message = JSON.parse(event.data);

    if (message.type === "connected") {
      setConnected(true, `${username}`);
      appendMessage({ type: "system", text: message.text || "Ready to chat", time: message.time });
      return;
    }

    if (message.type === "error") {
      appendMessage(message);
      setStatus("Connection failed", false);
      return;
    }

    appendMessage(message);
  });

  socket.addEventListener("close", () => {
    setConnected(false);
    connectButton.textContent = "Connect";
    connectButton.disabled = false;
    if (connected) {
      appendMessage({ type: "system", text: "WebSocket closed.", time: "" });
    }
  });

  socket.addEventListener("error", () => {
    setStatus("Connection failed", false);
    appendMessage({ type: "error", text: "Could not open the FastAPI WebSocket.", time: "" });
  });
}

document.getElementById("connect-button").addEventListener("click", () => {
  usernameInput = document.getElementById("username");
  bridgeUrlInput = document.getElementById("bridge-url");
  alcolour = "rgba(255, 0, 0, 0.76)";

  if (usernameInput.value.trim() === "") {
    usernameInput.style.borderColor = alcolour;
  }
  else if (bridgeUrlInput.value.trim() === "") {
    bridgeUrlInput.style.borderColor = alcolour;
  }
  else {
    usernameInput.style.borderColor = "";
    bridgeUrlInput.style.borderColor = "";
    document.getElementById("connect-button").type = "submit";
  }
});

connectForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const form = new FormData(connectForm);

  const username = form.get("username").trim();
  const bridgeUrl = form.get("bridgeUrl").trim();

  if (socket) {
    socket.close();
  }
  bridgeUrl == "" ? bridgeUrl == "localhost:8000" : bridgeUrl;
  connectButton.disabled = true;
  connectButton.textContent = "Connecting...";
  setStatus("Connecting...", false);
  connectWebSocket({ username, bridgeUrl });
});

messageForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const text = messageInput.value.trim();
  const recipient = recipientInput.value.trim();
  if (!text) {
    return;
  }

  messageInput.value = "";
  sendSocket({ type: "send", text, recipient });
});

disconnectButton.addEventListener("click", () => {
  sendSocket({ type: "disconnect" });
  if (socket) {
    socket.close();
  }
  setConnected(false);
});

showEmptyState();
setConnected(false);