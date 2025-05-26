#include "web_manager.h"
#include "display_manager.h" 
#include "lora_manager.h"    
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h" 

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//STATIC VARIABLES 
static String currentMyDeviceId_web;
static String currentLoraPacketPrefix_web;
static String currentBoardName_web;


// HTML WEB PAGE 
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LoRa Messenger Node</title> <!-- Generic Title -->
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 0; background-color: #f0f2f5; display: flex; flex-direction: column; height: 100vh; color: #333; }
        .chat-container { display: flex; flex-direction: column; flex-grow: 1; max-width: 800px; width: 100%; margin: 0 auto; background-color: #fff; box-shadow: 0 0 15px rgba(0,0,0,0.1); overflow: hidden; border-radius: 8px; }
        header { background-color: #007bff; color: white; padding: 12px 20px; text-align: center; font-size: 1.3em; box-shadow: 0 2px 5px rgba(0,0,0,0.1); z-index: 10; display: flex; justify-content: space-between; align-items: center; }
        header .title { flex-grow: 1; text-align: center; }
        #connectionStatus { width: 12px; height: 12px; border-radius: 50%; display: inline-block; margin-left: 10px; transition: background-color 0.3s ease; }
        .status-connected { background-color: #28a745; } .status-disconnected { background-color: #dc3545; } .status-connecting { background-color: #ffc107; }
        #chatbox { flex-grow: 1; overflow-y: auto; padding: 20px; background-color: #e9ecef; display: flex; flex-direction: column; }
        .message { padding: 10px 15px; margin-bottom: 12px; border-radius: 18px; line-height: 1.5; max-width: 75%; word-wrap: break-word; position: relative; display: flex; flex-direction: column; }
        .message .sender-info { font-size: 0.8em; font-weight: bold; margin-bottom: 4px; color: #495057; }
        .message .timestamp { font-size: 0.7em; color: #888; margin-top: 5px; text-align: right; }
        .sent { background-color: #007bff; color: white; align-self: flex-end; border-bottom-right-radius: 5px; }
        .sent .sender-info { color: #e0e0e0; } .sent .timestamp { color: #e0e0e0; }
        .received { background-color: #f8f9fa; color: #333; align-self: flex-start; border: 1px solid #dee2e6; border-bottom-left-radius: 5px; }
        .received .sender-info { color: #007bff; } .received .timestamp { color: #6c757d; }
        .system-message { font-style: italic; color: #6c757d; text-align: center; font-size: 0.9em; margin: 10px 0; padding: 5px; width: 100%; align-self: center; background-color: #f8f9fa; border-radius: 4px; }
        #controls { display: flex; padding: 15px; background-color: #fff; border-top: 1px solid #dee2e6; }
        #messageInput { flex-grow: 1; padding: 12px 15px; border: 1px solid #ced4da; border-radius: 20px; margin-right: 10px; font-size: 1em; outline: none; }
        #messageInput:focus { border-color: #007bff; box-shadow: 0 0 0 0.2rem rgba(0,123,255,.25); }
        #sendButton, #clearChatButton { padding: 12px 20px; color: white; border: none; cursor: pointer; border-radius: 20px; font-size: 1em; transition: background-color 0.2s ease; }
        #sendButton { background-color: #007bff; } #sendButton:hover { background-color: #0056b3; }
        #clearChatButton { margin-left: 10px; background-color: #6c757d; } #clearChatButton:hover { background-color: #5a6268; }
        #chatbox::-webkit-scrollbar { width: 8px; } #chatbox::-webkit-scrollbar-track { background: #f1f1f1; }
        #chatbox::-webkit-scrollbar-thumb { background: #007bff; border-radius: 4px; } #chatbox::-webkit-scrollbar-thumb:hover { background: #0056b3; }
        .typing-indicator { font-style: italic; color: #6c757d; padding: 5px 20px; font-size: 0.9em; height: 20px; }
        
        /* ACK Status Styling - Applied to the message div directly */
        .message.status-acked { background-color: #d4edda !important; border-color: #c3e6cb !important; color: #155724 !important; }
        .message.status-acked .sender-info, .message.status-acked .timestamp { color: #0c5460 !important; }
        .message.status-failed-ack { background-color: #f8d7da !important; border-color: #f5c6cb !important; color: #721c24 !important; }
        .message.status-failed-ack .sender-info, .message.status-failed-ack .timestamp { color: #721c24 !important; }
        .message.status-pending-ack { background-color: #fff3cd !important; border-color: #ffeeba !important; color: #856404 !important; }
        .message.status-pending-ack .sender-info, .message.status-pending-ack .timestamp { color: #856404 !important; }

        @media (max-width: 600px) { /* Responsive adjustments */
            .chat-container { max-width: 100vw; border-radius: 0; box-shadow: none; margin: 0; } #chatbox { padding: 8px; }
            #controls { flex-direction: column; padding: 8px; } #messageInput { margin-right: 0; margin-bottom: 8px; font-size: 1em; }
            #sendButton, #clearChatButton { width: 100%; font-size: 1em; padding: 12px 0; }
            #clearChatButton { margin-left: 0; margin-top: 8px;} header { font-size: 1em; padding: 10px 8px; }
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <header><span class="title" id="pageTitle">LoRa Messenger</span><span id="connectionStatus" title="Connection Status"></span></header>
        <div id="chatbox"></div>
        <div class="typing-indicator" id="typingIndicator"></div>
        <div id="controls">
            <input type="text" id="messageInput" placeholder="Type a message...">
            <button id="sendButton">Send</button>
            <button id="clearChatButton">Clear Chat</button>
        </div>
    </div>
    <script>
        const chatbox = document.getElementById('chatbox');
        const messageInput = document.getElementById('messageInput');
        const sendButton = document.getElementById('sendButton');
        const connectionStatusElement = document.getElementById('connectionStatus');
        const pageTitleElement = document.getElementById('pageTitle');
        let websocket;
        let myDeviceId = 'UnknownDevice';
        let boardName = 'Node';

        function generateLocalId() { return 'local_msg_' + Date.now() + '_' + Math.random().toString(36).substr(2, 5); }
        function getCurrentTime() { return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }); }

        function updateConnectionStatus(status) {
            connectionStatusElement.className = ''; 
            let statusTitle = 'Connection Status';
            if (status === 'connected') { connectionStatusElement.classList.add('status-connected'); statusTitle = 'Connected to ' + boardName; }
            else if (status === 'disconnected') { connectionStatusElement.classList.add('status-disconnected'); statusTitle = 'Disconnected from ' + boardName; }
            else { connectionStatusElement.classList.add('status-connecting'); statusTitle = 'Connecting to ' + boardName + '...'; }
            connectionStatusElement.title = statusTitle;
        }

        function appendMessage(text, sender, typeOverride = null, localId = null) {
            const msgDiv = document.createElement('div');
            let messageType = typeOverride || ((sender === myDeviceId) ? 'sent' : 'received');
            msgDiv.className = 'message ' + messageType; // Base classes
            if (localId) { msgDiv.setAttribute('data-local-id', localId); }
            if (messageType === 'system-message') { msgDiv.classList.remove('message'); }

            const senderInfoSpan = document.createElement('span');
            senderInfoSpan.className = 'sender-info';
            if (messageType !== 'system-message') {
                 senderInfoSpan.textContent = (sender === myDeviceId) ? `Me (${myDeviceId})` : sender;
                 msgDiv.appendChild(senderInfoSpan);
            }
            const contentSpan = document.createElement('span');
            contentSpan.textContent = text;
            msgDiv.appendChild(contentSpan);

            const timestampSpan = document.createElement('span');
            timestampSpan.className = 'timestamp';
            timestampSpan.textContent = getCurrentTime();
            msgDiv.appendChild(timestampSpan);

            chatbox.appendChild(msgDiv);
            chatbox.scrollTop = chatbox.scrollHeight;
            saveChatToLocalStorage(); // Save after each append
        }

        function updateMessageStatus(localId, status) {
            const msgDiv = chatbox.querySelector(`div[data-local-id="${localId}"]`);
            if (msgDiv) {
                msgDiv.classList.remove('status-pending-ack', 'status-acked', 'status-failed-ack'); // Clear previous ACK statuses
                // Re-apply base sent/received class if it got removed by status, then add new status
                if (!msgDiv.classList.contains('sent') && !msgDiv.classList.contains('received') && !msgDiv.classList.contains('system-message')) {
                    // This case should ideally not happen if classes are managed well, but as a safeguard:
                    // Determine original type if possible, or default to 'sent' if it has a localId
                    msgDiv.classList.add(myDeviceId && msgDiv.querySelector('.sender-info')?.textContent.startsWith('Me') ? 'sent' : 'received');
                }
                if (status === 'acked') { msgDiv.classList.add('status-acked'); }
                else if (status === 'failed_ack') { msgDiv.classList.add('status-failed-ack'); }
                else if (status === 'pending_ack') { msgDiv.classList.add('status-pending-ack'); }
                saveChatToLocalStorage(); // Save after status update
            } else { console.warn(`Could not find message with local_id ${localId} to update status.`); }
        }

        function initWebSocket() {
            console.log('Attempting to connect WebSocket...');
            updateConnectionStatus('connecting');
            // appendMessage('Connecting to ESP32...', 'System', 'system-message'); // Initial system message is less critical now
            
            websocket = new WebSocket(`ws://${window.location.hostname}/ws`);

            websocket.onopen = () => { console.log('WebSocket connection established'); updateConnectionStatus('connected'); };
            websocket.onclose = () => {
                console.log('WebSocket connection closed. Retrying...');
                updateConnectionStatus('disconnected');
                appendMessage(`Disconnected from ${boardName}. Retrying in 3s...`, 'System', 'system-message');
                setTimeout(initWebSocket, 3000);
            };
            websocket.onmessage = event => {
                console.log('Message from server:', event.data);
                try {
                    const parsed = JSON.parse(event.data);
                    if (parsed.type === 'system' && parsed.event === 'identity') {
                        myDeviceId = parsed.deviceId;
                        boardName = parsed.boardName || 'Node'; // Use board name from server
                        pageTitleElement.textContent = `${boardName} LoRa Messenger`;
                        document.title = `${boardName} LoRa Messenger`;
                        updateConnectionStatus('connected'); // Update status with board name
                        appendMessage(`Connected to ${boardName}. Your ID: ${myDeviceId} `, 'System', 'system-message');
                    } else if (parsed.type === 'ack_status') { updateMessageStatus(parsed.local_id, parsed.status); }
                    else if (parsed.sender && parsed.text) { appendMessage(parsed.text, parsed.sender); }
                    else { appendMessage(event.data, 'Peer?');  }
                } catch (e) { console.error("Error processing message from server:", e); appendMessage(event.data, 'RawData'); }
            };
            websocket.onerror = error => { console.error('WebSocket Error:', error); appendMessage('WebSocket error. Check console.', 'System', 'system-message'); };
        }

        sendButton.onclick = () => {
            const messageText = messageInput.value;
            if (messageText.trim() === "" || !websocket || websocket.readyState !== WebSocket.OPEN) { return; }
            const localMsgId = generateLocalId();
            const payload = JSON.stringify({ text: messageText, local_id: localMsgId }); 
            websocket.send(payload);
            appendMessage(messageText, myDeviceId, 'sent', localMsgId); // Explicitly 'sent'
            updateMessageStatus(localMsgId, 'pending_ack'); // Set initial status for UI
            messageInput.value = "";
            messageInput.focus();
        };
        messageInput.addEventListener('keypress', e => { if (e.key === 'Enter') { sendButton.onclick(); } });

        function saveChatToLocalStorage() {
            const messagesToSave = [];
            chatbox.querySelectorAll('div[class*="message"], div.system-message').forEach(div => {
                let status = null;
                if (div.classList.contains('status-acked')) status = 'acked';
                else if (div.classList.contains('status-failed-ack')) status = 'failed_ack';
                else if (div.classList.contains('status-pending-ack')) status = 'pending_ack';
                
                messagesToSave.push({
                    htmlContent: div.innerHTML, // Save innerHTML to reconstruct sender/content/timestamp
                    classes: Array.from(div.classList), // Save all classes
                    localId: div.getAttribute('data-local-id') || null,
                    // 'status' is implicitly saved via classes now
                });
            });
            localStorage.setItem('loraChatHistory-' + myDeviceId, JSON.stringify(messagesToSave));
        }

        function loadChatFromLocalStorage() {
            const storedMessages = JSON.parse(localStorage.getItem('loraChatHistory-' + myDeviceId) || '[]');
            chatbox.innerHTML = ''; // Clear existing chatbox content
            storedMessages.forEach(msgData => {
                const msgDiv = document.createElement('div');
                msgData.classes.forEach(cls => msgDiv.classList.add(cls));
                msgDiv.innerHTML = msgData.htmlContent;
                if (msgData.localId) {
                    msgDiv.setAttribute('data-local-id', msgData.localId);
                }
                chatbox.appendChild(msgDiv);
            });
            if (chatbox.children.length > 0) {
                 chatbox.scrollTop = chatbox.scrollHeight;
            }
        }
        
        document.getElementById('clearChatButton').onclick = () => {
            if (confirm('Are you sure you want to clear the chat history?')) {
                localStorage.removeItem('loraChatHistory-' + myDeviceId);
                chatbox.innerHTML = '';
                appendMessage('Chat history cleared.', 'System', 'system-message');
            }
        };
        
        window.onload = () => { 
            // myDeviceId might not be set yet, so loadChatFromLocalStorage might load generic history
            // It's better to load history AFTER identity is received, or make storage key independent of myDeviceId if it can change.
            // For now, let's try loading. If myDeviceId changes, old history might not be relevant or could be keyed differently.
            // A simple approach: always load 'loraChatHistory' (generic key)
            // Or, wait for identity. For now, let's load what we can.
            const genericHistory = JSON.parse(localStorage.getItem('loraChatHistory-UnknownDevice') || localStorage.getItem('loraChatHistory-undefined') || '[]');
            if (genericHistory.length > 0 && !localStorage.getItem('loraChatHistory-' + myDeviceId) ) {
                 // If there's generic history and no device-specific, maybe it's from before ID was set.
                 // This logic can be refined. For simplicity, we load based on current (possibly unset) myDeviceId.
            }
            loadChatFromLocalStorage(); // Will use 'UnknownDevice' initially if myDeviceId isn't set
            initWebSocket(); 
        };
    </script>
</body>
</html>
)rawliteral";

// SEND LoRa ACK STATUS UPDATES TO ALL WEBSOCKET CLIENTS
void sendLoraAckStatusToWebSocket(const String& localWebId, uint32_t loraMessageId, bool acked, bool finalFailure) {
    JsonDocument doc;
    doc["type"] = "ack_status";
    doc["local_id"] = localWebId;
    doc["lora_msg_id"] = loraMessageId; 

    if (finalFailure) { doc["status"] = "failed_ack"; }
    else if (acked) { doc["status"] = "acked"; }
    else { doc["status"] = "pending_ack"; }

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    ws.textAll(jsonOutput);
    Serial.printf("[Web] Sent ACK status to WS: %s\n", jsonOutput.c_str());
}

// WEBSOCKET EVENT HANDLER
void onWSEvent(AsyncWebSocket *socket_server, AsyncWebSocketClient *client, AwsEventType type,
                     void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      Serial.printf("[Web] WS Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      setDisplayWebSocketStatus(true); 
      JsonDocument identityDoc;
      identityDoc["type"] = "system";
      identityDoc["event"] = "identity";
      identityDoc["deviceId"] = currentMyDeviceId_web;
      identityDoc["boardName"] = currentBoardName_web; 
      String identityMessage;
      serializeJson(identityDoc, identityMessage);
      client->text(identityMessage);
      break;
    }
    case WS_EVT_DISCONNECT:
      Serial.printf("[Web] WS Client #%u disconnected\n", client->id());
      setDisplayWebSocketStatus(false); 
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0; 
        String messagePayload = (char*)data;
        Serial.printf("[Web] WS RX from Client #%u: %s\n", client->id(), messagePayload.c_str());

        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, messagePayload);

        if (error) {
          Serial.print(F("[Web] deserializeJson() failed: ")); Serial.println(error.f_str());
          client->text("{\"type\":\"error\", \"message\":\"Invalid JSON payload\"}"); 
          return;
        }

        const char* ws_text_cstr = doc["text"];
        const char* local_id_cstr = doc["local_id"];

        if (ws_text_cstr && local_id_cstr) {
          String messageContent = String(ws_text_cstr);
          String localWebId = String(local_id_cstr);
          
          Serial.printf("  Parsed from WS: text='%s', local_id='%s'\n", messageContent.c_str(), localWebId.c_str());

          if (!currentMyDeviceId_web.isEmpty() && !currentLoraPacketPrefix_web.isEmpty()) {
              bool queued = queueLoRaMessage(messageContent, currentMyDeviceId_web, currentLoraPacketPrefix_web, localWebId);
              if (!queued) {
                  Serial.println("  Error: Failed to queue message for LoRa TX.");
                  // client->text("{\"type\":\"error\", \"message\":\"Failed to queue LoRa message\", \"local_id\":\"" + localWebId + "\"}");
              }
          } else {
              Serial.println("[Web] Error: Device ID or LoRa prefix not set for sending LoRa from WS.");
          }
        } else {
            Serial.println("[Web] Error: WS JSON message does not contain 'text' and/or 'local_id' field.");
        }
      }
      break;
    }
    case WS_EVT_PONG: 
    case WS_EVT_ERROR: 
      break;
  }
}

// SETS UP THE WEB SERVER, WEBSOCKET, AND WIFI ACCESS POINT
void setupWebServer(const String& myDeviceId, const String& loraPrefix, const String& apSsid, const String& apPassword) {
  currentMyDeviceId_web = myDeviceId;
  currentLoraPacketPrefix_web = loraPrefix;
  currentBoardName_web = BOARD_TYPE_NAME; 

  Serial.print(F("[Web] Setting up AP: ")); Serial.println(apSsid);
  WiFi.softAP(apSsid.c_str(), apPassword.c_str()); 
  
  IPAddress AP_IP = WiFi.softAPIP();
  Serial.print(F("[Web] AP IP address: ")); Serial.println(AP_IP);
  setDisplayAPIP(AP_IP.toString()); 

  // WIFI EVENT HANDLER TO TRACK CONNECTED STATIONS AND UPDATE DISPLAY
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.printf("[WiFiEvt] Event: %d\n", event);
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        Serial.println("  Client Connected to AP");
        setDisplayWiFiClientCount(WiFi.softAPgetStationNum());
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        Serial.println("  Client Disconnected from AP");
        setDisplayWiFiClientCount(WiFi.softAPgetStationNum());
    }
  });

  ws.onEvent(onWSEvent); 
  server.addHandler(&ws);

  // SERVE THE MAIN HTML PAGE
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });

  server.begin(); 
  Serial.println(F("[Web] HTTP server started."));
}

// SENDS A JSON MESSAGE TO ALL CONNECTED WEBSOCKET CLIENTS
void sendWebSocketMessage(const String& jsonMessage) { 
  if (ws.count() > 0) { 
    ws.textAll(jsonMessage);
    // Serial.printf("[Web] Sent to WS (%u clients): %s\n", ws.count(), jsonMessage.c_str());
  }
}

void loopWebManager() {
    ws.cleanupClients();
}