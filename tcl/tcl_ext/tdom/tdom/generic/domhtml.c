/*----------------------------------------------------------------------------
|   Copyright (c) 2000  Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|
| !! EXPERIMENTAL / pre alpha !!
|   A simple (hopefully fast) HTML parser to build up a DOM structure
|   in memory.
|   Based on xmlsimple.c.
| !! EXPERIMENTAL / pre alpha !!
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
|   written by Jochen Loewer
|   October 2000
|
|   ------------------------------------------------------------------------
|
|   A parser for XML.
|
|   Copyright (C) 1998 D. Richard Hipp
|
|   This library is free software; you can redistribute it and/or
|   modify it under the terms of the GNU Library General Public
|   License as published by the Free Software Foundation; either
|   version 2 of the License, or (at your option) any later version.
|
|   This library is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|   Library General Public License for more details.
|
|   You should have received a copy of the GNU Library General Public
|   License along with this library; if not, write to the
|   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
|   Boston, MA  02111-1307, USA.
|
|   Author contact information:
|     drh@acm.org
|     http://www.hwaci.com/drh/
|
\---------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <tcl.h>
#include <ctype.h>
#include <string.h>
#include <dom.h>



/*----------------------------------------------------------------------------
|   Defines
|
\---------------------------------------------------------------------------*/
#define DBG(x)          
#define RetError(m,p)   *errStr = tdomstrdup(m); *pos = p; return TCL_ERROR;
#define SPACE(c)        ((c)==' ' || (c)=='\n' || (c)=='\t' || (c)=='\r')
#define IsLetter(c)     ( ((c)>='A' && (c)<='Z') || ((c)>='a' && (c)<='z') || ((c) >= '0' && (c) <= '9') )
#define TU(c)           toupper(c)



/*----------------------------------------------------------------------------
|   Begin Character Entity Translator
|
|
|   The next section of code implements routines used to translate
|   character entity references into their corresponding strings.
|
|   Examples:
|
|         &amp;          "&"
|         &lt;           "<"
|         &gt;           ">"
|         &nbsp;         " "
|
\---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
|   Each entity reference is recorded as an instance of the following
|   tructure
\---------------------------------------------------------------------------*/
typedef struct Er Er;
struct Er {
    char *zName;     /* The name of this entity reference.  ex:  "amp" */
    char *zValue;    /* The value for this entity.          ex:  "&"   */
    Er *pNext;       /* Next entity with the same hash on zName        */
};


/*----------------------------------------------------------------------------
|   The size of the hash table.  For best results this should
|   be a prime number which is about the same size as the number of
|   character entity references known to the system.
|
\---------------------------------------------------------------------------*/
#if TclOnly8Bits
#define ER_HASH_SIZE 107
#else
#define ER_HASH_SIZE 257
#endif

/*----------------------------------------------------------------------------
|   The following flag is TRUE if entity reference hash table needs
|   to be initialized.
|
\---------------------------------------------------------------------------*/
static int bErNeedsInit = 1;
TDomThreaded(static Tcl_Mutex initMutex;)


/*----------------------------------------------------------------------------
|   The hash table
|
|   If the name of an entity reference hashes to the value H, then
|   apErHash[H] will point to a linked list of Er structures, one of
|   which will be the Er structure for that entity reference
|
\---------------------------------------------------------------------------*/
static Er *apErHash[ER_HASH_SIZE];



/*----------------------------------------------------------------------------
|   ErHash  --
|
|       Hash an entity reference name.  The value returned is an
|       integer between 0 and Er_HASH_SIZE-1, inclusive.
|
\---------------------------------------------------------------------------*/
static int ErHash(
    const char *zName
)
{
    int h = 0;      /* The hash value to be returned */
    char c;         /* The next character in the name being hashed */

    while( (c=*zName)!=0 ){
        h = h<<5 ^ h ^ c;
        zName++;
    }
    if( h<0 ) h = -h;
    return h % ER_HASH_SIZE;

} /* ErHash */


