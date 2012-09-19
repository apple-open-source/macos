## cwd  := $(shell pwd)

Path1 := "/Developer/Applications/Utilities/PackageMaker.app"
Path2 := "/DevTools/Utilities/PackageMaker.app"
PPATH := $(shell if [ -d ${Path1} ]; then echo ${Path1}; else if [ -d ${Path2} ]; then echo ${Path2}; fi; fi)
PM := "${PPATH}/Contents/MacOS/PackageMaker"

# grab the long version number out of my prefix file
version  := $(word 3, $(shell grep VersionNumber ../Common/AppleUSBCDCCommon.h))

NAME := AppleUSBCDCDriver-${version}
PKG  := ../build/${NAME}.pkg
DMG  := ../build/${NAME}.dmg
#PM   := /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker


all:	${PKG}

${PKG}:
	${PM} --doc AppleUSBCDCDriver.pmdoc --out "${PKG}"

${DMG}:	${PKG}
	rm -f ${DMG}
	/usr/bin/perl buildDMG.pl -buildDir ../build/ -compressionLevel 9 -dmgName ${NAME} -volName ${NAME} ${PKG}


# Bug avoidance
#
# If the targets are not installed on this machine, then when the package is created
# it won't be using the correct permissions.  You can extract the package (xar -xf sss.pkg) and
# check the bom file (lsbom x/bom) and check on it w/out running the installer.
#
# For now -- have the parent Makefile do a 'make install' before making the package
