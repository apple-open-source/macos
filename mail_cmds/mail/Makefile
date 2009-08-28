Project = mail
Install_Dir = /usr/bin

CFILES = aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c \
         edit.c fio.c getname.c head.c lex.c list.c main.c names.c\
         popen.c quit.c send.c strings.c temp.c tty.c v7.local.c\
         vars.c version.c
MANPAGES = mail.1 mailx.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

ETCDIR=$(DSTROOT)/private/etc
MISCDIR=$(DSTROOT)/usr/share/misc

after_install:
	$(INSTALL_DIRECTORY) $(MISCDIR)
	$(INSTALL_FILE) -c misc/mail.help misc/mail.tildehelp $(MISCDIR)
	$(INSTALL_DIRECTORY) $(ETCDIR)
	$(INSTALL_FILE) -c -m 644 misc/mail.rc $(ETCDIR)/mail.rc
	$(LN) -f $(DSTROOT)$(Install_Dir)/mail \
		 $(DSTROOT)$(Install_Dir)/mailx
