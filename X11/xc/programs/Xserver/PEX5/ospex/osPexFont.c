/* $Xorg: osPexFont.c,v 1.4 2001/02/09 02:04:19 xorgcvs Exp $ */

/*

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

*/
/* $XFree86: xc/programs/Xserver/PEX5/ospex/osPexFont.c,v 3.19 2002/05/31 18:45:53 dawes Exp $ */

#ifdef WIN32
#define _WILLWINSOCK_
#endif

#include "mipex.h"
#include "miFont.h"
#include "PEXErr.h"
#define XK_LATIN1
#include "keysymdef.h"
#define NEED_GETENV
#define NEED_OS_LIMITS
#include "pexos.h"

#ifndef PEX_DEFAULT_FONTPATH
#define PEX_DEFAULT_FONTPATH "/usr/lib/X11/fonts/PEX"
#endif

#ifndef WIN32

#ifndef XFree86LOADER

#if !defined(X_NOT_POSIX) || defined(SYSV) || defined(__CYGWIN__) || defined(USG)
#include <dirent.h>
#else
#include <sys/dir.h>
#ifndef dirent
#define dirent direct
#endif
#endif
typedef struct dirent	 ENTRY;

#ifndef FILENAME_MAX
#ifdef MAXNAMLEN
#define FILENAME_MAX MAXNAMLEN
#else
#define FILENAME_MAX 255
#endif
#endif

#else /* XFree86LOADER */

/* XXX This should be taken care of elsewhere */
typedef struct _xf86dirent ENTRY;

#endif /* XFree86LOADER */

#define FileName(file) file->d_name

#else  /* WIN32 */

#define BOOL wBOOL
#define ATOM wATOM
#include <windows.h>
#undef BOOL
#undef ATOM
#define FileName(file) file.cFileName

#endif /* WIN32 */

extern void CopyISOLatin1Lowered();
extern int get_lowered_truncated_entry();

void ClosePEXFontFile();
void SetPEXFontFilePtr();

extern  diFontHandle defaultPEXFont;

/*
 * Unless an environment variable named PEX_FONTPATH is set before the
 * server is started up, PEX will look in the path defined in the
 * PEX_DEFAULT_FONTPATH compiler constant defined in miFont.h for PEX fonts.
 * If environment variable PEX_FONTPATH is defined, then this will
 * be used as the path to the fonts .
 */
 
static char *
pex_get_font_directory_path()
{
    static int	 already_determined = 0;
    static char *font_dir_path = NULL;
    
    if (!already_determined) {
	if (getenv("PEX_FONTPATH")) {
	    font_dir_path = 
	       (char *)xalloc((unsigned long)(1+strlen(getenv("PEX_FONTPATH"))));
	    strcpy(font_dir_path, getenv("PEX_FONTPATH"));
	} else {
#ifndef __UNIXOS2__
	    font_dir_path =
		(char *)xalloc((unsigned long)(1+strlen(PEX_DEFAULT_FONTPATH)));
	    strcpy(font_dir_path, PEX_DEFAULT_FONTPATH);
#else
	    char *p = (char*)__XOS2RedirRoot(PEX_DEFAULT_FONTPATH);
	    font_dir_path =
		(char *)xalloc((unsigned long)(1+strlen(p)));
	    strcpy(font_dir_path, p);
#endif
	}
	already_determined = 1;
    }
    
    return (font_dir_path);
}


/*
 * The next two functions (pex_setup_wild_match() and pex_is_matching()) are 
 * stolen (and slightly modified) from MIT X11R4 fonts/mkfontdir/fontdir.c.
 * pex_setup_wild_match() sets up some state about the pattern to match, which
 * pex_is_matching() then uses.
 */
 
 
/* results of this function are used by pex_is_matching() */

