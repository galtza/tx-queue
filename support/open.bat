@echo off
setlocal enabledelayedexpansion

:: Check for administrative privileges

net session >nul 2>&1
if %errorlevel% == 0 (
    echo Running with administrative privileges
) else (
    echo Requesting administrative privileges...
    powershell -Command "Start-Process cmd -ArgumentList '/c ^"%~f0^" %*' -Verb runAs" | Out-Null
    exit /b
)

:: Set the directory to the script's location

cd /d %~dp0

:: Generate if it requires to

if not exist ".build" (
    call gen.bat --nopause
    if %errorlevel% neq 0 (
        exit /b %errorlevel%
        timeout /t 5
    )
)

:: Move one folder up

pushd ..

:: Open visual studio

FOR /R .build %%I IN (*.sln) DO (
    start "" "%%I"
    GOTO :EOF
)

:: Pop the directory to return to original

:EOF
popd
