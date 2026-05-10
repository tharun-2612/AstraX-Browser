@echo off
setlocal

set "QT_BIN=C:\Qt\6.8.3\msvc2022_64\bin"
set "APP_EXE=build\release\astrax-browser.exe"

if not exist "%APP_EXE%" (
    echo Release executable was not found. Build it first:
    echo cmake --build --preset release
    exit /b 1
)

if not exist "%QT_BIN%\windeployqt.exe" (
    echo windeployqt.exe was not found at %QT_BIN%.
    exit /b 1
)

"%QT_BIN%\windeployqt.exe" --release --compiler-runtime "%APP_EXE%"
if errorlevel 1 exit /b %errorlevel%

echo.
echo Deployment complete. Run:
echo %APP_EXE%
