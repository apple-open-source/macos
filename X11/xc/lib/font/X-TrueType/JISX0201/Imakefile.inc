XCOMM $XFree86: xc/lib/font/X-TrueType/JISX0201/Imakefile.inc,v 1.1 1999/03/28 15:31:57 dawes Exp $

#define ModuleName JISX0201
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c JISX0201toUCS2.c
OBJS =  main.o JISX0201toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(JISX0201toUCS2.c,$(MODULESRCDIR))
