/* $Xorg: miUtils.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miUtils.c,v 1.9 2001/12/14 19:57:40 dawes Exp $ */

#include "mipex.h"
#include "PEX.h"
#include "PEXprotost.h"
#include "ddpex3.h"
#include "miRender.h"
#include "miWks.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miMatIdent
 |
 |  Function Description:
 | 	initializes 4x4 matrices to identity.
 |
 --*/

void
miMatIdent(m)
     ddFLOAT        m[4][4];
{
    int      i, j;

    for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
	m[i][j] = ((i==j) ? 1.0 : 0.0);
      }
    }
}

/*++
 |
 |  Function Name:	miMatCopy
 |
 |  Function Description:
 | 	copies 4x4 mat
 |
 --*/

void
miMatCopy(src, dest)
     ddFLOAT        src[4][4];
     ddFLOAT        dest[4][4];
{
    int      i, j;

    for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
	dest[i][j] = src[i][j];
      }
    }
}

/*++
 |
 |  Function Name:	miMatTranspose
 |
 |  Function Description:
 | 	transposes 4x4 matrices.
 |
 --*/

void
miMatTranspose(m)
     ddFLOAT        m[4][4];
{
    int      i, j;
    ddFLOAT  t;

    for (i=1; i<4; i++) {
      for (j=0; j<i; j++) {
	t = m[i][j];
	m[i][j] = m[j][i];
	m[j][i] = t;
      }
    }
}

/*++
 |
 |  Function Name:	miMatMult
 |
 |  Function Description:
 | 	implements m = b x a for 4x4 matrices.
 |
 |  Note(s):
 |
 |	NOTE the order of the multiply: BxA *NOT* AxB
 |
 --*/

void
miMatMult(m, a, b)
ddFLOAT		m[4][4];
ddFLOAT		a[4][4];
ddFLOAT		b[4][4];
{
    register	int	i,j,k;
    register	float	*col_ptr;
    register	float	*row_ptr;
    register	float	*result;

    if ((m != a) && (m != b))
    {
     result = &(m[0][0]);
     for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
		*result = 0.0;
		col_ptr = &(b[i][0]);
		row_ptr = &(a[0][j]);
		for (k = 0; k < 4; k++) {
			*result += *row_ptr * *(col_ptr++);
			row_ptr += 4;
		}
		result++;
	}
     }
    } else {
     float	t[4][4];

     result = &(t[0][0]);
     for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
		*result = 0.0;
		col_ptr = &(b[i][0]);
		row_ptr = &(a[0][j]);
		for (k = 0; k < 4; k++) {
			*result += *row_ptr * *(col_ptr++);
			row_ptr += 4;
		}
		result++;
	}
     }
     memcpy( (char *)m, (char *)t, 16*sizeof(float));
    }
}

/*++
 |
 |  Function Name:	miPrintVertList
 |
 |  Function Description: print a formatted version of the
 |			  miListHeader structure and its contents.
 |
 --*/

void
miPrintVertList(vinput)
/* in */
miListHeader    *vinput;
{
    int			i, j;
    listofddPoint	*pddlist;
    int			vert_count;
    char                *pt;
    int			point_size;

    /*
     * Print each list.
     */
    ErrorF(" Number of lists: %d,  list data type: %d \n",
	   vinput->numLists, vinput->type);

    DD_VertPointSize(vinput->type, point_size);

    pddlist = vinput->ddList;
    for (i = 0; i < vinput->numLists; i++) {
 
      vert_count = pddlist->numPoints;
      ErrorF(" num points, list %d:  %d", i, vert_count);
 
      /*
       *
       */
      pt = (char *)pddlist->pts.p4Dpt;
 
      for (j = 0; j < vert_count; j++) {

         if (DD_IsVertFloat(vinput->type)) {

           if (DD_IsVert2D(vinput->type)) 
		ErrorF("      x %f, y %f \n", 
			((ddCoord4D *)pt)->x, 
			((ddCoord4D *)pt)->y );

           else if (DD_IsVert3D(vinput->type)) 
		ErrorF("      x %f, y %f, z %f \n", 
			((ddCoord4D *)pt)->x, 
			((ddCoord4D *)pt)->y,
			((ddCoord4D *)pt)->z );

           else
		ErrorF("      x %f, y %f, z %f \n", 
			((ddCoord4D *)pt)->x, 
			((ddCoord4D *)pt)->y,
			((ddCoord4D *)pt)->z,
			((ddCoord4D *)pt)->w );


          } else {

           if (DD_IsVert2D(vinput->type)) 
		ErrorF("      x %d, y %d \n", 
			((ddCoord3DS *)pt)->x, 
			((ddCoord3DS *)pt)->y );

           else if (DD_IsVert3D(vinput->type)) 
		ErrorF("      x %d, y %d, z %d \n", 
			((ddCoord3DS *)pt)->x, 
			((ddCoord3DS *)pt)->y,
			((ddCoord3DS *)pt)->z );

          }

	pt += point_size;

      }
      /* Now, ski to next input list */
      pddlist++;
    }
}

