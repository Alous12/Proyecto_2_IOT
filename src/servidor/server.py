import argparse
import math
import socket
import threading
import time
from datetime import datetime

DIRECCION_POR_DEFECTO = "0.0.0.0"
PUERTO_POR_DEFECTO = 5000
CODIFICACION = "utf-8"
TAMANO_BLOQUE = 1024
LONGITUD_MAXIMA_MENSAJE = 192
TIEMPO_MAXIMO_SIN_LECTURAS = 3.0
TIEMPO_MAXIMO_REGISTRO = 5.0
TIEMPO_ESPERA_SOCKET = 0.2

UMBRAL_CERCA_MEDIO_CM = 20.0
UMBRAL_MEDIO_LEJOS_CM = 40.0
MARGEN_HISTERESIS_CM = 2.0
DISTANCIA_MINIMA_VALIDA_CM = 2.0
DISTANCIA_MAXIMA_VALIDA_CM = 200.0

CERCANO, MEDIO, LEJANO, ERROR = "CERCANO", "MEDIO", "LEJANO", "ERROR"

LEDS_POR_RANGO = {
    CERCANO: ("on", "off", "off"),
    MEDIO: ("off", "on", "off"),
    LEJANO: ("off", "off", "on"),
    ERROR: ("off", "off", "off"),
}
TIPOS_VALIDOS = {"sensor", "actuator"}


def clasificar_por_umbrales(distancia):
    if distancia < UMBRAL_CERCA_MEDIO_CM:
        return CERCANO
    if distancia < UMBRAL_MEDIO_LEJOS_CM:
        return MEDIO
    return LEJANO


def clasificar_distancia(distancia, rango_anterior):
    if distancia is None or not math.isfinite(distancia):
        return ERROR
    if not DISTANCIA_MINIMA_VALIDA_CM <= distancia <= DISTANCIA_MAXIMA_VALIDA_CM:
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

    return clasificar_por_umbrales(distancia)


def construir_comando_luces(rango):
    rojo, amarillo, verde = LEDS_POR_RANGO[rango]
    return f"SET redLed={rojo}, yellowLed={amarillo}, greenLed={verde}"


def interpretar_mensaje(linea):
    linea = linea.strip()
    if not linea:
        return None, {}

    partes = linea.split(None, 1)
    comando = partes[0].upper()
    argumentos = {}
    if len(partes) == 2:
        for par in partes[1].split(","):
            if par.count("=") != 1:
                raise ValueError("Cada argumento debe tener una clave y un valor")
            clave, valor = (parte.strip() for parte in par.split("=", 1))
            clave = clave.lower()
            if not clave or not valor or clave in argumentos:
                raise ValueError("Argumento vacio o repetido")
            argumentos[clave] = valor
    return comando, argumentos


def registrar_evento(mensaje):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {mensaje}", flush=True)


class Cliente:
    def __init__(self, conexion, direccion):
        self.conexion = conexion
        self.direccion = direccion
        self.tipo = None
        self.rango = ERROR
        self.instante_conexion = time.monotonic()
        self.ultima_lectura = None

    @property
    def nombre(self):
        tipo = "actuador" if self.tipo == "actuator" else self.tipo or "sin registro"
        return f"{self.direccion[0]}:{self.direccion[1]} ({tipo})"

    def enviar(self, mensaje):
        self.conexion.sendall((mensaje + "\n").encode(CODIFICACION))


