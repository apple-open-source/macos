/* $Xorg: pl_util.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include <math.h>
#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_util.h"
#include "pl_oc_util.h"


int
PEXRotate (axis, angle, matrix_return)

INPUT int		axis;
INPUT double		angle;
OUTPUT PEXMatrix	matrix_return;

{
    double	sine;
    double	cosine;

    sine = sin (angle);
    cosine = cos (angle);

    switch (axis)
    {
    case PEXXAxis:
        matrix_return[0][0] = 1.0;
        matrix_return[0][1] = 0.0;
        matrix_return[0][2] = 0.0;
        matrix_return[0][3] = 0.0;

        matrix_return[1][0] = 0.0;
        matrix_return[1][1] = cosine;
        matrix_return[1][2] = -sine;
        matrix_return[1][3] = 0.0;

        matrix_return[2][0] = 0.0;
        matrix_return[2][1] = sine;
        matrix_return[2][2] = cosine;
        matrix_return[2][3] = 0.0;

        matrix_return[3][0] = 0.0;
        matrix_return[3][1] = 0.0;
        matrix_return[3][2] = 0.0;
        matrix_return[3][3] = 1.0;
        break;

    case PEXYAxis:
        matrix_return[0][0] = cosine;
        matrix_return[0][1] = 0.0;
        matrix_return[0][2] = sine;
        matrix_return[0][3] = 0.0;

        matrix_return[1][0] = 0.0;
        matrix_return[1][1] = 1.0;
        matrix_return[1][2] = 0.0;
        matrix_return[1][3] = 0.0;

        matrix_return[2][0] = -sine;
        matrix_return[2][1] = 0.0;
        matrix_return[2][2] = cosine;
        matrix_return[2][3] = 0.0;

        matrix_return[3][0] = 0.0;
        matrix_return[3][1] = 0.0;
        matrix_return[3][2] = 0.0;
        matrix_return[3][3] = 1.0;
        break;

    case PEXZAxis:
        matrix_return[0][0] = cosine;
        matrix_return[0][1] = -sine;
        matrix_return[0][2] = 0.0;
        matrix_return[0][3] = 0.0;

        matrix_return[1][0] = sine;
        matrix_return[1][1] = cosine;
        matrix_return[1][2] = 0.0;
        matrix_return[1][3] = 0.0;

        matrix_return[2][0] = 0.0;
        matrix_return[2][1] = 0.0;
        matrix_return[2][2] = 1.0;
        matrix_return[2][3] = 0.0;

        matrix_return[3][0] = 0.0;
        matrix_return[3][1] = 0.0;
        matrix_return[3][2] = 0.0;
        matrix_return[3][3] = 1.0;
        break;

    default:
        return (PEXBadAxis); 	/* error - invalid axis specifier */
    }

    return (0);
}


void
PEXRotate2D (angle, matrix_return)

INPUT double		angle;
OUTPUT PEXMatrix3x3	matrix_return;

{
    double	sine;
    double	cosine;

    sine = sin (angle);
    cosine = cos (angle);

    matrix_return[0][0] = cosine;
    matrix_return[0][1] = -sine;
    matrix_return[0][2] = 0.0;

    matrix_return[1][0] = sine;
    matrix_return[1][1] = cosine;
    matrix_return[1][2] = 0.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
}


int
PEXRotateGeneral (point1, point2, angle, matrix_return)

INPUT PEXCoord		*point1;
INPUT PEXCoord		*point2;
INPUT double		angle;
OUTPUT PEXMatrix	matrix_return;

{
    PEXMatrix	preMatrix, calcMatrix, postMatrix, tempMatrix;
    PEXCoord	diff, rot;
    float	dist, temp, s;
    double	sine, cosine;
    int		i, j;

    /*
     * The matrix is calculated as preMatrix * calcMatrix * postMatrix
     * where postMatrix translates by point1 and preMatrix translates back
     * by point1 and calcMatrix does the real work.
     */

    preMatrix[0][0] = 1.0;
    preMatrix[0][1] = 0.0;
    preMatrix[0][2] = 0.0;
    preMatrix[0][3] = point1->x;

    preMatrix[1][0] = 0.0;
    preMatrix[1][1] = 1.0;
    preMatrix[1][2] = 0.0;
    preMatrix[1][3] = point1->y;

    preMatrix[2][0] = 0.0;
    preMatrix[2][1] = 0.0;
    preMatrix[2][2] = 1.0;
    preMatrix[2][3] = point1->z;

    preMatrix[3][0] = 0.0;
    preMatrix[3][1] = 0.0;
    preMatrix[3][2] = 0.0;
    preMatrix[3][3] = 1.0;


    postMatrix[0][0] = 1.0;
    postMatrix[0][1] = 0.0;
    postMatrix[0][2] = 0.0;
    postMatrix[0][3] = -(point1->x);

    postMatrix[1][0] = 0.0;
    postMatrix[1][1] = 1.0;
    postMatrix[1][2] = 0.0;
    postMatrix[1][3] = -(point1->y);

    postMatrix[2][0] = 0.0;
    postMatrix[2][1] = 0.0;
    postMatrix[2][2] = 1.0;
    postMatrix[2][3] = -(point1->z);

    postMatrix[3][0] = 0.0;
    postMatrix[3][1] = 0.0;
    postMatrix[3][2] = 0.0;
    postMatrix[3][3] = 1.0;


    /*
     * Compute calcMatrix
     */

    diff.x = point2->x - point1->x;
    diff.y = point2->y - point1->y;
    diff.z = point2->z - point1->z;

    dist = sqrt (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

    if (NEAR_ZERO (dist))
	return (PEXBadAxis);

    rot.x = diff.x = diff.x / dist;	/* normalize rotation vector */
    rot.y = diff.y = diff.y / dist;
    rot.z = diff.z = diff.z / dist;

    rot.x = rot.x * rot.x;		/* square it */
    rot.y = rot.y * rot.y;
    rot.z = rot.z * rot.z;

    cosine = cos (angle);
    sine = sin (angle);

    calcMatrix[0][0] = rot.x + cosine * (1.0 - rot.x);
    calcMatrix[1][1] = rot.y + cosine * (1.0 - rot.y);
    calcMatrix[2][2] = rot.z + cosine * (1.0 - rot.z);

    temp = diff.x * diff.y * (1.0 - cosine);
    s = sine * diff.z;

    calcMatrix[1][0] = temp - s;
    calcMatrix[0][1] = temp + s;

    temp = diff.x * diff.z * (1.0 - cosine);
    s = sine * diff.y;

    calcMatrix[2][0] = temp + s;
    calcMatrix[0][2] = temp - s;

    temp = diff.y * diff.z * (1.0 - cosine);
    s = sine * diff.x;

    calcMatrix[2][1] = temp - s;
    calcMatrix[1][2] = temp + s;

    calcMatrix[0][3] = 0.0;
    calcMatrix[1][3] = 0.0;
    calcMatrix[2][3] = 0.0;
    calcMatrix[3][0] = 0.0;
    calcMatrix[3][1] = 0.0;
    calcMatrix[3][2] = 0.0;
    calcMatrix[3][3] = 1.0;


    /* Multiply preMatrix by calcMatrix */

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            tempMatrix[i][j] = preMatrix[i][0] * calcMatrix[0][j] +
                               preMatrix[i][1] * calcMatrix[1][j] +
                               preMatrix[i][2] * calcMatrix[2][j] +
                               preMatrix[i][3] * calcMatrix[3][j];
        }
    }


    /* Multiply by postMatrix and return the new matrix */

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            matrix_return[i][j] = tempMatrix[i][0] * postMatrix[0][j] +
                                  tempMatrix[i][1] * postMatrix[1][j] +
                                  tempMatrix[i][2] * postMatrix[2][j] +
                                  tempMatrix[i][3] * postMatrix[3][j];
        }
    }

    return (0);
}


void
PEXScale (scale_vector, matrix_return)

INPUT PEXVector		*scale_vector;
OUTPUT PEXMatrix	matrix_return;

{
    matrix_return[0][0] = scale_vector->x;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;
    matrix_return[0][3] = 0.0;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = scale_vector->y;
    matrix_return[1][2] = 0.0;
    matrix_return[1][3] = 0.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = scale_vector->z;
    matrix_return[2][3] = 0.0;

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;
}


void
PEXScale2D (scale_vector, matrix_return)

INPUT PEXVector2D	*scale_vector;
OUTPUT PEXMatrix3x3	matrix_return;

{
    matrix_return[0][0] = scale_vector->x;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = scale_vector->y;
    matrix_return[1][2] = 0.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
}


void
PEXTranslate (trans_vector, matrix_return)

INPUT PEXVector		*trans_vector;
OUTPUT PEXMatrix	matrix_return;

{
    matrix_return[0][0] = 1.0;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;
    matrix_return[0][3] = trans_vector->x;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0;
    matrix_return[1][2] = 0.0;
    matrix_return[1][3] = trans_vector->y;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
    matrix_return[2][3] = trans_vector->z;

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;
}


void
PEXTranslate2D (trans_vector, matrix_return)

INPUT PEXVector2D	*trans_vector;
OUTPUT PEXMatrix3x3	matrix_return;

{
    matrix_return[0][0] = 1.0;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = trans_vector->x;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0;
    matrix_return[1][2] = trans_vector->y;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
}


void
PEXMatrixMult (matrix1, matrix2, matrix_return)

INPUT PEXMatrix		matrix1;
INPUT PEXMatrix		matrix2;
OUTPUT PEXMatrix	matrix_return;

{
    register float	*r;
    register int	i;

    if ((matrix_return != matrix1) && (matrix_return != matrix2))
    {
	for (i = 0; i < 4; i++)
	{
	    r = matrix1[i];
	    matrix_return[i][0] = r[0] * matrix2[0][0] + r[1] * matrix2[1][0] +
				  r[2] * matrix2[2][0] + r[3] * matrix2[3][0];
	    matrix_return[i][1] = r[0] * matrix2[0][1] + r[1] * matrix2[1][1] +
				  r[2] * matrix2[2][1] + r[3] * matrix2[3][1];
	    matrix_return[i][2] = r[0] * matrix2[0][2] + r[1] * matrix2[1][2] +
				  r[2] * matrix2[2][2] + r[3] * matrix2[3][2];
	    matrix_return[i][3] = r[0] * matrix2[0][3] + r[1] * matrix2[1][3] +
				  r[2] * matrix2[2][3] + r[3] * matrix2[3][3];
	}
    }
    else
    {
	register float	*src, *dst;
	PEXMatrix	temp;
	
	for (i = 0; i < 4; i++)
	{
	    r = matrix1[i];
	    temp[i][0] = r[0] * matrix2[0][0] + r[1] * matrix2[1][0] +
		      	 r[2] * matrix2[2][0] + r[3] * matrix2[3][0];
	    temp[i][1] = r[0] * matrix2[0][1] + r[1] * matrix2[1][1] +
			 r[2] * matrix2[2][1] + r[3] * matrix2[3][1];
	    temp[i][2] = r[0] * matrix2[0][2] + r[1] * matrix2[1][2] +
			 r[2] * matrix2[2][2] + r[3] * matrix2[3][2];
	    temp[i][3] = r[0] * matrix2[0][3] + r[1] * matrix2[1][3] +
			 r[2] * matrix2[2][3] + r[3] * matrix2[3][3];
	}

	src = (float *) temp;
	dst = (float *) matrix_return;
	for (i = 0; i < 16; i++)
	    *dst++ = *src++;
    }
}


void
PEXMatrixMult2D (matrix1, matrix2, matrix_return)

