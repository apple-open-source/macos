XCOMM $XFree86: xc/lib/GL/mesa/src/Imakefile.inc,v 1.8 2002/11/14 21:01:16 tsi Exp $

#ifndef MesaBuildDir
#define MesaBuildDir $(GLXLIBSRC)/mesa/src/
#endif

MESABUILDDIR = MesaBuildDir

#ifdef NeedAllMesaSrc
 DISPATCHSRCS = $(MESABUILDDIR)dispatch.c
    GLAPISRCS = $(MESABUILDDIR)glapi.c
GLAPINOOPSRCS = $(MESABUILDDIR)glapinoop.c
 GLTHREADSRCS = $(MESABUILDDIR)glthread.c
#endif

MESA_CORE_INCLUDES = -I$(MESASRCDIR)/src/array_cache \
                -I$(MESASRCDIR)/src/math \
                -I$(MESASRCDIR)/src/swrast -I$(MESASRCDIR)/src/swrast_setup \
                -I$(MESASRCDIR)/src/tnl -I$(MESASRCDIR)/src/tnl_dd


COREMESABASESRCS = \
		$(MESABUILDDIR)accum.c \
		$(MESABUILDDIR)api_arrayelt.c \
		$(MESABUILDDIR)api_eval.c \
		$(MESABUILDDIR)api_loopback.c \
		$(MESABUILDDIR)api_noop.c \
		$(MESABUILDDIR)api_validate.c \
		$(MESABUILDDIR)attrib.c \
		$(MESABUILDDIR)blend.c \
		$(MESABUILDDIR)buffers.c \
		$(MESABUILDDIR)clip.c \
		$(MESABUILDDIR)colortab.c \
		$(MESABUILDDIR)config.c \
		$(MESABUILDDIR)context.c \
		$(MESABUILDDIR)convolve.c \
		$(MESABUILDDIR)debug.c \
		$(MESABUILDDIR)depth.c \
		$(DISPATCHSRCS) \
		$(MESABUILDDIR)dlist.c \
		$(MESABUILDDIR)drawpix.c \
		$(MESABUILDDIR)enable.c \
		$(MESABUILDDIR)enums.c \
		$(MESABUILDDIR)eval.c \
		$(MESABUILDDIR)extensions.c \
		$(MESABUILDDIR)feedback.c \
		$(MESABUILDDIR)fog.c \
		$(MESABUILDDIR)get.c \
		$(GLAPISRCS) \
		$(GLTHREADSRCS) \
		$(MESABUILDDIR)hash.c \
		$(MESABUILDDIR)hint.c \
		$(MESABUILDDIR)histogram.c \
		$(MESABUILDDIR)image.c \
		$(MESABUILDDIR)imports.c \
		$(MESABUILDDIR)light.c \
		$(MESABUILDDIR)lines.c \
		$(MESABUILDDIR)matrix.c \
		$(MESABUILDDIR)mem.c \
		$(MESABUILDDIR)mmath.c \
		$(MESABUILDDIR)pixel.c \
		$(MESABUILDDIR)points.c \
		$(MESABUILDDIR)polygon.c \
		$(MESABUILDDIR)rastpos.c \
		$(MESABUILDDIR)state.c \
		$(MESABUILDDIR)stencil.c \
		$(MESABUILDDIR)texformat.c \
		$(MESABUILDDIR)teximage.c \
		$(MESABUILDDIR)texobj.c \
		$(MESABUILDDIR)texstate.c \
		$(MESABUILDDIR)texstore.c \
		$(MESABUILDDIR)texutil.c \
		$(MESABUILDDIR)varray.c \
		$(MESABUILDDIR)vtxfmt.c