static void
pex_setup_wild_match(pat, phead, ptail, plen)
char	*pat;			/* in */
int	*phead, *ptail, *plen;	/* out */
{
    register int head, tail;
    register char c, *firstWild;

    *plen = tail = strlen(pat);
    for (   firstWild = pat; 
	    ((c = *firstWild) && !((c == XK_asterisk) || (c == XK_question)));
            firstWild++)
	;
	    
    head = firstWild - pat;

    while ((c = pat[head]) && (c != XK_asterisk))
        head++;
    if (head < tail)
    {
        while (pat[tail-1] != XK_asterisk)
            tail--;
    }
    *phead = head;
    *ptail = tail;
}

/* returns value greater than 0 if successful.  head, tail, and plen
 * come from a previous call to pex_setup_wild_match 
 */
static int 
pex_is_matching(string, pat, head, tail, plen)
register char       *string;		/* in */
register char       *pat;		/* in */
int                 head, tail, plen;	/* in */
{
    register int i, l;
    int j, m, res;
    register char cp, cs;
 
    res = -1;
    for (i = 0; i < head; i++)
    {
        cp = pat[i];
        if (cp == XK_question)
        {
            if (!string[i])
                return res;
            res = 0;
        }
        else if (cp != string[i])
            return res;
    }
    if (head == plen)
        return (string[head] ? res : 1);
    l = head;
    while (++i < tail)
    {
        /* we just skipped an asterisk */
        j = i;
        m = l;
        while ((cp = pat[i]) != XK_asterisk)
        {
            if (!(cs = string[l]))
                return 0;
            if ((cp != cs) && (cp != XK_question))
            {
                m++;
                cp = pat[j];
                if (cp == XK_asterisk)
                {
                    if (!string[m])
                        return 0;
                }
                else
                {
                    while ((cs = string[m]) != cp)
                    {
                        if (!cs)
                            return 0;
                        m++;
                    }
                }
                l = m;
                i = j;
            }
            l++;
            i++;
        }
    }
    m = strlen(&string[l]);
    j = plen - tail;
    if (m < j)
        return 0;
    l = (l + m) - j;
    while (cp = pat[i])
    {
        if ((cp != string[l]) && (cp != XK_question))
            return 0;
        l++;
        i++;
    }
    return 1;
}

/*
 * Caller is responsible for freeing contents of buffer and buffer when
 * done with it.
 */
#define ABSOLUTE_MAX_NAMES 200

int
pex_get_matching_names(patLen, pPattern, maxNames, numNames, names)
ddUSHORT   patLen;		/* in */
ddUCHAR	  *pPattern;		/* in */
ddUSHORT   maxNames;		/* in */
ddULONG   *numNames;		/* out - number of names found */
char    ***names;		/* out - pointer to list of strings */
{
#ifdef WIN32
    HANDLE		fontdirh;
    WIN32_FIND_DATA	dir_entry;
    char	        path[MAX_PATH];
#else
    DIR		    *fontdir;
    ENTRY           *dir_entry;
#endif
    char	     *pattern;
    char	     entry[PATH_MAX+1];
    int		     i, head, tail, len, total = 0;
    
    if (!(pattern = (char *)xalloc((unsigned long)(1 + patLen))))
	return 0;

    CopyISOLatin1Lowered((unsigned char*)pattern, pPattern, patLen);
    
    if (!(*names = (char **)xalloc((unsigned long)(ABSOLUTE_MAX_NAMES * sizeof(char *)))))
	return 0;
    
#ifdef WIN32
    sprintf(path, "%s/*.*", pex_get_font_directory_path());
    if ((fontdirh = FindFirstFile(path, &dir_entry)) == INVALID_HANDLE_VALUE) {
	xfree(*names);
	xfree(pattern);
	return 0;
    }
#else
    if (!(fontdir = opendir(pex_get_font_directory_path()))) {
	xfree(*names);
	xfree(pattern);
	return 0;
    }
#endif

    pex_setup_wild_match(pattern, &head, &tail, &len);
    
#ifdef WIN32
    do
#else
    while (total < maxNames && (dir_entry = readdir(fontdir)))
#endif
	{
	
	    if (!get_lowered_truncated_entry(FileName(dir_entry), entry))
		continue;

	    if (pex_is_matching(entry, pattern, head, tail, len) > 0) {
	    
		if (!( (*names)[total] = (char *)xalloc((unsigned long)(1 + strlen(entry))))) {
		    for (i = 0; i < total; i++)
			xfree((*names)[i]);
		    xfree(*names);
		    xfree(pattern);
		    return 0;
		}
		
		strcpy((*names)[total], entry);
		total++;
	    }
	}
#ifdef WIN32
    while (total < maxNames && FindNextFile(fontdirh, &dir_entry));
#endif

#ifdef WIN32
    FindClose(fontdirh);
#else
    closedir(fontdir);
#endif
    
    *numNames = total;
    
    return 1;
}

