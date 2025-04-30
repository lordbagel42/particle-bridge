/******************************************************************************  
 *                            PARTICLE-MQTT BRIDGE                            *  
 *                                                                            *  
 * This firmware bridges the Particle Cloud and a local MQTT broker. It       *  
 * relays events between the two in both directions, while supporting         *  
 * Home Assistant MQTT discovery and availability.                            *  
 *                                                                            *  
 * ---------------------------- USAGE OVERVIEW -----------------------------  *  
 *                                                                            *  
 * 1. MQTT → PARTICLE (Commands from HA or user)                              *  
 *    - Command topics:                                                       *  
 *        homeassistant/bridge/device/<deviceID>/cmd/<component>             *  
 *        → Relays as Particle event:                                         *  
 *            mqttbridge/to_particle/<deviceID>/<component>                  *  
 *                                                                            *  
 * 2. PARTICLE → MQTT (Events to HA)                                          *  
 *    - Particle events published as:                                         *  
 *        mqttbridge/from_particle/<deviceID>/<component>                    *  
 *      → Relayed to MQTT:                                                    *  
 *        homeassistant/bridge/device/<deviceID>/<component>/state           *  
 *                                                                            *  
 * 3. DISCOVERY (Enabling auto-add in HA)                                     *  
 *    - Each new device or component triggers discovery messages:             *  
 *        homeassistant/<component>/mqttbridge_<deviceID>_<component>/config *  
 *    - Example (binary sensor):                                              *  
 *        homeassistant/binary_sensor/mqttbridge_device123_button/config     *  
 *                                                                            *  
 * 4. AVAILABILITY                                                            *  
 *    - Bridge publishes its own availability:                                *  
 *        homeassistant/bridge/availability                                  *  
 *    - Each device has its own availability topic:                           *  
 *        homeassistant/bridge/device/<deviceID>/availability                *  
 *                                                                            *  
 * 5. DYNAMIC SUBSCRIPTION                                                    *  
 *    - Particle devices can request subscriptions via this event:            *  
 *        Particle.publish("mqttbridge/register_device",                     *  
 *                         "{ \\\"deviceID\\\": \\\"abc\\\",                          *  
 *                            \\\"components\\\": [\\\"button\\\", \\\"sensor\\\"] }")      *  
 *                                                                            *  
 * 6. STATUS                                                                 *  
 *    - Publishes JSON object to MQTT every 30 seconds:                       *  
 *        homeassistant/bridge/status                                         *  
 *      → Includes registered devices, components, subscription counts        *  
 *                                                                            *  
 ******************************************************************************/  

// Include Particle Device OS APIs  
#include "Particle.h"  
#include "MQTT.h"  
#include "ArduinoJson.h"  
#include "environment.h"

SYSTEM_MODE(AUTOMATIC);  
SerialLogHandler logHandler;  

// Prototypes  
void mqttCallback(char *topic, byte *payload, unsigned int length);  
void mqttHandler(const char *event, const char *data);  
void mqttSubRelay(const char *event, const char *data);  
void mqttSubscribeHandler(const char *event, const char *data);  
void publishStatus();  
void subscribeInitialTopics();  
void addMqttSubscription(const String &topic);  
void handleCommand(const char *topic, const char *data); // New function to handle commands
void publishDiscoveryMessage(const String &deviceID, const String &component); // New function to publish discovery messages

// MQTT server settings  
MQTT client(MQTT_SERVER, MQTT_PORT, mqttCallback);

// Subscriptions  
Vector<String> mqttSubscribers;  
Vector<String> particleSubscribers;  

unsigned long lastStatusTime = 0;  
const unsigned long STATUS_INTERVAL_MS = 30000;  

// ---------------- MQTT Callback ----------------  
void mqttCallback(char *topic, byte *payload, unsigned int length)  
{  
  String topicStr = String(topic);  
  String payloadStr((char *)payload, length);  

  Log.info("MQTT message received on topic: %s | Payload: %s", topic, payloadStr.c_str());  

  if (topicStr.startsWith("mqttbridge/to_particle/"))  
  {  
    Particle.publish(topicStr, payloadStr, PRIVATE);  
  }  
  else if (topicStr.startsWith("homeassistant/bridge/device/"))  
  {  
    handleCommand(topicStr.c_str(), payloadStr.c_str()); // Handle commands  
  }  
  else  
  {  
    topicStr = "mqttbridge/to_particle/" + topicStr;  
    Particle.publish(topicStr, payloadStr, PRIVATE);  
  }  
}  

