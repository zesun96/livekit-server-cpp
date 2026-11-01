using System.Text.Json;

namespace LiveKit.Server;

/// <summary>Immutable, distinct protocol models using canonical protobuf JSON.</summary>
public abstract class ProtocolModel
{
    public string Json { get; }
    protected ProtocolModel(string json) => Json = json ?? throw new ArgumentNullException(nameof(json));
    protected ProtocolModel(object fields)
    {
        ArgumentNullException.ThrowIfNull(fields);
        Json = JsonSerializer.Serialize(fields);
    }
    public JsonElement ToJsonElement()
    {
        using var document = JsonDocument.Parse(Json);
        if (document.RootElement.ValueKind != JsonValueKind.Object)
            throw new JsonException("model JSON must contain an object");
        return document.RootElement.Clone();
    }
    public bool IsEmpty => Json is "" or "{}";
    public override string ToString() => Json;
    public override bool Equals(object? value) =>
        value is ProtocolModel model && GetType() == model.GetType() && Json == model.Json;
    public override int GetHashCode() => HashCode.Combine(GetType(), Json);
}
