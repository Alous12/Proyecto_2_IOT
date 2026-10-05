#ifndef INDICADOR_LEDS_H
#define INDICADOR_LEDS_H

enum class EstadoIndicador {
    Rojo,
    Amarillo,
    Verde,
    Error
};

class IndicadorLeds {
public:
    void iniciar();
    void mostrar(EstadoIndicador estado);
    bool procesarComando(const char* comando);
};

#endif
