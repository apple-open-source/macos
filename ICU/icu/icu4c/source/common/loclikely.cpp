// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  loclikely.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010feb25
*   created by: Markus W. Scherer
*
*   Code for likely and minimized locale subtags, separated out from other .cpp files
*   that then do not depend on resource bundle code and likely-subtags data.
*/

#include <string_view>
#include <utility>

#include "unicode/bytestream.h"
#include "unicode/utypes.h"
#include "unicode/localebuilder.h"
#include "unicode/locid.h"
#include "unicode/putil.h"
#include "unicode/uchar.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/uscript.h"
#include "bytesinkutil.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#if !APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
#include "loclikelysubtags.h"
#endif // !APPLE_ICU_CHANGES
#include "ulocimp.h"

namespace {

#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
/**
 * These are the canonical strings for unknown languages, scripts and regions.
 **/
static const char* const unknownLanguage = "und";
static const char* const unknownScript = "Zzzz";
static const char* const unknownRegion = "ZZ";

/**
 * This function looks for the localeID in the likelySubtags resource.
 *
 * @param localeID The tag to find.
 * @param buffer A buffer to hold the matching entry
 * @param bufferLength The length of the output buffer
 * @return A pointer to "buffer" if found, or a null pointer if not.
 */
static const char*  U_CALLCONV
findLikelySubtags(const char* localeID,
                  char* buffer,
                  int32_t bufferLength,
                  UErrorCode* err) {
    const char* result = NULL;

    if (!U_FAILURE(*err)) {
        int32_t resLen = 0;
        const UChar* s = NULL;
        UErrorCode tmpErr = U_ZERO_ERROR;
        icu::LocalUResourceBundlePointer subtags(ures_openDirect(NULL, "likelySubtags", &tmpErr));
        if (U_SUCCESS(tmpErr)) {
            icu::CharString und;
            if (localeID != NULL) {
                if (*localeID == '\0') {
                    localeID = unknownLanguage;
                } else if (*localeID == '_') {
                    und.append(unknownLanguage, *err);
                    und.append(localeID, *err);
                    if (U_FAILURE(*err)) {
                        return NULL;
                    }
                    localeID = und.data();
                }
            }
            s = ures_getStringByKey(subtags.getAlias(), localeID, &resLen, &tmpErr);

            if (U_FAILURE(tmpErr)) {
                /*
                 * If a resource is missing, it's not really an error, it's
                 * just that we don't have any data for that particular locale ID.
                 */
                if (tmpErr != U_MISSING_RESOURCE_ERROR) {
                    *err = tmpErr;
                }
            }
            else if (resLen >= bufferLength) {
                /* The buffer should never overflow. */
                *err = U_INTERNAL_PROGRAM_ERROR;
            }
            else {
                u_UCharsToChars(s, buffer, resLen + 1);
                result = buffer;
            }
        } else {
            *err = tmpErr;
        }
    }

    return result;
}
#endif // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
/**
 * Append a tag to a buffer, adding the separator if necessary.  The buffer
 * must be large enough to contain the resulting tag plus any separator
 * necessary. The tag must not be a zero-length string.
 *
 * @param tag The tag to add.
 * @param tagLength The length of the tag.
 * @param buffer The output buffer.
 * @param bufferLength The length of the output buffer.  This is an input/output parameter.
 **/
static void U_CALLCONV
appendTag(
    const char* tag,
    int32_t tagLength,
    char* buffer,
    int32_t* bufferLength,
    UBool withSeparator) {

    if (withSeparator) {
        buffer[*bufferLength] = '_';
        ++(*bufferLength);
    }

    uprv_memmove(
        &buffer[*bufferLength],
        tag,
        tagLength);

    *bufferLength += tagLength;
}

/**
 * Create a tag string from the supplied parameters.  The lang, script and region
 * parameters may be nullptr pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.
 *
 * If any of the language, script or region parameters are empty, and the alternateTags
 * parameter is not nullptr, it will be parsed for potential language, script and region tags
 * to be used when constructing the new tag.  If the alternateTags parameter is nullptr, or
 * it contains no language tag, the default tag for the unknown language is used.
 *
 * If the length of the new string exceeds the capacity of the output buffer, 
 * the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * Note that this function can return the warning U_STRING_NOT_TERMINATED_WARNING if
 * the tag string fits in the output buffer, but the null terminator doesn't.
 *
 * @param lang The language tag to use.
 * @param langLength The length of the language tag.
 * @param script The script tag to use.
 * @param scriptLength The length of the script tag.
 * @param region The region tag to use.
 * @param regionLength The length of the region tag.
 * @param trailing Any trailing data to append to the new tag.
 * @param trailingLength The length of the trailing data.
 * @param alternateTags A string containing any alternate tags.
 * @param sink The output sink receiving the tag string.
 * @param err A pointer to a UErrorCode for error reporting.
 **/
static void U_CALLCONV
createTagStringWithAlternates(
    const icu::CharString& lang,
    const icu::CharString& script,
    const icu::CharString& region,
    const char* trailing,
    int32_t trailingLength,
    const char* alternateTags,
    icu::ByteSink& sink,
    UErrorCode* err) {

    if (U_FAILURE(*err)) {
        goto error;
    }
    else {
        /**
         * ULOC_FULLNAME_CAPACITY will provide enough capacity
         * that we can build a string that contains the language,
         * script and region code without worrying about overrunning
         * the user-supplied buffer.
         **/
        char tagBuffer[ULOC_FULLNAME_CAPACITY];
        int32_t tagLength = 0;
        UBool regionAppended = false;

        if (!lang.isEmpty()) {
            appendTag(
                lang.data(),
                lang.length(),
                tagBuffer,
                &tagLength,
                /*withSeparator=*/false);
        }
        else if (alternateTags == nullptr) {
            /*
             * Use the empty string for an unknown language, if
             * we found no language.
             */
        }
        else {
            /*
             * Parse the alternateTags string for the language.
             */
            char alternateLang[ULOC_LANG_CAPACITY];
            int32_t alternateLangLength = sizeof(alternateLang);

            alternateLangLength =
                uloc_getLanguage(
                    alternateTags,
                    alternateLang,
                    alternateLangLength,
                    err);
            if(U_FAILURE(*err) ||
                alternateLangLength >= ULOC_LANG_CAPACITY) {
                goto error;
            }
            else if (alternateLangLength == 0) {
                /*
                 * Use the empty string for an unknown language, if
                 * we found no language.
                 */
            }
            else {
                appendTag(
                    alternateLang,
                    alternateLangLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/false);
            }
        }

        if (!script.isEmpty()) {
            appendTag(
                script.data(),
                script.length(),
                tagBuffer,
                &tagLength,
                /*withSeparator=*/true);
        }
        else if (alternateTags != nullptr) {
            /*
             * Parse the alternateTags string for the script.
             */
            char alternateScript[ULOC_SCRIPT_CAPACITY];

            const int32_t alternateScriptLength =
                uloc_getScript(
                    alternateTags,
                    alternateScript,
                    sizeof(alternateScript),
                    err);

            if (U_FAILURE(*err) ||
                alternateScriptLength >= ULOC_SCRIPT_CAPACITY) {
                goto error;
            }
            else if (alternateScriptLength > 0) {
                appendTag(
                    alternateScript,
                    alternateScriptLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/true);
            }
        }

        if (!region.isEmpty()) {
            appendTag(
                region.data(),
                region.length(),
                tagBuffer,
                &tagLength,
                /*withSeparator=*/true);

            regionAppended = true;
        }
        else if (alternateTags != nullptr) {
            /*
             * Parse the alternateTags string for the region.
             */
            char alternateRegion[ULOC_COUNTRY_CAPACITY];

            const int32_t alternateRegionLength =
                uloc_getCountry(
                    alternateTags,
                    alternateRegion,
                    sizeof(alternateRegion),
                    err);
            if (U_FAILURE(*err) ||
                alternateRegionLength >= ULOC_COUNTRY_CAPACITY) {
                goto error;
            }
            else if (alternateRegionLength > 0) {
                appendTag(
                    alternateRegion,
                    alternateRegionLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/true);

                regionAppended = true;
            }
        }

        /**
         * Copy the partial tag from our internal buffer to the supplied
         * target.
         **/
        sink.Append(tagBuffer, tagLength);

        if (trailingLength > 0) {
            if (*trailing != '@') {
                sink.Append("_", 1);
                if (!regionAppended) {
                    /* extra separator is required */
                    sink.Append("_", 1);
                }
            }

            /*
             * Copy the trailing data into the supplied buffer.
             */
            sink.Append(trailing, trailingLength);
        }

        return;
    }

error:

    /**
     * An overflow indicates the locale ID passed in
     * is ill-formed.  If we got here, and there was
     * no previous error, it's an implicit overflow.
     **/
    if (*err ==  U_BUFFER_OVERFLOW_ERROR ||
        U_SUCCESS(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }
}
#else
/**
 * Create a tag string from the supplied parameters.  The lang, script and region
 * parameters may be nullptr pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param lang The language tag to use.
 * @param langLength The length of the language tag.
 * @param script The script tag to use.
 * @param scriptLength The length of the script tag.
 * @param region The region tag to use.
 * @param regionLength The length of the region tag.
 * @param variant The region tag to use.
 * @param variantLength The length of the region tag.
 * @param trailing Any trailing data to append to the new tag.
 * @param trailingLength The length of the trailing data.
 * @param sink The output sink receiving the tag string.
 * @param err A pointer to a UErrorCode for error reporting.
 **/
void U_CALLCONV
createTagStringWithAlternates(
    const char* lang,
    int32_t langLength,
    const char* script,
    int32_t scriptLength,
    const char* region,
    int32_t regionLength,
    const char* variant,
    int32_t variantLength,
    const char* trailing,
    int32_t trailingLength,
    icu::ByteSink& sink,
    UErrorCode& err) {
    if (U_FAILURE(err)) {
        return;
    }

    if (langLength >= ULOC_LANG_CAPACITY ||
            scriptLength >= ULOC_SCRIPT_CAPACITY ||
            regionLength >= ULOC_COUNTRY_CAPACITY) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (langLength > 0) {
        sink.Append(lang, langLength);
    }

    if (scriptLength > 0) {
        sink.Append("_", 1);
        sink.Append(script, scriptLength);
    }

    if (regionLength > 0) {
        sink.Append("_", 1);
        sink.Append(region, regionLength);
    }

    if (variantLength > 0) {
        if (regionLength == 0) {
            /* extra separator is required */
            sink.Append("_", 1);
        }
        sink.Append("_", 1);
        sink.Append(variant, variantLength);
    }

    if (trailingLength > 0) {
        /*
         * Copy the trailing data into the supplied buffer.
         */
        sink.Append(trailing, trailingLength);
    }
}
#endif // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
/**
 * Create a tag string from the supplied parameters.  The lang, script and region
 * parameters may be NULL pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.  If the lang parameter is an empty string, the
 * default value for an unknown language is written to the output buffer.
 *
 * If the length of the new string exceeds the capacity of the output buffer, 
 * the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param lang The language tag to use.
 * @param langLength The length of the language tag.
 * @param script The script tag to use.
 * @param scriptLength The length of the script tag.
 * @param region The region tag to use.
 * @param regionLength The length of the region tag.
 * @param trailing Any trailing data to append to the new tag.
 * @param trailingLength The length of the trailing data.
 * @param sink The output sink receiving the tag string.
 * @param err A pointer to a UErrorCode for error reporting.
 **/
static void U_CALLCONV
createTagString(
    const icu::CharString& lang,
    const icu::CharString& script,
    const icu::CharString& region,
    const char* trailing,
    int32_t trailingLength,
    icu::ByteSink& sink,
    UErrorCode* err)
{
    createTagStringWithAlternates(
                lang,
                script,
                region,
                trailing,
                trailingLength,
                NULL,
                sink,
                err);
}

/**
 * Parse the language, script, and region subtags from a tag string, and copy the
 * results into the corresponding output parameters. The buffers are null-terminated,
 * unless overflow occurs.
 *
 * The langLength, scriptLength, and regionLength parameters are input/output
 * parameters, and must contain the capacity of their corresponding buffers on
 * input.  On output, they will contain the actual length of the buffers, not
 * including the null terminator.
 *
 * If the length of any of the output subtags exceeds the capacity of the corresponding
 * buffer, the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.  It will not parse any more subtags once overflow
 * occurs.
 *
 * (NOTE: This function is now Apple-specific, interal, and temporary, so I'm
 * not bothering to update the docs, as I'm hoping this whole function can be removed in
 * the near future.  --rtg 12/12/24(
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param localeID The locale ID to parse.
 * @param lang The language tag buffer.
 * @param langLength The length of the language tag.
 * @param script The script tag buffer.
 * @param scriptLength The length of the script tag.
 * @param region The region tag buffer.
 * @param regionLength The length of the region tag.
 * @param err A pointer to a UErrorCode for error reporting.
 * @return The number of chars of the localeID parameter consumed.
 **/
static void U_CALLCONV
parseTagString(
    const char* localeID,
    icu::CharString& lang,
    icu::CharString& script,
    icu::CharString& region,
    UErrorCode& err)
{
    if (U_FAILURE(err)) {
        return;
    }
    
    lang = ulocimp_getLanguage(localeID, err);
    script = ulocimp_getScript(localeID, err);
    if (script.toStringPiece().compare(unknownScript) == 0) {
        script.clear();
    }
    region = ulocimp_getRegion(localeID, err);
    if (region.toStringPiece().compare(unknownRegion) == 0) {
        region.clear();
    }
}

static UBool U_CALLCONV
createLikelySubtagsString(
    const icu::CharString& lang,
    const icu::CharString& script,
    const icu::CharString& region,
    const char* variants,
    int32_t variantsLength,
    icu::ByteSink& sink,
    UErrorCode* err) {
    /**
     * ULOC_FULLNAME_CAPACITY will provide enough capacity
     * that we can build a string that contains the language,
     * script and region code without worrying about overrunning
     * the user-supplied buffer.
     **/
    char likelySubtagsBuffer[ULOC_FULLNAME_CAPACITY];

    if(U_FAILURE(*err)) {
        goto error;
    }

    /**
     * Try the language with the script and region first.
     **/
    if (!script.isEmpty() && !region.isEmpty()) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                script,
                region,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != nullptr) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        icu::CharString(),
                        icu::CharString(),
                        icu::CharString(),
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Try the language with just the script.
     **/
    if (!script.isEmpty()) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                script,
                icu::CharString(),
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != nullptr) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        icu::CharString(),
                        icu::CharString(),
                        region,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Try the language with just the region.
     **/
    if (!region.isEmpty()) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                icu::CharString(),
                region,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        icu::CharString(),
                        script,
                        icu::CharString(),
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Finally, try just the language.
     **/
    {
        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                icu::CharString(),
                icu::CharString(),
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        icu::CharString(),
                        script,
                        region,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    return false;

error:

    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }

