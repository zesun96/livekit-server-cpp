using System.Globalization;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using LiveKit.Server;
using M = LiveKit.Server.Models;

internal static class Program
{
    private const string Key = "dotnet-test-key";
    private const string Secret = "dotnet-test-secret-at-least-32-characters";
    private static int _checks;
    private static NativeServer _native = null!;

    private static void Check(bool condition, string message)
    {
        Interlocked.Increment(ref _checks);
        if (!condition) throw new InvalidOperationException("check failed: " + message);
    }
    private static T Throws<T>(Action action) where T : Exception
    {
        try { action(); }
        catch (T error) { Interlocked.Increment(ref _checks); return error; }
        throw new InvalidOperationException("expected " + typeof(T).Name);
    }
    private static LiveKitException Failure(ErrorCode code, Action action)
    {
        var error = Throws<LiveKitException>(action);
        Check(error.ErrorCode == code, "SDK error category");
        return error;
    }
    private static string B64(ReadOnlySpan<byte> value) => Convert.ToBase64String(value).TrimEnd('=').Replace('+', '-').Replace('/', '_');
    private static byte[] Decode(string value) => Convert.FromBase64String(
        value.Replace('-', '+').Replace('_', '/') + new string('=', (4 - value.Length % 4) % 4));
    private static string Sign(object claims)
    {
        var input = B64(JsonSerializer.SerializeToUtf8Bytes(new { alg = "HS256", typ = "JWT" })) + "." +
                    B64(JsonSerializer.SerializeToUtf8Bytes(claims));
        return input + "." + B64(HMACSHA256.HashData(Encoding.UTF8.GetBytes(Secret), Encoding.UTF8.GetBytes(input)));
    }
    private static string Authorization(byte[] body, string issuer = Key, long? nbf = null, long? expiry = null)
    {
        var now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        return Sign(new
        {
            iss = issuer,
            nbf = nbf ?? now - 60,
            exp = expiry ?? now + 3600,
            sha256 = Convert.ToBase64String(SHA256.HashData(body))
        });
    }
    private static LiveKitApi Api(Func<HttpRequest, HttpResponse> transport, string token = "") =>
        new(new ApiOptions
        {
            Url = "ws://offline.invalid/",
            ApiKey = Key,
            ApiSecret = Secret,
            AccessToken = token,
            Transport = transport,
            TimeoutMs = 1234
        }, _native);

    private static void ManagedContracts()
    {
        var values = new Dictionary<string, object> { ["name"] = "room" };
        var model = new M.CreateRoomRequest(values);
        values["name"] = "changed";
        Check(model.ToJsonElement().GetProperty("name").GetString() == "room", "model snapshots input");
        Check(model.Equals(M.CreateRoomRequest.FromJson(model.Json)), "same-type model equality");
        Check(model.GetHashCode() == new M.CreateRoomRequest(model.Json).GetHashCode(), "same-type model hash");
        Check(!new M.Room().Equals(new M.CreateRoomRequest()), "distinct model types");
        Check(new M.Room().IsEmpty, "empty model");
        Throws<JsonException>(() => new M.Room("[]").ToJsonElement());
        Throws<ArgumentNullException>(() => new M.Room((string)null!));
        Throws<ArgumentOutOfRangeException>(() => new ApiOptions { TimeoutMs = 0 });
        Throws<ArgumentNullException>(() => new ApiOptions { Url = null! });
        Throws<ArgumentNullException>(() => new VideoGrant { Room = null! });
        Throws<ArgumentException>(() => new VideoGrant { CanPublishSources = new[] { (string)null! } });
        Throws<ArgumentOutOfRangeException>(() => new AccessToken(Key, Secret).SetValidFor(0));
        Throws<ArgumentException>(() => new AccessToken(Key, Secret).SetValidFor(TimeSpan.FromTicks(1)));
        Check(!new ApiOptions { ApiSecret = Secret }.ToString().Contains(Secret), "options redaction");
        Check(!new AccessToken(Key, Secret).ToString().Contains(Secret), "token redaction");
        Check(new LiveKitException("future", 123).ErrorCode == ErrorCode.Unknown, "future code fallback");
        // Lazy API/token/receiver construction must work even without a native SDK.
        _ = new LiveKitApi();
        _ = new AccessToken(Key, Secret);
        _ = new WebhookReceiver(Key, Secret);
        Check(true, "lazy native loading");
        Throws<DllNotFoundException>(() => new NativeServer(Path.GetFullPath("missing-dotnet-test-library")));
        if (OperatingSystem.IsWindows())
            Throws<EntryPointNotFoundException>(() => new NativeServer(Path.Combine(Environment.SystemDirectory, "kernel32.dll")));
    }

