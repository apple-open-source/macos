#!/bin/sh

#
# Copyright (c) 2009 Apple Inc. All rights reserved.
#
# @APPLE_APACHE_LICENSE_HEADER_START@
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# @APPLE_APACHE_LICENSE_HEADER_END@
#

# Simple script to run the libclosure tests
# Note: to build the testing root, the makefile will ask to authenticate with sudo
# Use the RootsDirectory environment variable to direct the build to somewhere other than /tmp/

RootsDirectory=${RootsDirectory:-/tmp/}
StartingDir="$PWD"
AutoDir="`dirname $0`"
TestsDir="tests/"
cd "$AutoDir"
# <rdar://problem/6456031> ER: option to not require extra privileges (-nosudo or somesuch)
Buildit="/Network/Servers/xs1/release//bin/buildit -rootsDirectory ${RootsDirectory} -arch i386 -arch ppc -arch x86_64 -project libauto ."  
#Buildit=~rc/bin/buildit -rootsDirectory "${RootsDirectory}" -arch i386 -arch ppc -arch x86_64 -project libauto
echo Sudoing for buildit:
sudo $Buildit
XIT=$?
if [[ $XIT == 0 ]]; then
  cd "$TestsDir"
  AutoRootPath="$RootsDirectory/libauto.roots/libauto~dst/usr/lib/"
  DYLD_LIBRARY_PATH="$AutoRootPath" make
  XIT=$?
  make clean
fi
cd "$StartingDir"
exit $XIT
