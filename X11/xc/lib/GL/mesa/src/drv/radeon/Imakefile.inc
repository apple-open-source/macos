XCOMM $XFree86: xc/lib/GL/mesa/src/drv/radeon/Imakefile.inc,v 1.4 2002/10/30 12:51:54 alanh Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvRadeonBuildDir
#define MesaDrvRadeonBuildDir /**/
#endif
MESADRVRADEONBUILDDIR = MesaDrvRadeonBuildDir

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
		-I$(GLXLIBSRC)/include \
		-I$(XTOP)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/radeon
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

   RADEONSRCS = $(MESADRVRADEONBUILDDIR)radeon_context.c \
		$(MESADRVRADEONBUILDDIR)radeon_compat.c \
		$(MESADRVRADEONBUILDDIR)radeon_ioctl.c \
		$(MESADRVRADEONBUILDDIR)radeon_lock.c \
		$(MESADRVRADEONBUILDDIR)radeon_maos.c \
	        $(MESADRVRADEONBUILDDIR)radeon_sanity.c \
	        $(MESADRVRADEONBUILDDIR)radeon_screen.c \
		$(MESADRVRADEONBUILDDIR)radeon_span.c \
		$(MESADRVRADEONBUILDDIR)radeon_state.c \
		$(MESADRVRADEONBUILDDIR)radeon_state_init.c \
		$(MESADRVRADEONBUILDDIR)radeon_swtcl.c \
		$(MESADRVRADEONBUILDDIR)radeon_tcl.c \
		$(MESADRVRADEONBUILDDIR)radeon_tex.c \
		$(MESADRVRADEONBUILDDIR)radeon_texmem.c \
		$(MESADRVRADEONBUILDDIR)radeon_texstate.c \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt.c \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt_x86.c \
		$(MESADRVRADEONBUILDDIR)radeon_vtxtmp_x86.S \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt_sse.c \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt_c.c

   RADEONOBJS = $(MESADRVRADEONBUILDDIR)radeon_context.o \
		$(MESADRVRADEONBUILDDIR)radeon_compat.o \
		$(MESADRVRADEONBUILDDIR)radeon_ioctl.o \
		$(MESADRVRADEONBUILDDIR)radeon_lock.o \
		$(MESADRVRADEONBUILDDIR)radeon_maos.o \
	        $(MESADRVRADEONBUILDDIR)radeon_sanity.o \
	        $(MESADRVRADEONBUILDDIR)radeon_screen.o \
		$(MESADRVRADEONBUILDDIR)radeon_span.o \
		$(MESADRVRADEONBUILDDIR)radeon_state.o \
		$(MESADRVRADEONBUILDDIR)radeon_state_init.o \
		$(MESADRVRADEONBUILDDIR)radeon_swtcl.o \
		$(MESADRVRADEONBUILDDIR)radeon_tcl.o \
		$(MESADRVRADEONBUILDDIR)radeon_tex.o \
		$(MESADRVRADEONBUILDDIR)radeon_texmem.o \
		$(MESADRVRADEONBUILDDIR)radeon_texstate.o \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt.o \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt_c.o

#ifdef i386Architecture
   RADEONOBJS += $(MESADRVRADEONBUILDDIR)radeon_vtxfmt_x86.o \
		$(MESADRVRADEONBUILDDIR)radeon_vtxtmp_x86.o \
		$(MESADRVRADEONBUILDDIR)radeon_vtxfmt_sse.o 
#endif

   RADEONUOBJS = $(MESADRVRADEONBUILDDIR)unshared/radeon_context.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_compat.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_ioctl.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_lock.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_maos.o \
	        $(MESADRVRADEONBUILDDIR)unshared/radeon_sanity.o \
	        $(MESADRVRADEONBUILDDIR)unshared/radeon_screen.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_span.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_state.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_state_init.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_swtcl.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_tcl.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_tex.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_texmem.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_texstate.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_vtxfmt.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_vtxfmt_c.o

#ifdef i386Architecture
   RADEONUOBJS += $(MESADRVRADEONBUILDDIR)unshared/radeon_vtxfmt_x86.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_vtxtmp_x86.o \
		$(MESADRVRADEONBUILDDIR)unshared/radeon_vtxfmt_sse.o 
#endif

   RADEONDOBJS = $(MESADRVRADEONBUILDDIR)debugger/radeon_context.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_compat.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_ioctl.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_lock.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_maos.o \
	        $(MESADRVRADEONBUILDDIR)debugger/radeon_sanity.o \
	        $(MESADRVRADEONBUILDDIR)debugger/radeon_screen.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_span.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_state.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_state_init.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_swtcl.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_tcl.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_tex.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_texmem.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_texstate.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_vtxfmt.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_vtxfmt_c.o

#ifdef i386Architecture
   RADEONDOBJS += $(MESADRVRADEONBUILDDIR)debugger/radeon_vtxfmt_x86.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_vtxtmp_x86.o \
		$(MESADRVRADEONBUILDDIR)debugger/radeon_vtxfmt_sse.o 
#endif

   RADEONPOBJS = $(MESADRVRADEONBUILDDIR)profiled/radeon_context.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_compat.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_ioctl.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_lock.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_maos.o \
	        $(MESADRVRADEONBUILDDIR)profiled/radeon_sanity.o \
	        $(MESADRVRADEONBUILDDIR)profiled/radeon_screen.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_span.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_state.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_state_init.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_swtcl.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_tcl.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_tex.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_texmem.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_texstate.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_vtxfmt.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_vtxfmt_c.o

#ifdef i386Architecture
   RADEONPOBJS += $(MESADRVRADEONBUILDDIR)profiled/radeon_vtxfmt_x86.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_vtxtmp_x86.o \
		$(MESADRVRADEONBUILDDIR)profiled/radeon_vtxfmt_sse.o 
#endif

#ifdef NeedToLinkMesaSrc
LinkSourceFile(radeon_context.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_compat.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_ioctl.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_lock.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_maos.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_sanity.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_screen.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_span.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_state.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_state_init.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_swtcl.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_tcl.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_tex.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_texmem.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_texstate.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_vtxfmt.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_vtxfmt_c.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_vtxfmt_x86.c, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_vtxtmp_x86.S, $(MESADRVSRCDIR)/radeon)
LinkSourceFile(radeon_vtxfmt_sse.c, $(MESADRVSRCDIR)/radeon)
#endif
