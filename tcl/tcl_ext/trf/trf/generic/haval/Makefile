CC=gcc
#CFLAGS=-fast

haval: haval.o havaltest.o 
	${CC} ${CFLAGS} haval.o havaltest.o -o $@

haval.o havaltest.o: havalapp.h

clean:
	rm -f *.o haval
