"""Opt-in loopback probe for a shared SDK. No LiveKit server or credentials required.

Usage: python test/transport/portable_smoke.py /absolute/path/to/livekitserver.dll
Provide dependency DLLs on PATH (or shared libraries on the OS loader path).
"""
import concurrent.futures
import ctypes
import http.server
import json
import os
from pathlib import Path
import sys
import threading


def main():
    library = Path(sys.argv[1]).resolve()
    # Python 3.8+ uses explicit DLL directories on Windows. Keep handles alive throughout probe.
    dll_dirs = []
    if os.name == 'nt':
        for directory in [str(library.parent), *os.environ.get('PATH', '').split(os.pathsep)]:
            if directory and Path(directory).is_dir():
                dll_dirs.append(os.add_dll_directory(directory))
    sdk = ctypes.CDLL(str(library))
    sdk.lks_abi_version.restype = ctypes.c_int32
    sdk.lks_invoke.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p,
                               ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    sdk.lks_invoke.restype = ctypes.c_void_p
    sdk.lks_free.argtypes = [ctypes.c_void_p]
    sdk.lks_free.restype = None
    assert sdk.lks_abi_version() == 1
    name = 'room-\u4e2d\u6587-\U0001f600'
    encoded_name = name.encode('utf-8')
    checks = []
    timeout_gate = threading.Event()

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def handle(self):
            try:
                super().handle()
            except (ConnectionResetError, ConnectionAbortedError):
                pass  # A timed-out client may abort the persistent connection.

        def log_message(self, *_):
            pass

        def do_POST(self):
            body = self.rfile.read(int(self.headers.get('Content-Length', '0')))
            checks.append((self.path, self.headers.get('Authorization'), body))
            if self.path.startswith('/timeout/'):
                timeout_gate.wait(5)
                status, response = 200, b''
            elif self.path.startswith('/deny/'):
                status, response = 403, b'{"code":"permission_denied","msg":"test denied"}'
            elif self.path.startswith('/redirect/'):
                status, response = 302, b''
            else:
                status, response = 200, bytes([18, len(encoded_name)]) + encoded_name
            self.send_response(status)
            if status == 302:
                self.send_header('Location', '/must-not-follow')
            self.send_header('Content-Type', 'application/protobuf')
            self.send_header('Content-Length', str(len(response)))
            self.end_headers()
            try:
                self.wfile.write(response)
            except OSError:
                pass  # The timeout client has already closed the socket.

    class Server(http.server.ThreadingHTTPServer):
        daemon_threads = False

    server = Server(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        def invoke(prefix='', timeout_ms='2000'):
            request = json.dumps({'operation': 'Room.CreateRoom',
                                  'request': json.dumps({'name': name}),
                                  'options': {'url': f'http://127.0.0.1:{server.server_port}{prefix}',
                                              'access_token': 'test-only-preissued-token',
                                              'timeout_ms': timeout_ms}}).encode('utf-8')
            size = ctypes.c_size_t()
            result = sdk.lks_invoke(request, len(request), None, None, ctypes.byref(size))
            assert result, 'native allocation failed'
            try:
                return json.loads(ctypes.string_at(result, size.value))
            finally:
                sdk.lks_free(result)

        response = invoke()
        assert response['ok'], response
        assert json.loads(response['value'])['name'] == name
        response = invoke('/deny')
        assert response['ok'] is False and response['code'] == 3 and response['http_status'] == 403
        assert response['twirp_code'] == 'permission_denied' and response['message'] == 'test denied'
        response = invoke('/redirect')
        assert response['ok'] is False and response['http_status'] == 302
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            for response in pool.map(lambda _: invoke(), range(12)):
                assert response['ok'] and json.loads(response['value'])['name'] == name
        try:
            response = invoke('/timeout', '100')
            assert response['ok'] is False and response['code'] == 2, response
        finally:
            timeout_gate.set()
        assert len(checks) == 16, len(checks)
        for path, authorization, body in checks:
            assert path.endswith('/twirp/livekit.RoomService/CreateRoom'), path
            assert authorization == 'Bearer test-only-preissued-token'
            assert body == bytes([10, len(encoded_name)]) + encoded_name, body
        print('Shared C ABI loopback probe passed: binary/Unicode, HTTP errors, redirects, timeout, concurrency (16 requests)')
    finally:
        timeout_gate.set()
        server.shutdown()
        server.server_close()
        thread.join()
        for directory in dll_dirs:
            directory.close()


if __name__ == '__main__':
    main()
