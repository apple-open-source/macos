XCOMM $XFree86: xc/lib/GL/mesa/src/drv/i810/Imakefile.inc,v 1.4 2003/05/27 16:34:33 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(GLXLIBSRC)/mesa/src/drv
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvI810BuildDir
#define MesaDrvI810BuildDir /**/
#endif
MESADRVI810BUILDDIR = MesaDrvI810BuildDir

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
		-I$(XF86DRIVERSRC)/i810 \
		-I$(XF86COMSRC) \
		-I$(GLXLIBSRC)/dri/drm \
		-I$(GLXLIBSRC)/include
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/i810
   X_INCLUDES = -I$(XINCLUDESRC) -I$(EXTINCSRC)

     I810SRCS = $(MESADRVI810BUILDDIR)i810context.c \
		$(MESADRVI810BUILDDIR)i810ioctl.c \
		$(MESADRVI810BUILDDIR)i810render.c \
		$(MESADRVI810BUILDDIR)i810screen.c \
		$(MESADRVI810BUILDDIR)i810span.c \
		$(MESADRVI810BUILDDIR)i810state.c \
		$(MESADRVI810BUILDDIR)i810tex.c \
		$(MESADRVI810BUILDDIR)i810texmem.c \
		$(MESADRVI810BUILDDIR)i810texstate.c \
		$(MESADRVI810BUILDDIR)i810tris.c \
		$(MESADRVI810BUILDDIR)i810vb.c

     I810OBJS = $(MESADRVI810BUILDDIR)i810context.o \
		$(MESADRVI810BUILDDIR)i810ioctl.o \
		$(MESADRVI810BUILDDIR)i810render.o \
		$(MESADRVI810BUILDDIR)i810screen.o \
		$(MESADRVI810BUILDDIR)i810span.o \
		$(MESADRVI810BUILDDIR)i810state.o \
		$(MESADRVI810BUILDDIR)i810tex.o \
		$(MESADRVI810BUILDDIR)i810texmem.o \
		$(MESADRVI810BUILDDIR)i810texstate.o \
		$(MESADRVI810BUILDDIR)i810tris.o \
		$(MESADRVI810BUILDDIR)i810vb.o

     I810UOBJS = $(MESADRVI810BUILDDIR)unshared/i810context.o \
		$(MESADRVI810BUILDDIR)unshared/i810ioctl.o \
		$(MESADRVI810BUILDDIR)unshared/i810render.o \
		$(MESADRVI810BUILDDIR)unshared/i810screen.o \
		$(MESADRVI810BUILDDIR)unshared/i810span.o \
		$(MESADRVI810BUILDDIR)unshared/i810state.o \
		$(MESADRVI810BUILDDIR)unshared/i810tex.o \
		$(MESADRVI810BUILDDIR)unshared/i810texmem.o \
		$(MESADRVI810BUILDDIR)unshared/i810texstate.o \
		$(MESADRVI810BUILDDIR)unshared/i810tris.o \
		$(MESADRVI810BUILDDIR)unshared/i810vb.o

     I810DOBJS = $(MESADRVI810BUILDDIR)debugger/i810context.o \
		$(MESADRVI810BUILDDIR)debugger/i810ioctl.o \
		$(MESADRVI810BUILDDIR)debugger/i810render.o \
		$(MESADRVI810BUILDDIR)debugger/i810screen.o \
		$(MESADRVI810BUILDDIR)debugger/i810span.o \
		$(MESADRVI810BUILDDIR)debugger/i810state.o \
		$(MESADRVI810BUILDDIR)debugger/i810tex.o \
		$(MESADRVI810BUILDDIR)debugger/i810texmem.o \
		$(MESADRVI810BUILDDIR)debugger/i810texstate.o \
		$(MESADRVI810BUILDDIR)debugger/i810tris.o \
		$(MESADRVI810BUILDDIR)debugger/i810vb.o

     I810POBJS = $(MESADRVI810BUILDDIR)profiled/i810context.o \
		$(MESADRVI810BUILDDIR)profiled/i810ioctl.o \
		$(MESADRVI810BUILDDIR)profiled/i810render.o \
		$(MESADRVI810BUILDDIR)profiled/i810screen.o \
		$(MESADRVI810BUILDDIR)profiled/i810span.o \
		$(MESADRVI810BUILDDIR)profiled/i810state.o \
		$(MESADRVI810BUILDDIR)profiled/i810tex.o \
		$(MESADRVI810BUILDDIR)profiled/i810texmem.o \
		$(MESADRVI810BUILDDIR)profiled/i810texstate.o \
		$(MESADRVI810BUILDDIR)profiled/i810tris.o \
		$(MESADRVI810BUILDDIR)profiled/i810vb.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(i810context.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810ioctl.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810render.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810screen.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810span.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810state.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810tex.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810texmem.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810texstate.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810tris.c, $(MESADRVSRCDIR)/i810)
LinkSourceFile(i810vb.c, $(MESADRVSRCDIR)/i810)
#endif

