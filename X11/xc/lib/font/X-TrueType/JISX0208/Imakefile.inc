XCOMM $XFree86: xc/lib/font/X-TrueType/JISX0208/Imakefile.inc,v 1.1 1999/03/28 15:31:58 dawes Exp $

#define ModuleName JISX0208
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c JISX0208toUCS2.c
OBJS =  main.o JISX0208toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(JISX0208toUCS2.c,$(MODULESRCDIR))
