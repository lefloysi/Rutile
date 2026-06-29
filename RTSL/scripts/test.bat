@echo off
setlocal

set repo_root=%~dp0..
set build_dir=%repo_root%\out\build

set config=%1
if "%config%"=="" set config=Debug

if not exist "%build_dir%" call "%repo_root%\scripts\build.bat" %config%
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir "%build_dir%" -C %config% --output-on-failure
if errorlevel 1 exit /b %errorlevel%

endlocal
