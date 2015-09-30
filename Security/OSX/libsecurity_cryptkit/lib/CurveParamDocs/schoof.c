/* schoof.c
   
   Elliptic curve order calculator, for

   y^2 = x^3 + a x + b (mod p)
   
   Compile with:

   % cc -O schoof.c giants.c tools.c ellproj.c -lm -o schoof

   and run, entering p and the a,b parameters.
   Eventually the curve order o is reported, together with
   the twist order o'.  The sum of these two orders is always
   2p + 2.  A final check is performed to verify that a
   random point (x,y,z) enjoys the annihilation
   o * (x,y,z) = point-at-infinity.  This is not absolutely
   definitive, rather it is a necessary condition on the oder o
   (i.e. it is a sanity check of sorts).

 * Change history:

     18 Nov 99   REC  fixed M. Scott bug (MAX_DIGS bumped by 2)
      5 Jul 99   REC  Installed improved (base-4) power ladder
      5 Jul 99   REC  Made use of binary segmentation square (per se)
      5 Jul 99   REC  Improved memory usage
      2 May 99   REC  Added initial primality check
     30 Apr 99   REC  Added actual order-annihilation test
     29 Apr 99   REC  Improvements due to A. Powell
      2 Feb 99   REC  Added explicit CRT result
     12 Jan 99   REC  Removed (hopefully) last of memory crashes
     20 Jan 98   REC  Creation

 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

#include <stdio.h>
#include<assert.h>
#include <math.h>
#include <stdlib.h>
#include "giants.h"
#include "tools.h"
#include "ellproj.h"

#define P_BREAK 32

#ifdef _WIN32
#include <string.h>
#define bzero(D, n) memset(D, 0x00, n)
#define bcopy(S, D, n) memcpy(D, S, n)
#endif
  
#define Q_MAX 256       /* Bits in largest primes handled. */
#define L_CEIL 100       /* Bound on Schoof L values (not all needed in general). */


typedef struct
         {
         int deg;    
         giant *coe;
         } polystruct;   
typedef polystruct *poly;

extern int *pr;

static int Q, L_MAX;
static int MAX_DIGS, MAX_COEFFS;

static giant *mcand, coe, tmp, err, aux, aux2, globx, globy, t1, t2, 
	t3, t4, t5;
static poly qscratch, rscratch, sscratch, tscratch, pbuff,
     pbuff2, precip, cubic, powx, powy, kxn, kyn, kxd, kyd,
     txn, txd, tyn, tyd, txn1, txd1, tyn1, tyd1,
     nx, dx, ny, dy, mn, md, tmp1, tmp2, tmp3, tmp4;
static poly s[L_CEIL+2], smonic; 
static giant p, a, b;
static int L;

void quickmodg(giant g, giant x) 
{       int sgn = x->sign;

        if(sgn == 0) return;
        if(sgn > 0) {
                if(gcompg(x, g) >= 0) subg(g, x);
                return;
        }
        addg(g,x);
        return;
}

int
log_2(int n) {
        int c = 1, d = -1;
        while(c <= n) {
                c <<= 1;
                ++d;
        }
        return(d);   
}

void
justifyp(poly x) {
        int j;
        for(j = x->deg; j >= 0; j--) {
                if(!is0(x->coe[j])) break;
        }
        x->deg = (j>0)? j : 0;
}

void
polyrem(poly x) {
        int j;
   for(j=0; j <= x->deg; j++) {
      modg(p, x->coe[j]);
   }
   justifyp(x);
}


giant *
newa(int n) {
        giant *p = (giant *)malloc(n*sizeof(giant));
        int j;
        for(j=0; j<n; j++) {
                p[j] = newgiant(MAX_DIGS);
        }
        return(p);
}

poly
newpoly(int coeffs) {
        poly pol;
        pol = (poly) malloc(sizeof(polystruct));
        pol->coe = (giant *)newa(coeffs);
        return(pol);
}

