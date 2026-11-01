<?php
declare(strict_types=1);

namespace LiveKit\Server;

final readonly class WebhookRoom
{
    public function __construct(public string $sid, public string $name, public string $metadata) {}
}

final readonly class WebhookParticipant
{
    public function __construct(public string $sid, public string $identity, public string $name, public string $metadata) {}
}

final readonly class WebhookTrack
{
    public function __construct(public string $sid, public string $name) {}
}

final readonly class WebhookEgress
{
    public function __construct(public string $egressId, public string $roomId, public string $roomName) {}
}

final readonly class WebhookIngress
{
    public function __construct(public string $ingressId, public string $name, public string $roomName) {}
}

final readonly class WebhookEvent
{
    public function __construct(
        public string $event,
        public string $id,
        public int $createdAt,
        public string $rawBody,
        public ?WebhookRoom $room = null,
        public ?WebhookParticipant $participant = null,
        public ?WebhookTrack $track = null,
        public ?WebhookEgress $egress = null,
        public ?WebhookIngress $ingress = null,
    ) {}

    /** @internal */
    public static function fromFields(\stdClass $value): self
    {
        return new self($value->event, $value->id, (int) $value->created_at, $value->raw_body,
            isset($value->room) ? new WebhookRoom($value->room->sid, $value->room->name, $value->room->metadata) : null,
            isset($value->participant) ? new WebhookParticipant($value->participant->sid,
                $value->participant->identity, $value->participant->name, $value->participant->metadata) : null,
            isset($value->track) ? new WebhookTrack($value->track->sid, $value->track->name) : null,
            isset($value->egress) ? new WebhookEgress($value->egress->egress_id,
                $value->egress->room_id, $value->egress->room_name) : null,
            isset($value->ingress) ? new WebhookIngress($value->ingress->ingress_id,
                $value->ingress->name, $value->ingress->room_name) : null,
        );
    }
}

final class WebhookReceiver
{
    private \stdClass $secrets;
    private ?\Closure $keyProvider;
    private ?\Closure $callback;

    /** Provide exactly one of key/secret, secrets map, or keyProvider. */
    public function __construct(
        ?string $apiKey = null,
        ?string $apiSecret = null,
        ?array $secrets = null,
        ?callable $keyProvider = null,
        ?callable $callback = null,
        private ?NativeLibrary $native = null,
    ) {
        if ((int) ($apiKey !== null || $apiSecret !== null) + (int) ($secrets !== null)
            + (int) ($keyProvider !== null) !== 1) {
            throw new \InvalidArgumentException('Provide key/secret, secrets map, or keyProvider');
        }
        if (($apiKey === null) !== ($apiSecret === null)) {
            throw new \InvalidArgumentException('Both API key and secret are required');
        }
        $this->secrets = Wire::stringMap($secrets ?? ($apiKey === null ? [] : [$apiKey => $apiSecret]));
        $this->keyProvider = $keyProvider === null ? null : \Closure::fromCallable($keyProvider);
        $this->setCallback($callback);
    }

    public function setCallback(?callable $callback): self
    {
        $this->callback = $callback === null ? null : \Closure::fromCallable($callback);
        return $this;
    }

    /** Body is the exact raw HTTP request bytes, before JSON decoding or normalization. */
    public function receive(string $body, string $authorization): WebhookEvent
    {
        $value = ($this->native ?? NativeLibrary::shared())->invoke([
            'operation' => 'webhook', 'options' => ['body' => Wire::base64url($body),
                'authorization' => $authorization, 'secrets' => $this->secrets,
                'use_key_provider' => $this->keyProvider !== null],
        ], keyProvider: $this->keyProvider);
        return WebhookEvent::fromFields($value);
    }

    public function dispatch(WebhookEvent $event): void
    {
        $callback = $this->callback;
        if ($callback !== null) {
            $callback($event);
        }
    }

    public function receiveAndDispatch(string $body, string $authorization): WebhookEvent
    {
        $event = $this->receive($body, $authorization);
        $this->dispatch($event);
        return $event;
    }

    public function __debugInfo(): array
    {
        return ['credentials' => '[redacted]'];
    }
}
