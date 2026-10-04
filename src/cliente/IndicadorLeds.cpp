#include <Arduino.h>
#include <ctype.h>
#include <string.h>

#include "Config.h"
#include "IndicadorLeds.h"

namespace {

char* recortar(char* texto) {
    while (isspace(static_cast<unsigned char>(*texto))) {
        ++texto;
    }
    char* final = texto + strlen(texto);
    while (final > texto && isspace(static_cast<unsigned char>(final[-1]))) {
        --final;
    }
    *final = '\0';
    return texto;
}

}

void IndicadorLeds::iniciar() {
    for (int indice = 0; indice < CANTIDAD_LEDS; ++indice) {
        pinMode(pinDe(indice), OUTPUT);
    }
    apagarTodos();
}

void IndicadorLeds::configurar(ModoLed rojo, ModoLed amarillo, ModoLed verde) {
    aplicarModo(0, rojo);
    aplicarModo(1, amarillo);
    aplicarModo(2, verde);
}

bool IndicadorLeds::procesarComando(const char* comando) {
    if (strlen(comando) > LONGITUD_MAXIMA_MENSAJE) {
        return false;
    }

    char copia[LONGITUD_MAXIMA_MENSAJE + 1];
    strcpy(copia, comando);
    char* texto = recortar(copia);
    if (strncmp(texto, "SET", 3) != 0 ||
        !isspace(static_cast<unsigned char>(texto[3]))) {
        return false;
    }

    ModoLed nuevosModos[CANTIDAD_LEDS];
    bool encontrados[CANTIDAD_LEDS] = {false, false, false};
    char* argumento = texto + 3;

    while (argumento != nullptr) {
        char* siguiente = strchr(argumento, ',');
        if (siguiente != nullptr) {
            *siguiente++ = '\0';
        }

        char* igual = strchr(argumento, '=');
        if (igual == nullptr || strchr(igual + 1, '=') != nullptr) {
            return false;
        }
        *igual++ = '\0';

        const int indice = indiceDeNombre(recortar(argumento));
        if (indice < 0 || encontrados[indice] ||
            !textoAModo(recortar(igual), nuevosModos[indice])) {
            return false;
        }
        encontrados[indice] = true;
        argumento = siguiente;
    }

    for (int indice = 0; indice < CANTIDAD_LEDS; ++indice) {
        if (!encontrados[indice]) {
            return false;
        }
    }

    configurar(nuevosModos[0], nuevosModos[1], nuevosModos[2]);
    return true;
}

void IndicadorLeds::actualizar() {
    const uint32_t ahora = millis();
    for (int indice = 0; indice < CANTIDAD_LEDS; ++indice) {
        const uint32_t medioPeriodo = medioPeriodoMs(_modos[indice]);
        if (medioPeriodo == 0) {
            continue;
        }
        const uint32_t pasos = (ahora - _ultimoCambioMs[indice]) / medioPeriodo;
        if (pasos > 0) {
            _ultimoCambioMs[indice] += pasos * medioPeriodo;
            if (pasos % 2 != 0) {
                escribir(indice, !_niveles[indice]);
            }
        }
    }
}

uint8_t IndicadorLeds::pinDe(int indice) {
    static const uint8_t pines[CANTIDAD_LEDS] = {
        PIN_LED_ROJO, PIN_LED_AMARILLO, PIN_LED_VERDE
    };
    return pines[indice];
}

int IndicadorLeds::indiceDeNombre(const char* nombre) {
    if (strcmp(nombre, "redLed") == 0) {
        return 0;
    }
    if (strcmp(nombre, "yellowLed") == 0) {
        return 1;
    }
    if (strcmp(nombre, "greenLed") == 0) {
        return 2;
    }
    return -1;
}

bool IndicadorLeds::textoAModo(const char* valor, ModoLed& modo) {
    if (strcmp(valor, "on") == 0) {
        modo = ModoLed::Encendido;
    } else if (strcmp(valor, "off") == 0) {
        modo = ModoLed::Apagado;
    } else if (strcmp(valor, "blink_2") == 0) {
        modo = ModoLed::Parpadeo2;
    } else if (strcmp(valor, "blink_4") == 0) {
        modo = ModoLed::Parpadeo4;
    } else {
        return false;
    }
    return true;
}

uint32_t IndicadorLeds::medioPeriodoMs(ModoLed modo) {
    if (modo == ModoLed::Parpadeo2) {
        return 250;
    }
    if (modo == ModoLed::Parpadeo4) {
        return 125;
    }
    return 0;
}

void IndicadorLeds::aplicarModo(int indice, ModoLed modo) {
    if (_modos[indice] != modo) {
        _modos[indice] = modo;
        _ultimoCambioMs[indice] = millis();
        escribir(indice, modo != ModoLed::Apagado);
    }
}

void IndicadorLeds::escribir(int indice, bool encendido) {
    _niveles[indice] = encendido;
    digitalWrite(pinDe(indice), encendido ? HIGH : LOW);
}

void IndicadorLeds::apagarTodos() {
    for (int indice = 0; indice < CANTIDAD_LEDS; ++indice) {
        _modos[indice] = ModoLed::Apagado;
        escribir(indice, false);
    }
}