// ---------------- Setup ----------------  
void setup()  
{  
  waitUntil(Particle.connected);  
  Log.info("Particle connected");  

  if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS))  
  {  
    Log.info("MQTT connected");  

    subscribeInitialTopics();  

    publishStatus();  
  }  
  else  
  {  
    Log.error("MQTT connection failed");  
  }  
}  

// ---------------- Loop ----------------  
void loop()  
{  
  if (client.isConnected())  
  {  
    client.loop();  

    if (millis() - lastStatusTime > STATUS_INTERVAL_MS)  
    {  
      lastStatusTime = millis();  
      publishStatus();  
    }  
  }  
  else  
  {  
    Log.warn("MQTT disconnected, retrying...");  
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS))  
    {  
      for (String sub : mqttSubscribers)  
      {  
        client.subscribe(sub);  
      }  

      publishStatus();  
      Log.info("Reconnected to MQTT");  
    }  
    delay(1000);  
  }  
}  

// ---------------- Subscribe Initial Topics ----------------  
void subscribeInitialTopics()  
{  
  Particle.subscribe("mqttbridge/subscribe_particle", mqttHandler);  
  Particle.subscribe("mqttbridge/subscribe_mqtt", mqttSubscribeHandler);  
  Particle.subscribe("mqttbridge/register_device", mqttHandler); // New subscription for device registration  
}  

// ---------------- Add MQTT Subscription ----------------  
void addMqttSubscription(const String &topic)  
{  
  if (mqttSubscribers.indexOf(topic) == -1)  
  {  
    mqttSubscribers.append(topic);  
    client.subscribe(topic);  
    Log.info("Subscribed to MQTT topic: %s", topic.c_str());  
  }  
  else  
  {  
    Log.info("Already subscribed to MQTT topic: %s", topic.c_str());  
  }  
}  

// ---------------- Publish Status ----------------  
void publishStatus()  
{  
  StaticJsonDocument<768> doc;  

  JsonArray particleArray = doc.createNestedArray("particle_subscriptions");  
  for (String sub : particleSubscribers)  
  {  
    particleArray.add(sub);  
  }  

  JsonArray mqttArray = doc.createNestedArray("mqtt_subscriptions");  
  for (String sub : mqttSubscribers)  
  {  
    mqttArray.add(sub);  
  }  

  doc["particle_subscriptions_count"] = particleSubscribers.size();  
  doc["mqtt_subscriptions_count"] = mqttSubscribers.size();  

  char output[768];  
  serializeJson(doc, output);  
  client.publish("homeassistant/bridge/status", output); // Publish status on Home Assistant topic  
  Log.info("Published status: %s", output);  
}  

// ---------------- Handle Command ----------------  
void handleCommand(const char *topic, const char *data)  
{  
  String topicStr = String(topic);  
  String commandData = String(data);  

  // Extract deviceID and component from the topic  
  int deviceIDStart = topicStr.indexOf("/device/") + 8;  
  int deviceIDEnd = topicStr.indexOf("/cmd/");  
  String deviceID = topicStr.substring(deviceIDStart, deviceIDEnd);  

  int componentStart = topicStr.indexOf("/cmd/") + 5;  
  String component = topicStr.substring(componentStart);  

  Log.info("Command received for device %s, component %s: %s", deviceID.c_str(), component.c_str(), commandData.c_str());  

  // Relay the command to Particle event (e.g., mqttbridge/to_particle/<deviceID>/<component>)  
  String particleEvent = "mqttbridge/to_particle/" + deviceID + "/" + component;  
  Particle.publish(particleEvent.c_str(), commandData.c_str(), PRIVATE);  

  // Optionally, publish back to MQTT for confirmation  
  String confirmationTopic = "homeassistant/bridge/device/" + deviceID + "/" + component + "/ack";  
  client.publish(confirmationTopic.c_str(), commandData.c_str());  
}  

