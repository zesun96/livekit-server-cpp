using System.Collections.ObjectModel;
using System.Globalization;
using System.Text.Json;

namespace LiveKit.Server;

public sealed record WebhookRoom(string Sid, string Name, string Metadata);
public sealed record WebhookParticipant(string Sid, string Identity, string Name, string Metadata);
public sealed record WebhookTrack(string Sid, string Name);
public sealed record WebhookEgress(string EgressId, string RoomId, string RoomName);
public sealed record WebhookIngress(string IngressId, string Name, string RoomName);
public sealed record WebhookEvent(string Event, string Id, long CreatedAt, string RawBody,
    WebhookRoom? Room = null, WebhookParticipant? Participant = null, WebhookTrack? Track = null,
    WebhookEgress? Egress = null, WebhookIngress? Ingress = null);

public sealed class WebhookReceiver
{
    private readonly IReadOnlyDictionary<string, string> _secrets;
    private readonly Func<string, string?>? _keyProvider;
    private readonly NativeServer? _native;
    private readonly object _callbackLock = new();
    private Action<WebhookEvent>? _callback;

    public WebhookReceiver(string apiKey, string apiSecret, Action<WebhookEvent>? callback = null,
                           NativeServer? native = null)
        : this(new Dictionary<string, string>
        {
            [apiKey ?? throw new ArgumentNullException(nameof(apiKey))] =
                   apiSecret ?? throw new ArgumentNullException(nameof(apiSecret))
        }, callback, native)
    { }

    public WebhookReceiver(IReadOnlyDictionary<string, string> secrets,
                           Action<WebhookEvent>? callback = null, NativeServer? native = null)
    {
        ArgumentNullException.ThrowIfNull(secrets);
        if (secrets.Any(pair => pair.Value is null)) throw new ArgumentException("secrets must contain strings", nameof(secrets));
        _secrets = new ReadOnlyDictionary<string, string>(new Dictionary<string, string>(secrets));
        _callback = callback;
        _native = native;
    }

    public WebhookReceiver(Func<string, string?> keyProvider,
                           Action<WebhookEvent>? callback = null, NativeServer? native = null)
        : this(new Dictionary<string, string>(), callback, native) =>
            _keyProvider = keyProvider ?? throw new ArgumentNullException(nameof(keyProvider));

    public WebhookReceiver SetCallback(Action<WebhookEvent>? callback)
    {
        lock (_callbackLock) _callback = callback;
        return this;
    }

    public WebhookEvent Receive(string body, string authorization)
    {
        ArgumentNullException.ThrowIfNull(body);
        return Receive(Wire.Utf8.GetBytes(body), authorization);
    }

    public WebhookEvent Receive(ReadOnlyMemory<byte> body, string authorization)
    {
        ArgumentNullException.ThrowIfNull(authorization);
        var result = (_native ?? NativeServer.Default).Invoke(new
        {
            operation = "webhook",
            options = new
            {
                body = Wire.Base64Url(body.Span),
                authorization,
                secrets = _secrets,
                use_key_provider = _keyProvider is not null
            }
        }, keyProvider: _keyProvider);
        return new(result.GetProperty("event").GetString()!, result.GetProperty("id").GetString()!,
            long.Parse(result.GetProperty("created_at").GetString()!, CultureInfo.InvariantCulture),
            result.GetProperty("raw_body").GetString()!,
            Snapshot<WebhookRoom>(result, "room"), Snapshot<WebhookParticipant>(result, "participant"),
            Snapshot<WebhookTrack>(result, "track"), Snapshot<WebhookEgress>(result, "egress"),
            Snapshot<WebhookIngress>(result, "ingress"));
    }

    private static T? Snapshot<T>(JsonElement result, string field) where T : class =>
        result.TryGetProperty(field, out var value) ? value.Deserialize<T>(Wire.SnakeCase) : null;

    public void Dispatch(WebhookEvent value)
    {
        ArgumentNullException.ThrowIfNull(value);
        Action<WebhookEvent>? callback;
        lock (_callbackLock) callback = _callback;
        callback?.Invoke(value);
    }

    public WebhookEvent ReceiveAndDispatch(ReadOnlyMemory<byte> body, string authorization)
    {
        var value = Receive(body, authorization);
        Dispatch(value);
        return value;
    }
    public WebhookEvent ReceiveAndDispatch(string body, string authorization)
    {
        ArgumentNullException.ThrowIfNull(body);
        return ReceiveAndDispatch(Wire.Utf8.GetBytes(body), authorization);
    }
}
