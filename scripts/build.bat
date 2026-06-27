@echo off
:: Build the Rutile C/C++ runtime.
::
:: Most of the project is orchestrated from batch/shell scripts; CMake is
:: invoked only for the C/C++ pieces under bindings\c and runtime\. Future
:: language bindings under bindings\{zig,rust,...} are built with their own
:: toolchains and should be added to this script as separate phases.

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
