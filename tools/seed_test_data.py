#!/usr/bin/env python3
"""
Тестовое наполнение PostgreSQL для VapManager:
  - 100 моделей AGV
  - 100 AGV (1:1 с моделями)
  - у каждого AGV свои даты ТО (размазаны по календарю)
  - 10 пользователей с полностью заполненным профилем

Requires: pip install psycopg2-binary

Examples:
  python tools/seed_test_data.py --clear-test
  python tools/seed_test_data.py --pg-host 127.0.0.1 --pg-password 51525354 --clear-test
"""

from __future__ import annotations

import argparse
import hashlib
import io
import struct
import sys
import time
import uuid
import zlib
from datetime import date, datetime, time as dt_time, timedelta
from typing import Any, Iterator

MODEL_PREFIX = "TEST-MODEL-"
AGV_PREFIX = "TEST-AGV-"
USER_PREFIX = "test_"
DEFAULT_PASSWORD = "Test12345!"
APP_SALT = "CHANGE_THIS_SALT_123456789"

CATEGORIES = ("Вилочный", "Тягач", "Паллетный", "Универсальный", "Компактный")
TASK_NAMES = (
    "Ежедневный осмотр",
    "Смазка ходовой части",
    "Проверка датчиков",
    "Калибровка навигации",
)

TEST_USERS = (
    {
        "username": "test_admin",
        "role": "admin",
        "full_name": "Иванов Алексей Петрович",
        "employee_id": "EMP-1001",
        "position": "Главный администратор системы",
        "department": "IT / Эксплуатация AGV",
        "mobile": "+7 (900) 111-22-33",
        "ext_phone": "101",
        "email": "test.admin@vapmanager.local",
        "telegram": "@test_admin_agv",
        "avatar_color": (37, 99, 235),
    },
    {
        "username": "test_tech",
        "role": "tech",
        "full_name": "Смирнова Елена Сергеевна",
        "employee_id": "EMP-1002",
        "position": "Разработчик / инженер AGV",
        "department": "Технический отдел",
        "mobile": "+7 (900) 222-33-44",
        "ext_phone": "102",
        "email": "test.tech@vapmanager.local",
        "telegram": "@test_tech_agv",
        "avatar_color": (124, 58, 237),
    },
    {
        "username": "test_operator1",
        "role": "viewer",
        "full_name": "Кузнецов Дмитрий Андреевич",
        "employee_id": "EMP-1003",
        "position": "Оператор смены A",
        "department": "Склад №1",
        "mobile": "+7 (900) 333-44-55",
        "ext_phone": "201",
        "email": "operator1@vapmanager.local",
        "telegram": "@test_op1",
        "avatar_color": (5, 150, 105),
    },
    {
        "username": "test_operator2",
        "role": "viewer",
        "full_name": "Петрова Мария Игоревна",
        "employee_id": "EMP-1004",
        "position": "Оператор смены B",
        "department": "Склад №1",
        "mobile": "+7 (900) 444-55-66",
        "ext_phone": "202",
        "email": "operator2@vapmanager.local",
        "telegram": "@test_op2",
        "avatar_color": (234, 88, 12),
    },
    {
        "username": "test_master1",
        "role": "viewer",
        "full_name": "Волков Сергей Николаевич",
        "employee_id": "EMP-1005",
        "position": "Мастер участка ТО",
        "department": "Сервис AGV",
        "mobile": "+7 (900) 555-66-77",
        "ext_phone": "301",
        "email": "master1@vapmanager.local",
        "telegram": "@test_master1",
        "avatar_color": (220, 38, 38),
    },
    {
        "username": "test_master2",
        "role": "viewer",
        "full_name": "Орлова Анна Владимировна",
        "employee_id": "EMP-1006",
        "position": "Инженер по обслуживанию",
        "department": "Сервис AGV",
        "mobile": "+7 (900) 666-77-88",
        "ext_phone": "302",
        "email": "master2@vapmanager.local",
        "telegram": "@test_master2",
        "avatar_color": (14, 165, 233),
    },
    {
        "username": "test_logist1",
        "role": "viewer",
        "full_name": "Морозов Павел Олегович",
        "employee_id": "EMP-1007",
        "position": "Логист",
        "department": "Логистика",
        "mobile": "+7 (900) 777-88-99",
        "ext_phone": "401",
        "email": "logist1@vapmanager.local",
        "telegram": "@test_logist1",
        "avatar_color": (168, 85, 247),
    },
    {
        "username": "test_logist2",
        "role": "viewer",
        "full_name": "Соколова Юлия Александровна",
        "employee_id": "EMP-1008",
        "position": "Специалист по маршрутам",
        "department": "Логистика",
        "mobile": "+7 (900) 888-99-00",
        "ext_phone": "402",
        "email": "logist2@vapmanager.local",
        "telegram": "@test_logist2",
        "avatar_color": (236, 72, 153),
    },
    {
        "username": "test_qc1",
        "role": "viewer",
        "full_name": "Новиков Артём Денисович",
        "employee_id": "EMP-1009",
        "position": "Инспектор качества",
        "department": "Контроль качества",
        "mobile": "+7 (900) 999-00-11",
        "ext_phone": "501",
        "email": "qc1@vapmanager.local",
        "telegram": "@test_qc1",
        "avatar_color": (202, 138, 4),
    },
    {
        "username": "test_qc2",
        "role": "viewer",
        "full_name": "Лебедева Ольга Константиновна",
        "employee_id": "EMP-1010",
        "position": "Аналитик эксплуатации",
        "department": "Контроль качества",
        "mobile": "+7 (900) 100-20-30",
        "ext_phone": "502",
        "email": "qc2@vapmanager.local",
        "telegram": "@test_qc2",
        "avatar_color": (71, 85, 105),
    },
)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Seed test data for VapManager PostgreSQL")
    p.add_argument("--pg-host", default="127.0.0.1")
    p.add_argument("--pg-port", type=int, default=5432)
    p.add_argument("--pg-user", default="vapmanager")
    p.add_argument("--pg-password", default="51525354")
    p.add_argument("--pg-database", default="agv_manager_db")
    p.add_argument("--model-count", type=int, default=100)
    p.add_argument("--agv-count", type=int, default=100)
    p.add_argument("--clear-test", action="store_true", help="Remove previous TEST-* / test_* rows")
    return p.parse_args()


