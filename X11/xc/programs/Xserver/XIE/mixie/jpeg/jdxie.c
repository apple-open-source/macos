/* $Xorg: jdxie.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* Module jdxie.c */

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
 * Copyright (C) 1992, Thomas G. Lane.
 * This file was derived in part from the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 */

#include "jinclude.h"
#include "macro.h"

METHODDEF void
#if NeedFunctionPrototypes
d_ui_method_selection (decompress_info_ptr cinfo)
#else
d_ui_method_selection (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  /* if grayscale or CMYK input, force similar output; */
  /* else leave the output colorspace as set by options. */
  if (cinfo->jpeg_color_space == CS_GRAYSCALE)
    cinfo->out_color_space = CS_GRAYSCALE;
  else if (cinfo->jpeg_color_space == CS_CMYK)
    cinfo->out_color_space = CS_CMYK;
}

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

METHODDEF void
#if NeedFunctionPrototypes
d_per_scan_method_selection (decompress_info_ptr cinfo)
#else
d_per_scan_method_selection (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Central point for per-scan method selection */
{
  /* MCU disassembly */
  jseldmcu(cinfo);
  if (cinfo->XIE_upsample)
    /* Upsampling of pixels */
    jselupsample(cinfo);
}


LOCAL void
#if NeedFunctionPrototypes
d_initial_method_selection (decompress_info_ptr cinfo)
#else
d_initial_method_selection (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Central point for initial method selection (after reading file header) */
{
  /* JPEG file scanning method selection is already done. */
  /* So is output file format selection (both are done by user interface). */

  /* Entropy decoding: Huffman coding. */
  jseldhuffman(cinfo);
  cinfo->do_block_smoothing = FALSE;

  /* Overall control (that's me!) */
  cinfo->methods->d_per_scan_method_selection = d_per_scan_method_selection;
}


LOCAL int
#if NeedFunctionPrototypes
initial_setup (decompress_info_ptr cinfo)
#else
initial_setup (cinfo)
	decompress_info_ptr cinfo;
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
 * About the data structures:
 *
 * The processing chunk size for upsampling is referred to in this file as
 * a "row group": a row group is defined as Vk (v_samp_factor) sample rows of
 * any component while downsampled, or Vmax (max_v_samp_factor) unsubsampled
 * rows.  In an interleaved scan each MCU row contains exactly DCTSIZE row
 * groups of each component in the scan.  In a noninterleaved scan an MCU row
 * is one row of blocks, which might not be an integral number of row groups;
 * therefore, we read in Vk MCU rows to obtain the same amount of data as we'd
 * have in an interleaved scan.
 * To provide context for the upsampling step, we have to retain the last
 * two row groups of the previous MCU row while reading in the next MCU row
 * (or set of Vk MCU rows).  To do this without copying data about, we create
 * a rather strange data structure.  Exactly DCTSIZE+2 row groups of samples
 * are allocated, but we create two different sets of pointers to this array.
 * The second set swaps the last two pairs of row groups.  By working
 * alternately with the two sets of pointers, we can access the data in the
 * desired order.
 *
 * Cross-block smoothing also needs context above and below the "current" row.
 * Since this is an optional feature, I've implemented it in a way that is
 * much simpler but requires more than the minimum amount of memory.  We
 * simply allocate three extra MCU rows worth of coefficient blocks and use
 * them to "read ahead" one MCU row in the file.  For a typical 1000-pixel-wide
 * image with 2x2,1x1,1x1 sampling, each MCU row is about 50Kb; an 80x86
 * machine may be unable to apply cross-block smoothing to wider images.
 */


/*
 * Utility routines: common code for pipeline controller
 */

LOCAL int
#if NeedFunctionPrototypes
interleaved_scan_setup (decompress_info_ptr cinfo)
#else
interleaved_scan_setup (cinfo)
	decompress_info_ptr cinfo;
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

  (*cinfo->methods->d_per_scan_method_selection) (cinfo);
  return(0);
}


LOCAL void
#if NeedFunctionPrototypes
noninterleaved_scan_setup (decompress_info_ptr cinfo)
#else
noninterleaved_scan_setup (cinfo)
	decompress_info_ptr cinfo;
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

  (*cinfo->methods->d_per_scan_method_selection) (cinfo);
}

