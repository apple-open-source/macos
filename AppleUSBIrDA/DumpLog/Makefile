DIRS	= -I.. -I../IrDA -I../IrDA/Debugging -I../IrDA/IO
FRAMES	= -framework IOKit
WARN	= -Wall

all:	dumplog deltatime

dumplog:	dumplog.cpp
		cc $(DIRS) $(FRAMES) -fno-common $(WARN) dumplog.cpp -o dumplog 

deltatime:	deltatime.c
		cc $(DIRS) $(FRAMES) $(WARN) deltatime.c -o deltatime 

clean:
		rm -f dumplog deltatime

