<?php
declare(strict_types=1);

namespace LiveKit\Server;

/** @internal */
final class Wire
{
    public static function encode(mixed $value): string
    {
        return json_encode($value, JSON_THROW_ON_ERROR | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    }

    public static function decode(string $value): mixed
    {
        // Preserve objects, empty maps and decimal strings, including nested protocol values.
        return json_decode($value, false, 512, JSON_THROW_ON_ERROR | JSON_BIGINT_AS_STRING);
    }

    public static function base64url(string $bytes): string
    {
        return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
    }

    public static function unbase64url(string $text): string
    {
        if (preg_match('/[^A-Za-z0-9_-]/', $text) || strlen($text) % 4 === 1) {
            throw new \UnexpectedValueException('Invalid base64url payload');
        }
        $bytes = base64_decode(strtr($text, '-_', '+/'), true);
        if ($bytes === false) {
            throw new \UnexpectedValueException('Invalid base64url payload');
        }
        return $bytes;
    }

    public static function stringMap(array $values): \stdClass
    {
        $result = new \stdClass();
        foreach ($values as $key => $value) {
            // PHP converts decimal string keys to integers; restore them when encoding the map.
            if (!is_string($value)) {
                throw new \InvalidArgumentException('Map values must be strings');
            }
            $result->{(string) $key} = $value;
        }
        return $result;
    }
}
