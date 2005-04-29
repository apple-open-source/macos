XCOMM $XFree86: xc/lib/GL/mesa/src/drv/r200/Imakefile.inc,v 1.3 2003/05/27 16:34:34 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvR200BuildDir
#define MesaDrvR200BuildDir /**/
#endif
MESADRVR200BUILDDIR = MesaDrvR200BuildDir

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
		-I$(XF86DRIVERSRC)/ati \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include \
		-I$(XTOP)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/r200
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

   R200SRCS = $(MESADRVR200BUILDDIR)r200_context.c \
		$(MESADRVR200BUILDDIR)r200_sanity.c \
		$(MESADRVR200BUILDDIR)r200_state.c \
		$(MESADRVR200BUILDDIR)r200_state_init.c \
		$(MESADRVR200BUILDDIR)r200_swtcl.c \
		$(MESADRVR200BUILDDIR)r200_cmdbuf.c \
		$(MESADRVR200BUILDDIR)r200_ioctl.c \
		$(MESADRVR200BUILDDIR)r200_lock.c \
		$(MESADRVR200BUILDDIR)r200_maos.c \
		$(MESADRVR200BUILDDIR)r200_pixel.c \
	        $(MESADRVR200BUILDDIR)r200_screen.c \
		$(MESADRVR200BUILDDIR)r200_span.c \
		$(MESADRVR200BUILDDIR)r200_tcl.c \
		$(MESADRVR200BUILDDIR)r200_tex.c \
		$(MESADRVR200BUILDDIR)r200_texmem.c \
		$(MESADRVR200BUILDDIR)r200_texstate.c \
		$(MESADRVR200BUILDDIR)r200_vtxfmt.c \
		$(MESADRVR200BUILDDIR)r200_vtxfmt_x86.c \
		$(MESADRVR200BUILDDIR)r200_vtxtmp_x86.S \
		$(MESADRVR200BUILDDIR)r200_vtxfmt_sse.c \
		$(MESADRVR200BUILDDIR)r200_vtxfmt_c.c

   R200OBJS = $(MESADRVR200BUILDDIR)r200_context.o \
		$(MESADRVR200BUILDDIR)r200_sanity.o \
		$(MESADRVR200BUILDDIR)r200_state.o \
		$(MESADRVR200BUILDDIR)r200_state_init.o \
		$(MESADRVR200BUILDDIR)r200_swtcl.o \
		$(MESADRVR200BUILDDIR)r200_cmdbuf.o \
		$(MESADRVR200BUILDDIR)r200_ioctl.o \
		$(MESADRVR200BUILDDIR)r200_lock.o \
		$(MESADRVR200BUILDDIR)r200_maos.o \
		$(MESADRVR200BUILDDIR)r200_pixel.o \
	        $(MESADRVR200BUILDDIR)r200_screen.o \
		$(MESADRVR200BUILDDIR)r200_span.o \
		$(MESADRVR200BUILDDIR)r200_tcl.o \
		$(MESADRVR200BUILDDIR)r200_tex.o \
		$(MESADRVR200BUILDDIR)r200_texmem.o \
		$(MESADRVR200BUILDDIR)r200_texstate.o \
		$(MESADRVR200BUILDDIR)r200_vtxfmt.o \
		$(MESADRVR200BUILDDIR)r200_vtxfmt_c.o

#ifdef i386Architecture
   R200OBJS +=	$(MESADRVR200BUILDDIR)r200_vtxfmt_x86.o \
		$(MESADRVR200BUILDDIR)r200_vtxtmp_x86.o \
		$(MESADRVR200BUILDDIR)r200_vtxfmt_sse.o 
#endif

   R200UOBJS = $(MESADRVR200BUILDDIR)unshared/r200_context.o \
		$(MESADRVR200BUILDDIR)unshared/r200_sanity.o \
		$(MESADRVR200BUILDDIR)unshared/r200_cmdbuf.o \
		$(MESADRVR200BUILDDIR)unshared/r200_ioctl.o \
		$(MESADRVR200BUILDDIR)unshared/r200_lock.o \
		$(MESADRVR200BUILDDIR)unshared/r200_maos.o \
		$(MESADRVR200BUILDDIR)unshared/r200_pixel.o \
	        $(MESADRVR200BUILDDIR)unshared/r200_screen.o \
		$(MESADRVR200BUILDDIR)unshared/r200_span.o \
		$(MESADRVR200BUILDDIR)unshared/r200_state.o \
		$(MESADRVR200BUILDDIR)unshared/r200_state_init.o \
		$(MESADRVR200BUILDDIR)unshared/r200_swtcl.o \
		$(MESADRVR200BUILDDIR)unshared/r200_tcl.o \
		$(MESADRVR200BUILDDIR)unshared/r200_tex.o \
		$(MESADRVR200BUILDDIR)unshared/r200_texmem.o \
		$(MESADRVR200BUILDDIR)unshared/r200_texstate.o \
		$(MESADRVR200BUILDDIR)unshared/r200_vtxfmt.o \
		$(MESADRVR200BUILDDIR)unshared/r200_vtxfmt_c.o

