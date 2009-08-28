Project = chpass
Install_Dir = /usr/bin

HFILES = chpass.h open_directory.h pw_copy.h
CFILES = chpass.c edit.c field.c pw_copy.c table.c util.c \
	 open_directory.c
MANPAGES = chpass.1

Extra_CC_Flags = -mdynamic-no-pic
Extra_CC_Flags += -DOPEN_DIRECTORY -fconstant-cfstrings \
	-I../pwd_mkdb.tproj -I../vipw.tproj
Extra_LD_Flags = -dead_strip

Extra_Frameworks = -framework OpenDirectory -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

after_install:
	$(LN) -sf chpass.1 $(DSTROOT)/usr/share/man/man1/chfn.1
	$(LN) -sf chpass.1 $(DSTROOT)/usr/share/man/man1/chsh.1
	$(LN) -f $(DSTROOT)$(Install_Dir)/chpass $(DSTROOT)$(Install_Dir)/chfn
	$(LN) -f $(DSTROOT)$(Install_Dir)/chpass $(DSTROOT)$(Install_Dir)/chsh
