/*
 * xmlversionInternal.h: libxml2 utility functions for versioning
 *
 * For license and disclaimer see the license and disclaimer of
 * libxml2.
 *
 * ddkilzer@apple.com
 */

#ifndef __XML_VERSION_INTERNAL_H__
#define __XML_VERSION_INTERNAL_H__

#include <stdbool.h>
#include <libxml/xmlversion.h>

extern bool linkedOnOrAfterFall2022OSVersions(void);

#ifdef __APPLE__
#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000 \
    || defined(__TV_OS_VERSION_MIN_REQUIRED) && __TV_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__WATCH_OS_VERSION_MIN_REQUIRED) && __WATCH_OS_VERSION_MIN_REQUIRED >= 90000
#define LIBXML_HAS_XPOINTER_LOCATIONS_DISABLED_AT_RUNTIME
#define LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#else
#undef LIBXML_HAS_XPOINTER_LOCATIONS_DISABLED_AT_RUNTIME
#undef LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#endif
#else /* __APPLE__ */
#undef LIBXML_HAS_XPOINTER_LOCATIONS_DISABLED_AT_RUNTIME
#undef LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#endif /* __APPLE__ */

#endif /* __XML_VERSION_INTERNAL_H__ */
