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
/* $XFree86: xc/lib/GL/glx/glxextensions.c,v 1.2 2004/01/28 21:18:48 alanh Exp $ */

#include "packsingle.h"
#include "glxclient.h"
#include <extensions/extutil.h>
#include <extensions/Xext.h>
#include <string.h>
#include "glapi.h"
#include "glxextensions.h"
#include "simple_list.h"

#define SET_BIT(m,b)   (m[ (b) / 8 ] |=  (1U << ((b) % 8)))
#define CLR_BIT(m,b)   (m[ (b) / 8 ] &= ~(1U << ((b) % 8)))
#define IS_SET(m,b)    ((m[ (b) / 8 ] & (1U << ((b) % 8))) != 0)
#define CONCAT(a,b) a ## b
#define GLX(n) "GLX_" # n, 4 + sizeof( # n ) - 1, CONCAT(n,_bit)
#define VER(a,b)  a, b
#define Y  1
#define N  0
#define EXT_ENABLED(bit,supported) ((bit < 255) && IS_SET( supported, bit ))

static const struct {
   const char * const name;
   unsigned   name_len;

   unsigned char  bit;

   /* This is the lowest version of GLX that "requires" this extension.
    * For example, GLX 1.3 requires SGIX_fbconfig, SGIX_pbuffer, and
    * SGI_make_current_read.  If the extension is not required by any known
    * version of GLX, use 0, 0.
    */
   unsigned char  version_major;
   unsigned char  version_minor;
   unsigned char  client_support;
   unsigned char  direct_support;
   unsigned char  client_only;     /** Is the extension client-side only? */
   unsigned char  direct_only;     /** Is the extension for direct
				      * contexts only?
				      */
} known_glx_extensions[] = {
   { GLX(ARB_get_proc_address),        VER(1,4), Y, N, Y, N },
   { GLX(ARB_multisample),             VER(1,4), Y, N, N, N },
   { GLX(ARB_render_texture),          VER(0,0), N, N, N, N },
   { GLX(ATI_pixel_format_float),      VER(0,0), N, N, N, N },
   { GLX(EXT_import_context),          VER(0,0), Y, Y, N, N },
   { GLX(EXT_visual_info),             VER(0,0), Y, Y, N, N },
   { GLX(EXT_visual_rating),           VER(0,0), Y, Y, N, N },
   { GLX(MESA_agp_offset),             VER(0,0), N, N, N, Y }, /* Deprecated */
   { GLX(MESA_allocate_memory),        VER(0,0), Y, N, N, Y },
   { GLX(MESA_copy_sub_buffer),        VER(0,0), N, N, N, N }, /* Deprecated? */
   { GLX(MESA_pixmap_colormap),        VER(0,0), N, N, N, N }, /* Deprecated */
   { GLX(MESA_release_buffers),        VER(0,0), N, N, N, N }, /* Deprecated */
   { GLX(MESA_set_3dfx_mode),          VER(0,0), N, N, N, N }, /* Deprecated */
   { GLX(MESA_swap_control),           VER(0,0), Y, N, N, Y },
   { GLX(MESA_swap_frame_usage),       VER(0,0), Y, N, N, Y },
   { GLX(NV_float_buffer),             VER(0,0), N, N, N, N },
   { GLX(NV_render_depth_texture),     VER(0,0), N, N, N, N },
   { GLX(NV_render_texture_rectangle), VER(0,0), N, N, N, N },
   { GLX(NV_vertex_array_range),       VER(0,0), N, N, N, Y }, /* Deprecated */
   { GLX(OML_swap_method),             VER(0,0), Y, N, N, N },
   { GLX(OML_sync_control),            VER(0,0), Y, N, N, Y },
   { GLX(SGI_cushion),                 VER(0,0), N, N, N, N },
   { GLX(SGI_make_current_read),       VER(1,3), Y, N, N, N },
   { GLX(SGI_swap_control),            VER(0,0), Y, N, N, N },
   { GLX(SGI_video_sync),              VER(0,0), Y, N, N, Y },
   { GLX(SGIS_blended_overlay),        VER(0,0), N, N, N, N },
   { GLX(SGIS_color_range),            VER(0,0), N, N, N, N },
   { GLX(SGIS_multisample),            VER(0,0), Y, N, N, N },
   { GLX(SGIX_dm_buffer),              VER(0,0), N, N, N, N },
   { GLX(SGIX_fbconfig),               VER(1,3), Y, N, N, N },
   { GLX(SGIX_pbuffer),                VER(1,3), N, N, N, N },
   { GLX(SGIX_swap_barrier),           VER(0,0), N, N, N, N },
   { GLX(SGIX_swap_group),             VER(0,0), N, N, N, N },
   { GLX(SGIX_video_resize),           VER(0,0), N, N, N, N },
   { GLX(SGIX_video_source),           VER(0,0), N, N, N, N },
   { GLX(SGIX_visual_select_group),    VER(0,0), Y, N, N, N },
   { GLX(SUN_get_transparent_index),   VER(0,0), N, N, N, N },
   { NULL }
};

