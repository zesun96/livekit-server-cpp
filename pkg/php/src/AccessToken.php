<?php
declare(strict_types=1);

namespace LiveKit\Server;

final class AccessToken
{
    private array $fields;

    public function __construct(string $apiKey, string $apiSecret, private ?NativeLibrary $native = null)
    {
        $this->fields = ['api_key' => $apiKey, 'api_secret' => $apiSecret, 'valid_for' => '21600'];
    }

    public function setIdentity(string $identity): self
    {
        $this->fields['identity'] = $identity;
        return $this;
    }

    public function setName(string $name): self
    {
        $this->fields['name'] = $name;
        return $this;
    }

    public function setMetadata(string $metadata): self
    {
        $this->fields['metadata'] = $metadata;
        return $this;
    }

    /** @param array<string, string> $attributes */
    public function setAttributes(array $attributes): self
    {
        $this->fields['attributes'] = Wire::stringMap($attributes);
        return $this;
    }

    public function setValidFor(int $seconds): self
    {
        if ($seconds <= 0) {
            throw new \InvalidArgumentException('Token lifetime must be positive seconds');
        }
        $this->fields['valid_for'] = (string) $seconds;
        return $this;
    }

    public function setVideoGrant(VideoGrant $grant): self
    {
        $this->fields['video'] = $grant->toFields();
        return $this;
    }

    public function setSipGrant(SipGrant $grant): self
    {
        $this->fields['sip'] = $grant->toFields();
        return $this;
    }

    public function setAgentGrant(AgentGrant $grant): self
    {
        $this->fields['agent'] = $grant->toFields();
        return $this;
    }

    public function toJwt(int|\DateTimeInterface|null $now = null): string
    {
        $now = $now instanceof \DateTimeInterface ? $now->getTimestamp() : ($now ?? time());
        return ($this->native ?? NativeLibrary::shared())->invoke([
            'operation' => 'token', 'options' => [...$this->fields, 'now' => (string) $now],
        ]);
    }

    public function __debugInfo(): array
    {
        return ['credentials' => '[redacted]'];
    }
}
