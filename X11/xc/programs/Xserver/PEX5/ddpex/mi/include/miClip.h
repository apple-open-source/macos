/* $Xorg: miClip.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */
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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc

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

#ifndef MI_CLIP_H
#define MI_CLIP_H
#include "miLight.h"
/*
 * CLIP_POINT4D - set oc to the clip code for the specified point.
 *
 */

#define CLIP_POINT4D(in_pt, oc, clip_mode)				\
  if ((clip_mode) == MI_MCLIP) {					\
    float t;								\
    int count;								\
    int num_halfspaces =  pddc->Static.misc.ms_MCV->numObj;		\
    ddHalfSpace *MC_HSpace = 						\
	(ddHalfSpace *)(pddc->Static.misc.ms_MCV->pList);		\
									\
    for (count = 0, (oc) = 0; count < num_halfspaces; count++) {	\
      DOT_PRODUCT(&(MC_HSpace->vector), (in_pt), t);                    \
      if ((t) < MC_HSpace->dist ) 					\
	  (oc) |= MI_CLIP_LEFT; /* any one will do */			\
      MC_HSpace++;							\
    }									\
  } else {								\
        if (((ddCoord4D *)(in_pt))->x < -((ddCoord4D *)(in_pt))->w)	\
          (oc) = MI_CLIP_LEFT;						\
        else if (((ddCoord4D *)(in_pt))->x > ((ddCoord4D *)(in_pt))->w)	\
          (oc) = MI_CLIP_RIGHT;						\
        else (oc) = 0;							\
        if (((ddCoord4D *)(in_pt))->y < -((ddCoord4D *)(in_pt))->w)	\
          (oc) |= MI_CLIP_BOTTOM;					\
        else if (((ddCoord4D *)(in_pt))->y > ((ddCoord4D *)(in_pt))->w)	\
          (oc) |= MI_CLIP_TOP;						\
        if (((ddCoord4D *)(in_pt))->z < -((ddCoord4D *)(in_pt))->w)	\
          (oc) |= MI_CLIP_FRONT;					\
        else if (((ddCoord4D *)(in_pt))->z > ((ddCoord4D *)(in_pt))->w)	\
          (oc) |= MI_CLIP_BACK;						\
  }

#endif
/*********************************************************************/

/* COMPUTE_CLIP_PARAMS - compute whether or not a point is clipped, and
 *			 how far it is to the current clipping boundary
 */

