XCOMM $XFree86: xc/lib/GL/mesa/src/drv/tdfx/Imakefile.inc,v 1.4 2002/02/22 21:45:03 dawes Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvTdfxBuildDir
#define MesaDrvTdfxBuildDir /**/
#endif
MESADRVTDFXBUILDDIR = MesaDrvTdfxBuildDir

#if Malloc0ReturnsNull
ALLOC_DEFINES = -DMALLOC_0_RETURNS_NULL
#endif

#if BuildXF86DRI
  DRI_DEFINES = GlxDefines -DX_BYTE_ORDER=ByteOrder
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri \
		-I$(GLXLIBSRC)/glx \
		-I$(INCLUDESRC) \
		-I$(INCLUDESRC)/GL \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86DRIVERSRC)/tdfx \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/tdfx
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

     TDFXSRCS = $(MESADRVTDFXBUILDDIR)tdfx_context.c \
		$(MESADRVTDFXBUILDDIR)tdfx_dd.c \
		$(MESADRVTDFXBUILDDIR)tdfx_lock.c \
		$(MESADRVTDFXBUILDDIR)tdfx_pixels.c \
		$(MESADRVTDFXBUILDDIR)tdfx_render.c \
		$(MESADRVTDFXBUILDDIR)tdfx_screen.c \
		$(MESADRVTDFXBUILDDIR)tdfx_span.c \
		$(MESADRVTDFXBUILDDIR)tdfx_state.c \
		$(MESADRVTDFXBUILDDIR)tdfx_tex.c \
		$(MESADRVTDFXBUILDDIR)tdfx_texman.c \
		$(MESADRVTDFXBUILDDIR)tdfx_texstate.c \
		$(MESADRVTDFXBUILDDIR)tdfx_tris.c \
		$(MESADRVTDFXBUILDDIR)tdfx_vb.c

     TDFXOBJS = $(MESADRVTDFXBUILDDIR)tdfx_context.o \
		$(MESADRVTDFXBUILDDIR)tdfx_dd.o \
		$(MESADRVTDFXBUILDDIR)tdfx_lock.o \
		$(MESADRVTDFXBUILDDIR)tdfx_pixels.o \
		$(MESADRVTDFXBUILDDIR)tdfx_render.o \
		$(MESADRVTDFXBUILDDIR)tdfx_screen.o \
		$(MESADRVTDFXBUILDDIR)tdfx_span.o \
		$(MESADRVTDFXBUILDDIR)tdfx_state.o \
		$(MESADRVTDFXBUILDDIR)tdfx_tex.o \
		$(MESADRVTDFXBUILDDIR)tdfx_texman.o \
		$(MESADRVTDFXBUILDDIR)tdfx_texstate.o \
		$(MESADRVTDFXBUILDDIR)tdfx_tris.o \
		$(MESADRVTDFXBUILDDIR)tdfx_vb.o

     TDFXUOBJS = $(MESADRVTDFXBUILDDIR)unshared/tdfx_context.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_dd.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_lock.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_pixels.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_render.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_screen.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_span.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_state.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_tex.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_texman.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_texstate.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_tris.o \
		$(MESADRVTDFXBUILDDIR)unshared/tdfx_vb.o

     TDFXDOBJS = $(MESADRVTDFXBUILDDIR)debugger/tdfx_context.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_dd.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_lock.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_pixels.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_render.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_screen.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_span.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_state.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_tex.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_texman.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_texstate.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_tris.o \
		$(MESADRVTDFXBUILDDIR)debugger/tdfx_vb.o

     TDFXPOBJS = $(MESADRVTDFXBUILDDIR)profiled/tdfx_context.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_dd.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_lock.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_pixels.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_render.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_screen.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_span.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_state.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_tex.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_texman.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_texstate.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_tris.o \
		$(MESADRVTDFXBUILDDIR)profiled/tdfx_vb.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(tdfx_context.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_dd.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_lock.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_pixels.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_render.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_screen.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_span.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_state.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_tex.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_texman.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_texstate.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_tris.c, $(MESADRVSRCDIR)/tdfx)
LinkSourceFile(tdfx_vb.c, $(MESADRVSRCDIR)/tdfx)
#endif
