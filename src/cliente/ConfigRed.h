#ifndef CONFIG_RED_H
#define CONFIG_RED_H

#include <WiFi.h>

// Completar con la red Wi-Fi a la que tambien se conectara la computadora.
static const char SSID_WIFI[] = "";
static const char CLAVE_WIFI[] = "";

// Las direcciones siguen el diagrama. Cambiarlas si la red real usa otras.
static const bool USAR_IP_FIJA = true;
static const IPAddress IP_SENSOR(192, 168, 0, 101);
static const IPAddress IP_ROUTER(192, 168, 0, 1);
static const IPAddress MASCARA_RED(255, 255, 255, 0);
static const IPAddress IP_SERVIDOR(192, 168, 0, 102);

// El diagrama no especifica puerto: coordinar este valor con el equipo
// que implemente el servidor Python.
static const uint16_t PUERTO_SERVIDOR = 5000;

static const unsigned long REINTENTO_WIFI_MS = 5000;
static const unsigned long REINTENTO_SERVIDOR_MS = 3000;
static const unsigned long INTERVALO_MEDICION_MS = 100;
static const float DISTANCIA_MAXIMA_ENVIO_CM = 200.0f;

#endif
