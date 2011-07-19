Project = migcom
ProductType = tool
Install_Dir = /usr/libexec

MANPAGES = mig.1 migcom.1

LFILES = lexxer.l
YFILES = parser.y

CFILES = error.c global.c header.c mig.c routine.c server.c\
         statement.c string.c type.c user.c utils.c \
	$(SYMROOT)/ident.c

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-I.
Extra_LD_Flags = -dead_strip
Extra_I_Flags = -u -s

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(SYMROOT)/ident.c:
	@$(MKDIR) $(SYMROOT)
	echo "char *MigGenerationDate = \""`date`"\";" > $@
	echo "char *MigMoreData = \""`whoami`"@"`hostname`"\";" >> $@

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/usr/bin
	$(INSTALL_SCRIPT) mig.sh "$(DSTROOT)"/usr/bin/mig
