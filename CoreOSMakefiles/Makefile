Project=CoreOSMakefiles

CoreOSMakefiles = .

include $(CoreOSMakefiles)/ReleaseControl/Common.make

Destination = $(MAKEFILEPATH)/CoreOS

install_headers::
	@$(MAKE) install_source SRCROOT=$(DSTROOT)$(Destination)
	$(_v) $(RM) -f $(DSTROOT)$(Destination)/Makefile