INPUT PEXMatrix3x3	matrix1;
INPUT PEXMatrix3x3	matrix2;
OUTPUT PEXMatrix3x3	matrix_return;

{
    register float	*r;
    register int	i;

    if ((matrix_return != matrix1) && (matrix_return != matrix2))
    {
	for (i = 0; i < 3; i++)
	{
	    r = matrix1[i];
	    matrix_return[i][0] = r[0] * matrix2[0][0] + r[1] * matrix2[1][0] +
				  r[2] * matrix2[2][0];
	    matrix_return[i][1] = r[0] * matrix2[0][1] + r[1] * matrix2[1][1] +
				  r[2] * matrix2[2][1];
	    matrix_return[i][2] = r[0] * matrix2[0][2] + r[1] * matrix2[1][2] +
				  r[2] * matrix2[2][2];
	}
    }
    else
    {
	register float	*src, *dst;
	PEXMatrix3x3 	temp;
	
	for (i = 0; i < 3; i++)
	{
	    r = matrix1[i];
	    temp[i][0] = r[0] * matrix2[0][0] + r[1] * matrix2[1][0] +
		      	 r[2] * matrix2[2][0];
	    temp[i][1] = r[0] * matrix2[0][1] + r[1] * matrix2[1][1] +
			 r[2] * matrix2[2][1];
	    temp[i][2] = r[0] * matrix2[0][2] + r[1] * matrix2[1][2] +
			 r[2] * matrix2[2][2];
	}

	src = (float *) temp;
	dst = (float *) matrix_return;
	for (i = 0; i < 9; i++)
	    *dst++ = *src++;
    }
}


void
PEXBuildTransform (fixed_point, trans_vector, angle_x, angle_y, angle_z,
    scale_vector, matrix_return)

INPUT PEXCoord		*fixed_point;
INPUT PEXVector		*trans_vector;
INPUT double		angle_x;
INPUT double		angle_y;
INPUT double		angle_z;
INPUT PEXVector		*scale_vector;
OUTPUT PEXMatrix	matrix_return;

{
    /*
     * Translate fixed_point to the origin, scale, rotate, translate back
     * to fixed_point, then apply trans_vector:
     *
     *			T * Tf~ * Rz * Ry * Rx * S * Tf.
     *
     *    where:	T is the "trans_vector" transform,
     *			Tf ia the translation of fixed_point to the origin and
     *			Tf~ is it's inverse,
     *			Ri is the rotation transform about the i'th axis,
     *			S is the scaling transform.
     */

    register float	cz, sz, cx, sx, cy, sy;
    register float	*r;

    cx = cos (angle_x); sx = sin (angle_x);
    cy = cos (angle_y); sy = sin (angle_y);
    cz = cos (angle_z); sz = sin (angle_z);

    r = matrix_return[0];
    r[0] = cz * cy * scale_vector->x;
    r[1] = (cz * sx * sy - sz * cx) * scale_vector->y;
    r[2] = (cz * sy * cx + sz * sx) * scale_vector->z;
    r[3] = trans_vector->x + fixed_point->x -
	(r[0] * fixed_point->x + r[1] * fixed_point->y +
        r[2] * fixed_point->z);

    r = matrix_return[1];
    r[0] = sz * cy * scale_vector->x;
    r[1] = (sz * sx * sy + cz * cx) * scale_vector->y;
    r[2] = (sz * sy * cx - cz * sx) * scale_vector->z;
    r[3] = trans_vector->y + fixed_point->y -
	(r[0] * fixed_point->x + r[1] * fixed_point->y +
        r[2] * fixed_point->z);

    r = matrix_return[2];
    r[0] = -sy * scale_vector->x;
    r[1] = cy * sx * scale_vector->y;
    r[2] = cy * cx * scale_vector->z;
    r[3] = trans_vector->z + fixed_point->z -
	(r[0] * fixed_point->x + r[1] * fixed_point->y +
        r[2] * fixed_point->z);

    r = matrix_return[3];
    r[0] = r[1] = r[2] = 0.0;
    r[3] = 1.0;
}


void
PEXBuildTransform2D (fixed_point, trans_vector, angle_z,
    scale_vector, matrix_return)

INPUT PEXCoord2D	*fixed_point;
INPUT PEXVector2D	*trans_vector;
INPUT double		angle_z;
INPUT PEXVector2D	*scale_vector;
OUTPUT PEXMatrix3x3	matrix_return;

{
    /*
     * Translate fixed_point to the origin, scale, rotate, translate back
     * to fixed_point, then apply trans_vector:
     *
     *			T * Tf~ * R * S * Tf.
     *
     *    where:	T is the "trans_vector" transform,
     *			Tf ia the translation of fixed_point to the origin and
     *			Tf~ is it's inverse,
     *			R is the rotation transform,
     *			S is the scaling transform.
     */

    register float	*r;
    register float	c, s;

    c = cos (angle_z);
    s = sin (angle_z);

    r = matrix_return[0];
    r[0] = c * scale_vector->x;
    r[1] = -s * scale_vector->y;
    r[2] = trans_vector->x + fixed_point->x -
	c * scale_vector->x * fixed_point->x +
        s * scale_vector->y * fixed_point->y;

    r = matrix_return[1];
    r[0] = s * scale_vector->x;
    r[1] = c * scale_vector->y;
    r[2] = trans_vector->y + fixed_point->y -
	(s * scale_vector->x * fixed_point->x +
        c * scale_vector->y * fixed_point->y);

    r = matrix_return[2];
    r[0] = r[1] = 0.0;
    r[2] = 1.0;
}


int
PEXViewOrientationMatrix (vrp, vpn, vup, matrix_return)

INPUT PEXCoord          *vrp;
INPUT PEXVector         *vpn;
INPUT PEXVector         *vup;
OUTPUT PEXMatrix        matrix_return;

{
    /*
     * Translate to VRP then change the basis.
     * The old basis is: e1 = < 1, 0, 0>,  e2 = < 0, 1, 0>, e3 = < 0, 0, 1>.
     * The new basis is: ("x" means cross product)
     *		e3' = VPN / |VPN|
     *		e1' = VUP x VPN / |VUP x VPN|
     *		e2' = e3' x e1'
     * Therefore the transform from old to new is x' = ATx, where:
     *
     *	     | e1' 0 |         | 1 0 0 -vrp.x |
     *   A = |       |,    T = | 0 1 0 -vrp.y |
     *       | e2' 0 |         | 0 0 1 -vrp.z |
     *	     |       |         | 0 0 0    1   |
     *	     | e3' 0 |
     *	     |       |
     *	     | -0-  1|
     */

    /* These ei's are really ei primes. */
    float	*e1 = matrix_return[0],
                *e2 = matrix_return[1],
                *e3 = matrix_return[2];
    double	s, mag_vpn;

    if (ZERO_MAG (mag_vpn = MAG_V3 (vpn)))
    {
	return (PEXBadVector);
    }
    else if (ZERO_MAG (MAG_V3 (vup)))
    {
	return (PEXBadVector);
    }
    else
    {
	/*
         * e1' = VUP x VPN / |VUP x VPN|, but do the division later.
	 */

	e1[0] = vup->y * vpn->z - vup->z * vpn->y;
	e1[1] = vup->z * vpn->x - vup->x * vpn->z;
	e1[2] = vup->x * vpn->y - vup->y * vpn->x;

	s = sqrt (e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);


	/*
	 * Check for vup and vpn colinear (zero dot product).
	 */

	if (ZERO_MAG (s))
	    return (PEXBadVectors);
    }
    

    /*
     * Normalize e1
     */

    s = 1.0 / s;
    e1[0] *= s; e1[1] *= s; e1[2] *= s;


    /*
     * e3 = VPN / |VPN|
     */

    s = 1.0 / mag_vpn;
    e3[0] = s * vpn->x; e3[1] = s * vpn->y; e3[2] = s * vpn->z;


    /*
     * e2 = e3 x e1
     */

    e2[0] = e3[1] * e1[2] - e3[2] * e1[1];
    e2[1] = e3[2] * e1[0] - e3[0] * e1[2];
    e2[2] = e3[0] * e1[1] - e3[1] * e1[0];


    /*
     * Add the translation
     */

    e1[3] = -(e1[0] * vrp->x + e1[1] * vrp->y + e1[2] * vrp->z);
    e2[3] = -(e2[0] * vrp->x + e2[1] * vrp->y + e2[2] * vrp->z);
    e3[3] = -(e3[0] * vrp->x + e3[1] * vrp->y + e3[2] * vrp->z);


    /*
     * Homogeneous entries
     */

    matrix_return[3][0] = matrix_return[3][1] = matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;

    return (0);
}


int
PEXViewOrientationMatrix2D (vrp, vup, matrix_return)

INPUT PEXCoord2D        *vrp;
INPUT PEXVector2D       *vup;
OUTPUT PEXMatrix3x3     matrix_return;

{
    /*
     * The old basis is: e1 = < 1, 0>,  e2 = < 0, 1>
     * The new basis is: e1' = < vup.y, -vup.x> / |vup|,  e2' = vup / |vup|.
     * Therefore the transform for old to new is x' = ATx, where:
     *
     *	     | e1' 0 |         | 1 0 -vrp.x |
     *	 A = |       |,    T = | 0 1 -vrp.y |
     *	     | e2' 0 |         | 0 0    1   |
     *	     |       |
     *	     | -0-  1|
     */

    register double	s;

    if (ZERO_MAG (s = MAG_V2 (vup)))
    {
	return (PEXBadVector);
    }
    else
    {
	/*
	 * Compute the new basis, note that matrix_return[0] is e1'
	 * and matrix_return[1] is e2'.
	 */

	s = 1.0 / s;
	matrix_return[0][0] = s * vup->y;
	matrix_return[0][1] = s * -vup->x;
	matrix_return[1][0] = s * vup->x;
	matrix_return[1][1] = s * vup->y;


	/*
	 * Add the translation
	 */

	matrix_return[0][2] = -(matrix_return[0][0] * vrp->x +
				matrix_return[0][1] * vrp->y);
	matrix_return[1][2] = -(matrix_return[1][0] * vrp->x +
				matrix_return[1][1] * vrp->y);


	/*
	 * Homogeneous entries
	 */

	matrix_return[2][0] = matrix_return[2][1] = 0.0;
	matrix_return[2][2] = 1.0;

	return (0);
    }
}


int
PEXViewMappingMatrix (frame, viewport, perspective, prp, view_plane,
    back_plane, front_plane, matrix_return)

INPUT PEXCoord2D	*frame;
INPUT PEXNPCSubVolume	*viewport;
INPUT int		perspective;
INPUT PEXCoord		*prp;
INPUT double		view_plane;
INPUT double		back_plane;
INPUT double		front_plane;
OUTPUT PEXMatrix	matrix_return;

