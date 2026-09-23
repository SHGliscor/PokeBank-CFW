@echo off
setlocal
cd /d "%~dp0"
title Pokebot3DS-CFW v0p43EJ HF102b History Space + Hidden Current Hunt
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 run_qt_live.py
) else (
  python run_qt_live.py
)
if errorlevel 1 pause
