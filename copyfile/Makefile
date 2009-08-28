Project = copyfile
Install_Dir = /usr/local/lib/system
ProductType = staticlib
BuildProfile = YES
BuildDebug = YES

CFILES = copyfile.c $(OBJROOT)/$(Project)/_version.c
MANPAGES = copyfile.3
MAN_DIR = $(DSTROOT)/usr/share/man/man3

Install_Headers_Directory = /usr/include
Install_Headers = copyfile.h

WFLAGS= -Wno-trigraphs -Wmissing-prototypes -Wreturn-type -Wformat \
	-Wmissing-braces -Wparentheses -Wswitch -Wunused-function \
	-Wunused-label -Wunused-variable -Wunused-value -Wshadow \
	-Wsign-compare -Wall -Wextra -Wpointer-arith -Wreturn-type \
	-Wwrite-strings -Wcast-align -Wbad-function-cast \
	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls \
	-Wno-parentheses -Wformat=2 -Wimplicit-function-declaration \
	-Wshorten-64-to-32 -Wformat-security

SDKROOT ?= /

Extra_CC_Flags = ${WFLAGS} -fno-common \
	-D__DARWIN_NOW_CANCELABLE=1 -I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
  
$(OBJROOT)/$(Project)/_version.c:
	/Developer/Makefiles/bin/version.pl copyfile > $@
 
after_install:
	for a in fcopyfile copyfile_state_alloc copyfile_state_free \
		copyfile_state_get copyfile_state_set ; do \
			ln $(MAN_DIR)/copyfile.3 $(MAN_DIR)/$$a.3 ; \
		done