/* global bit-fields of available extensions and their characteristics */
static unsigned char client_support[8];
static unsigned char client_only[8];
static unsigned char direct_only[8];

/**
 * Bits representing the set of extensions that are enabled by default in all
 * direct rendering drivers.
 */
static unsigned char direct_support[8];

/* client extensions string */
static const char * __glXGLXClientExtensions = NULL;

static void __glXExtensionsCtr( void );
static void __glXExtensionsCtrScreen( __GLXscreenConfigs *psc );
static void __glXProcessServerString( const char * server_string,
    unsigned char * server_support );

/**
 * Set the state of a GLX extension.
 *
 * \param name      Name of the extension.
 * \param name_len  Length, in characters, of the extension name.
 * \param state     New state (either enabled or disabled) of the extension.
 * \param supported Table in which the state of the extension is to be set.
 */
static void
set_glx_extension( const char * name, unsigned name_len, GLboolean state,
		   unsigned char * supported )
{
   unsigned   i;


   for ( i = 0 ; known_glx_extensions[i].name != NULL ; i++ ) {
      if ( (name_len == known_glx_extensions[i].name_len)
	   && (strncmp( known_glx_extensions[i].name, name, name_len ) == 0) ) {
	 if ( state ) {
	    SET_BIT( supported, known_glx_extensions[i].bit );
	 }
	 else {
	    CLR_BIT( supported, known_glx_extensions[i].bit );
	 }

	 return;
      }
   }
}


#define NUL '\0'
#define SEPARATOR ' '

/**
 * Convert the server's extension string to a bit-field.
 * 
 * \param server_string   GLX extension string from the server.
 * \param server_support  Bit-field of supported extensions.
 */
static void
__glXProcessServerString( const char * server_string,
			  unsigned char * server_support )
{
   unsigned  base;
   unsigned  len;

   (void) memset( server_support, 0, sizeof( server_support ) );
   
   for ( base = 0 ; server_string[ base ] != NUL ; /* empty */ ) {
      /* Determine the length of the next extension name.
       */
      for ( len = 0
	    ; (server_string[ base + len ] != SEPARATOR)
	    && (server_string[ base + len ] != NUL)
	    ; len++ ) {
	 /* empty */
      }

      /* Set the bit for the extension in the server_support table.
       */
      set_glx_extension( & server_string[ base ], len, GL_TRUE,
			 server_support );
      
      /* Advance to the next extension string.  This means that we skip
       * over the previous string and any trialing white-space.
       */
      for ( base += len ;
	    (server_string[ base ] == SEPARATOR)
	    && (server_string[ base ] != NUL)
	    ; base++ ) {
	 /* empty */
      }
   }
}


/**
 * Enable a named GLX extension on a given screen.
 * 
 * \param psc   Pointer to GLX per-screen record.
 * \param name  Name of the extension to enable.
 */
void
__glXScrEnableExtension( __GLXscreenConfigs *psc, const char * name )
{
   __glXExtensionsCtr();
   __glXExtensionsCtrScreen(psc);
   set_glx_extension( name, strlen( name ), GL_TRUE, psc->direct_support );
}


/**
 * Initialize global extension support tables.
 */

static void
__glXExtensionsCtr( void )
{
   unsigned   i;
   static GLboolean ext_list_first_time = GL_TRUE;


   if ( ext_list_first_time ) {
      ext_list_first_time = GL_FALSE;

      (void) memset( client_support, 0, sizeof( client_support ) );
      (void) memset( direct_support, 0, sizeof( direct_support ) );
      (void) memset( client_only,    0, sizeof( client_only ) );
      (void) memset( direct_only,    0, sizeof( direct_only ) );

      for ( i = 0 ; known_glx_extensions[i].name != NULL ; i++ ) {
	 const unsigned  bit = known_glx_extensions[i].bit;

	 if ( known_glx_extensions[i].client_support ) {
	    SET_BIT( client_support, bit );
	 }

	 if ( known_glx_extensions[i].direct_support ) {
	    SET_BIT( direct_support, bit );
	 }

	 if ( known_glx_extensions[i].client_only ) {
	    SET_BIT( client_only, bit );
	 }

	 if ( known_glx_extensions[i].direct_only ) {
	    SET_BIT( direct_only, bit );
	 }
      }
   }
}


/**
 * Make sure that per-screen direct-support table is initialized.
 *
 * \param psc  Pointer to GLX per-screen record.
 */

