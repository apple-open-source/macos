/*
 * Summary: set of internal utilities for the XSLT engine
 * Description: internal interfaces for the utilities module of the XSLT engine.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: David Kilzer <ddkilzer@apple.com>
 */

#ifndef __XML_XSLTUTILSINTERNAL_H__
#define __XML_XSLTUTILSINTERNAL_H__

#include <stdbool.h>
#include "xsltexports.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __clang_tapi__
bool linkedOnOrAfterFall2022OSVersions(void);
#endif

#ifdef __APPLE__
#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000 \
    || defined(__TV_OS_VERSION_MIN_REQUIRED) && __TV_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__WATCH_OS_VERSION_MIN_REQUIRED) && __WATCH_OS_VERSION_MIN_REQUIRED >= 90000
#define LIBXSLT_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#else
#undef LIBXSLT_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#endif
#else /* __APPLE__ */
#undef LIBXSLT_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#endif /* __APPLE__ */

XSLTPUBFUN bool linkedOnOrAfterFall2023OSVersions(void);

#ifdef __APPLE__
#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 170000 \
    || defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 140000 \
    || defined(__TV_OS_VERSION_MIN_REQUIRED) && __TV_OS_VERSION_MIN_REQUIRED >= 170000 \
    || defined(__WATCH_OS_VERSION_MIN_REQUIRED) && __WATCH_OS_VERSION_MIN_REQUIRED >= 100000
#define LIBXSLT_LINKED_ON_OR_AFTER_MACOS14_IOS17_WATCHOS10_TVOS17
#else
#undef LIBXSLT_LINKED_ON_OR_AFTER_MACOS14_IOS17_WATCHOS10_TVOS17
#endif
#else /* __APPLE__ */
#undef LIBXSLT_LINKED_ON_OR_AFTER_MACOS14_IOS17_WATCHOS10_TVOS17
#endif /* __APPLE__ */

/* Moved internal declarations from xsltutils.h. */

#ifdef LIBXSLT_API_FOR_MACOS14_IOS17_WATCHOS10_TVOS17

#define XSLT_SOURCE_NODE_MASK       15u
#define XSLT_SOURCE_NODE_HAS_KEY    1u
#define XSLT_SOURCE_NODE_HAS_ID     2u
#ifndef __clang_tapi__
int
xsltGetSourceNodeFlags(xmlNodePtr node);
int
xsltSetSourceNodeFlags(xsltTransformContextPtr ctxt, xmlNodePtr node,
                       int flags);
int
xsltClearSourceNodeFlags(xmlNodePtr node, int flags);
void **
xsltGetPSVIPtr(xmlNodePtr cur);
#endif /* __clang_tapi__ */

#endif /* LIBXSLT_API_FOR_MACOS14_IOS17_WATCHOS10_TVOS17 */

#ifdef __cplusplus
}
#endif

#endif /* __XML_XSLTUTILSINTERNAL_H__ */
