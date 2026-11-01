"""Token builder; HS256 signing is delegated to the shared native SDK."""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
import time
from typing import Mapping

from ._native import NativeLibrary, get_native
from .grants import AgentGrant, SipGrant, VideoGrant


class AccessToken:
    def __init__(self, api_key: str, api_secret: str, *, native: NativeLibrary | None = None):
        if not isinstance(api_key, str) or not isinstance(api_secret, str):
            raise TypeError("API key and secret must be strings")
        self._fields = {"api_key": api_key, "api_secret": api_secret, "valid_for": "21600"}
        self._native = native

    def _set_string(self, field, value):
        if not isinstance(value, str):
            raise TypeError(f"{field} must be a string")
        self._fields[field] = value
        return self

    def set_identity(self, identity: str) -> AccessToken:
        return self._set_string("identity", identity)

    def set_name(self, name: str) -> AccessToken:
        return self._set_string("name", name)

    def set_metadata(self, metadata: str) -> AccessToken:
        return self._set_string("metadata", metadata)

    def set_attributes(self, attributes: Mapping[str, str]) -> AccessToken:
        if not isinstance(attributes, Mapping) or not all(
                isinstance(k, str) and isinstance(v, str) for k, v in attributes.items()):
            raise TypeError("attributes must be a string-to-string mapping")
        self._fields["attributes"] = dict(attributes)
        return self

    def set_valid_for(self, seconds: int | timedelta) -> AccessToken:
        if isinstance(seconds, timedelta):
            if seconds.microseconds:
                raise ValueError("token lifetime must use whole seconds")
            seconds = seconds.days * 86400 + seconds.seconds
        if type(seconds) is not int:
            raise TypeError("token lifetime must be integer seconds or timedelta")
        if seconds <= 0:
            raise ValueError("token lifetime must be positive")
        self._fields["valid_for"] = str(seconds)
        return self

    def _set_grant(self, name, grant, expected):
        if not isinstance(grant, expected):
            raise TypeError(f"expected {expected.__name__}")
        self._fields[name] = grant.to_dict()
        return self

    def set_video_grant(self, grant: VideoGrant) -> AccessToken:
        return self._set_grant("video", grant, VideoGrant)

    def set_sip_grant(self, grant: SipGrant) -> AccessToken:
        return self._set_grant("sip", grant, SipGrant)

    def set_agent_grant(self, grant: AgentGrant) -> AccessToken:
        return self._set_grant("agent", grant, AgentGrant)

    def to_jwt(self, *, now: int | datetime | None = None) -> str:
        if now is None:
            now = int(time.time())
        elif isinstance(now, datetime):
            if now.tzinfo is None or now.utcoffset() is None:
                raise ValueError("now must be a timezone-aware datetime")
            delta = now.astimezone(timezone.utc) - datetime(1970, 1, 1, tzinfo=timezone.utc)
            now = delta.days * 86400 + delta.seconds
        if type(now) is not int:
            raise TypeError("now must be integer Unix seconds or an aware datetime")
        return get_native(self._native).invoke(
            {"operation": "token", "options": {**self._fields, "now": str(now)}})

    def __repr__(self):
        return "<livekit_server.AccessToken credentials redacted>"
