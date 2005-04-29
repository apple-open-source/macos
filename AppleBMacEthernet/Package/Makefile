cwd  := $(shell pwd)

all:	../build/bmac-installer.pkg

clean:
	-chmod -R 777 /tmp/AppleBMacEthernet.dst
	rm -rf /tmp/AppleBMacEthernet.dst
	rm -rf ../build/bmac-installer.pkg

../build/bmac-installer.pkg:	/tmp/AppleBMacEthernet.dst
	-/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
	     -build \
	    -f /tmp/AppleBMacEthernet.dst/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns \
	    -p ${cwd}/../build/bmac-installer.pkg \
	    -r ${cwd}/resources \
	    -d ${cwd}/Description.plist \
	    -i ${cwd}/Info.plist

/tmp/AppleBMacEthernet.dst:
	-chmod -R 777 /tmp/AppleBMacEthernet.dst
	rm -rf /tmp/AppleBMacEthernet.dst
	(cd .. ; pbxbuild install )
