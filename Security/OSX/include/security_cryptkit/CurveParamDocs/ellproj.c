/**************************************************************
 *
 * ellproj.c
 *
   Fast algorithms for fundamental elliptic curve arithmetic,
   projective format.  Such algorithms apply in domains such as:
    -- factoring
    -- primality studies (e.g. rigorous primality proofs)
    -- elliptic curve cryptography (ECC) 
  
   PROJECTIVE FORMAT

   Functions are supplied herein for projective format
   of points.  Alternative formats differ in their
   range of applicability, efficiency, and so on.
   Primary advantages of the projective format herein are:
    -- No explicit inversions (until perhaps one such at the end of
       an elliptic multiply operation)
    -- Fairly low operation count (~11 muls for point doubling,
       ~16 muls for point addition)  

   The elliptic curve is over F_p, with p > 3 prime, and Weierstrass
   parameterization:

      y^2 = x^3 + a x + b

   The projective-format coordinates are actually stored in
   the form {X, Y, Z}, with true x,y 
   coordinates on the curve given by {x,y} = {X/Z^2, Y/Z^3}.  
   The function normalize_proj() performs the
   transformation from projective->true.  
   (The other direction is trivial, i.e. {x,y} -> {x,y,1} will do.)
   The basic point multiplication function is

      ell_mul_proj()

   which obtains the result k * P for given point P and integer
   multiplier k.  If true {x,y} are required for a multiple, one 
   passes a point P = {X, Y, 1} to ell_mul_proj(), then afterwards 
   calls normalize_proj(), 

   Projective format is an answer to the typical sluggishness of
   standard elliptic arithmetic, whose explicit inversion in the
   field is, depending of course on the machinery and programmer,
   costly.  Projective format is thus especially interesting for
   cryptography.  
 
   REFERENCES

   Crandall R and Pomerance C 1998, "Prime numbers: a computational
		perspective," Springer-Verlag, manuscript

   Solinas J 1998, IEEE P1363 Annex A (draft standard)

   LEGAL AND PATENT NOTICE

   This and related PSI library source code is intended solely for 
   educational and research applications, and should not be used
   for commercial purposes without explicit permission from PSI
   (not to mention proper clearance of legal restrictions beyond
   the purview of PSI).  
   The code herein will not perform cryptography per se,
   although the number-theoretical algorithms herein -- all of which 
   are in the public domain -- can be used in principle to effect 
   what is known as elliptic curve cryptography (ECC).  Note that 
   there are strict constraints on how cryptography may be used, 
   especially in regard to exportability.
   Therefore one should avoid any casual distribution of actual 
   cryptographic software.  Along similar lines, because of various 
   patents, proprietary to Apple Computer, Inc., and perhaps to other 
   organizations, one should not tacitly assume that an ECC scheme is 
   unconstrained.  For example,the commercial use of certain fields 
   F_p^k (i.e., fixation of certain primes p) is covered in Apple 
   patents.

 *	Updates:
 *		3 Apr 98    REC  Creation
 *
 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

/* include files */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32 

#include <process.h>

#endif

#include <string.h>
#include "giants.h"
#include "ellproj.h"
#include "tools.h"

/* global variables */

static giant t0 = NULL, t1 = NULL, t2 = NULL, t3 = NULL, t4 = NULL,
	         t5 = NULL, t6 = NULL, t7 = NULL;

/**************************************************************
 *
 *	Maintenance functions
 *
 **************************************************************/

point_proj
new_point_proj(int shorts)
{
	point_proj pt;

	if(t0 == NULL) init_ell_proj(shorts);
	pt = (point_proj) malloc(sizeof(point_struct_proj));
	pt->x = newgiant(shorts);
	pt->y = newgiant(shorts);
	pt->z = newgiant(shorts);
	return(pt);
}

void
free_point_proj(point_proj pt)
{
	free(pt->x); free(pt->y); free(pt->z);
	free(pt);
}

