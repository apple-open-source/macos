cwd  := $(shell pwd)

all:
	xcodebuild
	# make install - to put into /System/Library/Extensions/IOSerialFamily.kext/Contents/PlugIns
	# make clean - to delete build

install:
	xcodebuild
	sudo xcodebuild install DSTROOT=/
	sudo touch /System/Library/Extensions
	sudo -k

clean:
	rm -rf build
