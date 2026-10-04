#include <Arduino.h>
#include <WiFi.h>

#include "ConfigRed.h"
#include "SensorUltrasonico.h"

namespace {

SensorUltrasonico sensor;
WiFiClient servidor;

unsigned long ultimoIntentoWifi = 0;
unsigned long ultimoIntentoServidor = 0;

void mantenerConexion() {
    if (WiFi.status() != WL_CONNECTED) {
        servidor.stop();

        if (millis() - ultimoIntentoWifi >= REINTENTO_WIFI_MS) {
            ultimoIntentoWifi = millis();
            WiFi.reconnect();
            Serial.println("Reintentando Wi-Fi...");
        }
        return;
    }

    if (servidor.connected()) {
        return;
    }

    servidor.stop();
    if (millis() - ultimoIntentoServidor < REINTENTO_SERVIDOR_MS) {
        return;
    }

    ultimoIntentoServidor = millis();
    if (!servidor.connect(IP_SERVIDOR, PUERTO_SERVIDOR)) {
        Serial.println("No se pudo conectar al servidor TCP");
        return;
    }

    // TCP es un flujo: cada comando termina en \n para que el servidor
    // pueda separar mensajes aunque lleguen juntos o fragmentados.
    static const char registro[] = "REGISTER type=sensor\n";
    if (servidor.write(reinterpret_cast<const uint8_t*>(registro), sizeof(registro) - 1)
            != sizeof(registro) - 1) {
        servidor.stop();
        return;
    }

    Serial.println("Sensor registrado en el servidor TCP");
}

void enviarDistancia(float distanciaCm) {
    if (!servidor.connected()) {
        return;
    }

    char mensaje[48];
    const int longitud = snprintf(mensaje, sizeof(mensaje), "POST distance=%.2f\n", distanciaCm);
    if (longitud <= 0 || longitud >= static_cast<int>(sizeof(mensaje)) ||
            servidor.write(reinterpret_cast<const uint8_t*>(mensaje), longitud)
                != static_cast<size_t>(longitud)) {
        servidor.stop();
        Serial.println("Error al enviar distancia; se intentara reconectar");
        return;
    }

    Serial.print("Distancia enviada (cm): ");
    Serial.println(distanciaCm, 2);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    sensor.begin();

    if (SSID_WIFI[0] == '\0') {
        Serial.println("Configure SSID_WIFI y CLAVE_WIFI en ConfigRed.h");
        return;
    }

    WiFi.mode(WIFI_STA);
    if (USAR_IP_FIJA && !WiFi.config(IP_SENSOR, IP_ROUTER, MASCARA_RED)) {
        Serial.println("No se pudo configurar la IP fija del sensor");
    }
    WiFi.begin(SSID_WIFI, CLAVE_WIFI);
    ultimoIntentoWifi = millis();
    ultimoIntentoServidor = millis() - REINTENTO_SERVIDOR_MS;
}

void loop() {
    if (SSID_WIFI[0] != '\0') {
        mantenerConexion();
    }

    const LecturaDistancia lectura = sensor.medirDistanciaCm();
    // El protocolo solo define distancia: no enviamos un valor falso
    // cuando no hay eco o la medicion sale del rango de trabajo.
    if (lectura.valida && lectura.distanciaCm <= DISTANCIA_MAXIMA_ENVIO_CM) {
        enviarDistancia(lectura.distanciaCm);
    } else {
        Serial.println("Lectura invalida o fuera del rango; no se envia");
    }

    delay(INTERVALO_MEDICION_MS);
}