    return false;
}

#endif // APPLE_ICU_CHANGES

bool CHECK_TRAILING_VARIANT_SIZE(const char* variant, int32_t variantLength) {
    int32_t count = 0;
    for (int32_t i = 0; i < variantLength; i++) {
        if (_isIDSeparator(variant[i])) {
            count = 0;
        } else if (count == 8) {
            return false;
        } else {
            count++;
        }
    }
    return true;
}

#if APPLE_ICU_CHANGES
// rdar://116151591 The old code (before being rewritten to use the langInfo data) would return "root" when
bool
_uloc_addLikelySubtags(const char* localeID,
                       icu::ByteSink& sink,
                       UErrorCode& err) {
    if (U_FAILURE(err)) {
        return false;
    }

    if (localeID == nullptr) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }

    // rdar://116151591 The old code (before being rewritten to use the langInfo data) would return "root" when
    // passed "root"-- preserve that behavior
    if (uprv_strcmp(localeID, "root") == 0) {
        sink.Append(localeID, uprv_strlen(localeID));
        return false;
    }

    icu::CharString lang;
    icu::CharString script;
    icu::CharString region;
    icu::CharString variant;
    const char* trailing = nullptr;
    ulocimp_getSubtags(localeID, &lang, &script, &region, &variant, &trailing, err);
    // the old Apple code calls parseTagString() instead of ulocimp_getSubtags() (OS changed this),
    // but parseTagString() checks for script code Zzzz and region code ZZ, neither of which
    // are done by ulocimp_getSubtags().  We can't replace the call here, because ulocimp_getSubtags()
    // fills in more stuff, but we CAN just call parseTagString() AFTER we call ulocimp_getSubtags().
    // There's a performance penalty to doing this; I'm hoping it's not too severe.
    parseTagString(localeID, lang, script, region, err);
    if (U_FAILURE(err)) {
        return false;
    }

    if (!CHECK_TRAILING_VARIANT_SIZE(variant.data(), variant.length())) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }

    if (lang.length() == 4) {
        if (script.isEmpty()) {
            script = std::move(lang);
            lang.clear();
        } else {
            err = U_ILLEGAL_ARGUMENT_ERROR;
            return false;
        }
    } else if (lang.length() > 8) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }

    int32_t trailingLength = static_cast<int32_t>(uprv_strlen(trailing));

    if (!variant.isEmpty()) {
        trailing -= variant.length();
        trailingLength = static_cast<int32_t>(uprv_strlen(trailing));
    }

    bool createSucceeded = createLikelySubtagsString(
        lang,
        script,
        region,
        trailing,
        trailingLength,
        sink,
        &err);

    if (U_FAILURE(err) || !createSucceeded) {
        const int32_t localIDLength = (int32_t)uprv_strlen(localeID);

        /*
         * If we get here, we need to return localeID.
         */
        sink.Append(localeID, localIDLength);
        return false;
    }
    return true;
}
#else
void
_uloc_addLikelySubtags(const char* localeID,
                       icu::ByteSink& sink,
                       UErrorCode& err) {
    if (U_FAILURE(err)) {
        return;
    }

    if (localeID == nullptr) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    icu::CharString lang;
    icu::CharString script;
    icu::CharString region;
    icu::CharString variant;
    const char* trailing = nullptr;
    ulocimp_getSubtags(localeID, &lang, &script, &region, &variant, &trailing, err);
    if (U_FAILURE(err)) {
        return;
    }

    if (!CHECK_TRAILING_VARIANT_SIZE(variant.data(), variant.length())) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (lang.length() == 4) {
        if (script.isEmpty()) {
            script = std::move(lang);
            lang.clear();
        } else {
            err = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    } else if (lang.length() > 8) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t trailingLength = static_cast<int32_t>(uprv_strlen(trailing));

    const icu::LikelySubtags* likelySubtags = icu::LikelySubtags::getSingleton(err);
    if (U_FAILURE(err)) {
        return;
    }
    // We need to keep l on the stack because lsr may point into internal
    // memory of l.
    icu::Locale l = icu::Locale::createFromName(localeID);
    if (l.isBogus()) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    icu::LSR lsr = likelySubtags->makeMaximizedLsrFrom(l, true, err);
    if (U_FAILURE(err)) {
        return;
    }
    const char* language = lsr.language;
    if (uprv_strcmp(language, "und") == 0) {
        language = "";
    }
    createTagStringWithAlternates(
        language,
        static_cast<int32_t>(uprv_strlen(language)),
        lsr.script,
        static_cast<int32_t>(uprv_strlen(lsr.script)),
        lsr.region,
        static_cast<int32_t>(uprv_strlen(lsr.region)),
        variant.data(),
        variant.length(),
        trailing,
        trailingLength,
        sink,
        err);
}
#endif // APPLE_ICU_CHANGES

void
_uloc_minimizeSubtags(const char* localeID,
                      icu::ByteSink& sink,
#if !APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
                      bool favorScript,
#endif // !APPLE_ICU_CHANGES
                      UErrorCode& err) {
    if (U_FAILURE(err)) {
        return;
    }

    if (localeID == nullptr) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    icu::CharString lang;
    icu::CharString script;
    icu::CharString region;
    icu::CharString variant;
    const char* trailing = nullptr;
    ulocimp_getSubtags(localeID, &lang, &script, &region, &variant, &trailing, err);
    if (U_FAILURE(err)) {
        return;
    }

    if (!CHECK_TRAILING_VARIANT_SIZE(variant.data(), variant.length())) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t trailingLength = static_cast<int32_t>(uprv_strlen(trailing));

#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
    if (!variant.isEmpty()) {
        trailing -= variant.length();
        trailingLength = static_cast<int32_t>(uprv_strlen(trailing));
    }
    
    icu::CharString maximizedTagBuffer;
    icu::CharString base;
    {
        icu::CharStringByteSink baseSink(&base);
        createTagString(
            lang,
            script,
            region,
            NULL,
            0,
            baseSink,
            &err);
    }

    /**
     * First, we need to first get the maximization
     * from AddLikelySubtags.
     **/
    {
        icu::CharStringByteSink maxSink(&maximizedTagBuffer);
        bool maxSucceeded = _uloc_addLikelySubtags(base.data(), maxSink, err);

        if (!maxSucceeded) {
            /**
             * If ulocimp_addLikelySubtags() couldn't maximize and returned the original locale ID unchanged,
             * we want to do the same here
             **/
            const int32_t localeIDLength = (int32_t)uprv_strlen(localeID);
            sink.Append(localeID, localeIDLength);
            return;
        }
    }

    if(U_FAILURE(err)) {
        goto error;
    }


    // In the following, the lang, script, region are referring to those in
    // the maximizedTagBuffer, not the one in the localeID.
    parseTagString(
        maximizedTagBuffer.data(),
        lang,
        script,
        region,
        err);
    if(U_FAILURE(err)) {
        goto error;
    }

    /**
     * Start first with just the language.
     **/
    {
        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                icu::CharString(),
                icu::CharString(),
                NULL,
                0,
                tagSink,
                &err);
        }
        if(U_FAILURE(err)) {
            goto error;
        }
        else if (uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        icu::CharString(),
                        icu::CharString(),
                        trailing,
                        trailingLength,
                        sink,
                        &err);
            return;
        }
    }

    /**
     * Next, try the language and region.
     **/
    if (!region.isEmpty()) {

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                icu::CharString(),
                region,
                NULL,
                0,
                tagSink,
                &err);
        }
        if(U_FAILURE(err)) {
            goto error;
        }
       else if (!tagBuffer.isEmpty() &&
                 uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        icu::CharString(),
                        region,
                        trailing,
                        trailingLength,
                        sink,
                        &err);
            return;
        }
    }

    /**
     * Finally, try the language and script.  This is our last chance,
     * since trying with all three subtags would only yield the
     * maximal version that we already have.
     **/
    if (!script.isEmpty()) {
        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                script,
                icu::CharString(),
                NULL,
                0,
                tagSink,
                &err);
        }

        if(U_FAILURE(err)) {
            goto error;
        }
        else if (!tagBuffer.isEmpty() &&
                 uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        script,
                        icu::CharString(),
                        trailing,
                        trailingLength,
                        sink,
                        &err);
            return;
        }
    }

    {
        /**
         * If we got here, return the max + trail.
         **/
        createTagString(
                    lang,
                    script,
                    region,
                    trailing,
                    trailingLength,
                    sink,
                    &err);
        return;
    }

