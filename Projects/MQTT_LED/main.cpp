#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Public HiveMQ test broker.
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

const char* TOPIC_OUT =
    "testtopic/temp/outTopic/voltage";

const char* TOPIC_LED_COMMAND = "testtopic/temp/inTopic";

const char* TOPIC_PEER =
    "testtopic/temp/outTopic/FRIEND_UNIQUE_ID";

const char* PEER_PLACEHOLDER = "FRIEND_UNIQUE_ID";

// NodeMCU pins.
const uint8_t POT_PIN = A0;
const uint8_t BUTTON_PIN = D5;  // Button between D5 and GND
const uint8_t LED_PIN = D6;     // External LED with 220–330 ohm resistor

const unsigned long POT_INTERVAL_MS = 30000;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long SWITCH_PULSE_MS = 5000;
const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long MQTT_RETRY_MS = 5000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

char mqttClientId[32];

unsigned long lastPotReadMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastButtonChangeMs = 0;
unsigned long switchOffDeadlineMs = 0;

bool firstPotRead = true;
bool wifiWasConnected = false;
bool mqttAttempted = false;

bool lastRawButtonState = HIGH;
bool stableButtonState = HIGH;

bool ledIsOn = false;
bool ledStatePending = true;

bool switchPulseActive = false;
bool switchOnPending = false;
bool switchOffPending = false;

bool peerTopicIsConfigured()
{
    return TOPIC_PEER[0] != '\0' &&
           strstr(TOPIC_PEER, PEER_PLACEHOLDER) == nullptr;
}

bool publishOutput(const char* payload, bool retain = false)
{
    if (!mqttClient.connected()) {
        return false;
    }

    const bool published = mqttClient.publish(TOPIC_OUT, payload, retain);

    if (published) {
        Serial.print("[MQTT] Published to ");
        Serial.print(TOPIC_OUT);
        Serial.print(": ");
        Serial.println(payload);
    } else {
        Serial.println("[MQTT] Publish failed.");
    }

    return published;
}

void setLed(bool turnOn)
{
    ledIsOn = turnOn;
    digitalWrite(LED_PIN, ledIsOn ? HIGH : LOW);
    ledStatePending = true;

    Serial.print("[LED] ");
    Serial.println(ledIsOn ? "ON" : "OFF");
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    char message[64];

    if (length >= sizeof(message)) {
        return;
    }

    memcpy(message, payload, length);
    message[length] = '\0';

    // Commands from the app must be exactly "1" or "0".
    if (strcmp(topic, TOPIC_LED_COMMAND) == 0) {
        if (strcmp(message, "1") == 0) {
            setLed(true);
        } else if (strcmp(message, "0") == 0) {
            setLed(false);
        }
        return;
    }

    if (peerTopicIsConfigured() && strcmp(topic, TOPIC_PEER) == 0) {
        if (strcmp(message, "SWITCH,1") == 0) {
            setLed(true);
        } else if (strcmp(message, "SWITCH,0") == 0) {
            setLed(false);
        }
    }
}

void connectToMqtt(unsigned long now)
{
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) {
        return;
    }

    if (mqttAttempted && now - lastMqttAttemptMs < MQTT_RETRY_MS) {
        return;
    }

    mqttAttempted = true;
    lastMqttAttemptMs = now;

    Serial.print("[MQTT] Connecting to ");
    Serial.print(MQTT_HOST);
    Serial.print("... ");

    if (!mqttClient.connect(mqttClientId)) {
        Serial.print("failed, state=");
        Serial.println(mqttClient.state());
        return;
    }

    Serial.println("connected.");

    if (mqttClient.subscribe(TOPIC_LED_COMMAND)) {
        Serial.print("[MQTT] Subscribed to LED command topic: ");
        Serial.println(TOPIC_LED_COMMAND);
    }

    if (peerTopicIsConfigured()) {
        if (mqttClient.subscribe(TOPIC_PEER)) {
            Serial.print("[MQTT] Subscribed to friend's topic: ");
            Serial.println(TOPIC_PEER);
        }
    } else {
        Serial.println(
            "[MQTT] Set TOPIC_PEER to your friend's topic for part E.");
    }

    // Publish the current LED state after every MQTT reconnection.
    ledStatePending = true;
}

