@echo off

cd x86
drvsetup install
cd ..\x64
drvsetup install
cd ..

echo.
