#
# silly makefile cause Jim's fingers type 'make' without any cognitive input
#
all:	build DumpLog/dumplog IrDAStatus/build  IrDADebugLog/build

build:
	pbxbuild

DumpLog/dumplog: 
	(cd DumpLog ; make)

IrDAStatus/build:
	(cd IrDAStatus ; pbxbuild)

IrDADebugLog/build:
	(cd IrDADebugLog ; pbxbuild)
	
clean:
	(cd DumpLog ; make clean)
	pbxbuild clean ; rm -rf build
	(cd IrDAStatus ; pbxbuild clean ; rm -rf build)
	(cd IrDADebugLog ; pbxbuild clean ; rm -rf build)