error:

    if (!U_FAILURE(err)) {
        err = U_ILLEGAL_ARGUMENT_ERROR;
    }
#else
    const icu::LikelySubtags* likelySubtags = icu::LikelySubtags::getSingleton(err);
    if (U_FAILURE(err)) {
        return;
    }
    icu::LSR lsr = likelySubtags->minimizeSubtags(
        lang.toStringPiece(),
        script.toStringPiece(),
        region.toStringPiece(),
        favorScript,
        err);
    if (U_FAILURE(err)) {
        return;
    }
    const char* language = lsr.language;
    if (uprv_strcmp(language, "und") == 0) {
        language = "";
    }
    createTagStringWithAlternates(
        language,
        static_cast<int32_t>(uprv_strlen(language)),
        lsr.script,
        static_cast<int32_t>(uprv_strlen(lsr.script)),
        lsr.region,
        static_cast<int32_t>(uprv_strlen(lsr.region)),
        variant.data(),
        variant.length(),
        trailing,
        trailingLength,
        sink,
        err);
#endif // APPLE_ICU_CHANGES
}

}  // namespace

U_CAPI int32_t U_EXPORT2
uloc_addLikelySubtags(const char* localeID,
                      char* maximizedLocaleID,
                      int32_t maximizedLocaleIDCapacity,
                      UErrorCode* status) {
    return icu::ByteSinkUtil::viaByteSinkToTerminatedChars(
        maximizedLocaleID, maximizedLocaleIDCapacity,
        [&](icu::ByteSink& sink, UErrorCode& status) {
            ulocimp_addLikelySubtags(localeID, sink, status);
        },
        *status);
}

