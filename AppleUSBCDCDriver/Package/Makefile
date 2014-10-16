ARCHS   ?= x86_64 
cwd  := $(shell pwd)

#Path1 := "/Developer/Applications/Utilities/PackageMaker.app"
#Path2 := "/DevTools/Utilities/PackageMaker.app"
#PPATH := $(shell if [ -d ${Path1} ]; then echo ${Path1}; else if [ -d ${Path2} ]; then echo ${Path2}; fi; fi)
#PM := "${PPATH}/Contents/MacOS/PackageMaker"

# grab the long version number out of my prefix file
#version  := $(word 3, $(shell grep VersionNumber ../Common/AppleUSBCDCCommon.h))
version  := $(word 3, $(shell grep COM_APPLE_CDC_DRIVER_LONG_VERSION ../AppleUSBCDC.xcconfig))

NAME := AppleUSBCDCDriver-${version}
PKG  := ../build/${NAME}.pkg
TMP  := /tmp/cdc.root/
#DMG  := ../build/${NAME}.dmg
TARBALL  := "${cwd}/../build/${NAME}-Root.tar.gz"


all:	${PKG}

clean:
	sudo rm -rf ${TMP}

#${PKG}:
#	${PM} --doc AppleUSBCDCDriver.pmdoc --out "${PKG}"

#${DMG}:	${PKG}
#	rm -f ${DMG}
#	/usr/bin/perl buildDMG.pl -buildDir ../build/ -compressionLevel 9 -dmgName ${NAME} -volName ${NAME} ${PKG}

${PKG}:
	(cd .. ; sudo xcodebuild ARCHS="${ARCHS}" install DSTROOT="${TMP}" )
	sudo pkgbuild --root=${TMP} --identifier com.apple.iokit.AppleUSBCDCDriver --version ${version} ${PKG}
	(cd ${TMP} ; sudo tar cvzf ${TARBALL} .)
	ls -ltr ../build
