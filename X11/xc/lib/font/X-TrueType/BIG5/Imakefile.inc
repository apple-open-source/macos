XCOMM $XFree86: xc/lib/font/X-TrueType/BIG5/Imakefile.inc,v 1.1 1999/03/28 15:31:39 dawes Exp $

#define ModuleName BIG5
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c BIG5toUCS2.c
OBJS =  main.o BIG5toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(BIG5toUCS2.c,$(MODULESRCDIR))

