/* $Xorg: miFontLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
/*

Copyright 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1990, 1991 by Sun Microsystems, Inc. 
All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miFontLUT.c,v 1.9 2001/12/14 19:57:38 dawes Exp $ */

#include "miLUT.h"
#include "miWks.h"
#include "miInfo.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/*  Level 4 Shared Resources  */
/* Lookup Table Procedures */

extern	diFontHandle	defaultPEXFont;
extern	unsigned	colour_type_sizes[];	/* in miLUT.c */

/* definitions used by miLUTProcs.ci */
#define	LUT_TYPE	PEXTextFontLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddTextFontEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miTextFontEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexTextFontEntry

#define LUT_REND_DYN_BIT	PEXDynTextFontTableContents

#define LUT_START_INDEX          1
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  2
#define LUT_0_NUM_PREDEFINED     1
#define LUT_0_PREDEFINED_MIN     1
#define LUT_0_PREDEFINED_MAX     1

#define LUT_TABLE_START(pheader)	(pheader)->plut.font

/* no	DYNAMIC		*/

/* predefined entries table: change this to work with your devPriv data */
static	DD_LUT_ENTRY_STR	pdeTextFontEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeTextFontEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)		\
        (pentry)->entry = *(pdeentry);

/* predefined entry 0 is set to the default values
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeTextFontEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
	(pentry)->entry = LUT_DEFAULT_VALUES

/* which procedure definitions in miLUTProcs.h to use and their names
 * take out USE flags if you're defining those procs in here
 * but leave the name definitions
 */

#define LUT_USE_FREE
#define LUT_USE_INQ_PREDEF
#define LUT_USE_INQ_ENTRIES
#define LUT_USE_COPY
#define LUT_USE_INQ_INFO
#define LUT_USE_INQ_IND
#define LUT_USE_INQ_ENTRY
#define LUT_USE_SET_ENTRIES
#define LUT_USE_DEL_ENTRIES
#define LUT_USE_INQ_ENTRY_ADDRESS
#define LUT_USE_CREATE
#define LUT_USE_MOD_CALL_BACK

/* these three are redefined in miLUTProcs.h
#define LUT_FREE	TextFontLUT_free
#define LUT_INQ_PREDEF	TextFontLUT_inq_predef
#define LUT_INQ_ENTRIES	TextFontLUT_inq_entries
*/
#define LUT_COPY	TextFontLUT_copy
#define LUT_INQ_INFO	TextFontLUT_inq_info
#define LUT_INQ_IND	TextFontLUT_inq_ind
#define LUT_INQ_ENTRY	TextFontLUT_inq_entry
#define LUT_SET_ENTRIES	TextFontLUT_set_entries
#define LUT_DEL_ENTRIES	TextFontLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	TextFontLUT_inq_entry_address
#define LUT_CREATE	TextFontLUT_create
#define LUT_ENTRY_CHECK	TextFontLUT_entry_check
#define LUT_COPY_PEX_MI	TextFontLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	TextFontLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	TextFontLUT_mod_call_back

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
        miLUTHeader             *pheader;
	ddUSHORT                valueType;
	MI_LUT_ENTRY_STR	*pentry;
	ddPointer		*ppbuf;
{
	ddPointer	pb = *ppbuf;
	DD_LUT_ENTRY_STR	*pdev_entry;

	if (pentry == NULL)
		pdev_entry = &(LUT_DEFAULT_VALUES);
	else if (pentry->entry_info.status == MILUT_UNDEFINED)
		pdev_entry = &(LUT_DEFAULT_VALUES);
	else
		pdev_entry = &pentry->entry;


	mibcopy(pdev_entry, pb, sizeof(PEX_LUT_ENTRY_STR));

	pb+= sizeof(PEX_LUT_ENTRY_STR);

	mibcopy(pdev_entry->fonts, pb, pdev_entry->numFonts * sizeof(pexFont));

	pb += pdev_entry->numFonts * sizeof(pexFont);

	*ppbuf = pb;
	return(Success);
}

/* copy from a pex entry to an mi entry and increment ppsrc */
ddpex43rtn
LUT_COPY_PEX_MI ( pheader, ppsrc, pentry )	
        miLUTHeader             *pheader;
	ddPointer		*ppsrc;
	MI_LUT_ENTRY_STR	*pentry;
{
	ddPointer	ps = *ppsrc;
	diFontHandle	*psfonts, *pdfonts;
	register int	j;

	mibcopy(ps, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));

	ps+= sizeof(PEX_LUT_ENTRY_STR);

	for (psfonts = (diFontHandle *)ps, pdfonts = pentry->entry.fonts, j = 0;
		j < pentry->entry.numFonts; j++, psfonts++, pdfonts++)
			*pdfonts = *psfonts;
	for (; j < MILUT_MAX_CS_PER_ENTRY;  j++, pdfonts++)
			*pdfonts = defaultPEXFont;

	*ppsrc = (ddPointer)psfonts;

	return(Success);
}

/* check for bad values and increment ppPexEntry */

ddpex43rtn
LUT_ENTRY_CHECK (pheader, ppPexEntry)
	miLUTHeader		*pheader;
	PEX_LUT_ENTRY_STR	**ppPexEntry;
{
	ddPointer	pe = (ddPointer)*ppPexEntry;

	/* numFonts: imp. dep limitation */
	if ((*ppPexEntry)->numFonts > MILUT_MAX_CS_PER_ENTRY)
		return(BadValue);
	/* dipex should have looked up font id's and made them handles
	 * and would have found any bad ids */

	pe += sizeof(PEX_LUT_ENTRY_STR) + 
		(*ppPexEntry)->numFonts * sizeof(pexFont);

	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;
	return(Success);
}


void
TextFontLUT_init_pde()
{
    register int i;

    pdeTextFontEntry[0].numFonts = 1;
    pdeTextFontEntry[0].fonts[0] = defaultPEXFont;
    /* fill in the rest of this with this font, since we want 'undefined'
     * character sets to default to the default font (0) in the font group. */
    for (i = 1; i < MILUT_MAX_CS_PER_ENTRY; i++)
        pdeTextFontEntry[0].fonts[i] = defaultPEXFont;
}

#include "miLUTProcs.ci"
