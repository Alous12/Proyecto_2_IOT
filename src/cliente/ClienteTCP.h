#ifndef CLIENTE_TCP_H
#define CLIENTE_TCP_H

#include <WiFi.h>

#include "Config.h"

class ClienteTCP {
public:
    ClienteTCP(const char* tipo, const IPAddress& direccionLocal);
    void iniciar();
    bool actualizar();
    bool estaConectado();
    bool enviarLinea(const char* mensaje);
    bool recibirLinea(char* mensaje, size_t capacidad);
    void desconectar();

private:
    WiFiClient _conexion;
    const char* _tipo;
    IPAddress _direccionLocal;
    bool _configurado = false;
    unsigned long _ultimoIntentoWifi = 0;
    unsigned long _ultimoIntentoServidor = 0;
    char _pendiente[LONGITUD_MAXIMA_MENSAJE + 1] = {};
    size_t _longitudPendiente = 0;
};

#endif
