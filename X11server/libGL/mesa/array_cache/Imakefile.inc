XCOMM $XFree86: xc/lib/GL/mesa/src/array_cache/Imakefile.inc,v 1.2 2002/11/14 21:01:16 tsi Exp $

#ifndef MesaACBuildDir
#define MesaACBuildDir $(GLXLIBSRC)/mesa/src/array_cache/
#endif

MESAACBUILDDIR = MesaACBuildDir

  MESA_AC_SRCS = $(MESAACBUILDDIR)ac_context.c \
		 $(MESAACBUILDDIR)ac_import.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(ac_context.c, $(MESASRCDIR)/src/array_cache)
LinkSourceFile(ac_import.c, $(MESASRCDIR)/src/array_cache)
#endif

  MESA_AC_OBJS = $(MESAACBUILDDIR)ac_context.o \
		 $(MESAACBUILDDIR)ac_import.o

#if defined(DoSharedLib) && DoSharedLib
 MESA_AC_UOBJS = $(MESAACBUILDDIR)unshared/ac_context.o \
		 $(MESAACBUILDDIR)unshared/ac_import.o
#else
 MESA_AC_UOBJS = $(MESA_AC_OBJS)
#endif

 MESA_AC_DOBJS = $(MESAACBUILDDIR)debugger/ac_context.o \
		 $(MESAACBUILDDIR)debugger/ac_import.o

 MESA_AC_POBJS = $(MESAACBUILDDIR)profiled/ac_context.o \
		 $(MESAACBUILDDIR)profiled/ac_import.o

