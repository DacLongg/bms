@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ORIGINAL_ARGS=%*"
set "RUN_BUILD=1"
set "INSTALL_VSCODE_EXT=1"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--no-build" (
    set "RUN_BUILD=0"
    shift
    goto parse_args
)
if /I "%~1"=="--no-vscode-ext" (
    set "INSTALL_VSCODE_EXT=0"
    shift
    goto parse_args
)
if /I "%~1"=="-h" goto usage
if /I "%~1"=="--help" goto usage
echo Unknown option: %~1
echo.
goto usage_error

:usage
echo Usage: setup_env.bat [options]
echo.
echo Options:
echo   --no-build        Install/check tools only; skip test build.
echo   --no-vscode-ext  Skip VS Code extension installation.
echo   -h, --help       Show this help.
exit /b 0

:usage_error
echo Usage: setup_env.bat [--no-build] [--no-vscode-ext]
exit /b 2

:args_done
if not exist "%SCRIPT_DIR%BMS\Makefile" (
    echo Cannot find "%SCRIPT_DIR%BMS\Makefile".
    echo Run this script from the repository root.
    exit /b 1
)

call :AddKnownPaths
call :PrintStatus
if errorlevel 1 (
    call :RequireAdmin %ORIGINAL_ARGS%
    if errorlevel 100 exit /b 100
    if errorlevel 1 exit /b 1

    call :EnsureChocolatey
    if errorlevel 1 exit /b 1

    call :InstallPackages
    call :RefreshEnvironment
    call :AddKnownPaths
)

call :PersistShellForMake

call :PrintStatus
if errorlevel 1 (
    echo.
    echo Some required tools are still missing after installation.
    echo Install the missing tool manually, then re-run setup_env.bat.
    exit /b 1
)

call :InstallVSCodeExtensions
call :ConfigureVSCodeWorkspace
if errorlevel 1 exit /b 1
if "%RUN_BUILD%"=="1" call :BuildProject
if errorlevel 1 exit /b 1

echo.
    echo Environment setup completed.
    echo Build: make -C BMS all
    echo Flash: make -C BMS flash
    echo Debug server: make -C BMS debug-server
exit /b 0

:PrintStatus
set "FAILED=0"
echo.
echo ==^> Checking build/debug tools
call :CheckTool make.exe "GNU Make"
call :CheckTool arm-none-eabi-gcc.exe "ARM GCC"
call :CheckTool arm-none-eabi-objcopy.exe "ARM objcopy"
call :CheckTool arm-none-eabi-size.exe "ARM size"
call :CheckTool arm-none-eabi-objdump.exe "ARM objdump"
call :CheckTool arm-none-eabi-gdb.exe "ARM GDB"
call :CheckTool openocd.exe "OpenOCD"
call :CheckOptionalTool st-flash.exe "ST-Link st-flash"
call :FindBash
if errorlevel 1 (
    echo [MISS] Git Bash/coreutils
    set "FAILED=1"
) else (
    echo [OK]   Git Bash/coreutils          !BASH_EXE!
)
exit /b !FAILED!

:CheckTool
where "%~1" >nul 2>nul
if errorlevel 1 (
    echo [MISS] %~2
    set "FAILED=1"
) else (
    for /f "delims=" %%P in ('where "%~1" 2^>nul') do (
        echo [OK]   %~2 %%P
        goto :eof
    )
)
goto :eof

:FindToolPath
set "%~2="
for /f "delims=" %%P in ('where "%~1" 2^>nul') do (
    set "%~2=%%P"
    goto :eof
)
exit /b 1

:CheckOptionalTool
where "%~1" >nul 2>nul
if errorlevel 1 (
    echo [SKIP] %~2 ^(optional; OpenOCD flash/debug is supported^)
) else (
    for /f "delims=" %%P in ('where "%~1" 2^>nul') do (
        echo [OK]   %~2 %%P
        goto :eof
    )
)
goto :eof

:RequireAdmin
net session >nul 2>nul
if not errorlevel 1 exit /b 0

echo.
echo Administrator permission is required to install missing tools.
echo Requesting elevation...
if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -ArgumentList '%*' -Verb RunAs"
)
if errorlevel 1 (
    echo Failed to request elevation.
    exit /b 1
)
exit /b 100

:EnsureChocolatey
where choco.exe >nul 2>nul
if not errorlevel 1 exit /b 0

echo.
echo ==^> Installing Chocolatey
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
if errorlevel 1 (
    echo Failed to install Chocolatey.
    exit /b 1
)
call :RefreshEnvironment
exit /b 0

:InstallPackages
echo.
echo ==^> Installing missing packages with Chocolatey
call :ChocoInstall git
call :ChocoInstall make
call :ChocoInstall gcc-arm-embedded
call :ChocoInstall openocd
exit /b 0

