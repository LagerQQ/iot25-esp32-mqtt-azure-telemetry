import os
import time
import json
import paho.mqtt.client as mqtt
from azure.iot.device import IoTHubDeviceClient

last_sent_at = {}

def on_connect(client, userdata, flags, reason_code):
    print("MQTT connection result:", reason_code)
    if reason_code == 0:
        client.subscribe("sensors/+/+")

def on_message(client, userdata, msg):
    try:
        sensor_data = json.loads(msg.payload.decode("utf-8"))
    except json.JSONDecodeError:
        print("Invalid JSON")
        return

    if "deviceId" not in sensor_data:
        print("Missing deviceId")
        return
    if "value" not in sensor_data:
        print("Missing value")
        return
    if "unit" not in sensor_data:
        print("Missing unit")
        return

    current_time = time.monotonic()
    last_sent_time = last_sent_at.get(msg.topic)

    if last_sent_time is not None and current_time - last_sent_time < 60:
        return

    azure_payload = json.dumps(sensor_data)
    try:
        azure_client.send_message(azure_payload)
        last_sent_at[msg.topic] = current_time
        print("Sent to Azure:", msg.topic)
    except Exception as error:
        print("Azure send failed:", error)

    print("Received topic:", msg.topic)
    print("Received payload:", sensor_data)
    print("Sensor value:", sensor_data["value"])
    print("Device:", sensor_data["deviceId"])
    print("Unit:", sensor_data["unit"])

gateway_client = mqtt.Client(client_id="rpi-gateway")
print("IoT Gateway starting...")
gateway_client.on_connect = on_connect
gateway_client.on_message = on_message

credentials_path = os.path.expanduser("~/.config/iot-gateway/credentials.json")
with open(credentials_path, "r", encoding="utf-8") as credentials_file:
    credentials = json.load(credentials_file)

mqtt_password = credentials["MQTT_PASSWORD"]
print("MQTT password configured:", bool(mqtt_password))

azure_connection_string = credentials["AZURE_IOT_CONNECTION_STRING"]
print("Azure connection string configured:", bool(azure_connection_string))

azure_client = IoTHubDeviceClient.create_from_connection_string(azure_connection_string)
azure_client.connect()
print("Azure IoT Hub connected!")

gateway_client.username_pw_set("esp32_client", mqtt_password)
gateway_client.connect(host="localhost", port=1883)
gateway_client.loop_forever()
