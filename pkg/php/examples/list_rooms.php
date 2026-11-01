<?php
declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

use LiveKit\Server\LiveKitApi;

// The native SDK reads LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET.
$rooms = (new LiveKitApi())->room->listRooms();
echo $rooms->json, PHP_EOL;
