/*
 * $XFree86: xc/programs/Xserver/XIE/include/dixie_i.h,v 1.1 1998/10/25 07:11:45 dawes Exp $
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
 * Interfaces of XIE/dixie/import
 */

#ifndef _XIEH_DIXIE_IMPORT
#define _XIEH_DIXIE_IMPORT 1

#include <X.h>

#include <XIE.h>
#include <XIEproto.h>		/* declares xieFloEvn */
#include <misc.h>
#include <flostr.h>
#include <dixstruct.h>
#include <photomap.h>

/* iclut.h */
extern peDefPtr MakeICLUT(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* icphoto.h */
extern peDefPtr MakeICPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* icroi.h */
extern peDefPtr MakeICROI(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* idraw.h */
extern peDefPtr MakeIDraw(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* idrawp.h */
extern peDefPtr MakeIDrawP(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* ilut.h */
extern peDefPtr MakeILUT(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/* iphoto.h */
extern peDefPtr MakeIPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);
extern photomapPtr GetImportPhotomap(peDefPtr ped);
extern pointer GetImportTechnique(peDefPtr ped, CARD16 *num_ret, CARD16 *len_ret);

/* iroi.h */
extern peDefPtr MakeIROI(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

#endif /* _XIEH_DIXIE_IMPORT */
