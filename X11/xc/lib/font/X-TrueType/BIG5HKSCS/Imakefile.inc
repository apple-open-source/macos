XCOMM $XFree86: xc/lib/font/X-TrueType/BIG5HKSCS/Imakefile.inc,v 1.1 2001/03/06 18:54:44 dawes Exp $

#define ModuleName BIG5HKSCS
MODULENAME = ModuleName
MODULESRCDIR = $(XTTSOURCEDIR)/$(MODULENAME)

SRCS =  main.c BIG5HKSCStoUCS2.c
OBJS =  main.o BIG5HKSCStoUCS2.o

LinkSourceFile(main.c,$(MODULESRCDIR))
LinkSourceFile(BIG5HKSCStoUCS2.c,$(MODULESRCDIR))

