# extremely primitive makefile
# just for people that don't have gencat but need it to make man

# note: you only need gencat if you want non-English messages

gencat: gencat.o genlib.o
	$(CC) $(LDFLAGS) -o gencat gencat.o genlib.o

gencat.o genlib.o: gencat.h

genlib.o: msgcat.h

install: gencat
	cp gencat$(EXEEXT) $(DESTDIR)$(PREFIX)/usr/bin

clean:
	rm -f *~ *.o gencat

