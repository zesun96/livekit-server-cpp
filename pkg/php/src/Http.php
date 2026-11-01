<?php
declare(strict_types=1);

namespace LiveKit\Server;

final readonly class HttpRequest
{
    /** @param list<array{string, string}> $headers Ordered header pairs. Body is binary protobuf. */
    public function __construct(
        public string $url,
        public string $method,
        public array $headers,
        public string $body,
        public int $timeoutMs,
    ) {
    }
}

final readonly class HttpResponse
{
    public function __construct(public int $statusCode, public string $body = '')
    {
    }
}
