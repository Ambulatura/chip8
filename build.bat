@echo off

IF NOT EXIST build mkdir build
pushd build
call vcvarsall.bat x64

set common_compiler_flags=/TC /O2 /Oi /MTd /Zi /FC /nologo /W4 /WX /wd4100 /wd4505 /wd4189 /wd4201 /wd4706

set common_linker_flags=/incremental:no /opt:ref

REM 64-bit build.

cl %common_compiler_flags% ..\source\win32_chip8.c /link %common_linker_flags% gdi32.lib user32.lib winmm.lib shell32.lib

popd