U_EXPORT icu::CharString
ulocimp_addLikelySubtags(const char* localeID,
                         UErrorCode& status) {
    return icu::ByteSinkUtil::viaByteSinkToCharString(
        [&](icu::ByteSink& sink, UErrorCode& status) {
            ulocimp_addLikelySubtags(localeID, sink, status);
        },
        status);
}

U_EXPORT void
ulocimp_addLikelySubtags(const char* localeID,
                         icu::ByteSink& sink,
                         UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    icu::CharString localeBuffer = ulocimp_canonicalize(localeID, status);
    _uloc_addLikelySubtags(localeBuffer.data(), sink, status);
}

U_CAPI int32_t U_EXPORT2
uloc_minimizeSubtags(const char* localeID,
                     char* minimizedLocaleID,
                     int32_t minimizedLocaleIDCapacity,
                     UErrorCode* status) {
    return icu::ByteSinkUtil::viaByteSinkToTerminatedChars(
        minimizedLocaleID, minimizedLocaleIDCapacity,
        [&](icu::ByteSink& sink, UErrorCode& status) {
            ulocimp_minimizeSubtags(localeID, sink, false, status);
        },
        *status);
}

U_EXPORT icu::CharString
ulocimp_minimizeSubtags(const char* localeID,
#if !APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
                        bool favorScript,
#endif // !APPLE_ICU_CHANGES
                        UErrorCode& status) {
    return icu::ByteSinkUtil::viaByteSinkToCharString(
        [&](icu::ByteSink& sink, UErrorCode& status) {
#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
            ulocimp_minimizeSubtags(localeID, sink, false, status);
#else
            ulocimp_minimizeSubtags(localeID, sink, favorScript, status);
#endif // APPLE_ICU_CHANGES
        },
        status);
}

