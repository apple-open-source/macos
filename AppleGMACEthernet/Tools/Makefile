
DIRS	= -I..
FRAMES	= -framework IOKit
WARN	= -Wall
LIBS	= -lm


all:	gmacLog gmacPHYregs gmacRegs gmacRings


gmacLog:	gmacLog.c
			cc $(DIRS) $(FRAMES) $(WARN)  -o gmacLog $(LIBS) gmacLog.c

gmacPHYregs:	gmacPHYregs.c
			cc $(DIRS) $(FRAMES) $(WARN) -o gmacPHYregs  $(LIBS) gmacPHYregs.c 

gmacRegs:	gmacRegs.c
			cc $(DIRS) $(FRAMES) $(WARN) -o gmacRegs $(LIBS) gmacRegs.c

gmacRings:	gmacRings.c
			cc $(DIRS) $(FRAMES) $(WARN) -o gmacRings $(LIBS) gmacRings.c 

clean:
			rm -f gmacLog gmacPHYregs gmacRegs gmacRings

