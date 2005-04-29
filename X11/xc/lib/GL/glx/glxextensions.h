/*
 * (c) Copyright IBM Corporation 2002
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Ian Romanick <idr@us.ibm.com>
 */
/* $XFree86: xc/lib/GL/glx/glxextensions.h,v 1.3 2003/10/23 15:30:09 tsi Exp $ */

#ifndef GLX_GLXEXTENSIONS_H
#define GLX_GLXEXTENSIONS_H

enum {
   ARB_get_proc_address_bit = 0,
   ARB_multisample_bit,
   ARB_render_texture_bit,
   ATI_pixel_format_float_bit,
   EXT_visual_info_bit,
   EXT_visual_rating_bit,
   EXT_import_context_bit,
   MESA_agp_offset_bit,
   MESA_allocate_memory_bit, /* Replaces MESA_agp_offset & NV_vertex_array_range */
   MESA_copy_sub_buffer_bit,
   MESA_depth_float_bit,
   MESA_pixmap_colormap_bit,
   MESA_release_buffers_bit,
   MESA_set_3dfx_mode_bit,
   MESA_swap_control_bit,
   MESA_swap_frame_usage_bit,
   NV_float_buffer_bit,
   NV_render_depth_texture_bit,
   NV_render_texture_rectangle_bit,
   NV_vertex_array_range_bit,
   OML_swap_method_bit,
   OML_sync_control_bit,
   SGI_cushion_bit,
   SGI_make_current_read_bit,
   SGI_swap_control_bit,
   SGI_video_sync_bit,
   SGIS_blended_overlay_bit,
   SGIS_color_range_bit,
   SGIS_multisample_bit,
   SGIX_dm_buffer_bit,
   SGIX_fbconfig_bit,
   SGIX_pbuffer_bit,
   SGIX_swap_barrier_bit,
   SGIX_swap_group_bit,
   SGIX_video_resize_bit,
   SGIX_video_source_bit,
   SGIX_visual_select_group_bit,
   SUN_get_transparent_index_bit
};

extern GLboolean __glXExtensionBitIsEnabled( __GLXscreenConfigs *psc, unsigned bit );
extern const char * __glXGetClientExtensions( void );
extern void __glXCalculateUsableExtensions( __GLXscreenConfigs *psc,
    GLboolean display_is_direct_capable, int server_minor_version );
extern void __glXScrEnableExtension( __GLXscreenConfigs *psc, const char * name );

/* The void pointers here are because __GLXscreenConfigs is opaque for
 * DRI drivers.
 */
typedef void (* PFNGLXSCRENABLEEXTENSIONPROC) ( void *psc, const char * name );

/* Source-level backwards compatibility with old drivers. They won't
 * find the respective functions, though. 
 */
typedef void (* PFNGLXENABLEEXTENSIONPROC) ( const char * name,
    GLboolean force_client );
typedef void (* PFNGLXDISABLEEXTENSIONPROC) ( const char * name );


#ifdef GLX_NO_STATIC_EXTENSION_FUNCTIONS
# define GLX_ALIAS(return_type, real_func, proto_args, args, aliased_func)
#else
# if defined(__GNUC__) && !defined(GLX_ALIAS_UNSUPPORTED)
#  define GLX_ALIAS(return_type, real_func, proto_args, args, aliased_func) \
	return_type GLX_PREFIX( real_func ) proto_args \
	__attribute__ ((alias( # aliased_func ) ));
# else
#  define GLX_ALIAS(return_type, real_func, proto_args, args, aliased_func) \
	return_type GLX_PREFIX( real_func ) proto_args \
	{ return aliased_func args ; }
# endif /* __GNUC__ */
#endif /* GLX_NO_STATIC_EXTENSION_FUNCTIONS */

#endif /* GLX_GLXEXTENSIONS_H */
