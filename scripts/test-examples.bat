@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-examples.ps1" %*
exit /b %errorlevel%