/*----------------------------------------------------------------------------
|   The following is a table of all entity references.  To create
|   new character entities, add entries to this table.
|
|   Note: For the decoder to work, the name of the entity reference
|   must not be shorter than the value.
|
\---------------------------------------------------------------------------*/
static Er er_sequences[] = {
    { "amp",       "&",        0 },
    { "lt",        "<",        0 },
    { "gt",        ">",        0 },
    { "apos",      "'",        0 },
    { "quot",      "\"",       0 },
#if TclOnly8Bits
    { "nbsp",      "\240",     0 },
    { "iexcl",     "\241",     0 }, /* inverted exclamation mark  */
    { "cent",      "\242",     0 }, /* cent sign  */
    { "pound",     "\243",     0 }, /* pound sterling sign  */
    { "curren",    "\244",     0 }, /* general currency sign  */
    { "yen",       "\245",     0 }, /* yen sign  */
    { "brvbar",    "\246",     0 }, /* broken (vertical) bar  */
    { "sect",      "\247",     0 }, /* section sign  */
    { "uml",       "\250",     0 }, /* umlaut (dieresis)  */
    { "copy",      "\251",     0 }, /* copyright sign  */
    { "ordf",      "\252",     0 }, /* ordinal indicator, feminine  */
    { "laquo",     "\253",     0 }, /* angle quotation mark, left  */
    { "not",       "\254",     0 }, /* not sign  */
    { "shy",       "\255",     0 }, /* soft hyphen  */
    { "reg",       "\256",     0 }, /* registered sign  */
    { "macr",      "\257",     0 }, /* macron  */
    { "deg",       "\260",     0 }, /* degree sign  */
    { "plusmn",    "\261",     0 }, /* plus-or-minus sign  */
    { "sup2",      "\262",     0 }, /* superscript two  */
    { "sup3",      "\263",     0 }, /* superscript three  */
    { "acute",     "\264",     0 }, /* acute accent  */
    { "micro",     "\265",     0 }, /* micro sign  */
    { "para",      "\266",     0 }, /* pilcrow (paragraph sign)  */
    { "middot",    "\267",     0 }, /* middle dot  */
    { "cedil",     "\270",     0 }, /* cedilla  */
    { "sup1",      "\271",     0 }, /* superscript one  */
    { "ordm",      "\272",     0 }, /* ordinal indicator, masculine  */
    { "raquo",     "\273",     0 }, /* angle quotation mark, right  */
    { "frac14",    "\274",     0 }, /* fraction one-quarter  */
    { "frac12",    "\275",     0 }, /* fraction one-half  */
    { "frac34",    "\276",     0 }, /* fraction three-quarters  */
    { "iquest",    "\277",     0 }, /* inverted question mark  */
    { "Agrave",    "\300",     0 }, /* capital A, grave accent  */
    { "Aacute",    "\301",     0 }, /* capital A, acute accent  */
    { "Acirc",     "\302",     0 }, /* capital A, circumflex accent  */
    { "Atilde",    "\303",     0 }, /* capital A, tilde  */
    { "Auml",      "\304",     0 }, /* capital A, dieresis or umlaut mark  */
    { "Aring",     "\305",     0 }, /* capital A, ring  */
    { "AElig",     "\306",     0 }, /* capital AE diphthong (ligature)  */
    { "Ccedil",    "\307",     0 }, /* capital C, cedilla  */
    { "Egrave",    "\310",     0 }, /* capital E, grave accent  */
    { "Eacute",    "\311",     0 }, /* capital E, acute accent  */
    { "Ecirc",     "\312",     0 }, /* capital E, circumflex accent  */
    { "Euml",      "\313",     0 }, /* capital E, dieresis or umlaut mark  */
    { "Igrave",    "\314",     0 }, /* capital I, grave accent  */
    { "Iacute",    "\315",     0 }, /* capital I, acute accent  */
    { "Icirc",     "\316",     0 }, /* capital I, circumflex accent  */
    { "Iuml",      "\317",     0 }, /* capital I, dieresis or umlaut mark  */
    { "ETH",       "\320",     0 }, /* capital Eth, Icelandic  */
    { "Ntilde",    "\321",     0 }, /* capital N, tilde  */
    { "Ograve",    "\322",     0 }, /* capital O, grave accent  */
    { "Oacute",    "\323",     0 }, /* capital O, acute accent  */
    { "Ocirc",     "\324",     0 }, /* capital O, circumflex accent  */
    { "Otilde",    "\325",     0 }, /* capital O, tilde  */
    { "Ouml",      "\326",     0 }, /* capital O, dieresis or umlaut mark  */
    { "times",     "\327",     0 }, /* multiply sign  */
    { "Oslash",    "\330",     0 }, /* capital O, slash  */
    { "Ugrave",    "\331",     0 }, /* capital U, grave accent  */
    { "Uacute",    "\332",     0 }, /* capital U, acute accent  */
    { "Ucirc",     "\333",     0 }, /* capital U, circumflex accent  */
    { "Uuml",      "\334",     0 }, /* capital U, dieresis or umlaut mark  */
    { "Yacute",    "\335",     0 }, /* capital Y, acute accent  */
    { "THORN",     "\336",     0 }, /* capital THORN, Icelandic  */
    { "szlig",     "\337",     0 }, /* small sharp s, German (sz ligature)  */
    { "agrave",    "\340",     0 }, /* small a, grave accent  */
    { "aacute",    "\341",     0 }, /* small a, acute accent  */
    { "acirc",     "\342",     0 }, /* small a, circumflex accent  */
    { "atilde",    "\343",     0 }, /* small a, tilde  */
    { "auml",      "\344",     0 }, /* small a, dieresis or umlaut mark  */
    { "aring",     "\345",     0 }, /* small a, ring  */
    { "aelig",     "\346",     0 }, /* small ae diphthong (ligature)  */
    { "ccedil",    "\347",     0 }, /* small c, cedilla  */
    { "egrave",    "\350",     0 }, /* small e, grave accent  */
    { "eacute",    "\351",     0 }, /* small e, acute accent  */
    { "ecirc",     "\352",     0 }, /* small e, circumflex accent  */
    { "euml",      "\353",     0 }, /* small e, dieresis or umlaut mark  */
    { "igrave",    "\354",     0 }, /* small i, grave accent  */
    { "iacute",    "\355",     0 }, /* small i, acute accent  */
    { "icirc",     "\356",     0 }, /* small i, circumflex accent  */
    { "iuml",      "\357",     0 }, /* small i, dieresis or umlaut mark  */
    { "eth",       "\360",     0 }, /* small eth, Icelandic  */
    { "ntilde",    "\361",     0 }, /* small n, tilde  */
    { "ograve",    "\362",     0 }, /* small o, grave accent  */
    { "oacute",    "\363",     0 }, /* small o, acute accent  */
    { "ocirc",     "\364",     0 }, /* small o, circumflex accent  */
    { "otilde",    "\365",     0 }, /* small o, tilde  */
    { "ouml",      "\366",     0 }, /* small o, dieresis or umlaut mark  */
    { "divide",    "\367",     0 }, /* divide sign  */
    { "oslash",    "\370",     0 }, /* small o, slash  */
    { "ugrave",    "\371",     0 }, /* small u, grave accent  */
    { "uacute",    "\372",     0 }, /* small u, acute accent  */
    { "ucirc",     "\373",     0 }, /* small u, circumflex accent  */
    { "uuml",      "\374",     0 }, /* small u, dieresis or umlaut mark  */
    { "yacute",    "\375",     0 }, /* small y, acute accent  */
    { "thorn",     "\376",     0 }, /* small thorn, Icelandic  */
    { "yuml",      "\377",     0 }, /* small y, dieresis or umlaut mark  */
#else
    { "nbsp",      "\xC2\xA0",    0 },
    { "iexcl",     "\xC2\xA1",    0 },
    { "cent",      "\xC2\xA2",    0 },
    { "pound",     "\xC2\xA3",    0 },
    { "curren",    "\xC2\xA4",    0 },
    { "yen",       "\xC2\xA5",    0 },
    { "brvbar",    "\xC2\xA6",    0 },
    { "sect",      "\xC2\xA7",    0 },
    { "uml",       "\xC2\xA8",    0 },
    { "copy",      "\xC2\xA9",    0 },
    { "ordf",      "\xC2\xAA",    0 },
    { "laquo",     "\xC2\xAB",    0 },
    { "not",       "\xC2\xAC",    0 },
    { "shy",       "\xC2\xAD",    0 },
    { "reg",       "\xC2\xAE",    0 },
    { "macr",      "\xC2\xAF",    0 },
    { "deg",       "\xC2\xB0",    0 },
    { "plusmn",    "\xC2\xB1",    0 },
    { "sup2",      "\xC2\xB2",    0 },
    { "sup3",      "\xC2\xB3",    0 },
    { "acute",     "\xC2\xB4",    0 },
    { "micro",     "\xC2\xB5",    0 },
    { "para",      "\xC2\xB6",    0 },
    { "middot",    "\xC2\xB7",    0 },
    { "cedil",     "\xC2\xB8",    0 },
    { "sup1",      "\xC2\xB9",    0 },
    { "ordm",      "\xC2\xBA",    0 },
    { "raquo",     "\xC2\xBB",    0 },
    { "frac14",    "\xC2\xBC",    0 },
    { "frac12",    "\xC2\xBD",    0 },
    { "frac34",    "\xC2\xBE",    0 },
    { "iquest",    "\xC2\xBF",    0 },
    { "Agrave",    "\xC3\x80",    0 },
    { "Aacute",    "\xC3\x81",    0 },
    { "Acirc",     "\xC3\x82",    0 },
    { "Atilde",    "\xC3\x83",    0 },
    { "Auml",      "\xC3\x84",    0 },
    { "Aring",     "\xC3\x85",    0 },
    { "AElig",     "\xC3\x86",    0 },
    { "Ccedil",    "\xC3\x87",    0 },
    { "Egrave",    "\xC3\x88",    0 },
    { "Eacute",    "\xC3\x89",    0 },
    { "Ecirc",     "\xC3\x8A",    0 },
    { "Euml",      "\xC3\x8B",    0 },
    { "Igrave",    "\xC3\x8C",    0 },
    { "Iacute",    "\xC3\x8D",    0 },
    { "Icirc",     "\xC3\x8E",    0 },
    { "Iuml",      "\xC3\x8F",    0 },
    { "ETH",       "\xC3\x90",    0 },
    { "Ntilde",    "\xC3\x91",    0 },
    { "Ograve",    "\xC3\x92",    0 },
    { "Oacute",    "\xC3\x93",    0 },
    { "Ocirc",     "\xC3\x94",    0 },
    { "Otilde",    "\xC3\x95",    0 },
    { "Ouml",      "\xC3\x96",    0 },
    { "times",     "\xC3\x97",    0 },
    { "Oslash",    "\xC3\x98",    0 },
    { "Ugrave",    "\xC3\x99",    0 },
    { "Uacute",    "\xC3\x9A",    0 },
    { "Ucirc",     "\xC3\x9B",    0 },
    { "Uuml",      "\xC3\x9C",    0 },
    { "Yacute",    "\xC3\x9D",    0 },
    { "THORN",     "\xC3\x9E",    0 },
    { "szlig",     "\xC3\x9F",    0 },
    { "agrave",    "\xC3\xA0",    0 },
    { "aacute",    "\xC3\xA1",    0 },
    { "acirc",     "\xC3\xA2",    0 },
    { "atilde",    "\xC3\xA3",    0 },
    { "auml",      "\xC3\xA4",    0 },
    { "aring",     "\xC3\xA5",    0 },
    { "aelig",     "\xC3\xA6",    0 },
    { "ccedil",    "\xC3\xA7",    0 },
    { "egrave",    "\xC3\xA8",    0 },
    { "eacute",    "\xC3\xA9",    0 },
    { "ecirc",     "\xC3\xAA",    0 },
    { "euml",      "\xC3\xAB",    0 },
    { "igrave",    "\xC3\xAC",    0 },
    { "iacute",    "\xC3\xAD",    0 },
    { "icirc",     "\xC3\xAE",    0 },
    { "iuml",      "\xC3\xAF",    0 },
    { "eth",       "\xC3\xB0",    0 },
    { "ntilde",    "\xC3\xB1",    0 },
    { "ograve",    "\xC3\xB2",    0 },
    { "oacute",    "\xC3\xB3",    0 },
    { "ocirc",     "\xC3\xB4",    0 },
    { "otilde",    "\xC3\xB5",    0 },
    { "ouml",      "\xC3\xB6",    0 },
    { "divide",    "\xC3\xB7",    0 },
    { "oslash",    "\xC3\xB8",    0 },
    { "ugrave",    "\xC3\xB9",    0 },
    { "uacute",    "\xC3\xBA",    0 },
    { "ucirc",     "\xC3\xBB",    0 },
    { "uuml",      "\xC3\xBC",    0 },
    { "yacute",    "\xC3\xBD",    0 },
    { "thorn",     "\xC3\xBE",    0 },
    { "yuml",      "\xC3\xBF",    0 },
    /* "Special" chars, according to XHTML xhtml-special.ent */
    { "OElig",     "\xC5\x92",    0 },
    { "oelig",     "\xC5\x93",    0 },
    { "Scaron",    "\xC5\xA0",    0 },
    { "scaron",    "\xC5\xA1",    0 },
    { "Yuml",      "\xC5\xB8",    0 },
    { "circ",      "\xCB\x86",    0 },
    { "tilde",     "\xCB\x9C",    0 },
    { "ensp",      "\xE2\x80\x82",    0 },
    { "emsp",      "\xE2\x80\x83",    0 },
    { "thinsp",    "\xE2\x80\x89",    0 },
    { "zwnj",      "\xE2\x81\xB0",    0 },
    { "zwj",       "\xE2\x80\x8D",    0 },
    { "lrm",       "\xE2\x80\x8E",    0 },
    { "rlm",       "\xE2\x80\x8F",    0 },
    { "ndash",     "\xE2\x80\x93",    0 },
    { "mdash",     "\xE2\x80\x94",    0 },
    { "lsquo",     "\xE2\x80\x98",    0 },
    { "rsquo",     "\xE2\x80\x99",    0 },
    { "sbquo",     "\xE2\x80\x9A",    0 },
    { "ldquo",     "\xE2\x80\x9C",    0 },
    { "rdquo",     "\xE2\x80\x9D",    0 },
    { "bdquo",     "\xE2\x80\x9E",    0 },
    { "dagger",    "\xE2\x80\xA0",    0 },
    { "Dagger",    "\xE2\x80\xA1",    0 },
    { "permil",    "\xE2\x80\xB0",    0 },
    { "lsaquo",    "\xE2\x80\xB9",    0 },
    { "rsaquo",    "\xE2\x80\xBA",    0 },
    { "euro",      "\xE2\x82\xAC",    0 },
    /* "Symbol" chars, according to XHTML xhtml-symbol.ent */
    { "fnof",      "\xC6\x92",    0 },
    { "Alpha",     "\xCE\x91",    0 },
    { "Beta",      "\xCE\x92",    0 },
    { "Gamma",     "\xCE\x93",    0 },
    { "Delta",     "\xCE\x94",    0 },
    { "Epsilon",   "\xCE\x95",    0 },
    { "Zeta",      "\xCE\x96",    0 },
    { "Eta",       "\xCE\x97",    0 },
    { "Theta",     "\xCE\x98",    0 },
    { "Iota",      "\xCE\x99",    0 },
    { "Kappa",     "\xCE\x9A",    0 },
    { "Lambda",    "\xCE\x9B",    0 },
    { "Mu",        "\xCE\x9C",    0 },
    { "Nu",        "\xCE\x9D",    0 },
    { "Xi",        "\xCE\x9E",    0 },
    { "Omicron",   "\xCE\x9F",    0 },
    { "Pi",        "\xCE\xA0",    0 },
    { "Rho",       "\xCE\xA1",    0 },
    { "Sigma",     "\xCE\xA3",    0 },
    { "Tau",       "\xCE\xA4",    0 },
    { "Upsilon",   "\xCE\xA5",    0 },
    { "Phi",       "\xCE\xA6",    0 },
    { "Chi",       "\xCE\xA7",    0 },
    { "Psi",       "\xCE\xA8",    0 },
    { "Omega",     "\xCE\xA9",    0 },
    { "alpha",     "\xCE\xB1",    0 },
    { "beta",      "\xCE\xB2",    0 },
    { "gamma",     "\xCE\xB3",    0 },
    { "delta",     "\xCE\xB4",    0 },
    { "epsilon",   "\xCE\xB5",    0 },
    { "zeta",      "\xCE\xB6",    0 },
    { "eta",       "\xCE\xB7",    0 },
    { "theta",     "\xCE\xB8",    0 },
    { "iota",      "\xCE\xB9",    0 },
    { "kappa",     "\xCE\xBA",    0 },
    { "lambda",    "\xCE\xBB",    0 },
    { "mu",        "\xCE\xBC",    0 },
    { "nu",        "\xCE\xBD",    0 },
    { "xi",        "\xCE\xBE",    0 },
    { "omicron",   "\xCE\xBF",    0 },
    { "pi",        "\xCF\x80",    0 },
    { "rho",       "\xCF\x81",    0 },
    { "sigmaf",    "\xCF\x82",    0 },
    { "sigma",     "\xCF\x83",    0 },
    { "tau",       "\xCF\x84",    0 },
    { "upsilon",   "\xCF\x85",    0 },
    { "phi",       "\xCF\x86",    0 },
    { "chi",       "\xCF\x87",    0 },
    { "psi",       "\xCF\x88",    0 },
    { "omega",     "\xCF\x89",    0 },
    { "thetasym",  "\xCF\x91",    0 },
    { "upsih",     "\xCF\x92",    0 },
    { "piv",       "\xCF\x96",    0 },
    { "bull",      "\xE2\x80\xA2",    0 },
    { "hellip",    "\xE2\x80\xA6",    0 },
    { "prime",     "\xE2\x80\xB2",    0 },
    { "Prime",     "\xE2\x80\xB3",    0 },
    { "oline",     "\xE2\x80\xBE",    0 },
    { "frasl",     "\xE2\x81\x84",    0 },
    { "weierp",    "\xE2\x84\x98",    0 },
    { "image",     "\xE2\x84\x91",    0 },
    { "real",      "\xE2\x84\x9C",    0 },
    { "trade",     "\xE2\x84\xA2",    0 },
    { "alefsym",   "\xE2\x84\xB5",    0 },
    { "larr",      "\xE2\x86\x90",    0 },
    { "uarr",      "\xE2\x86\x91",    0 },
    { "rarr",      "\xE2\x86\x92",    0 },
    { "darr",      "\xE2\x86\x93",    0 },
    { "harr",      "\xE2\x86\x94",    0 },
    { "crarr",     "\xE2\x86\xB5",    0 },
    { "lArr",      "\xE2\x87\x90",    0 },
    { "uArr",      "\xE2\x87\x91",    0 },
    { "rArr",      "\xE2\x87\x92",    0 },
    { "dArr",      "\xE2\x87\x93",    0 },
    { "hArr",      "\xE2\x87\x94",    0 },
    { "forall",    "\xE2\x88\x80",    0 },
    { "part",      "\xE2\x88\x82",    0 },
    { "exist",     "\xE2\x88\x83",    0 },
    { "empty",     "\xE2\x88\x85",    0 },
    { "nabla",     "\xE2\x88\x87",    0 },
    { "isin",      "\xE2\x88\x88",    0 },
    { "notin",     "\xE2\x88\x89",    0 },
    { "ni",        "\xE2\x88\x8B",    0 },
    { "prod",      "\xE2\x88\x8F",    0 },
    { "sum",       "\xE2\x88\x91",    0 },
    { "minus",     "\xE2\x88\x92",    0 },
    { "lowast",    "\xE2\x88\x97",    0 },
    { "radic",     "\xE2\x88\x9A",    0 },
    { "prop",      "\xE2\x88\x9D",    0 },
    { "infin",     "\xE2\x88\x9E",    0 },
    { "ang",       "\xE2\x88\xA0",    0 },
    { "and",       "\xE2\x88\xA7",    0 },
    { "or",        "\xE2\x88\xA8",    0 },
    { "cap",       "\xE2\x88\xA9",    0 },
    { "cup",       "\xE2\x88\xAA",    0 },
    { "int",       "\xE2\x88\xAB",    0 },
    { "there4",    "\xE2\x88\xB4",    0 },
    { "sim",       "\xE2\x88\xBC",    0 },
    { "cong",      "\xE2\x89\x85",    0 },
    { "asymp",     "\xE2\x89\x88",    0 },
    { "ne",        "\xE2\x89\xA0",    0 },
    { "equiv",     "\xE2\x89\xA1",    0 },
    { "le",        "\xE2\x89\xA4",    0 },
    { "ge",        "\xE2\x89\xA5",    0 },
    { "sub",       "\xE2\x8A\x82",    0 },
    { "sup",       "\xE2\x8A\x83",    0 },
    { "nsub",      "\xE2\x8A\x84",    0 },
    { "sube",      "\xE2\x8A\x86",    0 },
    { "supe",      "\xE2\x8A\x87",    0 },
    { "oplus",     "\xE2\x8A\x95",    0 },
    { "otimes",    "\xE2\x8A\x97",    0 },
    { "perp",      "\xE2\x8A\xA5",    0 },
    { "sdot",      "\xE2\x8B\x85",    0 },
    { "lceil",     "\xE2\x8C\x88",    0 },
    { "rceil",     "\xE2\x8C\x89",    0 },
    { "lfloor",    "\xE2\x8C\x8A",    0 },
    { "rfloor",    "\xE2\x8C\x8B",    0 },
    { "lang",      "\xE2\x8C\xA9",    0 },
    { "rang",      "\xE2\x8C\xAA",    0 },
    { "loz",       "\xE2\x97\x8A",    0 },
    { "spades",    "\xE2\x99\xA0",    0 },
    { "clubs",     "\xE2\x99\xA3",    0 },
    { "hearts",    "\xE2\x99\xA5",    0 },
    { "diams",     "\xE2\x99\xA6",    0 },
#endif
};


