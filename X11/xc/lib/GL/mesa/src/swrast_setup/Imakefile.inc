XCOMM $XFree86: xc/lib/GL/mesa/src/swrast_setup/Imakefile.inc,v 1.2 2002/11/14 21:01:17 tsi Exp $

#ifndef MesaSwrastSetupBuildDir
#define MesaSwrastSetupBuildDir $(GLXLIBSRC)/mesa/src/swrast_setup/
#endif

MESASWRSETUPDIR = MesaSwrastSetupBuildDir

 MESA_SWR_SETUP_SRCS = $(MESASWRSETUPDIR)ss_context.c \
		       $(MESASWRSETUPDIR)ss_triangle.c \
		       $(MESASWRSETUPDIR)ss_vb.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(ss_context.c, $(MESASRCDIR)/src/swrast_setup)
LinkSourceFile(ss_triangle.c, $(MESASRCDIR)/src/swrast_setup)
LinkSourceFile(ss_vb.c, $(MESASRCDIR)/src/swrast_setup)
#endif

 MESA_SWR_SETUP_OBJS = $(MESASWRSETUPDIR)ss_context.o \
		       $(MESASWRSETUPDIR)ss_triangle.o \
		       $(MESASWRSETUPDIR)ss_vb.o

#if defined(DoSharedLib) && DoSharedLib
MESA_SWR_SETUP_UOBJS = $(MESASWRSETUPDIR)unshared/ss_context.o \
		       $(MESASWRSETUPDIR)unshared/ss_triangle.o \
		       $(MESASWRSETUPDIR)unshared/ss_vb.o
#else
MESA_SWR_SETUP_UOBJS = $(MESA_SWR_SETUP_OBJS)
#endif

MESA_SWR_SETUP_DOBJS = $(MESASWRSETUPDIR)debugger/ss_context.o \
		       $(MESASWRSETUPDIR)debugger/ss_triangle.o \
		       $(MESASWRSETUPDIR)debugger/ss_vb.o

MESA_SWR_SETUP_POBJS = $(MESASWRSETUPDIR)profiled/ss_context.o \
		       $(MESASWRSETUPDIR)profiled/ss_triangle.o \
		       $(MESASWRSETUPDIR)profiled/ss_vb.o

