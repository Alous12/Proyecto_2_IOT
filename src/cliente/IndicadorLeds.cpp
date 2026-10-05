#include <Arduino.h>
#include <string.h>

#include "Config.h"
#include "IndicadorLeds.h"

void IndicadorLeds::iniciar() {
    pinMode(PIN_LED_ROJO, OUTPUT);
    pinMode(PIN_LED_AMARILLO, OUTPUT);
    pinMode(PIN_LED_VERDE, OUTPUT);
    mostrar(EstadoIndicador::Error);
}

void IndicadorLeds::mostrar(EstadoIndicador estado) {
    digitalWrite(PIN_LED_ROJO, estado == EstadoIndicador::Rojo ? HIGH : LOW);
    digitalWrite(PIN_LED_AMARILLO, estado == EstadoIndicador::Amarillo ? HIGH : LOW);
    digitalWrite(PIN_LED_VERDE, estado == EstadoIndicador::Verde ? HIGH : LOW);
}

bool IndicadorLeds::procesarComando(const char* comando) {
    EstadoIndicador estado;
    if (strcmp(comando, "SET redLed=on, yellowLed=off, greenLed=off") == 0) {
        estado = EstadoIndicador::Rojo;
    } else if (strcmp(comando, "SET redLed=off, yellowLed=on, greenLed=off") == 0) {
        estado = EstadoIndicador::Amarillo;
    } else if (strcmp(comando, "SET redLed=off, yellowLed=off, greenLed=on") == 0) {
        estado = EstadoIndicador::Verde;
    } else if (strcmp(comando, "SET redLed=off, yellowLed=off, greenLed=off") == 0) {
        estado = EstadoIndicador::Error;
    } else {
        return false;
    }
    mostrar(estado);
    return true;
}
