/* $XConsortium: XKBui.c /main/2 1995/12/07 21:18:19 kaleb $ */
/************************************************************
 Copyright (c) 1996 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
/* $XFree86: xc/lib/xkbui/XKBui.c,v 3.6 1999/06/20 07:14:08 dawes Exp $ */

#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(SVR4) && defined(i386) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE
#  include <math.h>
#  undef _XOPEN_SOURCE
#else
#  include <math.h>
#endif /* _XOPEN_SOURCE */

#include <X11/Xfuncs.h>
#include "XKBuiPriv.h"
#include "XKBfileInt.h"
#include <X11/extensions/XKBfile.h>

#ifndef M_PI
#  define M_PI	3.141592653589793238462
#endif

static XkbUI_ViewOptsRec dfltOpts = { 
	XkbUI_AllViewOptsMask	/* present */,
	1			/* fg */,
	0			/* bg */,
	XkbUI_KeyNames		/* label_mode */,
	0 			/* color_mode */,
	{ 
		0	/* viewport.x */,
		0	/* viewport.y */,
		640	/* viewport.width */,
		480	/* viewport.height */
	},
	10, 10,		/* margin_width, margin_height */
	None
};

XkbUI_ViewPtr 
#if NeedFunctionPrototypes
XkbUI_SimpleInit(Display *dpy,Window win,int width,int height)
#else
XkbUI_SimpleInit(dpy,win,width,height)
	Display *		dpy;
	Window			win;
	int			width;
	int			height;
#endif
{
XkbDescPtr	xkb;

    if ((!dpy)||(win==None)||(width<1)||(height<1))
	return NULL;
    xkb= XkbGetKeyboard(dpy,XkbGBN_AllComponentsMask,XkbUseCoreKbd);
    if (!xkb)
	return NULL;
    return XkbUI_Init(dpy,win,width,height,xkb,NULL);
}

static void
#if NeedFunctionPrototypes
_XkbUI_AllocateColors(XkbUI_ViewPtr view)
#else
_XkbUI_AllocateColors(view)
    XkbUI_ViewPtr	view;
#endif
{
register int i;
Display *	dpy;
XColor		sdef,xdef;
XkbDescPtr	xkb;

    dpy= view->dpy;
    xkb= view->xkb;
    if (view->opts.cmap==None)
	view->opts.cmap= DefaultColormap(dpy,DefaultScreen(dpy));
    for (i=0;i<xkb->geom->num_colors;i++) {
	char *spec;
	Bool		found;

	spec= xkb->geom->colors[i].spec;
	found= False;
	if (XAllocNamedColor(view->dpy,view->opts.cmap,spec,&sdef,&xdef)) {
	    xkb->geom->colors[i].pixel= sdef.pixel;
#ifdef DEBUG
	    fprintf(stderr,"got pixel %d for \"%s\"\n",sdef.pixel,spec);
#endif
	    found= True;
	}
	if ((!found)&&(XkbLookupCanonicalRGBColor(spec,&sdef))) { 
	    char buf[20];
	    sprintf(buf,"#%02x%02x%02x",(sdef.red>>8)&0xff,
						(sdef.green>>8)&0xff,
						(sdef.blue>>8)&&0xff);
	    if (XAllocNamedColor(view->dpy,view->opts.cmap,buf,&sdef,&xdef)) {
		xkb->geom->colors[i].pixel= sdef.pixel;
#ifdef DEBUG
		fprintf(stderr,"got pixel %d for \"%s\"\n",sdef.pixel,spec);
#endif
		found= True;
	    }
	}
	if (!found) {
	    xkb->geom->colors[i].pixel= view->opts.fg;
	    fprintf(stderr,"Couldn't allocate color \"%s\"\n",spec);
	}
    }
    return;
}

XkbUI_ViewPtr 
#if NeedFunctionPrototypes
XkbUI_Init(	Display *		dpy,
		Window			win,
		int			width,
		int			height,
		XkbDescPtr		xkb,
		XkbUI_ViewOptsPtr	opts)