static void
__glXExtensionsCtrScreen( __GLXscreenConfigs *psc )
{
    if (psc->ext_list_first_time) {
	psc->ext_list_first_time = GL_FALSE;
	(void) memcpy( psc->direct_support, direct_support,
		       sizeof( direct_support ) );
    }
}


/**
 * Check if a certain extension is enabled on a given screen.
 *
 * \param psc  Pointer to GLX per-screen record.
 * \param bit  Bit index in the direct-support table.
 * \returns If the extension bit is enabled for the screen, \c GL_TRUE is
 *          returned.  If the extension bit is not enabled or if \c psc is
 *          \c NULL, then \c GL_FALSE is returned.
 */
GLboolean
__glXExtensionBitIsEnabled( __GLXscreenConfigs *psc, unsigned bit )
{
   GLboolean enabled = GL_FALSE;

   if ( psc != NULL ) {
      __glXExtensionsCtr();
      __glXExtensionsCtrScreen( psc );
      enabled = EXT_ENABLED( bit, psc->direct_support );
   }

   return enabled;
}


/**
 * Convert a bit-field to a string of supported extensions.
 */
static char *
__glXGetStringFromTable( const unsigned char * supported )
{
   unsigned   i;
   unsigned   ext_str_len;
   char * ext_str;
   char * point;


   ext_str_len = 0;
   for ( i = 0 ; known_glx_extensions[i].name != NULL ; i++ ) {
      if ( EXT_ENABLED( known_glx_extensions[i].bit, supported ) ) {
	 ext_str_len += known_glx_extensions[i].name_len + 1;
      }
   }

   ext_str = Xmalloc( ext_str_len + 1 );
   if ( ext_str != NULL ) {
      point = ext_str;

      for ( i = 0 ; known_glx_extensions[i].name != NULL ; i++ ) {
	 if ( EXT_ENABLED( known_glx_extensions[i].bit, supported ) ) {
	    (void) memcpy( point, known_glx_extensions[i].name,
			   known_glx_extensions[i].name_len );
	    point += known_glx_extensions[i].name_len;

	    *point = ' ';
	    point++;
	 }
      }

      *point = '\0';
   }

   return ext_str;
}


/**
 * Get the string of client library supported extensions.
 */
const char *
__glXGetClientExtensions( void )
{
   if ( __glXGLXClientExtensions == NULL ) {
      __glXExtensionsCtr();
      __glXGLXClientExtensions = __glXGetStringFromTable( client_support );
   }

   return __glXGLXClientExtensions;
}


/**
 * Calculate the list of application usable extensions.  The resulting
 * string is stored in \c psc->effectiveGLXexts.
 * 
 * \param psc                        Pointer to GLX per-screen record.
 * \param display_is_direct_capable  True if the display is capable of
 *                                   direct rendering.
 * \param minor_version              GLX minor version from the server.
 * 
 * \todo Once libGL has full GLX 1.3 support, remove the SGI_make_current_read
 *       hack.
 */

void
__glXCalculateUsableExtensions( __GLXscreenConfigs *psc,
				GLboolean display_is_direct_capable,
				int minor_version )
{
   unsigned char server_support[8];
   unsigned char usable[8];
   unsigned      i;

   __glXExtensionsCtr();
   __glXExtensionsCtrScreen( psc );
   __glXProcessServerString( psc->serverGLXexts, server_support );


   /* This is a hack.  Some servers support GLX 1.3 but don't export
    * SGI_make_current_read.  This libGL supports SGI_make_current_read but
    * doesn't have full GLX 1.3 support.  Until GLX 1.3 is fully supported
    * this hack is done to export the functionality to applications.
    */

   if ( minor_version >= 3 ) {
      SET_BIT( server_support, SGI_make_current_read_bit );
   }


   /* An extension is supported if the client-side (i.e., libGL) supports
    * it and the "server" supports it.  In this case that means that either
    * the true server supports it or it is only for direct-rendering and
    * the direct rendering driver supports it.
    * 
    * If the display is not capable of direct rendering, then the extension
    * is enabled if and only if the client-side library and the server
    * support it.
    */

   if ( display_is_direct_capable ) {
      for ( i = 0 ; i < 8 ; i++ ) {
	 usable[i] = (client_support[i] & client_only[i])
	     | (client_support[i] & psc->direct_support[i] & server_support[i])
	     | (client_support[i] & psc->direct_support[i] & direct_only[i]);
      }
   }
   else {
      for ( i = 0 ; i < 8 ; i++ ) {
	 usable[i] = (client_support[i] & client_only[i])
	     | (client_support[i] & server_support[i]);
      }
   }

   psc->effectiveGLXexts = __glXGetStringFromTable( usable );
}
