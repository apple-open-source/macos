XCOMM $XFree86: xc/lib/GL/mesa/src/drv/sis/Imakefile.inc,v 1.1 2001/03/23 19:18:43 dawes Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvSisBuildDir
#define MesaDrvSisBuildDir /**/
#endif
MESADRVSISBUILDDIR = MesaDrvSisBuildDir

#if Malloc0ReturnsNull
ALLOC_DEFINES = -DMALLOC_0_RETURNS_NULL
#endif

#if BuildXF86DRI
  DRI_DEFINES = GlxDefines $(SIS_DEFINES)
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri -I$(GLXLIBSRC)/glx \
		-I$(INCLUDESRC) -I$(INCLUDESRC)/GL \
		-I$(GLXLIBSRC)/mesa/dri \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86DRIVERSRC)/sis \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/mesa/src/X
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src -I$(MESASRCDIR)/include \
		-I$(MESADRVSRCDIR)/common -I$(MESADRVSRCDIR)/sis
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

#if 0
 DEBUG_SRC = $(MESADRVSISBUILDDIR)sis_debug.c
 DEBUG_OBJ = $(MESADRVSISBUILDDIR)sis_debug.o
 DEBUG_UOBJ = $(MESADRVSISBUILDDIR)unshared/sis_debug.o
 DEBUG_DOBJ = $(MESADRVSISBUILDDIR)debugger/sis_debug.o
 DEBUG_POBJ = $(MESADRVSISBUILDDIR)profiled/sis_debug.o
 DEBUG_DEFINES += -DSIS_DUMP
#ifdef NeedToLinkMesaSrc
LinkSourceFile(sis_debug.c, $(MESADRVSRCDIR)/sis)
#endif
#endif

#if 0
 STEREO_SRC = $(MESADRVSISBUILDDIR)sis_stereo.c
 STEREO_OBJ = $(MESADRVSISBUILDDIR)sis_stereo.o
 STEREO_UOBJ = $(MESADRVSISBUILDDIR)unshared/sis_stereo.o
 STEREO_DOBJ = $(MESADRVSISBUILDDIR)debugger/sis_stereo.o
 STEREO_POBJ = $(MESADRVSISBUILDDIR)profiled/sis_stereo.o
 STEREO_DEFINES = -DSIS_STEREO=1
#ifdef NeedToLinkMesaSrc
LinkSourceFile(sis_stereo.c, $(MESADRVSRCDIR)/sis)
#endif
#else
 STEREO_DEFINES = -DSIS_STEREO=0
