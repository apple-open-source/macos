XCOMM $XFree86: xc/lib/font/X-TrueType/TCVN/Imakefile.inc,v 1.1 1999/03/28 15:32:05 dawes Exp $

#define ModuleName TCVN
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c TCVNtoUCS2.c
OBJS =  main.o TCVNtoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(TCVNtoUCS2.c,$(MODULESRCDIR))
