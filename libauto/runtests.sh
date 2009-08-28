#!/bin/sh
# Simple script to run the libclosure tests
# Note: to build the testing root, the makefile will ask to authenticate with sudo
# Use the RootsDirectory environment variable to direct the build to somewhere other than /tmp/

RootsDirectory=${RootsDirectory:-/tmp/}
StartingDir="$PWD"
AutoDir="`dirname $0`"
TestsDir="tests/"
cd "$AutoDir"
# <rdar://problem/6456031> ER: option to not require extra privileges (-nosudo or somesuch)
Buildit="/Network/Servers/xs1/release//bin/buildit -rootsDirectory ${RootsDirectory} -arch i386 -arch ppc -arch x86_64 -project autozone ."  
#Buildit=~rc/bin/buildit -rootsDirectory "${RootsDirectory}" -arch i386 -arch ppc -arch x86_64 -project autozone
echo Sudoing for buildit:
sudo $Buildit
XIT=$?
if [[ $XIT == 0 ]]; then
  cd "$TestsDir"
  AutozoneRootPath="$RootsDirectory/autozone.roots/autozone~dst/usr/lib/"
  DYLD_LIBRARY_PATH="$AutozoneRootPath" make
  XIT=$?
  make clean
fi
cd "$StartingDir"
exit $XIT
