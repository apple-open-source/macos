XCOMM $XFree86: xc/lib/font/X-TrueType/VISCII/Imakefile.inc,v 1.1 1999/03/28 15:32:05 dawes Exp $

#define ModuleName VISCII
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c VISCIItoUCS2.c
OBJS =  main.o VISCIItoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(VISCIItoUCS2.c,$(MODULESRCDIR))
