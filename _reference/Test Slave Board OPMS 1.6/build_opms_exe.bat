@echo off
REM ============================================
REM  Build opms_test_app.py thanh file .exe doc lap
REM  Tu dong tim Python: py launcher -> python -> python3
REM ============================================

cd /d "%~dp0"

REM ---- Tim Python ----
set PY=
py -3 --version >nul 2>&1 && set PY=py -3
if not defined PY python --version >nul 2>&1 && set PY=python
if not defined PY python3 --version >nul 2>&1 && set PY=python3

if not defined PY (
    echo.
    echo [LOI] Khong tim thay Python tren may.
    echo Cai Python tu https://www.python.org/downloads/
    echo Khi cai nho tick "Add python.exe to PATH".
    echo.
    pause
    exit /b 1
)

echo Dung Python: %PY%
%PY% --version

echo.
echo [1/3] Cai dat pyserial + openpyxl + pyinstaller + pillow...
%PY% -m pip install --upgrade pip pyserial openpyxl pyinstaller pillow
if errorlevel 1 (
    echo [LOI] Cai dat that bai. Kiem tra mang / quyen admin.
    pause
    exit /b 1
)

echo.
echo [2/3] Build exe (onefile, khong console)...
set DATAOPT=
if exist app_config.json set DATAOPT=%DATAOPT% --add-data "app_config.json;."
if exist OPMS_Device.png set DATAOPT=%DATAOPT% --add-data "OPMS_Device.png;."
if exist Vivoo_logo.png  set DATAOPT=%DATAOPT% --add-data "Vivoo_logo.png;."
%PY% -m PyInstaller --onefile --windowed --name OPMS_Slave_Test %DATAOPT% --hidden-import ch348_test_app --hidden-import opms_theme opms_test_app.py
if errorlevel 1 (
    echo [LOI] Build that bai.
    pause
    exit /b 1
)

echo.
echo [3/3] Xong!
echo File exe: %~dp0dist\OPMS_Slave_Test.exe
pause
