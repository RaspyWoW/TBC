-- MySQL dump 10.19  Distrib 10.3.32-MariaDB, for debian-linux-gnu (aarch64)
--
-- Host: localhost    Database: tbc_logs
-- ------------------------------------------------------
-- Server version	10.3.32-MariaDB-0ubuntu0.20.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `logs_anticheat`
--

DROP TABLE IF EXISTS `logs_anticheat`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `logs_anticheat` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `realm` int(10) unsigned NOT NULL,
  `account` int(10) unsigned NOT NULL,
  `ip` varchar(16) NOT NULL,
  `fingerprint` int(10) unsigned NOT NULL,
  `actionMask` int(10) unsigned DEFAULT NULL,
  `player` varchar(32) NOT NULL,
  `info` varchar(512) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `account` (`account`),
  KEY `ip` (`ip`),
  KEY `time` (`time`),
  KEY `realm` (`realm`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `logs_anticheat`
--

LOCK TABLES `logs_anticheat` WRITE;
/*!40000 ALTER TABLE `logs_anticheat` DISABLE KEYS */;
/*!40000 ALTER TABLE `logs_anticheat` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `logs_db_version`
--

DROP TABLE IF EXISTS `logs_db_version`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `logs_db_version` (
  `required_s2433_01_logs_anticheat` bit(1) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Last applied sql update to DB';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `logs_db_version`
--

LOCK TABLES `logs_db_version` WRITE;
/*!40000 ALTER TABLE `logs_db_version` DISABLE KEYS */;
INSERT INTO `logs_db_version` VALUES (NULL);
/*!40000 ALTER TABLE `logs_db_version` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `logs_spamdetect`
--

DROP TABLE IF EXISTS `logs_spamdetect`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `logs_spamdetect` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `time` timestamp NOT NULL DEFAULT current_timestamp(),
  `realm` int(10) unsigned NOT NULL,
  `accountId` int(11) DEFAULT 0,
  `fromGuid` bigint(20) unsigned DEFAULT 0,
  `fromIP` varchar(16) NOT NULL,
  `fromFingerprint` int(10) unsigned NOT NULL,
  `comment` varchar(8192) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `guid` (`fromGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `logs_spamdetect`
--

LOCK TABLES `logs_spamdetect` WRITE;
/*!40000 ALTER TABLE `logs_spamdetect` DISABLE KEYS */;
/*!40000 ALTER TABLE `logs_spamdetect` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2021-12-31 20:05:31