def hash_password(password: str) -> str:
    data = (APP_SALT + password).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def recovery_key() -> str:
    u = uuid.uuid4().hex.upper()
    return f"RK-{u[0:4]}-{u[4:8]}-{u[8:12]}"


def invite_key() -> str:
    return uuid.uuid4().hex[:8].upper()


def solid_png(rgb: tuple[int, int, int], size: int = 64) -> bytes:
    r, g, b = rgb
    raw = b"".join(b"\x00" + bytes([r, g, b]) * size for _ in range(size))

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            chunk(b"IHDR", ihdr),
            chunk(b"IDAT", zlib.compress(raw, 9)),
            chunk(b"IEND", b""),
        ]
    )


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


def model_name(i: int) -> str:
    return f"{MODEL_PREFIX}{i:03d}"


def agv_id(i: int) -> str:
    return f"{AGV_PREFIX}{i:03d}"


def agv_status(_agv_index: int) -> str:
    """«Предстоящее ТО» показывает только online/working."""
    return "online"


def primary_to_date(agv_index: int, today: date) -> date:
    """
    Главная задача (первая) — строго в окне виджета «Предстоящее ТО»:
      UI: next_date <= today+6 и daysLeft < 7 (просрочка или 0–6 дней).
    """
    if agv_index < 50:
        return today - timedelta(days=1 + (agv_index % 25))
    return today + timedelta(days=agv_index % 7)


def secondary_task_dates(agv_index: int, today: date) -> tuple[date, date]:
    """Второстепенные задачи — размазаны по текущему месяцу для календаря."""
    month_start = today.replace(day=1)
    return (
        month_start + timedelta(days=agv_index % 28),
        month_start + timedelta(days=(agv_index * 2 + 5) % 28),
    )


