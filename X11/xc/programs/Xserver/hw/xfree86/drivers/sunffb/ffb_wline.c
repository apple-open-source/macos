/*
 * Acceleration for the Creator and Creator3D framebuffer - Wide line rops.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK OR DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_wline.c,v 1.3 2001/03/03 22:41:34 tsi Exp $ */

#define PSZ 32

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"

#include "miwideline.h"

#error If we start using this again, need to fixup FFB_WRITE_ATTRIBUTES for wids -DaveM

/* Wheee, wide lines... */
extern int miPolyBuildEdge();

static int LeftClip, RightClip, TopClip, BottomClip;

#define Y_IN_BOX(y)	(((y) >= TopClip) && ((y) <= BottomClip))
#define CLIPSTEPEDGE(edgey,edge,edgeleft)	\
if (ybase == edgey) {				\
	if (edgeleft) {				\
		if (edge->x > xcl)		\
			xcl = edge->x;		\
	} else {				\
		if (edge->x < xcr)		\
			xcr = edge->x;		\
	}					\
	edgey++;				\
	edge->x += edge->stepx;			\
	edge->e += edge->dx;			\
	if (edge->e > 0) {			\
		edge->x += edge->signdx;	\
		edge->e -= edge->dy;		\
	}					\
}

#define CreatorPointHelper(pFfb, x, y)			\
do {								\
	ffb_fbcPtr ffb = (pFfb)->regs;			\
	FFB_WRITE_DRAWOP((pFfb), ffb, FFB_DRAWOP_DOT);	\
	FFBFifo(pFfb, 2);					\
	FFB_WRITE64(&ffb->bh, (y), (x));			\
} while (0)

static void CreatorFillRectHelper(FFBPtr pFfb,
				  int x, int y, int dx, int dy)
{
	ffb_fbcPtr ffb = pFfb->regs;
	int x2 = x + dx - 1;
	int y2 = y + dy - 1;
	
	if(x < LeftClip)
		x = LeftClip;
	if(x2 > RightClip)
		x2 = RightClip;
	if(y < TopClip)
		y = TopClip;
	if(y2 > BottomClip)
		y2 = BottomClip;

	dx = x2 - x + 1;
	dy = y2 - y + 1;

	if((dx > 0) && (dy > 0)) {
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
		FFBFifo(pFfb, 4);
		FFB_WRITE64(&ffb->by, y, x);
		FFB_WRITE64_2(&ffb->bh, dy, dx);
	}
}

/* The span helper does not check for y being clipped, caller beware */
static void CreatorSpanHelper(FFBPtr pFfb,
			      int x1, int y, int width)
{
	ffb_fbcPtr ffb = pFfb->regs;
	int x2 = x1 + width - 1;

	if(x1 < LeftClip)
		x1 = LeftClip;
	if(x2 > RightClip)
		x2 = RightClip;
	width = x2 - x1 + 1;	

	if(width > 0) {
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
		FFBFifo(pFfb, 4);
		FFB_WRITE64(&ffb->by, y, x1);
		FFB_WRITE64_2(&ffb->bh, 1, width);
	}
}

#define FixError(x, dx, dy, e, sign, step, h)	{	\
	e += (h) * dx;					\
	x += (h) * step;				\
	if(e > 0) {					\
		x += e * sign/dy;			\
		e %= dy;				\
		if(e) {					\
			x += sign;			\
			e -= dy;			\
		}					\
	} 	 					\
}

