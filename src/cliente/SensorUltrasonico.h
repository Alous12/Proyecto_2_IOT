#ifndef SENSOR_ULTRASONICO_H
#define SENSOR_ULTRASONICO_H

#include "Config.h"
#include "LecturaDistancia.h"

class SensorUltrasonico {
public:
    SensorUltrasonico(uint8_t pinDisparo = PIN_DISPARO, uint8_t pinEco = PIN_ECO,
                     unsigned long tiempoMaximoEcoUs = TIEMPO_MAXIMO_ECO_US);
    void iniciar();
    LecturaDistancia medirDistanciaCm();

private:
    uint8_t _pinDisparo;
    uint8_t _pinEco;
    unsigned long _tiempoMaximoEcoUs;
};

#endif
