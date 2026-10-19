"""Small deterministic artwork server for the real downloader/decoder tests."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import struct
import threading
import time


def bitmap(width, height):
    stride = (width * 3 + 3) & ~3
    pixels = (b'\x35\x70\xc0' * width + b'\0' * (stride - width * 3)) * height
    return (b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54)
            + struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 0, 0, 0, 0)
            + pixels)


@contextmanager
def artwork_server():
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            if self.path == '/stall':
                time.sleep(2)
            if self.path == '/bad-until-import' or (self.path == '/retry' and requests.count('/retry') == 1):
                body = b'not an image' * 100
            else:
                dimensions = (80, 120) if self.path == '/good' else (160, 90)
                if self.path == '/boxart':
                    dimensions = (200, 280)
                if self.path == '/hero'  or self.path.startswith('/large'):
                    dimensions = (2560, 1440)
                body = bitmap(*dimensions)
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server, requests
    finally:
        server.shutdown()
        thread.join()
        server.server_close()