static void
CreatorFillPolyHelper (DrawablePtr pDrawable, GCPtr pGC,
		       int y, int overall_height,
		       PolyEdgePtr left, PolyEdgePtr right, 
		       int left_count, int right_count)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	int left_x, left_e, left_stepx, left_signdx, left_dy, left_dx;
	int right_x, right_e, right_stepx, right_signdx, right_dy, right_dx, height;
	int left_height = 0;
	int right_height = 0;
	int xorg = 0;
    
	if (pGC->miTranslate) {
		y += pDrawable->y;
		xorg = pDrawable->x;
	}

	while ((left_count || left_height) && (right_count || right_height)) {
		if (!left_height && left_count) { 
			left_height = left->height; 
			left_x = left->x + xorg; 
			left_stepx = left->stepx; 
			left_signdx = left->signdx; 
			left_e = left->e; 
			left_dy = left->dy; 
			left_dx = left->dx; 
			left_count--; 
			left++;
		}
		if (!right_height && right_count) { 
			right_height = right->height; 
			right_x = right->x + xorg + 1; 
			right_stepx = right->stepx; 
			right_signdx = right->signdx; 
			right_e = right->e; 
			right_dy = right->dy; 
			right_dx = right->dx; 
			right_count--; 
			right++; 
		}

		height = (left_height > right_height) ? right_height : left_height;

		left_height -= height;
		right_height -= height;
		while (height--) {
			if((right_x > left_x) && Y_IN_BOX(y))
				CreatorSpanHelper(pFfb, left_x, y, right_x - left_x);

			y++;
    	
			left_x += left_stepx; 
			left_e += left_dx; 
			if (left_e > 0) { 
				left_x += left_signdx; 
				left_e -= left_dy; 
			}
			right_x += right_stepx; 
			right_e += right_dx; 
			if (right_e > 0) { 
				right_x += right_signdx; 
				right_e -= right_dy; 
			}
		}
	}
}

static void
CreatorWideSegment (DrawablePtr pDrawable, GCPtr pGC,
		    int x1, int y1, int x2, int y2, 
		    Bool projectLeft, Bool projectRight,
		    LineFacePtr leftFace, LineFacePtr rightFace)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	double l, L, r, xa, ya, projectXoff, projectYoff, k, maxy;
	int x, y, dx, dy, finaly, lefty, righty, topy, bottomy, signdx;
	PolyEdgePtr left, right, top, bottom;
	PolyEdgeRec lefts[2], rights[2];
	LineFacePtr tface;
	int lw = pGC->lineWidth;

	/* draw top-to-bottom always */
	if (y2 < y1 || y2 == y1 && x2 < x1) {
		x = x1;
		x1 = x2;
		x2 = x;

		y = y1;
		y1 = y2;
		y2 = y;

		x = projectLeft;
		projectLeft = projectRight;
		projectRight = x;

		tface = leftFace;
		leftFace = rightFace;
		rightFace = tface;
	}

	dy = y2 - y1;
	signdx = 1;
	dx = x2 - x1;
	if (dx < 0)
		signdx = -1;

	leftFace->x = x1;
	leftFace->y = y1;
	leftFace->dx = dx;
	leftFace->dy = dy;

	rightFace->x = x2;
	rightFace->y = y2;
	rightFace->dx = -dx;
	rightFace->dy = -dy;

	if (!dy) {
		rightFace->xa = 0;
		rightFace->ya = (double) lw / 2.0;
		rightFace->k = -(double) (lw * dx) / 2.0;
		leftFace->xa = 0;
		leftFace->ya = -rightFace->ya;
		leftFace->k = rightFace->k;
		x = x1;
		if (projectLeft)
			x -= (lw >> 1);
		y = y1 - (lw >> 1);
		dx = x2 - x;
		if (projectRight)
			dx += (lw + 1 >> 1);
		dy = lw;
		if(pGC->miTranslate) {
			x += pDrawable->x;
			y += pDrawable->y;
		}
		CreatorFillRectHelper(pFfb, x, y, dx, dy);	
	} else if (!dx) {
		leftFace->xa =  (double) lw / 2.0;
		leftFace->ya = 0;
		leftFace->k = (double) (lw * dy) / 2.0;
		rightFace->xa = -leftFace->xa;
		rightFace->ya = 0;
		rightFace->k = leftFace->k;
		y = y1;
		if (projectLeft)
			y -= lw >> 1;
		x = x1 - (lw >> 1);
		dy = y2 - y;
		if (projectRight)
			dy += (lw + 1 >> 1);
		dx = lw;
		if(pGC->miTranslate) {
			x += pDrawable->x;
			y += pDrawable->y;
		}
		CreatorFillRectHelper(pFfb, x, y, dx, dy);
	} else {
		l = ((double) lw) / 2.0;
		L = sqrt((double)(dx*dx + dy*dy));

		if (dx < 0) {
			right = &rights[1];
			left = &lefts[0];
			top = &rights[0];
			bottom = &lefts[1];
		} else {
			right = &rights[0];
			left = &lefts[1];
			top = &lefts[0];
			bottom = &rights[1];
		}
		r = l / L;

		/* coord of upper bound at integral y */
		ya = -r * dx;
		xa = r * dy;

		if (projectLeft | projectRight) {
			projectXoff = -ya;
			projectYoff = xa;
		}

		/* xa * dy - ya * dx */
		k = l * L;

		leftFace->xa = xa;
		leftFace->ya = ya;
		leftFace->k = k;
		rightFace->xa = -xa;
		rightFace->ya = -ya;
		rightFace->k = k;

		if (projectLeft)
			righty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
						  k, dx, dy, x1, y1, 0, right);
		else
			righty = miPolyBuildEdge (xa, ya,
						  k, dx, dy, x1, y1, 0, right);

		/* coord of lower bound at integral y */
		ya = -ya;
		xa = -xa;

		/* xa * dy - ya * dx */
		k = - k;

		if (projectLeft)
			lefty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
						 k, dx, dy, x1, y1, 1, left);
		else
			lefty = miPolyBuildEdge (xa, ya,
						 k, dx, dy, x1, y1, 1, left);

		/* coord of top face at integral y */
		if (signdx > 0) {
			ya = -ya;
			xa = -xa;
		}

		if (projectLeft) {
			double xap = xa - projectXoff;
			double yap = ya - projectYoff;
			topy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
						-dy, dx, x1, y1, dx > 0, top);
		}
		else
			topy = miPolyBuildEdge(xa, ya, 0.0, 
					       -dy, dx, x1, y1, dx > 0, top);

		/* coord of bottom face at integral y */
		if (projectRight) {
			double xap = xa + projectXoff;
			double yap = ya + projectYoff;
			bottomy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
						   -dy, dx, x2, y2, dx < 0, bottom);
			maxy = -ya + projectYoff;
		} else {
			bottomy = miPolyBuildEdge (xa, ya, 0.0,
						   -dy, dx, x2, y2, dx < 0, bottom);
			maxy = -ya;
		}

		finaly = ICEIL (maxy) + y2;

		if (dx < 0) {
			left->height = bottomy - lefty;
			right->height = finaly - righty;
			top->height = righty - topy;
		} else {
			right->height =  bottomy - righty;
			left->height = finaly - lefty;
			top->height = lefty - topy;
		}
		bottom->height = finaly - bottomy;
		CreatorFillPolyHelper (pDrawable, pGC, topy, 
				       bottom->height + bottomy - topy, lefts, rights, 2, 2);
	}
}

