@echo off

SET CompilerFlags=/nologo /Z7
SET LinkerFlags=/incremental:no user32.lib gdi32.lib Comdlg32.lib

if not exist ..\build mkdir ..\build
pushd ..\build

cl.exe %CompilerFlags% ..\code\win32_pixeleditor.cpp /link %LinkerFlags%

popd
