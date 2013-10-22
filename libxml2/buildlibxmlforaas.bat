set ARCH=32
set PROGRAMFILESAAS=Program Files (x86)\Common Files\Apple\Apple Application Support
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" x86
nmake /f NMakefileArch %1
set ARCH=64
set PROGRAMFILESAAS=Program Files\Common Files\Apple\Apple Application Support
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" amd64
nmake /f NMakefileArch %1