#ifdef i386Architecture
   R200UOBJS +=	$(MESADRVR200BUILDDIR)unshared/r200_vtxfmt_x86.o \
		$(MESADRVR200BUILDDIR)unshared/r200_vtxtmp_x86.o \
		$(MESADRVR200BUILDDIR)unshared/r200_vtxfmt_sse.o 
#endif

   R200DOBJS = $(MESADRVR200BUILDDIR)debugger/r200_context.o \
		$(MESADRVR200BUILDDIR)debugger/r200_sanity.o \
		$(MESADRVR200BUILDDIR)debugger/r200_cmdbuf.o \
		$(MESADRVR200BUILDDIR)debugger/r200_ioctl.o \
		$(MESADRVR200BUILDDIR)debugger/r200_lock.o \
		$(MESADRVR200BUILDDIR)debugger/r200_maos.o \
		$(MESADRVR200BUILDDIR)debugger/r200_pixel.o \
	        $(MESADRVR200BUILDDIR)debugger/r200_screen.o \
		$(MESADRVR200BUILDDIR)debugger/r200_span.o \
		$(MESADRVR200BUILDDIR)debugger/r200_state.o \
		$(MESADRVR200BUILDDIR)debugger/r200_state_init.o \
		$(MESADRVR200BUILDDIR)debugger/r200_swtcl.o \
		$(MESADRVR200BUILDDIR)debugger/r200_tcl.o \
		$(MESADRVR200BUILDDIR)debugger/r200_tex.o \
		$(MESADRVR200BUILDDIR)debugger/r200_texmem.o \
		$(MESADRVR200BUILDDIR)debugger/r200_texstate.o \
		$(MESADRVR200BUILDDIR)debugger/r200_vtxfmt.o \
		$(MESADRVR200BUILDDIR)debugger/r200_vtxfmt_c.o

#ifdef i386Architecture
   R200DOBJS +=	$(MESADRVR200BUILDDIR)debugger/r200_vtxfmt_x86.o \
		$(MESADRVR200BUILDDIR)debugger/r200_vtxtmp_x86.o \
		$(MESADRVR200BUILDDIR)debugger/r200_vtxfmt_sse.o 
#endif

   R200POBJS = $(MESADRVR200BUILDDIR)profiled/r200_context.o \
		$(MESADRVR200BUILDDIR)profiled/r200_sanity.o \
		$(MESADRVR200BUILDDIR)profiled/r200_cmdbuf.o \
		$(MESADRVR200BUILDDIR)profiled/r200_ioctl.o \
		$(MESADRVR200BUILDDIR)profiled/r200_lock.o \
		$(MESADRVR200BUILDDIR)profiled/r200_maos.o \
		$(MESADRVR200BUILDDIR)profiled/r200_pixel.o \
	        $(MESADRVR200BUILDDIR)profiled/r200_screen.o \
		$(MESADRVR200BUILDDIR)profiled/r200_span.o \
		$(MESADRVR200BUILDDIR)profiled/r200_state.o \
		$(MESADRVR200BUILDDIR)profiled/r200_state_init.o \
		$(MESADRVR200BUILDDIR)profiled/r200_swtcl.o \
		$(MESADRVR200BUILDDIR)profiled/r200_tcl.o \
		$(MESADRVR200BUILDDIR)profiled/r200_tex.o \
		$(MESADRVR200BUILDDIR)profiled/r200_texmem.o \
		$(MESADRVR200BUILDDIR)profiled/r200_texstate.o \
		$(MESADRVR200BUILDDIR)profiled/r200_vtxfmt.o \
		$(MESADRVR200BUILDDIR)profiled/r200_vtxfmt_c.o

#ifdef i386Architecture
   R200POBJS +=	$(MESADRVR200BUILDDIR)profiled/r200_vtxfmt_x86.o \
		$(MESADRVR200BUILDDIR)profiled/r200_vtxtmp_x86.o \
		$(MESADRVR200BUILDDIR)profiled/r200_vtxfmt_sse.o 
#endif

#ifdef NeedToLinkMesaSrc
LinkSourceFile(r200_context.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_cmdbuf.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_ioctl.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_lock.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_maos.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_pixel.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_screen.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_span.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_sanity.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_state.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_state_init.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_swtcl.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_tcl.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_tex.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_texmem.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_texstate.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_vtxfmt.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_vtxfmt_c.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_vtxfmt_x86.c, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_vtxtmp_x86.S, $(MESADRVSRCDIR)/r200)
LinkSourceFile(r200_vtxfmt_sse.c, $(MESADRVSRCDIR)/r200)
#endif
