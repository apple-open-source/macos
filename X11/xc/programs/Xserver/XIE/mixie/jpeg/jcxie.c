/* $Xorg: jcxie.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jcxie.c */

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

	jcxie.c: Xie JPEG Compression Interface Routines 

	Gary Rogers, AGE Logic, Inc., October 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

/*
 * Copyright (C) 1992, Thomas G. Lane.
 * This file was derived in part from the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 */

#include "jinclude.h"
#include "macro.h"

/* On normal machines we can apply MEMCOPY() and MEMZERO() to sample arrays
 * and coefficient-block arrays.  This won't work on 80x86 because the arrays
 * are FAR and we're assuming a small-pointer memory model.  However, some
 * DOS compilers provide far-pointer versions of memcpy() and memset() even
 * in the small-model libraries.  These will be used if USE_FMEM is defined.
 * Otherwise, the routines below do it the hard way.  (The performance cost
 * is not all that great, because these routines aren't very heavily used.)
 */

#ifndef NEED_FAR_POINTERS			/* normal case, same as regular macros */
#define FMEMCOPY(dest,src,size)	MEMCOPY(dest,src,size)
#define FMEMZERO(target,size)	MEMZERO(target,size)
#else				/* 80x86 case, define if we can */
#ifdef USE_FMEM
#define FMEMCOPY(dest,src,size)	_fmemcpy((pointer*)(dest), (const pointer*)(src), (size_t)(size))
#define FMEMZERO(target,size)	_fmemset((pointer*)(target), 0, (size_t)(size))
#endif
#endif

/*
 * This routine gets control after the input file header has been read.
 * It must determine what output JPEG file format is to be written,
 * and make any other compression parameter changes that are desirable.
 */

METHODDEF void
#if NeedFunctionPrototypes
c_ui_method_selection (compress_info_ptr cinfo)
#else
c_ui_method_selection (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  /* If the input is gray scale, generate a monochrome JPEG file. */
  if (cinfo->in_color_space == CS_GRAYSCALE)
    j_monochrome_default(cinfo);
  /* For now, always select JFIF output format. */
  jselwjfif(cinfo);
}

