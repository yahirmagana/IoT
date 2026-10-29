document.addEventListener('DOMContentLoaded', function () {
    const canvas = document.getElementById('mycanvas');
    const message = document.getElementById('chartMessage');
    let selectedNode = new URLSearchParams(window.location.search).get('node');

    // Read the selected node from the dashboard containing this graph.
    try {
        const parentSelect = window.parent.document.getElementById('node-select');
        if (parentSelect && parentSelect.value) {
            selectedNode = parentSelect.value;
        }
    } catch (error) {
        // If opened on its own, use the node from graph.html's URL instead.
    }

    if (!canvas || !message) {
        console.error('The chart canvas or status message is missing from graph.html.');
        return;
    }

    if (typeof Chart === 'undefined') {
        message.textContent = 'Chart.js did not load. Check that chartjs/chart.umd.min.js exists.';
        return;
    }

    const dataUrl = new URL('/data.php', window.location.href);
    if (selectedNode) {
        dataUrl.searchParams.set('node', selectedNode);
    }

    fetch(dataUrl.toString(), {
        headers: { Accept: 'application/json' },
        cache: 'no-store'
    })
        .then(function (response) {
            if (!response.ok) {
                throw new Error('data.php returned HTTP ' + response.status);
            }
            return response.json();
        })
        .then(function (readings) {
            if (!Array.isArray(readings)) {
                throw new Error('data.php did not return a JSON array.');
            }

            if (readings.length === 0) {
                canvas.hidden = true;
                message.textContent = selectedNode
                    ? 'There are no readings for ' + selectedNode + '.'
                    : 'There are no registered sensor readings to plot.';
                return;
            }

            const labels = readings.map(function (reading) {
                return reading.time_received;
            });
            const temperatures = readings.map(function (reading) {
                return Number(reading.temperature);
            });

            canvas.setAttribute(
                'aria-label',
                'Temperature over time for ' + (selectedNode || 'the selected sensor')
            );
            message.textContent = '';

            new Chart(canvas.getContext('2d'), {
                type: 'bar',
                data: {
                    labels: labels,
                    datasets: [{
                        label: (selectedNode || 'Sensor') + ' Temperature (°C)',
                        data: temperatures,
                        backgroundColor: 'rgba(0, 0, 200, 0.75)',
                        borderColor: 'rgba(0, 0, 200, 1)',
                        hoverBackgroundColor: 'rgba(0, 200, 0, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        title: {
                            display: true,
                            text: 'Sensor Node'
                        }
                    },
                    scales: {
                        x: {
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        y: {
                            title: {
                                display: true,
                                text: 'Temperature (°C)'
                            }
                        }
                    }
                }
            });
        })
        .catch(function (error) {
            console.error('Could not draw the sensor chart:', error);
            message.textContent =
                'The chart data could not be loaded. Check the selected node, data.php, and database settings.';
        });
});