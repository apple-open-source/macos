##
# Makefile for Flex
##

# Project info
Project           = flex
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = after_install
Extra_CC_Flags    = -mdynamic-no-pic
Extra_Environment = FLEX=lex FLEXLIB=libl.a			\
		    STRIP_LIB_FLAGS="-S"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

after_install::
	$(LN) -f $(DSTROOT)$(USRBINDIR)/lex $(DSTROOT)$(USRBINDIR)/flex
	$(LN) -f $(DSTROOT)/usr/share/man/man1/lex.1 $(DSTROOT)/usr/share/man/man1/flex++.1
	$(LN) -f $(DSTROOT)/usr/share/man/man1/lex.1 $(DSTROOT)/usr/share/man/man1/lex++.1
	$(LN) -f $(DSTROOT)/usr/share/man/man1/lex.1 $(DSTROOT)/usr/share/man/man1/flex.1
	$(LN) -fs flex $(DSTROOT)$(USRBINDIR)/flex++
	$(LN) -fs libl.a $(DSTROOT)$(USRLIBDIR)/libfl.a
