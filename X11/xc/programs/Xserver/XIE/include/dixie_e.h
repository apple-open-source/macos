/*
 * $XFree86: xc/programs/Xserver/XIE/include/dixie_e.h,v 1.1 1998/10/25 07:11:45 dawes Exp $
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
 * Interfaces of XIE/dixie/export
 */

#ifndef _XIEH_DIXIE_EXPORT
#define _XIEH_DIXIE_EXPORT 1

#include <X.h>
#include <XIE.h>
#include <XIEproto.h>		/* declares xieFloEvn */
#include <misc.h>
#include <flostr.h>
#include <dixstruct.h>

/* echist.h */
extern peDefPtr MakeECHistogram(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* eclut.c */
extern peDefPtr MakeECLUT(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* ecphoto.h */
peDefPtr MakeECPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* ecroi.c */
extern peDefPtr MakeECROI(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* edraw.c */
extern peDefPtr MakeEDraw(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);
extern Bool DrawableAndGC(
			floDefPtr flo,
			peDefPtr ped,
			Drawable draw_id,
			GContext gc_id,
			DrawablePtr *draw_ret,
			GCPtr *gc_ret);

/* edrawp.c */
extern peDefPtr MakeEDrawPlane(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* elut.h */
extern peDefPtr MakeELUT(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* ephoto.h */
extern peDefPtr MakeEPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);
extern Bool BuildDecodeFromEncode(floDefPtr flo, peDefPtr ped);
extern Bool CompareDecode(floDefPtr flo, peDefPtr ped);

/* eroi.c */
extern peDefPtr MakeEROI(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

#endif /* _XIEH_DIXIE_EXPORT */