U_EXPORT void
ulocimp_minimizeSubtags(const char* localeID,
                        icu::ByteSink& sink,
                        bool favorScript,
                        UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    icu::CharString localeBuffer = ulocimp_canonicalize(localeID, status);
#if APPLE_ICU_CHANGES
// rdar://121488288 (21E188: footprint regressions in multiple processes since 21E187)
    _uloc_minimizeSubtags(localeBuffer.data(), sink, status);
#else
    _uloc_minimizeSubtags(localeBuffer.data(), sink, favorScript, status);
#endif // APPLE_ICU_CHANGES
}

// Pairs of (language subtag, + or -) for finding out fast if common languages
// are LTR (minus) or RTL (plus).
static const char LANG_DIR_STRING[] =
        "root-en-es-pt-zh-ja-ko-de-fr-it-ar+he+fa+ru-nl-pl-th-tr-";

// Implemented here because this calls ulocimp_addLikelySubtags().
U_CAPI UBool U_EXPORT2
uloc_isRightToLeft(const char *locale) {
    UErrorCode errorCode = U_ZERO_ERROR;
    icu::CharString lang;
    icu::CharString script;
    ulocimp_getSubtags(locale, &lang, &script, nullptr, nullptr, nullptr, errorCode);
    if (U_FAILURE(errorCode) || script.isEmpty()) {
        // Fastpath: We know the likely scripts and their writing direction
        // for some common languages.
        if (!lang.isEmpty()) {
            const char* langPtr = uprv_strstr(LANG_DIR_STRING, lang.data());
            if (langPtr != nullptr) {
                switch (langPtr[lang.length()]) {
                case '-': return false;
                case '+': return true;
                default: break;  // partial match of a longer code
                }
            }
        }
        // Otherwise, find the likely script.
        errorCode = U_ZERO_ERROR;
        icu::CharString likely = ulocimp_addLikelySubtags(locale, errorCode);
        if (U_FAILURE(errorCode)) {
            return false;
        }
        ulocimp_getSubtags(likely.data(), nullptr, &script, nullptr, nullptr, nullptr, errorCode);
        if (U_FAILURE(errorCode) || script.isEmpty()) {
            return false;
        }
    }
    UScriptCode scriptCode = (UScriptCode)u_getPropertyValueEnum(UCHAR_SCRIPT, script.data());
    return uscript_isRightToLeft(scriptCode);
}

