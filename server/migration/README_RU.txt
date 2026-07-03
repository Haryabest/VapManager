Перенос базы VapManager на новый сервер
======================================

Файл дампа: agv_manager_full.dump (не в git — положите в эту папку вручную)

На СТАРОМ сервере (один раз):
  1. server\migration\1_snyat_dump.bat  (от администратора)
  2. Скопируйте папку server\migration\ с agv_manager_full.dump на новый ПК

На НОВОМ сервере:
  1. setup_server.bat  (от администратора)
  2. server\migration\2_vosstanovit_bazu.bat  (от администратора)
  3. config.ini:
       — на самом сервере: db_host=127.0.0.1  (config.ini.server-local.example)
       — на клиентах: db_host=85.249.17.47 или 192.168.x.x  (config.ini.client.example)

Восстановление вручную:

  set PGPASSWORD=<пароль postgres>
  pg_restore -U postgres -h localhost -d agv_manager_db ^
    --clean --if-exists --no-owner --role=vapmanager -v agv_manager_full.dump

Проверка:

  set PGPASSWORD=51525354
  psql -U vapmanager -h localhost -d agv_manager_db -c "SELECT COUNT(*) FROM users;"

Подробнее: DEPLOY_SERVER_RU.txt, раздел 12.
