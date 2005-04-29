/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#include "render.h"
#include "colortbl.h"


static void hsv2rgb(double h, double s, double v, double *r, double *g, double *b)
{
	int i;
	double f,p,q,t;

	if (s <= 0.0) {	/* achromatic */
		*r = v;
		*g = v;
		*b = v;
	}
	else {
		if (h >= 1.0) h = 0.0;
		h = 6.0 * h;
		i = (int)h;
		f = h - (double)i;
		p = v * (1 - s);
		q = v * (1 - (s * f));
		t = v * ( 1 - (s * (1 - f)));
		switch (i) {
			case 0: *r = v; *g = t; *b = p; break;
			case 1: *r = q; *g = v; *b = p; break;
			case 2: *r = p; *g = v; *b = t; break;
			case 3: *r = p; *g = q; *b = v; break;
			case 4: *r = t; *g = p; *b = v; break;
			case 5: *r = v; *g = p; *b = q; break;
		}
	}
}

static void rgb2hsv(double r, double g, double b, double *h, double *s, double *v)
{

	double		rgbmin,rgbmax;
	double		rc,bc,gc;
	double		ht=0.0,st=0.0;

	rgbmin = MIN(r,MIN(g,b));
	rgbmax = MAX(r,MAX(g,b));

	if (rgbmax > 0.0)
		st = (rgbmax - rgbmin) / rgbmax;

	if (st > 0.0) {
		rc = (rgbmax - r) / (rgbmax - rgbmin);
		gc = (rgbmax - g) / (rgbmax - rgbmin);
		bc = (rgbmax - b) / (rgbmax - rgbmin);
		if (r == rgbmax) ht = bc - gc;
		else if (g == rgbmax) ht = 2 + rc - bc;
		else if (b == rgbmax) ht = 4 + gc - rc;
		ht = ht * 60.0;
		if (ht < 0.0) ht += 360.0;
	}
	*h = ht / 360.0;
	*v = rgbmax;
	*s = st;
}

static void rgb2cmyk(double r, double g, double b, double *c, double *m, double *y, double *k)
{
	*c = 1.0 - r;
	*m = 1.0 - g;
	*y = 1.0 - b;
	*k = *c < *m ? *c : *m;
	*k = *y < *k ? *y : *k;
	*c -= *k;
	*m -= *k;
	*y -= *k;
}

static int colorcmpf(void *p0, void *p1)
{
	/* fast comparison of first character */
	int	i = (((hsbcolor_t*)p0)->name[0] - ((hsbcolor_t*)p1)->name[0]);
	/* if first character matches then compare full color string */
	return (i ? i : strcmp(((hsbcolor_t*)p0)->name,((hsbcolor_t*)p1)->name));
}

char *canontoken(char *str)
{
	static char canon[SMALLBUF];
	char c;
	char *p=str;
	char *q=canon;
	int i=SMALLBUF;

	while ((c = *p++) && (--i)) {
		if (isalnum(c) == FALSE) continue;
		if (isupper(c)) c = tolower(c);
		*q++ = c;
	}
    if (c) agerr (AGWARN, "color value '%s' truncated\n", str);
	*q = '\0';
	return canon;
}

