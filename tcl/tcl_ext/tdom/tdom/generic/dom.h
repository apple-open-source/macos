/*---------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+----------------------------------------------------------------------------
|
|   $Id: dom.h,v 1.54 2007/08/18 12:51:46 rolf Exp $
|
|
|   A DOM interface upon the expat XML parser for the C language
|   according to the W3C recommendation REC-DOM-Level-1-19981001
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
|   April 5, 1999
|
\--------------------------------------------------------------------------*/

#ifndef __DOM_H__
#define __DOM_H__

#include <tcl.h>
#include <ctype.h>
#include <expat.h>
#include <utf8conv.h>
#include <domalloc.h>

/*
 * tDOM provides it's own memory allocator which is optimized for
 * low heap usage. It uses the native Tcl allocator underneath,
 * though, but it is not very MT-friendly. Therefore, you might
 * use the (normal) Tcl allocator with USE_NORMAL_ALLOCATOR
 * defined during compile time. Actually, the symbols name is 
 * a misnomer. It should have benn called "USE_TCL_ALLOCATOR"
 * but I did not want to break any backward compatibility. 
 */

#ifndef USE_NORMAL_ALLOCATOR
# define MALLOC             malloc
# define FREE               free
# define REALLOC            realloc
# define tdomstrdup         strdup
#else
# define domAllocInit()
# define domAlloc           MALLOC 
# define domFree            FREE
# if defined(TCL_MEM_DEBUG) || defined(NS_AOLSERVER) 
#  define MALLOC            Tcl_Alloc
#  define FREE(a)           Tcl_Free((char*)(a))
#  define REALLOC           Tcl_Realloc
#  define tdomstrdup(s)     (char*)strcpy(MALLOC(strlen((s))+1),(char*)s)
# else    
#  define MALLOC            malloc
#  define FREE              free
#  define REALLOC           realloc
#  define tdomstrdup        strdup
# endif /* TCL_MEM_DEBUG */
#endif /* USE_NORMAL_ALLOCATOR */

#if defined(TCL_MEM_DEBUG) || defined(NS_AOLSERVER) 
   static void* my_malloc(size_t size){return Tcl_Alloc(size);}
   static void  my_free(void *ptr){Tcl_Free((char*)ptr);}
   static void* my_realloc(void *ptr,size_t size){return Tcl_Realloc(ptr,size);}
   static XML_Memory_Handling_Suite memsuite = {
       my_malloc, my_realloc, my_free
   };
#  define MEM_SUITE &memsuite
#else
#  define MEM_SUITE NULL
#endif

/*
 * Beginning with 8.4, Tcl API is CONST'ified
 */
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION <= 3)
# define CONST84
#endif

/*
 * Starting with Tcl 8.2 the Tcl_Panic() is defined properly
 * over the stubs table.
 * Also, we have a proper Tcl_GetString() shortcut afterwards.
 */
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION < 2)
# define Tcl_Panic panic
# define Tcl_GetString(a) Tcl_GetStringFromObj((a), NULL)
#endif

#define domPanic(msg) Tcl_Panic((msg));

/*
 * If compiled against threaded Tcl core, we must take
 * some extra care about process-wide globals and the
 * way we name Tcl object accessor commands.
 */
#ifndef TCL_THREADS
  extern unsigned long domUniqueNodeNr;
  extern unsigned long domUniqueDocNr;
  extern Tcl_HashTable tdom_tagNames;
  extern Tcl_HashTable tdom_attrNames;
# define TDomNotThreaded(x) x
# define TDomThreaded(x)
# define HASHTAB(doc,tab)   tab
# define NODE_NO(doc)       ++domUniqueNodeNr
# define DOC_NO(doc)        ++domUniqueDocNr
#else
# define TDomNotThreaded(x)
# define TDomThreaded(x)    x
# define HASHTAB(doc,tab)   (doc)->tab
# define NODE_NO(doc)       ((doc)->nodeCounter)++
# define DOC_NO(doc)        (unsigned long)(doc)
#endif /* TCL_THREADS */

