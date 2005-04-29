/*
 * (C) Copyright IBM Corporation 2003
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
 * VA LINUX SYSTEM, IBM AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file glcontextmodes.c
 * \author Ian Romanick <idr@us.ibm.com>
 */

#include "glapi.h"
#include "glxclient.h"
#include "glcontextmodes.h"

#define NUM_VISUAL_TYPES   6

/**
 * Convert an X visual type to a GLX visual type.
 * 
 * \param visualType X visual type (i.e., \c TrueColor, \c StaticGray, etc.)
 *        to be converted.
 * \return If \c visualType is a valid X visual type, a GLX visual type will
 *         be returned.  Otherwise \c GLX_NONE will be returned.
 */

GLint
_gl_convert_from_x_visual_type( int visualType )
{
    static const int glx_visual_types[ NUM_VISUAL_TYPES ] = {
	GLX_STATIC_GRAY,  GLX_GRAY_SCALE,
	GLX_STATIC_COLOR, GLX_PSEUDO_COLOR,
	GLX_TRUE_COLOR,   GLX_DIRECT_COLOR
    };

    return ( (unsigned) visualType < NUM_VISUAL_TYPES )
	? glx_visual_types[ visualType ] : GLX_NONE;
}


/**
 * Convert a GLX visual type to an X visual type.
 * 
 * \param visualType GLX visual type (i.e., \c GLX_TRUE_COLOR, 
 *                   \c GLX_STATIC_GRAY, etc.) to be converted.
 * \return If \c visualType is a valid GLX visual type, an X visual type will
 *         be returned.  Otherwise -1 will be returned.
 */

GLint
_gl_convert_to_x_visual_type( int visualType )
{
    static const int x_visual_types[ NUM_VISUAL_TYPES ] = {
	StaticGray,  GrayScale,
	StaticColor, PseudoColor,
	TrueColor,   DirectColor
    };

    return ( (unsigned) (visualType - GLX_STATIC_GRAY) <= NUM_VISUAL_TYPES )
	? x_visual_types[ visualType - GLX_STATIC_GRAY ] : -1;
}


/**
 * Copy a GLX visual config structure to a GL context mode structure.  All
 * of the fields in \c config are copied to \c mode.  Additional fields in
 * \c mode that can be derrived from the fields of \c config (i.e.,
 * \c haveDepthBuffer) are also filled in.  The remaining fields in \c mode
 * that cannot be derrived are set to default values.
 * 
 * \param mode   Destination GL context mode.
 * \param config Source GLX visual config.
 */

void
_gl_copy_visual_to_context_mode( __GLcontextModes * mode,
				 const __GLXvisualConfig * config )
{
    (void) memset( mode, 0, sizeof( __GLcontextModes ) );

    mode->visualID = config->vid;
    mode->visualType = _gl_convert_from_x_visual_type( config->class );
    mode->xRenderable = GL_TRUE;
    mode->fbconfigID = GLX_DONT_CARE;
    mode->drawableType = GLX_WINDOW_BIT;

    mode->rgbMode = (config->rgba != 0);
    mode->renderType = (mode->rgbMode) ? GLX_RGBA_BIT : GLX_COLOR_INDEX_BIT;

    mode->colorIndexMode = !(mode->rgbMode);
    mode->doubleBufferMode = (config->doubleBuffer != 0);
    mode->stereoMode = (config->stereo != 0);

    mode->haveAccumBuffer = ((config->accumRedSize +
			      config->accumGreenSize +
			      config->accumBlueSize +
			      config->accumAlphaSize) > 0);
    mode->haveDepthBuffer = (config->depthSize > 0);
    mode->haveStencilBuffer = (config->stencilSize > 0);

    mode->redBits = config->redSize;
    mode->greenBits = config->greenSize;
    mode->blueBits = config->blueSize;
    mode->alphaBits = config->alphaSize;
    mode->redMask = config->redMask;
    mode->greenMask = config->greenMask;
    mode->blueMask = config->blueMask;
    mode->alphaMask = config->alphaMask;
    mode->rgbBits = config->bufferSize;
    mode->indexBits = config->bufferSize;

    mode->accumRedBits = config->accumRedSize;
    mode->accumGreenBits = config->accumGreenSize;
    mode->accumBlueBits = config->accumBlueSize;
    mode->accumAlphaBits = config->accumAlphaSize;
    mode->depthBits = config->depthSize;
    mode->stencilBits = config->stencilSize;

    mode->numAuxBuffers = config->auxBuffers;
    mode->level = config->level;

    mode->visualRating = config->visualRating;
    mode->transparentPixel = config->transparentPixel;
    mode->transparentRed   = config->transparentRed;
    mode->transparentGreen = config->transparentGreen;
    mode->transparentBlue  = config->transparentBlue;
    mode->transparentAlpha = config->transparentAlpha;
    mode->transparentIndex = config->transparentIndex;
}