GLOBAL JSAMPIMAGE
#if NeedFunctionPrototypes
alloc_sampimage (decompress_info_ptr cinfo,
		 int num_comps, long num_rows, long num_cols)
#else
alloc_sampimage (cinfo,	num_comps, num_rows, num_cols)
		 decompress_info_ptr cinfo;
		 int num_comps; 
		 long num_rows; 
		 long num_cols;
#endif	/* NeedFunctionPrototypes */
/* Allocate an in-memory sample image (all components same size) */
{
  JSAMPIMAGE image;
  int ci;

  image = (JSAMPIMAGE) (*cinfo->emethods->d_alloc_small)
				(cinfo, (num_comps * SIZEOF(JSAMPARRAY)));
  if (image == (JSAMPIMAGE) NULL)
  	return((JSAMPIMAGE) NULL);
  for (ci = 0; ci < num_comps; ci++) {
    image[ci] = (*cinfo->emethods->d_alloc_small_sarray) 
    			(cinfo, num_cols, num_rows);
    if (image[ci] == NULL)
   	  return((JSAMPIMAGE) NULL);
  }
  return image;
}

LOCAL JBLOCKIMAGE
#if NeedFunctionPrototypes
alloc_MCU_row (decompress_info_ptr cinfo)
#else
alloc_MCU_row (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Allocate one MCU row's worth of coefficient blocks */
{
  JBLOCKIMAGE image;
  int ci;

  image = (JBLOCKIMAGE) (*cinfo->emethods->d_alloc_small)
				(cinfo, (cinfo->comps_in_scan * SIZEOF(JBLOCKARRAY)));
  if (image == (JBLOCKIMAGE) NULL)
  	return((JBLOCKIMAGE) NULL);
  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    image[ci] = (*cinfo->emethods->d_alloc_small_barray)
			(cinfo, cinfo->cur_comp_info[ci]->downsampled_width / DCTSIZE,
			 (long) cinfo->cur_comp_info[ci]->MCU_height);
    if (image[ci] == NULL)
  	  return((JBLOCKIMAGE) NULL);
  }
  return image;
}

LOCAL int
#if NeedFunctionPrototypes
alloc_sampling_buffer (decompress_info_ptr cinfo, JSAMPIMAGE sampled_data[2])
#else
alloc_sampling_buffer (cinfo, sampled_data)
	decompress_info_ptr cinfo;
    JSAMPIMAGE sampled_data[2];
#endif	/* NeedFunctionPrototypes */
/* Create a downsampled-data buffer having the desired structure */
/* (see comments at head of file) */
{
  short ci, vs, i;

  /* Get top-level space for array pointers */
  sampled_data[0] = (JSAMPIMAGE) (*cinfo->emethods->d_alloc_small)
				(cinfo, (cinfo->comps_in_scan * SIZEOF(JSAMPARRAY)));
  if (cinfo->sampled_data[0] == (JSAMPIMAGE) NULL)
    return(XIE_ERR);
  sampled_data[1] = (JSAMPIMAGE) (*cinfo->emethods->d_alloc_small)
				(cinfo, (cinfo->comps_in_scan * SIZEOF(JSAMPARRAY)));
  if (cinfo->sampled_data[1] == (JSAMPIMAGE) NULL)
    return(XIE_ERR);

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    vs = cinfo->cur_comp_info[ci]->v_samp_factor; /* row group height */
    /* Allocate the real storage */
    sampled_data[0][ci] = (*cinfo->emethods->d_alloc_small_sarray)
				(cinfo, cinfo->cur_comp_info[ci]->downsampled_width,
				(long) (vs * (DCTSIZE+2)));
    if (cinfo->sampled_data[0][ci] ==  NULL)
      return(XIE_ERR);
    /* Create space for the scrambled-order pointers */
    sampled_data[1][ci] = (JSAMPARRAY) (*cinfo->emethods->d_alloc_small)
				(cinfo, (vs * (DCTSIZE+2) * SIZEOF(JSAMPROW)));
    if (cinfo->sampled_data[1][ci] ==  (JSAMPARRAY) NULL)
      return(XIE_ERR);
    /* Duplicate the first DCTSIZE-2 row groups */
    for (i = 0; i < vs * (DCTSIZE-2); i++) {
      sampled_data[1][ci][i] = sampled_data[0][ci][i];
    }
    /* Copy the last four row groups in swapped order */
    for (i = 0; i < vs * 2; i++) {
      sampled_data[1][ci][vs*DCTSIZE + i] = sampled_data[0][ci][vs*(DCTSIZE-2) + i];
      sampled_data[1][ci][vs*(DCTSIZE-2) + i] = sampled_data[0][ci][vs*DCTSIZE + i];
    }
  }
  return(0);
}