void
init_all() {
   int j;
   
   j = (2*Q + log_2(MAX_COEFFS) + 32 + 15)/16;
   j = j * MAX_COEFFS;
   globx = newgiant(j);
   globy = newgiant(j);
   s[0] = newpoly(MAX_COEFFS);

   for(j=1; j<=L_MAX+1; j++) {
                s[j] = newpoly(j*(j+1));
   }
   smonic = newpoly(MAX_COEFFS);
   powx = newpoly(MAX_COEFFS);
   powy = newpoly(MAX_COEFFS);
   kxn = newpoly(MAX_COEFFS);
   kxd = newpoly(MAX_COEFFS);
   kyn = newpoly(MAX_COEFFS);
   kyd = newpoly(MAX_COEFFS);
   txn = newpoly(MAX_COEFFS);
   txd = newpoly(MAX_COEFFS);
   tyn = newpoly(MAX_COEFFS);
   tyd = newpoly(MAX_COEFFS);
   txn1 = newpoly(MAX_COEFFS);
   txd1 = newpoly(MAX_COEFFS);
   tyn1 = newpoly(MAX_COEFFS);
   tyd1 = newpoly(MAX_COEFFS);
   nx = newpoly(MAX_COEFFS);
   ny = newpoly(MAX_COEFFS);
   dx = newpoly(MAX_COEFFS);
   dy = newpoly(MAX_COEFFS);
   mn = newpoly(MAX_COEFFS);
   md = newpoly(MAX_COEFFS);
   tmp1 = newpoly(MAX_COEFFS);
   tmp2 = newpoly(MAX_COEFFS);
   tmp3 = newpoly(MAX_COEFFS);
   tmp4 = newpoly(MAX_COEFFS);
   mcand = (giant *)newa(MAX_COEFFS);
/* The next three need extra space for remaindering routines. */
        qscratch = newpoly(2*MAX_COEFFS);
        rscratch = newpoly(2*MAX_COEFFS);
        pbuff = newpoly(2*MAX_COEFFS);
        pbuff2 = newpoly(MAX_COEFFS);
        sscratch = newpoly(MAX_COEFFS);
        tscratch = newpoly(MAX_COEFFS);
        tmp = newgiant(MAX_DIGS);
        err = newgiant(MAX_DIGS);
        coe = newgiant(MAX_DIGS);
        aux = newgiant(MAX_DIGS);
        aux2 = newgiant(MAX_DIGS);
        t3 = newgiant(MAX_DIGS);
        t4 = newgiant(MAX_DIGS);
        t5 = newgiant(MAX_DIGS);
   		cubic = newpoly(4);
        precip = newpoly(MAX_COEFFS);
}

void 
atoa(giant *a, giant *b, int n) {
        int j;
        for(j=0; j<n; j++) gtog(a[j], b[j]);
}

void
ptop(poly x, poly y)
/* y := x. */
{
        y->deg = x->deg;
        atoa(x->coe, y->coe, y->deg+1);
}

void
negp(poly y)
/* y := -y. */
{       int j;
        for(j=0; j <= y->deg; j++) {
                negg(y->coe[j]);
                quickmodg(p, y->coe[j]);
        }
}

int
iszer(giant a) {

  if(a->sign == 0) return(1);
  return(0);

}

int
iszerop(poly x) {
   if(x->deg == 0 && iszer(x->coe[0])) return 1;
   return 0;
}

int
is0(giant a) {
        return(iszer(a));
}

int
is1(giant a) {
        return(isone(a));
}


void
addp(poly x, poly y)
/* y += x. */
{
        int d = x->deg, j;

        if(y->deg > d) d = y->deg;
        for(j = 0; j <= d; j++) {
                if((j <= x->deg) && (j <= y->deg)) {
                        addg(x->coe[j], y->coe[j]);
                        quickmodg(p, y->coe[j]);
                        continue;
                }
                if((j <= x->deg) && (j > y->deg)) {
                        gtog(x->coe[j], y->coe[j]);
                        quickmodg(p, y->coe[j]);
                        continue;
                }
        }
        y->deg = d;
        justifyp(y);
}

void
subp(poly x, poly y)
/* y -= x. */
{
        int d = x->deg, j;

        if(y->deg > d) d = y->deg;
        for(j = 0; j <= d; j++) {
                if((j <= x->deg) && (j <= y->deg)) {
                        subg(x->coe[j], y->coe[j]);
                        quickmodg(p, y->coe[j]);
                        continue;
                }
                if((j <= x->deg) && (j > y->deg)) {
                        gtog(x->coe[j], y->coe[j]);
                        negg(y->coe[j]);
                        quickmodg(p, y->coe[j]);
                        continue;
                }
        }
        y->deg = d;
        justifyp(y);
}


void
grammarmulp(poly a, poly b) 
/* b *= a. */
{
        int dega = a->deg, degb = b->deg, deg = dega + degb;
        register int d, kk, first, diffa;

        for(d=deg; d>=0; d--) {
                diffa = d-dega;
                itog(0, coe);
                for(kk=0; kk<=d; kk++) {
                        if((kk>degb)||(kk<diffa)) continue;
                        gtog(b->coe[kk], tmp);
                        mulg(a->coe[d-kk], tmp);
                        modg(p, tmp);
                        addg(tmp, coe);
                        quickmodg(p, coe);
                }
                gtog(coe, mcand[d]);
        }
        atoa(mcand, b->coe, deg+1);
        b->deg = deg;
        justifyp(b);
}

void
atop(giant *a, poly x, int deg)
/* Copy array to poly, with monic option. */ 
{
        int adeg = abs(deg);
        x->deg = adeg;
        atoa(a, x->coe, adeg);
        if(deg < 0) {
           itog(1, x->coe[adeg]);
        } else {
           gtog(a[adeg], x->coe[adeg]);
        }
}

