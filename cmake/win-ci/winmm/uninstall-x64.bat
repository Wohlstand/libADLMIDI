@echo off

cd x86
drvsetup uninstall
cd ..\x64
drvsetup uninstall
cd ..

echo.
