Project = fsck_hfs
ProductType = tool
Install_Dir = /sbin

SubProjects = dfalib

HFILES = fsck_hfs.h cache.h fsck_debug.h \
	fsck_keys.h fsck_hfs_msgnums.h fsck_msgnums.h
CFILES = fsck_hfs.c utilities.c cache.c fsck_debug.c \
	fsck_messages.c fsck_strings.c fsck_hfs_strings.c \
	$(OBJROOT)/$(Project)/_version.c
MANPAGES = fsck_hfs.8

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

export CONFIG_HFS_TRIM ?= 1

Extra_CC_Flags = -DCONFIG_HFS_TRIM=$(CONFIG_HFS_TRIM) -Wall -Werror \
	-Wno-error=deprecated-declarations  -Wno-error=unused-function -Idfalib

Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldfalib -framework IOKit -framework CoreFoundation

ifeq ($(Embedded),NO)
	Extra_CC_Flags += -mdynamic-no-pic
endif

HFS_INSTALLDIR = /System/Library/Filesystems/hfs.fs
FSCK_STRINGPATH = $(HFS_INSTALLDIR)/Contents/Resources/English.lproj/fsck.strings
INCINSTALLDIR = ${DSTROOT}/usr/local/include/fsck

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

${SYMROOT}/fsck_makestrings:	${SYMROOT}/fsck_makestring1.o fsck_strings.o fsck_hfs_strings.o
	${CC} ${CFLAGS} ${SYMROOT}/fsck_makestring1.o fsck_strings.o fsck_hfs_strings.o -o ${SYMROOT}/fsck_makestrings

${SYMROOT}/fsck_makestring1.o:	fsck_messages.c fsck_messages.h
	${CC} ${CFLAGS} -I. -Idfalib -DBSD -DFSCK_MAKESTRINGS -c fsck_messages.c -o ${SYMROOT}/fsck_makestring1.o

$(OBJROOT)/$(Project)/_version.c:
	${MAKEFILEPATH}/bin/version.pl diskdev_cmds > $@

after_install:	${SYMROOT}/fsck_makestrings
	$(RMDIR) "$(DSTROOT)/scratch"
	$(INSTALL_DIRECTORY) "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents/Resources/English.lproj
	printf "\376\377" > "$(DSTROOT)$(FSCK_STRINGPATH)"
	${SYMROOT}/fsck_makestrings | tr \\012 \\015 | sed 's/./~&/g' | tr -d \\012 | tr \\015 \\012 | tr \\176 \\000 >> $(DSTROOT)$(FSCK_STRINGPATH)
	$(CHOWN) -Rf root:wheel "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents
	find "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents -type d -exec chmod 755 {} \;
	find "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents -type f -exec chmod 644 {} \;
	install -d -o root -g wheel -m 0755 ${INCINSTALLDIR}
	install -c -o root -g wheel -m 0444 fsck_keys.h ${INCINSTALLDIR}