/*
 * get_stroke(stroke, fp) extracts the definition of characters
 * from the font file.  It return -1 if anything goes wrong, 0 if
 * everything is OK.  
 */

static int
get_stroke(stroke, fp)
    Ch_stroke_data     *stroke;
    FILE               *fp;
{
    listofddPoint	   *spath;
    register int	    i;
    unsigned long	    closed;	/* placeholder, really unused */
    register ddULONG	    npath;
    register miListHeader  *hdr = &(stroke->strokes);

    stroke->n_vertices = 0;
    npath = hdr->maxLists = hdr->numLists;
    hdr->type = DD_2D_POINT;
    hdr->ddList = spath = (listofddPoint *)
	xalloc((unsigned long)(sizeof(listofddPoint) * npath));
	
    if (spath == NULL)
	return -1;

    for (i = 0; i < npath; i++, spath++)
	spath->pts.p2Dpt = NULL;

    for (i = 0, spath = hdr->ddList; i < npath; i++, spath++) {
    
	/* for each subpath of the character definition ... */
	
	if (fread((char *) &spath->numPoints,
		  sizeof(spath->numPoints), 1, fp) != 1 ||
	    fread((char *) &closed, sizeof(closed), 1, fp) != 1)
	    return -1;

	if (spath->numPoints <= 0)
	    continue;

	spath->maxData = sizeof(ddCoord2D) * spath->numPoints;
	
	if (!(spath->pts.p2Dpt = (ddCoord2D *) xalloc((unsigned long)(spath->maxData))))
	    return -1;
	    
	if (fread((char *)spath->pts.p2Dpt, sizeof(ddCoord2D), 
		  spath->numPoints, fp) != spath->numPoints)
	    return -1;
	    
	stroke->n_vertices += spath->numPoints;
    }
    return 0;
}



/*
    read in the pex font
 */