def batched(iterable: Iterator[tuple[Any, ...]], size: int) -> Iterator[list[tuple[Any, ...]]]:
    batch: list[tuple[Any, ...]] = []
    for item in iterable:
        batch.append(item)
        if len(batch) >= size:
            yield batch
            batch = []
    if batch:
        yield batch


def clear_test_data(conn) -> None:
    cur = conn.cursor()
    user_like = f"{USER_PREFIX}%"
    agv_like = f"{AGV_PREFIX}%"
    model_like = f"{MODEL_PREFIX}%"

    for table, col in (
        ("notifications", "target_user"),
        ("task_chat_messages", "from_user"),
        ("task_chat_threads", "created_by"),
        ("task_chat_threads", "recipient_user"),
        ("agv_error_logs", "created_by"),
        ("agv_task_history", "performed_by"),
    ):
        cur.execute(f"DELETE FROM {table} WHERE {col} LIKE %s", (user_like,))

    for table, col in (
        ("agv_error_logs", "agv_id"),
        ("agv_task_history", "agv_id"),
        ("maintenance_notification_sent", "agv_id"),
        ("agv_tasks", "agv_id"),
        ("agv_list", "agv_id"),
    ):
        cur.execute(f"DELETE FROM {table} WHERE {col} LIKE %s", (agv_like,))

    cur.execute("DELETE FROM model_maintenance_template WHERE model_name LIKE %s", (model_like,))
    cur.execute("DELETE FROM agv_models WHERE name LIKE %s", (model_like,))
    cur.execute("DELETE FROM users WHERE username LIKE %s", (user_like,))
    conn.commit()
    cur.close()
    print("Cleared previous TEST-* / test_* data.")


def seed_users(conn, execute_batch, now: datetime) -> list[str]:
    cur = conn.cursor()
    usernames: list[str] = []
    rows = []
    for u in TEST_USERS:
        username = u["username"]
        usernames.append(username)
        invite_admin = invite_key() if u["role"] == "admin" else None
        invite_tech = invite_key() if u["role"] == "tech" else None
        invite_expire = now + timedelta(days=30) if invite_admin or invite_tech else None
        rows.append(
            (
                username,
                hash_password(DEFAULT_PASSWORD),
                u["role"],
                True,
                now - timedelta(days=1 + len(usernames)),
                recovery_key(),
                invite_admin,
                invite_expire if invite_admin else None,
                invite_tech,
                invite_expire if invite_tech else None,
                u["full_name"],
                u["employee_id"],
                u["position"],
                u["department"],
                u["mobile"],
                u["ext_phone"],
                u["email"],
                u["telegram"],
                solid_png(u["avatar_color"]),
                now - timedelta(days=30 + len(usernames)),
            )
        )

    sql = """
        INSERT INTO users (
            username, password_hash, role, is_active, last_login,
            permanent_recovery_key, admin_invite_key, admin_invite_key_expire,
            tech_invite_key, tech_invite_key_expire,
            full_name, employee_id, position, department,
            mobile, ext_phone, email, telegram, avatar, created_at
        ) VALUES (
            %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s
        )
        ON CONFLICT (username) DO UPDATE SET
            password_hash = EXCLUDED.password_hash,
            role = EXCLUDED.role,
            is_active = EXCLUDED.is_active,
            last_login = EXCLUDED.last_login,
            permanent_recovery_key = EXCLUDED.permanent_recovery_key,
            admin_invite_key = EXCLUDED.admin_invite_key,
            admin_invite_key_expire = EXCLUDED.admin_invite_key_expire,
            tech_invite_key = EXCLUDED.tech_invite_key,
            tech_invite_key_expire = EXCLUDED.tech_invite_key_expire,
            full_name = EXCLUDED.full_name,
            employee_id = EXCLUDED.employee_id,
            position = EXCLUDED.position,
            department = EXCLUDED.department,
            mobile = EXCLUDED.mobile,
            ext_phone = EXCLUDED.ext_phone,
            email = EXCLUDED.email,
            telegram = EXCLUDED.telegram,
            avatar = EXCLUDED.avatar
    """
    execute_batch(cur, sql, rows, page_size=20)
    conn.commit()
    cur.close()
    print(f"Users: {len(rows)}")
    return usernames