#else
XkbUI_Init(dpy,win,width,height,xkb,opts)
	Display *		dpy;
	Window			win;
	int			width;
	int			height;
	XkbDescPtr		xkb;
	XkbUI_ViewOptsPtr	opts;
#endif
{
XGCValues	xgcv;
XkbUI_ViewPtr	view;
int		scrn;

    if ((!dpy)||(!xkb)||(!xkb->geom)||(win==None)||(width<1)||(height<1))
	return NULL;
    view= _XkbTypedCalloc(1,XkbUI_ViewRec);
    if (!view)
	return NULL;
    scrn= DefaultScreen(dpy);
    view->dpy= 			dpy;
    view->xkb= 			xkb;
    view->win=			win;
    view->opts= 		dfltOpts;
    view->opts.fg=		WhitePixel(dpy,scrn);
    view->opts.bg=		BlackPixel(dpy,scrn);
    view->opts.viewport.x=	0;
    view->opts.viewport.y=	0;
    view->opts.viewport.width=	width;
    view->opts.viewport.height=	height;
    if ((opts)&&(opts->present)) {
	if (opts->present&XkbUI_BackgroundMask)
	    view->opts.bg=		opts->bg;
	if (opts->present&XkbUI_ForegroundMask)
	    view->opts.fg=		opts->fg;
	if (opts->present&XkbUI_LabelModeMask)
	    view->opts.label_mode=	opts->label_mode;
	if (opts->present&XkbUI_ColorModeMask)
	    view->opts.color_mode=	opts->color_mode;
	if (opts->present&XkbUI_WidthMask)
	    view->opts.viewport.width=	opts->viewport.width;
	if (opts->present&XkbUI_HeightMask)
	    view->opts.viewport.height=	opts->viewport.height;
	if (opts->present&XkbUI_XOffsetMask)
	    view->opts.viewport.x=	opts->viewport.x;
	if (opts->present&XkbUI_YOffsetMask)
	    view->opts.viewport.y=	opts->viewport.y;
	if (opts->present&XkbUI_MarginWidthMask)
	    view->opts.margin_width=	opts->margin_width;
	if (opts->present&XkbUI_MarginHeightMask)
	    view->opts.margin_height=	opts->margin_height;
	if (opts->present&XkbUI_ColormapMask)
	    view->opts.cmap=		opts->cmap;
    }
    view->canvas_width= width+(2*view->opts.margin_width);
    view->canvas_height= height+(2*view->opts.margin_height);
    if (view->opts.viewport.width>view->canvas_width) {
	int tmp;
	tmp= (view->opts.viewport.width-view->canvas_width)/2;
	view->opts.margin_width+= tmp;
    }
    if (view->opts.viewport.height>view->canvas_height) {
	int tmp;
	tmp= (view->opts.viewport.height-view->canvas_height)/2;
	view->opts.margin_height+= tmp;
    }
    bzero(view->state,XkbMaxLegalKeyCode+1);

    xgcv.foreground= view->opts.fg;
    xgcv.background= view->opts.bg;
    view->gc= XCreateGC(view->dpy,view->win,GCForeground|GCBackground,&xgcv);
    view->xscale= ((double)width)/((double)xkb->geom->width_mm);
    view->yscale= ((double)height)/((double)xkb->geom->height_mm);

    _XkbUI_AllocateColors(view);
    return view;
}

Status 
#if NeedFunctionPrototypes
XkbUI_SetViewOpts(XkbUI_ViewPtr	view,XkbUI_ViewOptsPtr opts)
#else
XkbUI_SetViewOpts(view,opts)
	XkbUI_ViewPtr		view;
	XkbUI_ViewOptsPtr	opts;
