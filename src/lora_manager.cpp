#include "lora_manager.h"
#include "display_manager.h"
#include "config.h"
#include "encryption.h"

// INITIALIZE LORA MODULE
SX1262 radio = new Module(LORA_NSS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN);

// LORA PHYSICAL LAYER PARAMETERS
float lora_frequency = 915.0;
float lora_bandwidth = 125.0;
uint8_t lora_sf = 7;
uint8_t lora_cr = 5;
uint8_t lora_sync_word = 0x34;
int8_t lora_power = 17;
uint16_t lora_preamble = 8;

// GLOBAL VARIABLES
volatile bool loraPacketReceivedFlag = false;
uint32_t currentLoRaMessageId = 0;
std::vector<OutgoingMessage> outgoingMessageQueue;

// CALLBACK FUNCTION POINTERS
static LoRaPacketCallback onExternalReceiveCallback = nullptr;
static LoraAckStatusCallback onLoraAckStatusCallback = nullptr;

const size_t ABSOLUTE_MIN_PACKET_LEN = 5;

// INTERRUPT SERVICE ROUTINE - FLAG WHEN DIO1 IS TRIGGERED
void IRAM_ATTR onLoRaInterrupt()
{
  loraPacketReceivedFlag = true;
}

// SETUP LORA RADIO MODULE
void setupLoRa(const String &myDeviceId, const String &packetPrefix, LoRaPacketCallback rxCb, LoraAckStatusCallback ackCb)
{
  onExternalReceiveCallback = rxCb;
  onLoraAckStatusCallback = ackCb;

  Serial.print(F("[LoRa] Initializing ... "));
  setDisplayStatusLine("LoRa Init...");

  int radio_state = radio.begin(lora_frequency, lora_bandwidth, lora_sf, lora_cr, lora_sync_word, lora_power, lora_preamble);
  if (radio_state == RADIOLIB_ERR_NONE)
  {
#if defined(HELTEC_V3_BOARD)
    radio.setDio2AsRfSwitch(true);
    Serial.print(F("RF Switch (DIO2) enabled for Heltec. "));
#endif
    Serial.println(F("OK"));
    setDisplayStatusLine("LoRa OK");
  }
  else
  {
    Serial.print(F("FAILED, code: "));
    Serial.println(radio_state);
    setDisplayStatusLine("LoRa Fail " + String(radio_state));
    while (true)
      ; // Halt on critical LoRa failure
  }
  radio.setDio1Action(onLoRaInterrupt);
  startLoRaReceive();
}

// SET LORA RADIO INTO RECEIVE MODE
void startLoRaReceive()
{
  Serial.print(F("[LoRa] Starting RX mode ... "));
  int radio_state = radio.startReceive();
  if (radio_state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("OK"));
    setDisplayStatusLine("Listening...");
  }
  else
  {
    Serial.print(F("FAILED, code: "));
    Serial.println(radio_state);
    setDisplayStatusLine("Listen Fail " + String(radio_state));
  }
}

// TRANSMIT A LORA PACKET WITH SPECIFIED CONTENT
static bool transmitLoRaPacket(const String &packetToSend, const String &originalMessageContent)
{
  Serial.printf("[LoRa] TX Attempt: %s (Length: %d)\n", packetToSend.c_str(), packetToSend.length());
  setLastLoRaTx(originalMessageContent);
  setDisplayStatusLine("Sending LoRa...");

  // TRANSMIT THE PACKET USING EXPLICIT LENGTH
  int tx_state = radio.transmit((uint8_t *)packetToSend.c_str(), packetToSend.length());

  if (tx_state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("  LoRa TX Success (RadioLib)"));
    setDisplayStatusLine("LoRa Sent");
  }
  else
  {
    Serial.print(F("  LoRa TX FAILED (RadioLib), code: "));
    Serial.println(tx_state);
    setDisplayStatusLine("LoRa Send Fail");
    startLoRaReceive();
    return false; // Indicate TX failure
  }
  startLoRaReceive(); // Reenable RX mode after a successful transmission
  return true;        // Indicate TX success
}

// QUEUE A MESSAGE FOR LORA TRANSMISSION, MANAGE ACK TRACKING
bool queueLoRaMessage(const String &messageContent, const String &myDeviceId, const String &packetPrefix, const String &localWebId)
{
  currentLoRaMessageId++;
  if (currentLoRaMessageId == 0)
    currentLoRaMessageId = 1;

  // ENCRYPT THE MESSAGE CONTENT
  String encryptedContent = encryptMessage(messageContent);
  Serial.printf("[LoRa] Original: '%s', Encrypted: '%s'\n", messageContent.c_str(), encryptedContent.c_str());

  // SENDER_ID:PACKET_PREFIX:MESSAGE_ID:ENCRYPTED_MESSAGE_CONTENT
  String loraPacket = myDeviceId + ":" + packetPrefix + String(currentLoRaMessageId) + ":" + encryptedContent;

  OutgoingMessage newMessage;
  newMessage.localWebId = localWebId;
  newMessage.loraMessageId = currentLoRaMessageId;
  newMessage.packetContent = loraPacket;
  newMessage.lastSendTime = millis();
  newMessage.retriesLeft = MAX_SEND_RETRIES;
  newMessage.status = OutgoingMessage::PENDING_ACK;

  outgoingMessageQueue.push_back(newMessage);
  Serial.printf("[LoRa] Queued MSG_ID:%u (LocalWebID:%s) for TX. Content: %s\n", currentLoRaMessageId, localWebId.c_str(), messageContent.c_str());

  transmitLoRaPacket(loraPacket, messageContent);

  // NOTIFY WEB UI THAT MESSAGE IS SENT AND NOW PENDING ACKNOWLEDGMENT
  if (onLoraAckStatusCallback)
  {
    onLoraAckStatusCallback(localWebId, currentLoRaMessageId, false, false); // acked=false, finalFailure=false
  }
  return true; // Successful queuing
}

