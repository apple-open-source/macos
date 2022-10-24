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
#else
#undef LIBXML_HAS_XPOINTER_LOCATIONS_DISABLED_AT_RUNTIME
#endif
#else
#undef LIBXML_HAS_XPOINTER_LOCATIONS_DISABLED_AT_RUNTIME
#endif /* __APPLE__ */

#endif /* __XML_VERSION_INTERNAL_H__ */
