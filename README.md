# **1.**  **User Manual**

## 1.1. Hardware Requirements

·      One or more supported ESP32 LoRa boards (Any ESP32 with SX1262 module will work):

o   Heltec WiFi LoRa 32 V3

o   Seeed Studio XIAO ESP32S3 (with appropriate LoRa module and OLED if using all features)

·      USB C cable for programming and power.

·      Computer with PlatformIO Visual Code extension installed.

·      LoRa Antenna (appropriate for your board and region, e.g., 915MHz for AU).

·      Client device with WiFi and a web browser (smartphone, laptop) to access the chat UI.

## 1.2. Software Requirements & Dependencies

·      **PlatformIO IDE Extension**: Recommended for managing project and dependencies.

·      **Libraries:**

o   radiolib (JGromes/RadioLib)

o   ESPAsyncWebServer (me-no-dev/ESPAsyncWebServer)

o   AsyncTCP (me-no-dev/AsyncTCP)

o   U8g2 (olikraus/U8g2)

o   ArduinoJson (bblanchon/ArduinoJson)

## 6.3. Configuration

·      **Board Selection (in platformio.ini file):**

o   Uncomment the desired board environment (e.g., default_envs = heltec_wifi_lora_32_v3 or default_envs = seeed_xiao_esp32s3).

·      **Device Configuration (in src/config.h):**

The file uses build flags (e.g., HELTEC_V3_BOARD, XIAO_ESP32S3_BOARD) set by PlatformIO environments to select the correct block of settings.

o   **MY_DEVICE_ID:** Unique name for this node (e.g., "BigNode", "PhoneNode"). Displayed in messages.

o   **LORA_PACKET_PREFIX:** Default "P:", used to identify data packets.

o   **WIFI_SSID**: SSID for the WiFi Access Point created by the device.

o   **WIFI_PASSWORD:** Password for the WiFi AP.

o   **BOARD_TYPE_NAME**: A friendly name for the board type.

·      **LoRa Parameters (in src/lora_manager.cpp):**

o   Frequency (lora_frequency), bandwidth, spreading factor, etc., can be adjusted if needed, but ensure all nodes use the same settings.

## 1.4. Installation & Flashing

1.        Clone the project repository (or download the source code).

2.        Open the project folder in Visual Studio.

3.        Ensure the correct board environment is selected in platformio.ini (see Configuration).

4.        Connect the ESP32 board to your computer via USB.

5.        PlatformIO should auto-detect the port. 

6.        Build and Upload the project using PlatformIO's "Upload" button or command (pio run -t upload).

## 1.5. Operation

·      **Power On:** After flashing, power on the ESP32 device.

·      **OLED Display:**

o   A splash screen will appear briefly.

o   Then, it will display "OFFGRID COMMS", AP details (AP: [SSID], PASS: [Password]), and status.

·      **Connect to WiFi AP:**

o   On your smartphone or laptop, search for WiFi networks.

o   Connect to the SSID displayed on the OLED using the password.

·      **Access Web UI:**

o   Once connected to the WiFi AP, the OLED display should update to show the device's IP address (e.g., "Open: 192.168.4.1").

o   Open a web browser on your client device and navigate to this IP address.

o   The "LoRa Messenger" web chat interface will load.

·      **Using the Chat:**

o   The UI will show a connection status indicator (green for connected WebSocket).

o   Your Device ID will be displayed.

o   Type messages in the input field and click "Send" or press Enter.

o   Sent messages appear on the right, initially with a "pending_ack" status (yellow background).

o   If the other node receives and ACKs the message, the status will change to "acked" (green).

o   If the message fails after retries, it will show "failed_ack" (red).

o   Received messages from other nodes appear on the left.

o   The OLED display will show snippets of the last TX and RX LoRa messages.

o   Chat history is saved in the browser's local storage. Use "Clear Chat" to remove it.

·      **Button Press (GPIO 0):**

o   Pressing the button (usually labeled "BOOT" or "FLASH" on dev boards, connected to GPIO0) will send a predefined LoRa message: "im alive". This will appear on the other node's UI and OLED.

# **2.**  **Link to Demonstration Video**

**Demo Video**: [https://deakin.au.panopto.com/Panopto/Pages/Viewer.aspx?id=5dae21fe-6c4f-45e1-8a6c-b2e90084d484](https://deakin.au.panopto.com/Panopto/Pages/Viewer.aspx?id=5dae21fe-6c4f-45e1-8a6c-b2e90084d484)