COREMESASRCS = $(COREMESABASESRCS) \
		$(MESABUILDDIR)array_cache/ac_context.c \
		$(MESABUILDDIR)array_cache/ac_import.c \
		$(MESABUILDDIR)math/m_debug_clip.c \
		$(MESABUILDDIR)math/m_debug_norm.c \
		$(MESABUILDDIR)math/m_debug_vertex.c \
		$(MESABUILDDIR)math/m_debug_xform.c \
		$(MESABUILDDIR)math/m_eval.c \
		$(MESABUILDDIR)math/m_matrix.c \
		$(MESABUILDDIR)math/m_translate.c \
		$(MESABUILDDIR)math/m_vector.c \
		$(MESABUILDDIR)math/m_vertices.c \
		$(MESABUILDDIR)math/m_xform.c \
		$(MESABUILDDIR)swrast/s_aaline.c \
		$(MESABUILDDIR)swrast/s_aatriangle.c \
		$(MESABUILDDIR)swrast/s_accum.c \
		$(MESABUILDDIR)swrast/s_alpha.c \
		$(MESABUILDDIR)swrast/s_alphabuf.c \
		$(MESABUILDDIR)swrast/s_bitmap.c \
		$(MESABUILDDIR)swrast/s_blend.c \
		$(MESABUILDDIR)swrast/s_buffers.c \
		$(MESABUILDDIR)swrast/s_copypix.c \
		$(MESABUILDDIR)swrast/s_context.c \
		$(MESABUILDDIR)swrast/s_depth.c \
		$(MESABUILDDIR)swrast/s_drawpix.c \
		$(MESABUILDDIR)swrast/s_feedback.c \
		$(MESABUILDDIR)swrast/s_fog.c \
		$(MESABUILDDIR)swrast/s_histogram.c \
		$(MESABUILDDIR)swrast/s_imaging.c \
		$(MESABUILDDIR)swrast/s_lines.c \
		$(MESABUILDDIR)swrast/s_logic.c \
		$(MESABUILDDIR)swrast/s_masking.c \
		$(MESABUILDDIR)swrast/s_pb.c \
		$(MESABUILDDIR)swrast/s_pixeltex.c \
		$(MESABUILDDIR)swrast/s_points.c \
		$(MESABUILDDIR)swrast/s_readpix.c \
		$(MESABUILDDIR)swrast/s_scissor.c \
		$(MESABUILDDIR)swrast/s_span.c \
		$(MESABUILDDIR)swrast/s_stencil.c \
		$(MESABUILDDIR)swrast/s_texture.c \
		$(MESABUILDDIR)swrast/s_texstore.c \
		$(MESABUILDDIR)swrast/s_triangle.c \
		$(MESABUILDDIR)swrast/s_zoom.c \
		$(MESABUILDDIR)swrast_setup/ss_context.c \
		$(MESABUILDDIR)swrast_setup/ss_triangle.c \
		$(MESABUILDDIR)swrast_setup/ss_vb.c \
		$(MESABUILDDIR)tnl/t_array_api.c \
		$(MESABUILDDIR)tnl/t_array_import.c \
		$(MESABUILDDIR)tnl/t_context.c \
		$(MESABUILDDIR)tnl/t_eval_api.c \
		$(MESABUILDDIR)tnl/t_imm_alloc.c \
		$(MESABUILDDIR)tnl/t_imm_api.c \
		$(MESABUILDDIR)tnl/t_imm_debug.c \
		$(MESABUILDDIR)tnl/t_imm_dlist.c \
		$(MESABUILDDIR)tnl/t_imm_elt.c \
		$(MESABUILDDIR)tnl/t_imm_eval.c \
		$(MESABUILDDIR)tnl/t_imm_exec.c \
		$(MESABUILDDIR)tnl/t_imm_fixup.c \
		$(MESABUILDDIR)tnl/t_pipeline.c \
		$(MESABUILDDIR)tnl/t_vb_fog.c \
		$(MESABUILDDIR)tnl/t_vb_light.c \
		$(MESABUILDDIR)tnl/t_vb_normals.c \
		$(MESABUILDDIR)tnl/t_vb_points.c \
		$(MESABUILDDIR)tnl/t_vb_render.c \
		$(MESABUILDDIR)tnl/t_vb_texgen.c \
		$(MESABUILDDIR)tnl/t_vb_texmat.c \
		$(MESABUILDDIR)tnl/t_vb_vertex.c