void
just(giant g) {
   while((g->n[g->sign-1] == 0) && (g->sign > 0)) --g->sign;
}

void
unstuff_partial(giant g, poly y, int words){
        int j;
        for(j=0; j < y->deg; j++) {
                bcopy((g->n) + j*words, y->coe[j]->n, words*sizeof(short));
      y->coe[j]->sign = words;
      just(y->coe[j]);
   }
}

void
stuff(poly x, giant g, int words) {
        int deg = x->deg, j, coedigs;

   g->sign = words*(1 + deg);
   for(j=0; j <= deg; j++) {
                coedigs = (x->coe[j])->sign;
                bcopy(x->coe[j]->n, (g->n) + j*words, coedigs*sizeof(short));
      bzero((g->n) + (j*words+coedigs), 
                                sizeof(short)*(words-coedigs));
        }
   just(g);
}

int maxwords = 0;
void

binarysegmul(poly x, poly y) {
        int bits = bitlen(p), xwords, ywords, words;
   
        xwords = (2*bits + log_2(x->deg+1) + 32 + 15)/16;
   ywords = (2*bits + log_2(y->deg+1) + 32 + 15)/16;
   if(xwords > ywords) words = xwords; else words = ywords;
   stuff(x, globx, words);
   stuff(y, globy, words);
   mulg(globx, globy);
   gtog(y->coe[y->deg], globx);  /* Save high coeff. */
   y->deg += x->deg;
   gtog(globx, y->coe[y->deg]);  /* Move high coeff. */
   unstuff_partial(globy, y, words);
   mulg(x->coe[x->deg], y->coe[y->deg]); /* resolve high coeff. */
   justifyp(y);
}

binarysegsquare(poly y) {
        int bits = bitlen(p), words;
      words = (2*bits + log_2(y->deg+1) + 32 + 15)/16;
   stuff(y, globy, words);
   squareg(globy);
   gtog(y->coe[y->deg], globx);  /* Save high coeff. */
   y->deg += y->deg;
   gtog(globx, y->coe[y->deg]);  /* Move high coeff. */
   unstuff_partial(globy, y, words);
   mulg(y->coe[y->deg], y->coe[y->deg]); /* resolve high coeff. */
   justifyp(y);
}

void
assess(poly x, poly y){
        int max = 0, j;
   for(j=0; j <= x->deg; j++) {
                if(bitlen(x->coe[j]) > max) max = bitlen(x->coe[j]);
   }
   max = 0;
   for(j=0; j <= y->deg; j++) {
                if(bitlen(y->coe[j]) > max) max = bitlen(y->coe[j]);
   }
}

int
pcompp(poly x, poly y) {
    int j;
    if(x->deg != y->deg) return 1;
    for(j=0; j <= x->deg; j++) {
                        if(gcompg(x->coe[j], y->coe[j])) return 1;
    }
    return 0;
}

/*
int max_deg = 0;
*/

void
mulp(poly x, poly y)
/*  y *= x. */
{
        int n, degx = x->deg, degy = y->deg;

/*
if(degx > max_deg) {
	max_deg = degx; printf("xdeg: %d\n", degx);
}

if(degy > max_deg) {
	max_deg = degy; printf("ydeg: %d\n", degy);
}
*/
        if((degx < P_BREAK) || (degy < P_BREAK)) {
                grammarmulp(x,y);
                justifyp(y);
                return;
        }
   if(x==y) binarysegsquare(y);
   	else binarysegmul(x, y);
}

void
revp(poly x) 
/* Reverse the coefficients of x. */
{       int j, deg = x->deg;

        for(j=0; j <= deg/2; j++) {
                gtog(x->coe[deg-j], tmp);
                gtog(x->coe[j], x->coe[deg-j]);
                gtog(tmp, x->coe[j]);
        }
   justifyp(x);
}

void
recipp(poly f, int deg) 
/* f := 1/f, via newton method.  */
{
        int lim = deg + 1, prec = 1;

        sscratch->deg = 0; itog(1, sscratch->coe[0]);
        itog(1, aux);
        while(prec < lim) {
                prec <<= 1;
                if(prec > lim) prec = lim;
                f->deg = prec-1;
                ptop(sscratch, tscratch);
                mulp(f, tscratch); 
				tscratch->deg = prec-1;
				polyrem(tscratch);
                subg(aux, tscratch->coe[0]);
                quickmodg(p, tscratch->coe[0]);
                mulp(sscratch, tscratch); 
				tscratch->deg = prec-1;
				polyrem(tscratch);
                subp(tscratch, sscratch); 
                sscratch->deg = prec-1;
				polyrem(sscratch);
        }
        justifyp(sscratch);
        ptop(sscratch, f);
}

