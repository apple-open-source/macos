XCOMM $XFree86: xc/lib/font/X-TrueType/JISX0212/Imakefile.inc,v 1.1 1999/03/28 15:31:59 dawes Exp $

#define ModuleName JISX0212
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c JISX0212toUCS2.c
OBJS =  main.o JISX0212toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(JISX0212toUCS2.c,$(MODULESRCDIR))