#ifdef NeedToLinkMesaSrc
LinkSourceFile(accum.c, $(MESASRCDIR)/src)
LinkSourceFile(api_arrayelt.c, $(MESASRCDIR)/src)
LinkSourceFile(api_eval.c, $(MESASRCDIR)/src)
LinkSourceFile(api_loopback.c, $(MESASRCDIR)/src)
LinkSourceFile(api_noop.c, $(MESASRCDIR)/src)
LinkSourceFile(api_validate.c, $(MESASRCDIR)/src)
LinkSourceFile(attrib.c, $(MESASRCDIR)/src)
LinkSourceFile(blend.c, $(MESASRCDIR)/src)
LinkSourceFile(buffers.c, $(MESASRCDIR)/src)
LinkSourceFile(clip.c, $(MESASRCDIR)/src)
LinkSourceFile(colortab.c, $(MESASRCDIR)/src)
LinkSourceFile(config.c, $(MESASRCDIR)/src)
LinkSourceFile(context.c, $(MESASRCDIR)/src)
LinkSourceFile(convolve.c, $(MESASRCDIR)/src)
LinkSourceFile(debug.c, $(MESASRCDIR)/src)
LinkSourceFile(depth.c, $(MESASRCDIR)/src)
#ifdef NeedAllMesaSrc
LinkSourceFile(dispatch.c, $(MESASRCDIR)/src)
#endif
LinkSourceFile(dlist.c, $(MESASRCDIR)/src)
LinkSourceFile(drawpix.c, $(MESASRCDIR)/src)
LinkSourceFile(enable.c, $(MESASRCDIR)/src)
LinkSourceFile(enums.c, $(MESASRCDIR)/src)
LinkSourceFile(eval.c, $(MESASRCDIR)/src)
LinkSourceFile(extensions.c, $(MESASRCDIR)/src)
LinkSourceFile(feedback.c, $(MESASRCDIR)/src)
LinkSourceFile(fog.c, $(MESASRCDIR)/src)
LinkSourceFile(get.c, $(MESASRCDIR)/src)
#ifdef NeedAllMesaSrc
LinkSourceFile(glapi.c, $(MESASRCDIR)/src)
LinkSourceFile(glthread.c, $(MESASRCDIR)/src)
#endif
LinkSourceFile(hash.c, $(MESASRCDIR)/src)
LinkSourceFile(hint.c, $(MESASRCDIR)/src)
LinkSourceFile(histogram.c, $(MESASRCDIR)/src)
LinkSourceFile(image.c, $(MESASRCDIR)/src)
LinkSourceFile(imports.c, $(MESASRCDIR)/src)
LinkSourceFile(light.c, $(MESASRCDIR)/src)
LinkSourceFile(lines.c, $(MESASRCDIR)/src)
LinkSourceFile(matrix.c, $(MESASRCDIR)/src)
LinkSourceFile(mem.c, $(MESASRCDIR)/src)
LinkSourceFile(mmath.c, $(MESASRCDIR)/src)
LinkSourceFile(pixel.c, $(MESASRCDIR)/src)
LinkSourceFile(points.c, $(MESASRCDIR)/src)
LinkSourceFile(polygon.c, $(MESASRCDIR)/src)
LinkSourceFile(rastpos.c, $(MESASRCDIR)/src)
LinkSourceFile(state.c, $(MESASRCDIR)/src)
LinkSourceFile(stencil.c, $(MESASRCDIR)/src)
LinkSourceFile(texformat.c, $(MESASRCDIR)/src)
LinkSourceFile(teximage.c, $(MESASRCDIR)/src)
LinkSourceFile(texobj.c, $(MESASRCDIR)/src)
LinkSourceFile(texstate.c, $(MESASRCDIR)/src)
LinkSourceFile(texstore.c, $(MESASRCDIR)/src)
LinkSourceFile(texutil.c, $(MESASRCDIR)/src)
LinkSourceFile(varray.c, $(MESASRCDIR)/src)
LinkSourceFile(vtxfmt.c, $(MESASRCDIR)/src)
#endif

#ifdef NeedAllMesaSrc
 DISPATCHOBJS = $(MESABUILDDIR)dispatch.o
    GLAPIOBJS = $(MESABUILDDIR)glapi.o
GLAPINOOPOBJS = $(MESABUILDDIR)glapinoop.o
 GLTHREADOBJS = $(MESABUILDDIR)glthread.o
#endif

