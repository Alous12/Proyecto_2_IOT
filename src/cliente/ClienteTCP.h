#ifndef CLIENTE_TCP_H
#define CLIENTE_TCP_H

#include <WiFi.h>

class ClienteTCP {
public:
    ClienteTCP(const char* tipo, const IPAddress& direccionLocal);
    void iniciar();
    void actualizar();
    bool estaConectado();
    bool enviarLinea(const String& mensaje);
    bool recibirLinea(String& mensaje);

private:
    WiFiClient _conexion;
    const char* _tipo;
    IPAddress _direccionLocal;
    bool _configurado = false;
    unsigned long _ultimoIntento = 0;
    String _pendiente;
};

#endif
