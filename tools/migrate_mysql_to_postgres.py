#!/usr/bin/env python3
"""
Migrate agv_manager_db from MySQL to PostgreSQL for VapManager.

Requires: pip install mysql-connector-python psycopg2-binary

Example:
  python migrate_mysql_to_postgres.py ^
    --mysql-host localhost --mysql-user root --mysql-password "" ^
    --pg-host localhost --pg-user vapmanager --pg-password vapmanager_change_me
"""

from __future__ import annotations

import argparse
import sys
from typing import Any

TABLES = [
    "users",
    "agv_models",
    "model_maintenance_template",
    "agv_list",
    "agv_tasks",
    "agv_task_history",
    "agv_error_logs",
    "notifications",
    "maintenance_notification_sent",
    "task_chat_threads",
    "task_chat_messages",
    "task_chat_hidden",
    "task_chat_message_hidden",
]

# MySQL column name -> PostgreSQL (quoted camelCase)
COLUMN_MAP = {
    "capacityKg": '"capacityKg"',
    "maxSpeed": '"maxSpeed"',
    "blueprintPath": '"blueprintPath"',
    "lastActive": '"lastActive"',
}


def pg_column(name: str) -> str:
    return COLUMN_MAP.get(name, name)


def fetch_mysql_rows(mysql_conn, table: str) -> tuple[list[str], list[tuple[Any, ...]]]:
    cur = mysql_conn.cursor()
    cur.execute(f"SELECT * FROM `{table}`")
    cols = [d[0] for d in cur.description]
    rows = cur.fetchall()
    cur.close()
    return cols, rows


def truncate_pg(pg_conn, table: str) -> None:
    cur = pg_conn.cursor()
    cur.execute(f'TRUNCATE TABLE "{table}" RESTART IDENTITY CASCADE')
    pg_conn.commit()
    cur.close()


def pg_table_meta(pg_conn, table: str) -> dict[str, str]:
    cur = pg_conn.cursor()
    cur.execute(
        """
        SELECT column_name, data_type FROM information_schema.columns
        WHERE table_schema = 'public' AND table_name = %s
        """,
        (table,),
    )
    meta = {r[0]: r[1] for r in cur.fetchall()}
    cur.close()
    return meta


def adapt_value(data_type: str, value: Any) -> Any:
    if value is None:
        return None
    if data_type == "boolean" and isinstance(value, int):
        return bool(value)
    return value


def insert_pg(pg_conn, table: str, cols: list[str], rows: list[tuple[Any, ...]]) -> int:
    if not rows:
        return 0
    meta = pg_table_meta(pg_conn, table)
    indices = [i for i, c in enumerate(cols) if c in meta]
    if not indices:
        return 0
    use_cols = [cols[i] for i in indices]
    pg_cols = [pg_column(c) for c in use_cols]
    placeholders = ", ".join(["%s"] * len(use_cols))
    col_sql = ", ".join(pg_cols)
    sql = f"INSERT INTO {table} ({col_sql}) VALUES ({placeholders})"
    trimmed = [
        tuple(adapt_value(meta[cols[i]], row[i]) for i in indices)
        for row in rows
    ]
    cur = pg_conn.cursor()
    cur.executemany(sql, trimmed)
    pg_conn.commit()
    cur.close()
    return len(trimmed)


def reset_sequences(pg_conn) -> None:
    serial_tables = [
        "users", "agv_models", "model_maintenance_template", "agv_list",
        "agv_tasks", "agv_task_history", "agv_error_logs", "notifications",
        "task_chat_threads", "task_chat_messages",
    ]
    cur = pg_conn.cursor()
    for table in serial_tables:
        cur.execute(
            f"SELECT setval(pg_get_serial_sequence('{table}', 'id'), "
            f"COALESCE((SELECT MAX(id) FROM {table}), 1), true)"
        )
    pg_conn.commit()
    cur.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="MySQL agv_manager_db -> PostgreSQL")
    parser.add_argument("--mysql-host", default="localhost")
    parser.add_argument("--mysql-port", type=int, default=3306)
    parser.add_argument("--mysql-user", default="root")
    parser.add_argument("--mysql-password", default="")
    parser.add_argument("--mysql-database", default="agv_manager_db")
    parser.add_argument("--pg-host", default="localhost")
    parser.add_argument("--pg-port", type=int, default=5432)
    parser.add_argument("--pg-user", default="vapmanager")
    parser.add_argument("--pg-password", default="vapmanager_change_me")
    parser.add_argument("--pg-database", default="agv_manager_db")
    parser.add_argument("--skip-truncate", action="store_true")
    args = parser.parse_args()

    try:
        import mysql.connector
        import psycopg2
    except ImportError as e:
        print("Install dependencies: pip install mysql-connector-python psycopg2-binary", file=sys.stderr)
        print(e, file=sys.stderr)
        return 1

    print("Connecting to MySQL...")
    mysql_conn = mysql.connector.connect(
        host=args.mysql_host,
        port=args.mysql_port,
        user=args.mysql_user,
        password=args.mysql_password,
        database=args.mysql_database,
        charset="utf8mb4",
    )

    print("Connecting to PostgreSQL...")
    pg_conn = psycopg2.connect(
        host=args.pg_host,
        port=args.pg_port,
        user=args.pg_user,
        password=args.pg_password,
        dbname=args.pg_database,
    )

    total = 0
    for table in TABLES:
        print(f"  {table}...", end=" ", flush=True)
        cols, rows = fetch_mysql_rows(mysql_conn, table)
        if not args.skip_truncate:
            truncate_pg(pg_conn, table)
        n = insert_pg(pg_conn, table, cols, rows)
        print(n)
        total += n

    print("Resetting sequences...")
    reset_sequences(pg_conn)

    mysql_conn.close()
    pg_conn.close()
    print(f"Done. Migrated {total} rows.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
