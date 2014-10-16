#ifndef __FORWARD_H__
#define __FORWARD_H__

/* forward.h -- Forward declarations for major Tidy structures

  (c) 1998-2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

  Avoids many include file circular dependencies.

  Try to keep this file down to the minimum to avoid
  cross-talk between modules.

  Header files include this file.  C files include tidy-int.h.

*/

#include "platform.h"
#include "tidy.h"

/* Internal symbols are prefixed to avoid clashes with other libraries */
#define TYDYAPPEND(str1,str2) str1##str2
#define TY_(str) TYDYAPPEND(prvTidy,str)

/* Apple Inc. Changes:
   2007-01-29 iccir Do not prefix symbols
*/
#ifdef TIDY_APPLE_CHANGES
#undef TY_
#define TY_(str) str
#endif

struct _StreamIn;
typedef struct _StreamIn StreamIn;

struct _StreamOut;
typedef struct _StreamOut StreamOut;

struct _TidyDocImpl;
typedef struct _TidyDocImpl TidyDocImpl;


struct _Dict;
typedef struct _Dict Dict;

struct _Attribute;
typedef struct _Attribute Attribute;

struct _AttVal;
typedef struct _AttVal AttVal;

struct _Node;
typedef struct _Node Node;

struct _IStack;
typedef struct _IStack IStack;

struct _Lexer;
typedef struct _Lexer Lexer;



#endif /* __FORWARD_H__ */
