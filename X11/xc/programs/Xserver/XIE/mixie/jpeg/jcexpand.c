/* $Xorg: jcexpand.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jcexpand.c */

/****************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************

	Gary Rogers, AGE Logic, Inc., October 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

/*
 * jcexpand.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains image edge-expansion routines.
 * These routines are invoked via the edge_expand method.
 */

#include "jinclude.h"


/*
 * Expand an image so that it is a multiple of the MCU dimensions.
 * This is to be accomplished by duplicating the rightmost column
 * and/or bottommost row of pixels.  The image has not yet been
 * downsampled, so all components have the same dimensions.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
edge_expand (compress_info_ptr cinfo,
	     long input_cols, int input_rows,
	     long output_cols, int output_rows,
	     JSAMPIMAGE image_data)
#else
edge_expand (cinfo,	input_cols, input_rows, output_cols, output_rows, image_data)
	compress_info_ptr cinfo;
	long input_cols; 
	int input_rows;
	long output_cols; 
	int output_rows;
	JSAMPIMAGE image_data;
#endif	/* NeedFunctionPrototypes */
#else
edge_expand (compress_info_ptr cinfo,
	     long input_cols, int input_rows,
	     long output_cols, int output_rows,
	     JSAMPIMAGE image_data)
#endif	/* XIE_SUPPORTED */
{
  /* Expand horizontally */
  if (input_cols < output_cols) {
    register JSAMPROW ptr;
    register JSAMPLE pixval;
    register long count;
    register int row;
    short ci;
    long numcols = output_cols - input_cols;

    for (ci = 0; ci < cinfo->num_components; ci++) {
      for (row = 0; row < input_rows; row++) {
	ptr = image_data[ci][row] + (input_cols-1);
	pixval = GETJSAMPLE(*ptr++);
	for (count = numcols; count > 0; count--)
	  *ptr++ = pixval;
      }
    }
  }

  /* Expand vertically */
  /* This happens only once at the bottom of the image, */
  /* so it needn't be super-efficient */
  if (input_rows < output_rows) {
    register int row;
    short ci;
    JSAMPARRAY this_component;

    for (ci = 0; ci < cinfo->num_components; ci++) {
      this_component = image_data[ci];
      for (row = input_rows; row < output_rows; row++) {
	jcopy_sample_rows(this_component, input_rows-1, this_component, row,
			  1, output_cols);
      }
    }
  }
}


/*
 * The method selection routine for edge expansion.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jselexpand (compress_info_ptr cinfo)
#else
jselexpand (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jselexpand (compress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* just one implementation for now */
  cinfo->methods->edge_expand = edge_expand;
}
