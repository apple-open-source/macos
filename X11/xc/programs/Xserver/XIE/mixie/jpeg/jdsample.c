/* $Xorg: jdsample.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* Module jdsample.c */

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
 * jdsample.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains upsampling routines.
 * These routines are invoked via the upsample and
 * upsample_init/term methods.
 *
 * An excellent reference for image resampling is
 *   Digital Image Warping, George Wolberg, 1990.
 *   Pub. by IEEE Computer Society Press, Los Alamitos, CA. ISBN 0-8186-8944-7.
 */

#include "jinclude.h"


/*
 * Initialize for upsampling a scan.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
upsample_init (decompress_info_ptr cinfo)
#else
upsample_init (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
upsample_init (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
}


/*
 * Upsample pixel values of a single component.
 * This version handles any integral sampling ratios.
 *
 * This is not used for typical JPEG files, so it need not be fast.
 * Nor, for that matter, is it particularly accurate: the algorithm is
 * simple replication of the input pixel onto the corresponding output
 * pixels.  The hi-falutin sampling literature refers to this as a
 * "box filter".  A box filter tends to introduce visible artifacts,
 * so if you are actually going to use 3:1 or 4:1 sampling ratios
 * you would be well advised to improve this code.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
int_upsample (decompress_info_ptr cinfo, int which_component,
	      long input_cols, int input_rows,
	      long output_cols, int output_rows,
	      JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	      JSAMPARRAY output_data)
#else
int_upsample (cinfo, which_component,
	      input_cols, input_rows,
	      output_cols, output_rows,
	      above, input_data, below,
	      output_data)
		  decompress_info_ptr cinfo; 
		  int which_component;
	      long input_cols; 
	      int input_rows;
	      long output_cols; 
	      int output_rows;
	      JSAMPARRAY above; 
	      JSAMPARRAY input_data; 
	      JSAMPARRAY below;
	      JSAMPARRAY output_data;
#endif	/* NeedFunctionPrototypes */
#else
int_upsample (decompress_info_ptr cinfo, int which_component,
	      long input_cols, int input_rows,
	      long output_cols, int output_rows,
	      JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	      JSAMPARRAY output_data)
#endif	/* XIE_SUPPORTED */
{
  jpeg_component_info * compptr = cinfo->cur_comp_info[which_component];
  register JSAMPROW inptr, outptr;
  register JSAMPLE invalue;
  register short h_expand, h;
  short v_expand, v;
  int inrow, outrow;
  register long incol;

#ifndef XIE_SUPPORTED
#ifdef DEBUG			/* for debugging pipeline controller */
  if (input_rows != compptr->v_samp_factor ||
      output_rows != cinfo->max_v_samp_factor ||
      (input_cols % compptr->h_samp_factor) != 0 ||
      (output_cols % cinfo->max_h_samp_factor) != 0 ||
      output_cols*compptr->h_samp_factor != input_cols*cinfo->max_h_samp_factor)
    ERREXIT(cinfo->emethods, "Bogus upsample parameters");
#endif
#endif	/* XIE_SUPPORTED */

  h_expand = cinfo->max_h_samp_factor / compptr->h_samp_factor;
  v_expand = cinfo->max_v_samp_factor / compptr->v_samp_factor;

  outrow = 0;
  for (inrow = 0; inrow < input_rows; inrow++) {
    for (v = 0; v < v_expand; v++) {
      inptr = input_data[inrow];
      outptr = output_data[outrow++];
      for (incol = 0; incol < input_cols; incol++) {
	invalue = GETJSAMPLE(*inptr++);
	for (h = 0; h < h_expand; h++) {
	  *outptr++ = invalue;
	}
      }
    }
  }
}


/*
 * Upsample pixel values of a single component.
 * This version handles the common case of 2:1 horizontal and 1:1 vertical.
 *
 * The upsampling algorithm is linear interpolation between pixel centers,
 * also known as a "triangle filter".  This is a good compromise between
 * speed and visual quality.  The centers of the output pixels are 1/4 and 3/4
 * of the way between input pixel centers.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
h2v1_upsample (decompress_info_ptr cinfo, int which_component,
	       long input_cols, int input_rows,
	       long output_cols, int output_rows,
	       JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	       JSAMPARRAY output_data)
#else
h2v1_upsample (cinfo, which_component,
	       input_cols, input_rows,
	       output_cols, output_rows,
	       above, input_data, below,
	       output_data)
		   decompress_info_ptr cinfo; 
		   int which_component;
	       long input_cols; 
	       int input_rows;
	       long output_cols; 
	       int output_rows;
	       JSAMPARRAY above; 
	       JSAMPARRAY input_data; 
	       JSAMPARRAY below;
	       JSAMPARRAY output_data;
#endif	/* NeedFunctionPrototypes */
#else
h2v1_upsample (decompress_info_ptr cinfo, int which_component,
	       long input_cols, int input_rows,
	       long output_cols, int output_rows,
	       JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	       JSAMPARRAY output_data)