    private const string Routes = """
Room CreateRoom CreateRoomRequest CreateRoom
Room ListRooms ListRoomsRequest ListRooms
Room DeleteRoom DeleteRoomRequest DeleteRoom
Room ListParticipants ListParticipantsRequest ListParticipants
Room GetParticipant RoomParticipantIdentity GetParticipant
Room RemoveParticipant RoomParticipantIdentity RemoveParticipant
Room MutePublishedTrack MuteRoomTrackRequest MutePublishedTrack
Room UpdateParticipant UpdateParticipantRequest UpdateParticipant
Room UpdateSubscriptions UpdateSubscriptionsRequest UpdateSubscriptions
Room SendData SendDataRequest SendData
Room UpdateRoomMetadata UpdateRoomMetadataRequest UpdateRoomMetadata
Room ForwardParticipant ForwardParticipantRequest ForwardParticipant
Room MoveParticipant MoveParticipantRequest MoveParticipant
Room PerformRpc PerformRpcRequest PerformRpc
Egress StartEgress StartEgressRequest StartEgress
Egress StartRoomCompositeEgress RoomCompositeEgressRequest StartRoomCompositeEgress
Egress StartWebEgress WebEgressRequest StartWebEgress
Egress StartParticipantEgress ParticipantEgressRequest StartParticipantEgress
Egress StartTrackCompositeEgress TrackCompositeEgressRequest StartTrackCompositeEgress
Egress StartTrackEgress TrackEgressRequest StartTrackEgress
Egress UpdateLayout UpdateLayoutRequest UpdateLayout
Egress UpdateStream UpdateStreamRequest UpdateStream
Egress ListEgress ListEgressRequest ListEgress
Egress StopEgress StopEgressRequest StopEgress
Ingress CreateIngress CreateIngressRequest CreateIngress
Ingress UpdateIngress UpdateIngressRequest UpdateIngress
Ingress ListIngress ListIngressRequest ListIngress
Ingress DeleteIngress DeleteIngressRequest DeleteIngress
Sip CreateInboundTrunk CreateSIPInboundTrunkRequest CreateSIPInboundTrunk
Sip CreateOutboundTrunk CreateSIPOutboundTrunkRequest CreateSIPOutboundTrunk
Sip UpdateInboundTrunk UpdateSIPInboundTrunkRequest UpdateSIPInboundTrunk
Sip UpdateOutboundTrunk UpdateSIPOutboundTrunkRequest UpdateSIPOutboundTrunk
Sip GetInboundTrunk GetSIPInboundTrunkRequest GetSIPInboundTrunk
Sip GetOutboundTrunk GetSIPOutboundTrunkRequest GetSIPOutboundTrunk
Sip ListTrunks ListSIPTrunkRequest ListSIPTrunk
Sip ListInboundTrunks ListSIPInboundTrunkRequest ListSIPInboundTrunk
Sip ListOutboundTrunks ListSIPOutboundTrunkRequest ListSIPOutboundTrunk
Sip DeleteTrunk DeleteSIPTrunkRequest DeleteSIPTrunk
Sip CreateDispatchRule CreateSIPDispatchRuleRequest CreateSIPDispatchRule
Sip UpdateDispatchRule UpdateSIPDispatchRuleRequest UpdateSIPDispatchRule
Sip ListDispatchRules ListSIPDispatchRuleRequest ListSIPDispatchRule
Sip DeleteDispatchRule DeleteSIPDispatchRuleRequest DeleteSIPDispatchRule
Sip CreateParticipant CreateSIPParticipantRequest CreateSIPParticipant
Sip TransferParticipant TransferSIPParticipantRequest TransferSIPParticipant
AgentDispatch CreateDispatch CreateAgentDispatchRequest CreateDispatch
AgentDispatch DeleteDispatch DeleteAgentDispatchRequest DeleteDispatch
AgentDispatch ListDispatch ListAgentDispatchRequest ListDispatch
Connector DialWhatsAppCall DialWhatsAppCallRequest DialWhatsAppCall
Connector DisconnectWhatsAppCall DisconnectWhatsAppCallRequest DisconnectWhatsAppCall
Connector ConnectWhatsAppCall ConnectWhatsAppCallRequest ConnectWhatsAppCall
Connector AcceptWhatsAppCall AcceptWhatsAppCallRequest AcceptWhatsAppCall
Connector ConnectTwilioCall ConnectTwilioCallRequest ConnectTwilioCall
""";