:ChocoInstall
echo Installing %~1...
choco install -y --no-progress "%~1"
if errorlevel 1 (
    echo Warning: Chocolatey could not install %~1.
)
exit /b 0

:RefreshEnvironment
if defined ChocolateyInstall (
    if exist "%ChocolateyInstall%\bin\refreshenv.cmd" call "%ChocolateyInstall%\bin\refreshenv.cmd"
)
if exist "%ProgramData%\chocolatey\bin\refreshenv.cmd" call "%ProgramData%\chocolatey\bin\refreshenv.cmd"
exit /b 0

:AddKnownPaths
call :AddPath "%ProgramData%\chocolatey\bin"
call :AddPath "%ProgramFiles%\Git\cmd"
call :AddPath "%ProgramFiles%\Git\bin"
call :AddPath "%ProgramFiles%\Git\usr\bin"
call :AddPath "%ProgramFiles(x86)%\Git\cmd"
call :AddPath "%ProgramFiles(x86)%\Git\bin"
call :AddPath "%ProgramFiles(x86)%\Git\usr\bin"
call :AddPath "%LocalAppData%\Programs\Microsoft VS Code\bin"
call :AddPath "%ProgramFiles%\Microsoft VS Code\bin"
call :AddPath "%ProgramFiles(x86)%\Microsoft VS Code\bin"
call :AddPath "%ProgramFiles%\OpenOCD\bin"
call :AddPath "%ProgramFiles(x86)%\OpenOCD\bin"
call :AddPath "%ProgramFiles%\stlink\bin"
call :AddPath "%ProgramFiles(x86)%\stlink\bin"
for /d %%D in ("%ProgramFiles%\Arm GNU Toolchain arm-none-eabi\*") do call :AddPath "%%~fD\bin"
for /d %%D in ("%ProgramFiles(x86)%\Arm GNU Toolchain arm-none-eabi\*") do call :AddPath "%%~fD\bin"
for /d %%D in ("%ProgramFiles%\GNU Arm Embedded Toolchain\*") do call :AddPath "%%~fD\bin"
for /d %%D in ("%ProgramFiles(x86)%\GNU Arm Embedded Toolchain\*") do call :AddPath "%%~fD\bin"
exit /b 0

:AddPath
if "%~1"=="" exit /b 0
if not exist "%~1" exit /b 0
echo ;!PATH!; | "%SystemRoot%\System32\find.exe" /I ";%~1;" >nul
if errorlevel 1 set "PATH=%~1;!PATH!"
exit /b 0

:FindBash
set "BASH_EXE="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles%\Git\bin\bash.exe"
if not defined BASH_EXE if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles(x86)%\Git\bin\bash.exe"
if not defined BASH_EXE (
    for /f "delims=" %%P in ('where bash.exe 2^>nul') do (
        if not defined BASH_EXE set "BASH_EXE=%%P"
    )
)
if defined BASH_EXE exit /b 0
exit /b 1

:PersistShellForMake
call :FindBash
if errorlevel 1 exit /b 0
set "MAKE_SHELL=%BASH_EXE%"
for %%I in ("%BASH_EXE%") do set "MAKE_SHELL=%%~sI"
set "SHELL=%MAKE_SHELL%"
setx SHELL "%MAKE_SHELL%" >nul 2>nul
exit /b 0

:InstallVSCodeExtensions
if not "%INSTALL_VSCODE_EXT%"=="1" exit /b 0
where code >nul 2>nul
if errorlevel 1 (
    echo.
    echo VS Code 'code' command not found; skipping extension installation.
    exit /b 0
)

echo.
echo ==^> Installing VS Code extensions
code --install-extension marus25.cortex-debug --force
code --install-extension ms-vscode.cpptools --force
exit /b 0

:ConfigureVSCodeWorkspace
echo.
echo ==^> Configuring VS Code workspace for Windows

set "VSCODE_DIR=%SCRIPT_DIR%.vscode"
if not exist "%VSCODE_DIR%" mkdir "%VSCODE_DIR%"

call :FindToolPath arm-none-eabi-gdb.exe GDB_EXE
if not defined GDB_EXE set "GDB_EXE=arm-none-eabi-gdb.exe"
call :FindToolPath arm-none-eabi-objdump.exe OBJDUMP_EXE
if not defined OBJDUMP_EXE set "OBJDUMP_EXE=arm-none-eabi-objdump.exe"
call :FindToolPath openocd.exe OPENOCD_EXE
if not defined OPENOCD_EXE set "OPENOCD_EXE=openocd.exe"

