/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_mesa.h,v 1.4 2002/10/30 12:52:00 alanh Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifndef _sis_mesa_h_
#define _sis_mesa_h_

void sis_UpdateState (GLcontext * ctx);
void sis_ClearIndex (GLcontext * ctx, GLuint index);
void sis_ClearColor (GLcontext * ctx, GLubyte red, GLubyte green,
		     GLubyte blue, GLubyte alpha);
GLbitfield sis_Clear (GLcontext * ctx, GLbitfield mask, GLboolean all,
		      GLint x, GLint y, GLint width, GLint height);
GLboolean sis_SetDrawBuffer (GLcontext * ctx, GLenum mode);
void sis_SetReadBuffer (GLcontext *ctx, GLframebuffer *colorBuffer,
                        GLenum buffer);
void sis_GetBufferSize (GLframebuffer * buffer, GLuint * width, GLuint * height);
const char *sis_ExtensionString (GLcontext * ctx);
const GLubyte *sis_GetString (GLcontext * ctx, GLenum name);
void sis_Finish (GLcontext * ctx);
void sis_Flush (GLcontext * ctx);
GLboolean sis_IndexMask (GLcontext * ctx, GLuint mask);
GLboolean sis_ColorMask (GLcontext * ctx,
			 GLboolean rmask, GLboolean gmask,
			 GLboolean bmask, GLboolean amask);
GLboolean sis_LogicOp (GLcontext * ctx, GLenum op);
void sis_Dither (GLcontext * ctx, GLboolean enable);
void sis_Error (GLcontext * ctx);
void sis_NearFar (GLcontext * ctx, GLfloat nearVal, GLfloat farVal);
GLint sis_GetParameteri (const GLcontext * ctx, GLint param);
GLboolean sis_DrawPixels (GLcontext * ctx,
			  GLint x, GLint y, GLsizei width, GLsizei height,
			  GLenum format, GLenum type,
			  const struct gl_pixelstore_attrib *unpack,
			  const GLvoid * pixels);
GLboolean sis_Bitmap (GLcontext * ctx,
		      GLint x, GLint y, GLsizei width, GLsizei height,
		      const struct gl_pixelstore_attrib *unpack,
		      const GLubyte * bitmap);
void sis_RenderStart (GLcontext * ctx);
void sis_RenderFinish (GLcontext * ctx);

typedef void (*sis_setup_func_t)(struct vertex_buffer *, GLuint, GLuint);
sis_setup_func_t sis_ChooseRasterSetupFunc (GLcontext * ctx);

void sis_ReducedPrimitiveChange (GLcontext * ctx, GLenum primitive);
GLboolean sis_MultipassFunc (struct vertex_buffer *VB, GLuint passno);

/* 
 * Texture
 */
void sis_validate_texture (GLcontext * ctx);

void sis_TexEnv( GLcontext *ctx, GLenum target, GLenum pname, 
                 const GLfloat *param );

void sis_TexImage (GLcontext * ctx, GLenum target,
		   struct gl_texture_object *tObj, GLint level,
		   GLint internalFormat,
		   const struct gl_texture_image *image);

void sis_TexSubImage (GLcontext * ctx, GLenum target,
		      struct gl_texture_object *tObj, GLint level,
		      GLint xoffset, GLint yoffset,
		      GLsizei width, GLsizei height,
		      GLint internalFormat,
		      const struct gl_texture_image *image);

void sis_TexParameter (GLcontext * ctx, GLenum target,
		       struct gl_texture_object *tObj,
		       GLenum pname, const GLfloat * params);

void sis_BindTexture (GLcontext * ctx, GLenum target,
		      struct gl_texture_object *tObj);

void sis_DeleteTexture (GLcontext * ctx, struct gl_texture_object *tObj);

void sis_UpdateTexturePalette (GLcontext * ctx,
			       struct gl_texture_object *tObj);

void sis_UseGlobalTexturePalette (GLcontext * ctx, GLboolean state);

void sis_ActiveTexture (GLcontext * ctx, GLuint texUnitNumber);

GLboolean sis_IsTextureResident (GLcontext * ctx,
				 struct gl_texture_object *t);
void sis_PrioritizeTexture (GLcontext * ctx,
			    struct gl_texture_object *t, GLclampf priority);

/* 
 * Optimization
 */
void sis_RegisterVB (struct vertex_buffer *VB);
void sis_UnregisterVB (struct vertex_buffer *VB);
void sis_ResetVB (struct vertex_buffer *VB);
void sis_ResetCvaVB (struct vertex_buffer *VB, GLuint stages);
GLuint sis_RegisterPipelineStages (struct gl_pipeline_stage *out,
				   const struct gl_pipeline_stage *in,
				   GLuint nr);
GLboolean sis_BuildPrecalcPipeline (GLcontext * ctx);
GLboolean sis_BuildEltPipeline (GLcontext * ctx);
void sis_OptimizePrecalcPipeline (GLcontext * ctx, struct gl_pipeline *pipe);
void sis_OptimizeImmediatePipeline (GLcontext * ctx,
				    struct gl_pipeline *pipe);

