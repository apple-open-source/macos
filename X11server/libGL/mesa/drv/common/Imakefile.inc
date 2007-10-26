XCOMM $XFree86: xc/lib/GL/mesa/src/drv/common/Imakefile.inc,v 1.4 2003/09/28 20:15:07 alanh Exp $

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
  DRI_DEFINES = GlxDefines -DX_BYTE_ORDER=$(X_BYTE_ORDER)
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri \
		-I$(GLXLIBSRC)/glx \
		-I$(INCLUDESRC) \
		-I$(INCLUDESRC)/GL \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

   COMMONSRCS = $(MESADRVCOMMONBUILDDIR)hwlog.c \
		$(MESADRVCOMMONBUILDDIR)mm.c \
		$(MESADRVCOMMONBUILDDIR)utils.c \
		$(MESADRVCOMMONBUILDDIR)vblank.c \
		$(MESADRVCOMMONBUILDDIR)texmem.c

   COMMONOBJS = $(MESADRVCOMMONBUILDDIR)hwlog.o \
		$(MESADRVCOMMONBUILDDIR)mm.o \
		$(MESADRVCOMMONBUILDDIR)utils.o \
		$(MESADRVCOMMONBUILDDIR)vblank.o \
		$(MESADRVCOMMONBUILDDIR)texmem.o

   COMMONUOBJS = $(MESADRVCOMMONBUILDDIR)unshared/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)unshared/mm.o \
		$(MESADRVCOMMONBUILDDIR)unshared/utils.o \
		$(MESADRVCOMMONBUILDDIR)unshared/vblank.o \
		$(MESADRVCOMMONBUILDDIR)unshared/texmem.o

   COMMONDOBJS = $(MESADRVCOMMONBUILDDIR)debugger/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)debugger/mm.o \
		$(MESADRVCOMMONBUILDDIR)debugger/utils.o \
		$(MESADRVCOMMONBUILDDIR)debugger/vblank.o \
		$(MESADRVCOMMONBUILDDIR)debugger/texmem.o

   COMMONPOBJS = $(MESADRVCOMMONBUILDDIR)profiled/hwlog.o \
		$(MESADRVCOMMONBUILDDIR)profiled/mm.o \
		$(MESADRVCOMMONBUILDDIR)profiled/utils.o \
		$(MESADRVCOMMONBUILDDIR)profiled/vblank.o \
		$(MESADRVCOMMONBUILDDIR)profiled/texmem.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(hwlog.c, $(MESADRVSRCDIR)/common)
LinkSourceFile(mm.c, $(MESADRVSRCDIR)/common)
LinkSourceFile(texmem.c, $(MESADRVSRCDIR)/common)
LinkSourceFile(utils.c, $(MESADRVSRCDIR)/common)
LinkSourceFile(vblank.c, $(MESADRVSRCDIR)/common)
#endif
