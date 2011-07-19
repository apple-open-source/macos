# buildit may need "-project removefile-123" (any number should do)
Project = removefile
ProductType = dylib
Install_Dir = /usr/lib/system
BuildDebug = YES
BuildProfile = YES

CFILES = removefile.c \
	removefile_random.c \
	removefile_sunlink.c \
	removefile_rename_unlink.c \
	removefile_tree_walker.c \
	$(OBJROOT)/__version.c

MANPAGES = removefile.3 checkint.3

Install_Headers = removefile.h checkint.h

Extra_CC_Flags = -Wall -Werror \
	-D__DARWIN_NON_CANCELABLE=1

Extra_LD_Flags = -Wl,-umbrella -Wl,System

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

$(OBJROOT)/__version.c:
	/Developer/Makefiles/bin/version.pl $(Project) > $@

after_install:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) LICENSE $(OSL)/$(Project).txt
	@for a in removefile_state_alloc removefile_state_free \
		removefile_state_get removefile_state_set ; do \
			CMD="$(LN) $(DSTROOT)/usr/share/man/man3/removefile.3 \
				$(DSTROOT)/usr/share/man/man3/$$a.3" ; \
			echo $$CMD ; $$CMD ; \
		done
	@for a in check_int32_add check_uint32_add check_int64_add \
		check_uint64_add check_int32_sub check_uint32_sub \
		check_int64_sub check_uint64_sub check_int32_mul \
		check_uint32_mul check_int64_mul check_uint64_mul \
		check_int32_div check_uint32_div check_int64_div \
		check_uint64_div ; do \
			CMD="$(LN) $(DSTROOT)/usr/share/man/man3/checkint.3 \
				$(DSTROOT)/usr/share/man/man3/$$a.3" ; \
			echo $$CMD ; $$CMD ; \
		done

test: 
	$(CC) -g test/test-removefile.c -o /tmp/test-removefile
	/tmp/test-removefile