def seed_notifications(conn, execute_batch, usernames: list[str], now: datetime) -> None:
    cur = conn.cursor()
    rows = []
    for i, user in enumerate(usernames):
        rows.append(
            (
                user,
                "Тестовое уведомление",
                f"Привет, {user}! Это тестовое уведомление #{i + 1} после seed_test_data.",
                i % 3 == 0,
                now - timedelta(hours=i + 1),
            )
        )
    execute_batch(
        cur,
        """
        INSERT INTO notifications (target_user, title, message, is_read, created_at)
        VALUES (%s,%s,%s,%s,%s)
        """,
        rows,
        page_size=20,
    )
    conn.commit()
    cur.close()
    print(f"Notifications: {len(rows)}")


def main() -> int:
    args = parse_args()
    today = date.today()
    now = datetime.now()
    t0 = time.time()

    conn, execute_batch = connect(args)
    if args.clear_test:
        clear_test_data(conn)

    model_count = max(1, min(args.model_count, 999))
    agv_count = max(1, min(args.agv_count, 999))

    usernames = seed_users(conn, execute_batch, now)
    seed_notifications(conn, execute_batch, usernames, now)

    cur = conn.cursor()

    def gen_models() -> Iterator[tuple[Any, ...]]:
        for mi in range(1, model_count + 1):
            yield (
                model_name(mi),
                f"PO-{mi:03d}.1",
                f"EP-{mi:03d}",
                CATEGORIES[mi % len(CATEGORIES)],
                800 + (mi % 120) * 10,
                6 + (mi % 8),
                f"{120 + (mi % 20)}x{80 + (mi % 15)}x{160 + (mi % 25)}",
                1 + (mi % 3),
                "forward" if mi % 2 else "bidirectional",
                now - timedelta(days=10 + mi),
            )

    model_sql = """
        INSERT INTO agv_models (
            name, version_po, version_eplan, category,
            "capacityKg", "maxSpeed", dimensions, coupling_count, direction, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
        ON CONFLICT (name) DO NOTHING
    """
    for batch in batched(gen_models(), 100):
        execute_batch(cur, model_sql, batch, page_size=100)
    conn.commit()
    print(f"Models: {model_count}")

    tpl_sql = """
        INSERT INTO model_maintenance_template (
            model_name, task_name, task_description, interval_days, duration_minutes, is_default
        ) VALUES (%s,%s,%s,%s,%s,%s)
    """
    tpl_rows = []
    for mi in range(1, model_count + 1):
        name = model_name(mi)
        for ti, task in enumerate(TASK_NAMES):
            tpl_rows.append(
                (
                    name,
                    task,
                    f"Шаблон «{task}» для {name}",
                    7 + ti * 14,
                    30 + ti * 10,
                    True,
                )
            )
    execute_batch(cur, tpl_sql, tpl_rows, page_size=500)
    conn.commit()
    print(f"Templates: {len(tpl_rows)}")

    def gen_agv() -> Iterator[tuple[Any, ...]]:
        for ai in range(1, agv_count + 1):
            yield (
                agv_id(ai),
                model_name(ai),
                f"SN-T{ai:04d}",
                agv_status(ai - 1),
                f"Тестовый AGV #{ai}",
                500 + ai * 17,
                f"/blueprints/{model_name(ai)}.pdf",
                today - timedelta(days=ai % 60),
                "",
                "",
                now - timedelta(days=ai % 120),
            )

    agv_sql = """
        INSERT INTO agv_list (
            agv_id, model, serial, status, alias, kilometers,
            "blueprintPath", "lastActive", assigned_user, assigned_by, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
        ON CONFLICT (agv_id) DO NOTHING
    """
    for batch in batched(gen_agv(), 100):
        execute_batch(cur, agv_sql, batch, page_size=100)
    conn.commit()
    print(f"AGV list: {agv_count}")

    def gen_tasks() -> Iterator[tuple[Any, ...]]:
        sec_dates = secondary_task_dates
        for ai in range(1, agv_count + 1):
            aid = agv_id(ai)
            idx = ai - 1
            s1, s2 = sec_dates(idx, today)
            extra_dates = (primary_to_date(idx, today), s1, s2)
            for ti, task in enumerate(TASK_NAMES[:3]):
                yield (
                    aid,
                    task,
                    f"Плановое ТО: {task} ({aid})",
                    14 + (ai % 14) + ti * 7,
                    40 + (ai % 20),
                    ti == 0,
                    extra_dates[ti],
                    "",
                    "",
                )

    task_sql = """
        INSERT INTO agv_tasks (
            agv_id, task_name, task_description, interval_days, duration_minutes,
            is_default, next_date, assigned_to, delegated_by
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)
    """
    task_rows = list(gen_tasks())
    execute_batch(cur, task_sql, task_rows, page_size=500)
    conn.commit()
    print(f"AGV tasks: {len(task_rows)}")

    cur.execute(
        "SELECT id, agv_id, task_name FROM agv_tasks WHERE agv_id LIKE %s",
        (f"{AGV_PREFIX}%",),
    )
    task_id_map = {(r[1], r[2]): r[0] for r in cur.fetchall()}

    def gen_history() -> Iterator[tuple[Any, ...]]:
        """История только для части AGV и никогда «сегодня» — иначе UI скрывает задачу из предстоящего ТО."""
        for ai in range(1, agv_count + 1):
            if ai % 2 != 0:
                continue
            aid = agv_id(ai)
            interval = 14 + (ai % 14)
            tid = task_id_map.get((aid, TASK_NAMES[2]))
            if not tid:
                continue
            completed = today - timedelta(days=14 + (ai % 50))
            completed_ts = datetime.combine(completed, dt_time(hour=9 + (ai % 8), minute=ai % 60))
            yield (
                aid,
                tid,
                TASK_NAMES[2],
                interval,
                completed,
                completed_ts,
                completed + timedelta(days=interval),
                usernames[(ai + 3) % len(usernames)],
            )

    hist_rows = list(gen_history())
    execute_batch(
        cur,
        """
        INSERT INTO agv_task_history (
            agv_id, task_id, task_name, interval_days, completed_at,
            completed_ts, next_date_after, performed_by
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)
        """,
        hist_rows,
        page_size=500,
    )
    conn.commit()
    print(f"Task history: {len(hist_rows)}")

    err_rows = []
    for ai in range(1, agv_count + 1):
        aid = agv_id(ai)
        err_date = today - timedelta(days=3 + (ai % 20))
        err_rows.append(
            (
                aid,
                err_date,
                "Диагностика",
                f"Тестовая ошибка #{ai}",
                dt_time(hour=10, minute=0),
                dt_time(hour=10, minute=45),
                45,
                usernames[ai % len(usernames)],
                datetime.combine(err_date, dt_time(hour=10, minute=0)),
            )
        )
    execute_batch(
        cur,
        """
        INSERT INTO agv_error_logs (
            agv_id, error_date, error_type, title, time_from, time_to,
            duration_minutes, created_by, created_at
        ) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)
        """,
        err_rows,
        page_size=200,
    )
    conn.commit()
    print(f"Error logs: {len(err_rows)}")

    cur.close()
    conn.close()

    elapsed = time.time() - t0
    print()
    print(f"Done in {elapsed:.1f}s.")
    print(f"  Models: {model_name(1)} … {model_name(model_count)} ({model_count})")
    print(f"  AGV:    {agv_id(1)} … {agv_id(agv_count)} ({agv_count})")
    print(f"  Users:  {len(usernames)} (prefix {USER_PREFIX}*)")
    print()
    print("Test logins (password for all):")
    print(f"  Password: {DEFAULT_PASSWORD}")
    for u in TEST_USERS:
        print(f"  - {u['username']} ({u['role']}) — {u['full_name']}")
    print()
    print("TO для «Предстоящее ТО» (100 AGV, все общие, status=online):")
    print("  50 AGV — просрочено (главная задача в прошлом)")
    print("  50 AGV — скоро (главная задача: сегодня … +6 дней)")
    print("  + 2 доп. задачи на AGV — по текущему месяцу для календаря")
    print("  assigned_user / assigned_to пустые — видны любому пользователю")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
