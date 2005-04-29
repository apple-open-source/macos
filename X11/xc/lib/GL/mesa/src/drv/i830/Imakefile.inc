XCOMM $XFree86: xc/lib/GL/mesa/src/drv/i830/Imakefile.inc,v 1.3 2003/05/27 16:34:33 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvI830BuildDir
#define MesaDrvI830BuildDir /**/
#endif
MESADRVI830BUILDDIR = MesaDrvI830BuildDir

#if Malloc0ReturnsNull
ALLOC_DEFINES = -DMALLOC_0_RETURNS_NULL
#endif

#if BuildXF86DRI
  DRI_DEFINES = GlxDefines -DX_BYTE_ORDER=$(X_BYTE_ORDER)
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri \
		-I$(GLXLIBSRC)/glx \
		-I$(INCLUDESRC) \
		-I$(INCLUDESRC)/GL \
		-I$(GLXLIBSRC)/dri \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86DRIVERSRC)/i810 \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src -I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/i830
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

     I830SRCS = $(MESADRVI830BUILDDIR)i830_context.c \
		$(MESADRVI830BUILDDIR)i830_debug.c \
		$(MESADRVI830BUILDDIR)i830_ioctl.c \
		$(MESADRVI830BUILDDIR)i830_render.c \
		$(MESADRVI830BUILDDIR)i830_screen.c \
		$(MESADRVI830BUILDDIR)i830_span.c \
		$(MESADRVI830BUILDDIR)i830_state.c \
		$(MESADRVI830BUILDDIR)i830_tex.c \
		$(MESADRVI830BUILDDIR)i830_texmem.c \
		$(MESADRVI830BUILDDIR)i830_texstate.c \
		$(MESADRVI830BUILDDIR)i830_tris.c \
		$(MESADRVI830BUILDDIR)i830_vb.c

     I830OBJS = $(MESADRVI830BUILDDIR)i830_context.o \
		$(MESADRVI830BUILDDIR)i830_debug.o \
		$(MESADRVI830BUILDDIR)i830_ioctl.o \
		$(MESADRVI830BUILDDIR)i830_render.o \
		$(MESADRVI830BUILDDIR)i830_screen.o \
		$(MESADRVI830BUILDDIR)i830_span.o \
		$(MESADRVI830BUILDDIR)i830_state.o \
		$(MESADRVI830BUILDDIR)i830_tex.o \
		$(MESADRVI830BUILDDIR)i830_texmem.o \
		$(MESADRVI830BUILDDIR)i830_texstate.o \
		$(MESADRVI830BUILDDIR)i830_tris.o \
		$(MESADRVI830BUILDDIR)i830_vb.o

    I830UOBJS = $(MESADRVI830BUILDDIR)unshared/i830_context.o \
		$(MESADRVI830BUILDDIR)unshared/i830_debug.o \
		$(MESADRVI830BUILDDIR)unshared/i830_ioctl.o \
		$(MESADRVI830BUILDDIR)unshared/i830_render.o \
		$(MESADRVI830BUILDDIR)unshared/i830_screen.o \
		$(MESADRVI830BUILDDIR)unshared/i830_span.o \
		$(MESADRVI830BUILDDIR)unshared/i830_state.o \
		$(MESADRVI830BUILDDIR)unshared/i830_tex.o \
		$(MESADRVI830BUILDDIR)unshared/i830_texmem.o \
		$(MESADRVI830BUILDDIR)unshared/i830_texstate.o \
		$(MESADRVI830BUILDDIR)unshared/i830_tris.o \
		$(MESADRVI830BUILDDIR)unshared/i830_vb.o

    I830DOBJS = $(MESADRVI830BUILDDIR)debugger/i830_context.o \
		$(MESADRVI830BUILDDIR)debugger/i830_debug.o \
		$(MESADRVI830BUILDDIR)debugger/i830_ioctl.o \
		$(MESADRVI830BUILDDIR)debugger/i830_render.o \
		$(MESADRVI830BUILDDIR)debugger/i830_screen.o \
		$(MESADRVI830BUILDDIR)debugger/i830_span.o \
		$(MESADRVI830BUILDDIR)debugger/i830_state.o \
		$(MESADRVI830BUILDDIR)debugger/i830_tex.o \
		$(MESADRVI830BUILDDIR)debugger/i830_texmem.o \
		$(MESADRVI830BUILDDIR)debugger/i830_texstate.o \
		$(MESADRVI830BUILDDIR)debugger/i830_tris.o \
		$(MESADRVI830BUILDDIR)debugger/i830_vb.o

    I830POBJS = $(MESADRVI830BUILDDIR)profiled/i830_context.o \
		$(MESADRVI830BUILDDIR)profiled/i830_debug.o \
		$(MESADRVI830BUILDDIR)profiled/i830_ioctl.o \
		$(MESADRVI830BUILDDIR)profiled/i830_render.o \
		$(MESADRVI830BUILDDIR)profiled/i830_screen.o \
		$(MESADRVI830BUILDDIR)profiled/i830_span.o \
		$(MESADRVI830BUILDDIR)profiled/i830_state.o \
		$(MESADRVI830BUILDDIR)profiled/i830_tex.o \
		$(MESADRVI830BUILDDIR)profiled/i830_texmem.o \
		$(MESADRVI830BUILDDIR)profiled/i830_texstate.o \
		$(MESADRVI830BUILDDIR)profiled/i830_tris.o \
		$(MESADRVI830BUILDDIR)profiled/i830_vb.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(i830_context.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_debug.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_ioctl.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_render.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_screen.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_span.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_state.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_tex.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_texmem.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_texstate.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_tris.c, $(MESADRVSRCDIR)/i830)
LinkSourceFile(i830_vb.c, $(MESADRVSRCDIR)/i830)
#endif