#endif
{
    if ((!view)||(!opts))
	return BadValue;
    if (opts->present==0)
	return Success;
    if (opts->present&XkbUI_BackgroundMask)
	view->opts.bg=			opts->bg;
    if (opts->present&XkbUI_ForegroundMask)
	view->opts.fg=			opts->fg;
    if (opts->present&XkbUI_LabelModeMask)
	view->opts.label_mode=		opts->label_mode;
    if (opts->present&XkbUI_ColorModeMask)
	view->opts.color_mode=		opts->color_mode;
    if (opts->present&XkbUI_WidthMask)
	view->opts.viewport.width=	opts->viewport.width;
    if (opts->present&XkbUI_HeightMask)
	view->opts.viewport.height=	opts->viewport.height;
    if (opts->present&XkbUI_XOffsetMask)
	view->opts.viewport.x=		opts->viewport.x;
    if (opts->present&XkbUI_YOffsetMask)
	view->opts.viewport.y=		opts->viewport.y;
    if (opts->present&XkbUI_MarginWidthMask)
	view->opts.margin_width=	opts->margin_width;
    if (opts->present&XkbUI_MarginHeightMask)
	view->opts.margin_height=	opts->margin_height;
    if (opts->present&XkbUI_ColormapMask) {
	view->opts.cmap=		opts->cmap;
	_XkbUI_AllocateColors(view);
    }
    return Success;
}

Status 
#if NeedFunctionPrototypes
XbUI_GetViewOpts(XkbUI_ViewPtr view,XkbUI_ViewOptsPtr opts_rtrn)
#else
XbUI_GetViewOpts(view,opts_rtrn)
	XkbUI_ViewPtr		view;
	XkbUI_ViewOptsPtr	opts_rtrn;
#endif
{
    if ((!view)||(!opts_rtrn))
	return BadValue;
    *opts_rtrn= view->opts;
    return Success;
}

Status 
#if NeedFunctionPrototypes
XkbUI_SetCanvasSize(XkbUI_ViewPtr view,int width,int height)
#else
XkbUI_SetCanvasSize(view,width,height)
	XkbUI_Viewtr	view;
	int		width;
	int		height;
#endif
{
    if ((!view)||(!view->xkb)||(!view->xkb->geom))
	return BadValue;
    view->canvas_width= width;
    view->canvas_height= height;
    view->xscale= ((double)width)/((double)view->xkb->geom->width_mm);
    view->yscale= ((double)height)/((double)view->xkb->geom->height_mm);
    return Success;
}

Status 
#if NeedFunctionPrototypes
XkbUI_GetCanvasSize(XkbUI_ViewPtr view,int *width_rtrn,int *height_rtrn)
#else
XkbUI_GetCanvasSize(view,width_rtrn,height_rtrn)
	XkbUI_ViewPtr	view;
	int *		width_rtrn;
	int *		height_rtrn;
#endif
{
    if (!view)
	return BadValue;
    if (width_rtrn)	*width_rtrn= view->canvas_width;
    if (height_rtrn)	*height_rtrn= view->canvas_height;
    return Success;
}

/***====================================================================***/

static void
#if NeedFunctionPrototypes
_RotatePoints(	double		rangle,
		int 		corner_x,
		int		corner_y,
		int		nPts,
		XkbUI_PointPtr	pts)
#else
_RotatePoints(rangle,corner_x,corner_y,nPts,pts)
	double		rangle;
	int		corner_x;
	int		corner_y;
	int		nPts;
	XkbUI_PointPtr	pts;
#endif
{
register int	i;
double		rr,rx,ry,rt;

    for (i=0;i<nPts;i++,pts++) {
	rx= pts->x-corner_x; ry= pts->y-corner_y; /* translate */
	rr= hypot(rx,ry);
	rt= atan2(ry,rx)+rangle;
	rx= rr*cos(rt);
	ry= rr*sin(rt);
	pts->x= rx+corner_x; pts->y= ry+corner_y;
    }
    return;
}

static void
#if NeedFunctionPrototypes
_DrawPoints(XkbUI_ViewPtr view,int nPts,XkbUI_PointPtr pts,XPoint *xpts)
#else
_DrawPoints(view,nPts,pts)
	XkbUI_ViewPtr	view;
	int		nPts;
	XkbUI_PointPtr	pts;
	XPoint *	xpts;
