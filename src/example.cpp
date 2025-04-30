// Example: Particle device firmware for bidirectional MQTT communication with Home Assistant
//
// This example demonstrates how a Particle device can:
// 1. Register itself and its components (button and LED) with the MQTT bridge for Home Assistant discovery.
// 2. Send button press events to Home Assistant via MQTT.
// 3. Receive commands from Home Assistant to control an LED via MQTT.
//
// - When the device starts, it registers itself and its components.
// - When the button (D2) is pressed, it publishes a message to the bridge.
// - When a command is received for the LED, it turns the LED (D7) on or off.

#include "Particle.h"

SYSTEM_MODE(AUTOMATIC); // Ensure the device connects to the Particle Cloud automatically

const char* deviceID = Particle.deviceID(); // Get the unique device ID
const char* components[] = {"button"}; // Define the components for this device
const size_t numComponents = 2;

// Register the device and its components with the bridge for Home Assistant discovery
void registerDevice() {
    char payload[128];
    // Format: {"deviceID":"<id>","components":["button","led"]}
    snprintf(payload, sizeof(payload), "{\"deviceID\":\"%s\",\"components\":[\"%s\"]}", deviceID, components[0]);
    Particle.publish("mqttbridge/register_device", payload, PRIVATE);
}

// Handle commands from Home Assistant (e.g., turn LED on/off)
void commandHandler(const char* event, const char* data) {
    // If the event is for the LED component
    if (strstr(event, "/led")) {
        if (strcmp(data, "on") == 0) digitalWrite(D7, HIGH); // Turn LED on
        else digitalWrite(D7, LOW); // Turn LED off
    }
}

void setup() {
    pinMode(D2, INPUT_PULLUP); // Configure D2 as button input (active low)
    pinMode(D7, OUTPUT);       // Configure D7 as LED output
    waitUntil(Particle.connected); // Wait for cloud connection
    registerDevice(); // Register with the bridge
    // Subscribe to LED command topic from the bridge
    Particle.subscribe(String("mqttbridge/to_particle/") + deviceID + "/led", commandHandler);
}

void loop() {
    static bool lastState = HIGH; // Track previous button state
    bool state = digitalRead(D2); // Read current button state
    if (state == LOW && lastState == HIGH) {
        // Button was just pressed (active low)
        String topic = String("mqttbridge/from_particle/") + deviceID + "/button";
        Particle.publish(topic, "pressed", PRIVATE); // Notify bridge of button press
        delay(500); // Debounce delay
    }
    lastState = state;
}