void maintainWiFi(unsigned long now)
{
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            Serial.print("[WiFi] Connected. IP: ");
            Serial.println(WiFi.localIP());
        }
        return;
    }

    wifiWasConnected = false;

    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }

    if (now - lastWiFiAttemptMs >= WIFI_RETRY_MS) {
        lastWiFiAttemptMs = now;
        Serial.println("[WiFi] Reconnecting...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

void readSerialCommands()
{
    while (Serial.available() > 0) {
        const char command = static_cast<char>(Serial.read());

        if (command == '1') {
            setLed(true);
        } else if (command == '0') {
            setLed(false);
        }
    }
}

void readButton(unsigned long now)
{
    const bool rawState = digitalRead(BUTTON_PIN);

    if (rawState != lastRawButtonState) {
        lastRawButtonState = rawState;
        lastButtonChangeMs = now;
    }

    if (now - lastButtonChangeMs >= DEBOUNCE_MS &&
        rawState != stableButtonState) {
        stableButtonState = rawState;

        // INPUT_PULLUP means LOW is pressed.
        if (stableButtonState == LOW) {
            Serial.println("[Button] Press detected; sending SWITCH,1.");

            switchOnPending = true;
            switchOffPending = false;
            switchPulseActive = true;
            switchOffDeadlineMs = now + SWITCH_PULSE_MS;
        }
    }

    if (switchPulseActive &&
        static_cast<int32_t>(now - switchOffDeadlineMs) >= 0) {
        switchPulseActive = false;
        switchOffPending = true;
        Serial.println("[Button] Five seconds elapsed; sending SWITCH,0.");
    }
}

void sendPendingButtonMessages()
{
    if (!mqttClient.connected()) {
        return;
    }

    if (switchOnPending && publishOutput("SWITCH,1")) {
        switchOnPending = false;
    }

    if (switchOffPending && !switchPulseActive &&
        publishOutput("SWITCH,0")) {
        switchOffPending = false;
    }
}

void sendPendingLedState()
{
    if (!mqttClient.connected() || !ledStatePending) {
        return;
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "LED,%d", ledIsOn ? 1 : 0);

    // Retain the latest LED state for dashboard subscribers.
    if (publishOutput(payload, true)) {
        ledStatePending = false;
    }
}

void readAndPublishPotentiometer(unsigned long now)
{
    if (!firstPotRead && now - lastPotReadMs < POT_INTERVAL_MS) {
        return;
    }

    firstPotRead = false;
    lastPotReadMs = now;

    const int rawValue = analogRead(POT_PIN);

    // For a NodeMCU board with a 0–3.3 V A0 range.
    const float voltage = rawValue * (3.3f / 1023.0f);

    char voltageText[12];
    dtostrf(voltage, 1, 3, voltageText);

    char payload[40];
    snprintf(payload, sizeof(payload), "POT,%d,%s", rawValue, voltageText);

    Serial.print("[Potentiometer] ADC=");
    Serial.print(rawValue);
    Serial.print(", voltage=");
    Serial.print(voltage, 3);
    Serial.println(" V");

    if (mqttClient.connected()) {
        publishOutput(payload);
    } else {
        Serial.println("[MQTT] Offline; this potentiometer sample was not sent.");
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    snprintf(mqttClientId, sizeof(mqttClientId),
             "ESP8266-%06lX",
             static_cast<unsigned long>(ESP.getChipId()));

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);

    Serial.println();
    Serial.println("[Setup] MQTT ESP project starting.");
    Serial.print("[Setup] One outgoing topic: ");
    Serial.println(TOPIC_OUT);
    Serial.print("[Setup] LED command topic: ");
    Serial.println(TOPIC_LED_COMMAND);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWiFiAttemptMs = millis();

    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);
}

void loop()
{
    const unsigned long now = millis();

    maintainWiFi(now);

    if (WiFi.status() == WL_CONNECTED) {
        connectToMqtt(now);

        if (mqttClient.connected()) {
            mqttClient.loop();
        }
    }

    readSerialCommands();
    readButton(now);
    readAndPublishPotentiometer(now);
    sendPendingButtonMessages();
    sendPendingLedState();
}
