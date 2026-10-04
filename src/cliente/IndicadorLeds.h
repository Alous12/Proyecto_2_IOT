#ifndef INDICADOR_LEDS_H
#define INDICADOR_LEDS_H

#include <Arduino.h>

enum class EstadoIndicador {
    Rojo,
    Amarillo,
    Verde,
    Error
};

// Valores posibles del comando SET: on, off, blink_2, blink_4
enum class ModoLed {
    Apagado,
    Encendido,
    Parpadeo2,
    Parpadeo4
};

class IndicadorLeds {
public:
    void begin();

    void mostrar(EstadoIndicador estado);

    void configurar(ModoLed rojo, ModoLed amarillo, ModoLed verde);

    // Ej. "SET redLed=off, yellowLed=on, greenLed=off"
    // Devuelve false si el comando es invalido (no se aplica ningun cambio).
    bool procesarComandoSet(const String& comando);

    // Debe llamarse en cada loop() para que funcione el parpadeo.
    void actualizar();

private:
    static constexpr int PIN_ROJO = 27;
    static constexpr int PIN_AMARILLO = 32;
    static constexpr int PIN_VERDE = 33;

    static constexpr int CANTIDAD_LEDS = 3;
    static constexpr int IDX_ROJO = 0;
    static constexpr int IDX_AMARILLO = 1;
    static constexpr int IDX_VERDE = 2;

    ModoLed _modos[CANTIDAD_LEDS] = {ModoLed::Apagado, ModoLed::Apagado, ModoLed::Apagado};
    bool _niveles[CANTIDAD_LEDS] = {false, false, false};
    unsigned long _ultimoCambioMs[CANTIDAD_LEDS] = {0, 0, 0};

    static int pinDe(int indice);
    static int indiceDeNombre(const String& nombre);
    static bool textoAModo(const String& valor, ModoLed& modo);
    static unsigned long medioPeriodoMs(ModoLed modo);

    void aplicarModo(int indice, ModoLed modo);
    void escribir(int indice, bool encendido);
    void apagarTodos();
};

#endif
