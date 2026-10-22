
-- ============================================
-- MariaDB / MySQL Tutorial
-- Database: u901022221_YahirMagana
-- Final tables: Book and Price
-- ============================================

USE `u901022221_YahirMagana`;


-- ============================================
-- STEP 1: REMOVE OLD TABLES
-- ============================================

-- Remove existing tutorial tables to avoid
-- "Table already exists" errors.

DROP TABLE IF EXISTS `Price`;
DROP TABLE IF EXISTS `Book`;
DROP TABLE IF EXISTS `t2`;


-- ============================================
-- STEP 2: CREATE BOOK TABLE
-- ============================================

CREATE TABLE `Book` (
    `id` INT NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(100) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB;


-- ============================================
-- STEP 3: CREATE PRICE TABLE
-- ============================================

CREATE TABLE `Price` (
    `id` INT NOT NULL AUTO_INCREMENT,
    `price` DECIMAL(10,2) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB;


-- ============================================
-- STEP 4: INSERT BOOK RECORDS
-- ============================================

INSERT INTO `Book` (`id`, `name`)
VALUES
    (1, 'MariaDB Book1'),
    (2, 'MariaDB Book2'),
    (3, 'MariaDB Book3'),
    (4, 'MariaDB Book4'),
    (5, 'MariaDB Book5');


-- ============================================
-- STEP 5: INSERT PRICE RECORDS
-- ============================================

INSERT INTO `Price` (`id`, `price`)
VALUES
    (1, 251.00),
    (2, 252.00),
    (3, 223.00),
    (4, 194.00),
    (5, 305.00);


-- ============================================
-- STEP 6: DISPLAY BOTH TABLES
-- ============================================

SELECT * FROM `Book`;

SELECT * FROM `Price`;


-- ============================================
-- STEP 7: UPDATE A PRICE
-- ============================================

-- Change the price of Book 1 to 250

UPDATE `Price`
SET `price` = 250.00
WHERE `id` = 1;


-- ============================================
-- STEP 8: UPDATE A BOOK NAME
-- ============================================

-- Rename Book 1

UPDATE `Book`
SET `name` = 'MariaDB Book1a'
WHERE `id` = 1;


-- ============================================
-- STEP 9: INSERT AN ADDITIONAL PRICE
-- ============================================

-- AUTO_INCREMENT assigns ID 6.
-- This price is not associated with a book.

INSERT INTO `Price` (`price`)
VALUES (251.00);

SELECT * FROM `Price`;


-- ============================================
-- STEP 10: DELETE THE EXTRA PRICE
-- ============================================

DELETE FROM `Price`
WHERE `id` = 6;


-- ============================================
-- STEP 11: SELECT PRICES BELOW 250
-- ============================================

SELECT *
FROM `Price`
WHERE `price` < 250;


-- ============================================
-- STEP 12: SELECT USING OR
-- ============================================

SELECT *
FROM `Price`
WHERE `id` > 3
OR `price` < 250;


-- ============================================
-- STEP 13: FIND BOOK NAMES ENDING IN 4
-- ============================================

SELECT `name`
FROM `Book`
WHERE `name` LIKE '%4';


-- ============================================
-- STEP 14: FIND PRICES STARTING WITH 2
-- ============================================

SELECT *
FROM `Price`
WHERE `price` >= 200
AND `price` < 300
ORDER BY `price` ASC;


-- ============================================
-- STEP 15: INNER JOIN
-- ============================================

-- Combine book names and prices using IDs

SELECT
    `Book`.`id`,
    `Book`.`name`,
    `Price`.`price`
FROM `Book`
INNER JOIN `Price`
ON `Book`.`id` = `Price`.`id`;


-- ============================================
-- STEP 16: VERIFY DATABASE STRUCTURE
-- ============================================

SHOW TABLES;

DESCRIBE `Book`;

DESCRIBE `Price`;


-- ============================================
-- STEP 17: DISPLAY FINAL RESULTS
-- ============================================

SELECT * FROM `Book`
ORDER BY `id`;

SELECT * FROM `Price`
ORDER BY `id`;

SELECT
    Book.id,
    Book.name,
    Price.price
FROM Book
INNER JOIN Price
ON Book.id = Price.id;
