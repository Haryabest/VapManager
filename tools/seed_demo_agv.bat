@echo off
chcp 65001 >nul
cd /d "%~dp0.."
python tools\seed_demo_agv_data.py --clear-demo --model-count 5000 --agv-count 5000 %*
if errorlevel 1 pause
exit /b %errorlevel%
