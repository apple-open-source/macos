XCOMM $XFree86: xc/lib/GL/mesa/src/drv/common/Imakefile.inc,v 1.2 2001/05/02 15:06:03 dawes Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvCommonBuildDir
#define MesaDrvCommonBuildDir $(MESADRVSRCDIR)/common/
#endif
MESADRVCOMMONBUILDDIR = MesaDrvCommonBuildDir

#if Malloc0ReturnsNull
ALLOC_DEFINES = -DMALLOC_0_RETURNS_NULL
#endif

#if BuildXF86DRI
  DRI_DEFINES = GlxDefines
 DRI_INCLUDES = 
#endif

MESA_INCLUDES = -I$(MESADRVSRCDIR)/common
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

   COMMONSRCS = $(MESADRVCOMMONBUILDDIR)hwlog.c \
		$(MESADRVCOMMONBUILDDIR)mm.c

   COMMONOBJS = $(MESADRVCOMMONBUILDDIR)hwlog.o \
		$(MESADRVCOMMONBUILDDIR)mm.o

   COMMONUOBJS = $(MESADRVCOMMONBUILDDIR)unshared/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)unshared/mm.o

   COMMONDOBJS = $(MESADRVCOMMONBUILDDIR)debugger/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)debugger/mm.o

   COMMONPOBJS = $(MESADRVCOMMONBUILDDIR)profiled/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)profiled/mm.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(hwlog.c, $(MESADRVSRCDIR)/common)
LinkSourceFile(mm.c, $(MESADRVSRCDIR)/common)
#endif
