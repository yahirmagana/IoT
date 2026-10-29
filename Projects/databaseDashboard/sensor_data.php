<?php
declare(strict_types=1);

$sensors = [];
$measurements = [];
$selectedNodeSummary = [];
$selectedNode = null;
$loadError = '';

// A Base64 query is accepted as the entire query string, for example
// sensor_data.php?bm9kZT1ub2RlXzI= (decoded value: node=node_2).
$rawQuery = $_SERVER['QUERY_STRING'] ?? '';
if ($rawQuery !== '' && !isset($_GET['node'])) {
    $decodedQuery = base64_decode(rawurldecode($rawQuery), true);

    if ($decodedQuery === false) {
        $loadError = 'The node selection query was not valid Base64.';
    } else {
        $decodedParameters = [];
        parse_str($decodedQuery, $decodedParameters);

        if (isset($decodedParameters['node']) && is_string($decodedParameters['node'])) {
            $_GET = $decodedParameters;
        } else {
            $loadError = 'The Base64 query must decode to a node parameter.';
        }
    }
}

try {
    require_once __DIR__ . '/db_connection.php';
    $pdo = getSensorDatabaseConnection();

    $sensors = $pdo->query(
        'SELECT node_name, manufacturer, longitude, latitude
         FROM sensor_register
         ORDER BY node_name ASC'
    )->fetchAll();

    $measurements = $pdo->query(
        'SELECT node_name, time_received, temperature, humidity
         FROM sensor_data
         ORDER BY node_name ASC, time_received DESC'
    )->fetchAll();

    if ($sensors !== []) {
        $requestedNode = $_GET['node'] ?? '';
        if (!is_string($requestedNode)) {
            $requestedNode = '';
        }

        $registeredNodeNames = array_column($sensors, 'node_name');
        $selectedNode = in_array($requestedNode, $registeredNodeNames, true)
            ? $requestedNode
            : (string) $sensors[0]['node_name'];

        $summaryQuery = $pdo->prepare(
            'SELECT sr.node_name,
                    COUNT(sd.time_received) AS measurement_count,
                    AVG(sd.temperature) AS average_temperature,
                    AVG(sd.humidity) AS average_humidity,
                    MAX(sd.temperature) AS max_temperature,
                    MIN(sd.humidity) AS min_humidity
             FROM sensor_register AS sr
             LEFT JOIN sensor_data AS sd ON sd.node_name = sr.node_name
             WHERE sr.node_name = :node_name
             GROUP BY sr.node_name'
        );
        $summaryQuery->execute(['node_name' => $selectedNode]);
        $selectedNodeSummary = $summaryQuery->fetchAll();
    }
} catch (Throwable $exception) {
    error_log('Sensor dashboard database error: ' . $exception->getMessage());
    if ($exception->getMessage() === 'DB_CREDENTIALS_NOT_SET') {
        $loadError = 'Add your Hostinger database username and password in db_connection.php.';
    } else {
        $loadError = 'The dashboard could not load data. Check the database settings and your Hostinger PHP error log.';
    }
}

