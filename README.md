# ESP32-to-Azure IoT Telemetry Gateway

An end-to-end IoT telemetry prototype built with **two Arduino Nano ESP32 boards**, a **Raspberry Pi running Mosquitto**, and **Azure IoT Hub**.

The project demonstrates sensor acquisition, Wi-Fi/MQTT communication, Python-based message processing, cloud forwarding, and verification of device-to-cloud telemetry.

Developed as an individual embedded systems and IoT coursework project in October 2026.

## Overview

The two ESP32 devices publish JSON-formatted measurements to a local MQTT broker. A Python gateway on the Raspberry Pi subscribes to the sensor topics, validates incoming messages, and forwards their payloads to Azure IoT Hub.

```text
Arduino Nano ESP32 #1 ─ DHT11 (ambient temperature + humidity) ─┐
                                                               │
Arduino Nano ESP32 #2 ─ Internal chip temperature sensor ───────┤
                                                               ▼
                                                     Wi-Fi / MQTT
                                                               │
                                                               ▼
                                               Mosquitto on Raspberry Pi
                                                               │
                                                               ▼
                                                 Python MQTT subscriber
                                               (JSON parsing + validation)
                                                               │
                                                               ▼
                                                  Azure IoT Hub gateway
                                                 (per-topic rate limiting)
                                                               │
                                                               ▼
                                                  Azure IoT Hub (cloud)
                                                               │
                                                               ▼
                                            Azure Cloud Shell event monitor
```

The Raspberry Pi connects to Azure as the registered device **`rpi-gateway`**. Each original ESP32 `deviceId` is preserved in the JSON payload, making it possible to distinguish the sources even though the gateway is the Azure device.

## Technology stack

| Layer | Technologies |
| --- | --- |
| Embedded hardware | 2 × Arduino Nano ESP32 (ESP32-S3), DHT11 |
| Firmware | C++, Arduino framework, PlatformIO |
| Local connectivity | Wi-Fi, MQTT, Mosquitto |
| Gateway | Raspberry Pi OS, Python 3, Paho MQTT |
| Cloud | Azure IoT Hub, `azure-iot-device` Python SDK |
| Testing | Serial Monitor, Mosquitto subscriber, Azure Cloud Shell |

## Sensor data and MQTT topics

| Device | MQTT topic | Measurement |
| --- | --- | --- |
| `esp32-01` | `sensors/esp32-01/temperature` | DHT11 ambient temperature (°C) |
| `esp32-01` | `sensors/esp32-01/humidity` | DHT11 relative humidity (%) |
| `esp32-02` | `sensors/esp32-02/chip-temperature` | ESP32-S3 chip temperature (°C) |

Example DHT11 temperature payload:

```json
{
  "deviceId": "esp32-01",
  "value": 27.6,
  "unit": "C"
}
```

Example chip-temperature payload:

```json
{
  "deviceId": "esp32-02",
  "value": 39.7,
  "unit": "C",
  "measurement": "chipTemperature"
}
```

All forwarded messages retain at least `deviceId`, `value`, and `unit`.

**Measurement note:** The second board measures its **internal chip temperature**, not ambient air temperature. This is a genuine hardware measurement, but it should not be interpreted as a calibrated room-temperature reading. Acceptance of this sensor choice for the coursework is subject to instructor confirmation.

## Repository structure

```text
.
├── esp32-01/
│   ├── include/secrets.example.h
│   ├── platformio.ini
│   └── src/main.cpp
├── esp32-02/
│   ├── include/secrets.example.h
│   ├── platformio.ini
│   └── src/main.cpp
├── raspberry-pi/
│   ├── gateway.py
│   └── requirements.txt
├── .gitignore
└── README.md
```

The ESP32 firmware projects are kept separate for independent builds and uploads. The Raspberry Pi gateway can handle messages from both boards using a single MQTT topic filter: `sensors/+/+`.

## Getting started

### 1. ESP32 firmware (PlatformIO)

Open either `esp32-01/` or `esp32-02/` as a PlatformIO project.

