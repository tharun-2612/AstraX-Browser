@echo off
setlocal

set "VS_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
set "NINJA_EXE=C:\Users\kumar\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe"
set "QT_PREFIX=C:\Qt\6.8.3\msvc2022_64"
set "BUILD_PRESET=%~1"

if "%BUILD_PRESET%"=="" set "BUILD_PRESET=debug"
if /I "%BUILD_PRESET%"=="Debug" set "BUILD_PRESET=debug"
if /I "%BUILD_PRESET%"=="Release" set "BUILD_PRESET=release"

if /I not "%BUILD_PRESET%"=="debug" if /I not "%BUILD_PRESET%"=="release" (
    echo Usage: scripts\build-vs2022.bat [debug^|release]
    exit /b 1
)

if not exist "%VS_VCVARS%" (
    echo Visual Studio vcvars64.bat was not found.
    exit /b 1
)

if not exist "%CMAKE_EXE%" (
    echo CMake was not found.
    exit /b 1
)

if not exist "%NINJA_EXE%" (
    echo Ninja was not found.
    exit /b 1
)

if not exist "%QT_PREFIX%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo Qt was not found at %QT_PREFIX%.
    exit /b 1
)

tasklist /FI "IMAGENAME eq astrax-browser.exe" 2>NUL | find /I "astrax-browser.exe" >NUL
if not errorlevel 1 (
    echo AstraX is still running. Close astrax-browser.exe, then run this build again.
    echo If it is stuck, run: taskkill /IM astrax-browser.exe /F
    exit /b 1
)

call "%VS_VCVARS%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --fresh --preset %BUILD_PRESET% -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build --preset %BUILD_PRESET%
if errorlevel 1 exit /b %errorlevel%

echo.
echo Build complete: build\%BUILD_PRESET%\astrax-browser.exe
