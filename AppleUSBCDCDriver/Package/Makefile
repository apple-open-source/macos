cwd  := $(shell pwd)

#
# usage:	make
# 	   or
#		make clean
#
# don't do
#		make ../build/USB-CDC-Drivers.pkg
#		make dmg
#
# as the dependencies are not set up yet
#

DSTROOT := /tmp/AppleUSBCDC.dst
PKG     := ../build/USB-CDC-Drivers.pkg
DMG	:= Apple-USB-CDC-Installer

all:	clean ${PKG} dmg

clean:
	-sudo chmod -R 777 ${DSTROOT}
	sudo rm -rf        ${DSTROOT}
	rm -rf ../build

${PKG}:	${DSTROOT}
	-/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
	    -build \
	    -f ${DSTROOT}/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns \
	    -p "${cwd}/${PKG}" \
	    -r "${cwd}/Resources" \
	    -d "${cwd}/Description.plist" \
	    -i "${cwd}/Info.plist"

${DSTROOT}:	clean
	-( cd .. ; xcodebuild ; sudo xcodebuild install ; sudo rm -rf build/AppleUSBCDC.build)


dmg:	${PKG}
	/usr/bin/perl buildDMG.pl \
		-buildDir ../build/ \
		-compressionLevel 9 \
		-dmgName "${DMG}" \
		-volName "${DMG}" \
		"${PKG}"