#endif
{
register int	i;

    for (i=0;i<nPts;i++) {
	if (pts[i].x>=0.0)	xpts[i].x=	pts[i].x*view->xscale+0.5;
	else			xpts[i].x=	pts[i].x*view->xscale-0.5;
	xpts[i].x+= view->opts.viewport.x;
	if (pts[i].y>=0.0)	xpts[i].y=	pts[i].y*view->yscale+0.5;
	else			xpts[i].x=	pts[i].y*view->yscale-0.5;
	xpts[i].y+= 	view->opts.viewport.y;
    }
    if ((xpts[nPts-1].x!=xpts[0].x)||(xpts[nPts-1].y!=xpts[0].y))
	xpts[nPts++]= xpts[0]; /* close the shape, if necessary */
    XDrawLines(view->dpy,view->win,view->gc,xpts,nPts,CoordModeOrigin);
XFlush(view->dpy);
    return;
}

static void
#if NeedFunctionPrototypes
_DrawSolidPoints(XkbUI_ViewPtr view,int nPts,XkbUI_PointPtr pts,XPoint *xpts)
#else
_DrawSolidPoints(view,nPts,pts)
	XkbUI_ViewPtr	view;
	int		nPts;
	XkbUI_PointPtr	pts;
	XPoint *	xpts;
#endif
{
register int	i;

    for (i=0;i<nPts;i++) {
	if (pts[i].x>=0.0)	xpts[i].x=	pts[i].x*view->xscale+0.5;
	else			xpts[i].x=	pts[i].x*view->xscale-0.5;
	xpts[i].x+= view->opts.viewport.x;
	if (pts[i].y>=0.0)	xpts[i].y=	pts[i].y*view->yscale+0.5;
	else			xpts[i].x=	pts[i].y*view->yscale-0.5;
	xpts[i].y+= 	view->opts.viewport.y;
    }
    if ((xpts[nPts-1].x!=xpts[0].x)||(xpts[nPts-1].y!=xpts[0].y))
	xpts[nPts++]= xpts[0]; /* close the shape, if necessary */
    XFillPolygon(view->dpy,view->win,view->gc,xpts,nPts,Nonconvex,
							CoordModeOrigin);
XFlush(view->dpy);
    return;
}

static void
#if NeedFunctionPrototypes
_DrawShape(	XkbUI_ViewPtr	view,
		double		rangle,
		int		xoff,
		int		yoff,
		int		rotx,
		int		roty,
		XkbShapePtr	shape,
		Bool		key)
#else
_DrawShape(view,rangle,xoff,yoff,rotx,roty,shape,key)
	XkbUI_ViewPtr	view;
	double		rangle;
	int		xoff;
	int		yoff;
	int		rotx;
	int		roty;
	XkbShapePtr	shape;
	Bool		key;
