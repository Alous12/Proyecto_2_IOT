# Proyecto 2 IoT

Sistema distribuido con un ESP32 que mide distancias, un servidor TCP en Python que las clasifica y otro ESP32 que controla los LEDs.

La explicación del código, protocolo, configuración, compilación, carga y pruebas está en [Funcionamiento del sistema](doc/funcionamiento.md).

Antes de cargar las placas, completar la red Wi-Fi y verificar las direcciones y el puerto en [ConfigRed.h](src/cliente/ConfigRed.h).

```powershell
pio run
python src/servidor/server.py
```

Los entornos de PlatformIO son `sensor` y `actuador`. Cada uno se carga en una placa diferente.
