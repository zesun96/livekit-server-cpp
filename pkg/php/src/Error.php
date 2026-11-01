<?php
declare(strict_types=1);

namespace LiveKit\Server;

final class Error extends \RuntimeException
{
    public const INVALID_ARGUMENT = 0;
    public const AUTHENTICATION = 1;
    public const TRANSPORT = 2;
    public const HTTP = 3;
    public const PROTOCOL = 4;
    public const UNSUPPORTED = 5;
    public const UNKNOWN = 255;

    public function __construct(
        string $message,
        public readonly int $errorCode,
        public readonly int $httpStatus = 0,
        public readonly string $twirpCode = '',
    ) {
        parent::__construct($message, $errorCode);
    }
}
