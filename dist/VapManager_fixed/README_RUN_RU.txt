Запуск VapManager

1. Запустите VapManager.exe.
2. Настройка сервера БД: файл config.ini, параметр db_host=IP_ИЛИ_HOST[:PORT].
3. SQL для создания/обновления БД: install_agv_manager_db.sql.

MySQL-драйвер уже вложен и проверен:
- sqldrivers\qsqlmysql.dll
- libmysql.dll рядом с VapManager.exe
- MSVCR120.dll и MSVCP120.dll рядом с VapManager.exe
- VCRUNTIME140.dll рядом с VapManager.exe

Проверка: qsqlmysql.dll загружается, ошибка Driver not loaded устранена.
