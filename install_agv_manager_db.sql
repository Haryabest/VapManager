-- =============================================================================
-- VapManager / AGV Manager - full database installer
-- MySQL 5.7+ / MariaDB 10.2+
--
-- Run on the MySQL server:
--   mysql -u root -p < install_agv_manager_db.sql
--
-- The application uses database name: agv_manager_db
-- Client host is configured in config.ini: db_host=SERVER_IP[:PORT]
-- First normal user/admin is created from the application UI.
-- =============================================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE DATABASE IF NOT EXISTS agv_manager_db
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE agv_manager_db;

-- =============================================================================
-- Users, authentication, profile, avatar, invite keys
-- =============================================================================
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin','tech','viewer') NOT NULL DEFAULT 'viewer',
    is_active TINYINT(1) NOT NULL DEFAULT 1,
    last_login DATETIME NULL,
    remember_token VARCHAR(128) NULL,
    active_session_token VARCHAR(128) NULL,
    permanent_recovery_key VARCHAR(32) NULL,
    admin_invite_key VARCHAR(16) NULL,
    admin_invite_key_expire DATETIME NULL,
    tech_invite_key VARCHAR(16) NULL,
    tech_invite_key_expire DATETIME NULL,
    full_name VARCHAR(128) NULL,
    employee_id VARCHAR(16) NULL,
    position VARCHAR(128) NULL,
    department VARCHAR(128) NULL,
    mobile VARCHAR(32) NULL,
    ext_phone VARCHAR(16) NULL,
    email VARCHAR(128) NULL,
    telegram VARCHAR(64) NULL,
    avatar LONGBLOB NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_users_username (username),
    KEY idx_users_full_name (full_name),
    KEY idx_users_role (role),
    KEY idx_users_active (is_active)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- AGV models and model maintenance templates
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_models (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    version_po VARCHAR(64) DEFAULT '',
    version_eplan VARCHAR(64) DEFAULT '',
    category VARCHAR(64) DEFAULT '',
    capacityKg INT NOT NULL DEFAULT 0,
    maxSpeed INT NOT NULL DEFAULT 0,
    dimensions VARCHAR(64) DEFAULT '',
    coupling_count INT NOT NULL DEFAULT 0,
    direction VARCHAR(16) DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_agv_models_name (name),
    KEY idx_agv_models_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS model_maintenance_template (
    id INT AUTO_INCREMENT PRIMARY KEY,
    model_name VARCHAR(128) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default TINYINT(1) NOT NULL DEFAULT 1,
    KEY idx_model (model_name),
    KEY idx_model_task (model_name, task_name(120))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- AGV list and scheduled maintenance tasks
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_list (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    model VARCHAR(128) NOT NULL,
    serial VARCHAR(128) DEFAULT '',
    status VARCHAR(64) DEFAULT '',
    alias VARCHAR(128) DEFAULT '',
    kilometers INT NOT NULL DEFAULT 0,
    blueprintPath VARCHAR(512) DEFAULT '',
    lastActive DATE NULL,
    assigned_user VARCHAR(64) DEFAULT '',
    assigned_by VARCHAR(64) DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_agv_list_agv_id (agv_id),
    KEY idx_status (status),
    KEY idx_assigned (assigned_user),
    KEY idx_agv_list_created_at (created_at),
    KEY idx_agv_list_model (model)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS agv_tasks (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default TINYINT(1) NOT NULL DEFAULT 1,
    next_date DATE NOT NULL,
    assigned_to VARCHAR(64) DEFAULT '',
    delegated_by VARCHAR(64) DEFAULT '',
    KEY idx_agv (agv_id),
    KEY idx_next_date (next_date),
    KEY idx_agv_tasks_next_date (next_date),
    KEY idx_agv_tasks_agv_next (agv_id, next_date),
    KEY idx_agv_tasks_assigned_to (assigned_to),
    KEY idx_agv_tasks_delegated_by (delegated_by)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS agv_task_history (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_id INT NULL,
    task_name VARCHAR(255) NOT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    completed_at DATE NOT NULL,
    completed_ts DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    next_date_after DATE NULL,
    performed_by VARCHAR(128) NULL,
    KEY idx_hist_agv_date (agv_id, completed_at),
    KEY idx_hist_completed_ts (completed_ts),
    KEY idx_hist_done_task (completed_at, agv_id, task_name(120)),
    KEY idx_hist_performed_by (performed_by)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- AGV error journal
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_error_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    error_date DATE NOT NULL,
    error_type VARCHAR(64) NOT NULL,
    title VARCHAR(256) NOT NULL,
    time_from TIME NULL,
    time_to TIME NULL,
    duration_minutes INT NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_agv_date (agv_id, error_date),
    KEY idx_date (error_date),
    KEY idx_error_type (error_type),
    KEY idx_created_by (created_by)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- Notifications
-- =============================================================================
CREATE TABLE IF NOT EXISTS notifications (
    id INT AUTO_INCREMENT PRIMARY KEY,
    target_user VARCHAR(64) NOT NULL,
    title VARCHAR(256) NOT NULL,
    message MEDIUMTEXT NULL,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_target (target_user),
    KEY idx_notifications_target_read (target_user, is_read, created_at),
    KEY idx_notifications_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS maintenance_notification_sent (
    agv_id VARCHAR(64) NOT NULL,
    notify_date DATE NOT NULL,
    PRIMARY KEY (agv_id, notify_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- Task chat
-- =============================================================================
CREATE TABLE IF NOT EXISTS task_chat_threads (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_id INT NULL,
    task_name VARCHAR(255) NULL,
    created_by VARCHAR(64) NOT NULL,
    recipient_user VARCHAR(64) NULL COMMENT 'NULL means all admins',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    closed_at DATETIME NULL,
    closed_by VARCHAR(64) NULL,
    KEY idx_created_by (created_by),
    KEY idx_recipient (recipient_user),
    KEY idx_closed (closed_at),
    KEY idx_chat_threads_created_at (created_by, created_at),
    KEY idx_chat_threads_recipient_at (recipient_user, created_at),
    KEY idx_chat_threads_agv_task (agv_id, task_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS task_chat_messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    thread_id INT NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    message MEDIUMTEXT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_thread (thread_id),
    KEY idx_chat_messages_thread_created (thread_id, created_at),
    KEY idx_chat_messages_thread_id_id (thread_id, id),
    KEY idx_chat_messages_from_user (from_user)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS task_chat_hidden (
    thread_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id, username),
    KEY idx_hidden_user (username),
    KEY idx_chat_hidden_user_thread (username, thread_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS task_chat_message_hidden (
    message_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (message_id, username),
    KEY idx_msg_hidden_user (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- Existing database normalization.
-- Old installations may already have tables created with shorter columns
-- (for example agv_models.name VARCHAR(15)), so CREATE TABLE IF NOT EXISTS
-- alone is not enough. These ALTERs are safe for a fresh database too.
-- =============================================================================
DROP PROCEDURE IF EXISTS add_column_if_missing;
DELIMITER //
CREATE PROCEDURE add_column_if_missing(
    IN p_table_name VARCHAR(64),
    IN p_column_name VARCHAR(64),
    IN p_column_definition TEXT
)
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = p_table_name
          AND COLUMN_NAME = p_column_name
    ) THEN
        SET @sql_text = CONCAT('ALTER TABLE `', p_table_name, '` ADD COLUMN ', p_column_definition);
        PREPARE stmt FROM @sql_text;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;
    END IF;
END//
DELIMITER ;

CALL add_column_if_missing('users', 'remember_token', '`remember_token` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'active_session_token', '`active_session_token` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'permanent_recovery_key', '`permanent_recovery_key` VARCHAR(32) NULL');
CALL add_column_if_missing('users', 'admin_invite_key', '`admin_invite_key` VARCHAR(16) NULL');
CALL add_column_if_missing('users', 'admin_invite_key_expire', '`admin_invite_key_expire` DATETIME NULL');
CALL add_column_if_missing('users', 'tech_invite_key', '`tech_invite_key` VARCHAR(16) NULL');
CALL add_column_if_missing('users', 'tech_invite_key_expire', '`tech_invite_key_expire` DATETIME NULL');
CALL add_column_if_missing('users', 'full_name', '`full_name` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'employee_id', '`employee_id` VARCHAR(16) NULL');
CALL add_column_if_missing('users', 'position', '`position` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'department', '`department` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'mobile', '`mobile` VARCHAR(32) NULL');
CALL add_column_if_missing('users', 'ext_phone', '`ext_phone` VARCHAR(16) NULL');
CALL add_column_if_missing('users', 'email', '`email` VARCHAR(128) NULL');
CALL add_column_if_missing('users', 'telegram', '`telegram` VARCHAR(64) NULL');
CALL add_column_if_missing('users', 'avatar', '`avatar` LONGBLOB NULL');
CALL add_column_if_missing('users', 'created_at', '`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP');

CALL add_column_if_missing('agv_models', 'version_po', '`version_po` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_models', 'version_eplan', '`version_eplan` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_models', 'category', '`category` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_models', 'capacityKg', '`capacityKg` INT NOT NULL DEFAULT 0');
CALL add_column_if_missing('agv_models', 'maxSpeed', '`maxSpeed` INT NOT NULL DEFAULT 0');
CALL add_column_if_missing('agv_models', 'dimensions', '`dimensions` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_models', 'coupling_count', '`coupling_count` INT NOT NULL DEFAULT 0');
CALL add_column_if_missing('agv_models', 'direction', '`direction` VARCHAR(16) DEFAULT ''''');
CALL add_column_if_missing('agv_models', 'created_at', '`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP');

CALL add_column_if_missing('agv_list', 'serial', '`serial` VARCHAR(128) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'status', '`status` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'alias', '`alias` VARCHAR(128) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'kilometers', '`kilometers` INT NOT NULL DEFAULT 0');
CALL add_column_if_missing('agv_list', 'blueprintPath', '`blueprintPath` VARCHAR(512) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'lastActive', '`lastActive` DATE NULL');
CALL add_column_if_missing('agv_list', 'assigned_user', '`assigned_user` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'assigned_by', '`assigned_by` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_list', 'created_at', '`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP');

CALL add_column_if_missing('agv_tasks', 'task_description', '`task_description` TEXT NULL');
CALL add_column_if_missing('agv_tasks', 'duration_minutes', '`duration_minutes` INT NOT NULL DEFAULT 0');
CALL add_column_if_missing('agv_tasks', 'assigned_to', '`assigned_to` VARCHAR(64) DEFAULT ''''');
CALL add_column_if_missing('agv_tasks', 'delegated_by', '`delegated_by` VARCHAR(64) DEFAULT ''''');

ALTER TABLE users
    ENGINE=InnoDB,
    CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

ALTER TABLE agv_models
    ENGINE=InnoDB,
    CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    MODIFY COLUMN name VARCHAR(128) NOT NULL,
    MODIFY COLUMN version_po VARCHAR(64) DEFAULT '',
    MODIFY COLUMN version_eplan VARCHAR(64) DEFAULT '',
    MODIFY COLUMN category VARCHAR(64) DEFAULT '',
    MODIFY COLUMN capacityKg INT NOT NULL DEFAULT 0,
    MODIFY COLUMN maxSpeed INT NOT NULL DEFAULT 0,
    MODIFY COLUMN dimensions VARCHAR(64) DEFAULT '',
    MODIFY COLUMN coupling_count INT NOT NULL DEFAULT 0,
    MODIFY COLUMN direction VARCHAR(16) DEFAULT '',
    MODIFY COLUMN created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP;

ALTER TABLE model_maintenance_template
    ENGINE=InnoDB,
    CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    MODIFY COLUMN model_name VARCHAR(128) NOT NULL,
    MODIFY COLUMN task_name VARCHAR(255) NOT NULL,
    MODIFY COLUMN task_description TEXT NULL,
    MODIFY COLUMN interval_days INT NOT NULL DEFAULT 0,
    MODIFY COLUMN duration_minutes INT NOT NULL DEFAULT 0,
    MODIFY COLUMN is_default TINYINT(1) NOT NULL DEFAULT 1;

ALTER TABLE agv_list
    ENGINE=InnoDB,
    CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    MODIFY COLUMN agv_id VARCHAR(64) NOT NULL,
    MODIFY COLUMN model VARCHAR(128) NOT NULL,
    MODIFY COLUMN serial VARCHAR(128) DEFAULT '',
    MODIFY COLUMN status VARCHAR(64) DEFAULT '',
    MODIFY COLUMN alias VARCHAR(128) DEFAULT '',
    MODIFY COLUMN kilometers INT NOT NULL DEFAULT 0,
    MODIFY COLUMN blueprintPath VARCHAR(512) DEFAULT '',
    MODIFY COLUMN assigned_user VARCHAR(64) DEFAULT '',
    MODIFY COLUMN assigned_by VARCHAR(64) DEFAULT '',
    MODIFY COLUMN created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP;

ALTER TABLE agv_tasks
    ENGINE=InnoDB,
    CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    MODIFY COLUMN agv_id VARCHAR(64) NOT NULL,
    MODIFY COLUMN task_name VARCHAR(255) NOT NULL,
    MODIFY COLUMN task_description TEXT NULL,
    MODIFY COLUMN interval_days INT NOT NULL DEFAULT 0,
    MODIFY COLUMN duration_minutes INT NOT NULL DEFAULT 0,
    MODIFY COLUMN is_default TINYINT(1) NOT NULL DEFAULT 1,
    MODIFY COLUMN assigned_to VARCHAR(64) DEFAULT '',
    MODIFY COLUMN delegated_by VARCHAR(64) DEFAULT '';

DROP PROCEDURE IF EXISTS add_column_if_missing;

SET FOREIGN_KEY_CHECKS = 1;

-- =============================================================================
-- Finished. Database agv_manager_db is ready for VapManager.
-- =============================================================================
