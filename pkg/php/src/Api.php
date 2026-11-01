<?php
declare(strict_types=1);

namespace LiveKit\Server;

final readonly class ApiOptions
{
    public ?\Closure $transport;

    public function __construct(
        public string $url = '',
        public string $apiKey = '',
        public string $apiSecret = '',
        public string $accessToken = '',
        public int $timeoutMs = 30000,
        ?callable $transport = null,
    ) {
        if ($timeoutMs <= 0) {
            throw new \InvalidArgumentException('Timeout must be positive milliseconds');
        }
        $this->transport = $transport === null ? null : \Closure::fromCallable($transport);
    }

    /** @internal */
    public function toFields(): array
    {
        return ['url' => $this->url, 'api_key' => $this->apiKey, 'api_secret' => $this->apiSecret,
            'access_token' => $this->accessToken, 'timeout_ms' => (string) $this->timeoutMs,
            'use_transport' => $this->transport !== null];
    }

    public function __debugInfo(): array
    {
        return ['credentials' => '[redacted]'];
    }
}

final readonly class LiveKitApi
{
    public RoomServiceClient $room;
    public EgressClient $egress;
    public IngressClient $ingress;
    public SipClient $sip;
    public AgentDispatchClient $agentDispatch;
    public ConnectorClient $connector;

    public function __construct(private ApiOptions $options = new ApiOptions(), private ?NativeLibrary $native = null)
    {
        $this->room = new RoomServiceClient($this);
        $this->egress = new EgressClient($this);
        $this->ingress = new IngressClient($this);
        $this->sip = new SipClient($this);
        $this->agentDispatch = new AgentDispatchClient($this);
        $this->connector = new ConnectorClient($this);
    }

    /** @internal */
    public function call(string $operation, ProtocolModel $request): string
    {
        return ($this->native ?? NativeLibrary::shared())->invoke([
            'operation' => $operation, 'options' => $this->options->toFields(), 'request' => $request->json,
        ], $this->options->transport);
    }

    /** @internal */
    public function getDispatch(string $dispatchId, string $room): ?Models\AgentDispatch
    {
        $value = ($this->native ?? NativeLibrary::shared())->invoke([
            'operation' => 'get_dispatch', 'dispatch_id' => $dispatchId, 'room' => $room,
            'options' => $this->options->toFields(),
        ], $this->options->transport);
        return $value === null ? null : new Models\AgentDispatch($value);
    }

    public function __debugInfo(): array
    {
        return ['credentials' => '[redacted]'];
    }
}
