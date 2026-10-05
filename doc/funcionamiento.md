# Funcionamiento del proyecto

Se mantiene el comportamiento original: medir la distancia, clasificarla y encender el LED correspondiente. La diferencia es que ahora la clasificación se realiza en un servidor Python, mientras un ESP32 mide y otro controla las luces.

El código propio está en español y sin comentarios. Se conservan los nombres requeridos por Arduino, Python y el protocolo, como `setup`, `loop`, `REGISTER`, `POST` y `SET`.

## Reglas originales

| Distancia inicial o condición | Resultado |
| --- | --- |
| Menor que 2 cm, mayor que 200 cm o lectura inválida | Todos los LEDs apagados. |
| Desde 2 cm hasta menos de 20 cm | LED rojo encendido. |
| Desde 20 cm hasta menos de 40 cm | LED amarillo encendido. |
| Desde 40 cm hasta 200 cm | LED verde encendido. |

Se conserva la histéresis de 2 cm: desde cercano cambia al alcanzar 22 cm; desde medio cambia a cercano por debajo de 18 cm y a lejano desde 42 cm; desde lejano vuelve a clasificar por debajo de 38 cm. Después de una lectura inválida se utilizan los límites base de 20 y 40 cm.

Los LEDs tienen luz fija. Como máximo se enciende uno, y una lectura inválida apaga los tres.

## Responsabilidad de cada archivo

| Archivo | Función |
| --- | --- |
| `src/cliente/main.cpp` | Inicializa el sensor, obtiene una lectura, la envía y espera 100 ms. |
| `src/cliente/SensorUltrasonico.h/.cpp` | Genera el pulso de disparo, mide el eco y calcula centímetros. |
| `src/cliente/LecturaDistancia.h` | Guarda la distancia y si la lectura es válida. |
| `src/cliente/PrincipalActuador.cpp` | Recibe las órdenes del servidor y las entrega al indicador. |
| `src/cliente/IndicadorLeds.h/.cpp` | Enciende rojo, amarillo o verde, o apaga todos, según la orden recibida. |
| `src/cliente/ClienteTCP.h/.cpp` | Gestiona Wi-Fi, conexión TCP, registro y mensajes de ambos ESP32. |
| `src/cliente/Config.h` | Define pines, intervalo de medición y límites físicos del sensor. |
| `src/cliente/ConfigRed.h` | Define credenciales, IP y puerto del servidor. |
| `src/servidor/server.py` | Recibe distancias, aplica las reglas originales y envía el estado de las luces. |
| `platformio.ini` | Selecciona los archivos para compilar el firmware `sensor` o `actuador`. |

El clasificador anterior del cliente se trasladó al servidor; no se mantiene una segunda clasificación en las placas.

## Intercambio de mensajes

Al conectarse, cada ESP32 se registra:

```text
REGISTER type=sensor
REGISTER type=actuator
```

El sensor envía una medición en centímetros:

```text
POST distance=15.27
```

El servidor aplica los límites y la histéresis y envía:

```text
SET redLed=on, yellowLed=off, greenLed=off
```

Para medio enciende amarillo y para lejano enciende verde. Si no hubo eco, el sensor envía `POST distance=invalida` y el servidor responde con los tres valores en `off`.

Cada mensaje termina en `\n`. El módulo TCP conserva las partes de una línea hasta completarla; en Python, `socketserver` se encarga de aceptar las conexiones y leer las líneas. Los mensajes usan el formato mostrado. El actuador reconoce las cuatro órdenes que genera el servidor: rojo, amarillo, verde y apagado.

## Medición y control

El sensor conserva TRIG en GPIO25 y ECHO en GPIO26. Genera un pulso de 10 microsegundos y espera el eco hasta 30000 microsegundos. Calcula `distancia = duracion × 0.0343 / 2`, por el recorrido de ida y vuelta del sonido.

El sensor acepta físicamente de 2 a 400 cm. El servidor mantiene el rango de trabajo original de 2 a 200 cm: una medición física de 250 cm se transmite, pero produce todos los LEDs apagados.

El actuador utiliza GPIO27 para rojo, GPIO32 para amarillo y GPIO33 para verde. Sus salidas permanecen en el estado recibido hasta una nueva orden o una desconexión detectada. No utiliza temporizadores de luces.

Los clientes reintentan la conexión cuando Wi-Fi o TCP están desconectados. No se fuerza una reconexión por dejar de recibir mensajes ni se borra la histéresis por una pausa. Al detectar la desconexión del sensor que publicó el último dato, el servidor envía apagado; el actuador también apaga las luces si detecta que perdió su conexión al servidor.

## Configuración y ejecución

Completar `SSID_WIFI` y `CLAVE_WIFI` en `ConfigRed.h`. Las IP iniciales son:

| Equipo | IP |
| --- | --- |
| Puerta de enlace | `192.168.0.1` |
| Actuador | `192.168.0.100` |
| Sensor | `192.168.0.101` |
| Computadora con el servidor | `192.168.0.102` |

Las IP deben ajustarse a la red real. `IP_SERVIDOR` debe ser la IP de la computadora y el puerto debe coincidir en ambos lados; inicialmente es `5000`. Con `USAR_IP_FIJA = false`, el router asigna las IP de las placas por DHCP.

Desde la raíz del proyecto:

```powershell
pio run
python src/servidor/server.py
```

El servidor escucha en `0.0.0.0:5000`; `0.0.0.0` significa todas las interfaces de la computadora, no la dirección que se configura en las placas.

Para cargar cada firmware, cambiar los puertos de ejemplo por los reales:

```powershell
pio run -e sensor -t upload --upload-port COM3
pio run -e actuador -t upload --upload-port COM4
```

El monitor serie utiliza 115200 baudios. Python solo necesita su biblioteca estándar. La computadora y las placas deben poder comunicarse en la red local y el firewall debe permitir el puerto del servidor.

## Pruebas

Las pruebas verifican las reglas originales, los mensajes TCP y las salidas fijas de los LEDs:

```powershell
python -m unittest discover -s pruebas -p "test_*.py" -v
```

Las pruebas nativas usan el código real del sensor y del indicador con GPIO y eco simulados:

```powershell
New-Item -ItemType Directory -Force -Path .pio/pruebas | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I pruebas/soporte -I src/cliente pruebas/prueba_cliente.cpp src/cliente/IndicadorLeds.cpp src/cliente/SensorUltrasonico.cpp -o .pio/pruebas/cliente.exe
./.pio/pruebas/cliente.exe
```

La compilación y estas pruebas verifican el software; la comprobación física requiere cargar ambos ESP32 y probarlos con la red y el montaje reales.

El archivo `doc/informe.md` conserva el informe histórico de la versión con una sola placa.
