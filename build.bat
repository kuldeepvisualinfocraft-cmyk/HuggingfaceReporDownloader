@echo off
chcp 65001 >nul
echo Building Hugging Face Repo Downloader...
g++ -std=c++17 -O2 -o hf_downloader.exe main.cpp -static
if %errorlevel% equ 0 (
    echo.
    echo Build successful! Run: hf_downloader.exe
) else (
    echo.
    echo Build failed.
)
pause
