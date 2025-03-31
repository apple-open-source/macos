#	Makefile for f2c, a Fortran 77 to C converter

.SUFFIXES: .c .o
CC = cc
CFLAGS = -O
SHELL = /bin/sh
YACC = yacc
YFLAGS =

.c.o:
	$(CC) -c $(CFLAGS) $*.c

OBJECTSd = main.o init.o gram.o lex.o proc.o equiv.o data.o format.o \
	  expr.o exec.o intr.o io.o misc.o error.o mem.o names.o \
	  output.o p1output.o pread.o put.o putpcc.o vax.o formatdata.o \
	  parse_args.o niceprintf.o cds.o sysdep.o version.o

MALLOC =
# To use the malloc whose source accompanies the f2c source, add malloc.o
# to the right-hand side of the "MALLOC =" line above, so it becomes
#	MALLOC = malloc.o
# This gives faster execution on some systems, but some other systems do
# not tolerate replacement of the system's malloc.

OBJECTS = $(OBJECTSd) $(MALLOC)

all: xsum.out f2c

f2c: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o f2c

# The following used to be a rule for gram.c rather than gram1.c, but
# there are too many broken variants of yacc around, so now we
# distribute a correctly functioning gram.c (derived with a Unix variant
# of the yacc from plan9).

gram1.c: gram.head gram.dcl gram.expr gram.exec gram.io defs.h tokdefs.h
	( sed <tokdefs.h "s/#define/%token/" ;\
		cat gram.head gram.dcl gram.expr gram.exec gram.io ) >gram.in
	$(YACC) $(YFLAGS) gram.in
	@echo "(There should be 4 shift/reduce conflicts.)"
	sed 's/^# line.*/\/* & *\//' y.tab.c >gram.c
	rm -f gram.in y.tab.c

$(OBJECTSd): defs.h ftypes.h defines.h machdefs.h sysdep.h

tokdefs.h: tokens
	grep -n . <tokens | sed "s/\([^:]*\):\(.*\)/#define \2 \1/" >tokdefs.h

cds.o: sysdep.h
exec.o: p1defs.h names.h
expr.o: output.h niceprintf.h names.h
format.o: p1defs.h format.h output.h niceprintf.h names.h iob.h
formatdata.o: format.h output.h niceprintf.h names.h
gram.o: p1defs.h
init.o: output.h niceprintf.h iob.h
intr.o: names.h
io.o: names.h iob.h
lex.o : tokdefs.h p1defs.h
main.o: parse.h usignal.h
mem.o: iob.h
names.o: iob.h names.h output.h niceprintf.h
niceprintf.o: defs.h names.h output.h niceprintf.h
output.o: output.h niceprintf.h names.h
p1output.o: p1defs.h output.h niceprintf.h names.h
parse_args.o: parse.h
proc.o: tokdefs.h names.h niceprintf.h output.h p1defs.h
put.o: names.h pccdefs.h p1defs.h
putpcc.o: names.h
vax.o: defs.h output.h pccdefs.h
output.h: niceprintf.h
sysdep.o: sysdep.c sysdep.hd

put.o putpcc.o: pccdefs.h

sysdep.hd:
	if $(CC) sysdeptest.c; then echo '/*OK*/' > sysdep.hd;\
	elif $(CC) -DNO_MKDTEMP sysdeptest.c; then echo '#define NO_MKDTEMP' >sysdep.hd;\
	else echo '#define NO_MKDTEMP' >sysdep.hd; echo '#define NO_MKSTEMP' >>sysdep.hd; fi
	rm -f a.out

f2c.t: f2c.1t
	troff -man f2c.1t >f2c.t

#f2c.1: f2c.1t
#	nroff -man f2c.1t | col -b | uniq >f2c.1

clean:
	rm -f *.o f2c sysdep.hd tokdefs.h f2c.t

veryclean: clean
	rm -f xsum

b = Notice README cds.c data.c defines.h defs.h equiv.c error.c \
	exec.c expr.c f2c.1 f2c.1t f2c.h format.c format.h formatdata.c \
	ftypes.h gram.c gram.dcl gram.exec gram.expr gram.head gram.io \
	init.c intr.c io.c iob.h lex.c machdefs.h main.c makefile.u makefile.vc \
	malloc.c mem.c memset.c misc.c names.c names.h niceprintf.c \
	niceprintf.h output.c output.h p1defs.h p1output.c \
	parse.h parse_args.c pccdefs.h pread.c proc.c put.c putpcc.c \
	sysdep.c sysdep.h sysdeptest.c tokens usignal.h vax.c version.c xsum.c

xsum: xsum.c
	$(CC) $(CFLAGS) -o xsum xsum.c

#Check validity of transmitted source...
xsum.out: xsum $b
	./xsum $b >xsum1.out
	cmp xsum0.out xsum1.out && mv xsum1.out xsum.out

#On non-Unix systems that end lines with carriage-return/newline pairs,
#use "make xsumr.out" rather than "make xsum.out".  The -r flag ignores
#carriage-return characters.
xsumr.out: xsum $b
	./xsum -r $b >xsum1.out
	cmp xsum0.out xsum1.out && mv xsum1.out xsumr.out