// HANDLER FOR INCOMING LORA PACKETS AND ACK PROCESSING
void handleLoRaEvents(const String &myDeviceId, const String &packetPrefix)
{
  checkAckTimeouts();

  bool rxEventOccurredThisCycle = false;

  if (loraPacketReceivedFlag)
  {
    loraPacketReceivedFlag = false;  // Clear the ISR flag
    rxEventOccurredThisCycle = true; // Mark that we are processing an RX event

    // Attempt a read if the flag was set, then scrutinize the result.
    String rawPacketStr;
    int rx_state = radio.readData(rawPacketStr);

    if (rx_state == RADIOLIB_ERR_NONE && rawPacketStr.length() < ABSOLUTE_MIN_PACKET_LEN)
    {
      Serial.printf("Ignored (Packet too short after read: %d chars). \n", rawPacketStr.length());
    }
    else if (rx_state == RADIOLIB_ERR_NONE)
    {
      Serial.print(F("[LoRa] RX Raw (passed initial checks): "));
      Serial.println(rawPacketStr);
      float rssi = radio.getRSSI();
      float snr = radio.getSNR();
      Serial.printf("  RSSI: %.2f dBm, SNR: %.2f dB\n", rssi, snr);

      int firstColon = rawPacketStr.indexOf(':');
      if (firstColon <= 0 || firstColon == rawPacketStr.length() - 1)
      {
        Serial.println(F("Ignored (No valid Device ID separator ':' found)."));
        setLastLoRaRx("No ID");
        setDisplayStatusLine("LoRa RX NoID");
      }
      else
      {
        String senderId = rawPacketStr.substring(0, firstColon);
        String restOfPacket = rawPacketStr.substring(firstColon + 1);
        Serial.print(F("  Parsed RX Sender ID: "));
        Serial.println(senderId);

        if (senderId.length() == 0 || senderId.length() > 20)
        {
          Serial.printf("  Ignored (Wrong Sender ID length: %d). Parsed: '%s'\n", senderId.length(), senderId.c_str());
        }
        else if (senderId == myDeviceId)
        {
          Serial.println(F("  Ignored (Self-Echo: ID Match)."));
        }
        else if (restOfPacket.startsWith(LORA_ACK_PREFIX))
        {
          String ackPayload = restOfPacket.substring(strlen(LORA_ACK_PREFIX));
          uint32_t ackedMessageId = ackPayload.toInt();
          Serial.printf("  Received ACK from %s for MSG_ID: %u (Raw ACK Payload: '%s')\n", senderId.c_str(), ackedMessageId, ackPayload.c_str());
          bool foundAndUpdated = false;
          for (auto it = outgoingMessageQueue.begin(); it != outgoingMessageQueue.end();)
          {
            if (it->loraMessageId == ackedMessageId && it->status == OutgoingMessage::PENDING_ACK)
            {
              Serial.printf("  Matched ACK to outgoing MSG_ID: %u (LocalWebID: %s). Marking ACKED.\n", ackedMessageId, it->localWebId.c_str());
              it->status = OutgoingMessage::ACKNOWLEDGED;
              if (onLoraAckStatusCallback)
              {
                onLoraAckStatusCallback(it->localWebId, it->loraMessageId, true, false);
              }
              it = outgoingMessageQueue.erase(it);
              foundAndUpdated = true;
            }
            else
            {
              ++it;
            }
          }
          if (!foundAndUpdated)
          {
            Serial.printf("  Warning: Received ACK for unknown/already-acked/failed MSG_ID: %u\n", ackedMessageId);
          }
        }
        else if (restOfPacket.startsWith(packetPrefix))
        {
          if (restOfPacket.length() < packetPrefix.length() + 3)
          {
            Serial.println(F("  Ignored (Data packet too short after prefix)."));
          }
          else
          {
            String payloadWithMsgId = restOfPacket.substring(packetPrefix.length());
            int secondColon = payloadWithMsgId.indexOf(':');
            if (secondColon <= 0 || secondColon == payloadWithMsgId.length() - 1)
            {
              Serial.println(F("  Ignored (Malformed data packet - no valid MSG_ID:PAYLOAD separator)."));
              setLastLoRaRx("Malformed");
              setDisplayStatusLine("LoRa RX Bad");
            }
            else
            {
              uint32_t receivedMessageId = payloadWithMsgId.substring(0, secondColon).toInt();
              String encryptedMessage = payloadWithMsgId.substring(secondColon + 1);

              // Decrypt the message
              String actualMessage = decryptMessage(encryptedMessage);
              Serial.printf("  [LoRa] Encrypted: '%s', Decrypted: '%s'\n", encryptedMessage.c_str(), actualMessage.c_str());

              Serial.print(F("  Peer Message (MSG_ID:"));
              Serial.print(receivedMessageId);
              Serial.print(F(") from "));
              Serial.print(senderId);
              Serial.print(F(": "));
              Serial.println(actualMessage);
              setLastLoRaRx(actualMessage);
              setDisplayStatusLine("LoRa RX OK");

              String ackPacket = myDeviceId + ":" + LORA_ACK_PREFIX + String(receivedMessageId); // Simple ACK
              Serial.printf("  Sending ACK for MSG_ID %u to %s -> Packet: %s (Length: %d)\n", receivedMessageId, senderId.c_str(), ackPacket.c_str(), ackPacket.length());
              int ack_tx_status = radio.transmit((uint8_t *)ackPacket.c_str(), ackPacket.length());
              if (ack_tx_status == RADIOLIB_ERR_NONE)
                Serial.println("    ACK sent successfully.");
              else
                Serial.printf("    ACK send failed, code: %d\n", ack_tx_status);
              startLoRaReceive(); // After sending ACK

              if (onExternalReceiveCallback)
              {
                onExternalReceiveCallback(senderId, actualMessage);
              }
            }
          }
        }
        else
        {
          Serial.println(F("  Ignored (Packet has no known prefix for this app after sender ID)."));
          if (senderId != myDeviceId)
          {
            setLastLoRaRx("Wrong Prefix");
            setDisplayStatusLine("LoRa RX Prefix");
          }
        }
      }
    }
    else if (rx_state == RADIOLIB_ERR_CRC_MISMATCH)
    {
      Serial.println(F("[LoRa] RX CRC error!"));
      setLastLoRaRx("CRC Error!");
      setDisplayStatusLine("LoRa RX CRC");
    }
    else
    {
      Serial.printf("  [LoRa DEBUG] radio.readData() returned error: %d. No valid packet to parse.\n", rx_state);
      setLastLoRaRx("Read Fail");
      setDisplayStatusLine("LoRa RX Fail");
    }

    delay(150);
    startLoRaReceive();
  } 

  if (rxEventOccurredThisCycle)
  {
    Serial.println(F("[LoRa DEBUG] Applying post-RX-event cool-down delay."));
    // delay(150);
    startLoRaReceive();
  }
}