#define DOC_CMD(s,doc)      sprintf((s), "domDoc%p", (doc))
#define NODE_CMD(s,node)    sprintf((s), "domNode%p", (node))
#define XSLT_CMD(s,doc)     sprintf((s), "XSLTcmd%p", (doc))

#define XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"
#define XMLNS_NAMESPACE "http://www.w3.org/2000/xmlns"

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0) || TCL_MAJOR_VERSION < 8
#define TclOnly8Bits 1
#else
#define TclOnly8Bits 0
#endif

#define UTF8_1BYTE_CHAR(c) ( 0    == ((c) & 0x80))
#define UTF8_2BYTE_CHAR(c) ( 0xC0 == ((c) & 0xE0))
#define UTF8_3BYTE_CHAR(c) ( 0xE0 == ((c) & 0xF0))
#define UTF8_4BYTE_CHAR(c) ( 0xF0 == ((c) & 0xF8))

#if TclOnly8Bits
#define UTF8_CHAR_LEN(c) 1
#else
#define UTF8_CHAR_LEN(c) \
  UTF8_1BYTE_CHAR((c)) ? 1 : \
   (UTF8_2BYTE_CHAR((c)) ? 2 : \
     (UTF8_3BYTE_CHAR((c)) ? 3 : 0))
#endif

/* The following 2 defines are out of the expat code */

/* A 2 byte UTF-8 representation splits the characters 11 bits
between the bottom 5 and 6 bits of the bytes.
We need 8 bits to index into pages, 3 bits to add to that index and
5 bits to generate the mask. */
#define UTF8_GET_NAMING2(pages, byte) \
    (namingBitmap[((pages)[(((byte)[0]) >> 2) & 7] << 3) \
                      + ((((byte)[0]) & 3) << 1) \
                      + ((((byte)[1]) >> 5) & 1)] \
         & (1 << (((byte)[1]) & 0x1F)))

/* A 3 byte UTF-8 representation splits the characters 16 bits
between the bottom 4, 6 and 6 bits of the bytes.
We need 8 bits to index into pages, 3 bits to add to that index and
5 bits to generate the mask. */
#define UTF8_GET_NAMING3(pages, byte) \
  (namingBitmap[((pages)[((((byte)[0]) & 0xF) << 4) \
                             + ((((byte)[1]) >> 2) & 0xF)] \
                       << 3) \
                      + ((((byte)[1]) & 3) << 1) \
                      + ((((byte)[2]) >> 5) & 1)] \
         & (1 << (((byte)[2]) & 0x1F)))

#define UTF8_GET_NAMING_NMTOKEN(p, n) \
  ((n) == 1 \
  ? nameChar7Bit[(int)(*(p))] \
  : ((n) == 2 \
    ? UTF8_GET_NAMING2(namePages, (const unsigned char *)(p)) \
    : ((n) == 3 \
      ? UTF8_GET_NAMING3(namePages, (const unsigned char *)(p)) \
      : 0)))

#define UTF8_GET_NAMING_NCNMTOKEN(p, n) \
  ((n) == 1 \
  ? NCnameChar7Bit[(int)(*(p))] \
  : ((n) == 2 \
    ? UTF8_GET_NAMING2(namePages, (const unsigned char *)(p)) \
    : ((n) == 3 \
      ? UTF8_GET_NAMING3(namePages, (const unsigned char *)(p)) \
      : 0)))

#define UTF8_GET_NAME_START(p, n) \
  ((n) == 1 \
  ? nameStart7Bit[(int)(*(p))] \
  : ((n) == 2 \
    ? UTF8_GET_NAMING2(nmstrtPages, (const unsigned char *)(p)) \
    : ((n) == 3 \
      ? UTF8_GET_NAMING3(nmstrtPages, (const unsigned char *)(p)) \
      : 0)))