void
ptop_proj(point_proj pt1, point_proj pt2)
{
	gtog(pt1->x, pt2->x);
	gtog(pt1->y, pt2->y);
	gtog(pt1->z, pt2->z);
}

void
init_ell_proj(int shorts) 
/* Called by new_point_proj(), to set up giant registers. */
{	
	t0 = newgiant(shorts);
	t1 = newgiant(shorts);
	t2 = newgiant(shorts);
	t3 = newgiant(shorts);
	t4 = newgiant(shorts);
	t5 = newgiant(shorts);
	t6 = newgiant(shorts);
	t7 = newgiant(shorts);
}


/**************************************************************
 *
 *	Elliptic curve operations
 *
 **************************************************************/

/* Begin projective-format functions for 
 
   y^2 = x^3 + a x + b.

   These are useful in elliptic curve cryptography (ECC).
   A point is kept as a triple {X,Y,Z}, with the true (x,y)
   coordinates given by

	{x,y} = {X/Z^2, Y/Z^3}

   The function normalize_proj() performs the inverse conversion to get
   the true (x,y) pair.
 */

void
ell_double_proj(point_proj pt, giant a, giant p)
/* pt := 2 pt on the curve. */
{	
	giant x = pt->x, y = pt->y, z = pt->z;

	if(isZero(y) || isZero(z)) {
		itog(1,x); itog(1,y); itog(0,z);
		return;
	}	
	gtog(z,t1); squareg(t1); modg(p, t1);
	squareg(t1); modg(p, t1);
	mulg(a, t1); modg(p, t1); /* t1 := a z^4. */
	gtog(x, t2); squareg(t2); smulg(3, t2); modg(p, t2); /* t2 := 3x^2. */
	addg(t2, t1); modg(p, t1);  /* t1 := slope m. */
	mulg(y, z); addg(z,z); modg(p, z);  /* z := 2 y z. */
	gtog(y, t2); squareg(t2); modg(p, t2); /* t2 := y^2. */
	gtog(t2, t3); squareg(t3); modg(p, t3); /* t3 := y^4. */
	gshiftleft(3, t3);  /* t3 := 8 y^4. */
	mulg(x, t2); gshiftleft(2, t2); modg(p, t2); /* t2 := 4xy^2. */
	gtog(t1, x); squareg(x); modg(p, x);
	subg(t2, x); subg(t2, x); modg(p, x); /* x done. */
	gtog(t1, y); subg(x, t2); mulg(t2, y); subg(t3, y);
	modg(p, y);
} 
/*
elldouble[pt_] := Block[{x,y,z,m,y2,s},
	x = pt[[1]]; y = pt[[2]]; z = pt[[3]];
	If[(y==0) || (z==0), Return[{1,1,0}]];
	m = Mod[3 x^2 + a Mod[Mod[z^2,p]^2,p],p];
	z = Mod[2 y z, p];
	y2 = Mod[y^2,p];
	s = Mod[4 x y2,p]; 
	x = Mod[m^2 - 2s,p];
	y = Mod[m(s - x) - 8 y2^2,p];
	Return[{x,y,z}];
];
*/

