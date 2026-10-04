#include "IndicadorLeds.h"

void IndicadorLeds::begin() {
    pinMode(PIN_ROJO, OUTPUT);
    pinMode(PIN_AMARILLO, OUTPUT);
    pinMode(PIN_VERDE, OUTPUT);
    apagarTodos();
}

void IndicadorLeds::mostrar(EstadoIndicador estado) {
    switch (estado) {
        case EstadoIndicador::Rojo:
            configurar(ModoLed::Encendido, ModoLed::Apagado, ModoLed::Apagado);
            break;
        case EstadoIndicador::Amarillo:
            configurar(ModoLed::Apagado, ModoLed::Encendido, ModoLed::Apagado);
            break;
        case EstadoIndicador::Verde:
            configurar(ModoLed::Apagado, ModoLed::Apagado, ModoLed::Encendido);
            break;
        case EstadoIndicador::Error:
            configurar(ModoLed::Apagado, ModoLed::Apagado, ModoLed::Apagado);
            break;
    }
}

void IndicadorLeds::configurar(ModoLed rojo, ModoLed amarillo, ModoLed verde) {
    aplicarModo(IDX_ROJO, rojo);
    aplicarModo(IDX_AMARILLO, amarillo);
    aplicarModo(IDX_VERDE, verde);
}

bool IndicadorLeds::procesarComandoSet(const String& comando) {
    String texto = comando;
    texto.trim();

    if (!texto.substring(0, 3).equalsIgnoreCase("SET")) {
        return false;
    }
    texto = texto.substring(3);

    ModoLed nuevosModos[CANTIDAD_LEDS];
    for (int i = 0; i < CANTIDAD_LEDS; i++) {
        nuevosModos[i] = _modos[i];
    }

    int inicio = 0;
    while (inicio <= (int)texto.length()) {
        int fin = texto.indexOf(',', inicio);
        if (fin < 0) {
            fin = texto.length();
        }

        String par = texto.substring(inicio, fin);
        par.trim();

        if (par.length() > 0) {
            int igual = par.indexOf('=');
            if (igual < 0) {
                return false;
            }

            String nombre = par.substring(0, igual);
            String valor = par.substring(igual + 1);
            nombre.trim();
            valor.trim();

            int indice = indiceDeNombre(nombre);
            ModoLed modo;
            if (indice < 0 || !textoAModo(valor, modo)) {
                return false;
            }
            nuevosModos[indice] = modo;
        }

        inicio = fin + 1;
    }

    configurar(nuevosModos[IDX_ROJO], nuevosModos[IDX_AMARILLO], nuevosModos[IDX_VERDE]);
    return true;
}

void IndicadorLeds::actualizar() {
    const unsigned long ahora = millis();

    for (int i = 0; i < CANTIDAD_LEDS; i++) {
        const unsigned long medioPeriodo = medioPeriodoMs(_modos[i]);
        if (medioPeriodo == 0) {
            continue;
        }
        if (ahora - _ultimoCambioMs[i] >= medioPeriodo) {
            _ultimoCambioMs[i] = ahora;
            escribir(i, !_niveles[i]);
        }
    }
}

int IndicadorLeds::pinDe(int indice) {
    switch (indice) {
        case IDX_ROJO:
            return PIN_ROJO;
        case IDX_AMARILLO:
            return PIN_AMARILLO;
        case IDX_VERDE:
        default:
            return PIN_VERDE;
    }
}

int IndicadorLeds::indiceDeNombre(const String& nombre) {
    if (nombre.equalsIgnoreCase("redLed")) {
        return IDX_ROJO;
    }
    if (nombre.equalsIgnoreCase("yellowLed")) {
        return IDX_AMARILLO;
    }
    if (nombre.equalsIgnoreCase("greenLed")) {
        return IDX_VERDE;
    }
    return -1;
}

bool IndicadorLeds::textoAModo(const String& valor, ModoLed& modo) {
    if (valor.equalsIgnoreCase("on")) {
        modo = ModoLed::Encendido;
    } else if (valor.equalsIgnoreCase("off")) {
        modo = ModoLed::Apagado;
    } else if (valor.equalsIgnoreCase("blink_2")) {
        modo = ModoLed::Parpadeo2;
    } else if (valor.equalsIgnoreCase("blink_4")) {
        modo = ModoLed::Parpadeo4;
    } else {
        return false;
    }
    return true;
}

// blink_N = N parpadeos por segundo -> cambia de estado cada 500/N ms
unsigned long IndicadorLeds::medioPeriodoMs(ModoLed modo) {
    switch (modo) {
        case ModoLed::Parpadeo2:
            return 250;
        case ModoLed::Parpadeo4:
            return 125;
        default:
            return 0;
    }
}

void IndicadorLeds::aplicarModo(int indice, ModoLed modo) {
    if (_modos[indice] == modo) {
        return;
    }
    _modos[indice] = modo;
    _ultimoCambioMs[indice] = millis();

    // Los parpadeos arrancan encendidos para que el cambio sea visible de inmediato
    escribir(indice, modo != ModoLed::Apagado);
}

void IndicadorLeds::escribir(int indice, bool encendido) {
    _niveles[indice] = encendido;
    digitalWrite(pinDe(indice), encendido ? HIGH : LOW);
}

void IndicadorLeds::apagarTodos() {
    for (int i = 0; i < CANTIDAD_LEDS; i++) {
        _modos[i] = ModoLed::Apagado;
        escribir(i, false);
    }
}
