@echo off
echo ============================================
echo Hunter Transfer Test Script
echo ============================================
echo.
echo 1. Copying latest exe to dist...
copy "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\build-release\App\HunterTransfer.exe" "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\dist\HunterTransfer-1.0.0-windows-x86_64\bin\HunterTransfer.exe" /Y
echo.
echo 2. Running program from dist directory...
cd "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\dist\HunterTransfer-1.0.0-windows-x86_64\bin"
echo Current directory: %cd%
echo.
echo 3. Starting HunterTransfer.exe...
echo Please check:
echo   - Language dropdown menu (top-right corner)
echo   - File selection dialog (click ... button next to Source)
echo   - Should show FILE selection, not folder selection
echo.
start HunterTransfer.exe
echo.
echo 4. Program started. Check the interface.
pause