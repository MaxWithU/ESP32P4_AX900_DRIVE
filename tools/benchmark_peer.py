#!/usr/bin/env python3
"""Local LAN peer for synthetic AX900 tests. No telemetry and no request logs.
Run on a computer on the same LAN; specify its LAN address with --bind.
TCP: AX9U / AX9D + big-endian byte count + 4 opaque bytes.
UDP: echoes AX9P synthetic datagrams. HTTP: GET /health returns 204.
The peer acknowledges upload bytes only after receiving and checking the payload.
"""
import argparse
import socketserver
import struct
import threading

LIMIT = 8 * 1024 * 1024

def receive(sock, size):
    data = bytearray()
    while len(data) < size:
        part = sock.recv(size - len(data))
        if not part:
            raise EOFError()
        data.extend(part)
    return bytes(data)

class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        if not self.server.slots.acquire(blocking=False):
            return
        try:
            self.request.settimeout(30)
            magic = receive(self.request, 4)
            if magic == b'GET ':
                header = bytearray(magic)
                while b'\r\n\r\n' not in header and len(header) < 2048:
                    header.extend(receive(self.request, 1))
                healthy = header.startswith(b'GET /health HTTP/1.')
                status = b'204 No Content' if healthy else b'404 Not Found'
                self.request.sendall(b'HTTP/1.1 ' + status + b'\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
                return
            if magic not in (b'AX9U', b'AX9D'):
                return
            size = struct.unpack('!I', receive(self.request, 4))[0]
            receive(self.request, 4)
            if not 1024 <= size <= LIMIT:
                return
            left = size
            while left:
                n = min(left, 4096)
                if magic == b'AX9U':
                    if receive(self.request, n) != b'\xa5' * n:
                        return
                else:
                    self.request.sendall(b'\xa5' * n)
                left -= n
            if magic == b'AX9U':
                self.request.sendall(struct.pack('!I', size))
        except (OSError, EOFError):
            pass
        finally:
            self.server.slots.release()

class TCP(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    def __init__(self, address):
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(address, Handler)

class Echo(socketserver.BaseRequestHandler):
    def handle(self):
        data, sock = self.request
        if 12 <= len(data) <= 1200 and data.startswith(b'AX9P'):
            sock.sendto(data, self.client_address)

class UDP(socketserver.UDPServer):
    allow_reuse_address = True

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bind', default='127.0.0.1')
    p.add_argument('--port', type=int, default=5201)
    args = p.parse_args()
    with TCP((args.bind, args.port)) as tcp, UDP((args.bind, args.port), Echo) as udp:
        threading.Thread(target=udp.serve_forever, daemon=True).start()
        print('Local synthetic TCP/HTTP/UDP peer ready', flush=True)
        try:
            tcp.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            udp.shutdown()

if __name__ == '__main__':
    main()
