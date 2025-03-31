# Microsoft Visual C++ Makefile for f2c, a Fortran 77 to C converter
# Invoke with "nmake -f makefile.vc", or execute the commands
#	copy makefile.vc makefile
#	nmake .

CC = cl
CFLAGS = -Ot1 -nologo -DNO_LONG_LONG

.c.obj:
	$(CC) -c $(CFLAGS) $*.c

OBJECTS = main.obj init.obj gram.obj lex.obj proc.obj equiv.obj data.obj format.obj \
	  expr.obj exec.obj intr.obj io.obj misc.obj error.obj mem.obj names.obj \
	  output.obj p1output.obj pread.obj put.obj putpcc.obj vax.obj formatdata.obj \
	  parse_args.obj niceprintf.obj cds.obj sysdep.obj version.obj

checkfirst: xsum.out

f2c.exe: $(OBJECTS)
	$(CC) -Fef2c.exe $(OBJECTS) setargv.obj

$(OBJECTS): defs.h ftypes.h defines.h machdefs.h sysdep.h

cds.obj: sysdep.h
exec.obj: p1defs.h names.h
expr.obj: output.h niceprintf.h names.h
format.obj: p1defs.h format.h output.h niceprintf.h names.h iob.h
formatdata.obj: format.h output.h niceprintf.h names.h
gram.obj: p1defs.h
init.obj: output.h niceprintf.h iob.h
intr.obj: names.h
io.obj: names.h iob.h
lex.obj : tokdefs.h p1defs.h
main.obj: parse.h usignal.h
mem.obj: iob.h
names.obj: iob.h names.h output.h niceprintf.h
niceprintf.obj: defs.h names.h output.h niceprintf.h
output.obj: output.h niceprintf.h names.h
p1output.obj: p1defs.h output.h niceprintf.h names.h
parse_args.obj: parse.h
proc.obj: tokdefs.h names.h niceprintf.h output.h p1defs.h
put.obj: names.h pccdefs.h p1defs.h
putpcc.obj: names.h
vax.obj: defs.h output.h pccdefs.h
output.h: niceprintf.h

put.obj putpcc.obj: pccdefs.h

clean:
	deltree /Y *.obj f2c.exe

veryclean: clean
	deltree /Y xsum.exe

b = Notice README cds.c data.c defines.h defs.h equiv.c error.c \
	exec.c expr.c f2c.1 f2c.1t f2c.h format.c format.h formatdata.c \
	ftypes.h gram.c gram.dcl gram.exec gram.expr gram.head gram.io \
	init.c intr.c io.c iob.h lex.c machdefs.h main.c makefile.u makefile.vc \
	malloc.c mem.c memset.c misc.c names.c names.h niceprintf.c \
	niceprintf.h output.c output.h p1defs.h p1output.c \
	parse.h parse_args.c pccdefs.h pread.c proc.c put.c putpcc.c \
	sysdep.c sysdep.h sysdeptest.c tokens usignal.h vax.c version.c xsum.c

xsum.exe: xsum.c
	$(CC) $(CFLAGS) -DMSDOS xsum.c

#Check validity of transmitted source...
# Unfortunately, conditional execution is hard here, since fc does not set a
# nonzero exit code when files differ.

xsum.out: xsum.exe $b
	xsum $b >xsum1.out
	fc xsum0.out xsum1.out
	@echo If fc showed no differences, manually rename xsum1.out xsum.out:
	@echo if xsum.out exists, first "del xsum.out"; then "ren xsum1.out xsum.out".
	@echo Once you are happy that your source is OK, "nmake -f makefile.vc f2c.exe".
