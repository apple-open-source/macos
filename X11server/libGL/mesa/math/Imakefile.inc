XCOMM $XFree86: xc/lib/GL/mesa/src/math/Imakefile.inc,v 1.3 2003/09/28 20:15:39 alanh Exp $

#ifndef MesaMathBuildDir
#define MesaMathBuildDir $(GLXLIBSRC)/mesa/src/math/
#endif

MESAMATHBUILDDIR = MesaMathBuildDir

  MESA_MATH_SRCS = $(MESAMATHBUILDDIR)m_debug_clip.c \
		   $(MESAMATHBUILDDIR)m_debug_norm.c \
		   $(MESAMATHBUILDDIR)m_debug_xform.c \
		   $(MESAMATHBUILDDIR)m_eval.c \
		   $(MESAMATHBUILDDIR)m_matrix.c \
		   $(MESAMATHBUILDDIR)m_translate.c \
		   $(MESAMATHBUILDDIR)m_vector.c \
		   $(MESAMATHBUILDDIR)m_xform.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(m_debug_clip.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_debug_norm.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_debug_xform.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_eval.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_matrix.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_translate.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_vector.c, $(MESASRCDIR)/src/math)
LinkSourceFile(m_xform.c, $(MESASRCDIR)/src/math)
#endif

  MESA_MATH_OBJS = $(MESAMATHBUILDDIR)m_debug_clip.o \
		   $(MESAMATHBUILDDIR)m_debug_norm.o \
		   $(MESAMATHBUILDDIR)m_debug_xform.o \
		   $(MESAMATHBUILDDIR)m_eval.o \
		   $(MESAMATHBUILDDIR)m_matrix.o \
		   $(MESAMATHBUILDDIR)m_translate.o \
		   $(MESAMATHBUILDDIR)m_vector.o \
		   $(MESAMATHBUILDDIR)m_xform.o

#if defined(DoSharedLib) && DoSharedLib
 MESA_MATH_UOBJS = $(MESAMATHBUILDDIR)unshared/m_debug_clip.o \
		   $(MESAMATHBUILDDIR)unshared/m_debug_norm.o \
		   $(MESAMATHBUILDDIR)unshared/m_debug_xform.o \
		   $(MESAMATHBUILDDIR)unshared/m_eval.o \
		   $(MESAMATHBUILDDIR)unshared/m_matrix.o \
		   $(MESAMATHBUILDDIR)unshared/m_translate.o \
		   $(MESAMATHBUILDDIR)unshared/m_vector.o \
		   $(MESAMATHBUILDDIR)unshared/m_xform.o
#else
 MATH_MESA_UOBJS = $(MESA_MATH_OBJS)
#endif

 MESA_MATH_DOBJS = $(MESAMATHBUILDDIR)debugger/m_debug_clip.o \
		   $(MESAMATHBUILDDIR)debugger/m_debug_norm.o \
		   $(MESAMATHBUILDDIR)debugger/m_debug_xform.o \
		   $(MESAMATHBUILDDIR)debugger/m_eval.o \
		   $(MESAMATHBUILDDIR)debugger/m_matrix.o \
		   $(MESAMATHBUILDDIR)debugger/m_translate.o \
		   $(MESAMATHBUILDDIR)debugger/m_vector.o \
		   $(MESAMATHBUILDDIR)debugger/m_xform.o

 MESA_MATH_POBJS = $(MESAMATHBUILDDIR)profiled/m_debug_clip.o \
		   $(MESAMATHBUILDDIR)profiled/m_debug_norm.o \
		   $(MESAMATHBUILDDIR)profiled/m_debug_xform.o \
		   $(MESAMATHBUILDDIR)profiled/m_eval.o \
		   $(MESAMATHBUILDDIR)profiled/m_matrix.o \
		   $(MESAMATHBUILDDIR)profiled/m_translate.o \
		   $(MESAMATHBUILDDIR)profiled/m_vector.o \
		   $(MESAMATHBUILDDIR)profiled/m_xform.o

