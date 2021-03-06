@echo off

REM SET CompilerFlags=/nologo /Zi /fp:fast /Wall /W1
SET LinkerFlags=/incremental:no user32.lib gdi32.lib Comdlg32.lib winmm.lib

REM Release Settings
SET CompilerFlags=/nologo /Oi /Ox /fp:fast /favor:INTEL64 /w /MT

if not exist ..\build mkdir ..\build
pushd ..\build

cl.exe %CompilerFlags% ..\code\win32_pixeleditor.cpp /link %LinkerFlags%

popd
