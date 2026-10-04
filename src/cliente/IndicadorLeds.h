#ifndef INDICADOR_LEDS_H
#define INDICADOR_LEDS_H

#include <stdint.h>

enum class ModoLed {
    Apagado,
    Encendido,
    Parpadeo2,
    Parpadeo4
};

class IndicadorLeds {
public:
    void iniciar();
    void configurar(ModoLed rojo, ModoLed amarillo, ModoLed verde);
    bool procesarComando(const char* comando);
    void actualizar();
    void apagarTodos();

private:
    static constexpr int CANTIDAD_LEDS = 3;
    ModoLed _modos[CANTIDAD_LEDS] = {
        ModoLed::Apagado, ModoLed::Apagado, ModoLed::Apagado
    };
    bool _niveles[CANTIDAD_LEDS] = {false, false, false};
    uint32_t _ultimoCambioMs[CANTIDAD_LEDS] = {0, 0, 0};

    static uint8_t pinDe(int indice);
    static int indiceDeNombre(const char* nombre);
    static bool textoAModo(const char* valor, ModoLed& modo);
    static uint32_t medioPeriodoMs(ModoLed modo);
    void aplicarModo(int indice, ModoLed modo);
    void escribir(int indice, bool encendido);
};

#endif
