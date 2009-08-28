ARCHS   ?= ppc i386 x86_64
DSTROOT ?= /


all:
	make clean
	xcodebuild ARCHS="${ARCHS}"

utils:
	xcodebuild ARCHS="${ARCHS}" -target DumpLog
	xcodebuild ARCHS="${ARCHS}" -target Status
	xcodebuild ARCHS="${ARCHS}" -target "Debug Log"

clean:
	sudo rm -rf build

install:
	make clean
	sudo xcodebuild install DSTROOT="${DSTROOT}"
	sudo touch /System/Library/Extensions/IOSerialFamily.kext/Contents/PlugIns
	sudo touch /System/Library/Extensions/IOSerialFamily.kext/Contents
	sudo touch /System/Library/Extensions/IOSerialFamily.kext
	sudo touch /System/Library/Extensions
	sync ; sync
