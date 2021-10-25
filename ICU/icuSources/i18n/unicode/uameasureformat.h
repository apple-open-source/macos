/*
*****************************************************************************************
* Copyright (C) 2014-2017 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#ifndef UAMEASUREFORMAT_H
#define UAMEASUREFORMAT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef U_HIDE_DRAFT_API

#include "unicode/localpointer.h"
#include "unicode/unum.h"
#include "unicode/umisc.h"
#include "unicode/uameasureunit.h"
#include "unicode/ufieldpositer.h"

/**
 * \file
 * \brief C API: Format combinations of measurement units and numeric values.
 *
 * This is a somewhat temporary Apple-specific wrapper for using C++ MeasureFormat
 * to format Measure objects, until the official ICU C API is available.
 */

/**
 * Opaque UAMeasureFormat object for use in C programs.
 * @draft ICU 53
 */
struct UAMeasureFormat;
typedef struct UAMeasureFormat UAMeasureFormat;  /**< C typedef for struct UAMeasureFormat. @draft ICU 53 */

/**
 * Constants for various widths.
 * @draft ICU 53
 */
typedef enum UAMeasureFormatWidth {
    /**
     * Full unit names, e.g. "5 hours, 37 minutes"
     * @draft ICU 53 
     */
    UAMEASFMT_WIDTH_WIDE,
 
    /**
     * Abbreviated unit names, e.g. "5 hr, 37 min"
     * @draft ICU 53
     */
    UAMEASFMT_WIDTH_SHORT,

    /**
     * Use unit symbols if possible, e.g. "5h 37m"
     * @draft ICU 53
     */
    UAMEASFMT_WIDTH_NARROW,

    /**
     * Completely omit unit designatins if possible, e.g. "5:37"
     * @draft ICU 53
     */
    UAMEASFMT_WIDTH_NUMERIC,

    /**
     * Shorter, between SHORT and NARROW, e.g. "5hr 37min"
     * @draft ICU 57
     */
    UAMEASFMT_WIDTH_SHORTER,

    /**
     * Count of values in this enum.
     * @draft ICU 53
     */
    UAMEASFMT_WIDTH_COUNT
} UAMeasureFormatWidth;

enum {
    // Mask bit set in UFieldPosition, in addition to a UAMeasureUnit value,
    // to indicate the numeric portion of the field corresponding to the UAMeasureUnit.
    UAMEASFMT_NUMERIC_FIELD_FLAG = (1 << 30)
};

/**
 * Structure that combines value and UAMeasureUnit,
 * for use with uameasfmt_formatMultiple to specify a
 * list of value/unit combinations to format.
 * @draft ICU 54
 */
typedef struct UAMeasure {
    double          value;
    UAMeasureUnit   unit;
} UAMeasure;


/**
 * Open a new UAMeasureFormat object for a given locale using the specified width,
 * along with a number formatter (if desired) to override the default formatter
 * that would be used for the numeric part of the unit in uameasfmt_format, or the
 * numeric part of the *last unit* (only) in uameasfmt_formatMultiple. The default
 * formatter typically rounds toward 0 and has a minimum of 0 fraction digits and a
 * maximum of 3 fraction digits (i.e. it will show as many decimal places as
 * necessary up to 3, without showing trailing 0s). An alternate number formatter
 * can be used to produce (e.g.) "37.0 mins" instead of "37 mins", or
 * "5 hours, 37.2 minutes" instead of "5 hours, 37.217 minutes".
 *
 * @param locale
 *          The locale
 * @param style
 *          The width - wide, short, narrow, etc.
 * @param nfToAdopt
 *          A number formatter to set for this UAMeasureFormat object (instead of
 *          the default decimal formatter). Ownership of this UNumberFormat object
 *          will pass to the UAMeasureFormat object (the UAMeasureFormat adopts the
 *          UNumberFormat), which becomes responsible for closing it. If the caller
 *          wishes to retain ownership of the UNumberFormat object, the caller must
 *          clone it (with unum_clone) and pass the clone to
 *          uatmufmt_openWithNumberFormat. May be NULL to use the default decimal
 *          formatter.
 * @param status
 *          A pointer to a UErrorCode to receive any errors.
 * @return
 *          A pointer to a UAMeasureFormat object for the specified locale,
 *          or NULL if an error occurred.
 * @draft ICU 54
 */
