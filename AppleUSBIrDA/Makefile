#
# silly makefile cause Jim's fingers type 'make' without any cognitive input
#
all:	build DumpLog/dumplog 

build:
	xcodebuild

DumpLog/dumplog: 
	(cd DumpLog ; make)

clean:
	(cd DumpLog ; make clean)
	xcodebuild clean ; rm -rf build

install:
	sudo xcodebuild install DSTROOT=/
	sudo touch /System/Library/Extensions
	sync

