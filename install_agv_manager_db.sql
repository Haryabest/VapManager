-- =============================================================================
-- AgvNewUi / AGV Manager — полная установка базы данных (одним файлом)
-- MySQL 5.7+ / MariaDB 10.2+
--
-- Запуск с консоли сервера:
--   mysql -u root -p < install_agv_manager_db.sql
--
-- Или:
--   mysql -u root -p -e "SOURCE /path/to/install_agv_manager_db.sql"
--
-- После выполнения в config.ini клиента укажите db_host = IP этого сервера.
-- Первого пользователя создайте через окно регистрации/логина приложения.
-- =============================================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE DATABASE IF NOT EXISTS agv_manager_db
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE agv_manager_db;

-- =============================================================================
-- 1. users
-- =============================================================================
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin','tech','viewer') NOT NULL DEFAULT 'viewer',
    is_active TINYINT(1) NOT NULL DEFAULT 1,
    last_login DATETIME NULL,
    remember_token VARCHAR(128) NULL,
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
    INDEX idx_users_full_name (full_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 2. agv_models
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_models (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(128) NOT NULL UNIQUE,
    version_po VARCHAR(64) DEFAULT '',
    version_eplan VARCHAR(64) DEFAULT '',
    category VARCHAR(64) DEFAULT '',
    capacityKg INT NOT NULL DEFAULT 0,
    maxSpeed INT NOT NULL DEFAULT 0,
    dimensions VARCHAR(64) DEFAULT '',
    coupling_count INT NOT NULL DEFAULT 0,
    direction VARCHAR(16) DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 3. model_maintenance_template
-- =============================================================================
CREATE TABLE IF NOT EXISTS model_maintenance_template (
    id INT AUTO_INCREMENT PRIMARY KEY,
    model_name VARCHAR(128) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default TINYINT(1) NOT NULL DEFAULT 1,
    INDEX idx_model (model_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 4. agv_list
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_list (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL UNIQUE,
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
    INDEX idx_status (status),
    INDEX idx_assigned (assigned_user),
    INDEX idx_agv_list_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 5. agv_tasks
-- =============================================================================
CREATE TABLE IF NOT EXISTS agv_tasks (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default TINYINT(1) NOT NULL DEFAULT 1,
    next_date DATE NOT NULL,
    assigned_to VARCHAR(64) DEFAULT '',
    delegated_by VARCHAR(64) DEFAULT '',
    INDEX idx_agv (agv_id),
    INDEX idx_next_date (next_date),
    INDEX idx_agv_tasks_agv_next (agv_id, next_date),
    INDEX idx_agv_tasks_assigned_to (assigned_to)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 6. agv_task_history
-- =============================================================================
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
    INDEX idx_hist_agv_date (agv_id, completed_at),
    INDEX idx_hist_completed_ts (completed_ts),
    INDEX idx_hist_done_task (completed_at, agv_id, task_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 7. notifications
-- =============================================================================
CREATE TABLE IF NOT EXISTS notifications (
    id INT AUTO_INCREMENT PRIMARY KEY,
    target_user VARCHAR(64) NOT NULL,
    title VARCHAR(256) NOT NULL,
    message MEDIUMTEXT,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_target (target_user),
    INDEX idx_notifications_target_read (target_user, is_read, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 8. maintenance_notification_sent
-- =============================================================================
CREATE TABLE IF NOT EXISTS maintenance_notification_sent (
    agv_id VARCHAR(64) NOT NULL,
    notify_date DATE NOT NULL,
    PRIMARY KEY (agv_id, notify_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 9. task_chat_threads
-- =============================================================================
CREATE TABLE IF NOT EXISTS task_chat_threads (
    id INT AUTO_INCREMENT PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_id INT NULL,
    task_name VARCHAR(255) NULL,
    created_by VARCHAR(64) NOT NULL,
    recipient_user VARCHAR(64) NULL COMMENT 'NULL = все админы',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    closed_at DATETIME NULL,
    closed_by VARCHAR(64) NULL,
    INDEX idx_created_by (created_by),
    INDEX idx_recipient (recipient_user),
    INDEX idx_closed (closed_at),
    INDEX idx_chat_threads_created_at (created_by, created_at),
    INDEX idx_chat_threads_recipient_at (recipient_user, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 10. task_chat_messages
-- =============================================================================
CREATE TABLE IF NOT EXISTS task_chat_messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    thread_id INT NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    message MEDIUMTEXT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_thread (thread_id),
    INDEX idx_chat_messages_thread_created (thread_id, created_at),
    INDEX idx_chat_messages_thread_id_id (thread_id, id),
    CONSTRAINT fk_task_chat_messages_thread
        FOREIGN KEY (thread_id) REFERENCES task_chat_threads(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 11. task_chat_hidden
-- =============================================================================
CREATE TABLE IF NOT EXISTS task_chat_hidden (
    thread_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id, username),
    INDEX idx_hidden_user (username),
    INDEX idx_chat_hidden_user_thread (username, thread_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- =============================================================================
-- 12. task_chat_message_hidden
-- =============================================================================
CREATE TABLE IF NOT EXISTS task_chat_message_hidden (
    message_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (message_id, username),
    INDEX idx_msg_hidden_user (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SET FOREIGN_KEY_CHECKS = 1;

-- =============================================================================
-- Готово. Имя базы: agv_manager_db (как в db.cpp).
-- =============================================================================
