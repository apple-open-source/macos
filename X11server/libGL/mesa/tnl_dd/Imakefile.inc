XCOMM $XFree86: xc/lib/GL/mesa/src/tnl_dd/Imakefile.inc,v 1.2 2002/11/14 21:01:18 tsi Exp $

#ifndef MesaTnlDdBuildDir
#define MesaTnlDdBuildDir $(GLXLIBSRC)/mesa/src/tnl_dd/
#endif

MESATNLDDBUILDDIR = MesaTnlDdBuildDir

 MESA_TNL_DD_SRCS = $(MESATNLDDBUILDDIR)t_array_api.c \
		    $(MESATNLDDBUILDDIR)t_dd.c \
		    $(MESATNLDDBUILDDIR)t_dd_vb.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(t_array_api.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_dd.c, $(MESASRCDIR)/src/tnl_dd)
LinkSourceFile(t_dd_vb.c, $(MESASRCDIR)/src/tnl_dd)
#endif

 MESA_TNL_DD_OBJS = $(MESATNLDDBUILDDIR)t_array_api.o \
		    $(MESATNLDDBUILDDIR)t_dd.o \
		    $(MESATNLDDBUILDDIR)t_dd_vb.o

#if defined(DoSharedLib) && DoSharedLib
MESA_TNL_DD_UOBJS = $(MESATNLDDBUILDDIR)unshared/t_array_api.o \
		    $(MESATNLDDBUILDDIR)unshared/t_dd.o \
		    $(MESATNLDDBUILDDIR)unshared/t_dd_vb.o
#else
MESA_TNL_DD_UOBJS = $(MESA_TNL_DD_OBJS)
#endif

MESA_TNL_DD_DOBJS = $(MESATNLDDBUILDDIR)debugger/t_array_api.o \
		    $(MESATNLDDBUILDDIR)debugger/t_dd.o \
		    $(MESATNLDDBUILDDIR)debugger/t_dd_vb.o

MESA_TNL_DD_POBJS = $(MESATNLDDBUILDDIR)profiled/t_array_api.o \
		    $(MESATNLDDBUILDDIR)profiled/t_dd.o \
		    $(MESATNLDDBUILDDIR)profiled/t_dd_vb.o

