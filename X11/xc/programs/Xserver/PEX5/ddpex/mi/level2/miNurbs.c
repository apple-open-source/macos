/* $Xorg: miNurbs.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miNurbs.c,v 3.8 2001/12/14 19:57:29 dawes Exp $ */

#include "X.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "ddpex.h"
#include "ddpex3.h"
#include "miRender.h"
#include "ddpex2.h"
#include "miNurbs.h"
#include "pexos.h"


/*
 * mtx to convert polynomial coeffs ai, to fwd basis coeffs Aj  is
 *        D   j+1     k                     i
 * Aj =	 Sum [Sum (-1) * (j!/k!(j-k)!)*(j-k)  ] * ai   where D=degree
 *       i=0  k=0
 */

#if MAXORD == 4
/* Debugging is often easier if MAXORD is made small. */

double mi_nu_ptofd[MAXORD][MAXORD] = {
{ 1.0, 0.0, 0.0, 0.0},
{ 0.0, 1.0, 1.0, 1.0},
{ 0.0, 0.0, 2.0, 6.0},
{ 0.0, 0.0, 0.0, 6.0}
};

#else

double mi_nu_ptofd[MAXORD][MAXORD] = {
{ 1.0, 0.0, 0.0, 0.0,  0.0,   0.0,    0.0,     0.0,      0.0,       0.0},
{ 0.0, 1.0, 1.0, 1.0,  1.0,   1.0,    1.0,     1.0,      1.0,       1.0},
{ 0.0, 0.0, 2.0, 6.0, 14.0,  30.0,   62.0,   126.0,    254.0,     510.0},
{ 0.0, 0.0, 0.0, 6.0, 36.0, 150.0,  540.0,  1806.0,   5796.0,   18150.0},
{ 0.0, 0.0, 0.0, 0.0, 24.0, 240.0, 1560.0,  8400.0,  40824.0,  186480.0},
{ 0.0, 0.0, 0.0, 0.0,  0.0, 120.0, 1800.0, 16800.0, 126000.0,  834120.0},
{ 0.0, 0.0, 0.0, 0.0,  0.0,   0.0,  720.0, 15120.0, 191520.0, 1905120.0},
{ 0.0, 0.0, 0.0, 0.0,  0.0,   0.0,    0.0,  5040.0, 141120.0, 2328480.0},
{ 0.0, 0.0, 0.0, 0.0,  0.0,   0.0,    0.0,     0.0,  40320.0, 1451520.0},
{ 0.0, 0.0, 0.0, 0.0,  0.0,   0.0,    0.0,     0.0,      0.0,  362880.0}
};

#endif

#undef HUGE
#define HUGE 10E30



/*++
 |
 |  Function Name:      mi_nu_preprocess_knots
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

void
mi_nu_preprocess_knots( order, nk, knots, rp )
    ddUSHORT		order;
    int		nk;
    ddFLOAT	*knots;
    ddFLOAT	rp[][MAXORD];	/* reciprocal of knots diff */
{
    double x;

    register	int i, j;

    for ( i = 0; i < nk; i++ )
	rp[i][0] = 1.0 ;
    for ( j = 1; j < order; j++ ) {
	for ( i = 0; i <= nk - j; i++ ) {
	    if ( (x = knots[i+j] - knots[i]) == 0.0 ) {
		rp[i][j] = HUGE;
	    } else {
	        rp[i][j] = 1.0 / x;
	    }
	}
    }
}



/*++
 |
 |  Function Name:      mi_nu_compute_nurb_basis_function
 |
 |  Function Description:
 |
 | Recursive definition of polynomial coefficients for span (0<= s <=1).
 | C(i,j,k) = a*C(i,j-1,k-1) + b*C(i,j,k-1) + c*C(i+1,j-1,k-1) + d*C(i+1,j,k-1)
 | a = (s(l+1) - s(l))/(s(i+k-1) - s(i)), b = (s(l) - s(i))/(s(i+k-1) - s(i))
 | c = -(s(l+1) - s(l))/(s(i+k) - s(i+1)), d = (s(i+k) - s(l))/(s(i+k) - s(i+1))
 |
 |  Note(s):
 |
 --*/

