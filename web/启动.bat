@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================
echo   小车遥控 - 本地服务器
echo ============================================
echo.

where python >nul 2>nul
if errorlevel 1 (
    echo [错误] 找不到 python，请先安装或加入 PATH
    pause
    exit /b 1
)

echo 正在启动 http://localhost:8765/control.html
echo 浏览器打开后，点「连接串口」选择 COM 口。
echo 关闭这个窗口即可停止服务器。
echo.

start "" http://localhost:8765/control.html
python -m http.server 8765
