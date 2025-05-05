# ESP32 Telegram Bot - Wake-on-LAN and PC Control

This project implements a Telegram-controlled bot using an ESP32 microcontroller. The ESP32 connects to a WiFi network and listens for specific Telegram commands from an authorized user to control a personal computer (PC) over the local network.

## Features

- Wake-on-LAN (WOL) to remotely power on a PC
- Shutdown and restart PC via HTTP requests
- Monitor PC status using ICMP ping
- Query system info and logs from a Flask-based backend
- OTA (Over-The-Air) firmware updates via a web interface
- ESP32 system status reporting
- Telegram-based user interface
- WiFi reconnection and watchdog handling

## Requirements

- ESP32 development board
- Telegram bot token and chat ID
- PC configured for Wake-on-LAN and running a Flask server
- Arduino IDE with the following libraries installed:
  - WiFi
  - WiFiClientSecure
  - WiFiUDP
  - UniversalTelegramBot
  - ArduinoJson
  - ESPping
  - WebServer
  - Update

## Setup

1. Clone the repository and open the `.ino` file in the Arduino IDE.
2. Create a `secrets.h` file containing your credentials:

```cpp
// secrets.h
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID_AUTORIZADO "YOUR_CHAT_ID"
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress broadcastIP(192, 168, 1, 255);
IPAddress ipPC(192, 168, 1, 101);
#define PUERTO_FLASK 5000
String tokenPC = "YOUR_SECRET_TOKEN_FOR_PC";
```

3. Flash the firmware to your ESP32.

4. Start the Flask server on your PC with the corresponding endpoints:
   - `/apagar`, `/reiniciar`, `/info`, `/logs`

## Commands

Send any of the following commands to the bot from your authorized Telegram account:

| Command             | Description                         |
|---------------------|-------------------------------------|
| /encender_pc        | Wake the PC via Wake-on-LAN         |
| /apagar_pc          | Shutdown the PC                     |
| /reiniciar_pc       | Restart the PC                      |
| /verificar_pc       | Check if the PC is online           |
| /estado_esp32       | Show current status of the ESP32    |
| /info_pc            | Request system info from the PC     |
| /logs_pc            | View latest PC logs                 |
| /actualizar_ota     | Trigger OTA update interface        |
| /reiniciar_esp32    | Restart the ESP32                   |
| /ayuda              | Show help menu                      |

## Notes

- Ensure the PC is configured to accept WOL packets and is on the same subnet.
- Telegram messages sent before the ESP32 boot time are ignored.
- For production use, consider encrypting the communication between ESP32 and the PC.

## License

This project is open-source and distributed under the MIT License.
