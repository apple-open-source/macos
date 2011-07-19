/*
*****************************************************************************************
* Copyright (C) 2010 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uplrule.h"
#include "unicode/plurrule.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"

U_NAMESPACE_USE

U_CAPI UPluralRules* U_EXPORT2
uplrule_open(const char *locale,
              UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status)) {
        return 0;
    }
	return (UPluralRules*)PluralRules::forLocale(Locale(locale), *status);
}

U_CAPI void U_EXPORT2
uplrule_close(UPluralRules *plrules)
{
  delete (PluralRules*)plrules;
}

U_CAPI int32_t U_EXPORT2
uplrule_select(const UPluralRules *plrules,
               int32_t number,
               UChar *keyword, int32_t capacity,
               UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status)) {
        return 0;
    }
    UnicodeString result = ((PluralRules*)plrules)->select(number);
	return result.extract(keyword, capacity, *status);
}

U_CAPI int32_t U_EXPORT2
uplrule_selectDouble(const UPluralRules *plrules,
                     double number,
                     UChar *keyword, int32_t capacity,
                     UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status)) {
        return 0;
    }
    UnicodeString result = ((PluralRules*)plrules)->select(number);
	return result.extract(keyword, capacity, *status);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
