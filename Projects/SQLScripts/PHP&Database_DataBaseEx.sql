
-- ==============================================
-- SENSOR DATABASE ASSIGNMENT
-- Database: u901022221_YahirMagana
-- ==============================================

USE `u901022221_YahirMagana`;

-- ==============================================
-- STEP 1: REMOVE OLD SENSOR TABLES AND VIEW
-- ==============================================

DROP VIEW IF EXISTS ensor_combined;
DROP TABLE IF EXISTS sensor_data;
DROP TABLE IF EXISTS sensor_register;


-- ==============================================
-- STEP 2: CREATE SENSOR REGISTER
-- ==============================================

CREATE TABLE sensor_register (
    node_name VARCHAR(10) NOT NULL,
    manufacturer VARCHAR(10) NOT NULL,
    longitude DECIMAL(15,10) NOT NULL,
    latitude DECIMAL(15,10) NOT NULL,

    PRIMARY KEY (node_name),

    CONSTRAINT chk_node_name
        CHECK (node_name REGEXP '^node_[0-9]+$'),

    CONSTRAINT chk_longitude
        CHECK (longitude BETWEEN -180 AND 180),

    CONSTRAINT chk_latitude
        CHECK (latitude BETWEEN -90 AND 90)

) ENGINE=InnoDB;


-- ==============================================
-- STEP 3: CREATE SENSOR DATA
-- ==============================================

CREATE TABLE sensor_data (
    node_name VARCHAR(10) NOT NULL,
    time_received DATETIME NOT NULL,
    temperature DECIMAL(6,2) NOT NULL,
    humidity DECIMAL(6,2) NOT NULL,

    PRIMARY KEY (node_name, time_received),

    CONSTRAINT chk_temperature
        CHECK (temperature BETWEEN -10 AND 100),

    CONSTRAINT chk_humidity
        CHECK (humidity BETWEEN 0 AND 100),

    CONSTRAINT fk_sensor_node
        FOREIGN KEY (node_name)
        REFERENCES sensor_register(node_name)
        ON UPDATE CASCADE
        ON DELETE RESTRICT

) ENGINE=InnoDB;


-- ==============================================
-- STEP 4: REGISTER FIVE SENSORS
-- ==============================================

-- Example coordinates in Northern California

INSERT INTO sensor_register
(node_name, manufacturer, longitude, latitude)
VALUES
('node_1', 'Bosch',     -122.6765000000, 38.3390000000),
('node_2', 'Honeywell', -122.7141000000, 38.4405000000),
('node_3', 'Sensirion', -122.4580000000, 38.2919000000),
('node_4', 'Omron',     -122.5776000000, 38.1074000000),
('node_5', 'Siemens',   -122.2869000000, 38.2975000000);


-- ==============================================
-- STEP 5: INSERT SENSOR MEASUREMENTS
-- ==============================================

-- Starting: October 1, 2022 at 11:00 AM
-- Interval: Every 30 minutes
-- Four measurements per sensor
-- Total: 20 measurements

INSERT INTO sensor_data
(node_name, time_received, temperature, humidity)
VALUES

-- 11:00 AM
('node_1', '2022-10-01 11:00:00', 22.50, 45.00),
('node_2', '2022-10-01 11:00:00', 24.10, 48.00),
('node_3', '2022-10-01 11:00:00', 21.80, 52.00),
('node_4', '2022-10-01 11:00:00', 25.20, 40.00),
('node_5', '2022-10-01 11:00:00', 23.60, 55.00),

-- 11:30 AM
('node_1', '2022-10-01 11:30:00', 22.80, 46.00),
('node_2', '2022-10-01 11:30:00', 24.50, 47.00),
('node_3', '2022-10-01 11:30:00', 22.00, 51.00),
('node_4', '2022-10-01 11:30:00', 25.50, 41.00),
('node_5', '2022-10-01 11:30:00', 23.90, 54.00),

-- 12:00 PM
('node_1', '2022-10-01 12:00:00', 23.10, 44.00),
('node_2', '2022-10-01 12:00:00', 24.80, 49.00),
('node_3', '2022-10-01 12:00:00', 22.30, 50.00),
('node_4', '2022-10-01 12:00:00', 25.90, 39.00),
('node_5', '2022-10-01 12:00:00', 24.20, 53.00),

-- 12:30 PM
('node_1', '2022-10-01 12:30:00', 23.50, 43.00),
('node_2', '2022-10-01 12:30:00', 25.10, 46.00),
('node_3', '2022-10-01 12:30:00', 22.70, 49.00),
('node_4', '2022-10-01 12:30:00', 26.20, 38.00),
('node_5', '2022-10-01 12:30:00', 24.60, 52.00);


-- ==============================================
-- STEP 6: CREATE COMBINED VIEW
-- ==============================================

CREATE VIEW ensor_combined AS
SELECT
    sd.node_name,
    sr.manufacturer,
    sr.longitude,
    sr.latitude,
    sd.time_received,
    sd.temperature,
    sd.humidity
FROM sensor_data AS sd
INNER JOIN sensor_register AS sr
    ON sd.node_name = sr.node_name;


-- ==============================================
-- STEP 7: DISPLAY RESULTS
-- ==============================================

SELECT * FROM sensor_register;

SELECT * FROM sensor_data
ORDER BY time_received, node_name;

SELECT * FROM ensor_combined
ORDER BY time_received, node_name;


-- ==============================================
-- STEP 8: VERIFY RECORD COUNTS
-- ==============================================

SELECT
    node_name,
    COUNT(*) AS total_readings
FROM sensor_data
GROUP BY node_name
ORDER BY node_name;


-- ==============================================
-- STEP 9: VERIFY DATABASE STRUCTURE
-- ==============================================

SHOW TABLES;
