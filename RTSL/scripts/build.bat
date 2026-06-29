@echo off
:: Build the RTSL compiler library and rtslc CLI.

setlocal

set repo_root=%~dp0..
set build_dir=%repo_root%\out\build

set config=%1
if "%config%"=="" set config=Debug

cmake -S "%repo_root%" -B "%build_dir%" -DCMAKE_BUILD_TYPE=%config%
if errorlevel 1 exit /b %errorlevel%

cmake --build "%build_dir%" --config %config% --parallel
if errorlevel 1 exit /b %errorlevel%

endlocal