int
left_justifyp(poly x, int start)
/* Left-justify the polynomial, checking from position "start." */
{
        int j, shift = 0;

        for(j = start; j <= x->deg; j++) {
                if(!is0(x->coe[j])) {
                     shift = start;
                     break;
                }
        }
        x->deg -= shift;
        for(j=0; j<= x->deg; j++) {
                gtog(x->coe[j+shift], x->coe[j]);
        }
        return(shift);
}

void
remp(poly x, poly y, int mode)
/* y := x (mod y). 
  mode = 0 is normal operation,
       = 1 jams a fixed reciprocal,
       = 2 uses the fixed reciprocal.
 */
{
        int degx = x->deg, degy = y->deg, d, shift;

        if(degy == 0) {
                y->deg = 0;
                itog(0, y->coe[0]);
                return;
        }
        d = degx - degy;
        if(d < 0) {
                ptop(x, y);
                return;
        }
        revp(x); revp(y);
        ptop(y, rscratch);
        switch(mode) {
           case 0: recipp(rscratch, d);
                   break;
      case 1: recipp(rscratch, degy); /* degy -1. */
                   ptop(rscratch, precip);
                   rscratch->deg = d; justifyp(rscratch);
                   break;
           case 2: ptop(precip, rscratch);
                   rscratch->deg = d; justifyp(rscratch);
                   break;
        } 
/* Next, a limited-precision multiply. */
        if(d < degx) { x->deg = d; justifyp(x);}
        mulp(x, rscratch); 
        rscratch->deg = d; 
		polyrem(rscratch);
		justifyp(rscratch);
        x->deg = degx; justifyp(x);
        mulp(rscratch, y);
        subp(x, y);
        negp(y); polyrem(y);
        shift = left_justifyp(y, d+1);
   for(d = y->deg+1; d <= degx-shift; d++) itog(0, y->coe[d]);
        y->deg = degx - shift;
        revp(y);
        revp(x);
}

fullmod(poly x) {
   polyrem(x);
   ptop(smonic, s[0]);
   remp(x, s[0], 2);
   ptop(s[0], x);
   polyrem(x);
}

scalarmul(giant s, poly x) {
        int j;
   for(j=0; j <= x->deg; j++) {
                mulg(s, x->coe[j]);
      modg(p, x->coe[j]);
   }
}


schain(int el) {
   int j;

        s[0]->deg = 0;
   itog(0, s[0]->coe[0]);

        s[1]->deg = 0;
   itog(1, s[1]->coe[0]);
        s[2]->deg = 0;
   itog(2, s[2]->coe[0]);

   s[3]->deg = 4;
   gtog(a, aux); mulg(a, aux); negg(aux);   
   gtog(aux, s[3]->coe[0]);
   gtog(b, aux); smulg(12, aux);
   gtog(aux, s[3]->coe[1]);
   gtog(a, aux); smulg(6, aux);
   gtog(aux, s[3]->coe[2]);
   itog(0, s[3]->coe[3]);
   itog(3, s[3]->coe[4]);

   s[4]->deg = 6;
   gtog(a, aux); mulg(a, aux); mulg(a, aux);
   gtog(b, tmp); mulg(b, tmp); smulg(8, tmp); addg(tmp, aux);
   negg(aux);   
   gtog(aux, s[4]->coe[0]);
   gtog(b, aux); mulg(a, aux); smulg(4, aux); negg(aux);
   gtog(aux, s[4]->coe[1]);
   gtog(a, aux); mulg(a, aux); smulg(5, aux); negg(aux);
   gtog(aux, s[4]->coe[2]);
   gtog(b, aux); smulg(20, aux);
   gtog(aux, s[4]->coe[3]);
   gtog(a, aux); smulg(5, aux);
   gtog(aux, s[4]->coe[4]);
   itog(0, s[4]->coe[5]);
   itog(1, s[4]->coe[6]);
   itog(4, aux);
   scalarmul(aux, s[4]);
   cubic->deg = 3;
   itog(1, cubic->coe[3]);
   itog(0, cubic->coe[2]);
   gtog(a, cubic->coe[1]);
   gtog(b, cubic->coe[0]);
   for(j=5; j <= el; j++) {
        if(j % 2 == 0) {
                                ptop(s[j/2 + 2], s[j]); mulp(s[j/2-1], s[j]);
            polyrem(s[j]); mulp(s[j/2-1], s[j]); polyrem(s[j]);
            ptop(s[j/2-2], s[0]); mulp(s[j/2+1], s[0]); polyrem(s[0]);
            mulp(s[j/2+1], s[0]); polyrem(s[0]);
            subp(s[0], s[j]); mulp(s[j/2], s[j]); polyrem(s[j]);
                           gtog(p, aux); iaddg(1, aux); gshiftright(1, aux);
                                scalarmul(aux, s[j]);
        } else {
            ptop(s[(j-1)/2+2], s[j]);
            mulp(s[(j-1)/2], s[j]); 
polyrem(s[j]);
            mulp(s[(j-1)/2], s[j]); 
polyrem(s[j]);
            mulp(s[(j-1)/2], s[j]); 
polyrem(s[j]);
            ptop(s[(j-1)/2-1], s[0]);
            mulp(s[(j-1)/2 + 1], s[0]); polyrem(s[0]);
            mulp(s[(j-1)/2 + 1], s[0]); polyrem(s[0]);
            mulp(s[(j-1)/2 + 1], s[0]); polyrem(s[0]);
            if(((j-1)/2) % 2 == 1) {
                                        mulp(cubic, s[0]); polyrem(s[0]);
                                        mulp(cubic, s[0]); polyrem(s[0]);
            } else {
                                        mulp(cubic, s[j]); polyrem(s[j]);
                                        mulp(cubic, s[j]); polyrem(s[j]);
            }
// polyout(s[1]); polyout(s[3]); polyout(s[0]); polyout(s[j]);
            subp(s[0], s[j]);
            polyrem(s[j]);
        }
   }
}      

