/*
*****************************************************************************************
* Copyright (C) 2014-2019 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#define DEBUG_UALOC 0
#if DEBUG_UALOC
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "unicode/utypes.h"
#include "unicode/ualoc.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "unicode/ustring.h"
#include "cstring.h"
#include "cmemory.h"
#include "uhash.h"
#include "umutex.h"
#include "ucln_cmn.h"
// the following has replacements for some math.h funcs etc
#include "putilimp.h"
// For <rdar://problem/63880069>
#include "uresimp.h"

#include <algorithm>
#include <vector>

using namespace icu;

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

// language codes to version with default script
// must be sorted by language code
static const char * langToDefaultScript[] = {
    "az",   "az_Latn",
    "bm",   "bm_Latn",  // <rdar://problem/47494729> added
    "bs",   "bs_Latn",
    "byn",  "byn_Ethi", // <rdar://problem/47494729> added
    "cu",   "cu_Cyrl",  // <rdar://problem/47494729> added
    "ff",   "ff_Latn",  // <rdar://problem/47494729> added
    "ha",   "ha_Latn",  // <rdar://problem/47494729> added
    "iu",   "iu_Cans",
    "kk",   "kk_Cyrl",  // <rdar://problem/47494729> changed from _Arab
    "ks",   "ks_Arab",  // unnecessary?
    "ku",   "ku_Latn",
    "ky",   "ky_Cyrl",
    "mn",   "mn_Cyrl",
    "ms",   "ms_Latn",
    "pa",   "pa_Guru",
    "rif",  "rif_Tfng", // unnecessary? no locale support anyway
    "sd",   "sd_Arab",  // <rdar://problem/47494729> added
    "shi",  "shi_Tfng",
    "sr",   "sr_Cyrl",
    "tg",   "tg_Cyrl",
    "tk",   "tk_Latn",  // unnecessary?
    "ug",   "ug_Arab",
    "uz",   "uz_Latn",
    "vai",  "vai_Vaii",
    "yue",  "yue_Hant", // to match CLDR data, not Apple default
    "zh",   "zh_Hans",
    NULL
};

static const char * langCodeWithScriptIfAmbig(const char * langCode) {
	const char ** langToDefScriptPtr = langToDefaultScript;
	const char * testCurLoc;
	while ( (testCurLoc = *langToDefScriptPtr++) != NULL ) {
		int cmp = uprv_strcmp(langCode, testCurLoc);
		if (cmp <= 0) {
			if (cmp == 0) {
			    return *langToDefScriptPtr;
			}
			break;
		}
		langToDefScriptPtr++;
	}
    return langCode;
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
            uprv_strcpy(langEntries[entryCount].languageCode, langCodeWithScriptIfAmbig(langCode));
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

/**
 * Internal function used by ualoc_getRegionsForLanguage().  This is split out both to clarify the logic
 * below and because we might actually want to expose this in the API at some point.  This takes a language
 * ID, converts it into canonical form, strips off any extraneous junk other than language and script code, and
 * also removes the script code if it's the default script for the specified language.  (For example, "zh_Hant"
 * is returned unchanged, but "zh_Hans" turns into "zh".  "zh_CN" turns into "zh", and "zh_TW" turns into "zh_Hant".)
 * @param languageID The language ID to convert into canonical form.
 * @param canonicalizedLanguageID A pointer to the memory where the result is to be written.  This function
 * assumes the caller has allocated enough space for the result (it assumes ULOC_FULLNAME_CAPACITY, although
 * @param err Pointer to the error code.
 * in practice it'll never use that much space).
 */
static void canonicalizeLanguageID(const char* languageID, char* canonicalizedLanguageID, UErrorCode* err) {
    char tempLanguageID[ULOC_FULLNAME_CAPACITY];
    // convert the input language ID to canonical form-- this normalizes capitalization and delimiters,
    // strips off parameters, and resolves aliases
    ualoc_canonicalForm(languageID, canonicalizedLanguageID, ULOC_FULLNAME_CAPACITY, err);
    
    // if the input language has a variant code, strip that off too
    UErrorCode tempErr = U_ZERO_ERROR;
    if (uloc_getVariant(canonicalizedLanguageID, NULL, 0, &tempErr) > 0) {
        *(uprv_strrchr(canonicalizedLanguageID, '_')) = '\0';
    }
    
    // expand both the original language ID and just its language code with their likely subtags
    uloc_getLanguage(canonicalizedLanguageID, tempLanguageID, ULOC_FULLNAME_CAPACITY, err);
    uloc_addLikelySubtags(canonicalizedLanguageID, canonicalizedLanguageID, ULOC_FULLNAME_CAPACITY, err);
    uloc_addLikelySubtags(tempLanguageID, tempLanguageID, ULOC_FULLNAME_CAPACITY, err);
    
    // if they actually expanded, lop off the region codes from both (everything from the last _ to the end)
    if (U_SUCCESS(*err) && uprv_strrchr(canonicalizedLanguageID, '_') != nullptr) {
        *(uprv_strrchr(canonicalizedLanguageID, '_')) = '\0';
        *(uprv_strrchr(tempLanguageID, '_')) = '\0';

        // if they're equal, it means we're using the default script code, so lop that off too
        if (uprv_strcmp(canonicalizedLanguageID, tempLanguageID) == 0) {
            *(uprv_strrchr(canonicalizedLanguageID, '_')) = '\0';
        }
    }
}

