<?php
declare(strict_types=1);

// Optional Composer-installed consumer entry point, otherwise run directly from source.
require getenv('LIVEKIT_SERVER_PHP_AUTOLOAD') ?: __DIR__ . '/../src/bootstrap.php';

use LiveKit\Server as S;
use LiveKit\Server\Models as M;

const KEY = 'php-test-key';
const SECRET = 'php-test-secret-at-least-32-characters';
$checks = 0;

function check(bool $condition, string $message = 'Contract mismatch'): void
{
    global $checks;
    ++$checks;
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

function raises(string $type, callable $action): Throwable
{
    try {
        $action();
    } catch (Throwable $error) {
        check($error instanceof $type, 'Unexpected exception: ' . get_class($error) . ': ' . $error->getMessage());
        return $error;
    }
    throw new RuntimeException('Expected exception ' . $type);
}

function b64(string $bytes): string
{
    return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

function unb64(string $text): string
{
    return base64_decode(strtr($text, '-_', '+/'), true);
}

function authorization(string $body, array $changes = []): string
{
    $claims = array_replace(['iss' => KEY, 'nbf' => time() - 60, 'exp' => time() + 3600,
        'sha256' => base64_encode(hash('sha256', $body, true))], $changes);
    $unsigned = b64('{"alg":"HS256","typ":"JWT"}') . '.' . b64(json_encode($claims, JSON_THROW_ON_ERROR));
    return $unsigned . '.' . b64(hash_hmac('sha256', $unsigned, SECRET, true));
}

$models = array_values(array_filter(get_declared_classes(),
    static fn ($class) => is_subclass_of($class, S\ProtocolModel::class)));
check(count($models) === 86);
foreach ($models as $model) {
    check((new $model())->json === '{}');
    check($model::fromJson('{"unknown":"9007199254740993"}')->toObject()->unknown === '9007199254740993');
    check((new $model()) == new $model('{}'));
}
$source = (object) ['nested' => (object) ['value' => 'original'], 'empty' => new stdClass()];
$model = new M\Room($source);
$source->nested->value = 'changed';
$snapshot = $model->toObject();
$snapshot->nested->value = 'other';
check($model->toObject()->nested->value === 'original');
check($model->toObject()->empty instanceof stdClass);
check((new M\Room()) != new M\CreateRoomRequest());
raises(Error::class, static function () use ($model) { $model->json = '{}'; });
raises(UnexpectedValueException::class, static fn () => M\Room::fromJson('[]')->toObject());
raises(JsonException::class, static fn () => new M\Room(['bad' => NAN]));
raises(JsonException::class, static fn () => new M\Room(['bad' => "\xff"]));
raises(InvalidArgumentException::class, static fn () => new S\ApiOptions(timeoutMs: 0));
raises(InvalidArgumentException::class, static fn () => new S\VideoGrant(canPublishSources: [1]));
raises(InvalidArgumentException::class, static fn () => new S\VideoGrant(canPublishSources: ['key' => 'camera']));
raises(TypeError::class, static fn () => new S\VideoGrant(canPublish: 1));
raises(InvalidArgumentException::class, static fn () => new S\WebhookReceiver());
raises(InvalidArgumentException::class, static fn () => new S\WebhookReceiver(apiKey: KEY));
raises(InvalidArgumentException::class, static fn () => new S\WebhookReceiver(KEY, SECRET, secrets: []));
raises(InvalidArgumentException::class, static fn () => (new S\AccessToken(KEY, SECRET))->setValidFor(0));
raises(InvalidArgumentException::class, static fn () => (new S\AccessToken(KEY, SECRET))->setAttributes(['key' => 1]));
$sources = ['microphone'];
$grant = new S\VideoGrant(roomJoin: true, canPublish: false, canPublishSources: $sources);
$sources[] = 'camera';
check($grant->canPublishSources === ['microphone']);
check($grant->toFields()['can_publish'] === false);
check(!array_key_exists('can_subscribe', $grant->toFields()));
$referencedSource = 'microphone';
$referenceGrant = new S\VideoGrant(canPublishSources: [&$referencedSource]);
$referencedSource = 'camera';
check($referenceGrant->canPublishSources === ['microphone']);
foreach ([new S\ApiOptions(apiSecret: SECRET), new S\AccessToken(KEY, SECRET),
    new S\LiveKitApi(new S\ApiOptions(apiSecret: SECRET)), new S\WebhookReceiver(KEY, SECRET)] as $value) {
    ob_start();
    var_dump($value);
    check(!str_contains(ob_get_clean(), SECRET));
}
if (!getenv('LIVEKIT_SERVER_PHP_LIBRARY')) {
    echo "$checks PHP checks passed; native tests skipped (set LIVEKIT_SERVER_PHP_LIBRARY).\n";
    exit(0);
}

$native = new S\NativeLibrary();
raises(Error::class, static fn () => clone $native);
function api(callable $transport, string $accessToken = ''): S\LiveKitApi
{
    global $native;
    return new S\LiveKitApi(new S\ApiOptions('ws://offline.invalid/', KEY, SECRET, $accessToken, 1234, $transport), $native);
}

// Explicit protocol route fixture; not derived from generated PHP implementation.
$routes = <<<'ROUTES'
room createRoom CreateRoomRequest RoomService/CreateRoom
room listRooms ListRoomsRequest RoomService/ListRooms
room deleteRoom DeleteRoomRequest RoomService/DeleteRoom
room listParticipants ListParticipantsRequest RoomService/ListParticipants
room getParticipant RoomParticipantIdentity RoomService/GetParticipant
room removeParticipant RoomParticipantIdentity RoomService/RemoveParticipant
room mutePublishedTrack MuteRoomTrackRequest RoomService/MutePublishedTrack
room updateParticipant UpdateParticipantRequest RoomService/UpdateParticipant
room updateSubscriptions UpdateSubscriptionsRequest RoomService/UpdateSubscriptions
room sendData SendDataRequest RoomService/SendData
room updateRoomMetadata UpdateRoomMetadataRequest RoomService/UpdateRoomMetadata
room forwardParticipant ForwardParticipantRequest RoomService/ForwardParticipant
room moveParticipant MoveParticipantRequest RoomService/MoveParticipant
room performRpc PerformRpcRequest RoomService/PerformRpc
egress startEgress StartEgressRequest Egress/StartEgress
egress startRoomCompositeEgress RoomCompositeEgressRequest Egress/StartRoomCompositeEgress
egress startWebEgress WebEgressRequest Egress/StartWebEgress
egress startParticipantEgress ParticipantEgressRequest Egress/StartParticipantEgress
egress startTrackCompositeEgress TrackCompositeEgressRequest Egress/StartTrackCompositeEgress
egress startTrackEgress TrackEgressRequest Egress/StartTrackEgress
egress updateLayout UpdateLayoutRequest Egress/UpdateLayout
egress updateStream UpdateStreamRequest Egress/UpdateStream
egress listEgress ListEgressRequest Egress/ListEgress
egress stopEgress StopEgressRequest Egress/StopEgress
ingress createIngress CreateIngressRequest Ingress/CreateIngress
ingress updateIngress UpdateIngressRequest Ingress/UpdateIngress
ingress listIngress ListIngressRequest Ingress/ListIngress
ingress deleteIngress DeleteIngressRequest Ingress/DeleteIngress
sip createInboundTrunk CreateSIPInboundTrunkRequest SIP/CreateSIPInboundTrunk
sip createOutboundTrunk CreateSIPOutboundTrunkRequest SIP/CreateSIPOutboundTrunk
sip updateInboundTrunk UpdateSIPInboundTrunkRequest SIP/UpdateSIPInboundTrunk
sip updateOutboundTrunk UpdateSIPOutboundTrunkRequest SIP/UpdateSIPOutboundTrunk
sip getInboundTrunk GetSIPInboundTrunkRequest SIP/GetSIPInboundTrunk
sip getOutboundTrunk GetSIPOutboundTrunkRequest SIP/GetSIPOutboundTrunk
sip listTrunks ListSIPTrunkRequest SIP/ListSIPTrunk
sip listInboundTrunks ListSIPInboundTrunkRequest SIP/ListSIPInboundTrunk
sip listOutboundTrunks ListSIPOutboundTrunkRequest SIP/ListSIPOutboundTrunk
sip deleteTrunk DeleteSIPTrunkRequest SIP/DeleteSIPTrunk
sip createDispatchRule CreateSIPDispatchRuleRequest SIP/CreateSIPDispatchRule
sip updateDispatchRule UpdateSIPDispatchRuleRequest SIP/UpdateSIPDispatchRule
sip listDispatchRules ListSIPDispatchRuleRequest SIP/ListSIPDispatchRule
sip deleteDispatchRule DeleteSIPDispatchRuleRequest SIP/DeleteSIPDispatchRule
sip createParticipant CreateSIPParticipantRequest SIP/CreateSIPParticipant
sip transferParticipant TransferSIPParticipantRequest SIP/TransferSIPParticipant
agentDispatch createDispatch CreateAgentDispatchRequest AgentDispatchService/CreateDispatch
agentDispatch deleteDispatch DeleteAgentDispatchRequest AgentDispatchService/DeleteDispatch
agentDispatch listDispatch ListAgentDispatchRequest AgentDispatchService/ListDispatch
connector dialWhatsAppCall DialWhatsAppCallRequest Connector/DialWhatsAppCall
connector disconnectWhatsAppCall DisconnectWhatsAppCallRequest Connector/DisconnectWhatsAppCall
connector connectWhatsAppCall ConnectWhatsAppCallRequest Connector/ConnectWhatsAppCall
connector acceptWhatsAppCall AcceptWhatsAppCallRequest Connector/AcceptWhatsAppCall
connector connectTwilioCall ConnectTwilioCallRequest Connector/ConnectTwilioCall
ROUTES;
$calls = 0;
$expected = '';
$client = api(static function (S\HttpRequest $request) use (&$calls, &$expected): S\HttpResponse {
    check($request->url === 'http://offline.invalid/twirp/livekit.' . $expected, $request->url);
    check($request->method === 'POST');
    check($request->timeoutMs === 1234);
    $headers = array_column($request->headers, 1, 0);
    check($headers['Content-Type'] === 'application/protobuf');
    check(str_starts_with($headers['Authorization'], 'Bearer '));
    ++$calls;
    return new S\HttpResponse(200);
});
foreach (explode("\n", $routes) as $row) {
    [$service, $method, $type, $expected] = explode(' ', $row);
    $type = 'LiveKit\\Server\\Models\\' . $type;
    $result = $client->$service->$method(new $type());
    $declaredType = (new ReflectionMethod($client->$service, $method))->getReturnType()->getName();
    check(get_class($result) === $declaredType);
    check($result->toObject() == new stdClass());
}
check($calls === 52);
$expected = 'RoomService/ListRooms';
$client->room->listRooms();
$expected = 'AgentDispatchService/ListDispatch';
check($client->agentDispatch->getDispatch('missing', 'room') === null);
$found = api(static fn () => new S\HttpResponse(200, "\x0a\x06\x0a\x04AD_1"));
check($found->agentDispatch->getDispatch('AD_1', 'room')->toObject()->id === 'AD_1');
raises(TypeError::class, static fn () => $client->room->createRoom(new M\Room()));

$name = "中文😀\0";
$result = api(static function (S\HttpRequest $request) use ($name): S\HttpResponse {
    check($request->body === "\x0a" . chr(strlen($name)) . $name);
    raises(Error::class, static function () use ($request) { $request->body = 'mutated'; });
    return new S\HttpResponse(200, "\x12" . chr(strlen($name)) . $name);
})->room->createRoom(new M\CreateRoomRequest(['name' => $name]));
check($result->toObject()->name === $name);
api(static function (S\HttpRequest $request): S\HttpResponse {
    check(array_column($request->headers, 1, 0)['Authorization'] === 'Bearer preissued-test-token');
    return new S\HttpResponse(200);
}, 'preissued-test-token')->room->listRooms();

$environment = ['LIVEKIT_URL' => 'http://env.invalid', 'LIVEKIT_API_KEY' => KEY,
    'LIVEKIT_API_SECRET' => SECRET, 'LIVEKIT_TOKEN' => ''];
$saved = [];
foreach ($environment as $key => $value) {
    $saved[$key] = getenv($key);
    putenv("$key=$value");
}
try {
    (new S\LiveKitApi(new S\ApiOptions(transport: static function (S\HttpRequest $request): S\HttpResponse {
        check(str_starts_with($request->url, 'http://env.invalid/'));
        $jwt = substr(array_column($request->headers, 1, 0)['Authorization'], 7);
        [$header, $claims, $signature] = explode('.', $jwt);
        check(hash_equals(unb64($signature), hash_hmac('sha256', "$header.$claims", SECRET, true)));
        return new S\HttpResponse(200);
    }), $native))->room->listRooms();
    putenv('LIVEKIT_TOKEN=environment-test-token');
    (new S\LiveKitApi(new S\ApiOptions(transport: static function (S\HttpRequest $request): S\HttpResponse {
        check(array_column($request->headers, 1, 0)['Authorization'] === 'Bearer environment-test-token');
        return new S\HttpResponse(200);
    }), $native))->room->listRooms();
} finally {
    foreach ($saved as $key => $value) {
        putenv($value === false ? $key : "$key=$value");
    }
}

$referencedAttribute = 'reference-value';
$attributes = ['语言' => '中文😀', '123' => 'numeric-key', 'reference' => &$referencedAttribute];
$token = (new S\AccessToken(KEY, SECRET, $native))->setIdentity($name)->setName('name')
    ->setMetadata("quote\"\n\0")->setAttributes($attributes)->setValidFor(60)->setVideoGrant($grant)
    ->setSipGrant(new S\SipGrant(call: true))->setAgentGrant(new S\AgentGrant(simulationAdmin: true));
$attributes['语言'] = 'changed';
$referencedAttribute = 'changed-reference';
$jwt = $token->toJwt(1700000000);
[$header, $payload, $signature] = explode('.', $jwt);
check(hash_equals(unb64($signature), hash_hmac('sha256', "$header.$payload", SECRET, true)));
$claims = json_decode(unb64($payload), true, 512, JSON_THROW_ON_ERROR);
check($claims['sub'] === $name);
check($claims['metadata'] === "quote\"\n\0");
check($claims['attributes']['语言'] === '中文😀');
check($claims['attributes']['123'] === 'numeric-key');
check($claims['attributes']['reference'] === 'reference-value');
check($claims['video']['canPublish'] === false);
check(!isset($claims['video']['canSubscribe']));
check($claims['video']['canPublishSources'] === ['microphone']);
check($claims['sip']['call'] && $claims['agent']['simulationAdmin']);
check([$claims['nbf'], $claims['exp']] === [1700000000, 1700000060]);
check($token->toJwt(new DateTimeImmutable('@1700000000')) === $jwt);
check((new S\AccessToken(KEY, SECRET))->toJwt(1700000000) !== '');
$selectedLibrary = getenv('LIVEKIT_SERVER_PHP_LIBRARY');
putenv('LIVEKIT_SERVER_PHP_LIBRARY=missing-after-cached-load');
try {
    check((new S\AccessToken(KEY, SECRET))->toJwt(1700000000) !== '');
} finally {
    putenv('LIVEKIT_SERVER_PHP_LIBRARY=' . $selectedLibrary);
}

foreach ([
    [S\Error::AUTHENTICATION, static fn () => (new S\AccessToken('', '', $native))->toJwt(1700000000)],
    [S\Error::INVALID_ARGUMENT, static fn () => $token->toJwt(-1)],
    [S\Error::INVALID_ARGUMENT, static fn () => $token->setValidFor(PHP_INT_MAX)->toJwt(1700000000)],
    [S\Error::PROTOCOL, static fn () => $client->room->createRoom(new M\CreateRoomRequest('not JSON'))],
    [S\Error::PROTOCOL, static fn () => $client->room->createRoom(new M\CreateRoomRequest(['unknown' => true]))],
    [S\Error::PROTOCOL, static fn () => api(static fn () => new S\HttpResponse(200, "\xff"))->room->listRooms()],
    [S\Error::TRANSPORT, static fn () => api(static fn () => new S\HttpResponse(999))->room->listRooms()],
] as [$code, $action]) {
    check(raises(S\Error::class, $action)->errorCode === $code);
}
$error = raises(S\Error::class, static fn () => api(static fn () => new S\HttpResponse(403,
    '{"code":"permission_denied","msg":"denied 中文😀"}'))->room->listRooms());
check([$error->httpStatus, $error->twirpCode, $error->getMessage()] === [403, 'permission_denied', 'denied 中文😀']);
$sentinel = new LogicException('callback sentinel');
check(raises(LogicException::class, static fn () => api(static function () use ($sentinel) {
    throw $sentinel;
})->room->listRooms()) === $sentinel);
raises(UnexpectedValueException::class, static fn () => api(static fn () => null)->room->listRooms());

// Same dispatcher, nested native invocations and GC while outer request buffers are borrowed.
$inner = api(static fn () => new S\HttpResponse(200));
$nested = api(static function () use ($inner, $native): S\HttpResponse {
    $inner->room->listRooms();
    check((new S\AccessToken(KEY, SECRET, $native))->toJwt(1700000000) !== '');
    gc_collect_cycles();
    return new S\HttpResponse(200);
});
for ($i = 0; $i < 40; ++$i) {
    check($nested->room->listRooms()->toObject() == new stdClass());
}

$body = json_encode(['event' => 'participant_joined', 'id' => 'EV_1', 'createdAt' => '1700000000',
    'room' => ['sid' => 'RM_1', 'name' => 'room', 'metadata' => 'r'],
    'participant' => ['sid' => 'PA_1', 'identity' => 'alice', 'name' => 'A', 'metadata' => 'p'],
    'track' => ['sid' => 'TR_1', 'name' => 'microphone'],
    'egressInfo' => ['egressId' => 'EG_1', 'roomId' => 'RM_1', 'roomName' => 'room'],
    'ingressInfo' => ['ingressId' => 'IN_1', 'name' => 'input', 'roomName' => 'room'],
    'futureField' => ['中文' => true]], JSON_THROW_ON_ERROR | JSON_UNESCAPED_UNICODE);
$seen = [];
$receiver = new S\WebhookReceiver(KEY, SECRET, callback: static function ($event) use (&$seen) { $seen[] = $event; }, native: $native);
$authorization = authorization($body);
$event = $receiver->receiveAndDispatch($body, 'Bearer ' . $authorization);
check($seen === [$event]);
check($event->rawBody === $body);
check($event->createdAt === 1700000000);
check($event->room->sid === 'RM_1');
check($event->participant->identity === 'alice');
check($event->track->sid === 'TR_1');
check($event->egress->egressId === 'EG_1');
check($event->ingress->ingressId === 'IN_1');
raises(Error::class, static function () use ($event) { $event->room->name = 'changed'; });
$seen = [];
foreach ([[$body . ' ', $authorization], [$body, authorization($body, ['iss' => 'unknown'])],
    [$body, authorization($body, ['exp' => time() - 3600, 'nbf' => time() - 7200])],
    [$body, authorization($body, ['nbf' => time() + 3600, 'exp' => time() + 7200])],
    [$body, substr($authorization, 0, strrpos($authorization, '.') + 1) . b64(str_repeat("\0", 32))]] as [$bytes, $auth]) {
    check(raises(S\Error::class, static fn () => $receiver->receiveAndDispatch($bytes, $auth))->errorCode === S\Error::AUTHENTICATION);
}
check($seen === []);
$keys = [];
$dynamic = new S\WebhookReceiver(keyProvider: static function (string $key) use (&$keys, $inner): ?string {
    $keys[] = $key;
    $inner->room->listRooms();
    gc_collect_cycles();
    return $key === KEY ? SECRET : null;
}, native: $native);
check($dynamic->receive($body, $authorization)->id === 'EV_1');
check($keys === [KEY]);
raises(S\Error::class, static fn () => $dynamic->receive($body, authorization($body, ['iss' => 'unknown'])));
check(raises(LogicException::class, static fn () => (new S\WebhookReceiver(keyProvider: static function () use ($sentinel) {
    throw $sentinel;
}, native: $native))->receive($body, $authorization)) === $sentinel);
raises(UnexpectedValueException::class, static fn () => (new S\WebhookReceiver(keyProvider: static fn () => 123,
    native: $native))->receive($body, $authorization));
check((new S\WebhookReceiver(secrets: ['other' => 'unused', KEY => SECRET], native: $native))->receive($body, $authorization)->id === 'EV_1');
$receiver->setCallback(static function ($value) use ($receiver, &$seen) {
    $receiver->setCallback(static function ($next) use (&$seen) { $seen[] = $next; });
    $receiver->dispatch($value);
})->dispatch($event);
check($seen === [$event]);
$receiver->setCallback(null)->dispatch($event);
check($seen === [$event]);
$receiver->setCallback(static function () use ($sentinel) { throw $sentinel; });
check(raises(LogicException::class, static fn () => $receiver->dispatch($event)) === $sentinel);
check($inner->room->listRooms()->toObject() == new stdClass());
echo "$checks PHP checks passed (source/package, native C ABI, no network).\n";