/*
 * Several decompression processes need to range-limit values to the range
 * 0..MAXJSAMPLE; the input value may fall somewhat outside this range
 * due to noise introduced by quantization, roundoff error, etc.  These
 * processes are inner loops and need to be as fast as possible.  On most
 * machines, particularly CPUs with pipelines or instruction prefetch,
 * a (range-check-less) C table lookup
 *		x = sample_range_limit[x];
 * is faster than explicit tests
 *		if (x < 0)  x = 0;
 *		else if (x > MAXJSAMPLE)  x = MAXJSAMPLE;
 * These processes all use a common table prepared by the routine below.
 *
 * The table will work correctly for x within MAXJSAMPLE+1 of the legal
 * range.  This is a much wider range than is needed for most cases,
 * but the wide range is handy for color quantization.
 * Note that the table is allocated in near data space on PCs; it's small
 * enough and used often enough to justify this.
 */

LOCAL int
#if NeedFunctionPrototypes
prepare_range_limit_table (decompress_info_ptr cinfo)
#else
prepare_range_limit_table (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Allocate and fill in the sample_range_limit table */
{
  JSAMPLE * table;
  int i;

  table = (JSAMPLE *) (*cinfo->emethods->d_alloc_small)
			(cinfo, (3 * (MAXJSAMPLE+1) * SIZEOF(JSAMPLE)));
  if (table == (JSAMPLE *) NULL)
  	return(XIE_ERR);
  cinfo->sample_range_limit = table + (MAXJSAMPLE+1);
  for (i = 0; i <= MAXJSAMPLE; i++) {
    table[i] = 0;			/* sample_range_limit[x] = 0 for x<0 */
    table[i+(MAXJSAMPLE+1)] = (JSAMPLE) i;	/* sample_range_limit[x] = x */
    table[i+(MAXJSAMPLE+1)*2] = MAXJSAMPLE;	/* x beyond MAXJSAMPLE */
  }
  return(0);
}

LOCAL void
#if NeedFunctionPrototypes
duplicate_row (JSAMPARRAY image_data,
	       long num_cols, int source_row, int num_rows)
#else
duplicate_row (image_data, num_cols, source_row, num_rows)
		   JSAMPARRAY image_data;
	       long num_cols; 
	       int source_row; 
	       int num_rows;
#endif	/* NeedFunctionPrototypes */
/* Duplicate the source_row at source_row+1 .. source_row+num_rows */
/* This happens only at the bottom of the image, */
/* so it needn't be super-efficient */
{
  register int row;

  for (row = 1; row <= num_rows; row++) {
    jcopy_sample_rows(image_data, source_row, image_data, source_row + row,
		      1, num_cols);
  }
}

LOCAL void
#if NeedFunctionPrototypes
expand (decompress_info_ptr cinfo,
	JSAMPIMAGE sampled_data, JSAMPIMAGE fullsize_data,
	long fullsize_width,
	short above, short current, short below, short out)
#else
expand (cinfo,
	sampled_data, fullsize_data,
	fullsize_width,
	above, current, below, out)
	decompress_info_ptr cinfo;
	JSAMPIMAGE sampled_data; 
	JSAMPIMAGE fullsize_data;
	long fullsize_width;
	short above; 
	short current; 
	short below; 
	short out;
#endif	/* NeedFunctionPrototypes */
/* Do upsampling expansion of a single row group (of each component). */
/* above, current, below are indexes of row groups in sampled_data;       */
/* out is the index of the target row group in fullsize_data.             */
/* Special case: above, below can be -1 to indicate top, bottom of image. */
{
  jpeg_component_info *compptr;
  JSAMPARRAY above_ptr, below_ptr;
  JSAMPROW dummy[MAX_SAMP_FACTOR]; /* for downsample expansion at top/bottom */
  short ci, vs, i;

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    vs = compptr->v_samp_factor; /* row group height */

    if (above >= 0)
      above_ptr = sampled_data[ci] + above * vs;
    else {
      /* Top of image: make a dummy above-context with copies of 1st row */
      /* We assume current=0 in this case */
      for (i = 0; i < vs; i++)
	dummy[i] = sampled_data[ci][0];
      above_ptr = (JSAMPARRAY) dummy; /* possible near->far pointer conv */
    }

    if (below >= 0)
      below_ptr = sampled_data[ci] + below * vs;
    else {
      /* Bot of image: make a dummy below-context with copies of last row */
      for (i = 0; i < vs; i++)
	dummy[i] = sampled_data[ci][(current+1)*vs-1];
      below_ptr = (JSAMPARRAY) dummy; /* possible near->far pointer conv */
    }

    (*cinfo->methods->upsample[ci])
		(cinfo, (int) ci,
		 compptr->downsampled_width, (int) vs,
		 fullsize_width, (int) cinfo->max_v_samp_factor,
		 above_ptr,
		 sampled_data[ci] + current * vs,
		 below_ptr,
		 fullsize_data[ci] + out * cinfo->max_v_samp_factor);
  }
}