/*----------------------------------------------------------------------------
|   ErInit --
|
|       Initialize the entity reference hash table
|
\---------------------------------------------------------------------------*/
static void ErInit (void)
{
    int i;  /* For looping thru the list of entity references */
    int h;  /* The hash on a entity */

    for(i=0; i<sizeof(er_sequences)/sizeof(er_sequences[0]); i++){
        h = ErHash(er_sequences[i].zName);
        er_sequences[i].pNext = apErHash[h];
        apErHash[h] = &er_sequences[i];
    }

} /* ErInit */


/*----------------------------------------------------------------------------
|    TranslateEntityRefs  --
|
|        Translate entity references and character references in the string
|        "z".  "z" is overwritten with the translated sequence.
|
|        Unrecognized entity references are unaltered.
|
|        Example:
|
|          input =    "AT&amp;T &gt MCI"
|          output =   "AT&T > MCI"
|
\---------------------------------------------------------------------------*/
static void TranslateEntityRefs (
    char *z,
    int  *newLen
)
{
    int from;    /* Read characters from this position in z[] */
    int to;      /* Write characters into this position in z[] */
    int h;       /* A hash on the entity reference */
    char *zVal;  /* The substituted value */
    Er *p;       /* For looping down the entity reference collision chain */
    int value;

    from = to = 0;

    if (bErNeedsInit) {
        TDomThreaded(Tcl_MutexLock(&initMutex);)
        if (bErNeedsInit) {
            ErInit();
            bErNeedsInit = 0;
        }
        TDomThreaded(Tcl_MutexUnlock(&initMutex);)
    }

    while (z[from]) {
        if (z[from]=='&') {
            int i = from+1;
            int c;

            if (z[i] == '#') {
                /*---------------------------------------------
                |   convert character reference
                \--------------------------------------------*/
                value = 0;
                if (z[++i] == 'x') {
                    i++;
                    while ((c=z[i]) && (c!=';')) {
                        value = value * 16;
                        if ((c>='0') && (c<='9')) {
                            value += c-'0';
                        } else
                        if ((c>='A') && (c<='F')) {
                            value += c-'A' + 10;
                        } else
                        if ((c>='a') && (c<='f')) {
                            value += c-'a' + 10;
                        } else {
                            /* error */
                        }
                        i++;
                    }
                } else {
                    while ((c=z[i]) && (c!=';')) {
                        value = value * 10;
                        if ((c>='0') && (c<='9')) {
                            value += c-'0';
                        } else {
                            /* error */
                        }
                        i++;
                    }
                }
                if (z[i]!=';') {
                    /* error */
                }
                from = i+1;
#if TclOnly8Bits
                z[to++] = value;
#else 
                if (value < 0x80) {
                    z[to++] = value;
                } else if (value <= 0x7FF) {
                    z[to++] = (char) ((value >> 6) | 0xC0);
                    z[to++] = (char) ((value | 0x80) & 0xBF);
                } else if (value <= 0xFFFF) {
                    z[to++] = (char) ((value >> 12) | 0xE0);
                    z[to++] = (char) (((value >> 6) | 0x80) & 0xBF);
                    z[to++] = (char) ((value | 0x80) & 0xBF);
                } else {
                    /* error */
                }
#endif
            } else {
                while (z[i] && isalpha(z[i])) {
                   i++;
                }
                c = z[i];
                z[i] = 0;
                h = ErHash(&z[from+1]);
                p = apErHash[h];
                while (p && strcmp(p->zName,&z[from+1])!=0 ) {
                    p = p->pNext;
                }
                z[i] = c;
                if (p) {
                    zVal = p->zValue;
                    while (*zVal) {
                        z[to++] = *(zVal++);
                    }
                    from = i;
                    if (c==';') from++;
                } else {
                    z[to++] = z[from++];
                }
            }
        } else {
            z[to++] = z[from++];
        }
    }
    z[to] = 0;
    *newLen = to;
}
/*----------------------------------------------------------------------------
|   End Of Character Entity Translator
\---------------------------------------------------------------------------*/



