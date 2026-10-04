import math
import socket
import threading
import unittest
from unittest.mock import patch

from src.servidor.server import (
    CERCANO, MEDIO, LEJANO, ERROR, LONGITUD_MAXIMA_MENSAJE,
    ServidorTCP, clasificar_distancia, construir_comando_luces, interpretar_mensaje,
)


class PruebasClasificacion(unittest.TestCase):
    def test_limites_y_lecturas_invalidas(self):
        casos = [
            (None, ERROR), (math.nan, ERROR), (math.inf, ERROR), (-math.inf, ERROR),
            (-1, ERROR), (1.99, ERROR), (2, CERCANO), (19.99, CERCANO),
            (20, MEDIO), (39.99, MEDIO), (40, LEJANO), (200, LEJANO), (200.01, ERROR),
        ]
        for distancia, esperado in casos:
            with self.subTest(distancia=distancia):
                self.assertEqual(clasificar_distancia(distancia, ERROR), esperado)

    def test_histeresis(self):
        casos = [
            (CERCANO, 21.99, CERCANO), (CERCANO, 22, MEDIO), (CERCANO, 80, LEJANO),
            (MEDIO, 18, MEDIO), (MEDIO, 17.99, CERCANO),
            (MEDIO, 41.99, MEDIO), (MEDIO, 42, LEJANO),
            (LEJANO, 38, LEJANO), (LEJANO, 37.99, MEDIO), (LEJANO, 5, CERCANO),
        ]
        for anterior, distancia, esperado in casos:
            with self.subTest(anterior=anterior, distancia=distancia):
                self.assertEqual(clasificar_distancia(distancia, anterior), esperado)

    def test_interpretacion_de_argumentos(self):
        self.assertEqual(interpretar_mensaje(" POST distance = 9\r "), ("POST", {"distance": "9"}))
        self.assertEqual(interpretar_mensaje("REGISTER type=sensor"), ("REGISTER", {"type": "sensor"}))
        self.assertEqual(interpretar_mensaje("  "), (None, {}))
        for mensaje in ["POST distance=1,distance=2", "POST distance=", "POST valor", "POST distance==1"]:
            with self.subTest(mensaje=mensaje), self.assertRaises(ValueError):
                interpretar_mensaje(mensaje)