LOCAL void
#if NeedFunctionPrototypes
jdcopy_pixel_rows (decompress_info_ptr cinfo, 
         JSAMPIMAGE input_array, JSAMPIMAGE output_array)
#else
jdcopy_pixel_rows (cinfo,input_array, output_array)
		 decompress_info_ptr cinfo; 
         JSAMPIMAGE input_array; 
         JSAMPIMAGE output_array;
#endif	/* NeedFunctionPrototypes */
/* Copy some rows of samples from one place to another.
 * num_rows rows are copied from input_array[source_row++]
 * to output_array[dest_row++]; these areas should not overlap.
 * The source and destination arrays must be at least as wide as num_cols.
 */
{
  register JSAMPROW inptr, outptr;
#ifdef FMEMCOPY
  register size_t count;
#else
  register long count;
#endif
  register int ci, row;
  int num_components; 
  int num_rows; 
  long num_cols;

  num_components = cinfo->num_components;
  if (cinfo->XIE_upsample) {
    num_rows = cinfo->rows_in_mem; 
    num_cols = cinfo->image_width;
#ifdef FMEMCOPY
    count = (size_t) (num_cols * SIZEOF(JSAMPLE));
#endif
  }
 
  for (ci = 0; ci < num_components; ci++) {
    if (!cinfo->XIE_upsample) {
      num_rows = cinfo->comp_info[ci].h_samp_factor*DCTSIZE; 
      num_cols = cinfo->comp_info[ci].true_comp_width;
#ifdef FMEMCOPY
      count = (size_t) (num_cols * SIZEOF(JSAMPLE));
#endif
    }
    for (row = 0; row < num_rows; row++) {
      inptr = input_array[ci][row];
      outptr = output_array[ci][row];
#ifdef FMEMCOPY
      FMEMCOPY(outptr, inptr, count);
#else
      for (count = 0; count < num_cols; count++)
        outptr[count] = inptr[count];	/* needn't bother with GETJSAMPLE() here */
#endif
    }
  }
}

