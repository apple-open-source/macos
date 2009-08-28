Project = telnet
Install_Dir = /usr/bin

HFILES = externs.h fdset.h general.h krb4-proto.h ring.h types.h\
         defines.h misc.h misc-proto.h
CFILES = authenc.c commands.c main.c network.c ring.c sys_bsd.c\
         telnet.c terminal.c tn3270.c utilities.c
MANPAGES = telnet.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK \
		-DAUTHENTICATION -DKRB5 -DSKEY -DIPSEC -DINET6 -DFORWARD 
		# -DENCRYPTION -DKRB4
Extra_LD_Libraries = -lcurses -lkrb4 -lkrb5 -lipsec -ltelnet

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