#endif
{
XkbOutlinePtr	ol;
register int	o;
int		maxPts;
XkbUI_PointPtr	uipts;
XPoint *	xpts;

    for (maxPts=4,o=0,ol=shape->outlines;o<shape->num_outlines;o++,ol++) {
	if ((shape->num_outlines>1)&&(ol==shape->approx))
	    continue;
	if (ol->num_points>maxPts)
	    maxPts= ol->num_points;
    }
    uipts= _XkbTypedCalloc(maxPts,XkbUI_PointRec);
    xpts= _XkbTypedCalloc(maxPts+1,XPoint);
    XSetForeground(view->dpy,view->gc,view->xkb->geom->label_color->pixel);
    for (o=0,ol=shape->outlines;o<shape->num_outlines;o++,ol++) {
	XkbPointPtr	gpts;
	register int 	p;
	if ((shape->num_outlines>1)&&(ol==shape->approx))
	    continue;
	gpts= ol->points;
	if (ol->num_points==1) {
	    uipts[0].x= xoff;		uipts[0].y= yoff;
	    uipts[1].x= xoff+gpts[0].x;	uipts[1].y= yoff;
	    uipts[2].x= xoff+gpts[0].x;	uipts[2].y= yoff+gpts[0].y;
	    uipts[3].x= xoff;		uipts[3].y= yoff+gpts[0].y;
	    p= 4;
	}
	else if (ol->num_points==2) {
	    uipts[0].x= xoff+gpts[0].x;	uipts[0].y= yoff+gpts[0].y;
	    uipts[1].x= xoff+gpts[1].x;	uipts[1].y= yoff+gpts[0].y;
	    uipts[2].x= xoff+gpts[1].x;	uipts[2].y= yoff+gpts[1].y;
	    uipts[3].x= xoff+gpts[0].x;	uipts[3].y= yoff+gpts[1].y;
	    p= 4;
	}
	else {
	    for (p=0;p<ol->num_points;p++) {
		uipts[p].x= xoff+gpts[p].x; 
		uipts[p].y= yoff+gpts[p].y;
	    }
	    p= ol->num_points;
	}
	if (rangle!=0.0)
	    _RotatePoints(rangle,rotx,roty,p,uipts);
	if (key) {
	    if (o==0) {
		XSetForeground(view->dpy,view->gc,
					view->xkb->geom->base_color->pixel);
		_DrawSolidPoints(view,p,uipts,xpts);
		XSetForeground(view->dpy,view->gc,
					view->xkb->geom->label_color->pixel);
	    }
	    _DrawPoints(view,p,uipts,xpts);
	}
	else {
	    _DrawPoints(view,p,uipts,xpts);
	}
    }
    _XkbFree(uipts);
    _XkbFree(xpts);
    return;
}

static void
#if NeedFunctionPrototypes
_DrawRect(	XkbUI_ViewPtr	view,
		double		rangle,
		int		x1,
		int		y1,
		int		x2,
		int		y2,
		Bool		key)
#else
_DrawRect(view,rangle,x1,y1,x2,y2,key)
	XkbUI_ViewPtr	view;
	double		rangle;
	int		x1;
	int		y1;
	int		x2;
	int		y2;
	Bool		key;
#endif
{
XkbUI_PointRec	uipts[4];
XPoint 		xpts[4];

    XSetForeground(view->dpy,view->gc,view->xkb->geom->label_color->pixel);
    uipts[0].x= x1; uipts[0].y= y1;
    uipts[1].x= x2; uipts[1].y= y1;
    uipts[2].x= x2; uipts[2].y= y2;
    uipts[3].x= x1; uipts[3].y= y2;
    if (rangle!=0.0)
	_RotatePoints(rangle,0,0,4,uipts);
    if (key) {
	XSetForeground(view->dpy,view->gc,view->xkb->geom->base_color->pixel);
	_DrawSolidPoints(view,4,uipts,xpts);
	XSetForeground(view->dpy,view->gc,view->xkb->geom->label_color->pixel);
	_DrawPoints(view,4,uipts,xpts);
    }
    else {
	_DrawPoints(view,4,uipts,xpts);
    }
    return;
}

static void
#if NeedFunctionPrototypes
_DrawDoodad(	XkbUI_ViewPtr	view,
		double		rangle,
		int		xoff,
		int		yoff,
		XkbDoodadPtr	doodad)
#else
_DrawDoodad(view,rangle,xoff,yoff,doodad)
	XkbUI_ViewPtr	view;
	double		rangle;
	int		xoff;
	int		yoff;
	XkbDoodadPtr	doodad;
#endif
{
int		x;
int		y;
XkbShapePtr	shape;
Bool		solid;

    x= 		doodad->any.left+xoff;
    y= 		doodad->any.top+yoff;
    shape=	NULL;
    solid= 	False;
    switch (doodad->any.type) {
	case XkbOutlineDoodad:
	    shape= XkbShapeDoodadShape(view->xkb->geom,(&doodad->shape));
	    break;
	case XkbSolidDoodad:
	    shape= XkbShapeDoodadShape(view->xkb->geom,(&doodad->shape));
	    solid= True;
	    break;
	case XkbTextDoodad:
	    break;
	case XkbIndicatorDoodad:
	    shape= XkbIndicatorDoodadShape(view->xkb->geom,&doodad->indicator);
	    solid= True;
	    break;
	case XkbLogoDoodad:
	    shape= XkbLogoDoodadShape(view->xkb->geom,&doodad->logo);
	    solid= True;
	    break;
    }
    if (shape)
	_DrawShape(view,rangle,x,y,x,y,shape,solid);
    return;
}