ErrorCode
LoadPEXFontFile(length, fontname, pFont)
    unsigned	    length;
    char *	    fontname;
    diFontHandle    pFont;
{
    char                fname[FILENAME_MAX+1];
    FILE               *fp;
    Font_file_header    header;
    Property           *properties = 0;
    Dispatch           *table = 0;
    miFontHeader       *font = (miFontHeader *)(pFont->deviceData);
    int                 found_first, found_it = 0, err = Success, numChars, np;
    char		*name_to_match;
    char		lowered_entry[PATH_MAX+1];
#ifdef WIN32
    HANDLE		fontdirh;
    WIN32_FIND_DATA	dir_entry;
    char	        path[MAX_PATH];
#else
    DIR		       *fontdir;
    ENTRY              *dir_entry;
#endif
    register int        i;
    register Ch_stroke_data **ch_font, *ch_stroke = 0;
    register Dispatch  *tblptr = 0;
    register Property  *propptr = 0;
    register pexFontProp *fpptr = 0;

    if (!(name_to_match = (char *)xalloc((unsigned int)(1 + length))))
	return (PEXERR(PEXFontError));

    CopyISOLatin1Lowered((unsigned char *)name_to_match,
			 (unsigned char *)fontname, length);

    /* open up the font directory and look for matching file names */
#ifdef WIN32
    sprintf(path, "%s/*.*", pex_get_font_directory_path());
    if ((fontdirh = FindFirstFile(path, &dir_entry)) == INVALID_HANDLE_VALUE)
	return (PEXERR(PEXFontError));
#else
    if (!(fontdir = opendir(pex_get_font_directory_path())))
	return (PEXERR(PEXFontError));
#endif

#ifdef WIN32
    do
#else
    while(!found_it && (dir_entry = readdir(fontdir)))
#endif
	{
	    /* strip off .phont and make all lower case */
	    if (!get_lowered_truncated_entry(FileName(dir_entry), lowered_entry))
		continue;
		
	    /* does this match what got passed in? */
	    if (strcmp(lowered_entry, name_to_match) == 0)
		found_it = 1;
	}
    xfree(name_to_match);
#ifdef WIN32
    while (!found_it && FindNextFile(fontdirh, &dir_entry) && !found_it);
#endif
    
    if (!found_it)
	return (PEXERR(PEXFontError));
    
    (void) strcpy(fname, pex_get_font_directory_path());
    (void) strcat(fname, "/");
    (void) strcat(fname, FileName(dir_entry));
    
#ifdef WIN32
    FindClose(fontdirh);
#else
    closedir(fontdir);
#endif

    if ((fp = fopen(fname, "r")) == NULL)
	return (PEXERR(PEXFontError));

    /*
     * read in the file header.  The file header has fields containing the
     * num of characters in the font, the extreme values, and number of font
     * properties defined, if any.
     */

    tblptr = 0;
    if (fread((char *) &header, sizeof(header), 1, fp) != 1) {
	(void) ClosePEXFontFile(fp);
	return (PEXERR(PEXFontError)); }
    
    /* Initialize font structure */
    (void) strcpy(font->name, header.name);
    font->font_type = FONT_POLYLINES;
    font->top = header.top;
    font->bottom = header.bottom;
    font->num_ch = header.num_ch;
    font->font_info.numProps = (CARD32)header.num_props;
    font->max_width = header.max_width;

    /* read in the font properties, if any, into font data area */
    if (header.num_props > 0) {

	(void) SetPEXFontFilePtr(fp, START_PROPS);   /* Get to props position */
	properties = (Property *) xalloc(header.num_props * sizeof(Property));
	if (properties == NULL) {
	    (void) ClosePEXFontFile(fp);
	    return (BadAlloc); }

	if (fread((char *) properties, sizeof(Property), 
		  header.num_props, fp) != header.num_props) {
	    xfree((char *) properties);
	    (void) ClosePEXFontFile(fp);
	    return (PEXERR(PEXFontError)); }
    
	/* Create space for font properties in the font data area */

	font->properties =
	    (pexFontProp *) xalloc( (unsigned long)(header.num_props
				    * sizeof(pexFontProp)));
	if (font->properties == NULL) {
	    xfree((char *) properties);
	    (void) ClosePEXFontFile(fp);
	    return (BadAlloc); }

	np = header.num_props;
	for (	i=0, propptr = properties, fpptr = font->properties;
		i < np;
		i++, propptr++, fpptr++) {

	    if (propptr->propname == NULL) {
		(header.num_props)--;
		continue; }

	    fpptr->name = MakeAtom( (char *)propptr->propname,
				    strlen(propptr->propname), 1);

	    if (propptr->propvalue != NULL)
		fpptr->value = MakeAtom((char *)propptr->propvalue,
					strlen(propptr->propvalue), 1);
	    else fpptr->value = 0;
	}

	/* free up local storage allocated for properties */
	 xfree((char *) properties);
    }

    /* position file pointer to dispatch data */
    (void) SetPEXFontFilePtr(fp, (long) START_DISPATCH(header.num_props));

    /*
     * read in the distable font, use the offset to see if the
     * character is defined or not.  The strokes are defined in Phigs style.
     * The "center" of the character is not the physical center. It is the
     * center defined by the font designer.  The actual center is half the
     * "right" value.
     */

    table = (Dispatch *)xalloc((unsigned long)(sizeof(Dispatch) *font->num_ch));
    
    if (table == NULL) {
	(void) ClosePEXFontFile(fp);
	return (BadAlloc); }
    
    if (fread((char *) table, sizeof(Dispatch), font->num_ch, fp)
	    != font->num_ch) {
	xfree((char *) table);
	(void) ClosePEXFontFile(fp);
	return (PEXERR(PEXFontError)); }
    
    font->ch_data =
	(Ch_stroke_data **) xalloc((unsigned long)(sizeof(Ch_stroke_data *) * 
						    font->num_ch));
    if (font->ch_data == NULL) {
	xfree((char *) table);
	(void) ClosePEXFontFile(fp);
	return (BadAlloc); }
    
    /* The next loop initializes all ch_data pointers to null; essential
	for non-crashing during font clean-up in case of failed font file
	read.  Also count the number of non-blank chars.
     */
    for (   i = 0, ch_font = font->ch_data, tblptr = table, numChars = 0;
	    i < font->num_ch;
	    i++, ch_font++, tblptr++) {
	*ch_font = NULL;
	if (tblptr->offset != 0) numChars++; }

    ch_stroke = (Ch_stroke_data *)xalloc((unsigned long)(numChars *
						    sizeof(Ch_stroke_data)));
    if (!ch_stroke) {
	err = BadAlloc;
	goto disaster;
    }

    /* read in the char data  (the font file format should be changed
	so that the allocation can be done outside this loop--this
	method is inefficient)
     */
    for (   i = 0, ch_font = font->ch_data, tblptr = table, found_first = 0;
	    i < font->num_ch;
	    i++, ch_font++, tblptr++) {
	if (tblptr->offset != 0) {
	    (*ch_font) = ch_stroke++;
	    (*ch_font)->strokes.ddList = NULL;
	    (*ch_font)->center = table[i].center;
	    (*ch_font)->right = table[i].right;
	    
	    (void) SetPEXFontFilePtr(fp, tblptr->offset);

	    /* read in the type, number of subpaths, and n_vertices fields */
	    if (    (fread(&((*ch_font)->type),
			   sizeof(Font_path_type), 1, fp) != 1) 
		||  (fread(&((*ch_font)->strokes.numLists),
			   sizeof(ddULONG),1,fp) != 1)
		||  (fread(&((*ch_font)->n_vertices),
			   sizeof(ddULONG), 1, fp) != 1) )
		{		  
		    err = PEXERR(PEXFontError);
		    goto disaster;
		}
	    
	    (*ch_font)->strokes.maxLists = (*ch_font)->strokes.numLists;
	    if ((*ch_font)->strokes.numLists > 0) {
	    
		if (get_stroke(*ch_font, fp)) {
		    err = BadAlloc;
		    goto disaster; }
	    
		if (!found_first) {
		    font->font_info.firstGlyph = i;
		    found_first = 1; }

		font->font_info.lastGlyph = i; }
	    }
    }

    xfree((char *)table);

    (void) ClosePEXFontFile(fp);
    
    return (Success);

disaster:
    (void) ClosePEXFontFile(fp);
    if (table) xfree(table);
#if 0
    if (pFont == defaultPEXFont) defaultPEXFont = 0;	/* force free */
#endif
    FreePEXFont((diFontHandle) pFont, pFont->id);
    return (err);
    
}
void
ClosePEXFontFile(fp)
    FILE *fp;
{
    fclose (fp);
}

void
SetPEXFontFilePtr(fp,where)
    FILE *fp;
    long where;
{
    (void) fseek(fp, where, SEEK_SET);	    /* set pointer at "where" bytes
						from the beginning of the file */
}
