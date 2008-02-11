Project = getty
Install_Dir = /usr/libexec

HFILES = extern.h gettytab.h pathnames.h
CFILES = chat.c init.c main.c subr.c
MANPAGES = getty.8 gettytab.5 ttys.5

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),YES)
LAUNCHD_PLISTS = com.apple.getty.plist
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
