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

#include <stdbool.h>
#include "platform.h"
#include "tidy.h"

#ifdef __APPLE__
#include <TargetConditionals.h>

#if (defined(TARGET_OS_MAC) && TARGET_OS_MAC && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150400) \
    || (defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180400) \
    || (defined(TARGET_OS_IOS) && TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180400) \
    || (defined(TARGET_OS_TV) && TARGET_OS_TV && __TV_OS_VERSION_MAX_ALLOWED >= 180400) \
    || (defined(TARGET_OS_WATCH) && TARGET_OS_WATCH && __WATCH_OS_VERSION_MAX_ALLOWED >= 110400) \
    || (defined(TARGET_OS_VISION) && TARGET_OS_VISION && __VISION_OS_VERSION_MAX_ALLOWED >= 20400)
#define TIDY_DEPRECATE_ALLOC_API
#define TIDY_LINKED_ON_OR_AFTER_MACOS15_4_IOS18_4_TVOS18_4_VISIONOS2_4_WATCHOS11_4
#else
#undef TIDY_DEPRECATE_ALLOC_API
#undef TIDY_LINKED_ON_OR_AFTER_MACOS15_4_IOS18_4_TVOS18_4_VISIONOS2_4_WATCHOS11_4
#endif
#else
#undef TIDY_DEPRECATE_ALLOC_API
#undef TIDY_LINKED_ON_OR_AFTER_MACOS15_4_IOS18_4_TVOS18_4_VISIONOS2_4_WATCHOS11_4
#endif /* defined(__APPLE__) */

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

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DMALLOC)
#include "dmalloc.h"
#endif

extern bool linkedOnOrAfter2024EReleases(void);

#ifdef TIDY_DEPRECATE_ALLOC_API
#define TY_MEM(str) _##str
#define MemAlloc(size) (linkedOnOrAfter2024EReleases() ? (malloc(size) ?: (FatalError("Out of memory!"), NULL)) : _MemAlloc(size))
#define MemRealloc(ptr, size) (linkedOnOrAfter2024EReleases() ? (realloc(ptr, size) ?: (FatalError("Out of memory!"), NULL)) : _MemRealloc(ptr, size))
#define MemFree(ptr) (linkedOnOrAfter2024EReleases() ? (free((void*)ptr), (ptr) = NULL) : _MemFree((void*)ptr))
#else
#define TY_MEM(str) str
#endif

void *TY_MEM(MemAlloc)(size_t size);
void *TY_MEM(MemRealloc)(void *mem, size_t newsize);
void TY_MEM(MemFree)(void *mem);
void ClearMemory(void *, size_t size);
void FatalError( ctmbstr msg );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __FORWARD_H__ */
