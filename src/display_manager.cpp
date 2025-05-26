#include "display_manager.h"
#include "splash.h"   
#include <Wire.h>     
#include <WiFi.h>     

// BOARD BASED U8g2 CONFIGURATION
#if defined(HELTEC_V3_BOARD)
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ DISPLAY_OLED_RESET_PIN);
#elif defined(XIAO_ESP32S3_BOARD)
    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ DISPLAY_OLED_RESET_PIN);
#endif

// DISPLAY STATE AND CONTENT VARIABLES
static DisplayState currentDisplayState = STATE_BOOTING;
static String storedApSsid_disp;
static String storedApPassword_disp;
static String currentApIp_disp = "0.0.0.0";
static int wifiClientCount_disp = 0;

static String lastLoRaRx_disp_content = "---";
static String lastLoRaTx_disp_content = "---";
static String statusMsg_disp_content = "Booting...";

volatile bool displayNeedsUpdate = true;

// HELPER FUNCTIONS (BOARD-SPECIFIC IF NECESSARY)
#if defined(HELTEC_V3_BOARD)
void powerOnDisplay_internal() {
    pinMode(DISPLAY_VEXT_PIN, OUTPUT);
    digitalWrite(DISPLAY_VEXT_PIN, LOW);
    delay(100);
}
#endif

// PUBLIC API IMPLEMENTATIONS

void setupDisplay() {
    storedApSsid_disp = WIFI_SSID;
    storedApPassword_disp = WIFI_PASSWORD;

    #if defined(HELTEC_V3_BOARD)
        powerOnDisplay_internal();
    #endif

    Wire.begin(DISPLAY_OLED_SDA_PIN, DISPLAY_OLED_SCL_PIN);
    if (!u8g2.begin()) {
        Serial.println(F("  Display Init FAILED!"));
        statusMsg_disp_content = "Display Fail";
        displayNeedsUpdate = true;
        return;
    }
    
    Serial.println(F("  Display Init OK."));
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    Serial.println(F("  Displaying Splash Screen..."));
    u8g2.drawBitmap(0, 0, SPLASH_SCREEN_WIDTH / 8, SPLASH_SCREEN_HEIGHT, splash_logo);
    u8g2.sendBuffer();
    delay(2000); // Display splash for 2 seconds

    currentDisplayState = STATE_AP_DETAILS; // New default state after splash
    statusMsg_disp_content = "AP Starting..."; 
    displayNeedsUpdate = true;
}

void updateDisplay() {
    if (statusMsg_disp_content == "Display Fail" && currentDisplayState != STATE_BOOTING) {
        if (!displayNeedsUpdate) return; 
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(0,10); u8g2.print("Display Fail!");
        u8g2.sendBuffer();
        displayNeedsUpdate = false;
        return;
    }
    
    if (!displayNeedsUpdate) return;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tr); 

    // COMMON HEADER FOR MOST STATES
    if (currentDisplayState != STATE_BOOTING) {
        u8g2.setCursor(0, 10); u8g2.print("OFFGRID COMMS");
        u8g2.drawLine(0, 12, u8g2.getDisplayWidth(), 12); // Horizontal line
    }
    

    switch (currentDisplayState) {
        case STATE_BOOTING: // Briefly shown by setupDisplay's splash
            u8g2.setCursor(0, 10); u8g2.print("Booting...");
            u8g2.setCursor(0, 22); u8g2.print(BOARD_TYPE_NAME);
            break;

        case STATE_AP_DETAILS:
            u8g2.setCursor(0, 28); u8g2.print("AP: " + storedApSsid_disp.substring(0, 18));
            u8g2.setCursor(0, 42); u8g2.print("PASS: " + storedApPassword_disp.substring(0, 16));
            break;

        case STATE_IP_READY:
            u8g2.setCursor(0, 35); u8g2.print("Open: " + currentApIp_disp); 
            break;

        case STATE_CHAT_VIEW:
            u8g2.setCursor(0, 28); u8g2.print("TX: " + lastLoRaTx_disp_content.substring(0,18));
            u8g2.setCursor(0, 42); u8g2.print("RX: " + lastLoRaRx_disp_content.substring(0,18));
            break;
        
        case STATE_RX_ALERT:
            u8g2.setCursor(0, 35); u8g2.print("RX: " + lastLoRaRx_disp_content.substring(0,18)); 
            break;
    }
    if (currentDisplayState != STATE_BOOTING) {
        u8g2.setCursor(0, 58); u8g2.print(statusMsg_disp_content.substring(0,20));
    }

    u8g2.sendBuffer();
    displayNeedsUpdate = false;
}

