#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// DEVICE CONFIGURATION - SET IN PLATFORMIO.INI
#if defined(HELTEC_V3_BOARD)
    const String MY_DEVICE_ID = "BigNode"; 
    const String LORA_PACKET_PREFIX = "P:";
    const String WIFI_SSID = "BigNode_AP";
    const String WIFI_PASSWORD = "offlinecomms";
    const String BOARD_TYPE_NAME = "BigNode";

#elif defined(XIAO_ESP32S3_BOARD) 
    const String MY_DEVICE_ID = "PhoneNode";
    const String LORA_PACKET_PREFIX = "P:";
    const String WIFI_SSID = "PhoneNode_AP";
    const String WIFI_PASSWORD = "offlinecomms"; 
    const String BOARD_TYPE_NAME = "PhoneNode";

#else
    #error "Board type not defined!"
#endif

#endif