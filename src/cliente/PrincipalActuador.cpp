#include <Arduino.h>

#include "ClienteTCP.h"
#include "ConfigRed.h"
#include "IndicadorLeds.h"

namespace {

IndicadorLeds indicador;
ClienteTCP cliente("actuator", IP_ACTUADOR);
unsigned long ultimoComando = 0;

}

void setup() {
    Serial.begin(115200);
    indicador.iniciar();
    cliente.iniciar();
}

void loop() {
    const bool conectado = cliente.estaConectado();
    const bool vencido = millis() - ultimoComando >= TIEMPO_MAXIMO_SIN_COMANDOS_MS;
    if (!conectado || vencido) {
        indicador.apagarTodos();
        if (conectado && vencido) {
            cliente.desconectar();
        }
    }

    if (cliente.actualizar()) {
        indicador.apagarTodos();
        ultimoComando = millis();
    }

    char mensaje[LONGITUD_MAXIMA_MENSAJE + 1];
    for (int recibidos = 0; recibidos < 8 && cliente.recibirLinea(mensaje, sizeof(mensaje));
         ++recibidos) {
        if (indicador.procesarComando(mensaje)) {
            ultimoComando = millis();
            Serial.print("Aplicado: ");
            Serial.println(mensaje);
        } else {
            Serial.println("Comando de luces invalido; se apagan los LEDs");
            indicador.apagarTodos();
        }
    }

    if (!cliente.estaConectado()) {
        indicador.apagarTodos();
    }
    indicador.actualizar();
    delay(1);
}