U_DRAFT UAMeasureFormat* U_EXPORT2
uameasfmt_open( const char*          locale,
                UAMeasureFormatWidth width,
                UNumberFormat*       nfToAdopt,
                UErrorCode*          status );

/**
 * Close a UAMeasureFormat object. Once closed it may no longer be used.
 * @param measfmt
 *            The UATimeUnitFormat object to close.
 * @draft ICU 54
 */
U_DRAFT void U_EXPORT2
uameasfmt_close(UAMeasureFormat *measfmt);

#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * \class LocalUAMeasureFormatPointer
 * "Smart pointer" class, closes a UAMeasureFormat via uameasfmt_close().
 * For most methods see the LocalPointerBase base class.
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @draft ICU 54
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUAMeasureFormatPointer, UAMeasureFormat, uameasfmt_close);

U_NAMESPACE_END

#endif // U_SHOW_CPLUSPLUS_API


/**
 * Format a value like 1.0 and a field like UAMEASUNIT_DURATION_MINUTE to e.g. "1.0 minutes".
 *
 * @param measfmt
 *          The UAMeasureFormat object specifying the format conventions.
 * @param value
 *          The numeric value to format
 * @param unit
 *          The unit to format with the specified numeric value
 * @param result
 *          A pointer to a buffer to receive the formatted result.
 * @param resultCapacity
 *          The maximum size of result.
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the contents of result are undefined.
 * @return
 *          The length of the formatted result; may be greater than resultCapacity,
 *          in which case an error is returned.
 * @draft ICU 54
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_format( const UAMeasureFormat* measfmt,
                  double            value,
                  UAMeasureUnit     unit,
                  UChar*            result,
                  int32_t           resultCapacity,
                  UErrorCode*       status );

/**
 * Format a value like 1.0 and a field like UAMEASUNIT_DURATION_MINUTE to e.g. "1.0 minutes",
 * and get the position in the formatted result for certain types for fields.
 *
 * @param measfmt
 *          The UAMeasureFormat object specifying the format conventions.
 * @param value
 *          The numeric value to format
 * @param unit
 *          The unit to format with the specified numeric value
 * @param result
 *          A pointer to a buffer to receive the formatted result.
 * @param resultCapacity
 *          The maximum size of result.
 * @param pos
 *          A pointer to a UFieldPosition. On input, pos->field is read; this should
 *          be a value from the UNumberFormatFields enum in unum.h. On output,
 *          pos->beginIndex and pos->endIndex indicate the beginning and ending offsets
 *          of that field in the formatted output, if relevant. This parameter may be
 *          NULL if no position information is desired.
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the contents of result are undefined.
 * @return
 *          The length of the formatted result; may be greater than resultCapacity,
 *          in which case an error is returned.
 * @draft ICU 54
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_formatGetPosition( const UAMeasureFormat* measfmt,
                            double            value,
                            UAMeasureUnit     unit,
                            UChar*            result,
                            int32_t           resultCapacity,
                            UFieldPosition*   pos,
                            UErrorCode*       status );

/**
 * Format a list of value and unit combinations, using locale-appropriate
 * conventions for the list. Each combination is represented by a UAMeasure
 * that combines a value and unit, such as 5.3 + UAMEASUNIT_DURATION_HOUR or
 * 37.2 + UAMEASUNIT_DURATION_MINUTE. For all except the last UAMeasure in the
 * list, the numeric part will be formatted using the default formatter (zero
 * decimal places, rounds toward 0); for the last UAMeasure, the default may
 * be overriden by passing a number formatter in uameasfmt_open. The result
 * can thus be something like "5 hours, 37.2 minutes" or "5 hrs, 37.2 mins".
 *
 * @param measfmt
 *            The UAMeasureFormat object specifying the format conventions.
 * @param measures
 *            A list of UAMeasure structs each specifying a numeric value
 *            and a UAMeasureUnit.
 * @param measureCount
 *            The count of UAMeasureUnits in measures. Currently this has a limit of 8.
 * @param result
 *            A pointer to a buffer to receive the formatted result.
 * @param resultCapacity
 *            The maximum size of result.
 * @param status
 *            A pointer to a UErrorCode to receive any errors. In case of error status,
 *            the contents of result are undefined.
 * @return
 *            The length of the formatted result; may be greater than resultCapacity,
 *            in which case an error is returned.
 * @draft ICU 54
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_formatMultiple( const UAMeasureFormat* measfmt,
                          const UAMeasure*  measures,
                          int32_t           measureCount,
                          UChar*            result,
                          int32_t           resultCapacity,
                          UErrorCode*       status );

/**
 * Format a list of value and unit combinations, using locale-appropriate
 * conventions for the list. This has the same format behavior as
 * uameasfmt_formatMultiple but adds the fpositer parameter. 
 *
 * @param measfmt
 *            The UAMeasureFormat object specifying the format conventions.
 * @param measures
 *            A list of UAMeasure structs each specifying a numeric value
 *            and a UAMeasureUnit.
 * @param measureCount
 *            The count of UAMeasureUnits in measures. Currently this has a limit of 8.
 * @param result
 *            A pointer to a buffer to receive the formatted result.
 * @param resultCapacity
 *            The maximum size of result.
 * @param fpositer
 *            A pointer to a UFieldPositionIterator created by ufieldpositer_open
 *            (may be NULL if field position information is not needed). Any
 *            iteration information already present in the UFieldPositionIterator
 *            will be deleted, and the iterator will be reset to apply to the
 *            fields in the formatted string created by this function call. In the
 *            the formatted result, each unit field (unit name or symbol plus any
 *            associated numeric value) will correspond to one or two results from
 *            ufieldpositer_next. The first result returns a UAMeasureUnit value and
 *            indicates the begin and end index for the complete field. If there is
 *            a numeric value contained in the field, then a subsequent call to
 *            ufieldpositer_next returns a value with UAMEASFMT_NUMERIC_FIELD_FLAG
 *            set and the same UAMeasureUnit value in the low-order bits, and
 *            indicates the begin and end index for the numeric portion of the field.
 *            For example with the string "3 hours, dualminute" the sequence of
 *            calls to ufieldpositer_next would result in:
 *            (1) return UAMEASUNIT_DURATION_HOUR, begin index 0, end index 7
 *            (2) return UAMEASUNIT_DURATION_HOUR | UAMEASFMT_NUMERIC_FIELD_FLAG, begin index 0, end index 1
 *            (3) return UAMEASUNIT_DURATION_MINUTE, begin index 9, end index 19
 *            (4) return -1 to indicate end of interation
 * @param status
 *            A pointer to a UErrorCode to receive any errors. In case of error status,
 *            the contents of result are undefined.
 * @return
 *            The length of the formatted result; may be greater than resultCapacity,
 *            in which case an error is returned.
 * @draft ICU 58
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_formatMultipleForFields( const UAMeasureFormat* measfmt,
                                   const UAMeasure*  measures,
                                   int32_t           measureCount,
                                   UChar*            result,
                                   int32_t           resultCapacity,
                                   UFieldPositionIterator* fpositer,
                                   UErrorCode*       status );

/**
 * Get the display name for a unit, such as "minutes" or "kilometers".
 *
 * @param measfmt
 *          The UAMeasureFormat object specifying the format conventions.
 * @param unit
 *          The unit whose localized name to get
 * @param result
 *          A pointer to a buffer to receive the name.
 * @param resultCapacity
 *          The maximum size of result.
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the contents of result are undefined.
 * @return
 *          The length of the name; may be greater than resultCapacity,
 *          in which case an error is returned.
 * @draft ICU 57
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_getUnitName( const UAMeasureFormat* measfmt,
                       UAMeasureUnit unit,
                       UChar* result,
                       int32_t resultCapacity,
                       UErrorCode* status );

/**
 * Constants for unit display name list styles
 * @draft ICU 57
 */