COREMESABASEOBJS = \
		$(MESABUILDDIR)accum.o \
		$(MESABUILDDIR)api_arrayelt.o \
		$(MESABUILDDIR)api_eval.o \
		$(MESABUILDDIR)api_loopback.o \
		$(MESABUILDDIR)api_noop.o \
		$(MESABUILDDIR)api_validate.o \
		$(MESABUILDDIR)attrib.o \
		$(MESABUILDDIR)blend.o \
		$(MESABUILDDIR)buffers.o \
		$(MESABUILDDIR)clip.o \
		$(MESABUILDDIR)colortab.o \
		$(MESABUILDDIR)config.o \
		$(MESABUILDDIR)context.o \
		$(MESABUILDDIR)convolve.o \
		$(MESABUILDDIR)debug.o \
		$(MESABUILDDIR)depth.o \
		$(DISPATCHOBJS) \
		$(MESABUILDDIR)dlist.o \
		$(MESABUILDDIR)drawpix.o \
		$(MESABUILDDIR)enable.o \
		$(MESABUILDDIR)enums.o \
		$(MESABUILDDIR)eval.o \
		$(MESABUILDDIR)extensions.o \
		$(MESABUILDDIR)feedback.o \
		$(MESABUILDDIR)fog.o \
		$(MESABUILDDIR)get.o \
		$(GLAPIOBJS) \
		$(GLTHREADOBJS) \
		$(MESABUILDDIR)hash.o \
		$(MESABUILDDIR)hint.o \
		$(MESABUILDDIR)histogram.o \
		$(MESABUILDDIR)image.o \
		$(MESABUILDDIR)imports.o \
		$(MESABUILDDIR)light.o \
		$(MESABUILDDIR)lines.o \
		$(MESABUILDDIR)matrix.o \
		$(MESABUILDDIR)mem.o \
		$(MESABUILDDIR)mmath.o \
		$(MESABUILDDIR)pixel.o \
		$(MESABUILDDIR)points.o \
		$(MESABUILDDIR)polygon.o \
		$(MESABUILDDIR)rastpos.o \
		$(MESABUILDDIR)state.o \
		$(MESABUILDDIR)stencil.o \
		$(MESABUILDDIR)texformat.o \
		$(MESABUILDDIR)teximage.o \
		$(MESABUILDDIR)texobj.o \
		$(MESABUILDDIR)texstate.o \
		$(MESABUILDDIR)texstore.o \
		$(MESABUILDDIR)texutil.o \
		$(MESABUILDDIR)varray.o \
		$(MESABUILDDIR)vtxfmt.o

COREMESAOBJS = $(COREMESABASEOBJS) \
		$(MESA_AC_OBJS) \
		$(MESA_MATH_OBJS) \
		$(MESA_SWRAST_OBJS) \
		$(MESA_SWR_SETUP_OBJS) \
		$(MESA_TNL_OBJS)

#ifdef NeedAllMesaSrc
 DISPATCHUOBJS = $(MESABUILDDIR)unshared/dispatch.o
    GLAPIUOBJS = $(MESABUILDDIR)unshared/glapi.o
GLAPINOOPUOBJS = $(MESABUILDDIR)unshared/glapinoop.o
 GLTHREADUOBJS = $(MESABUILDDIR)unshared/glthread.o
#endif

#if defined(DoSharedLib) && DoSharedLib
COREMESABASEUOBJS = $(MESABUILDDIR)unshared/accum.o \
		$(MESABUILDDIR)unshared/api_arrayelt.o \
		$(MESABUILDDIR)unshared/api_eval.o \
		$(MESABUILDDIR)unshared/api_loopback.o \
		$(MESABUILDDIR)unshared/api_noop.o \
		$(MESABUILDDIR)unshared/api_validate.o \
		$(MESABUILDDIR)unshared/attrib.o \
		$(MESABUILDDIR)unshared/blend.o \
		$(MESABUILDDIR)unshared/buffers.o \
		$(MESABUILDDIR)unshared/clip.o \
		$(MESABUILDDIR)unshared/colortab.o \
		$(MESABUILDDIR)unshared/config.o \
		$(MESABUILDDIR)unshared/context.o \
		$(MESABUILDDIR)unshared/convolve.o \
		$(MESABUILDDIR)unshared/debug.o \
		$(MESABUILDDIR)unshared/depth.o \
		$(DISPATCHUOBJS) \
		$(MESABUILDDIR)unshared/dlist.o \
		$(MESABUILDDIR)unshared/drawpix.o \
		$(MESABUILDDIR)unshared/enable.o \
		$(MESABUILDDIR)unshared/enums.o \
		$(MESABUILDDIR)unshared/eval.o \
		$(MESABUILDDIR)unshared/extensions.o \
		$(MESABUILDDIR)unshared/feedback.o \
		$(MESABUILDDIR)unshared/fog.o \
		$(MESABUILDDIR)unshared/get.o \
		$(GLAPIUOBJS) \
		$(GLTHREADUOBJS) \
		$(MESABUILDDIR)unshared/hash.o \
		$(MESABUILDDIR)unshared/hint.o \
		$(MESABUILDDIR)unshared/histogram.o \
		$(MESABUILDDIR)unshared/image.o \
		$(MESABUILDDIR)unshared/imports.o \
		$(MESABUILDDIR)unshared/light.o \
		$(MESABUILDDIR)unshared/lines.o \
		$(MESABUILDDIR)unshared/matrix.o \
		$(MESABUILDDIR)unshared/mem.o \
		$(MESABUILDDIR)unshared/mmath.o \
		$(MESABUILDDIR)unshared/pixel.o \
		$(MESABUILDDIR)unshared/points.o \
		$(MESABUILDDIR)unshared/polygon.o \
		$(MESABUILDDIR)unshared/rastpos.o \
		$(MESABUILDDIR)unshared/state.o \
		$(MESABUILDDIR)unshared/stencil.o \
		$(MESABUILDDIR)unshared/texformat.o \
		$(MESABUILDDIR)unshared/teximage.o \
		$(MESABUILDDIR)unshared/texobj.o \
		$(MESABUILDDIR)unshared/texstate.o \
		$(MESABUILDDIR)unshared/texstore.o \
		$(MESABUILDDIR)unshared/texutil.o \
		$(MESABUILDDIR)unshared/varray.o \
		$(MESABUILDDIR)unshared/vtxfmt.o \
