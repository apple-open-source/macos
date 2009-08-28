Project = talk
Install_Dir = /usr/bin

HFILES = talk.h talk_ctl.h
CFILES = ctl.c ctl_transact.c display.c get_addrs.c get_names.c\
         get_iface.c init_disp.c invite.c io.c look_up.c msgs.c talk.c
MANPAGES = talk.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

Extra_LD_Libraries = -lcurses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