/*++
 |
 |  Function Name:	miTransformVector
 |
 |  Function Description: Transform a 3D point by the 3x3
 |  portion of a 4x4 matrix
 |  Added 4/8/91 by JSH. 
 |
 --*/
void
miTransformVector (p3, matrix, xp3)
/* in */
ddVector3D   *p3;
ddFLOAT     matrix[4][4];
/* out */
ddVector3D	*xp3;
{

    xp3->x = matrix[0][0]*p3->x;
    xp3->x += matrix[0][1]*p3->y;
    xp3->x += matrix[0][2]*p3->z;

    xp3->y =  matrix[1][0]*p3->x;
    xp3->y +=  matrix[1][1]*p3->y;
    xp3->y +=  matrix[1][2]*p3->z;

    xp3->z =  matrix[2][0]*p3->x;
    xp3->z +=  matrix[2][1]*p3->y;
    xp3->z +=  matrix[2][2]*p3->z;

}

/*++
 |
 |  Function Name:	miTransformPoint
 |
 |  Function Description: Transform a 4D point by a 4x4 matrix
 |
 --*/
void
miTransformPoint (p4, matrix, xp4)
/* in */
ddCoord4D   *p4;
ddFLOAT     matrix[4][4];
/* out */
ddCoord4D	*xp4;
{

    xp4->x = matrix[0][0]*p4->x;
    xp4->x += matrix[0][1]*p4->y;
    xp4->x += matrix[0][2]*p4->z;
    xp4->x += matrix[0][3]*p4->w;

    xp4->y =  matrix[1][0]*p4->x;
    xp4->y +=  matrix[1][1]*p4->y;
    xp4->y +=  matrix[1][2]*p4->z;
    xp4->y +=  matrix[1][3]*p4->w;

    xp4->z =  matrix[2][0]*p4->x;
    xp4->z +=  matrix[2][1]*p4->y;
    xp4->z +=  matrix[2][2]*p4->z;
    xp4->z +=  matrix[2][3]*p4->w;

    xp4->w =  matrix[3][0]*p4->x;
    xp4->w +=  matrix[3][1]*p4->y;
    xp4->w +=  matrix[3][2]*p4->z;
    xp4->w +=  matrix[3][3]*p4->w;
}

/*++
 |
 |  Function Name:	miMatInverse
 |
 |  Function Description: 
 |	miMatInverse - a fairly robust matrix inversion routine
 |	inverts a 4x4 matrix.  
 |
 |	TODO: If the matrix is singular, call a more robust routine (SVD)
 |	to find a solution. See Numerical Recipes in C
 |
 |
 --*/