#endif	/* XIE_SUPPORTED */
{
  register JSAMPROW inptr, outptr;
  register int invalue;
  int inrow;
  register long colctr;

#ifndef XIE_SUPPORTED
#ifdef DEBUG			/* for debugging pipeline controller */
  jpeg_component_info * compptr = cinfo->cur_comp_info[which_component];
  if (input_rows != compptr->v_samp_factor ||
      output_rows != cinfo->max_v_samp_factor ||
      (input_cols % compptr->h_samp_factor) != 0 ||
      (output_cols % cinfo->max_h_samp_factor) != 0 ||
      output_cols*compptr->h_samp_factor != input_cols*cinfo->max_h_samp_factor)
    ERREXIT(cinfo->emethods, "Bogus upsample parameters");
#endif
#endif	/* XIE_SUPPORTED */

  for (inrow = 0; inrow < input_rows; inrow++) {
    inptr = input_data[inrow];
    outptr = output_data[inrow];
    /* Special case for first column */
    invalue = GETJSAMPLE(*inptr++);
    *outptr++ = (JSAMPLE) invalue;
    *outptr++ = (JSAMPLE) ((invalue * 3 + GETJSAMPLE(*inptr) + 2) >> 2);

    for (colctr = input_cols - 2; colctr > 0; colctr--) {
      /* General case: 3/4 * nearer pixel + 1/4 * further pixel */
      invalue = GETJSAMPLE(*inptr++) * 3;
      *outptr++ = (JSAMPLE) ((invalue + GETJSAMPLE(inptr[-2]) + 2) >> 2);
      *outptr++ = (JSAMPLE) ((invalue + GETJSAMPLE(*inptr) + 2) >> 2);
    }

    /* Special case for last column */
    invalue = GETJSAMPLE(*inptr);
    *outptr++ = (JSAMPLE) ((invalue * 3 + GETJSAMPLE(inptr[-1]) + 2) >> 2);
    *outptr++ = (JSAMPLE) invalue;
  }
}


/*
 * Upsample pixel values of a single component.
 * This version handles the common case of 2:1 horizontal and 2:1 vertical.
 *
 * The upsampling algorithm is linear interpolation between pixel centers,
 * also known as a "triangle filter".  This is a good compromise between
 * speed and visual quality.  The centers of the output pixels are 1/4 and 3/4
 * of the way between input pixel centers.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
h2v2_upsample (decompress_info_ptr cinfo, int which_component,
	       long input_cols, int input_rows,
	       long output_cols, int output_rows,
	       JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	       JSAMPARRAY output_data)
#else
h2v2_upsample (cinfo, which_component,
	       input_cols, input_rows,
	       output_cols, output_rows,
	       above, input_data, below,
	       output_data)
		   decompress_info_ptr cinfo; 
		   int which_component;
	       long input_cols; 
	       int input_rows;
	       long output_cols; 
	       int output_rows;
	       JSAMPARRAY above; 
	       JSAMPARRAY input_data; 
	       JSAMPARRAY below;
	       JSAMPARRAY output_data;
#endif	/* NeedFunctionPrototypes */
#else
h2v2_upsample (decompress_info_ptr cinfo, int which_component,
	       long input_cols, int input_rows,
	       long output_cols, int output_rows,
	       JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
	       JSAMPARRAY output_data)
#endif	/* XIE_SUPPORTED */
{
  register JSAMPROW inptr0, inptr1, outptr;
#ifdef EIGHT_BIT_SAMPLES
  register int thiscolsum, lastcolsum, nextcolsum;
#else
  register INT32 thiscolsum, lastcolsum, nextcolsum;
#endif
  int inrow, outrow, v;
  register long colctr;

#ifndef XIE_SUPPORTED
#ifdef DEBUG			/* for debugging pipeline controller */
  jpeg_component_info * compptr = cinfo->cur_comp_info[which_component];
  if (input_rows != compptr->v_samp_factor ||
      output_rows != cinfo->max_v_samp_factor ||
      (input_cols % compptr->h_samp_factor) != 0 ||
      (output_cols % cinfo->max_h_samp_factor) != 0 ||
      output_cols*compptr->h_samp_factor != input_cols*cinfo->max_h_samp_factor)
    ERREXIT(cinfo->emethods, "Bogus upsample parameters");
#endif
#endif	/* XIE_SUPPORTED */

  outrow = 0;
  for (inrow = 0; inrow < input_rows; inrow++) {
    for (v = 0; v < 2; v++) {
      /* inptr0 points to nearest input row, inptr1 points to next nearest */
      inptr0 = input_data[inrow];
      if (v == 0) {		/* next nearest is row above */
	if (inrow == 0)
	  inptr1 = above[input_rows-1];
	else
	  inptr1 = input_data[inrow-1];
      } else {			/* next nearest is row below */
	if (inrow == input_rows-1)
	  inptr1 = below[0];
	else
	  inptr1 = input_data[inrow+1];
      }
      outptr = output_data[outrow++];

      /* Special case for first column */
      thiscolsum = GETJSAMPLE(*inptr0++) * 3 + GETJSAMPLE(*inptr1++);
      nextcolsum = GETJSAMPLE(*inptr0++) * 3 + GETJSAMPLE(*inptr1++);
      *outptr++ = (JSAMPLE) ((thiscolsum * 4 + 8) >> 4);
      *outptr++ = (JSAMPLE) ((thiscolsum * 3 + nextcolsum + 8) >> 4);
      lastcolsum = thiscolsum; thiscolsum = nextcolsum;

      for (colctr = input_cols - 2; colctr > 0; colctr--) {
	/* General case: 3/4 * nearer pixel + 1/4 * further pixel in each */
	/* dimension, thus 9/16, 3/16, 3/16, 1/16 overall */
	nextcolsum = GETJSAMPLE(*inptr0++) * 3 + GETJSAMPLE(*inptr1++);
	*outptr++ = (JSAMPLE) ((thiscolsum * 3 + lastcolsum + 8) >> 4);
	*outptr++ = (JSAMPLE) ((thiscolsum * 3 + nextcolsum + 8) >> 4);
	lastcolsum = thiscolsum; thiscolsum = nextcolsum;
      }

      /* Special case for last column */
      *outptr++ = (JSAMPLE) ((thiscolsum * 3 + lastcolsum + 8) >> 4);
      *outptr++ = (JSAMPLE) ((thiscolsum * 4 + 8) >> 4);
    }
  }
}


