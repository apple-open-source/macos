// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "numberformattesttuple.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/testlog.h"
#include "ustrfmt.h"
#include "charstr.h"
#include "cstring.h"
#include "cmemory.h"

#if !APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
static NumberFormatTestTuple emptyObject;

static NumberFormatTestTuple *gNullPtr = &emptyObject;

#define FIELD_OFFSET(fieldName) ((int32_t) (((char *) &gNullPtr->fieldName) - ((char *) gNullPtr)))
#define FIELD_FLAG_OFFSET(fieldName) ((int32_t) (((char *) &gNullPtr->fieldName##Flag) - ((char *) gNullPtr)))

#define FIELD_INIT(fieldName, fieldType) {#fieldName, FIELD_OFFSET(fieldName), FIELD_FLAG_OFFSET(fieldName), fieldType}

struct Numberformattesttuple_EnumConversion {
    const char *str;
    int32_t value;
};
#endif // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
static NumberFormatTestTuple::Numberformattesttuple_EnumConversion gRoundingEnum[] = {
#else
static Numberformattesttuple_EnumConversion gRoundingEnum[] = {
#endif // APPLE_ICU_CHANGES

    {"ceiling", DecimalFormat::kRoundCeiling},
    {"floor", DecimalFormat::kRoundFloor},
    {"down", DecimalFormat::kRoundDown},
    {"up", DecimalFormat::kRoundUp},
    {"halfEven", DecimalFormat::kRoundHalfEven},
    {"halfDown", DecimalFormat::kRoundHalfDown},
    {"halfUp", DecimalFormat::kRoundHalfUp},
    {"unnecessary", DecimalFormat::kRoundUnnecessary},
    {"halfOdd", DecimalFormat::kRoundHalfOdd},
    {"halfCeiling", DecimalFormat::kRoundHalfCeiling},
    {"halfFloor", DecimalFormat::kRoundHalfFloor}};

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
static NumberFormatTestTuple::Numberformattesttuple_EnumConversion gCurrencyUsageEnum[] = {
#else
static Numberformattesttuple_EnumConversion gCurrencyUsageEnum[] = {
#endif // APPLE_ICU_CHANGES
    {"standard", UCURR_USAGE_STANDARD},
    {"cash", UCURR_USAGE_CASH}};

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
static NumberFormatTestTuple::Numberformattesttuple_EnumConversion gPadPositionEnum[] = {
#else
static Numberformattesttuple_EnumConversion gPadPositionEnum[] = {
#endif // APPLE_ICU_CHANGES
    {"beforePrefix", DecimalFormat::kPadBeforePrefix},
    {"afterPrefix", DecimalFormat::kPadAfterPrefix},
    {"beforeSuffix", DecimalFormat::kPadBeforeSuffix},
    {"afterSuffix", DecimalFormat::kPadAfterSuffix}};

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
static NumberFormatTestTuple::Numberformattesttuple_EnumConversion gFormatStyleEnum[] = {
#else
static Numberformattesttuple_EnumConversion gFormatStyleEnum[] = {
#endif // APPLE_ICU_CHANGES
    {"patternDecimal", UNUM_PATTERN_DECIMAL},
    {"decimal", UNUM_DECIMAL},
    {"currency", UNUM_CURRENCY},
    {"percent", UNUM_PERCENT},
    {"scientific", UNUM_SCIENTIFIC},
    {"spellout", UNUM_SPELLOUT},
    {"ordinal", UNUM_ORDINAL},
    {"duration", UNUM_DURATION},
    {"numberingSystem", UNUM_NUMBERING_SYSTEM},
    {"patternRuleBased", UNUM_PATTERN_RULEBASED},
    {"currencyIso", UNUM_CURRENCY_ISO},
    {"currencyPlural", UNUM_CURRENCY_PLURAL},
    {"currencyAccounting", UNUM_CURRENCY_ACCOUNTING},
    {"cashCurrency", UNUM_CASH_CURRENCY},
    {"default", UNUM_DEFAULT},
    {"ignore", UNUM_IGNORE}};

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
int32_t NumberFormatTestTuple::toEnum(
#else
static int32_t toEnum(
#endif // APPLE_ICU_CHANGES
        const Numberformattesttuple_EnumConversion *table,
        int32_t tableLength,
        const UnicodeString &str,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    CharString cstr;
    cstr.appendInvariantChars(str, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    for (int32_t i = 0; i < tableLength; ++i) {
        if (uprv_strcmp(cstr.data(), table[i].str) == 0) {
            return table[i].value;
        }
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0;
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::fromEnum(
#else
static void fromEnum(
#endif // APPLE_ICU_CHANGES
        const Numberformattesttuple_EnumConversion *table,
        int32_t tableLength,
        int32_t val,
        UnicodeString &appendTo) {
    for (int32_t i = 0; i < tableLength; ++i) {
        if (table[i].value == val) {
            appendTo.append(table[i].str);
        }
    }
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::identVal(
#else
static void identVal(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *strPtr, UErrorCode & /*status*/) {
    *static_cast<UnicodeString *>(strPtr) = str;
}
 
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::identStr(
#else
static void identStr(
#endif // APPLE_ICU_CHANGES
        const void *strPtr, UnicodeString &appendTo) {
    appendTo.append(*static_cast<const UnicodeString *>(strPtr));
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToLocale(
#else
static void strToLocale(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *localePtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    CharString localeStr;
    localeStr.appendInvariantChars(str, status);
    *static_cast<Locale *>(localePtr) = Locale(localeStr.data());
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::localeToStr(
#else
static void localeToStr(
#endif // APPLE_ICU_CHANGES
        const void *localePtr, UnicodeString &appendTo) {
    appendTo.append(
            UnicodeString(
                    static_cast<const Locale *>(localePtr)->getName()));
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToInt(
#else
static void strToInt(
#endif // APPLE_ICU_CHANGES
       const UnicodeString &str, void *intPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t len = str.length();
    int32_t start = 0;
    UBool neg = false;
    if (len > 0 && str[0] == 0x2D) { // negative
        neg = true;
        start = 1;
    }
    if (start == len) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int64_t value = 0;
    for (int32_t i = start; i < len; ++i) {
        char16_t ch = str[i];
        if (ch < 0x30 || ch > 0x39) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        value = value * 10 - 0x30 + static_cast<int32_t>(ch);
    }
    int32_t signedValue = neg ? static_cast<int32_t>(-value) : static_cast<int32_t>(value);
    *static_cast<int32_t *>(intPtr) = signedValue;
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::intToStr(
#else
static void intToStr(
#endif // APPLE_ICU_CHANGES
        const void *intPtr, UnicodeString &appendTo) {
    char16_t buffer[20];
    // int64_t such that all int32_t values can be negated
    int64_t xSigned = *static_cast<const int32_t *>(intPtr);
    uint32_t x;
    if (xSigned < 0) {
        appendTo.append(static_cast<char16_t>(0x2D));
        x = static_cast<uint32_t>(-xSigned);
    } else {
        x = static_cast<uint32_t>(xSigned);
    }
    int32_t len = uprv_itou(buffer, UPRV_LENGTHOF(buffer), x, 10, 1);
    appendTo.append(buffer, 0, len);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToDouble(
#else
static void strToDouble(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *doublePtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    CharString buffer;
    buffer.appendInvariantChars(str, status);
    if (U_FAILURE(status)) {
        return;
    }
    *static_cast<double *>(doublePtr) = atof(buffer.data());
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::doubleToStr(
#else
static void doubleToStr(
#endif // APPLE_ICU_CHANGES
        const void *doublePtr, UnicodeString &appendTo) {
    char buffer[256];
    double x = *static_cast<const double *>(doublePtr);
    snprintf(buffer, sizeof(buffer), "%f", x);
    appendTo.append(buffer);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToERounding(
#else
static void strToERounding(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *roundPtr, UErrorCode &status) {
    int32_t val = toEnum(
            gRoundingEnum, UPRV_LENGTHOF(gRoundingEnum), str, status);
    *static_cast<DecimalFormat::ERoundingMode*>(roundPtr) = static_cast<DecimalFormat::ERoundingMode>(val);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::eRoundingToStr(
#else
static void eRoundingToStr(
#endif // APPLE_ICU_CHANGES
        const void *roundPtr, UnicodeString &appendTo) {
    DecimalFormat::ERoundingMode rounding = 
            *static_cast<const DecimalFormat::ERoundingMode *>(roundPtr);
    fromEnum(
            gRoundingEnum,
            UPRV_LENGTHOF(gRoundingEnum),
            rounding,
            appendTo);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToCurrencyUsage(
#else
static void strToCurrencyUsage(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *currencyUsagePtr, UErrorCode &status) {
    int32_t val = toEnum(
            gCurrencyUsageEnum, UPRV_LENGTHOF(gCurrencyUsageEnum), str, status);
    *static_cast<UCurrencyUsage*>(currencyUsagePtr) = static_cast<UCurrencyUsage>(val);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::currencyUsageToStr(
#else
static void currencyUsageToStr(
#endif // APPLE_ICU_CHANGES
        const void *currencyUsagePtr, UnicodeString &appendTo) {
    UCurrencyUsage currencyUsage = 
            *static_cast<const UCurrencyUsage *>(currencyUsagePtr);
    fromEnum(
            gCurrencyUsageEnum,
            UPRV_LENGTHOF(gCurrencyUsageEnum),
            currencyUsage,
            appendTo);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToEPadPosition(
#else
static void strToEPadPosition(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *padPositionPtr, UErrorCode &status) {
    int32_t val = toEnum(
            gPadPositionEnum, UPRV_LENGTHOF(gPadPositionEnum), str, status);
    *static_cast<DecimalFormat::EPadPosition *>(padPositionPtr) =
            static_cast<DecimalFormat::EPadPosition>(val);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::ePadPositionToStr(
#else
static void ePadPositionToStr(
#endif // APPLE_ICU_CHANGES
        const void *padPositionPtr, UnicodeString &appendTo) {
    DecimalFormat::EPadPosition padPosition = 
            *static_cast<const DecimalFormat::EPadPosition *>(padPositionPtr);
    fromEnum(
            gPadPositionEnum,
            UPRV_LENGTHOF(gPadPositionEnum),
            padPosition,
            appendTo);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::strToFormatStyle(
#else
static void strToFormatStyle(
#endif // APPLE_ICU_CHANGES
        const UnicodeString &str, void *formatStylePtr, UErrorCode &status) {
    int32_t val = toEnum(
            gFormatStyleEnum, UPRV_LENGTHOF(gFormatStyleEnum), str, status);
    *static_cast<UNumberFormatStyle*>(formatStylePtr) = static_cast<UNumberFormatStyle>(val);
}

#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
void NumberFormatTestTuple::formatStyleToStr(
#else
static void formatStyleToStr(
#endif // APPLE_ICU_CHANGES
        const void *formatStylePtr, UnicodeString &appendTo) {
    UNumberFormatStyle formatStyle = 
            *static_cast<const UNumberFormatStyle *>(formatStylePtr);
    fromEnum(
            gFormatStyleEnum,
            UPRV_LENGTHOF(gFormatStyleEnum),
            formatStyle,
            appendTo);
}

#if !APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
struct NumberFormatTestTupleFieldOps {
    void (*toValue)(const UnicodeString &str, void *valPtr, UErrorCode &);
    void (*toString)(const void *valPtr, UnicodeString &appendTo);
};

const NumberFormatTestTupleFieldOps gStrOps = {identVal, identStr};
const NumberFormatTestTupleFieldOps gIntOps = {strToInt, intToStr};
const NumberFormatTestTupleFieldOps gLocaleOps = {strToLocale, localeToStr};
const NumberFormatTestTupleFieldOps gDoubleOps = {strToDouble, doubleToStr};
const NumberFormatTestTupleFieldOps gERoundingOps = {strToERounding, eRoundingToStr};
const NumberFormatTestTupleFieldOps gCurrencyUsageOps = {strToCurrencyUsage, currencyUsageToStr};
const NumberFormatTestTupleFieldOps gEPadPositionOps = {strToEPadPosition, ePadPositionToStr};
const NumberFormatTestTupleFieldOps gFormatStyleOps = {strToFormatStyle, formatStyleToStr};

struct NumberFormatTestTupleFieldData {
    const char *name;
    int32_t offset;
    int32_t flagOffset;
    const NumberFormatTestTupleFieldOps *ops;
};

// Order must correspond to ENumberFormatTestTupleField
const NumberFormatTestTupleFieldData gFieldData[] = {
    FIELD_INIT(locale, &gLocaleOps),
    FIELD_INIT(currency, &gStrOps),
    FIELD_INIT(pattern, &gStrOps),
    FIELD_INIT(format, &gStrOps),
    FIELD_INIT(output, &gStrOps),
    FIELD_INIT(comment, &gStrOps),
    FIELD_INIT(minIntegerDigits, &gIntOps),
    FIELD_INIT(maxIntegerDigits, &gIntOps),
    FIELD_INIT(minFractionDigits, &gIntOps),
    FIELD_INIT(maxFractionDigits, &gIntOps),
    FIELD_INIT(minGroupingDigits, &gIntOps),
    FIELD_INIT(breaks, &gStrOps),
    FIELD_INIT(useSigDigits, &gIntOps),
    FIELD_INIT(minSigDigits, &gIntOps),
    FIELD_INIT(maxSigDigits, &gIntOps),
    FIELD_INIT(useGrouping, &gIntOps),
    FIELD_INIT(multiplier, &gIntOps),
    FIELD_INIT(roundingIncrement, &gDoubleOps),
    FIELD_INIT(formatWidth, &gIntOps),
    FIELD_INIT(padCharacter, &gStrOps),
    FIELD_INIT(useScientific, &gIntOps),
    FIELD_INIT(grouping, &gIntOps),
    FIELD_INIT(grouping2, &gIntOps),
    FIELD_INIT(roundingMode, &gERoundingOps),
    FIELD_INIT(currencyUsage, &gCurrencyUsageOps),
    FIELD_INIT(minimumExponentDigits, &gIntOps),
    FIELD_INIT(exponentSignAlwaysShown, &gIntOps),
    FIELD_INIT(decimalSeparatorAlwaysShown, &gIntOps),
    FIELD_INIT(padPosition, &gEPadPositionOps),
    FIELD_INIT(positivePrefix, &gStrOps),
    FIELD_INIT(positiveSuffix, &gStrOps),
    FIELD_INIT(negativePrefix, &gStrOps),
    FIELD_INIT(negativeSuffix, &gStrOps),
    FIELD_INIT(signAlwaysShown, &gIntOps),
    FIELD_INIT(localizedPattern, &gStrOps),
    FIELD_INIT(toPattern, &gStrOps),
    FIELD_INIT(toLocalizedPattern, &gStrOps),
    FIELD_INIT(style, &gFormatStyleOps),
    FIELD_INIT(parse, &gStrOps),
    FIELD_INIT(lenient, &gIntOps),
    FIELD_INIT(plural, &gStrOps),
    FIELD_INIT(parseIntegerOnly, &gIntOps),
    FIELD_INIT(decimalPatternMatchRequired, &gIntOps),
    FIELD_INIT(parseNoExponent, &gIntOps),
    FIELD_INIT(parseCaseSensitive, &gIntOps),
    FIELD_INIT(outputCurrency, &gStrOps)
};
#endif // APPLE_ICU_CHANGES

UBool
NumberFormatTestTuple::setField(
        ENumberFormatTestTupleField fieldId, 
        const UnicodeString &fieldValue,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    if (fieldId == kNumberFormatTestTupleFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }
    gFieldData[fieldId].ops->toValue(
            fieldValue, getMutableFieldAddress(fieldId), status);
    if (U_FAILURE(status)) {
        return false;
    }
    setFlag(fieldId, true);
    return true;
}

UBool
NumberFormatTestTuple::clearField(
        ENumberFormatTestTupleField fieldId, 
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    if (fieldId == kNumberFormatTestTupleFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }
    setFlag(fieldId, false);
    return true;
}

void
NumberFormatTestTuple::clear() {
    for (int32_t i = 0; i < kNumberFormatTestTupleFieldCount; ++i) {
        setFlag(i, false);
    }
}

UnicodeString &
NumberFormatTestTuple::toString(
        UnicodeString &appendTo) const {
    appendTo.append("{");
    UBool first = true;
    for (int32_t i = 0; i < kNumberFormatTestTupleFieldCount; ++i) {
        if (!isFlag(i)) {
            continue;
        }
        if (!first) {
            appendTo.append(", ");
        }
        first = false;
        appendTo.append(gFieldData[i].name);
        appendTo.append(": ");
        gFieldData[i].ops->toString(getFieldAddress(i), appendTo);
    }
    appendTo.append("}");
    return appendTo;
}

ENumberFormatTestTupleField
NumberFormatTestTuple::getFieldByName(
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
                                      const UnicodeString &name) const {
#else
                                      const UnicodeString &name) {
#endif // APPLE_ICU_CHANGES
    CharString buffer;
    UErrorCode status = U_ZERO_ERROR;
    buffer.appendInvariantChars(name, status);
    if (U_FAILURE(status)) {
        return kNumberFormatTestTupleFieldCount;
    }
    int32_t result = -1;
    for (int32_t i = 0; i < UPRV_LENGTHOF(gFieldData); ++i) {
        if (uprv_strcmp(gFieldData[i].name, buffer.data()) == 0) {
            result = i;
            break;
        }
    }
    if (result == -1) {
        return kNumberFormatTestTupleFieldCount;
    }
    return static_cast<ENumberFormatTestTupleField>(result);
}

const void *
NumberFormatTestTuple::getFieldAddress(int32_t fieldId) const {
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
    return gFieldData[fieldId].fieldPtr;
#else
    return reinterpret_cast<const char *>(this) + gFieldData[fieldId].offset;
#endif // APPLE_ICU_CHANGES
}

void *
NumberFormatTestTuple::getMutableFieldAddress(int32_t fieldId) {
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
    return gFieldData[fieldId].fieldPtr;
#else
    return reinterpret_cast<char *>(this) + gFieldData[fieldId].offset;
#endif // APPLE_ICU_CHANGES
}

void 
NumberFormatTestTuple::setFlag(int32_t fieldId, UBool value) {
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
    gFieldData[fieldId].flag = value;
#else
    void *flagAddr = reinterpret_cast<char *>(this) + gFieldData[fieldId].flagOffset;
    *static_cast<UBool *>(flagAddr) = value;
#endif // APPLE_ICU_CHANGES
}

UBool
NumberFormatTestTuple::isFlag(int32_t fieldId) const {
#if APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
    return gFieldData[fieldId].flag;
#else
    const void *flagAddr = reinterpret_cast<const char *>(this) + gFieldData[fieldId].flagOffset;
    return *static_cast<const UBool *>(flagAddr);
#endif // APPLE_ICU_CHANGES
}

#endif /* !UCONFIG_NO_FORMATTING */