#define COMPUTE_CLIP_PARAMS(pt,t,Shift,mode,c_clip,HSpace,clip_code )	\
									\
		/* pt		-4D point in question 			\
		 * t		-floating point scale factor		\
		 * Shift	-Portion of clip code to work on 	\
		 * mode		-Model or view clipping			\
		 * c_clip	-current clip plane for view clipping	\
		 * HSpace	-pointer to half space for model clip	\
		 * clip_code	-composire clipping code output		\
		 */							\
                                                                        \
  if((mode) == MI_MCLIP) {                                              \
                                                                        \
    DOT_PRODUCT(&(HSpace)->vector, (pt).ptr, t);			\
    if((t)< ((HSpace)->dist)) clip_code |= (1<<(Shift));                  \
                                                                        \
  } else {                                                              \
  switch (c_clip)                                                 	\
   {                                                                    \
    case MI_CLIP_LEFT:                                                  \
     if ((pt).p4Dpt->x < -(pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w + (pt).p4Dpt->x;                               \
     break;                                                             \
    case MI_CLIP_RIGHT:                                                 \
     if ((pt).p4Dpt->x >  (pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w - (pt).p4Dpt->x;                               \
     break;                                                             \
    case MI_CLIP_BOTTOM:                                                \
     if ((pt).p4Dpt->y < -(pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w + (pt).p4Dpt->y;                               \
     break;                                                             \
    case MI_CLIP_TOP:                                                   \
     if ((pt).p4Dpt->y >  (pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w - (pt).p4Dpt->y;                               \
     break;                                                             \
    case MI_CLIP_FRONT:                                                 \
     if ((pt).p4Dpt->z < -(pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w + (pt).p4Dpt->z;                               \
     break;                                                             \
    case MI_CLIP_BACK:                                                  \
     if ((pt).p4Dpt->z >  (pt).p4Dpt->w)                                \
        clip_code |= (1 << (Shift));                                    \
     (t) = (pt).p4Dpt->w - (pt).p4Dpt->z;                               \
     break;                                                             \
   }                                                                    \
  }

 
    /* remember that ALL vertex types are of the form:
     *
     *   |---------------------------|---------|----------|---------|
     *             coords               color     normal      edge
     *                                  (opt)     (opt)      (opt)
     */

/* Assumes that point A is "out" and point B is "in" */
#define CLIP_AND_COPY(pt_type, in_ptA, t_A, in_ptB, t_B, out_pt)        \
   {                                                                    \
     float t;                                                           \
     if (clip_mode == MI_MCLIP)                                         \
        t = ((MC_HSpace->dist - t_A) / (t_B - t_A));                    \
     else t = (t_A) / ((t_A) - (t_B));                                  \
                                                                        \
     *(out_pt).p4Dpt = *(in_ptA).p4Dpt;                                 \
                                                                        \
     (out_pt).p4Dpt->x += t * ((in_ptB).p4Dpt->x - (in_ptA).p4Dpt->x);  \
     (out_pt).p4Dpt->y += t * ((in_ptB).p4Dpt->y - (in_ptA).p4Dpt->y);  \
     (out_pt).p4Dpt->z += t * ((in_ptB).p4Dpt->z - (in_ptA).p4Dpt->z);  \
     (out_pt).p4Dpt->w += t * ((in_ptB).p4Dpt->w - (in_ptA).p4Dpt->w);  \
                                                                        \
    (in_ptA).p4Dpt++;                                                   \
    (in_ptB).p4Dpt++;                                                   \
    (out_pt).p4Dpt++;                                                   \
                                                                        \
    if (DD_IsVertColour(pt_type))                                       \
      {                                                                 \
        *(out_pt).pRgbFloatClr = *(in_ptA).pRgbFloatClr;                \
                                                                        \
        (out_pt).pRgbFloatClr->red +=                                   \
                        t * ((in_ptB).pRgbFloatClr->red -               \
                             (in_ptA).pRgbFloatClr->red);               \
        (out_pt).pRgbFloatClr->green +=                                 \
                        t * ((in_ptB).pRgbFloatClr->green -             \
                             (in_ptA).pRgbFloatClr->green);             \
        (out_pt).pRgbFloatClr->blue +=                                  \
                        t * ((in_ptB).pRgbFloatClr->blue -              \
                             (in_ptA).pRgbFloatClr->blue);              \
      (in_ptA).pRgbFloatClr++;                                          \
      (in_ptB).pRgbFloatClr++;                                          \
      (out_pt).pRgbFloatClr++;                                          \
    }                                                                   \
    if (DD_IsVertNormal(pt_type))                                       \
      {                                                                 \
        *(out_pt).pNormal = *(in_ptA).pNormal;                          \
                                                                        \
           (out_pt).pNormal->x +=                                       \
                                t * ((in_ptB).pNormal->x -              \
                                     (in_ptA).pNormal->x);              \
           (out_pt).pNormal->y +=                                       \
                                t * ((in_ptB).pNormal->y -              \
                                     (in_ptA).pNormal->y);              \
           (out_pt).pNormal->z +=                                       \
                                t * ((in_ptB).pNormal->z -              \
                                     (in_ptA).pNormal->z);              \
      (in_ptA).pNormal++;                                               \
      (in_ptB).pNormal++;                                               \
      (out_pt).pNormal++;                                               \
    }                                                                   \
                                                                        \
    if (DD_IsVertEdge(pt_type)) {                                       \
      *(out_pt).pEdge = *(in_ptA).pEdge;                                \
      (in_ptA).pEdge++;                                                 \
      (in_ptB).pEdge++;                                                 \
      (out_pt).pEdge++;                                                 \
    }                                                                   \
    (in_ptA).ptr -= point_size;                                         \
    (in_ptB).ptr -= point_size;                                         \
    (out_pt).ptr -= point_size;                                         \
  }


/* Macros to manipulate the forward and backward edge flags of the triangle
 * strip and quad mesh primitives
 */
#define FWD_EDGE_FLAG (1<<0)
#define BKWD_EDGE_FLAG (1<<1)

#define SET_FWD_EDGE(v_ptr, edge_offset) *(v_ptr + edge_offset) |= FWD_EDGE_FLAG
#define CLEAR_FWD_EDGE(v_ptr, edge_offset) *(v_ptr + edge_offset) &= ~FWD_EDGE_FLAG
#define SET_BKWD_EDGE(v_ptr, edge_offset) *(v_ptr + edge_offset) |= BKWD_EDGE_FLAG
#define CLEAR_BKWD_EDGE(v_ptr, edge_offset) *(v_ptr + edge_offset) 	\
						&= ~BKWD_EDGE_FLAG

#define IS_ODD(num) (num & 1)  /* macro to determine the correct "sense" of
			        * triangle strip facet in order to correctly
				* calculate facet normals.
				*/

#define MI_MCLIP 0
#define MI_VCLIP 1


/* JSH - assuming copy may overlap */
#define COPY_POINT(in_pt, out_pt, point_size)                           \
        memmove( (out_pt).ptr, (in_pt).ptr, (point_size) )

/* JSH - assuming copy may overlap */
#define COPY_FACET(in_fct, out_fct, facet_size)                         \
        memmove( (out_fct), (in_fct), (facet_size) )


