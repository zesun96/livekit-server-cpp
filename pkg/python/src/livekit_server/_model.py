"""Shared model and grant support; protobuf JSON validation remains in the native SDK."""
from __future__ import annotations

from dataclasses import dataclass, fields
import json
from typing import Any, Mapping

from ._native import _encode


@dataclass(frozen=True, slots=True, init=False)
class ProtocolModel:
    json: str

    def __init__(self, value: Mapping[str, Any] | str | None = None, **keywords: Any):
        if value is not None and keywords:
            raise TypeError("pass a mapping/JSON string or keyword fields, not both")
        if value is None:
            value = keywords
        if isinstance(value, str):
            encoded = value
        elif isinstance(value, Mapping):
            encoded = _encode(dict(value)).decode("utf-8")
        else:
            raise TypeError("model must be a mapping or JSON string")
        object.__setattr__(self, "json", encoded)

    @classmethod
    def from_json(cls, value: str):
        if not isinstance(value, str):
            raise TypeError("JSON must be a string")
        return cls(value)

    def to_dict(self) -> dict[str, Any]:
        value = json.loads(self.json)
        if not isinstance(value, dict):
            raise ValueError("model JSON must contain an object")
        return value

    @property
    def empty(self) -> bool:
        return self.json in ("", "{}")

    def __str__(self) -> str:
        return self.json


class Grant:
    __slots__ = ()

    def __post_init__(self):
        for field in fields(self):
            value = getattr(self, field.name)
            kind = field.metadata["kind"]
            if kind == "optional_boolean" and value is None:
                continue
            if kind in ("boolean", "optional_boolean"):
                valid = type(value) is bool
            elif kind == "string":
                valid = isinstance(value, str)
            else:
                valid = isinstance(value, (tuple, list)) and all(isinstance(v, str) for v in value)
                if valid:
                    object.__setattr__(self, field.name, tuple(value))
            if not valid:
                raise TypeError(f"invalid grant field {field.name}")

    def to_dict(self) -> dict[str, Any]:
        return {field.name: getattr(self, field.name) for field in fields(self)
                if getattr(self, field.name) is not None}
