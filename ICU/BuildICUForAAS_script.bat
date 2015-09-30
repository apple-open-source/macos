@echo off
rem ================
rem # This assumes the following (for all the path variables,
rem # the path in the variable should not be quoted, and a final slash is optional):
rem # 1. The current directory is set to the top of the ICU source directory,
rem #    i.e. the location of this bat file and of the ICU top-level makefile.
rem # 2. SRCROOT is set to the Win-style path for that source directory.
rem # 3. DSTROOT is set to the Win-style install path, e.g.
rem #    C:\cygwin\home\Peter\ICUroot or C:\AppleInternal
rem # 4. OBJROOT and SYMROOT are also set to the appropriate Win-style path;
rem #    separate subdirectories under these directories will be used for 32-bit and 64-bit builds.
rem # 5. CYGWINPATH is set to the full Windows-style path for Cygwin tools including bash, e.g.
rem #    C:\cygwin\bin
rem # 6. VS100COMNTOOLS is set to the full Windows-style path for VS 2010 common tools, e.g.
rem #    C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\Tools\
rem #    (don't set explicitly, it should get set up by windows)
rem # 7. VC tools (compiler, linker) are in the following directories:
rem #    "%VS100COMNTOOLS%\..\..\VC\bin" (32-bit)
rem #    "%VS100COMNTOOLS%\..\..\VC\bin\amd64" (64-bit)
rem #    (the make parameter VS100VCTOOLS_PATH specifies the appropriate one)
rem # 8. This bat script is invoked with an argument of "'install" or "install_debug" depending
rem #    on which is desired.
rem #
rem # Example of usage:
rem # >cd to whatever SRCROOT should be, e.g. >cd C:\cygwin\home\Peter\ICU
rem # >set SRCROOT=C:\cygwin\home\Peter\ICU
rem # >set DSTROOT=C:\cygwin\home\Peter\ICUroot
rem # >set OBJROOT=C:\cygwin\home\Peter\ICU\build
rem # >set SYMROOT=C:\cygwin\home\Peter\ICU\build
rem # >set CYGWINPATH=C:\Cygwin\bin
rem # >BuildICUForAAS_script.bat install
rem ================
echo # CYGWINPATH= "%CYGWINPATH%"
echo # DSTROOT= "%DSTROOT%"
echo # OBJROOT= "%OBJROOT%"
echo # == Run vcvarsall for 32-bit
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
echo # == Run ICU make for 32-bit
@echo on
"%CYGWINPATH%\bash.exe" -c '/bin/make %1 WINDOWS=YES ARCH64=NO VS100VCTOOLS_PATH="%VS100COMNTOOLS%\..\..\VC\bin"'
@echo off
echo # == Run vcvarsall for 64-bit
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" amd64
echo # == Run ICU make for 64-bit
@echo on
"%CYGWINPATH%\bash.exe" -c '/bin/make %1 WINDOWS=YES ARCH64=YES VS100VCTOOLS_PATH="%VS100COMNTOOLS%\..\..\VC\bin\amd64"'
@echo off
echo # == All done!