typedef enum UAMeasureNameListStyle {
    /**
     * Use standard (linguistic) list style, the same for all unit widths; e.g.
     *   wide:    "hours, minutes, and seconds"
     *   short:   "hours, min, and secs"
     *   narrow:  "hour, min, and sec"
     * @draft ICU 57 
     */
    UAMEASNAME_LIST_STANDARD,
 
    /**
      * Use the same list style as used by the formatted units, depends on width; e.g.
     *   wide:    "hours, minutes, seconds"
     *   short:   "hours, min, secs"
     *   narrow:  "hour min sec"
     * @draft ICU 57
     */
    UAMEASNAME_LIST_MATCHUNITS,
} UAMeasureNameListStyle;

/**
 * Get a list of display names for multiple units
 *
 * @param measfmt
 *          The UAMeasureFormat object specifying the format conventions.
 * @param units
 *          The array of unit types whose names to get.
 * @param unitCount
 *          The number of unit types in the units array.
 * @param listStyle
 *          The list style used for combining the unit names.
 * @param result
 *          A pointer to a buffer to receive the list of names.
 * @param resultCapacity
 *          The maximum size of result.
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the contents of result are undefined.
 * @return
 *          The length of the list of names; may be greater than resultCapacity,
 *          in which case an error is returned.
 * @draft ICU 57
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_getMultipleUnitNames( const UAMeasureFormat* measfmt,
                                const UAMeasureUnit* units,
                                int32_t unitCount,
                                UAMeasureNameListStyle listStyle,
                                UChar* result,
                                int32_t resultCapacity,
                                UErrorCode* status );

/**
 * Get the units used for a particular usage. This low-level function depends
 * one some knowledge of the relevant CLDR keys. After more experience with
 * usage, enums for relevant usage values may be created.
 *
 * This is sensitive to two locale keywords.
 * If the "ms" keyword is present, then the measurement system specified by its
 * value is used (except for certain categories like duration and concentr).
 * Else if the "rg" keyword is present, then the region specified by its value
 * determines the unit usage.
 * Else if the locale has a region subtag, it determines the unit usage.
 * Otherwise the likely region for the language determines the usage.
 *
 * @param locale
 *          The locale, which determines the usage as specified above.
 * @param category
 *          A string representing the CLDR category key for the desired usage,
 *          such as "length" or "mass". Must not be NULL.
 * @param usage
 *          A string representing the CLDR usage subkey for the desired usage,
 *          such as "person", "person-small" (for infants), "person-informal"
 *          (for conversational/informal usage), etc. To get the general unit
 *          for the category (not for a specific usage), this may be NULL, or
 *          may be just "large" or "small" to indicate a variant of the general
 *          unit for larger or smaller ranges than normal.
 * @param units
 *          Array to be filled in with UAMeasureUnit values; the size is
 *          specified by unitsCapacity (which in general should be at least 3).
 *          The number of array elements actually filled in is indicated by
 *          the return value; if no error status is set then this will be
 *          non-zero.
 *
 *          If the return value is positive then units represents an ordered
 *          list of one or more units that should be used in combination for
 *          the desired usage (e.g. the values UAMEASUNIT_LENGTH_FOOT,
 *          UAMEASUNIT_LENGTH_INCH to indicate a height expressed as a
 *          combination of feet and inches, or just UAMEASUNIT_LENGTH_CENTIMETER
 *          to indicate height expressed in centimeters alone).
 *
 *          Negative return values may be used for future uses (such as
 *          indicating an X-per-Y relationship among the returned units).
 *
 *          The units parameter may be NULL if unitsCapacity is 0, for
 *          pre-flighting (i.e. to determine the size of the units array that
 *          woud be required for the given category and usage).
 * @param unitsCapacity
 *          The maximum capacity of the passed-in units array.
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the contents of result are undefined.
 * @return
 *          Positive values indicate the number of units require for the usage;
 *          may be greater than resultCapacity, in which case an error is returned.
 *          If no error, than this number of units are actually provided in the
 *          units array. Negative return values are reserved for future uses.
 * @draft ICU 57
 */
U_DRAFT int32_t U_EXPORT2
uameasfmt_getUnitsForUsage( const char*     locale,
                            const char*     category,
                            const char*     usage,
                            UAMeasureUnit*  units,
                            int32_t         unitsCapacity,
                            UErrorCode*     status );

/**
 * Get the (non-localized) category name for a unit. For example, for
 * UAMEASUNIT_VOLUME_LITER, returns "volume".
 *
 * @param unit
 *          The unit whose category name to get
 * @param status
 *          A pointer to a UErrorCode to receive any errors. In case of error status,
 *          the return value is undefined.
 * @return
 *          Pointer to a zero-terminated string giving the
 *          (non-localized) category name.
 * @draft ICU 58
 */
U_DRAFT const char * U_EXPORT2
uameasfmt_getUnitCategory(UAMeasureUnit unit,
                          UErrorCode* status );


#endif /* U_HIDE_DRAFT_API */
#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #ifndef UAMEASUREFORMAT_H */