static void
#if NeedFunctionPrototypes
_DrawRow(	XkbUI_ViewPtr	view,
		double		rangle,
		int		xoff,
		int		yoff,
		XkbRowPtr	row)
#else
_DrawRow(view,rangle,xoff,yoff,row)
	XkbUI_ViewPtr	view;
	double		rangle;
	int		xoff;
	int		yoff;
	XkbRowPtr	row;
#endif
{
register int 	k,x,y;
XkbKeyPtr	key;

    x= xoff+row->left; y= yoff+row->top;
    for (k=0,key=row->keys;k<row->num_keys;k++,key++) {
	XkbShapePtr	shape;
	shape= XkbKeyShape(view->xkb->geom,key);
	if (row->vertical) {
	    y+= key->gap;
	    _DrawShape(view,rangle,x,y,xoff,yoff,shape,True);
	    y+= shape->bounds.y2;
	}
	else {
	    x+= key->gap;
	    _DrawShape(view,rangle,x,y,xoff,yoff,shape,True);
	    x+= shape->bounds.x2;
	}
    }
    return;
}

static void
#if NeedFunctionPrototypes
_DrawSection(XkbUI_ViewPtr view,XkbSectionPtr section)
#else
_DrawSection(view,section)
	XkbUI_ViewPtr	view;
	XkbSectionPtr	section;
#endif
{
double	rangle;

    rangle= ((((double)(section->angle%3600))/3600.0)*(2.0*M_PI));
    if (section->doodads) {
	XkbDrawablePtr	first,draw;
	first= XkbGetOrderedDrawables(NULL,section);
	if (first) {
	    for (draw=first;draw!=NULL;draw=draw->next) {
		_DrawDoodad(view,rangle,section->left,section->top,draw->u.doodad);
	    }
	    XkbFreeOrderedDrawables(first);
	}
    }
    if (section->rows) {
	register int	r;
	XkbRowPtr	row;
	for (r=0,row=section->rows;r<section->num_rows;r++,row++) {
	    _DrawRow(view,rangle,section->left,section->top,row);
	}
    }
    return;
}

static void
#if NeedFunctionPrototypes
_DrawAll(XkbUI_ViewPtr view)
#else
_DrawAll(view)
	XkbUI_ViewPtr	view;
#endif
{
XkbGeometryPtr	geom;
XkbDrawablePtr	first,draw;
Bool		dfltBorder;

    geom= view->xkb->geom;
    first= XkbGetOrderedDrawables(geom,NULL);
    if (first) {
	dfltBorder= True;
	for (draw=first;draw!=NULL;draw=draw->next) {
	    char *name;
	    if ((draw->type!=XkbDW_Doodad)||
		((draw->u.doodad->any.type!=XkbOutlineDoodad)&&
		 (draw->u.doodad->any.type!=XkbSolidDoodad))) {
		continue;
	    }
	    name= XkbAtomGetString(view->dpy,draw->u.doodad->any.name);
	    if ((name!=NULL)&&(_XkbStrCaseCmp(name,"edges")==0)) {
		dfltBorder= False;
		break;
	    }
	}
	if (dfltBorder)
	    _DrawRect(view,0.0,0,0,geom->width_mm,geom->height_mm,True);
	for (draw=first;draw!=NULL;draw=draw->next) {
	    switch (draw->type) {
		case XkbDW_Section:
		    _DrawSection(view,draw->u.section);
		    break;
		case XkbDW_Doodad:
		    _DrawDoodad(view,0.0,0,0,draw->u.doodad);
		    break;
	    }
	}
	XkbFreeOrderedDrawables(first);
    }	
    XFlush(view->dpy);
    return;
}

