#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Config.h"
#include "IndicadorLeds.h"
#include "SensorUltrasonico.h"

uint32_t instante = 0;
uint8_t niveles[40] = {};
uint8_t modos[40] = {};
unsigned long duracionEco = 0;
unsigned long ultimoTiempoMaximo = 0;
uint8_t ultimoPinEco = 0;
int comprobaciones = 0;

unsigned long millis() {
    return instante;
}

void pinMode(uint8_t pin, uint8_t modo) {
    modos[pin] = modo;
}

void digitalWrite(uint8_t pin, uint8_t nivel) {
    niveles[pin] = nivel;
}

void delayMicroseconds(unsigned int) {
}

unsigned long pulseIn(uint8_t pin, uint8_t, unsigned long tiempoMaximo) {
    ultimoPinEco = pin;
    ultimoTiempoMaximo = tiempoMaximo;
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
    exigir(ultimoTiempoMaximo == TIEMPO_MAXIMO_ECO_US, "Limite de espera de eco");
    exigir(ultimoPinEco == PIN_ECO, "Entrada de eco");

    duracionEco = 116;
    exigir(!sensor.medirDistanciaCm().valida, "Distancia menor a 2 cm");
    duracionEco = 117;
    exigir(sensor.medirDistanciaCm().valida, "Distancia desde 2 cm");
    duracionEco = 1000;
    const LecturaDistancia lectura = sensor.medirDistanciaCm();
    exigir(lectura.valida && std::fabs(lectura.distanciaCm - 17.15f) < 0.001f,
           "Conversion a centimetros");
    duracionEco = 12000;
    exigir(sensor.medirDistanciaCm().valida, "El servidor recibe distancias mayores a 200 cm");
    duracionEco = 23323;
    exigir(sensor.medirDistanciaCm().valida, "Limite fisico cercano a 400 cm");
    duracionEco = 23324;
    exigir(!sensor.medirDistanciaCm().valida, "Distancia superior a 400 cm");
    exigir(niveles[PIN_DISPARO] == LOW, "Disparo termina apagado");
}

void comprobarIndicador() {
    IndicadorLeds indicador;
    indicador.iniciar();
    exigir(niveles[PIN_LED_ROJO] == LOW && niveles[PIN_LED_AMARILLO] == LOW &&
           niveles[PIN_LED_VERDE] == LOW, "Inicio con LEDs apagados");

    exigir(indicador.procesarComando("SET redLed=on, yellowLed=off, greenLed=off"),
           "Comando del servidor para cercano");
    exigir(niveles[PIN_LED_ROJO] == HIGH && niveles[PIN_LED_AMARILLO] == LOW &&
           niveles[PIN_LED_VERDE] == LOW, "Salida cercana");

    exigir(indicador.procesarComando(" SET greenLed = on, redLed = off, yellowLed = off\r "),
           "Espacios y orden de argumentos");
    exigir(niveles[PIN_LED_VERDE] == HIGH && niveles[PIN_LED_ROJO] == LOW,
           "Salida lejana");

    const char* invalidos[] = {
        "", "SET", "SETTING redLed=on, yellowLed=off, greenLed=off",
        "SET redLed=on", "SET redLed=on, yellowLed=off, greenLed=off,",
        "SET redLed=on, redLed=off, greenLed=off",
        "SET redLed=on, yellowLed=off, blueLed=off",
        "SET redLed=blink_3, yellowLed=off, greenLed=off",
        "SET redLed==on, yellowLed=off, greenLed=off"
    };
    for (const char* comando : invalidos) {
        exigir(!indicador.procesarComando(comando), "Rechazo de comando invalido");
        exigir(niveles[PIN_LED_VERDE] == HIGH && niveles[PIN_LED_ROJO] == LOW,
               "Un comando invalido no se aplica parcialmente");
    }
    const std::string demasiadoLargo(LONGITUD_MAXIMA_MENSAJE + 1, 'x');
    exigir(!indicador.procesarComando(demasiadoLargo.c_str()), "Limite de mensaje");

    instante = 0;
    exigir(indicador.procesarComando("SET redLed=blink_2, yellowLed=off, greenLed=off"),
           "Parpadeo de 2 Hz");
    instante = 100;
    indicador.procesarComando("SET redLed=blink_2, yellowLed=off, greenLed=off");
    instante = 249;
    indicador.actualizar();
    exigir(niveles[PIN_LED_ROJO] == HIGH, "Parpadeo antes del medio periodo");
    instante = 250;
    indicador.actualizar();
    exigir(niveles[PIN_LED_ROJO] == LOW, "Comandos repetidos no reinician el parpadeo");
    instante = 500;
    indicador.actualizar();
    exigir(niveles[PIN_LED_ROJO] == HIGH, "Periodo completo de 2 Hz");

    instante = 0;
    indicador.configurar(ModoLed::Apagado, ModoLed::Parpadeo4, ModoLed::Apagado);
    instante = 124;
    indicador.actualizar();
    exigir(niveles[PIN_LED_AMARILLO] == HIGH, "Inicio de parpadeo de 4 Hz");
    instante = 125;
    indicador.actualizar();
    exigir(niveles[PIN_LED_AMARILLO] == LOW, "Medio periodo de 4 Hz");
    instante = 500;
    indicador.actualizar();
    exigir(niveles[PIN_LED_AMARILLO] == HIGH, "Recuperacion de ciclos demorados");

    indicador.apagarTodos();
    instante = UINT32_MAX - 100;
    indicador.configurar(ModoLed::Parpadeo2, ModoLed::Apagado, ModoLed::Apagado);
    instante = 149;
    indicador.actualizar();
    exigir(niveles[PIN_LED_ROJO] == LOW, "Parpadeo al desbordarse el reloj");
    indicador.apagarTodos();
    exigir(niveles[PIN_LED_ROJO] == LOW && niveles[PIN_LED_AMARILLO] == LOW &&
           niveles[PIN_LED_VERDE] == LOW, "Apagado de todas las salidas");
}

int main() {
    comprobarSensor();
    comprobarIndicador();
    std::cout << comprobaciones << " comprobaciones del cliente aprobadas" << std::endl;
}
