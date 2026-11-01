"""SDK errors retain stable C ABI category, HTTP status and Twirp code."""
from enum import IntEnum


class ErrorCode(IntEnum):
    INVALID_ARGUMENT = 0
    AUTHENTICATION = 1
    TRANSPORT = 2
    HTTP = 3
    PROTOCOL = 4
    UNSUPPORTED = 5
    UNKNOWN = 255


class Error(RuntimeError):
    def __init__(self, message: str, *, code: int, http_status: int = 0, twirp_code: str = ""):
        super().__init__(message)
        self.code = code
        self.http_status = http_status
        self.twirp_code = twirp_code

    @property
    def error_code(self) -> ErrorCode:
        try:
            return ErrorCode(self.code)
        except ValueError:
            return ErrorCode.UNKNOWN
