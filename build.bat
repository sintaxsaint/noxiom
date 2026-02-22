@echo off
chcp 65001 >nul
title Noxiom OS Build

echo.
echo  ________    ________      ___    ___  ___   ________   _____ ______
echo  ^|^\ ^  ___  ^\ ^|^\ ^  __  ^\    ^|^\  ^\  /  /^|^|^\  ^\ ^|^\   __  ^\ ^|^\   _ ^\  _   ^\
echo  ^\ ^\  ^\ ^\  ^\ ^\  ^|^\ ^\  ^\   ^\ ^\  \/  / /^\ ^\  ^\^\ ^\  ^|^\ ^\  ^\^\ ^\  ^\^\^\__^\ ^\  ^\
echo   ^\ ^\  ^\ ^\  ^\ ^\  ^\^\^\  ^\   ^\ ^\    / /  ^\ ^\  ^\^\ ^\  ^\^\^\  ^\^\ ^\  ^\^\^|__^| ^\  ^\
echo    ^\ ^\  ^\ ^\  ^\ ^\  ^\^\^\  ^\   /     \/    ^\ ^\  ^\^\ ^\  ^\^\  ^\^\ ^\  ^\    ^\ ^\  ^\
echo     ^\ ^\__^\ ^\__^\ ^\_______ ^\ /  /^\   ^\     ^\ ^\__^\^\ ^\_______ ^\^\ ^\__^\    ^\ ^\__^\
echo      ^\^|__^| ^\^|__^| ^\^|_______^|/__^/ /^\ __^\     ^\^|__^| ^\^|_______^| ^\^|__^|     ^\^|__^|
echo                            ^|__^|/ ^\^|__^|
echo.
echo  noxiom OS Build Script -sorry about the banner
echo  -------------------------------------------------------
echo.

pause

:: Check WSL is available and has a distro installed
:: wsl --list exits with code 1 when no distro is installed
wsl --list --quiet >nul 2>&1
if errorlevel 1 (
    echo [!] WSL2 has no Linux distro installed.
    echo.
    echo     Run this command in PowerShell or Windows Terminal:
    echo.
    echo         wsl --install -d Ubuntu
    echo.
    echo     Your PC will restart. After it boots back up, Ubuntu will
    echo     finish setting up -- just create a username and password.
    echo     Then run this script again.
    echo.
    pause
    exit /b 1
)

:: Check dependencies
echo [*] Checking build dependencies in WSL...
wsl bash -c "command -v nasm >/dev/null" >nul 2>&1
if errorlevel 1 goto install_deps
wsl bash -c "command -v aarch64-linux-gnu-gcc >/dev/null" >nul 2>&1
if errorlevel 1 goto install_deps
wsl bash -c "command -v qemu-system-x86_64 >/dev/null" >nul 2>&1
if errorlevel 1 goto install_deps
goto skip_install

:install_deps
echo [*] Some tools are missing -- installing now (this only happens once)...
echo     (You may be asked for your WSL password)
echo.
wsl sudo apt-get update -qq
wsl sudo apt-get install -y nasm gcc binutils qemu-system-x86 qemu-system-arm gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu build-essential
if errorlevel 1 (
    echo.
    echo [!] Dependency install failed. Check your internet connection.
    pause
    exit /b 1
)
echo.
echo [+] Dependencies installed.
echo.

:skip_install
echo.
echo  What do you want to do?
echo.
echo    [1] Build for PC        (x86_64)
echo    [2] Build for Pi        (arm64)
echo    [3] Build BOTH          (copies images to imgs\ for release upload)
echo    [4] Test in QEMU        (x86_64 - press Ctrl+A then X to quit)
echo    [5] Test in QEMU        (arm64 Pi - press Ctrl+A then X to quit)
echo    [6] Clean build output
echo    [7] Exit
echo.

set /p choice="  Enter number: "
echo.

if "%choice%"=="1" goto build_x86
if "%choice%"=="2" goto build_arm
if "%choice%"=="3" goto build_dist
if "%choice%"=="4" goto run_x86
if "%choice%"=="5" goto run_arm
if "%choice%"=="6" goto clean
if "%choice%"=="7" exit /b 0

echo [!] Invalid choice.
pause
exit /b 1

:build_x86
echo [*] Building for PC (x86_64)...
wsl make ARCH=x86_64
goto done

:build_arm
echo [*] Building for Raspberry Pi (arm64)...
wsl make ARCH=arm64
goto done

:build_dist
echo [*] Building x86_64...
wsl make ARCH=x86_64
if errorlevel 1 goto done
echo.
echo [*] Building arm64...
wsl make ARCH=arm64
if errorlevel 1 goto done
echo.
echo [*] Copying images to imgs\...
wsl make dist
goto done

:run_x86
echo [*] Launching QEMU (x86_64)...
echo     Press Ctrl+A then X to quit QEMU.
echo.
wsl make run ARCH=x86_64
goto done

:run_arm
echo [*] Launching QEMU (Raspberry Pi arm64)...
echo     Press Ctrl+A then X to quit QEMU.
echo.
wsl make run ARCH=arm64
goto done

:clean
echo [*] Cleaning build output...
wsl make clean
goto done

:done
echo.
if errorlevel 1 (
    echo [!] Something went wrong. Read the output above for the error.
) else (
    echo [+] Done!
)
echo.
pause