1. Copy `include/secrets.example.h` to `include/secrets.h`.
2. Enter your **local** Wi-Fi and MQTT credentials in `secrets.h`.
3. Update `mqtt_server` in `src/main.cpp` if your broker does not use the example LAN address `192.168.1.73`.
4. Select the `arduino_nano_esp32` environment and run **Build**, then **Upload** to the corresponding board.
5. Open Serial Monitor at **115200 baud** to inspect sensor readings and MQTT publishing.

The PlatformIO configuration includes the following USB build flags, which were used to maintain the expected USB serial behavior on these boards:

```ini
build_flags =
    -DARDUINO_USB_MODE=0
    -DARDUINO_USB_CDC_ON_BOOT=1
```

If required, double-press the board's reset button to enter DFU upload mode, then reconnect USB after uploading.

**ESP32 #1 DHT11 wiring:** signal → `D2`, power → `3.3V`, ground → `GND`.

### 2. Raspberry Pi and Mosquitto

The gateway assumes Mosquitto is running on the Raspberry Pi, listening on **TCP port 1883** with MQTT username/password authentication enabled.

The ESP32 boards connect to the Pi's LAN address. The Python gateway connects to the same broker through `localhost:1883`.

### 3. Python gateway

On the Raspberry Pi, inside the `raspberry-pi/` directory:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

The gateway reads credentials from a **local file outside this repository**:

`~/.config/iot-gateway/credentials.json`

The file must contain the following keys (replace placeholders locally; **never commit this file**):

```json
{
  "MQTT_PASSWORD": "YOUR_MQTT_PASSWORD",
  "AZURE_IOT_CONNECTION_STRING": "YOUR_DEVICE_CONNECTION_STRING"
}
```

The Azure connection string must belong to the registered **`rpi-gateway` device identity**, not an IoT Hub-wide shared access policy.

Restrict access to the configuration directory and file:

```bash
chmod 700 ~/.config/iot-gateway
chmod 600 ~/.config/iot-gateway/credentials.json
```

Start the gateway:

```bash
python gateway.py
```

### 4. Azure IoT Hub

The prototype was verified using **Azure IoT Hub Free (F1)**. Register a device with ID `rpi-gateway`, enable it, and configure the gateway using that device's connection string.

The Python gateway forwards **at most one message per 60 seconds per MQTT topic**. With the three current topics, that is a theoretical maximum of **4,320 forwarded messages per 24 hours**, below the 8,000-messages-per-day F1 quota under normal operation.

## Validation and results

The following behavior was observed during end-to-end testing on **20 October 2026**:

- Both ESP32 boards published MQTT messages simultaneously to Mosquitto.
- The Python gateway received all three MQTT topics and parsed their JSON payloads.
- The gateway successfully invoked the Azure IoT Hub SDK to forward measurements.
- Azure Cloud Shell independently displayed messages from **both** `esp32-01` and `esp32-02`, preserving the original `deviceId` and measured values.
- The 60-second per-topic forwarding interval was observed in operation.

Example verification command in Azure Cloud Shell:

```bash
az iot hub monitor-events \
  --hub-name iot25-telemetry-7392 \
  --device-id rpi-gateway \
  --message-count 3
```

The Azure event's `origin` is `rpi-gateway` because the Raspberry Pi owns the Azure device connection. The originating ESP32 is identified by `deviceId` inside the event payload.

## Security considerations and limitations

- Real Wi-Fi, MQTT, and Azure credentials are **not stored in the repository**. `secrets.h`, `credentials.json`, and common secret-file formats are excluded using `.gitignore`.
- The local MQTT broker currently uses **unencrypted TCP/1883** on the trusted LAN. Communication with Azure IoT Hub uses TLS.
- The gateway handles malformed JSON and checks for the required fields, but does **not** perform comprehensive JSON type/schema validation.
- The gateway is currently started manually; an automatic `systemd` service has not been configured.
- More extensive connection recovery, telemetry buffering, stronger credential management, and integration tests would be appropriate for a production deployment.

## Potential improvements

Future iterations could add mutual TLS for local MQTT, independent MQTT identities and permissions for each device, persistent gateway operation, offline buffering, and automated testing.

---

**Project scope:** A working educational proof of concept for end-to-end embedded IoT telemetry—not a production-ready monitoring service.
