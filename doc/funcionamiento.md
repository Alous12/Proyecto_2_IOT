# Funcionamiento del proyecto

Se mantiene el comportamiento original: medir la distancia, clasificarla y encender el LED correspondiente. La diferencia es que ahora la clasificación se realiza en un servidor Python, mientras un ESP32 mide y otro controla las luces.

El código propio está en español y documentado: cada clase, método público y función de Python tiene un comentario o docstring que explica su responsabilidad. Se conservan los nombres requeridos por Arduino, Python y el protocolo, como `setup`, `loop`, `REGISTER`, `POST` y `SET`.

## Reglas originales

| Distancia inicial o condición | Resultado |
| --- | --- |
| Menor que 2 cm, mayor que 200 cm o lectura inválida, 3 veces seguidas | Todos los LEDs apagados. |
| Desde 2 cm hasta menos de 20 cm | LED rojo encendido. |
| Desde 20 cm hasta menos de 40 cm | LED amarillo encendido. |
| Desde 40 cm hasta 200 cm | LED verde encendido. |

Se conserva la histéresis de 2 cm: desde cercano cambia al alcanzar 22 cm; desde medio cambia a cercano por debajo de 18 cm y a lejano desde 42 cm; desde lejano vuelve a clasificar por debajo de 38 cm. Después de pasar a error se utilizan los límites base de 20 y 40 cm.

Los LEDs tienen luz fija. Como máximo se enciende uno. Para pasar a error y apagar los tres LEDs se necesitan 3 lecturas inválidas seguidas (unos 0,3 s): el HC-SR04 produce lecturas falsas aisladas, sin eco o de más de 200 cm, y sin este filtro los LEDs se apagarían un instante cada vez. Mientras no se llega a 3, se mantiene el último rango válido. Cualquier lectura válida reinicia el conteo.

## Responsabilidad de cada archivo

| Archivo | Función |
| --- | --- |
| `src/cliente/PrincipalSensor.cpp` | Inicializa el sensor, obtiene una lectura, la envía y espera 100 ms. |
| `src/cliente/SensorUltrasonico.h/.cpp` | Genera el pulso de disparo, mide el eco y devuelve una `LecturaDistancia` (centímetros y validez). |
| `src/cliente/PrincipalActuador.cpp` | Recibe las órdenes del servidor y las entrega al indicador. |
| `src/cliente/IndicadorLeds.h/.cpp` | Interpreta la orden `SET` y aplica `on`/`off` a cada LED. |
| `src/cliente/ClienteTCP.h/.cpp` | Gestiona Wi-Fi, conexión TCP, registro y mensajes de ambos ESP32. |
| `src/cliente/Config.h` | Define pines, intervalo de medición y límites físicos del sensor. |
| `src/cliente/ConfigRed.h` | Define credenciales, IP, puerto del servidor y longitud máxima de los mensajes. |
| `src/servidor/server.py` | Recibe distancias, aplica las reglas originales (algoritmo de control) y envía el estado de las luces. |
| `platformio.ini` | Selecciona los archivos para compilar el firmware `sensor` o `actuador`. |

El clasificador anterior del cliente se trasladó al servidor; no se mantiene una segunda clasificación en las placas. El actuador no conoce los rangos: solo ejecuta el estado de cada LED que indica el servidor.

Estructura de `server.py`:

| Elemento | Función |
| --- | --- |
| `clasificar_distancia()` | Algoritmo de control: rangos de 2–200 cm con histéresis de 2 cm. |
| `ControlDistancia` | Guarda el rango actual de un sensor y aplica el filtro de 3 lecturas inválidas seguidas. |
| `construir_comando_luces()` | Convierte un rango en el mensaje `SET`. |
| `interpretar_mensaje()` | Separa `COMANDO clave=valor, ...` en comando y argumentos. |
| `AtencionCliente` | Un hilo por conexión: lee líneas y atiende `REGISTER` y `POST`. |
| `ServidorTCP` | Guarda los actuadores conectados y el último comando, y se lo reenvía. |

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

Para medio enciende amarillo y para lejano enciende verde. Si no hubo eco, el sensor envía `POST distance=invalida`. A la tercera lectura inválida seguida, el servidor responde con los tres valores en `off`.

Cada mensaje termina en `\n` y tiene como máximo 192 caracteres; una línea más larga cierra la conexión. El módulo TCP conserva las partes de una línea hasta completarla; en Python, `socketserver` se encarga de aceptar las conexiones y leer las líneas. El servidor envía un `SET` por cada `POST`, y cuando un actuador se registra le envía de inmediato el último estado. Los mensajes desconocidos, o un `POST` de un cliente no registrado como sensor, se ignoran y aparecen como advertencia en la consola del servidor. El actuador acepta `on` u `off` en cada uno de los tres LEDs; cualquier otra orden se ignora sin cambiar las luces.

## Medición y control

El sensor conserva TRIG en GPIO25 y ECHO en GPIO26. Genera un pulso de 10 microsegundos y espera el eco hasta 30000 microsegundos. Calcula `distancia = duracion × 0.0343 / 2`, por el recorrido de ida y vuelta del sonido.

El sensor acepta físicamente de 2 a 400 cm. El servidor mantiene el rango de trabajo original de 2 a 200 cm: una medición física de 250 cm se transmite, pero cuenta como inválida (si se repite 3 veces, apaga todos los LEDs).

El actuador utiliza GPIO13 para rojo, GPIO27 para amarillo y GPIO14 para verde, cada LED con una resistencia de 220 Ω. Sus salidas permanecen en el estado recibido hasta una nueva orden o una desconexión detectada. No utiliza temporizadores de luces.

El ESP32 restablece el Wi-Fi automáticamente y el cliente reintenta la conexión TCP cada 3 s mientras esté desconectado. No se fuerza una reconexión por dejar de recibir mensajes ni se borra la histéresis por una pausa. Al detectar la desconexión del sensor que publicó el último dato, el servidor envía apagado; el actuador también apaga las luces si detecta que perdió su conexión al servidor.

## Configuración y ejecución

Completar `SSID_WIFI` y `CLAVE_WIFI` en `ConfigRed.h`. Las IP iniciales son:

| Equipo | IP |
| --- | --- |
| Puerta de enlace | `192.168.0.1` |
| Actuador | `192.168.0.100` |
| Sensor | `192.168.0.101` |
| Computadora con el servidor | `192.168.0.26` |

Las IP deben ajustarse a la red real. `IP_SERVIDOR` debe ser la IP de la computadora y el puerto debe coincidir en ambos lados; inicialmente es `5000`. Con `USAR_IP_FIJA = false`, el router asigna las IP de las placas por DHCP.

Desde la raíz del proyecto:

```powershell
pio run
python src/servidor/server.py
```

El servidor escucha en `0.0.0.0:5000`; `0.0.0.0` significa todas las interfaces de la computadora, no la dirección que se configura en las placas. Se pueden cambiar con `--direccion` y `--puerto`. La consola muestra, con la hora en milisegundos, las conexiones, los registros y cada cambio de rango; esto sirve para medir el tiempo de respuesta durante las pruebas.

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

Para probar el actuador sin el HC-SR04, con el servidor en ejecución, `simular_sensor.py` se registra como sensor y envía una distancia de cada rango (10, 30 y 60 cm) y luego lecturas inválidas, 4 s cada una:

```powershell
python pruebas/simular_sensor.py --direccion 127.0.0.1
```

La compilación y estas pruebas verifican el software; la comprobación física requiere cargar ambos ESP32 y probarlos con la red y el montaje reales.