// CHECK FOR ACK TIMEOUTS AND HANDLE RETRANSMISSIONS
void checkAckTimeouts()
{
  unsigned long currentTime = millis();
  for (auto it = outgoingMessageQueue.begin(); it != outgoingMessageQueue.end();)
  {
    if (it->status == OutgoingMessage::PENDING_ACK)
    {
      if (currentTime - it->lastSendTime > ACK_TIMEOUT_MS)
      { // Check if timeout expired
        if (it->retriesLeft > 0)
        { 
          it->retriesLeft--;
          it->lastSendTime = currentTime; // Update last send time
          Serial.printf("[LoRa] ACK Timeout for MSG_ID: %u (LocalWebID: %s). Retrying (%d left). Packet: %s\n",
                        it->loraMessageId, it->localWebId.c_str(), it->retriesLeft, it->packetContent.c_str());
          String originalMsg = it->packetContent.substring(it->packetContent.lastIndexOf(':') + 1);
          transmitLoRaPacket(it->packetContent, originalMsg); // Retransmit
          ++it;
        }
        else
        { // No retries left
          Serial.printf("[LoRa] ACK Timeout for MSG_ID: %u (LocalWebID: %s). MAX RETRIES REACHED. Marking FAILED.\n",
                        it->loraMessageId, it->localWebId.c_str());
          it->status = OutgoingMessage::FAILED_ACK;
          if (onLoraAckStatusCallback)
          { // Notify about final failure
            onLoraAckStatusCallback(it->localWebId, it->loraMessageId, false, true);
          }
          it = outgoingMessageQueue.erase(it); // Remove from queue
        }
      }
      else
      { // Timeout not yet expired
        ++it;
      }
    }
    else
    { 
      if (it->status == OutgoingMessage::ACKNOWLEDGED || it->status == OutgoingMessage::FAILED_ACK)
      {
        Serial.printf("[LoRa DEBUG] Cleaning up already processed MSG_ID: %u (LocalWebID: %s) Status: %d\n", it->loraMessageId, it->localWebId.c_str(), it->status);
        it = outgoingMessageQueue.erase(it); // Defensive removal
      }
      else
      {
        ++it; 
      }
    }
  }
}