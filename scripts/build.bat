@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "repo_root=%%~fI"
set "config=%~1"
set "build_dir=%~2"
if not defined config set "config=Debug"
if not defined build_dir set "build_dir=%repo_root%\out\build\x64-%config%"

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%vswhere%" (
	for /f "usebackq tokens=*" %%I in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "visual_studio=%%I"
)
if defined visual_studio if exist "%visual_studio%\VC\Auxiliary\Build\vcvars64.bat" (
	call "%visual_studio%\VC\Auxiliary\Build\vcvars64.bat" >nul
	if errorlevel 1 exit /b %errorlevel%
)

set "vcpkg_root=%VCPKG_ROOT%"
if not defined vcpkg_root if defined VCPKG_INSTALLATION_ROOT set "vcpkg_root=%VCPKG_INSTALLATION_ROOT%"
if not defined vcpkg_root if defined visual_studio set "vcpkg_root=%visual_studio%\VC\vcpkg"
if not defined vcpkg_root for /f "delims=" %%I in ('where vcpkg.exe 2^>nul') do if not defined vcpkg_root set "vcpkg_root=%%~dpI"

set "vcpkg_toolchain=%vcpkg_root%\scripts\buildsystems\vcpkg.cmake"
if not exist "%vcpkg_toolchain%" (
	echo Could not find vcpkg. Install the Visual Studio vcpkg component or set VCPKG_ROOT. 1>&2
	exit /b 1
)

if not defined VCPKG_TARGET_TRIPLET set "VCPKG_TARGET_TRIPLET=x64-windows-static"
if not defined RUTILE_VCPKG_FEATURES set "RUTILE_VCPKG_FEATURES=examples;opengl;tests;vulkan"

shift
shift
set "cmake_args="
:collect_cmake_args
if "%~1"=="" goto configure
set "arg=%~1"
set "next=%~2"
if /I "!arg:~0,2!"=="-D" (
	if defined next (
		if not "!next:~0,1!"=="-" (
			set "arg=!arg!=!next!"
			shift
		)
	)
)
set "cmake_args=!cmake_args! ^"!arg!^""
shift
goto collect_cmake_args

:configure
cmake -S "%repo_root%" -B "%build_dir%" ^
	-DCMAKE_BUILD_TYPE="%config%" ^
	-DCMAKE_TOOLCHAIN_FILE="%vcpkg_toolchain%" ^
	-DVCPKG_TARGET_TRIPLET="%VCPKG_TARGET_TRIPLET%" ^
	-DVCPKG_MANIFEST_FEATURES="%RUTILE_VCPKG_FEATURES%" ^
	!cmake_args!
if errorlevel 1 exit /b %errorlevel%

cmake --build "%build_dir%" --config "%config%" --parallel
if errorlevel 1 exit /b %errorlevel%

endlocal & exit /b 0
