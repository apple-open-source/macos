XCOMM $XFree86: xc/lib/GL/mesa/src/X86/Imakefile.inc,v 1.7 2003/09/28 20:15:06 alanh Exp $

#ifndef MesaX86BuildDir
#define MesaX86BuildDir $(GLXLIBSRC)/mesa/src/X86/
#endif

MESAX86BUILDDIR = MesaX86BuildDir

MESA_X86_SRCS = $(MESAX86BUILDDIR)common_x86.c \
		$(MESAX86BUILDDIR)common_x86_asm.S \
		$(MESAX86BUILDDIR)glapi_x86.S \
		$(MESAX86BUILDDIR)x86.c \
		$(MESAX86BUILDDIR)x86_cliptest.S \
		$(MESAX86BUILDDIR)x86_xform2.S \
		$(MESAX86BUILDDIR)x86_xform3.S \
		$(MESAX86BUILDDIR)x86_xform4.S

#ifdef NeedToLinkMesaSrc
LinkSourceFile(common_x86.c, $(MESASRCDIR)/src/X86)
LinkSourceFile(common_x86_asm.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(glapi_x86.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(x86.c, $(MESASRCDIR)/src/X86)
LinkSourceFile(x86_cliptest.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(x86_xform2.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(x86_xform3.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(x86_xform4.S, $(MESASRCDIR)/src/X86)
#endif

MESA_X86_OBJS = $(MESAX86BUILDDIR)common_x86.o \
		$(MESAX86BUILDDIR)common_x86_asm.o \
		$(MESAX86BUILDDIR)x86.o \
		$(MESAX86BUILDDIR)x86_cliptest.o \
		$(MESAX86BUILDDIR)x86_xform2.o \
		$(MESAX86BUILDDIR)x86_xform3.o \
		$(MESAX86BUILDDIR)x86_xform4.o

#if defined(DoSharedLib) && DoSharedLib
MESA_X86_UOBJS = $(MESAX86BUILDDIR)unshared/common_x86.o \
		$(MESAX86BUILDDIR)common_x86_asm.o \
		$(MESAX86BUILDDIR)unshared/x86.o \
		$(MESAX86BUILDDIR)x86_cliptest.o \
		$(MESAX86BUILDDIR)x86_xform2.o \
		$(MESAX86BUILDDIR)x86_xform3.o \
		$(MESAX86BUILDDIR)x86_xform4.o
#else
MESA_X86_UOBJS = $(MESA_X86_OBJS)
#endif

MESA_X86_DOBJS = $(MESAX86BUILDDIR)debugger/common_x86.o \
		$(MESAX86BUILDDIR)common_x86_asm.o \
		$(MESAX86BUILDDIR)debugger/x86.o \
		$(MESAX86BUILDDIR)x86_cliptest.o \
		$(MESAX86BUILDDIR)x86_xform2.o \
		$(MESAX86BUILDDIR)x86_xform3.o \
		$(MESAX86BUILDDIR)x86_xform4.o

MESA_X86_POBJS = $(MESAX86BUILDDIR)profiled/common_x86.o \
		$(MESAX86BUILDDIR)common_x86_asm.o \
		$(MESAX86BUILDDIR)profiled/x86.o \
		$(MESAX86BUILDDIR)x86_cliptest.o \
		$(MESAX86BUILDDIR)x86_xform2.o \
		$(MESAX86BUILDDIR)x86_xform3.o \
		$(MESAX86BUILDDIR)x86_xform4.o

MESA_X86_DEFS = -DUSE_X86_ASM


#if MesaUseMMX
MESA_MMX_SRCS = $(MESAX86BUILDDIR)mmx_blend.S

#ifdef NeedToLinkMesaSrc
LinkSourceFile(mmx_blend.S, $(MESASRCDIR)/src/X86)
#endif

MESA_MMX_OBJS = $(MESAX86BUILDDIR)mmx_blend.o

MESA_MMX_UOBJS = $(MESA_MMX_OBJS)
MESA_MMX_DOBJS = $(MESA_MMX_OBJS)
MESA_MMX_POBJS = $(MESA_MMX_OBJS)

MESA_MMX_DEFS = -DUSE_MMX_ASM
#endif

#if MesaUse3DNow
MESA_3DNOW_SRCS = $(MESAX86BUILDDIR)3dnow.c \
		$(MESAX86BUILDDIR)3dnow_normal.S \
		$(MESAX86BUILDDIR)3dnow_xform1.S \
		$(MESAX86BUILDDIR)3dnow_xform2.S \
		$(MESAX86BUILDDIR)3dnow_xform3.S \
		$(MESAX86BUILDDIR)3dnow_xform4.S

#ifdef NeedToLinkMesaSrc
LinkSourceFile(3dnow.c, $(MESASRCDIR)/src/X86)
LinkSourceFile(3dnow_normal.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(3dnow_xform1.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(3dnow_xform2.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(3dnow_xform3.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(3dnow_xform4.S, $(MESASRCDIR)/src/X86)
#endif
MESA_3DNOW_OBJS = $(MESAX86BUILDDIR)3dnow.o \
		$(MESAX86BUILDDIR)3dnow_normal.o \
		$(MESAX86BUILDDIR)3dnow_xform1.o \
		$(MESAX86BUILDDIR)3dnow_xform2.o \
		$(MESAX86BUILDDIR)3dnow_xform3.o \
		$(MESAX86BUILDDIR)3dnow_xform4.o

#if defined(DoSharedLib) && DoSharedLib
MESA_3DNOW_UOBJS = $(MESAX86BUILDDIR)unshared/3dnow.o \
		$(MESAX86BUILDDIR)3dnow_normal.o \
		$(MESAX86BUILDDIR)3dnow_xform1.o \
		$(MESAX86BUILDDIR)3dnow_xform2.o \
		$(MESAX86BUILDDIR)3dnow_xform3.o \
		$(MESAX86BUILDDIR)3dnow_xform4.o
#else
MESA_3DNOW_UOBJS = $(MESA_3DNOW_OBJS)
#endif

MESA_3DNOW_DOBJS = $(MESAX86BUILDDIR)debugger/3dnow.o \
		$(MESAX86BUILDDIR)3dnow_normal.o \
		$(MESAX86BUILDDIR)3dnow_xform1.o \
		$(MESAX86BUILDDIR)3dnow_xform2.o \
		$(MESAX86BUILDDIR)3dnow_xform3.o \
		$(MESAX86BUILDDIR)3dnow_xform4.o

MESA_3DNOW_POBJS = $(MESAX86BUILDDIR)profiled/3dnow.o \
		$(MESAX86BUILDDIR)3dnow_normal.o \
		$(MESAX86BUILDDIR)3dnow_xform1.o \
		$(MESAX86BUILDDIR)3dnow_xform2.o \
		$(MESAX86BUILDDIR)3dnow_xform3.o \
		$(MESAX86BUILDDIR)3dnow_xform4.o

MESA_3DNOW_DEFS = -DUSE_3DNOW_ASM
#endif

#if MesaUseSSE
MESA_SSE_SRCS = $(MESAX86BUILDDIR)sse.c \
		$(MESAX86BUILDDIR)sse_normal.S \
		$(MESAX86BUILDDIR)sse_xform1.S \
		$(MESAX86BUILDDIR)sse_xform2.S \
		$(MESAX86BUILDDIR)sse_xform3.S \
		$(MESAX86BUILDDIR)sse_xform4.S

#ifdef NeedToLinkMesaSrc
LinkSourceFile(sse.c, $(MESASRCDIR)/src/X86)
LinkSourceFile(sse_normal.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(sse_xform1.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(sse_xform2.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(sse_xform3.S, $(MESASRCDIR)/src/X86)
LinkSourceFile(sse_xform4.S, $(MESASRCDIR)/src/X86)
#endif

MESA_SSE_OBJS = $(MESAX86BUILDDIR)sse.o \
		$(MESAX86BUILDDIR)sse_normal.o \
		$(MESAX86BUILDDIR)sse_xform1.o \
		$(MESAX86BUILDDIR)sse_xform2.o \
		$(MESAX86BUILDDIR)sse_xform3.o \
		$(MESAX86BUILDDIR)sse_xform4.o

#if defined(DoSharedLib) && DoSharedLib
MESA_SSE_UOBJS = $(MESAX86BUILDDIR)unshared/sse.o \
		$(MESAX86BUILDDIR)sse_normal.o \
		$(MESAX86BUILDDIR)sse_xform1.o \
		$(MESAX86BUILDDIR)sse_xform2.o \
		$(MESAX86BUILDDIR)sse_xform3.o \
		$(MESAX86BUILDDIR)sse_xform4.o
#else
MESA_SSE_UOBJS = $(MESA_SSE_OBJS)
#endif

MESA_SSE_DOBJS = $(MESAX86BUILDDIR)debugger/sse.o \
		$(MESAX86BUILDDIR)sse_normal.o \
		$(MESAX86BUILDDIR)sse_xform1.o \
		$(MESAX86BUILDDIR)sse_xform2.o \
		$(MESAX86BUILDDIR)sse_xform3.o \
		$(MESAX86BUILDDIR)sse_xform4.o

MESA_SSE_POBJS = $(MESAX86BUILDDIR)profiled/sse.o \
		$(MESAX86BUILDDIR)sse_normal.o \
		$(MESAX86BUILDDIR)sse_xform1.o \
		$(MESAX86BUILDDIR)sse_xform2.o \
		$(MESAX86BUILDDIR)sse_xform3.o \
		$(MESAX86BUILDDIR)sse_xform4.o

MESA_SSE_DEFS = -DUSE_SSE_ASM
#endif

MESA_ASM_SRCS = $(MESA_X86_SRCS) $(MESA_MMX_SRCS) $(MESA_3DNOW_SRCS) \
		$(MESA_SSE_SRCS)

MESA_ASM_OBJS = $(MESA_X86_OBJS) $(MESA_MMX_OBJS) $(MESA_3DNOW_OBJS) \
		$(MESA_SSE_OBJS)

MESA_ASM_UOBJS = $(MESA_X86_UOBJS) $(MESA_MMX_UOBJS) $(MESA_3DNOW_UOBJS) \
		$(MESA_SSE_UOBJS)

MESA_ASM_DOBJS = $(MESA_X86_DOBJS) $(MESA_MMX_DOBJS) $(MESA_3DNOW_DOBJS) \
		$(MESA_SSE_DOBJS)

MESA_ASM_POBJS = $(MESA_X86_POBJS) $(MESA_MMX_POBJS) $(MESA_3DNOW_POBJS) \
		$(MESA_SSE_POBJS)

MESA_ASM_DEFINES = $(MESA_X86_DEFS) $(MESA_MMX_DEFS) $(MESA_3DNOW_DEFS) $(MESA_SSE_DEFS)