/*
 * Upsample pixel values of a single component.
 * This version handles the special case of a full-size component.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
fullsize_upsample (decompress_info_ptr cinfo, int which_component,
		   long input_cols, int input_rows,
		   long output_cols, int output_rows,
		   JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
		   JSAMPARRAY output_data)
#else
fullsize_upsample (cinfo, which_component,
		   input_cols, input_rows,
		   output_cols, output_rows,
		   above, input_data, below,
		   output_data)
		   decompress_info_ptr cinfo; 
		   int which_component;
		   long input_cols; 
		   int input_rows;
		   long output_cols; 
		   int output_rows;
		   JSAMPARRAY above; 
		   JSAMPARRAY input_data; 
		   JSAMPARRAY below;
		   JSAMPARRAY output_data;
#endif	/* NeedFunctionPrototypes */
#else
fullsize_upsample (decompress_info_ptr cinfo, int which_component,
		   long input_cols, int input_rows,
		   long output_cols, int output_rows,
		   JSAMPARRAY above, JSAMPARRAY input_data, JSAMPARRAY below,
		   JSAMPARRAY output_data)
#endif	/* XIE_SUPPORTED */
{
#ifndef XIE_SUPPORTED
#ifdef DEBUG			/* for debugging pipeline controller */
  if (input_cols != output_cols || input_rows != output_rows)
    ERREXIT(cinfo->emethods, "Pipeline controller messed up");
#endif
#endif	/* XIE_SUPPORTED */

  jcopy_sample_rows(input_data, 0, output_data, 0, output_rows, output_cols);
}



/*
 * Clean up after a scan.
 */

#ifndef XIE_SUPPORTED
METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
upsample_term (decompress_info_ptr cinfo)
#else
upsample_term (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
upsample_term (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
}
#endif   /* XIE_SUPPORTED */


/*
 * The method selection routine for upsampling.
 * Note that we must select a routine for each component.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jselupsample (decompress_info_ptr cinfo)
#else
jselupsample (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jselupsample (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  short ci;
  jpeg_component_info * compptr;

#ifndef XIE_SUPPORTED
  if (cinfo->CCIR601_sampling)
    ERREXIT(cinfo->emethods, "CCIR601 upsampling not implemented yet");
#endif	/* XIE_SUPPORTED */

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    if (compptr->h_samp_factor == cinfo->max_h_samp_factor &&
	compptr->v_samp_factor == cinfo->max_v_samp_factor)
      cinfo->methods->upsample[ci] = fullsize_upsample;
    else if (compptr->h_samp_factor * 2 == cinfo->max_h_samp_factor &&
	     compptr->v_samp_factor == cinfo->max_v_samp_factor)
      cinfo->methods->upsample[ci] = h2v1_upsample;
    else if (compptr->h_samp_factor * 2 == cinfo->max_h_samp_factor &&
	     compptr->v_samp_factor * 2 == cinfo->max_v_samp_factor)
      cinfo->methods->upsample[ci] = h2v2_upsample;
    else if ((cinfo->max_h_samp_factor % compptr->h_samp_factor) == 0 &&
	     (cinfo->max_v_samp_factor % compptr->v_samp_factor) == 0)
      cinfo->methods->upsample[ci] = int_upsample;
    else
#ifdef XIE_SUPPORTED
      {}
#else        
      ERREXIT(cinfo->emethods, "Fractional upsampling not implemented yet");
#endif	/* XIE_SUPPORTED */
  }

  cinfo->methods->upsample_init = upsample_init;
#ifndef XIE_SUPPORTED	  
  cinfo->methods->upsample_term = upsample_term;
#endif   /* XIE_SUPPORTED */  
}
