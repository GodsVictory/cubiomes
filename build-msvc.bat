@echo off

set CommonCompilerFlags=-O2 -nologo -GR- -wd4201 -wd4028 -GS-

IF NOT EXIST build mkdir build
pushd build

cl %CommonCompilerFlags% -Fesearcher.exe ..\searcher.c ..\layers.c ..\generator.c ..\finders.c ..\util.c

popd