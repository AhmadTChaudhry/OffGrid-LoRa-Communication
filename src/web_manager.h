#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

// FUNCTION DECLARATIONS
void setupWebServer(const String& myDeviceId, const String& loraPrefix, const String& apSsid, const String& apPassword);
void sendWebSocketMessage(const String& jsonMessage);
void sendLoraAckStatusToWebSocket(const String& localWebId, uint32_t loraMessageId, bool acked, bool finalFailure);
void loopWebManager(); 

#endif 