{
    /*
     * Procedure:
     *
     * (Perspective):
     *   - Translate to PRP,			Tc
     *	 - Convert to left handed coords,	Tlr
     *	 - Shear,				H
     *	 - Scale to canonical view volume,	S
     *	 - Normalize perspective view volume,	Ntp
     *	 - Scale to viewport,			Svp
     *	 - Convert to right handed coords,	Tlr
     *	 - Translate to viewport,		Tvp
     *
     * (Parallel):
     *	 - Translate to view plane,		Tc
     *	 - Shear about the view plane,		H
     *	 - Translate back,			Tc inverse
     *	 - Translate frame to origin,		Tl
     *	 - Scale to canonical view volume,	S
     *	 - Scale to viewport,			Svp
     *	 - Translate to viewport,		Tvp
     */

    double	dx = viewport->max.x - viewport->min.x;
    double	dy = viewport->max.y - viewport->min.y;
    double	dz = viewport->max.z - viewport->min.z;
    double	vvz = front_plane - back_plane;
    double	sz, sx, sy;
    double	hx, hy;
    double	d;
    double	zf;
    float	*r;

    if (!(frame[0].x < frame[1].x) || !(frame[0].y < frame[1].y))
    {
	return (PEXBadLimits);
    }
    else if (!(viewport->min.x < viewport->max.x) ||
             !(viewport->min.y < viewport->max.y) ||
             !(viewport->min.z <= viewport->max.z))
    {
	return (PEXBadViewport);
    }
    else if (NEAR_ZERO (vvz) && viewport->min.z != viewport->max.z)
    {
	return (PEXBadPlanes);
    }
    else if (perspective && prp->z < front_plane && prp->z > back_plane)
    {
	return (PEXBadPlanes);
    }
    else if (prp->z == view_plane)
    {
	return (PEXBadPRP);
    }
    else if (front_plane < back_plane)
    {
	return (PEXBadPlanes);
    }
    else if (!IN_RANGE (0.0, 1.0, viewport->min.x) ||
	     !IN_RANGE (0.0, 1.0, viewport->max.x) ||
	     !IN_RANGE (0.0, 1.0, viewport->min.y) ||
	     !IN_RANGE (0.0, 1.0, viewport->max.y) ||
	     !IN_RANGE (0.0, 1.0, viewport->min.z) ||
	     !IN_RANGE (0.0, 1.0, viewport->max.z))
    {
	return (PEXBadViewport);
    }
    
    if (perspective)
    {
	d = prp->z - view_plane;
	sz = 1.0 / (prp->z - back_plane);
	sx = sz * d * 2.0 / (frame[1].x - frame[0].x);
	sy = sz * d * 2.0 / (frame[1].y - frame[0].y);
	hx = (prp->x - 0.5 * (frame[0].x + frame[1].x)) / d;
	hy = (prp->y - 0.5 * (frame[0].y + frame[1].y)) / d;

	r = matrix_return[0];
	r[0] = 0.5 * dx * sx;
	r[1] = 0.0;
	r[2] = -(0.5 * dx * (sx * hx + sz) + sz * viewport->min.x);
	r[3] = -(0.5 * dx * sx * (prp->x - hx * prp->z) -
	    sz * prp->z * (0.5 * dx + viewport->min.x));

	r = matrix_return[1];
	r[0] = 0.0;
	r[1] = 0.5 * dy * sy;
	r[2] = -(0.5 * dy * (sy * hy + sz) + sz * viewport->min.y);
	r[3] = -(0.5 * dy * sy * (prp->y - hy * prp->z) -
	    sz * prp->z * (0.5 * dy + viewport->min.y));

	r = matrix_return[2];
	r[0] = r[1] = 0.0;
	zf = (prp->z - front_plane) / (prp->z - back_plane);
	if (NEAR_ZERO (1.0 - zf))
	{
	    r[2] = 0.0;
	    r[3] = sz * prp->z * viewport->max.z;
	}
	else
	{
	    r[2] = sz * ((dz / (1.0 - zf)) - viewport->max.z);
	    r[3] = sz * prp->z * viewport->max.z -
		(dz / (1.0 - zf)) * (sz * prp->z - zf);
	}

	r = matrix_return[3];
	r[0] = r[1] = 0.0;
	r[2] = -sz;
	r[3] = sz * prp->z;
    }
    else
    {	/* parallel */
	sx = dx / (frame[1].x - frame[0].x);
	sy = dy / (frame[1].y - frame[0].y);
	hx = (prp->x - 0.5 * (frame[0].x + frame[1].x)) /
	    (view_plane - prp->z);
	hy = (prp->y - 0.5 * (frame[0].y + frame[1].y)) /
	    (view_plane - prp->z);

	r = matrix_return[0];
	r[0] = sx;
	r[1] = 0.0;
	r[2] = sx * hx;
	r[3] = viewport->min.x - sx * (hx * view_plane + frame[0].x);

	r = matrix_return[1];
	r[0] = 0.0;
	r[1] = sy;
	r[2] = sy * hy;
	r[3] = viewport->min.y - sy * (hy * view_plane + frame[0].y);

	r  = matrix_return[2];
	r[0] = r[1] = 0.0;
	if (NEAR_ZERO (vvz))
	    r[2] = 0.0;
	else
	    r[2] = dz / vvz;
	r[3] = viewport->min.z - r[2] * back_plane;

	r = matrix_return[3];
	r[0] = r[1] = r[2] = 0.0;
	r[3] = 1.0;
    }
    
    return (0);
}


int
PEXViewMappingMatrix2D (frame, viewport, matrix_return)

INPUT PEXCoord2D	*frame;
INPUT PEXCoord2D	*viewport;
OUTPUT PEXMatrix3x3	matrix_return;

{
    /*
     * 1. Translate frame's lower-left-corner to 0,0.
     * 2. Scale size of frame to size of viewport.
     * 3. Translate 0,0 to viewport's lower-left-corner.
     *
     * Matrices are:
     *
     * 1:  1 0 -frame[0].x    2:  scale.x   0    0   3:  1 0  viewport[0].x
     * 	   0 1 -frame[0].y	   0     scale.y 0       0 1  viewport[0].y
     * 	   0 0   1		   0	   0	 1       0 0   1
     */

    /* scale factors: len (viewport) / len (frame) */
    register float	sx, sy;


    if (!(frame[0].x < frame[1].x) || !(frame[0].y < frame[1].y))
    {
	return (PEXBadLimits);
    }
    else if (!(viewport[0].x < viewport[1].x) ||
	     !(viewport[0].y < viewport[1].y))
    {
	return (PEXBadViewport);
    }
    else if (!IN_RANGE (0.0, 1.0, viewport[0].x) ||
	     !IN_RANGE (0.0, 1.0, viewport[1].x) ||
	     !IN_RANGE (0.0, 1.0, viewport[0].y) ||
	     !IN_RANGE (0.0, 1.0, viewport[1].y))
    {
	return (PEXBadViewport);
    }

    sx = (viewport[1].x - viewport[0].x) / (frame[1].x - frame[0].x);
    sy = (viewport[1].y - viewport[0].y) / (frame[1].y - frame[0].y);

    matrix_return[0][0] = sx;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = sx * (-frame[0].x + viewport[0].x);

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = sy;
    matrix_return[1][2] = sy * (-frame[0].y + viewport[0].y);

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;

    return (0);
}


int
PEXLookAtViewMatrix (from, to, up, matrix_return)

INPUT PEXCoord		*from;
INPUT PEXCoord		*to;
INPUT PEXVector		*up;
OUTPUT PEXMatrix	matrix_return;

{
    PEXCoord		a, b, c, d, e, f, t;
    float		magnitude;
    float		dot_product;

    /*
     * This matrix can be thought of as having 2 parts.  The first part (next
     * to the coordinate point when it is being transformed) moves the to
     * point to the origin.  The second part performs the rotation of the data.
     *
     * The three basis vectors of the rotation are obtained as follows.
     * The Z basis vector is determined by subtracting from from to and
     * dividing by its length (b).  The Y basis vector is determined by
     * calculating the vector perpendicular to b and in the plane defined by
     * the to and from points and the up vector and then normalizing it (e).
     * The X basis vector (f) is calculated by e CROSS b.
     *
     * The resulting matrix looks like this:
     *
     *  | fx fy fz 0 | | 1 0 0 -tox | | fx fy fz tz |
     *  | ex ey ez 0 |*| 0 1 0 -toy |=| ex ey ez ty |
     *  | bx by bz 0 | | 0 0 1 -toz | | bx by bz tz |
     *  |  0  0  0 1 | | 0 0 0   1  | |  0  0  0  1 |
     *
     * where tx = -to DOT f, ty = -to DOT e, and tz = -to DOT b.
     */

    /*
     * Calculate the rotations
     */

    a.x = from->x - to->x;	    /* difference between to and from */
    a.y = from->y - to->y;
    a.z = from->z - to->z;

    magnitude = sqrt (a.x * a.x + a.y * a.y + a.z * a.z);

    if (ZERO_MAG (magnitude))
    {
	return (PEXBadVectors);    /* from and to are the same */
    }


    /*
     * normalize the from-to vector
     */

    b.x = a.x / magnitude;
    b.y = a.y / magnitude;
    b.z = a.z / magnitude;


    /*
     * up is second basis vector
     */

    c.x = up->x;
    c.y = up->y;
    c.z = up->z;


    /*
     * compute the dot product of the previous two vectors
     */

    dot_product = (c.x * b.x) + (c.y * b.y) + (c.z * b.z);


    /*
     * calculate the vector perpendicular to the up vector and in the
     * plane defined by the to and from points and the up vector.
     */

    d.x = c.x - (dot_product * b.x);
    d.y = c.y - (dot_product * b.y);
    d.z = c.z - (dot_product * b.z);

    magnitude = sqrt (d.x * d.x + d.y * d.y + d.z * d.z);

    if (ZERO_MAG (magnitude))   /* use the defaults */
    {
        c.x = 0.0;
        c.y = 1.0;
        c.z = 0.0;

        dot_product = b.y;

        d.x = -(dot_product * b.x);
        d.y = 1.0 - (dot_product * b.y);
        d.z = -(dot_product * b.z);

        magnitude = sqrt (d.x * d.x + d.y * d.y + d.z * d.z);

        if (ZERO_MAG (magnitude))
        {
            c.x = 0.0;
            c.y = 0.0;
            c.z = 1.0;

            dot_product = b.z;

            d.x = -(dot_product * b.x);
            d.y = -(dot_product * b.y);
            d.z = 1.0 -(dot_product * b.z);

            magnitude = sqrt (d.x * d.x + d.y * d.y + d.z * d.z);
        }
    }

    /*
     * calculate the unit vector in the from, to, and at plane and
     * perpendicular to the up vector
     */

    e.x = d.x / magnitude;
    e.y = d.y / magnitude;
    e.z = d.z / magnitude;


    /*
     * calculate the unit vector perpendicular to the other two
     * by crossing them
     */

    f.x = (e.y * b.z) - (b.y * e.z);
    f.y = (e.z * b.x) - (b.z * e.x);
    f.z = (e.x * b.y) - (b.x * e.y);


    /*
     * fill in the rotation values
     */

    matrix_return[0][0] = f.x;
    matrix_return[0][1] = f.y;
    matrix_return[0][2] = f.z;

    matrix_return[1][0] = e.x;
    matrix_return[1][1] = e.y;
    matrix_return[1][2] = e.z;

    matrix_return[2][0] = b.x;
    matrix_return[2][1] = b.y;
    matrix_return[2][2] = b.z;


    /*
     * calculate the translation part of the matrix
     */

    t.x = (-to->x * f.x) + (-to->y * f.y) + (-to->z * f.z);
    t.y = (-to->x * e.x) + (-to->y * e.y) + (-to->z * e.z);
    t.z = (-to->x * b.x) + (-to->y * b.y) + (-to->z * b.z);

    matrix_return[0][3] = t.x;
    matrix_return[1][3] = t.y;
    matrix_return[2][3] = t.z;


    /*
     * and fill in the rest of the matrix
     */

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;

    return (0);
}


int
PEXPolarViewMatrix (from, distance, azimuth, altitude, twist, matrix_return)

INPUT PEXCoord		*from;
INPUT double		distance;
INPUT double		azimuth;
INPUT double		altitude;
INPUT double		twist;
OUTPUT PEXMatrix	matrix_return;