GLOBAL int
#if NeedFunctionPrototypes
jdXIE_init (decompress_info_ptr cinfo)
#else
jdXIE_init (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int status;
  
  if (cinfo->XIErestart == XIE_RNUL) {
    /* Install, but don't yet enable signal catcher. */

    /* Set up compression parameters */
    /* Initialize cinfo with default switch settings */
    /* (Re-)initialize the system-dependent error and memory managers. */
    jdselmemmgr(cinfo, cinfo->emethods);	/* memory allocation routines */
    cinfo->methods->d_ui_method_selection = d_ui_method_selection;

    /* Set up default JPEG parameters. */
    j_d_defaults(cinfo, FALSE);

    /* Set up to read a baseline-JPEG file. */
    jselrjfif(cinfo);

    /* Init pass counts to 0 --- total_passes is adjusted in method selection */
    cinfo->total_passes = 0;
    cinfo->completed_passes = 0;
  
    cinfo->XIErestart = XIE_RNUL;
    cinfo->XIEmcuindex = 0;
  }   /* if (cinfo->XIErestart != XIE_RNUL) */

  /* Read the JPEG file header markers; everything up through the first SOS
   * marker is read now.  NOTE: the user interface must have initialized the
   * read_file_header method pointer (eg, by calling jselrjfif or jselrtiff).
   * The other file reading methods (read_scan_header etc.) were probably
   * set at the same time, but could be set up by read_file_header itself.
   */
  if (cinfo->XIErestart != XIE_RRSH) {
   
    /* need more data */
    if ((status = ((*cinfo->methods->read_file_header)(cinfo))) < 0) {
      if (status == XIE_ERR)
      	return(XIE_ERR);
      cinfo->XIErestart = XIE_RRFH;
      cinfo->next_input_byte = cinfo->XIEnext_input_byte;
      cinfo->bytes_in_buffer = cinfo->XIEbytes_in_buffer;
      return(XIE_INP);
    } else {
      cinfo->XIErestart = XIE_RNUL;
    }  
  }
  /* need more data */
  if ((status = ((*cinfo->methods->read_scan_header)(cinfo))) < 0) {
    if (status == XIE_ERR)
      return(XIE_ERR);
    cinfo->XIErestart = XIE_RRSH;
    cinfo->next_input_byte = cinfo->XIEnext_input_byte;
    cinfo->bytes_in_buffer = cinfo->XIEbytes_in_buffer;
    return(XIE_INP);
  } 
  cinfo->XIErestart = XIE_RNUL;

  /* Give UI a chance to adjust decompression parameters and select */
  /* output file format based on info from file header. */
  (*cinfo->methods->d_ui_method_selection) (cinfo);

  /* Now select methods for decompression steps. */
  if (initial_setup(cinfo) == XIE_ERR)
    return(XIE_ERR);
  d_initial_method_selection(cinfo);

  /* Compute dimensions of full-size pixel buffers */
  /* Note these are the same whether interleaved or not. */
  cinfo->rows_in_mem = cinfo->max_v_samp_factor * DCTSIZE;
  cinfo->fullsize_width = jround_up(cinfo->image_width,
			     (long) (cinfo->max_h_samp_factor * DCTSIZE));

  /* Prepare for single scan containing all components */
  if (cinfo->comps_in_scan == 1) {
    noninterleaved_scan_setup(cinfo);
    /* Need to read Vk MCU rows to obtain Vk block rows */
    cinfo->mcu_rows_per_loop = cinfo->cur_comp_info[0]->v_samp_factor;
  } else {
    if (interleaved_scan_setup(cinfo) == XIE_ERR)
      return(XIE_ERR);
    /* in an interleaved scan, one MCU row provides Vk block rows */
    cinfo->mcu_rows_per_loop = 1;
  }
  cinfo->total_passes++;

  /* Allocate working memory: */
  /* coeff_data holds a single MCU row of coefficient blocks */
  cinfo->coeff_data = alloc_MCU_row(cinfo);
  if (cinfo->coeff_data == (JBLOCKIMAGE) NULL)
  	return(XIE_ERR);
  /* sampled_data is sample data before upsampling */
  if (alloc_sampling_buffer(cinfo, cinfo->sampled_data) == XIE_ERR)
    return(XIE_ERR);
  if (cinfo->XIE_upsample) {
    /* fullsize_data is sample data after upsampling */
    cinfo->fullsize_data = alloc_sampimage(cinfo, (int) cinfo->num_components,
      				  (long) cinfo->rows_in_mem, cinfo->fullsize_width);
    if (cinfo->fullsize_data == (JSAMPIMAGE) NULL)
  	  return(XIE_ERR);
  }        
  if (prepare_range_limit_table(cinfo) == XIE_ERR)
    return(XIE_ERR);

  /* Initialize to read scan data */

  if (((*cinfo->methods->entropy_decode_init) (cinfo)) == XIE_ERR)
    return(XIE_ERR);
  if (cinfo->XIE_upsample)
    (*cinfo->methods->upsample_init) (cinfo);
  (*cinfo->methods->disassemble_init) (cinfo);

  cinfo->pixel_rows_output = 0;
  if (cinfo->XIE_upsample) {
    cinfo->whichss = 1;			/* arrange to start with sampled_data[0] */
  } else {
    cinfo->whichss = 0;			/* arrange to start with sampled_data[0] */
  }
  cinfo->cur_mcu_row = 0;
  cinfo->first_mcu_row = TRUE;
  return(XIE_NRML);
}	/* jdXIE_init */  


