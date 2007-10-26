###
# This program is copyright Alec Muffett 1993. The author disclaims all
# responsibility or liability with respect to it's usage or its effect
# upon hardware or computer systems, and maintains copyright as set out
# in the "LICENCE" document which accompanies distributions of Crack v4.0
# and upwards.
###

###
# You MIGHT want to remove "-Bstatic" if you're not on SunOS machines.
# If you ARE, then leave it and suffer a few extra Kb for security's sake
###

#SunOS users (and others?) should consider static linking of their passwd binary
#CFLAGS= -O -I../cracklib '-DCRACKLIB_DICTPATH="$(DICTPATH)"' -Bstatic

CFLAGS= -O -I../cracklib '-DCRACKLIB_DICTPATH="$(DICTPATH)"'
LIBS=	../cracklib/libcrack.a

all:	packer unpacker testnum teststr testlib
	touch all

packer: packer.o $(LIBS)
	cc $(CFLAGS) -o $@ $@.o $(LIBS)

unpacker: unpacker.o $(LIBS)
	cc $(CFLAGS) -o $@ $@.o $(LIBS)

testnum: testnum.o $(LIBS)
	cc $(CFLAGS) -o $@ $@.o $(LIBS)

teststr: teststr.o $(LIBS)
	cc $(CFLAGS) -o $@ $@.o $(LIBS)

testlib: testlib.o $(LIBS)
	cc $(CFLAGS) -o $@ $@.o $(LIBS)

clean:
	-rm *.o *~ all
	-rm teststr testnum testlib packer unpacker
