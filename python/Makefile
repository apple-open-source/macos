##
# Makefile for python
##

Project               = python
Extra_Configure_Flags = --enable-ipv6 --with-threads
Extra_Install_Flags   = DESTDIR=${DSTROOT} MANDIR=${DSTROOT}/usr/share/man
GnuAfterInstall       = strip-installed-files

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install 

strip-installed-files:
	strip -x ${DSTROOT}/usr/bin/python*
	rm ${DSTROOT}/usr/lib/python2.2/config/*.a
	rm ${DSTROOT}/usr/lib/python2.2/config/python.o
	rm -rf ${DSTROOT}/usr/lib/python2.2/test
	strip -x ${DSTROOT}/usr/lib/python2.2/lib-dynload/*.so
