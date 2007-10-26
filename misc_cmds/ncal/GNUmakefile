Project            = ncal
Extra_CC_Flags     = -mdynamic-no-pic -D__FBSDID=__RCSID
Extra_Environment  = NO_TOOLCHAIN=1
BSD_Before_Build = copy-manpage

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

copy-manpage:
	$(MKDIR) $(OBJROOT)
	$(CP) $(SRCROOT)/ncal.1 $(OBJROOT)/cal.1
