/* $Xorg: miMarkers.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#ifndef MI_MARKERS_H
#define MI_MARKERS_H

/*
 * Marker geometry definitions
 *
 * There are five predefined marker types:
 *        PEXMarkerDot: draw a single pixel
 *        PEXMarkerCross: draws a "+" sign
 *        PEXMarkerAsterisk: draws an "*"
 *        PEXMarkerCircle: draws a circle.
 *        PEXMarkerX: draw an "X"
 *
 * The geometry for a pixel is simple and requires no pre-definition.
 * the geometry for a circle must be computed on the fly.
 *
 * The geomtries for the remaining three marker types are pre-defined
 * in the following data structures.
 *
 */

/*
 * PEXMarkerCross
 *
 * the cross is defined as two polylines:	(-1.0, 0.0) <-> (1.0, 0.0)
 * 						(0.0, -1.0) <-> (0.0, 1.0)
 */
static	ddCoord2D	cross_data_1[2]={	-1.0,  0.0,
						 1.0,  0.0 }; 
static	ddCoord2D	cross_data_2[2]={	 0.0, -1.0, 
						 0.0,  1.0 };
static	MarkerlistofddPoint	cross_list[2]={	
					2,		/* numPoints */
				2*sizeof(ddCoord2D),	/* maxData */
					cross_data_1,	/* data pointer */
					2,		/* numPoints */
				2*sizeof(ddCoord2D),	/* maxData */
					cross_data_2 };	/* data pointer */
static	miListHeader	cross_header={	DD_2D_POINT,	/* type */
					0,		/* flags */
					2,		/* numLists */
					2,		/* maxLists */
			(listofddPoint *)cross_list };	/* listofddPoint */

/*
 * PEXMarkerAsterisk
 *
 * the Asterisk is defined as four polylines: 
 * 					(-1.0, 0.0) <-> (1.0, 0.0)
 * 					(0.0, -1.0) <-> (0.0, 1.0)
 * 					(-0.707, -0.707) <-> (0.707, 0.707)
 * 					(0.707,  -0.707) <-> (-0.707, 0.707)
 *
 * Note that 0.707 is ~ sqrt(2.0)/2.0, or the endpoints of either of the
 * vertical or horizontal segments rotated by 45 degrees.
 */
static	ddCoord2D	asterisk_data_1[2]={	-1.0,  0.0,
						 1.0,  0.0 }; 
static	ddCoord2D	asterisk_data_2[2]={	 0.0, -1.0, 
						 0.0,  1.0 };
static	ddCoord2D	asterisk_data_3[2]={	-0.7071, -0.7071, 
						 0.7071,  0.7071 };
static	ddCoord2D	asterisk_data_4[2]={	 0.7071, -0.7071, 
						-0.7071,  0.7071 };
static	MarkerlistofddPoint	asterisk_list[4]={	
						2,		/*numPoints*/
				2*sizeof(ddCoord2D),		/* maxData */
						asterisk_data_1, /* data */
						2,		/*numPoints*/
				2*sizeof(ddCoord2D),		/* maxData */
						asterisk_data_2, /* data */
						2,		/*numPoints*/
				2*sizeof(ddCoord2D),		/* maxData */
						asterisk_data_3, /* data */
						2,		/*numPoints*/
				2*sizeof(ddCoord2D),		/* maxData */
						asterisk_data_4 }; /* data */
static	miListHeader	asterisk_header={	DD_2D_POINT,	/* type */
						0,		/* flags */
						4,		/* numLists */
						4,		/* maxLists */
				(listofddPoint *)asterisk_list }; /* data */
 
/*
 * PEXMarkerX
 *
 * the X is defined as two polylines:	(-1.0, -1.0) <-> (1.0, 1.0)
 * 					(1.0, -1.0)  <-> (-1.0, 1.0)
 */
static	ddCoord2D	X_data_1[2]={	-1.0, -1.0,
					 1.0,  1.0 }; 
static	ddCoord2D	X_data_2[2]={	 1.0, -1.0, 
					-1.0,  1.0 };
static	MarkerlistofddPoint	X_list[2]={	
					2,		/* numPoints */
					16,		/* maxData */
					X_data_1,	/* data pointer */
					2,		/* numPoints */
				2*sizeof(ddCoord2D),	/* maxData */
					X_data_2 };	/* data pointer */
static	miListHeader	X_header={	DD_2D_POINT,	/* type */
					0,		/* flags */
					2,		/* numLists */
					2,		/* maxLists */
			(listofddPoint *)X_list };	/* listofddPoint */


#endif
