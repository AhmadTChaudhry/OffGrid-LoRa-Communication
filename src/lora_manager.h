#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <RadioLib.h>
#include <Arduino.h>
#include <vector>
#include "config.h" 

// LORA PIN DEFINITIONS 
#if defined(HELTEC_V3_BOARD)
    #define LORA_SCK_PIN    9
    #define LORA_MISO_PIN   11
    #define LORA_MOSI_PIN   10
    #define LORA_NSS_PIN    8
    #define LORA_RESET_PIN  12
    #define LORA_DIO1_PIN   14
    #define LORA_BUSY_PIN   13
#elif defined(XIAO_ESP32S3_BOARD)
    #define LORA_SCK_PIN    7
    #define LORA_MISO_PIN   8
    #define LORA_MOSI_PIN   9
    #define LORA_NSS_PIN    41 
    #define LORA_RESET_PIN  42 
    #define LORA_DIO1_PIN   39 
    #define LORA_BUSY_PIN   40 
#else
    #error "LoRa pins not defined!"
#endif

// ACK MECHANISM CONFIGURATION
#define ACK_TIMEOUT_MS 5000    
#define MAX_SEND_RETRIES 4      
#define LORA_ACK_PREFIX "A:"    

extern SX1262 radio;

extern volatile bool loraPacketReceivedFlag;

extern uint32_t currentLoRaMessageId;

// CALLBACK FUNCTIONS
typedef void (*LoRaPacketCallback)(const String& senderId, const String& message); 
typedef void (*LoraAckStatusCallback)(const String& localWebId, uint32_t loraMessageId, bool acked, bool finalFailure); 

// STRUCTURE TO MANAGE OUTGOING MESSAGES
struct OutgoingMessage {
    String localWebId;          // ID from the web UI to correlate messages
    uint32_t loraMessageId;     // Unique LoRa message ID
    String packetContent;       // Full packet content (SENDER:P:MSG_ID:PAYLOAD)
    unsigned long lastSendTime; // Timestamp of the last transmission attempt
    int retriesLeft;            // Number of retries remaining
    enum Status { PENDING_ACK, ACKNOWLEDGED, FAILED_ACK } status; // Current status of the message
};
extern std::vector<OutgoingMessage> outgoingMessageQueue; // Queue for messages awaiting ACKs

// FUNCTION DECLARATIONS
void IRAM_ATTR onLoRaInterrupt();
void setupLoRa(const String& myDeviceId, const String& packetPrefix, LoRaPacketCallback rxCb, LoraAckStatusCallback ackCb);
bool queueLoRaMessage(const String& messageContent, const String& myDeviceId, const String& packetPrefix, const String& localWebId);
void handleLoRaEvents(const String& myDeviceId, const String& packetPrefix); 
void checkAckTimeouts();
void startLoRaReceive();

#endif 