METHODDEF void
#if NeedFunctionPrototypes
c_per_scan_method_selection (compress_info_ptr cinfo)
#else
c_per_scan_method_selection (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Central point for per-scan method selection */
{
  /* Edge expansion */
  jselexpand(cinfo);
  /* Downsampling of pixels */
  jseldownsample(cinfo);
  /* MCU extraction */
  jselcmcu(cinfo);
}

LOCAL void
#if NeedFunctionPrototypes
c_initial_method_selection (compress_info_ptr cinfo)
#else
c_initial_method_selection (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Central point for initial method selection */
{
  /* Input image reading method selection is already done. */
  /* So is output file header formatting (both are done by user interface). */

  /* Entropy encoding: Huffman coding. */
  cinfo->arith_code = FALSE;	/* force Huffman mode */
  jselchuffman(cinfo);

  /* Overall control (that's me!) */
  cinfo->methods->c_per_scan_method_selection = c_per_scan_method_selection;
}

LOCAL int
#if NeedFunctionPrototypes
initial_setup (compress_info_ptr cinfo)
#else
initial_setup (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Do computations that are needed before initial method selection */
{
  short ci;
  jpeg_component_info *compptr;

  /* Compute maximum sampling factors; check factor validity */
  cinfo->max_h_samp_factor = 1;
  cinfo->max_v_samp_factor = 1;
  for (ci = 0; ci < cinfo->num_components; ci++) {
    compptr = &cinfo->comp_info[ci];
    if (compptr->h_samp_factor<=0 || compptr->h_samp_factor>MAX_SAMP_FACTOR ||
	compptr->v_samp_factor<=0 || compptr->v_samp_factor>MAX_SAMP_FACTOR)
      /* ERREXIT(cinfo->emethods, "Bogus sampling factors"); */
      return(XIE_ERR);
    cinfo->max_h_samp_factor = MAX(cinfo->max_h_samp_factor,
				   compptr->h_samp_factor);
    cinfo->max_v_samp_factor = MAX(cinfo->max_v_samp_factor,
				   compptr->v_samp_factor);
  }

  /* Compute logical downsampled dimensions of components */
  for (ci = 0; ci < cinfo->num_components; ci++) {
    compptr = &cinfo->comp_info[ci];
    compptr->true_comp_width = (cinfo->image_width * compptr->h_samp_factor
				+ cinfo->max_h_samp_factor - 1)
				/ cinfo->max_h_samp_factor;
    compptr->true_comp_height = (cinfo->image_height * compptr->v_samp_factor
				 + cinfo->max_v_samp_factor - 1)
				 / cinfo->max_v_samp_factor;
  }
  return(0);                   
}

/*
 * Utility routines: common code for pipeline controllers
 */

LOCAL int
#if NeedFunctionPrototypes
interleaved_scan_setup (compress_info_ptr cinfo)
#else
interleaved_scan_setup (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Compute all derived info for an interleaved (multi-component) scan */
/* On entry, cinfo->comps_in_scan and cinfo->cur_comp_info[] are set up */
{
  short ci, mcublks;
  jpeg_component_info *compptr;

  if (cinfo->comps_in_scan > MAX_COMPS_IN_SCAN)
    /* ERREXIT(cinfo->emethods, "Too many components for interleaved scan"); */
    return(XIE_ERR);

  cinfo->MCUs_per_row = (cinfo->image_width
			 + cinfo->max_h_samp_factor*DCTSIZE - 1)
			/ (cinfo->max_h_samp_factor*DCTSIZE);

  cinfo->MCU_rows_in_scan = (cinfo->image_height
			     + cinfo->max_v_samp_factor*DCTSIZE - 1)
			    / (cinfo->max_v_samp_factor*DCTSIZE);
  
  cinfo->blocks_in_MCU = 0;

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    /* for interleaved scan, sampling factors give # of blocks per component */
    compptr->MCU_width = compptr->h_samp_factor;
    compptr->MCU_height = compptr->v_samp_factor;
    compptr->MCU_blocks = compptr->MCU_width * compptr->MCU_height;
    /* compute physical dimensions of component */
    compptr->downsampled_width = jround_up(compptr->true_comp_width,
					   (long) (compptr->MCU_width*DCTSIZE));
    compptr->downsampled_height = jround_up(compptr->true_comp_height,
					    (long) (compptr->MCU_height*DCTSIZE));
    /* Sanity check */
    if (compptr->downsampled_width !=
	(cinfo->MCUs_per_row * (compptr->MCU_width*DCTSIZE)))
      /* ERREXIT(cinfo->emethods, "I'm confused about the image width"); */
      return(XIE_ERR);
    /* Prepare array describing MCU composition */
    mcublks = compptr->MCU_blocks;
    if (cinfo->blocks_in_MCU + mcublks > MAX_BLOCKS_IN_MCU)
      /* ERREXIT(cinfo->emethods, "Sampling factors too large for interleaved scan"); */
      return(XIE_ERR);
    while (mcublks-- > 0) {
      cinfo->MCU_membership[cinfo->blocks_in_MCU++] = ci;
    }
  }

  /* Convert restart specified in rows to actual MCU count. */
  /* Note that count must fit in 16 bits, so we provide limiting. */
  if (cinfo->restart_in_rows > 0) {
    long nominal = cinfo->restart_in_rows * cinfo->MCUs_per_row;
    cinfo->restart_interval = (UINT16) MIN(nominal, 65535L);
  }

  (*cinfo->methods->c_per_scan_method_selection) (cinfo);
  return(0);
}

LOCAL void
#if NeedFunctionPrototypes
noninterleaved_scan_setup (compress_info_ptr cinfo)
#else
noninterleaved_scan_setup (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Compute all derived info for a noninterleaved (single-component) scan */
/* On entry, cinfo->comps_in_scan = 1 and cinfo->cur_comp_info[0] is set up */
{
  jpeg_component_info *compptr = cinfo->cur_comp_info[0];

  /* for noninterleaved scan, always one block per MCU */
  compptr->MCU_width = 1;
  compptr->MCU_height = 1;
  compptr->MCU_blocks = 1;
  /* compute physical dimensions of component */
  compptr->downsampled_width = jround_up(compptr->true_comp_width,
					 (long) DCTSIZE);
  compptr->downsampled_height = jround_up(compptr->true_comp_height,
					  (long) DCTSIZE);

  cinfo->MCUs_per_row = compptr->downsampled_width / DCTSIZE;
  cinfo->MCU_rows_in_scan = compptr->downsampled_height / DCTSIZE;

  /* Prepare array describing MCU composition */
  cinfo->blocks_in_MCU = 1;
  cinfo->MCU_membership[0] = 0;

  /* Convert restart specified in rows to actual MCU count. */
  /* Note that count must fit in 16 bits, so we provide limiting. */
  if (cinfo->restart_in_rows > 0) {
    long nominal = cinfo->restart_in_rows * cinfo->MCUs_per_row;
    cinfo->restart_interval = (UINT16) MIN(nominal, 65535L);
  }

  (*cinfo->methods->c_per_scan_method_selection) (cinfo);
}


LOCAL int
#if NeedFunctionPrototypes
alloc_sampling_buffer (compress_info_ptr cinfo, JSAMPIMAGE fullsize_data[2],
		       long fullsize_width)
#else
alloc_sampling_buffer (cinfo, fullsize_data, fullsize_width)
			   compress_info_ptr cinfo; 
			   JSAMPIMAGE fullsize_data[2];
		       long fullsize_width;
#endif	/* NeedFunctionPrototypes */
/* Create a pre-downsampling data buffer having the desired structure */
/* (see comments at head of file) */
{
  short ci, vs, i;

  vs = cinfo->max_v_samp_factor; /* row group height */

  /* Get top-level space for array pointers */
  fullsize_data[0] = (JSAMPIMAGE) (*cinfo->emethods->c_alloc_small)
				(cinfo, (cinfo->num_components * SIZEOF(JSAMPARRAY)));
  if (fullsize_data[0] == NULL)
  	return(XIE_ERR);
  fullsize_data[1] = (JSAMPIMAGE) (*cinfo->emethods->c_alloc_small)
				(cinfo, (cinfo->num_components * SIZEOF(JSAMPARRAY)));
  if (fullsize_data[1] == NULL)
  	return(XIE_ERR);

  for (ci = 0; ci < cinfo->num_components; ci++) {
    /* Allocate the real storage */
    fullsize_data[0][ci] = (*cinfo->emethods->c_alloc_small_sarray)
				(cinfo, fullsize_width,
				(long) (vs * (DCTSIZE+2)));
    if (fullsize_data[0][ci] == NULL)
  	  return(XIE_ERR);
    /* Create space for the scrambled-order pointers */
    fullsize_data[1][ci] = (JSAMPARRAY) (*cinfo->emethods->c_alloc_small)
				(cinfo, (vs * (DCTSIZE+2) * SIZEOF(JSAMPROW)));
    if (fullsize_data[1][ci] == NULL)
  	  return(XIE_ERR);
    /* Duplicate the first DCTSIZE-2 row groups */
    for (i = 0; i < vs * (DCTSIZE-2); i++) {
      fullsize_data[1][ci][i] = fullsize_data[0][ci][i];
    }
    /* Copy the last four row groups in swapped order */
    for (i = 0; i < vs * 2; i++) {
      fullsize_data[1][ci][vs*DCTSIZE + i] = fullsize_data[0][ci][vs*(DCTSIZE-2) + i];
      fullsize_data[1][ci][vs*(DCTSIZE-2) + i] = fullsize_data[0][ci][vs*DCTSIZE + i];
    }
  }
  return(0);
}


LOCAL void
#if NeedFunctionPrototypes
downsample (compress_info_ptr cinfo,
	    JSAMPIMAGE fullsize_data, JSAMPIMAGE sampled_data,
	    long fullsize_width,
	    short above, short current, short below, short out)
#else
downsample (cinfo,
	    fullsize_data, sampled_data,
	     fullsize_width,
	    above, current, below, out)
		compress_info_ptr cinfo;
	    JSAMPIMAGE fullsize_data; 
	    JSAMPIMAGE sampled_data;
	    long fullsize_width;
	    short above; 
	    short current; 
	    short below; 
	    short out;
#endif	/* NeedFunctionPrototypes */
/* Do downsampling of a single row group (of each component). */
/* above, current, below are indexes of row groups in fullsize_data;      */
/* out is the index of the target row group in sampled_data.              */
/* Special case: above, below can be -1 to indicate top, bottom of image. */
{
  jpeg_component_info *compptr;
  JSAMPARRAY above_ptr, below_ptr;
  JSAMPROW dummy[MAX_SAMP_FACTOR]; /* for downsample expansion at top/bottom */
  short ci, vs, i;

  vs = cinfo->max_v_samp_factor; /* row group height */

  for (ci = 0; ci < cinfo->num_components; ci++) {
    compptr = & cinfo->comp_info[ci];

    if (above >= 0)
      above_ptr = fullsize_data[ci] + above * vs;
    else {
      /* Top of image: make a dummy above-context with copies of 1st row */
      /* We assume current=0 in this case */
      for (i = 0; i < vs; i++)
	dummy[i] = fullsize_data[ci][0];
      above_ptr = (JSAMPARRAY) dummy; /* possible near->far pointer conv */
    }

    if (below >= 0)
      below_ptr = fullsize_data[ci] + below * vs;
    else {
      /* Bot of image: make a dummy below-context with copies of last row */
      for (i = 0; i < vs; i++)
	dummy[i] = fullsize_data[ci][(current+1)*vs-1];
      below_ptr = (JSAMPARRAY) dummy; /* possible near->far pointer conv */
    }

    (*cinfo->methods->downsample[ci])
		(cinfo, (int) ci,
		 fullsize_width, (int) vs,
		 compptr->downsampled_width, (int) compptr->v_samp_factor,
		 above_ptr,
		 fullsize_data[ci] + current * vs,
		 below_ptr,
		 sampled_data[ci] + out * compptr->v_samp_factor);
  }
}

METHODDEF int
#if NeedFunctionPrototypes
jcXIE_init (compress_info_ptr cinfo)
#else
jcXIE_init (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int status;
  short ci;

/* 
 * Initialize cinfo with default switch settings.
 */

  /* (Re-)initialize the system-dependent error and memory managers. */
  jcselmemmgr(cinfo, cinfo->emethods);	/* memory allocation routines */
  cinfo->methods->c_ui_method_selection = c_ui_method_selection;

  /* Set up default JPEG parameters. */
  /* Note that default -quality level here need not, and does not,
   * match the default scaling for an explicit -qtables argument.
   */
   
   /* default quality level = 75 */
  if (j_c_defaults(cinfo, 75, FALSE) == XIE_ERR)	
    return(XIE_ERR); 

  /* Init pass counts to 0 --- total_passes is adjusted in method selection */
  cinfo->total_passes = 0;
  cinfo->completed_passes = 0;
  cinfo->data_precision = 8;	/* always, even if 12-bit JSAMPLEs */

  /* Give UI a chance to adjust compression parameters and select */
  /* output file format based on results of input_init. */
  (*cinfo->methods->c_ui_method_selection) (cinfo);

  /* Now select methods for compression steps. */
  if ((status = initial_setup(cinfo)) < 0)
    return(status);
  c_initial_method_selection(cinfo);

  /* Prepare for single scan containing all components */
  if (cinfo->num_components > MAX_COMPS_IN_SCAN)
    /* ERREXIT(cinfo->emethods, "Too many components for interleaved scan"); */
    return(XIE_ERR);
  cinfo->comps_in_scan = cinfo->num_components;
  for (ci = 0; ci < cinfo->num_components; ci++) {
    cinfo->cur_comp_info[ci] = &cinfo->comp_info[ci];
  }
  if (cinfo->comps_in_scan == 1) {
    noninterleaved_scan_setup(cinfo);
    /* Vk block rows constitute the same number of MCU rows */
    cinfo->mcu_rows_per_loop = cinfo->cur_comp_info[0]->v_samp_factor;
  } else {
    if ((status = interleaved_scan_setup(cinfo)) < 0)
      return(status);
    /* in an interleaved scan, one MCU row contains Vk block rows */
    cinfo->mcu_rows_per_loop = 1;
  }
  cinfo->total_passes++;

  /* Compute dimensions of full-size pixel buffers */
  /* Note these are the same whether interleaved or not. */
  cinfo->rows_in_mem = cinfo->max_v_samp_factor * DCTSIZE;
  cinfo->fullsize_width = jround_up(cinfo->image_width,
			     (long) (cinfo->max_h_samp_factor * DCTSIZE));

  /* Allocate working memory: */
  /* fullsize_data is sample data before downsampling */
  if (alloc_sampling_buffer(cinfo, cinfo->fullsize_data, 
                 cinfo->fullsize_width) == XIE_ERR)
    return(XIE_ERR);
  /* sampled_data is sample data after downsampling */
  cinfo->sampled_data = (JSAMPIMAGE) (*cinfo->emethods->c_alloc_small)
				(cinfo, (cinfo->num_components * SIZEOF(JSAMPARRAY)));
  if (cinfo->sampled_data == NULL)
    return(XIE_ERR);
  for (ci = 0; ci < cinfo->num_components; ci++) {
    cinfo->sampled_data[ci] = (*cinfo->emethods->c_alloc_small_sarray)
			(cinfo, cinfo->comp_info[ci].downsampled_width,
			 (long) (cinfo->comp_info[ci].v_samp_factor * DCTSIZE));
    if (cinfo->sampled_data[ci] == NULL)
      return(XIE_ERR);
  }

  /* Do per-scan object init */

  if ((status = ((*cinfo->methods->entropy_encode_init) (cinfo))) < 0)
    return(status);
  (*cinfo->methods->downsample_init) (cinfo);
  (*cinfo->methods->extract_init) (cinfo);

  cinfo->mcu_rows_output = 0;
  cinfo->whichss = 0;			/* arrange to start with fullsize_data[0] */
  cinfo->cur_pixel_row = 0;
  cinfo->first_pixel_row = TRUE;
  return(0);
}

LOCAL void
#if NeedFunctionPrototypes
jcopy_pixel_rows (JSAMPARRAY input_array,
		JSAMPIMAGE output_array, int dest_row,
		int num_rows, long num_cols)
#else
jcopy_pixel_rows (input_array,
		output_array, dest_row,
		num_rows, num_cols)
		JSAMPARRAY input_array;
		JSAMPIMAGE output_array; 
		int dest_row;
		int num_rows; 
		long num_cols;
#endif	/* NeedFunctionPrototypes */
/* Copy some rows of samples from one place to another.
 * num_rows rows are copied from input_array[source_row++]
 * to output_array[dest_row++]; these areas should not overlap.
 * The source and destination arrays must be at least as wide as num_cols.
 */
{
  register JSAMPROW inptr, outptr;
#ifdef FMEMCOPY
  register size_t count = (size_t) (num_cols * SIZEOF(JSAMPLE));
#else
  register long count;
#endif
  register int row;

  for (row = 0; row < num_rows; row++) {
    inptr = input_array[row];
    outptr = output_array[row][dest_row];
#ifdef FMEMCOPY
    FMEMCOPY(outptr, inptr, count);
#else
    for (count = 0; count < num_cols; count++)
      outptr[count] = inptr[count];	/* needn't bother with GETJSAMPLE() here */
#endif
  }
}

GLOBAL int
#if NeedFunctionPrototypes
jcXIE_get (compress_info_ptr cinfo,
		int row_to_process, JSAMPARRAY pixel_row)
#else
jcXIE_get (cinfo, row_to_process, pixel_row)
		compress_info_ptr cinfo;
		int row_to_process; 
		JSAMPARRAY pixel_row;
#endif	/* NeedFunctionPrototypes */
{
  int status;
  short i;	
  short whichss;

  whichss = cinfo->whichss;

  if (cinfo->XIErestart == XIE_RNUL) {
  	
    /* Obtain rows_this_time pixel rows and expand to rows_in_mem rows. */
    /* Then we have exactly DCTSIZE row groups for downsampling. */   
  
    jcopy_pixel_rows (pixel_row,
  					cinfo->fullsize_data[whichss], cinfo->cur_pixel_row,
  					cinfo->num_components, cinfo->image_width);

    if ((++(cinfo->cur_pixel_row)) < cinfo->rows_this_time)
      return(0);
  
  (*cinfo->methods->edge_expand) (cinfo,
				    cinfo->image_width, cinfo->rows_this_time,
				    cinfo->fullsize_width, cinfo->rows_in_mem,
				    cinfo->fullsize_data[whichss]);
  
  }
  /* Downsample the data (all components) */
  /* First time through is a special case */
  
  if (cinfo->first_pixel_row == FALSE) {
    if (cinfo->XIErestart == XIE_RNUL) {
      /* Downsample last row group of previous set */
      downsample(cinfo, cinfo->fullsize_data[whichss], cinfo->sampled_data,
    				cinfo->fullsize_width, (short) DCTSIZE,
    				(short) (DCTSIZE+1), (short) 0, (short) (DCTSIZE-1));
    }
    /* and dump the previous set's downsampled data */
    if ((status = (*cinfo->methods->extract_MCUs) (cinfo, cinfo->sampled_data,
				    cinfo->mcu_rows_per_loop,
				    cinfo->methods->entropy_encode)) < 0) {
      return(status);
    }                    	
    cinfo->mcu_rows_output += cinfo->mcu_rows_per_loop;
    /* Downsample first row group of this set */
    downsample(cinfo, cinfo->fullsize_data[whichss], cinfo->sampled_data, 
    				cinfo->fullsize_width, 
    				(short) (DCTSIZE+1), (short) 0, (short) 1, (short) 0);
  } else {
    /* Downsample first row group with dummy above-context */
    downsample(cinfo, cinfo->fullsize_data[whichss], cinfo->sampled_data, 
    				cinfo->fullsize_width, 
    				(short) (-1), (short) 0, (short) 1, (short) 0);
    cinfo->first_pixel_row = FALSE;
  }
  /* Downsample second through next-to-last row groups of this set */
  for (i = 1; i <= DCTSIZE-2; i++) {
    downsample(cinfo, cinfo->fullsize_data[whichss], cinfo->sampled_data, 
    				cinfo->fullsize_width, 
    				(short) (i-1), (short) i, (short) (i+1), (short) i);
  }
  cinfo->whichss ^= 1;		/* switch to other fullsize_data buffer */
  cinfo->cur_pixel_row = 0;
  return(0);
}
  
METHODDEF int
#if NeedFunctionPrototypes
jcXIE_term (compress_info_ptr cinfo)
#else
jcXIE_term (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int status;	
  short whichss;
  
  if (cinfo->XIErestart == XIE_RNUL) {
    whichss = cinfo->whichss ^ 1;	    /* switch back to other buffer */
    
    /* Downsample the last row group with dummy below-context */
    /* Note whichss points to last buffer side used */
    downsample(cinfo, cinfo->fullsize_data[whichss], cinfo->sampled_data, 
  		cinfo->fullsize_width, (short) (DCTSIZE-2), (short) (DCTSIZE-1), 
  		(short) (-1), (short) (DCTSIZE-1));
  }        
  /* Dump the remaining data (may be less than full height if uninterleaved) */
  if ((status = (*cinfo->methods->extract_MCUs) (cinfo, cinfo->sampled_data,
		(int) (cinfo->MCU_rows_in_scan - cinfo->mcu_rows_output),
		cinfo->methods->entropy_encode)) < 0) {
    return(status);
  }                    	
  (*cinfo->methods->extract_term) (cinfo);
  (*cinfo->methods->downsample_term) (cinfo);
  (*cinfo->methods->entropy_encode_term) (cinfo);
  cinfo->completed_passes++;
  (*cinfo->emethods->c_free_all) (cinfo);
  return(0);
}

GLOBAL void
#if NeedFunctionPrototypes
jselrXIE (compress_info_ptr cinfo)
#else
jselrXIE (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  cinfo->methods->input_init = jcXIE_init;
  /* cinfo->methods->get_input_row is set by input_init */
  cinfo->methods->input_term = jcXIE_term;
}