U_NAMESPACE_BEGIN

UBool
Locale::isRightToLeft() const {
    return uloc_isRightToLeft(getBaseName());
}

U_NAMESPACE_END

namespace {
icu::CharString
GetRegionFromKey(const char* localeID, std::string_view key, UErrorCode& status) {
    icu::CharString result;
    // First check for keyword value
    icu::CharString kw = ulocimp_getKeywordValue(localeID, key, status);
    int32_t len = kw.length();
    // In UTS35
    //   type = alphanum{3,8} (sep alphanum{3,8})* ;
    // so we know the subdivision must fit the type already.
    //
    //   unicode_subdivision_id = unicode_region_subtag unicode_subdivision_suffix ;
    //   unicode_region_subtag = (alpha{2} | digit{3}) ;
    //   unicode_subdivision_suffix = alphanum{1,4} ;
    // But we also know there are no id in start with digit{3} in
    // https://github.com/unicode-org/cldr/blob/main/common/validity/subdivision.xml
    // Therefore we can simplify as
    // unicode_subdivision_id = alpha{2} alphanum{1,4}
    //
    // and only need to accept/reject the code based on the alpha{2} and the length.
    if (U_SUCCESS(status) && len >= 3 && len <= 6 &&
        uprv_isASCIILetter(kw[0]) && uprv_isASCIILetter(kw[1])) {
        // Additional Check
        static icu::RegionValidateMap valid;
        const char region[] = {kw[0], kw[1], '\0'};
        if (valid.isSet(region)) {
            result.append(uprv_toupper(kw[0]), status);
            result.append(uprv_toupper(kw[1]), status);
        }
    }
    return result;
}
}  // namespace

