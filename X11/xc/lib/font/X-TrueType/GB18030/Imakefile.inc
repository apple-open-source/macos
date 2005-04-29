XCOMM $XFree86: xc/lib/font/X-TrueType/GB18030/Imakefile.inc,v 1.2 2003/10/24 16:33:08 tsi Exp $

#define ModuleName GB18030
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c GB18030toUCS2.c
OBJS =  main.o GB18030toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(GB18030toUCS2.c,$(MODULESRCDIR))