void
ell_add_proj(point_proj pt0, point_proj pt1, giant a, giant p)
/* pt0 := pt0 + pt1 on the curve. */
{   
	giant x0 = pt0->x, y0 = pt0->y, z0 = pt0->z,
		  x1 = pt1->x, y1 = pt1->y, z1 = pt1->z;
 
	if(isZero(z0)) {
		gtog(x1,x0); gtog(y1,y0); gtog(z1,z0);
		return;
	}
	if(isZero(z1)) return;
	gtog(x0, t1); gtog(y0,t2); gtog(z0, t3);
	gtog(x1, t4); gtog(y1, t5);
	if(!isone(z1)) {
		gtog(z1, t6);
		gtog(t6, t7); squareg(t7); modg(p, t7);
		mulg(t7, t1); modg(p, t1);
		mulg(t6, t7); modg(p, t7);
		mulg(t7, t2); modg(p, t2);
	}
	gtog(t3, t7); squareg(t7); modg(p, t7);
	mulg(t7, t4); modg(p, t4);
	mulg(t3, t7); modg(p, t7);
	mulg(t7, t5); modg(p, t5);
	negg(t4); addg(t1, t4); modg(p, t4);
	negg(t5); addg(t2, t5); modg(p, t5);
	if(isZero(t4)) {
		if(isZero(t5)) {
			ell_double_proj(pt0, a, p);
	    } else {
			itog(1, x0); itog(1, y0); itog(0, z0);
		}
		return;
	}
	addg(t1, t1); subg(t4, t1); modg(p, t1);
	addg(t2, t2); subg(t5, t2); modg(p, t2);
	if(!isone(z1)) {
		mulg(t6, t3); modg(p, t3);
	}
	mulg(t4, t3); modg(p, t3);
	gtog(t4, t7); squareg(t7); modg(p, t7);
	mulg(t7, t4); modg(p, t4);
	mulg(t1, t7); modg(p, t7);
	gtog(t5, t1); squareg(t1); modg(p, t1);
	subg(t7, t1); modg(p, t1);
	subg(t1, t7); subg(t1, t7); modg(p, t7);
	mulg(t7, t5); modg(p, t5);
	mulg(t2, t4); modg(p, t4);
	gtog(t5, t2); subg(t4,t2); modg(p, t2);
	if(t2->n[0] & 1) { /* Test if t2 is odd. */
		addg(p, t2);
	}
	gshiftright(1, t2);
	gtog(t1, x0); gtog(t2, y0); gtog(t3, z0);
}

/*
elladd[pt0_, pt1_] := Block[
	{x0,y0,z0,x1,y1,z1,
	t1,t2,t3,t4,t5,t6,t7},
	x0 = pt0[[1]]; y0 = pt0[[2]]; z0 = pt0[[3]];
	x1 = pt1[[1]]; y1 = pt1[[2]]; z1 = pt1[[3]];
	If[z0 == 0, Return[pt1]];
	If[z1 == 0, Return[pt0]];

	t1 = x0;
	t2 = y0;
	t3 = z0;
	t4 = x1;
	t5 = y1;
	If[(z1 != 1),
		t6 = z1;
		t7 = Mod[t6^2, p];
		t1 = Mod[t1 t7, p];
		t7 = Mod[t6 t7, p];
		t2 = Mod[t2 t7, p];
	];
	t7 = Mod[t3^2, p];
	t4 = Mod[t4 t7, p];
	t7 = Mod[t3 t7, p];
	t5 = Mod[t5 t7, p];
	t4 = Mod[t1-t4, p];
	t5 = Mod[t2 - t5, p];
	If[t4 == 0, If[t5 == 0,
				    Return[elldouble[pt0]],
	   				Return[{1,1,0}]
	   			]
	];
	t1 = Mod[2t1 - t4,p];
	t2 = Mod[2t2 - t5, p];
	If[z1 != 1, t3 = Mod[t3 t6, p]];
	t3 = Mod[t3 t4, p];
	t7 = Mod[t4^2, p];
	t4 = Mod[t4 t7, p];
	t7 = Mod[t1 t7, p];
	t1 = Mod[t5^2, p];
	t1 = Mod[t1-t7, p];
	t7 = Mod[t7 - 2t1, p];
	t5 = Mod[t5 t7, p];
	t4 = Mod[t2 t4, p];
	t2 = Mod[t5-t4, p];
	If[EvenQ[t2], t2 = t2/2, t2 = (p+t2)/2];
	Return[{t1, t2, t3}];
];
*/

void
ell_neg_proj(point_proj pt, giant p)
/* pt := -pt on the curve. */
{
	negg(pt->y); modg(p, pt->y);
}

