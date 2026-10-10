# Proyecto 2 IoT — Integración de Objetos Inteligentes con TCP/IP

Práctica 2 de Internet de las Cosas [SIS-234]. Es un sistema distribuido sobre WiFi (IEEE 802.11):

- **Objeto sensor** (ESP32 + HC-SR04): mide la distancia y la envía al servidor.
- **Servidor TCP** (Python, PC): clasifica la distancia en tres rangos con histéresis y decide qué LED encender.
- **Objeto actuador** (ESP32 + 3 LEDs): enciende el LED que ordena el servidor.

## 📄 Informe técnico

**[doc/informe.md](doc/informe.md)**: requerimientos, diagramas (arquitectura, circuito, clases, secuencia y estados), especificación del protocolo, implementación, pruebas, resultados, conclusiones y anexos.

## Estructura

| Carpeta | Contenido |
| --- | --- |
| [`src/cliente/`](src/cliente) | Firmware de ambos ESP32 (C++ / Arduino) |
| [`src/servidor/`](src/servidor) | Servidor TCP y algoritmo de control (Python) |
| [`pruebas/`](pruebas) | Pruebas del servidor, prueba nativa del firmware y simulador del sensor |
| [`doc/`](doc) | Informe técnico y anexos |

## Uso rápido

1. Completar la red WiFi, las IP y el puerto en [`src/cliente/ConfigRed.h`](src/cliente/ConfigRed.h).
2. Compilar y cargar cada firmware en su placa:

   ```powershell
   pio run -e sensor   -t upload --upload-port COM3
   pio run -e actuador -t upload --upload-port COM4
   ```

3. Iniciar el servidor en la PC:

   ```powershell
   python src/servidor/server.py
   ```

4. Ejecutar las pruebas:

   ```powershell
   python -m unittest discover -s pruebas -p "test_*.py" -v
   ```

Los detalles de configuración, ejecución y pruebas están en la [sección 3.8 del informe](doc/informe.md#38-configuración-compilación-y-ejecución).

## Integrantes

Alejandro Machaca · Rubén Cordero · Luciana Leaño
