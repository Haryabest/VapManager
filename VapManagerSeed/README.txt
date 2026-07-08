VapManager — заполнение тестовой БД
====================================

Запуск: двойной клик по «Заполнить тестовую БД.bat»

Что создаётся:
  - 100 моделей TEST-MODEL-001 … 100
  - 100 AGV TEST-AGV-001 … 100 (у каждого свои даты ТО)
  - 10 пользователей test_* с полным профилем

Пароль всех test_* пользователей: Test12345!

Логины:
  test_admin, test_tech, test_operator1, test_operator2,
  test_master1, test_master2, test_logist1, test_logist2,
  test_qc1, test_qc2

Подключение к БД берётся из config.ini в этой папке.
Для сервера (этот ПК): db_host=127.0.0.1
Для клиента в сети: db_host=192.168.0.1

Перед первым запуском (один раз):
  pip install psycopg2-binary

Повторный запуск с --clear-test удаляет старые TEST-* / test_* данные.

Важно для «Предстоящее ТО»:
  - Главная задача каждого TEST-AGV: просрочка или 0–6 дней (как в VapManager).
  - Все TEST-AGV: status=online, без assigned_user/assigned_to — видны любому логину.
  - Доп. задачи размазаны по текущему месяцу для календаря.
