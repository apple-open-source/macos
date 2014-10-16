/*
*****************************************************************************************
* Copyright (C) 2014 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/ualoc.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "cstring.h"
#include "cmemory.h"
// the following has replacements for some math.h funcs etc
#include "putilimp.h"


// The numeric values in territoryInfo are in "IntF" format from LDML2ICUConverter.
// From its docs (adapted): [IntF is] a special integer that represents the number in
// normalized scientific notation.
// Resultant integers are in the form -?xxyyyyyy, where xx is the exponent
// offset by 50 and yyyyyy is the coefficient to 5 decimal places (range 1.0 to 9.99999), e.g.
// 14660000000000 -> 1.46600E13 -> 63146600
// 0.0001 -> 1.00000E-4 -> 46100000
// -123.456 -> -1.23456E-2 -> -48123456
//
// Here to avoid an extra division we have the max coefficient as 999999 (instead of
// 9.99999) and instead offset the exponent by -55.
//
static double doubleFromIntF(int32_t intF) {
    double coefficient = (double)(intF % 1000000);
    int32_t exponent = (intF / 1000000) - 55;
    return coefficient * uprv_pow10(exponent);
}

static int compareLangEntries(const void * entry1, const void * entry2) {
    double fraction1 = ((const UALanguageEntry *)entry1)->userFraction;
    double fraction2 = ((const UALanguageEntry *)entry2)->userFraction;
    // want descending order
    if (fraction1 > fraction2) return -1;
    if (fraction1 < fraction2) return 1;
    // userFractions the same, sort by languageCode
    return uprv_strcmp(((const UALanguageEntry *)entry1)->languageCode,((const UALanguageEntry *)entry2)->languageCode);
}

static const UChar ustrLangStatusDefacto[]  = {0x64,0x65,0x5F,0x66,0x61,0x63,0x74,0x6F,0x5F,0x6F,0x66,0x66,0x69,0x63,0x69,0x61,0x6C,0}; //"de_facto_official"
static const UChar ustrLangStatusOfficial[] = {0x6F,0x66,0x66,0x69,0x63,0x69,0x61,0x6C,0}; //"official"
static const UChar ustrLangStatusRegional[] = {0x6F,0x66,0x66,0x69,0x63,0x69,0x61,0x6C,0x5F,0x72,0x65,0x67,0x69,0x6F,0x6E,0x61,0x6C,0}; //"official_regional"

enum {
    kLocalLangEntriesMax = 26, // enough for most regions to minimumFraction 0.001 except India
    kLangEntriesFactor = 3     // if we have to allocate, multiply existing size by this
};

U_CAPI int32_t U_EXPORT2
ualoc_getLanguagesForRegion(const char *regionID, double minimumFraction,
                            UALanguageEntry *entries, int32_t entriesCapacity,
                            UErrorCode *err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }
    if ( regionID == NULL || minimumFraction < 0.0 || minimumFraction > 1.0 ||
        ((entries==NULL)? entriesCapacity!=0: entriesCapacity<0) ) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UResourceBundle *rb = ures_openDirect(NULL, "supplementalData", err);
    rb = ures_getByKey(rb, "territoryInfo", rb, err);
    rb = ures_getByKey(rb, regionID, rb, err);
    if (U_FAILURE(*err)) {
        ures_close(rb);
        return 0;
    }

    int32_t entryCount = 0;
    UResourceBundle *langBund = NULL;
    int32_t lbIdx, lbCount = ures_getSize(rb);
    UALanguageEntry localLangEntries[kLocalLangEntriesMax];
    UALanguageEntry * langEntries = localLangEntries;
    int32_t langEntriesMax = kLocalLangEntriesMax;

    for (lbIdx = 0; lbIdx < lbCount; lbIdx++) {
        langBund = ures_getByIndex(rb, lbIdx, langBund, err);
        if (U_FAILURE(*err)) {
            break;
        }
        const char * langCode = ures_getKey(langBund);
        if (uprv_strcmp(langCode,"territoryF") == 0) {
            continue;
        }
        if (strnlen(langCode, UALANGDATA_CODELEN+1) > UALANGDATA_CODELEN) { // no uprv_strnlen
            continue; // a code we cannot handle
        }

        UErrorCode localErr = U_ZERO_ERROR;
        double userFraction = 0.0;
        UResourceBundle *itemBund = ures_getByKey(langBund, "populationShareF", NULL, &localErr);
        if (U_SUCCESS(localErr)) {
            int32_t intF = ures_getInt(itemBund, &localErr);
            if (U_SUCCESS(localErr)) {
                userFraction = doubleFromIntF(intF);
            }
            ures_close(itemBund);
        }
        if (userFraction < minimumFraction) {
            continue;
        }
        if (entries != NULL) {
            localErr = U_ZERO_ERROR;
            UALanguageStatus langStatus = UALANGSTATUS_UNSPECIFIED;
            int32_t ulen;
            const UChar * ustrLangStatus = ures_getStringByKey(langBund, "officialStatus", &ulen, &localErr);
            if (U_SUCCESS(localErr)) {
                int32_t cmp = u_strcmp(ustrLangStatus, ustrLangStatusOfficial);
                if (cmp == 0) {
                    langStatus = UALANGSTATUS_OFFICIAL;
                } else if (cmp < 0 && u_strcmp(ustrLangStatus, ustrLangStatusDefacto) == 0) {
                    langStatus = UALANGSTATUS_DEFACTO_OFFICIAL;
                } else if (u_strcmp(ustrLangStatus, ustrLangStatusRegional) == 0) {
                    langStatus = UALANGSTATUS_REGIONAL_OFFICIAL;
                }
            }
            // Now we have all of the info for our next entry
            if (entryCount >= langEntriesMax) {
                int32_t newMax = langEntriesMax * kLangEntriesFactor;
                if (langEntries == localLangEntries) {
                    // first allocation, copy from local buf
                    langEntries = (UALanguageEntry*)uprv_malloc(newMax*sizeof(UALanguageEntry));
                    if (langEntries == NULL) {
                        *err = U_MEMORY_ALLOCATION_ERROR;
                        break;
                    }
                    uprv_memcpy(langEntries, localLangEntries, entryCount*sizeof(UALanguageEntry));
                } else {
                    langEntries = (UALanguageEntry*)uprv_realloc(langEntries, newMax*sizeof(UALanguageEntry));
                    if (langEntries == NULL) {
                        *err = U_MEMORY_ALLOCATION_ERROR;
                        break;
                    }
                }
                langEntriesMax = newMax;
            }
            uprv_strcpy(langEntries[entryCount].languageCode, langCode);
            langEntries[entryCount].userFraction = userFraction;
            langEntries[entryCount].status = langStatus;
        }
        entryCount++;
    }
    ures_close(langBund);
    ures_close(rb);
    if (U_FAILURE(*err)) {
        if (langEntries != localLangEntries) {
            free(langEntries);
        }
        return 0;
    }
    if (entries != NULL) {
        // sort langEntries, copy entries that fit to provided array
        qsort(langEntries, entryCount, sizeof(UALanguageEntry), compareLangEntries);
        if (entryCount > entriesCapacity) {
            entryCount = entriesCapacity;
        }
        uprv_memcpy(entries, langEntries, entryCount*sizeof(UALanguageEntry));
        if (langEntries != localLangEntries) {
            free(langEntries);
        }
    }
    return entryCount;
}


static const char * forceParent[] = {
    "zh",      "zh_CN",
    "zh_CN",   "root",
    "zh_Hant", "zh_TW",
    "zh_TW",   "root",
    NULL
};

U_CAPI int32_t U_EXPORT2
ualoc_getAppleParent(const char* localeID,
                     char * parent,
                     int32_t parentCapacity,
                     UErrorCode* err)
{
    UResourceBundle *rb;
    int32_t len;
    UErrorCode tempStatus;
    char locbuf[ULOC_FULLNAME_CAPACITY+1];

    if (U_FAILURE(*err)) {
        return 0;
    }
    if ( (parent==NULL)? parentCapacity!=0: parentCapacity<0 ) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    len = uloc_canonicalize(localeID, locbuf, ULOC_FULLNAME_CAPACITY, err);
    if (U_FAILURE(*err)) {
        return 0;
    }
    if (*err == U_STRING_NOT_TERMINATED_WARNING) {
        locbuf[ULOC_FULLNAME_CAPACITY] = 0;
        *err = U_ZERO_ERROR;
    }
    if (len >= 2 && uprv_strncmp(locbuf, "zh", 2) == 0) {
        const char ** forceParentPtr = forceParent;
        const char * testCurLoc;
        while ( (testCurLoc = *forceParentPtr++) != NULL ) {
            int cmp = uprv_strcmp(locbuf, testCurLoc);
            if (cmp <= 0) {
                if (cmp == 0) {
                    len = uprv_strlen(*forceParentPtr);
                    if (len < parentCapacity) {
                        uprv_strcpy(parent, *forceParentPtr);
                    } else {
                        *err = U_BUFFER_OVERFLOW_ERROR;
                    }
                    return len;
                }
                break;
            }
            forceParentPtr++;
        }
    }
    tempStatus = U_ZERO_ERROR;
    rb = ures_openDirect(NULL, locbuf, &tempStatus);
    if (U_SUCCESS(tempStatus)) {
        const char * actualLocale = ures_getLocaleByType(rb, ULOC_ACTUAL_LOCALE, &tempStatus);
        if (U_SUCCESS(tempStatus) && uprv_strcmp(locbuf, actualLocale) != 0) {
            // we have followed an alias
            len = uprv_strlen(actualLocale);
            if (len < parentCapacity) {
                uprv_strcpy(parent, actualLocale);
            } else {
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
            ures_close(rb);
            return len;
        }
        tempStatus = U_ZERO_ERROR;
        const UChar * parentUName = ures_getStringByKey(rb, "%%Parent", &len, &tempStatus);
        if (U_SUCCESS(tempStatus) && tempStatus != U_USING_FALLBACK_WARNING) {
            if (len < parentCapacity) {
                u_UCharsToChars(parentUName, parent, len + 1);
            } else {
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
            ures_close(rb);
            return len;
        }
        ures_close(rb);
    }
    len = uloc_getParent(locbuf, parent, parentCapacity, err);
    if (U_SUCCESS(*err) && len == 0) {
        len = 4;
        if (len < parentCapacity) {
            uprv_strcpy(parent, "root");
        } else {
            *err = U_BUFFER_OVERFLOW_ERROR;
        }
    }
    return len;
}

