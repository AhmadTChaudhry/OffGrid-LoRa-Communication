#include <Arduino.h>
#include "config.h"         
#include "display_manager.h"
#include "lora_manager.h"
#include "web_manager.h"
#include <ArduinoJson.h> 

// BUTTON CONFIGURATION
#define BUTTON_PIN 0
#define DEBOUNCE_DELAY 50

// BUTTON STATE VARIABLES
volatile bool buttonPressed = false;
unsigned long lastButtonTime = 0;

// INTERRUPT HANDLER FOR BUTTON
void IRAM_ATTR onButtonPressed() {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonTime > DEBOUNCE_DELAY) {
        buttonPressed = true;
        lastButtonTime = currentTime;
    }
}

// CALLBACK WHEN A VALID PEER DATA MESSAGE IS RECEIVED
void onLoRaPacketReceivedForWeb(const String& senderId, const String& message) {
    Serial.printf("[MainApp] LoRa RX from %s: '%s'. Forwarding to WebSocket.\n", senderId.c_str(), message.c_str());
    setLastLoRaRx(message); // Update display with the received message

    JsonDocument doc; 
    doc["sender"] = senderId;
    doc["text"] = message;
    String jsonMessage;
    serializeJson(doc, jsonMessage);
    sendWebSocketMessage(jsonMessage); 
}

// CALLBACK WHEN A LORA ACK STATUS IS UPDATED TO WEB
void onLoraAckStatusUpdateToWeb(const String& localWebId, uint32_t loraMessageId, bool acked, bool finalFailure) {
    Serial.printf("[MainApp] LoRa ACK Status for MSG_ID: %u (WebLocalID: %s) -> Acked: %s, FinalFail: %s\n", 
                  loraMessageId, localWebId.c_str(), acked ? "Yes" : "No", finalFailure ? "Yes" : "No");
    
    sendLoraAckStatusToWebSocket(localWebId, loraMessageId, acked, finalFailure);
}

void onWebSocketConnectionChanged(bool connected) {
    Serial.printf("[MainApp] WebSocket connection status changed: %s\n", connected ? "Connected" : "Disconnected");
    setDisplayWebSocketStatus(connected);
}

void onLoRaMessageSentFromUI(const String& message) {
    Serial.printf("[MainApp] LoRa message sent from UI: %s\n", message.c_str());
    setLastLoRaTx(message);
}

void setup() {
  Serial.begin(115200);

  Serial.println(F("\n============================================="));
  Serial.printf("LoRa Messenger Node: %s (%s)\n", BOARD_TYPE_NAME.c_str(), MY_DEVICE_ID.c_str());
  Serial.println(F("============================================="));

  Serial.println(F("[Setup] Initializing Display Module..."));
  setupDisplay();

  Serial.println(F("[Setup] Initializing Button..."));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPressed, FALLING);

  Serial.println(F("[Setup] Initializing LoRa Module..."));
  // Pass Device ID, Packet Prefix, and callbacks to LoRa Manager
  setupLoRa(MY_DEVICE_ID, LORA_PACKET_PREFIX, onLoRaPacketReceivedForWeb, onLoraAckStatusUpdateToWeb);

  Serial.println(F("[Setup] Initializing Web Server Module..."));
  // Pass Device ID, LoRa Prefix (for sending from UI), and AP credentials from config.h
  setupWebServer(MY_DEVICE_ID, LORA_PACKET_PREFIX, WIFI_SSID, WIFI_PASSWORD);
  setDisplayAPIP(WiFi.softAPIP().toString());
  
  Serial.println(F("[Setup] System Setup Complete. Ready."));
  setDisplayStatusLine("System Ready");
}

unsigned long lastWifiClientCheck = 0;
const unsigned long WIFI_CLIENT_CHECK_INTERVAL = 2000; 

void loop() {
  // HANDLE BUTTON PRESS
  if (buttonPressed) {
    buttonPressed = false; 
    
    Serial.println(F("[Button] Button pressed! Sending 'im alive' message..."));
    setDisplayStatusLine("Button: Sending...");
    
    // SEND "IM ALIVE" MESSAGE VIA LORA
    String aliveMessage = "im alive";
    bool queued = queueLoRaMessage(aliveMessage, MY_DEVICE_ID, LORA_PACKET_PREFIX, "button_msg");
    
    if (queued) {
      Serial.println(F("[Button] 'im alive' message queued successfully"));
      setLastLoRaTx(aliveMessage);
      setDisplayStatusLine("Button: Sent OK");
    } else {
      Serial.println(F("[Button] Failed to queue 'im alive' message"));
      setDisplayStatusLine("Button: Send Failed");
    }
  }

  handleLoRaEvents(MY_DEVICE_ID, LORA_PACKET_PREFIX);

  if (millis() - lastWifiClientCheck > WIFI_CLIENT_CHECK_INTERVAL) {
    int numClients = WiFi.softAPgetStationNum();
    setDisplayWiFiClientCount(numClients);
    lastWifiClientCheck = millis();
  }
  updateDisplay();
  loopWebManager();
}