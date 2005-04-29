XCOMM $XFree86: xc/lib/font/X-TrueType/ISO8859.13/Imakefile.inc,v 1.1 2003/07/27 02:18:46 dawes Exp $

#define ModuleName ISO8859_13
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/ISO8859.13

SRCS =  main.c ISO8859_13toUCS2.c
OBJS =  main.o ISO8859_13toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(ISO8859_13toUCS2.c,$(MODULESRCDIR))