    private static void Services()
    {
        string expected = "";
        var calls = 0;
        var thread = Environment.CurrentManagedThreadId;
        var api = Api(request =>
        {
            Check(Environment.CurrentManagedThreadId == thread, "callback thread");
            Check(request.Url == "http://offline.invalid/twirp/livekit." + expected, "RPC route");
            Check(request.Method == "POST" && request.TimeoutMs == 1234, "HTTP options");
            var headers = request.Headers.ToDictionary(pair => pair.Key, pair => pair.Value);
            Check(headers["Content-Type"] == "application/protobuf", "binary content type");
            Check(headers["Authorization"].StartsWith("Bearer "), "authorization");
            calls++;
            return new HttpResponse(200);
        });
        var assembly = typeof(M.Room).Assembly;
        var models = assembly.GetTypes().Where(type => type.Namespace == "LiveKit.Server.Models" && type.IsSubclassOf(typeof(ProtocolModel)));
        Check(models.Count() == 86, "86 models");
        var services = new Dictionary<string, string>
        {
            ["Room"] = "RoomService",
            ["Egress"] = "Egress",
            ["Ingress"] = "Ingress",
            ["Sip"] = "SIP",
            ["AgentDispatch"] = "AgentDispatchService",
            ["Connector"] = "Connector"
        };
        foreach (var line in Routes.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            var route = line.Trim().Split(' ');
            expected = services[route[0]] + "/" + route[3];
            var service = typeof(LiveKitApi).GetProperty(route[0])!.GetValue(api)!;
            var requestType = assembly.GetType("LiveKit.Server.Models." + route[2])!;
            var method = service.GetType().GetMethod(route[1])!;
            var response = method.Invoke(service, new[] { Activator.CreateInstance(requestType) });
            Check(response is ProtocolModel, "typed response " + route[1]);
            Check(((ProtocolModel)response!).ToJsonElement().EnumerateObject().Count() == 0, "empty protobuf response");
        }
        Check(calls == 52, "all 52 routes");
        expected = "RoomService/ListRooms";
        Check(api.Room.ListRooms().IsEmpty, "default list rooms");
        expected = "AgentDispatchService/ListDispatch";
        Check(api.AgentDispatch.GetDispatch("missing", "room") is null, "missing dispatch");
        var found = Api(_ => new HttpResponse(200, new byte[] { 10, 6, 10, 4, 65, 68, 95, 49 }));
        Check(found.AgentDispatch.GetDispatch("AD_1", "room")!.ToJsonElement().GetProperty("id").GetString() == "AD_1", "found dispatch");

        var name = "room-\u4e2d\u6587-\U0001f600\0";
        var bytes = Encoding.UTF8.GetBytes(name);
        var echo = Api(request =>
        {
            Check(request.Body.Span.SequenceEqual(new byte[] { 10, (byte)bytes.Length }.Concat(bytes).ToArray()), "UTF8/NUL request");
            return new HttpResponse(200, new byte[] { 18, (byte)bytes.Length }.Concat(bytes).ToArray());
        });
        Check(echo.Room.CreateRoom(new M.CreateRoomRequest(new { name })).ToJsonElement().GetProperty("name").GetString() == name,
              "binary UTF8/NUL response");
        var issued = Api(request =>
        {
            Check(request.Headers.Single(pair => pair.Key == "Authorization").Value == "Bearer test-token", "preissued token");
            return new HttpResponse(200);
        }, "test-token");
        issued.Room.ListRooms();
    }

