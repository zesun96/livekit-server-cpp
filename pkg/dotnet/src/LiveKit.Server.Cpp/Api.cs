using System.Globalization;

namespace LiveKit.Server;

public sealed class HttpRequest
{
    public string Url { get; }
    public string Method { get; }
    public IReadOnlyList<KeyValuePair<string, string>> Headers { get; }
    public ReadOnlyMemory<byte> Body { get; }
    public long TimeoutMs { get; }

    internal HttpRequest(string url, string method, KeyValuePair<string, string>[] headers,
                         byte[] body, long timeoutMs)
    {
        Url = url;
        Method = method;
        Headers = Array.AsReadOnly(headers);
        Body = body;
        TimeoutMs = timeoutMs;
    }
}

public sealed class HttpResponse
{
    public int StatusCode { get; }
    public ReadOnlyMemory<byte> Body { get; }
    public HttpResponse(int statusCode, ReadOnlyMemory<byte> body = default)
    {
        StatusCode = statusCode;
        Body = body.ToArray();
    }
}

public sealed class ApiOptions
{
    private string _url = "", _apiKey = "", _apiSecret = "", _accessToken = "";
    private long _timeoutMs = 30_000;
    public string Url { get => _url; init => _url = value ?? throw new ArgumentNullException(nameof(Url)); }
    public string ApiKey { get => _apiKey; init => _apiKey = value ?? throw new ArgumentNullException(nameof(ApiKey)); }
    public string ApiSecret { get => _apiSecret; init => _apiSecret = value ?? throw new ArgumentNullException(nameof(ApiSecret)); }
    public string AccessToken { get => _accessToken; init => _accessToken = value ?? throw new ArgumentNullException(nameof(AccessToken)); }
    public long TimeoutMs
    {
        get => _timeoutMs;
        init => _timeoutMs = value > 0 ? value : throw new ArgumentOutOfRangeException(nameof(TimeoutMs));
    }
    public Func<HttpRequest, HttpResponse>? Transport { get; init; }
    internal object ToFields() => new
    {
        url = Url,
        api_key = ApiKey,
        api_secret = ApiSecret,
        access_token = AccessToken,
        timeout_ms = TimeoutMs.ToString(CultureInfo.InvariantCulture),
        use_transport = Transport is not null
    };
    public override string ToString() => "LiveKit.Server.ApiOptions (credentials redacted)";
}

public sealed class LiveKitApi
{
    private readonly ApiOptions _options;
    private readonly NativeServer? _native;
    public RoomServiceClient Room { get; }
    public EgressClient Egress { get; }
    public IngressClient Ingress { get; }
    public SipClient Sip { get; }
    public AgentDispatchClient AgentDispatch { get; }
    public ConnectorClient Connector { get; }

    public LiveKitApi(ApiOptions? options = null, NativeServer? native = null)
    {
        _options = options ?? new ApiOptions();
        _native = native;
        Room = new(this);
        Egress = new(this);
        Ingress = new(this);
        Sip = new(this);
        AgentDispatch = new(this);
        Connector = new(this);
    }

    internal string Call(string operation, ProtocolModel request)
    {
        ArgumentNullException.ThrowIfNull(request);
        return (_native ?? NativeServer.Default).Invoke(new
        {
            operation,
            options = _options.ToFields(),
            request = request.Json
        }, transport: _options.Transport).GetString()!;
    }

    internal Models.AgentDispatch? GetDispatch(string dispatchId, string room)
    {
        ArgumentNullException.ThrowIfNull(dispatchId);
        ArgumentNullException.ThrowIfNull(room);
        var result = (_native ?? NativeServer.Default).Invoke(new
        {
            operation = "get_dispatch",
            dispatch_id = dispatchId,
            room,
            options = _options.ToFields()
        }, transport: _options.Transport);
        return result.ValueKind == System.Text.Json.JsonValueKind.Null ? null : new(result.GetString()!);
    }
}
