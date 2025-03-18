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

:: Push the directory and change to parent

pushd ..

:: == Check if we want to pause

set "pause_flag=1"
set "new_params="
for %%a in (%*) do (
    if "%%a" == "--nopause" (
        set "pause_flag=0"
    ) else (
        set "new_params=!new_params! %%a"
    )
)

:: == Check if premake5 exists in the PATH

where premake5 2> nul
if %errorlevel% neq 0 (
    echo premake5 is not found in the system's PATH.
    echo Please make sure premake5 is installed and added to the PATH environment variable.
    if %pause_flag% equ 1 (
        timeout /t 5
    )
    popd
    exit /b %errorlevel%
)

:: == Invoke premake5

premake5 vs2022 %new_params%

if %errorlevel% neq 0 (
    if %pause_flag% equ 1 (
        timeout /t 5
    )
    popd
    exit /b %errorlevel%
)

:: == Wait

if %pause_flag% equ 1 (
    timeout /t 5
)

:: Pop the directory to return to original

popd
