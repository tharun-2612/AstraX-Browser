@echo off
setlocal

set "APP_EXE=build\release\astrax-browser.exe"

if not exist "%APP_EXE%" (
    echo Release executable was not found. Build it first.
    exit /b 1
)

start "" "%APP_EXE%"
