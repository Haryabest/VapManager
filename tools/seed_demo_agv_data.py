#!/usr/bin/env python3
"""
Generate demo AGV models and AGVs for VapManager (PostgreSQL).
Default: 5000 models + 5000 AGV (1:1), tasks, history and errors for calendar UI.

Requires: pip install psycopg2-binary

Examples:
  python tools/seed_demo_agv_data.py --clear-demo
  python tools/seed_demo_agv_data.py --model-count 5000 --agv-count 5000 --clear-demo
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import date, datetime, time as dt_time, timedelta
from typing import Any, Iterator

MODEL_PREFIX = "DEMO-MODEL-"
AGV_PREFIX = "DEMO-AGV-"

STATUSES = ("online", "offline", "working")
CATEGORIES = ("Вилочный", "Тягач", "Паллетный", "Универсальный", "Компактный")
ERROR_TYPES = ("Электрика", "Механика", "Навигация", "Связь", "Датчики", "ПО", "Батарея")
TASK_NAMES = (
    "Ежедневный осмотр",
    "Смазка ходовой части",
    "Проверка датчиков",
    "Калибровка навигации",
    "Замена фильтров",
    "Диагностика батареи",
    "Проверка тормозов",
    "ТО редуктора",
)
PERFORMERS = ("admin", "tech1", "tech2", "operator1", "operator2")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Seed demo AGV data for VapManager PostgreSQL")
    p.add_argument("--pg-host", default="127.0.0.1")
    p.add_argument("--pg-port", type=int, default=5432)
    p.add_argument("--pg-user", default="vapmanager")
    p.add_argument("--pg-password", default="vapmanager2026")
    p.add_argument("--pg-database", default="agv_manager_db")
    p.add_argument("--model-count", type=int, default=5000, help="AGV models (default 5000)")
    p.add_argument("--agv-count", type=int, default=5000, help="AGVs (default 5000)")
    p.add_argument("--clear-demo", action="store_true", help="Remove previous DEMO-* rows before insert")
    p.add_argument("--seed", type=int, default=42, help="RNG seed for reproducibility")
    return p.parse_args()


def connect(args: argparse.Namespace):
    try:
        import psycopg2
        from psycopg2.extras import execute_batch
    except ImportError as e:
        print("Install: pip install psycopg2-binary", file=sys.stderr)
        raise SystemExit(1) from e
    conn = psycopg2.connect(
        host=args.pg_host,
        port=args.pg_port,
        user=args.pg_user,
        password=args.pg_password,
        dbname=args.pg_database,
    )
    conn.autocommit = False
    return conn, execute_batch


def clear_demo(conn) -> None:
    cur = conn.cursor()
    patterns = (f"{AGV_PREFIX}%", f"{MODEL_PREFIX}%")
    for table, col in (
        ("agv_error_logs", "agv_id"),
        ("agv_task_history", "agv_id"),
        ("maintenance_notification_sent", "agv_id"),
        ("agv_tasks", "agv_id"),
        ("agv_list", "agv_id"),
    ):
        cur.execute(f"DELETE FROM {table} WHERE {col} LIKE %s", (patterns[0],))
    cur.execute("DELETE FROM model_maintenance_template WHERE model_name LIKE %s", (patterns[1],))
    cur.execute("DELETE FROM agv_models WHERE name LIKE %s", (patterns[1],))
    conn.commit()
    cur.close()
    print("Cleared previous DEMO-* data.")


def reset_sequences(conn) -> None:
    tables = (
        "agv_models",
        "model_maintenance_template",
        "agv_list",
        "agv_tasks",
        "agv_task_history",
        "agv_error_logs",
    )
    cur = conn.cursor()
    try:
        for table in tables:
            cur.execute(
                f"SELECT setval(pg_get_serial_sequence('{table}', 'id'), "
                f"COALESCE((SELECT MAX(id) FROM {table}), 1), true)"
            )
        conn.commit()
    except Exception as e:
        conn.rollback()
        print(f"Note: could not reset sequences ({e}).")
    finally:
        cur.close()


def model_name(i: int) -> str:
    return f"{MODEL_PREFIX}{i:05d}"


def agv_id(i: int) -> str:
    return f"{AGV_PREFIX}{i:05d}"


def maintenance_bucket(agv_index: int, today: date, total_agv: int) -> list[date]:
    """Three next_date values per AGV; spread for calendar (overdue / soon / planned)."""
    n = agv_index
    chunk = max(1, total_agv // 10)

    if n < chunk:
        base = today - timedelta(days=20)
    elif n < 2 * chunk:
        base = today - timedelta(days=10)
    elif n < 3 * chunk:
        base = today - timedelta(days=2)
    elif n < 4 * chunk:
        base = today + timedelta(days=4)
    elif n < 5 * chunk:
        base = today + timedelta(days=6)
    elif n < 7 * chunk:
        # Размазать по ±4 месяцам — видно при листании календаря
        month_shift = (n % 9) - 4
        d = today.replace(day=min(28, 1 + (n % 27)))
        try:
            base = d + timedelta(days=month_shift * 31)
        except OverflowError:
            base = today + timedelta(days=n % 120)
    else:
        base = today + timedelta(days=7 + (n % 90))

    return [
        base,
        base + timedelta(days=1 + (n % 5)),
        base + timedelta(days=3 + (n % 7)),
    ]


def batched(iterable: Iterator[tuple[Any, ...]], size: int) -> Iterator[list[tuple[Any, ...]]]:
    batch: list[tuple[Any, ...]] = []
    for item in iterable:
        batch.append(item)
        if len(batch) >= size:
            yield batch
            batch = []
    if batch:
        yield batch


def main() -> int:
    args = parse_args()
    import random

    rng = random.Random(args.seed)
    today = date.today()
    now = datetime.now()
    t0 = time.time()

    conn, execute_batch = connect(args)

    if args.clear_demo:
        clear_demo(conn)

    model_count = max(1, args.model_count)
    agv_count = max(1, args.agv_count)

    cur = conn.cursor()
    if not args.clear_demo:
        cur.execute("SELECT COUNT(*) FROM agv_models WHERE name LIKE %s", (f"{MODEL_PREFIX}%",))
        if cur.fetchone()[0] > 0:
            print("Found existing DEMO models. Use --clear-demo to replace.", file=sys.stderr)
            return 1

    # --- Models (batch insert) ---
    def gen_models() -> Iterator[tuple[Any, ...]]:
        for mi in range(1, model_count + 1):
            yield (
                model_name(mi),
                f"PO-{mi:05d}.{rng.randint(1, 9)}",
                f"EP-{mi:05d}",
                CATEGORIES[mi % len(CATEGORIES)],
                500 + (mi % 500) * 10,
                8 + (mi % 12),
                f"{120 + (mi % 40)}x{80 + (mi % 30)}x{150 + (mi % 50)}",
                1 + (mi % 4),
                "forward" if mi % 2 else "bidirectional",
                now - timedelta(days=rng.randint(30, 800)),
            )

    def gen_templates() -> Iterator[tuple[Any, ...]]:
        for mi in range(1, model_count + 1):
            name = model_name(mi)
            for ti, task in enumerate(TASK_NAMES[:4]):
                yield (
                    name,
                    task,
                    f"Шаблон ТО «{task}» для {name}",
                    7 + ti * 14 + (mi % 5),
                    30 + ti * 15 + (mi % 20),
                    True,
                )

    model_sql = """
        INSERT INTO agv_models (
            name, version_po, version_eplan, category,
            "capacityKg", "maxSpeed", dimensions, coupling_count, direction, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
        ON CONFLICT (name) DO NOTHING
    """
    for batch in batched(gen_models(), 500):
        execute_batch(cur, model_sql, batch, page_size=500)
    conn.commit()
    print(f"Models: {model_count}")

    tpl_sql = """
        INSERT INTO model_maintenance_template (
            model_name, task_name, task_description, interval_days, duration_minutes, is_default
        ) VALUES (%s,%s,%s,%s,%s,%s)
    """
    tpl_count = 0
    for batch in batched(gen_templates(), 1000):
        execute_batch(cur, tpl_sql, batch, page_size=1000)
        tpl_count += len(batch)
    conn.commit()
    print(f"Templates: {tpl_count}")

    users = list(PERFORMERS)
    cur.execute("SELECT username FROM users WHERE is_active = TRUE LIMIT 20")
    db_users = [r[0] for r in cur.fetchall()]
    if db_users:
        users = db_users

    # --- AGV list ---
    def gen_agv() -> Iterator[tuple[Any, ...]]:
        for ai in range(1, agv_count + 1):
            mi = min(ai, model_count)
            status = STATUSES[ai % len(STATUSES)]
            assigned = users[ai % len(users)] if ai % 3 != 0 else ""
            assigned_by = users[(ai + 1) % len(users)] if assigned else ""
            yield (
                agv_id(ai),
                model_name(mi),
                f"SN-{ai:07d}-{rng.randint(1000, 9999)}",
                status,
                f"Робот #{ai} ({CATEGORIES[mi % len(CATEGORIES)]})",
                1000 * ai + (ai % 997),
                f"/blueprints/{model_name(mi)}.pdf",
                today - timedelta(days=(ai % 365)),
                assigned,
                assigned_by,
                now - timedelta(days=rng.randint(1, 500)),
            )

    agv_sql = """
        INSERT INTO agv_list (
            agv_id, model, serial, status, alias, kilometers,
            "blueprintPath", "lastActive", assigned_user, assigned_by, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
        ON CONFLICT (agv_id) DO NOTHING
    """
    for batch in batched(gen_agv(), 1000):
        execute_batch(cur, agv_sql, batch, page_size=1000)
    conn.commit()
    print(f"AGV list: {agv_count}")

    # --- Tasks ---
    def gen_tasks() -> Iterator[tuple[Any, ...]]:
        for ai in range(1, agv_count + 1):
            aid = agv_id(ai)
            dates = maintenance_bucket(ai - 1, today, agv_count)
            for ti, task in enumerate(TASK_NAMES[:3]):
                yield (
                    aid,
                    task,
                    f"Плановое ТО: {task} ({aid})",
                    14 + (ai % 21) + ti * 7,
                    45 + (ai % 30) + ti * 5,
                    ti == 0,
                    dates[ti],
                    users[ai % len(users)] if ai % 4 == 0 else "",
                    users[(ai + 2) % len(users)] if ai % 5 == 0 else "",
                )

    task_sql = """
        INSERT INTO agv_tasks (
            agv_id, task_name, task_description, interval_days, duration_minutes,
            is_default, next_date, assigned_to, delegated_by
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)
    """
    task_total = 0
    for batch in batched(gen_tasks(), 2000):
        execute_batch(cur, task_sql, batch, page_size=2000)
        task_total += len(batch)
    conn.commit()
    print(f"AGV tasks: {task_total}")

    cur.execute(
        "SELECT id, agv_id, task_name FROM agv_tasks WHERE agv_id LIKE %s",
        (f"{AGV_PREFIX}%",),
    )
    task_id_map = {(r[1], r[2]): r[0] for r in cur.fetchall()}
    print(f"Task id map: {len(task_id_map)} rows")

    # --- History (2–4 completions per task; dates in last 120 days for calendar) ---
    def gen_history() -> Iterator[tuple[Any, ...]]:
        for ai in range(1, agv_count + 1):
            aid = agv_id(ai)
            interval = 14 + (ai % 21)
            completions = 2 + (ai % 3)
            for task in TASK_NAMES[:3]:
                tid = task_id_map.get((aid, task))
                for hi in range(completions):
                    days_ago = 5 + (ai % 115) + hi * (interval // 2)
                    completed = today - timedelta(days=days_ago)
                    completed_ts = datetime.combine(
                        completed,
                        dt_time(hour=8 + (hi % 10), minute=(ai + hi) % 60),
                    )
                    yield (
                        aid,
                        tid,
                        task,
                        interval,
                        completed,
                        completed_ts,
                        completed + timedelta(days=interval),
                        users[(ai + hi) % len(users)],
                    )

    hist_sql = """
        INSERT INTO agv_task_history (
            agv_id, task_id, task_name, interval_days, completed_at,
            completed_ts, next_date_after, performed_by
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)
    """
    hist_total = 0
    for batch in batched(gen_history(), 3000):
        execute_batch(cur, hist_sql, batch, page_size=3000)
        hist_total += len(batch)
    conn.commit()
    print(f"Task history: {hist_total}")

    # --- Errors (2–4 per AGV, last 180 days) ---
    def gen_errors() -> Iterator[tuple[Any, ...]]:
        for ai in range(1, agv_count + 1):
            aid = agv_id(ai)
            for ei in range(2 + (ai % 3)):
                err_date = today - timedelta(days=3 + (ai % 175) + ei * 2)
                t_from = dt_time(hour=7 + ei, minute=(ai % 60))
                t_to = dt_time(hour=9 + ei, minute=((ai + 15) % 60))
                mins = max(0, (t_to.hour * 60 + t_to.minute) - (t_from.hour * 60 + t_from.minute))
                yield (
                    aid,
                    err_date,
                    ERROR_TYPES[(ai + ei) % len(ERROR_TYPES)],
                    f"{ERROR_TYPES[(ai + ei) % len(ERROR_TYPES)]}: #{ai}-{ei + 1}",
                    t_from,
                    t_to,
                    mins,
                    users[(ai + ei) % len(users)],
                    datetime.combine(err_date, t_from),
                )

    err_sql = """
        INSERT INTO agv_error_logs (
            agv_id, error_date, error_type, title, time_from, time_to,
            duration_minutes, created_by, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)
    """
    err_total = 0
    for batch in batched(gen_errors(), 3000):
        execute_batch(cur, err_sql, batch, page_size=3000)
        err_total += len(batch)
    conn.commit()
    print(f"Error logs: {err_total}")

    reset_sequences(conn)
    cur.close()
    conn.close()

    elapsed = time.time() - t0
    chunk = max(1, agv_count // 10)
    print()
    print(f"Done in {elapsed:.1f}s.")
    print(f"  Models: {model_name(1)} … {model_name(model_count)} ({model_count})")
    print(f"  AGV:    {agv_id(1)} … {agv_id(agv_count)} ({agv_count})")
    print()
    print("Calendar / ТО groups (~10% fleet each):")
    print(f"  {agv_id(1)}–{agv_id(chunk)}: просрочено ~20 дней")
    print(f"  {agv_id(chunk + 1)}–{agv_id(2 * chunk)}: просрочено ~10 дней")
    print(f"  {agv_id(2 * chunk + 1)}–{agv_id(3 * chunk)}: просрочено ~2 дня")
    print(f"  {agv_id(3 * chunk + 1)}–{agv_id(4 * chunk)}: скоро (+4 дня)")
    print(f"  {agv_id(4 * chunk + 1)}–{agv_id(5 * chunk)}: скоро (+6 дней)")
    print(f"  {agv_id(5 * chunk + 1)}+: разные месяцы и «в срок»")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
