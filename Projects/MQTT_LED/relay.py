#!/usr/bin/env python3
"""Laptop MQTT relay for the ESP8266 potentiometer project.

Expected ESP messages on MQTT_TOPIC:
    POT,<raw ADC count>,<voltage>
    SWITCH,1
    SWITCH,0
    LED,1
    LED,0

The relay forwards only POT messages to the existing Base64 GET PHP insert file.
It stores the potentiometer voltage as node_3's sensor_data.temperature value
and sends 0.00 for the required humidity column.
"""

import base64
import os
import re
import socket
from datetime import datetime
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen
from decimal import Decimal, InvalidOperation

import paho.mqtt.client as mqtt


BROKER_HOST = os.getenv("MQTT_HOST", "broker.hivemq.com")
BROKER_PORT = int(os.getenv("MQTT_PORT", "1883"))

# Must match TOPIC_OUT in the ESP sketch.
MQTT_TOPIC = os.getenv(
    "MQTT_TOPIC",
    "testtopic/temp/outTopic/YOUR_UNIQUE_ID",
)

# Full URL to the existing PHP insert file.
PHP_INSERT_URL = os.getenv("PHP_INSERT_URL", "")
PHP_NODE_ID = os.getenv("PHP_NODE_ID", "node_3")


def check_configuration():
    if "YOUR_UNIQUE_ID" in MQTT_TOPIC:
        raise SystemExit(
            "Set MQTT_TOPIC to the exact outgoing topic used by the ESP."
        )

    if not PHP_INSERT_URL.startswith(("https://", "http://")):
        raise SystemExit(
            "Set PHP_INSERT_URL to the full URL of your PHP insert file."
        )

    if not re.fullmatch(r"node_[0-9]+", PHP_NODE_ID):
        raise SystemExit("PHP_NODE_ID must look like node_3.")

    if PHP_NODE_ID != "node_3":
        print(
            f"[Config] Current node is {PHP_NODE_ID}; "
            "the requested database node is node_3."
        )


def parse_pot_payload(payload_text):
    """Return (raw_count, voltage) for POT messages; None for other types."""
    fields = payload_text.strip().split(",")

    if not fields or fields[0] != "POT":
        return None

    if len(fields) != 3:
        raise ValueError("Expected POT,<raw ADC count>,<voltage>.")

    try:
        raw_count = int(fields[1])
        voltage = Decimal(fields[2])
    except (ValueError, InvalidOperation) as exc:
        raise ValueError("ADC count or voltage is not numeric.") from exc

    if not 0 <= raw_count <= 1023:
        raise ValueError(f"ADC count is out of range: {raw_count}")

    if not voltage.is_finite() or not Decimal("0") <= voltage <= Decimal("3.3"):
        raise ValueError(f"Voltage is out of range: {voltage}")

    return raw_count, voltage


def send_to_php(raw_count, voltage):
    """Send fields expected by the existing sensor_data_insert.php endpoint."""
    received_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # The endpoint expects these exact decoded field names.
    decoded_query = urlencode(
        {
            "nodeId": PHP_NODE_ID,
            "nodeTemp": format(voltage, ".3f"),
            "nodeHumidity": "0.00",
            "timeReceived": received_time,
        }
    )

    base64_message = base64.b64encode(
        decoded_query.encode("utf-8")
    ).decode("ascii")

    # PHP expects Base64 as the entire query string.
    request_url = PHP_INSERT_URL.rstrip("?") + "?" + quote(base64_message, safe="")
    request = Request(
        request_url,
        headers={"User-Agent": "Laptop-MQTT-Relay/1.0"},
        method="GET",
    )

    print(
        f"[Relay] Sending nodeId={PHP_NODE_ID}, "
        f"nodeTemp={voltage:.3f}, nodeHumidity=0.00, "
        f"timeReceived={received_time}"
    )

    expected_response = (
        f"Accepted measurement for {PHP_NODE_ID} at {received_time}."
    )

    try:
        with urlopen(request, timeout=15) as response:
            response_text = response.read().decode(
                "utf-8", errors="replace"
            ).strip()

            print(f"[PHP] HTTP {response.status}: {response_text}")

            if response.status == 201 and response_text == expected_response:
                print(
                    f"[Relay] Database insert confirmed for {PHP_NODE_ID}: "
                    f"{voltage} V (raw ADC={raw_count})."
                )
                return True

            print(
                "[Relay] Database insert NOT confirmed. "
                "Expected HTTP 201 and the PHP 'Accepted measurement' message."
            )
            return False

    except HTTPError as exc:
        response_text = exc.read().decode("utf-8", errors="replace").strip()
        print(f"[PHP] HTTP {exc.code}: {response_text}")
        print("[Relay] Database insert was not confirmed.")
        return False

    except URLError as exc:
        print(f"[PHP] Request failed: {exc.reason}")
        return False

    except TimeoutError:
        print("[PHP] Request timed out; database insert was not confirmed.")
        return False


def on_connect(client, userdata, flags, reason_code, properties):
    """Subscribe again after each successful MQTT connection."""
    if reason_code == 0:
        print(f"[MQTT] Connected to {BROKER_HOST}:{BROKER_PORT}")
        result, _ = client.subscribe(MQTT_TOPIC, qos=0)

        if result == mqtt.MQTT_ERR_SUCCESS:
            print(f"[MQTT] Subscribed to {MQTT_TOPIC}")
        else:
            print(f"[MQTT] Subscribe failed with code {result}")
    else:
        print(f"[MQTT] Broker rejected the connection: {reason_code}")


def on_message(client, userdata, message):
    """Display all messages; forward only potentiometer readings to PHP."""
    payload_text = message.payload.decode("utf-8", errors="replace").strip()
    print(f"[MQTT] {message.topic}: {payload_text}")

    try:
        reading = parse_pot_payload(payload_text)
    except ValueError as exc:
        print(f"[Relay] Invalid potentiometer message: {exc}")
        return

    if reading is None:
        print("[Relay] Non-potentiometer message; not sent to PHP.")
        return

    raw_count, voltage = reading
    send_to_php(raw_count, voltage)


def make_client_id():
    computer = re.sub(r"[^A-Za-z0-9_-]", "-", socket.gethostname())[:24]
    return f"laptop-relay-{computer}"


def main():
    check_configuration()

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=make_client_id(),
        protocol=mqtt.MQTTv311,
        transport="tcp",
    )
    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    print(f"[Relay] MQTT broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"[Relay] MQTT topic:  {MQTT_TOPIC}")
    print(f"[Relay] PHP endpoint: {PHP_INSERT_URL}")
    print(f"[Relay] Database node: {PHP_NODE_ID}")
    print("[Relay] Press Ctrl+C to stop.")

    try:
        client.connect_async(BROKER_HOST, BROKER_PORT, keepalive=60)
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        print("\n[Relay] Stopping.")
    finally:
        try:
            client.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    main()
