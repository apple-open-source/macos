XCOMM $XFree86: xc/lib/GL/mesa/src/drv/gamma/Imakefile.inc,v 1.4 2003/05/27 16:34:33 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvGammaBuildDir
#define MesaDrvGammaBuildDir /**/
#endif
MESADRVGAMMABUILDDIR = MesaDrvGammaBuildDir

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
		-I$(XF86DRIVERSRC)/glint \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/gamma
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)


    GAMMASRCS = $(MESADRVGAMMABUILDDIR)gamma_context.c \
		$(MESADRVGAMMABUILDDIR)gamma_dd.c \
		$(MESADRVGAMMABUILDDIR)gamma_inithw.c \
		$(MESADRVGAMMABUILDDIR)gamma_lock.c \
		$(MESADRVGAMMABUILDDIR)gamma_render.c \
		$(MESADRVGAMMABUILDDIR)gamma_screen.c \
		$(MESADRVGAMMABUILDDIR)gamma_span.c \
		$(MESADRVGAMMABUILDDIR)gamma_state.c \
		$(MESADRVGAMMABUILDDIR)gamma_tex.c \
		$(MESADRVGAMMABUILDDIR)gamma_texmem.c \
		$(MESADRVGAMMABUILDDIR)gamma_texstate.c \
		$(MESADRVGAMMABUILDDIR)gamma_tris.c \
		$(MESADRVGAMMABUILDDIR)gamma_vb.c \
		$(MESADRVGAMMABUILDDIR)gamma_xmesa.c

    GAMMAOBJS = $(MESADRVGAMMABUILDDIR)gamma_context.o \
		$(MESADRVGAMMABUILDDIR)gamma_dd.o \
		$(MESADRVGAMMABUILDDIR)gamma_inithw.o \
		$(MESADRVGAMMABUILDDIR)gamma_lock.o \
		$(MESADRVGAMMABUILDDIR)gamma_render.o \
		$(MESADRVGAMMABUILDDIR)gamma_screen.o \
		$(MESADRVGAMMABUILDDIR)gamma_span.o \
		$(MESADRVGAMMABUILDDIR)gamma_state.o \
		$(MESADRVGAMMABUILDDIR)gamma_tex.o \
		$(MESADRVGAMMABUILDDIR)gamma_texmem.o \
		$(MESADRVGAMMABUILDDIR)gamma_texstate.o \
		$(MESADRVGAMMABUILDDIR)gamma_tris.o \
		$(MESADRVGAMMABUILDDIR)gamma_vb.o \
		$(MESADRVGAMMABUILDDIR)gamma_xmesa.o

    GAMMAUOBJS = $(MESADRVGAMMABUILDDIR)unshared/gamma_context.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_dd.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_inithw.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_lock.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_render.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_screen.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_span.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_state.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_tex.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_texmem.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_texstate.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_tris.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_vb.o \
		$(MESADRVGAMMABUILDDIR)unshared/gamma_xmesa.o

    GAMMADOBJS = $(MESADRVGAMMABUILDDIR)debugger/gamma_context.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_dd.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_inithw.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_lock.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_render.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_screen.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_span.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_state.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_tex.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_texmem.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_texstate.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_tris.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_vb.o \
		$(MESADRVGAMMABUILDDIR)debugger/gamma_xmesa.o

    GAMMAPOBJS = $(MESADRVGAMMABUILDDIR)profiled/gamma_context.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_dd.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_inithw.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_lock.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_render.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_screen.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_span.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_state.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_tex.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_texmem.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_texstate.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_tris.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_vb.o \
		$(MESADRVGAMMABUILDDIR)profiled/gamma_xmesa.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(gamma_gl.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_xmesa.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_init.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_matrix.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_inithw.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_texture.c, $(MESADRVSRCDIR)/gamma)
LinkSourceFile(gamma_dlist.c, $(MESADRVSRCDIR)/gamma)
#endif
