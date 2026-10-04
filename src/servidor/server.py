#!/usr/bin/env python3
"""
Servidor TCP - Proyecto 2 IoT (medidor de proximidad distribuido)

Topología (según el diagrama del proyecto):
    ESP32 + sensor ultrasónico  (TCP Client, sensor)   ---\
                                                           >--  WiFi  --  Servidor TCP (Python)
    ESP32 + LEDs rojo/amarillo/verde (TCP Client, actuador) -/

Pila: My Protocol / TCP / IP / 802.11

Protocolo de aplicación (mensajes de texto, uno por línea, terminados en '\n'):

    REGISTER type=<sensor|actuator>                     Cliente  -> Servidor
    POST distance=<cm>                                  Sensor   -> Servidor
    SET redLed=<v>, yellowLed=<v>, greenLed=<v>         Servidor -> Actuador
        con <v> en {on, off, blink_2, blink_4}

Flujo:
    1. Cada ESP32 se conecta y envía REGISTER con su tipo.
    2. El sensor envía POST distance=<cm> en cada lectura.
    3. El servidor aplica las reglas de distancia y envía SET a todos
       los actuadores registrados.

Reglas de distancia: se replican las del informe del proyecto
(Config.h / ClasificadorDistancia.cpp):
    - fuera de 2..200 cm o valor no numérico -> ERROR   -> todo off
    - 2  <= d < 20 cm                        -> CERCANO -> rojo on
    - 20 <= d < 40 cm                        -> MEDIO   -> amarillo on
    - 40 <= d <= 200 cm                      -> LEJANO  -> verde on
    con histéresis de 2 cm (se mantiene un estado por cada sensor).

Uso:
    python3 server.py                 # escucha en 0.0.0.0:5000
    python3 server.py --port 8080
"""

import argparse
import socket
import threading
from datetime import datetime

# ---------------------------------------------------------------------------
# Configuración de red
# ---------------------------------------------------------------------------
HOST_POR_DEFECTO = "0.0.0.0"   # todas las interfaces (IP del PC: 192.168.0.102)
PUERTO_POR_DEFECTO = 5000
CODIFICACION = "utf-8"
TAM_BUFFER = 1024

# ---------------------------------------------------------------------------
# Reglas de distancia (mismos valores que src/cliente/Config.h)
# ---------------------------------------------------------------------------
UMBRAL_CERCA_MEDIO_CM = 20.0
UMBRAL_MEDIO_LEJOS_CM = 40.0
MARGEN_HISTERESIS_CM = 2.0
DISTANCIA_MINIMA_VALIDA_CM = 2.0
DISTANCIA_MAXIMA_VALIDA_CM = 200.0

CERCANO, MEDIO, LEJANO, ERROR = "CERCANO", "MEDIO", "LEJANO", "ERROR"

# Estado de los LEDs para cada rango -> (redLed, yellowLed, greenLed)
# Valores permitidos por el protocolo: on, off, blink_2, blink_4
LEDS_POR_RANGO = {
    CERCANO: ("on", "off", "off"),
    MEDIO:   ("off", "on", "off"),
    LEJANO:  ("off", "off", "on"),
    ERROR:   ("off", "off", "off"),
}

VALORES_LED_VALIDOS = {"on", "off", "blink_2", "blink_4"}
TIPOS_VALIDOS = {"sensor", "actuator"}


# ---------------------------------------------------------------------------
# Lógica de clasificación (equivalente a ClasificadorDistancia.cpp)
# ---------------------------------------------------------------------------
def clasificar_por_umbrales(distancia):
    if distancia < UMBRAL_CERCA_MEDIO_CM:
        return CERCANO
    if distancia < UMBRAL_MEDIO_LEJOS_CM:
        return MEDIO
    return LEJANO


def clasificar_distancia(distancia, rango_anterior):
    """distancia: float o None (lectura inválida)."""
    if distancia is None:
        return ERROR
    if distancia < DISTANCIA_MINIMA_VALIDA_CM or distancia > DISTANCIA_MAXIMA_VALIDA_CM:
        return ERROR

    if rango_anterior == CERCANO:
        if distancia >= UMBRAL_CERCA_MEDIO_CM + MARGEN_HISTERESIS_CM:
            return clasificar_por_umbrales(distancia)
        return CERCANO

    if rango_anterior == MEDIO:
        if distancia < UMBRAL_CERCA_MEDIO_CM - MARGEN_HISTERESIS_CM:
            return CERCANO
        if distancia >= UMBRAL_MEDIO_LEJOS_CM + MARGEN_HISTERESIS_CM:
            return LEJANO
        return MEDIO

    if rango_anterior == LEJANO:
        if distancia < UMBRAL_MEDIO_LEJOS_CM - MARGEN_HISTERESIS_CM:
            return clasificar_por_umbrales(distancia)
        return LEJANO

    # rango_anterior == ERROR (o desconocido): límites base
    return clasificar_por_umbrales(distancia)


def construir_set(rango):
    rojo, amarillo, verde = LEDS_POR_RANGO[rango]
    for v in (rojo, amarillo, verde):
        assert v in VALORES_LED_VALIDOS, f"Valor de LED inválido: {v}"
    return f"SET redLed={rojo}, yellowLed={amarillo}, greenLed={verde}"


# ---------------------------------------------------------------------------
# Parser del protocolo
# ---------------------------------------------------------------------------
def parsear_mensaje(linea):
    """
    Convierte 'POST distance = 9' o 'REGISTER type=sensor' en
    ('POST', {'distance': '9'}). Tolera espacios alrededor de '=' y comas.
    Devuelve (None, {}) si la línea está vacía.
    """
    linea = linea.strip()
    if not linea:
        return None, {}

    partes = linea.split(None, 1)
    comando = partes[0].upper()
    argumentos = {}

    if len(partes) > 1:
        for par in partes[1].split(","):
            if "=" not in par:
                continue
            clave, valor = par.split("=", 1)
            argumentos[clave.strip().lower()] = valor.strip()

    return comando, argumentos


