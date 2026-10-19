"""Local LibreTranslate contract fixture; never contacts an external service."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
import time
from urllib.parse import parse_qs

TEXT = {
    "en": "Notes: includes a debug menu. Open it with Touchpad + Options. Catalog credits: Pippo & DeCKerr97.",
    "pl": "Uwagi: zawiera menu debugowania. Otwórz je przyciskami Touchpad + Options. Autorzy katalogu: Pippo i DeCKerr97.",
    "es": "Notas: incluye un menú de depuración. Ábrelo con Touchpad + Options. Créditos del catálogo: Pippo y DeCKerr97.",
    "fr": "Notes : inclut un menu de débogage. Ouvrez-le avec Touchpad + Options. Crédits du catalogue : Pippo et DeCKerr97.",
    "nl": "Opmerkingen: bevat een debugmenu. Open het met Touchpad + Options. Cataloguscredits: Pippo en DeCKerr97.",
    "de": "Hinweise: enthält ein Debug-Menü. Öffnen mit Touchpad + Options. Katalog erstellt von Pippo und DeCKerr97.",
    "it": "Note: include un menu di debug. Aprilo con Touchpad + Options. Crediti del catalogo: Pippo e DeCKerr97.",
    "zh": "备注：包含调试菜单。按 Touchpad + Options 打开。目录制作：Pippo 和 DeCKerr97。",
    "ru": "Примечания: есть меню отладки. Открывается кнопками Touchpad + Options. Авторы каталога: Pippo и DeCKerr97.",
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"])).decode("utf-8")
        request = {key: value[0] for key, value in parse_qs(body, keep_blank_values=True).items()}
        self.server.requests.append(request)
        source, target = request.get("q", ""), request.get("target", "")
        if source.startswith("slow"):
            time.sleep(0.5)
        status = 200
        if source == "error":
            status, response = 429, {"error": "rate limited"}
        elif source == "malformed":
            response = {"translatedText": ["unexpected array"]}
        elif source == "nested":
            response = {"other": {"translatedText": "wrong"}}
        elif source == "oversized":
            response = {"translatedText": "x" * (128 * 1024)}
        else:
            value = TEXT.get(target, "unknown")
            response = {"translatedText": value if source.startswith("Notes:") else f"[{target}] {value} {source}"}
        data = json.dumps(response).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass


@contextmanager
def translation_server():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    with translation_server() as server:
        print(f"http://127.0.0.1:{server.server_port}/translate", flush=True)
        threading.Event().wait()