    private static void EnvironmentFallback()
    {
        // Set the child's initial environment: Windows CRT snapshots can differ from changes
        // made later through managed Environment.SetEnvironmentVariable.
        var start = new ProcessStartInfo(Environment.ProcessPath!)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        start.ArgumentList.Add("--environment-child");
        start.Environment["LIVEKIT_URL"] = "http://env.invalid";
        start.Environment["LIVEKIT_API_KEY"] = Key;
        start.Environment["LIVEKIT_API_SECRET"] = Secret;
        start.Environment["LIVEKIT_TOKEN"] = "";
        using var child = Process.Start(start)!;
        var output = child.StandardOutput.ReadToEndAsync();
        var errors = child.StandardError.ReadToEndAsync();
        if (!child.WaitForExit(30000))
        {
            child.Kill(entireProcessTree: true);
            child.WaitForExit();
            throw new TimeoutException("environment child did not exit");
        }
        Check(child.ExitCode == 0, "environment child: " + errors.GetAwaiter().GetResult());
        Check(output.GetAwaiter().GetResult().Contains("Default .NET loader and environment fallback passed"), "default loader child");
    }

    private static void EnvironmentChild()
    {
        var api = new LiveKitApi(new ApiOptions
        {
            Transport = request =>
        {
            Check(request.Url.StartsWith("http://env.invalid/"), "environment URL");
            var jwt = request.Headers.Single(pair => pair.Key == "Authorization").Value[7..].Split('.');
            Check(Decode(jwt[2]).SequenceEqual(HMACSHA256.HashData(Encoding.UTF8.GetBytes(Secret),
                Encoding.UTF8.GetBytes(jwt[0] + "." + jwt[1]))), "environment credentials");
            return new HttpResponse(200);
        }
        });
        api.Room.ListRooms();
        Environment.SetEnvironmentVariable("LIVEKIT_SERVER_DOTNET_LIBRARY", "missing-after-first-load");
        Check(new AccessToken(Key, Secret).ToJwt(1700000000).Count(ch => ch == '.') == 2, "cached default module");
        Console.WriteLine("Default .NET loader and environment fallback passed");
    }

