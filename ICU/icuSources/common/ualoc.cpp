/*
*****************************************************************************************
* Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/ualoc.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "cstring.h"
#include "cmemory.h"
#include "uhash.h"
#include "umutex.h"
#include "ucln_cmn.h"
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
    "en_AU",   "en_GB",
    "en_BD",   "en_GB", // en for Bangladesh
    "en_HK",   "en_GB", // en for Hong Kong
    "en_IN",   "en_GB",
    "en_MY",   "en_GB", // en for Malaysia
    "en_PK",   "en_GB", // en for Pakistan
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
    char * foundDoubleUnderscore;

    if (U_FAILURE(*err)) {
        return 0;
    }
    if ( (parent==NULL)? parentCapacity!=0: parentCapacity<0 ) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    len = uloc_getBaseName(localeID, locbuf, ULOC_FULLNAME_CAPACITY, err); /* canonicalize and strip keywords */
    if (U_FAILURE(*err)) {
        return 0;
    }
    if (*err == U_STRING_NOT_TERMINATED_WARNING) {
        locbuf[ULOC_FULLNAME_CAPACITY] = 0;
        *err = U_ZERO_ERROR;
    }
    foundDoubleUnderscore = uprv_strstr(locbuf, "__"); /* __ comes from bad/missing subtag or variant */
    if (foundDoubleUnderscore != NULL) {
        *foundDoubleUnderscore = 0; /* terminate at the __ */
        len = uprv_strlen(locbuf);
    }
    if (len >= 2 && (uprv_strncmp(locbuf, "en", 2) == 0 || uprv_strncmp(locbuf, "zh", 2) == 0)) {
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

// =================
// Data and related functions for ualoc_localizationsToUse
// =================

static const char * appleAliasMap[][2] = {
    // names are lowercase here because they are looked up after being processed by uloc_getBaseName
    { "arabic",     "ar"      },    // T2
    { "chinese",    "zh_Hans" },    // T0
    { "danish",     "da"      },    // T2
    { "dutch",      "nl"      },    // T1, still in use
    { "english",    "en"      },    // T0, still in use
    { "finnish",    "fi"      },    // T2
    { "french",     "fr"      },    // T0, still in use
    { "german",     "de"      },    // T0, still in use
    { "italian",    "it"      },    // T1, still in use
    { "japanese",   "ja"      },    // T0, still in use
    { "korean",     "ko"      },    // T1
    { "norwegian",  "nb"      },    // T2
    { "polish",     "pl"      },    // T2
    { "portuguese", "pt"      },    // T2
    { "russian",    "ru"      },    // T2
    { "spanish",    "es"      },    // T1, still in use
    { "swedish",    "sv"      },    // T2
    { "thai",       "th"      },    // T2
    { "turkish",    "tr"      },    // T2
    { "zh",         "zh_Hans" },    // special
};
enum { kAppleAliasMapCount = sizeof(appleAliasMap)/sizeof(appleAliasMap[0]) };

static const char * appleParentMap[][2] = {
    { "en_150",     "en_GB"   },    // Apple custom parent
    { "en_AD",      "en_150"  },    // Apple locale addition
    { "en_AL",      "en_150"  },    // Apple locale addition
    { "en_AT",      "en_150"  },    // Apple locale addition
    { "en_AU",      "en_GB"   },    // Apple custom parent
    { "en_BA",      "en_150"  },    // Apple locale addition
    { "en_BD",      "en_GB"   },    // Apple custom parent
    { "en_CH",      "en_150"  },    // Apple locale addition
    { "en_CY",      "en_150"  },    // Apple locale addition
    { "en_CZ",      "en_150"  },    // Apple locale addition
    { "en_DE",      "en_150"  },    // Apple locale addition
    { "en_DK",      "en_150"  },    // Apple locale addition
    { "en_EE",      "en_150"  },    // Apple locale addition
    { "en_ES",      "en_150"  },    // Apple locale addition
    { "en_FI",      "en_150"  },    // Apple locale addition
    { "en_FR",      "en_150"  },    // Apple locale addition
    { "en_GR",      "en_150"  },    // Apple locale addition
    { "en_HK",      "en_GB"   },    // Apple custom parent
    { "en_HR",      "en_150"  },    // Apple locale addition
    { "en_HU",      "en_150"  },    // Apple locale addition
    { "en_IL",      "en_001"  },    // Apple locale addition
    { "en_IN",      "en_GB"   },    // Apple custom parent
    { "en_IS",      "en_150"  },    // Apple locale addition
    { "en_IT",      "en_150"  },    // Apple locale addition
    { "en_LT",      "en_150"  },    // Apple locale addition
    { "en_LU",      "en_150"  },    // Apple locale addition
    { "en_LV",      "en_150"  },    // Apple locale addition
    { "en_ME",      "en_150"  },    // Apple locale addition
    { "en_MY",      "en_GB"   },    // Apple custom parent
    { "en_NL",      "en_150"  },    // Apple locale addition
    { "en_NO",      "en_150"  },    // Apple locale addition
    { "en_PK",      "en_GB"   },    // Apple custom parent
    { "en_PL",      "en_150"  },    // Apple locale addition
    { "en_PT",      "en_150"  },    // Apple locale addition
    { "en_RO",      "en_150"  },    // Apple locale addition
    { "en_RU",      "en_150"  },    // Apple locale addition
    { "en_SE",      "en_150"  },    // Apple locale addition
    { "en_SI",      "en_150"  },    // Apple locale addition
    { "en_SK",      "en_150"  },    // Apple locale addition
    { "en_TR",      "en_150"  },    // Apple locale addition
};
enum { kAppleParentMapCount = sizeof(appleParentMap)/sizeof(appleParentMap[0]) };

// Might do something better for this, perhaps maximizing locales then stripping.
// Selected parents of available localizations, add as necessary.
static const char * locParentMap[][2] = {
    { "pt_BR",      "pt"        },
    { "pt_PT",      "pt"        },
    { "zh_Hans_CN", "zh_Hans"   },
    { "zh_Hant_TW", "zh_Hant"   },
};
enum { kLocParentMapCount = sizeof(locParentMap)/sizeof(locParentMap[0]) };

enum {
    kStringsAllocSize = 4096, // cannot expand; current actual usage 3610
    kParentMapInitCount = 161 // can expand; current actual usage 161
};

U_CDECL_BEGIN
static UBool U_CALLCONV ualocale_cleanup(void);
U_CDECL_END

U_NAMESPACE_BEGIN

static UInitOnce gUALocaleCacheInitOnce = U_INITONCE_INITIALIZER;

static int gMapDataState = 0; // 0 = not initialized, 1 = initialized, -1 = failure
static char* gStrings = NULL;
static UHashtable* gAliasMap = NULL;
static UHashtable* gParentMap = NULL;

U_NAMESPACE_END

U_CDECL_BEGIN

static UBool U_CALLCONV ualocale_cleanup(void)
{
    U_NAMESPACE_USE

    gUALocaleCacheInitOnce.reset();

    if (gMapDataState > 0) {
        uhash_close(gParentMap);
        gParentMap = NULL;
        uhash_close(gAliasMap);
        gAliasMap = NULL;
        uprv_free(gStrings);
        gStrings = NULL;
    }
    gMapDataState = 0;
    return TRUE;
}

static void initializeMapData() {
    U_NAMESPACE_USE

    UResourceBundle * curBundle;
    char* stringsPtr;
    char* stringsEnd;
    UErrorCode status;
    int32_t entryIndex, icuEntryCount;

    ucln_common_registerCleanup(UCLN_COMMON_LOCALE, ualocale_cleanup);

    gStrings = (char*)uprv_malloc(kStringsAllocSize);
    if (gStrings) {
        stringsPtr = gStrings;
        stringsEnd = gStrings + kStringsAllocSize;
    }

    status = U_ZERO_ERROR;
    curBundle = NULL;
    icuEntryCount = 0;
    if (gStrings) {
        curBundle = ures_openDirect(NULL, "metadata", &status);
        curBundle = ures_getByKey(curBundle, "alias", curBundle, &status);
        curBundle = ures_getByKey(curBundle, "language", curBundle, &status); // language resource is URES_TABLE
        if (U_SUCCESS(status)) {
            icuEntryCount = ures_getSize(curBundle); // currently 331
        }
    }
    status = U_ZERO_ERROR;
    gAliasMap = uhash_openSize(uhash_hashIChars, uhash_compareIChars, uhash_compareIChars,
                                kAppleAliasMapCount + icuEntryCount, &status);
    // defaults to keyDeleter NULL
    if (U_SUCCESS(status)) {
        for (entryIndex = 0; entryIndex < kAppleAliasMapCount && U_SUCCESS(status); entryIndex++) {
            uhash_put(gAliasMap, (void*)appleAliasMap[entryIndex][0], (void*)appleAliasMap[entryIndex][1], &status);
        }
        status = U_ZERO_ERROR;
        UResourceBundle * aliasMapBundle = NULL;
        for (entryIndex = 0; entryIndex < icuEntryCount && U_SUCCESS(status); entryIndex++) {
            aliasMapBundle = ures_getByIndex(curBundle, entryIndex, aliasMapBundle, &status);
            if (U_FAILURE(status)) {
                break; // error
            }
            const char * keyStr = ures_getKey(aliasMapBundle);
            int32_t len = uprv_strlen(keyStr);
            if (len >= stringsEnd - stringsPtr) {
                break; // error
            }
            uprv_strcpy(stringsPtr, keyStr);
            char * inLocStr = stringsPtr;
            stringsPtr += len + 1;

            len = stringsEnd - stringsPtr - 1;
            ures_getUTF8StringByKey(aliasMapBundle, "replacement", stringsPtr, &len, TRUE, &status);
            if (U_FAILURE(status)) {
                break; // error
            }
            stringsPtr[len] = 0;
            uhash_put(gAliasMap, inLocStr, stringsPtr, &status);
            stringsPtr += len + 1;
        }
        ures_close(aliasMapBundle);
    } else {
        ures_close(curBundle);
        uprv_free(gStrings);
        gMapDataState = -1; // failure
        return;
    }
    ures_close(curBundle);

    status = U_ZERO_ERROR;
    gParentMap = uhash_openSize(uhash_hashIChars, uhash_compareIChars, uhash_compareIChars,
                                kParentMapInitCount, &status);
    // defaults to keyDeleter NULL
    if (U_SUCCESS(status)) {
        curBundle = ures_openDirect(NULL, "supplementalData", &status);
        curBundle = ures_getByKey(curBundle, "parentLocales", curBundle, &status); // parentLocales resource is URES_TABLE
        if (U_SUCCESS(status)) {
            UResourceBundle * parentMapBundle = NULL;
            while (TRUE) {
                parentMapBundle = ures_getNextResource(curBundle, parentMapBundle, &status);
                if (U_FAILURE(status)) {
                    break; // no more parent bundles, normal exit
                }
                const char * keyStr = ures_getKey(parentMapBundle);
                int32_t len = uprv_strlen(keyStr);
                if (len >= stringsEnd - stringsPtr) {
                    break; // error
                }
                uprv_strcpy(stringsPtr, keyStr);
                char * parentStr = stringsPtr;
                stringsPtr += len + 1;

                if (ures_getType(parentMapBundle) == URES_STRING) {
                    len = stringsEnd - stringsPtr - 1;
                    ures_getUTF8String(parentMapBundle, stringsPtr, &len, TRUE, &status);
                    if (U_FAILURE(status)) {
                        break; // error
                    }
                    stringsPtr[len] = 0;
                    uhash_put(gParentMap, stringsPtr, parentStr, &status);
                    stringsPtr += len + 1;
                } else {
                    // should be URES_ARRAY
                    icuEntryCount = ures_getSize(parentMapBundle);
                    for (entryIndex = 0; entryIndex < icuEntryCount && U_SUCCESS(status); entryIndex++) {
                        len = stringsEnd - stringsPtr - 1;
                        ures_getUTF8StringByIndex(parentMapBundle, entryIndex, stringsPtr, &len, TRUE, &status);
                        if (U_FAILURE(status)) {
                            break;
                        }
                        stringsPtr[len] = 0;
                        uhash_put(gParentMap, stringsPtr, parentStr, &status);
                        stringsPtr += len + 1;
                    }
                }
            }
            ures_close(parentMapBundle);
        }
        ures_close(curBundle);

        status = U_ZERO_ERROR;
        for (entryIndex = 0; entryIndex < kAppleParentMapCount && U_SUCCESS(status); entryIndex++) {
            uhash_put(gParentMap, (void*)appleParentMap[entryIndex][0], (void*)appleParentMap[entryIndex][1], &status);
        }
    } else {
        uhash_close(gAliasMap);
        gAliasMap = NULL;
        uprv_free(gStrings);
        gMapDataState = -1; // failure
        return;
    }

    //printf("# gStrings size %ld\n", stringsPtr - gStrings);
    //printf("# gParentMap count %d\n", uhash_count(gParentMap));
    gMapDataState = 1;
}

U_CDECL_END

// The following maps aliases, etc. Ensures 0-termination if no error.
static void ualoc_normalize(const char *locale, char *normalized, int32_t normalizedCapacity, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    // uloc_minimizeSubtags(locale, normalized, normalizedCapacity, status);

    const char *replacement =  NULL;
    if (gMapDataState > 0) {
        replacement = (const char *)uhash_get(gAliasMap, locale);
    }
    if (replacement == NULL) {
        replacement = locale;
    }
    int32_t len = uprv_strlen(replacement);
    if (len < normalizedCapacity) { // allow for 0 termination
        uprv_strcpy(normalized, replacement);
    } else {
        *status = U_BUFFER_OVERFLOW_ERROR;
    }
}

static void ualoc_getParent(const char *locale, char *parent, int32_t parentCapacity, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    if (gMapDataState > 0) {
        const char *replacement = (const char *)uhash_get(gParentMap, locale);
        if (replacement) {
            int32_t len = uprv_strlen(replacement);
            if (len < parentCapacity) { // allow for 0 termination
                uprv_strcpy(parent, replacement);
            } else {
                *status = U_BUFFER_OVERFLOW_ERROR;
            }
            return;
        }
    }
    uloc_getParent(locale, parent, parentCapacity - 1, status);
    parent[parentCapacity - 1] = 0; // ensure 0 termination in case of U_STRING_NOT_TERMINATED_WARNING
}

// Might do something better for this, perhaps maximizing locales then stripping
const char * getLocParent(const char *locale)
{
    int32_t locParentIndex;
    for (locParentIndex = 0; locParentIndex < kLocParentMapCount; locParentIndex++) {
        if (uprv_strcmp(locale, locParentMap[locParentIndex][0]) == 0) {
            return locParentMap[locParentIndex][1];
        }
    }
    return NULL;
}

// this just checks if the *pointer* value is already in the array
static UBool locInArray(const char* *localizationsToUse, int32_t locsToUseCount, const char *locToCheck)
{
    int32_t locIndex;
    for (locIndex = 0; locIndex < locsToUseCount; locIndex++) {
        if (locToCheck == localizationsToUse[locIndex]) {
            return TRUE;
        }
    }
    return FALSE;
}

enum { kLangScriptRegMaxLen = ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY }; // currently 22

int32_t
ualoc_localizationsToUse( const char* const *preferredLanguages,
                          int32_t preferredLanguagesCount,
                          const char* const *availableLocalizations,
                          int32_t availableLocalizationsCount,
                          const char* *localizationsToUse,
                          int32_t localizationsToUseCapacity,
                          UErrorCode *status )
{
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (preferredLanguages == NULL || availableLocalizations == NULL || localizationsToUse == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    // get resource data, need to protect with mutex
    if (gMapDataState == 0) {
        umtx_initOnce(gUALocaleCacheInitOnce, initializeMapData);
    }
    int32_t locsToUseCount = 0;
    int32_t prefLangIndex, availLocIndex = 0;
    char (*availLocBase)[kLangScriptRegMaxLen + 1] = NULL;
    char (*availLocNorm)[kLangScriptRegMaxLen + 1] = NULL;
    UBool checkAvailLocParents = FALSE;
    UBool foundMatch = FALSE;

    // Part 1, find the best matching localization, if any
    for (prefLangIndex = 0; prefLangIndex < preferredLanguagesCount; prefLangIndex++) {
        char prefLangBaseName[kLangScriptRegMaxLen + 1];
        char prefLangNormName[kLangScriptRegMaxLen + 1];
        char prefLangParentName[kLangScriptRegMaxLen + 1];
        UErrorCode tmpStatus = U_ZERO_ERROR;

        if (preferredLanguages[prefLangIndex] == NULL) {
            continue; // skip NULL preferredLanguages entry, go to next one
        }
        // use underscores, fix bad capitalization, delete any keywords
        uloc_getBaseName(preferredLanguages[prefLangIndex], prefLangBaseName, kLangScriptRegMaxLen, &tmpStatus);
        if (U_FAILURE(tmpStatus) || prefLangBaseName[0] == 0 ||
                uprv_strcmp(prefLangBaseName, "root") == 0 || prefLangBaseName[0] == '_') {
            continue; // can't handle this preferredLanguages entry or it is invalid, go to next one
        }
        prefLangBaseName[kLangScriptRegMaxLen] = 0; // ensure 0 termination, could have U_STRING_NOT_TERMINATED_WARNING
        //printf("   # prefLangBaseName %s\n", prefLangBaseName);

        // if we have not already allocated and filled the array of
        // base availableLocalizations, do so now.
        if (availLocBase == NULL) {
            availLocBase = (char (*)[kLangScriptRegMaxLen + 1])uprv_malloc(availableLocalizationsCount * (kLangScriptRegMaxLen + 1));
            if (availLocBase == NULL) {
                continue; // cannot further check this preferredLanguages entry, go to next one
            }
            for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                tmpStatus = U_ZERO_ERROR;
                uloc_getBaseName(availableLocalizations[availLocIndex], availLocBase[availLocIndex], kLangScriptRegMaxLen, &tmpStatus);
                if (U_FAILURE(tmpStatus) || uprv_strcmp(availLocBase[availLocIndex], "root") == 0 || availLocBase[availLocIndex][0] == '_') {
                    availLocBase[availLocIndex][0] = 0; // effectively remove this entry
                } else {
                    availLocBase[availLocIndex][kLangScriptRegMaxLen] = 0; // ensure 0 termination, could have U_STRING_NOT_TERMINATED_WARNING
                }
            }
        }
        // first compare base preferredLanguage to base versions of availableLocalizations names
        for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
            if (uprv_strcmp(prefLangBaseName, availLocBase[availLocIndex]) == 0) {
                foundMatch = TRUE; // availLocIndex records where
                break;
            }
        }
        if (foundMatch) {
            //printf("   # matched actualLocName\n");
            break; // found a loc for this preferredLanguages entry
        }

        // get normalized preferredLanguage
        tmpStatus = U_ZERO_ERROR;
        ualoc_normalize(prefLangBaseName, prefLangNormName, kLangScriptRegMaxLen + 1, &tmpStatus);
        if (U_FAILURE(tmpStatus)) {
            continue; // can't handle this preferredLanguages entry, go to next one
        }
        //printf("   # prefLangNormName %s\n", prefLangNormName);
        // if we have not already allocated and filled the array of
        // normalized availableLocalizations, do so now.
        // Note: ualoc_normalize turns "zh_TW" into "zh_Hant_TW", zh_HK" into "zh_Hant_HK",
        // and fixes deprecated codes "iw" > "he", "in" > "id" etc.
        if (availLocNorm == NULL) {
            availLocNorm = (char (*)[kLangScriptRegMaxLen + 1])uprv_malloc(availableLocalizationsCount * (kLangScriptRegMaxLen + 1));
            if (availLocNorm == NULL) {
                continue; // cannot further check this preferredLanguages entry, go to next one
            }
            for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                tmpStatus = U_ZERO_ERROR;
                ualoc_normalize(availLocBase[availLocIndex], availLocNorm[availLocIndex], kLangScriptRegMaxLen + 1, &tmpStatus);
                if (U_FAILURE(tmpStatus)) {
                    availLocNorm[availLocIndex][0] = 0; // effectively remove this entry
                } else if (getLocParent(availLocNorm[availLocIndex]) != NULL) {
                    checkAvailLocParents = TRUE;
                }
                //printf("   # actualLoc %-11s -> norm %s\n", availableLocalizations[availLocIndex], availLocNorm[availLocIndex]);
            }
        }
        // now compare normalized preferredLanguage to normalized localization names
        // if matches, copy *original* localization name
        for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
            if (uprv_strcmp(prefLangNormName, availLocNorm[availLocIndex]) == 0) {
                foundMatch = TRUE; // availLocIndex records where
                break;
            }
        }
        if (foundMatch) {
            //printf("   # matched actualLocNormName\n");
            break; // found a loc for this preferredLanguages entry
        }

        // now walk up the parent chain for preferredLanguage
        // until we find a match or hit root
        uprv_strcpy(prefLangBaseName, prefLangNormName);
        while (!foundMatch) {
            tmpStatus = U_ZERO_ERROR;
            ualoc_getParent(prefLangBaseName, prefLangParentName, kLangScriptRegMaxLen + 1, &tmpStatus);
            if (U_FAILURE(tmpStatus) || uprv_strcmp(prefLangParentName, "root") == 0 || prefLangParentName[0] == 0) {
                break; // reached root or cannot proceed further
            }
            //printf("   # prefLangParentName %s\n", prefLangParentName);

            // now compare this preferredLanguage parent to normalized localization names
            // if matches, copy *original* localization name
            for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                if (uprv_strcmp(prefLangParentName, availLocNorm[availLocIndex]) == 0) {
                    foundMatch = TRUE; // availLocIndex records where
                    break;
                }
            }
            uprv_strcpy(prefLangBaseName, prefLangParentName);
        }
        if (foundMatch) {
            break; // found a loc for this preferredLanguages entry
        }

        // last try, use parents of selected
        if (checkAvailLocParents) {
            // now walk up the parent chain for preferredLanguage again
            // checking against parents of selected availLocNorm entries
            // but this time start with current prefLangNormName
            uprv_strcpy(prefLangBaseName, prefLangNormName);
            while (TRUE) {
                tmpStatus = U_ZERO_ERROR;
                // now compare this preferredLanguage to normalized localization names
                // parent if have one for this;  if matches, copy *original* localization name
                for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                    const char *availLocParent = getLocParent(availLocNorm[availLocIndex]);
                    if (availLocParent && uprv_strcmp(prefLangBaseName, availLocParent) == 0) {
                        foundMatch = TRUE; // availLocIndex records where
                        break;
                    }
                }
                if (foundMatch) {
                    break;
                }
                ualoc_getParent(prefLangBaseName, prefLangParentName, kLangScriptRegMaxLen + 1, &tmpStatus);
                if (U_FAILURE(tmpStatus) || uprv_strcmp(prefLangParentName, "root") == 0 || prefLangParentName[0] == 0) {
                    break; // reached root or cannot proceed further
                }
                uprv_strcpy(prefLangBaseName, prefLangParentName);
            }
        }
        if (foundMatch) {
            break; // found a loc for this preferredLanguages entry
        }
    }

    // Part 2, if we found a matching localization, then walk up its parent tree to find any fallback matches in availableLocalizations
    if (foundMatch) {
        // Here availLocIndex corresponds to the first matched localization
        UErrorCode tmpStatus = U_ZERO_ERROR;
        int32_t availLocMatchIndex = availLocIndex;
        if (locsToUseCount < localizationsToUseCapacity) {
            localizationsToUse[locsToUseCount++] = availableLocalizations[availLocMatchIndex];
        }
        // at this point we must have availLocBase, and minimally matched against that.
        // if we have not already allocated and filled the array of
        // normalized availableLocalizations, do so now, but don't require it
        if (availLocNorm == NULL) {
            availLocNorm = (char (*)[kLangScriptRegMaxLen + 1])uprv_malloc(availableLocalizationsCount * (kLangScriptRegMaxLen + 1));
            if (availLocNorm != NULL) {
                for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                    tmpStatus = U_ZERO_ERROR;
                    ualoc_normalize(availLocBase[availLocIndex], availLocNorm[availLocIndex], kLangScriptRegMaxLen + 1, &tmpStatus);
                    if (U_FAILURE(tmpStatus)) {
                        availLocNorm[availLocIndex][0] = 0; // effectively remove this entry
                    }
                }
            }
        }

       // add normalized form of matching loc, if different and in availLocBase
        if (locsToUseCount < localizationsToUseCapacity) {
            tmpStatus = U_ZERO_ERROR;
            char matchedLocNormName[kLangScriptRegMaxLen + 1];
            char matchedLocParentName[kLangScriptRegMaxLen + 1];
            // get normalized form of matching loc
            if (availLocNorm != NULL) {
                uprv_strcpy(matchedLocNormName, availLocNorm[availLocMatchIndex]);
            } else {
                ualoc_normalize(availLocBase[availLocMatchIndex], matchedLocNormName, kLangScriptRegMaxLen + 1, &tmpStatus);
            }
            if (U_SUCCESS(tmpStatus)) {
                // add normalized form of matching loc, if different and in availLocBase
                if (uprv_strcmp(matchedLocNormName, localizationsToUse[0]) != 0) {
                    // normalization of matched localization is different, see if we have the normalization in availableLocalizations
                    // from this point on, availLocIndex no longer corresponds to the matched localization.
                    for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                        if ( (uprv_strcmp(matchedLocNormName, availLocBase[availLocIndex]) == 0
                                || (availLocNorm != NULL && uprv_strcmp(matchedLocNormName, availLocNorm[availLocIndex]) == 0))
                                && !locInArray(localizationsToUse, locsToUseCount, availableLocalizations[availLocIndex])) {
                            localizationsToUse[locsToUseCount++] = availableLocalizations[availLocIndex];
                            break;
                        }
                    }
                }

                // now walk up the parent chain from matchedLocNormName, adding parents if they are in availLocBase
                while (locsToUseCount < localizationsToUseCapacity) {
                    ualoc_getParent(matchedLocNormName, matchedLocParentName, kLangScriptRegMaxLen + 1, &tmpStatus);
                    if (U_FAILURE(tmpStatus) || uprv_strcmp(matchedLocParentName, "root") == 0 || matchedLocParentName[0] == 0) {
                        break; // reached root or cannot proceed further
                    }

                    // now compare this matchedLocParentName parent to base localization names (and norm ones if we have them)
                    for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                        if ( (uprv_strcmp(matchedLocParentName, availLocBase[availLocIndex]) == 0
                                || (availLocNorm != NULL && uprv_strcmp(matchedLocParentName, availLocNorm[availLocIndex]) == 0))
                                && !locInArray(localizationsToUse, locsToUseCount, availableLocalizations[availLocIndex])) {
                            localizationsToUse[locsToUseCount++] = availableLocalizations[availLocIndex];
                            break;
                        }
                    }
                    uprv_strcpy(matchedLocNormName, matchedLocParentName);
                }

                // The above still fails to include "zh_TW" if it is in availLocBase and the matched localization
                // base name is "zh_HK" or "zh_MO". One option would be to walk up the parent chain from
                // matchedLocNormName again, comparing against parents of of selected availLocNorm entries.
                // But this picks up too many matches that are not parents of the matched localization. So
                // we just handle these specially.
                if ( locsToUseCount < localizationsToUseCapacity
                        && (uprv_strcmp(availLocBase[availLocMatchIndex], "zh_HK") == 0
                        || uprv_strcmp(availLocBase[availLocMatchIndex], "zh_MO") == 0) ) {
                    int32_t zhTW_matchIndex = -1;
                    UBool zhHant_found = FALSE;
                    for (availLocIndex = 0; availLocIndex < availableLocalizationsCount; availLocIndex++) {
                        if ( zhTW_matchIndex < 0 && uprv_strcmp("zh_TW", availLocBase[availLocIndex]) == 0 ) {
                            zhTW_matchIndex = availLocIndex;
                        }
                        if ( !zhHant_found && uprv_strcmp("zh_Hant", availLocBase[availLocIndex]) == 0 ) {
                            zhHant_found = TRUE;
                        }
                    }
                    if (zhTW_matchIndex >= 0 && !zhHant_found
                            && !locInArray(localizationsToUse, locsToUseCount, availableLocalizations[zhTW_matchIndex])) {
                        localizationsToUse[locsToUseCount++] = availableLocalizations[zhTW_matchIndex];
                    }
                }
            }
        }
    }

    uprv_free(availLocNorm);
    uprv_free(availLocBase);
    return locsToUseCount;
}

