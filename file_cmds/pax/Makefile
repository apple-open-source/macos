Project = pax
Install_Dir = /bin

CFILES = ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c\
         ftree.c gen_subs.c getoldopt.c options.c pat_rep.c pax.c\
         sel_subs.c tables.c tar.c tty_subs.c pax_format.c
MANPAGES = pax.1

Extra_CC_Flags = -Werror -Wall -D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