void setDisplayState(DisplayState newState) {
    if (currentDisplayState != newState) {
        Serial.printf("[Display] State changed from %d to: %d\n", currentDisplayState, newState);
        currentDisplayState = newState;
        switch (newState) {
            case STATE_AP_DETAILS:
                statusMsg_disp_content = "AP Mode Active";
                break;
            case STATE_IP_READY:
                statusMsg_disp_content = String(wifiClientCount_disp) + " WiFi Client(s)";
                break;
            case STATE_CHAT_VIEW:
                statusMsg_disp_content = "Web Client Online";
                break;
            case STATE_RX_ALERT:
                statusMsg_disp_content = "LoRa Msg Received!";
                break;
            default:
                break;
        }
        displayNeedsUpdate = true;
    }
}

void setDisplayAPIP(const String& ipAddress) {
    if (currentApIp_disp != ipAddress) {
        currentApIp_disp = ipAddress;
        if (currentDisplayState == STATE_AP_DETAILS && ipAddress != "0.0.0.0") {
            statusMsg_disp_content = "AP Ready. Clients: " + String(wifiClientCount_disp);
        }
        displayNeedsUpdate = true;
    }
}

void setDisplayWiFiClientCount(int count) {
    bool prevHadClients = (wifiClientCount_disp > 0);
    bool nowHasClients = (count > 0);
    wifiClientCount_disp = count;

    if (prevHadClients != nowHasClients) { 
        if (nowHasClients) {
            if (currentDisplayState == STATE_AP_DETAILS) {
                setDisplayState(STATE_IP_READY);
            }
        } else {
            if (currentDisplayState == STATE_IP_READY || currentDisplayState == STATE_CHAT_VIEW ) { 
                // If in IP_READY or CHAT_VIEW and all WiFi clients disconnect, revert to AP_DETAILS
                setDisplayState(STATE_AP_DETAILS);
            }
        }
    }
    // Update status line if relevant and state hasn't changed to something else
    if (currentDisplayState == STATE_AP_DETAILS) {
         statusMsg_disp_content = "AP Ready. Clients: " + String(wifiClientCount_disp);
    } else if (currentDisplayState == STATE_IP_READY) {
         statusMsg_disp_content = String(wifiClientCount_disp) + " WiFi Client(s)";
    }
    displayNeedsUpdate = true;
}

void setDisplayWebSocketStatus(bool connected) {
    if (connected) {
        if (currentDisplayState == STATE_AP_DETAILS || currentDisplayState == STATE_IP_READY || currentDisplayState == STATE_RX_ALERT) {
            setDisplayState(STATE_CHAT_VIEW);
        }
    } else { // WebSocket disconnected
        if (currentDisplayState == STATE_CHAT_VIEW) {
            if (wifiClientCount_disp > 0) {
                setDisplayState(STATE_IP_READY); // Revert to IP view if WiFi clients still present
            } else {
                setDisplayState(STATE_AP_DETAILS); // Revert to AP details if no WiFi clients
            }
        }
    }
    displayNeedsUpdate = true; 
}

void setDisplayStatusLine(const String& status) {
    if (statusMsg_disp_content != status) {
        statusMsg_disp_content = status;
        displayNeedsUpdate = true;
    }
}

void setLastLoRaRx(const String& rx) {
    if (lastLoRaRx_disp_content != rx) {
        lastLoRaRx_disp_content = rx;

        if (currentDisplayState == STATE_AP_DETAILS || currentDisplayState == STATE_IP_READY) {
            setDisplayState(STATE_RX_ALERT);
        } else {
            if (currentDisplayState == STATE_RX_ALERT) statusMsg_disp_content = "LoRa Msg Updated!";
            else if (currentDisplayState == STATE_CHAT_VIEW) statusMsg_disp_content = "New LoRa RX";
        }
        displayNeedsUpdate = true;
    }
}

void setLastLoRaTx(const String& tx) {
    if (lastLoRaTx_disp_content != tx) {
        lastLoRaTx_disp_content = tx;
        if (currentDisplayState == STATE_CHAT_VIEW) { 
             statusMsg_disp_content = "LoRa TX Sent";
        }
        displayNeedsUpdate = true;
    }
}