"""Verified webhook snapshots with exact raw payloads and synchronous host callbacks."""
from __future__ import annotations

from dataclasses import dataclass
import threading
from types import MappingProxyType
from typing import Callable, Mapping

from ._native import NativeLibrary, _b64, get_native


@dataclass(frozen=True, slots=True)
class WebhookRoom:
    sid: str
    name: str
    metadata: str


@dataclass(frozen=True, slots=True)
class WebhookParticipant:
    sid: str
    identity: str
    name: str
    metadata: str


@dataclass(frozen=True, slots=True)
class WebhookTrack:
    sid: str
    name: str


@dataclass(frozen=True, slots=True)
class WebhookEgress:
    egress_id: str
    room_id: str
    room_name: str


@dataclass(frozen=True, slots=True)
class WebhookIngress:
    ingress_id: str
    name: str
    room_name: str


@dataclass(frozen=True, slots=True)
class WebhookEvent:
    event: str
    id: str
    created_at: int
    raw_body: str
    room: WebhookRoom | None = None
    participant: WebhookParticipant | None = None
    track: WebhookTrack | None = None
    egress: WebhookEgress | None = None
    ingress: WebhookIngress | None = None


class WebhookReceiver:
    def __init__(self, *, api_key: str | None = None, api_secret: str | None = None,
                 secrets: Mapping[str, str] | None = None,
                 key_provider: Callable[[str], str | None] | None = None,
                 callback: Callable[[WebhookEvent], None] | None = None,
                 native: NativeLibrary | None = None):
        modes = (api_key is not None or api_secret is not None, secrets is not None, key_provider is not None)
        if sum(modes) != 1:
            raise ValueError("provide key/secret, a secrets mapping, or a key_provider")
        if key_provider is not None and not callable(key_provider):
            raise TypeError("key_provider must be callable")
        selected = secrets if secrets is not None else ({} if key_provider is not None else {api_key: api_secret})
        if not isinstance(selected, Mapping) or not all(
                isinstance(k, str) and isinstance(v, str) for k, v in selected.items()):
            raise TypeError("secrets must be a string-to-string mapping")
        self._secrets = MappingProxyType(dict(selected))
        self._key_provider = key_provider
        self._native = native
        self._callback_lock = threading.Lock()
        self._callback = None
        self.set_callback(callback)

    def set_callback(self, callback: Callable[[WebhookEvent], None] | None) -> WebhookReceiver:
        if callback is not None and not callable(callback):
            raise TypeError("callback must be callable or None")
        with self._callback_lock:
            self._callback = callback
        return self

    def receive(self, body: bytes | str, authorization: str) -> WebhookEvent:
        if isinstance(body, str):
            body = body.encode("utf-8")
        if not isinstance(body, bytes) or not isinstance(authorization, str):
            raise TypeError("body must be bytes or str; authorization must be a string")
        result = get_native(self._native).invoke(
            {"operation": "webhook", "options": {"body": _b64(body), "authorization": authorization,
             "secrets": dict(self._secrets), "use_key_provider": self._key_provider is not None}},
            key_provider=self._key_provider)
        values = dict(result)
        values["created_at"] = int(values["created_at"])
        for name, cls in (("room", WebhookRoom), ("participant", WebhookParticipant),
                          ("track", WebhookTrack), ("egress", WebhookEgress), ("ingress", WebhookIngress)):
            if name in values:
                values[name] = cls(**values[name])
        return WebhookEvent(**values)

    def dispatch(self, event: WebhookEvent) -> None:
        if not isinstance(event, WebhookEvent):
            raise TypeError("expected WebhookEvent")
        with self._callback_lock:
            callback = self._callback
        if callback is not None:
            callback(event)

    def receive_and_dispatch(self, body: bytes | str, authorization: str) -> WebhookEvent:
        event = self.receive(body, authorization)
        self.dispatch(event)
        return event
