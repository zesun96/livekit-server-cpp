using System.Text;
using System.Text.Json;

namespace LiveKit.Server;

internal static class Wire
{
    internal static readonly Encoding Utf8 = new UTF8Encoding(false, true);
    internal static readonly JsonSerializerOptions SnakeCase = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    internal static string Base64Url(ReadOnlySpan<byte> bytes) =>
        Convert.ToBase64String(bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_');

    internal static byte[] DecodeBase64Url(string text) =>
        Convert.FromBase64String(text.Replace('-', '+').Replace('_', '/') +
                                 new string('=', (4 - text.Length % 4) % 4));
}

public enum ErrorCode
{
    InvalidArgument = 0,
    Authentication = 1,
    Transport = 2,
    Http = 3,
    Protocol = 4,
    Unsupported = 5,
    Unknown = 255
}

public sealed class LiveKitException : Exception
{
    public int Code { get; }
    public ErrorCode ErrorCode => Enum.IsDefined((ErrorCode)Code) ? (ErrorCode)Code : ErrorCode.Unknown;
    public int HttpStatus { get; }
    public string TwirpCode { get; }

    public LiveKitException(string message, int code, int httpStatus = 0, string twirpCode = "")
        : base(message)
    {
        Code = code;
        HttpStatus = httpStatus;
        TwirpCode = twirpCode;
    }
}
