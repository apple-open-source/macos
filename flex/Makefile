##
# Makefile for Flex
##

# Project info
Project           = flex
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = after_install
Extra_Environment = FLEX=lex FLEXLIB=libl.a			\
		    STRIP_LIB_FLAGS="-S"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

after_install::
	$(LN) -f $(RC_Install_Prefix)/bin/lex $(RC_Install_Prefix)/bin/flex
	$(LN) -fs flex $(RC_Install_Prefix)/bin/flex++
