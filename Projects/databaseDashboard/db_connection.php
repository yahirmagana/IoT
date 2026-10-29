<?php
declare(strict_types=1);

/** Create the shared PDO connection used by the dashboard and data endpoint. */
function getSensorDatabaseConnection(): PDO
{
    $dbHost = getenv('DB_HOST') ?: 'localhost';
    $dbName = getenv('DB_NAME') ?: 'u901022221_YahirMagana';
    $dbUser = getenv('DB_USER') ?: '';
    $dbPass = getenv('DB_PASS') ?: '';
    $dbPort = getenv('DB_PORT') ?: '3306';

    if (
        $dbUser === 'YOUR_HOSTINGER_DB_USERNAME' ||
        $dbPass === 'YOUR_HOSTINGER_DB_PASSWORD'
    ) {
        throw new RuntimeException('DB_CREDENTIALS_NOT_SET');
    }

    $dsn = 'mysql:host=' . $dbHost
        . ';port=' . $dbPort
        . ';dbname=' . $dbName
        . ';charset=utf8mb4';

    return new PDO($dsn, $dbUser, $dbPass, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]);
}
