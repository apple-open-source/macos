
CC=gcc
CFLAGS=-O2

haval:	haval.o havaltest.o 
	${CC} ${CFLAGS} haval.o havaltest.o -o $@
haval.o havaltest.o: havalapp.h

clean:
	/usr/bin/rm -f *.o haval