static void
CreatorLineArcI (DrawablePtr pDraw, GCPtr pGC, int xorg, int yorg)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDraw->pScreen);
	int x, y, e, ex, slw;

	if (pGC->miTranslate) {
		xorg += pDraw->x;
		yorg += pDraw->y;
	}

	slw = pGC->lineWidth;
	if (slw == 1) {
		CreatorPointHelper(pFfb, xorg, yorg);
		return;
	}

	y = (slw >> 1) + 1;
	if (slw & 1)
		e = - ((y << 2) + 3);
	else
		e = - (y << 3);
	ex = -4;
	x = 0;
	while (y) {
		e += (y << 3) - 4;
		while (e >= 0) {
			x++;
			e += (ex = -((x << 3) + 4));
		}
		y--;
		slw = (x << 1) + 1;
		if ((e == ex) && (slw > 1))
			slw--;
	    
		if(Y_IN_BOX(yorg - y))
			CreatorSpanHelper(pFfb, xorg - x, yorg - y, slw);

		if ((y != 0) && ((slw > 1) || (e != ex)) && Y_IN_BOX(yorg + y))	
			CreatorSpanHelper(pFfb, xorg - x, yorg + y, slw);
	}
}

static void
CreatorLineArcD (DrawablePtr pDraw, GCPtr pGC,
		 double xorg, double yorg,
		 PolyEdgePtr edge1, int edgey1, Bool edgeleft1,
		 PolyEdgePtr edge2, int edgey2, Bool edgeleft2)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDraw->pScreen);
	double radius, x0, y0, el, er, yk, xlk, xrk, k;
	int xbase, ybase, y, boty, xl, xr, xcl, xcr, ymin, ymax, ymin1, ymin2;
	Bool edge1IsMin, edge2IsMin;

	xbase = floor(xorg);
	x0 = xorg - xbase;
	ybase = ICEIL (yorg);
	y0 = yorg - ybase;
	if (pGC->miTranslate) {
		xbase += pDraw->x;
		ybase += pDraw->y;
		edge1->x += pDraw->x;
		edge2->x += pDraw->x;
		edgey1 += pDraw->y;
		edgey2 += pDraw->y;
	}

	xlk = x0 + x0 + 1.0;
	xrk = x0 + x0 - 1.0;
	yk = y0 + y0 - 1.0;
	radius = ((double)pGC->lineWidth) / 2.0;
	y = floor(radius - y0 + 1.0);
	ybase -= y;
	ymin = ybase;
	ymax = 65536;
	edge1IsMin = FALSE;
	ymin1 = edgey1;
	if (edge1->dy >= 0) {
		if (!edge1->dy) {
			if (edgeleft1)
				edge1IsMin = TRUE;
			else
				ymax = edgey1;
			edgey1 = 65536;
		} else if ((edge1->signdx < 0) == edgeleft1)
			edge1IsMin = TRUE;
	}
	edge2IsMin = FALSE;
	ymin2 = edgey2;
	if (edge2->dy >= 0) {
		if (!edge2->dy) {
			if (edgeleft2)
				edge2IsMin = TRUE;
			else
				ymax = edgey2;
			edgey2 = 65536;
		} else if ((edge2->signdx < 0) == edgeleft2)
			edge2IsMin = TRUE;
	}
	if (edge1IsMin) {
		ymin = ymin1;
		if (edge2IsMin && (ymin1 > ymin2))
			ymin = ymin2;
	} else if (edge2IsMin)
		ymin = ymin2;
	el = radius * radius - ((y + y0) * (y + y0)) - (x0 * x0);
	er = el + xrk;
	xl = 1;
	xr = 0;
	if (x0 < 0.5) {
		xl = 0;
		el -= xlk;
	}
	boty = (y0 < -0.5) ? 1 : 0;
	if (ybase + y - boty > ymax)
		boty = ymax - ybase - y;
	while (y > boty) {
		k = (y << 1) + yk;
		er += k;
		while (er > 0.0) {
			xr++;
			er += xrk - (xr << 1);
		}
		el += k;
		while (el >= 0.0) {
			xl--;
			el += (xl << 1) - xlk;
		}
		y--;
		ybase++;
		if (ybase < ymin)
			continue;
		xcl = xl + xbase;
		xcr = xr + xbase;
		CLIPSTEPEDGE(edgey1, edge1, edgeleft1);
		CLIPSTEPEDGE(edgey2, edge2, edgeleft2);
		if((xcr >= xcl) && Y_IN_BOX(ybase)) 
			CreatorSpanHelper(pFfb, xcl, ybase, xcr - xcl + 1);
	}
	er = xrk - (xr << 1) - er;
	el = (xl << 1) - xlk - el;
	boty = floor(-y0 - radius + 1.0);
	if (ybase + y - boty > ymax)
		boty = ymax - ybase - y;
	while (y > boty) {
		k = (y << 1) + yk;
		er -= k;
		while ((er >= 0.0) && (xr >= 0)) {
			xr--;
			er += xrk - (xr << 1);
		}
		el -= k;
		while ((el > 0.0) && (xl <= 0)) {
			xl++;
			el += (xl << 1) - xlk;
		}
		y--;
		ybase++;
		if (ybase < ymin)
			continue;
		xcl = xl + xbase;
		xcr = xr + xbase;
		CLIPSTEPEDGE(edgey1, edge1, edgeleft1);
		CLIPSTEPEDGE(edgey2, edge2, edgeleft2);
		if((xcr >= xcl) && Y_IN_BOX(ybase)) 
			CreatorSpanHelper(pFfb, xcl, ybase, xcr - xcl + 1);
	}
}


