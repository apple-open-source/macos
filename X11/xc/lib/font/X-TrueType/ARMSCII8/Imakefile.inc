XCOMM $XFree86: xc/lib/font/X-TrueType/ARMSCII8/Imakefile.inc,v 1.1 1999/03/28 15:31:38 dawes Exp $

#define ModuleName ARMSCII8
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =	main.c ARMSCII8toUCS2.c
OBJS =	main.o ARMSCII8toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(ARMSCII8toUCS2.c,$(MODULESRCDIR))