/* 
 * State-Change
 */
void sis_AlphaFunc (GLcontext * ctx, GLenum func, GLclampf ref);
void sis_BlendEquation (GLcontext * ctx, GLenum mode);
void sis_BlendFunc (GLcontext * ctx, GLenum sfactor, GLenum dfactor);
void sis_BlendFuncSeparate (GLcontext * ctx, GLenum sfactorRGB,
			    GLenum dfactorRGB, GLenum sfactorA,
			    GLenum dfactorA);
void sis_ClearDepth (GLcontext * ctx, GLclampd d);
void sis_CullFace (GLcontext * ctx, GLenum mode);
void sis_FrontFace (GLcontext * ctx, GLenum mode);
void sis_DepthFunc (GLcontext * ctx, GLenum func);
void sis_DepthMask (GLcontext * ctx, GLboolean flag);
void sis_DepthRange (GLcontext * ctx, GLclampd nearval, GLclampd farval);
void sis_Enable (GLcontext * ctx, GLenum cap, GLboolean state);
void sis_Fogfv (GLcontext * ctx, GLenum pname, const GLfloat * params);
void sis_Hint (GLcontext * ctx, GLenum target, GLenum mode);
void sis_Lightfv (GLcontext * ctx, GLenum light,
		  GLenum pname, const GLfloat * params, GLint nparams);
void sis_LightModelfv (GLcontext * ctx, GLenum pname, const GLfloat * params);
void sis_PolygonMode (GLcontext * ctx, GLenum face, GLenum mode);
void sis_Scissor (GLcontext * ctx, GLint x, GLint y, GLsizei w, GLsizei h);
void sis_ShadeModel (GLcontext * ctx, GLenum mode);
void sis_ClearStencil (GLcontext * ctx, GLint s);
void sis_StencilFunc (GLcontext * ctx, GLenum func, GLint ref, GLuint mask);
void sis_StencilMask (GLcontext * ctx, GLuint mask);
void sis_StencilOp (GLcontext * ctx, GLenum fail, GLenum zfail, GLenum zpass);
void sis_Viewport (GLcontext * ctx, GLint x, GLint y, GLsizei w, GLsizei h);

/* 
 * Buffer Allocation
 */
void sis_alloc_z_stencil_buffer (GLcontext * ctx);
void sis_free_z_stencil_buffer (XMesaBuffer buf);
void sis_alloc_back_image (GLcontext * ctx, XMesaImage *image, void **free,
                           ENGPACKET *packet);
void sis_free_back_image (XMesaBuffer buf, XMesaImage *image, void *free);
void sis_alloc_texture_image (GLcontext * ctx, GLtextureImage * image);
void sis_free_texture_image (GLtextureImage * image);

/* 
 * Raster-Function Picker
 */
void sis_set_render_func (GLcontext * ctx);

/* 
 * VB Rendering Function
 */
void sis_init_vbrender (void);
void sis_set_render_vb_tabs (GLcontext * ctx);
 
/* 
 * Local
 */
void set_color_pattern (__GLSiScontext * hwcx, GLubyte red, GLubyte green,
			GLubyte blue, GLubyte alpha);

void set_z_stencil_pattern (__GLSiScontext * hwcx, GLclampd z, int stencil);

void sis_init_driver (GLcontext * ctx);
void sis_sw_init_driver (GLcontext * ctx);
void sis_sw_set_zfuncs_static (GLcontext * ctx);

void WaitEngIdle (__GLSiScontext * hwcx);
void Wait2DEngIdle (__GLSiScontext * hwcx);

void sis_set_buffer_static (GLcontext * ctx);
void sis_set_render_pos(GLcontext * ctx, GLubyte *base, GLuint pitch);

/* 
 * X window system Information
 */
#define sis_is_window(xmesa) (!((xmesa)->xm_buffer->pixmap_flag))
GLboolean sis_get_clip_rects (XMesaContext xmesa, BoxPtr * ppExtents,
			      int *pCount);
void *sis_get_drawable_pos (XMesaContext xmesa);
void sis_get_drawable_origin (XMesaContext xmesa, GLuint * x, GLuint * y);
void sis_get_drawable_size (XMesaContext xmesa, GLuint * w, GLuint * h);
void sis_get_drawable_box (XMesaContext xmesa, BoxPtr pBox);

/* GLX */
void SiSCreateContext (XMesaContext xmesa);
void SiSDestroyContext (XMesaContext xmesa);
void sis_swap_buffers (XMesaBuffer b);
void SiSDestroyBuffer (XMesaBuffer buf);

/* Profile */
#ifndef SIS_DEBUG
# define SIS_VERBOSE 0
#else
extern int SIS_VERBOSE; 
#endif

enum _sis_verbose {
	VERBOSE_SIS_BUFFER  = 0x1,
	VERBOSE_SIS_MEMORY  = 0x2
}; 

#endif
