// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* shareddateformatsymbols.h
*/

#ifndef __SHARED_DATEFORMATSYMBOLS_H__
#define __SHARED_DATEFORMATSYMBOLS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "sharedobject.h"
#include "unicode/dtfmtsym.h"
#include "unifiedcache.h"

U_NAMESPACE_BEGIN


class U_I18N_API_CLASS SharedDateFormatSymbols : public SharedObject {
public:
    U_I18N_API SharedDateFormatSymbols(const Locale& loc, const char* type, UErrorCode& status)
            : dfs(loc, type, status) { }
    U_I18N_API virtual ~SharedDateFormatSymbols();
    U_I18N_API const DateFormatSymbols& get() const { return dfs; }
private:
    DateFormatSymbols dfs;
    SharedDateFormatSymbols(const SharedDateFormatSymbols &) = delete;
    SharedDateFormatSymbols &operator=(const SharedDateFormatSymbols &) = delete;
#if APPLE_ICU_CHANGES && U_PLATFORM_IS_DARWIN_BASED // rdar://165873670
    // The allocator adds uninitialized padding bytes here,
    // so let's initialize them.
    // (See the message text for the static_assert below.)
#if __LP64__ // 64 bit
    uint8_t padding_bytes[232] = {};
#else // 32 bit
    uint8_t padding_bytes[240] = {};
#endif
#endif
};

#if APPLE_ICU_CHANGES && U_PLATFORM_IS_DARWIN_BASED // rdar://165873670
static_assert(sizeof(SharedDateFormatSymbols) % 256 == 0, "SharedDateFormatSymbols' size must be a multiple of 256 to make sizeof() == malloc_size(), thus allowing us to ensure the allocated memory is fully initialized. Comment out padding_bytes[] and compile again. This assert will show 'Expression evaluates to x == 0'. Now uncomment padding_bytes[] and use (256 - x) for the length.");
#endif

template<> U_I18N_API
const SharedDateFormatSymbols *
        LocaleCacheKey<SharedDateFormatSymbols>::createObject(
            const void * /*unusedContext*/, UErrorCode &status) const;

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif
