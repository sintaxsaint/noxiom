@echo off
:: build_exe.bat — Build NoxiomInstaller.exe using PyInstaller
::
:: Prerequisites:
::   pip install pyinstaller
::
:: Output:
::   installer\dist\NoxiomInstaller.exe   (single self-contained file)
::
:: The --uac-admin flag embeds a Windows manifest that requests Administrator
:: privileges automatically on launch — no manual UAC prompt code needed.

echo [*] Checking for PyInstaller...
python -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo [!] PyInstaller not found. Installing...
    pip install pyinstaller
    if errorlevel 1 (
        echo [!] Failed to install PyInstaller. Run: pip install pyinstaller
        pause
        exit /b 1
    )
)

echo [*] Building NoxiomInstaller.exe...
python -m PyInstaller ^
    --onefile ^
    --windowed ^
    --uac-admin ^
    --name "NoxiomInstaller" ^
    --clean ^
    noxiom_installer.py

if errorlevel 1 (
    echo [!] Build failed.
    pause
    exit /b 1
)

echo.
echo [+] Done! Executable is at: dist\NoxiomInstaller.exe
pause
