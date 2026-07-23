@echo off
cd /d "C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\dist\HunterTransfer-1.0.0-windows-x86_64\bin"
set QT_DEBUG_PLUGINS=1
echo Running from: %cd%
echo qt.conf content:
type qt.conf
echo.
echo Directory structure:
dir ..\plugins
echo.
echo Running HunterTransfer.exe...
HunterTransfer.exe
pause