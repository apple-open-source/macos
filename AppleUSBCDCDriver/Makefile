ARCHS   ?= x86_64
DSTROOT ?= /
#cwd  := $(shell pwd)

all:
	make clean
	xcodebuild ARCHS="${ARCHS}"
	sudo chown -R root:wheel build/*/*.kext
	sudo chmod 755 build/*/*.kext/Contents/MacOS/*

install:
	make clean
	xcodebuild ARCHS="${ARCHS}"
	sudo xcodebuild ARCHS="${ARCHS}" install DSTROOT="${DSTROOT}"
	sudo touch /System/Library/Extensions
	sync

pkg:
	make clean
	( cd Package ; make ARCHS="${ARCHS}")

clean:
	sudo rm -rf build DerivedData

check:
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDC.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMControl.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMControl.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCWCM.kext
	ls -ld /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCDMM.kext	
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDC.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMControl.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMControl.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCWCM.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCDMM.kext
	

uninstall:
	sudo rm -rf /System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns/AppleUSBEthernet.kext
	sudo rm -rf /System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns/AppleUSBEthernet.old

	sudo rm -rf -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDC.kext
	sudo rm -rf -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDC.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMControl.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMControl.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMControl.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMControl.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCWCM.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCWCM.old

	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCDMM.kext
	sudo kextutil -k /mach_kernel -nt /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCDMM.old