{
    PEXCoord	trans;
    float	cost;
    float	sint;
    float	cosaz;
    float	sinaz;
    float	cosalt;
    float	sinalt;

    if (distance <= ZERO_TOLERANCE)
    {
	return (PEXBadDistance);
    }

    cost = cos (twist);
    sint = sin (twist);
    cosaz = cos (-azimuth);
    sinaz = sin (-azimuth);
    cosalt = cos (-altitude);
    sinalt = sin (-altitude);


    /*
     * This matrix can be thought of as having five parts.  The first part
     * (the part nearest the point to be transformed) translates the from point
     * to the origin.  The last part translates the rotated data back by
     * distance so that the to point is at the origin.  (Since the data is
     * lined up along the Z axis, there are no X and Y parts to this
     * translation.)
     *
     * The three parts in the middle rotate around the Y axis by azimuth,
     * rotate around the X axis by altitude, and rotate around the Z axis by
     * twist.
     *
     * The matrix calculated in this routine is the result of:
     *
     * (trans, -distance)*(twist, Z)*(altitude, X)*(azimuth, Y)*(trans, -from)
     *
     *     | cost -sint 0 0 | | 1    0       0   0 | | cosaz 0 sinaz 0 |
     *	Td*| sint  cost 0 0 |*| 0 cosalt -sinalt 0 |*|   0   1   0   0 |*Tfrom
     *	   |  0     0   1 0 | | 0 sinalt  cosalt 0 | |-sinaz 0 cosaz 0 |
     *	   |  0     0   0 1 | | 0    0       0   1 | |   0   0   0   1 |
     */

    matrix_return[0][0] = cost * cosaz + (-sint) * (-sinalt) * (-sinaz);
    matrix_return[0][1] = (-sint) * cosalt;
    matrix_return[0][2] = cost * sinaz + (-sint) * (-sinalt) * cosaz;

    matrix_return[1][0] = sint * cosaz + cost * (-sinalt) * (-sinaz);
    matrix_return[1][1] = cost * cosalt;
    matrix_return[1][2] = sint * sinaz + cost * (-sinalt) * cosaz;

    matrix_return[2][0] = cosalt * (-sinaz);
    matrix_return[2][1] = sinalt;
    matrix_return[2][2] = cosalt * cosaz;


    /*
     * calculate the translation part of the matrix
     */

    trans.x = -from->x * matrix_return[0][0] + -from->y * matrix_return[0][1] +
	      -from->z * matrix_return[0][2];
    trans.y = -from->x * matrix_return[1][0] + -from->y * matrix_return[1][1] +
	      -from->z * matrix_return[1][2];
    trans.z = -from->x * matrix_return[2][0] + -from->y * matrix_return[2][1] +
	      -from->z * matrix_return[2][2];

    matrix_return[0][3] = trans.x;
    matrix_return[1][3] = trans.y;
    matrix_return[2][3] = trans.z + distance;


    /*
     * finish filling in the matrix
     */

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;

    return (0);
}


int
PEXOrthoProjMatrix (height, aspect, near_plane, far_plane, matrix_return)

INPUT double		height;
INPUT double		aspect;
INPUT double		near_plane;
INPUT double		far_plane;
OUTPUT PEXMatrix	matrix_return;

{
    float 	width;
    float	depth;

    width = height * aspect;
    depth = near_plane - far_plane;

    if (NEAR_ZERO (depth) || NEAR_ZERO (width) || NEAR_ZERO (height))
    {
	return (PEXBadLimits);
    }


    /*
     * This matrix maps the width, height, and depth to the range of zero to
     * one in all three directions.  It also maps the z values to be
     * in front of the origin.  The near plane is mapped to z = 1 and the
     * far plane is mapped to z = 0.  It also translates the x, y coordinates
     * over by .5 so that x=0 and y=0 is in the middle of the NPC space.
     */

    matrix_return[0][0] = 1.0 / width;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;
    matrix_return[0][3] = 0.5;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0 / height;
    matrix_return[1][2] = 0.0;
    matrix_return[1][3] = 0.5;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0 / depth;
    matrix_return[2][3] = 1.0  - (near_plane / depth);

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;

    return (0);
}


int
PEXPerspProjMatrix (fovy, distance, aspect, near_plane, far_plane, matrix_return)

INPUT double		fovy;
INPUT double		distance;
INPUT double		aspect;
INPUT double		near_plane;
INPUT double		far_plane;
OUTPUT PEXMatrix	matrix_return;

{
    double	fovy2;
    double	c_hy;
    double	s_hy;
    float	height;
    float	depth;
    float	width;
    float	eye_distance;

    if (near_plane <= far_plane || NEAR_ZERO (fovy) ||
        NEAR_ZERO (aspect) || distance <= near_plane)
    {
	return (PEXBadLimits);
    }


    /*
     * limit the field of view to be less than pi (minus a little) and ensure
     * that it's positive and then halve it
     */

    if ((fovy > 3.140) || (fovy < -3.140))
    {
	fovy2 = 3.140 / 2.0;
    }
    else if (fovy < 0.0)
    {
	fovy2 = -fovy / 2.0;
    }
    else
    {
	fovy2 = fovy / 2.0;
    }


    /*
     * calculate some dimensions we need
     */

    c_hy = cos (fovy2);
    s_hy = sin (fovy2);

    eye_distance = distance - near_plane;
    height = 2.0 * eye_distance * (s_hy / c_hy);
    depth = near_plane - far_plane;
    width = height * aspect;

    /*
       This matrix is made up of three parts.  The first part is a perspective
       transformation matrix.  The second part is a matrix to scale and
       translate the coordinates so that z is between 0 and 1, x is
       between height/2 and -height/2 and y is between width/2 and
       -width/2.  The third part is a matrix to translate the eyepoint to
       .5 in x and y.
     
       The viewing frustum looks like this.  We want to project the point
       (x, y, z) onto the near plane to get (x', y', z').

  +Z             |      o (x, y, z)
 <==             |    / :
                 |  /   :
      (x',y',z') |/     :
                /|      :
              /  |      :
            <----|----------------
                 |      z
                 |
           eye  near

       By similar triangles, x'/eye_dist = x/(eye_dist+near-z)
                             y'/eye_dist = y/(eye_dist+near-z)
                             z' = near

       So the projection matrix, Mproj =

             | 1   0      0        0                |
             | 0   1      0        0                |
             | 0   0      0       near              |
             | 0   0 -1/eye_dist  1+(near/eye_dist) |

       (Row vectors are shown below for simplicity.  They're really column
       vectors.)

       It can be shown that:
	Mproj * (0, 0, near, 1) = (0, 0, near, 1)
	Mproj * (0, 0, far, 1)  = (0, 0, near, 1+near/eye_dist-far/eye_dist)

       Next, we want to find a matrix, Mst, such that the near plane is
       transformed to z=1 and the far plane is transformed to z=0.
	Mst * Mproj * (0, 0, near, 1) = (0, 0, 1, 1)
	Mst * Mproj * (0, 0, far, 1)  = (0, 0, 0, t)
                                          where t = (eye_dist-far+near)/eye_dist

       Using the equations just above, this means that
	Mst * (0, 0, near, 1) = (0, 0, 1, 1)
	Mst * (0, 0, near, t) = (0, 0, 0, t)

       Concentrating on the lower right-hand corner of Mst, this means
	Mst * | near near | = | 1 0 |
              |  1    t   |   | 1 t |

       Solving for Mst, we get
	Mst = | t/(far(t-1))  -1/(t-1) |
              |     0            1     |

       Multiplying and simplifying, we get
	Mst * Mproj = | 1   0       0               0         |
                      | 0   1       0               0         |
                      | 0   0    1/depth       -far/depth     |
                      | 0   0  -1/eye_dist  1 + near/eye_dist |
                                                            where depth=near-far

       Now scale X and Y so that they have a range of "width"
       in X and "height" in Y.   The resulting matrix is
	M2 = | 1/width      0        0              0         |
             |    0     1/height     0              0         |
             |    0         0    1/depth       -far/depth     |
             |    0         0  -1/eye_dist  1 + near/eye_dist |

       Last, we need to translate the results by .5 in X and Y so that the eye
       point is in the middle of NPC space.
	matrix_return = | 1 0 0 .5 | * M2
                    | 0 1 0 .5 |
                    | 0 0 1  0 |
                    | 0 0 0  1 |

	= | 1/width      0    -1/(2*eye_dist) (1+near/eye_dist)/2 |
          |    0     1/height -1/(2*eye_dist) (1+near/eye_dist)/2 |
          |    0         0        1/depth          -far/depth     |
          |    0         0      -1/eye_dist     1+near/eye_dist   |
    */	  

    matrix_return[0][0] = 1.0 / width;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = -1.0 / (2.0 * eye_distance);
    matrix_return[0][3] = (1.0 + (near_plane / eye_distance)) / 2.0;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0 / height;
    matrix_return[1][2] = -1.0 / (2.0 * eye_distance);
    matrix_return[1][3] = (1.0 + (near_plane / eye_distance)) / 2.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0 / depth;
    matrix_return[2][3] = -far_plane / depth;

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = -1.0 / eye_distance;
    matrix_return[3][3] = 1.0 + (near_plane / eye_distance);

    return (0);
}


int
PEXTransformPoints (mat, count, points, points_return)

INPUT PEXMatrix		mat;
INPUT int		count;
INPUT PEXCoord		*points;
OUTPUT PEXCoord		*points_return;

{
    register PEXCoord	*point_in = points;
    register PEXCoord	*point_out = points_return;
    register int	i;
    PEXCoord		temp;
    int			status = 0;


    if (NEAR_ZERO (mat[3][0]) && NEAR_ZERO (mat[3][1]) &&
	NEAR_ZERO (mat[3][2]) && NEAR_ZERO (mat[3][3] - 1.0))
    {
	for (i = 0; i < count; i++, point_in++, point_out++)
	{
	    temp.x = mat[0][0] * point_in->x +
		     mat[0][1] * point_in->y +
		     mat[0][2] * point_in->z + mat[0][3];

	    temp.y = mat[1][0] * point_in->x +
		     mat[1][1] * point_in->y +
		     mat[1][2] * point_in->z + mat[1][3];

	    temp.z = mat[2][0] * point_in->x +
		     mat[2][1] * point_in->y +
		     mat[2][2] * point_in->z + mat[2][3];

	    point_out->x = temp.x;
	    point_out->y = temp.y;
	    point_out->z = temp.z;
	}
    }
    else
    {
	register float	w;

	for (i = 0; i < count; i++, point_in++, point_out++)
	{
	    w = mat[3][0] * point_in->x + mat[3][1] * point_in->y +
	        mat[3][2] * point_in->z + mat[3][3];

	    if (NEAR_ZERO (w))
	    {
		point_out->x = 0.0;
		point_out->y = 0.0;
		point_out->z = 0.0;

		status = PEXBadHomoCoord;   /* return an error */
	    }
	    else
	    {
		temp.x = (mat[0][0] * point_in->x +
			  mat[0][1] * point_in->y +
                          mat[0][2] * point_in->z + mat[0][3]) / w;

		temp.y = (mat[1][0] * point_in->x +
			  mat[1][1] * point_in->y +
			  mat[1][2] * point_in->z + mat[1][3]) / w;

		temp.z = (mat[2][0] * point_in->x +
			  mat[2][1] * point_in->y +
			  mat[2][2] * point_in->z + mat[2][3]) / w;

		point_out->x = temp.x;
		point_out->y = temp.y;
		point_out->z = temp.z;
	    }
	}
    }

    return (status);
}


