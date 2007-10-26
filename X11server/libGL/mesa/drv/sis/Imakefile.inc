XCOMM $XFree86: xc/lib/GL/mesa/src/drv/sis/Imakefile.inc,v 1.3 2003/09/28 20:15:33 alanh Exp $

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
  DRI_DEFINES = GlxDefines $(SIS_DEFINES) -DX_BYTE_ORDER=$(X_BYTE_ORDER)
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri \
		-I$(GLXLIBSRC)/glx \
		-I$(INCLUDESRC) \
		-I$(INCLUDESRC)/GL \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86DRIVERSRC)/sis \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
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

      SISSRCS = $(MESADRVSISBUILDDIR)sis_alloc.c \
		$(MESADRVSISBUILDDIR)sis_dd.c \
		$(MESADRVSISBUILDDIR)sis_lock.c \
		$(MESADRVSISBUILDDIR)sis_context.c \
		$(MESADRVSISBUILDDIR)sis_fog.c \
		$(MESADRVSISBUILDDIR)sis_clear.c \
		$(MESADRVSISBUILDDIR)sis_screen.c \
		$(MESADRVSISBUILDDIR)sis_state.c \
		$(MESADRVSISBUILDDIR)sis_stencil.c \
		$(MESADRVSISBUILDDIR)sis_span.c \
		$(MESADRVSISBUILDDIR)sis_tex.c \
		$(MESADRVSISBUILDDIR)sis_texstate.c \
		$(MESADRVSISBUILDDIR)sis_tris.c \
		$(MESADRVSISBUILDDIR)sis_vb.c \
		$(DEBUG_SRC)

      SISOBJS = $(MESADRVSISBUILDDIR)sis_alloc.o \
		$(MESADRVSISBUILDDIR)sis_dd.o \
		$(MESADRVSISBUILDDIR)sis_lock.o \
		$(MESADRVSISBUILDDIR)sis_context.o \
		$(MESADRVSISBUILDDIR)sis_fog.o \
		$(MESADRVSISBUILDDIR)sis_clear.o \
		$(MESADRVSISBUILDDIR)sis_screen.o \
		$(MESADRVSISBUILDDIR)sis_state.o \
		$(MESADRVSISBUILDDIR)sis_stencil.o \
		$(MESADRVSISBUILDDIR)sis_span.o \
		$(MESADRVSISBUILDDIR)sis_tex.o \
		$(MESADRVSISBUILDDIR)sis_texstate.o \
		$(MESADRVSISBUILDDIR)sis_tris.o \
		$(MESADRVSISBUILDDIR)sis_vb.o \
		$(DEBUG_OBJ)

      SISUOBJS = $(MESADRVSISBUILDDIR)unshared/sis_alloc.o \
		$(MESADRVSISBUILDDIR)unshared/sis_dd.o \
		$(MESADRVSISBUILDDIR)unshared/sis_lock.o \
		$(MESADRVSISBUILDDIR)unshared/sis_context.o \
		$(MESADRVSISBUILDDIR)unshared/sis_fog.o \
		$(MESADRVSISBUILDDIR)unshared/sis_clear.o \
		$(MESADRVSISBUILDDIR)unshared/sis_screen.o \
		$(MESADRVSISBUILDDIR)unshared/sis_state.o \
		$(MESADRVSISBUILDDIR)unshared/sis_stencil.o \
		$(MESADRVSISBUILDDIR)unshared/sis_span.o \
		$(MESADRVSISBUILDDIR)unshared/sis_tex.o \
		$(MESADRVSISBUILDDIR)unshared/sis_texstate.o \
		$(MESADRVSISBUILDDIR)unshared/sis_tris.o \
		$(MESADRVSISBUILDDIR)unshared/sis_vb.o \
		$(DEBUG_UOBJ)

      SISDOBJS = $(MESADRVSISBUILDDIR)debugger/sis_alloc.o \
		$(MESADRVSISBUILDDIR)debugger/sis_dd.o \
		$(MESADRVSISBUILDDIR)debugger/sis_lock.o \
		$(MESADRVSISBUILDDIR)debugger/sis_context.o \
		$(MESADRVSISBUILDDIR)debugger/sis_fog.o \
		$(MESADRVSISBUILDDIR)debugger/sis_clear.o \
		$(MESADRVSISBUILDDIR)debugger/sis_screen.o \
		$(MESADRVSISBUILDDIR)debugger/sis_state.o \
		$(MESADRVSISBUILDDIR)debugger/sis_stencil.o \
		$(MESADRVSISBUILDDIR)debugger/sis_span.o \
		$(MESADRVSISBUILDDIR)debugger/sis_tex.o \
		$(MESADRVSISBUILDDIR)debugger/sis_texstate.o \
		$(MESADRVSISBUILDDIR)debugger/sis_tris.o \
		$(MESADRVSISBUILDDIR)debugger/sis_vb.o \
		$(DEBUG_DOBJ)

      SISPOBJS = $(MESADRVSISBUILDDIR)profiled/sis_alloc.o \
		$(MESADRVSISBUILDDIR)profiled/sis_dd.o \
		$(MESADRVSISBUILDDIR)profiled/sis_lock.o \
		$(MESADRVSISBUILDDIR)profiled/sis_context.o \
		$(MESADRVSISBUILDDIR)profiled/sis_fog.o \
		$(MESADRVSISBUILDDIR)profiled/sis_clear.o \
		$(MESADRVSISBUILDDIR)profiled/sis_screen.o \
		$(MESADRVSISBUILDDIR)profiled/sis_state.o \
		$(MESADRVSISBUILDDIR)profiled/sis_stencil.o \
		$(MESADRVSISBUILDDIR)profiled/sis_span.o \
		$(MESADRVSISBUILDDIR)profiled/sis_tex.o \
		$(MESADRVSISBUILDDIR)profiled/sis_texstate.o \
		$(MESADRVSISBUILDDIR)profiled/sis_tris.o \
		$(MESADRVSISBUILDDIR)profiled/sis_vb.o \
		$(DEBUG_POBJ)

#ifdef NeedToLinkMesaSrc
LinkSourceFile(sis_alloc.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_dd.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_lock.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_context.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_fog.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_clear.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_screen.c $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_state.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_stencil.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_span.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_tex.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_texstate.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_tris.c, $(MESADRVSRCDIR)/sis)
LinkSourceFile(sis_vb.c, $(MESADRVSRCDIR)/sis)
#endif