U_EXPORT icu::CharString
ulocimp_getRegionForSupplementalData(const char *localeID, bool inferRegion,
                                     UErrorCode& status) {
    if (U_FAILURE(status)) {
        return {};
    }
    icu::CharString rgBuf = GetRegionFromKey(localeID, "rg", status);
    if (U_SUCCESS(status) && rgBuf.isEmpty()) {
        // No valid rg keyword value, try for unicode_region_subtag
        rgBuf = ulocimp_getRegion(localeID, status);
        if (U_SUCCESS(status) && rgBuf.isEmpty() && inferRegion) {
            // Second check for sd keyword value
            rgBuf = GetRegionFromKey(localeID, "sd", status);
            if (U_SUCCESS(status) && rgBuf.isEmpty()) {
                // no unicode_region_subtag but inferRegion true, try likely subtags
                UErrorCode rgStatus = U_ZERO_ERROR;
                icu::CharString locBuf = ulocimp_addLikelySubtags(localeID, rgStatus);
                if (U_SUCCESS(rgStatus)) {
                    rgBuf = ulocimp_getRegion(locBuf.data(), status);
                }
            }
        }
    }

    return rgBuf;
}

#if APPLE_ICU_CHANGES
//rdar://106566783 (Migrated locale from es_US to es_ES@rg=uszzzz changed formatters systemwide unexpectedly)
U_CAPI int32_t U_EXPORT2
ulocimp_setRegionToSupplementalRegion(const char *localeID, char *newLocaleID, int32_t newLocaleIDCapacity, UErrorCode* status) {
    UErrorCode err = U_ZERO_ERROR;
    icu::Locale locale(localeID);
    std::string supplementalRegion = locale.getKeywordValue<std::string>("rg", err);
    if (U_SUCCESS(err) && supplementalRegion.length() >= 2) {
        icu::LocaleBuilder builder;
        
        builder.setLocale(locale);
        
        UErrorCode builderErr = U_ZERO_ERROR;
        builder.copyErrorTo(builderErr);
        if (U_FAILURE(builderErr)) {
            // NOTE: This extra logic is because LocaleBuilder validates its input and we still have
            // a few locale IDs in the ICU unit tests that are not well-formed, so for those we work
            // around the problem by just setting the language and region and skipping everything else
            // (I think we have a Radar or an OSICU ticket to fix this)
            builder.clear();
            builder.setLanguage(locale.getLanguage());
            builder.setScript(locale.getScript());
        }
        
        // strip off the subdivision code (which will generally be "zzzz" anyway) and convert to upper case
        supplementalRegion.resize(2);
        supplementalRegion[0] = uprv_toupper(supplementalRegion[0]);
        supplementalRegion[1] = uprv_toupper(supplementalRegion[1]);
        builder.setRegion(supplementalRegion);
        builder.setUnicodeLocaleKeyword("rg", "");

        err = U_ZERO_ERROR;
        icu::Locale newLocale = builder.build(err);
        if (U_SUCCESS(err)) {
            const char* newID = newLocale.getName();
            int32_t newIDLength = uprv_strlen(newID);
            uprv_strncpy(newLocaleID, newID, newLocaleIDCapacity);
            return u_terminateChars(newLocaleID, newLocaleIDCapacity, newIDLength, &err);
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}
#endif // APPLE_ICU_CHANGES

namespace {

// The following data is generated by unit test code inside
// test/intltest/regiontst.cpp from the resource data while
// the test failed.
const uint32_t gValidRegionMap[] = {
    0xeedf597c, 0xdeddbdef, 0x15943f3f, 0x0e00d580, 
    0xb0095c00, 0x0015fb9f, 0x781c068d, 0x0340400f, 
    0xf42b1d00, 0xfd4f8141, 0x25d7fffc, 0x0100084b, 
    0x538f3c40, 0x40000001, 0xfdf15100, 0x9fbb7ae7, 
    0x0410419a, 0x00408557, 0x00004002, 0x00100001, 
    0x00400408, 0x00000001, 
};

}  // namespace
   //
U_NAMESPACE_BEGIN
RegionValidateMap::RegionValidateMap() {
    uprv_memcpy(map, gValidRegionMap, sizeof(map));
}

RegionValidateMap::~RegionValidateMap() {
}

bool RegionValidateMap::isSet(const char* region) const {
    int32_t index = value(region);
    if (index < 0) {
        return false;
    }
    return 0 != (map[index / 32] & (1L << (index % 32)));
}

bool RegionValidateMap::equals(const RegionValidateMap& that) const {
    return uprv_memcmp(map, that.map, sizeof(map)) == 0;
}

// The code transform two letter a-z to a integer valued between -1, 26x26.
// -1 indicate the region is outside the range of two letter a-z
// the rest of value is between 0 and 676 (= 26x26) and used as an index
// the the bigmap in map. The map is an array of 22 int32_t.
// since 32x21 < 676/32 < 32x22 we store this 676 bits bitmap into 22 int32_t.
int32_t RegionValidateMap::value(const char* region) const {
    if (uprv_isASCIILetter(region[0]) && uprv_isASCIILetter(region[1]) &&
        region[2] == '\0') {
        return (uprv_toupper(region[0])-'A') * 26 +
               (uprv_toupper(region[1])-'A');
    }
    return -1;
}

U_NAMESPACE_END
