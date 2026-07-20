@echo off
setlocal

set "repo_root=%~dp0.."
set "build_dir=%repo_root%\out\tests"
set "config=%~1"
if not defined config set "config=Debug"

set "RUTILE_VCPKG_FEATURES=gl33;tests;vulkan"
call "%repo_root%\scripts\build.bat" "%config%" "%build_dir%" "-DBUILD_TESTING=ON" "-DRUTILE_BUILD_EXAMPLES=OFF" "-DRUTILE_INSTALL=OFF"
if errorlevel 1 exit /b 1

ctest --test-dir "%build_dir%" -C "%config%" --output-on-failure --no-tests=error
if errorlevel 1 exit /b 1

endlocal & exit /b 0
