# IoT25 – ESP32, MQTT och Azure IoT Hub

Individuell IoT-uppgift: trådlös telemetri från **två Arduino Nano ESP32** via Wi-Fi och **Mosquitto** på Raspberry Pi, vidarebefordrad med en **Python-gateway** till **Azure IoT Hub**.

## Arkitektur

```text
ESP32 #1 + DHT11 (temperatur, luftfuktighet) ─┐
                                              ├─ Wi-Fi / MQTT ─► Mosquitto (Raspberry Pi)
ESP32 #2 + intern chiptemperatursensor ────────┘                       │
                                                                     ▼
                                                          Python-gateway (Paho MQTT)
                                                                     │
                                                                     ▼
                                                             Azure IoT Hub
                                                                     │
                                                                     ▼
                                                         Azure Cloud Shell (monitor)
```

Raspberry Pi är registrerad i Azure som enheten **`rpi-gateway`**. Meddelandenas ursprungliga `deviceId` från respektive ESP32 finns kvar i JSON-payloaden.

## Hårdvara

- 2 × Arduino Nano ESP32 (ESP32-S3)
- 1 × DHT11, ansluten till ESP32 #1: signal på **D2**, 3,3 V och GND
- Raspberry Pi med Raspberry Pi OS och Mosquitto MQTT-broker
- Wi-Fi-nätverk

ESP32 #2 använder ESP32-S3:s **interna chiptemperatursensor**, inte en extern omgivningstemperatursensor. Mätningen avser därför chipets temperatur. **Lärarens godkännande för detta sensorval behöver bekräftas.**

## MQTT-topics och JSON

| Källa | Topic | Mätvärde |
| --- | --- | --- |
| ESP32 #1 | `sensors/esp32-01/temperature` | Temperatur, °C |
| ESP32 #1 | `sensors/esp32-01/humidity` | Luftfuktighet, % |
| ESP32 #2 | `sensors/esp32-02/chip-temperature` | Chiptemperatur, °C |

Exempel från ESP32 #1:

```json
{"deviceId":"esp32-01","value":27.6,"unit":"C"}
```

Exempel från ESP32 #2:

```json
{"deviceId":"esp32-02","value":39.7,"unit":"C","measurement":"chipTemperature"}
```

Minimikraven `deviceId`, `value` och `unit` bevaras vid vidarebefordran till Azure.

## Projektstruktur

```text
esp32-01/
  platformio.ini
  include/secrets.example.h
  src/main.cpp
esp32-02/
  platformio.ini
  include/secrets.example.h
  src/main.cpp
raspberry-pi/
  gateway.py
  requirements.txt
.gitignore
README.md
```

## Konfiguration och start

### ESP32 – PlatformIO

Öppna mappen `esp32-01` eller `esp32-02` som ett separat PlatformIO-projekt.

1. Kopiera `include/secrets.example.h` till `include/secrets.h`.
2. Fyll i Wi-Fi- och MQTT-inloggningsuppgifter lokalt i `secrets.h`.
3. Kör **Build** och därefter **Upload** på rätt ESP32-kort.
4. Öppna Serial Monitor med **115200 baud**.

För Arduino Nano ESP32 används USB-byggflaggorna `ARDUINO_USB_MODE=0` och `ARDUINO_USB_CDC_ON_BOOT=1`. Vid behov: dubbeltryck RST för DFU-läge och anslut USB-kabeln på nytt efter uppladdning.

### Raspberry Pi – Python-gateway

Mosquitto måste köras på Raspberry Pi och ta emot MQTT på port 1883 med autentisering. Gatewayen ansluter till `localhost:1883`, prenumererar på `sensors/+/+` och vidarebefordrar JSON till Azure IoT Hub.

Installera beroendena i en virtuell miljö (exempel):

```bash
cd raspberry-pi
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

Gatewayen läser inloggningsuppgifter från följande **lokala** fil på Raspberry Pi (ska aldrig läggas på GitHub):

`~/.config/iot-gateway/credentials.json`

Filens JSON-nycklar ska vara `MQTT_PASSWORD` och `AZURE_IOT_CONNECTION_STRING`. Den senare ska vara **enhetens** Primary connection string för Azure-enheten `rpi-gateway`, inte en övergripande IoT Hub-policysträng.

Rekommenderade filbehörigheter:

```bash
chmod 700 ~/.config/iot-gateway
chmod 600 ~/.config/iot-gateway/credentials.json
```

Starta sedan från projektets `raspberry-pi`-mapp:

```bash
source .venv/bin/activate
python gateway.py
```

Gatewayen vidarebefordrar högst ett meddelande var 60:e sekund **per MQTT-topic**, för att hålla nere trafiken till Azure IoT Hub Free (F1, 8 000 meddelanden/dygn).

## Test och verifiering

Följande har testats den **20 oktober 2026**:

- Två ESP32-enheter publicerar samtidigt via MQTT.
- Mosquitto tar emot båda enheternas unika topics.
- Python-gatewayen läser JSON, kontrollerar obligatoriska fält och skickar vidare till Azure.
- Azure Cloud Shell tog emot tre meddelanden och visade både `esp32-01` och `esp32-02`.

Exempelkommando för att övervaka nya händelser från Raspberry Pi-enheten i Azure Cloud Shell:

```bash
az iot hub monitor-events --hub-name iot25-telemetry-7392 --device-id rpi-gateway --message-count 3
```

Notera att `origin` i Azure är `rpi-gateway`, medan ursprunglig ESP32-identitet finns i `payload.deviceId`.

## Säkerhet och begränsningar

- **Publicera aldrig** `secrets.h`, `credentials.json`, Wi-Fi-lösenord, MQTT-lösenord eller Azure-anslutningssträngar.
- MQTT på det lokala nätverket använder för närvarande port 1883 utan TLS; Azure-kommunikationen använder TLS.
- Projektet är en kursprototyp. Automatisk uppstart via systemd, avancerad återanslutningshantering och fullständig validering av alla JSON-datatyper är inte implementerade.
