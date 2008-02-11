Project = getconf
Install_Dir = /usr/bin

CFILES = getconf.c \
	$(OBJROOT)/$(Project)/confstr.c \
	$(OBJROOT)/$(Project)/limits.c \
	$(OBJROOT)/$(Project)/pathconf.c \
	$(OBJROOT)/$(Project)/progenv.c \
	$(OBJROOT)/$(Project)/sysconf.c 
MANPAGES = getconf.1

Extra_CC_Flags = -include _fbsd_compat.h -DAPPLE_GETCONF_UNDERSCORE \
		-DAPPLE_GETCONF_SPEC -I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(OBJROOT)/$(Project)/%.c: %.gperf
	LC_ALL=C awk -f fake-gperf.awk $< > $@