U_CAPI int32_t U_EXPORT2
ualoc_getRegionsForLanguage(const char *languageID, double minimumFraction,
                            UARegionEntry *entries, int32_t entriesCapacity,
                            UErrorCode *err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }
    
    int32_t entryCount = 0;
    std::vector<UARegionEntry> tempEntries;
    
    // canonicalize the input language ID
    char canonicalizedLanguageID[ULOC_FULLNAME_CAPACITY];
    canonicalizeLanguageID(languageID, canonicalizedLanguageID, err);
    
    // open the supplementalData/territoryInfo resource
    LocalUResourceBundlePointer supplementalData(ures_openDirect(NULL, "supplementalData", err));
    LocalUResourceBundlePointer territoryInfo(ures_getByKey(supplementalData.getAlias(), "territoryInfo", NULL, err));
    
    // iterate over all the regions in the territoryInfo resource
    LocalUResourceBundlePointer regionInfo, languageInfo, populationShareRB;
    while (U_SUCCESS(*err) && ures_hasNext(territoryInfo.getAlias())) {
        regionInfo.adoptInstead(ures_getNextResource(territoryInfo.getAlias(), regionInfo.orphan(), err));
        if (U_SUCCESS(*err)) {
            
            // does this region have a resource for the language the caller requested?
            UErrorCode localErr = U_ZERO_ERROR;
            if (uprv_strcmp(ures_getKey(regionInfo.getAlias()), "US") == 0 && uprv_strcmp(canonicalizedLanguageID, "zh") == 0) {
                // this is a KLUDGE to work around the fact that the entry for Simplified Chinese in the US section of
                // territoryInfo uses "zh_Hans" as its language ID instead of just "zh" (we don't want to change
                // supplementalData.txt in case other people are depending on this)
                languageInfo.adoptInstead(ures_getByKey(regionInfo.getAlias(), "zh_Hans", languageInfo.orphan(), &localErr));
            } else {
                languageInfo.adoptInstead(ures_getByKey(regionInfo.getAlias(), canonicalizedLanguageID, languageInfo.orphan(), &localErr));
            }
            if (U_SUCCESS(localErr)) {
                
                // fetch the populationShareF resource from the language resource
                double populationShare = 0.0;
                populationShareRB.adoptInstead(ures_getByKey(languageInfo.getAlias(), "populationShareF", populationShareRB.orphan(), &localErr));
                if (U_SUCCESS(localErr)) {
                    int32_t populationShareAsInt = ures_getInt(populationShareRB.getAlias(), &localErr);
                    if (U_SUCCESS(localErr)) {
                        populationShare = doubleFromIntF(populationShareAsInt);
                    }
                }
                
                // if it meets the specified threshold, add a record to the result list
                if (populationShare >= minimumFraction) {
                    if (entries == NULL) {
                        // if we're preflighting, just bump the entry count; don't both building a result list
                        ++entryCount;
                    } else {
                        UARegionEntry tempEntry;
                        uprv_strcpy(tempEntry.regionCode, ures_getKey(regionInfo.getAlias()));
                        tempEntry.userFraction = populationShare;
                        tempEntry.status = UALANGSTATUS_UNSPECIFIED;
                        
                        // fetch the officialStatus resource from the language resource and add it to the result record
                        localErr = U_ZERO_ERROR;
                        const UChar* officialStatus = ures_getStringByKey(languageInfo.getAlias(), "officialStatus", NULL, &localErr);
                        if (U_SUCCESS(localErr) && officialStatus != NULL) {
                            if (u_strcmp(officialStatus, u"official") == 0) {
                                tempEntry.status = UALANGSTATUS_OFFICIAL;
                            } else if (u_strcmp(officialStatus, u"official_regional") == 0) {
                                tempEntry.status = UALANGSTATUS_REGIONAL_OFFICIAL;
                            } else if (u_strcmp(officialStatus, u"de_facto_official") == 0) {
                                tempEntry.status = UALANGSTATUS_DEFACTO_OFFICIAL;
                            }
                            // otherwise, leave it as UALANGSTATUS_UNSPECIFIED
                        }
                        
                        tempEntries.push_back(tempEntry);
                    }
                }
            }
        }
    }
    
    if (entries == NULL) {
        // if we're preflighting, just return the entry count
        return entryCount;
    } else {
        // otherwise, sort our temporary result list in descending order by population share and copy the top
        // `entriesCapacity` entries into `entries`
        std::sort(tempEntries.begin(), tempEntries.end(), [](const UARegionEntry& a, const UARegionEntry& b) {
            return a.userFraction > b.userFraction || (a.userFraction == b.userFraction && uprv_strcmp(a.regionCode, b.regionCode) < 0);
        });
        
        entryCount = std::min(entriesCapacity, (int32_t)tempEntries.size());
        std::copy_n(tempEntries.begin(), entryCount, entries);
        return entryCount;
    }
}

static const char * forceParent[] = { // Not used by ualoc_localizationsToUse
    "en_150",  "en_GB",  // en for Europe
    "en_AI",   "en_GB",
    "en_AU",   "en_GB",
    "en_BB",   "en_GB",
    "en_BD",   "en_GB",  // en for Bangladesh
    "en_BE",   "en_150", // en for Belgium goes to en for Europe
    "en_BM",   "en_GB",
    "en_BN",   "en_GB",
    "en_BS",   "en_GB",
    "en_BW",   "en_GB",
    "en_BZ",   "en_GB",
    "en_CC",   "en_AU",
    "en_CK",   "en_AU",
    "en_CX",   "en_AU",
    "en_CY",   "en_150",    // Apple locale addition
    "en_DG",   "en_GB",
    "en_DM",   "en_GB",
    "en_FJ",   "en_GB",
    "en_FK",   "en_GB",
    "en_GD",   "en_GB",
    "en_GG",   "en_GB",
    "en_GH",   "en_GB",
    "en_GI",   "en_GB",
    "en_GM",   "en_GB",
    "en_GY",   "en_GB",
    "en_HK",   "en_GB",  // en for Hong Kong
    "en_IE",   "en_GB",
    "en_IM",   "en_GB",
    "en_IN",   "en_GB",
    "en_IO",   "en_GB",
    "en_JE",   "en_GB",
    "en_JM",   "en_GB",
    "en_KE",   "en_GB",
    "en_KI",   "en_GB",
    "en_KN",   "en_GB",
    "en_KY",   "en_GB",
    "en_LC",   "en_GB",
    "en_LK",   "en_GB",
    "en_LS",   "en_GB",
    "en_MO",   "en_GB",
    "en_MS",   "en_GB",
    "en_MT",   "en_GB",
    "en_MU",   "en_GB",
    "en_MV",   "en_GB",  // for Maldives
    "en_MW",   "en_GB",
    "en_MY",   "en_GB",  // en for Malaysia
    "en_NA",   "en_GB",
    "en_NF",   "en_AU",
    "en_NG",   "en_GB",
    "en_NR",   "en_AU",
    "en_NU",   "en_AU",
    "en_NZ",   "en_AU",
    "en_PK",   "en_GB",  // en for Pakistan
    "en_SB",   "en_GB",
    "en_SC",   "en_GB",
    "en_SD",   "en_GB",
    "en_SG",   "en_GB",
    "en_SH",   "en_GB",
    "en_SL",   "en_GB",
    "en_SS",   "en_GB",
    "en_SZ",   "en_GB",
    "en_TC",   "en_GB",
    "en_TO",   "en_GB",
    "en_TT",   "en_GB",
    "en_TV",   "en_GB",
    "en_TZ",   "en_GB",
    "en_UG",   "en_GB",
    "en_VC",   "en_GB",
    "en_VG",   "en_GB",
    "en_VU",   "en_GB",
    "en_WS",   "en_AU",
    "en_ZA",   "en_GB",
    "en_ZM",   "en_GB",
    "en_ZW",   "en_GB",
    "yue",     "yue_HK",
    "yue_CN",  "root",  // should this change to e.g. "zh_Hans_CN" for <rdar://problem/30671866>?
    "yue_HK",  "root",  // should this change to e.g. "zh_Hant_HK" for <rdar://problem/30671866>?
    "yue_Hans","yue_CN",
    "yue_Hant","yue_HK",
    "zh",      "zh_CN",
    "zh_CN",   "root",
    "zh_Hant", "zh_TW",
    "zh_TW",   "root",
    NULL
};

