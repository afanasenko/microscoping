@echo off
set PATH=C:/TOOLS/openslide/bin;%PATH%
set FILE="E:\08.02.2017(6)\244045 Heksa.mrxs"
cd x64/Release
mrxconvert.exe %FILE%
pause