void
miMatInverse( a )
    ddFLOAT	a[4][4];
{
    short index[4][2], ipivot[4];
    float pivot[4];
    short row, colum;
    float themax;
    short i, j, k, l;

    for (j = 0; j < 4; j++)
	ipivot[j] = 0;

    for (i = 0; i < 4; i++) {	/* do matrix inversion */
	themax = 0.0;
	for (j = 0; j < 4; j++) {	/* search for pivot element */
	    if (ipivot[j] == 1)
		continue;
	    for (k = 0; k < 4; k++) {
		if (ipivot[k] == 1)
		    continue;
		/* what does this mean? is it another singular case?
		if (ipivot[k] > 1)
		    TODO:
		*/
		if (fabs(themax) < fabs(a[j][k])) {
		    row = j;
		    colum = k;
		    themax = a[j][k];
		}
	    }
	}
	if (MI_NEAR_ZERO(themax)) {
	    /* input matrix is singular, return the an identity matrix */
	    MI_MAT_IDENTITY( a, 4 );
	   /* TODO: restore matix 'a' and call SVD routine */
	    return;
	}
	ipivot[colum] += 1;
	if (row != colum) {	/* interchange rows to put */
	    for (l = 0; l < 4; l++) {
		themax = a[row][l];
		a[row][l] = a[colum][l];
		a[colum][l] = themax;
	    }
	}
	index[i][0] = row;
	index[i][1] = colum;
	pivot[i] = a[colum][colum];
#ifndef NDEBUG
	if ((pivot[i] < 1.0e-6) && (pivot[i] > -1.0e-6) ) {
	    /* input matrix is singular, return the an identity matrix */
	    MI_MAT_IDENTITY( a, 4 );
	   /* TODO: restore matix 'a' and call SVD routine */
	}
#endif
	/* the following isn't needed if we have SVD routine */
	if (MI_NEAR_ZERO(pivot[i])) {
	   pivot[i] = MI_ZERO_TOLERANCE;
	}
	a[colum][colum] = 1.0;	/* divide pivot row by pivot element */
	for (l = 0; l < 4; l++)
	    a[colum][l] /= pivot[i];
	for (j = 0; j < 4; j++)
	    if (j != colum) {
	      themax = a[j][colum];
	      a[j][colum] = 0.0;
	      for (l = 0; l < 4; l++)
		a[j][l] -= a[colum][l] * themax;
	    }
    }

    for (i = 0; i < 4; i++) {	/* interchange columns */
	l = 4 - 1 - i;
	if (index[l][0] != index[l][1]) {
	    row = index[l][0];
	    colum = index[l][1];
	    for (k = 0; k < 4; k++) {
		themax = a[k][row];
		a[k][row] = a[k][colum];
		a[k][colum] = themax;
	    }
	}
    }
    /* determinant is

	(row == column)?1:(-1) * pivot[0] * pivot[1] * pivot[2] * pivot[3]

       if needed*/
}

/*++
 |
 |  Function Name:	miMatInverseTranspose
 |
 |  Function Description: 
 |	miMatInverseTranspose - compute the inverse transpose of a matrix.
 |
 --*/

void
miMatInverseTranspose( m )
    ddFLOAT	m[4][4];
{
	miMatInverse( m );
	miMatTranspose( m );
}

/*++
 |
 |  Function Name:	LostXResource
 |
 |  Function Description:
 |	General purpose procedure to inform ddpex when an X 
 |	resource is deleted.
 |
 |  Note(s):
 |
 --*/

void
LostXResource( pPEXResource, PEXtype, Xtype )
	diResourceHandle	pPEXResource;
	ddResourceType		PEXtype;
	ddXResourceType		Xtype;
{
	/* the only case known to use this is when a drawable is
	 * deleted and a workstation is using the drawable
	 */
    if ( (PEXtype == WORKSTATION_RESOURCE) && (Xtype == X_DRAWABLE_RESOURCE) )
    {
    	register miWksPtr   pwks = (miWksPtr)((diWKSHandle)pPEXResource)->deviceData;

	pwks->pRend->pDrawable = NULL;
	pwks->pRend->drawableId = PEXAlreadyFreed;
    }
    /* no else should ever happen */
    return;
}

/*++
 |
 |  Function Name:	mi_set_filters
 |
 |  Function Description:
 |	sets the filter flags in the ddcontext
 |
 |  Note(s):
 |
 --*/

