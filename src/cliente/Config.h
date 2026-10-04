#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t PIN_DISPARO = 25;
constexpr uint8_t PIN_ECO = 26;
constexpr uint8_t PIN_LED_ROJO = 27;
constexpr uint8_t PIN_LED_AMARILLO = 32;
constexpr uint8_t PIN_LED_VERDE = 33;
constexpr unsigned long TIEMPO_MAXIMO_ECO_US = 30000;
constexpr unsigned long INTERVALO_MEDICION_MS = 100;
constexpr unsigned long TIEMPO_MAXIMO_SIN_COMANDOS_MS = 3000;
constexpr float VELOCIDAD_SONIDO_CM_US = 0.0343f;
constexpr float DISTANCIA_MINIMA_SENSOR_CM = 2.0f;
constexpr float DISTANCIA_MAXIMA_SENSOR_CM = 400.0f;
constexpr size_t LONGITUD_MAXIMA_MENSAJE = 192;

#endif