    private static void TokensAndErrors()
    {
        var sources = new[] { "microphone" };
        var grant = new VideoGrant { RoomJoin = true, Room = "room", CanPublish = false, CanPublishSources = sources };
        sources[0] = "changed";
        var attributes = new Dictionary<string, string> { ["language"] = "\u4e2d\u6587\U0001f600" };
        var token = new AccessToken(Key, Secret, _native).SetIdentity("alice\U0001f600").SetName("A")
            .SetMetadata("quote\"\n\0").SetAttributes(attributes).SetValidFor(TimeSpan.FromSeconds(60))
            .SetVideoGrant(grant).SetSipGrant(new SipGrant { Call = true })
            .SetAgentGrant(new AgentGrant { SimulationAdmin = true });
        attributes["language"] = "changed";
        var jwt = token.ToJwt(DateTimeOffset.FromUnixTimeSeconds(1700000000));
        var parts = jwt.Split('.');
        Check(Decode(parts[2]).SequenceEqual(HMACSHA256.HashData(Encoding.UTF8.GetBytes(Secret),
              Encoding.UTF8.GetBytes(parts[0] + "." + parts[1]))), "independent HMAC");
        using var document = JsonDocument.Parse(Decode(parts[1]));
        var claims = document.RootElement;
        Check(claims.GetProperty("sub").GetString() == "alice\U0001f600", "JWT UTF8");
        Check(claims.GetProperty("metadata").GetString() == "quote\"\n\0", "JWT metadata");
        Check(claims.GetProperty("attributes").GetProperty("language").GetString() == "\u4e2d\u6587\U0001f600", "attributes snapshot");
        Check(!claims.GetProperty("video").GetProperty("canPublish").GetBoolean(), "explicit false grant");
        Check(!claims.GetProperty("video").TryGetProperty("canSubscribe", out _), "omitted optional boolean");
        Check(claims.GetProperty("video").GetProperty("canPublishSources")[0].GetString() == "microphone", "source snapshot");
        Check(claims.GetProperty("sip").GetProperty("call").GetBoolean(), "SIP grant");
        Check(claims.GetProperty("agent").GetProperty("simulationAdmin").GetBoolean(), "Agent grant");
        Check(claims.GetProperty("nbf").GetInt64() == 1700000000 && claims.GetProperty("exp").GetInt64() == 1700000060, "explicit clock");
        var responseBytes = new byte[] { 1, 2, 3 };
        var responseCopy = new HttpResponse(200, responseBytes);
        responseBytes[0] = 9;
        Check(responseCopy.Body.Span[0] == 1, "response snapshots input bytes");
        var originalCulture = CultureInfo.CurrentCulture;
        try
        {
            CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("ar-SA");
            Check(token.ToJwt(1700000000).Count(ch => ch == '.') == 2, "culture-independent integer options");
        }
        finally { CultureInfo.CurrentCulture = originalCulture; }
        Failure(ErrorCode.Authentication, () => new AccessToken("", "", _native).ToJwt());
        Failure(ErrorCode.InvalidArgument, () => token.ToJwt(-1));
        Failure(ErrorCode.InvalidArgument, () => token.SetValidFor(long.MaxValue).ToJwt());
        var api = Api(_ => new HttpResponse(200));
        Throws<ArgumentNullException>(() => api.Room.CreateRoom(null!));
        Failure(ErrorCode.Protocol, () => api.Room.CreateRoom(new M.CreateRoomRequest("not JSON")));
        Failure(ErrorCode.Protocol, () => api.Room.CreateRoom(new M.CreateRoomRequest(new { unknown = true })));
        Failure(ErrorCode.Protocol, () => Api(_ => new HttpResponse(200, new byte[] { 255 })).Room.ListRooms());
        Failure(ErrorCode.Transport, () => Api(_ => new HttpResponse(999)).Room.ListRooms());
        var message = "denied \u4e2d\u6587\U0001f600";
        var error = Failure(ErrorCode.Http, () => Api(_ => new HttpResponse(403,
            JsonSerializer.SerializeToUtf8Bytes(new { code = "permission_denied", msg = message }))).Room.ListRooms());
        Check(error.HttpStatus == 403 && error.TwirpCode == "permission_denied" && error.Message == message, "Twirp error details");
        var sentinel = new OperationCanceledException("test transport failure");
        HttpResponse Failed(HttpRequest _) => throw sentinel;
        Check(ReferenceEquals(Throws<OperationCanceledException>(() => Api(Failed).Room.ListRooms()), sentinel), "callback exception identity");
        Check(sentinel.StackTrace!.Contains(nameof(Failed)), "callback exception original stack");
        Throws<InvalidOperationException>(() => Api(_ => null!).Room.ListRooms());
    }