void
mi_set_filters(pRend, pddc, namesets)
	ddRendererPtr	pRend;
	miDDContext	*pddc;
	ddBitmask	namesets;
{
	ddUSHORT	isempty;
	ddUSHORT	incl_match, excl_match;
	ddUSHORT	invert_incl_match, invert_excl_match;
	ddNamePtr	pnames;

	/* TODO: look at bitmask to smartly change filter_flags
	 * instead of always reseting everything
	 */
	pddc->Dynamic->filter_flags = 0;
	MINS_IS_NAMESET_EMPTY(pddc->Dynamic->currentNames, isempty);

	/* do search first */
	if (pRend->render_mode == MI_REND_SEARCHING)
	{
	    /* filters pass if they are empty, regardless of
	     * whether there are names in the current name set 
	     */
	    pnames = pddc->Static.search.norm_inclusion;
	    MINS_IS_NAMESET_EMPTY( pnames, incl_match );

	    pnames = pddc->Static.search.norm_exclusion;
	    MINS_IS_NAMESET_EMPTY( pnames, excl_match );

	    if (incl_match && excl_match) {
		/* norm list is empty, then all elements pass,
		 * so fake it to pass
		 */
		incl_match = 1;
		excl_match = 0;
	    } else {
		/* norm list is not empty */
	    	pnames = pddc->Static.search.norm_inclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, incl_match);
		pnames = pddc->Static.search.norm_exclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, excl_match);
	    }

	    pnames = pddc->Static.search.invert_inclusion;
	    MINS_IS_NAMESET_EMPTY( pnames, invert_incl_match );

	    pnames = pddc->Static.search.invert_exclusion;
	    MINS_IS_NAMESET_EMPTY( pnames, invert_excl_match );

	    if (invert_incl_match && invert_excl_match) {
		/* invert list is empty, then all elements rejected,
		 * so fake it to reject
		 */
		invert_incl_match = 0;
		invert_excl_match = 1;
	    } else {
		/* invert list is not empty */
		pnames = pddc->Static.search.invert_inclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, invert_incl_match);
		pnames = pddc->Static.search.invert_exclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, invert_excl_match);
	    }
	    if ((incl_match && !excl_match) &&
		    !(invert_incl_match && !invert_excl_match))
		MI_DDC_SET_DETECTABLE(pddc);
	}

	/* now, go on to other filters */
	if (isempty)
		/* current name set is empty. no filters pass */
		return;

	/* highlight */
	if ( pRend->ns[DD_HIGH_INCL_NS] )
	{
		pnames = ((miNSHeader *)pRend->ns[DD_HIGH_INCL_NS]->deviceData)->names;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, incl_match);
		if ( pRend->ns[DD_HIGH_EXCL_NS] )
		{
			pnames = ((miNSHeader *)pRend->ns[DD_HIGH_EXCL_NS]->deviceData)->names;
			MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, excl_match);
		}
		else
			excl_match = 0;
		if (incl_match && !excl_match)
			MI_DDC_SET_HIGHLIGHT(pddc);
	}
	/* else inclusion set is empty; filter does not pass */ 

	/* invisibility */
	if ( pRend->ns[DD_INVIS_INCL_NS] )
	{
		pnames = ((miNSHeader *)pRend->ns[DD_INVIS_INCL_NS]->deviceData)->names;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, incl_match);
		if ( pRend->ns[DD_INVIS_EXCL_NS] )
		{
			pnames = ((miNSHeader *)pRend->ns[DD_INVIS_EXCL_NS]->deviceData)->names;
			MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, excl_match);
		}
		else
			excl_match = 0;
		if (incl_match && !excl_match)
			MI_DDC_SET_INVISIBLE(pddc);
	}
	/* else inclusion set is empty; filter does not pass */ 

	if (pRend->render_mode == MI_REND_PICKING)
	{
		pnames = pddc->Static.pick.inclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, incl_match);
		pnames = pddc->Static.pick.exclusion;
		MINS_MATCH_NAMESETS(pnames, pddc->Dynamic->currentNames, excl_match);
		if (incl_match && !excl_match)
			MI_DDC_SET_DETECTABLE(pddc);
	} 

	if (pRend->render_mode == MI_REND_DRAWING)
		MI_DDC_SET_DETECTABLE(pddc);

	return;
}
