/* $Xorg: miFont.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/include/miFont.h,v 1.4 2001/12/14 19:57:10 dawes Exp $ */
/* 
 * font internal format
 */
#ifndef MI_FONT_H
#define MI_FONT_H

#include "ddpex.h"
#include "ddpex3.h"
#include "miRender.h"
#include "PEXprotost.h"

#define START_PROPS 0x100
#define START_DISPATCH(_num_props)  (START_PROPS + 160 * _num_props)
#define START_PATH(_num_ch_, _num_props)  (START_DISPATCH(_num_props) + sizeof(Dispatch) * _num_ch_)
#define NUM_DISPATCH    128

#ifndef PADDING
#define PADDING(n) ( (n)%4 ? (4 - (n)%4) : 0)
#endif /* PADDING */

/* definitions in the local font coordinate system */
#define FONT_COORD_HEIGHT   100.0
#define FONT_COORD_BASE	    0.0
#define FONT_COORD_CAP	    100.0
#define FONT_COORD_HALF	    50.0

typedef enum {
    PATH_2D=0,
    PATH_3D=1,
    PATH_4D=2
} Font_path_type;

typedef struct {
    Font_path_type  type;
    ddFLOAT	    center;
    ddFLOAT	    right;
    ddULONG	    n_vertices;
    miListHeader    strokes;
} Ch_stroke_data;

typedef enum {
    FONT_POLYLINES=0,
    FONT_SPLINES=1,
    FONT_POLYGONS=2
} Font_type;

typedef struct {
    Font_type		font_type;
    char		name[80];
    unsigned long	num_ch;
    float		top, bottom, max_width;
    Ch_stroke_data    **ch_data;    /* list of *Ch_stroke_data, one per char */
    pexFontInfo		font_info;
    pexFontProp	       *properties; /* list of associated properties */
    int		       lutRefCount;/* number of LUTs referencing this font */
    int			freeFlag;   /* should this be freed when no more refs */
} miFontHeader;

typedef struct {
    ddFLOAT    top, bottom, width;
} Meta_font;

typedef struct {
  char      propname[80];
  char      propvalue[80];
}Property;

typedef struct
{
    int     magic;	  /* magic number */
    char    name[80];     /* name of this font */
    float   top,	  /* extreme values */
            bottom,
            max_width;
    int     num_ch;       /* no. of fonts in the set */
    int     num_props;    /* no. of font properties */
    Property *properties; /* array of properties */
} Font_file_header;

typedef struct
{
    float   center,	  /* center of the character */
            right;	  /* right edge */
    long    offset;	  /* offset in the file of the character
			   * description */
} Dispatch;

#endif	/* MI_FONT_H */
