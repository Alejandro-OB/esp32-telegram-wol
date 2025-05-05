# Flask Server for PC Control

This folder contains the backend server that runs on the PC to allow remote control via Telegram bot using an ESP32.

## Requirements

- Python 3.x
- Flask
- psutil

Install dependencies with:

```bash
pip install flask psutil
```

## Server Script

### `pc_control_server.py`
This script runs a Flask HTTP server that exposes routes to:

- Shutdown the PC
- Restart the PC
- Return status and system info
- Return logs

You can test the endpoints locally using curl:

```bash
curl http://localhost:5000/info
curl http://localhost:5000/apagar?token=YOUR_SECRET_TOKEN
```

> Replace `YOUR_SECRET_TOKEN` with the token expected by the ESP32 bot.

## Systemd Services

### `pc_control_server.service`
This service ensures the Flask server runs automatically on system startup.

### `wol.service`
This service ensures Wake-on-LAN is enabled on the network interface at boot time (useful if BIOS doesn't enable it by default).

> Make sure to replace `enp3s0` with the name of your actual network interface. You can check it using:
```bash
ip link
```

## Deployment

To enable the services:

```bash
sudo cp pc_control_server.service /etc/systemd/system/
sudo cp wol.service /etc/systemd/system/
sudo systemctl daemon-reexec
sudo systemctl enable pc_control_server
sudo systemctl start pc_control_server
sudo systemctl enable wol
sudo systemctl start wol
```

Ensure the path to the Python script is correct in `pc_control_server.service`.
