XCOMM $XFree86: xc/lib/font/X-TrueType/ISO8859.15/Imakefile.inc,v 1.1 1999/03/28 15:31:48 dawes Exp $

#define ModuleName ISO8859_15
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/ISO8859.15

SRCS =  main.c
OBJS =  main.o

LinkSourceFile(main.c,$(MODULESRCDIR))
