/*
*****************************************************************************************
* Copyright (C) 2014 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#ifndef UALOC_H
#define UALOC_H

#include "unicode/utypes.h"

#ifndef U_HIDE_DRAFT_API

/** 
 * Codes for language status in a country or region.
 * @draft ICU 54
 */
typedef enum {
    /**
     * The status is unknown, or the language has no special status.
     * @draft ICU 54 */
    UALANGSTATUS_UNSPECIFIED       = 0,
    /**
     * The language is official in a region of the specified country,
     * e.g. Hawaiian for U.S.
     * @draft ICU 54 */
    UALANGSTATUS_REGIONAL_OFFICIAL = 4,
    /**
     * The language is de-facto official for the specified country or region,
     * e.g. English for U.S.
     * @draft ICU 54 */
    UALANGSTATUS_DEFACTO_OFFICIAL  = 8,
    /**
     * The language is explicitly official for the specified country or region.
     * @draft ICU 54 */
    UALANGSTATUS_OFFICIAL          = 12
} UALanguageStatus;

/**
 * UALANGDATA_CODELEN is the maximum length of a language code
 * (language subtag, possible extension, possible script subtag)
 * in the UALanguageEntry struct. 
 * @draft ICU 54
 */
#define UALANGDATA_CODELEN 23

/**
 * The UALanguageEntry structure provides information about
 * one of the languages for a specified region.
 * @draft ICU 54
 */
typedef struct {
    char languageCode[UALANGDATA_CODELEN + 1];  /**< language code, may include script or
                                                   other subtags after primary language.
                                                   This may be "und" (undetermined) for
                                                   some regions such as AQ Antarctica.
                                                   @draft ICU 54 */
    double userFraction;      /**< fraction of region's population (from 0.0 to 1.0) that
                                uses this language (not necessarily as a first language).
                                This may be 0.0 if the fraction is known to be very small.
                                @draft ICU 54 */
    UALanguageStatus status;  /**< status of the language, if any.
                                @draft ICU 54 */
} UALanguageEntry;

/**
 * Fills out a provided UALanguageEntry entry with information about the languages
 * used in a specified region.
 *
 * @param regionID
 *          The specified regionID (currently only ISO-3166-style IDs are supported).
 * @param minimumFraction
 *          The minimum fraction of language users for a language to be included
 *          in the UALanguageEntry array. Must be in the range 0.0 to 1.0; set to
 *          0.0 if no minimum threshold is desired. As an example, as of March 2014
 *          ICU lists 74 languages for India; setting minimumFraction to 0.001 (0.1%)
 *          skips 27, keeping the top 47 languages for inclusion in the entries array
 *          (depending on entriesCapacity); setting minimumFraction to 0.01 (1%)
 *          skips 54, keeping the top 20 for inclusion.
 * @param entries
 *          Caller-provided array of UALanguageEntry elements. This will be filled
 *          out with information for languages of the specified region that meet
 *          the minimumFraction threshold, sorted in descending order by
 *          userFraction, up to the number of elements specified by entriesCapacity
 *          (so the number of languages for which data is provided can be limited by
 *          total count, by userFraction, or both).
 *          Preflight option: You may set this to NULL (and entriesCapacity to 0)
 *          to just obtain a count of all of the languages that meet the specified
 *          minimumFraction, without actually getting data for any of them.
 * @param entriesCapacity
 *          The number of elements in the provided entries array. Must be 0 if
 *          entries is NULL (preflight option).
 * @param status
 *          A pointer to a UErrorCode to receive any errors, for example
 *          U_MISSING_RESOURCE_ERROR if there is no data available for the
 *          specified region.
 * @return
 *          The number of elements in entries filled out with data, or if
 *          entries is NULL and entriesCapacity is 0 (preflight option ), the total
 *          number of languages for the specified region that meet the minimumFraction
 *          threshold.
 * @draft ICU 54
 */
U_DRAFT int32_t U_EXPORT2
ualoc_getLanguagesForRegion(const char *regionID, double minimumFraction,
                            UALanguageEntry *entries, int32_t entriesCapacity,
                            UErrorCode *err);


/**
 * Gets the desired lproj parent locale ID for the specified locale,
 * using ICU inheritance chain plus Apple additions (for zh*). For
 * example, provided any ID in the following chains it would return
 * the next one to the right:
 *
 *                                 en_US → en → root;
 *                en_IN → en_GB → en_001 → en → root;
 *                                 es_ES → es → root;
 *                        es_MX → es_419 → es → root;
 *                                        haw → root;
 *                                 pt_BR → pt → root;
 *                                 pt_PT → pt → root;
 *                               sr_Cyrl → sr → root;
 *                                    sr_Latn → root;
 *                       zh_Hans → zh → zh_CN → root;
 *  zh_Hant_MO → zh_Hant_HK → zh_Hant → zh_TW → root;
 *                                       root → root;
 *                                        tlh → root;
 *
 * @param localeID  The locale whose parent to get. This can use either '-' or '_' for separator.
 * @param parent    Buffer into which the parent localeID will be copied.
 *                  This will always use '_' for separator.
 * @param parentCapacity  The size of the buffer for parent localeID (including room for null terminator).
 * @param err       Pointer to UErrorCode. If on input it indicates failure, function will return 0.
 *                  If on output it indicates an error the contents of parent buffer are undefined.
 * @return          The actual buffer length of the parent localeID. If it is greater than parentCapacity,
 *                  an overflow error will be set and the contents of parent buffer are undefined.   
 * @draft ICU 53
 */
U_DRAFT int32_t U_EXPORT2
ualoc_getAppleParent(const char* localeID,
                     char * parent,
                     int32_t parentCapacity,
                     UErrorCode* err);

#endif /* U_HIDE_DRAFT_API */
#endif /*UALOC_H*/