void colorxlate(char *str, color_t *color, color_type target_type)
{
	static hsbcolor_t	*last;
	hsbcolor_t	fake;
	char		canon[SMALLBUF];
	char		*p, *q, ch;
	double		H,S,V,R,G,B;
	double		C,M,Y,K;
	int		r,g,b,a;
	int		i;

	color->type = target_type;

	/* skip over any leading whitespace */
	for (; *str == ' '; str++);
    p = str;

	/* test for rgb value such as: "#ff0000" or rgba value such as "#ff000080" */
	a = 255; /* default alpha channel value in case not supplied */
	if ((*p == '#') && (sscanf(p,"#%2x%2x%2x%2x",&r,&g,&b,&a) >= 3)) {
		switch (target_type) {
		case HSV_DOUBLE:
			R = (double)r/255.0;
			G = (double)g/255.0;
			B = (double)b/255.0;
			rgb2hsv(R,G,B,&H,&S,&V);
			color->u.HSV[0] = H;
			color->u.HSV[1] = S;
			color->u.HSV[2] = V;
			break;
		case RGBA_BYTE:
			color->u.rgba[0] = r;
			color->u.rgba[1] = g;
			color->u.rgba[2] = b;
			color->u.rgba[3] = a;
			break;
		case CMYK_BYTE:
			R = (double)r/255.0;
			G = (double)g/255.0;
			B = (double)b/255.0;
			rgb2cmyk(R,G,B,&C,&M,&Y,&K);
			color->u.cmyk[0] = (int)C*255;
			color->u.cmyk[1] = (int)M*255;
			color->u.cmyk[2] = (int)Y*255;
			color->u.cmyk[3] = (int)K*255;
			break;
		case RGBA_WORD:
			color->u.rrggbbaa[0] = r*65535/255;
			color->u.rrggbbaa[1] = g*65535/255;
			color->u.rrggbbaa[2] = b*65535/255;
			color->u.rrggbbaa[3] = a*65535/255;
			break;
		}
		/* color->type = target_type; */
		return; 
	}

	/* test for hsv value such as: ".6,.5,.3" */
    if (((ch = *p) == '.') || isdigit(ch)) {
	    q = canon;
	    i = SMALLBUF;
	    while ((ch = *p++) && (--i)) {
            if (ch == ',') ch = ' ';
	        *q++ = ch;
	    }
        if (ch) agerr (AGWARN, "color value '%s' truncated\n", str);
	    *q = '\0';
    
	    if (sscanf(canon,"%lf%lf%lf",&H,&S,&V) == 3) {
		    /* clip to reasonable values */
		    H = MAX(MIN(H,1.0),0.0);
		    S = MAX(MIN(S,1.0),0.0);
		    V = MAX(MIN(V,1.0),0.0);
		    switch (target_type) {
		    case HSV_DOUBLE:
			    color->u.HSV[0] = H;
			    color->u.HSV[1] = S;
			    color->u.HSV[2] = V;
			    break;
		    case RGBA_BYTE:
			    hsv2rgb(H,S,V,&R,&G,&B);
			    color->u.rgba[0] = (int)(R*255);
			    color->u.rgba[1] = (int)(G*255);
			    color->u.rgba[2] = (int)(B*255);
			    color->u.rgba[3] = 255;
			    break;
		    case CMYK_BYTE:
			    hsv2rgb(H,S,V,&R,&G,&B);
			    rgb2cmyk(R,G,B,&C,&M,&Y,&K);
			    color->u.cmyk[0] = (int)C*255;
			    color->u.cmyk[1] = (int)M*255;
			    color->u.cmyk[2] = (int)Y*255;
			    color->u.cmyk[3] = (int)K*255;
			    break;
		    case RGBA_WORD:
			    hsv2rgb(H,S,V,&R,&G,&B);
			    color->u.rrggbbaa[0] = (int)(R*65535);
			    color->u.rrggbbaa[1] = (int)(G*65535);
			    color->u.rrggbbaa[2] = (int)(B*65535);
			    color->u.rrggbbaa[3] = 65535;
			    break;
		    }
		/* color->type = target_type; */
		    return;
	    }
	}

	/* test for known color name */
	fake.name = canontoken(str);
	if ((last == NULL)
	  ||(last->name[0] != fake.name[0])
	  ||(strcmp(last->name,fake.name))) {
		last = (hsbcolor_t*) bsearch((void*)&fake,
				(void*)color_lib,
				sizeof(color_lib)/sizeof(hsbcolor_t),
				sizeof(fake),(bsearch_cmpf)colorcmpf);
	}
	if (last != NULL) {
		switch (target_type) {
		case HSV_DOUBLE:
			color->u.HSV[0] = ((double)last->h)/255.0;
			color->u.HSV[1] = ((double)last->s)/255.0;
			color->u.HSV[2] = ((double)last->b)/255.0;
			break;
		case RGBA_BYTE:
			H = (last->h)/255.0;
			S = (last->s)/255.0;
			V = (last->b)/255.0;
			hsv2rgb(H,S,V,&R,&G,&B);
			color->u.rgba[0] = (int)(R*255);
			color->u.rgba[1] = (int)(G*255);
			color->u.rgba[2] = (int)(B*255);
			color->u.rgba[3] = 255;
			break;
		case CMYK_BYTE:
			H = (last->h)/255.0;
			S = (last->s)/255.0;
			V = (last->b)/255.0;
			hsv2rgb(H,S,V,&R,&G,&B);
			rgb2cmyk(R,G,B,&C,&M,&Y,&K);
			color->u.cmyk[0] = (int)C*255;
			color->u.cmyk[1] = (int)M*255;
			color->u.cmyk[2] = (int)Y*255;
			color->u.cmyk[3] = (int)K*255;
			break;
		case RGBA_WORD:
			H = (last->h)/255.0;
			S = (last->s)/255.0;
			V = (last->b)/255.0;
			hsv2rgb(H,S,V,&R,&G,&B);
			color->u.rrggbbaa[0] = (int)(R*65535);
			color->u.rrggbbaa[1] = (int)(G*65535);
			color->u.rrggbbaa[2] = (int)(B*65535);
			color->u.rrggbbaa[3] = 65535;
			break;
		}
		/* color->type = target_type; */
		return;
	}

	/* if we're still here then we failed to find a valid color spec */
	agerr (AGWARN, "%s is not a known color. Using black.\n",str);
	switch (target_type) {
	case HSV_DOUBLE:
		color->u.HSV[0] =
		       	color->u.HSV[1] =
		       	color->u.HSV[2] = 0.0;
		break;
	case RGBA_BYTE:
		color->u.rgba[0] =
		       	color->u.rgba[1] =
		       	color->u.rgba[2] = 0;
		color->u.rgba[3] = 255;
		break;
	case CMYK_BYTE:
		color->u.cmyk[0] = 
			color->u.cmyk[1] = 
			color->u.cmyk[2] = 
			color->u.cmyk[3] = 0;
		break;
	case RGBA_WORD:
		color->u.rrggbbaa[0] =
		       	color->u.rrggbbaa[1] =
		       	color->u.rrggbbaa[2] = 0;
		color->u.rrggbbaa[3] = 65535;
		break;
	}
	/* color->type = target_type; */
	return;
}