static void
CreatorLineArc (DrawablePtr pDraw, GCPtr pGC,
		LineFacePtr leftFace, LineFacePtr rightFace,
		double xorg, double yorg,
		Bool isInt)
{
	int xorgi, yorgi, edgey1, edgey2;
	PolyEdgeRec	edge1, edge2;
	Bool edgeleft1, edgeleft2;

	if (isInt) {
		xorgi = leftFace ? leftFace->x : rightFace->x;
		yorgi = leftFace ? leftFace->y : rightFace->y;
	}
	edgey1 = 65536;
	edgey2 = 65536;
	edge1.x = 0; /* not used, keep memory checkers happy */
	edge1.dy = -1;
	edge2.x = 0; /* not used, keep memory checkers happy */
	edge2.dy = -1;
	edgeleft1 = FALSE;
	edgeleft2 = FALSE;

	if ((pGC->lineWidth > 2) &&
	    (pGC->capStyle == CapRound && pGC->joinStyle != JoinRound ||
	     pGC->joinStyle == JoinRound && pGC->capStyle == CapButt)) {
		if (isInt) {
			xorg = (double) xorgi;
			yorg = (double) yorgi;
		}

		if (leftFace && rightFace) {
			miRoundJoinClip (leftFace, rightFace, &edge1, &edge2,
					 &edgey1, &edgey2, &edgeleft1, &edgeleft2);
		} else if (leftFace) { 
			edgey1 = miRoundCapClip (leftFace, isInt, &edge1, &edgeleft1);
		} else if (rightFace) {
			edgey2 = miRoundCapClip (rightFace, isInt, &edge2, &edgeleft2);
		}

		isInt = FALSE;
	}

	if (isInt)
		CreatorLineArcI(pDraw, pGC, xorgi, yorgi);
	else
		CreatorLineArcD(pDraw, pGC, xorg, yorg, 
				&edge1, edgey1, edgeleft1,
				&edge2, edgey2, edgeleft2);
}