int
PEXTransformPoints2D (mat, count, points, points_return)

INPUT PEXMatrix3x3	mat;
INPUT int		count;
INPUT PEXCoord2D	*points;
OUTPUT PEXCoord2D	*points_return;

{
    register PEXCoord2D *point_in = points;
    register PEXCoord2D	*point_out = points_return;
    register int	i;
    PEXCoord2D		temp;
    int			status = 0;


    if (NEAR_ZERO (mat[2][0]) && NEAR_ZERO (mat[2][1]) &&
	NEAR_ZERO (mat[2][2] - 1.0))
    {
	for (i = 0; i < count; i++, point_in++, point_out++)
	{
	    temp.x = mat[0][0] * point_in->x +
		     mat[0][1] * point_in->y + mat[0][2];

	    temp.y = mat[1][0] * point_in->x +
		     mat[1][1] * point_in->y + mat[1][2];

	    point_out->x = temp.x;
	    point_out->y = temp.y;
	}
    }
    else
    {
	register float	w;

	for (i = 0; i < count; i++, point_in++, point_out++)
	{
	    w = mat[2][0] * point_in->x + mat[2][1] * point_in->y + mat[2][2];

	    if (NEAR_ZERO (w))
	    {
		point_out->x = 0.0;
		point_out->y = 0.0;

		status = PEXBadHomoCoord;   /* return an error */
	    }
	    else
	    {
		temp.x = (mat[0][0] * point_in->x +
			  mat[0][1] * point_in->y + mat[0][2]) / w;

		temp.y = (mat[1][0] * point_in->x +
			  mat[1][1] * point_in->y + mat[1][2]) / w;

		point_out->x = temp.x;
		point_out->y = temp.y;
	    }
	}
    }

    return (status);
}


void
PEXTransformPoints4D (mat, count, points, points_return)

INPUT PEXMatrix		mat;
INPUT int		count;
INPUT PEXCoord4D	*points;
OUTPUT PEXCoord4D	*points_return;

{
    register PEXCoord4D	*point_in = points;
    register PEXCoord4D	*point_out = points_return;
    register int	i;
    PEXCoord4D		temp;


    for (i = 0; i < count; i++, point_in++, point_out++)
    {
        temp.x = mat[0][0] * point_in->x + mat[0][1] * point_in->y +
                 mat[0][2] * point_in->z + mat[0][3] * point_in->w;

        temp.y = mat[1][0] * point_in->x + mat[1][1] * point_in->y +
                 mat[1][2] * point_in->z + mat[1][3] * point_in->w;

        temp.z = mat[2][0] * point_in->x + mat[2][1] * point_in->y +
                 mat[2][2] * point_in->z + mat[2][3] * point_in->w;

        temp.w = mat[3][0] * point_in->x + mat[3][1] * point_in->y +
                 mat[3][2] * point_in->z + mat[3][3] * point_in->w;

	point_out->x = temp.x;
	point_out->y = temp.y;
	point_out->z = temp.z;
	point_out->w = temp.w;
    }
}


void
PEXTransformPoints2DH (mat, count, points, points_return)

INPUT PEXMatrix3x3	mat;
INPUT int		count;
INPUT PEXCoord		*points;
OUTPUT PEXCoord		*points_return;

{
    register PEXCoord	*point_in = points;
    register PEXCoord	*point_out = points_return;
    register int	i;
    PEXCoord		temp;


    for (i = 0; i < count; i++, point_in++, point_out++)
    {
        temp.x = mat[0][0] * point_in->x + mat[0][1] * point_in->y +
                 mat[0][2] * point_in->z;

        temp.y = mat[1][0] * point_in->x + mat[1][1] * point_in->y +
                 mat[1][2] * point_in->z;

        temp.z = mat[2][0] * point_in->x + mat[2][1] * point_in->y +
                 mat[2][2] * point_in->z;

	point_out->x = temp.x;
	point_out->y = temp.y;
	point_out->z = temp.z;
    }
}


void
PEXTransformVectors (mat, count, vectors, vectors_return)

INPUT PEXMatrix		mat;
INPUT int		count;
INPUT PEXVector		*vectors;
OUTPUT PEXVector	*vectors_return;

{
    register PEXVector	*vec_in = vectors;
    register PEXVector	*vec_out = vectors_return;
    register int	i;
    PEXVector		temp;


    for (i = 0; i < count; i++, vec_in++, vec_out++)
    {
        temp.x = mat[0][0] * vec_in->x + mat[0][1] * vec_in->y +
                 mat[0][2] * vec_in->z;

        temp.y = mat[1][0] * vec_in->x + mat[1][1] * vec_in->y +
                 mat[1][2] * vec_in->z;

        temp.z = mat[2][0] * vec_in->x + mat[2][1] * vec_in->y +
                 mat[2][2] * vec_in->z;

        vec_out->x = temp.x;
        vec_out->y = temp.y;
        vec_out->z = temp.z;
    }
}


void
PEXTransformVectors2D (mat, count, vectors, vectors_return)

INPUT PEXMatrix3x3	mat;
INPUT int		count;
INPUT PEXVector2D	*vectors;
OUTPUT PEXVector2D	*vectors_return;

{
    register PEXVector2D	*vec_in = vectors;
    register PEXVector2D	*vec_out = vectors_return;
    register int		i;
    PEXVector2D			temp;


    for (i = 0; i < count; i++, vec_in++, vec_out++)
    {
        temp.x = mat[0][0] * vec_in->x + mat[0][1] * vec_in->y;
        temp.y = mat[1][0] * vec_in->x + mat[1][1] * vec_in->y;

        vec_out->x = temp.x;
        vec_out->y = temp.y;

    }
}


int
PEXNormalizeVectors (count, vectors, vectors_return)

INPUT int 		count;
INPUT PEXVector		*vectors;
OUTPUT PEXVector	*vectors_return;

{
    register int 	i;
    register PEXVector	*vec_in = vectors;
    register PEXVector	*vec_out = vectors_return;
    float		sum, length;
    int 		status = 0;


    for (i = 0; i < count; i++, vec_in++, vec_out++)
    {
	sum = vec_in->x * vec_in->x +
	      vec_in->y * vec_in->y +
              vec_in->z * vec_in->z;

	if (NEAR_ZERO (sum))
	{
	    vec_out->x = 0.0;
	    vec_out->y = 0.0;
	    vec_out->z = 0.0;
	    status = PEXBadVector;
	}
	else
	{
	    length = sqrt (sum);

	    vec_out->x = vec_in->x / length;
	    vec_out->y = vec_in->y / length;
	    vec_out->z = vec_in->z / length;
	}   
    }

    return (status);
}


int
PEXNormalizeVectors2D (count, vectors, vectors_return)

INPUT int 		count;
INPUT PEXVector2D	*vectors;
OUTPUT PEXVector2D	*vectors_return;

{
    register int 		i;
    register PEXVector2D	*vec_in = vectors;
    register PEXVector2D	*vec_out = vectors_return;
    float			sum, length;
    int 			status = 0;


    for (i = 0; i < count; i++, vec_in++, vec_out++)
    {
	sum = vec_in->x * vec_in->x +
	      vec_in->y * vec_in->y;

	if (NEAR_ZERO (sum))
	{
	    vec_out->x = 0.0;
	    vec_out->y = 0.0;
	    status = PEXBadVector;
	}
	else
	{
	    length = sqrt (sum);

	    vec_out->x = vec_in->x / length;
	    vec_out->y = vec_in->y / length;
	}   
    }

    return (status);
}


int
PEXNPCToXCTransform (npc_sub_volume, viewport,
    window_height, transform_return)

INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT PEXDeviceCoord	*viewport;
INPUT unsigned int	window_height;
OUTPUT PEXMatrix	transform_return;

{
    /*
     *       M4          M3            M2            M1
     *
     *    1  0 0 0     1 0 0 Vx     Sx 0  0  0     1 0 0 -Nx
     *    0 -1 0 H     0 1 0 Vy     0  Sy 0  0     0 1 0 -Ny
     *    0  0 1 0     0 0 1 Vz     0  0  Sz 0     0 0 1 -Nz
     *    0  0 0 1     0 0 0  1     0  0  0  1     0 0 0  1
     *
     *    M1 : translates NPC subvolume to origin
     *         (Nx, Ny, Nz) is the lower left corner of the NPC volume.
     *
     *    M2 : scales NPC subvolme at origin to DC viewport at origin.
     *         Sx, Sy, Sz are the scale factors that maintain the aspect ratio.
     *
     *    M3 : translates DC viewport at origin to the correct position.
     *         (Vx, Vy, Vz) is the lower left corner of the DC viewport.
     *
     *    M4 : maps DC to X drawable coordinates (flips Y).
     *         H is the window height.
     */

    float 		scale_x, scale_y, scale_z;
    int 		dvx, dvy;
    float		dnx, dny;
    float		dvz, dnz;
    PEXDeviceCoord	new_viewport[2];


    dvx = viewport[1].x - viewport[0].x;
    dvy = viewport[1].y - viewport[0].y;
    dvz = viewport[1].z - viewport[0].z;

    if (dvx <= 0 || dvy <= 0 || !(viewport[0].z <= viewport[1].z))
	return (PEXBadViewport);

    if (BAD_SUBVOLUME (npc_sub_volume))
	return (PEXBadSubVolume);

    dnx = npc_sub_volume->max.x - npc_sub_volume->min.x;
    dny = npc_sub_volume->max.y - npc_sub_volume->min.y;
    dnz = npc_sub_volume->max.z - npc_sub_volume->min.z;

    scale_x = (float) dvx / dnx;
    scale_y = (float) dvy / dny;
    scale_z = NEAR_ZERO (dnz) ? 1.0 : (dvz / dnz);

    if (!NEAR_ZERO (scale_x - scale_y))
    {
	new_viewport[0].x = viewport[0].x;
	new_viewport[0].y = viewport[0].y;
	new_viewport[0].z = viewport[0].z;

	if (scale_y < scale_x)
	{
	    new_viewport[1].x = viewport[0].x + (float) dvy * dnx / dny;
	    new_viewport[1].y = viewport[1].y;
	    new_viewport[1].z = viewport[1].z;
	}
	else
	{
	    new_viewport[1].x = viewport[1].x;
	    new_viewport[1].y = viewport[0].y + (float) dvx * dny / dnx;
	    new_viewport[1].z = viewport[1].z;
	}

	viewport = new_viewport;
    }

    transform_return[0][0] = scale_x;
    transform_return[0][1] = 0.0;
    transform_return[0][2] = 0.0;
    transform_return[0][3] =
	viewport[0].x - (scale_x * npc_sub_volume->min.x);

    transform_return[1][0] = 0.0;
    transform_return[1][1] = -scale_y;
    transform_return[1][2] = 0.0;
    transform_return[1][3] =
	window_height - viewport[0].y + (scale_y * npc_sub_volume->min.y);

    transform_return[2][0] = 0.0;
    transform_return[2][1] = 0.0;
    transform_return[2][2] = scale_z;
    transform_return[2][3] =
	viewport[0].z - (scale_z * npc_sub_volume->min.z);

    transform_return[3][0] = 0.0;
    transform_return[3][1] = 0.0;
    transform_return[3][2] = 0.0;
    transform_return[3][3] = 1.0;

    return (0);
}


int
PEXNPCToXCTransform2D (npc_sub_volume, viewport,
    window_height, transform_return)

INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT PEXDeviceCoord2D	*viewport;
INPUT unsigned int	window_height;
OUTPUT PEXMatrix3x3	transform_return;