set "GDB_JSON=!GDB_EXE:\=/!"
set "OBJDUMP_JSON=!OBJDUMP_EXE:\=/!"
set "OPENOCD_JSON=!OPENOCD_EXE:\=/!"

> "%VSCODE_DIR%\launch.json" (
    echo {
    echo   "version": "0.2.0",
    echo   "configurations": [
    echo     {
    echo       "name": "Debug BMS (ST-Link)",
    echo       "cwd": "${workspaceFolder}/BMS",
    echo       "executable": "${workspaceFolder}/BMS/build/BMS.elf",
    echo       "request": "launch",
    echo       "type": "cortex-debug",
    echo       "servertype": "openocd",
    echo       "gdbPath": "/usr/bin/gdb-multiarch",
    echo       "objdumpPath": "/usr/bin/arm-none-eabi-objdump",
    echo       "configFiles": [
    echo         "interface/stlink.cfg",
    echo         "target/stm32l0.cfg"
    echo       ],
    echo       "device": "STM32L010C6Tx",
    echo       "runToEntryPoint": "main",
    echo       "preLaunchTask": "Rebuild BMS",
    echo       "windows": {
    echo         "gdbPath": "!GDB_JSON!",
    echo         "objdumpPath": "!OBJDUMP_JSON!",
    echo         "serverpath": "!OPENOCD_JSON!"
    echo       }
    echo     }
    echo   ]
    echo }
)
if errorlevel 1 exit /b 1

> "%VSCODE_DIR%\tasks.json" (
    echo {
    echo   "version": "2.0.0",
    echo   "tasks": [
    echo     {
    echo       "label": "Build BMS",
    echo       "type": "shell",
    echo       "command": "make",
    echo       "args": [
    echo         "-C",
    echo         "BMS",
    echo         "all"
    echo       ],
    echo       "windows": {
    echo         "command": "${workspaceFolder}\\build.bat",
    echo         "args": [
    echo           "all"
    echo         ]
    echo       },
    echo       "group": {
    echo         "kind": "build",
    echo         "isDefault": true
    echo       },
    echo       "problemMatcher": [
    echo         "$gcc"
    echo       ]
    echo     },
    echo     {
    echo       "label": "Build BMS UART Protocol",
    echo       "type": "shell",
    echo       "command": "make",
    echo       "args": [
    echo         "-C",
    echo         "BMS",
    echo         "all",
    echo         "USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1"
    echo       ],
    echo       "windows": {
    echo         "command": "${workspaceFolder}\\build.bat",
    echo         "args": [
    echo           "all",
    echo           "USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1"
    echo         ]
    echo       },
    echo       "problemMatcher": [
    echo         "$gcc"
    echo       ]
    echo     },
    echo     {
    echo       "label": "Rebuild BMS",
    echo       "dependsOrder": "sequence",
    echo       "dependsOn": [
    echo         "Clean BMS",
    echo         "Build BMS"
    echo       ],
    echo       "problemMatcher": []
    echo     },
    echo     {
    echo       "label": "Clean BMS",
    echo       "type": "shell",
    echo       "command": "make",
    echo       "args": [
    echo         "-C",
    echo         "BMS",
    echo         "clean"
    echo       ],
    echo       "windows": {
    echo         "command": "${workspaceFolder}\\build.bat",
    echo         "args": [
    echo           "clean"
    echo         ]
    echo       },
    echo       "problemMatcher": []
    echo     },
    echo     {
    echo       "label": "Flash BMS (ST-Link)",
    echo       "type": "shell",
    echo       "command": "make",
    echo       "args": [
    echo         "-C",
    echo         "BMS",
    echo         "flash-stlink"
    echo       ],
    echo       "windows": {
    echo         "command": "${workspaceFolder}\\build.bat",
    echo         "args": [
    echo           "flash"
    echo         ]
    echo       },
    echo       "problemMatcher": []
    echo     }
    echo   ]
    echo }
)
if errorlevel 1 exit /b 1

> "%VSCODE_DIR%\extensions.json" (
    echo {
    echo   "recommendations": [
    echo     "marus25.cortex-debug",
    echo     "ms-vscode.cpptools"
    echo   ]
    echo }
)
if errorlevel 1 exit /b 1
exit /b 0

:BuildProject
call :FindBash
if errorlevel 1 (
    echo Git Bash was not found; cannot run a reliable Windows build for this Makefile.
    exit /b 1
)

set "ROOT_FOR_BASH=%SCRIPT_DIR:\=/%"
echo.
echo ==^> Building BMS firmware
"%BASH_EXE%" -lc "cd '%ROOT_FOR_BASH%' && export SHELL=bash && make -C BMS all"
exit /b %ERRORLEVEL%