static void
CreatorLineJoin (DrawablePtr pDrawable, GCPtr pGC, LineFacePtr pLeft, LineFacePtr pRight)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	double mx, my, denom;
	PolyVertexRec vertices[4];
	PolySlopeRec slopes[4];
	PolyEdgeRec left[4], right[4];
	int edgecount, nleft, nright, y, height, swapslopes;
	int joinStyle = pGC->joinStyle;
	int lw = pGC->lineWidth;

	if (lw == 1) {
		/* Lines going in the same direction have no join */
		if ((pLeft->dx >= 0) == (pRight->dx <= 0))
			return;
		if (joinStyle != JoinRound) {
			denom = - pLeft->dx * (double)pRight->dy + pRight->dx *
				(double)pLeft->dy;
			if (denom == 0.0)
				return;	/* no join to draw */
		}
		if (joinStyle != JoinMiter) {
			if(pGC->miTranslate)
				CreatorPointHelper(pFfb,
						   pLeft->x + pDrawable->x,
						   pLeft->y + pDrawable->y);
			else
				CreatorPointHelper(pFfb, pLeft->x, pLeft->y);	
			return;
		}
	} else {
		if (joinStyle == JoinRound) {
			CreatorLineArc(pDrawable, pGC, pLeft, pRight,
				       (double)0.0, (double)0.0, TRUE);
			return;
		}
		denom = - pLeft->dx * (double)pRight->dy + pRight->dx * 
			(double)pLeft->dy;
		if (denom == 0.0)
			return;	/* no join to draw */
	}

	swapslopes = 0;
	if (denom > 0) {
		pLeft->xa = -pLeft->xa;
		pLeft->ya = -pLeft->ya;
		pLeft->dx = -pLeft->dx;
		pLeft->dy = -pLeft->dy;
	} else {
		swapslopes = 1;
		pRight->xa = -pRight->xa;
		pRight->ya = -pRight->ya;
		pRight->dx = -pRight->dx;
		pRight->dy = -pRight->dy;
	}

	vertices[0].x = pRight->xa;
	vertices[0].y = pRight->ya;
	slopes[0].dx = -pRight->dy;
	slopes[0].dy =  pRight->dx;
	slopes[0].k = 0;

	vertices[1].x = 0;
	vertices[1].y = 0;
	slopes[1].dx =  pLeft->dy;
	slopes[1].dy = -pLeft->dx;
	slopes[1].k = 0;

	vertices[2].x = pLeft->xa;
	vertices[2].y = pLeft->ya;

	if (joinStyle == JoinMiter) {
		my = (pLeft->dy  * (pRight->xa * pRight->dy - pRight->ya * pRight->dx) -
		      pRight->dy * (pLeft->xa  * pLeft->dy  - pLeft->ya  * pLeft->dx ))/
			denom;
		if (pLeft->dy != 0) 
			mx = pLeft->xa + (my - pLeft->ya) *
				(double) pLeft->dx / (double) pLeft->dy;
		else
			mx = pRight->xa + (my - pRight->ya) *
				(double) pRight->dx / (double) pRight->dy;
    	
		/* check miter limit */
		if ((mx * mx + my * my) * 4 > SQSECANT * lw * lw)
			joinStyle = JoinBevel;
	}

	if (joinStyle == JoinMiter) {
		slopes[2].dx = pLeft->dx;
		slopes[2].dy = pLeft->dy;
		slopes[2].k =  pLeft->k;
		if (swapslopes) {
			slopes[2].dx = -slopes[2].dx;
			slopes[2].dy = -slopes[2].dy;
			slopes[2].k  = -slopes[2].k;
		}
		vertices[3].x = mx;
		vertices[3].y = my;
		slopes[3].dx = pRight->dx;
		slopes[3].dy = pRight->dy;
		slopes[3].k  = pRight->k;
		if (swapslopes) {
			slopes[3].dx = -slopes[3].dx;
			slopes[3].dy = -slopes[3].dy;
			slopes[3].k  = -slopes[3].k;
		}
		edgecount = 4;
	} else {
		double	scale, dx, dy, adx, ady;

		adx = dx = pRight->xa - pLeft->xa;
		ady = dy = pRight->ya - pLeft->ya;
		if (adx < 0)
			adx = -adx;
		if (ady < 0)
			ady = -ady;
		scale = ady;
		if (adx > ady)
			scale = adx;
		slopes[2].dx = (dx * 65536) / scale;
		slopes[2].dy = (dy * 65536) / scale;
		slopes[2].k = ((pLeft->xa + pRight->xa) * slopes[2].dy -
			       (pLeft->ya + pRight->ya) * slopes[2].dx) / 2.0;
		edgecount = 3;
	}

	y = miPolyBuildPoly (vertices, slopes, edgecount, pLeft->x, pLeft->y,
			     left, right, &nleft, &nright, &height);
	CreatorFillPolyHelper(pDrawable, pGC, y, height, left, right, nleft, nright);
}