class ServidorTCP:
    def __init__(self, direccion, puerto, tiempo_maximo_sin_lecturas=TIEMPO_MAXIMO_SIN_LECTURAS):
        self.direccion = direccion
        self.puerto = puerto
        self.tiempo_maximo_sin_lecturas = tiempo_maximo_sin_lecturas
        self.clientes = []
        self.listo = threading.Event()
        self._bloqueo = threading.RLock()
        self._detenido = threading.Event()
        self._escucha = None
        self._hilos = []
        self._sensor_actual = None
        self._ultima_lectura = 0.0
        self._ultimo_comando = construir_comando_luces(ERROR)

    def iniciar(self):
        try:
            self._escucha = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._escucha.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._escucha.bind((self.direccion, self.puerto))
            self.puerto = self._escucha.getsockname()[1]
            self._escucha.listen()
            self._escucha.settimeout(TIEMPO_ESPERA_SOCKET)
            registrar_evento(f"Servidor TCP escuchando en {self.direccion}:{self.puerto}")
            self.listo.set()

            while not self._detenido.is_set():
                self.revisar_vigencia()
                try:
                    conexion, direccion = self._escucha.accept()
                except socket.timeout:
                    continue
                except OSError:
                    if self._detenido.is_set():
                        break
                    raise

                conexion.settimeout(TIEMPO_ESPERA_SOCKET)
                conexion.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                conexion.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                cliente = Cliente(conexion, direccion)
                with self._bloqueo:
                    if self._detenido.is_set():
                        conexion.close()
                        break
                    self.clientes.append(cliente)
                registrar_evento(f"Conexion nueva desde {cliente.nombre}")
                hilo = threading.Thread(target=self.atender, args=(cliente,), daemon=True)
                self._hilos = [anterior for anterior in self._hilos if anterior.is_alive()]
                self._hilos.append(hilo)
                hilo.start()
        except KeyboardInterrupt:
            registrar_evento("Servidor detenido por el usuario")
        finally:
            self.detener()
            for hilo in self._hilos:
                hilo.join(timeout=1.0)

    def detener(self):
        self._detenido.set()
        if self._escucha is not None:
            self._escucha.close()
        with self._bloqueo:
            clientes = list(self.clientes)
        for cliente in clientes:
            self.desconectar(cliente)

    def atender(self, cliente):
        pendiente = b""
        try:
            while not self._detenido.is_set():
                if cliente.tipo is None and (
                    time.monotonic() - cliente.instante_conexion >= TIEMPO_MAXIMO_REGISTRO
                ):
                    registrar_evento(f"Tiempo de registro agotado: {cliente.nombre}")
                    break
                try:
                    datos = cliente.conexion.recv(TAMANO_BLOQUE)
                except socket.timeout:
                    continue
                if not datos:
                    break
                pendiente += datos
                while b"\n" in pendiente:
                    linea, pendiente = pendiente.split(b"\n", 1)
                    if len(linea) > LONGITUD_MAXIMA_MENSAJE or b"\0" in linea:
                        raise ValueError("Mensaje demasiado largo o con caracteres nulos")
                    self.procesar(cliente, linea.decode(CODIFICACION).rstrip("\r"))
                if len(pendiente) > LONGITUD_MAXIMA_MENSAJE:
                    raise ValueError("Mensaje sin terminador demasiado largo")
        except (OSError, ValueError) as error:
            if not self._detenido.is_set():
                registrar_evento(f"Conexion terminada con {cliente.nombre}: {error}")
        finally:
            self.desconectar(cliente)

    def desconectar(self, cliente):
        with self._bloqueo:
            if cliente not in self.clientes:
                return
            self.clientes.remove(cliente)
            try:
                cliente.conexion.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            cliente.conexion.close()
            registrar_evento(f"Desconectado {cliente.nombre}")
            if cliente is self._sensor_actual and not self._detenido.is_set():
                self.invalidar_lectura("El sensor activo se desconecto")

    def procesar(self, cliente, linea):
        try:
            comando, argumentos = interpretar_mensaje(linea)
        except ValueError as error:
            registrar_evento(f"Mensaje rechazado de {cliente.nombre}: {error}")
            return
        if comando is None:
            return

        with self._bloqueo:
            if cliente not in self.clientes:
                return
            if comando == "REGISTER":
                self.registrar_cliente(cliente, argumentos)
            elif comando == "POST":
                self.recibir_distancia(cliente, argumentos)
            else:
                registrar_evento(f"Comando desconocido de {cliente.nombre}: {comando}")

    def registrar_cliente(self, cliente, argumentos):
        tipo = argumentos.get("type", "").lower()
        if set(argumentos) != {"type"} or tipo not in TIPOS_VALIDOS:
            registrar_evento(f"Registro invalido de {cliente.nombre}")
            return
        if cliente.tipo is not None:
            registrar_evento(f"El cliente ya esta registrado: {cliente.nombre}")
            return

        cliente.tipo = tipo
        cliente.rango = ERROR
        registrar_evento(f"Registrado {cliente.nombre}")
        if tipo == "actuator":
            self.revisar_vigencia()
            self.enviar_a(cliente, self._ultimo_comando)

    def recibir_distancia(self, cliente, argumentos):
        if cliente.tipo != "sensor":
            registrar_evento(f"Medicion rechazada: {cliente.nombre} no es un sensor")
            return

        try:
            distancia = float(argumentos["distance"]) if set(argumentos) == {"distance"} else None
        except (KeyError, ValueError):
            distancia = None

        if cliente.ultima_lectura is not None and (
            time.monotonic() - cliente.ultima_lectura >= self.tiempo_maximo_sin_lecturas
        ):
            cliente.rango = ERROR
        cliente.rango = clasificar_distancia(distancia, cliente.rango)
        self._sensor_actual = cliente
        self._ultima_lectura = time.monotonic()
        cliente.ultima_lectura = self._ultima_lectura
        self._ultimo_comando = construir_comando_luces(cliente.rango)
        registrar_evento(f"{cliente.nombre}: {distancia} cm -> {cliente.rango}")
        self.difundir_estado()

    def revisar_vigencia(self):
        with self._bloqueo:
            if self._sensor_actual is not None and (
                time.monotonic() - self._ultima_lectura >= self.tiempo_maximo_sin_lecturas
            ):
                self.invalidar_lectura("Se agoto el tiempo de espera de mediciones")

    def invalidar_lectura(self, motivo):
        if self._sensor_actual is not None:
            self._sensor_actual.rango = ERROR
        self._sensor_actual = None
        self._ultimo_comando = construir_comando_luces(ERROR)
        registrar_evento(motivo)
        self.difundir_estado()

    def difundir_estado(self):
        for cliente in list(self.clientes):
            if cliente.tipo == "actuator":
                self.enviar_a(cliente, self._ultimo_comando)

    def enviar_a(self, cliente, mensaje):
        try:
            cliente.enviar(mensaje)
        except OSError as error:
            registrar_evento(f"No se pudo enviar a {cliente.nombre}: {error}")
            self.desconectar(cliente)


def principal():
    argumentos = argparse.ArgumentParser(description="Servidor TCP del proyecto IoT")
    argumentos.add_argument(
        "--direccion", "--host", default=DIRECCION_POR_DEFECTO,
        help="Direccion de escucha; por defecto 0.0.0.0"
    )
    argumentos.add_argument(
        "--puerto", "--port", type=int, default=PUERTO_POR_DEFECTO,
        help="Puerto TCP; por defecto 5000"
    )
    opciones = argumentos.parse_args()
    if not 1 <= opciones.puerto <= 65535:
        argumentos.error("El puerto debe estar entre 1 y 65535")
    try:
        ServidorTCP(opciones.direccion, opciones.puerto).iniciar()
    except OSError as error:
        argumentos.exit(1, f"No se pudo iniciar el servidor: {error}\n")


if __name__ == "__main__":
    principal()
