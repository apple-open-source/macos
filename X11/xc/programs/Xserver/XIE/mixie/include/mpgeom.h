/* $Xorg: mpgeom.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module mpgeom.h ****/
/******************************************************************************

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
******************************************************************************
  
	mpgeom.h -- DDXIE MI prototype Geometry definitions
  
	Ben Fahy -- AGE Logic, Inc. June 1993
  
*****************************************************************************/

#ifndef _XIEH_MPGEOM
#define _XIEH_MPGEOM

/*
 * peTex extension for the Geometry element
 */
typedef struct _mpgeombanddef {
  	int	finished;	/* done with this band */
	int	yOut;		/* what output line we are on */
	int	out_of_bounds;	/* if entire input image is missed */

	double	first_mlow,	/* lowest  input line mapped by first output */
		first_mhigh;	/* highest input line mapped by first output */
	double	last_mlow,	/* lowest  input line mapped by last  output */
		last_mhigh;	/* highest input line mapped by last  output */
	double	global_mlow,	/* lowest  input line mapped, overall 	     */
		global_mhigh;	/* highest input line mapped, overall 	     */

	int	first_ilow,	/* rounded first_mlow   */
		first_ihigh;	/* rounded first_mhigh  */
	int	last_ilow,	/* rounded last_mlow    */
		last_ihigh;	/* rounded last_mlow    */
	int	global_ilow,	/* rounded global_mlow  */
		global_ihigh;	/* rounded global_mlow  */

	/* useful data precalculated for scaling */
	int	*x_locs;
	int	x_start;
	int	x_end;
	int	int_constant;	/* precalculated for Constrained data fill */
	RealPixel flt_constant;	/* precalculated for UnConstrained data fill */

	/* required by general line filling routine */
	int	in_width;
	int	in_height;

	/* keep track of what input lines we've come across */
	int	lo_src_available;
	int	hi_src_available;

	void	(*linefunc) ();
	void	(*fillfunc) ();
  }
  mpGeometryBandRec, *mpGeometryBandPtr;

typedef struct _mpgeometrydef {
  	int	input_line_increases_as_output_line_increases;
	int	input_map_size;
  	mpGeometryBandPtr bandInfo[xieValMaxBands];
  }
  mpGeometryDefRec, *mpGeometryDefPtr;

#endif /* module _XIEH_MPGEOM */
