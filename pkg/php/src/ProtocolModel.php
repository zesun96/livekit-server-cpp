<?php
declare(strict_types=1);

namespace LiveKit\Server;

abstract readonly class ProtocolModel implements \JsonSerializable, \Stringable
{
    public string $json;

    /** Array/object fields follow protobuf JSON; string input is preserved for native validation. */
    final public function __construct(string|array|\stdClass $fields = [])
    {
        $this->json = is_string($fields) ? $fields : Wire::encode((object) $fields);
    }

    final public static function fromJson(string $json): static
    {
        return new static($json);
    }

    /** A detached object snapshot; empty JSON maps remain objects rather than arrays. */
    final public function toObject(): \stdClass
    {
        $value = Wire::decode($this->json);
        if (!$value instanceof \stdClass) {
            throw new \UnexpectedValueException('Model JSON must be an object');
        }
        return $value;
    }

    final public function jsonSerialize(): \stdClass
    {
        return $this->toObject();
    }

    final public function __toString(): string
    {
        return $this->json;
    }
}
