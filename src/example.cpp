#include "Particle.h"
#include "ArduinoJson.h"

SYSTEM_MODE(AUTOMATIC);
SerialLogHandler logHandler;

const int BUTTON_PIN = D1;
const int LED_PIN = D2;

bool lastButtonState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long debounceDelay = 50;

const String deviceID = "buttonclient-01";

void handleLedCommand(const char *event, const char *data);

// ----------------- Setup -----------------
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // LED off by default

  waitUntil(Particle.connected);

  // Register Particle subscription to receive MQTT commands
  String subEvent = "mqttbridge/to_particle/" + deviceID + "/led1";
  Particle.subscribe(subEvent, handleLedCommand);

  // Tell the bridge to subscribe to that MQTT topic
  StaticJsonDocument<128> subDoc;
  subDoc["topic"] = deviceID + "/led1";
  char subPayload[128];
  serializeJson(subDoc, subPayload);
  Particle.publish("mqttbridge/subscribe_mqtt", subPayload, PRIVATE);

  // Publish Home Assistant discovery info
  StaticJsonDocument<768> doc;

  // Device info
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = deviceID;
  dev["name"] = "Test Button + LED";
  dev["mf"] = "SylvieCorp";
  dev["mdl"] = "BTNLED1";
  dev["sw"] = "1.0.0";
  dev["sn"] = "001";
  dev["hw"] = "v1";

  JsonObject o = doc.createNestedObject("o");
  o["name"] = "Sylvie";
  o["sw"] = "1.0";
  o["url"] = "https://example.com";

  // Components
  JsonObject cmps = doc.createNestedObject("cmps");

  JsonObject button = cmps.createNestedObject("button1");
  button["name"] = "D1 Button";
  button["type"] = "binary_sensor";
  button["device_class"] = "motion";

  JsonObject led = cmps.createNestedObject("led1");
  led["name"] = "D2 LED";
  led["type"] = "switch";
  led["command_topic"] = "homeassistant/bridge/device/" + deviceID + "/cmd/led1";
  led["state_topic"] = "mqttbridge/from_particle/" + deviceID + "/led1";
  led["payload_on"] = "on";
  led["payload_off"] = "off";

  doc["state_topic"] = "mqttbridge/from_particle/" + deviceID + "/button1";
  doc["qos"] = 0;

  char payload[768];
  serializeJson(doc, payload);
  Particle.publish("mqttbridge/register_device", payload, PRIVATE);
}

// ----------------- Loop -----------------
void loop() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      // Button pressed
      Particle.publish("mqttbridge/from_particle/" + deviceID + "/button1", "1", PRIVATE);
      Log.info("Button pressed");
    }
  }

  lastButtonState = reading;
}

// ------------- Handle LED Command -------------
void handleLedCommand(const char *event, const char *data) {
  String cmd = String(data).toLowerCase();
  Log.info("LED command received: %s", cmd.c_str());

  if (cmd == "on") {
    digitalWrite(LED_PIN, HIGH);
    Particle.publish("mqttbridge/from_particle/" + deviceID + "/led1", "on", PRIVATE);
  } else if (cmd == "off") {
    digitalWrite(LED_PIN, LOW);
    Particle.publish("mqttbridge/from_particle/" + deviceID + "/led1", "off", PRIVATE);
  } else {
    Log.warn("Unknown LED command: %s", cmd.c_str());
  }
}
