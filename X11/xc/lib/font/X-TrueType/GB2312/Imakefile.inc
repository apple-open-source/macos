XCOMM $XFree86: xc/lib/font/X-TrueType/GB2312/Imakefile.inc,v 1.1 1999/03/28 15:31:41 dawes Exp $

#define ModuleName GB2312
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c GB2312toUCS2.c
OBJS =  main.o GB2312toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(GB2312toUCS2.c,$(MODULESRCDIR))
