"""Length-delimited ctypes adapter for the public C ABI; no SDK implementation lives here."""
from __future__ import annotations

import base64
from contextlib import ExitStack
import ctypes as C
from ctypes.util import find_library
import json
import os
from pathlib import Path
import sys
import threading
from typing import TYPE_CHECKING, Any, Callable, Iterable

from .errors import Error

if TYPE_CHECKING:
    from .api import HttpRequest, HttpResponse

_Callback = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_size_t,
                       C.POINTER(C.c_void_p), C.POINTER(C.c_size_t))


def _encode(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")


def _b64(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).decode("ascii").rstrip("=")


def _unb64(value: str) -> bytes:
    return base64.b64decode(value + "=" * (-len(value) % 4), altchars=b"-_", validate=True)


class NativeLibrary:
    """Load ABI 1 from an explicit path or LIVEKIT_SERVER_PYTHON_LIBRARY.

    The library stays loaded for this object's lifetime. Calls own their result and callback
    state; the same instance supports concurrent and nested calls. No close() is required.
    On Windows, dll_directories lists additional directories containing native dependencies.
    """
    def __init__(self, path: str | os.PathLike[str] | None = None, *,
                 dll_directories: Iterable[str | os.PathLike[str]] | None = None):
        selected = path if path is not None else os.environ.get("LIVEKIT_SERVER_PYTHON_LIBRARY")
        if selected:
            library = str(Path(selected).expanduser().resolve())
        else:
            default = "livekitserver.dll" if os.name == "nt" else (
                "liblivekitserver.dylib" if sys.platform == "darwin" else "liblivekitserver.so")
            library = find_library("livekitserver") or default
        with ExitStack() as directories:
            if os.name == "nt":
                paths = list(dll_directories) if dll_directories is not None else list(filter(None,
                    os.environ.get("LIVEKIT_SERVER_PYTHON_DLL_DIRECTORIES", "").split(os.pathsep)))
                if selected:
                    paths.insert(0, Path(library).parent)
                for directory in paths:
                    directories.enter_context(os.add_dll_directory(str(Path(directory).resolve())))
            self._library = C.CDLL(library)
        try:
            abi_version = self._library.lks_abi_version
        except AttributeError as error:
            raise ImportError("native library is missing the LiveKit server C ABI exports") from error
        abi_version.argtypes = []
        abi_version.restype = C.c_int32
        if abi_version() != 1:
            raise ImportError("unsupported LiveKit server C ABI version (expected 1)")
        try:
            self._library.lks_free.argtypes = [C.c_void_p]
            self._library.lks_free.restype = None
            self._library.lks_invoke.argtypes = [C.c_char_p, C.c_size_t, _Callback,
                                               C.c_void_p, C.POINTER(C.c_size_t)]
            # Keep the allocated address: c_char_p converts it to bytes and loses ownership.
            self._library.lks_invoke.restype = C.c_void_p
        except AttributeError as error:
            raise ImportError("native library is missing the LiveKit server C ABI exports") from error

    def invoke(self, input: dict[str, Any], *,
               transport: Callable[[HttpRequest], HttpResponse] | None = None,
               key_provider: Callable[[str], str | None] | None = None) -> Any:
        # Strong references survive callback GC and remain alive until native invoke returns.
        buffers = []
        callback_errors: list[BaseException] = []

        @_Callback
        def exchange(_user, pointer, length, response, response_length):
            try:
                from .api import HttpRequest, HttpResponse
                request = json.loads(C.string_at(pointer, length).decode("utf-8"))
                if request["kind"] == "transport":
                    if transport is None:
                        raise TypeError("transport callback is required")
                    http = HttpRequest(url=request["url"], method=request["method"],
                                       headers=tuple(tuple(pair) for pair in request["headers"]),
                                       body=_unb64(request["body"]), timeout_ms=int(request["timeout_ms"]))
                    result = transport(http)
                    if not isinstance(result, HttpResponse):
                        raise TypeError("transport must return HttpResponse")
                    if type(result.status_code) is not int or not isinstance(result.body, bytes):
                        raise TypeError("HttpResponse requires an integer status and bytes body")
                    payload = {"status_code": str(result.status_code), "body": _b64(result.body)}
                elif request["kind"] == "key":
                    if key_provider is None:
                        raise TypeError("key_provider callback is required")
                    secret = key_provider(request["key"])
                    if secret is not None and not isinstance(secret, str):
                        raise TypeError("key_provider must return str or None")
                    payload = {"secret": secret or ""}
                else:
                    raise RuntimeError("unknown native callback kind")
                data = _encode(payload)
                buffer = C.create_string_buffer(data, len(data))
                buffers.append(buffer)
                response[0] = C.addressof(buffer)
                response_length[0] = len(data)
                return 0
            except BaseException as error:
                # Never unwind Python exceptions through ctypes/C++; rethrow after native cleanup.
                callback_errors.append(error)
                return 1

        data = _encode(input)
        size = C.c_size_t()
        output = self._library.lks_invoke(data, len(data), exchange, None, C.byref(size))
        try:
            if callback_errors:
                raise callback_errors[0]
            if not output:
                raise MemoryError("native C ABI could not allocate or encode its result")
            result = json.loads(C.string_at(output, size.value).decode("utf-8"))
            if not result["ok"]:
                raise Error(result["message"], code=int(result["code"]),
                            http_status=int(result["http_status"]), twirp_code=result["twirp_code"])
            return result["value"]
        finally:
            if output:
                self._library.lks_free(output)


_default_lock = threading.Lock()
_default: NativeLibrary | None = None


def get_native(native: NativeLibrary | None) -> NativeLibrary:
    if native is not None:
        return native
    global _default
    with _default_lock:
        if _default is None:
            _default = NativeLibrary()
        return _default
