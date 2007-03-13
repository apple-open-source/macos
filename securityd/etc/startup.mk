#
#	Makefile to install the system-startup code for SecurityServer
#

# wouldn't it be nice if PBX actually $#@?@! defined those?
# (for future ref: CoreOS Makefiles define many standard path vars)
# Note: CORE_SERVICES_DIR should be absolute path in target environment (don't prefix with DSTROOT)
SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library
SYSTEM_CORE_SERVICES_DIR=/System/Library/CoreServices
ETC_DIR=$(DSTROOT)/private/etc
AUTHORIZATION_LOCATION=$(ETC_DIR)
AUTHORIZATION_PLIST=$(AUTHORIZATION_LOCATION)/authorization
VARDB=$(DSTROOT)/private/var/db
CANDIDATES=$(VARDB)/CodeEquivalenceCandidates

DST=$(ETC_DIR)/mach_init.d
SRC=$(SRCROOT)/etc


#
# The other phases do nothing
#
build:	
	@echo null build.

debug:
	@echo null debug.

profile:
	@echo null profile.

#
# Install
#
install:
	mkdir -p $(DST)
	cp $(SRC)/securityd.plist $(SRC)/securityd-installCD.plist $(DST)
	mkdir -p $(AUTHORIZATION_LOCATION)
	cp $(SRC)/authorization.plist $(AUTHORIZATION_PLIST)
	chown root:wheel $(AUTHORIZATION_PLIST)
	chmod 644 $(AUTHORIZATION_PLIST)
	mkdir -p $(VARDB)
	cp $(SRC)/CodeEquivalenceCandidates $(CANDIDATES)
	chown root:admin $(CANDIDATES)
	chmod 644 $(CANDIDATES)

installhdrs:
	@echo null installhdrs.

installsrc:
	@echo null installsrc.

clean:
	@echo null clean.
