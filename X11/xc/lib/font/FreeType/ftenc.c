/* 
Copyright (c) 1998-2002 by Juliusz Chroboczek
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions: 
 
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software. 

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XFree86: xc/lib/font/FreeType/ftenc.c,v 1.21 2002/10/01 00:02:10 alanh Exp $ */

#ifndef FONTMODULE
#include <string.h>
#else
#include "Xmd.h"
#include "Xdefs.h"
#include "xf86_ansic.h"
#endif

#include "fontmisc.h"
#include "fontenc.h"
#include "freetype/freetype.h"
#include "freetype/ttnameid.h"
#include "freetype/tttables.h"
#include "freetype/t1tables.h"
#include "freetype/ftxf86.h"
#include "ft.h"

static int find_cmap(int, int, int, FT_Face, FT_CharMap *);

static int
FTEncFontSpecific(char *encoding)
{
    char *p = encoding;

    while(*p != '-') {
        if(*p == '\0')
            return 0;
        p++;
    }
    p++;
    return (strcasecmp(p, "fontspecific") == 0);
}

int
FTPickMapping(char *xlfd, int length, char *filename, FT_Face face,
              FTMappingPtr tm)
{
    FontEncPtr encoding;
    FontMapPtr mapping;
    FT_CharMap cmap;
    int ftrc;
    
    char *encoding_name = 0;

    if(xlfd)
      encoding_name = FontEncFromXLFD(xlfd, length);
    if(!encoding_name)
        encoding_name = "iso8859-1";

    if(FTEncFontSpecific(encoding_name)) {
        ftrc = FT_Select_Charmap(face, ft_encoding_adobe_custom);
        if(ftrc != 0) {
            ErrorF("FreeType: couldn't select custom Type 1 encoding\n");
            return -1;
        } else {
            tm->named = 0;
            tm->cmap = face->charmap;
            tm->base = 0;
            tm->mapping = NULL;
            return 0;
        }
    }

    encoding = FontEncFind(encoding_name, filename);
    if(encoding == NULL) {
        ErrorF("FreeType: couldn't find encoding %s\n", encoding_name);
        encoding = FontEncFind("iso8859-1", filename);
    }
    if(encoding == NULL) {
        ErrorF("FreeType: couldn't find encoding iso8859-1.\n");
        return -1;
    }

    if(FT_Has_PS_Glyph_Names(face)) {
        for(mapping = encoding->mappings; mapping; mapping = mapping->next) {
            if(mapping->type == FONT_ENCODING_POSTSCRIPT) {
                tm->named = 1;
                tm->base = 0;
                tm->mapping = mapping;
                return 0;
            }
        }
    }
    
    for(mapping = encoding->mappings; mapping; mapping = mapping->next) {
        if(find_cmap(mapping->type, mapping->pid, mapping->eid, face, 
                     &cmap)) {
            tm->named = 0;
            tm->cmap = cmap;
            if(strcasecmp(encoding_name, "microsoft-symbol") == 0) {
                /* deal with undocumented lossage */
                TT_OS2 *os2;
                os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
                if(os2)
                    tm->base = os2->usFirstCharIndex - 0x20;
                else
                    tm->base = 0;
            } else
                tm->base = 0;
            tm->mapping = mapping;
            return 0;
        }
    }
    
    return -1;
}

static int 
find_cmap(int type, int pid, int eid, FT_Face face, FT_CharMap *cmap_return)
{
    int i, n;
    FT_CharMap cmap = NULL;

    n = face->num_charmaps;

    switch(type) {
    case FONT_ENCODING_TRUETYPE:  /* specific cmap */
        for(i=0; i<n; i++) {
            cmap = face->charmaps[i];
            if(cmap->platform_id == pid && cmap->encoding_id == eid) {
                *cmap_return = cmap;
                return 1;
            }
        }
        break;
    case FONT_ENCODING_UNICODE:   /* any Unicode cmap */
        /* prefer Microsoft Unicode */
        for(i=0; i<n; i++) {
            cmap = face->charmaps[i];
            if(cmap->platform_id == TT_PLATFORM_MICROSOFT && 
               cmap->encoding_id == TT_MS_ID_UNICODE_CS) {
                *cmap_return = cmap;
                return 1;
            }
        }
        break;
        /* Try Apple Unicode */
        for(i=0; i<n; i++) {
            cmap = face->charmaps[i];
            if(cmap->platform_id == TT_PLATFORM_APPLE_UNICODE) {
                *cmap_return = cmap;
                return 1;
            }
        }
        /* ISO Unicode? */
        for(i=0; i<n; i++) {
            cmap = face->charmaps[i];
            if(cmap->platform_id == TT_PLATFORM_ISO) {
                *cmap_return = cmap;
                return 1;
            }
        }
        break;
    default:
        return 0;
    }
    return 0;
}

unsigned 
FTRemap(FT_Face face, FTMappingPtr tm, unsigned code)
{
    unsigned index;
    char *name;
    unsigned glyph_index;

    if(tm->mapping) {
        if(tm->named) {
            name = FontEncName(code, tm->mapping);
            if(!name)
                return 0;
            glyph_index = FT_Get_Name_Index(face, name);
            return glyph_index;
        } else {
            index = FontEncRecode(code, tm->mapping) + tm->base;
            FT_Set_Charmap(face, tm->cmap);
            glyph_index = FT_Get_Char_Index(face, index);
            return glyph_index;
        }
    } else {
        if(code < 0x100) {
            index = code;
            FT_Set_Charmap(face, tm->cmap);
            glyph_index = FT_Get_Char_Index(face, index);
            return glyph_index;
        } else
            return 0;
    }
}
