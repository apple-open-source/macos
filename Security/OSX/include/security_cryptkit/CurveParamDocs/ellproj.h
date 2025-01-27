/**************************************************************
 *
 *	ellproj.h
 *
 *	Header file for ellproj.c
 *
 *	Updates:
 *		3 Apr 98    REC - Creation
 *
 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

/* definitions */

typedef struct  /* This is how to define a projective point. */
{
	 giant x;
	 giant y; 
	 giant z;
} point_struct_proj;

typedef point_struct_proj *point_proj;

point_proj  /* Allocates a new projective point. */
new_point_proj(int shorts);

void  /* Frees point. */
free_point_proj(point_proj pt);

void  /* Copies point to point. */
ptop_proj(point_proj pt1, point_proj pt2);

void  /* Initialization. */
init_ell_proj(int shorts);

void /* Point doubling. */
ell_double_proj(point_proj pt, giant a, giant p);

void /* Point addition. */
ell_add_proj(point_proj pt0, point_proj pt1, giant a, giant p);

void /* Point negation. */
ell_neg_proj(point_proj pt, giant p);

void /* Point subtraction. */
ell_sub_proj(point_proj pt0, point_proj pt1, giant a, giant p);

void /* General elliptic mul. */
ell_mul_proj(point_proj pt0, point_proj pt1, giant k, giant a, giant p);

void /* Generate normalized point (X, Y, 1) from given (x,y,z). */
normalize_proj(point_proj pt, giant p);

void /* Find a point (x, y, 1) on the curve. */
find_point_proj(point_proj pt, giant seed, giant a, giant b, giant p);