void
mi_nu_compute_nurb_basis_function( order, span, knots, kr, C )
    ddUSHORT	order;
    int		span;
    ddFLOAT	*knots;
    ddFLOAT	kr[][MAXORD]; /* reciprocal of knots diff */
    double	C[MAXORD][MAXORD]; 
{
    int		i, j, k, m, im, degree = order - 1;
    double	t0, t1, t2, t3;

    if ( order == 2 ) {
	C[0][0] =  1.0;
	C[0][1] =  0.0;
	C[1][0] = -1.0;
	C[1][1] =  1.0;
	return;
    }

    /* Compute the coefficients of Nik in polynomial basis, for the span
     * knots[i] to knots[i+1] where s goes from 0 to 1.0
     */
    t1 = knots[span+1] - knots[span];
    C[0][degree] = 1.0;			/* Ni1 = 1.0 within span */
    for ( k = 1; k < order; k++ ) {	/* recurse on order for Cj,i,k */
	t0 = t1 * kr[span-k+1][k];
	im = degree - k;
	C[0][im] = t0 * C[0][im+1];		/* top left coeff */
	for ( j = k-1; j > 0; j-- )
	    C[j][im] = t0 * ( C[j][im+1] - C[j-1][im+1] ); /*middle*/
	C[k][im] = -t0 * C[k-1][im+1];		/* top right coeff */ 
	for (m=k-1; m>0; m--) {			/* central section */
	    i = span - m;			/* right edge first */
	    im = degree - m;
	    C[k][im] = t1 * (kr[i][k] * C[k-1][im] - kr[i+1][k] * C[k-1][im+1]);
	    t2 = knots[i+k+1] - knots[span];
	    t3 = knots[span] - knots[i];
	    for ( j = k-1; j > 0; j-- ) 	/* then j down to 1 */
		C[j][im] =
		      kr[i][k] * (t1 * C[j-1][im]
		    + t3 * C[j][im])
		    + kr[i+1][k] * (t2 * C[j][im+1] - t1 * C[j-1][im+1]);
	    C[0][im] = kr[i][k] * t3 * C[0][im]
		+ kr[i+1][k] * t2 * C[0][im+1]; /* left edge */
	}
	t0 = t1 * kr[span][k];			/* bottom rt,middle coeffs */
	for ( j = k; j > 0; j-- )
	    C[j][degree] = t0 * C[j-1][degree]; 
	C[0][degree] = 0.0;			/* bottom left coeff */
    }
}



/*++
 |
 |  Function Name:      mi_nu_insert_knots
 |
 |  Function Description:
 |
 |  Note(s):
 |
 --*/

