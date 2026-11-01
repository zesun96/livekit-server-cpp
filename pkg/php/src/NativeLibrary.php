<?php
declare(strict_types=1);

namespace LiveKit\Server;

/** @internal One context per synchronous invocation, retained through native result cleanup. */
final class CallContext
{
    public array $buffers = [];
    public ?\Throwable $error = null;

    public function __construct(public ?\Closure $transport, public ?\Closure $keyProvider)
    {
    }
}

/** Loads the shared SDK directly; no PHP-specific native bridge or background callbacks. */
final class NativeLibrary
{
    private \FFI $ffi;
    private ?\FFI\CData $callback = null;
    /** @var list<CallContext> */
    private array $calls = [];
    private static ?self $default = null;

    // The reusable trampoline belongs to this instance's invocation stack.
    private function __clone() {}

    public function __construct(?string $path = null)
    {
        if (PHP_INT_SIZE !== 8) {
            throw new \RuntimeException('The PHP binding requires a 64-bit PHP process');
        }
        if (!extension_loaded('ffi')) {
            throw new \RuntimeException('Enable ext-ffi and ffi.enable for this PHP process');
        }
        $path ??= getenv('LIVEKIT_SERVER_PHP_LIBRARY') ?: match (PHP_OS_FAMILY) {
            'Windows' => 'livekitserver.dll',
            'Darwin' => 'liblivekitserver.dylib',
            default => 'liblivekitserver.so',
        };
        if (str_contains($path, "\0")) {
            throw new \InvalidArgumentException('Native library path contains NUL');
        }
        // void pointers preserve length-delimited bytes in PHP FFI. They have the same pointer
        // representation as char pointers in c_api.h; all integer types and calling conventions match.
        $this->ffi = \FFI::cdef(<<<'C'
            typedef int32_t (*lks_callback)(void*, const void*, size_t, const void**, size_t*);
            int32_t lks_abi_version(void);
            void* lks_invoke(const char*, size_t, lks_callback, void*, size_t*);
            void lks_free(void*);
            typedef struct { lks_callback fn; } lks_callback_slot;
            C, $path);
        if ($this->ffi->lks_abi_version() !== 1) {
            throw new \RuntimeException('Unsupported LiveKit server C ABI version (expected 1)');
        }
    }

    public static function shared(): self
    {
        return self::$default ??= new self();
    }

    /** @internal Callback dispatch is synchronous, including nested invocations on this instance. */
    private function exchange($request, int $length, $response, $responseLength): int
    {
        $context = $this->calls[array_key_last($this->calls)];
        try {
            $input = Wire::decode(\FFI::string($request, $length));
            if ($input->kind === 'transport') {
                if ($context->transport === null) {
                    throw new \LogicException('Transport callback is required');
                }
                $result = ($context->transport)(new HttpRequest(
                    $input->url, $input->method, $input->headers,
                    Wire::unbase64url($input->body), (int) $input->timeout_ms,
                ));
                if (!$result instanceof HttpResponse) {
                    throw new \UnexpectedValueException('Transport must return HttpResponse');
                }
                $payload = ['status_code' => (string) $result->statusCode,
                    'body' => Wire::base64url($result->body)];
            } elseif ($input->kind === 'key') {
                if ($context->keyProvider === null) {
                    throw new \LogicException('Key provider is required');
                }
                $secret = ($context->keyProvider)($input->key);
                if ($secret !== null && !is_string($secret)) {
                    throw new \UnexpectedValueException('Key provider must return string or null');
                }
                $payload = ['secret' => $secret ?? ''];
            } else {
                throw new \UnexpectedValueException('Unknown native callback kind');
            }
            $data = Wire::encode($payload);
            $buffer = $this->ffi->new('char[' . strlen($data) . ']');
            \FFI::memcpy($buffer, $data, strlen($data));
            $context->buffers[] = $buffer;
            $response[0] = \FFI::addr($buffer[0]);
            $responseLength[0] = strlen($data);
            return 0;
        } catch (\Throwable $error) {
            // Never unwind PHP exceptions through libffi/C++; retain the original instance/trace.
            $context->error ??= $error;
            return 1;
        }
    }

    public function invoke(array $input, ?callable $transport = null, ?callable $keyProvider = null): mixed
    {
        $data = Wire::encode($input);
        $context = new CallContext($transport === null ? null : \Closure::fromCallable($transport),
            $keyProvider === null ? null : \Closure::fromCallable($keyProvider));
        if (($transport !== null || $keyProvider !== null) && $this->callback === null) {
            // Reuse one libffi trampoline per library instead of allocating one per RPC.
            $owner = \WeakReference::create($this);
            $this->callback = $this->ffi->new('lks_callback_slot');
            $this->callback->fn = static function ($user, $request, $length, $response, $responseLength) use ($owner): int {
                return $owner->get()->exchange($request, $length, $response, $responseLength);
            };
        }
        $size = $this->ffi->new('size_t');
        $output = null;
        $this->calls[] = $context;
        try {
            $output = $this->ffi->lks_invoke($data, strlen($data),
                ($transport !== null || $keyProvider !== null) ? $this->callback->fn : null,
                null, \FFI::addr($size));
            if ($context->error !== null) {
                throw $context->error;
            }
            if ($output === null || \FFI::isNull($output)) {
                throw new \RuntimeException('Native C ABI could not allocate or encode its result');
            }
            $result = Wire::decode(\FFI::string($output, $size->cdata));
            if (!$result->ok) {
                throw new Error($result->message, (int) $result->code,
                    (int) $result->http_status, $result->twirp_code);
            }
            return $result->value;
        } finally {
            try {
                if ($output !== null && !\FFI::isNull($output)) {
                    $this->ffi->lks_free($output);
                }
            } finally {
                array_pop($this->calls);
                // PHP-owned callback buffers are released with the context after native cleanup.
            }
        }
    }

    public function __debugInfo(): array
    {
        return ['abiVersion' => 1];
    }
}