# ---------------------------------------------------------------------------
# Servidor
# ---------------------------------------------------------------------------
def log(msg):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)


class Cliente:
    def __init__(self, conn, addr):
        self.conn = conn
        self.addr = addr
        self.tipo = None              # 'sensor' | 'actuator'
        self.rango = ERROR            # estado de histéresis (solo sensores)
        self.lock_envio = threading.Lock()

    @property
    def nombre(self):
        return f"{self.addr[0]}:{self.addr[1]}" + (f" ({self.tipo})" if self.tipo else "")

    def enviar(self, texto):
        with self.lock_envio:
            self.conn.sendall((texto + "\n").encode(CODIFICACION))


class ServidorTCP:
    def __init__(self, host, puerto):
        self.host = host
        self.puerto = puerto
        self.clientes = []                 # lista de Cliente
        self.lock = threading.Lock()
        self.ultimo_set = None             # se reenvía a actuadores que se registran tarde

    # ---- ciclo principal -------------------------------------------------
    def iniciar(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((self.host, self.puerto))
        srv.listen()
        log(f"Servidor TCP escuchando en {self.host}:{self.puerto}")
        try:
            while True:
                conn, addr = srv.accept()
                conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                cliente = Cliente(conn, addr)
                with self.lock:
                    self.clientes.append(cliente)
                log(f"Conexión nueva desde {cliente.nombre}")
                threading.Thread(target=self.atender, args=(cliente,), daemon=True).start()
        except KeyboardInterrupt:
            log("Servidor detenido por el usuario")
        finally:
            with self.lock:
                for c in self.clientes:
                    try:
                        c.conn.close()
                    except OSError:
                        pass
            srv.close()

    # ---- atención de cada cliente ---------------------------------------
    def atender(self, cliente):
        buffer = ""
        try:
            while True:
                datos = cliente.conn.recv(TAM_BUFFER)
                if not datos:
                    break
                buffer += datos.decode(CODIFICACION, errors="replace")
                # Un mensaje por línea; acepta '\n' y '\r\n'
                while "\n" in buffer:
                    linea, buffer = buffer.split("\n", 1)
                    self.procesar(cliente, linea.rstrip("\r"))
        except (ConnectionResetError, OSError) as e:
            log(f"Error con {cliente.nombre}: {e}")
        finally:
            self.desconectar(cliente)

    def desconectar(self, cliente):
        with self.lock:
            if cliente in self.clientes:
                self.clientes.remove(cliente)
        try:
            cliente.conn.close()
        except OSError:
            pass
        log(f"Desconectado {cliente.nombre}")

    # ---- procesamiento de comandos --------------------------------------
    def procesar(self, cliente, linea):
        comando, args = parsear_mensaje(linea)
        if comando is None:
            return
        log(f"<- {cliente.nombre}: {linea.strip()}")

        if comando == "REGISTER":
            self.cmd_register(cliente, args)
        elif comando == "POST":
            self.cmd_post(cliente, args)
        else:
            log(f"   Comando desconocido '{comando}' ignorado")

    def cmd_register(self, cliente, args):
        tipo = args.get("type", "").lower()
        if tipo not in TIPOS_VALIDOS:
            log(f"   REGISTER con tipo inválido '{tipo}' (esperado: sensor | actuator)")
            return
        cliente.tipo = tipo
        cliente.rango = ERROR
        log(f"   Registrado {cliente.nombre}")

        # Si ya hay un estado calculado, el actuador lo recibe al registrarse
        if tipo == "actuator" and self.ultimo_set:
            self.enviar_a(cliente, self.ultimo_set)

    def cmd_post(self, cliente, args):
        if cliente.tipo != "sensor":
            log("   POST ignorado: el cliente no está registrado como sensor")
            return

        texto = args.get("distance")
        try:
            distancia = float(texto)
        except (TypeError, ValueError):
            distancia = None
            log(f"   Distancia no numérica '{texto}', se trata como lectura inválida")

        cliente.rango = clasificar_distancia(distancia, cliente.rango)
        mensaje_set = construir_set(cliente.rango)
        self.ultimo_set = mensaje_set
        log(f"   Regla aplicada: {distancia} cm -> {cliente.rango}")

        actuadores = self.obtener_actuadores()
        if not actuadores:
            log("   No hay actuadores registrados; SET no enviado")
            return
        for act in actuadores:
            self.enviar_a(act, mensaje_set)

    # ---- utilidades -----------------------------------------------------
    def obtener_actuadores(self):
        with self.lock:
            return [c for c in self.clientes if c.tipo == "actuator"]

    def enviar_a(self, cliente, mensaje):
        try:
            cliente.enviar(mensaje)
            log(f"-> {cliente.nombre}: {mensaje}")
        except OSError as e:
            log(f"   No se pudo enviar a {cliente.nombre}: {e}")
            self.desconectar(cliente)


def main():
    parser = argparse.ArgumentParser(description="Servidor TCP - Proyecto 2 IoT")
    parser.add_argument("--host", default=HOST_POR_DEFECTO, help="IP de escucha (por defecto 0.0.0.0)")
    parser.add_argument("--port", type=int, default=PUERTO_POR_DEFECTO, help="Puerto TCP (por defecto 5000)")
    a = parser.parse_args()
    ServidorTCP(a.host, a.port).iniciar()


if __name__ == "__main__":
    main()
