@echo off
set PATH=C:/TOOLS/openslide/bin;%PATH%
set PATH=C:/TOOLS/gdcm/bin/Release;%PATH%
set FILE="E:\08.02.2017(6)\244045 Heksa.mrxs"
call "x64/Release/mrxconvert.exe" %FILE%
pause