init_recip(int el) {
   int j;
   ptop(s[el], smonic); 
   if(el == 2) {
		mulp(cubic, smonic); polyrem(smonic);
   }
   gtog(smonic->coe[smonic->deg], aux); /* High coeff. */
   binvg(p, aux);
   scalarmul(aux, smonic);  /* s is now monic. */
   s[0]->deg = smonic->deg + 1;
   for(j=0; j <= s[0]->deg; j++) itog(1, s[0]->coe[j]);
   ptop(smonic, pbuff);
   remp(s[0], pbuff, 1);  /* Initialize reciprocal of s as precip. */
}

/* void powerpoly(poly x, giant n)
{       int len, pos;
        ptop(x, pbuff);
        x->deg = 0; itog(1, x->coe[0]);
        len = bitlen(n);
        pos = 0;
        while(1) {
                if(bitval(n, pos++)) {
                        mulp(pbuff, x);
                        fullmod(x);
                }
                if(pos>=len) break;
                mulp(pbuff, pbuff);
                fullmod(pbuff);
        }
}
*/

void powerpoly(poly x, giant n)
/* Base-4 window. */
{       int pos, code;
        ptop(x, pbuff);  /* x. */
	ptop(pbuff, pbuff2);
	mulmod(pbuff2, pbuff2); mulmod(pbuff, pbuff2); /* x^3. */ 
        pos = bitlen(n)-2;
        while(pos >= 0) {
		mulmod(x, x);
		if(pos==0) {
			if(bitval(n, pos) != 0) {
				mulmod(pbuff, x);
			}
			break;
		}
		code = (bitval(n, pos) != 0) * 2 + (bitval(n, pos-1) != 0);
		switch(code) {
			case 0: mulmod(x,x); break;
			case 1: mulmod(x,x); 
				mulmod(pbuff, x);
				break;
			case 2: mulmod(pbuff, x); 
				mulmod(x,x); break;
			case 3: mulmod(x,x); mulmod(pbuff2, x); break;
		}
		pos -= 2;
        }
}

mulmod(poly x, poly y) {
        mulp(x, y); fullmod(y);
}

elldoublep(poly n1, poly d1, poly m1, poly c1, poly n0, poly d0,
                poly m0, poly c0) {

     ptop(n1, mn); mulmod(n1, mn);
          ptop(mn, pbuff); addp(mn, mn); addp(pbuff, mn); 
     fullmod(mn);
          ptop(d1, pbuff); mulmod(d1, pbuff);
          scalarmul(a, pbuff); addp(pbuff, mn);
          fullmod(mn);
          mulmod(c1, mn);
          ptop(m1, md); addp(md, md);
          mulmod(d1, md); mulmod(d1, md); mulmod(cubic, md);

     ptop(d1, n0); mulmod(mn, n0); mulmod(mn, n0);
          mulmod(cubic, n0);
          ptop(n1, pbuff); addp(pbuff, pbuff); fullmod(pbuff);
          mulmod(md, pbuff); mulmod(md, pbuff);
          subp(pbuff, n0); fullmod(n0);
     ptop(md, d0); mulmod(md, d0); mulmod(d1, d0);

     ptop(mn, m0); mulmod(c1, m0);
          ptop(d0, pbuff); mulmod(n1, pbuff);
          ptop(n0, c0); mulmod(d1, c0); subp(c0, pbuff);
          fullmod(pbuff);
          mulmod(pbuff, m0);
          ptop(m1, pbuff); mulmod(md, pbuff);
          mulmod(d1, pbuff); mulmod(d0, pbuff);
          subp(pbuff, m0); fullmod(m0);

     ptop(c1, c0); mulmod(md, c0); mulmod(d1, c0); mulmod(d0, c0);
}

