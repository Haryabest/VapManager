-- =============================================================================
-- VapManager — PostgreSQL installer
-- Run: psql -U postgres -f install_agv_manager_db.sql
-- Client: config.ini (db_host, db_port, db_name, db_user, db_password)
-- =============================================================================

-- Create DB once (from psql as superuser). On PG 15+: IF NOT EXISTS is supported.
SELECT 'CREATE DATABASE agv_manager_db ENCODING ''UTF8'' TEMPLATE template0'
WHERE NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = 'agv_manager_db')\gexec

\c agv_manager_db

CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- Users
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role VARCHAR(16) NOT NULL DEFAULT 'viewer'
        CHECK (role IN ('admin', 'tech', 'viewer')),
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    last_login TIMESTAMP NULL,
    remember_token VARCHAR(128) NULL,
    active_session_token VARCHAR(128) NULL,
    permanent_recovery_key VARCHAR(32) NULL,
    admin_invite_key VARCHAR(16) NULL,
    admin_invite_key_expire TIMESTAMP NULL,
    tech_invite_key VARCHAR(16) NULL,
    tech_invite_key_expire TIMESTAMP NULL,
    full_name VARCHAR(128) NULL,
    employee_id VARCHAR(16) NULL,
    position VARCHAR(128) NULL,
    department VARCHAR(128) NULL,
    mobile VARCHAR(32) NULL,
    ext_phone VARCHAR(16) NULL,
    email VARCHAR(128) NULL,
    telegram VARCHAR(64) NULL,
    avatar BYTEA NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_users_full_name ON users (full_name);
CREATE INDEX IF NOT EXISTS idx_users_role ON users (role);
CREATE INDEX IF NOT EXISTS idx_users_active ON users (is_active);