void
ell_sub_proj(point_proj pt0, point_proj pt1,	giant a, giant p)
/* pt0 := pt0 - pt1 on the curve. */
{
	ell_neg_proj(pt1, p);
	ell_add_proj(pt0, pt1,a,p);
	ell_neg_proj(pt1,p);
}

void
ell_mul_proj(point_proj pt0, point_proj pt1, giant k, giant a, giant p)
/* General elliptic multiplication;
   pt1 := k*pt0 on the curve, 
   with k an arbitrary integer. 
 */
{	
	giant x = pt0->x, y = pt0->y, z = pt0->z,
		  xx = pt1->x, yy = pt1->y, zz = pt1->z;
	int ksign, hlen, klen, b, hb, kb;
    
	if(isZero(k)) {
		itog(1, xx);
		itog(1, yy);
		itog(0, zz);
		return;
	}
    ksign = k->sign;
	if(ksign < 0) negg(k);
	gtog(x,xx); gtog(y,yy); gtog(z,zz);
	gtog(k, t0); addg(t0, t0); addg(k, t0); /* t0 := 3k. */
	hlen = bitlen(t0);
	klen = bitlen(k);
	for(b = hlen-2; b > 0; b--) {
		ell_double_proj(pt1,a,p);
		hb = bitval(t0, b);
		if(b < klen) kb = bitval(k, b); else kb = 0;
		if((hb != 0) && (kb == 0))
			ell_add_proj(pt1, pt0, a, p);
		else if((hb == 0) && (kb !=0))
			ell_sub_proj(pt1, pt0, a, p);
	}
	if(ksign < 0) {
		ell_neg_proj(pt1, p);
		k->sign = -k->sign;
	}
}

/*
elliptic[pt_, k_] := Block[{pt2, hh, kk, hb, kb, lenh, lenk},
	If[k==0, Return[{1,1,0}]];
	hh = Reverse[bitList[3k]];
	kk = Reverse[bitList[k]];
	pt2 = pt;
	lenh = Length[hh];
	lenk = Length[kk];
	Do[
		pt2 = elldouble[pt2];
		hb = hh[[b]];
		If[b <= lenk, kb = kk[[b]], kb = 0];
		If[{hb,kb} == {1,0},
			pt2 = elladd[pt2, pt],
			If[{hb, kb} == {0,1},
			pt2 = ellsub[pt2, pt]]
		]
	   ,{b, lenh-1, 2,-1}
	 ];
	Return[pt2];
];
*/

void
normalize_proj(point_proj pt, giant p)
/* Obtain actual x,y coords via normalization:
   {x,y,z} := {x/z^2, y/z^3, 1}.
 */

{	giant x = pt->x, y = pt->y, z = pt->z;

	if(isZero(z)) {
		itog(1,x); itog(1,y);
		return;
	}
	binvaux(p, z); gtog(z, t1);
	squareg(z); modg(p, z);
	mulg(z, x); modg(p, x);
	mulg(t1, z); mulg(z, y); modg(p, y);
	itog(1, z);
}

/*
normalize[pt_] := Block[{z,z2,z3},
		If[pt[[3]] == 0, Return[pt]];
		z = ellinv[pt[[3]]];
		z2 = Mod[z^2,p];
		z3 = Mod[z z2,p];
		Return[{Mod[pt[[1]] z2, p], Mod[pt[[2]] z3, p], 1}];
		];
*/
				

void
find_point_proj(point_proj pt, giant seed, giant a, giant b, giant p)
/* Starting with seed, finds a random (projective) point {x,y,1} on curve.
 */
{	giant x = pt->x, y = pt->y, z = pt->z;

    modg(p, seed);
    while(1) {
		gtog(seed, x);
		squareg(x); modg(p, x);
		addg(a, x);
		mulg(seed,x); addg(b, x);
		modg(p, x); /* x := seed^3 + a seed + b. */
		if(sqrtmod(p, x)) break;  /* Test if cubic form has root. */
		iaddg(1, seed);
	}
	gtog(x, y);
    gtog(seed,x);
	itog(1, z);
}
