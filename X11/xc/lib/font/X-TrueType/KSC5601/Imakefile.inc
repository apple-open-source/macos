XCOMM $XFree86: xc/lib/font/X-TrueType/KSC5601/Imakefile.inc,v 1.1 1999/03/28 15:32:01 dawes Exp $

#define ModuleName KSC5601
MODULENAME = ModuleName
MODULESRCDIR= $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c KSC5601toUCS2.c
OBJS =  main.o KSC5601toUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(KSC5601toUCS2.c,$(MODULESRCDIR))
