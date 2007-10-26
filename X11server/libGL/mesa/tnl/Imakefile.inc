XCOMM $XFree86: xc/lib/GL/mesa/src/tnl/Imakefile.inc,v 1.3 2003/09/28 20:15:40 alanh Exp $

#ifndef MesaTnlBuildDir
#define MesaTnlBuildDir $(GLXLIBSRC)/mesa/src/tnl/
#endif

MESATNLBUILDDIR = MesaTnlBuildDir

  MESA_TNL_SRCS = $(MESATNLBUILDDIR)t_array_api.c \
		  $(MESATNLBUILDDIR)t_array_import.c \
		  $(MESATNLBUILDDIR)t_context.c \
		  $(MESATNLBUILDDIR)t_eval_api.c \
		  $(MESATNLBUILDDIR)t_imm_alloc.c \
		  $(MESATNLBUILDDIR)t_imm_api.c \
		  $(MESATNLBUILDDIR)t_imm_debug.c \
		  $(MESATNLBUILDDIR)t_imm_dlist.c \
		  $(MESATNLBUILDDIR)t_imm_elt.c \
		  $(MESATNLBUILDDIR)t_imm_eval.c \
		  $(MESATNLBUILDDIR)t_imm_exec.c \
		  $(MESATNLBUILDDIR)t_imm_fixup.c \
		  $(MESATNLBUILDDIR)t_pipeline.c \
		  $(MESATNLBUILDDIR)t_vb_fog.c \
		  $(MESATNLBUILDDIR)t_vb_light.c \
		  $(MESATNLBUILDDIR)t_vb_normals.c \
		  $(MESATNLBUILDDIR)t_vb_points.c \
		  $(MESATNLBUILDDIR)t_vb_program.c \
		  $(MESATNLBUILDDIR)t_vb_render.c \
		  $(MESATNLBUILDDIR)t_vb_texgen.c \
		  $(MESATNLBUILDDIR)t_vb_texmat.c \
		  $(MESATNLBUILDDIR)t_vb_vertex.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(t_array_api.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_array_import.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_context.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_eval_api.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_alloc.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_api.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_debug.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_dlist.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_elt.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_eval.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_exec.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_imm_fixup.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_pipeline.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_fog.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_light.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_normals.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_points.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_program.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_render.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_texgen.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_texmat.c, $(MESASRCDIR)/src/tnl)
LinkSourceFile(t_vb_vertex.c, $(MESASRCDIR)/src/tnl)
#endif

  MESA_TNL_OBJS = $(MESATNLBUILDDIR)t_array_api.o \
		  $(MESATNLBUILDDIR)t_array_import.o \
		  $(MESATNLBUILDDIR)t_context.o \
		  $(MESATNLBUILDDIR)t_eval_api.o \
		  $(MESATNLBUILDDIR)t_imm_alloc.o \
		  $(MESATNLBUILDDIR)t_imm_api.o \
		  $(MESATNLBUILDDIR)t_imm_debug.o \
		  $(MESATNLBUILDDIR)t_imm_dlist.o \
		  $(MESATNLBUILDDIR)t_imm_elt.o \
		  $(MESATNLBUILDDIR)t_imm_eval.o \
		  $(MESATNLBUILDDIR)t_imm_exec.o \
		  $(MESATNLBUILDDIR)t_imm_fixup.o \
		  $(MESATNLBUILDDIR)t_pipeline.o \
		  $(MESATNLBUILDDIR)t_vb_fog.o \
		  $(MESATNLBUILDDIR)t_vb_light.o \
		  $(MESATNLBUILDDIR)t_vb_normals.o \
		  $(MESATNLBUILDDIR)t_vb_points.o \
		  $(MESATNLBUILDDIR)t_vb_program.o \
		  $(MESATNLBUILDDIR)t_vb_render.o \
		  $(MESATNLBUILDDIR)t_vb_texgen.o \
		  $(MESATNLBUILDDIR)t_vb_texmat.o \
		  $(MESATNLBUILDDIR)t_vb_vertex.o

#if defined(DoSharedLib) && DoSharedLib
 MESA_TNL_UOBJS = $(MESATNLBUILDDIR)unshared/t_array_api.o \
		  $(MESATNLBUILDDIR)unshared/t_array_import.o \
		  $(MESATNLBUILDDIR)unshared/t_context.o \
		  $(MESATNLBUILDDIR)unshared/t_eval_api.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_alloc.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_api.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_debug.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_dlist.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_elt.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_eval.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_exec.o \
		  $(MESATNLBUILDDIR)unshared/t_imm_fixup.o \
		  $(MESATNLBUILDDIR)unshared/t_pipeline.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_fog.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_light.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_normals.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_points.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_program.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_render.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_texgen.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_texmat.o \
		  $(MESATNLBUILDDIR)unshared/t_vb_vertex.o
#else
 MESA_TNL_UOBJS = $(MESA_TNL_OBJS)
#endif

 MESA_TNL_DOBJS = $(MESATNLBUILDDIR)debugger/t_array_api.o \
		  $(MESATNLBUILDDIR)debugger/t_array_import.o \
		  $(MESATNLBUILDDIR)debugger/t_context.o \
		  $(MESATNLBUILDDIR)debugger/t_eval_api.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_alloc.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_api.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_debug.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_dlist.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_elt.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_eval.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_exec.o \
		  $(MESATNLBUILDDIR)debugger/t_imm_fixup.o \
		  $(MESATNLBUILDDIR)debugger/t_pipeline.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_fog.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_light.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_normals.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_points.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_program.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_render.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_texgen.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_texmat.o \
		  $(MESATNLBUILDDIR)debugger/t_vb_vertex.o

 MESA_TNL_POBJS = $(MESATNLBUILDDIR)profiled/t_array_api.o \
		  $(MESATNLBUILDDIR)profiled/t_array_import.o \
		  $(MESATNLBUILDDIR)profiled/t_context.o \
		  $(MESATNLBUILDDIR)profiled/t_eval_api.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_alloc.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_api.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_profiled.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_dlist.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_elt.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_eval.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_exec.o \
		  $(MESATNLBUILDDIR)profiled/t_imm_fixup.o \
		  $(MESATNLBUILDDIR)profiled/t_pipeline.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_fog.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_light.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_normals.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_points.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_program.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_render.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_texgen.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_texmat.o \
		  $(MESATNLBUILDDIR)profiled/t_vb_vertex.o

