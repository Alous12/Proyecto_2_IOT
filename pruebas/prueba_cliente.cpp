#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Config.h"
#include "IndicadorLeds.h"
#include "SensorUltrasonico.h"

uint8_t niveles[40] = {};
uint8_t modos[40] = {};
unsigned long duracionEco = 0;
int comprobaciones = 0;

void pinMode(uint8_t pin, uint8_t modo) {
    modos[pin] = modo;
}

void digitalWrite(uint8_t pin, uint8_t nivel) {
    niveles[pin] = nivel;
}

void delayMicroseconds(unsigned int) {
}

unsigned long pulseIn(uint8_t, uint8_t, unsigned long) {
    return duracionEco;
}

void exigir(bool condicion, const char* descripcion) {
    ++comprobaciones;
    if (!condicion) {
        std::cerr << "Fallo: " << descripcion << std::endl;
        std::exit(1);
    }
}

void comprobarSensor() {
    SensorUltrasonico sensor;
    sensor.iniciar();
    exigir(modos[PIN_DISPARO] == OUTPUT && modos[PIN_ECO] == INPUT, "Pines del sensor");
    duracionEco = 0;
    exigir(!sensor.medirDistanciaCm().valida, "Ausencia de eco");
    duracionEco = 116;
    exigir(!sensor.medirDistanciaCm().valida, "Menos de 2 cm");
    duracionEco = 117;
    exigir(sensor.medirDistanciaCm().valida, "Desde 2 cm");
    duracionEco = 1000;
    const LecturaDistancia lectura = sensor.medirDistanciaCm();
    exigir(lectura.valida && std::fabs(lectura.distanciaCm - 17.15f) < 0.001f,
           "Conversion a centimetros");
    duracionEco = 12000;
    exigir(sensor.medirDistanciaCm().valida, "El servidor valida el limite de trabajo");
    duracionEco = 23324;
    exigir(!sensor.medirDistanciaCm().valida, "Mas de 400 cm");
}

void comprobarIndicador() {
    IndicadorLeds indicador;
    indicador.iniciar();
    exigir(niveles[PIN_LED_ROJO] == LOW && niveles[PIN_LED_AMARILLO] == LOW &&
           niveles[PIN_LED_VERDE] == LOW, "Inicio apagado");

    const char* comandos[] = {
        "SET redLed=on, yellowLed=off, greenLed=off",
        "SET redLed=off, yellowLed=on, greenLed=off",
        "SET redLed=off, yellowLed=off, greenLed=on",
        "SET redLed=off, yellowLed=off, greenLed=off"
    };
    for (int estado = 0; estado < 4; ++estado) {
        for (int repeticion = 0; repeticion < 2; ++repeticion) {
            exigir(indicador.procesarComando(comandos[estado]), "Comando del servidor");
            exigir(niveles[PIN_LED_ROJO] == (estado == 0) &&
                   niveles[PIN_LED_AMARILLO] == (estado == 1) &&
                   niveles[PIN_LED_VERDE] == (estado == 2), "Solo el LED correspondiente");
        }
    }

    const char* invalidos[] = {
        "SET redLed=on",
        "SET redLed=on, yellowLed=on, greenLed=off",
        "SET redLed=blink_2, yellowLed=off, greenLed=off",
        "SET redLed=blink_4, yellowLed=off, greenLed=off"
    };
    for (const char* comando : invalidos) {
        exigir(!indicador.procesarComando(comando), "Rechaza estados ajenos a las reglas");
        exigir(niveles[PIN_LED_ROJO] == LOW && niveles[PIN_LED_AMARILLO] == LOW &&
               niveles[PIN_LED_VERDE] == LOW, "Sin cambios ante una orden invalida");
    }
}

int main() {
    comprobarSensor();
    comprobarIndicador();
    std::cout << comprobaciones << " comprobaciones aprobadas" << std::endl;
}
