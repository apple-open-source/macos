XCOMM $XFree86: xc/lib/font/X-TrueType/ISO8859.10/Imakefile.inc,v 1.1 1999/03/28 15:31:44 dawes Exp $

#define ModuleName ISO8859_10
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/ISO8859.10

SRCS =  main.c ISO8859_10toUCS2.c
OBJS =  main.o ISO8859_10toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(ISO8859_10toUCS2.c,$(MODULESRCDIR))