DBG(
/*----------------------------------------------------------------------------
|   getDeep
|
\---------------------------------------------------------------------------*/
static int getDeep (domNode *n) {
    int d;
    d = 0;
    while (n->parentNode != NULL) {
        d++;
        n  = n->parentNode;
    }
    return d;
}
)

/*----------------------------------------------------------------------------
|   HTML_SimpleParse (non recursive)
|
|       Parses the HTML string starting at 'pos' and continuing to the
|       first encountered error.
|
\---------------------------------------------------------------------------*/
static int
HTML_SimpleParse (
    char        *html,  /* HTML string  */
    int         *pos,   /* Index of next unparsed character in xml */
    domDocument *doc,
    domNode     *parent_nodeOld,
    int          ignoreWhiteSpaces,
    char       **errStr
) {
    register int   c;          /* Next character of the input file */
    register char *pn, *e;
    register char *x, *start, *piSep;
    int            saved;
    int            hasContent;
    domNode       *pnode, *toplevel;
    domNode       *node = NULL, *parent_node = NULL;
    domTextNode   *tnode;
    domAttrNode   *attrnode, *lastAttr;
    int            ampersandSeen = 0;
    int            only_whites   = 0;
    int            hnew, autoclose, ignore;
    char           tmp[250], *y = NULL;
    Tcl_HashEntry *h;
    domProcessingInstructionNode *pinode;

    x = &(html[*pos]);

    while ( (c=*x)!=0 ) {

        start = x;

        if ((c!='<') || ((c=='<') && (x[1]!='!') && (x[2]!='-') && (x[3]!='-') && (x[1]!='/') && !IsLetter(x[1])) ) {
            /*----------------------------------------------------------------
            |   read text between tags
            |
            \---------------------------------------------------------------*/
          readText:
            ampersandSeen = 0;
            only_whites = 1;
            if (c=='<') x++;
            while ( (c=*x)!=0 && c!='<' ) {
                if (c=='&') ampersandSeen = 1;
                if ( !SPACE(c) ) only_whites = 0;
                x++;
            }
            if (!(only_whites && ignoreWhiteSpaces) && parent_node) {
                /*--------------------------------------------------------
                |   allocate new TEXT node
                \-------------------------------------------------------*/
                tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                memset(tnode, 0, sizeof(domTextNode));
                tnode->nodeType    = TEXT_NODE;
                tnode->nodeFlags   = 0;
                tnode->namespace   = 0;
                tnode->ownerDocument = doc;
                tnode->nodeNumber  = NODE_NO(doc);
                tnode->valueLength = (x - start);
                tnode->nodeValue   = (char*)MALLOC((x - start)+1);
                memmove(tnode->nodeValue, start, (x - start));
                *(tnode->nodeValue + (x - start)) = 0;
                DBG(fprintf(stderr, "New text node: '%s'\n", tnode->nodeValue);)
                if (ampersandSeen) {
                    TranslateEntityRefs(tnode->nodeValue, &(tnode->valueLength) );
                }
                tnode->parentNode = parent_node;
                if (parent_node->firstChild)  {
                    parent_node->lastChild->nextSibling = (domNode*)tnode;
                    tnode->previousSibling = parent_node->lastChild;
                    parent_node->lastChild = (domNode*)tnode;
                } else {
                    parent_node->firstChild = parent_node->lastChild = (domNode*)tnode;
                }
                node = (domNode*)tnode;
            }

        } else if (x[1]=='/') {
            /*------------------------------------------------------------
            |   read and check closing tag
            \-----------------------------------------------------------*/
            x += 2;
            while ((c=*x)!=0 && c!='>' && c!='<' && !SPACE(c) ) {
                *x = tolower(c);
                x++;
            }
            if (c==0) {
                RetError("Missing \">\"",(start-html) );
            }
            if ( (x-start)==2) {
                RetError("Null markup name",(start-html) );
            }
            *x = '\0'; /* temporarily terminate the string */


            /*----------------------------------------------------------------------
            |   check for tags which could optional be close 
            |   like <option>...</option>
            \---------------------------------------------------------------------*/
            ignore = 0;
            pnode = NULL;
            if (parent_node) {
                if (parent_node->lastChild 
                    && (parent_node->lastChild->nodeType == ELEMENT_NODE)) {
                    pnode = parent_node->lastChild;
                } else 
                    if (parent_node->lastChild 
                        && parent_node->lastChild->previousSibling 
                        && (parent_node->lastChild->previousSibling->nodeType 
                            == ELEMENT_NODE)) {
                        pnode = parent_node->lastChild->previousSibling;
                    }
            }
            if (pnode) {
                DBG(fprintf(stderr, "'%s' closing with last empty tag '%s' ?\n", start+2, pnode->nodeName);)
                if (strcmp(start+2,pnode->nodeName)==0)  {
                    switch (*(start+2)) {
                        case 'o': if (!strcmp(start+2,"option")) ignore = 1; break;
                    }
                }
            } 

            if (!ignore) {
                /*----------------------------------------------------------------------
                |   look for a corresponding opening tag the way up the tag hierarchy
                \---------------------------------------------------------------------*/
                pnode = parent_node;
                while (pnode != NULL) {
                    DBG(fprintf(stderr, "checking '%s' to top hierarchy: '%s' \n", start+2,pnode->nodeName);)
                    if (!strcmp(start+2,pnode->nodeName)) break;
                    pnode = pnode->parentNode;
                }
                if (pnode == NULL) {
                    /* begining tag was not found the way up the tag hierarchy
                       -> ignore the tag */
                    DBG(fprintf(stderr,"ignoring closing '%s' \n", start+2);)
                    ignore = 1;
                }
            }            
            if (!ignore) {

                pn = (char*)parent_node->nodeName;                        

                while (1) {
                    DBG(fprintf(stderr, "comparing '%s' with pn='%s' \n", start+2, pn);)
                    if (strcmp(start+2,pn)!=0) {

                        /*----------------------------------------------------------
                        |   check for parent tags which allow closing of sub tags
                        |   which belong to the parent tag
                        \---------------------------------------------------------*/
                        ignore = 0;
                        if (!strcmp(pn,"table")
                            && (!strcmp(start+2,"tr") || !strcmp(start+2,"td"))
                        ) {
                            ignore = 1;
                        }
                        if (ignore) {
                            parent_node = node->parentNode;
                            break;
                        }


                        /*---------------------------------------------------------------
                        |   check for tags for which end tag can be omitted
                        \--------------------------------------------------------------*/
                        autoclose = 0;
                        switch (pn[0]) {
                            case 'a': if (!strcmp(pn,"a"))        autoclose = 1; break;
                            case 'b': if (!strcmp(pn,"b"))        autoclose = 1; break;
                            case 'c': if (!strcmp(pn,"colgroup")) autoclose = 1; break;
                            case 'd': if (!strcmp(pn,"dd") ||
                                          !strcmp(pn,"dt") ||
                                          (!strcmp(start+2,"form") && !strcmp(pn,"div"))
                                         )                        autoclose = 1; break;
                            case 'h': if (!strcmp(pn,"head") ||
                                          !strcmp(pn,"html"))     autoclose = 1; break;
                            case 'f': if (!strcmp(pn,"font")||
                                          !strcmp(pn,"form"))     autoclose = 1; break;
                            case 'i': if (!strcmp(pn,"i"))        autoclose = 1; break;
                            case 'l': if (!strcmp(pn,"li"))       autoclose = 1; break;
                            case 'n': if (!strcmp(pn,"noscript")) autoclose = 1; break;
                            case 'o': if (!strcmp(pn,"option"))   autoclose = 1; break;
                            case 'p': if (!strcmp(pn,"p"))        autoclose = 1; break;
                            case 's': if (!strcmp(pn,"span"))     autoclose = 1; break;
                            case 't': if (!strcmp(pn,"tbody") ||
                                          !strcmp(pn,"td")    ||
                                          !strcmp(pn,"tfoot") ||
                                          !strcmp(pn,"thead") ||
                                          !strcmp(pn,"th")    ||
                                          !strcmp(pn,"tr")    ||
                                          !strcmp(pn,"tt"))       autoclose = 1; break;
                            case 'u': if (!strcmp(pn,"ul"))       autoclose = 1; break; /* ext */
                        }
                        /*---------------------------------------------------------------
                        |   check for tags for close inner tags
                        \--------------------------------------------------------------*/
                        switch (start[2]) {
                            case 'b': if (!strcmp(start+2,"body")) autoclose = 1; break;
                        }
                        if (autoclose) {
                            DBG(fprintf(stderr, "autoclose '%s' with '%s' \n", pn, start+2);)
                            if (parent_node != NULL) {
                                node = parent_node;
                                parent_node = node->parentNode;
                                pn = (char*)node->nodeName;
                                break; 
                            }
                        }
                        sprintf(tmp, "Unterminated element '%s' (within '%s')", start+2, pn);
                        *x = c;  /* remove temporarily termination */
                        RetError(tmp,(x - html));
                    }
                    break;
                }
                
                /* move up */
                node = parent_node;
                parent_node = NULL;
                if (node) parent_node = node->parentNode;
            }
            *x = c;  /* remove temporarily termination */

                     
            while (SPACE(*x)) {
                x++;
            }
            if (*x=='>') {
                x++;
            } else {
                if (*x == '<') {
                    /* start of new tag, ends closing tag */
                } else {
                    RetError("Missing \">\"",(x - html)-1);
                }
            }            
            if (parent_node == NULL) {
                /* we return to main node and so finished parsing */
                return TCL_OK;
            }
            continue;

        } else {

            x++;
            if (*x=='!') {
                if (x[1]=='-' && x[2]=='-') {
                    /*--------------------------------------------------------
                    |   read over a comment
                    \-------------------------------------------------------*/
                    x += 3;
                    while ( (c=*x)!=0 &&
                            (c!='-' || x[1]!='-' || x[2]!='>')) {
                        x++;
                    }
                    if (*x) {
                        /*----------------------------------------------------
                        |   allocate new COMMENT node for comments
                        \---------------------------------------------------*/
                        tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                        memset(tnode, 0, sizeof(domTextNode));
                        tnode->nodeType      = COMMENT_NODE;
                        tnode->nodeFlags     = 0;
                        tnode->namespace     = 0;
                        tnode->ownerDocument = doc;
                        tnode->nodeNumber    = NODE_NO(doc);
                        tnode->parentNode    = parent_node;
                        tnode->valueLength   = x - start - 4;
                        tnode->nodeValue     = (char*)MALLOC(tnode->valueLength+1);
                        memmove(tnode->nodeValue, start+4, tnode->valueLength);
                        *(tnode->nodeValue + tnode->valueLength) = 0;
                        if (parent_node == NULL) {
                            if (doc->documentElement) {
                                toplevel = doc->documentElement;
                                while (toplevel->nextSibling) {
                                    toplevel = toplevel->nextSibling;
                                }
                                toplevel->nextSibling   = (domNode*)tnode;
                                tnode->previousSibling = (domNode*)toplevel;
                            } else {
                                doc->documentElement = (domNode*)tnode;
                            }
                        } else {
                            if (parent_node->firstChild)  {
                                parent_node->lastChild->nextSibling = (domNode*)tnode;
                                tnode->previousSibling = parent_node->lastChild;
                                parent_node->lastChild = (domNode*)tnode;
                            } else {
                                parent_node->firstChild = parent_node->lastChild = (domNode*)tnode;
                            }
                        }
                        x += 3;
                    } else {
                        RetError("Unterminated comment",(start-html));
                    }
                    continue;

                } else if (TU(x[1])=='D' && TU(x[2])=='O' &&
                           TU(x[3])=='C' && TU(x[4])=='T' &&
                           TU(x[5])=='Y' && TU(x[6])=='P' && TU(x[7])=='E' ) {
                    /*--------------------------------------------------------
                    |   read over a DOCTYPE definition
                    \-------------------------------------------------------*/
                    x += 8;
                    start = x;
                    while (*x!=0) {
                        if (*x=='[') {
                            x++;
                            while ((*x!=0) && (*x!=']')) x++;
                        } else
                        if (*x=='>') {
                            break;
                        } else {
                            x++;
                        }
                    }
                    if (*x) {
                        x++;
                    } else {
                        RetError("Unterminated DOCTYPE definition",(start-html));
                    }
                    continue;

                } else if (x[1]=='[' && x[2]=='C' &&
                           x[3]=='D' && x[4]=='A' &&
                           x[5]=='T' && x[6]=='A' && x[7]=='[' ) {
                    /*--------------------------------------------------------
                    |   read over a <![CDATA[ section
                    \-------------------------------------------------------*/
                    x += 8;
                    start = x;
                    while ( (*x!=0) &&
                            ((*x!=']') || (x[1]!=']') || (x[2]!='>'))) {
                        x++;
                    }
                    if (*x) {
                        if (parent_node) {
                            /*----------------------------------------------------
                            |   allocate new TEXT node for CDATA section data
                            \---------------------------------------------------*/
                            tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                            memset(tnode, 0, sizeof(domTextNode));
                            tnode->nodeType      = TEXT_NODE;
                            tnode->nodeFlags     = 0;
                            tnode->namespace     = 0;
                            tnode->ownerDocument = doc;
                            tnode->nodeNumber    = NODE_NO(doc);
                            tnode->parentNode    = parent_node;
                            tnode->valueLength   = (x - start);
                            tnode->nodeValue     = (char*)MALLOC((x - start)+1);
                            memmove(tnode->nodeValue, start, (x - start));
                            *(tnode->nodeValue + (x - start)) = 0;
                            if (parent_node->firstChild)  {
                                parent_node->lastChild->nextSibling = (domNode*)tnode;
                                tnode->previousSibling = parent_node->lastChild;
                                parent_node->lastChild = (domNode*)tnode;
                            } else {
                                parent_node->firstChild = parent_node->lastChild = (domNode*)tnode;
                            }
                        }
                        x += 3;
                    } else {
                        RetError("Unterminated CDATA definition",(start-html) );
                    }
                    continue;
                 } else {
                        RetError("Incorrect <!... tag",(start-html) );
                 }

            } else if (*x=='?') {
                /*--------------------------------------------------------
                |   read over a processing instructions(PI) / XMLDecl
                \-------------------------------------------------------*/
                x++;
                start = x;
                while ( (c=*x)!=0 &&
                        (c!='?' || x[1]!='>')) {
                    x++;
                }
                if (*x) {
                    /*------------------------------------------------------------
                    |   allocate new PI node for processing instruction section
                    \-----------------------------------------------------------*/
                    pinode = (domProcessingInstructionNode*)
                            MALLOC(sizeof(domProcessingInstructionNode));
                    memset(pinode, 0, sizeof(domProcessingInstructionNode));
                    pinode->nodeType      = PROCESSING_INSTRUCTION_NODE;
                    pinode->nodeFlags     = 0;
                    pinode->namespace     = 0;
                    pinode->ownerDocument = doc;
                    pinode->nodeNumber    = NODE_NO(doc);
                    pinode->parentNode    = parent_node;

                    /*-------------------------------------------------
                    |   extract PI target
                    \------------------------------------------------*/
                    piSep = start;
                    while ( (c=*piSep)!=0 && !SPACE(c) &&
                            (c!='?' || piSep[1]!='>')) {
                         piSep++;
                    }
                    *piSep = '\0'; /* temporarily terminate the string */

                    pinode->targetLength = strlen(start);
                    pinode->targetValue  = (char*)MALLOC(pinode->targetLength);
                    memmove(pinode->targetValue, start, pinode->targetLength);

                    *piSep = c;  /* remove temporarily termination */

                    /*-------------------------------------------------
                    |   extract PI data
                    \------------------------------------------------*/
                    while (SPACE(*piSep)) {
                        piSep++;
                    }
                    pinode->dataLength = x - piSep;
                    pinode->dataValue  = (char*)MALLOC(pinode->dataLength);
                    memmove(pinode->dataValue, piSep, pinode->dataLength);

                    if (parent_node == NULL) {
                        if (doc->documentElement) {
                            toplevel = doc->documentElement;
                            while (toplevel->nextSibling) {
                                toplevel = toplevel->nextSibling;
                            }
                            toplevel->nextSibling   = (domNode*)pinode;
                            pinode->previousSibling = (domNode*)toplevel;
                        } else {
                            doc->documentElement = (domNode*)pinode;
                        }
                    } else {
                        if (parent_node->firstChild)  {
                            parent_node->lastChild->nextSibling = (domNode*)pinode;
                            pinode->previousSibling = parent_node->lastChild;
                            parent_node->lastChild = (domNode*)pinode;
                        } else {
                            parent_node->firstChild = parent_node->lastChild = (domNode*)pinode;
                        }
                    }
                    x += 2;
                } else {
                    RetError("Unterminated processing instruction(PI)",(start-html) );
                }
                continue;
            }


            /*----------------------------------------------------------------
            |   new tag/element
            |
            \---------------------------------------------------------------*/
            while ((c=*x)!=0 && c!='/' && c!='>' && c!='<' && !SPACE(c) ) {
                if (!isalnum(c)) goto readText;
                *x = tolower(c);
                x++;
            }
            hasContent = 1;
            if (c==0) {
                RetError("Missing \">\"",(start-html) );
            }
            if ( (x-start)==1) {
                RetError("Null markup name",(start-html) );
            }
            DBG(fprintf(stderr, "\nnew tag '%70.70s...' \n", start);)
            *x = '\0'; /* temporarily terminate the string */


            /*-----------------------------------------------------------
            |   check, whether new starting element close an other
            |   currently open one
            \----------------------------------------------------------*/
            e = start+1;
            pn = ""; if (parent_node) { pn = (char*)parent_node->nodeName; }
            autoclose = 0;
            switch (*e) {
                case 'a': if(!strcmp(e,"a")&&!strcmp(pn,"a")) autoclose=1;
                          break;
                case 'b': if(!strcmp(e,"b")&&!strcmp(pn,"b")) autoclose=1;
                          break;
                case 'p': if((!strcmp(e,"pre")&&!strcmp(pn,"pre")) 
                             ||(!strcmp(e,"p")&&!strcmp(pn,"p"))) autoclose=1;
                          break;
            }
            if (autoclose) {
                DBG(fprintf(stderr, "autoclose '%s' because of new '%s' \n", pn, e);)
                node = parent_node;
                parent_node = node->parentNode;
            }

            /*-----------------------------------------------------------
            |   create new DOM element node
            \----------------------------------------------------------*/
            h = Tcl_CreateHashEntry(&HASHTAB(doc,tagNames), e, &hnew);

            node = (domNode*) domAlloc(sizeof(domNode));
            memset(node, 0, sizeof(domNode));
            node->nodeType      = ELEMENT_NODE;
            node->nodeFlags     = 0;
            node->namespace     = 0;
            node->nodeName      = (char *)&(h->key);
            node->ownerDocument = doc;
            node->nodeNumber    = NODE_NO(doc);

            if (parent_node == NULL) {
                if (doc->documentElement) {
                    toplevel = doc->documentElement;
                    while (toplevel->nextSibling) {
                        toplevel = toplevel->nextSibling;
                    }
                    toplevel->nextSibling = node;
                    node->previousSibling = toplevel;
                }
                doc->documentElement = node;
            } else {
                node->parentNode = parent_node;
                if (parent_node->firstChild)  {
                    parent_node->lastChild->nextSibling = node;
                    node->previousSibling = parent_node->lastChild;
                    parent_node->lastChild = node;
                } else {
                    parent_node->firstChild = parent_node->lastChild = node;
                }
            }

            *x = c;  /* remove temporarily termination */

            while (SPACE(*x) ) {
                x++;
            }
            /*-----------------------------------------------------------
            |   read attribute name-value pairs
            \----------------------------------------------------------*/
            lastAttr = NULL;
            while ( (c=*x) && (c!='/') && (c!='>') && (c!='<') ) {
                char *ArgName = x;
                int nArgName;
                char *ArgVal = NULL;
                int nArgVal = 0;

                while ((c=*x)!=0 && c!='=' && c!='>' && !SPACE(c) ) {
                    x++;
                }
                nArgName = x - ArgName;
                
                if (*x == '=') {
                
                    /* attribute with value, like width="1234" */
                    
                    x++;
                    saved = *(ArgName + nArgName);
                    *(ArgName + nArgName) = '\0'; /* terminate arg name */

                    if (*x=='>' || *x==0) {
                        ArgVal = ArgName;
                        nArgVal = nArgName;
                    } else if ((c=*x)=='\"' || c=='\'') {
                        register int cDelim = c;
                        x++;
                        ArgVal = x;
                        ampersandSeen = 0;
                        while ((c=*x)!=0 && c!=cDelim) {
                            if (c=='&') ampersandSeen = 1;
                            x++;
                        }
                        nArgVal = x - ArgVal;
                        if (c==0) {
                            RetError("Unterminated string",(ArgVal - html - 1) );
                        } else {
                            x++;
                        }
                    } else if (c!=0 && c!='>') {
                        ArgVal = x;
                        while ((c=*x)!=0 && c!='>' && !SPACE(c)) {
                            if (c=='&') ampersandSeen = 1;
                            x++;
                        }
                        if (c==0) {
                            RetError("Missing \">\"",(start-html));
                        }
                        nArgVal = x - ArgVal;
                    }
                } else {
                    /* attribute without value, like 'nowrap' */
                    x++;
                    saved = *(ArgName + nArgName);
                    *(ArgName + nArgName) = '\0'; /* terminate arg name */
                    ArgVal = "1"; /* current hack */
                    nArgVal = 1;                
                }

                /*--------------------------------------------------
                |   allocate new attribute node
                \--------------------------------------------------*/
                h = Tcl_CreateHashEntry(&HASHTAB(doc,attrNames), ArgName, &hnew);
                attrnode = (domAttrNode*) domAlloc(sizeof(domAttrNode));
                memset(attrnode, 0, sizeof(domAttrNode));
                attrnode->parentNode  = node;
                attrnode->nodeName    = (char *)&(h->key);
                attrnode->nodeType    = ATTRIBUTE_NODE;
                attrnode->nodeFlags   = 0;
                attrnode->nodeValue   = (char*)MALLOC(nArgVal+1);
                attrnode->valueLength = nArgVal;
                memmove(attrnode->nodeValue, ArgVal, nArgVal);
                *(attrnode->nodeValue + nArgVal) = 0;
                if (ampersandSeen) {
                    TranslateEntityRefs(attrnode->nodeValue, &(attrnode->valueLength) );
                }
                if (node->firstAttr) {
                    lastAttr->nextSibling = attrnode;
                } else {
                    node->firstAttr = attrnode;
                }
                lastAttr = attrnode;

                *(ArgName + nArgName) = saved;

                while (SPACE(*x)) {
                    x++;
                }
            }

            /*-----------------------------------------------------------
            |   check for empty HTML tags
            \----------------------------------------------------------*/
            switch (node->nodeName[0]) {
                case 'a':  if (!strcmp(node->nodeName,"area"))     hasContent = 0; break;
                case 'b':  if (!strcmp(node->nodeName,"br")     ||
                               !strcmp(node->nodeName,"base")   ||
                               !strcmp(node->nodeName,"basefont")) hasContent = 0; break;
                case 'c':  if (!strcmp(node->nodeName,"col"))      hasContent = 0; break;
                case 'e':  if (!strcmp(node->nodeName,"embed"))    hasContent = 0; break; /*ext*/
                case 'f':  if (!strcmp(node->nodeName,"frame"))    hasContent = 0; break;
                case 'h':  if (!strcmp(node->nodeName,"hr"))       hasContent = 0; break;
                case 'i':  if (!strcmp(node->nodeName,"img")   ||
                               !strcmp(node->nodeName,"input") ||
                               !strcmp(node->nodeName,"isindex"))  hasContent = 0; break;
                case 'l':  if (!strcmp(node->nodeName,"link"))     hasContent = 0; break;
                case 'o':  if (!strcmp(node->nodeName,"option"))   hasContent = 0; break;
                case 'm':  if (!strcmp(node->nodeName,"meta"))     hasContent = 0; break;
                case 'p':  if (!strcmp(node->nodeName,"param"))    hasContent = 0; break;
                case 's':  if (!strcmp(node->nodeName,"spacer"))   hasContent = 0; break; /*ext*/
            }

            if (*x=='/') {
                hasContent = 0;
                x++;
                if (*x!='>') {
                    RetError("Syntax Error",(x - html - 1) );
                }
            }
            if (*x=='>') {
                x++;
            }
            DBG(fprintf(stderr, "%d: new node '%s' hasContent=%d \n", getDeep(node), node->nodeName, hasContent);)

            if ((strcmp(node->nodeName,"style" )==0) ||
                (strcmp(node->nodeName,"script")==0)
            ) {
                /*-----------------------------------------------------------
                |   read over any data within a 'style' or 'script' tag
                \----------------------------------------------------------*/
                hasContent = 1;
                start = x;
                while (1) {
                    while ( (*x!=0) && ((*x!='<') || (x[1]!='/'))) {
                        x++;
                    }
                    if (!*x) break;
                    y = x + 2;
                    while (*y!=0 && SPACE(*y)) y++;
                    if (TU(y[0]) == 'S' && 
                        TU(y[1]) == 'C' &&
                        TU(y[2]) == 'R' &&
                        TU(y[3]) == 'I' &&
                        TU(y[4]) == 'P' &&
                        TU(y[5]) == 'T' )  break;
                         
                    if (TU(y[0]) == 'S' && 
                        TU(y[1]) == 'T' &&
                        TU(y[2]) == 'Y' &&
                        TU(y[3]) == 'L' &&
                        TU(y[4]) == 'E' ) break;
                    x++;                                      
                }
                if (*x) {
                    /*----------------------------------------------------
                    |   allocate new TEXT node for style/script data
                    \---------------------------------------------------*/
                    tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                    memset(tnode, 0, sizeof(domTextNode));
                    tnode->nodeType      = TEXT_NODE;
                    tnode->nodeFlags     = 0;
                    tnode->namespace     = 0;
                    tnode->ownerDocument = doc;
                    tnode->nodeNumber    = NODE_NO(doc);
                    tnode->parentNode    = node;
                    tnode->valueLength   = (x - start);
                    tnode->nodeValue     = (char*)MALLOC((x - start)+1);
                    memmove(tnode->nodeValue, start, (x - start));
                    *(tnode->nodeValue + (x - start)) = 0;
                    if (node->firstChild)  {
                        node->lastChild->nextSibling = (domNode*)tnode;
                        tnode->previousSibling = node->lastChild;
                        node->lastChild = (domNode*)tnode;
                    } else {
                        node->firstChild = node->lastChild = (domNode*)tnode;
                    }
                }
            }
            if (hasContent) {
                /*------------------------------------------------------------
                |   recurs to read child tags/texts
                \-----------------------------------------------------------*/
                parent_node = node;
            }
            DBG(fprintf(stderr, "%d: after node '%s' \n", getDeep(node), node->nodeName);)
        }
    }

    while (parent_node != NULL) {

        pn = (char*)node->parentNode->nodeName;
        DBG(fprintf(stderr, "final autoclose '%s'? \n", pn);)
        /*---------------------------------------------------------------
        |   check for tags for which end tag can be omitted
        \--------------------------------------------------------------*/
        autoclose = 0;
        switch (pn[0]) {
            case 'b': if (!strcmp(pn,"body"))     autoclose = 1; break;
            case 'c': if (!strcmp(pn,"colgroup")) autoclose = 1; break;
            case 'd': if (!strcmp(pn,"dd") ||
                          !strcmp(pn,"dt"))       autoclose = 1; break;
            case 'h': if (!strcmp(pn,"head") ||
                          !strcmp(pn,"html"))     autoclose = 1; break;
            case 'l': if (!strcmp(pn,"li"))       autoclose = 1; break;
            case 'o': if (!strcmp(pn,"option"))   autoclose = 1; break;
            case 'p': if (!strcmp(pn,"p"))        autoclose = 1; break;
            case 't': if (!strcmp(pn,"tbody") ||
                          !strcmp(pn,"td")    ||
                          !strcmp(pn,"tfoot") ||
                          !strcmp(pn,"thead") ||
                          !strcmp(pn,"th")    ||
                          !strcmp(pn,"tr"))       autoclose = 1; break;
            case 'u': if (!strcmp(pn,"ul"))       autoclose = 1; break; /* ext */
        }
        if (!autoclose) break;
        DBG(fprintf(stderr, "final autoclosed '%s'! \n", pn);)
        node = node->parentNode;
        parent_node = node->parentNode;
    }
    if (parent_node == NULL) {
        /* we return to main node and so finished parsing */
        return TCL_OK;
    }
    RetError("Unexpected end",(x - html) );

} /* HTML_SimpleParse */


/*----------------------------------------------------------------------------
|   HTML_SimpleParseDocument
|
|       Create a document, parses the HTML string starting at 'pos' and
|       continuing to the first encountered error.
|
\---------------------------------------------------------------------------*/
domDocument *
HTML_SimpleParseDocument (
    char   *html,              /* Complete text of the file being parsed  */
    int     ignoreWhiteSpaces,
    int    *pos,
    char  **errStr
) {
    domDocument   *doc = domCreateDoc(NULL, 0);

    *pos = 0;
    HTML_SimpleParse (html, pos, doc, NULL, ignoreWhiteSpaces, errStr);

    doc->rootNode->firstChild 
        = doc->rootNode->lastChild
        = doc->documentElement;
    return doc;

} /* HTML_SimpleParseDocument */

