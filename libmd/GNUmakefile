Project            = libmd
Extra_CC_Flags     = -D__FBSDID=__RCSID
Extra_Environment  = NO_PROFILE=1 \
                     LIBDIR=/usr/local/lib \
                     INCLUDEDIR=/usr/local/include \
                     SHAREDIR=/usr/local/share \
                     CFLAGS="$(CFLAGS)"
BSD_Before_Install = make-install-dirs

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

make-install-dirs:
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MKDIR) $(DSTROOT)/usr/local/include
	$(MKDIR) $(DSTROOT)/usr/local/share/man/man3
