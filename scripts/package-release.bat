@echo off
setlocal

set "APP_NAME=AstraX Browser"
set "QT_BIN=C:\Qt\6.8.3\msvc2022_64\bin"
set "APP_EXE=build\release\astrax-browser.exe"
set "RELEASE_DIR=dist\AstraX-Browser-win64"
set "ZIP_FILE=dist\AstraX-Browser-win64.zip"
set "RELEASE_EXE=%RELEASE_DIR%\astrax-browser.exe"

if not exist "%APP_EXE%" (
    echo Release executable was not found. Build it first:
    echo scripts\build-vs2022.bat release
    exit /b 1
)

if not exist "%QT_BIN%\windeployqt.exe" (
    echo windeployqt.exe was not found at %QT_BIN%.
    exit /b 1
)

if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

copy "%APP_EXE%" "%RELEASE_EXE%" >NUL
if errorlevel 1 exit /b %errorlevel%

if exist "README.md" copy "README.md" "%RELEASE_DIR%\README.md" >NUL
if exist "assets\AstraX.ico" copy "assets\AstraX.ico" "%RELEASE_DIR%\AstraX.ico" >NUL

"%QT_BIN%\windeployqt.exe" --release --compiler-runtime "%RELEASE_EXE%"
if errorlevel 1 exit /b %errorlevel%

if exist "%ZIP_FILE%" del "%ZIP_FILE%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%RELEASE_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 exit /b %errorlevel%

echo.
echo Portable release package created:
echo %ZIP_FILE%
echo.
echo Upload this ZIP to your GitHub Release or portfolio download button.
