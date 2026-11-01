using LiveKit.Server;

// Read-only example: run explicitly with LIVEKIT_URL and API key/secret or LIVEKIT_TOKEN.
var rooms = new LiveKitApi().Room.ListRooms().ToJsonElement();
if (rooms.TryGetProperty("rooms", out var values))
    foreach (var room in values.EnumerateArray())
        Console.WriteLine(room.GetProperty("name").GetString());
