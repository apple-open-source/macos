XCOMM $XFree86: xc/lib/font/X-TrueType/KSCJOHAB/Imakefile.inc,v 1.1 1999/03/28 15:32:02 dawes Exp $

#define ModuleName KSCJOHAB
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c KSCJOHABtoUCS2.c
OBJS =  main.o KSCJOHABtoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(KSCJOHABtoUCS2.c,$(MODULESRCDIR))
