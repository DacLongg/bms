@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "BASH_EXE=%ProgramFiles%\Git\bin\bash.exe"
set "MAKE_TARGETS=%*"

if "%MAKE_TARGETS%"=="" set "MAKE_TARGETS=all"

if not exist "%SCRIPT_DIR%BMS\Makefile" (
    echo Cannot find "%SCRIPT_DIR%BMS\Makefile".
    echo Run this script from the repository root.
    exit /b 1
)

if exist "%ProgramData%\chocolatey\bin" set "PATH=%ProgramData%\chocolatey\bin;%PATH%"

if not exist "%BASH_EXE%" (
    for /f "delims=" %%P in ('where bash.exe 2^>nul') do (
        set "BASH_EXE=%%P"
        goto found_bash
    )
)

:found_bash
if not exist "%BASH_EXE%" (
    echo Git Bash was not found. Run setup_env.bat first.
    exit /b 1
)

set "ROOT_FOR_BASH=%SCRIPT_DIR:\=/%"
"%BASH_EXE%" -lc "cd '%ROOT_FOR_BASH%' && export SHELL=bash && make -C BMS %MAKE_TARGETS%"
exit /b %ERRORLEVEL%
