XCOMM $XFree86: xc/lib/font/X-TrueType/GEORGIAN/Imakefile.inc,v 1.1 2000/02/13 06:15:26 dawes Exp $

#define ModuleName GEORGIAN
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c GEORGIANtoUCS2.c
OBJS =  main.o GEORGIANtoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(GEORGIANtoUCS2.c,$(MODULESRCDIR))