function escapeHtml($value): string
{
    return htmlspecialchars((string) $value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}
?>
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>SSU IoT Lab Sensor Dashboard</title>
    <style>
        :root {
            color-scheme: light;
            --green: #a8c83f;
            --green-dark: #83a927;
            --green-pale: #edf3d4;
            --ink: #171717;
            --grid: #e9eded;
            --border: #9dbb37;
        }

        * { box-sizing: border-box; }

        body {
            margin: 0;
            padding: 8px 16px 36px;
            color: var(--ink);
            font-family: Arial, Helvetica, sans-serif;
            background-color: #fff;
            background-image:
                linear-gradient(var(--grid) 1px, transparent 1px),
                linear-gradient(90deg, var(--grid) 1px, transparent 1px);
            background-size: 20px 20px;
        }

        main {
            width: min(100%, 920px);
            margin: 0 auto;
        }

        h1 {
            margin: 0 auto 18px;
            text-align: center;
            font-size: clamp(2rem, 5vw, 2.65rem);
            line-height: 1.12;
            font-weight: 400;
        }

        section { margin: 0 auto 28px; }

        h2 {
            margin: 0 0 10px;
            text-align: center;
            font-size: 1rem;
            font-weight: 400;
        }

        .table-wrap {
            width: 100%;
            overflow-x: auto;
            border: 1px solid var(--border);
            background: rgba(255, 255, 255, .92);
        }

        table {
            width: 100%;
            border-collapse: collapse;
            text-align: left;
            font-size: .96rem;
        }

        th, td {
            padding: 5px 8px;
            border-right: 1px solid var(--border);
            border-bottom: 1px solid var(--border);
            white-space: nowrap;
        }

        th:last-child, td:last-child { border-right: 0; }
        tbody tr:last-child td { border-bottom: 0; }

        thead {
            color: #fff;
            background: var(--green);
        }

        th { font-weight: 700; }
        tbody tr:nth-child(even) { background: var(--green-pale); }
        tbody tr:nth-child(odd) { background: rgba(255, 255, 255, .94); }

        .summary {
            margin: 0 0 8px;
            text-align: right;
            color: #555;
            font-size: .82rem;
        }

        .node-picker form {
            display: flex;
            align-items: center;
            justify-content: center;
            flex-wrap: wrap;
            gap: 10px;
        }

        .node-picker select, .node-picker button {
            min-height: 38px;
            padding: 7px 10px;
            border: 1px solid var(--green-dark);
            border-radius: 3px;
            background: #fff;
            color: var(--ink);
            font: inherit;
        }

        .node-picker button {
            background: var(--green);
            cursor: pointer;
        }

        .notice {
            margin: 0 auto 22px;
            padding: 12px 14px;
            border: 1px solid #c78b2b;
            background: #fff7e7;
            color: #563900;
            line-height: 1.45;
        }

        .empty {
            padding: 14px;
            text-align: center;
            color: #555;
        }

        @media (max-width: 560px) {
            body { padding: 10px 10px 28px; }
            th, td { padding: 6px; font-size: .88rem; }
        }
    </style>
</head>
<body>
<main>
    <h1>Welcome to<br>SSU IoT Lab</h1>

    <?php if ($loadError !== ''): ?>
        <p class="notice"><?= escapeHtml($loadError) ?></p>
    <?php endif; ?>

    <section aria-labelledby="registered-heading">
        <h2 id="registered-heading">Registered Sensor Nodes</h2>
        <p class="summary"><?= count($sensors) ?> registered node(s)</p>
        <div class="table-wrap">
            <table>
                <thead>
                    <tr>
                        <th scope="col">Name</th>
                        <th scope="col">Manufacturer</th>
                        <th scope="col">Longitude</th>
                        <th scope="col">Latitude</th>
                    </tr>
                </thead>
                <tbody>
                <?php if ($sensors === []): ?>
                    <tr><td class="empty" colspan="4">No registered sensors to display.</td></tr>
                <?php else: ?>
                    <?php foreach ($sensors as $sensor): ?>
                        <tr>
                            <td><?= escapeHtml($sensor['node_name']) ?></td>
                            <td><?= escapeHtml($sensor['manufacturer']) ?></td>
                            <td><?= escapeHtml($sensor['longitude']) ?></td>
                            <td><?= escapeHtml($sensor['latitude']) ?></td>
                        </tr>
                    <?php endforeach; ?>
                <?php endif; ?>
                </tbody>
            </table>
        </div>
    </section>

    <section class="node-picker" aria-labelledby="node-picker-heading">
        <h2 id="node-picker-heading">Choose a Sensor Node</h2>
        <form method="get" action="" id="node-picker-form">
            <label for="node-select">Node:</label>
            <select id="node-select" name="node" required onchange="this.form.submit()">
                <?php foreach ($sensors as $sensor): ?>
                    <option value="<?= escapeHtml($sensor['node_name']) ?>" <?= $selectedNode === (string) $sensor['node_name'] ? 'selected' : '' ?>>
                        <?= escapeHtml($sensor['node_name']) ?>
                    </option>
                <?php endforeach; ?>
            </select>
            <button type="submit">Show node</button>
        </form>
    </section>

    <section aria-labelledby="summary-heading">
        <h2 id="summary-heading">Temperature and Humidity Summary for <?= $selectedNode === null ? 'No sensor selected' : escapeHtml($selectedNode) ?></h2>
        <p class="summary">Averages and extremes use all stored readings for the selected node.</p>
        <div class="table-wrap">
            <table>
                <thead>
                    <tr>
                        <th scope="col">Node</th>
                        <th scope="col">Readings</th>
                        <th scope="col">Average Temperature (°C)</th>
                        <th scope="col">Average Humidity (%)</th>
                        <th scope="col">Maximum Temperature (°C)</th>
                        <th scope="col">Minimum Humidity (%)</th>
                    </tr>
                </thead>
                <tbody>
                <?php if ($selectedNodeSummary === []): ?>
                    <tr><td class="empty" colspan="6"><?= $selectedNode === null ? 'No registered sensor nodes.' : 'No readings are available for ' . escapeHtml($selectedNode) . '.' ?></td></tr>
                <?php else: ?>
                    <?php foreach ($selectedNodeSummary as $summary): ?>
                        <tr>
                            <td><?= escapeHtml($summary['node_name']) ?></td>
                            <td><?= escapeHtml($summary['measurement_count']) ?></td>
                            <td><?= $summary['average_temperature'] === null ? '—' : escapeHtml(number_format((float) $summary['average_temperature'], 2)) ?></td>
                            <td><?= $summary['average_humidity'] === null ? '—' : escapeHtml(number_format((float) $summary['average_humidity'], 2)) ?></td>
                            <td><?= $summary['max_temperature'] === null ? '—' : escapeHtml(number_format((float) $summary['max_temperature'], 2)) ?></td>
                            <td><?= $summary['min_humidity'] === null ? '—' : escapeHtml(number_format((float) $summary['min_humidity'], 2)) ?></td>
                        </tr>
                    <?php endforeach; ?>
                <?php endif; ?>
                </tbody>
            </table>
        </div>
    </section>

    <section aria-labelledby="data-heading">
        <h2 id="data-heading">Data Received</h2>
        <p class="summary"><?= count($measurements) ?> measurement(s)</p>
        <div class="table-wrap">
            <table>
                <thead>
                    <tr>
                        <th scope="col">Node</th>
                        <th scope="col">Time</th>
                        <th scope="col">Temperature (°C)</th>
                        <th scope="col">Humidity (%)</th>
                    </tr>
                </thead>
                <tbody>
                <?php if ($measurements === []): ?>
                    <tr><td class="empty" colspan="4">No sensor measurements to display.</td></tr>
                <?php else: ?>
                    <?php foreach ($measurements as $measurement): ?>
                        <tr>
                            <td><?= escapeHtml($measurement['node_name']) ?></td>
                            <td><?= escapeHtml($measurement['time_received']) ?></td>
                            <td><?= escapeHtml($measurement['temperature']) ?></td>
                            <td><?= escapeHtml($measurement['humidity']) ?></td>
                        </tr>
                    <?php endforeach; ?>
                <?php endif; ?>
                </tbody>
            </table>
        </div>
    </section>

    <section aria-labelledby="graph-heading">
        <h2 id="graph-heading">Sensor Node Graph</h2>
        <iframe
            src="/graph.html<?= $selectedNode === null ? '' : '?node=' . rawurlencode($selectedNode) ?>"
            title="Temperature graph for <?= escapeHtml($selectedNode ?? 'registered sensor') ?>"
            loading="lazy"
            style="display:block;width:100%;height:470px;border:1px solid var(--border);background:#fff;"
        ></iframe>
    </section>
</main>
<script>
    document.getElementById('node-picker-form').addEventListener('submit', function (event) {
        event.preventDefault();

        const query = new URLSearchParams(new FormData(this)).toString();
        const base64Query = btoa(query);
        const destination = new URL(window.location.href);
        destination.search = encodeURIComponent(base64Query);

        window.location.assign(destination.toString());
    });
</script>
</body>
</html>