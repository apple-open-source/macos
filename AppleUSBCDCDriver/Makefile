cwd  := $(shell pwd)

all:
	xcodebuild
	# make install - to quickly install in root without using the installer package
	# make pkg     - to make build/AppleVerizonInstaller.pkg
	# make clean   - to delete build and /tmp/AppleVerizon.dst

install:
	xcodebuild
	sudo xcodebuild -buildstyle Deployment install DSTROOT=/
	# clean up root-owned files in build
	sudo rm -rf build
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDC.kext
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMControl.kext
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.kext
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMControl.kext
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCECMData.kext
	sudo chown -R root:wheel /System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCWMCData.kext
	sudo rm -rf /System/Library/Caches/com.apple.kernelcaches
	sudo rm -rf /System/Library/Extensions.kextcache
	sudo rm -rf /System/Library/Extensions.mkext

pkg:
	( cd Package; make )

clean:
	( cd Package ; make clean )
	rm -rf build

