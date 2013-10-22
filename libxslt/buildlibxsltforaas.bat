set ARCH=32
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" x86
nmake /f NMakefileArch %1
set ARCH=64
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" amd64
nmake /f NMakefileArch %1
