/*---------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/utf8conv.c,v 1.1.1.1 2002/02/22 01:05:35 rolf Exp $
|
|
|   Functions, which (try) to convert UTF-8 encoded Unicode strings back 
|   to some 8bit encodings like ISO-8859-*, ... 
|
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|
|
|   $Log: utf8conv.c,v $
|   Revision 1.1.1.1  2002/02/22 01:05:35  rolf
|   tDOM0.7test with Jochens first set of patches
|
|
|
|   written by Jochen Loewer
|   November, 1999
|
\--------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------
|   Includes
|
\--------------------------------------------------------------------------*/
#include <tcl.h>
#include <stdlib.h>
#include <string.h>
#include <utf8conv.h>

/*---------------------------------------------------------------------------
|   Defines
|
\--------------------------------------------------------------------------*/
#define DBG(x)

#define ENC_END       0
#define ENC_IDENTITY  1
#define ENC_MAP       2

#if defined(_MSC_VER)
# define STRCASECMP(a,b)  stricmp (a,b)
#else
# define STRCASECMP(a,b)  strcasecmp (a,b)
#endif


/*---------------------------------------------------------------------------
|   Static Globals
|
\--------------------------------------------------------------------------*/
#include "encodings.inc"



/*---------------------------------------------------------------------------
|   tdom_GetEncoding  -  Looks up a encoding table for the given encoding
|                        name. If nothing was found NULL is returned.
|
\--------------------------------------------------------------------------*/
TEncoding * 
tdom_GetEncoding (
    char  * name
)
{
    TEncoding *encoding = TDOM_UnicodeTo8bitEncodings;
    
    while (encoding && encoding->name) {
        DBG(fprintf(stderr, "encoding=%x encoding->name='%s' name='%s'",
                             encoding, encoding->name, name);)
        if (STRCASECMP(encoding->name,name)==0) {
            return encoding;
        }
        encoding++;
    }
    return NULL;
}


/*---------------------------------------------------------------------------
|   tdom_GetEncodingName
|
\--------------------------------------------------------------------------*/
char *
tdom_GetEncodingName (TEncoding *encoding) 
{
    TEncoding *knownencoding = TDOM_UnicodeTo8bitEncodings;
    
    while (knownencoding && knownencoding->name) {
        if (knownencoding == encoding) {
            return (char*) knownencoding->name;
        }
        knownencoding++;
    }
    return NULL;
}
    

/*---------------------------------------------------------------------------
|   tdom_Utf8to8Bit  -  Convert a UTF-8 encode string with byte length 
|                       *len to 8bit encoding using the specify encoding.
|
\--------------------------------------------------------------------------*/
void 
tdom_Utf8to8Bit (
    TEncoding  * encoding,
    const char * utf8_string,
    int        * len
)
{
    unsigned char  *in, *end, *out;
    TEncodingRule  *rule;
    int             byte;
    int             unicode;
        
        
    if (encoding == NULL) {
       /* don't convert; keep UTF-8 */
       return;
    }
         
    in  = (unsigned char*) utf8_string;
    out = (unsigned char*) utf8_string;
    end = in + *len;
    unicode = 0;
    
    while (in < end) {

        byte = *in;

        /* extract unicode character from (multiple) UTF-8 bytes */

        if (byte < 0xC0) { 
            unicode = byte;
            in++;
        } else if (byte < 0xE0) {
            if ((in[1] & 0xC0) == 0x80) {
                unicode = ((byte & 0x1F) << 6) | (in[1] & 0x3F);
                in += 2;
            } else {
                unicode = byte; 
                in++;
            }
        } else if (byte < 0xF0) {
            if (((in[1] & 0xC0) == 0x80) && ((in[2] & 0xC0) == 0x80)) {
                unicode =  ((byte  & 0x0F) << 12)
                         | ((in[1] & 0x3F) << 6 )
                         | ((in[2] & 0x3F)      );
                in += 3;
            } else {
                unicode = byte; 
                in++; 
            }
        } else {
            /* ??? > 3 bytes UTF chars ??? */
            in++;
        }

        /* convert unicode character to 8bit representation */
        rule = encoding->rules;
        while (rule && rule->type != ENC_END) {
            if (   (unicode >= rule->start_code) 
                && (unicode < (rule->start_code + rule->len)) ) {

                if (rule->type == ENC_MAP) {
                    *out++ = rule->map[unicode - rule->start_code];
                } else {
                    *out++ = unicode & 0xFF;
                }
                break;
            }
            rule++;
        }
        if (rule->type == ENC_END) {
            /* no rule foun, use fallback */
            *out++ = encoding->fallback_char & 0x0FF;
        }
    }
    if (out < end) {
        *out = '\0';
    }
    *len = ( (char*)out - utf8_string);
}

