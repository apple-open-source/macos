#
# quickie Makefile to build the utilities
#

APACHE=../../apache-1.3
CC    =gcc
CFLAGS=-Wall

PROGRAMS=fixvers dbu lockview

all: $(PROGRAMS)

clean:
	-rm -f $(PROGRAMS) sdbm_combined.o

fixvers: fixvers.c sdbm_combined.o
	$(CC) $(CFLAGS) -o $@ fixvers.c -I.. sdbm_combined.o

dbu: dbu.c sdbm_combined.o
	$(CC) $(CFLAGS) -o $@ dbu.c -I../sdbm sdbm_combined.o

lockview: lockview.c sdbm_combined.o
	$(CC) $(CFLAGS) -o $@ \
            -I$(APACHE)/src/os/unix \
            -I$(APACHE)/src/include \
            lockview.c sdbm_combined.o

sdbm_combined.o:
	cd ..; make sdbm_combined.o && cp sdbm_combined.o util/
