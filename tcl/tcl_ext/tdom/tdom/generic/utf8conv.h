/*---------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/utf8conv.h,v 1.2 2002/07/04 15:06:49 zoran Exp $
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
|   $Log: utf8conv.h,v $
|   Revision 1.2  2002/07/04 15:06:49  zoran
|   fixed reference to unsigned* to char since Sun compiler barfs at it.
|
|   Revision 1.1.1.1  2002/02/22 01:05:35  rolf
|   tDOM0.7test with Jochens first set of patches
|
|
|
|   written by Jochen Loewer
|   November, 1999
|
\--------------------------------------------------------------------------*/

#ifndef __UTF8CONV_H__
#define __UTF8CONV_H__


/*---------------------------------------------------------------------------
|   Includes
|
\--------------------------------------------------------------------------*/
#include <tcl.h>
#include <stdlib.h>
#include <string.h>


/*---------------------------------------------------------------------------
|   Typedefs
|
\--------------------------------------------------------------------------*/
typedef struct {
    int             type;
    int             start_code;
    int             len;
    char          * map;
} TEncodingRule;

typedef struct {
    const char    * name;
    int             fallback_char;
    TEncodingRule * rules;
} TEncoding;


             
/*--------------------------------------------------------------------------
|   Function prototypes
|
\-------------------------------------------------------------------------*/
TEncoding * tdom_GetEncoding (char *name);
char *      tdom_GetEncodingName (TEncoding *encoding);
void        tdom_Utf8to8Bit (TEncoding  *encoding, 
                             const char *utf8_string, int *len);

#endif

    
