#include <Arduino.h>
#include <stdio.h>

#include "ClienteTCP.h"
#include "ConfigRed.h"
#include "SensorUltrasonico.h"

namespace {

SensorUltrasonico sensor;
ClienteTCP cliente("sensor", IP_SENSOR);
unsigned long ultimaMedicion = 0;

}

void setup() {
    Serial.begin(115200);
    sensor.iniciar();
    cliente.iniciar();
    ultimaMedicion = millis() - INTERVALO_MEDICION_MS;
}

void loop() {
    cliente.actualizar();
    if (millis() - ultimaMedicion < INTERVALO_MEDICION_MS) {
        delay(1);
        return;
    }

    ultimaMedicion = millis();
    const LecturaDistancia lectura = sensor.medirDistanciaCm();
    char mensaje[48];

    if (lectura.valida) {
        snprintf(mensaje, sizeof(mensaje), "POST distance=%.2f", lectura.distanciaCm);
    } else {
        snprintf(mensaje, sizeof(mensaje), "POST distance=invalida");
    }

    Serial.print(cliente.enviarLinea(mensaje) ? "Enviado: " : "Sin conexion: ");
    Serial.println(mensaje);
}
