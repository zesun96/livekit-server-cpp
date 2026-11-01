using System.Globalization;

namespace LiveKit.Server;

/// <summary>Mutable token builder. Native SDK signs HS256; do not modify a builder concurrently.</summary>
public sealed class AccessToken
{
    private readonly Dictionary<string, object?> _fields;
    private readonly NativeServer? _native;
    public AccessToken(string apiKey, string apiSecret, NativeServer? native = null)
    {
        ArgumentNullException.ThrowIfNull(apiKey);
        ArgumentNullException.ThrowIfNull(apiSecret);
        _fields = new() { ["api_key"] = apiKey, ["api_secret"] = apiSecret, ["valid_for"] = "21600" };
        _native = native;
    }
    private AccessToken SetString(string field, string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        _fields[field] = value;
        return this;
    }
    public AccessToken SetIdentity(string value) => SetString("identity", value);
    public AccessToken SetName(string value) => SetString("name", value);
    public AccessToken SetMetadata(string value) => SetString("metadata", value);
    public AccessToken SetAttributes(IReadOnlyDictionary<string, string> attributes)
    {
        ArgumentNullException.ThrowIfNull(attributes);
        if (attributes.Any(pair => pair.Value is null)) throw new ArgumentException("attributes must contain strings", nameof(attributes));
        _fields["attributes"] = new Dictionary<string, string>(attributes);
        return this;
    }
    public AccessToken SetValidFor(long seconds)
    {
        if (seconds <= 0) throw new ArgumentOutOfRangeException(nameof(seconds));
        _fields["valid_for"] = seconds.ToString(CultureInfo.InvariantCulture);
        return this;
    }
    public AccessToken SetValidFor(TimeSpan duration)
    {
        if (duration.Ticks % TimeSpan.TicksPerSecond != 0)
            throw new ArgumentException("token lifetime must use whole seconds", nameof(duration));
        return SetValidFor(duration.Ticks / TimeSpan.TicksPerSecond);
    }
    public AccessToken SetVideoGrant(VideoGrant grant)
    {
        ArgumentNullException.ThrowIfNull(grant);
        _fields["video"] = grant.ToFields();
        return this;
    }
    public AccessToken SetSipGrant(SipGrant grant)
    {
        ArgumentNullException.ThrowIfNull(grant);
        _fields["sip"] = grant.ToFields();
        return this;
    }
    public AccessToken SetAgentGrant(AgentGrant grant)
    {
        ArgumentNullException.ThrowIfNull(grant);
        _fields["agent"] = grant.ToFields();
        return this;
    }
    public string ToJwt(DateTimeOffset? now = null) => ToJwt((now ?? DateTimeOffset.UtcNow).ToUnixTimeSeconds());
    public string ToJwt(long unixSeconds)
    {
        var options = new Dictionary<string, object?>(_fields)
        {
            ["now"] = unixSeconds.ToString(CultureInfo.InvariantCulture)
        };
        return (_native ?? NativeServer.Default).Invoke(new { operation = "token", options }).GetString()!;
    }
    public override string ToString() => "LiveKit.Server.AccessToken (credentials redacted)";
}
