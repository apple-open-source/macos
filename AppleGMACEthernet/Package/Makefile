cwd  := $(shell pwd)

all:	../build/gmac-installer.pkg

clean:
	-chmod -R 777 /tmp/AppleGMACEthernet.dst
	rm -rf /tmp/AppleGMACEthernet.dst
	rm -rf ../build/gmac-installer.pkg

../build/gmac-installer.pkg:	/tmp/AppleGMACEthernet.dst
	-/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
	     -build \
	    -f /tmp/AppleGMACEthernet.dst/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns \
	    -p ${cwd}/../build/gmac-installer.pkg \
	    -r ${cwd}/resources \
	    -d ${cwd}/Description.plist \
	    -i ${cwd}/Info.plist

/tmp/AppleGMACEthernet.dst:
	-chmod -R 777 /tmp/AppleGMACEthernet.dst
	rm -rf /tmp/AppleGMACEthernet.dst
	(cd .. ; xcodebuild install )
