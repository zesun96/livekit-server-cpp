package io.livekit.server;

/** Native SDK failure, retaining ErrorCode, HTTP status, and Twirp code. */
public final class LiveKitException extends RuntimeException {
    private static final long serialVersionUID = 1L;
    public enum ErrorCode {
        INVALID_ARGUMENT, AUTHENTICATION, TRANSPORT, HTTP, PROTOCOL, UNSUPPORTED, UNKNOWN
    }
    private final int code;
    private final int httpStatus;
    private final String twirpCode;
    public LiveKitException(String message, int code, int httpStatus, String twirpCode) {
        super(message);
        this.code = code;
        this.httpStatus = httpStatus;
        this.twirpCode = twirpCode;
    }
    public int code() { return code; }
    public ErrorCode errorCode() { return code >= 0 && code < 6 ? ErrorCode.values()[code] : ErrorCode.UNKNOWN; }
    public int httpStatus() { return httpStatus; }
    public String twirpCode() { return twirpCode; }
}