// Helper: Publish Home Assistant Discovery message for a component
void publishDiscoveryMessage(const String &deviceID, const String &component) {
  // Determine component type (e.g., binary_sensor, sensor, switch, etc.)
  // For this example, treat "button" as binary_sensor, others as sensor
  String haComponent = (component == "button") ? "binary_sensor" : "sensor";
  String unique_id = "mqttbridge_" + deviceID + "_" + component;
  String discoveryTopic = "homeassistant/" + haComponent + "/" + unique_id + "/config";

  StaticJsonDocument<512> doc;
  doc["name"] = component;
  doc["state_topic"] = "homeassistant/bridge/device/" + deviceID + "/" + component + "/state";
  doc["unique_id"] = unique_id;
  doc["availability_topic"] = "homeassistant/bridge/device/" + deviceID + "/availability";
  // Device object for device-based registration
  JsonObject deviceObj = doc.createNestedObject("device");
  deviceObj["identifiers"][0] = deviceID;
  deviceObj["manufacturer"] = "Particle";
  deviceObj["model"] = "Photon";
  deviceObj["name"] = "Particle Device " + deviceID;

  char payload[512];
  serializeJson(doc, payload);
  client.publish(discoveryTopic.c_str(), payload, true); // Retain discovery message
  Log.info("Published HA discovery: %s => %s", discoveryTopic.c_str(), payload);
}

// ---------------- Particle → MQTT Setup Handler ----------------  
void mqttHandler(const char *event, const char *data)  
{  
  Log.info("Setup request: %s", data ? data : "(null)");  
  if (!data) return;  

  StaticJsonDocument<200> doc;  
  DeserializationError error = deserializeJson(doc, data);  
  if (error)  
  {  
    Log.warn("JSON parsing failed: %s", error.c_str());  
    return;  
  }  

  // Device registration: { "deviceID": "abc", "components": ["button", "sensor"] }
  const char* deviceID = doc["deviceID"];
  JsonArray components = doc["components"];
  if (deviceID && components) {
    for (JsonVariant comp : components) {
      String component = comp.as<String>();
      publishDiscoveryMessage(deviceID, component);
    }
    // Publish device availability as online
    String availTopic = "homeassistant/bridge/device/" + String(deviceID) + "/availability";
    client.publish(availTopic.c_str(), "online", true);
    Log.info("Published device availability: %s => online", availTopic.c_str());
    return;
  }

  // No legacy topic support
  Log.warn("Invalid registration payload: missing deviceID or components");
}

// ---------------- Particle → MQTT Relay ----------------  
void mqttSubRelay(const char *event, const char *data)  
{  
  if (!event || !data)  
  {  
    Log.warn("Null event or data in mqttSubRelay");  
    return;  
  }  

  String topic = "mqttbridge/from_particle/" + String(event).substring(strlen("mqttbridge/from_particle/"));  
  client.publish(topic.c_str(), data);  
  Log.info("Relayed Particle event to MQTT: %s → %s", event, topic.c_str());  

  if (mqttSubscribers.indexOf(topic) == -1)  
  {  
    mqttSubscribers.append(topic);  
  }  
}  

// ---------------- MQTT Subscribe via Particle Event ----------------  
void mqttSubscribeHandler(const char *event, const char *data)  
{  
  Log.info("MQTT subscribe request: %s", data ? data : "(null)");  
  if (!data) return;  

  StaticJsonDocument<200> doc;  
  DeserializationError error = deserializeJson(doc, data);  
  if (error)  
  {  
    Log.warn("JSON parsing failed: %s", error.c_str());  
    return;  
  }  

  const char* topic = doc["topic"];  
  if (topic)  
  {  
    String topicStr = String(topic);  
    if (!topicStr.startsWith("mqttbridge/to_particle/"))  
    {  
      topicStr = "mqttbridge/to_particle/" + topicStr;  
    }  
    addMqttSubscription(topicStr);  
  }  
  else  
  {  
    Log.warn("No topic provided in MQTT subscribe request");  
  }  
}