enum { kLocBaseNameMax = 16 };

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
    if ((len >= 2 && (uprv_strncmp(locbuf, "en", 2) == 0 || uprv_strncmp(locbuf, "zh", 2) == 0)) || (len >= 3 && uprv_strncmp(locbuf, "yue", 3) == 0)) {
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
        ures_close(rb);
        if (U_SUCCESS(tempStatus) && uprv_strcmp(locbuf, actualLocale) != 0) {
            // we have followed an alias
            len = uprv_strlen(actualLocale);
            if (len < parentCapacity) {
                uprv_strcpy(parent, actualLocale);
            } else {
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
            return len;
        }
    }
    tempStatus = U_ZERO_ERROR;
    rb = ures_openDirect(NULL, "supplementalData", &tempStatus);
    rb = ures_getByKey(rb, "parentLocales", rb, &tempStatus);
    if (U_SUCCESS(tempStatus)) {
        UResourceBundle * parentMapBundle = NULL;
        int32_t childLen = 0;
        while (childLen == 0) {
            tempStatus = U_ZERO_ERROR;
            parentMapBundle = ures_getNextResource(rb, parentMapBundle, &tempStatus);
            if (U_FAILURE(tempStatus)) {
                break; // no more parent bundles, normal exit
            }
            char childName[kLocBaseNameMax + 1];
            childName[kLocBaseNameMax] = 0;
            const char * childPtr = NULL;
            if (ures_getType(parentMapBundle) == URES_STRING) {
                childLen = kLocBaseNameMax;
                childPtr = ures_getUTF8String(parentMapBundle, childName, &childLen, FALSE, &tempStatus);
                if (U_FAILURE(tempStatus) || uprv_strncmp(locbuf, childPtr, kLocBaseNameMax) != 0) {
                    childLen = 0;
                }
            } else { // should be URES_ARRAY
                int32_t childCur, childCount = ures_getSize(parentMapBundle);
                for (childCur = 0; childCur < childCount && childLen == 0; childCur++) {
                    tempStatus = U_ZERO_ERROR;
                    childLen = kLocBaseNameMax;
                    childPtr = ures_getUTF8StringByIndex(parentMapBundle, childCur, childName, &childLen, FALSE, &tempStatus);
                    if (U_FAILURE(tempStatus) || uprv_strncmp(locbuf, childPtr, kLocBaseNameMax) != 0) {
                        childLen = 0;
                    }
                }
            }
        }
        ures_close(rb);
        if (childLen > 0) {
            // parentMapBundle key is the parent we are looking for
            const char * keyStr = ures_getKey(parentMapBundle);
            len = uprv_strlen(keyStr);
            if (len < parentCapacity) {
                 uprv_strcpy(parent, keyStr);
            } else {
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
            ures_close(parentMapBundle);
            return len;
        }
        ures_close(parentMapBundle);
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
    { "no_NO",      "nb_NO"   },    // special
    { "norwegian",  "nb"      },    // T2
    { "polish",     "pl"      },    // T2
    { "portuguese", "pt"      },    // T2
    { "russian",    "ru"      },    // T2
    { "spanish",    "es"      },    // T1, still in use
    { "swedish",    "sv"      },    // T2
    { "thai",       "th"      },    // T2
    { "turkish",    "tr"      },    // T2
    // all of the following were originally in ICU's metadata.txt as "legacy" mappings, but have been removed
    // as of ICU 68
    { "yue_CN",     "yue_Hans_CN" },
    { "yue_HK",     "yue_Hant_HK" },
    { "zh_CN",      "zh_Hans_CN"  },
    { "zh_HK",      "zh_Hant_HK"  },
    { "zh_MO",      "zh_Hant_MO"  },
    { "zh_SG",      "zh_Hans_SG"  },
    { "zh_TW",      "zh_Hant_TW"  }
};
enum { kAppleAliasMapCount = UPRV_LENGTHOF(appleAliasMap) };

// Most of the entries in the following are cases in which
// localization bundle inheritance is different from
// ICU resource inheritance, and thus are not in parentLocales data.
// <rdar://problem/63880069> However, since this is now checked before
// the hashmap of parentLocales data, we add a few important entries
// from parentLocales data for lookup efficiency.
static const char * appleParentMap[][2] = {
    { "ars",         "ar"      },    // rdar://64497611
    { "en_150",      "en_GB"   },    // Apple custom parent
    { "en_AG",       "en_GB"   },    // Antigua & Barbuda
    { "en_AI",       "en_GB"   },    // Anguilla
    { "en_AU",       "en_GB"   },    // Apple custom parent
    { "en_BB",       "en_GB"   },    // Barbados
    { "en_BD",       "en_GB"   },    // Apple custom parent
    { "en_BM",       "en_GB"   },    // Bermuda
    { "en_BN",       "en_GB"   },    // Brunei
    { "en_BS",       "en_GB"   },    // Bahamas
    { "en_BW",       "en_GB"   },    // Botswana
    { "en_BZ",       "en_GB"   },    // Belize
    { "en_CC",       "en_AU"   },    // Cocos (Keeling) Islands
    { "en_CK",       "en_AU"   },    // Cook Islands (maybe to en_NZ instead?)
    { "en_CX",       "en_AU"   },    // Christmas Island
    { "en_CY",       "en_150"  },    // Apple locale addition
    { "en_DG",       "en_GB"   },
    { "en_DM",       "en_GB"   },    // Dominica
    { "en_FJ",       "en_GB"   },    // Fiji
    { "en_FK",       "en_GB"   },
    { "en_GB",       "en_001"  },    // from parentLocales, added here for efficiency
    { "en_GD",       "en_GB"   },    // Grenada
    { "en_GG",       "en_GB"   },
    { "en_GH",       "en_GB"   },    // Ghana
    { "en_GI",       "en_GB"   },
    { "en_GM",       "en_GB"   },    // Gambia
    { "en_GY",       "en_GB"   },    // Guyana
    { "en_HK",       "en_GB"   },    // Apple custom parent
    { "en_IE",       "en_GB"   },
    { "en_IM",       "en_GB"   },
    { "en_IN",       "en_GB"   },    // Apple custom parent
    { "en_IO",       "en_GB"   },
    { "en_JE",       "en_GB"   },
    { "en_JM",       "en_GB"   },
    { "en_KE",       "en_GB"   },    // Kenya
    { "en_KI",       "en_GB"   },    // Kiribati
    { "en_KN",       "en_GB"   },    // St. Kitts & Nevis
    { "en_KY",       "en_GB"   },    // Cayman Islands
    { "en_LC",       "en_GB"   },    // St. Lucia
    { "en_LK",       "en_GB"   },    // Apple custom parent
    { "en_LS",       "en_GB"   },    // Lesotho
    { "en_MO",       "en_GB"   },
    { "en_MS",       "en_GB"   },    // Montserrat
    { "en_MT",       "en_GB"   },
    { "en_MU",       "en_GB"   },    // Mauritius
    { "en_MV",       "en_GB"   },
    { "en_MW",       "en_GB"   },    // Malawi
    { "en_MY",       "en_GB"   },    // Apple custom parent
    { "en_NA",       "en_GB"   },    // Namibia
    { "en_NF",       "en_AU"   },    // Norfolk Island
    { "en_NG",       "en_GB"   },    // Nigeria
    { "en_NR",       "en_AU"   },    // Nauru
    { "en_NU",       "en_AU"   },    // Niue (maybe to en_NZ instead?)
    { "en_NZ",       "en_AU"   },
    { "en_PG",       "en_AU"   },    // Papua New Guinea
    { "en_PK",       "en_GB"   },    // Apple custom parent
    { "en_PN",       "en_GB"   },    // Pitcairn Islands
    { "en_SB",       "en_GB"   },    // Solomon Islands
    { "en_SC",       "en_GB"   },    // Seychelles
    { "en_SD",       "en_GB"   },    // Sudan
    { "en_SG",       "en_GB"   },
    { "en_SH",       "en_GB"   },
    { "en_SL",       "en_GB"   },    // Sierra Leone
    { "en_SS",       "en_GB"   },    // South Sudan
    { "en_SZ",       "en_GB"   },    // Swaziland
    { "en_TC",       "en_GB"   },    // Tristan da Cunha
    { "en_TO",       "en_GB"   },    // Tonga
    { "en_TT",       "en_GB"   },    // Trinidad & Tobago
    { "en_TV",       "en_GB"   },    // Tuvalu
    { "en_TZ",       "en_GB"   },    // Tanzania
    { "en_UG",       "en_GB"   },    // Uganda
    { "en_VC",       "en_GB"   },    // St. Vincent & Grenadines
    { "en_VG",       "en_GB"   },
    { "en_VU",       "en_GB"   },    // Vanuatu
    { "en_WS",       "en_AU"   },    // Samoa (maybe to en_NZ instead?)
    { "en_ZA",       "en_GB"   },    // South Africa
    { "en_ZM",       "en_GB"   },    // Zambia
    { "en_ZW",       "en_GB"   },    // Zimbabwe
    { "es_MX",       "es_419"  },    // from parentLocales, added here for efficiency
    { "wuu",         "wuu_Hans"},    // rdar://64497611
    { "wuu_Hans",    "zh_Hans" },    // rdar://64497611
    { "wuu_Hant",    "zh_Hant" },    // rdar://64497611
    { "yue",         "yue_Hant"},
    { "yue_HK",      "zh_Hant_HK" }, // rdar://67469388
    { "yue_Hans",    "zh_Hans" },    // rdar://30671866
    { "yue_Hant",    "yue_HK"  },    // rdar://67469388
    { "zh_HK",       "zh_Hant" },
    { "zh_Hant",     "zh_TW"   },
    { "zh_Hant_HK",  "zh_HK",  },
    { "zh_TW",       "root"    },
};
enum { kAppleParentMapCount = UPRV_LENGTHOF(appleParentMap) };

U_CDECL_BEGIN
static UBool U_CALLCONV ualocale_cleanup(void);
U_CDECL_END

U_NAMESPACE_BEGIN

static UInitOnce gUALocaleCacheInitOnce = U_INITONCE_INITIALIZER;

static int gMapDataState = 0; // 0 = not initialized, 1 = initialized, -1 = failure
static UResourceBundle* gLanguageAliasesBundle = NULL; 

U_NAMESPACE_END

U_CDECL_BEGIN

static UBool U_CALLCONV ualocale_cleanup(void)
{
    U_NAMESPACE_USE

    if (gMapDataState > 0) {
        ures_close(gLanguageAliasesBundle);
        gLanguageAliasesBundle = NULL;
    }
    gMapDataState = 0;
    gUALocaleCacheInitOnce.reset();
    return TRUE;
}

static void initializeMapData() {
    U_NAMESPACE_USE

    ucln_common_registerCleanup(UCLN_COMMON_LOCALE, ualocale_cleanup);

    UResourceBundle * curBundle;
    UErrorCode status = U_ZERO_ERROR;
    curBundle = ures_openDirect(NULL, "metadata", &status);
    curBundle = ures_getByKey(curBundle, "alias", curBundle, &status);
    curBundle = ures_getByKey(curBundle, "language", curBundle, &status);
    if (U_FAILURE(status)) {
        gMapDataState = -1; // failure
        return;
    }
    gLanguageAliasesBundle = curBundle; // URES_TABLE resource, 420 entries in ICU-6600n
#if DEBUG_UALOC
    printf("# metadata/alias/language size %d\n", ures_getSize(curBundle));
#endif

    gMapDataState = 1;
}

U_CDECL_END

// comparator for binary search of appleAliasMap
static int compareAppleMapElements(const void *key, const void *entry) {
    return uprv_strcmp((const char *)key, ((const char **)entry)[0]);
}

// The following maps aliases, etc. Ensures 0-termination if no error.
static void ualoc_normalize(const char *locale, char *normalized, int32_t normalizedCapacity, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    // uloc_minimizeSubtags(locale, normalized, normalizedCapacity, status);

    const char *replacement = locale; // fallback to no replacement
    int32_t len;
    // first check in appleAliasMap using binary search
    const char** entry = (const char**)bsearch(locale, appleAliasMap, kAppleAliasMapCount, sizeof(appleAliasMap[0]), compareAppleMapElements);
    if (entry != NULL) {
        replacement = entry[1];
    } else if (icu::gMapDataState > 0) {
        // check in gLanguageAliasesBundle
        UErrorCode localStatus = U_ZERO_ERROR;
        bool strippedScriptAndRegion = false;
        UResourceBundle * aliasMapBundle = ures_getByKey(icu::gLanguageAliasesBundle, locale, NULL, &localStatus);
        if (U_FAILURE(localStatus) && uprv_strchr(locale, '_') != NULL) {
            // if we didn't get a resource and the locale we started with has more than just a language code,
            // try again with just the language code and if we succeed, append the script and region codes
            // from the original locale to the end of the normalized one
            char languageOnly[ULOC_FULLNAME_CAPACITY];
            uprv_strcpy(languageOnly, locale);
            *uprv_strchr(languageOnly, '_') = '\0';
            localStatus = U_ZERO_ERROR;
            aliasMapBundle = ures_getByKey(icu::gLanguageAliasesBundle, languageOnly, NULL, &localStatus);
            strippedScriptAndRegion = true;
        }
        
        if (U_SUCCESS(localStatus) && aliasMapBundle != NULL) {
            len = normalizedCapacity;
            ures_getUTF8StringByKey(aliasMapBundle, "replacement", normalized, &len, TRUE, status);
            if (U_SUCCESS(*status) && len >= normalizedCapacity) {
                *status = U_BUFFER_OVERFLOW_ERROR; // treat unterminated as error
            }
            if (strippedScriptAndRegion) {
                // If we stripped off the script and region codes to find an entry for the locale in the alias
                // resource, restore them here.
                // But this is complicated, because the alias table may gives us back a locale ID that itself has
                // a script or region code in it, and we don't want to stomp or duplicate them.
                if (uprv_strchr(normalized, '_') == NULL) {
                    // if the alias entry was just a language code, just add the script and/or region from the original
                    // locale (if there were any) back onto the end
                    uprv_strcat(normalized, uprv_strchr(locale, '_'));
                } else {
                    // if the alias entry ends with a script code (four characters after the last _ mark), append
                    // the region code from the original locale; otherwise, just use the alias entry unchanged
                    int32_t normalizedLength = uprv_strlen(normalized);
                    if (normalizedLength > 5 && normalized[normalizedLength - 5] == '_') {
                        const char* localeRegion = uprv_strrchr(locale, '_');
                        if (uprv_strlen(localeRegion) <= 4) {
                            uprv_strcat(normalized, localeRegion);
                        }
                    }
                }
            }
            ures_close(aliasMapBundle);
            return;
        }
    } 

    len = strnlen(replacement, normalizedCapacity);
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
    // first check in appleParentMap using binary search
    int32_t len;
    const char** entry = (const char**)bsearch(locale, appleParentMap, kAppleParentMapCount, sizeof(appleParentMap[0]), compareAppleMapElements);
    if (entry != NULL) {
        const char* replacement = entry[1];
        len = uprv_strlen(replacement);
        if (len < parentCapacity) { // allow for 0 termination
            uprv_strcpy(parent, replacement);
        } else {
            *status = U_BUFFER_OVERFLOW_ERROR;
        }
        return;
    }
    len = ures_getLocParent(locale, parent, parentCapacity - 1, status);
    if (len > 0 || U_FAILURE(*status)) {
        parent[parentCapacity - 1] = 0; // ensure 0 termination in case of U_STRING_NOT_TERMINATED_WARNING
        return;
    }
    uloc_getParent(locale, parent, parentCapacity - 1, status);
    parent[parentCapacity - 1] = 0; // ensure 0 termination in case of U_STRING_NOT_TERMINATED_WARNING
}

enum { kLangScriptRegMaxLen = ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY }; // currently 22

const int32_t kMaxLocaleIDLength = 58;  // ULOC_FULLNAME_CAPACITY - ULOC_KEYWORD_AND_VALUES_CAPACITY: locales without variants should never be more than 24 chars, the excess is just to cover variant codes (+1 for null termination)
const int32_t kMaxParentChainLength = 10;
const int32_t kCharStorageBlockSize = 650; // very few of the unit tests used more than 650 bytes of character storage

struct LocIDCharStorage {
    char chars[kCharStorageBlockSize];
    char* curTop;
    char* limit;
    LocIDCharStorage* nextBlock;
    
    LocIDCharStorage() : chars(), curTop(chars), limit(curTop + kCharStorageBlockSize), nextBlock(NULL) {}
    ~LocIDCharStorage() { delete nextBlock; }
    
    char* nextPtr() {
        if (nextBlock == NULL) {
            if (limit - curTop > kMaxLocaleIDLength) {
                // return the top of the current block only if there's enough room for a maximum-length locale ID--
                // this keeps us from having to preflight or repeat any of the actual uloc calls and wastes
                // relatively little space
                return curTop;
            } else {
                // if we DON'T have enough space for a max-length locale ID, allocate a new block...
                nextBlock = new LocIDCharStorage();
                // ...and fall through to the line below to return its top pointer
            }
        }
        return nextBlock->nextPtr();
    }
    
    void advance(int32_t charsUsed) {
        if (nextBlock == NULL) {
            curTop += charsUsed;
            *curTop++ = '\0'; // in rare cases, the ICU call might not have null-terminated the result, so force it here
        } else {
            nextBlock->advance(charsUsed);
        }
    }
};

/**
 * Data structure used by ualoc_localizationsToUse() below to cache the various transformed versions of a single locale ID.
 * All char* members are pointers into storage managed separately by the caller-- usually pointers into a separate array of char intended to
 * hold all of the strings in bulk.
 */
struct LocaleIDInfo {
    const char* original;      //< Pointer to the original locale ID
    const char* base;          //< The result of uloc_getBaseName() on the original locale ID
    const char* normalized;    //< The result of ualoc_normalize() on the value of `base`
    const char* language;      //< The language code from `normalized`
    const char* languageGroup; //< Same as `language`, except for certain languages that fall back to other languages
    const char* parentChain[kMaxParentChainLength]; //< Array of the results of calling ualoc_getParent() repeatedly on `normalized`
    
    LocaleIDInfo();
    void initBaseNames(const char* originalID, LocIDCharStorage& charStorage, UErrorCode* err);
    void calcParentChain(LocIDCharStorage& charStorage, UBool penalizeNonDefaultCountry, UErrorCode* err);
    UBool specifiesCountry();
#if DEBUG_UALOC
    void dump(const char *originalID, LocIDCharStorage& charStorage, UBool penalizeNonDefaultCountry, UErrorCode *err);
#endif
};

LocaleIDInfo::LocaleIDInfo() {
    // these are the only two fields that HAVE to be initialized to NULL
    original = NULL;
    parentChain[0] = NULL;
}

/**
 * Caches the `originalID` in `original` and fills in `base`, `normalized`, and `language.  If these fields have already been filled in by an earlier call, this
 * function won't fill them in again.
 * @param originalID The locale ID to base the other values on.
 * @param textPtr A pointer to a `char*` variable that points into an array of character storage maintained by the caller.  The actual characters in this
 * object's strings are written to this storage and `textPtr` is advanced to point to the first memory position after the last string written to the storage.
 * @param textPtrLimit A pointer to the position immediately beyond the end of the separate character storage.  This function won't write beyond
 * this point and will return U_BUFFER_OVERFLOW if the storage is filled (which shouldn't happen).
 * @param err Pointer to a variable holding the ICU error code.
 */
void LocaleIDInfo::initBaseNames(const char *originalID, LocIDCharStorage& charStorage, UErrorCode *err) {
    // don't fill in the fields if they're already filled in
    if (original == NULL) {
        original = originalID;
        
        base = charStorage.nextPtr();
        int32_t length = uloc_getBaseName(original, const_cast<char*>(base), kMaxLocaleIDLength, err);
        charStorage.advance(length);
        
        normalized = charStorage.nextPtr();
        ualoc_normalize(base, const_cast<char*>(normalized), kMaxLocaleIDLength, err);
        charStorage.advance(uprv_strlen(normalized));
        
        language = charStorage.nextPtr();
        length = uloc_getLanguage(normalized, const_cast<char*>(language), kMaxLocaleIDLength, err);
        charStorage.advance(length);
        languageGroup = language;
        
        // The `languageGroup` field is used for performance optimization; we don't need to walk the parent chain if the
        // languages of the two locales being compared are different.  This code accounts for the few cases of different
        // language codes that need to be considered equivalent for comparison purposes.
        static const char* likeLanguages[] = {
            "ars", "ar",
            "hi",  "en",    // Hindi and English obviously aren't in the same group; we do this because hi_Latn falls back to en_IN
            "nb",  "no",
            "nn",  "no",
            "wuu", "zh",
            "yue", "zh"
        };
        for (int32_t i = 0; i < UPRV_LENGTHOF(likeLanguages); i += 2) {
            if (uprv_strcmp(language, likeLanguages[i]) == 0) {
                languageGroup = likeLanguages[i + 1];
                break;
            }
        }
    }
}

/**
 * Calculates the parent chain for the locale ID in `original` by calling `ualoc_getParent()` repeatedly until it returns the empty string or "root".  If this object's
 * parent chain has previously been calculated, this won't do it again. The parent chain in the LocaleIDInfo object is terminated by a NULL entry.
 * @param textPtr A pointer to a `char*` variable that points into an array of character storage maintained by the caller.  The actual characters in this
 * object's strings are written to this storage and `textPtr` is advanced to point to the first memory position after the last string written to the storage.
 * @param textPtrLimit A pointer to the position immediately beyond the end of the separate character storage.  This function won't write beyond
 * this point and will return U_BUFFER_OVERFLOW if the storage is filled (which shouldn't happen).
 * @param penalizeNonDefaultCountry If TRUE, an extra entry is added to the parent chain if the original locale specifies a country other than
 * the default country for the locale's language.
 * @param err Pointer to a variable holding the ICU error code.
 */
void LocaleIDInfo::calcParentChain(LocIDCharStorage& charStorage, UBool penalizeNonDefaultCountry, UErrorCode *err) {
    // don't calculate the parent chain if it's already been calculated
    if (parentChain[0] != NULL) {
        return;
    }
    
    int32_t index = 0;
    
    // Entry 0 in the parent chain is always the same as `normalized`-- this simplifies distance calculations.
    parentChain[index] = normalized;
    
    // If the caller asks to penalize the non-default country (which it does for entries in `availableLocalizations`
    // but not for entries in `preferredLanguages`), check to see if the original locale ID specifies a country code
    // for a country other than the default country for the specified language (as determined by uloc_minimizeSubtags() ).
    // If the country is NOT the default for the language, artifically lengthen the parent chain by also putting
    // `normalized` into entry 1 in the parent chain.  We do this to bias our similarity scores toward the default country.
    // (e.g., if `preferredLanguages` is { it } and `availableLocalizations` is { it_CH, it_IT }, this causes us to return
    // `it_IT` even though it comes second in the list because it's the default country for the language.)
    if (penalizeNonDefaultCountry) {
        UErrorCode dummyErr = U_ZERO_ERROR;
        if (uloc_getCountry(normalized, NULL, 0, &dummyErr) > 0) {
            if (uprv_strcmp(normalized, "es_MX") != 0 && uprv_strcmp(normalized, "zh_Hant_TW") != 0) {
                dummyErr = U_ZERO_ERROR;
                char minimizedLocale[kLocBaseNameMax];
                uloc_minimizeSubtags(normalized, minimizedLocale, kLocBaseNameMax, &dummyErr);
                if (uloc_getCountry(minimizedLocale, NULL, 0, &dummyErr) > 0) {
                    parentChain[++index] = normalized;
                }
            }
        }
    }
    
    // Walk the locale ID's parent chain using ualoc_getParent().  That function will return "" or "root" when it
    // gets to the end of the chain, but internall we use NULL to mark the end of the chain.
    while (++index < kMaxParentChainLength && parentChain[index - 1] != NULL) {
        char* textPtr = charStorage.nextPtr();
        ualoc_getParent(parentChain[index - 1], textPtr, kMaxLocaleIDLength, err);
        if (index + 1 == kMaxParentChainLength || textPtr[0] == '\0' || uprv_strcmp(textPtr, "root") == 0) {
            parentChain[index] = NULL;
        } else {
            parentChain[index] = textPtr;
            charStorage.advance(uprv_strlen(textPtr));
        }
    }
}

UBool LocaleIDInfo::specifiesCountry() {
    UErrorCode err = U_ZERO_ERROR;
    int32_t countryLength = uloc_getCountry(normalized, NULL, 0, &err);
    return countryLength != 0;
}

#if DEBUG_UALOC
/**
 * Debugging function that dumps the contents of this object to stdout.  Parameters are the same as the functions above.
 */
void LocaleIDInfo::dump(const char *originalID, LocIDCharStorage& charStorage, UBool penalizeNonDefaultCountry, UErrorCode *err) {
    initBaseNames(originalID, charStorage, err);
    calcParentChain(charStorage, penalizeNonDefaultCountry, err);
    
    printf("[ %s -> %s -> %s ]", original, base, normalized);
    for (int32_t i = 1; parentChain[i] != NULL; i++) {
        printf(" -> %s", parentChain[i]);
    }
    printf("\n");
}
#endif // DEBUG_UALOC

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
    if (icu::gMapDataState == 0) {
        umtx_initOnce(icu::gUALocaleCacheInitOnce, initializeMapData);
    }
    
#if DEBUG_UALOC
    printf("--------------------------------------------------------------------------------\n");
    printf("Preferred languages: ");
    for (int32_t i = 0; i < preferredLanguagesCount; i++) {
        printf("%s ", preferredLanguages[i]);
    }
    printf("\nAvailable localizations: ");
    for (int32_t i = 0; i < availableLocalizationsCount; i++) {
        printf("%s ", availableLocalizations[i]);
    }
    printf("\n\n");
#endif // DEBUG_UALOC
    
    LocaleIDInfo prefLangInfos[preferredLanguagesCount];
    LocaleIDInfo availLocInfos[availableLocalizationsCount];
    LocIDCharStorage charStorage;
    LocaleIDInfo* result = NULL;
    LocaleIDInfo* portugueseResult = NULL;
    int32_t resultScore = 999;
    
#if DEBUG_UALOC
    for (int32_t i = 0; i < preferredLanguagesCount; i++) {
        prefLangInfos[i].dump(preferredLanguages[i], charStorage, FALSE, status);
    }
    printf("\n");
    for (int32_t i = 0; i < availableLocalizationsCount; i++) {
        availLocInfos[i].dump(availableLocalizations[i], charStorage, TRUE, status);
    }
    printf("\n");
#endif // DEBUG_UALOC
    
    // Loop over the entries in `preferredLanguages` matching them against `availableLocalizations`.  The first preferred
    // language that has a matching available localization is the only one that contributes to the result (except in the
    // case of Portuguese, about which more below).
    for (int32_t prefLangIndex = 0; result == NULL && prefLangIndex < preferredLanguagesCount; ++prefLangIndex) {
        LocaleIDInfo* prefLangInfo = &prefLangInfos[prefLangIndex];
        prefLangInfo->initBaseNames(preferredLanguages[prefLangIndex], charStorage, status);
        
        // Loop over the entries in `availableLocalizations`, looking for the best match to the current entry
        // from `preferredLanguages`.
        for (int32_t availLocIndex = 0; availLocIndex < availableLocalizationsCount; ++availLocIndex) {
            LocaleIDInfo* availLocInfo = &availLocInfos[availLocIndex];
            availLocInfo->initBaseNames(availableLocalizations[availLocIndex], charStorage, status);
            
            // Give the highest preference (a score of -1) to locales whose base names are an exact match.
            if (resultScore > -1 && uprv_strcmp(prefLangInfo->base, availLocInfo->base) == 0) {
                result = availLocInfo;
                resultScore = -1;
            // Give the second-highest preference (a score of 0) to locales whose normalized names are an exact match.
            } else if (resultScore > 0 && uprv_strcmp(prefLangInfo->normalized, availLocInfo->normalized) == 0) {
                result = availLocInfo;
                resultScore = 0;
            } else if (resultScore > 0 && uprv_strcmp(prefLangInfo->languageGroup, availLocInfo->languageGroup) == 0) {
                // If we haven't yet found an exact match, look to see if the two locales have an exact match further
                // down in their parent chains.  We can skip checking the parent chains if the locales' languages are
                // different since (with a couple of important exceptions) the parent chain will never change language.
                prefLangInfo->calcParentChain(charStorage, FALSE, status);
                availLocInfo->calcParentChain(charStorage, TRUE, status);
                
                if (U_SUCCESS(*status)) {
                    // Compare each pair of entries in the two locales' parent chains.  If we find an exact match,
                    // assign it a score based on how deep into the two parent chains it is (preference is given
                    // to matches higher in the two locales' parent chains).  The locale with the lowest score
                    // will be our result.
                    for (int32_t prefLangParentIndex = 0; prefLangInfo->parentChain[prefLangParentIndex] != NULL; ++prefLangParentIndex) {
                        for (int32_t availLocParentIndex = 0; availLocInfo->parentChain[availLocParentIndex] != NULL; ++availLocParentIndex) {
                            if (uprv_strcmp(prefLangInfo->parentChain[prefLangParentIndex], availLocInfo->parentChain[availLocParentIndex]) == 0) {
                                if (uprv_strcmp(prefLangInfo->normalized, "pt_PT") == 0 && uprv_strcmp(availLocInfo->normalized, "pt_BR") == 0) {
                                    // We don't want to match pt_BR with pt_PT unless there are no better matches anywhere--
                                    // if we see this match, store it "off to the side", but continue as though we didn't find
                                    // a match at all.  We only return it if we _don't_ find any other matches.
                                    portugueseResult = availLocInfo;
                                } else {
                                    int32_t score = prefLangParentIndex + availLocParentIndex;
                                    if (uprv_strcmp(prefLangInfo->language, availLocInfo->language) != 0) {
                                        // Add a one-point penalty to the score if the two locales have different languages
                                        ++score;
                                    }
                                    if (score < resultScore) {
                                        resultScore = score;
                                        result = availLocInfo;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // If our result isn't an exact match and does specify a country, check to see if there are any entries further
    // down in the preferred language list that have the same language as the current result but ARE an exact match with
    // something in the available-localizations list.  That is, if the preferred languages list is [ fr-CH, fr-CA ] and
    // the available localizations list is [ fr-FR, fr-CA ], we want to return fr-CA, but we only want to do that with
    // variations of the language we originally matched.  (We do go with the match if it doesn't specify a country--
    // we want "en" to match "en-US" and to be preferred over matches later in the preferred-languages list.)
    // [NOTE: This logic was causing side effects with Chinese, which is more complicated, so for now we have logic
    // to skip it when the original result is Chinese.]
    if (result != NULL && resultScore > 0 && result->specifiesCountry() && uprv_strcmp(result->language, "zh") != 0) {
        for (int32_t prefLangIndex = 0; prefLangIndex < preferredLanguagesCount; ++prefLangIndex) {
            LocaleIDInfo* prefLangInfo = &prefLangInfos[prefLangIndex];
            prefLangInfo->initBaseNames(preferredLanguages[prefLangIndex], charStorage, status);
            if (uprv_strcmp(prefLangInfo->language, result->language) == 0) {
                for (int32_t availLocIndex = 0; availLocIndex < availableLocalizationsCount; ++availLocIndex) {
                    LocaleIDInfo* availLocInfo = &availLocInfos[availLocIndex];
                    if (uprv_strcmp(prefLangInfo->base, availLocInfo->base) == 0 || uprv_strcmp(prefLangInfo->normalized, availLocInfo->normalized) == 0) {
                        result = &availLocInfos[availLocIndex];
                        break;
                    }
                }
            }
        }
    }
        
    // Write out our results.
    int32_t locsToUseCount = 0;
    
    // If the only match we found above is matching pt_PT to pt_BR, we can use it as our result.
    if (result == NULL && portugueseResult != NULL) {
        result = portugueseResult;
    }
    
    // If we found a match above, walk its parent chain and search `availableLocales` for any entries that occur in the
    // main result's parent chain.  If we find any, we want to return those too.  (The extra wrinkles below are to keep
    // us from putting the same locale into the result list more than once.)
    if (result != NULL) {
        localizationsToUse[locsToUseCount++] = result->original;
        
        result->calcParentChain(charStorage, TRUE, status);
        for (int32_t parentChainIndex = 0; result->parentChain[parentChainIndex] != NULL; ++parentChainIndex) {
            if (parentChainIndex > 0 && result->parentChain[parentChainIndex - 1] == result->parentChain[parentChainIndex]) {
                continue;
            }
            for (int32_t availLocIndex = 0; availLocIndex < availableLocalizationsCount; ++availLocIndex) {
                LocaleIDInfo* availLocInfo = &availLocInfos[availLocIndex];
                if (result->original == availLocInfo->original) {
                    continue;
                } else if (locsToUseCount < localizationsToUseCapacity && uprv_strcmp(result->parentChain[parentChainIndex], "zh_Hant_HK") == 0 && uprv_strcmp(availLocInfo->normalized, "zh_Hant_TW") == 0) {
                    // HACK for Chinese: If we find "zh_Hant_HK" while walking the result's parent chain and the available localizations list includes "zh_Hant_TW", include "zh_Hant_TW" in the results list too
                    localizationsToUse[locsToUseCount++] = availLocInfo->original;
                } else if (locsToUseCount < localizationsToUseCapacity && uprv_strcmp(result->parentChain[parentChainIndex], availLocInfo->normalized) == 0) {
                    localizationsToUse[locsToUseCount++] = availLocInfo->original;
                }
            }
        }
    }
    
    // if our result array is empty, check to see if the availableLocalizations list contains the special sentinel
    // value "zxx" (which means "no linguistic content").  If it does, return that instead of the empty list
    if (locsToUseCount == 0) {
        int32_t zxxPos = -1;
        for (int32_t i = 0; i < availableLocalizationsCount; i++) {
            if (uprv_strcmp(availableLocalizations[i], "zxx") == 0) {
                zxxPos = i;
                break;
            }
        }
        if (zxxPos >= 0) {
            localizationsToUse[locsToUseCount++] = availableLocalizations[zxxPos];
        }
    }
    
#if DEBUG_UALOC
    printf("Localizations to use: ");
    for (int32_t i = 0; i < locsToUseCount; i++) {
        printf("%s ", localizationsToUse[i]);
    }
    printf("\n\n");
#endif // DEBUG_UALOC
    return locsToUseCount;
}
