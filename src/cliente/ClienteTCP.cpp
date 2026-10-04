#include <stdio.h>
#include <string.h>

#include "ClienteTCP.h"
#include "ConfigRed.h"

ClienteTCP::ClienteTCP(const char* tipo, const IPAddress& direccionLocal)
    : _tipo(tipo), _direccionLocal(direccionLocal) {
}

void ClienteTCP::iniciar() {
    if (SSID_WIFI[0] == '\0') {
        Serial.println("Complete SSID_WIFI y CLAVE_WIFI en ConfigRed.h");
        return;
    }

    WiFi.mode(WIFI_STA);
    if (USAR_IP_FIJA && !WiFi.config(_direccionLocal, IP_PUERTA_ENLACE, MASCARA_RED)) {
        Serial.println("No se pudo configurar la direccion IP");
        return;
    }

    WiFi.begin(SSID_WIFI, CLAVE_WIFI);
    _configurado = true;
    _ultimoIntentoWifi = millis();
    _ultimoIntentoServidor = millis() - REINTENTO_SERVIDOR_MS;
}

bool ClienteTCP::actualizar() {
    if (!_configurado) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        desconectar();
        if (millis() - _ultimoIntentoWifi >= REINTENTO_WIFI_MS) {
            _ultimoIntentoWifi = millis();
            WiFi.reconnect();
            Serial.println("Reintentando conexion Wi-Fi");
        }
        return false;
    }

    if (_conexion.connected()) {
        return false;
    }

    desconectar();
    if (millis() - _ultimoIntentoServidor < REINTENTO_SERVIDOR_MS) {
        return false;
    }

    const bool conectado = _conexion.connect(
        IP_SERVIDOR, PUERTO_SERVIDOR, TIEMPO_MAXIMO_CONEXION_MS);
    _ultimoIntentoServidor = millis();
    if (!conectado) {
        Serial.println("Servidor TCP no disponible");
        return false;
    }

    _conexion.setNoDelay(true);
    char registro[48];
    snprintf(registro, sizeof(registro), "REGISTER type=%s", _tipo);
    if (!enviarLinea(registro)) {
        return false;
    }

    Serial.print("Registro enviado; direccion local: ");
    Serial.println(WiFi.localIP());
    return true;
}

bool ClienteTCP::estaConectado() {
    return _configurado && WiFi.status() == WL_CONNECTED && _conexion.connected();
}

bool ClienteTCP::enviarLinea(const char* mensaje) {
    if (!estaConectado()) {
        return false;
    }

    const size_t longitud = strlen(mensaje);
    if (longitud > LONGITUD_MAXIMA_MENSAJE ||
        strchr(mensaje, '\n') != nullptr || strchr(mensaje, '\r') != nullptr) {
        return false;
    }

    char trama[LONGITUD_MAXIMA_MENSAJE + 1];
    memcpy(trama, mensaje, longitud);
    trama[longitud] = '\n';
    if (_conexion.write(reinterpret_cast<const uint8_t*>(trama), longitud + 1)
            != longitud + 1) {
        Serial.println("Fallo de envio; se restablecera la conexion");
        desconectar();
        return false;
    }
    return true;
}

bool ClienteTCP::recibirLinea(char* mensaje, size_t capacidad) {
    size_t recibidos = 0;
    while (estaConectado() && _conexion.available() > 0 &&
           recibidos < LONGITUD_MAXIMA_MENSAJE + 1) {
        const int dato = _conexion.read();
        if (dato < 0) {
            break;
        }
        ++recibidos;

        if (dato == '\n') {
            if (_longitudPendiente >= capacidad) {
                desconectar();
                return false;
            }
            memcpy(mensaje, _pendiente, _longitudPendiente);
            mensaje[_longitudPendiente] = '\0';
            _longitudPendiente = 0;
            return true;
        }

        if (dato == 0 || _longitudPendiente >= LONGITUD_MAXIMA_MENSAJE) {
            Serial.println("Mensaje TCP invalido o demasiado largo");
            desconectar();
            return false;
        }
        _pendiente[_longitudPendiente++] = static_cast<char>(dato);
    }
    return false;
}

void ClienteTCP::desconectar() {
    _conexion.stop();
    _longitudPendiente = 0;
}
