@echo off
setlocal enabledelayedexpansion

:: Push the directory and change to parent

pushd ..

:: Define the files and directories to delete

set "dirs=.vs .vscode .tmp .out .build"
set "files=.ignore .vsconfig"

:: Initialize variables to track existence

set "has_dirs=0"
set "has_files=0"

:: Check for existing directories and files

for %%d in (%dirs%) do if exist "%%d" set "has_dirs=1"
for %%f in (%files%) do if exist "%%f" set "has_files=1"

:: Exit if no directories or files exist

if "%has_dirs%%has_files%"=="00" (
    echo ^| No directories or files to delete.
    goto eof
)

:: Display existing directories and files

echo ^| =============================================================================================
echo ^| WARNING: This script will completely remove common files and folders not under source control
echo ^| =============================================================================================
if "%has_dirs%"=="1" (
    echo ^| Directories to be deleted:
    echo ^|
    echo ^| --------------------------------------------------------
    for %%d in (%dirs%) do if exist "%%d" echo ^| %%d
    echo ^| --------------------------------------------------------
    echo ^|
)
if "%has_files%"=="1" (
    echo ^| Files to be deleted:
    echo ^|
    echo ^| --------------------------------------------------------
    for %%f in (%files%) do if exist "%%f" echo ^| %%f
    echo ^| --------------------------------------------------------
    echo ^|
)

echo ^| ====^> Please confirm that you want to proceed (Y/N)?
set /p confirm=^| 
if /i not "%confirm%"=="Y" goto eof

:: Remove directories

for %%d in (%dirs%) do (
    if exist "%%d" (
        echo ^| Removing directory: %%d
        rmdir /s /q "%%d"
    )
)

:: Remove files

for %%f in (%files%) do (
    if exist "%%f" (
        echo ^| Removing file: %%f
        del /q "%%f"
    )
)

echo ^| Cleanup complete.
goto eof

:eof
ENDLOCAL
popd
timeout /t 5
