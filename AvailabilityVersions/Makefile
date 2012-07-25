Project=AvailabilityVersions

include $(MAKEFILEPATH)/CoreOS/Standard/Commands.make
include $(MAKEFILEPATH)/CoreOS/Standard/Variables.make

ifeq "$(RC_INDIGO)" "YES"
	VerifierDest	=	$(SDKROOT)/usr/local/libexec
else
	VerifierDest	=	/usr/local/libexec
endif

install:
	$(_v) $(MKDIR) "$(DSTROOT)$(VerifierDest)"
	$(_v) $(MKDIR) "$(DSTROOT)$(VerifierFiles)"
	$(_v) $(INSTALL) "$(SRCROOT)/availability.pl" "$(DSTROOT)$(VerifierDest)/availability.pl"

installhdrs:	install

installsrc:
	install -d "$(SRCROOT)"
	rsync -a --exclude=.svn --exclude=.git ./ "$(SRCROOT)"

clean:
	$(_v) echo Nothing to clean
