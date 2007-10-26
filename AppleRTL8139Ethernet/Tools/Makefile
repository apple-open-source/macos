
DIRS	= -I..
FRAMES	= -framework IOKit
WARN	= -Wall
LIBS	= -lm


all:	rltkLog rltkRegs rltkSetLoopback

clean:
		rm -f rltkLog rltkRegs rltkSetLoopback


rltkLog:			rltkLog.c
					cc $(DIRS) $(FRAMES) $(WARN)  -o rltkLog $(LIBS) rltkLog.c

rltkRegs:			rltkRegs.c
					cc $(DIRS) $(FRAMES) $(WARN) -o rltkRegs $(LIBS) rltkRegs.c

rltkSetLoopback:	rltkSetLoopback.c
					cc $(DIRS) $(FRAMES) $(WARN) -o rltkSetLoopback $(LIBS) rltkSetLoopback.c