    private static void Webhooks()
    {
        var body = JsonSerializer.SerializeToUtf8Bytes(new
        {
            @event = "participant_joined",
            id = "EV_1",
            createdAt = "1700000000",
            futureField = true,
            room = new { sid = "RM_1", name = "room", metadata = "r" },
            participant = new { sid = "PA_1", identity = "alice", name = "A", metadata = "p" },
            track = new { sid = "TR_1", name = "microphone" },
            egressInfo = new { egressId = "EG_1", roomId = "RM_1", roomName = "room" },
            ingressInfo = new { ingressId = "IN_1", name = "input", roomName = "room" }
        });
        var observed = new List<WebhookEvent>();
        var secrets = new Dictionary<string, string> { [Key] = Secret };
        var receiver = new WebhookReceiver(secrets, observed.Add, _native);
        secrets[Key] = "mutated";
        var token = Authorization(body);
        var value = receiver.ReceiveAndDispatch(body, "Bearer " + token);
        Check(observed.SequenceEqual(new[] { value }), "event callback");
        Check(value.RawBody == Encoding.UTF8.GetString(body), "exact verified raw payload");
        Check(value.CreatedAt == 1700000000, "typed timestamp");
        Check(value.Room!.Sid == "RM_1" && value.Participant!.Identity == "alice", "room and participant");
        Check(value.Track!.Sid == "TR_1" && value.Egress!.EgressId == "EG_1" && value.Ingress!.IngressId == "IN_1", "track/egress/ingress");
        var before = observed.Count;
        Failure(ErrorCode.Authentication, () => receiver.ReceiveAndDispatch(body.Concat(new byte[] { 32 }).ToArray(), token));
        var parts = token.Split('.');
        parts[2] = (parts[2][0] == 'A' ? 'B' : 'A') + parts[2][1..];
        Failure(ErrorCode.Authentication, () => receiver.Receive(body, string.Join('.', parts)));
        Failure(ErrorCode.Authentication, () => receiver.Receive(body, Authorization(body, "unknown")));
        var now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        Failure(ErrorCode.Authentication, () => receiver.Receive(body, Authorization(body, nbf: now - 7200, expiry: now - 3600)));
        Failure(ErrorCode.Authentication, () => receiver.Receive(body, Authorization(body, nbf: now + 3600, expiry: now + 7200)));
        Check(observed.Count == before, "no callbacks before verification");
        var lookups = new List<string>();
        var dynamic = new WebhookReceiver(issuer => { lookups.Add(issuer); return issuer == Key ? Secret : null; }, native: _native);
        Check(dynamic.Receive(body, token).Id == "EV_1" && lookups.SequenceEqual(new[] { Key }), "dynamic key lookup");
        Failure(ErrorCode.Authentication, () => dynamic.Receive(body, Authorization(body, "unknown")));
        var sentinel = new InvalidOperationException("test key provider failure");
        var failed = new WebhookReceiver(_ => throw sentinel, native: _native);
        Check(ReferenceEquals(Throws<InvalidOperationException>(() => failed.Receive(body, token)), sentinel), "key exception identity");
        receiver.SetCallback(item => { receiver.SetCallback(observed.Add); receiver.Dispatch(item); }).Dispatch(value);
        Check(observed.Count == before + 1, "reentrant callback replacement");
        receiver.SetCallback(null).Dispatch(value);
        Check(observed.Count == before + 1, "clear callback");
        receiver.SetCallback(_ => throw sentinel);
        Check(ReferenceEquals(Throws<InvalidOperationException>(() => receiver.Dispatch(value)), sentinel), "event exception identity");
    }

    private static void ConcurrentAndLifetime()
    {
        var api = Api(_ =>
        {
            var thread = Environment.CurrentManagedThreadId;
            new AccessToken(Key, Secret, _native).ToJwt(1700000000);
            GC.Collect();
            GC.WaitForPendingFinalizers();
            Check(Environment.CurrentManagedThreadId == thread, "nested GC callback thread");
            return new HttpResponse(200);
        });
        Task.WaitAll(Enumerable.Range(0, 24).Select(_ => Task.Run(() =>
            Check(api.Room.ListRooms().IsEmpty, "concurrent call"))).ToArray());

        using var native = new NativeServer();
        using var entered = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        var leased = new LiveKitApi(new ApiOptions
        {
            Url = "http://offline.invalid",
            AccessToken = "test",
            Transport = _ =>
            {
                entered.Set();
                if (!release.Wait(TimeSpan.FromSeconds(10))) throw new TimeoutException("disposal test synchronization");
                return new HttpResponse(200);
            }
        }, native);
        var task = Task.Run(() => leased.Room.ListRooms());
        try
        {
            Check(entered.Wait(TimeSpan.FromSeconds(10)), "in-flight lease acquired");
            native.Dispose();
        }
        finally { release.Set(); }
        Check(task.GetAwaiter().GetResult().IsEmpty, "in-flight invoke survives disposal through result cleanup");
        Throws<ObjectDisposedException>(() => leased.Room.ListRooms());
        native.Dispose(); // Idempotent disposal.
    }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Contains("--environment-child"))
            {
                EnvironmentChild();
                return 0;
            }
            ManagedContracts();
            if (args.Contains("--managed-only"))
            {
                Console.WriteLine($"Managed-only .NET smoke passed: {_checks} checks; native tests skipped explicitly");
                return 0;
            }
            using var native = new NativeServer();
            _native = native;
            Check(native.AbiVersion == 1, "ABI 1");
            Services();
            EnvironmentFallback();
            TokensAndErrors();
            Webhooks();
            ConcurrentAndLifetime();
            Console.WriteLine($"Offline .NET smoke passed: {_checks} checks (52 methods, 86 models)");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
