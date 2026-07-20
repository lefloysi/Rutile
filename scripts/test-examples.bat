@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "repo=%%~fI"
set "configuration=%~1"
if not defined configuration set "configuration=Debug"
set "build_dir=%~2"
if not defined build_dir set "build_dir=%repo%\out\build\x64-Debug"

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
	echo Could not find Visual Studio. Install the C++ desktop workload. 1>&2
	exit /b 1
)
for /f "usebackq tokens=*" %%I in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "visual_studio=%%I"
if not defined visual_studio (
	echo Could not find Visual Studio with the C++ desktop workload. 1>&2
	exit /b 1
)
call "%visual_studio%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

set "toolchain="
if defined VCPKG_ROOT set "toolchain=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if not defined toolchain if defined VCPKG_INSTALLATION_ROOT set "toolchain=%VCPKG_INSTALLATION_ROOT%\scripts\buildsystems\vcpkg.cmake"
if not defined toolchain set "toolchain=%visual_studio%\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"
if not exist "%toolchain%" (
	echo Could not find vcpkg. Install the Visual Studio vcpkg component or set VCPKG_ROOT. 1>&2
	exit /b 1
)

set "RUTILE_EXAMPLE_BACKENDS=%~3"
if not defined RUTILE_EXAMPLE_BACKENDS set "RUTILE_EXAMPLE_BACKENDS=rt-vulkan"
set "features=examples;opengl"
set "build_vulkan=OFF"
set "build_directx12=OFF"
for %%B in (%RUTILE_EXAMPLE_BACKENDS%) do (
	if /I "%%B"=="rt-vulkan" (
		set "build_vulkan=ON"
		set "features=!features!;vulkan"
	)
	if /I "%%B"=="rt-directx12" set "build_directx12=ON"
)

echo Configuring examples with vcpkg features: !features!
cmake -S "%repo%" -B "%build_dir%" ^
	-DCMAKE_BUILD_TYPE=%configuration% ^
	-DCMAKE_TOOLCHAIN_FILE="%toolchain%" ^
	-DVCPKG_TARGET_TRIPLET=x64-windows-static ^
	-DVCPKG_MANIFEST_FEATURES="!features!" ^
	-DRUTILE_BUILD_EXAMPLES=ON ^
	-DRUTILE_BUILD_VULKAN=%build_vulkan% ^
	-DRUTILE_BUILD_DIRECTX12=%build_directx12%
if errorlevel 1 exit /b %errorlevel%

echo Building examples...
cmake --build "%build_dir%" --config %configuration% --parallel --target ^
	rutile-01-triangle ^
	rutile-02-spinning-cube ^
	rutile-03-plasma-lab ^
	rutile-04-galaxy ^
	rutile-05-voxel-renderer
if errorlevel 1 exit /b %errorlevel%

set "bin_dir=%build_dir%\bin\%configuration%"
if not exist "%bin_dir%\rutile-01-triangle.exe" set "bin_dir=%build_dir%\bin"
if not exist "%bin_dir%\rutile-01-triangle.exe" (
	echo Could not find the built examples under "%build_dir%". 1>&2
	exit /b 1
)

for %%B in (%RUTILE_EXAMPLE_BACKENDS%) do (
	for %%E in (rutile-01-triangle rutile-02-spinning-cube rutile-03-plasma-lab rutile-04-galaxy rutile-05-voxel-renderer) do (
		call :run_example %%E %%B
		if errorlevel 1 exit /b 1
	)
)

echo Example run completed.
exit /b 0

:run_example
echo Running %~1 with %~2...
"%bin_dir%\%~1.exe" --backend %~2 --frames 30
if errorlevel 1 (
	echo FAILED: %~1 with %~2. 1>&2
	exit /b 1
)
echo PASS: %~1 with %~2
exit /b 0
