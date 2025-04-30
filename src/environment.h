// environment.h
// Declarations for environment variables used in the Particle-MQTT bridge.

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "Particle.h"

extern const byte MQTT_SERVER[4];
extern const int MQTT_PORT;
extern const char* MQTT_USER;
extern const char* MQTT_PASS;
extern const char* MQTT_CLIENT_ID;

#endif // ENVIRONMENT_H
