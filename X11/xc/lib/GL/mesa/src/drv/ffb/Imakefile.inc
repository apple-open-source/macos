XCOMM $XFree86: xc/lib/GL/mesa/src/drv/ffb/Imakefile.inc,v 1.4 2002/02/26 23:37:33 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvFfbBuildDir
#define MesaDrvFfbBuildDir /**/
#endif
MESADRVFFBBUILDDIR = MesaDrvFfbBuildDir

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
		-I$(XF86DRIVERSRC)/ati \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/ffb
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

   FFBSRCS    = $(MESADRVFFBBUILDDIR)ffb_bitmap.c \
		$(MESADRVFFBBUILDDIR)ffb_clear.c \
		$(MESADRVFFBBUILDDIR)ffb_dd.c \
		$(MESADRVFFBBUILDDIR)ffb_depth.c \
		/* $(MESADRVFFBBUILDDIR)ffb_fog.c */ \
		$(MESADRVFFBBUILDDIR)ffb_lines.c \
		$(MESADRVFFBBUILDDIR)ffb_points.c \
		$(MESADRVFFBBUILDDIR)ffb_span.c \
		$(MESADRVFFBBUILDDIR)ffb_state.c \
		$(MESADRVFFBBUILDDIR)ffb_stencil.c \
		$(MESADRVFFBBUILDDIR)ffb_tex.c \
		$(MESADRVFFBBUILDDIR)ffb_tris.c \
		$(MESADRVFFBBUILDDIR)ffb_vb.c \
		$(MESADRVFFBBUILDDIR)ffb_xmesa.c

   FFBOBJS    = $(MESADRVFFBBUILDDIR)ffb_bitmap.o \
		$(MESADRVFFBBUILDDIR)ffb_clear.o \
		$(MESADRVFFBBUILDDIR)ffb_dd.o \
		$(MESADRVFFBBUILDDIR)ffb_depth.o \
		/* $(MESADRVFFBBUILDDIR)ffb_fog.o */ \
		$(MESADRVFFBBUILDDIR)ffb_lines.o \
		$(MESADRVFFBBUILDDIR)ffb_points.o \
		$(MESADRVFFBBUILDDIR)ffb_span.o \
		$(MESADRVFFBBUILDDIR)ffb_state.o \
		$(MESADRVFFBBUILDDIR)ffb_stencil.o \
		$(MESADRVFFBBUILDDIR)ffb_tex.o \
		$(MESADRVFFBBUILDDIR)ffb_tris.o \
		$(MESADRVFFBBUILDDIR)ffb_vb.o \
		$(MESADRVFFBBUILDDIR)ffb_xmesa.o

   FFBUOBJS   = $(MESADRVFFBBUILDDIR)unshared/ffb_bitmap.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_clear.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_dd.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_depth.o \
		/* $(MESADRVFFBBUILDDIR)unshared/ffb_fog.o */ \
		$(MESADRVFFBBUILDDIR)unshared/ffb_lines.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_points.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_span.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_state.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_stencil.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_tex.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_tris.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_vb.o \
		$(MESADRVFFBBUILDDIR)unshared/ffb_xmesa.o

   FFBDOBJS   = $(MESADRVFFBBUILDDIR)debugger/ffb_bitmap.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_clear.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_dd.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_depth.o \
		/* $(MESADRVFFBBUILDDIR)debugger/ffb_fog.o */ \
		$(MESADRVFFBBUILDDIR)debugger/ffb_lines.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_points.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_span.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_state.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_stencil.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_tex.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_tris.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_vb.o \
		$(MESADRVFFBBUILDDIR)debugger/ffb_xmesa.o

   FFBPOBJS   = $(MESADRVFFBBUILDDIR)profiled/ffb_bitmap.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_clear.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_dd.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_depth.o \
		/* $(MESADRVFFBBUILDDIR)profiled/ffb_fog.o */ \
		$(MESADRVFFBBUILDDIR)profiled/ffb_lines.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_points.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_span.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_state.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_stencil.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_tex.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_tris.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_vb.o \
		$(MESADRVFFBBUILDDIR)profiled/ffb_xmesa.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(ffb_bitmap.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_clear.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_dd.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_depth.c, $(MESADRVSRCDIR)/ffb)
/* LinkSourceFile(ffb_fog.c, $(MESADRVSRCDIR)/ffb) */
LinkSourceFile(ffb_lines.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_points.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_span.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_state.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_stencil.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_tex.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_tris.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_vb.c, $(MESADRVSRCDIR)/ffb)
LinkSourceFile(ffb_xmesa.c, $(MESADRVSRCDIR)/ffb)
#endif