static void
#if NeedFunctionPrototypes
_RedrawKey(XkbUI_ViewPtr view,KeyCode kc)
#else
_RedrawKey(view,kc)
	XkbUI_ViewPtr	view;
	KeyCode 	kc;
#endif
{
/*    _DrawAll(view);*/
    return;
}

/***====================================================================***/

Bool 
#if NeedFunctionPrototypes
XkbUI_SetKeyAppearance(XkbUI_ViewPtr view,KeyCode kc,unsigned int flags)
#else
XkbUI_SetKeyAppearance(view,kc,flags)
	XkbUI_ViewPtr		view;
	KeyCode			kc;
	unsigned int		flags;
#endif
{
XkbDescPtr	xkb;
unsigned	old;

    if ((!view)||(!view->xkb))
	return False;
    xkb= view->xkb;
    if ((kc<xkb->min_key_code)||(kc>xkb->max_key_code))
	return False;
    old= view->state[kc];
    view->state[kc]= (flags&(~XkbUI_Obscured));
    if (old&XkbUI_Obscured) 
	view->state[kc]|= XkbUI_Obscured;
    else if (old!=view->state[kc])
	_RedrawKey(view,kc);
    return True;
}

Bool 
#if NeedFunctionPrototypes
XkbUI_SetKeyAppearanceByName(	XkbUI_ViewPtr 	view,
				XkbKeyNamePtr	name,
				unsigned int	flags)
#else
XkbUI_SetKeyAppearanceByName(view,name,flags)
	XkbUI_ViewPtr		view;
	XkbKeyNamePtr		name;
	unsigned int		flags;
#endif
{
KeyCode	kc;

    if ((!view)||(!view->xkb)||(!name))
	return False;
    kc= XkbFindKeycodeByName(view->xkb,name->name,True);
    if (!kc)
	return False;
    return XkbUI_SetKeyAppearance(view,kc,flags);
}

Bool 
#if NeedFunctionPrototypes
XkbUI_ResetKeyAppearance(	XkbUI_ViewPtr 	view,
				unsigned int 	mask,
				unsigned int 	values)
#else
XkbUI_ResetKeyAppearance(view,mask,values)
	XkbUI_ViewPtr		view;
	unsigned int		mask;
	unsigned int		values;
#endif
{
register int 	i;
unsigned	new_val;

    if ((!view)||(!view->xkb))
	return False;
    if (!mask)
	return True;
    for (i=view->xkb->min_key_code;i<=view->xkb->max_key_code;i++) {
	new_val= (view->state[i]&(~mask));
	new_val|= (mask&values);
	XkbUI_SetKeyAppearance(view,i,new_val);
    }
    return True;
}

Bool 
#if NeedFunctionPrototypes
XkbUI_DrawRegion(XkbUI_ViewPtr view,XRectangle *viewport)
#else
XkbUI_DrawRegion(view,viewport)
	XkbUI_ViewPtr		view;
	XRectangle *		viewport;
#endif
{
    if (!view) 
	return False;
    _DrawAll(view);
    return True;
}

Bool 
#if NeedFunctionPrototypes
XkbUI_DrawChanged(	XkbUI_ViewPtr	view,
			XRectangle *	viewport,
			XkbChangesPtr	changes,
			int		num_keys,
			XkbKeyNamePtr	keys)
#else
XkbUI_DrawChanged(view,viewport,changesnum_keys,keys)
	XkbUI_ViewPtr		view;
	XRectangle *		viewport;
	XkbChangesPtr		changes;
	int			num_keys;
	XkbKeyNamePtr		keys;
#endif
{
    return False;
}

Bool 
#if NeedFunctionPrototypes
XkbUI_Select(	XkbUI_ViewPtr	view,
		XPoint *	coord,
		unsigned int	which,
		XkbSectionPtr	section)
#else
XkbUI_Select(view,coord,which,section)
	XkbUI_ViewPtr		view;
	XPoint *		coord;
	unsigned int		which;
	XkbSectionPtr		section;
#endif
{
    return False;
}
