<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

try {
    require_once __DIR__ . '/db_connection.php';
    $pdo = getSensorDatabaseConnection();

    $registeredNodes = $pdo->query(
        'SELECT node_name
         FROM sensor_register
         ORDER BY node_name ASC'
    )->fetchAll(PDO::FETCH_COLUMN);

    if ($registeredNodes === []) {
        echo '[]';
        exit;
    }

    $requestedNode = $_GET['node'] ?? '';
    if (!is_string($requestedNode)) {
        $requestedNode = '';
    }

    if ($requestedNode === '') {
        $selectedNode = (string) $registeredNodes[0];
    } elseif (in_array($requestedNode, $registeredNodes, true)) {
        $selectedNode = $requestedNode;
    } else {
        http_response_code(400);
        echo json_encode(['error' => 'The requested sensor node is not registered.']);
        exit;
    }

    $query = $pdo->prepare(
        'SELECT time_received, temperature
         FROM sensor_data
         WHERE node_name = :node_name
         ORDER BY time_received ASC'
    );
    $query->execute(['node_name' => $selectedNode]);

    $readings = array_map(
        static function (array $row): array {
            return [
                'time_received' => (string) $row['time_received'],
                'temperature' => (float) $row['temperature'],
            ];
        },
        $query->fetchAll()
    );

    $json = json_encode($readings, JSON_UNESCAPED_SLASHES);
    if ($json === false) {
        throw new RuntimeException('Could not encode sensor readings as JSON.');
    }

    echo $json;
} catch (Throwable $exception) {
    error_log('Sensor chart data error: ' . $exception->getMessage());
    http_response_code(500);
    echo json_encode(['error' => 'Sensor readings could not be loaded.']);
}