int
mi_nu_insert_knots( order, pt_type, 
		    numinknots, oknots, opoints, 
		    numoutknots, nknots, npoints )
    ddUSHORT		order;
    ddPointType		pt_type;
    ddUSHORT		numinknots;
    ddFLOAT	*oknots;	/* original knots */
    ddFLOAT	*opoints;	/* original control points */
    int		*numoutknots;
    ddFLOAT	*nknots;	/* new knots */
    ddFLOAT	*npoints;	/* new control points */
{
    /* 
     * Assumptions: - inserted knots are within range of original knots.
     */
    int		i, k, iok, ink, mult, num_pts;
    int		numtmpknots;
    ddFLOAT	*tmpknots;
    ddFLOAT	alpha, alph1;
    ddCoord2D	*npts2;
    ddCoord3D	*npts3;
    ddCoord4D	*npts4;

    /* Check to see if new knots needed. Copy and return if not. */
    if ( *numoutknots <= 0 ) {
	*numoutknots = numinknots;
	memcpy( (char *)nknots, (char *)oknots, (int)numinknots * sizeof(ddFLOAT) );
	return 1;
    }

    /* Copy old control points into new space. */
    num_pts = numinknots - order;
    if ( DD_IsVert2D(pt_type) ) {
	memcpy( (char *)npoints, (char *)opoints, num_pts * sizeof(ddCoord2D));
	npts2 = (ddCoord2D *)npoints;
    } else if ( DD_IsVert3D(pt_type) ) {
	memcpy( (char *)npoints, (char *)opoints, num_pts * sizeof(ddCoord3D));
	npts3 = (ddCoord3D *)npoints;
    } else if ( DD_IsVert4D(pt_type) ) {
	memcpy( (char *)npoints, (char *)opoints, num_pts * sizeof(ddCoord4D));
	npts4 = (ddCoord4D *)npoints;
    } else return (1);

    if ( !(tmpknots = (ddFLOAT *)
	xalloc( (numinknots + *numoutknots) * sizeof(float))) )
	return 0;

    /* Insert new knots and control points, starting from the end of the
     * original lists.
     */
    memcpy( (char *)tmpknots, (char *)oknots, (int)numinknots * sizeof(ddFLOAT) );
    numtmpknots = numinknots;
    ink = *numoutknots;
    iok = numinknots - 1;

    while ( ink > 0 ) {

	mult = 1;
	--ink;
	/* Count mutiplicity of the new knot to be inserted. */
	while ( ink > 0 && nknots[ink] == nknots[ink-1] ) {
	    ++mult;
	    --ink;
	}

	/* Find position of knot(s) to insert. */
	while ( iok >= 0 && tmpknots[iok] >= nknots[ink] )
	    --iok;

	/* Move control points down to make space for inserted ones.
	 * Use memove so that the overlap is handled.
	 */
	 /* note that the funky &blah[...] notation is equivalent
	    to blah+...  since blah is a pointer. JSH 4-10-91
	 */
	if ( DD_IsVert2D(pt_type) )
	    memmove((char *)(&npts2[iok + 1 + mult]),(char *)(&npts2[iok + 1]), 
		((num_pts - iok) - 1) * sizeof(ddCoord2D) );
	else if ( DD_IsVert3D(pt_type) )
	    memmove((char *)(&npts3[iok + 1 + mult]),(char *)(&npts3[iok + 1]),
		((num_pts - iok) - 1) * sizeof(ddCoord3D) );
	else
	    memmove((char *)(&npts4[iok + 1 + mult]),(char *)(&npts4[iok + 1]),
		((num_pts - iok) - 1) * sizeof(ddCoord4D) );

	/* Do de Boor to insert new knot with multiplicity `mult'. */
	if ( DD_IsVert2D(pt_type) ) {
	    for ( k = 1; k <= mult; k++ ) { 
	    /* Move pts down recursively. */
		for ( i = iok + k; i > iok; i-- ) {
		    npts2[i].x = npts2[i-1].x;
		    npts2[i].y = npts2[i-1].y;
/********************************************************
		    if ( rat == PRATIONAL )
			npts2[i].z = npts2[i-1].z;
********************************************************/
		}
		for ( i = iok; i > iok - order + k; i-- ) { 
		    alpha = (nknots[ink] - tmpknots[i])
			/ (tmpknots[i + order - k] - tmpknots[i]);
		    alph1 = 1.0 - alpha;
		    npts2[i].x = alpha * npts2[i].x + alph1 * npts2[i-1].x;
		    npts2[i].y = alpha * npts2[i].y + alph1 * npts2[i-1].y;
/********************************************************
		    if ( rat == PRATIONAL )
			npts2[i].z = alpha * npts2[i].z + alph1 * npts2[i-1].z;
********************************************************/
		}
	    }

	} else if ( DD_IsVert3D(pt_type) ) { /* dim is 3 */
	    for ( k = 1; k <= mult; k++ ) { 
		for ( i = iok + k; i > iok; i-- ) {
		    npts3[i].x = npts3[i-1].x;
		    npts3[i].y = npts3[i-1].y;
		    npts3[i].z = npts3[i-1].z;
		}
		for ( i = iok; i > iok - order + k; i-- ) { 
		    alpha = (nknots[ink] - tmpknots[i])
			/ (tmpknots[i + order - k] - tmpknots[i]);
		    alph1 = 1.0 - alpha;
		    npts3[i].x = alpha * npts3[i].x + alph1 * npts3[i-1].x;
		    npts3[i].y = alpha * npts3[i].y + alph1 * npts3[i-1].y;
		    npts3[i].z = alpha * npts3[i].z + alph1 * npts3[i-1].z;
		}
	    }
	} else /* if ( DD_IsVert4D(pt_type) ) */ { /* dim is 4 */
	    for ( k = 1; k <= mult; k++ ) { 
		for ( i = iok + k; i > iok; i-- ) {
		    npts4[i].x = npts4[i-1].x;
		    npts4[i].y = npts4[i-1].y;
		    npts4[i].z = npts4[i-1].z;
		    npts4[i].w = npts4[i-1].w;
		}
		for ( i = iok; i > iok - order + k; i-- ) { 
		    alpha = (nknots[ink] - tmpknots[i])
			/ (tmpknots[i + order - k] - tmpknots[i]);
		    alph1 = 1.0 - alpha;
		    npts4[i].x = alpha * npts4[i].x + alph1 * npts4[i-1].x;
		    npts4[i].y = alpha * npts4[i].y + alph1 * npts4[i-1].y;
		    npts4[i].z = alpha * npts4[i].z + alph1 * npts4[i-1].z;
		    npts4[i].w = alpha * npts4[i].w + alph1 * npts4[i-1].w;
		}
	    }
	}

	/* Total number of points and knots increased by `mult'. */
	for ( k = numtmpknots - 1; k > iok; k-- )
	    tmpknots[k + mult] = tmpknots[k];
	for ( k = 1; k <= mult; k++ )
	    tmpknots[iok + k] = nknots[ink];
	numtmpknots += mult;
	num_pts +=mult; 
    }


    /* copy results into output buffers */
    *numoutknots = numtmpknots; /* resulting total knots */
    memcpy( (char *)nknots, (char *)tmpknots, numtmpknots * sizeof(ddFLOAT) );

    xfree( (char *)tmpknots );
    return 1;
}
