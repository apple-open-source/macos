# The library only exists while building various man-related programs. In
# order to build it, cheat and use BSD make. The install targets exist for
# building with the buildit script.

installsrc:
	/bin/mkdir -p -m 0755 $(SRCROOT)/lib
	/bin/cp -Rp * $(SRCROOT)/lib

installhdrs:
	(cd $(SRCROOT)/lib; /bin/chmod +w .; bsdmake)

install:
	# nothing

all:
	bsdmake all

clean:
	bsdmake clean
