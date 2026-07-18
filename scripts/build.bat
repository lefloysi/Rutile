@echo off
setlocal

set "repo_root=%~dp0.."
set "config=%~1"
set "build_dir=%~2"
if not defined config set "config=Debug"
if not defined build_dir set "build_dir=%repo_root%\out\build"

set "vcpkg_root=%VCPKG_ROOT%"
if not defined vcpkg_root if exist "%LOCALAPPDATA%\vcpkg\vcpkg.path.txt" (
	set /p "vcpkg_root="<"%LOCALAPPDATA%\vcpkg\vcpkg.path.txt"
)
if defined vcpkg_root if not exist "%vcpkg_root%\scripts\buildsystems\vcpkg.cmake" set "vcpkg_root="
if not defined vcpkg_root for /f "delims=" %%I in ('where vcpkg.exe 2^>nul') do if not defined vcpkg_root set "vcpkg_root=%%~dpI"

set "vcpkg_toolchain=%vcpkg_root%\scripts\buildsystems\vcpkg.cmake"
if not exist "%vcpkg_toolchain%" (
	echo Could not find vcpkg. Set VCPKG_ROOT or run "vcpkg integrate install". 1>&2
	exit /b 1
)

if not defined VCPKG_TARGET_TRIPLET set "VCPKG_TARGET_TRIPLET=x64-windows-static"

shift
shift
set "cmake_args="
:collect_cmake_args
if "%~1"=="" goto configure
set "cmake_args=%cmake_args% %1"
shift
goto collect_cmake_args

:configure
set "fresh_arg="
if not exist "%build_dir%\CMakeCache.txt" goto run_cmake_with_toolchain
findstr /b /c:"CMAKE_TOOLCHAIN_FILE:" "%build_dir%\CMakeCache.txt" >nul
if not errorlevel 1 goto run_cmake

echo Reconfiguring the existing build tree with the repository vcpkg manifest.
set "fresh_arg=--fresh"

:run_cmake_with_toolchain
cmake %fresh_arg% -S "%repo_root%" -B "%build_dir%" "-DCMAKE_BUILD_TYPE=%config%" "-DCMAKE_TOOLCHAIN_FILE=%vcpkg_toolchain%" "-DVCPKG_TARGET_TRIPLET=%VCPKG_TARGET_TRIPLET%" %cmake_args%
if errorlevel 1 exit /b 1
goto build

:run_cmake
cmake -S "%repo_root%" -B "%build_dir%" "-DCMAKE_BUILD_TYPE=%config%" "-DVCPKG_TARGET_TRIPLET=%VCPKG_TARGET_TRIPLET%" %cmake_args%
if errorlevel 1 exit /b 1

:build
cmake --build "%build_dir%" --config "%config%" --parallel
if errorlevel 1 exit /b 1

endlocal & exit /b 0
