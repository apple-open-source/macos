/* $Xorg: pexLookup.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

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

#ifndef PEX_LOOKUP_H
#define PEX_LOOKUP_H	1

#include "resource.h"
#include "dipex.h"

/*	PEX types	*/

#define LU_PEXFONT(id, ptr) \
    if (! ((ptr) = (diFontHandle) LookupIDByType ((id), PEXFontType)) ) { \
	err = PEX_ERROR_CODE(PEXFontError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_TABLE(id, ptr) \
    if (! ((ptr) = (diLUTHandle) LookupIDByType ((id), PEXLutType)) ) { \
	err = PEX_ERROR_CODE(PEXLookupTableError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_NAMESET(id,ns_handle) \
    if (! (ns_handle = (diNSHandle) LookupIDByType ((id), PEXNameType)) ) { \
	err = PEX_ERROR_CODE(PEXNameSetError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_PHIGSWKS(id,ptr) \
    if (! ((ptr) = (dipexPhigsWks *) LookupIDByType ((id), PEXWksType)) ) { \
	err = PEX_ERROR_CODE(PEXPhigsWksError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_PICKMEASURE(id,ptr) \
    if (! ((ptr) = (diPMHandle) LookupIDByType ((id), PEXPickType)) ) { \
	err = PEX_ERROR_CODE(PEXPickMeasureError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_PIPELINECONTEXT(id,ptr) \
    if (! ((ptr) = (ddPCStr *) LookupIDByType ((id), PEXPipeType)) ) { \
	err = PEX_ERROR_CODE(PEXPipelineContextError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_RENDERER(id, ptr) \
    if (! ((ptr) = (ddRendererStr *)LookupIDByType((id), PEXRendType)) ) { \
	err = PEX_ERROR_CODE(PEXRendererError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_SEARCHCONTEXT(id,ptr) \
    if (! ((ptr) = (ddSCStr *) LookupIDByType ((id), PEXSearchType)) ) { \
	err = PEX_ERROR_CODE(PEXSearchContextError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }

#define LU_STRUCTURE(id,ptr) \
    if (! ((ptr) = (diStructHandle) LookupIDByType ((id), PEXStructType))) {\
	err = PEX_ERROR_CODE(PEXStructureError); \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }


/*	Other useful types	*/

#define LU_DRAWABLE(id,ptr) \
    if (!((ptr) = (DrawablePtr) LookupIDByClass ((id), RC_DRAWABLE))) { \
	err = BadDrawable; \
	PEX_ERR_EXIT(err,(id),cntxtPtr); }


#endif