{
    /*
     *      M4         M3         M2          M1
     *
     *    1  0 0     1 0 Vx     Sx 0  0     1 0 -Nx
     *    0 -1 H     0 1 Vy     0  Sy 0     0 1 -Ny
     *    0  0 1     0 0  1     0  0  1     0 0   1 
     *
     *    M1 : translates NPC subvolume to origin
     *         (Nx, Ny) is the lower left corner of the NPC volume.
     *
     *    M2 : scales NPC subvolme at origin to DC viewport at origin.
     *         Sx, Sy are the scale factors that maintain the aspect ratio.
     *
     *    M3 : translates DC viewport at origin to the correct position.
     *         (Vx, Vy) is the lower left corner of the DC viewport.
     *
     *    M4 : maps DC to X drawable coordinates (flips Y).
     *         H is the window height.
     */

    float 		scale_x, scale_y;
    int 		dvx, dvy;
    float		dnx, dny;
    PEXDeviceCoord2D	new_viewport[2];


    dvx = viewport[1].x - viewport[0].x;
    dvy = viewport[1].y - viewport[0].y;

    if (dvx <= 0 || dvy <= 0)
	return (PEXBadViewport);

    if (BAD_SUBVOLUME (npc_sub_volume))
	return (PEXBadSubVolume);

    dnx = npc_sub_volume->max.x - npc_sub_volume->min.x;
    dny = npc_sub_volume->max.y - npc_sub_volume->min.y;

    scale_x = (float) dvx / dnx;
    scale_y = (float) dvy / dny;

    if (!NEAR_ZERO (scale_x - scale_y))
    {
	new_viewport[0].x = viewport[0].x;
	new_viewport[0].y = viewport[0].y;

	if (scale_y < scale_x)
	{
	    new_viewport[1].x = viewport[0].x + (float) dvy * dnx / dny;
	    new_viewport[1].y = viewport[1].y;
	}
	else
	{
	    new_viewport[1].x = viewport[1].x;
	    new_viewport[1].y = viewport[0].y + (float) dvx * dny / dnx;
	}

	viewport = new_viewport;
    }

    transform_return[0][0] = scale_x;
    transform_return[0][1] = 0.0;
    transform_return[0][2] =
	viewport[0].x - (scale_x * npc_sub_volume->min.x);

    transform_return[1][0] = 0.0;
    transform_return[1][1] = -scale_y;
    transform_return[1][2] =
	window_height - viewport[0].y + (scale_y * npc_sub_volume->min.y);

    transform_return[2][0] = 0.0;
    transform_return[2][1] = 0.0;
    transform_return[2][2] = 1.0;

    return (0);
}


int
PEXXCToNPCTransform (npc_sub_volume, viewport,
    window_height, transform_return)

INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT PEXDeviceCoord	*viewport;
INPUT unsigned int	window_height;
OUTPUT PEXMatrix	transform_return;

{
    /*
     *       M4           M3             M2           M1   
     *	      	                                        
     *    1 0 0 Nx     Sx 0  0  0     1 0 0 -Vx    1  0 0 0
     *    0 1 0 Ny     0  Sy 0  0     0 1 0 -Vy    0 -1 0 H
     *    0 0 1 Nz     0  0  Sz 0     0 0 1 -Vz    0  0 1 0
     *    0 0 0  1     0  0  0  1     0 0 0  1     0  0 0 1
     *
     *    M1 : maps X drawable coordinates to DC (flips Y).
     *         H is the window height.
     *
     *    M2 : translates DC viewport to origin
     *         (Vx, Vy, Vz) is the lower left corner of the DC viewport.
     *
     *    M3 : scales DC viewport at origin to NPC subvolme at origin.
     *         Sx, Sy, Sz are the scale factors that maintain the aspect ratio.
     *
     *    M4 : translates NPC subvolume at origin to the correct position.
     *         (Nx, Ny, Nz) is the lower left corner of the NPC volume.
     */

    float 		scale_x, scale_y, scale_z;
    int 		dvx, dvy;
    float		dnx, dny;
    float		dvz, dnz;
    PEXDeviceCoord	new_viewport[2];


    dvx = viewport[1].x - viewport[0].x;
    dvy = viewport[1].y - viewport[0].y;
    dvz = viewport[1].z - viewport[0].z;

    if (dvx <= 0 || dvy <= 0 || !(viewport[0].z <= viewport[1].z))
	return (PEXBadViewport);

    if (BAD_SUBVOLUME (npc_sub_volume))
	return (PEXBadSubVolume);

    dnx = npc_sub_volume->max.x - npc_sub_volume->min.x;
    dny = npc_sub_volume->max.y - npc_sub_volume->min.y;
    dnz = npc_sub_volume->max.z - npc_sub_volume->min.z;

    scale_x = dnx / (float) dvx;
    scale_y = dny / (float) dvy;
    scale_z = NEAR_ZERO (dvz) ? 1.0 : (dnz / dvz);

    if (!NEAR_ZERO (scale_x - scale_y))
    {
	new_viewport[0].x = viewport[0].x;
	new_viewport[0].y = viewport[0].y;
	new_viewport[0].z = viewport[0].z;

	if (scale_x < scale_y)
	{
	    new_viewport[1].x = viewport[0].x + (float) dvy * dnx / dny;
	    new_viewport[1].y = viewport[1].y;
	    new_viewport[1].z = viewport[1].z;
	}
	else
	{
	    new_viewport[1].x = viewport[1].x;
	    new_viewport[1].y = viewport[0].y + (float) dvx * dny / dnx;
	    new_viewport[1].z = viewport[1].z;
	}

	viewport = new_viewport;
    }

    transform_return[0][0] = scale_x;
    transform_return[0][1] = 0.0;
    transform_return[0][2] = 0.0;
    transform_return[0][3] =
	npc_sub_volume->min.x - (scale_x * viewport[0].x);

    transform_return[1][0] = 0.0;
    transform_return[1][1] = -scale_y;
    transform_return[1][2] = 0.0;
    transform_return[1][3] =
	npc_sub_volume->min.y + scale_y * (window_height - viewport[0].y);

    transform_return[2][0] = 0.0;
    transform_return[2][1] = 0.0;
    transform_return[2][2] = 1.0;
    transform_return[2][3] =
	npc_sub_volume->min.z - (scale_z * viewport[0].z);

    transform_return[3][0] = 0.0;
    transform_return[3][1] = 0.0;
    transform_return[3][2] = 0.0;
    transform_return[3][3] = 1.0;

    return (0);
}


int
PEXXCToNPCTransform2D (npc_sub_volume, viewport,
    window_height, transform_return)

INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT PEXDeviceCoord2D	*viewport;
INPUT unsigned int	window_height;
INPUT PEXMatrix3x3	transform_return;

{
    /*
     *      M4         M3          M2          M1   
     *	      	                                        
     *    1 0 Nx     Sx 0  0     1 0 -Vx     1  0 0
     *    0 1 Ny     0  Sy 0     0 1 -Vy     0 -1 H
     *    0 0  1     0  0  1     0 0  1      0  0 1
     *
     *    M1 : maps X drawable coordinates to DC (flips Y).
     *         H is the window height.
     *
     *    M2 : translates DC viewport to origin
     *         (Vx, Vy) is the lower left corner of the DC viewport.
     *
     *    M3 : scales DC viewport at origin to NPC subvolme at origin.
     *         Sx, Sy are the scale factors that maintain the aspect ratio.
     *
     *    M4 : translates NPC subvolume at origin to the correct position.
     *         (Nx, Ny) is the lower left corner of the NPC volume.
     */

    float 		scale_x, scale_y;
    int 		dvx, dvy;
    float		dnx, dny;
    PEXDeviceCoord2D	new_viewport[2];


    dvx = viewport[1].x - viewport[0].x;
    dvy = viewport[1].y - viewport[0].y;

    if (dvx <= 0 || dvy <= 0)
	return (PEXBadViewport);

    if (BAD_SUBVOLUME (npc_sub_volume))
	return (PEXBadSubVolume);

    dnx = npc_sub_volume->max.x - npc_sub_volume->min.x;
    dny = npc_sub_volume->max.y - npc_sub_volume->min.y;

    scale_x = dnx / (float) dvx;
    scale_y = dny / (float) dvy;

    if (!NEAR_ZERO (scale_x - scale_y))
    {
	new_viewport[0].x = viewport[0].x;
	new_viewport[0].y = viewport[0].y;

	if (scale_x < scale_y)
	{
	    new_viewport[1].x = viewport[0].x + (float) dvy * dnx / dny;
	    new_viewport[1].y = viewport[1].y;
	}
	else
	{
	    new_viewport[1].x = viewport[1].x;
	    new_viewport[1].y = viewport[0].y + (float) dvx * dny / dnx;
	}

	viewport = new_viewport;
    }

    transform_return[0][0] = scale_x;
    transform_return[0][1] = 0.0;
    transform_return[0][2] =
	npc_sub_volume->min.x - (scale_x * viewport[0].x);

    transform_return[1][0] = 0.0;
    transform_return[1][1] = -scale_y;
    transform_return[1][2] =
	npc_sub_volume->min.y + scale_y * (window_height - viewport[0].y);

    transform_return[2][0] = 0.0;
    transform_return[2][1] = 0.0;
    transform_return[2][2] = 1.0;

    return (0);
}


int
PEXMapXCToNPC (point_count, points, window_height,
    z_dc, viewport, npc_sub_volume, view_count, views,
    view_return, count_return, points_return)

INPUT int		point_count;
INPUT PEXDeviceCoord2D	*points;
INPUT unsigned int	window_height;
INPUT double		z_dc;
INPUT PEXDeviceCoord	*viewport;
INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT int		view_count;
INPUT PEXViewEntry	*views;
OUTPUT int		*view_return;
OUTPUT int		*count_return;
OUTPUT PEXCoord		*points_return;

{
    PEXCoord	*xc_wz_points;
    PEXMatrix 	xform;
    int		pts_in_view;
    int		max_pts_in_view;
    int		max_pts_view;
    int		status, i, j;

    /*
     * Fill in the Z value for each XC point.
     */

    xc_wz_points = (PEXCoord *) Xmalloc (
	(unsigned) (point_count * sizeof (PEXCoord)));

    for (i = 0; i < point_count; i++)
    {
	xc_wz_points[i].x = points[i].x;
	xc_wz_points[i].y = points[i].y;
	xc_wz_points[i].z = z_dc;
    }


    /*
     * Determine the transformation matrix that takes us from XC to NPC.
     */

    status = PEXXCToNPCTransform (npc_sub_volume, viewport,
	window_height, xform);

    if (status != 0)
	return (status);


    /*
     * Now transform the XC points to NPC.
     */

    status = PEXTransformPoints (xform, point_count,
	xc_wz_points, points_return);

    Xfree ((char *) xc_wz_points);

    if (status != 0)
	return (status);


    /*
     * Search for the view containing all (or most) of the NPC points.
     */

    max_pts_view = -1;
    max_pts_in_view = 0;

    for (i = 0; i < view_count; i++)
    {
	for (j = pts_in_view = 0; j < point_count; j++)
	{
	    if (POINT3D_IN_VIEW (points_return[j], views[i]))
		pts_in_view++;
	}
		
	if (pts_in_view == point_count)
	{
	    max_pts_in_view = point_count;
	    max_pts_view = i;
	    break;
	}
	else if (pts_in_view > max_pts_in_view)
	{
	    max_pts_in_view = pts_in_view;
	    max_pts_view = i;
	}
    }


    /*
     * Make sure we only return points that are in the view we found.
     */

    if (max_pts_in_view > 0 && max_pts_in_view != point_count)
    {
	int count = 0;

	for (i = 0; i < point_count && count < max_pts_in_view; i++)
	{
	    if (POINT3D_IN_VIEW (points_return[i], views[max_pts_view]))
	    {
		points_return[count].x = points_return[i].x;
		points_return[count].y = points_return[i].y;
		points_return[count].z = points_return[i].z;
		count++;
	    }
	}
    }
		
    *view_return = max_pts_view;
    *count_return = max_pts_in_view;

    return (0);
}


