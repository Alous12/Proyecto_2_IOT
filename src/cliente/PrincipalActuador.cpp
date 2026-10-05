#include <Arduino.h>

#include "ClienteTCP.h"
#include "ConfigRed.h"
#include "IndicadorLeds.h"

IndicadorLeds indicador;
ClienteTCP cliente("actuator", IP_ACTUADOR);

void setup() {
    Serial.begin(115200);
    indicador.iniciar();
    cliente.iniciar();
}

void loop() {
    cliente.actualizar();
    if (!cliente.estaConectado()) {
        indicador.mostrar(EstadoIndicador::Error);
        delay(10);
        return;
    }

    String mensaje;
    while (cliente.recibirLinea(mensaje)) {
        if (indicador.procesarComando(mensaje.c_str())) {
            Serial.println(mensaje);
        }
    }
    delay(10);
}
