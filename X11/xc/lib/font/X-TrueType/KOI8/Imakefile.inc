XCOMM $XFree86: xc/lib/font/X-TrueType/KOI8/Imakefile.inc,v 1.1 1999/03/28 15:32:00 dawes Exp $

#define ModuleName KOI8
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c KOI8_RtoUCS2.c KOI8_UNItoUCS2.c
OBJS =  main.o KOI8_RtoUCS2.o KOI8_UNItoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(KOI8_RtoUCS2.c,$(MODULESRCDIR))
LinkSourceFile(KOI8_UNItoUCS2.c,$(MODULESRCDIR))