int
PEXMapXCToNPC2D (point_count, points, window_height,
    viewport, npc_sub_volume, view_count, views,
    view_return, count_return, points_return)

INPUT int		point_count;
INPUT PEXDeviceCoord2D	*points;
INPUT unsigned int	window_height;
INPUT PEXDeviceCoord2D  *viewport;
INPUT PEXNPCSubVolume	*npc_sub_volume;
INPUT int		view_count;
INPUT PEXViewEntry	*views;
OUTPUT int		*view_return;
OUTPUT int		*count_return;
OUTPUT PEXCoord2D	*points_return;

{
    PEXMatrix3x3	xform;
    PEXCoord2D		*xc_points;
    int			pts_in_view;
    int			max_pts_in_view;
    int			max_pts_view;
    int			status, i, j;

    /*
     * Fill in the XC point.
     */

    xc_points = (PEXCoord2D *) Xmalloc (
	(unsigned) (point_count * sizeof (PEXCoord2D)));

    for (i = 0; i < point_count; i++)
    {
	xc_points[i].x = points[i].x;
	xc_points[i].y = points[i].y;
    }


    /*
     * Determine the transformation matrix that takes us from XC to NPC.
     */

    status = PEXXCToNPCTransform2D (npc_sub_volume, viewport,
	window_height, xform);

    if (status != 0)
	return (status);


    /*
     * Now transform the XC points to NPC.
     */

    status = PEXTransformPoints2D (xform, point_count,
	xc_points, points_return);

    Xfree ((char *) xc_points);

    if (status != 0)
	return (status);


    /*
     * Search for the view containing all (or most) of the NPC points.
     */

    max_pts_view = -1;
    max_pts_in_view = 0;

    for (i = 0; i < view_count; i++)
    {
	for (j = pts_in_view = 0; j < point_count; j++)
	{
	    if (POINT2D_IN_VIEW (points_return[j], views[i]))
		pts_in_view++;
	}
		
	if (pts_in_view == point_count)
	{
	    max_pts_in_view = point_count;
	    max_pts_view = i;
	    break;
	}
	else if (pts_in_view > max_pts_in_view)
	{
	    max_pts_in_view = pts_in_view;
	    max_pts_view = i;
	}
    }


    /*
     * Make sure we only return points that are in the view we found.
     */

    if (max_pts_in_view > 0 && max_pts_in_view != point_count)
    {
	int count = 0;

	for (i = 0; i < point_count && count < max_pts_in_view; i++)
	{
	    if (POINT2D_IN_VIEW (points_return[i], views[max_pts_view]))
	    {
		points_return[count].x = points_return[i].x;
		points_return[count].y = points_return[i].y;
		count++;
	    }
	}
    }
		
    *view_return = max_pts_view;
    *count_return = max_pts_in_view;

    return (0);
}


int
PEXInvertMatrix (matrix, inverseReturn)

INPUT PEXMatrix		matrix;
OUTPUT PEXMatrix	inverseReturn;

{
    /*
     * This routine calculates a 4x4 inverse matrix.  It uses Gaussian
     * elimination on the system [matrix][inverse] = [identity].  The values
     * of the matrix then become the coefficients of the system of equations
     * that evolves from this equation.  The system is then solved for the
     * values of [inverse].  The algorithm solves for each column of [inverse]
     * in turn.  Partial pivoting is also done to reduce the numerical error
     * involved in the computations.  If singularity is detected, the routine
     * ends and returns an error.
     *
     * (See Numerical Analysis, L.W. Johnson and R.D.Riess, 1982).
     */

    int		ipivot, h, i, j, k;
    float	aug[4][5];
    float	pivot, temp, q;

    for (h = 0; h < 4; h++)   /* solve column by column */
    {
        /*
	 * Set up the augmented matrix for [matrix][inverse] = [identity]
	 */

        for (i = 0; i < 4; i++)
        {
            aug[i][0] = matrix[i][0];
            aug[i][1] = matrix[i][1];
            aug[i][2] = matrix[i][2];
            aug[i][3] = matrix[i][3];
            aug[i][4] = (h == i) ? 1.0 : 0.0;
        }

        /*
	 * Search for the largest entry in column i, rows i through 3.
         * ipivot is the row index of the largest entry.
	 */

        for (i = 0; i < 3; i++)
        {
            pivot = 0.0;

            for (j = i; j < 4; j++)
            {
                temp = ABS (aug[j][i]);
                if (pivot < temp)
                {
                    pivot = temp;
                    ipivot = j;
                }
            }
            if (NEAR_ZERO (pivot))         	/* singularity check */
                return (PEXBadMatrix);

            /* interchange rows i and ipivot */

            if (ipivot != i)
            {
                for (k = i; k < 5; k++)
                {
                    temp = aug[i][k];
                    aug[i][k] = aug[ipivot][k];
                    aug[ipivot][k] = temp;
                }
            }

            /* put augmented matrix in echelon form */

            for (k = i + 1; k < 4; k++)
            {
                q = -aug[k][i] / aug[i][i];
                aug[k][i] = 0.0;
                for (j = i + 1; j < 5; j++)
                {
                    aug[k][j] = q * aug[i][j] + aug[k][j];
                }
            }
        }

        if (NEAR_ZERO (aug[3][3]))   		/* singularity check */
            return (PEXBadMatrix);

        /* backsolve to obtain values for inverse matrix */

        inverseReturn[3][h] = aug[3][4] / aug[3][3];

        for (k = 1; k < 4; k++)
        {
            q = 0.0;
            for (j = 1; j <= k; j++)
            {
                q = q + aug[3 - k][4 - j] * inverseReturn[4 - j][h];
            }
            inverseReturn[3 - k][h] = (aug[3 - k][4] - q) / aug[3 - k][3 - k];
        }
    }

    return (0);
}


int
PEXInvertMatrix2D (matrix, inverseReturn)

PEXMatrix3x3	matrix;
PEXMatrix3x3	inverseReturn;

{
    /*
     * This routine calculates a 4x4 inverse matrix.  It uses Gaussian
     * elimination on the system [matrix][inverse] = [identity].  The values
     * of the matrix then become the coefficients of the system of equations
     * that evolves from this equation.  The system is then solved for the
     * values of [inverse].  The algorithm solves for each column of [inverse]
     * in turn.  Partial pivoting is also done to reduce the numerical error
     * involved in the computations.  If singularity is detected, the routine
     * ends and returns an error.
     *
     * (See Numerical Analysis, L.W. Johnson and R.D.Riess, 1982).
     */

    int		ipivot, h, i, j, k;
    float	aug[3][4];
    float	pivot, temp, q;


    for (h = 0; h < 3; h++)   /* solve column by column */
    {
        /*
	 * Set up the augmented matrix for [matrix][inverse] = [identity]
	 */

        for (i = 0; i < 3; i++)
        {
            aug[i][0] = matrix[i][0];
            aug[i][1] = matrix[i][1];
            aug[i][2] = matrix[i][2];
            aug[i][3] = (h == i) ? 1.0 : 0.0;
        }

        /*
	 * Search for the largest entry in column i, rows i through 3.
         * ipivot is the row index of the largest entry.
	 */

        for (i = 0; i < 2; i++)
        {
            pivot = 0.0;

            for (j = i; j < 3; j++)
            {
                temp = ABS (aug[j][i]);
                if (pivot < temp)
                {
                    pivot = temp;
                    ipivot = j;
                }
            }
            if (NEAR_ZERO (pivot)) 		/* singularity check */
                return (PEXBadMatrix);

            /* interchange rows i and ipivot */

            if (ipivot != i)
            {
                for (k = i; k < 4; k++)
                {
                    temp = aug[i][k];
                    aug[i][k] = aug[ipivot][k];
                    aug[ipivot][k] = temp;
                }
            }

            /* put augmented matrix in echelon form */

            for (k = i + 1; k < 3; k++)
            {
                q = -aug[k][i] / aug[i][i];
                aug[k][i] = 0.;
                for (j = i + 1; j < 4; j++)
                {
                    aug[k][j] = q * aug[i][j] + aug[k][j];
                }
            }
        }

        if (NEAR_ZERO (aug[2][2]))   		/* singularity check */
            return (PEXBadMatrix);

        /* backsolve to obtain values for inverse matrix */

        inverseReturn[2][h] = aug[2][3] / aug[2][2];

        for (k = 1; k < 3; k++)
        {
            q = 0.0;
            for (j = 1; j <= k; j++)
            {
                q = q + aug[2 - k][3 - j] * inverseReturn[3 - j][h];
            }
            inverseReturn[2 - k][h] = (aug[2 - k][3] - q) / aug[2 - k][2 - k];
        }
    }

    return (0);
}


void
PEXIdentityMatrix (matrix_return)

OUTPUT PEXMatrix	matrix_return;

{
    matrix_return[0][0] = 1.0;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;
    matrix_return[0][3] = 0.0;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0;
    matrix_return[1][2] = 0.0;
    matrix_return[1][3] = 0.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
    matrix_return[2][3] = 0.0;

    matrix_return[3][0] = 0.0;
    matrix_return[3][1] = 0.0;
    matrix_return[3][2] = 0.0;
    matrix_return[3][3] = 1.0;
}


void
PEXIdentityMatrix2D (matrix_return)

OUTPUT PEXMatrix3x3	matrix_return;

{
    matrix_return[0][0] = 1.0;
    matrix_return[0][1] = 0.0;
    matrix_return[0][2] = 0.0;

    matrix_return[1][0] = 0.0;
    matrix_return[1][1] = 1.0;
    matrix_return[1][2] = 0.0;

    matrix_return[2][0] = 0.0;
    matrix_return[2][1] = 0.0;
    matrix_return[2][2] = 1.0;
}


int
PEXGeoNormFillArea (facet_attributes, vertex_attributes,
    color_type, facet_data, count, vertices)    

INPUT unsigned int	facet_attributes;
INPUT unsigned int	vertex_attributes;
INPUT int		color_type;
INOUT PEXFacetData	*facet_data;
INPUT unsigned int	count;
INPUT PEXArrayOfVertex	vertices;