/**
 * Get data from a GL context mode.
 * 
 * \param mode         GL context mode whose data is to be returned.
 * \param attribute    Attribute of \c mode that is to be returned.
 * \param value_return Location to store the data member of \c mode.
 * \return  If \c attribute is a valid attribute of \c mode, \c Success is
 *          returned.  Otherwise \c GLX_BAD_ATTRIBUTE is returned.
 */

int
_gl_get_context_mode_data(const __GLcontextModes *mode, int attribute,
			  int *value_return)
{
    switch (attribute) {
      case GLX_USE_GL:
	*value_return = GL_TRUE;
	return Success;
      case GLX_BUFFER_SIZE:
	*value_return = mode->rgbBits;
	return Success;
      case GLX_RGBA:
	*value_return = mode->rgbMode;
	return Success;
      case GLX_RED_SIZE:
	*value_return = mode->redBits;
	return Success;
      case GLX_GREEN_SIZE:
	*value_return = mode->greenBits;
	return Success;
      case GLX_BLUE_SIZE:
	*value_return = mode->blueBits;
	return Success;
      case GLX_ALPHA_SIZE:
	*value_return = mode->alphaBits;
	return Success;
      case GLX_DOUBLEBUFFER:
	*value_return = mode->doubleBufferMode;
	return Success;
      case GLX_STEREO:
	*value_return = mode->stereoMode;
	return Success;
      case GLX_AUX_BUFFERS:
	*value_return = mode->numAuxBuffers;
	return Success;
      case GLX_DEPTH_SIZE:
	*value_return = mode->depthBits;
	return Success;
      case GLX_STENCIL_SIZE:
	*value_return = mode->stencilBits;
	return Success;
      case GLX_ACCUM_RED_SIZE:
	*value_return = mode->accumRedBits;
	return Success;
      case GLX_ACCUM_GREEN_SIZE:
	*value_return = mode->accumGreenBits;
	return Success;
      case GLX_ACCUM_BLUE_SIZE:
	*value_return = mode->accumBlueBits;
	return Success;
      case GLX_ACCUM_ALPHA_SIZE:
	*value_return = mode->accumAlphaBits;
	return Success;
      case GLX_LEVEL:
	*value_return = mode->level;
	return Success;
      case GLX_TRANSPARENT_TYPE_EXT:
	*value_return = mode->transparentPixel;
	return Success;
      case GLX_TRANSPARENT_RED_VALUE:
	*value_return = mode->transparentRed;
	return Success;
      case GLX_TRANSPARENT_GREEN_VALUE:
	*value_return = mode->transparentGreen;
	return Success;
      case GLX_TRANSPARENT_BLUE_VALUE:
	*value_return = mode->transparentBlue;
	return Success;
      case GLX_TRANSPARENT_ALPHA_VALUE:
	*value_return = mode->transparentAlpha;
	return Success;
      case GLX_TRANSPARENT_INDEX_VALUE:
	*value_return = mode->transparentIndex;
	return Success;
      case GLX_X_VISUAL_TYPE:
	*value_return = mode->visualType;
	return Success;
      case GLX_CONFIG_CAVEAT:
	*value_return = mode->visualRating;
	return Success;
      case GLX_VISUAL_ID:
	*value_return = mode->visualID;
	return Success;
      case GLX_DRAWABLE_TYPE:
	*value_return = mode->drawableType;
	return Success;
      case GLX_RENDER_TYPE:
	*value_return = mode->renderType;
	return Success;
      case GLX_X_RENDERABLE:
	*value_return = mode->xRenderable;
	return Success;
      case GLX_FBCONFIG_ID:
	*value_return = mode->fbconfigID;
	return Success;
      case GLX_MAX_PBUFFER_WIDTH:
	*value_return = mode->maxPbufferWidth;
	return Success;
      case GLX_MAX_PBUFFER_HEIGHT:
	*value_return = mode->maxPbufferHeight;
	return Success;
      case GLX_MAX_PBUFFER_PIXELS:
	*value_return = mode->maxPbufferPixels;
	return Success;
      case GLX_OPTIMAL_PBUFFER_WIDTH_SGIX:
	*value_return = mode->optimalPbufferWidth;
	return Success;
      case GLX_OPTIMAL_PBUFFER_HEIGHT_SGIX:
	*value_return = mode->optimalPbufferHeight;
	return Success;
      case GLX_SWAP_METHOD_OML:
	*value_return = mode->swapMethod;
	return Success;
      case GLX_SAMPLE_BUFFERS_SGIS:
	*value_return = mode->sampleBuffers;
	return Success;
      case GLX_SAMPLES_SGIS:
	*value_return = mode->samples;
	return Success;

      /* Applications are NOT allowed to query GLX_VISUAL_SELECT_GROUP_SGIX.
       * It is ONLY for communication between the GLX client and the GLX
       * server.
       */
      case GLX_VISUAL_SELECT_GROUP_SGIX:
      default:
	return GLX_BAD_ATTRIBUTE;
    }
}
