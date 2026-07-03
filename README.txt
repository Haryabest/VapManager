VapManager — исходники и скрипты развёртывания
==============================================

Подробная инструкция: DEPLOY_SERVER_RU.txt

Состав папки:
  src\              — исходный код C++/Qt
  AgvNewUi.pro      — проект Qt Creator
  installer\        — Inno Setup (setup.exe)
  server\           — настройка PostgreSQL и server\migration\ (перенос БД)
  postgresql\       — скрипты удалённого доступа к БД
  updates\          — HTTP-сервер обновлений (bat/ps1)
  tools\            — seed_test_data (тестовая БД)
  docs\             — тексты для установщика (Inno Setup)
  assets\, noback\  — ресурсы (иконки, звуки)
  *.bat             — сборка, публикация апдейтов, setup_server

Готовый установщик (для развёртывания на ПК пользователей):
  dist\setup.exe    — 1.0.2 build 150 (собран build_installer.bat)

Не включено (создаётся при сборке/публикации на сервере):
  dist\VapManager\  — pack_vapmanager.bat (portable-сборка)
  updates\files\    — publish_local_updates.bat
  release\          — Qt Creator Release build

После изменений в коде: pack_vapmanager.bat → build_installer.bat →
скопировать dist\setup.exe сюда и на сервер обновлений.