{
    PEXCoord	*v1, *v2, *v3;
    int		found_v2, found_v3;
    float	dx, dy, dz, length;
    int		vert_size;
    PEXVector	*normal;
    char	*ptr;


    /*
     * Check to see if we should compute the normal.
     */

    if (!(facet_attributes & PEXGANormal))
	return (0);


    /*
     * Make sure there are enough vertices to compute the normal.
     */

    if (count < 3)
	return (PEXBadPrimitive);


    /*
     * Get a pointer to the normal data structure.
     */

    if (facet_attributes & PEXGAColor)
    {
	normal = (PEXVector *) ((char *) facet_data +
	    GetClientColorSize (color_type));
    }
    else
	normal = (PEXVector *) facet_data;


    /*
     * Now find the first 3 non-colinear points and take their
     * cross product to get the normal.
     */

    vert_size = GetClientVertexSize (color_type, vertex_attributes);

    ptr = (char *) vertices.no_data;
    v1 = (PEXCoord *) ptr;
    count--;

    found_v2 = 0;

    while (!found_v2 && count > 0)
    {
	/* find v2, such that v2 is not coincident with v1 */

	ptr += vert_size;
	v2 = (PEXCoord *) ptr;
	count--;

	dx = v2->x - v1->x;
	dy = v2->y - v1->y;
	dz = v2->z - v1->z;

	if (!NEAR_ZERO (dx) || !NEAR_ZERO (dy) || !NEAR_ZERO (dz))
	    found_v2 = 1;
    }

    found_v3 = 0;

    while (!found_v3 && count > 0)
    {
	/* find v3, such that v3 is non-colinear with v1 and v2 */

	ptr += vert_size;
	v3 = (PEXCoord *) ptr;
	count--;

	CROSS_PRODUCT (v1, v2, v1, v3, normal);
	NORMALIZE_VECTOR (normal, length);

	if (!NEAR_ZERO (length))
	    found_v3 = 1;
    }

    if (found_v3)
	return (0);
    else
	return (PEXBadPrimitive);
}


int
PEXGeoNormFillAreaSet (facet_attributes, vertex_attributes,
    color_type, count, facet_data, vertex_lists)

INPUT unsigned int	facet_attributes;
INPUT unsigned int	vertex_attributes;
INPUT int		color_type;
INPUT unsigned int	count;
INOUT PEXFacetData	*facet_data;
INPUT PEXListOfVertex	*vertex_lists;

{
    PEXCoord	*v1, *v2, *v3;
    int		vert_size, vcount;
    int		found_v2, done, i;
    float	dx, dy, dz, length;
    PEXVector	*normal;
    char	*ptr;


    /*
     * Check to see if we should compute the normal.
     */

    if (!(facet_attributes & PEXGANormal))
	return (0);


    /*
     * Get a pointer to the normal data structure.
     */

    if (facet_attributes & PEXGAColor)
    {
	normal = (PEXVector *) ((char *) facet_data +
	    GetClientColorSize (color_type));
    }
    else
	normal = (PEXVector *) facet_data;


    /*
     * Now find the first 3 non-colinear points in one of the fill areas
     * of the set, and take their cross product to get the normal.
     */

    vert_size = GetClientVertexSize (color_type, vertex_attributes);

    for (i = done = 0; i < count && !done; i++)
    {
	if ((vcount = vertex_lists[i].count) > 2)
	{
	    ptr = (char *) vertex_lists[i].vertices.no_data;

	    v1 = (PEXCoord *) ptr;
	    vcount--;

	    found_v2 = 0;

	    while (!found_v2 && vcount > 0)
	    {
		/* find v2, such that v2 is not coincident with v1 */

		ptr += vert_size;
		v2 = (PEXCoord *) ptr;
		vcount--;

		dx = v2->x - v1->x;
		dy = v2->y - v1->y;
		dz = v2->z - v1->z;

		if (!NEAR_ZERO (dx) || !NEAR_ZERO (dy) || !NEAR_ZERO (dz))
		    found_v2 = 1;
	    }

	    while (!done && vcount > 0)
	    {
		/* find v3, such that v3 is non-colinear with v1 and v2 */

		ptr += vert_size;
		v3 = (PEXCoord *) ptr;
		vcount--;

		CROSS_PRODUCT (v1, v2, v1, v3, normal);
		NORMALIZE_VECTOR (normal, length);

		if (!NEAR_ZERO (length))
		    done = 1;
	    }
	}
    }

    if (done)
	return (0);
    else
	return (PEXBadPrimitive);
}


int
PEXGeoNormTriangleStrip (facet_attributes, vertex_attributes,
    color_type, facet_data, count, vertices)

INPUT unsigned int		facet_attributes;
INPUT unsigned int		vertex_attributes;
INPUT int			color_type;
INOUT PEXArrayOfFacetData	facet_data;
INPUT unsigned int		count;
INPUT PEXArrayOfVertex		vertices;

{
    PEXCoord	*v1, *v2, *v3;
    int		vert_size;
    int		facet_size, i;
    PEXVector	*normal;
    float	length;
    char	*ptr;
    int		status = 0;


    /*
     * Check to see if we should compute the normals.
     */

    if (!(facet_attributes & PEXGANormal))
	return (0);


    /*
     * Make sure there are enough vertices to compute the normals.
     */

    if (count < 3)
	return (PEXBadPrimitive);


    /*
     * Get a pointer to the first normal.
     */

    if (facet_attributes & PEXGAColor)
    {
	normal = (PEXVector *) ((char *) facet_data.index +
	    GetClientColorSize (color_type));
    }
    else
	normal = (PEXVector *) facet_data.normal;


    /*
     * Now compute all of the normals in the strip.
     */

    vert_size = GetClientVertexSize (color_type, vertex_attributes);
    facet_size = GetClientFacetSize (color_type, facet_attributes);

    ptr = (char *) vertices.no_data;

    for (i = 0; i < count - 2; i++)
    {
	v1 = (PEXCoord *) ptr;
	ptr += vert_size;
	v2 = (PEXCoord *) ptr;
	ptr += vert_size;
	v3 = (PEXCoord *) ptr;
	ptr -= vert_size;

	if (i % 2)
	{
	    /* cross product of v1v3 x v1v2 */

	    CROSS_PRODUCT (v1, v3, v1, v2, normal);
	}
	else
	{
	    /* cross product of v1v2 x v1v3 */

	    CROSS_PRODUCT (v1, v2, v1, v3, normal);
	}

	NORMALIZE_VECTOR (normal, length);

	if (NEAR_ZERO (length))
	{
	    normal->x = normal->y = normal->z = 0.0;
	    status = PEXBadPrimitive;
	}

	normal = (PEXVector *) ((char *) normal + facet_size);
    }

    return (status);
}


int
PEXGeoNormQuadrilateralMesh (facet_attributes, vertex_attributes,
    color_type, facet_data, col_count, row_count, vertices)

INPUT unsigned int		facet_attributes;
INPUT unsigned int		vertex_attributes;
INPUT int			color_type;
INOUT PEXArrayOfFacetData	facet_data;
INPUT unsigned int		col_count;
INPUT unsigned int		row_count;
INPUT PEXArrayOfVertex		vertices;

{
    PEXCoord	*v1, *v2, *v3, *v4;
    int		vert_size, facet_size;
    int		row, col;
    PEXVector	*normal;
    float	length;
    int		status = 0;


    /*
     * Check to see if we should compute the normals.
     */

    if (!(facet_attributes & PEXGANormal))
	return (0);


    /*
     * Make sure there are enough vertices to compute the normals.
     */

    if (row_count < 2 || col_count < 2)
	return (PEXBadPrimitive);


    /*
     * Get a pointer to the first normal.
     */

    if (facet_attributes & PEXGAColor)
    {
	normal = (PEXVector *) ((char *) facet_data.index +
	    GetClientColorSize (color_type));
    }
    else
	normal = (PEXVector *) facet_data.normal;


    /*
     * Now compute all of the normals in the quad mesh.
     */

    vert_size = GetClientVertexSize (color_type, vertex_attributes);
    facet_size = GetClientFacetSize (color_type, facet_attributes);

    for (row = 0; row < row_count - 1; row++)
	for (col = 0; col < col_count - 1; col++)
	{
	    /*
	     * v1 = vert[row    , col    ]
	     * v2 = vert[row + 1, col + 1]
	     * v3 = vert[row + 1, col    ] 
	     * v4 = vert[row    , col + 1]
	     *
	     * Take cross product of v1v2 x v3v4, then normalize.
	     */

	    v1 = (PEXCoord *) ((char *) vertices.no_data +
		(row * col_count + col) * vert_size);
	    
	    v4 = (PEXCoord *) ((char *) v1 + vert_size);

	    v3 = (PEXCoord *) ((char *) v1 + col_count * vert_size);

	    v2 = (PEXCoord *) ((char *) v3 + vert_size);
	    
	    CROSS_PRODUCT (v1, v2, v3, v4, normal);
	    NORMALIZE_VECTOR (normal, length);

	    if (NEAR_ZERO (length))
	    {
		normal->x = normal->y = normal->z = 0.0;
		status = PEXBadPrimitive;
	    }

	    normal = (PEXVector *) ((char *) normal + facet_size);
	}

    return (status);
}


int
PEXGeoNormSetOfFillAreaSets (facet_attributes, vertex_attributes,
    color_type, set_count, facet_data, vertex_count, vertices,
    index_count, connectivity)

INPUT unsigned int		facet_attributes;
INPUT unsigned int		vertex_attributes;
INPUT int			color_type;
INPUT unsigned int		set_count;
INOUT PEXArrayOfFacetData	facet_data;
INPUT unsigned int		vertex_count;
INPUT PEXArrayOfVertex		vertices;
INPUT unsigned int		index_count;
INPUT PEXConnectivityData	*connectivity;
{
    PEXConnectivityData *pConnectivity;
    PEXListOfUShort 	*pList;
    PEXCoord		*v1, *v2, *v3;
    int			vert_size, facet_size;
    float		dx, dy, dz, length;
    int			index, done;
    int			found_v2, i, j;
    PEXVector		*normal;
    int			status = 0;


    /*
     * Check to see if we should compute the normals.
     */

    if (!(facet_attributes & PEXGANormal))
	return (0);


    /*
     * Make sure there are enough vertices/indices to compute the normals.
     */

    if (index_count < 3 || vertex_count < 3)
	return (PEXBadPrimitive);


    /*
     * Get a pointer to the first normal.
     */

    if (facet_attributes & PEXGAColor)
    {
	normal = (PEXVector *) ((char *) facet_data.index +
	    GetClientColorSize (color_type));
    }
    else
	normal = (PEXVector *) facet_data.normal;


    /*
     * Now compute a normal for each fill area set.
     */

    vert_size = GetClientVertexSize (color_type, vertex_attributes);
    facet_size = GetClientFacetSize (color_type, facet_attributes);

    pConnectivity = connectivity;

    for (i = 0; i < set_count; i++)
    {
	pList = pConnectivity->lists;
	done = 0;

	for (j = 0; j < (int) pConnectivity->count && !done; j++, pList++)
	{
	    if (pList->count > 2)
	    {
		v1 = (PEXCoord *) ((char *) vertices.no_data +
		    vert_size * pList->shorts[0]);

		index = 1;
		found_v2 = 0;

		while (!found_v2 && index < (int) pList->count)
		{
		    /* find v2, such that v2 is not coincident with v1 */

		    v2 = (PEXCoord *) ((char *) vertices.no_data +
		        vert_size * pList->shorts[index]);

		    index++;

		    dx = v2->x - v1->x;
		    dy = v2->y - v1->y;
		    dz = v2->z - v1->z;

		    if (!NEAR_ZERO (dx) || !NEAR_ZERO (dy) || !NEAR_ZERO (dz))
			found_v2 = 1;
		}

		while (!done && index < (int) pList->count)
		{
		    /* find v3, such that v3 is non-colinear with v1 and v2 */

		    v3 = (PEXCoord *) ((char *) vertices.no_data +
		        vert_size * pList->shorts[index]);

		    index++;

		    CROSS_PRODUCT (v1, v2, v1, v3, normal);
		    NORMALIZE_VECTOR (normal, length);

		    if (!NEAR_ZERO (length))
			done = 1;
		}
	    }
	}

	if (!done)
	{
	    normal->x = normal->y = normal->z = 0.0;
	    status = PEXBadPrimitive;
	}

	normal = (PEXVector *) ((char *) normal + facet_size);
	pConnectivity++;
    }

    return (status);
}
