"""Synchronous server clients with immutable options and binary HTTP callback snapshots."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from ._native import NativeLibrary, get_native
from . import models
from .services import (AgentDispatchClient, ConnectorClient, EgressClient, IngressClient,
                       RoomServiceClient, SipClient)


@dataclass(frozen=True, slots=True)
class HttpRequest:
    url: str
    method: str
    headers: tuple[tuple[str, str], ...]
    body: bytes
    timeout_ms: int


@dataclass(frozen=True, slots=True)
class HttpResponse:
    status_code: int
    body: bytes = b""


@dataclass(frozen=True, slots=True, repr=False)
class ApiOptions:
    url: str = ""
    api_key: str = ""
    api_secret: str = ""
    access_token: str = ""
    timeout_ms: int = 30_000
    transport: Callable[[HttpRequest], HttpResponse] | None = None

    def __post_init__(self):
        if not all(isinstance(v, str) for v in (self.url, self.api_key, self.api_secret, self.access_token)):
            raise TypeError("URL and credentials must be strings")
        if type(self.timeout_ms) is not int:
            raise TypeError("timeout_ms must be an integer")
        if not 0 < self.timeout_ms <= 2**63 - 1:
            raise ValueError("timeout_ms must be positive and fit a signed 64-bit integer")
        if self.transport is not None and not callable(self.transport):
            raise TypeError("transport must be callable")

    def _to_dict(self):
        return {"url": self.url, "api_key": self.api_key, "api_secret": self.api_secret,
                "access_token": self.access_token, "timeout_ms": str(self.timeout_ms),
                "use_transport": self.transport is not None}

    def __repr__(self):
        return "<livekit_server.ApiOptions credentials redacted>"


class LiveKitApi:
    """All six server services. Native state belongs to each call; no close() is required."""
    def __init__(self, options: ApiOptions | None = None, *, native: NativeLibrary | None = None,
                 **keywords):
        if options is not None and keywords:
            raise TypeError("pass ApiOptions or option keywords, not both")
        if options is not None and not isinstance(options, ApiOptions):
            raise TypeError("expected ApiOptions")
        self._options = options if options is not None else ApiOptions(**keywords)
        self._native = native
        self.room = RoomServiceClient(self)
        self.egress = EgressClient(self)
        self.ingress = IngressClient(self)
        self.sip = SipClient(self)
        self.agent_dispatch = AgentDispatchClient(self)
        self.connector = ConnectorClient(self)

    def _call(self, operation, request, expected, response):
        if not isinstance(request, expected):
            raise TypeError(f"expected {expected.__name__}")
        value = get_native(self._native).invoke(
            {"operation": operation, "options": self._options._to_dict(), "request": request.json},
            transport=self._options.transport)
        return response(value)

    def _get_dispatch(self, dispatch_id: str, room: str) -> models.AgentDispatch | None:
        if not isinstance(dispatch_id, str) or not isinstance(room, str):
            raise TypeError("dispatch_id and room must be strings")
        value = get_native(self._native).invoke(
            {"operation": "get_dispatch", "dispatch_id": dispatch_id, "room": room,
             "options": self._options._to_dict()}, transport=self._options.transport)
        return None if value is None else models.AgentDispatch(value)