elladdp(poly n1, poly d1, poly m1, poly c1, poly n2, poly d2, poly m2, poly c2, poly n0, poly d0, poly m0, poly c0) {
        ptop(m2, mn); mulmod(c1, mn);
   ptop(m1, pbuff); mulmod(c2, pbuff);
   subp(pbuff, mn); fullmod(mn);
   mulmod(d1, mn); mulmod(d2, mn);

        ptop(n2, md); mulmod(d1, md);
   ptop(n1, pbuff); mulmod(d2, pbuff);
   subp(pbuff, md); fullmod(md);
   mulmod(c1, md); mulmod(c2, md);

   ptop(cubic, n0); mulmod(mn, n0); mulmod(mn, n0); 
   mulmod(d1, n0); mulmod(d2, n0);
   ptop(n1, pbuff); mulmod(d2, pbuff);
   ptop(n2, d0); mulmod(d1, d0);
   addp(d0, pbuff); mulmod(md, pbuff); mulmod(md, pbuff);
   subp(pbuff, n0); fullmod(n0);

   ptop(md, d0); mulmod(md, d0); mulmod(d1, d0); mulmod(d2, d0);

   ptop(mn, m0); mulmod(c1, m0);
   ptop(d0, pbuff); mulmod(n1, pbuff);
   ptop(d1, c0); mulmod(n0, c0);
   subp(c0, pbuff); fullmod(pbuff);
   mulmod(pbuff, m0);
   ptop(m1, pbuff); mulmod(md, pbuff);
   mulmod(d0, pbuff); mulmod(d1, pbuff);
   subp(pbuff, m0); fullmod(m0);

   ptop(md, c0); mulmod(c1, c0); mulmod(d0, c0); mulmod(d1, c0);

}   

polyout(poly x) {
   int j;
   for(j=0; j <= x->deg; j++) {printf("%d: ",j); gout(x->coe[j]);}
}

