XCOMM $XFree86: xc/lib/font/X-TrueType/GBK/Imakefile.inc,v 1.1 2001/03/06 18:03:16 dawes Exp $

#define ModuleName GBK
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c GBKtoUCS2.c
OBJS =  main.o GBKtoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(GBKtoUCS2.c,$(MODULESRCDIR))