#else
COREMESABASEUOBJS = $(COREMESABASEOBJS)
#endif

COREMESAUOBJS = $(COREMESABASEUOBJS) \
		$(MESA_AC_UOBJS) \
		$(MESA_MATH_UOBJS) \
		$(MESA_SWRAST_UOBJS) \
		$(MESA_SWR_SETUP_UOBJS) \
		$(MESA_TNL_UOBJS)

#ifdef NeedAllMesaSrc
 DISPATCHDOBJS = $(MESABUILDDIR)debugger/dispatch.o
    GLAPIDOBJS = $(MESABUILDDIR)debugger/glapi.o
GLAPINOOPDOBJS = $(MESABUILDDIR)debugger/glapinoop.o
 GLTHREADDOBJS = $(MESABUILDDIR)debugger/glthread.o
#endif

COREMESABASEDOBJS = $(MESABUILDDIR)debugger/accum.o \
		$(MESABUILDDIR)debugger/api_arrayelt.o \
		$(MESABUILDDIR)debugger/api_eval.o \
		$(MESABUILDDIR)debugger/api_loopback.o \
		$(MESABUILDDIR)debugger/api_noop.o \
		$(MESABUILDDIR)debugger/api_validate.o \
		$(MESABUILDDIR)debugger/attrib.o \
		$(MESABUILDDIR)debugger/blend.o \
		$(MESABUILDDIR)debugger/buffers.o \
		$(MESABUILDDIR)debugger/clip.o \
		$(MESABUILDDIR)debugger/colortab.o \
		$(MESABUILDDIR)debugger/config.o \
		$(MESABUILDDIR)debugger/context.o \
		$(MESABUILDDIR)debugger/convolve.o \
		$(MESABUILDDIR)debugger/debug.o \
		$(MESABUILDDIR)debugger/depth.o \
		$(DISPATCHDOBJS) \
		$(MESABUILDDIR)debugger/dlist.o \
		$(MESABUILDDIR)debugger/drawpix.o \
		$(MESABUILDDIR)debugger/enable.o \
		$(MESABUILDDIR)debugger/enums.o \
		$(MESABUILDDIR)debugger/eval.o \
		$(MESABUILDDIR)debugger/extensions.o \
		$(MESABUILDDIR)debugger/feedback.o \
		$(MESABUILDDIR)debugger/fog.o \
		$(MESABUILDDIR)debugger/get.o \
		$(GLAPIDOBJS) \
		$(GLTHREADDOBJS) \
		$(MESABUILDDIR)debugger/hash.o \
		$(MESABUILDDIR)debugger/hint.o \
		$(MESABUILDDIR)debugger/histogram.o \
		$(MESABUILDDIR)debugger/image.o \
		$(MESABUILDDIR)debugger/imports.o \
		$(MESABUILDDIR)debugger/light.o \
		$(MESABUILDDIR)debugger/lines.o \
		$(MESABUILDDIR)debugger/matrix.o \
		$(MESABUILDDIR)debugger/mem.o \
		$(MESABUILDDIR)debugger/mmath.o \
		$(MESABUILDDIR)debugger/pixel.o \
		$(MESABUILDDIR)debugger/points.o \
		$(MESABUILDDIR)debugger/polygon.o \
		$(MESABUILDDIR)debugger/rastpos.o \
		$(MESABUILDDIR)debugger/state.o \
		$(MESABUILDDIR)debugger/stencil.o \
		$(MESABUILDDIR)debugger/texformat.o \
		$(MESABUILDDIR)debugger/teximage.o \
		$(MESABUILDDIR)debugger/texobj.o \
		$(MESABUILDDIR)debugger/texstate.o \
		$(MESABUILDDIR)debugger/texstore.o \
		$(MESABUILDDIR)debugger/texutil.o \
		$(MESABUILDDIR)debugger/varray.o \
		$(MESABUILDDIR)debugger/vtxfmt.o \

