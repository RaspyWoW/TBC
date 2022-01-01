
ALTER TABLE `characters`
	ADD COLUMN `xp_rate` FLOAT NOT NULL DEFAULT '-1.0' AFTER `xp`;

ALTER TABLE `characters`
	ADD COLUMN `allow_export` TINYINT(3) UNSIGNED NOT NULL DEFAULT '1' AFTER `deleteDate`;
