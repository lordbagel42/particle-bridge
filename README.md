# Particle-MQTT Bridge

This bridge firmware connects the Particle Cloud to a local MQTT broker, relaying events in both directions and supporting Home Assistant MQTT Discovery and availability. It enables seamless integration of Particle devices with Home Assistant and other MQTT consumers.

---

## Features
- **Bidirectional relay**: Forwards events from Particle to MQTT and vice versa.
- **Home Assistant Discovery**: Publishes discovery messages for auto-adding devices/components.
- **Availability topics**: Publishes online/offline status for the bridge and each device.
- **Dynamic registration**: Devices/components can register themselves at runtime.
- **Status reporting**: Publishes a JSON status object every 30 seconds.

---

## Topic Structure

### MQTT → Particle (Commands)
- **Command topics:**
  - `homeassistant/bridge/device/<deviceID>/cmd/<component>`
- **Relayed as Particle event:**
  - `mqttbridge/to_particle/<deviceID>/<component>`
- **Acknowledgment:**
  - Bridge publishes to `homeassistant/bridge/device/<deviceID>/<component>/ack` after relaying.

### Particle → MQTT (Events)
- **Particle event:**
  - `mqttbridge/from_particle/<deviceID>/<component>`
- **Relayed to MQTT:**
  - `homeassistant/bridge/device/<deviceID>/<component>/state`

### Home Assistant Discovery
- **Discovery topic:**
  - `homeassistant/<component>/mqttbridge_<deviceID>_<component>/config`
- **Example:**
  - `homeassistant/binary_sensor/mqttbridge_device123_button/config`

### Availability
- **Bridge:**
  - `homeassistant/bridge/availability`
- **Device:**
  - `homeassistant/bridge/device/<deviceID>/availability`

### Status
- **Status topic:**
  - `homeassistant/bridge/status`
- **Payload:**
  ```json
  {
    "particle_subscriptions": ["mqttbridge/to_particle/device123/button"],
    "mqtt_subscriptions": ["homeassistant/bridge/device/device123/cmd/button"],
    "particle_subscriptions_count": 1,
    "mqtt_subscriptions_count": 1
  }
  ```

---

## Device Registration & Dynamic Subscription

### Registering a Device/Components
A Particle device can register itself and its components by publishing to:
- **Event:** `mqttbridge/register_device`
- **Payload Example:**
  ```json
  {
    "deviceID": "device123",
    "components": ["button", "sensor"]
  }
  ```

### Subscribing to MQTT Topics from Particle
- **Event:** `mqttbridge/subscribe_mqtt`
- **Payload:**
  ```json
  { "topic": "homeassistant/bridge/device/device123/cmd/button" }
  ```

---

## Environment Configuration

This project uses an `environment.cpp` file to store sensitive or environment-specific variables (such as MQTT credentials and broker address).

- **To configure:**
  1. Copy `src/environment.cpp.example` to `src/environment.cpp`.
  2. Edit the values in `environment.cpp` to match your environment (MQTT broker IP, port, username, password, client ID).

**Do not commit your real `environment.cpp` to public repositories.**

---

## Example: Particle Device Firmware

Below is a minimal example for a Particle device to register itself and relay button events to the bridge:

```cpp
#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);

const char* deviceID = System.deviceID();
const char* component = "button";

void registerDevice() {
  char payload[128];
  snprintf(payload, sizeof(payload), "{\"deviceID\":\"%s\",\"components\":[\"%s\"]}", deviceID, component);
  Particle.publish("mqttbridge/register_device", payload, PRIVATE);
}

void setup() {
  waitUntil(Particle.connected);
  registerDevice();
}

void loop() {
  if (digitalRead(D2) == LOW) {
    Particle.publish("mqttbridge/from_particle/" + String(deviceID) + "/" + component, "pressed", PRIVATE);
    delay(500);
  }
}
```

To receive commands from Home Assistant (e.g., to control an LED):

```cpp
void commandHandler(const char* event, const char* data) {
  // Parse event to get component, act on data
  if (strcmp(data, "on") == 0) digitalWrite(D7, HIGH);
  else digitalWrite(D7, LOW);
}

void setup() {
  // ...existing code...
  Particle.subscribe("mqttbridge/to_particle/" + String(deviceID) + "/led", commandHandler);
}
```

---

## Notes
- MQTT broker address, port, and credentials are set in the bridge firmware (default: `192.168.0.200:1883`, user: `admin`, pass: `42F0rl!f3`).
- Discovery and availability messages are retained.
- All topic and payload formats are case-sensitive.
- The bridge must be running and connected to both Particle Cloud and MQTT broker for relaying to work.

---

Happy bridging! ✨