class PruebasIntegracionTCP(unittest.TestCase):
    def setUp(self):
        self.registro = patch("src.servidor.server.registrar_evento")
        self.registro.start()
        self.addCleanup(self.registro.stop)
        self.servidor = ServidorTCP("127.0.0.1", 0, tiempo_maximo_sin_lecturas=0.7)
        self.hilo = threading.Thread(target=self.servidor.iniciar)
        self.hilo.start()
        self.addCleanup(self.cerrar_servidor)
        self.assertTrue(self.servidor.listo.wait(3), "El servidor no inicio")
        self.conexiones = []

    def cerrar_servidor(self):
        for conexion in getattr(self, "conexiones", []):
            conexion.close()
        self.servidor.detener()
        self.hilo.join(timeout=3)
        self.assertFalse(self.hilo.is_alive(), "El servidor no termino")

    def conectar(self, tipo=None):
        conexion = socket.create_connection(("127.0.0.1", self.servidor.puerto), timeout=2)
        conexion.settimeout(2)
        self.conexiones.append(conexion)
        if tipo:
            conexion.sendall(f"REGISTER type={tipo}\n".encode())
        return conexion

    def leer(self, conexion):
        datos = b""
        while not datos.endswith(b"\n"):
            recibido = conexion.recv(1)
            self.assertTrue(recibido, "El servidor cerro la conexion")
            datos += recibido
        return datos.decode().strip()

    def esperar_estado(self, conexion, rango):
        self.assertEqual(self.leer(conexion), construir_comando_luces(rango))

    def test_fragmentacion_y_varios_mensajes_en_un_envio(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        sensor = self.conectar()
        sensor.sendall(b"REGIS")
        sensor.sendall(b"TER type=sensor\r\nPOST dist")
        sensor.sendall(b"ance=9\nPOST distance=21\nPOST distance=22\nPOST distance=80\n")
        for rango in (CERCANO, CERCANO, MEDIO, LEJANO):
            self.esperar_estado(actuador, rango)

    def test_invalidas_apagan_y_reinician_histeresis(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        sensor = self.conectar("sensor")
        for texto in ("invalida", "nan", "inf", "-inf", "201", "0", "1e999"):
            sensor.sendall(b"POST distance=9\n")
            self.esperar_estado(actuador, CERCANO)
            sensor.sendall(f"POST distance={texto}\n".encode())
            self.esperar_estado(actuador, ERROR)
            sensor.sendall(b"POST distance=21\n")
            self.esperar_estado(actuador, MEDIO)

    def test_registro_tardio_recibe_el_estado_vigente(self):
        primero = self.conectar("actuator")
        self.esperar_estado(primero, ERROR)
        sensor = self.conectar("sensor")
        sensor.sendall(b"POST distance=55\n")
        self.esperar_estado(primero, LEJANO)
        segundo = self.conectar("actuator")
        self.esperar_estado(segundo, LEJANO)
        sensor.sendall(b"POST distance=invalida\n")
        self.esperar_estado(primero, ERROR)
        self.esperar_estado(segundo, ERROR)

    def test_desconexion_sensor_apaga_actuador(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        sensor = self.conectar("sensor")
        sensor.sendall(b"POST distance=30\n")
        self.esperar_estado(actuador, MEDIO)
        sensor.shutdown(socket.SHUT_RDWR)
        sensor.close()
        self.esperar_estado(actuador, ERROR)

    def test_silencio_sensor_apaga_y_descarta_estado_antiguo(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        sensor = self.conectar("sensor")
        sensor.sendall(b"POST distance=9\n")
        self.esperar_estado(actuador, CERCANO)
        self.esperar_estado(actuador, ERROR)
        tardio = self.conectar("actuator")
        self.esperar_estado(tardio, ERROR)
        sensor.sendall(b"POST distance=21\n")
        self.esperar_estado(actuador, MEDIO)
        self.esperar_estado(tardio, MEDIO)

    def test_cliente_sin_registro_y_actuador_no_publican(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        desconocido = self.conectar()
        desconocido.sendall(b"POST distance=5\n")
        actuador.sendall(b"POST distance=5\nREGISTER type=sensor\nPOST distance=5\n")
        actuador.settimeout(0.25)
        with self.assertRaises(socket.timeout):
            actuador.recv(1)

    def test_mensaje_excesivo_cierra_conexion(self):
        cliente = self.conectar()
        cliente.sendall(b"x" * (LONGITUD_MAXIMA_MENSAJE + 1))
        self.assertEqual(cliente.recv(1), b"")

    def test_ultimo_sensor_publicado_controla_las_luces(self):
        actuador = self.conectar("actuator")
        self.esperar_estado(actuador, ERROR)
        primero = self.conectar("sensor")
        segundo = self.conectar("sensor")
        primero.sendall(b"POST distance=5\n")
        self.esperar_estado(actuador, CERCANO)
        segundo.sendall(b"POST distance=80\n")
        self.esperar_estado(actuador, LEJANO)
        primero.shutdown(socket.SHUT_RDWR)
        primero.close()
        segundo.sendall(b"POST distance=30\n")
        self.esperar_estado(actuador, MEDIO)

    def test_registro_incompleto_agota_espera(self):
        with patch("src.servidor.server.TIEMPO_MAXIMO_REGISTRO", 0.1):
            cliente = self.conectar()
            cliente.sendall(b"REGISTER type=desconocido\n")
            self.assertEqual(cliente.recv(1), b"")


if __name__ == "__main__":
    unittest.main()
