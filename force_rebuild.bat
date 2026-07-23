@echo off
echo ============================================
echo Hunter Transfer 强制重新构建脚本
echo ============================================
echo.

echo 1. 清理构建目录...
cd /d "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\build-release\GUI"
if exist moc_MainWindow.cpp del moc_MainWindow.cpp
echo   已删除 moc 文件

echo.
echo 2. 重新构建...
cd /d "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\build-release"
"C:\Users\DELL\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe"
if %errorlevel% neq 0 (
    echo 构建失败！
    pause
    exit /b 1
)
echo   构建成功

echo.
echo 3. 检查 exe 文件...
dir "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\build-release\App\HunterTransfer.exe"
echo.

echo 4. 更新发行包...
cd /d "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer"
copy "build-release\App\HunterTransfer.exe" "dist\HunterTransfer-1.0.0-windows-x86_64\bin\HunterTransfer.exe" /Y
echo   已复制 exe 文件

echo.
echo 5. 重新打包...
if exist "dist\HunterTransfer-1.0.0-windows-x86_64.zip" del "dist\HunterTransfer-1.0.0-windows-x86_64.zip"
powershell -Command "Compress-Archive -Path 'dist\HunterTransfer-1.0.0-windows-x86_64' -DestinationPath 'dist\HunterTransfer-1.0.0-windows-x86_64.zip' -Force"
echo   已重新打包

echo.
echo 6. 验证构建时间...
echo 最新构建时间：
dir "dist\HunterTransfer-1.0.0-windows-x86_64\bin\HunterTransfer.exe" | find "HunterTransfer"
echo.
echo ============================================
echo 请执行以下步骤：
echo 1. 删除旧的 HunterTransfer-1.0.0-windows-x86_64 目录
echo 2. 解压 dist\HunterTransfer-1.0.0-windows-x86_64.zip 到新目录
echo 3. 运行 bin\HunterTransfer.exe
echo 4. 检查右上角是否有 "Language:" 下拉菜单
echo 5. 点击 Source 旁边的 "..." 按钮测试文件选择
echo ============================================
pause