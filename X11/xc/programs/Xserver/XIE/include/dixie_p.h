/*
 * $XFree86: xc/programs/Xserver/XIE/include/dixie_p.h,v 1.1 1998/10/25 07:11:45 dawes Exp $
 */

/************************************************************

Copyright 1998 by Thomas E. Dickey <dickey@clark.net>

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/*
 * Interfaces of XIE/dixie/process
 */

#ifndef _XIEH_DIXIE_PROCESS
#define _XIEH_DIXIE_PROCESS 1

#include <X.h>
#include <XIE.h>
#include <XIEproto.h>		/* declares xieFloEvn */
#include <misc.h>
#include <flostr.h>
#include <dixstruct.h>

/* parith.c */
extern peDefPtr MakeArith(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pbandc.c */
extern peDefPtr MakeBandCom(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pbande.c */
extern peDefPtr MakeBandExt(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pbands.c */
extern peDefPtr MakeBandSel(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pblend.c */
extern peDefPtr MakeBlend(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pcfrgb.c */
extern peDefPtr MakeConvertFromRGB(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pcfromi.c */
extern peDefPtr MakeConvertFromIndex(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pcnst.c */
extern peDefPtr MakeConstrain(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pcomp.c */
extern peDefPtr MakeCompare(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pconv.c */
extern peDefPtr MakeConvolve(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pctoi.c */
extern peDefPtr MakeConvertToIndex(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pctrgb.c */
extern peDefPtr MakeConvertToRGB(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);
extern void copy_floats(double *doubles_out, xieTypFloat *funny_floats_in, int cnt);
extern void swap_floats(double *doubles_out, xieTypFloat *funny_floats_in, int cnt);

/* pdither.c */
extern peDefPtr MakeDither(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pgeom.c */
extern peDefPtr MakeGeometry(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* phist.c */
extern peDefPtr MakeMatchHistogram(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* plogic.c */
extern peDefPtr MakeLogic(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* pmath.c */
extern peDefPtr MakeMath(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);
 
/* ppaste.c */
extern peDefPtr MakePasteUp(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* ppoint.c */
extern peDefPtr MakePoint(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* puncst.c */
extern peDefPtr MakeUnconstrain(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

#endif /* _XIEH_DIXIE_PROCESS */