#endif

      SISSRCS = $(MESADRVSISBUILDDIR)sis_render.c \
		$(MESADRVSISBUILDDIR)sis_mesa.c \
		$(MESADRVSISBUILDDIR)sis_ctx.c \
		$(MESADRVSISBUILDDIR)sis_alloc.c \
		$(MESADRVSISBUILDDIR)sis_fog.c \
		$(MESADRVSISBUILDDIR)sis_clear.c \
		$(MESADRVSISBUILDDIR)sis_stencil.c \
		$(MESADRVSISBUILDDIR)sis_texture.c \
		$(MESADRVSISBUILDDIR)sis_xwin.c \
		$(MESADRVSISBUILDDIR)sis_xmesa.c \
		$(MESADRVSISBUILDDIR)sis_span.c \
		$(MESADRVSISBUILDDIR)sis_fastpath.c \
		$(DEBUG_SRC) \
		$(STEREO_SRC)

      SISOBJS = $(MESADRVSISBUILDDIR)sis_render.o \
		$(MESADRVSISBUILDDIR)sis_mesa.o \
		$(MESADRVSISBUILDDIR)sis_ctx.o \
		$(MESADRVSISBUILDDIR)sis_alloc.o \
		$(MESADRVSISBUILDDIR)sis_fog.o \
		$(MESADRVSISBUILDDIR)sis_clear.o \
		$(MESADRVSISBUILDDIR)sis_stencil.o \
		$(MESADRVSISBUILDDIR)sis_texture.o \
		$(MESADRVSISBUILDDIR)sis_xwin.o \
		$(MESADRVSISBUILDDIR)sis_xmesa.o \
		$(MESADRVSISBUILDDIR)sis_span.o \
		$(MESADRVSISBUILDDIR)sis_fastpath.o \
		$(DEBUG_OBJ) \
		$(STEREO_OBJ)

      SISUOBJS = $(MESADRVSISBUILDDIR)unshared/sis_render.o \
		$(MESADRVSISBUILDDIR)unshared/sis_mesa.o \
		$(MESADRVSISBUILDDIR)unshared/sis_ctx.o \
		$(MESADRVSISBUILDDIR)unshared/sis_alloc.o \
		$(MESADRVSISBUILDDIR)unshared/sis_fog.o \
		$(MESADRVSISBUILDDIR)unshared/sis_clear.o \
		$(MESADRVSISBUILDDIR)unshared/sis_stencil.o \
		$(MESADRVSISBUILDDIR)unshared/sis_texture.o \
		$(MESADRVSISBUILDDIR)unshared/sis_xwin.o \
		$(MESADRVSISBUILDDIR)unshared/sis_xmesa.o \
		$(MESADRVSISBUILDDIR)unshared/sis_span.o \
		$(MESADRVSISBUILDDIR)unshared/sis_fastpath.o \
		$(DEBUG_UOBJ) \
		$(STEREO_UOBJ)

      SISDOBJS = $(MESADRVSISBUILDDIR)debugger/sis_render.o \
		$(MESADRVSISBUILDDIR)debugger/sis_mesa.o \
		$(MESADRVSISBUILDDIR)debugger/sis_ctx.o \
		$(MESADRVSISBUILDDIR)debugger/sis_alloc.o \
		$(MESADRVSISBUILDDIR)debugger/sis_fog.o \
		$(MESADRVSISBUILDDIR)debugger/sis_clear.o \
		$(MESADRVSISBUILDDIR)debugger/sis_stencil.o \
		$(MESADRVSISBUILDDIR)debugger/sis_texture.o \
		$(MESADRVSISBUILDDIR)debugger/sis_xwin.o \
		$(MESADRVSISBUILDDIR)debugger/sis_xmesa.o \
		$(MESADRVSISBUILDDIR)debugger/sis_span.o \
		$(MESADRVSISBUILDDIR)debugger/sis_fastpath.o \
		$(DEBUG_DOBJ) \
		$(STEREO_DOBJ)

      SISPOBJS = $(MESADRVSISBUILDDIR)profiled/sis_render.o \
		$(MESADRVSISBUILDDIR)profiled/sis_mesa.o \
		$(MESADRVSISBUILDDIR)profiled/sis_ctx.o \
		$(MESADRVSISBUILDDIR)profiled/sis_alloc.o \
		$(MESADRVSISBUILDDIR)profiled/sis_fog.o \
		$(MESADRVSISBUILDDIR)profiled/sis_clear.o \
		$(MESADRVSISBUILDDIR)profiled/sis_stencil.o \
		$(MESADRVSISBUILDDIR)profiled/sis_texture.o \
		$(MESADRVSISBUILDDIR)profiled/sis_xwin.o \
		$(MESADRVSISBUILDDIR)profiled/sis_xmesa.o \
		$(MESADRVSISBUILDDIR)profiled/sis_span.o \
		$(MESADRVSISBUILDDIR)profiled/sis_fastpath.o \
		$(DEBUG_POBJ) \
		$(STEREO_POBJ)

#ifdef NeedToLinkMesaSrc
LinkSourceFile(sis_render.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_mesa.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_ctx.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_alloc.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_fog.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_clear.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_stencil.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_texture.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_xwin.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_xmesa.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_span.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_fastpath.c, $(MESADRVSRCDIR)/sis)
#endif

