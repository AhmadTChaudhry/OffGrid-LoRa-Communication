#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Arduino.h>
#include "config.h" 

// PIN DEFINITIONS - SET IN PLATFORMIO.INI
#if defined(HELTEC_V3_BOARD)
    #define DISPLAY_OLED_SDA_PIN 17
    #define DISPLAY_OLED_SCL_PIN 18
    #define DISPLAY_OLED_RESET_PIN 21 
    #define DISPLAY_VEXT_PIN 36       
#elif defined(XIAO_ESP32S3_BOARD)
    #define DISPLAY_OLED_SDA_PIN 5  
    #define DISPLAY_OLED_SCL_PIN 6  
    #define DISPLAY_OLED_RESET_PIN U8X8_PIN_NONE 
#else
    #error "Display pins not defined!"
#endif

#if defined(HELTEC_V3_BOARD)
    extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(XIAO_ESP32S3_BOARD)
    extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
#endif


extern volatile bool displayNeedsUpdate; 

// DISPLAY STATES
enum DisplayState {
    STATE_BOOTING,             
    STATE_AP_DETAILS,           
    STATE_IP_READY,             
    STATE_CHAT_VIEW,            
    STATE_RX_ALERT              
};

// FUNCTION DECLARATIONS
void setupDisplay(); 
void updateDisplay();
void setDisplayState(DisplayState newState); // To manually change state if needed from main
void setDisplayAPIP(const String& ipAddress); // To provide the AP IP once known
void setDisplayWiFiClientCount(int count);   // To inform display manager of WiFi clients
void setDisplayWebSocketStatus(bool connected); // To inform of WebSocket client connection
void setDisplayStatusLine(const String& status); // For a general status line at the bottom
void setLastLoRaRx(const String& rx);            // Update last received LoRa message
void setLastLoRaTx(const String& tx);            // Update last transmitted LoRa message

#endif