main(int argc, char **argv) {
      int j, ct = 0, el, xmatch, ymatch;
      int k, t;
      int T[L_CEIL], P[L_CEIL], LL[L_CEIL];
      giant ss[L_CEIL];
      unsigned int ord, ordminus;
      point_proj pt, pt2;

      p = newgiant(INFINITY);  /* Also sets up internal giants' stacks. */
      j = ((Q_MAX+15)/16);
      init_tools(2*j);
	  a = newgiant(j);
	  b = newgiant(j);
      
entry:
      printf("Give p > 3, a, b on separate lines:\n"); fflush(stdout);
      gin(p);  /* Field prime. */
      if((Q = bitlen(p)) > Q_MAX) {
		fprintf(stderr, "p too large, larger than %d bits.\n", Q);
		goto entry;
	  }
      if(!prime_probable(p)) {
		fprintf(stderr, "p is not but must be prime.\n", Q);
		goto entry;
	  }
      gin(a); gin(b); /* Curve parameters. */

	  t1 = newgiant(2*j);
	  t2 = newgiant(2*j);
/* Next, discriminant test for legitimacy of curve. */
	gtog(a, t1); squareg(t1); modg(p, t1); mulg(a, t1); modg(p, t1);
    gshiftleft(2, t1);  /* 4 a^3 mod p. */
    gtog(b, t2); squareg(t2); modg(p, t2); smulg(27, t2);
    addg(t2, t1); modg(p, t1);
    if(isZero(t1)) {
		fprintf(stderr, "Discriminant FAILED\n");
		goto entry;
    }
    printf("Discriminant PASSED\n"); fflush(stdout);

/* Next, find an efficient prime power array such that
   Prod[powers] >= 16 p. */

 /* Create minimal prime power array such that Prod[powers]^2 > 16p. */

    gtog(p, t2); gshiftleft(4, t2);   /* t2 := 16p. */

    L_MAX = 3;
    while(L_MAX <= L_CEIL-1) {
		for(j=0; j <= L_MAX; j++) LL[j] = 0;
   		for(j=2; j <= L_MAX; j++) {
			if(primeq(j)) { 
				LL[j] = 1;
		    	k = j;
				while(1) {
			    	k *= j;
					if(k <= L_MAX) {
						LL[k] = 1;
						LL[k/j] = 0;
					}
					else break;
				}
			}
		}
    	itog(1, t1);
    	for(j=2; j <= L_MAX; j++) {
			if(LL[j]) { smulg(j, t1); smulg(j, t1); } /* Building up t1^2. */
		}
		if(gcompg(t1, t2) > 0) break;
        ++L_MAX;
	}

   printf("Initializing for prime powers:\n"); 
   for(j=2; j <= L_MAX; j++) {
		if(LL[j]) printf("%d ", j);
   }
   printf("\n");
   fflush(stdout);


   MAX_DIGS = (2 + (Q+15)/8);  /* Size of (squared) coefficients. */   
   MAX_COEFFS = ((L_MAX+1)*(L_MAX+2));

   init_all();
   schain(L_MAX+1);

for(L = 2; L <= L_MAX; L++) {
      if(!LL[L]) continue;
printf("Resolving Schoof L = %d...\n", L);
      P[ct] = L;  /* Stuff another prime power. */
      init_recip(L);
// printf("s: "); polyout(s[L]);
      gtog(p, aux2);
      k = idivg(L, aux2);  /* p (mod L). */

printf("power...\n");
      txd->deg = 0; itog(1, txd->coe[0]);
      tyd->deg = 0; itog(1, tyd->coe[0]);
      txn->deg = 1; itog(0, txn->coe[0]); itog(1, txn->coe[1]);
      ptop(txn, kxn);
      
      gtog(p, aux2);
      powerpoly(txn, aux2); /* x^p. */
printf("x^p done...\n");
      ptop(txn, powx);
      powerpoly(powx, aux2);
printf("x^p^2 done...\n");
      ptop(cubic, tyn);
      gtog(p, aux2); itog(1, aux); subg(aux, aux2);
      gshiftright(1, aux2); /* aux2 := (p-1)/2. */
      powerpoly(tyn, aux2); /* y^p. */
printf("y^p done...\n");
      ptop(tyn, powy); mulp(tyn, powy); fullmod(powy);
      mulp(cubic, powy); fullmod(powy);
      powerpoly(powy, aux2);
      mulp(tyn, powy); fullmod(powy);
printf("Powers done...\n");

// printf("pow" ); polyout(powx); polyout(powy);
      ptop(txn, txn1); ptop(txd, txd1);  /* Save t = 1 case. */
      ptop(tyn, tyn1); ptop(tyd, tyd1);
/* We now shall test
     (powx, y powy) + k(kxn/kxd, y kyn/kyd) = t(txn/txd, y tyn/tyd)
 */

    if(k==1) { ptop(txd, kxd); ptop(txd, kyd);
                              ptop(txd, kyn);
    } else {
                   ptop(s[k], kxd); mulp(s[k], kxd); fullmod(kxd);
     if(k%2==0) { mulp(cubic, kxd); fullmod(kxd); }
     mulp(kxd, kxn); fullmod(kxn);
     ptop(s[k-1], pbuff); mulp(s[k+1], pbuff); fullmod(pbuff);
     if(k%2==1) {mulp(cubic, pbuff); fullmod(pbuff); }
     subp(pbuff, kxn); fullmod(kxn);

     ptop(s[k+2], kyn); mulp(s[k-1], kyn); fullmod(kyn);
          mulp(s[k-1], kyn); fullmod(kyn);
     if(k > 2) {
                ptop(s[k-2], pbuff); mulp(s[k+1], pbuff); fullmod(pbuff);
          mulp(s[k+1], pbuff); fullmod(pbuff);
     subp(pbuff, kyn); fullmod(kyn);
     }
     ptop(s[k], kyd); mulp(s[k], kyd); fullmod(kyd);     
          mulp(s[k], kyd); fullmod(kyd);
     if(k%2==0) { mulp(cubic, kyd); fullmod(kyd);
                        mulp(cubic, kyd); fullmod(kyd);}
     itog(4, aux2); scalarmul(aux2, kyd);
    }
//printf("kP: "); polyout(kxn); polyout(kxd); polyout(kyn); polyout(kyd);
/* Commence t = 0 check. */
printf("Checking t = %d ...\n", 0);
fflush(stdout);

     ptop(powx, pbuff); mulp(kxd, pbuff);
     subp(kxn, pbuff); 
     fullmod(pbuff);

     xmatch = ymatch = 0;
     if(iszerop(pbuff)) {
		 xmatch = 1;
         /* Now check y coords. */
 		 if(L == 2) goto resolve;
         ptop(powy, pbuff); mulp(kyd, pbuff);
         addp(kyn, pbuff);
         fullmod(pbuff);
         if(iszerop(pbuff)) {
               resolve:
					printf("%d %d\n", L, 0);
               		T[ct++] = 0;
                    continue;
         } else ymatch = 1;
     }
/* Combine pt1 and pt2. */
   if((xmatch == 1) && (ymatch == 1)) 
       elldoublep(powx, txd, powy, txd, nx, dx, ny, dy);
       else
       elladdp(powx, txd, powy, txd, kxn, kxd, kyn, kyd, nx, dx, ny, dy);
/* Now {nx/dx, ny/dy} is (fixed) LHS. */
// printf("add12: "); polyout(nx); polyout(dx); polyout(ny); polyout(dy);
/* Commence t > 0 check. */
    for(t=1; t <= L/2; t++) {
printf("Checking t = %d ...\n", t);
         if(t > 1) { /* Add (tx1, ty1) to (txn, tyn). */
                                ptop(txn1, pbuff); mulmod(txd, pbuff);
            ptop(txn, powx); mulmod(txd1, powx);
                                subp(powx, pbuff); fullmod(pbuff);
            if(!iszerop(pbuff))
                                 elladdp(txn1, txd1, tyn1, tyd1, txn, txd, tyn, tyd,
                                        tmp1, tmp2, tmp3, tmp4);
                           else elldoublep(txn, txd, tyn, tyd,
                                        tmp1, tmp2, tmp3, tmp4); 
            ptop(tmp1, txn); ptop(tmp2, txd);
                           ptop(tmp3, tyn); ptop(tmp4, tyd);
         }
// printf("tQ: "); polyout(txn); polyout(txd); polyout(tyn); polyout(tyd);
         /* Next, check {nx/dx, ny/dy} =? {txn/txd, tyn/tyd}. */
                        ptop(nx, pbuff); mulmod(txd, pbuff);
                   ptop(dx, powx); mulmod(txn, powx);
                        subp(powx, pbuff); fullmod(pbuff);
                   if(!iszerop(pbuff)) continue;
         /* Next, check y. */
                //      printf("y check!\n");
                        ptop(ny, pbuff); mulmod(tyd, pbuff);
                   ptop(dy, powx); mulmod(tyn, powx);
                        subp(powx, pbuff); fullmod(pbuff);
                   if(iszerop(pbuff)) {
                        printf("%d %d\n", L, t);
               			T[ct++] = t;
                   }  else {
                        printf("%d %d\n", L, L-t);
               			T[ct++] = L-t;
         			}
					fflush(stdout);
         			break;
   }
}

/* Now, prime powers P[] and CRT residues T[] are intact. */
	printf("Prime powers L:\n");
	printf("{");
    for(j=0; j < ct-1; j++) {
		printf("%d, ", P[j]);
    }
    printf("%d }\n", P[ct-1]);

	printf("Residues t (mod L):\n");
	printf("{");
    for(j=0; j < ct-1; j++) {
		printf("%d, ", T[j]);
    }
    printf("%d }\n", T[ct-1]);

/* Mathematica algorithm for order: 
plis = {2^5, 3^3, 5^2, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
tlis = {1,    26,   4, 2,  4, 11,  6,  5, 19, 22, 10, 16,  7, 22, 11};
prod = Apply[Times, plis];
prlis = prod/plis;
invlis = Table[PowerMod[prlis[[q]], -1, plis[[q]]],{q,1,Length[plis]}];
p = 2^127 - 1;
t = Mod[tlis . (prlis * invlis), prod];
ord = p + 1 - If[t^2 > 4p, t - prod, t]
*/

  itog(1, t1);
  for(j=0; j < ct; j++) {
    free(s[j]);  /* Just to clear memory. */
	smulg(P[j], t1);
  }

  for(j=0; j < 2*ct; j++) {
	   ss[j] = newgiant(MAX_DIGS);
  }

  for(j=0; j < ct; j++) {
	gtog(t1, ss[j]);
    itog(P[j], t2);
    divg(t2, ss[j]);
  }

  for(j=0; j < ct; j++) {
       gtog(ss[j], ss[j+ct]);
       itog(P[j], t2);
	   invg(t2, ss[j+ct]);
  }

  itog(0, t4);
  for(j=0; j < ct; j++) {
	itog(T[j], t5);
	mulg(ss[j], t5);
	mulg(ss[j+ct], t5);
	addg(t5, t4);
  }
  modg(t1, t4);
  gtog(p, t5);
  iaddg(1, t5);
  gtog(t4, t2);
  squareg(t4);
  gtog(p, t3); gshiftleft(2, t3);
  if(gcompg(t4, t3) > 0) subg(t1, t2);
  subg(t2, t5);
  printf("Parameters:\n");
  printf("p = "); gout(p);
  printf("a = "); gout(a);
  printf("b = "); gout(b);
  printf("Curve order:\n");
  printf("o = "); gout(t5); gtog(t5, t3); /* Save order as t3. */
  printf("Twist order:\n");
  printf("o' = ");
  addg(t2, t5);
  addg(t2, t5);
  gout(t5);
/* Next, verify the order. */
  printf("Verifying order o:...\n");
  init_ell_proj(MAX_DIGS);
  pt = new_point_proj(MAX_DIGS);
  pt2 = new_point_proj(MAX_DIGS);
  itog(1,t2);
  find_point_proj(pt, t2, a, b, p);
  printf("A point on the curve y^2 = x^3 + a x + b (mod p) is:\n");
  printf("(x,y,z) = {\n"); gout(pt->x); printf(",");
  gout(pt->y); printf(","); gout(pt->z);  
  printf("}\n");
  ell_mul_proj(pt, pt2, t3, a, p);
  printf("A multiple is:\n");
  printf("o * (x,y,z) = {\n");
  gout(pt2->x); printf(",");gout(pt2->y); printf(",");gout(pt2->z);  
  printf("}\n");
  if(!isZero(pt2->z)) {
	printf("TILT: multiple should be point-at-infinity.\n");
	exit(1);
  }
  printf("VERIFIED: multiple is indeed point-at-infinity.\n");
}