COREMESADOBJS = $(COREMESABASEDOBJS) \
		$(MESA_AC_DOBJS) \
		$(MESA_MATH_DOBJS) \
		$(MESA_SWRAST_DOBJS) \
		$(MESA_SWR_SETUP_DOBJS) \
		$(MESA_TNL_DOBJS)

#ifdef NeedAllMesaSrc
 DISPATCHPOBJS = $(MESABUILDDIR)profiled/dispatch.o
    GLAPIPOBJS = $(MESABUILDDIR)profiled/glapi.o
GLAPINOOPPOBJS = $(MESABUILDDIR)profiled/glapinoop.o
 GLTHREADPOBJS = $(MESABUILDDIR)profiled/glthread.o
#endif

COREMESABASEPOBJS = $(MESABUILDDIR)profiled/accum.o \
		$(MESABUILDDIR)profiled/api_arrayelt.o \
		$(MESABUILDDIR)profiled/api_eval.o \
		$(MESABUILDDIR)profiled/api_loopback.o \
		$(MESABUILDDIR)profiled/api_noop.o \
		$(MESABUILDDIR)profiled/api_validate.o \
		$(MESABUILDDIR)profiled/attrib.o \
		$(MESABUILDDIR)profiled/blend.o \
		$(MESABUILDDIR)profiled/buffers.o \
		$(MESABUILDDIR)profiled/clip.o \
		$(MESABUILDDIR)profiled/colortab.o \
		$(MESABUILDDIR)profiled/config.o \
		$(MESABUILDDIR)profiled/context.o \
		$(MESABUILDDIR)profiled/convolve.o \
		$(MESABUILDDIR)profiled/debug.o \
		$(MESABUILDDIR)profiled/depth.o \
		$(DISPATCHPOBJS) \
		$(MESABUILDDIR)profiled/dlist.o \
		$(MESABUILDDIR)profiled/drawpix.o \
		$(MESABUILDDIR)profiled/enable.o \
		$(MESABUILDDIR)profiled/enums.o \
		$(MESABUILDDIR)profiled/eval.o \
		$(MESABUILDDIR)profiled/extensions.o \
		$(MESABUILDDIR)profiled/feedback.o \
		$(MESABUILDDIR)profiled/fog.o \
		$(MESABUILDDIR)profiled/get.o \
		$(GLAPIPOBJS) \
		$(GLTHREADPOBJS) \
		$(MESABUILDDIR)profiled/hash.o \
		$(MESABUILDDIR)profiled/hint.o \
		$(MESABUILDDIR)profiled/histogram.o \
		$(MESABUILDDIR)profiled/image.o \
		$(MESABUILDDIR)profiled/imports.o \
		$(MESABUILDDIR)profiled/light.o \
		$(MESABUILDDIR)profiled/lines.o \
		$(MESABUILDDIR)profiled/matrix.o \
		$(MESABUILDDIR)profiled/mem.o \
		$(MESABUILDDIR)profiled/mmath.o \
		$(MESABUILDDIR)profiled/pixel.o \
		$(MESABUILDDIR)profiled/points.o \
		$(MESABUILDDIR)profiled/polygon.o \
		$(MESABUILDDIR)profiled/rastpos.o \
		$(MESABUILDDIR)profiled/state.o \
		$(MESABUILDDIR)profiled/stencil.o \
		$(MESABUILDDIR)profiled/texformat.o \
		$(MESABUILDDIR)profiled/teximage.o \
		$(MESABUILDDIR)profiled/texobj.o \
		$(MESABUILDDIR)profiled/texstate.o \
		$(MESABUILDDIR)profiled/texstore.o \
		$(MESABUILDDIR)profiled/texutil.o \
		$(MESABUILDDIR)profiled/varray.o \
		$(MESABUILDDIR)profiled/vtxfmt.o \

COREMESAPOBJS = $(COREMESABASEPOBJS) \
		$(MESA_AC_POBJS) \
		$(MESA_MATH_POBJS) \
		$(MESA_SWRAST_POBJS) \
		$(MESA_SWR_SETUP_POBJS) \
		$(MESA_TNL_POBJS)

