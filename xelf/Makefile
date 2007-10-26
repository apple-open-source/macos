##
# Makefile for Flex
##

# Project info
Flex_Version      = 2.5.4
Project           = flex
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = after_install
Extra_CC_Flags    = -mdynamic-no-pic
Extra_Environment = FLEXLIB=libl.a			\
		    STRIP_LIB_FLAGS="-S"
Install_Prefix	  = /usr/local
Install_Man       = /usr/local/share/man
# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
#Overrides


after_install::
	$(INSTALL) lex.sh $(RC_Install_Prefix)/bin/lex-$(Flex_Version)
	$(LN) -f $(RC_Install_Man)/man1/flex.1 $(RC_Install_Man)/man1/flex++.1
	$(LN) -f $(RC_Install_Man)/man1/flex.1 $(RC_Install_Man)/man1/lex.1
	$(LN) -fs flex-$(Flex_Version) $(RC_Install_Prefix)/bin/flex++-$(Flex_Version)
	$(MV) -f $(RC_Install_Prefix)/bin/flex $(RC_Install_Prefix)/bin/flex-$(Flex_Version)
	$(RM) -f $(RC_Install_Prefix)/bin/flex++
	@for arch in $(RC_ARCHS); do \
		case $$arch in \
		ppc64|x86_64) \
			echo "Deleting $$arch executable from $(RC_Install_Prefix)/bin/flex-$(Flex_Version)"; \
			lipo -remove $$arch $(RC_Install_Prefix)/bin/flex-$(Flex_Version) -output $(RC_Install_Prefix)/bin/flex-$(Flex_Version);; \
		esac; \
	done
	$(INSTALL) -d $(RC_Install_Prefix)/include/flex-$(Flex_Version)
	$(MV) -f $(RC_Install_Prefix)/include/FlexLexer.h $(RC_Install_Prefix)/include/flex-$(Flex_Version)
	$(MV) -f $(RC_Install_Prefix)/lib/libl.a $(RC_Install_Prefix)/lib/libl-$(Flex_Version).a
	$(LN) -fs libl-$(Flex_Version).a $(RC_Install_Prefix)/lib/libfl-$(Flex_Version).a 
	rm -rf $(DSTROOT)/Developer
