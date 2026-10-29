<?php
declare(strict_types=1);

/*
 * Register a sensor node from a browser GET request.
 * Accepts ordinary query parameters or a Base64-encoded query string.
 * The input is validated and inserted with a prepared statement.
 */

require_once __DIR__ . '/db_connection.php';

header('Content-Type: text/plain; charset=utf-8');
header('Cache-Control: no-store');

function respond(int $statusCode, string $message): void
{
    http_response_code($statusCode);
    echo $message . "\n";
    exit;
}

function inputString(array $source, string $key): ?string
{
    if (!array_key_exists($key, $source) || !is_string($source[$key])) {
        return null;
    }

    return trim($source[$key]);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET') {
    header('Allow: GET');
    respond(405, 'Only GET requests are accepted.');
}

$rawQuery = $_SERVER['QUERY_STRING'] ?? '';
$input = [];

if ($rawQuery === '') {
    respond(400, 'Provide nodeName, manufacturer, longitude, and latitude in the query string.');
}

// Accept plain query parameters in any order.
if (preg_match('/(?:^|&)(nodeName|manufacturer|longitude|latitude)=/', $rawQuery) === 1) {
    parse_str($rawQuery, $input);
} else {
    // Also accept Base64 of a query string, like the measurement insert endpoint.
    $encodedMessage = $rawQuery;

    // Optional wrappers: ?payload=BASE64, ?data=BASE64, or ?message=BASE64
    if (preg_match('/^(payload|data|message)=/', $rawQuery) === 1) {
        $wrapper = [];
        parse_str($rawQuery, $wrapper);
        $encodedMessage = (string) (
            $wrapper['payload'] ?? $wrapper['data'] ?? $wrapper['message'] ?? ''
        );
    }

    // Preserve literal + signs, which can otherwise be interpreted as spaces.
    $encodedMessage = rawurldecode($encodedMessage);
    $encodedMessage = str_replace(' ', '+', $encodedMessage);
    $decodedMessage = base64_decode($encodedMessage, true);

    if ($decodedMessage === false || $decodedMessage === '') {
        respond(400, 'The query must contain valid parameters or a Base64-encoded query string.');
    }

    parse_str($decodedMessage, $input);
}

$nodeName = inputString($input, 'nodeName');
$manufacturer = inputString($input, 'manufacturer');
$longitudeInput = inputString($input, 'longitude');
$latitudeInput = inputString($input, 'latitude');

if (
    $nodeName === null || $manufacturer === null ||
    $longitudeInput === null || $latitudeInput === null
) {
    respond(400, 'Required fields: nodeName, manufacturer, longitude, and latitude.');
}

if (strlen($nodeName) > 10 || preg_match('/^node_[0-9]+$/', $nodeName) !== 1) {
    respond(400, 'nodeName must match node_ followed by digits and be at most 10 characters.');
}

if ($manufacturer === '' || strlen($manufacturer) > 10) {
    respond(400, 'manufacturer must contain 1 to 10 characters.');
}

if (
    !is_numeric($longitudeInput) || !is_finite((float) $longitudeInput) ||
    !is_numeric($latitudeInput) || !is_finite((float) $latitudeInput)
) {
    respond(400, 'longitude and latitude must be valid numbers.');
}

$longitude = (float) $longitudeInput;
$latitude = (float) $latitudeInput;

if ($longitude < -180 || $longitude > 180) {
    respond(400, 'longitude must be between -180 and 180.');
}

if ($latitude < -90 || $latitude > 90) {
    respond(400, 'latitude must be between -90 and 90.');
}

try {
    $pdo = getSensorDatabaseConnection();
    $insert = $pdo->prepare(
        'INSERT INTO sensor_register (node_name, manufacturer, longitude, latitude)
         VALUES (:node_name, :manufacturer, :longitude, :latitude)'
    );
    $insert->execute([
        'node_name' => $nodeName,
        'manufacturer' => $manufacturer,
        'longitude' => $longitudeInput,
        'latitude' => $latitudeInput,
    ]);

    respond(201, 'Registered ' . $nodeName . '.');
} catch (PDOException $exception) {
    $driverCode = isset($exception->errorInfo[1]) ? (int) $exception->errorInfo[1] : 0;

    if ($driverCode === 1062) {
        respond(409, 'That node name is already registered.');
    }

    error_log('Sensor registration database error: ' . $exception->getMessage());
    respond(500, 'Database error. Check the PHP error log.');
} catch (Throwable $exception) {
    error_log('Sensor registration error: ' . $exception->getMessage());
    respond(500, 'The node could not be registered. Check the PHP error log.');
}
