Project = zic
Install_Dir = /usr/sbin

HFILES = private.h
CFILES = ialloc.c scheck.c zic.c
MANPAGES = zic.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

LOCALTIME=	US/Pacific
POSIXRULES=	US/Pacific

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

# Embedded zic is probably not built with the same architecture as
# the build host, so we can't use the recently built copy.
ifeq "$(Embedded)" "YES"
ZIC=/usr/sbin/zic
else
ZIC=$(DSTROOT)/usr/sbin/zic
endif

# pacificnew is obsolete and was removed from ZONE_FILES
ZONE_FILES = africa antarctica asia australasia europe northamerica southamerica etcetera factory backward systemv solar87 solar88 solar89
ZONEINFO = $(DSTROOT)/usr/share/zoneinfo

# ftp://elsie.nci.nih.gov/pub/tzdata*.tar.gz
# the tzdata*.tar.gz file is automatically unpacked and a version file created
# /usr/local/share/tz/tzdata*.tar.gz is installed by the TimeZoneData project
DATFILES = $(OBJROOT)/datfiles
TARBALL = $(shell ls $(SDKROOT)/usr/local/share/tz/tzdata* | sort | tail -n 1)
DATVERS = $(shell basename $(TARBALL) | sed -e 's,\..*,,' -e 's/^tzdata//')
VERSIONFILE = $(ZONEINFO)/+VERSION

after_install:
	$(MKDIR) $(DATFILES)
	cd $(DATFILES) && tar xzf $(TARBALL)
	$(INSTALL_DIRECTORY) $(ZONEINFO)
	for tz in $(ZONE_FILES); do \
	        if [[ $${tz} = "northamerica" ]]; then \
			ARG="-p America/New_York" ; \
		else \
			ARG="" ; \
		fi ; \
		$(ZIC) $${ARG} \
			-L /dev/null \
			-d $(ZONEINFO) \
			-y $(DATFILES)/yearistype.sh \
			$(DATFILES)/$${tz} ; \
	done
	chmod -R og-w $(ZONEINFO)
	$(INSTALL_FILE) $(DATFILES)/zone.tab $(ZONEINFO)
	$(INSTALL_FILE) $(DATFILES)/iso3166.tab $(ZONEINFO)
ifeq "$(Embedded)" "YES"
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/var/db
	$(INSTALL_DIRECTORY) -m a+rwx $(DSTROOT)/private/var/db/timezone
	$(LN) -hfs /usr/share/zoneinfo/$(LOCALTIME) $(DSTROOT)/private/var/db/timezone/localtime
else
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(LN) -hfs /usr/share/zoneinfo/$(LOCALTIME) $(DSTROOT)/private/etc/localtime
endif
	$(RM) $(VERSIONFILE)
	echo $(DATVERS) > $(VERSIONFILE)
	chmod 444 $(VERSIONFILE)
