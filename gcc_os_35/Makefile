installhdrs:
clean:

installsrc:
	cp Makefile $(SRCROOT)/Makefile

install:
	mkdir -p $(DSTROOT)/usr/bin
	ln -f -s gcc-4.0 $(DSTROOT)/usr/bin/gcc-3.5
	ln -f -s g++-4.0 $(DSTROOT)/usr/bin/g++-3.5
	mkdir -p $(DSTROOT)/usr/share/man/man1
	ln -f -s gcc-4.0.1 $(DSTROOT)/usr/share/man/man1/gcc-3.5.1
	ln -f -s g++-4.0.1 $(DSTROOT)/usr/share/man/man1/g++-3.5.1
 