#define UTF8_GET_NCNAME_START(p, n) \
  ((n) == 1 \
  ? NCnameStart7Bit[(int)(*(p))] \
  : ((n) == 2 \
    ? UTF8_GET_NAMING2(nmstrtPages, (const unsigned char *)(p)) \
    : ((n) == 3 \
      ? UTF8_GET_NAMING3(nmstrtPages, (const unsigned char *)(p)) \
      : 0)))

#if TclOnly8Bits 
#  define UTF8_XMLCHAR(p,n) \
 (*(p) < 0x80 ? CharBit[(int)(*(p))] : 1)
#else  
#  define UTF8_XMLCHAR3(p) \
  (*(p) == 0xED  \
   ? ((p)[1] < 0xA0 ? 1 : 0) \
   : (*(p) == 0xEF \
      ? ((p)[1] == 0xBF \
         ? ((p)[2] == 0xBE || (p)[2] == 0xBF ? 0 : 1) \
         : 1) \
      : 1)) \
    
#  define UTF8_XMLCHAR(p, n) \
  ((n) == 1 \
  ? CharBit[(int)(*(p))] \
  : ((n) == 2 \
    ? 1 \
    : ((n) == 3 \
      ? (UTF8_XMLCHAR3(p)) \
      : 0)))
#endif

#include "../expat/nametab.h"

