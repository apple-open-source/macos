Project = pax
Install_Dir = /bin

CFILES = ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c\
         ftree.c gen_subs.c getoldopt.c options.c pat_rep.c pax.c\
         sel_subs.c tables.c tar.c tty_subs.c pax_format.c
MANPAGES = pax.1 cpio.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID 
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/pax $(DSTROOT)/usr/bin/cpio