void
CreatorWideLineSolid (DrawablePtr pDrawable, GCPtr pGC, int mode, int npt, DDXPointPtr pPts)
{
	int x1, y1, x2, y2, first = TRUE;
	Bool projectLeft, projectRight, somethingDrawn = FALSE, selfJoin = FALSE;
	LineFaceRec leftFace, rightFace, prevRightFace, firstFace;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	RegionPtr clip;
	int numRects;
	unsigned int ppc;
	BoxPtr pbox;

	clip = ((cfbPrivGC *)(pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip;
	numRects = REGION_NUM_RECTS(clip);
	if (!numRects)
		return;
	if (!(ppc = FFBSetClip(pFfb, ffb, clip, numRects))) {
		miWideLine(pDrawable, pGC, mode, npt, pPts);
		return;
	}

	LeftClip = 2048; TopClip = 2048;
	RightClip = 0; BottomClip = 0;
	for (pbox = REGION_RECTS(clip); numRects; numRects--, pbox++) {
		if (pbox->x1 < LeftClip) LeftClip = pbox->x1;
		if (pbox->x2 > RightClip) RightClip = pbox->x2 - 1;
		if (pbox->y1 < TopClip) TopClip = pbox->y1;
		if (pbox->y2 > BottomClip) BottomClip = pbox->y2 - 1;
	}

	FFB_WRITE_ATTRIBUTES(pFfb,
			     ppc|FFB_PPC_APE_DISABLE|FFB_PPC_TBE_OPAQUE|
			     FFB_PPC_XS_CONST|FFB_PPC_YS_CONST|FFB_PPC_ZS_CONST|FFB_PPC_CS_CONST,
			     FFB_PPC_VCE_MASK|FFB_PPC_ACE_MASK|FFB_PPC_APE_MASK|FFB_PPC_TBE_MASK|
			     FFB_PPC_XS_MASK|FFB_PPC_YS_MASK|FFB_PPC_ZS_MASK|FFB_PPC_CS_MASK,
			     pGC->planemask,
			     FFB_ROP_EDIT_BIT|pGC->alu,
			     -1, pGC->fgPixel,
			     FFB_FBC_DEFAULT);

	x2 = pPts->x;
	y2 = pPts->y;
	if (npt > 1) {
		if (mode == CoordModePrevious) {
			int nptTmp;
			register DDXPointPtr pPtsTmp;
    
			x1 = x2;
			y1 = y2;
			nptTmp = npt;
			pPtsTmp = pPts + 1;
			while (--nptTmp) {
				x1 += pPtsTmp->x;
				y1 += pPtsTmp->y;
				++pPtsTmp;
			}
			if ((x2 == x1) && (y2 == y1))
				selfJoin = TRUE;
		} else if ((x2 == pPts[npt-1].x) && (y2 == pPts[npt-1].y)) 
			selfJoin = TRUE;
	}

	projectLeft = ((pGC->capStyle == CapProjecting) && !selfJoin);
	projectRight = FALSE;

	while (--npt) {
		x1 = x2;
		y1 = y2;
		++pPts;
		x2 = pPts->x;
		y2 = pPts->y;
		if (mode == CoordModePrevious) {
			x2 += x1;
			y2 += y1;
		}
		if ((x1 != x2) || (y1 != y2)) {
			somethingDrawn = TRUE;
			if ((npt == 1) && (pGC->capStyle == CapProjecting) && !selfJoin)
				projectRight = TRUE;
			CreatorWideSegment(pDrawable, pGC, x1, y1, x2, y2,
					   projectLeft, projectRight, &leftFace, &rightFace);
			if (first) {
				if (selfJoin)
					firstFace = leftFace;
				else if (pGC->capStyle == CapRound) {
					if (pGC->lineWidth == 1) {
						if(pGC->miTranslate) 
							CreatorPointHelper(pFfb,
									   x1 + pDrawable->x, 
									   y1 + pDrawable->y);
						else
							CreatorPointHelper(pFfb, x1, y1);
					} else
						CreatorLineArc(pDrawable, pGC,
							       &leftFace, (LineFacePtr) NULL,
							       (double)0.0, (double)0.0,
							       TRUE);
				}
			} else 
				CreatorLineJoin (pDrawable, pGC, &leftFace, &prevRightFace);

			prevRightFace = rightFace;
			first = FALSE;
			projectLeft = FALSE;
		}
		if (npt == 1 && somethingDrawn) {
			if (selfJoin)
				CreatorLineJoin (pDrawable, pGC, &firstFace, &rightFace);
			else if (pGC->capStyle == CapRound) {
				if (pGC->lineWidth == 1) {
					if(pGC->miTranslate) 
						CreatorPointHelper(pFfb,
								   x2 + pDrawable->x, 
								   y2 + pDrawable->y);
					else
						CreatorPointHelper(pFfb, x2, y2);
				} else
					CreatorLineArc (pDrawable, pGC, 
							(LineFacePtr) NULL, &rightFace,
							(double)0.0, (double)0.0,
							TRUE);
			}
		}
	}

	/* handle crock where all points are coincedent */
	if (!somethingDrawn) {
		projectLeft = (pGC->capStyle == CapProjecting);
		CreatorWideSegment (pDrawable, pGC,
				    x2, y2, x2, y2, projectLeft, projectLeft,
				    &leftFace, &rightFace);
		if (pGC->capStyle == CapRound) {
			CreatorLineArc (pDrawable, pGC, 
					&leftFace, (LineFacePtr) NULL,
					(double)0.0, (double)0.0,
					TRUE);
			rightFace.dx = -1;	/* sleezy hack to make it work */
			CreatorLineArc (pDrawable, pGC,
					(LineFacePtr) NULL, &rightFace,
					(double)0.0, (double)0.0,
					TRUE);
		}
	}
	pFfb->rp_active = 1;
}