GLOBAL int
#if NeedFunctionPrototypes
jdXIE_get (decompress_info_ptr cinfo)
#else
jdXIE_get (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int whichss;
  int ri;
  int start;
  short i;

  if (cinfo->cur_mcu_row < cinfo->MCU_rows_in_scan) {
    if (cinfo->XIErestart == XIE_RNUL) {
      start = 0;
      if (cinfo->XIE_upsample) {
        cinfo->whichss ^= 1;   /* switch to other downsampled-data buffer */
      }
    } else {
      start = cinfo->ri;
    }
    
    whichss = cinfo->whichss; /* localize variable */

    /* Obtain v_samp_factor block rows of each component in the scan. */
    /* This is a single MCU row if interleaved, multiple MCU rows if not. */
    /* In the noninterleaved case there might be fewer than v_samp_factor */
    /* block rows remaining; if so, pad with copies of the last pixel row */
    /* so that upsampling doesn't have to treat it as a special case. */

    for (ri = start; ri < cinfo->mcu_rows_per_loop; ri++) {
      if ((cinfo->cur_mcu_row + ri) < cinfo->MCU_rows_in_scan) {
	  /* OK to actually read an MCU row. */
	  if (((*cinfo->methods->disassemble_MCU) (cinfo, 
	         cinfo->coeff_data)) < 0) {
          cinfo->ri = ri;  /* save loop position */
          return(XIE_INP); /* need more data */
        }
	  (*cinfo->methods->reverse_DCT) (cinfo, cinfo->coeff_data,
					cinfo->sampled_data[whichss],
					ri * DCTSIZE);
      } else {
	  /* Need to pad out with copies of the last downsampled row. */
	  /* This can only happen if there is just one component. */
	  duplicate_row(cinfo->sampled_data[whichss][0],
		      cinfo->cur_comp_info[0]->downsampled_width,
		      ri * DCTSIZE - 1, DCTSIZE);
      }
    }

    if (cinfo->XIE_upsample) {
      /* Upsample the data */
      /* First time through is a special case */

      if (cinfo->first_mcu_row) {
        /* Expand first row group with dummy above-context */
        expand(cinfo, cinfo->sampled_data[whichss], 
           cinfo->fullsize_data, cinfo->fullsize_width,
  	     (short) (-1), (short) 0, (short) 1,
  	     (short) 0);
      } else {
        /* Expand last row group of previous set */
        expand(cinfo, cinfo->sampled_data[whichss], 
           cinfo->fullsize_data, cinfo->fullsize_width,
  	     (short) DCTSIZE, (short) (DCTSIZE+1), (short) 0,
  	     (short) (DCTSIZE-1));
        /* and dump the previous set's expanded data */
        jdcopy_pixel_rows (cinfo, cinfo->fullsize_data, cinfo->output_workspace);
        /* Expand first row group of this set */
        expand(cinfo, cinfo->sampled_data[whichss], 
           cinfo->fullsize_data, cinfo->fullsize_width,
  	     (short) (DCTSIZE+1), (short) 0, (short) 1,
  	     (short) 0);
      }
   
      /* Expand second through next-to-last row groups of this set */
      for (i = 1; i <= DCTSIZE-2; i++) {
        expand(cinfo, cinfo->sampled_data[whichss], 
         cinfo->fullsize_data, cinfo->fullsize_width,
  	     (short) (i-1), (short) i, (short) (i+1),
  	     (short) i);
      }
   
      /* Return for all but last row group */
      cinfo->cur_mcu_row += cinfo->mcu_rows_per_loop;
      if (cinfo->first_mcu_row) {
        cinfo->first_mcu_row = FALSE;      
        return(XIE_NRML); /* No errors, no output */
      } else {
        return(XIE_OUT);  /* No errors, output */
      }
   
    } else {
      
      /* Do not upsample the data: copy it directly into the output workspace */  
      jdcopy_pixel_rows (cinfo, 
         cinfo->sampled_data[whichss], cinfo->output_workspace);
      cinfo->cur_mcu_row += cinfo->mcu_rows_per_loop;
      if (cinfo->cur_mcu_row < cinfo->MCU_rows_in_scan) {
        return(XIE_OUT);  /* No errors, output */
      } else {
        return(XIE_EOI);  /* No errors, output, end of input */
      }
    }
  }
      
  if (cinfo->XIE_upsample) {
    /* Expand the last row group with dummy below-context */
    /* Note whichss points to last buffer side used */
    expand(cinfo, cinfo->sampled_data[cinfo->whichss], 
         cinfo->fullsize_data, cinfo->fullsize_width,
            (short) (DCTSIZE-2), (short) (DCTSIZE-1), (short) (-1),
            (short) (DCTSIZE-1));
    /* and dump the remaining data (may be less than full height) */
    jdcopy_pixel_rows (cinfo, cinfo->fullsize_data, cinfo->output_workspace);
  }
  return(XIE_EOI);   /* No errors, output, end of input */
}  /* jdXIE_get */

#ifndef XIE_SUPPORTED
GLOBAL int
#if NeedFunctionPrototypes
jdXIE_term (decompress_info_ptr cinfo)
#else
jdXIE_term (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  /* Clean up after the scan */
  (*cinfo->methods->disassemble_term) (cinfo);
  if (cinfo->XIE_upsample)
    (*cinfo->methods->upsample_term) (cinfo);
  (*cinfo->methods->entropy_decode_term) (cinfo);
  (*cinfo->methods->read_scan_trailer) (cinfo);
  cinfo->completed_passes++;

  /* Finish output file, release working storage, etc */
  (*cinfo->methods->output_term) (cinfo);
  (*cinfo->methods->read_file_trailer) (cinfo);

  (*cinfo->emethods->d_free_all) (cinfo);

  return(XIE_NRML);
}	/* jdXIE_term */


GLOBAL void
#if NeedFunctionPrototypes
jseldXIE (decompress_info_ptr cinfo)
#else
jseldXIE (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  /* NULL function */
}	/* jseldXIE */
#endif   /* XIE_SUPPORTED */
