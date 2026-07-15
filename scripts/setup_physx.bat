@echo off
setlocal
cd /d "%~dp0.."

set PHYSX_COMMIT=6b3a94534016270f8f6ebc665cd97ba7ce858d84

if exist 3rd-party\physx\physx\include\PxPhysicsAPI.h goto have_sdk
echo === Fetching PhysX 4.1 SDK ===
if not exist 3rd-party\physx-rs git clone https://github.com/EmbarkStudios/physx-rs 3rd-party\physx-rs
if errorlevel 1 exit /b 1
git -C 3rd-party\physx-rs fetch origin %PHYSX_COMMIT%
if errorlevel 1 exit /b 1
git -C 3rd-party\physx-rs -c advice.detachedHead=false checkout %PHYSX_COMMIT%
if errorlevel 1 exit /b 1
if not exist 3rd-party\physx-rs\physx-sys\PhysX\physx\include\PxPhysicsAPI.h git -C 3rd-party\physx-rs submodule update --init physx-sys/PhysX
if not exist 3rd-party\physx-rs\physx-sys\PhysX\physx\include\PxPhysicsAPI.h exit /b 1
echo Copying SDK to 3rd-party\physx ...
xcopy /E /I /Q /Y 3rd-party\physx-rs\physx-sys\PhysX 3rd-party\physx >nul
if errorlevel 1 exit /b 1
:have_sdk

set ARCH=%1
if "%ARCH%"=="" set ARCH=x64
if /i "%ARCH%"=="win32" set ARCH=x86
set SUFFIX=64
if /i "%ARCH%"=="x86" set SUFFIX=32

set CONFIG=%2
if "%CONFIG%"=="" set CONFIG=Release
set RSP=scripts\physx_msvc.rsp
if /i "%CONFIG%"=="Debug" set SUFFIX=%SUFFIX%d
if /i "%CONFIG%"=="Debug" set RSP=scripts\physx_msvc_debug.rsp

if not exist 3rd-party\physx\lib\physx_static_%SUFFIX%.lib goto build_lib
echo PhysX library physx_static_%SUFFIX%.lib is present.
exit /b 0

:build_lib
where cl >nul 2>nul
if not errorlevel 1 goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" echo vswhere.exe not found, run this from a VS developer command prompt & exit /b 1
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvarsall.bat" %ARCH%
where cl >nul 2>nul
if errorlevel 1 echo failed to set up the MSVC environment & exit /b 1
:have_cl

echo === Building PhysX static library for %ARCH% %CONFIG% ===
if not exist 3rd-party\physx\lib mkdir 3rd-party\physx\lib
if exist 3rd-party\physx\obj rmdir /s /q 3rd-party\physx\obj
mkdir 3rd-party\physx\obj
cl @%RSP% @scripts\physx_sources_msvc.rsp /Fo3rd-party\physx\obj\ 
if errorlevel 1 exit /b 1
lib /nologo /OUT:3rd-party\physx\lib\physx_static_%SUFFIX%.lib 3rd-party\physx\obj\*.obj
if errorlevel 1 exit /b 1
echo Done: 3rd-party\physx\lib\physx_static_%SUFFIX%.lib
exit /b 0
