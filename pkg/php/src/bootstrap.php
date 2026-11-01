<?php
declare(strict_types=1);

// Also usable without Composer: require this file directly.
foreach (['Wire', 'Error', 'ProtocolModel', 'Models', 'Grants', 'Http', 'NativeLibrary',
    'Api', 'Services', 'AccessToken', 'Webhook'] as $file) {
    require_once __DIR__ . '/' . $file . '.php';
}