static const unsigned char nameChar7Bit[] = {
/* 0x00 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x10 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x20 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x28 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
/* 0x30 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x38 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x40 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x48 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x50 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x58 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
/* 0x60 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x68 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x70 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x78 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char NCnameChar7Bit[] = {
/* 0x00 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x10 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x20 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x28 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
/* 0x30 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x38 */    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x40 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x48 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x50 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x58 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
/* 0x60 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x68 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x70 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x78 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const unsigned char nameStart7Bit[] = {
/* 0x00 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x10 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x20 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x28 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x30 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x38 */    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x40 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x48 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x50 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x58 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
/* 0x60 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x68 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x70 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x78 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const unsigned char NCnameStart7Bit[] = {
/* 0x00 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x10 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x20 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x28 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x30 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x38 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x40 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x48 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x50 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x58 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
/* 0x60 */    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x68 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x70 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x78 */    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char CharBit[] = {
/* 0x00 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */    0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
/* 0x10 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x20 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x28 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x30 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x38 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x40 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x48 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x50 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x58 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x60 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x68 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x70 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
/* 0x78 */    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
};


#if TclOnly8Bits == 1
#  define isNameStart(x)   (isalpha(*x) || ((*x)=='_') || ((*x)==':'))
#  define isNameChar(x)    (isalnum(*x)  || ((*x)=='_') || ((*x)=='-') || ((*x)=='.') || ((*x)==':'))
#  define isNCNameStart(x) (isalpha(*x) || ((*x)=='_'))
#  define isNCNameChar(x)  (isalnum(*x)  || ((*x)=='_') || ((*x)=='-') || ((*x)=='.'))
#else
#  define isNameStart(x)   UTF8_GET_NAME_START((x),UTF8_CHAR_LEN(*(x)))
#  define isNCNameStart(x) UTF8_GET_NCNAME_START((x),UTF8_CHAR_LEN(*(x)))
#  define isNameChar(x)    UTF8_GET_NAMING_NMTOKEN((x),UTF8_CHAR_LEN(*(x)))
#  define isNCNameChar(x)  UTF8_GET_NAMING_NCNMTOKEN((x),UTF8_CHAR_LEN(*(x)))
#endif

#define IS_XML_WHITESPACE(c)  ((c)==' ' || (c)=='\n' || (c)=='\r' || (c)=='\t')

/*--------------------------------------------------------------------------
|   DOMString
|
\-------------------------------------------------------------------------*/
typedef char* domString;   /* should 16-bit unicode character !!*/


/*--------------------------------------------------------------------------
|   domNodeType
|
\-------------------------------------------------------------------------*/
#if defined(_AIX) 
#    define    ELEMENT_NODE                 1
#    define    ATTRIBUTE_NODE               2
#    define    TEXT_NODE                    3
#    define    CDATA_SECTION_NODE           4
#    define    ENTITY_REFERENCE_NODE        5
#    define    ENTITY_NODE                  6
#    define    PROCESSING_INSTRUCTION_NODE  7
#    define    COMMENT_NODE                 8
#    define    DOCUMENT_NODE                9
#    define    DOCUMENT_TYPE_NODE           10
#    define    DOCUMENT_FRAGMENT_NODE       11
#    define    NOTATION_NODE                12
#    define    ALL_NODES                    100

#    define    domNodeType                  int

#else 

typedef enum {

    ELEMENT_NODE                = 1,
    ATTRIBUTE_NODE              = 2,
    TEXT_NODE                   = 3,
    CDATA_SECTION_NODE          = 4,
    ENTITY_REFERENCE_NODE       = 5,
    ENTITY_NODE                 = 6,
    PROCESSING_INSTRUCTION_NODE = 7,
    COMMENT_NODE                = 8,
    DOCUMENT_NODE               = 9,
    DOCUMENT_TYPE_NODE          = 10,
    DOCUMENT_FRAGMENT_NODE      = 11,
    NOTATION_NODE               = 12,
    ALL_NODES                   = 100
} domNodeType;

#endif

/*--------------------------------------------------------------------------
|   flags   -  indicating some internal features about nodes
|
\-------------------------------------------------------------------------*/
typedef unsigned int domNodeFlags;

#define HAS_LINE_COLUMN           1
#define VISIBLE_IN_TCL            2
#define IS_DELETED                4
#define HAS_BASEURI               8
#define DISABLE_OUTPUT_ESCAPING  16

typedef unsigned int domAttrFlags;

#define IS_ID_ATTRIBUTE           1
#define IS_NS_NODE                2

typedef unsigned int domDocFlags;

#define OUTPUT_DEFAULT_INDENT     1
#define NEEDS_RENUMBERING         2
#define DONT_FREE                 4

/*--------------------------------------------------------------------------
|   a index to the namespace records
|
\-------------------------------------------------------------------------*/
typedef unsigned int domNameSpaceIndex;



/*--------------------------------------------------------------------------
|   domException
|
\-------------------------------------------------------------------------*/
typedef enum {

    OK                          = 0,
    INDEX_SIZE_ERR              = 1,
    DOMSTRING_SIZE_ERR          = 2,
    HIERARCHY_REQUEST_ERR       = 3,
    WRONG_DOCUMENT_ERR          = 4,
    INVALID_CHARACTER_ERR       = 5,
    NO_DATA_ALLOWED_ERR         = 6,
    NO_MODIFICATION_ALLOWED_ERR = 7,
    NOT_FOUND_ERR               = 8,
    NOT_SUPPORTED_ERR           = 9,
    INUSE_ATTRIBUTE_ERR         = 10

} domException;

/*--------------------------------------------------------------------------
|   domDocInfo
|
\-------------------------------------------------------------------------*/
typedef struct domDocInfo {
    
    /* 'name' is always the name of the documentElement, no struct element
       needed for this */
    domString      publicId;
    domString      systemId;
    domString      internalSubset;
    /* Currently missing, according to DOM 2: 'entities' and 'notations'. */
    /* The following struct elements describes additional 'requested'
       facets of the document, following the xslt rec, section 16 */
    float          version;
    char          *encoding;
    int            omitXMLDeclaration;
    int            standalone;
    Tcl_HashTable *cdataSectionElements;
    domString      method;
    domString      mediaType;
    
} domDocInfo;

/*--------------------------------------------------------------------------
|   domDocument
|
\-------------------------------------------------------------------------*/
typedef struct domDocument {

    domNodeType       nodeType  : 8;
    domDocFlags       nodeFlags : 8;
    domNameSpaceIndex dummy     : 16;
    unsigned long     documentNumber;
    struct domNode   *documentElement;
    struct domNode   *fragments;
#ifdef TCL_THREADS
    struct domNode   *deletedNodes;
#endif
    struct domNS    **namespaces;
    int               nsptr;
    int               nslen;
    char            **prefixNSMappings; /* Stores doc global prefix ns
                                           mappings for resolving of
                                           prefixes in seletNodes expr */
#ifdef TCL_THREADS
    unsigned int      nodeCounter;
#endif
    struct domNode   *rootNode;
    Tcl_HashTable    *ids;
    Tcl_HashTable    *unparsedEntities;
    Tcl_HashTable    *baseURIs;
    Tcl_HashTable    *xpathCache;
    char             *extResolver;
    domDocInfo       *doctype;
    TDomThreaded (
        Tcl_HashTable tdom_tagNames;   /* Names of tags found in doc */
        Tcl_HashTable tdom_attrNames;  /* Names of tag attributes */
        unsigned int  refCount;        /* # of object commands attached */
        struct _domlock *lock;          /* Lock for this document */
    )
} domDocument;

/*--------------------------------------------------------------------------
|  domLock
|
\-------------------------------------------------------------------------*/

#ifdef TCL_THREADS
typedef struct _domlock {
    domDocument* doc;           /* The DOM document to be locked */
    int numrd;	                /* # of readers waiting for lock */
    int numwr;                  /* # of writers waiting for lock */
    int lrcnt;                  /* Lock ref count, > 0: # of shared
                                 * readers, -1: exclusive writer */
    Tcl_Mutex mutex;            /* Mutex for serializing access */
    Tcl_Condition rcond;        /* Condition var for reader locks */
    Tcl_Condition wcond;        /* Condition var for writer locks */
    struct _domlock *next;       /* Next doc lock in global list */
} domlock;

#define LOCK_READ  0
#define LOCK_WRITE 1

#endif


/*--------------------------------------------------------------------------
|   namespace
|
\-------------------------------------------------------------------------*/
typedef struct domNS {

   char   *uri;
   char   *prefix;
   int     index;

} domNS;


#define MAX_PREFIX_LEN   80



/*--------------------------------------------------------------------------
|   domLineColumn
|
\-------------------------------------------------------------------------*/
typedef struct domLineColumn {

    int   line;
    int   column;

} domLineColumn;


/*--------------------------------------------------------------------------
|   domNode
|
\-------------------------------------------------------------------------*/
typedef struct domNode {

    domNodeType         nodeType  : 8;
    domNodeFlags        nodeFlags : 8;
    domNameSpaceIndex   namespace : 8;
    unsigned int        info      : 8;
    unsigned int        nodeNumber;
    domDocument        *ownerDocument;
    struct domNode     *parentNode;
    struct domNode     *previousSibling;
    struct domNode     *nextSibling;

    domString           nodeName;  /* now the element node specific fields */
    struct domNode     *firstChild;
    struct domNode     *lastChild;
#ifdef TCL_THREADS
    struct domNode     *nextDeleted;
#endif
    struct domAttrNode *firstAttr;

} domNode;

/*--------------------------------------------------------------------------
|   domDeleteInfo
|
\-------------------------------------------------------------------------*/

typedef struct domDeleteInfo {
    domDocument * document;
    domNode     * node;
    Tcl_Interp  * interp;
    char        * traceVarName;
} domDeleteInfo;


/*--------------------------------------------------------------------------
|   domTextNode
|
\-------------------------------------------------------------------------*/
typedef struct domTextNode {

    domNodeType         nodeType  : 8;
    domNodeFlags        nodeFlags : 8;
    domNameSpaceIndex   namespace : 8;
    unsigned int        info      : 8;
    unsigned int        nodeNumber;
    domDocument        *ownerDocument;
    struct domNode     *parentNode;
    struct domNode     *previousSibling;
    struct domNode     *nextSibling;

    domString           nodeValue;   /* now the text node specific fields */
    int                 valueLength;

} domTextNode;


/*--------------------------------------------------------------------------
|   domProcessingInstructionNode
|
\-------------------------------------------------------------------------*/
typedef struct domProcessingInstructionNode {

    domNodeType         nodeType  : 8;
    domNodeFlags        nodeFlags : 8;
    domNameSpaceIndex   namespace : 8;
    unsigned int        info      : 8;
    unsigned int        nodeNumber;
    domDocument        *ownerDocument;
    struct domNode     *parentNode;
    struct domNode     *previousSibling;
    struct domNode     *nextSibling;

    domString           targetValue;   /* now the pi specific fields */
    int                 targetLength;
    domString           dataValue;
    int                 dataLength;

} domProcessingInstructionNode;


/*--------------------------------------------------------------------------
|   domAttrNode
|
\-------------------------------------------------------------------------*/
typedef struct domAttrNode {

    domNodeType         nodeType  : 8;
    domAttrFlags        nodeFlags : 8;
    domNameSpaceIndex   namespace : 8;
    unsigned int        info      : 8;
    domString           nodeName;
    domString           nodeValue;
    int                 valueLength;
    struct domNode     *parentNode;
    struct domAttrNode *nextSibling;

} domAttrNode;

/*--------------------------------------------------------------------------
|   domAddCallback
|
\-------------------------------------------------------------------------*/
typedef int  (*domAddCallback)  (domNode * node, void * clientData);
typedef void (*domFreeCallback) (domNode * node, void * clientData);

/*--------------------------------------------------------------------------
|   Function prototypes
|
\-------------------------------------------------------------------------*/
const char *   domException2String (domException expection);


void           domModuleInitialize (void);
domDocument *  domCreateDoc (const char *baseURI, int storeLineColumn);
domDocument *  domCreateDocument (Tcl_Interp *interp, const char *uri,
                                  char *documentElementTagName);
void           domSetDocumentElement (domDocument *doc);

domDocument *  domReadDocument   (XML_Parser parser,
                                  char *xml,
                                  int   length,
                                  int   ignoreWhiteSpaces,
                                  TEncoding *encoding_8bit,
                                  int   storeLineColumn,
                                  int   feedbackAfter,
                                  Tcl_Channel channel,
                                  const char *baseurl,
                                  char *extResolver,
                                  int   useForeignDTD,
                                  int   paramEntityParsing,
                                  Tcl_Interp *interp);

void           domFreeDocument   (domDocument *doc, 
                                  domFreeCallback freeCB, 
                                  void * clientData);

void           domFreeNode       (domNode *node, 
                                  domFreeCallback freeCB, 
                                  void *clientData, 
                                  int dontfree);

domTextNode *  domNewTextNode    (domDocument *doc,
                                  const char  *value,
                                  int          length,
                                  domNodeType  nodeType);

domNode *      domNewElementNode (domDocument *doc,
                                  const char  *tagName,
                                  domNodeType  nodeType);
		
domNode *      domNewElementNodeNS (domDocument *doc,
                                    const char  *tagName,
                                    const char  *uri,
                                    domNodeType  nodeType);

domProcessingInstructionNode * domNewProcessingInstructionNode (
                                  domDocument *doc,
                                  const char  *targetValue,
                                  int          targetLength,
                                  const char  *dataValue,
                                  int          dataLength);

domAttrNode *  domSetAttribute (domNode *node, const char *attributeName,
                                               const char *attributeValue);

domAttrNode *  domSetAttributeNS (domNode *node, const char *attributeName,
                                                 const char *attributeValue,
                                                 const char *uri,
                                                 int   createNSIfNeeded);
domAttrNode *  domGetAttributeNodeNS (domNode *node, const char *uri, 
                                                     const char *localname);

int            domRemoveAttribute (domNode *node, const char *attributeName);
int            domRemoveAttributeNS (domNode *node, const char *uri,
                                     const char *localName);
domNode *      domPreviousSibling (domNode *attr);
domException   domDeleteNode   (domNode *node, domFreeCallback freeCB, void *clientData);
domException   domRemoveChild  (domNode *node, domNode *childToRemove);
domException   domAppendChild  (domNode *node, domNode *childToAppend);
domException   domInsertBefore (domNode *node, domNode *childToInsert, domNode *refChild);
domException   domReplaceChild (domNode *node, domNode *newChild, domNode *oldChild);
domException   domSetNodeValue (domNode *node, const char *nodeValue,
                                int valueLen);
domNode *      domCloneNode (domNode *node, int deep);

domTextNode *  domAppendNewTextNode (domNode *parent, char *value, int length, domNodeType nodeType, int disableOutputEscaping);
domNode *      domAppendNewElementNode (domNode *parent, const char *tagName,
                                        const char *uri);
domNode *      domAppendLiteralNode (domNode *parent, domNode *node);
domNS *        domAddNSToNode (domNode *node, domNS *nsToAdd);
const char *   domNamespacePrefix (domNode *node);
const char *   domNamespaceURI    (domNode *node);
const char *   domGetLocalName    (const char *nodeName);
int            domSplitQName (const char *name, char *prefix,
                              const char **localName);
domNS *        domLookupNamespace (domDocument *doc, const char *prefix, 
                                   const char *namespaceURI);
domNS *        domLookupPrefix  (domNode *node, const char *prefix);
const char *   domLookupPrefixWithMappings (domNode *node, const char *prefix,
                                            char **prefixMappings);
domNS *        domLookupURI     (domNode *node, char *uri);
domNS *        domGetNamespaceByIndex (domDocument *doc, int nsIndex);
domNS *        domNewNamespace (domDocument *doc, const char *prefix,
                                const char *namespaceURI);
int            domGetLineColumn (domNode *node, int *line, int *column);

int            domXPointerChild (domNode * node, int all, int instance, domNodeType type,
                                 char *element, char *attrName, char *attrValue,
                                 int attrLen, domAddCallback addCallback,
                                 void * clientData);

int            domXPointerDescendant (domNode * node, int all, int instance,
                                      int * i, domNodeType type, char *element,
                                      char *attrName, char *attrValue, int attrLen,
                                      domAddCallback addCallback, void * clientData);

int            domXPointerAncestor (domNode * node, int all, int instance,
                                    int * i, domNodeType type, char *element,
                                    char *attrName, char *attrValue, int attrLen,
                                    domAddCallback addCallback, void * clientData);

int            domXPointerXSibling (domNode * node, int forward_mode, int all, int instance,
                                    domNodeType type, char *element, char *attrName,
                                    char *attrValue, int attrLen,
                                    domAddCallback addCallback, void * clientData);

const char *   findBaseURI (domNode *node);

void           tcldom_tolower (const char *str, char *str_out, int  len);
int            domIsNAME (const char *name);
int            domIsPINAME (const char *name);
int            domIsQNAME (const char *name);
int            domIsNCNAME (const char *name);
int            domIsChar (const char *str);
int            domIsComment (const char *str);
int            domIsCDATA (const char *str);
int            domIsPIValue (const char *str);
void           domCopyTo (domNode *node, domNode *parent, int copyNS);
void           domCopyNS (domNode *from, domNode *to);
domAttrNode *  domCreateXMLNamespaceNode (domNode *parent);
void           domRenumberTree (domNode *node);
int            domPrecedes (domNode *node, domNode *other);
void           domNormalize (domNode *node, int forXPath, 
                             domFreeCallback freeCB, void *clientData);
domException   domAppendData (domTextNode *node, char *value, int length, 
                              int disableOutputEscaping);

#ifdef TCL_THREADS
void           domLocksLock(domlock *dl, int how);
void           domLocksUnlock(domlock *dl);
void           domLocksAttach(domDocument *doc);
void           domLocksDetach(domDocument *doc);
void           domLocksFinalize(ClientData dummy);
#endif

/*---------------------------------------------------------------------------
|   coercion routines for calling from C++
|
\--------------------------------------------------------------------------*/
domAttrNode                  * coerceToAttrNode( domNode *n );
domTextNode                  * coerceToTextNode( domNode *n );
domProcessingInstructionNode * coerceToProcessingInstructionNode( domNode *n );


#endif

