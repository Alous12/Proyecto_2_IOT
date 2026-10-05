import argparse
import math
import socketserver
import threading

DIRECCION_POR_DEFECTO = "0.0.0.0"
PUERTO_POR_DEFECTO = 5000
LONGITUD_MAXIMA_MENSAJE = 192
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


class AtencionCliente(socketserver.StreamRequestHandler):
    def handle(self):
        self.tipo = None
        self.rango = ERROR
        try:
            while True:
                datos = self.rfile.readline(LONGITUD_MAXIMA_MENSAJE + 2)
                if not datos or len(datos) > LONGITUD_MAXIMA_MENSAJE + 1:
                    break
                if not datos.endswith(b"\n") or b"\0" in datos:
                    break
                with self.server.bloqueo:
                    self.procesar(datos.decode("utf-8").strip())
        except (OSError, UnicodeError):
            pass
        finally:
            with self.server.bloqueo:
                self.server.actuadores.discard(self.request)
                if self.server.sensor_actual is self:
                    self.server.sensor_actual = None
                    self.server.publicar(ERROR)

    def procesar(self, linea):
        if self.tipo is None:
            if linea == "REGISTER type=sensor":
                self.tipo = "sensor"
            elif linea == "REGISTER type=actuator":
                self.tipo = "actuator"
                self.server.actuadores.add(self.request)
                self.request.sendall(self.server.ultimo_comando)
        elif self.tipo == "sensor" and linea.startswith("POST distance="):
            try:
                distancia = float(linea.split("=", 1)[1])
            except ValueError:
                distancia = None
            self.rango = clasificar_distancia(distancia, self.rango)
            self.server.sensor_actual = self
            self.server.publicar(self.rango)


class ServidorTCP(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, direccion, puerto):
        super().__init__((direccion, puerto), AtencionCliente)
        self.bloqueo = threading.Lock()
        self.actuadores = set()
        self.sensor_actual = None
        self.ultimo_comando = (construir_comando_luces(ERROR) + "\n").encode("utf-8")

    def publicar(self, rango):
        self.ultimo_comando = (construir_comando_luces(rango) + "\n").encode("utf-8")
        for conexion in list(self.actuadores):
            try:
                conexion.sendall(self.ultimo_comando)
            except OSError:
                self.actuadores.discard(conexion)


def principal():
    argumentos = argparse.ArgumentParser(description="Servidor TCP del proyecto IoT")
    argumentos.add_argument("--direccion", "--host", default=DIRECCION_POR_DEFECTO)
    argumentos.add_argument("--puerto", "--port", type=int, default=PUERTO_POR_DEFECTO)
    opciones = argumentos.parse_args()
    if not 1 <= opciones.puerto <= 65535:
        argumentos.error("El puerto debe estar entre 1 y 65535")
    try:
        with ServidorTCP(opciones.direccion, opciones.puerto) as servidor:
            print(f"Servidor escuchando en {opciones.direccion}:{opciones.puerto}", flush=True)
            servidor.serve_forever()
    except KeyboardInterrupt:
        print("Servidor detenido")
    except OSError as error:
        argumentos.exit(1, f"No se pudo iniciar el servidor: {error}\n")


if __name__ == "__main__":
    principal()