-- AGV models
CREATE TABLE IF NOT EXISTS agv_models (
    id SERIAL PRIMARY KEY,
    name VARCHAR(128) NOT NULL UNIQUE,
    version_po VARCHAR(64) DEFAULT '',
    version_eplan VARCHAR(64) DEFAULT '',
    category VARCHAR(64) DEFAULT '',
    "capacityKg" INT NOT NULL DEFAULT 0,
    "maxSpeed" INT NOT NULL DEFAULT 0,
    dimensions VARCHAR(64) DEFAULT '',
    coupling_count INT NOT NULL DEFAULT 0,
    direction VARCHAR(16) DEFAULT '',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_agv_models_created_at ON agv_models (created_at);

CREATE TABLE IF NOT EXISTS model_maintenance_template (
    id SERIAL PRIMARY KEY,
    model_name VARCHAR(128) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default BOOLEAN NOT NULL DEFAULT TRUE
);
CREATE INDEX IF NOT EXISTS idx_model ON model_maintenance_template (model_name);
CREATE INDEX IF NOT EXISTS idx_model_task ON model_maintenance_template (model_name, task_name);

CREATE TABLE IF NOT EXISTS agv_list (
    id SERIAL PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL UNIQUE,
    model VARCHAR(128) NOT NULL,
    serial VARCHAR(128) DEFAULT '',
    status VARCHAR(64) DEFAULT '',
    alias VARCHAR(128) DEFAULT '',
    kilometers INT NOT NULL DEFAULT 0,
    "blueprintPath" VARCHAR(512) DEFAULT '',
    "lastActive" DATE NULL,
    assigned_user VARCHAR(64) DEFAULT '',
    assigned_by VARCHAR(64) DEFAULT '',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_status ON agv_list (status);
CREATE INDEX IF NOT EXISTS idx_assigned ON agv_list (assigned_user);
CREATE INDEX IF NOT EXISTS idx_agv_list_created_at ON agv_list (created_at);
CREATE INDEX IF NOT EXISTS idx_agv_list_model ON agv_list (model);

CREATE TABLE IF NOT EXISTS agv_tasks (
    id SERIAL PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_name VARCHAR(255) NOT NULL,
    task_description TEXT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    duration_minutes INT NOT NULL DEFAULT 0,
    is_default BOOLEAN NOT NULL DEFAULT TRUE,
    next_date DATE NOT NULL,
    assigned_to VARCHAR(64) DEFAULT '',
    delegated_by VARCHAR(64) DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_agv ON agv_tasks (agv_id);
CREATE INDEX IF NOT EXISTS idx_next_date ON agv_tasks (next_date);
CREATE INDEX IF NOT EXISTS idx_agv_tasks_next_date ON agv_tasks (next_date);
CREATE INDEX IF NOT EXISTS idx_agv_tasks_agv_next ON agv_tasks (agv_id, next_date);
CREATE INDEX IF NOT EXISTS idx_agv_tasks_assigned_to ON agv_tasks (assigned_to);
CREATE INDEX IF NOT EXISTS idx_agv_tasks_delegated_by ON agv_tasks (delegated_by);

CREATE TABLE IF NOT EXISTS agv_task_history (
    id SERIAL PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_id INT NULL,
    task_name VARCHAR(255) NOT NULL,
    interval_days INT NOT NULL DEFAULT 0,
    completed_at DATE NOT NULL,
    completed_ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    next_date_after DATE NULL,
    performed_by VARCHAR(128) NULL
);
CREATE INDEX IF NOT EXISTS idx_hist_agv_date ON agv_task_history (agv_id, completed_at);
CREATE INDEX IF NOT EXISTS idx_hist_completed_ts ON agv_task_history (completed_ts);
CREATE INDEX IF NOT EXISTS idx_hist_done_task ON agv_task_history (completed_at, agv_id, task_name);
CREATE INDEX IF NOT EXISTS idx_hist_performed_by ON agv_task_history (performed_by);

CREATE TABLE IF NOT EXISTS agv_error_logs (
    id SERIAL PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    error_date DATE NOT NULL,
    error_type VARCHAR(64) NOT NULL,
    title VARCHAR(256) NOT NULL,
    time_from TIME NULL,
    time_to TIME NULL,
    duration_minutes INT NOT NULL DEFAULT 0,
    created_by VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_agv_date ON agv_error_logs (agv_id, error_date);
CREATE INDEX IF NOT EXISTS idx_date ON agv_error_logs (error_date);
CREATE INDEX IF NOT EXISTS idx_error_type ON agv_error_logs (error_type);
CREATE INDEX IF NOT EXISTS idx_created_by ON agv_error_logs (created_by);

CREATE TABLE IF NOT EXISTS notifications (
    id SERIAL PRIMARY KEY,
    target_user VARCHAR(64) NOT NULL,
    title VARCHAR(256) NOT NULL,
    message TEXT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_target ON notifications (target_user);
CREATE INDEX IF NOT EXISTS idx_notifications_target_read ON notifications (target_user, is_read, created_at);
CREATE INDEX IF NOT EXISTS idx_notifications_created_at ON notifications (created_at);

CREATE TABLE IF NOT EXISTS maintenance_notification_sent (
    agv_id VARCHAR(64) NOT NULL,
    notify_date DATE NOT NULL,
    PRIMARY KEY (agv_id, notify_date)
);

CREATE TABLE IF NOT EXISTS task_chat_threads (
    id SERIAL PRIMARY KEY,
    agv_id VARCHAR(64) NOT NULL,
    task_id INT NULL,
    task_name VARCHAR(255) NULL,
    created_by VARCHAR(64) NOT NULL,
    recipient_user VARCHAR(64) NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    closed_at TIMESTAMP NULL,
    closed_by VARCHAR(64) NULL
);
CREATE INDEX IF NOT EXISTS idx_created_by ON task_chat_threads (created_by);
CREATE INDEX IF NOT EXISTS idx_recipient ON task_chat_threads (recipient_user);
CREATE INDEX IF NOT EXISTS idx_closed ON task_chat_threads (closed_at);
CREATE INDEX IF NOT EXISTS idx_chat_threads_created_at ON task_chat_threads (created_by, created_at);
CREATE INDEX IF NOT EXISTS idx_chat_threads_recipient_at ON task_chat_threads (recipient_user, created_at);
CREATE INDEX IF NOT EXISTS idx_chat_threads_agv_task ON task_chat_threads (agv_id, task_id);

CREATE TABLE IF NOT EXISTS task_chat_messages (
    id SERIAL PRIMARY KEY,
    thread_id INT NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    message TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_thread ON task_chat_messages (thread_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_thread_created ON task_chat_messages (thread_id, created_at);
CREATE INDEX IF NOT EXISTS idx_chat_messages_thread_id_id ON task_chat_messages (thread_id, id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_from_user ON task_chat_messages (from_user);

CREATE TABLE IF NOT EXISTS task_chat_hidden (
    thread_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id, username)
);
CREATE INDEX IF NOT EXISTS idx_hidden_user ON task_chat_hidden (username);
CREATE INDEX IF NOT EXISTS idx_chat_hidden_user_thread ON task_chat_hidden (username, thread_id);

CREATE TABLE IF NOT EXISTS task_chat_message_hidden (
    message_id INT NOT NULL,
    username VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (message_id, username)
);
CREATE INDEX IF NOT EXISTS idx_msg_hidden_user ON task_chat_message_hidden (username);

-- Application user (change password after install)
DO $$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'vapmanager') THEN
    CREATE ROLE vapmanager LOGIN PASSWORD 'vapmanager_change_me';
  END IF;
END$$;

GRANT CONNECT ON DATABASE agv_manager_db TO vapmanager;
GRANT USAGE ON SCHEMA public TO vapmanager;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO vapmanager;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO vapmanager;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
  GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO vapmanager;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
  GRANT USAGE, SELECT ON SEQUENCES TO vapmanager;
