// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CNUMTST.C
*
*     Madhu Katragadda              Creation
*
* Modification History:
*
*   Date        Name        Description
*   06/24/99    helena      Integrated Alan's NF enhancements and Java2 bug fixes
*   07/15/99    helena      Ported to HPUX 10/11 CC.
*********************************************************************************
*/

/* C API TEST FOR NUMBER FORMAT */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if APPLE_ICU_CHANGES
// rdar://
#include "unicode/ubidi.h" // for TestCurrencyBidiOrdering()
#endif  // APPLE_ICU_CHANGES
#include "unicode/uloc.h"
#include "unicode/umisc.h"
#include "unicode/unum.h"
#include "unicode/unumsys.h"
#include "unicode/ustring.h"
#include "unicode/udisplaycontext.h"

#include "cintltst.h"
#include "cnumtst.h"
#include "cmemory.h"
#include "cstring.h"
#include "putilimp.h"
#include "uassert.h"
#if APPLE_ICU_CHANGES
// rdar://112745117 (SEED: NSNumberFormatter not rounding correctly using roundingIncrement & usesSignificantDigits)
#include <math.h>
#endif // APPLE_ICU_CHANGES
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static const char *tagAssert(const char *f, int32_t l, const char *msg) {
    static char _fileline[1000];
    snprintf(_fileline, sizeof(_fileline), "%s:%d: ASSERT_TRUE(%s)", f, l, msg);
    return _fileline;
}

#define ASSERT_TRUE(x)   assertTrue(tagAssert(__FILE__, __LINE__, #x), (x))

void addNumForTest(TestNode** root);
static void TestTextAttributeCrash(void);
static void TestNBSPInPattern(void);
static void TestInt64Parse(void);
static void TestParseCurrency(void);
static void TestMaxInt(void);
static void TestNoExponent(void);
static void TestSignAlwaysShown(void);
static void TestMinimumGroupingDigits(void);
static void TestParseCaseSensitive(void);
static void TestUFormattable(void);
static void TestUNumberingSystem(void);
static void TestCurrencyIsoPluralFormat(void);
static void TestContext(void);
static void TestCurrencyUsage(void);
static void TestCurrFmtNegSameAsPositive(void);
static void TestVariousStylesAndAttributes(void);
static void TestParseCurrPatternWithDecStyle(void);
static void TestFormatForFields(void);
static void TestRBNFRounding(void);
static void Test12052_NullPointer(void);
static void TestParseCases(void);
static void TestSetMaxFracAndRoundIncr(void);
static void TestIgnorePadding(void);
static void TestSciNotationMaxFracCap(void);
static void TestMinIntMinFracZero(void);
static void Test21479_ExactCurrency(void);
static void Test22088_Ethiopic(void);
static void TestChangingRuleset(void);
static void TestParseWithEmptyCurr(void);
static void TestDuration(void);
#if APPLE_ICU_CHANGES
// rdar://
static void TestParseAltNum(void);
static void TestParseCurrPatternWithDecStyle(void);
static void TestFormatPrecision(void);
static void TestSetSigDigAndRoundIncr(void); // rdar://52538227
static void TestSetAffixOnCurrFmt(void); // rdar://46755430
static void TestParseWithEmptyCurr(void); // rdar://46915356
static void TestSciNotationNumbers(void); // rdar://50113359
static void TestSciNotationPrecision(void); // rdar://51601250
static void TestMinimumGrouping(void); // rdar://49808819
static void TestNumberSystemsMultiplier(void); // rdar://49120648
static void TestParseScientific(void); // rdar://39156484
static void TestCurrForUnkRegion(void); // rdar://51985640
static void TestMinIntMinFracZero(void); // rdar://54569257
static void TestFormatDecPerf(void); // rdar://51672521
static void TestCountryFallback(void); // rdar://54886964
static void TestCurrencyBidiOrdering(void); // rdar://76160456
static void TestPrecisionOverMultipleCalls(void); // rdar://76655283
static void TestCurrencySymbol(void);   // rdar://79879611
static void TestRgSubtag(void); // rdar://107276400
static void TestThousandsSeparator(void); // rdar://108506710
static void TestSigDigVsRounding(void); // rdar://112745117
static void TestDefaultArabicNumberingSystem(void); // rdar://116185298
static void TestMinGroupingDigits(void); // TEMPORARY!!!
#endif  // APPLE_ICU_CHANGES

#define TESTCASE(x) addTest(root, &x, "tsformat/cnumtst/" #x)

void addNumForTest(TestNode** root)
{
    TESTCASE(TestNumberFormat);
    TESTCASE(TestSpelloutNumberParse);
    TESTCASE(TestSignificantDigits);
    TESTCASE(TestSigDigRounding);
    TESTCASE(TestNumberFormatPadding);
    TESTCASE(TestInt64Format);
    TESTCASE(TestNonExistentCurrency);
    TESTCASE(TestCurrencyRegression);
    TESTCASE(TestTextAttributeCrash);
    TESTCASE(TestRBNFFormat);
    TESTCASE(TestRBNFRounding);
    TESTCASE(TestNBSPInPattern);
    TESTCASE(TestInt64Parse);
    TESTCASE(TestParseZero);
    TESTCASE(TestParseCurrency);
    TESTCASE(TestCloneWithRBNF);
    TESTCASE(TestMaxInt);
    TESTCASE(TestNoExponent);
    TESTCASE(TestSignAlwaysShown);
    TESTCASE(TestMinimumGroupingDigits);
    TESTCASE(TestParseCaseSensitive);
    TESTCASE(TestUFormattable);
    TESTCASE(TestUNumberingSystem);
    TESTCASE(TestCurrencyIsoPluralFormat);
    TESTCASE(TestContext);
    TESTCASE(TestCurrencyUsage);
    TESTCASE(TestCurrFmtNegSameAsPositive);
    TESTCASE(TestVariousStylesAndAttributes);
    TESTCASE(TestParseCurrPatternWithDecStyle);
    TESTCASE(TestFormatForFields);
    TESTCASE(Test12052_NullPointer);
    TESTCASE(TestParseCases);
    TESTCASE(TestSetMaxFracAndRoundIncr);
    TESTCASE(TestIgnorePadding);
    TESTCASE(TestSciNotationMaxFracCap);
    TESTCASE(TestMinIntMinFracZero);
    TESTCASE(Test21479_ExactCurrency);
    TESTCASE(Test22088_Ethiopic);
    TESTCASE(TestChangingRuleset);
    TESTCASE(TestParseWithEmptyCurr);
    TESTCASE(TestDuration);
#if APPLE_ICU_CHANGES
// rdar://
    TESTCASE(TestParseAltNum);
    TESTCASE(TestParseCurrPatternWithDecStyle);
    TESTCASE(TestFormatPrecision);
    TESTCASE(TestSetSigDigAndRoundIncr); // rdar://52538227
    TESTCASE(TestSetAffixOnCurrFmt); // rdar://46755430
    TESTCASE(TestParseWithEmptyCurr); // rdar://46915356
    TESTCASE(TestSciNotationNumbers); // rdar://50113359
    TESTCASE(TestSciNotationPrecision); // rdar://51601250
    TESTCASE(TestMinimumGrouping); // rdar://49808819
    TESTCASE(TestNumberSystemsMultiplier); // rdar://49120648
    TESTCASE(TestParseScientific); // rdar://39156484
    TESTCASE(TestCurrForUnkRegion); // rdar://51985640
    TESTCASE(TestFormatDecPerf); // rdar://51672521
    TESTCASE(TestCountryFallback); // rdar://51672521
    TESTCASE(TestCurrencyBidiOrdering); // rdar://76160456
    TESTCASE(TestPrecisionOverMultipleCalls); // rdar://76655283
    TESTCASE(TestCurrencySymbol);   // rdar://79879611
    TESTCASE(TestRgSubtag); // rdar://107276400
    TESTCASE(TestThousandsSeparator); // rdar://108506710
    TESTCASE(TestSigDigVsRounding); // rdar://112745117
    TESTCASE(TestDefaultArabicNumberingSystem); // rdar://116185298
    TESTCASE(TestMinGroupingDigits); // TEMPORARY!!!
#endif  // APPLE_ICU_CHANGES
}

/* test Parse int 64 */

static void TestInt64Parse()
{

    UErrorCode st = U_ZERO_ERROR;
    UErrorCode* status = &st;

    const char* st1 = "009223372036854775808";
    const int size = 21;
    UChar text[21];


    UNumberFormat* nf;

    int64_t a;

    u_charsToUChars(st1, text, size);
    nf = unum_open(UNUM_DEFAULT, NULL, -1, NULL, NULL, status);

    if(U_FAILURE(*status))
    {
        log_data_err("Error in unum_open() %s \n", myErrorName(*status));
        return;
    }

    log_verbose("About to test unum_parseInt64() with out of range number\n");

    a = unum_parseInt64(nf, text, size, 0, status);
    (void)a;     /* Suppress set but not used warning. */


    if(!U_FAILURE(*status))
    {
        log_err("Error in unum_parseInt64(): %s \n", myErrorName(*status));
    }
    else
    {
        log_verbose("unum_parseInt64() successful\n");
    }

    unum_close(nf);
    return;
}

/* test Number Format API */
static void TestNumberFormat()
{
    UChar *result=NULL;
    UChar temp1[512];
    UChar temp2[512];

    UChar temp[5];

    UChar prefix[5];
    UChar suffix[5];
    UChar symbol[20];
    int32_t resultlength;
    int32_t resultlengthneeded;
    int32_t parsepos;
    double d1 = -1.0;
    int32_t l1;
    double d = -10456.37;
    double a = 1234.56, a1 = 1235.0;
    int32_t l = 100000000;
    UFieldPosition pos1;
    UFieldPosition pos2;
    int32_t numlocales;
    int32_t i;

    UNumberFormatAttribute attr;
    UNumberFormatSymbol symType = UNUM_DECIMAL_SEPARATOR_SYMBOL;
    int32_t newvalue;
    UErrorCode status=U_ZERO_ERROR;
    UNumberFormatStyle style= UNUM_DEFAULT;
    UNumberFormat *pattern;
    UNumberFormat *def, *fr, *cur_def, *cur_fr, *per_def, *per_fr,
                  *cur_frpattern, *myclone, *spellout_def;

    /* Testing unum_open() with various Numberformat styles and locales*/
    status = U_ZERO_ERROR;
    log_verbose("Testing  unum_open() with default style and locale\n");
    def=unum_open(style, NULL,0,NULL, NULL,&status);

    /* Might as well pack it in now if we can't even get a default NumberFormat... */
    if(U_FAILURE(status))
    {
        log_data_err("Error in creating default NumberFormat using unum_open(): %s (Are you missing data?)\n", myErrorName(status));
        return;
    }

    log_verbose("\nTesting unum_open() with french locale and default style(decimal)\n");
    fr=unum_open(style,NULL,0, "fr_FR",NULL, &status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat (french): %s\n", myErrorName(status));

    log_verbose("\nTesting unum_open(currency,NULL,status)\n");
    style=UNUM_CURRENCY;
    /* Can't hardcode the result to assume the default locale is "en_US". */
    cur_def=unum_open(style, NULL,0,"en_US", NULL, &status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat using \n unum_open(currency, NULL, &status) %s\n",
                        myErrorName(status) );

    log_verbose("\nTesting unum_open(currency, frenchlocale, status)\n");
    cur_fr=unum_open(style,NULL,0, "fr_FR", NULL, &status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat using unum_open(currency, french, &status): %s\n",
                myErrorName(status));

    log_verbose("\nTesting unum_open(percent, NULL, status)\n");
    style=UNUM_PERCENT;
    per_def=unum_open(style,NULL,0, NULL,NULL, &status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat using unum_open(percent, NULL, &status): %s\n", myErrorName(status));

    log_verbose("\nTesting unum_open(percent,frenchlocale, status)\n");
    per_fr=unum_open(style, NULL,0,"fr_FR", NULL,&status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat using unum_open(percent, french, &status): %s\n", myErrorName(status));

    log_verbose("\nTesting unum_open(spellout, NULL, status)");
    style=UNUM_SPELLOUT;
    spellout_def=unum_open(style, NULL, 0, "en_US", NULL, &status);
    if(U_FAILURE(status))
        log_err("Error: could not create NumberFormat using unum_open(spellout, NULL, &status): %s\n", myErrorName(status));

    /* Testing unum_clone(..) */
    log_verbose("\nTesting unum_clone(fmt, status)");
    status = U_ZERO_ERROR;
    myclone = unum_clone(def,&status);
    if(U_FAILURE(status))
        log_err("Error: could not clone unum_clone(def, &status): %s\n", myErrorName(status));
    else
    {
        log_verbose("unum_clone() successful\n");
    }

    /*Testing unum_getAvailable() and unum_countAvailable()*/
    log_verbose("\nTesting getAvailableLocales and countAvailable()\n");
    numlocales=unum_countAvailable();
    if(numlocales < 0)
        log_err("error in countAvailable");
    else{
        log_verbose("unum_countAvailable() successful\n");
        log_verbose("The no: of locales where number formatting is applicable is %d\n", numlocales);
    }
    for(i=0;i<numlocales;i++)
    {
        log_verbose("%s\n", unum_getAvailable(i));
        if (unum_getAvailable(i) == 0)
            log_err("No locale for which number formatting patterns are applicable\n");
        else
            log_verbose("A locale %s for which number formatting patterns are applicable\n",unum_getAvailable(i));
    }


    /*Testing unum_format() and unum_formatdouble()*/
    u_uastrcpy(temp1, "$100,000,000.00");

    log_verbose("\nTesting unum_format() \n");
    resultlength=0;
    pos1.field = UNUM_INTEGER_FIELD;
    resultlengthneeded=unum_format(cur_def, l, NULL, resultlength, &pos1, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
/*        for (i = 0; i < 100000; i++) */
        {
            unum_format(cur_def, l, result, resultlength, &pos1, &status);
        }
    }

    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_format(.....): %s\n", myErrorName(status) );
    }
    if(u_strcmp(result, temp1)==0)
        log_verbose("Pass: Number formatting using unum_format() successful\n");
    else
        log_err("Fail: Error in number Formatting using unum_format()\n");
    if(pos1.beginIndex == 1 && pos1.endIndex == 12)
        log_verbose("Pass: Complete number formatting using unum_format() successful\n");
    else
        log_err("Fail: Error in complete number Formatting using unum_format()\nGot: b=%d end=%d\nExpected: b=1 end=12\n",
                pos1.beginIndex, pos1.endIndex);

    free(result);
    result = 0;

    log_verbose("\nTesting unum_formatDouble()\n");
    u_uastrcpy(temp1, "-$10,456.37");
    resultlength=0;
    pos2.field = UNUM_FRACTION_FIELD;
    resultlengthneeded=unum_formatDouble(cur_def, d, NULL, resultlength, &pos2, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
/*        for (i = 0; i < 100000; i++) */
        {
            unum_formatDouble(cur_def, d, result, resultlength, &pos2, &status);
        }
    }
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_formatDouble(.....): %s\n", myErrorName(status));
    }
    if(result && u_strcmp(result, temp1)==0)
        log_verbose("Pass: Number Formatting using unum_formatDouble() Successful\n");
    else {
      log_err("FAIL: Error in number formatting using unum_formatDouble() - got '%s' expected '%s'\n",
              aescstrdup(result, -1), aescstrdup(temp1, -1));
    }
    if(pos2.beginIndex == 9 && pos2.endIndex == 11)
        log_verbose("Pass: Complete number formatting using unum_format() successful\n");
    else
        log_err("Fail: Error in complete number Formatting using unum_formatDouble()\nGot: b=%d end=%d\nExpected: b=9 end=11",
                pos1.beginIndex, pos1.endIndex);


    /* Testing unum_parse() and unum_parseDouble() */
    log_verbose("\nTesting unum_parseDouble()\n");
/*    for (i = 0; i < 100000; i++)*/
    parsepos=0;
    if (result != NULL) {
      d1=unum_parseDouble(cur_def, result, u_strlen(result), &parsepos, &status);
    } else {
      log_err("result is NULL\n");
    }
    if(U_FAILURE(status)) {
      log_err("parse of '%s' failed. Parsepos=%d. The error is  : %s\n", aescstrdup(result,u_strlen(result)),parsepos, myErrorName(status));
    }

    if(d1!=d)
        log_err("Fail: Error in parsing\n");
    else
        log_verbose("Pass: parsing successful\n");
    if (result)
        free(result);
    result = 0;

    status = U_ZERO_ERROR;
    /* Testing unum_formatDoubleCurrency / unum_parseDoubleCurrency */
    log_verbose("\nTesting unum_formatDoubleCurrency\n");
    u_uastrcpy(temp1, "Y1,235");
    temp1[0] = 0xA5; /* Yen sign */
    u_uastrcpy(temp, "JPY");
    resultlength=0;
    pos2.field = UNUM_INTEGER_FIELD;
    resultlengthneeded=unum_formatDoubleCurrency(cur_def, a, temp, NULL, resultlength, &pos2, &status);
    if (status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        unum_formatDoubleCurrency(cur_def, a, temp, result, resultlength, &pos2, &status);
    }
    if (U_FAILURE(status)) {
        log_err("Error in formatting using unum_formatDoubleCurrency(.....): %s\n", myErrorName(status));
    }
    if (result && u_strcmp(result, temp1)==0) {
        log_verbose("Pass: Number Formatting using unum_formatDoubleCurrency() Successful\n");
    } else {
        log_err("FAIL: Error in number formatting using unum_formatDoubleCurrency() - got '%s' expected '%s'\n",
                aescstrdup(result, -1), aescstrdup(temp1, -1));
    }
    if (pos2.beginIndex == 1 && pos2.endIndex == 6) {
        log_verbose("Pass: Complete number formatting using unum_format() successful\n");
    } else {
        log_err("Fail: Error in complete number Formatting using unum_formatDouble()\nGot: b=%d end=%d\nExpected: b=1 end=6\n",
                pos1.beginIndex, pos1.endIndex);
    }

    log_verbose("\nTesting unum_parseDoubleCurrency\n");
    parsepos=0;
    if (result == NULL) {
        log_err("result is NULL\n");
    }
    else {
        d1=unum_parseDoubleCurrency(cur_def, result, u_strlen(result), &parsepos, temp2, &status);
        if (U_FAILURE(status)) {
          log_err("parseDoubleCurrency '%s' failed. The error is  : %s\n", aescstrdup(result, u_strlen(result)), myErrorName(status));
        }
        /* Note: a==1234.56, but on parse expect a1=1235.0 */
        if (d1!=a1) {
            log_err("Fail: Error in parsing currency, got %f, expected %f\n", d1, a1);
        } else {
            log_verbose("Pass: parsed currency amount successfully\n");
        }
        if (u_strcmp(temp2, temp)==0) {
            log_verbose("Pass: parsed correct currency\n");
        } else {
            log_err("Fail: parsed incorrect currency\n");
        }
    }
    status = U_ZERO_ERROR; /* reset */

    free(result);
    result = 0;


/* performance testing */
    u_uastrcpy(temp1, "$462.12345");
    resultlength=u_strlen(temp1);
/*    for (i = 0; i < 100000; i++) */
    {
        parsepos=0;
        d1=unum_parseDouble(cur_def, temp1, resultlength, &parsepos, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("parseDouble('%s') failed. The error is  : %s\n", aescstrdup(temp1, resultlength), myErrorName(status));
    }

    /*
     * Note: "for strict standard conformance all operations and constants are now supposed to be
              evaluated in precision of long double".  So,  we assign a1 before comparing to a double. Bug #7932.
     */
    a1 = 462.12345;

    if(d1!=a1)
        log_err("Fail: Error in parsing\n");
    else
        log_verbose("Pass: parsing successful\n");

free(result);

    u_uastrcpy(temp1, "($10,456.3E1])");
    parsepos=0;
    d1=unum_parseDouble(cur_def, temp1, u_strlen(temp1), &parsepos, &status);
    if(U_SUCCESS(status))
    {
        log_err("Error in unum_parseDouble(..., %s, ...): %s\n", temp1, myErrorName(status));
    }


    log_verbose("\nTesting unum_format()\n");
    status=U_ZERO_ERROR;
    resultlength=0;
    parsepos=0;
    resultlengthneeded=unum_format(per_fr, l, NULL, resultlength, &pos1, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
/*        for (i = 0; i < 100000; i++)*/
        {
            unum_format(per_fr, l, result, resultlength, &pos1, &status);
        }
    }
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_format(.....): %s\n", myErrorName(status));
    }


    log_verbose("\nTesting unum_parse()\n");
/*    for (i = 0; i < 100000; i++) */
    {
        parsepos=0;
        l1=unum_parse(per_fr, result, u_strlen(result), &parsepos, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("parse failed. The error is  : %s\n", myErrorName(status));
    }

    if(l1!=l)
        log_err("Fail: Error in parsing\n");
    else
        log_verbose("Pass: parsing successful\n");

free(result);

    /* create a number format using unum_openPattern(....)*/
    log_verbose("\nTesting unum_openPattern()\n");
    u_uastrcpy(temp1, "#,##0.0#;(#,##0.0#)");
    pattern=unum_open(UNUM_IGNORE,temp1, u_strlen(temp1), NULL, NULL,&status);
    if(U_FAILURE(status))
    {
        log_err("error in unum_openPattern(): %s\n", myErrorName(status) );
    }
    else
        log_verbose("Pass: unum_openPattern() works fine\n");

    /*test for unum_toPattern()*/
    log_verbose("\nTesting unum_toPattern()\n");
    resultlength=0;
    resultlengthneeded=unum_toPattern(pattern, false, NULL, resultlength, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        unum_toPattern(pattern, false, result, resultlength, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("error in extracting the pattern from UNumberFormat: %s\n", myErrorName(status));
    }
    else
    {
        if(u_strcmp(result, temp1)!=0)
            log_err("FAIL: Error in extracting the pattern using unum_toPattern()\n");
        else
            log_verbose("Pass: extracted the pattern correctly using unum_toPattern()\n");
free(result);
    }

    /*Testing unum_getSymbols() and unum_setSymbols()*/
    log_verbose("\nTesting unum_getSymbols and unum_setSymbols()\n");
    /*when we try to change the symbols of french to default we need to apply the pattern as well to fetch correct results */
    resultlength=0;
    resultlengthneeded=unum_toPattern(cur_def, false, NULL, resultlength, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        unum_toPattern(cur_def, false, result, resultlength, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("error in extracting the pattern from UNumberFormat: %s\n", myErrorName(status));
    }

    status=U_ZERO_ERROR;
    cur_frpattern=unum_open(UNUM_IGNORE,result, u_strlen(result), "fr_FR",NULL, &status);
    if(U_FAILURE(status))
    {
        log_err("error in unum_openPattern(): %s\n", myErrorName(status));
    }

free(result);

    /*getting the symbols of cur_def */
    /*set the symbols of cur_frpattern to cur_def */
    for (symType = UNUM_DECIMAL_SEPARATOR_SYMBOL; symType < UNUM_FORMAT_SYMBOL_COUNT; symType++) {
        status=U_ZERO_ERROR;
        unum_getSymbol(cur_def, symType, temp1, sizeof(temp1), &status);
        unum_setSymbol(cur_frpattern, symType, temp1, -1, &status);
        if(U_FAILURE(status))
        {
            log_err("Error in get/set symbols: %s\n", myErrorName(status));
        }
    }

    /*format to check the result */
    resultlength=0;
    resultlengthneeded=unum_format(cur_def, l, NULL, resultlength, &pos1, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        unum_format(cur_def, l, result, resultlength, &pos1, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_format(.....): %s\n", myErrorName(status));
    }

    if(U_FAILURE(status)){
        log_err("Fail: error in unum_setSymbols: %s\n", myErrorName(status));
    }
    unum_applyPattern(cur_frpattern, false, result, u_strlen(result),NULL,NULL);

    for (symType = UNUM_DECIMAL_SEPARATOR_SYMBOL; symType < UNUM_FORMAT_SYMBOL_COUNT; symType++) {
        status=U_ZERO_ERROR;
        unum_getSymbol(cur_def, symType, temp1, sizeof(temp1), &status);
        unum_getSymbol(cur_frpattern, symType, temp2, sizeof(temp2), &status);
        if(U_FAILURE(status) || u_strcmp(temp1, temp2) != 0)
        {
            log_err("Fail: error in getting symbols\n");
        }
        else
            log_verbose("Pass: get and set symbols successful\n");
    }

    /*format and check with the previous result */

    resultlength=0;
    resultlengthneeded=unum_format(cur_frpattern, l, NULL, resultlength, &pos1, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        unum_format(cur_frpattern, l, temp1, resultlength, &pos1, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_format(.....): %s\n", myErrorName(status));
    }
    /* TODO:
     * This test fails because we have not called unum_applyPattern().
     * Currently, such an applyPattern() does not exist on the C API, and
     * we have jitterbug 411 for it.
     * Since it is close to the 1.5 release, I (markus) am disabling this test just
     * for this release (I added the test itself only last week).
     * For the next release, we need to fix this.
     * Then, remove the uprv_strcmp("1.5", ...) and this comment, and the include "cstring.h" at the beginning of this file.
     */
    if(u_strcmp(result, temp1) != 0) {
        log_err("Formatting failed after setting symbols. result=%s temp1=%s\n", result, temp1);
    }


    /*----------- */

free(result);

    /* Testing unum_get/setSymbol() */
    for(i = 0; i < UNUM_FORMAT_SYMBOL_COUNT; ++i) {
        symbol[0] = (UChar)(0x41 + i);
        symbol[1] = (UChar)(0x61 + i);
        unum_setSymbol(cur_frpattern, (UNumberFormatSymbol)i, symbol, 2, &status);
        if(U_FAILURE(status)) {
            log_err("Error from unum_setSymbol(%d): %s\n", i, myErrorName(status));
            return;
        }
    }
    for(i = 0; i < UNUM_FORMAT_SYMBOL_COUNT; ++i) {
        resultlength = unum_getSymbol(cur_frpattern, (UNumberFormatSymbol)i, symbol, UPRV_LENGTHOF(symbol), &status);
        if(U_FAILURE(status)) {
            log_err("Error from unum_getSymbol(%d): %s\n", i, myErrorName(status));
            return;
        }
        if(resultlength != 2 || symbol[0] != 0x41 + i || symbol[1] != 0x61 + i) {
            log_err("Failure in unum_getSymbol(%d): got unexpected symbol\n", i);
        }
    }
    /*try getting from a bogus symbol*/
    unum_getSymbol(cur_frpattern, (UNumberFormatSymbol)i, symbol, UPRV_LENGTHOF(symbol), &status);
    if(U_SUCCESS(status)){
        log_err("Error : Expected U_ILLEGAL_ARGUMENT_ERROR for bogus symbol");
    }
    if(U_FAILURE(status)){
        if(status != U_ILLEGAL_ARGUMENT_ERROR){
            log_err("Error: Expected U_ILLEGAL_ARGUMENT_ERROR for bogus symbol, Got %s\n", myErrorName(status));
        }
    }
    status=U_ZERO_ERROR;

    /* Testing unum_getTextAttribute() and unum_setTextAttribute()*/
    log_verbose("\nTesting getting and setting text attributes\n");
    resultlength=5;
    unum_getTextAttribute(cur_fr, UNUM_NEGATIVE_SUFFIX, temp, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("Failure in getting the Text attributes of number format: %s\n", myErrorName(status));
    }
    unum_setTextAttribute(cur_def, UNUM_NEGATIVE_SUFFIX, temp, u_strlen(temp), &status);
    if(U_FAILURE(status))
    {
        log_err("Failure in getting the Text attributes of number format: %s\n", myErrorName(status));
    }
    unum_getTextAttribute(cur_def, UNUM_NEGATIVE_SUFFIX, suffix, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("Failure in getting the Text attributes of number format: %s\n", myErrorName(status));
    }
    if(u_strcmp(suffix,temp)!=0)
        log_err("Fail:Error in setTextAttribute or getTextAttribute in setting and getting suffix\n");
    else
        log_verbose("Pass: setting and getting suffix works fine\n");
    /*set it back to normal */
    u_uastrcpy(temp,"$");
    unum_setTextAttribute(cur_def, UNUM_NEGATIVE_SUFFIX, temp, u_strlen(temp), &status);

    /*checking some more text setter conditions */
    u_uastrcpy(prefix, "+");
    unum_setTextAttribute(def, UNUM_POSITIVE_PREFIX, prefix, u_strlen(prefix) , &status);
    if(U_FAILURE(status))
    {
        log_err("error in setting the text attributes : %s\n", myErrorName(status));
    }
    unum_getTextAttribute(def, UNUM_POSITIVE_PREFIX, temp, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("error in getting the text attributes : %s\n", myErrorName(status));
    }

    if(u_strcmp(prefix, temp)!=0)
        log_err("ERROR: get and setTextAttributes with positive prefix failed\n");
    else
        log_verbose("Pass: get and setTextAttributes with positive prefix works fine\n");

    u_uastrcpy(prefix, "+");
    unum_setTextAttribute(def, UNUM_NEGATIVE_PREFIX, prefix, u_strlen(prefix), &status);
    if(U_FAILURE(status))
    {
        log_err("error in setting the text attributes : %s\n", myErrorName(status));
    }
    unum_getTextAttribute(def, UNUM_NEGATIVE_PREFIX, temp, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("error in getting the text attributes : %s\n", myErrorName(status));
    }
    if(u_strcmp(prefix, temp)!=0)
        log_err("ERROR: get and setTextAttributes with negative prefix failed\n");
    else
        log_verbose("Pass: get and setTextAttributes with negative prefix works fine\n");

    u_uastrcpy(suffix, "+");
    unum_setTextAttribute(def, UNUM_NEGATIVE_SUFFIX, suffix, u_strlen(suffix) , &status);
    if(U_FAILURE(status))
    {
        log_err("error in setting the text attributes: %s\n", myErrorName(status));
    }

    unum_getTextAttribute(def, UNUM_NEGATIVE_SUFFIX, temp, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("error in getting the text attributes : %s\n", myErrorName(status));
    }
    if(u_strcmp(suffix, temp)!=0)
        log_err("ERROR: get and setTextAttributes with negative suffix failed\n");
    else
        log_verbose("Pass: get and settextAttributes with negative suffix works fine\n");

    u_uastrcpy(suffix, "++");
    unum_setTextAttribute(def, UNUM_POSITIVE_SUFFIX, suffix, u_strlen(suffix) , &status);
    if(U_FAILURE(status))
    {
        log_err("error in setting the text attributes: %s\n", myErrorName(status));
    }

    unum_getTextAttribute(def, UNUM_POSITIVE_SUFFIX, temp, resultlength, &status);
    if(U_FAILURE(status))
    {
        log_err("error in getting the text attributes : %s\n", myErrorName(status));
    }
    if(u_strcmp(suffix, temp)!=0)
        log_err("ERROR: get and setTextAttributes with negative suffix failed\n");
    else
        log_verbose("Pass: get and settextAttributes with negative suffix works fine\n");

    /*Testing unum_getAttribute and  unum_setAttribute() */
    log_verbose("\nTesting get and set Attributes\n");
    attr=UNUM_GROUPING_SIZE;
    assertTrue("unum_hasAttribute returned false for UNUM_GROUPING_SIZE", unum_hasAttribute(def, attr));
    newvalue=unum_getAttribute(def, attr);
    newvalue=2;
    unum_setAttribute(def, attr, newvalue);
    if(unum_getAttribute(def,attr)!=2)
        log_err("Fail: error in setting and getting attributes for UNUM_GROUPING_SIZE\n");
    else
        log_verbose("Pass: setting and getting attributes for UNUM_GROUPING_SIZE works fine\n");

    attr=UNUM_MULTIPLIER;
    assertTrue("unum_hasAttribute returned false for UNUM_MULTIPLIER", unum_hasAttribute(def, attr));
    newvalue=unum_getAttribute(def, attr);
    newvalue=8;
    unum_setAttribute(def, attr, newvalue);
    if(unum_getAttribute(def,attr) != 8)
        log_err("error in setting and getting attributes for UNUM_MULTIPLIER\n");
    else
        log_verbose("Pass:setting and getting attributes for UNUM_MULTIPLIER works fine\n");

    attr=UNUM_SECONDARY_GROUPING_SIZE;
    assertTrue("unum_hasAttribute returned false for UNUM_SECONDARY_GROUPING_SIZE", unum_hasAttribute(def, attr));
    newvalue=unum_getAttribute(def, attr);
    newvalue=2;
    unum_setAttribute(def, attr, newvalue);
    if(unum_getAttribute(def,attr) != 2)
        log_err("error in setting and getting attributes for UNUM_SECONDARY_GROUPING_SIZE: got %d\n",
                unum_getAttribute(def,attr));
    else
        log_verbose("Pass:setting and getting attributes for UNUM_SECONDARY_GROUPING_SIZE works fine\n");

    /*testing set and get Attributes extensively */
    log_verbose("\nTesting get and set attributes extensively\n");
    for(attr=UNUM_PARSE_INT_ONLY; attr<= UNUM_PADDING_POSITION; attr=(UNumberFormatAttribute)((int32_t)attr + 1) )
    {
        assertTrue("unum_hasAttribute returned false", unum_hasAttribute(fr, attr));
        newvalue=unum_getAttribute(fr, attr);
        unum_setAttribute(def, attr, newvalue);
        if(unum_getAttribute(def,attr)!=unum_getAttribute(fr, attr))
            log_err("error in setting and getting attributes\n");
        else
            log_verbose("Pass: attributes set and retrieved successfully\n");
    }

    /*testing spellout format to make sure we can use it successfully.*/
    log_verbose("\nTesting spellout format\n");
    if (spellout_def)
    {
        // check that unum_hasAttribute() works right with a spellout formatter
        assertTrue("unum_hasAttribute() returned true for UNUM_MULTIPLIER on a spellout formatter", !unum_hasAttribute(spellout_def, UNUM_MULTIPLIER));
        assertTrue("unum_hasAttribute() returned false for UNUM_LENIENT_PARSE on a spellout formatter", unum_hasAttribute(spellout_def, UNUM_LENIENT_PARSE));

        static const int32_t values[] = { 0, -5, 105, 1005, 105050 };
        for (i = 0; i < UPRV_LENGTHOF(values); ++i) {
            UChar buffer[128];
            int32_t len;
            int32_t value = values[i];
            status = U_ZERO_ERROR;
            len = unum_format(spellout_def, value, buffer, UPRV_LENGTHOF(buffer), NULL, &status);
            if(U_FAILURE(status)) {
                log_err("Error in formatting using unum_format(spellout_fmt, ...): %s\n", myErrorName(status));
            } else {
                int32_t pp = 0;
                int32_t parseResult;
                /*ustrToAstr(buffer, len, logbuf, UPRV_LENGTHOF(logbuf));*/
                log_verbose("formatted %d as '%s', length: %d\n", value, aescstrdup(buffer, len), len);

                parseResult = unum_parse(spellout_def, buffer, len, &pp, &status);
                if (U_FAILURE(status)) {
                    log_err("Error in parsing using unum_format(spellout_fmt, ...): %s\n", myErrorName(status));
                } else if (parseResult != value) {
                    log_err("unum_format result %d != value %d\n", parseResult, value);
                }
            }
        }
    }
    else {
        log_err("Spellout format is unavailable\n");
    }

    {    /* Test for ticket #7079 */
        UNumberFormat* dec_en;
        UChar groupingSep[] = { 0 };
        UChar numPercent[] = { 0x0031, 0x0032, 0x0025, 0 }; /* "12%" */
        double parseResult = 0.0;

        status=U_ZERO_ERROR;
        dec_en = unum_open(UNUM_DECIMAL, NULL, 0, "en_US", NULL, &status);
        unum_setAttribute(dec_en, UNUM_LENIENT_PARSE, 0);
        unum_setSymbol(dec_en, UNUM_GROUPING_SEPARATOR_SYMBOL, groupingSep, 0, &status);
        parseResult = unum_parseDouble(dec_en, numPercent, -1, NULL, &status);
        /* Without the fix in #7079, the above call will hang */
        if ( U_FAILURE(status) || parseResult != 12.0 ) {
            log_err("unum_parseDouble with empty groupingSep: status %s, parseResult %f not 12.0\n",
                    myErrorName(status), parseResult);
        } else {
            log_verbose("unum_parseDouble with empty groupingSep: no hang, OK\n");
        }
        unum_close(dec_en);
    }

    {   /* Test parse & format of big decimals.  Use a number with too many digits to fit in a double,
                                         to verify that it is taking the pure decimal path. */
        UNumberFormat *fmt;
        const char *bdpattern = "#,##0.#########";
        const char *numInitial     = "12345678900987654321.1234567896";
        const char *numFormatted  = "12,345,678,900,987,654,321.12345679";
        const char *parseExpected = "1.234567890098765432112345679E+19";
        const char *parseExpected2 = "3.4567890098765432112345679E+17";
        int32_t resultSize    = 0;
        int32_t parsePos      = 0;     /* Output parameter for Parse operations. */
        #define DESTCAPACITY 100
        UChar dest[DESTCAPACITY];
        char  desta[DESTCAPACITY];
        UFieldPosition fieldPos = {0};

        /* Format */

        status = U_ZERO_ERROR;
        u_uastrcpy(dest, bdpattern);
        fmt = unum_open(UNUM_PATTERN_DECIMAL, dest, -1, "en", NULL /*parseError*/, &status);
        if (U_FAILURE(status)) log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));

        resultSize = unum_formatDecimal(fmt, numInitial, -1, dest, DESTCAPACITY, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));
        }
        u_austrncpy(desta, dest, DESTCAPACITY);
        if (strcmp(numFormatted, desta) != 0) {
            log_err("File %s, Line %d, (expected, actual) =  (\"%s\", \"%s\")\n",
                    __FILE__, __LINE__, numFormatted, desta);
        }
        if ((int32_t)strlen(numFormatted) != resultSize) {
            log_err("File %s, Line %d, (expected, actual) = (%d, %d)\n",
                     __FILE__, __LINE__, (int32_t)strlen(numFormatted), resultSize);
        }

        /* Format with a FieldPosition parameter */

        fieldPos.field = UNUM_DECIMAL_SEPARATOR_FIELD;
        resultSize = unum_formatDecimal(fmt, numInitial, -1, dest, DESTCAPACITY, &fieldPos, &status);
        if (U_FAILURE(status)) {
            log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));
        }
        u_austrncpy(desta, dest, DESTCAPACITY);
        if (strcmp(numFormatted, desta) != 0) {
            log_err("File %s, Line %d, (expected, actual) =  (\"%s\", \"%s\")\n",
                    __FILE__, __LINE__, numFormatted, desta);
        }
        if (fieldPos.beginIndex != 26) {  /* index of "." in formatted number */
            log_err("File %s, Line %d, (expected, actual) =  (%d, %d)\n",
                    __FILE__, __LINE__, 0, fieldPos.beginIndex);
        }
        if (fieldPos.endIndex != 27) {
            log_err("File %s, Line %d, (expected, actual) =  (%d, %d)\n",
                    __FILE__, __LINE__, 0, fieldPos.endIndex);
        }

        /* Parse */

        status = U_ZERO_ERROR;
        u_uastrcpy(dest, numFormatted);   /* Parse the expected output of the formatting test */
        resultSize = unum_parseDecimal(fmt, dest, -1, NULL, desta, DESTCAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));
        }
        if (uprv_strcmp(parseExpected, desta) != 0) {
            log_err("File %s, Line %d, (expected, actual) = (\"%s\", \"%s\")\n",
                    __FILE__, __LINE__, parseExpected, desta);
        } else {
            log_verbose("File %s, Line %d, got expected = \"%s\"\n",
                    __FILE__, __LINE__, desta);
        }
        if ((int32_t)strlen(parseExpected) != resultSize) {
            log_err("File %s, Line %d, (expected, actual) = (%d, %d)\n",
                    __FILE__, __LINE__, (int32_t)strlen(parseExpected), resultSize);
        }

        /* Parse with a parsePos parameter */

        status = U_ZERO_ERROR;
        u_uastrcpy(dest, numFormatted);   /* Parse the expected output of the formatting test */
        parsePos = 3;                 /*      12,345,678,900,987,654,321.12345679         */
                                      /* start parsing at the the third char              */
        resultSize = unum_parseDecimal(fmt, dest, -1, &parsePos, desta, DESTCAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));
        }
        if (strcmp(parseExpected2, desta) != 0) {   /*  "3.4567890098765432112345679E+17" */
            log_err("File %s, Line %d, (expected, actual) = (\"%s\", \"%s\")\n",
                    __FILE__, __LINE__, parseExpected2, desta);
        } else {
            log_verbose("File %s, Line %d, got expected = \"%s\"\n",
                    __FILE__, __LINE__, desta);
        }
        if ((int32_t)strlen(numFormatted) != parsePos) {
            log_err("File %s, Line %d, parsePos (expected, actual) = (\"%d\", \"%d\")\n",
                    __FILE__, __LINE__, (int32_t)strlen(parseExpected), parsePos);
        }

        unum_close(fmt);
    }

    status = U_ZERO_ERROR;
    /* Test invalid symbol argument */
    {
        int32_t badsymbolLarge = UNUM_FORMAT_SYMBOL_COUNT + 1;
        int32_t badsymbolSmall = -1;
        UChar value[10];
        int32_t valueLength = 10;
        UNumberFormat *fmt = unum_open(UNUM_DEFAULT, NULL, 0, NULL, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("File %s, Line %d, status = %s\n", __FILE__, __LINE__, u_errorName(status));
        } else {
            unum_getSymbol(fmt, (UNumberFormatSymbol)badsymbolLarge, NULL, 0, &status);
            if (U_SUCCESS(status)) log_err("unum_getSymbol()'s status should be ILLEGAL_ARGUMENT with invalid symbol (> UNUM_FORMAT_SYMBOL_COUNT) argument\n");

            status = U_ZERO_ERROR;
            unum_getSymbol(fmt, (UNumberFormatSymbol)badsymbolSmall, NULL, 0, &status);
            if (U_SUCCESS(status)) log_err("unum_getSymbol()'s status should be ILLEGAL_ARGUMENT with invalid symbol (less than 0) argument\n");

            status = U_ZERO_ERROR;
            unum_setSymbol(fmt, (UNumberFormatSymbol)badsymbolLarge, value, valueLength, &status);
            if (U_SUCCESS(status)) log_err("unum_setSymbol()'s status should be ILLEGAL_ARGUMENT with invalid symbol (> UNUM_FORMAT_SYMBOL_COUNT) argument\n");

            status = U_ZERO_ERROR;
            unum_setSymbol(fmt, (UNumberFormatSymbol)badsymbolSmall, value, valueLength, &status);
            if (U_SUCCESS(status)) log_err("unum_setSymbol()'s status should be ILLEGAL_ARGUMENT with invalid symbol (less than 0) argument\n");

            unum_close(fmt);
        }
    }


    /*closing the NumberFormat() using unum_close(UNumberFormat*)")*/
    unum_close(def);
    unum_close(fr);
    unum_close(cur_def);
    unum_close(cur_fr);
    unum_close(per_def);
    unum_close(per_fr);
    unum_close(spellout_def);
    unum_close(pattern);
    unum_close(cur_frpattern);
    unum_close(myclone);

}

static void TestParseZero(void)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UChar input[] = {0x30, 0};   /*  Input text is decimal '0' */
    UChar pat[] = {0x0023,0x003b,0x0023,0}; /*  {'#', ';', '#', 0}; */
    double  dbl;

#if 0
    UNumberFormat* unum = unum_open( UNUM_DECIMAL /*or UNUM_DEFAULT*/, NULL, -1, NULL, NULL, &errorCode);
#else
    UNumberFormat* unum = unum_open( UNUM_PATTERN_DECIMAL /*needs pattern*/, pat, -1, NULL, NULL, &errorCode);
#endif

    dbl = unum_parseDouble( unum, input, -1 /*u_strlen(input)*/, 0 /* 0 = start */, &errorCode );
    if (U_FAILURE(errorCode)) {
        log_data_err("Result - %s\n", u_errorName(errorCode));
    } else {
        log_verbose("Double: %f\n", dbl);
    }
    unum_close(unum);
}

static const UChar dollars2Sym[] = { 0x24,0x32,0x2E,0x30,0x30,0 }; /* $2.00 */
static const UChar dollars4Sym[] = { 0x24,0x34,0 }; /* $4 */
static const UChar dollarsUS4Sym[] = { 0x55,0x53,0x24,0x34,0 }; /* US$4 */
static const UChar dollars9Sym[] = { 0x39,0xA0,0x24,0 }; /* 9 $ */
static const UChar pounds3Sym[]  = { 0xA3,0x33,0x2E,0x30,0x30,0 }; /* [POUND]3.00 */
static const UChar pounds5Sym[]  = { 0xA3,0x35,0 }; /* [POUND]5 */
static const UChar pounds7Sym[]  = { 0x37,0xA0,0xA3,0 }; /* 7 [POUND] */
static const UChar euros4Sym[]   = { 0x34,0x2C,0x30,0x30,0xA0,0x20AC,0 }; /* 4,00 [EURO] */
static const UChar euros6Sym[]   = { 0x36,0xA0,0x20AC,0 }; /* 6 [EURO] */
static const UChar euros8Sym[]   = { 0x20AC,0x38,0 }; /* [EURO]8 */
static const UChar dollars4PluEn[] = { 0x34,0x20,0x55,0x53,0x20,0x64,0x6F,0x6C,0x6C,0x61,0x72,0x73,0 }; /* 4 US dollars*/
static const UChar pounds5PluEn[]  = { 0x35,0x20,0x42,0x72,0x69,0x74,0x69,0x73,0x68,0x20,0x70,0x6F,0x75,0x6E,0x64,0x73,0x20,0x73,0x74,0x65,0x72,0x6C,0x69,0x6E,0x67,0 }; /* 5 British pounds sterling */
static const UChar euros8PluEn[]   = { 0x38,0x20,0x65,0x75,0x72,0x6F,0x73,0 }; /* 8 euros*/
static const UChar euros6PluFr[]   = { 0x36,0x20,0x65,0x75,0x72,0x6F,0x73,0 }; /* 6 euros*/

typedef struct {
    const char *  locale;
    const char *  descrip;
    const UChar * currStr;
    const UChar * plurStr;
    UErrorCode    parsDoubExpectErr;
    int32_t       parsDoubExpectPos;
    double        parsDoubExpectVal;
    UErrorCode    parsCurrExpectErr;
    int32_t       parsCurrExpectPos;
    double        parsCurrExpectVal;
    const char *  parsCurrExpectCurr;
} ParseCurrencyItem;

static const ParseCurrencyItem parseCurrencyItems[] = {
    { "en_US", "dollars2", dollars2Sym, NULL,           U_ZERO_ERROR,  5, 2.0, U_ZERO_ERROR,  5, 2.0, "USD" },
    { "en_US", "dollars4", dollars4Sym, dollars4PluEn,  U_ZERO_ERROR,  2, 4.0, U_ZERO_ERROR,  2, 4.0, "USD" },
    { "en_US", "dollars9", dollars9Sym, NULL,           U_PARSE_ERROR, 1, 0.0, U_PARSE_ERROR, 1, 0.0, ""    },
    { "en_US", "pounds3",  pounds3Sym,  NULL,           U_PARSE_ERROR, 0, 0.0, U_ZERO_ERROR,  5, 3.0, "GBP" },
    { "en_US", "pounds5",  pounds5Sym,  pounds5PluEn,   U_PARSE_ERROR, 0, 0.0, U_ZERO_ERROR,  2, 5.0, "GBP" },
    { "en_US", "pounds7",  pounds7Sym,  NULL,           U_PARSE_ERROR, 1, 0.0, U_PARSE_ERROR, 1, 0.0, ""    },
    { "en_US", "euros8",   euros8Sym,   euros8PluEn,    U_PARSE_ERROR, 0, 0.0, U_ZERO_ERROR,  2, 8.0, "EUR" },

    { "en_GB", "pounds3",  pounds3Sym,  NULL,           U_ZERO_ERROR,  5, 3.0, U_ZERO_ERROR,  5, 3.0, "GBP" },
    { "en_GB", "pounds5",  pounds5Sym,  pounds5PluEn,   U_ZERO_ERROR,  2, 5.0, U_ZERO_ERROR,  2, 5.0, "GBP" },
    { "en_GB", "pounds7",  pounds7Sym,  NULL,           U_PARSE_ERROR, 1, 0.0, U_PARSE_ERROR, 1, 0.0, ""    },
    { "en_GB", "euros4",   euros4Sym,   NULL,           U_PARSE_ERROR, 0, 0.0, U_PARSE_ERROR, 0, 0.0, ""    },
    { "en_GB", "euros6",   euros6Sym,   NULL,           U_PARSE_ERROR, 1, 0.0, U_PARSE_ERROR, 1, 0.0, ""    },
    { "en_GB", "euros8",   euros8Sym,    euros8PluEn,   U_PARSE_ERROR, 0, 0.0, U_ZERO_ERROR,  2, 8.0, "EUR" },
    { "en_GB", "dollars4", dollarsUS4Sym,dollars4PluEn, U_PARSE_ERROR, 0, 0.0, U_ZERO_ERROR,  4, 4.0, "USD" },

    { "fr_FR", "euros4",   euros4Sym,   NULL,           U_ZERO_ERROR,  6, 4.0, U_ZERO_ERROR,  6, 4.0, "EUR" },
    { "fr_FR", "euros6",   euros6Sym,   euros6PluFr,    U_ZERO_ERROR,  3, 6.0, U_ZERO_ERROR,  3, 6.0, "EUR" },
    { "fr_FR", "euros8",   euros8Sym,   NULL,           U_PARSE_ERROR, 2, 0.0, U_PARSE_ERROR, 2, 0.0, ""    },
    { "fr_FR", "dollars2", dollars2Sym, NULL,           U_PARSE_ERROR, 0, 0.0, U_PARSE_ERROR, 0, 0.0, ""    },
    { "fr_FR", "dollars4", dollars4Sym, NULL,           U_PARSE_ERROR, 0, 0.0, U_PARSE_ERROR, 0, 0.0, ""    },

    { NULL,    NULL,       NULL,        NULL,           0,             0, 0.0, 0,             0, 0.0, NULL  }
};

static void TestParseCurrency()
{
    const ParseCurrencyItem * itemPtr;
    for (itemPtr = parseCurrencyItems; itemPtr->locale != NULL; ++itemPtr) {
        UNumberFormat* unum;
        UErrorCode status;
        double parseVal;
        int32_t parsePos;
        UChar parseCurr[4];
        char parseCurrB[4];

        status = U_ZERO_ERROR;
        unum = unum_open(UNUM_CURRENCY, NULL, 0, itemPtr->locale, NULL, &status);
        if (U_SUCCESS(status)) {
            const UChar * currStr = itemPtr->currStr;
            status = U_ZERO_ERROR;
            parsePos = 0;
            parseVal = unum_parseDouble(unum, currStr, -1, &parsePos, &status);
            if (status != itemPtr->parsDoubExpectErr || parsePos != itemPtr->parsDoubExpectPos || parseVal != itemPtr->parsDoubExpectVal) {
                log_err("UNUM_CURRENCY parseDouble %s/%s, expect %s pos %d val %.1f, get %s pos %d val %.1f\n",
                        itemPtr->locale, itemPtr->descrip,
                        u_errorName(itemPtr->parsDoubExpectErr), itemPtr->parsDoubExpectPos, itemPtr->parsDoubExpectVal,
                        u_errorName(status), parsePos, parseVal );
            }
            status = U_ZERO_ERROR;
            parsePos = 0;
            parseCurr[0] = 0;
            parseVal = unum_parseDoubleCurrency(unum, currStr, -1, &parsePos, parseCurr, &status);
            u_austrncpy(parseCurrB, parseCurr, 4);
            if (status != itemPtr->parsCurrExpectErr || parsePos != itemPtr->parsCurrExpectPos || parseVal != itemPtr->parsCurrExpectVal ||
                    strncmp(parseCurrB, itemPtr->parsCurrExpectCurr, 4) != 0) {
                log_err("UNUM_CURRENCY parseDoubleCurrency %s/%s, expect %s pos %d val %.1f cur %s, get %s pos %d val %.1f cur %s\n",
                        itemPtr->locale, itemPtr->descrip,
                        u_errorName(itemPtr->parsCurrExpectErr), itemPtr->parsCurrExpectPos, itemPtr->parsCurrExpectVal, itemPtr->parsCurrExpectCurr,
                        u_errorName(status), parsePos, parseVal, parseCurrB );
            }
            unum_close(unum);
        } else {
            log_data_err("unexpected error in unum_open UNUM_CURRENCY for locale %s: '%s'\n", itemPtr->locale, u_errorName(status));
        }

        if (itemPtr->plurStr != NULL) {
            status = U_ZERO_ERROR;
            unum = unum_open(UNUM_CURRENCY_PLURAL, NULL, 0, itemPtr->locale, NULL, &status);
            if (U_SUCCESS(status)) {
                status = U_ZERO_ERROR;
                parsePos = 0;
                parseVal = unum_parseDouble(unum, itemPtr->plurStr, -1, &parsePos, &status);
                if (status != itemPtr->parsDoubExpectErr || parseVal != itemPtr->parsDoubExpectVal) {
                    log_err("UNUM_CURRENCY parseDouble Plural %s/%s, expect %s val %.1f, get %s val %.1f\n",
                            itemPtr->locale, itemPtr->descrip,
                            u_errorName(itemPtr->parsDoubExpectErr), itemPtr->parsDoubExpectVal,
                            u_errorName(status), parseVal );
                }
                status = U_ZERO_ERROR;
                parsePos = 0;
                parseCurr[0] = 0;
                parseVal = unum_parseDoubleCurrency(unum, itemPtr->plurStr, -1, &parsePos, parseCurr, &status);
                u_austrncpy(parseCurrB, parseCurr, 4);
                if (status != itemPtr->parsCurrExpectErr || parseVal != itemPtr->parsCurrExpectVal ||
                        strncmp(parseCurrB, itemPtr->parsCurrExpectCurr, 4) != 0) {
                    log_err("UNUM_CURRENCY parseDoubleCurrency Plural %s/%s, expect %s val %.1f cur %s, get %s val %.1f cur %s\n",
                            itemPtr->locale, itemPtr->descrip,
                            u_errorName(itemPtr->parsCurrExpectErr), itemPtr->parsCurrExpectVal, itemPtr->parsCurrExpectCurr,
                            u_errorName(status), parseVal, parseCurrB );
                }
                unum_close(unum);
            } else {
                log_data_err("unexpected error in unum_open UNUM_CURRENCY_PLURAL for locale %s: '%s'\n", itemPtr->locale, u_errorName(status));
            }
        }
    }
}

typedef struct {
    const char *  testname;
    const char *  locale;
#if APPLE_ICU_CHANGES
// rdar://
    UBool         lenient;
#endif  // APPLE_ICU_CHANGES
    const UChar * source;
    int32_t       startPos;
    int32_t       value;
    int32_t       endPos;
    UErrorCode    status;
#if APPLE_ICU_CHANGES
// rdar://
} NumParseTestItem;
#else
} SpelloutParseTest;
#endif  // APPLE_ICU_CHANGES

static const UChar ustr_en0[]   = {0x7A, 0x65, 0x72, 0x6F, 0}; /* zero */
static const UChar ustr_123[]   = {0x31, 0x32, 0x33, 0};       /* 123 */
static const UChar ustr_en123[] = {0x6f, 0x6e, 0x65, 0x20, 0x68, 0x75, 0x6e, 0x64, 0x72, 0x65, 0x64,
                                   0x20, 0x74, 0x77, 0x65, 0x6e, 0x74, 0x79,
                                   0x2d, 0x74, 0x68, 0x72, 0x65, 0x65, 0}; /* one hundred twenty-three */
static const UChar ustr_fr123[] = {0x63, 0x65, 0x6e, 0x74, 0x20, 0x76, 0x69, 0x6e, 0x67, 0x74, 0x2d,
                                   0x74, 0x72, 0x6f, 0x69, 0x73, 0};       /* cent vingt-trois */
static const UChar ustr_ja123[] = {0x767e, 0x4e8c, 0x5341, 0x4e09, 0};     /* kanji 100(+)2(*)10(+)3 */
#if APPLE_ICU_CHANGES
// rdar://
static const UChar ustr_zh50s[] = u"五十"; /* 4E94 5341 spellout 50 */
static const UChar ustr_zh50d[] = u"五〇"; /* 4E94 3007 decimal 50 */
static const UChar ustr_zh05a[] = u"零五"; /* 96F6 4E94 decimal-alt 05 */
static const UChar ustr_zh05d[] = u"〇五"; /* 3007 4E94 decimal 05 */
#endif  // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://
#define		NUMERIC_STRINGS_NOT_PARSEABLE	1	//	ticket/8224

static const NumParseTestItem spelloutParseTests[] = {
    /* name        loc                   lenent src       start val  end status */
    { "en0",       "en",                 false, ustr_en0,    0,   0,  4, U_ZERO_ERROR },
    { "en0",       "en",                 false, ustr_en0,    2,   0,  2, U_PARSE_ERROR },
    { "en0",       "ja",                 false, ustr_en0,    0,   0,  0, U_PARSE_ERROR },
#if NUMERIC_STRINGS_NOT_PARSEABLE
    { "123",       "en",                 false, ustr_123,    0,   0,  0, U_PARSE_ERROR },
#else
    { "123",       "en",                 false, ustr_123,    0, 123,  3, U_ZERO_ERROR },
#endif
    { "123L",      "en",                 true,  ustr_123,    0, 123,  3, U_ZERO_ERROR },
    { "en123",     "en",                 false, ustr_en123,  0, 123, 24, U_ZERO_ERROR },
    { "en123",     "en",                 false, ustr_en123, 12,  23, 24, U_ZERO_ERROR },
    { "en123",     "fr",                 false, ustr_en123, 16,   0, 16, U_PARSE_ERROR },
    { "fr123",     "fr",                 false, ustr_fr123,  0, 123, 16, U_ZERO_ERROR },
    { "fr123",     "fr",                 false, ustr_fr123,  5,  23, 16, U_ZERO_ERROR },
    { "fr123",     "en",                 false, ustr_fr123,  0,   0,  0, U_PARSE_ERROR },
    { "ja123",     "ja",                 false, ustr_ja123,  0, 123,  4, U_ZERO_ERROR },
    { "ja123",     "ja",                 false, ustr_ja123,  1,  23,  4, U_ZERO_ERROR },
    { "ja123",     "fr",                 false, ustr_ja123,  0,   0,  0, U_PARSE_ERROR },
    { "zh,50s",    "zh",                 false, ustr_zh50s,  0,  50,  2, U_ZERO_ERROR },
    // from ICU50m2, NUMERIC_STRINGS_NOT_PARSEABLE no longer affects the next three
    { "zh@hd,50d", "zh@numbers=hanidec", false, ustr_zh50d,  0,  50,  2, U_ZERO_ERROR },
    { "zh@hd,05a", "zh@numbers=hanidec", false, ustr_zh05a,  0,   5,  2, U_ZERO_ERROR },
    { "zh@hd,05d", "zh@numbers=hanidec", false, ustr_zh05d,  0,   5,  2, U_ZERO_ERROR },
    { "zh@hd,50dL","zh@numbers=hanidec", true,  ustr_zh50d,  0,  50,  2, U_ZERO_ERROR },
    { "zh@hd,05aL","zh@numbers=hanidec", true,  ustr_zh05a,  0,   5,  2, U_ZERO_ERROR },
    { "zh@hd,05dL","zh@numbers=hanidec", true,  ustr_zh05d,  0,   5,  2, U_ZERO_ERROR },
    { "zh,50dL",   "zh",                 true,  ustr_zh50d,  0,  50,  2, U_ZERO_ERROR }, /* changed in ICU50m2 */
    { "zh,05aL",   "zh",                 true,  ustr_zh05a,  0,   0,  1, U_ZERO_ERROR },
    { "zh,05dL",   "zh",                 true,  ustr_zh05d,  0,   5,  2, U_ZERO_ERROR }, /* changed in ICU50m2 */
    { NULL,        NULL,                 false, NULL,        0,   0,  0, 0 } /* terminator */
};
#else
static const SpelloutParseTest spelloutParseTests[] = {
    /* name    loc   src       start val  end status */
    { "en0",   "en", ustr_en0,    0,   0,  4, U_ZERO_ERROR },
    { "en0",   "en", ustr_en0,    2,   0,  2, U_PARSE_ERROR },
    { "en0",   "ja", ustr_en0,    0,   0,  0, U_PARSE_ERROR },
    { "123",   "en", ustr_123,    0, 123,  3, U_ZERO_ERROR },
    { "en123", "en", ustr_en123,  0, 123, 24, U_ZERO_ERROR },
    { "en123", "en", ustr_en123, 12,  23, 24, U_ZERO_ERROR },
    { "en123", "fr", ustr_en123, 16,   0, 16, U_PARSE_ERROR },
    { "fr123", "fr", ustr_fr123,  0, 123, 16, U_ZERO_ERROR },
    { "fr123", "fr", ustr_fr123,  5,  23, 16, U_ZERO_ERROR },
    { "fr123", "en", ustr_fr123,  0,   0,  0, U_PARSE_ERROR },
    { "ja123", "ja", ustr_ja123,  0, 123,  4, U_ZERO_ERROR },
    { "ja123", "ja", ustr_ja123,  1,  23,  4, U_ZERO_ERROR },
    { "ja123", "fr", ustr_ja123,  0,   0,  0, U_PARSE_ERROR },
    { NULL,    NULL, NULL,        0,   0,  0, 0 } /* terminator */
};
#endif  // APPLE_ICU_CHANGES

static void TestSpelloutNumberParse()
{
#if APPLE_ICU_CHANGES
// rdar://
    const NumParseTestItem * testPtr;
#else
    const SpelloutParseTest * testPtr;
#endif  // APPLE_ICU_CHANGES
    for (testPtr = spelloutParseTests; testPtr->testname != NULL; ++testPtr) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t value, position = testPtr->startPos;
        UNumberFormat *nf = unum_open(UNUM_SPELLOUT, NULL, 0, testPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "unum_open fails for UNUM_SPELLOUT with locale %s, status %s\n", testPtr->locale, myErrorName(status));
            continue;
        }
#if APPLE_ICU_CHANGES
// rdar://
        unum_setAttribute(nf, UNUM_LENIENT_PARSE, testPtr->lenient);
#endif  // APPLE_ICU_CHANGES
        status = U_ZERO_ERROR;
        value = unum_parse(nf, testPtr->source, -1, &position, &status);
        if ( value != testPtr->value || position != testPtr->endPos || status != testPtr->status ) {
            log_err("unum_parse SPELLOUT, locale %s, testname %s, startPos %d: for value / endPos / status, expected %d / %d / %s, got %d / %d / %s\n",
                    testPtr->locale, testPtr->testname, testPtr->startPos,
                    testPtr->value, testPtr->endPos, myErrorName(testPtr->status),
                    value, position, myErrorName(status) );
        }
        unum_close(nf);
    }
}

static void TestSignificantDigits()
{
    UChar temp[128];
    int32_t resultlengthneeded;
    int32_t resultlength;
    UErrorCode status = U_ZERO_ERROR;
    UChar *result = NULL;
    UNumberFormat* fmt;
    double d = 123456.789;

    u_uastrcpy(temp, "###0.0#");
    fmt=unum_open(UNUM_IGNORE, temp, -1, "en", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("got unexpected error for unum_open: '%s'\n", u_errorName(status));
        return;
    }
    unum_setAttribute(fmt, UNUM_SIGNIFICANT_DIGITS_USED, true);
    unum_setAttribute(fmt, UNUM_MAX_SIGNIFICANT_DIGITS, 6);

    u_uastrcpy(temp, "123457");
    resultlength=0;
    resultlengthneeded=unum_formatDouble(fmt, d, NULL, resultlength, NULL, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        unum_formatDouble(fmt, d, result, resultlength, NULL, &status);
    }
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_formatDouble(.....): %s\n", myErrorName(status));
        return;
    }
    if(u_strcmp(result, temp)==0)
        log_verbose("Pass: Number Formatting using unum_formatDouble() Successful\n");
    else
        log_err("FAIL: Error in number formatting using unum_formatDouble()\n");
    free(result);
    unum_close(fmt);
}

static void TestSigDigRounding()
{
    UErrorCode status = U_ZERO_ERROR;
    UChar expected[128];
    UChar result[128];
    char  temp1[128];
    char  temp2[128];
    UNumberFormat* fmt;
    double d = 123.4;

    fmt=unum_open(UNUM_DECIMAL, NULL, 0, "en", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("got unexpected error for unum_open: '%s'\n", u_errorName(status));
        return;
    }
    unum_setAttribute(fmt, UNUM_LENIENT_PARSE, false);
    unum_setAttribute(fmt, UNUM_SIGNIFICANT_DIGITS_USED, true);
    unum_setAttribute(fmt, UNUM_MAX_SIGNIFICANT_DIGITS, 2);
    /* unum_setAttribute(fmt, UNUM_MAX_FRACTION_DIGITS, 0); */

    unum_setAttribute(fmt, UNUM_ROUNDING_MODE, UNUM_ROUND_UP);
    unum_setDoubleAttribute(fmt, UNUM_ROUNDING_INCREMENT, 20.0);

    (void)unum_formatDouble(fmt, d, result, UPRV_LENGTHOF(result), NULL, &status);
    if(U_FAILURE(status))
    {
        log_err("Error in formatting using unum_formatDouble(.....): %s\n", myErrorName(status));
        return;
    }

    u_uastrcpy(expected, "140");
    if(u_strcmp(result, expected)!=0)
        log_err("FAIL: Error in unum_formatDouble result %s instead of %s\n", u_austrcpy(temp1, result), u_austrcpy(temp2, expected) );

    unum_close(fmt);
}

static void TestNumberFormatPadding()
{
    UChar *result=NULL;
    UChar temp1[512];
    UChar temp2[512];

    UErrorCode status=U_ZERO_ERROR;
    int32_t resultlength;
    int32_t resultlengthneeded;
    UNumberFormat *pattern;
    double d1;
    double d = -10456.37;
    UFieldPosition pos1;
    int32_t parsepos;

    /* create a number format using unum_openPattern(....)*/
    log_verbose("\nTesting unum_openPattern() with padding\n");
    u_uastrcpy(temp1, "*#,##0.0#*;(#,##0.0#)");
    status=U_ZERO_ERROR;
    pattern=unum_open(UNUM_IGNORE,temp1, u_strlen(temp1), NULL, NULL,&status);
    if(U_SUCCESS(status))
    {
        log_err("error in unum_openPattern(%s): %s\n", temp1, myErrorName(status) );
    }
    else
    {
        unum_close(pattern);
    }

/*    u_uastrcpy(temp1, "*x#,###,###,##0.0#;(*x#,###,###,##0.0#)"); */
    u_uastrcpy(temp1, "*x#,###,###,##0.0#;*x(###,###,##0.0#)"); // input pattern
    u_uastrcpy(temp2, "*x#########,##0.0#;(#########,##0.0#)"); // equivalent (?) output pattern
    status=U_ZERO_ERROR;
    pattern=unum_open(UNUM_IGNORE,temp1, u_strlen(temp1), "en_US",NULL, &status);
    if(U_FAILURE(status))
    {
        log_err_status(status, "error in padding unum_openPattern(%s): %s\n", temp1, myErrorName(status) );
    }
    else {
        log_verbose("Pass: padding unum_openPattern() works fine\n");

        /*test for unum_toPattern()*/
        log_verbose("\nTesting padding unum_toPattern()\n");
        resultlength=0;
        resultlengthneeded=unum_toPattern(pattern, false, NULL, resultlength, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            resultlength=resultlengthneeded+1;
            result=(UChar*)malloc(sizeof(UChar) * resultlength);
            unum_toPattern(pattern, false, result, resultlength, &status);
        }
        if(U_FAILURE(status))
        {
            log_err("error in extracting the padding pattern from UNumberFormat: %s\n", myErrorName(status));
        }
        else
        {
            if(u_strncmp(result, temp2, resultlengthneeded)!=0) {
                log_err(
                        "FAIL: Error in extracting the padding pattern using unum_toPattern(): %d: %s != %s\n",
                        resultlengthneeded,
                        austrdup(temp2),
                        austrdup(result));
            } else {
                log_verbose("Pass: extracted the padding pattern correctly using unum_toPattern()\n");
            }
        }
        free(result);
/*        u_uastrcpy(temp1, "(xxxxxxx10,456.37)"); */
        u_uastrcpy(temp1, "xxxxx(10,456.37)");
        resultlength=0;
        pos1.field = UNUM_FRACTION_FIELD;
        resultlengthneeded=unum_formatDouble(pattern, d, NULL, resultlength, &pos1, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            resultlength=resultlengthneeded+1;
            result=(UChar*)malloc(sizeof(UChar) * resultlength);
            unum_formatDouble(pattern, d, result, resultlength, NULL, &status);
        }
        if(U_FAILURE(status))
        {
            log_err("Error in formatting using unum_formatDouble(.....) with padding : %s\n", myErrorName(status));
        }
        else
        {
            if(u_strcmp(result, temp1)==0)
                log_verbose("Pass: Number Formatting using unum_formatDouble() padding Successful\n");
            else
                log_data_err("FAIL: Error in number formatting using unum_formatDouble() with padding\n");
            if(pos1.beginIndex == 13 && pos1.endIndex == 15)
                log_verbose("Pass: Complete number formatting using unum_formatDouble() successful\n");
            else
                log_err("Fail: Error in complete number Formatting using unum_formatDouble()\nGot: b=%d end=%d\nExpected: b=13 end=15\n",
                        pos1.beginIndex, pos1.endIndex);


            /* Testing unum_parse() and unum_parseDouble() */
            log_verbose("\nTesting padding unum_parseDouble()\n");
            parsepos=0;
            d1=unum_parseDouble(pattern, result, u_strlen(result), &parsepos, &status);
            if(U_FAILURE(status))
            {
                log_err("padding parse failed. The error is : %s\n", myErrorName(status));
            }

            if(d1!=d)
                log_err("Fail: Error in padding parsing\n");
            else
                log_verbose("Pass: padding parsing successful\n");
free(result);
        }
    }

    unum_close(pattern);
}

static UBool
withinErr(double a, double b, double err) {
    return uprv_fabs(a - b) < uprv_fabs(a * err);
}

static void TestInt64Format() {
    UChar temp1[512];
    UChar result[512];
    UNumberFormat *fmt;
    UErrorCode status = U_ZERO_ERROR;
    const double doubleInt64Max = (double)U_INT64_MAX;
    const double doubleInt64Min = (double)U_INT64_MIN;
    const double doubleBig = 10.0 * (double)U_INT64_MAX;
    int32_t val32;
    int64_t val64;
    double  valDouble;
    int32_t parsepos;

    /* create a number format using unum_openPattern(....) */
    log_verbose("\nTesting Int64Format\n");
    u_uastrcpy(temp1, "#.#E0");
    fmt = unum_open(UNUM_IGNORE, temp1, u_strlen(temp1), "en_US", NULL, &status);
    if(U_FAILURE(status)) {
        log_data_err("error in unum_openPattern() - %s\n", myErrorName(status));
    } else {
        unum_setAttribute(fmt, UNUM_MAX_FRACTION_DIGITS, 20);
#if APPLE_ICU_CHANGES
// rdar://
        unum_setAttribute(fmt, UNUM_FORMAT_WITH_FULL_PRECISION, true); // Needed due to ICU68 additions for Apple change rdar://39240173
#endif  // APPLE_ICU_CHANGES
        unum_formatInt64(fmt, U_INT64_MAX, result, 512, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("error in unum_format(): %s\n", myErrorName(status));
        } else {
            log_verbose("format int64max: '%s'\n", result);
            parsepos = 0;
            val32 = unum_parse(fmt, result, u_strlen(result), &parsepos, &status);
            if (status != U_INVALID_FORMAT_ERROR) {
                log_err("parse didn't report error: %s\n", myErrorName(status));
            } else if (val32 != INT32_MAX) {
                log_err("parse didn't pin return value, got: %d\n", val32);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            val64 = unum_parseInt64(fmt, result, u_strlen(result), &parsepos, &status);
            if (U_FAILURE(status)) {
                log_err("parseInt64 returned error: %s\n", myErrorName(status));
            } else if (val64 != U_INT64_MAX) {
                log_err("parseInt64 returned incorrect value, got: %ld\n", val64);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            valDouble = unum_parseDouble(fmt, result, u_strlen(result), &parsepos, &status);
            if (U_FAILURE(status)) {
                log_err("parseDouble returned error: %s\n", myErrorName(status));
            } else if (valDouble != doubleInt64Max) {
                log_err("parseDouble returned incorrect value, got: %g\n", valDouble);
            }
        }

        unum_formatInt64(fmt, U_INT64_MIN, result, 512, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("error in unum_format(): %s\n", myErrorName(status));
        } else {
            log_verbose("format int64min: '%s'\n", result);
            parsepos = 0;
            val32 = unum_parse(fmt, result, u_strlen(result), &parsepos, &status);
            if (status != U_INVALID_FORMAT_ERROR) {
                log_err("parse didn't report error: %s\n", myErrorName(status));
            } else if (val32 != INT32_MIN) {
                log_err("parse didn't pin return value, got: %d\n", val32);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            val64 = unum_parseInt64(fmt, result, u_strlen(result), &parsepos, &status);
            if (U_FAILURE(status)) {
                log_err("parseInt64 returned error: %s\n", myErrorName(status));
            } else if (val64 != U_INT64_MIN) {
                log_err("parseInt64 returned incorrect value, got: %ld\n", val64);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            valDouble = unum_parseDouble(fmt, result, u_strlen(result), &parsepos, &status);
            if (U_FAILURE(status)) {
                log_err("parseDouble returned error: %s\n", myErrorName(status));
            } else if (valDouble != doubleInt64Min) {
                log_err("parseDouble returned incorrect value, got: %g\n", valDouble);
            }
        }

        unum_formatDouble(fmt, doubleBig, result, 512, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("error in unum_format(): %s\n", myErrorName(status));
        } else {
            log_verbose("format doubleBig: '%s'\n", result);
            parsepos = 0;
            val32 = unum_parse(fmt, result, u_strlen(result), &parsepos, &status);
            if (status != U_INVALID_FORMAT_ERROR) {
                log_err("parse didn't report error: %s\n", myErrorName(status));
            } else if (val32 != INT32_MAX) {
                log_err("parse didn't pin return value, got: %d\n", val32);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            val64 = unum_parseInt64(fmt, result, u_strlen(result), &parsepos, &status);
            if (status != U_INVALID_FORMAT_ERROR) {
                log_err("parseInt64 didn't report error error: %s\n", myErrorName(status));
            } else if (val64 != U_INT64_MAX) {
                log_err("parseInt64 returned incorrect value, got: %ld\n", val64);
            }

            status = U_ZERO_ERROR;
            parsepos = 0;
            valDouble = unum_parseDouble(fmt, result, u_strlen(result), &parsepos, &status);
            if (U_FAILURE(status)) {
                log_err("parseDouble returned error: %s\n", myErrorName(status));
            } else if (!withinErr(valDouble, doubleBig, 1e-15)) {
                log_err("parseDouble returned incorrect value, got: %g\n", valDouble);
            }
        }

        u_uastrcpy(result, "5.06e-27");
        parsepos = 0;
        valDouble = unum_parseDouble(fmt, result, u_strlen(result), &parsepos, &status);
        if (U_FAILURE(status)) {
            log_err("parseDouble() returned error: %s\n", myErrorName(status));
        } else if (!withinErr(valDouble, 5.06e-27, 1e-15)) {
            log_err("parseDouble() returned incorrect value, got: %g\n", valDouble);
        }
    }
    unum_close(fmt);
}


static void test_fmt(UNumberFormat* fmt, UBool isDecimal) {
    char temp[512];
    UChar buffer[512];
    int32_t BUFSIZE = UPRV_LENGTHOF(buffer);
    double vals[] = {
        -.2, 0, .2, 5.5, 15.2, 250, 123456789
    };
    int i;

    for (i = 0; i < UPRV_LENGTHOF(vals); ++i) {
        UErrorCode status = U_ZERO_ERROR;
        unum_formatDouble(fmt, vals[i], buffer, BUFSIZE, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("failed to format: %g, returned %s\n", vals[i], u_errorName(status));
        } else {
            u_austrcpy(temp, buffer);
            log_verbose("formatting %g returned '%s'\n", vals[i], temp);
        }
    }

    /* check APIs now */
    {
        UErrorCode status = U_ZERO_ERROR;
        UParseError perr;
        u_uastrcpy(buffer, "#,##0.0#");
        unum_applyPattern(fmt, false, buffer, -1, &perr, &status);
        if (isDecimal ? U_FAILURE(status) : (status != U_UNSUPPORTED_ERROR)) {
            log_err("got unexpected error for applyPattern: '%s'\n", u_errorName(status));
        }
    }

    {
        int isLenient = unum_getAttribute(fmt, UNUM_LENIENT_PARSE);
        log_verbose("lenient: 0x%x\n", isLenient);
        if (isLenient != false) {
            log_err("didn't expect lenient value: %d\n", isLenient);
        }

        unum_setAttribute(fmt, UNUM_LENIENT_PARSE, true);
        isLenient = unum_getAttribute(fmt, UNUM_LENIENT_PARSE);
        if (isLenient != true) {
            log_err("didn't expect lenient value after set: %d\n", isLenient);
        }
    }

    {
        double val2;
        double val = unum_getDoubleAttribute(fmt, UNUM_LENIENT_PARSE);
        if (val != -1) {
            log_err("didn't expect double attribute\n");
        }
        val = unum_getDoubleAttribute(fmt, UNUM_ROUNDING_INCREMENT);
        if ((val == -1) == isDecimal) {
            log_err("didn't expect -1 rounding increment\n");
        }
        unum_setDoubleAttribute(fmt, UNUM_ROUNDING_INCREMENT, val+.5);
        val2 = unum_getDoubleAttribute(fmt, UNUM_ROUNDING_INCREMENT);
        if (isDecimal && (val2 - val != .5)) {
            log_err("set rounding increment had no effect on decimal format");
        }
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        int len = unum_getTextAttribute(fmt, UNUM_DEFAULT_RULESET, buffer, BUFSIZE, &status);
        if (isDecimal ? (status != U_UNSUPPORTED_ERROR) : U_FAILURE(status)) {
            log_err("got unexpected error for get default ruleset: '%s'\n", u_errorName(status));
        }
        if (U_SUCCESS(status)) {
            u_austrcpy(temp, buffer);
            log_verbose("default ruleset: '%s'\n", temp);
        }

        status = U_ZERO_ERROR;
        len = unum_getTextAttribute(fmt, UNUM_PUBLIC_RULESETS, buffer, BUFSIZE, &status);
        if (isDecimal ? (status != U_UNSUPPORTED_ERROR) : U_FAILURE(status)) {
            log_err("got unexpected error for get public rulesets: '%s'\n", u_errorName(status));
        }
        if (U_SUCCESS(status)) {
            u_austrcpy(temp, buffer);
            log_verbose("public rulesets: '%s'\n", temp);

            /* set the default ruleset to the first one found, and retry */

            if (len > 0) {
                for (i = 0; i < len && temp[i] != ';'; ++i){}
                if (i < len) {
                    buffer[i] = 0;
                    unum_setTextAttribute(fmt, UNUM_DEFAULT_RULESET, buffer, -1, &status);
                    if (U_FAILURE(status)) {
                        log_err("unexpected error setting default ruleset: '%s'\n", u_errorName(status));
                    } else {
                        int len2 = unum_getTextAttribute(fmt, UNUM_DEFAULT_RULESET, buffer, BUFSIZE, &status);
                        if (U_FAILURE(status)) {
                            log_err("could not fetch default ruleset: '%s'\n", u_errorName(status));
                        } else if (len2 != i) {
                            u_austrcpy(temp, buffer);
                            log_err("unexpected ruleset len: %d ex: %d val: %s\n", len2, i, temp);
                        } else {
                            for (i = 0; i < UPRV_LENGTHOF(vals); ++i) {
                                status = U_ZERO_ERROR;
                                unum_formatDouble(fmt, vals[i], buffer, BUFSIZE, NULL, &status);
                                if (U_FAILURE(status)) {
                                    log_err("failed to format: %g, returned %s\n", vals[i], u_errorName(status));
                                } else {
                                    u_austrcpy(temp, buffer);
                                    log_verbose("formatting %g returned '%s'\n", vals[i], temp);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        unum_toPattern(fmt, false, buffer, BUFSIZE, &status);
        if (U_SUCCESS(status)) {
            u_austrcpy(temp, buffer);
            log_verbose("pattern: '%s'\n", temp);
        } else if (status != U_BUFFER_OVERFLOW_ERROR) {
            log_err("toPattern failed unexpectedly: %s\n", u_errorName(status));
        } else {
            log_verbose("pattern too long to display\n");
        }
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        int len = unum_getSymbol(fmt, UNUM_CURRENCY_SYMBOL, buffer, BUFSIZE, &status);
        if (isDecimal ? U_FAILURE(status) : (status != U_UNSUPPORTED_ERROR)) {
            log_err("unexpected error getting symbol: '%s'\n", u_errorName(status));
        }

        unum_setSymbol(fmt, UNUM_CURRENCY_SYMBOL, buffer, len, &status);
        if (isDecimal ? U_FAILURE(status) : (status != U_UNSUPPORTED_ERROR)) {
            log_err("unexpected error setting symbol: '%s'\n", u_errorName(status));
        }
    }
}

static void TestNonExistentCurrency() {
    UNumberFormat *format;
    UErrorCode status = U_ZERO_ERROR;
    UChar currencySymbol[8];
    static const UChar QQQ[] = {0x51, 0x51, 0x51, 0};

    /* Get a non-existent currency and make sure it returns the correct currency code. */
    format = unum_open(UNUM_CURRENCY, NULL, 0, "th_TH@currency=QQQ", NULL, &status);
    if (format == NULL || U_FAILURE(status)) {
        log_data_err("unum_open did not return expected result for non-existent requested currency: '%s' (Are you missing data?)\n", u_errorName(status));
    }
    else {
        unum_getSymbol(format,
                UNUM_CURRENCY_SYMBOL,
                currencySymbol,
                UPRV_LENGTHOF(currencySymbol),
                &status);
        if (u_strcmp(currencySymbol, QQQ) != 0) {
            log_err("unum_open set the currency to QQQ\n");
        }
    }
    unum_close(format);
}

static void TestRBNFFormat() {
    UErrorCode status;
    UParseError perr;
    UChar pat[1024];
    UChar tempUChars[512];
    UNumberFormat *formats[4];
    int COUNT = UPRV_LENGTHOF(formats);
    int i;

    for (i = 0; i < COUNT; ++i) {
        formats[i] = 0;
    }

    /* instantiation */
    status = U_ZERO_ERROR;
    u_uastrcpy(pat, "#,##0.0#;(#,##0.0#)");
    formats[0] = unum_open(UNUM_PATTERN_DECIMAL, pat, -1, "en_US", &perr, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "unable to open decimal pattern -> %s\n", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR;
    formats[1] = unum_open(UNUM_SPELLOUT, NULL, 0, "en_US", &perr, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "unable to open spellout -> %s\n", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR;
    formats[2] = unum_open(UNUM_ORDINAL, NULL, 0, "en_US", &perr, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "unable to open ordinal -> %s\n", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR;
    u_uastrcpy(pat,
        "%standard:\n"
        "-x: minus >>;\n"
        "x.x: << point >>;\n"
        "zero; one; two; three; four; five; six; seven; eight; nine;\n"
        "ten; eleven; twelve; thirteen; fourteen; fifteen; sixteen;\n"
        "seventeen; eighteen; nineteen;\n"
        "20: twenty[->>];\n"
        "30: thirty[->>];\n"
        "40: forty[->>];\n"
        "50: fifty[->>];\n"
        "60: sixty[->>];\n"
        "70: seventy[->>];\n"
        "80: eighty[->>];\n"
        "90: ninety[->>];\n"
        "100: =#,##0=;\n");
    u_uastrcpy(tempUChars,
        "%simple:\n"
        "=%standard=;\n"
        "20: twenty[ and change];\n"
        "30: thirty[ and change];\n"
        "40: forty[ and change];\n"
        "50: fifty[ and change];\n"
        "60: sixty[ and change];\n"
        "70: seventy[ and change];\n"
        "80: eighty[ and change];\n"
        "90: ninety[ and change];\n"
        "100: =#,##0=;\n"
        "%bogus:\n"
        "0.x: tiny;\n"
        "x.x: << point something;\n"
        "=%standard=;\n"
        "20: some reasonable number;\n"
        "100: some substantial number;\n"
        "100,000,000: some huge number;\n");
    /* This is to get around some compiler warnings about char * string length. */
    u_strcat(pat, tempUChars);
    formats[3] = unum_open(UNUM_PATTERN_RULEBASED, pat, -1, "en_US", &perr, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "unable to open rulebased pattern -> %s\n", u_errorName(status));
    }
    if (U_FAILURE(status)) {
        log_err_status(status, "Something failed with %s\n", u_errorName(status));
        return;
    }

    for (i = 0; i < COUNT; ++i) {
        log_verbose("\n\ntesting format %d\n", i);
        test_fmt(formats[i], (UBool)(i == 0));
    }

    #define FORMAT_BUF_CAPACITY 64
    {
        UChar fmtbuf[FORMAT_BUF_CAPACITY];
        int32_t len;
        double nanvalue = uprv_getNaN();
        status = U_ZERO_ERROR;
        len = unum_formatDouble(formats[1], nanvalue, fmtbuf, FORMAT_BUF_CAPACITY, NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "unum_formatDouble NAN failed with %s\n", u_errorName(status));
        } else {
            UChar nansym[] = { 0x4E, 0x61, 0x4E, 0 }; /* NaN */
            if ( len != 3 || u_strcmp(fmtbuf, nansym) != 0 ) {
                log_err("unum_formatDouble NAN produced wrong answer for en_US\n");
            }
        }
    }

    for (i = 0; i < COUNT; ++i) {
        unum_close(formats[i]);
    }
}

static void TestRBNFRounding() {
    UChar fmtbuf[FORMAT_BUF_CAPACITY];
    UChar expectedBuf[FORMAT_BUF_CAPACITY];
    int32_t len;
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* fmt = unum_open(UNUM_SPELLOUT, NULL, 0, "en_US", NULL, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "unable to open spellout -> %s\n", u_errorName(status));
        return;
    }
    len = unum_formatDouble(fmt, 10.123456789, fmtbuf, FORMAT_BUF_CAPACITY, NULL, &status);
    U_ASSERT(len < FORMAT_BUF_CAPACITY);
    (void)len;
    if (U_FAILURE(status)) {
        log_err_status(status, "unum_formatDouble 10.123456789 failed with %s\n", u_errorName(status));
    }
    u_uastrcpy(expectedBuf, "ten point one two three four five six seven eight nine");
    if (u_strcmp(expectedBuf, fmtbuf) != 0) {
        log_err("Wrong result for unrounded value\n");
    }
    unum_setAttribute(fmt, UNUM_MAX_FRACTION_DIGITS, 3);
    if (unum_getAttribute(fmt, UNUM_MAX_FRACTION_DIGITS) != 3) {
        log_err("UNUM_MAX_FRACTION_DIGITS was incorrectly ignored -> %d\n", unum_getAttribute(fmt, UNUM_MAX_FRACTION_DIGITS));
    }
    if (unum_getAttribute(fmt, UNUM_ROUNDING_MODE) != UNUM_ROUND_UNNECESSARY) {
        log_err("UNUM_ROUNDING_MODE was set -> %d\n", unum_getAttribute(fmt, UNUM_ROUNDING_MODE));
    }
    unum_setAttribute(fmt, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
    if (unum_getAttribute(fmt, UNUM_ROUNDING_MODE) != UNUM_ROUND_HALFUP) {
        log_err("UNUM_ROUNDING_MODE was not set -> %d\n", unum_getAttribute(fmt, UNUM_ROUNDING_MODE));
    }
    len = unum_formatDouble(fmt, 10.123456789, fmtbuf, FORMAT_BUF_CAPACITY, NULL, &status);
    U_ASSERT(len < FORMAT_BUF_CAPACITY);
    if (U_FAILURE(status)) {
        log_err_status(status, "unum_formatDouble 10.123456789 failed with %s\n", u_errorName(status));
    }
    u_uastrcpy(expectedBuf, "ten point one two three");
    if (u_strcmp(expectedBuf, fmtbuf) != 0) {
        char temp[512];
        u_austrcpy(temp, fmtbuf);
        log_err("Wrong result for rounded value. Got: %s\n", temp);
    }
    unum_close(fmt);
}

static void TestCurrencyRegression(void) {
/*
 I've found a case where unum_parseDoubleCurrency is not doing what I
expect.  The value I pass in is $1234567890q123460000.00 and this
returns with a status of zero error & a parse pos of 22 (I would
expect a parse error at position 11).

I stepped into DecimalFormat::subparse() and it looks like it parses
the first 10 digits and then stops parsing at the q but doesn't set an
error. Then later in DecimalFormat::parse() the value gets crammed
into a long (which greatly truncates the value).

This is very problematic for me 'cause I try to remove chars that are
invalid but this allows my users to enter bad chars and truncates
their data!
*/

    UChar buf[1024];
    UChar currency[8];
    char acurrency[16];
    double d;
    UNumberFormat *cur;
    int32_t pos;
    UErrorCode status  = U_ZERO_ERROR;
    const int32_t expected = 11;

    currency[0]=0;
    u_uastrcpy(buf, "$1234567890q643210000.00");
    cur = unum_open(UNUM_CURRENCY, NULL,0,"en_US", NULL, &status);

    if(U_FAILURE(status)) {
        log_data_err("unum_open failed: %s (Are you missing data?)\n", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR; /* so we can test it later. */
    pos = 0;

    d = unum_parseDoubleCurrency(cur,
                         buf,
                         -1,
                         &pos, /* 0 = start */
                         currency,
                         &status);

    u_austrcpy(acurrency, currency);

    if(U_FAILURE(status) || (pos != expected)) {
        log_err("unum_parseDoubleCurrency should have failed with pos %d, but gave: value %.9f, err %s, pos=%d, currency [%s]\n",
            expected, d, u_errorName(status), pos, acurrency);
    } else {
        log_verbose("unum_parseDoubleCurrency failed, value %.9f err %s, pos %d, currency [%s]\n", d, u_errorName(status), pos, acurrency);
    }

    unum_close(cur);
}

static void TestTextAttributeCrash(void) {
    UChar ubuffer[64] = {0x0049,0x004E,0x0052,0};
    static const UChar expectedNeg[] = {0x0049,0x004E,0x0052,0x0031,0x0032,0x0033,0x0034,0x002E,0x0035,0};
    static const UChar expectedPos[] = {0x0031,0x0032,0x0033,0x0034,0x002E,0x0035,0};
    int32_t used;
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *nf = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("FAILED 1 -> %s (Are you missing data?)\n", u_errorName(status));
        return;
    }
    unum_setTextAttribute(nf, UNUM_CURRENCY_CODE, ubuffer, 3, &status);
    /*
     * the usual negative prefix and suffix seem to be '($' and ')' at this point
     * also crashes if UNUM_NEGATIVE_SUFFIX is substituted for UNUM_NEGATIVE_PREFIX here
     */
    used = unum_getTextAttribute(nf, UNUM_NEGATIVE_PREFIX, ubuffer, 64, &status);
    unum_setTextAttribute(nf, UNUM_NEGATIVE_PREFIX, ubuffer, used, &status);
    if (U_FAILURE(status)) {
        log_err("FAILED 2\n"); exit(1);
    }
    log_verbose("attempting to format...\n");
    used = unum_formatDouble(nf, -1234.5, ubuffer, 64, NULL, &status);
    if (U_FAILURE(status) || 64 < used) {
        log_err("Failed formatting %s\n", u_errorName(status));
        return;
    }
    if (u_strcmp(expectedNeg, ubuffer) == 0) {
        log_err("Didn't get expected negative result\n");
    }
    used = unum_formatDouble(nf, 1234.5, ubuffer, 64, NULL, &status);
    if (U_FAILURE(status) || 64 < used) {
        log_err("Failed formatting %s\n", u_errorName(status));
        return;
    }
    if (u_strcmp(expectedPos, ubuffer) == 0) {
        log_err("Didn't get expected positive result\n");
    }
    unum_close(nf);
}

static void TestNBSPPatternRtNum(const char *testcase, int line, UNumberFormat *nf, double myNumber) {
    UErrorCode status = U_ZERO_ERROR;
    UChar myString[20];
    char tmpbuf[200];
    double aNumber = -1.0;
    unum_formatDouble(nf, myNumber, myString, 20, NULL, &status);
    log_verbose("%s:%d: formatted %.2f into %s\n", testcase, line, myNumber, u_austrcpy(tmpbuf, myString));
    if(U_FAILURE(status)) {
      log_err("%s:%d: failed format of %.2g with %s\n", testcase, line, myNumber, u_errorName(status));
        return;
    }
    aNumber = unum_parse(nf, myString, -1, NULL, &status);
    if(U_FAILURE(status)) {
      log_err("%s:%d: failed parse with %s\n", testcase, line, u_errorName(status));
        return;
    }
    if(uprv_fabs(aNumber-myNumber)>.001) {
      log_err("FAIL: %s:%d formatted %.2f, parsed into %.2f\n", testcase, line, myNumber, aNumber);
    } else {
      log_verbose("PASS: %s:%d formatted %.2f, parsed into %.2f\n", testcase, line, myNumber, aNumber);
    }
}

static void TestNBSPPatternRT(const char *testcase, UNumberFormat *nf) {
  TestNBSPPatternRtNum(testcase, __LINE__, nf, 12345.);
  TestNBSPPatternRtNum(testcase, __LINE__, nf, -12345.);
}

static void TestNBSPInPattern(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* nf = NULL;
    const char *testcase;


    testcase="ar_AE UNUM_CURRENCY";
    nf  = unum_open(UNUM_CURRENCY, NULL, -1, "ar_AE", NULL, &status);
    if(U_FAILURE(status) || nf == NULL) {
      log_data_err("%s:%d: %s: unum_open failed with %s (Are you missing data?)\n", __FILE__, __LINE__, testcase, u_errorName(status));
        return;
    }
    TestNBSPPatternRT(testcase, nf);

    /* if we don't have CLDR 1.6 data, bring out the problem anyways */
    {
#define SPECIAL_PATTERN "\\u00A4\\u00A4'\\u062f.\\u0625.\\u200f\\u00a0'###0.00"
        UChar pat[200];
        testcase = "ar_AE special pattern: " SPECIAL_PATTERN;
        u_unescape(SPECIAL_PATTERN, pat, UPRV_LENGTHOF(pat));
        unum_applyPattern(nf, false, pat, -1, NULL, &status);
        if(U_FAILURE(status)) {
            log_err("%s: unum_applyPattern failed with %s\n", testcase, u_errorName(status));
        } else {
            TestNBSPPatternRT(testcase, nf);
        }
#undef SPECIAL_PATTERN
    }
    unum_close(nf); status = U_ZERO_ERROR;

    testcase="ar_AE UNUM_DECIMAL";
    nf  = unum_open(UNUM_DECIMAL, NULL, -1, "ar_AE", NULL, &status);
    if(U_FAILURE(status)) {
        log_err("%s: unum_open failed with %s\n", testcase, u_errorName(status));
    }
    TestNBSPPatternRT(testcase, nf);
    unum_close(nf); status = U_ZERO_ERROR;

    testcase="ar_AE UNUM_PERCENT";
    nf  = unum_open(UNUM_PERCENT, NULL, -1, "ar_AE", NULL, &status);
    if(U_FAILURE(status)) {
        log_err("%s: unum_open failed with %s\n", testcase, u_errorName(status));
    }
    TestNBSPPatternRT(testcase, nf);
    unum_close(nf); status = U_ZERO_ERROR;



}
static void TestCloneWithRBNF(void) {
    UChar pattern[1024];
    UChar pat2[512];
    UErrorCode status = U_ZERO_ERROR;
    UChar buffer[256];
    UChar buffer_cloned[256];
    char temp1[256];
    char temp2[256];
    UNumberFormat *pform_cloned;
    UNumberFormat *pform;

    u_uastrcpy(pattern,
        "%main:\n"
        "0.x: >%%millis-only>;\n"
        "x.0: <%%duration<;\n"
        "x.x: <%%durationwithmillis<>%%millis-added>;\n"
        "-x: ->>;%%millis-only:\n"
        "1000: 00:00.<%%millis<;\n"
        "%%millis-added:\n"
        "1000: .<%%millis<;\n"
        "%%millis:\n"
        "0: =000=;\n"
        "%%duration:\n"
        "0: =%%seconds-only=;\n"
        "60: =%%min-sec=;\n"
        "3600: =%%hr-min-sec=;\n"
        "86400/86400: <%%ddaayyss<[, >>];\n"
        "%%durationwithmillis:\n"
        "0: =%%seconds-only=;\n"
        "60: =%%min-sec=;\n"
        "3600: =%%hr-min-sec=;\n"
        "86400/86400: <%%ddaayyss<, >>;\n");
    u_uastrcpy(pat2,
        "%%seconds-only:\n"
        "0: 0:00:=00=;\n"
        "%%min-sec:\n"
        "0: :=00=;\n"
        "0/60: 0:<00<>>;\n"
        "%%hr-min-sec:\n"
        "0: :=00=;\n"
        "60/60: <00<>>;\n"
        "3600/60: <0<:>>>;\n"
        "%%ddaayyss:\n"
        "0 days;\n"
        "1 day;\n"
        "=0= days;");

    /* This is to get around some compiler warnings about char * string length. */
    u_strcat(pattern, pat2);

    pform = unum_open(UNUM_PATTERN_RULEBASED, pattern, -1, "en_US", NULL, &status);
    unum_formatDouble(pform, 3600, buffer, 256, NULL, &status);

    pform_cloned = unum_clone(pform,&status);
    unum_formatDouble(pform_cloned, 3600, buffer_cloned, 256, NULL, &status);

    unum_close(pform);
    unum_close(pform_cloned);

    if (u_strcmp(buffer,buffer_cloned)) {
        log_data_err("Result from cloned formatter not identical to the original. Original: %s Cloned: %s - (Are you missing data?)",u_austrcpy(temp1, buffer),u_austrcpy(temp2,buffer_cloned));
    }
}


static void TestNoExponent(void) {
    UErrorCode status = U_ZERO_ERROR;
    UChar str[100];
    const char *cstr;
    UNumberFormat *fmt;
    int32_t pos;
    int32_t expect = 0;
    int32_t num;

    fmt = unum_open(UNUM_DECIMAL, NULL, -1, "en_US", NULL, &status);

    if(U_FAILURE(status) || fmt == NULL) {
        log_data_err("%s:%d: unum_open failed with %s (Are you missing data?)\n", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    cstr = "10E6";
    u_uastrcpy(str, cstr);
    expect = 10000000;
    pos = 0;
    num = unum_parse(fmt, str, -1, &pos, &status);
    ASSERT_TRUE(pos==4);
    if(U_FAILURE(status)) {
        log_data_err("%s:%d: unum_parse failed with %s for %s (Are you missing data?)\n", __FILE__, __LINE__, u_errorName(status), cstr);
    } else if(expect!=num) {
        log_data_err("%s:%d: unum_parse failed, got %d expected %d for '%s'(Are you missing data?)\n", __FILE__, __LINE__, num, expect, cstr);
    } else {
        log_verbose("%s:%d: unum_parse returned %d for '%s'\n", __FILE__, __LINE__, num, cstr);
    }

    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_PARSE_NO_EXPONENT)==0);

    unum_setAttribute(fmt, UNUM_PARSE_NO_EXPONENT, 1); /* no error code */
    log_verbose("set UNUM_PARSE_NO_EXPONENT\n");

    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_PARSE_NO_EXPONENT)==1);

    pos = 0;
    expect=10;
    num = unum_parse(fmt, str, -1, &pos, &status);
    if(num==10000000) {
        log_err("%s:%d: FAIL: unum_parse should have returned 10, not 10000000 on %s after UNUM_PARSE_NO_EXPONENT\n", __FILE__, __LINE__, cstr);
    } else if(num==expect) {
        log_verbose("%s:%d: unum_parse gave %d for %s - good.\n", __FILE__, __LINE__, num, cstr);
    }
    ASSERT_TRUE(pos==2);

    status = U_ZERO_ERROR;

    unum_close(fmt);

    /* ok, now try scientific */
    fmt = unum_open(UNUM_SCIENTIFIC, NULL, -1, "en_US", NULL, &status);
    assertSuccess("unum_open(UNUM_SCIENTIFIC, ...)", &status);

    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_PARSE_NO_EXPONENT)==0);

    cstr = "10E6";
    u_uastrcpy(str, cstr);
    expect = 10000000;
    pos = 0;
    num = unum_parse(fmt, str, -1, &pos, &status);
    ASSERT_TRUE(pos==4);
    if(U_FAILURE(status)) {
        log_data_err("%s:%d: unum_parse failed with %s for %s (Are you missing data?)\n", __FILE__, __LINE__, u_errorName(status), cstr);
    } else if(expect!=num) {
        log_data_err("%s:%d: unum_parse failed, got %d expected %d for '%s'(Are you missing data?)\n", __FILE__, __LINE__, num, expect, cstr);
    } else {
        log_verbose("%s:%d: unum_parse returned %d for '%s'\n", __FILE__, __LINE__, num, cstr);
    }

    unum_setAttribute(fmt, UNUM_PARSE_NO_EXPONENT, 1); /* no error code */
    log_verbose("set UNUM_PARSE_NO_EXPONENT\n");

    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_PARSE_NO_EXPONENT)==1);

    // A scientific formatter should parse the exponent even if UNUM_PARSE_NO_EXPONENT is set
    cstr = "10E6";
    u_uastrcpy(str, cstr);
    expect = 10000000;
    pos = 0;
    num = unum_parse(fmt, str, -1, &pos, &status);
    ASSERT_TRUE(pos==4);
    if(U_FAILURE(status)) {
        log_data_err("%s:%d: unum_parse failed with %s for %s (Are you missing data?)\n", __FILE__, __LINE__, u_errorName(status), cstr);
    } else if(expect!=num) {
        log_data_err("%s:%d: unum_parse failed, got %d expected %d for '%s'(Are you missing data?)\n", __FILE__, __LINE__, num, expect, cstr);
    } else {
        log_verbose("%s:%d: unum_parse returned %d for '%s'\n", __FILE__, __LINE__, num, cstr);
    }

    unum_close(fmt);
}

static void TestMaxInt(void) {
    UErrorCode status = U_ZERO_ERROR;
    UChar pattern_hash[] = { 0x23, 0x00 }; /* "#" */
    UChar result1[1024] = { 0 }, result2[1024] = { 0 };
    int32_t len1, len2;
    UChar expect[] = { 0x0039, 0x0037, 0 };
    UNumberFormat *fmt = unum_open(
                  UNUM_PATTERN_DECIMAL,      /* style         */
                  &pattern_hash[0],          /* pattern       */
                  u_strlen(pattern_hash),    /* patternLength */
                  "en",
                  0,                         /* parseErr      */
                  &status);
    if(U_FAILURE(status) || fmt == NULL) {
        log_data_err("%s:%d: %s: unum_open failed with %s (Are you missing data?)\n", __FILE__, __LINE__, "TestMaxInt", u_errorName(status));
        return;
    }

    unum_setAttribute(fmt, UNUM_MAX_INTEGER_DIGITS, 2);

    status = U_ZERO_ERROR;
    /* #1 */
    len1 = unum_formatInt64(fmt, 1997, result1, 1024, NULL, &status);
    result1[len1]=0;
    if(U_FAILURE(status) || u_strcmp(expect, result1)) {
        log_err("unum_formatInt64 Expected %s but got %s status %s\n", austrdup(expect), austrdup(result1), u_errorName(status));
    }

    status = U_ZERO_ERROR;
    /* #2 */
    len2 = unum_formatDouble(fmt, 1997.0, result2, 1024, NULL, &status);
    result2[len2]=0;
    if(U_FAILURE(status) || u_strcmp(expect, result2)) {
        log_err("unum_formatDouble Expected %s but got %s status %s\n", austrdup(expect), austrdup(result2), u_errorName(status));
    }



    /* test UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS */
    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS)==0);

    unum_setAttribute(fmt, UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS, 1);
    /* test UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS */
    ASSERT_TRUE(unum_getAttribute(fmt, UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS)==1);

    status = U_ZERO_ERROR;
    /* max int digits still '2' */
    len1 = unum_formatInt64(fmt, 1997, result1, 1024, NULL, &status);
    ASSERT_TRUE(status==U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;

    /* But, formatting 97->'97' works fine. */

    /* #1 */
    len1 = unum_formatInt64(fmt, 97, result1, 1024, NULL, &status);
    result1[len1]=0;
    if(U_FAILURE(status) || u_strcmp(expect, result1)) {
        log_err("unum_formatInt64 Expected %s but got %s status %s\n", austrdup(expect), austrdup(result1), u_errorName(status));
    }

    status = U_ZERO_ERROR;
    /* #2 */
    len2 = unum_formatDouble(fmt, 97.0, result2, 1024, NULL, &status);
    result2[len2]=0;
    if(U_FAILURE(status) || u_strcmp(expect, result2)) {
        log_err("unum_formatDouble Expected %s but got %s status %s\n", austrdup(expect), austrdup(result2), u_errorName(status));
    }


    unum_close(fmt);
}

static void TestSignAlwaysShown(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *fmt = unum_open(
                  UNUM_DECIMAL, /* style         */
                  NULL,         /* pattern       */
                  0,            /* patternLength */
                  "en-US",
                  NULL,         /* parseErr      */
                  &status);
    assertSuccess("Creating UNumberFormat", &status);
    unum_setAttribute(fmt, UNUM_SIGN_ALWAYS_SHOWN, 1);
    UChar result[100];
    unum_formatDouble(fmt, 42, result, 100, NULL, &status);
    assertSuccess("Formatting with UNumberFormat", &status);
    assertUEquals("Result with sign always shown", u"+42", result);
    unum_close(fmt);
}

static void TestMinimumGroupingDigits(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *fmt = unum_open(
                  UNUM_DECIMAL, /* style         */
                  NULL,         /* pattern       */
                  0,            /* patternLength */
                  "en-US",
                  NULL,         /* parseErr      */
                  &status);
    assertSuccess("Creating UNumberFormat", &status);
    unum_setAttribute(fmt, UNUM_MINIMUM_GROUPING_DIGITS, 2);
    UChar result[100];
    unum_formatDouble(fmt, 1234, result, 100, NULL, &status);
    assertSuccess("Formatting with UNumberFormat A", &status);
    assertUEquals("Result with minimum grouping digits A", u"1234", result);
    unum_formatDouble(fmt, 12345, result, 100, NULL, &status);
    assertSuccess("Formatting with UNumberFormat B", &status);
    assertUEquals("Result with minimum grouping digits B", u"12,345", result);
    unum_close(fmt);
}

static void TestParseCaseSensitive(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *fmt = unum_open(
                  UNUM_DECIMAL, /* style         */
                  NULL,         /* pattern       */
                  0,            /* patternLength */
                  "en-US",
                  NULL,         /* parseErr      */
                  &status);
    assertSuccess("Creating UNumberFormat", &status);
    double result = unum_parseDouble(fmt, u"1e2", -1, NULL, &status);
    assertSuccess("Parsing with UNumberFormat, case insensitive", &status);
    assertIntEquals("Result with case sensitive", 100, (int64_t)result);
    unum_setAttribute(fmt, UNUM_PARSE_CASE_SENSITIVE, 1);
    int32_t ppos = 0;
    result = unum_parseDouble(fmt, u"1e2", -1, &ppos, &status);
    assertSuccess("Parsing with UNumberFormat, case sensitive", &status);
    assertIntEquals("Position with case sensitive", 1, ppos);
    assertIntEquals("Result with case sensitive", 1, (int64_t)result);
    unum_close(fmt);
}

static void TestUFormattable(void) {
  UChar out2k[2048];
  // simple test for API docs
  {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *unum = unum_open(UNUM_DEFAULT, NULL, -1, "en_US_POSIX", NULL, &status);
    if(assertSuccessCheck("calling unum_open()", &status, true)) {
      //! [unum_parseToUFormattable]
      const UChar str[] = { 0x0031, 0x0032, 0x0033, 0x0000 }; /* 123 */
      int32_t result = 0;
      UFormattable *ufmt = ufmt_open(&status);
      unum_parseToUFormattable(unum, ufmt, str, -1, NULL, &status);
      if (ufmt_isNumeric(ufmt)) {
          result = ufmt_getLong(ufmt, &status); /* == 123 */
      } /* else { ... } */
      ufmt_close(ufmt);
      //! [unum_parseToUFormattable]
      assertTrue("result == 123", (result == 123));
    }
    unum_close(unum);
  }
  // test with explicitly created ufmt_open
  {
    UChar buffer[2048];
    UErrorCode status = U_ZERO_ERROR;
    UFormattable *ufmt;
    UNumberFormat *unum;
    const char *pattern = "";

    ufmt = ufmt_open(&status);
    unum = unum_open(UNUM_DEFAULT, NULL, -1, "en_US_POSIX", NULL, &status);
    if(assertSuccessCheck("calling ufmt_open() || unum_open()", &status, true)) {

      pattern = "31337";
      log_verbose("-- pattern: %s\n", pattern);
      u_uastrcpy(buffer, pattern);
      unum_parseToUFormattable(unum, ufmt, buffer, -1, NULL, &status);
      if(assertSuccess("unum_parseToUFormattable(31337)", &status)) {
        assertTrue("ufmt_getLong()=31337", ufmt_getLong(ufmt, &status) == 31337);
        assertTrue("ufmt_getType()=UFMT_LONG", ufmt_getType(ufmt, &status) == UFMT_LONG);
        log_verbose("long = %d\n", ufmt_getLong(ufmt, &status));
        assertSuccess("ufmt_getLong()", &status);
      }
      unum_formatUFormattable(unum, ufmt, out2k, 2048, NULL, &status);
      if(assertSuccess("unum_formatUFormattable(31337)", &status)) {
        assertEquals("unum_formatUFormattable r/t", austrdup(buffer), austrdup(out2k));
      }

      pattern = "3.14159";
      log_verbose("-- pattern: %s\n", pattern);
      u_uastrcpy(buffer, pattern);
      unum_parseToUFormattable(unum, ufmt, buffer, -1, NULL, &status);
      if(assertSuccess("unum_parseToUFormattable(3.14159)", &status)) {
        assertTrue("ufmt_getDouble()==3.14159", withinErr(ufmt_getDouble(ufmt, &status), 3.14159, 1e-15));
        assertSuccess("ufmt_getDouble()", &status);
        assertTrue("ufmt_getType()=UFMT_DOUBLE", ufmt_getType(ufmt, &status) == UFMT_DOUBLE);
        log_verbose("double = %g\n", ufmt_getDouble(ufmt, &status));
      }
      unum_formatUFormattable(unum, ufmt, out2k, 2048, NULL, &status);
      if(assertSuccess("unum_formatUFormattable(3.14159)", &status)) {
        assertEquals("unum_formatUFormattable r/t", austrdup(buffer), austrdup(out2k));
      }
    }
    ufmt_close(ufmt);
    unum_close(unum);
  }

  // test with auto-generated ufmt
  {
    UChar buffer[2048];
    UErrorCode status = U_ZERO_ERROR;
    UFormattable *ufmt = NULL;
    UNumberFormat *unum;
    const char *pattern = "73476730924573500000000"; // weight of the moon, kg

    log_verbose("-- pattern: %s (testing auto-opened UFormattable)\n", pattern);
    u_uastrcpy(buffer, pattern);

    unum = unum_open(UNUM_DEFAULT, NULL, -1, "en_US_POSIX", NULL, &status);
    if(assertSuccessCheck("calling unum_open()", &status, true)) {

      ufmt = unum_parseToUFormattable(unum, NULL, /* will be ufmt_open()'ed for us */
                                   buffer, -1, NULL, &status);
      if(assertSuccess("unum_parseToUFormattable(weight of the moon)", &status)) {
        log_verbose("new formattable allocated at %p\n", (void*)ufmt);
        assertTrue("ufmt_isNumeric() true", ufmt_isNumeric(ufmt));
        unum_formatUFormattable(unum, ufmt, out2k, 2048, NULL, &status);
        if(assertSuccess("unum_formatUFormattable(3.14159)", &status)) {
          assertEquals("unum_formatUFormattable r/t", austrdup(buffer), austrdup(out2k));
        }

        log_verbose("double: %g\n",  ufmt_getDouble(ufmt, &status));
        assertSuccess("ufmt_getDouble()", &status);

        log_verbose("long: %ld\n", ufmt_getLong(ufmt, &status));
        assertTrue("failure on ufmt_getLong() for huge number:", U_FAILURE(status));
        // status is now a failure due to ufmt_getLong() above.
        // the intltest does extensive r/t testing of Formattable vs. UFormattable.
      }
    }

    unum_close(unum);
    ufmt_close(ufmt); // was implicitly opened for us by the first unum_parseToUFormattable()
  }
}

typedef struct {
    const char*  locale;
    const char*  numsys;
    int32_t      radix;
    UBool        isAlgorithmic;
    const UChar* description;
} NumSysTestItem;


static const UChar latnDesc[]    = {0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0}; // 0123456789
static const UChar romanDesc[]   = {0x25,0x72,0x6F,0x6D,0x61,0x6E,0x2D,0x75,0x70,0x70,0x65,0x72,0}; // %roman-upper
static const UChar arabDesc[]    = {0x0660,0x0661,0x0662,0x0663,0x0664,0x0665,0x0666,0x0667,0x0668,0x0669,0}; //
static const UChar arabextDesc[] = {0x06F0,0x06F1,0x06F2,0x06F3,0x06F4,0x06F5,0x06F6,0x06F7,0x06F8,0x06F9,0}; //
static const UChar hanidecDesc[] = {0x3007,0x4E00,0x4E8C,0x4E09,0x56DB,0x4E94,0x516D,0x4E03,0x516B,0x4E5D,0}; //
static const UChar hantDesc[]    = {0x7A,0x68,0x5F,0x48,0x61,0x6E,0x74,0x2F,0x53,0x70,0x65,0x6C,0x6C,0x6F,0x75,0x74,
                                    0x52,0x75,0x6C,0x65,0x73,0x2F,0x25,0x73,0x70,0x65,0x6C,0x6C,0x6F,0x75,0x74,0x2D,
                                    0x63,0x61,0x72,0x64,0x69,0x6E,0x61,0x6C,0}; // zh_Hant/SpelloutRules/%spellout-cardinal

static const NumSysTestItem numSysTestItems[] = {
    //locale                         numsys    radix isAlgo  description
    { "en",                          "latn",    10,  false,  latnDesc },
    { "en@numbers=roman",            "roman",   10,  true,   romanDesc },
    { "en@numbers=finance",          "latn",    10,  false,  latnDesc },
    { "ar-EG",                       "arab",    10,  false,  arabDesc },
#if APPLE_ICU_CHANGES
// some extra tests for rdar://116185298 (D74/21C15: Arabic numerals (1,2,3..) should be set as default when language of device is changed to Arabic instead of Arabic-Indic numerals (٣،٢،١))
    { "ar-SA",                       "arab",    10,  false,  arabDesc },
    { "ar-AE",                       "latn",    10,  false,  latnDesc },
    { "ar-GB",                       "latn",    10,  false,  latnDesc },
#endif
    { "fa",                          "arabext", 10,  false,  arabextDesc },
    { "zh_Hans@numbers=hanidec",     "hanidec", 10,  false,  hanidecDesc },
    { "zh_Hant@numbers=traditional", "hant",    10,  true,   hantDesc },
    { NULL,                          NULL,       0,  false,  NULL },
};
enum { kNumSysDescripBufMax = 64 };

static void TestUNumberingSystem(void) {
    const NumSysTestItem * itemPtr;
    UNumberingSystem * unumsys;
    UEnumeration * uenum;
    const char * numsys;
    UErrorCode status;

    for (itemPtr = numSysTestItems; itemPtr->locale != NULL; itemPtr++) {
        status = U_ZERO_ERROR;
        unumsys = unumsys_open(itemPtr->locale, &status);
        if ( U_SUCCESS(status) ) {
            UChar ubuf[kNumSysDescripBufMax];
            int32_t ulen, radix = unumsys_getRadix(unumsys);
            UBool isAlgorithmic = unumsys_isAlgorithmic(unumsys);
            numsys = unumsys_getName(unumsys);
            if ( uprv_strcmp(numsys, itemPtr->numsys) != 0 || radix != itemPtr->radix || !isAlgorithmic != !itemPtr->isAlgorithmic ) {
                log_data_err("unumsys name/radix/isAlgorithmic for locale %s, expected %s/%d/%d, got %s/%d/%d\n",
                        itemPtr->locale, itemPtr->numsys, itemPtr->radix, itemPtr->isAlgorithmic, numsys, radix, isAlgorithmic);
            }
            ulen = unumsys_getDescription(unumsys, ubuf, kNumSysDescripBufMax, &status);
            (void)ulen;   // Suppress variable not used warning.
            if ( U_FAILURE(status) || u_strcmp(ubuf, itemPtr->description) != 0 ) {
                log_data_err("unumsys description for locale %s, description unexpected and/or status %\n", myErrorName(status));
            }
            unumsys_close(unumsys);
        } else {
            log_data_err("unumsys_open for locale %s fails with status %s\n", itemPtr->locale, myErrorName(status));
        }
    }

    for (int i=0; i<3; ++i) {
        // Run the test of unumsys_openAvailableNames() multiple times.
        // Helps verify the management of the internal cache of the names.
        status = U_ZERO_ERROR;
        uenum = unumsys_openAvailableNames(&status);
        if ( U_SUCCESS(status) ) {
            int32_t numsysCount = 0;
            // sanity check for a couple of number systems that must be in the enumeration
            UBool foundLatn = false;
            UBool foundArab = false;
            while ( (numsys = uenum_next(uenum, NULL, &status)) != NULL && U_SUCCESS(status) ) {
                status = U_ZERO_ERROR;
                unumsys = unumsys_openByName(numsys, &status);
                if ( U_SUCCESS(status) ) {
                    numsysCount++;
                    if ( uprv_strcmp(numsys, "latn") ) foundLatn = true;
                    if ( uprv_strcmp(numsys, "arab") ) foundArab = true;
                    unumsys_close(unumsys);
                } else {
                    log_err("unumsys_openAvailableNames includes %s but unumsys_openByName on it fails with status %s\n",
                            numsys, myErrorName(status));
                }
            }
            uenum_close(uenum);
            if ( numsysCount < 40 || !foundLatn || !foundArab ) {
                log_err("unumsys_openAvailableNames results incomplete: numsysCount %d, foundLatn %d, foundArab %d\n",
                        numsysCount, foundLatn, foundArab);
            }
        } else {
            log_data_err("unumsys_openAvailableNames fails with status %s\n", myErrorName(status));
        }
    }
}

/* plain-C version of test in numfmtst.cpp */
#if APPLE_ICU_CHANGES
// rdar://
enum { kUBufMax = 64, kBBufMax = 128 };
#else
enum { kUBufMax = 64 };
#endif  // APPLE_ICU_CHANGES
static void TestCurrencyIsoPluralFormat(void) {
    static const char* DATA[][8] = {
        // the data are:
        // locale,
        // currency amount to be formatted,
        // currency ISO code to be formatted,
        // format result using CURRENCYSTYLE,
        // format result using CURRENCY_STANDARD,
        // format result using CURRENCY_ACCOUNTING,
        // format result using ISOCURRENCYSTYLE,
        // format result using PLURALCURRENCYSTYLE,

        // locale             amount     ISOcode CURRENCYSTYLE         CURRENCY_STANDARD     CURRENCY_ACCOUNTING   ISOCURRENCYSTYLE          PLURALCURRENCYSTYLE
        {"en_US",             "1",        "USD", "$1.00",              "$1.00",              "$1.00",              "USD\\u00A01.00",        "1.00 US dollars"},
        {"en_US",             "1234.56",  "USD", "$1,234.56",          "$1,234.56",          "$1,234.56",          "USD\\u00A01,234.56",    "1,234.56 US dollars"},
        {"en_US@cf=account",  "1234.56",  "USD", "$1,234.56",          "$1,234.56",          "$1,234.56",          "USD\\u00A01,234.56",    "1,234.56 US dollars"},
        {"en_US",             "-1234.56", "USD", "-$1,234.56",         "-$1,234.56",         "($1,234.56)",        "-USD\\u00A01,234.56",   "-1,234.56 US dollars"},
        {"en_US@cf=account",  "-1234.56", "USD", "($1,234.56)",        "-$1,234.56",         "($1,234.56)",        "-USD\\u00A01,234.56",   "-1,234.56 US dollars"},
        {"en_US@cf=standard", "-1234.56", "USD", "-$1,234.56",         "-$1,234.56",         "($1,234.56)",        "-USD\\u00A01,234.56",   "-1,234.56 US dollars"},
        {"zh_CN",             "1",        "USD", "US$1.00",            "US$1.00",            "US$1.00",            "USD\\u00A01.00",        "1.00\\u00A0\\u7F8E\\u5143"},
        {"zh_CN",             "-1",       "USD", "-US$1.00",           "-US$1.00",           "(US$1.00)",          "-USD\\u00A01.00",       "-1.00\\u00A0\\u7F8E\\u5143"},
        {"zh_CN@cf=account",  "-1",       "USD", "(US$1.00)",          "-US$1.00",           "(US$1.00)",          "-USD\\u00A01.00",       "-1.00\\u00A0\\u7F8E\\u5143"},
        {"zh_CN@cf=standard", "-1",       "USD", "-US$1.00",           "-US$1.00",           "(US$1.00)",          "-USD\\u00A01.00",       "-1.00\\u00A0\\u7F8E\\u5143"},
        {"zh_CN",             "1234.56",  "USD", "US$1,234.56",        "US$1,234.56",        "US$1,234.56",        "USD\\u00A01,234.56",    "1,234.56\\u00A0\\u7F8E\\u5143"},
        // {"zh_CN",          "1",        "CHY", "CHY1.00",            "CHY1.00",            "CHY1.00",            "CHY1.00",        "1.00 CHY"}, // wrong ISO code
        // {"zh_CN",          "1234.56",  "CHY", "CHY1,234.56",        "CHY1,234.56",        "CHY1,234.56",        "CHY1,234.56",    "1,234.56 CHY"}, // wrong ISO code
        {"zh_CN",             "1",        "CNY", "\\u00A51.00",        "\\u00A51.00",        "\\u00A51.00",        "CNY\\u00A01.00",        "1.00\\u00A0\\u4EBA\\u6C11\\u5E01"},
        {"zh_CN",             "1234.56",  "CNY", "\\u00A51,234.56",    "\\u00A51,234.56",    "\\u00A51,234.56",    "CNY\\u00A01,234.56",    "1,234.56\\u00A0\\u4EBA\\u6C11\\u5E01"},
        {"ru_RU",             "1",        "RUB", "1,00\\u00A0\\u20BD", "1,00\\u00A0\\u20BD", "1,00\\u00A0\\u20BD", "1,00\\u00A0RUB",        "1,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E "
                                                                                                                                            "\\u0440\\u0443\\u0431\\u043B\\u044F"},
        {"ru_RU",             "2",        "RUB", "2,00\\u00A0\\u20BD", "2,00\\u00A0\\u20BD", "2,00\\u00A0\\u20BD", "2,00\\u00A0RUB",        "2,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E "
                                                                                                                                            "\\u0440\\u0443\\u0431\\u043B\\u044F"},
        {"ru_RU",             "5",        "RUB", "5,00\\u00A0\\u20BD", "5,00\\u00A0\\u20BD", "5,00\\u00A0\\u20BD", "5,00\\u00A0RUB",        "5,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E "
                                                                                                                                            "\\u0440\\u0443\\u0431\\u043B\\u044F"},
#if APPLE_ICU_CHANGES
// rdar://
        {"ja_JP",             "42",       NULL,  "\\u00A542",          "\\u00A542",          "\\u00A542",          "JPY\\u00A042",          "42\\u00A0\\u5186"},
        {"ja_JP@currency=USD", "42",      NULL,  "$42.00",             "$42.00",             "$42.00",             "USD\\u00A042.00",       "42.00\\u00A0\\u7C73\\u30C9\\u30EB"},
        {"ms_MY",             "1234.56",  "MYR", "RM1,234.56",         "RM1,234.56",         "RM1,234.56",         "MYR1,234.56",           "1,234.56 Ringgit Malaysia"},
        {"id_ID",             "1234.56",  "IDR", "Rp1.235",            "Rp1.235",            "Rp1.235",            "IDR\\u00A01.235",       "1.235 Rupiah Indonesia"},
#endif  // APPLE_ICU_CHANGES
        // test locale without currency information
        {"root",              "-1.23",    "USD", "-US$\\u00A01.23",    "-US$\\u00A01.23",    "-US$\\u00A01.23",    "-USD\\u00A01.23", "-1.23 USD"},
        {"root@cf=account",   "-1.23",    "USD", "-US$\\u00A01.23",    "-US$\\u00A01.23",    "-US$\\u00A01.23",    "-USD\\u00A01.23", "-1.23 USD"},
        // test choice format
        {"es_AR",             "1",        "INR", "INR\\u00A01,00",     "INR\\u00A01,00",     "INR\\u00A01,00",     "INR\\u00A01,00",  "1,00 rupia india"},
#if APPLE_ICU_CHANGES
// rdar://
         // test EUR format for some now-redundant locale data removed in rdar://62544359
        {"en_NO",             "1234.56",  "EUR", "\\u20AC\\u00A01\\u00A0234,56", "\\u20AC\\u00A01\\u00A0234,56", "\\u20AC\\u00A01\\u00A0234,56", "EUR\\u00A01\\u00A0234,56", "1\\u00A0234,56 euros"},
        {"en_PL",             "1234.56",  "EUR", "1\\u00A0234,56\\u00A0\\u20AC", "1\\u00A0234,56\\u00A0\\u20AC", "1\\u00A0234,56\\u00A0\\u20AC", "1\\u00A0234,56\\u00A0EUR", "1\\u00A0234,56 euros"},
#endif  // APPLE_ICU_CHANGES
    };
    static const UNumberFormatStyle currencyStyles[] = {
        UNUM_CURRENCY,
        UNUM_CURRENCY_STANDARD,
        UNUM_CURRENCY_ACCOUNTING,
        UNUM_CURRENCY_ISO,
        UNUM_CURRENCY_PLURAL
    };

    int32_t i, sIndex;

    for (i=0; i<UPRV_LENGTHOF(DATA); ++i) {
      const char* localeString = DATA[i][0];
      double numberToBeFormat = atof(DATA[i][1]);
      const char* currencyISOCode = DATA[i][2];
      for (sIndex = 0; sIndex < UPRV_LENGTHOF(currencyStyles); ++sIndex) {
        UNumberFormatStyle style = currencyStyles[sIndex];
        UErrorCode status = U_ZERO_ERROR;
#if APPLE_ICU_CHANGES
// rdar://
        UChar currencyCode[4] = {0};
#else
        UChar currencyCode[4];
#endif  // APPLE_ICU_CHANGES
        UChar ubufResult[kUBufMax];
        UChar ubufExpected[kUBufMax];
        int32_t ulenRes;
#if APPLE_ICU_CHANGES
// rdar://
        const char* currencyISOCodeForLog = currencyISOCode;
#endif  // APPLE_ICU_CHANGES

        UNumberFormat* unumFmt = unum_open(style, NULL, 0, localeString, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: unum_open, locale %s, style %d - %s\n", localeString, (int)style, myErrorName(status));
            continue;
        }
#if APPLE_ICU_CHANGES
// rdar://
        if (currencyISOCode != NULL) {
            u_charsToUChars(currencyISOCode, currencyCode, 4);
            unum_setTextAttribute(unumFmt, UNUM_CURRENCY_CODE, currencyCode, 3, &status);
        } else {
            unum_setTextAttribute(unumFmt, UNUM_CURRENCY_CODE, NULL, 0, &status);
            currencyISOCodeForLog = "(null)";
        }
#else
        u_charsToUChars(currencyISOCode, currencyCode, 4);
        unum_setTextAttribute(unumFmt, UNUM_CURRENCY_CODE, currencyCode, 3, &status);
#endif  // APPLE_ICU_CHANGES
        if (U_FAILURE(status)) {
#if APPLE_ICU_CHANGES
// rdar://
            log_err("FAIL: unum_setTextAttribute, locale %s, UNUM_CURRENCY_CODE %s: %s\n", localeString, currencyISOCodeForLog, myErrorName(status));
#else
            log_err("FAIL: unum_setTextAttribute, locale %s, UNUM_CURRENCY_CODE %s\n", localeString, currencyISOCode);
#endif  // APPLE_ICU_CHANGES
        }
        ulenRes = unum_formatDouble(unumFmt, numberToBeFormat, ubufResult, kUBufMax, NULL, &status);
        if (U_FAILURE(status)) {
#if APPLE_ICU_CHANGES
// rdar://
            log_err("FAIL: unum_formatDouble, locale %s, UNUM_CURRENCY_CODE %s: %s\n", localeString, currencyISOCodeForLog, myErrorName(status));
#else
            log_err("FAIL: unum_formatDouble, locale %s, UNUM_CURRENCY_CODE %s - %s\n", localeString, currencyISOCode, myErrorName(status));
#endif  // APPLE_ICU_CHANGES
        } else {
            int32_t ulenExp = u_unescape(DATA[i][3 + sIndex], ubufExpected, kUBufMax);
            if (ulenRes != ulenExp || u_strncmp(ubufResult, ubufExpected, ulenExp) != 0) {
#if APPLE_ICU_CHANGES
// rdar://
                char bbufExpected[kBBufMax];
                char bbufResult[kBBufMax];
                u_strToUTF8(bbufExpected, kBBufMax, NULL, ubufExpected, ulenExp, &status);
                u_strToUTF8(bbufResult,   kBBufMax, NULL, ubufResult,   ulenRes, &status);
                log_err("FAIL: unum_formatDouble, locale %s, UNUM_CURRENCY_CODE %s, expected %s, got %s\n",
                        localeString, currencyISOCodeForLog, bbufExpected, bbufResult);
#else
                log_err("FAIL: unum_formatDouble, locale %s, UNUM_CURRENCY_CODE %s, expected %s, got something else\n",
                        localeString, currencyISOCode, DATA[i][3 + sIndex]);
#endif  // APPLE_ICU_CHANGES
            }
        }
        unum_close(unumFmt);
      }
    }
}

typedef struct {
    const char * locale;
    UNumberFormatStyle style;
    UDisplayContext context;
    const char * expectedResult;
} TestContextItem;

/* currently no locales have contextTransforms data for "symbol" type */
static const TestContextItem tcItems[] = { /* results for 123.45 */
    { "sv", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    "ett\\u00ADhundra\\u00ADtjugo\\u00ADtre komma fyra fem" },
    { "sv", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, "Ett\\u00ADhundra\\u00ADtjugo\\u00ADtre komma fyra fem" },
    { "sv", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       "ett\\u00ADhundra\\u00ADtjugo\\u00ADtre komma fyra fem" },
    { "sv", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            "ett\\u00ADhundra\\u00ADtjugo\\u00ADtre komma fyra fem" },
    { "en", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    "one hundred twenty-three point four five" },
    { "en", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, "One hundred twenty-three point four five" },
    { "en", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       "One hundred twenty-three point four five" },
    { "en", UNUM_SPELLOUT, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            "One hundred twenty-three point four five" },
    { NULL, (UNumberFormatStyle)0, (UDisplayContext)0, NULL }
};

static void TestContext(void) {
    UErrorCode status = U_ZERO_ERROR;
    const TestContextItem* itemPtr;

    UNumberFormat *unum = unum_open(UNUM_SPELLOUT, NULL, 0, "en", NULL, &status);
    if ( U_SUCCESS(status) ) {
        UDisplayContext context = unum_getContext(unum, UDISPCTX_TYPE_CAPITALIZATION, &status);
        if ( U_FAILURE(status) || context != UDISPCTX_CAPITALIZATION_NONE) {
            log_err("FAIL: Initial unum_getContext is not UDISPCTX_CAPITALIZATION_NONE\n");
            status = U_ZERO_ERROR;
        }
        unum_setContext(unum, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);
        context = unum_getContext(unum, UDISPCTX_TYPE_CAPITALIZATION, &status);
        if ( U_FAILURE(status) || context != UDISPCTX_CAPITALIZATION_FOR_STANDALONE) {
            log_err("FAIL: unum_getContext does not return the value set, UDISPCTX_CAPITALIZATION_FOR_STANDALONE\n");
        }
        unum_close(unum);
    } else {
        log_data_err("unum_open UNUM_SPELLOUT for en fails with status %s\n", myErrorName(status));
    }
#if !UCONFIG_NO_NORMALIZATION && !UCONFIG_NO_BREAK_ITERATION
    for (itemPtr = tcItems; itemPtr->locale != NULL; itemPtr++) {
        UChar ubufResult[kUBufMax];
        int32_t ulenRes;

        status = U_ZERO_ERROR;
        unum = unum_open(itemPtr->style, NULL, 0, itemPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: unum_open, locale %s, style %d - %s\n",
                        itemPtr->locale, (int)itemPtr->style, myErrorName(status));
            continue;
        }
        unum_setContext(unum, itemPtr->context, &status);
        ulenRes = unum_formatDouble(unum, 123.45, ubufResult, kUBufMax, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: unum_formatDouble, locale %s, style %d, context %d - %s\n",
                    itemPtr->locale, (int)itemPtr->style, (int)itemPtr->context, myErrorName(status));
        } else {
            UChar ubufExpected[kUBufMax];
            int32_t ulenExp = u_unescape(itemPtr->expectedResult, ubufExpected, kUBufMax);
            if (ulenRes != ulenExp || u_strncmp(ubufResult, ubufExpected, ulenExp) != 0) {
                char bbuf[kUBufMax*2];
                u_austrncpy(bbuf, ubufResult, sizeof(bbuf));
                log_err("FAIL: unum_formatDouble, locale %s, style %d, context %d, expected %d:\"%s\", got %d:\"%s\"\n",
                        itemPtr->locale, (int)itemPtr->style, (int)itemPtr->context, ulenExp,
                        itemPtr->expectedResult, ulenRes, bbuf);
            }
        }
        unum_close(unum);
    }
#endif /* #if !UCONFIG_NO_NORMALIZATION && !UCONFIG_NO_BREAK_ITERATION */
}

static void TestCurrencyUsage(void) {
    static const char* DATA[][2] = {
        /* the data are:
         * currency ISO code to be formatted,
         * format result using CURRENCYSTYLE with CASH purpose,-
         * Note that as of CLDR 26:-
         * - TWD switches from 0 decimals to 2; PKR still has 0, so change test to that
         * - CAD rounds to .05
         */

        {"PKR", "PKR\\u00A0124"},
        {"CAD", "CA$123.55"},
        {"USD", "$123.57"}
    };

    // 1st time for getter/setter, 2nd for factory method
    int32_t i;
    for(i=0; i<2; i++){
        const char* localeString = "en_US";
        double numberToBeFormat = 123.567;
        UNumberFormat* unumFmt;
        UNumberFormatStyle style = UNUM_CURRENCY;
        UErrorCode status = U_ZERO_ERROR;
        int32_t j;

        if(i == 1){ // change for factory method
            style = UNUM_CASH_CURRENCY;
        }

        unumFmt = unum_open(style, NULL, 0, localeString, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: unum_open, locale %s, style %d - %s\n",
                        localeString, (int)style, myErrorName(status));
            continue;
        }

        if(i == 0){ // this is for the getter/setter
            if(unum_getAttribute(unumFmt, UNUM_CURRENCY_USAGE) != UCURR_USAGE_STANDARD) {
                log_err("FAIL: currency usage attribute is not UNUM_CURRENCY_STANDARD\n");
            }

            unum_setAttribute(unumFmt, UNUM_CURRENCY_USAGE, UCURR_USAGE_CASH);
        }

        if(unum_getAttribute(unumFmt, UNUM_CURRENCY_USAGE) != UCURR_USAGE_CASH) {
            log_err("FAIL: currency usage attribute is not UNUM_CASH_CURRENCY\n");
        }

        for (j=0; j<UPRV_LENGTHOF(DATA); ++j) {
            UChar expect[64];
            int32_t expectLen;
            UChar currencyCode[4];
            UChar result[64];
            int32_t resultLen;
            UFieldPosition pos = {0};

            u_charsToUChars(DATA[j][0], currencyCode, 3);
            expectLen = u_unescape(DATA[j][1], expect, UPRV_LENGTHOF(expect));

            unum_setTextAttribute(unumFmt, UNUM_CURRENCY_CODE, currencyCode, 3, &status);
            assertSuccess("num_setTextAttribute()", &status);

            resultLen = unum_formatDouble(unumFmt, numberToBeFormat, result, UPRV_LENGTHOF(result),
                                        &pos, &status);
            assertSuccess("num_formatDouble()", &status);

            if(resultLen != expectLen || u_strcmp(result, expect) != 0) {
                log_err("Fail: Error in Number Format Currency Purpose using unum_setAttribute() expected: %s, got %s\n",
                aescstrdup(expect, expectLen), aescstrdup(result, resultLen));
            }

        }

        unum_close(unumFmt);
    }
}

static UChar currFmtNegSameAsPos[] = /* "\u00A4#,##0.00;\u00A4#,##0.00" */
    {0xA4,0x23,0x2C,0x23,0x23,0x30,0x2E,0x30,0x30,0x3B,0xA4,0x23,0x2C,0x23,0x23,0x30,0x2E,0x30,0x30,0};

// NOTE: As of ICU 62, identical positive and negative subpatterns means no minus sign!
// See CLDR ticket https://unicode.org/cldr/trac/ticket/10703
//static UChar currFmtToPatExpected[] = /* "\u00A4#,##0.00" */
//    {0xA4,0x23,0x2C,0x23,0x23,0x30,0x2E,0x30,0x30,0};
static const UChar* currFmtToPatExpected = currFmtNegSameAsPos;

static UChar currFmtResultExpected[] = /* "$100.00" */
    {0x24,0x31,0x30,0x30,0x2E,0x30,0x30,0};

static UChar emptyString[] = {0};

enum { kUBufSize = 64, kBBufSize = 128 };

static void TestCurrFmtNegSameAsPositive(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unumfmt = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
    if ( U_SUCCESS(status) ) {
        unum_applyPattern(unumfmt, false, currFmtNegSameAsPos, -1, NULL, &status);
        if (U_SUCCESS(status)) {
            UChar ubuf[kUBufSize];
            int32_t ulen = unum_toPattern(unumfmt, false, ubuf, kUBufSize, &status);
            if (U_FAILURE(status)) {
                log_err("unum_toPattern fails with status %s\n", myErrorName(status));
            } else if (u_strcmp(ubuf, currFmtToPatExpected) != 0) {
                log_err("unum_toPattern result wrong, expected %s, got %s\n", aescstrdup(currFmtToPatExpected,-1), aescstrdup(ubuf,ulen));
            }
            unum_setSymbol(unumfmt, UNUM_MINUS_SIGN_SYMBOL, emptyString, 0, &status);
            if (U_SUCCESS(status)) {
                ulen = unum_formatDouble(unumfmt, -100.0, ubuf, kUBufSize, NULL, &status);
                if (U_FAILURE(status)) {
                    log_err("unum_formatDouble fails with status %s\n", myErrorName(status));
                } else if (u_strcmp(ubuf, currFmtResultExpected) != 0) {
                    log_err("unum_formatDouble result wrong, expected %s, got %s\n", aescstrdup(currFmtResultExpected,-1), aescstrdup(ubuf,ulen));
                }
            } else {
                log_err("unum_setSymbol fails with status %s\n", myErrorName(status));
            }
        } else {
            log_err("unum_applyPattern fails with status %s\n", myErrorName(status));
        }
        unum_close(unumfmt);
    } else {
        log_data_err("unum_open UNUM_CURRENCY for en_US fails with status %s\n", myErrorName(status));
    }
}


typedef struct {
  double value;
#if APPLE_ICU_CHANGES
// rdar://
  const char *valueStr;
#endif  // APPLE_ICU_CHANGES
  const char *expected;
} ValueAndExpectedString;

#if APPLE_ICU_CHANGES
// rdar://
static const ValueAndExpectedString enDecMinFrac[] = {
  {0.0,           "0.0",           "0.0"},
  {0.17,          "0.17",          "0.17"},
  {1.0,           "1.0",           "1.0"},
  {1234.0,        "1234.0",        "1,234.0"},
  {12345.0,       "12345.0",       "12,345.0"},
  {123456.0,      "123456.0",      "123,456.0"},
  {1234567.0,     "1234567.0",     "1,234,567.0"},
  {12345678.0,    "12345678.0",    "12,345,678.0"},
  {0.0, NULL, NULL}
};
#endif  // APPLE_ICU_CHANGES

static const ValueAndExpectedString enShort[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {0.0,           "0.0",           "0"},
  {0.17,          "0.17",          "0.17"},
  {1.0,           "1.0",           "1"},
  {1234.0,        "1234.0",        "1.2K"},
  {12345.0,       "12345.0",       "12K"},
  {123456.0,      "123456.0",      "123K"},
  {1234567.0,     "1234567.0",     "1.2M"},
  {12345678.0,    "12345678.0",    "12M"},
  {123456789.0,   "123456789.0",   "123M"},
  {1.23456789E9,  "1.23456789E9",  "1.2B"},
  {1.23456789E10, "1.23456789E10", "12B"},
  {1.23456789E11, "1.23456789E11", "123B"},
  {1.23456789E12, "1.23456789E12", "1.2T"},
  {1.23456789E13, "1.23456789E13", "12T"},
  {1.23456789E14, "1.23456789E14", "123T"},
  {1.23456789E15, "1.23456789E15", "1235T"},
  {0.0, NULL, NULL}
#else
  {0.0, "0"},
  {0.17, "0.17"},
  {1.0, "1"},
  {1234.0, "1.2K"},
  {12345.0, "12K"},
  {123456.0, "123K"},
  {1234567.0, "1.2M"},
  {12345678.0, "12M"},
  {123456789.0, "123M"},
  {1.23456789E9, "1.2B"},
  {1.23456789E10, "12B"},
  {1.23456789E11, "123B"},
  {1.23456789E12, "1.2T"},
  {1.23456789E13, "12T"},
  {1.23456789E14, "123T"},
  {1.23456789E15, "1235T"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

static const ValueAndExpectedString enShortMax2[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {0.0,           "0.0",           "0"},
  {0.17,          "0.17",          "0.17"},
  {1.0,           "1.0",           "1"},
  {1234.0,        "1234.0",        "1.2K"},
  {12345.0,       "12345.0",       "12K"},
  {123456.0,      "123456.0",      "120K"},
  {1234567.0,     "1234567.0",     "1.2M"},
  {12345678.0,    "12345678.0",    "12M"},
  {123456789.0,   "123456789.0",   "120M"},
  {1.23456789E9,  "1.23456789E9",  "1.2B"},
  {1.23456789E10, "1.23456789E10", "12B"},
  {1.23456789E11, "1.23456789E11", "120B"},
  {1.23456789E12, "1.23456789E12", "1.2T"},
  {1.23456789E13, "1.23456789E13", "12T"},
  {1.23456789E14, "1.23456789E14", "120T"},
  {1.23456789E15, "1.23456789E15", "1200T"},
  {0.0, NULL, NULL}
#else
  {0.0, "0"},
  {0.17, "0.17"},
  {1.0, "1"},
  {1234.0, "1.2K"},
  {12345.0, "12K"},
  {123456.0, "120K"},
  {1234567.0, "1.2M"},
  {12345678.0, "12M"},
  {123456789.0, "120M"},
  {1.23456789E9, "1.2B"},
  {1.23456789E10, "12B"},
  {1.23456789E11, "120B"},
  {1.23456789E12, "1.2T"},
  {1.23456789E13, "12T"},
  {1.23456789E14, "120T"},
  {1.23456789E15, "1200T"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

static const ValueAndExpectedString enShortMax5[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {0.0,           "0.0",           "0"},
  {0.17,          "0.17",          "0.17"},
  {1.0,           "1.0",           "1"},
  {1234.0,        "1234.0",        "1.234K"},
  {12345.0,       "12345.0",       "12.345K"},
  {123456.0,      "123456.0",      "123.46K"},
  {1234567.0,     "1234567.0",     "1.2346M"},
  {12345678.0,    "12345678.0",    "12.346M"},
  {123456789.0,   "123456789.0",   "123.46M"},
  {1.23456789E9,  "1.23456789E9",  "1.2346B"},
  {1.23456789E10, "1.23456789E10", "12.346B"},
  {1.23456789E11, "1.23456789E11", "123.46B"},
  {1.23456789E12, "1.23456789E12", "1.2346T"},
  {1.23456789E13, "1.23456789E13", "12.346T"},
  {1.23456789E14, "1.23456789E14", "123.46T"},
  {1.23456789E15, "1.23456789E15", "1234.6T"},
  {0.0, NULL, NULL}
#else
  {0.0, "0"},
  {0.17, "0.17"},
  {1.0, "1"},
  {1234.0, "1.234K"},
  {12345.0, "12.345K"},
  {123456.0, "123.46K"},
  {1234567.0, "1.2346M"},
  {12345678.0, "12.346M"},
  {123456789.0, "123.46M"},
  {1.23456789E9, "1.2346B"},
  {1.23456789E10, "12.346B"},
  {1.23456789E11, "123.46B"},
  {1.23456789E12, "1.2346T"},
  {1.23456789E13, "12.346T"},
  {1.23456789E14, "123.46T"},
  {1.23456789E15, "1234.6T"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

static const ValueAndExpectedString enShortMin3[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {0.0,           "0.0",           "0.00"},
  {0.17,          "0.17",          "0.170"},
  {1.0,           "1.0",           "1.00"},
  {1234.0,        "1234.0",        "1.23K"},
  {12345.0,       "12345.0",       "12.3K"},
  {123456.0,      "123456.0",      "123K"},
  {1234567.0,     "1234567.0",     "1.23M"},
  {12345678.0,    "12345678.0",    "12.3M"},
  {123456789.0,   "123456789.0",   "123M"},
  {1.23456789E9,  "1.23456789E9",  "1.23B"},
  {1.23456789E10, "1.23456789E10", "12.3B"},
  {1.23456789E11, "1.23456789E11", "123B"},
  {1.23456789E12, "1.23456789E12", "1.23T"},
  {1.23456789E13, "1.23456789E13", "12.3T"},
  {1.23456789E14, "1.23456789E14", "123T"},
  {1.23456789E15, "1.23456789E15", "1230T"},
  {0.0, NULL, NULL}
#else
  {0.0, "0.00"},
  {0.17, "0.170"},
  {1.0, "1.00"},
  {1234.0, "1.23K"},
  {12345.0, "12.3K"},
  {123456.0, "123K"},
  {1234567.0, "1.23M"},
  {12345678.0, "12.3M"},
  {123456789.0, "123M"},
  {1.23456789E9, "1.23B"},
  {1.23456789E10, "12.3B"},
  {1.23456789E11, "123B"},
  {1.23456789E12, "1.23T"},
  {1.23456789E13, "12.3T"},
  {1.23456789E14, "123T"},
  {1.23456789E15, "1230T"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

static const ValueAndExpectedString jaShortMax2[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {1234.0,        "1234.0",        "1200"},
  {12345.0,       "12345.0",       "1.2\\u4E07"},
  {123456.0,      "123456.0",      "12\\u4E07"},
  {1234567.0,     "1234567.0",     "120\\u4E07"},
  {12345678.0,    "12345678.0",    "1200\\u4E07"},
  {123456789.0,   "123456789.0",   "1.2\\u5104"},
  {1.23456789E9,  "1.23456789E9",  "12\\u5104"},
  {1.23456789E10, "1.23456789E10", "120\\u5104"},
  {1.23456789E11, "1.23456789E11", "1200\\u5104"},
  {1.23456789E12, "1.23456789E12", "1.2\\u5146"},
  {1.23456789E13, "1.23456789E13", "12\\u5146"},
  {1.23456789E14, "1.23456789E14", "120\\u5146"},
  {0.0, NULL, NULL}
#else
  {1234.0, "1200"},
  {12345.0, "1.2\\u4E07"},
  {123456.0, "12\\u4E07"},
  {1234567.0, "120\\u4E07"},
  {12345678.0, "1200\\u4E07"},
  {123456789.0, "1.2\\u5104"},
  {1.23456789E9, "12\\u5104"},
  {1.23456789E10, "120\\u5104"},
  {1.23456789E11, "1200\\u5104"},
  {1.23456789E12, "1.2\\u5146"},
  {1.23456789E13, "12\\u5146"},
  {1.23456789E14, "120\\u5146"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

static const ValueAndExpectedString srLongMax2[] = {
#if APPLE_ICU_CHANGES
// rdar://
  {1234.0,         "1234.0",        "1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {12345.0,        "12345.0",       "12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {21789.0,        "21789.0",       "22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {123456.0,       "123456.0",      "120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {999999.0,       "999999.0",      "1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"}, // 10^6 one
  {1234567.0,      "1234567.0",     "1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 few
  {12345678.0,     "12345678.0",    "12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {123456789.0,    "123456789.0",   "120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {1.23456789E9,   "1.23456789E9",  "1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E10,  "1.23456789E10", "12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {2.08901234E10,  "2.08901234E10", "21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"}, // 10^9 one
  {2.18901234E10,  "2.18901234E10", "22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E11,  "1.23456789E11", "120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {-1234.0,        "-1234.0",        "-1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-12345.0,       "-12345.0",       "-12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-21789.0,       "-21789.0",       "-22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-123456.0,      "-123456.0",      "-120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-999999.0,      "-999999.0",      "-1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"},
  {-1234567.0,     "-1234567.0",     "-1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-12345678.0,    "-12345678.0",    "-12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-123456789.0,   "-123456789.0",   "-120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E9,  "-1.23456789E9",  "-1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E10, "-1.23456789E10", "-12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {-2.08901234E10, "-2.08901234E10", "-21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"},
  {-2.18901234E10, "-2.18901234E10", "-22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E11, "-1.23456789E11", "-120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {0.0, NULL, NULL}
#else
  {1234.0, "1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {12345.0, "12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {21789.0, "22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {123456.0, "120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {999999.0, "1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"}, // 10^6 one
  {1234567.0, "1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 few
  {12345678.0, "12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {123456789.0, "120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {1.23456789E9, "1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E10, "12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {2.08901234E10, "21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"}, // 10^9 one
  {2.18901234E10, "22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E11, "120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {-1234.0, "-1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-12345.0, "-12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-21789.0, "-22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-123456.0, "-120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-999999.0, "-1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"},
  {-1234567.0, "-1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-12345678.0, "-12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-123456789.0, "-120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E9, "-1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E10, "-12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {-2.08901234E10, "-21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"},
  {-2.18901234E10, "-22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E11, "-120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {0.0, NULL}
#endif  // APPLE_ICU_CHANGES
};

#if APPLE_ICU_CHANGES
// rdar://
// rdar://52188411
static const ValueAndExpectedString enINShortMax2[] = {
  {0.0,           "0.0",           "0"},
  {0.17,          "0.17",          "0.17"},
  {1.0,           "1.0",           "1"},
  {1234.0,        "1234.0",        "1.2K"},
  {12345.0,       "12345.0",       "12K"},
  {123456.0,      "123456.0",      "1.2L"},
  {1234567.0,     "1234567.0",     "12L"},
  {12345678.0,    "12345678.0",    "1.2Cr"},
  {123456789.0,   "123456789.0",   "12Cr"},
  {1.23456789E9,  "1.23456789E9",  "120Cr"},
  {1.23456789E10, "1.23456789E10", "1.2KCr"},
  {1.23456789E11, "1.23456789E11", "12KCr"},
  {1.23456789E12, "1.23456789E12", "1.2LCr"},
  {1.23456789E13, "1.23456789E13", "12LCr"},
  {1.23456789E14, "1.23456789E14", "120LCr"},
  {1.23456789E15, "1.23456789E15", "1200LCr"},
  {0.0, NULL, NULL}
};
#endif  // APPLE_ICU_CHANGES

typedef struct {
    const char *       locale;
    UNumberFormatStyle style;
    int32_t            attribute; // UNumberFormatAttribute, or -1 for none
    int32_t            attrValue; //
    const ValueAndExpectedString * veItems;
} LocStyleAttributeTest;

static const LocStyleAttributeTest lsaTests[] = {
#if APPLE_ICU_CHANGES
// rdar://
  { "en",   UNUM_DECIMAL,                   UNUM_MIN_FRACTION_DIGITS,       1,  enDecMinFrac },
#endif  // APPLE_ICU_CHANGES
  { "en",   UNUM_DECIMAL_COMPACT_SHORT,     -1,                             0,  enShort },
  { "en",   UNUM_DECIMAL_COMPACT_SHORT,     UNUM_MAX_SIGNIFICANT_DIGITS,    2,  enShortMax2 },
  { "en",   UNUM_DECIMAL_COMPACT_SHORT,     UNUM_MAX_SIGNIFICANT_DIGITS,    5,  enShortMax5 },
  { "en",   UNUM_DECIMAL_COMPACT_SHORT,     UNUM_MIN_SIGNIFICANT_DIGITS,    3,  enShortMin3 },
  { "ja",   UNUM_DECIMAL_COMPACT_SHORT,     UNUM_MAX_SIGNIFICANT_DIGITS,    2,  jaShortMax2 },
  { "sr",   UNUM_DECIMAL_COMPACT_LONG,      UNUM_MAX_SIGNIFICANT_DIGITS,    2,  srLongMax2  },
#if APPLE_ICU_CHANGES
// rdar://
  { "en_IN",UNUM_DECIMAL_COMPACT_SHORT,     UNUM_MAX_SIGNIFICANT_DIGITS,    2,  enINShortMax2 }, // rdar://52188411
#endif  // APPLE_ICU_CHANGES
  { NULL,   (UNumberFormatStyle)0,          -1,                             0,  NULL }
};

static void TestVariousStylesAndAttributes(void) {
    const LocStyleAttributeTest * lsaTestPtr;
    for (lsaTestPtr = lsaTests; lsaTestPtr->locale != NULL; lsaTestPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat * unum = unum_open(lsaTestPtr->style, NULL, 0, lsaTestPtr->locale, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: unum_open style %d, locale %s: error %s\n", (int)lsaTestPtr->style, lsaTestPtr->locale, u_errorName(status));
        } else {
            const ValueAndExpectedString * veItemPtr;
            if (lsaTestPtr->attribute >= 0) {
                unum_setAttribute(unum, (UNumberFormatAttribute)lsaTestPtr->attribute, lsaTestPtr->attrValue);
            }
            // ICU 62: should call minSignificantDigits in tandem with maxSignificantDigits.
            if (lsaTestPtr->attribute == UNUM_MIN_SIGNIFICANT_DIGITS) {
                unum_setAttribute(unum, UNUM_MAX_SIGNIFICANT_DIGITS, lsaTestPtr->attrValue);
            }
            for (veItemPtr = lsaTestPtr->veItems; veItemPtr->expected != NULL; veItemPtr++) {
                UChar uexp[kUBufSize];
                UChar uget[kUBufSize];
                int32_t uexplen, ugetlen;

                status = U_ZERO_ERROR;
                uexplen = u_unescape(veItemPtr->expected, uexp, kUBufSize);
                ugetlen = unum_formatDouble(unum, veItemPtr->value, uget, kUBufSize, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: unum_formatDouble style %d, locale %s, attr %d, value %.2f: error %s\n",
                            (int)lsaTestPtr->style, lsaTestPtr->locale, lsaTestPtr->attribute, veItemPtr->value, u_errorName(status));
                } else if (ugetlen != uexplen || u_strncmp(uget, uexp, uexplen) != 0) {
                    char bexp[kBBufSize];
                    char bget[kBBufSize];
                    u_strToUTF8(bexp, kBBufSize, NULL, uexp, uexplen, &status);
                    u_strToUTF8(bget, kBBufSize, NULL, uget, ugetlen, &status);
                    log_err("FAIL: unum_formatDouble style %d, locale %s, attr %d, value %.2f: expect \"%s\", get \"%s\"\n",
                            (int)lsaTestPtr->style, lsaTestPtr->locale, lsaTestPtr->attribute, veItemPtr->value, bexp, bget);
                }
#if APPLE_ICU_CHANGES
// rdar://
                status = U_ZERO_ERROR;
                ugetlen = unum_formatDecimal(unum, veItemPtr->valueStr, -1, uget, kUBufSize, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: unum_formatDecimal style %d, locale %s, attr %d, valueStr %s: error %s\n",
                            (int)lsaTestPtr->style, lsaTestPtr->locale, lsaTestPtr->attribute, veItemPtr->valueStr, u_errorName(status));
                } else if (ugetlen != uexplen || u_strncmp(uget, uexp, uexplen) != 0) {
                    char bexp[kBBufSize];
                    char bget[kBBufSize];
                    u_strToUTF8(bexp, kBBufSize, NULL, uexp, uexplen, &status);
                    u_strToUTF8(bget, kBBufSize, NULL, uget, ugetlen, &status);
                    log_err("FAIL: unum_formatDecimal style %d, locale %s, attr %d, valueStr %s: expect \"%s\", get \"%s\"\n",
                            (int)lsaTestPtr->style, lsaTestPtr->locale, lsaTestPtr->attribute, veItemPtr->valueStr, bexp, bget);
                }
#endif  // APPLE_ICU_CHANGES
            }
            unum_close(unum);
        }
    }
}

static const UChar currpat[]  = { 0xA4,0x23,0x2C,0x23,0x23,0x30,0x2E,0x30,0x30,0}; /* ¤#,##0.00 */
static const UChar parsetxt[] = { 0x78,0x30,0x79,0x24,0 }; /* x0y$ */

static void TestParseCurrPatternWithDecStyle() {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *unumfmt = unum_open(UNUM_DECIMAL, NULL, 0, "en_US", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("unum_open DECIMAL failed for en_US: %s (Are you missing data?)\n", u_errorName(status));
    } else {
        unum_applyPattern(unumfmt, false, currpat, -1, NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "unum_applyPattern failed: %s\n", u_errorName(status));
        } else {
            int32_t pos = 0;
            double value = unum_parseDouble(unumfmt, parsetxt, -1, &pos, &status);
            if (U_SUCCESS(status)) {
                log_err_status(status, "unum_parseDouble expected to fail but got status %s, value %f\n", u_errorName(status), value);
            }
        }
        unum_close(unumfmt);
    }
}

/*
 * Ticket #12684
 * Test unum_formatDoubleForFields (and UFieldPositionIterator)
 */

typedef struct {
    int32_t field;
    int32_t beginPos;
    int32_t endPos;
} FieldsData;

typedef struct {
    const char *       locale;
    UNumberFormatStyle style;
    double             value;
    const FieldsData * expectedFields;
} FormatForFieldsItem;

static const UChar patNoFields[] = { 0x0027, 0x0078, 0x0027, 0 }; /* "'x'", for UNUM_PATTERN_DECIMAL */


/* "en_US", UNUM_CURRENCY, 123456.0 : "¤#,##0.00" => "$123,456.00" */
static const FieldsData fields_en_CURR[] = {
    { UNUM_CURRENCY_FIELD /*7*/,            0, 1 },
    { UNUM_GROUPING_SEPARATOR_FIELD /*6*/,  4, 5 },
    { UNUM_INTEGER_FIELD /*0*/,             1, 8 },
    { UNUM_DECIMAL_SEPARATOR_FIELD /*2*/,   8, 9 },
    { UNUM_FRACTION_FIELD /*1*/,            9, 11 },
    { -1, -1, -1 },
};
#if APPLE_ICU_CHANGES
// rdar://
/* "en_US"/"es_US/MX" , UNUM_PERCENT, -34 : "#,##0%" => "-34%" */
#else
/* "en_US", UNUM_PERCENT, -34 : "#,##0%" => "-34%" */
#endif  // APPLE_ICU_CHANGES
static const FieldsData fields_en_PRCT[] = {
    { UNUM_SIGN_FIELD /*10*/,               0, 1 },
    { UNUM_INTEGER_FIELD /*0*/,             1, 3 },
    { UNUM_PERCENT_FIELD /*8*/,             3, 4 },
    { -1, -1, -1 },
};
/* "fr_FR", UNUM_CURRENCY, 123456.0 : "#,##0.00 ¤" => "123,456.00 €" */
static const FieldsData fields_fr_CURR[] = {
    { UNUM_GROUPING_SEPARATOR_FIELD /*6*/,  3, 4 },
    { UNUM_INTEGER_FIELD /*0*/,             0, 7 },
    { UNUM_DECIMAL_SEPARATOR_FIELD /*2*/,   7, 8 },
    { UNUM_FRACTION_FIELD /*1*/,            8, 10 },
    { UNUM_CURRENCY_FIELD /*7*/,           11, 12 },
    { -1, -1, -1 },
};
/* "en_US", UNUM_PATTERN_DECIMAL, 12.0 : "'x'" => "x12" */
static const FieldsData fields_en_PATN[] = {
    { UNUM_INTEGER_FIELD /*0*/,             1, 3 },
    { -1, -1, -1 },
};

static const FormatForFieldsItem fffItems[] = {
    { "en_US", UNUM_CURRENCY_STANDARD, 123456.0, fields_en_CURR },
    { "en_US", UNUM_PERCENT,              -0.34, fields_en_PRCT },
#if APPLE_ICU_CHANGES
// rdar://
    { "es_US", UNUM_PERCENT,              -0.34, fields_en_PRCT }, // rdar://57000745
    { "es_MX", UNUM_PERCENT,              -0.34, fields_en_PRCT }, // rdar://42948387
#endif  // APPLE_ICU_CHANGES
    { "fr_FR", UNUM_CURRENCY_STANDARD, 123456.0, fields_fr_CURR },
    { "en_US", UNUM_PATTERN_DECIMAL,       12.0, fields_en_PATN },
    { NULL, (UNumberFormatStyle)0, 0, NULL },
};

static void TestFormatForFields(void) {
    UErrorCode status = U_ZERO_ERROR;
    UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
    if ( U_FAILURE(status) ) {
        log_err("ufieldpositer_open fails, status %s\n", u_errorName(status));
    } else {
        const FormatForFieldsItem * itemPtr;
        for (itemPtr = fffItems; itemPtr->locale != NULL; itemPtr++) {
            UNumberFormat* unum;
            status = U_ZERO_ERROR;
            unum = (itemPtr->style == UNUM_PATTERN_DECIMAL)?
                unum_open(itemPtr->style, patNoFields, -1, itemPtr->locale, NULL, &status):
                unum_open(itemPtr->style, NULL, 0, itemPtr->locale, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_data_err("unum_open fails for locale %s, style %d: status %s (Are you missing data?)\n", itemPtr->locale, itemPtr->style, u_errorName(status));
            } else {
                UChar ubuf[kUBufSize];
                int32_t ulen = unum_formatDoubleForFields(unum, itemPtr->value, ubuf, kUBufSize, fpositer, &status);
                if ( U_FAILURE(status) ) {
                    log_err("unum_formatDoubleForFields fails for locale %s, style %d: status %s\n", itemPtr->locale, itemPtr->style, u_errorName(status));
                } else {
                    const FieldsData * fptr;
                    int32_t field, beginPos, endPos;
                    for (fptr = itemPtr->expectedFields; true; fptr++) {
                        field = ufieldpositer_next(fpositer, &beginPos, &endPos);
                        if (field != fptr->field || (field >= 0 && (beginPos != fptr->beginPos || endPos != fptr->endPos))) {
                            if (fptr->field >= 0) {
                                log_err("unum_formatDoubleForFields for locale %s as \"%s\"; expect field %d range %d-%d, get field %d range %d-%d\n",
                                    itemPtr->locale, aescstrdup(ubuf, ulen), fptr->field, fptr->beginPos, fptr->endPos, field, beginPos, endPos);
                            } else {
                                log_err("unum_formatDoubleForFields for locale %s as \"%s\"; expect field < 0, get field %d range %d-%d\n",
                                    itemPtr->locale, aescstrdup(ubuf, ulen), field, beginPos, endPos);
                            }
                            break;
                        }
                        if (field < 0) {
                            break;
                        }
                    }
                }
                unum_close(unum);
            }
        }
        ufieldpositer_close(fpositer);
    }
}

static void Test12052_NullPointer() {
    UErrorCode status = U_ZERO_ERROR;
    static const UChar input[] = u"199a";
    UChar currency[200] = {0};
    UNumberFormat *theFormatter = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
    if (!assertSuccessCheck("unum_open() failed", &status, true)) { return; }
    status = U_ZERO_ERROR;
    unum_setAttribute(theFormatter, UNUM_LENIENT_PARSE, 1);
    int32_t pos = 1;
    unum_parseDoubleCurrency(theFormatter, input, -1, &pos, currency, &status);
    assertEquals("should fail gracefully", "U_PARSE_ERROR", u_errorName(status));
    unum_close(theFormatter);
}

typedef struct {
    const char*  locale;
    const UChar* text;       // text to parse
    UBool        lenient;    // leniency to use
    UBool        intOnly;    // whether to set PARSE_INT_ONLY
    UErrorCode   intStatus;  // expected status from parse
    int32_t      intPos;     // expected final pos from parse
    int32_t      intValue;   // expected value from parse
    UErrorCode   doubStatus; // expected status from parseDouble
    int32_t      doubPos;    // expected final pos from parseDouble
    double       doubValue;  // expected value from parseDouble
    UErrorCode   decStatus;  // expected status from parseDecimal
    int32_t      decPos;     // expected final pos from parseDecimal
    const char*  decString;  // expected output string from parseDecimal

} ParseCaseItem;

static const ParseCaseItem parseCaseItems[] = {
    { "en", u"0,000",            false, false, U_ZERO_ERROR,            5,          0, U_ZERO_ERROR,  5,                0.0, U_ZERO_ERROR,  5, "0" },
    { "en", u"0,000",            true,  false, U_ZERO_ERROR,            5,          0, U_ZERO_ERROR,  5,                0.0, U_ZERO_ERROR,  5, "0" },
#if APPLE_ICU_CHANGES
// rdar://
    { "en", u",024",             false, false, U_ZERO_ERROR,            4,         24, U_ZERO_ERROR,  4,               24.0, U_ZERO_ERROR,  4, "24" },
    { "en", u",024",             true,  false, U_ZERO_ERROR,            4,         24, U_ZERO_ERROR,  4,               24.0, U_ZERO_ERROR,  4, "24" },
#endif  // APPLE_ICU_CHANGES
    { "en", u"1000,000",         false, false, U_PARSE_ERROR,           0,          0, U_PARSE_ERROR, 0,                0.0, U_PARSE_ERROR, 0, "" },
    { "en", u"1000,000",         true,  false, U_ZERO_ERROR,            8,    1000000, U_ZERO_ERROR,  8,          1000000.0, U_ZERO_ERROR,  8, "1000000" },
    { "en", u"",                 false, false, U_PARSE_ERROR,           0,          0, U_PARSE_ERROR, 0,                0.0, U_PARSE_ERROR, 0, "" },
    { "en", u"",                 true,  false, U_PARSE_ERROR,           0,          0, U_PARSE_ERROR, 0,                0.0, U_PARSE_ERROR, 0, "" },
    { "en", u"9999990000503021", false, false, U_INVALID_FORMAT_ERROR, 16, 2147483647, U_ZERO_ERROR, 16, 9999990000503020.0, U_ZERO_ERROR, 16, "9999990000503021" },
    { "en", u"9999990000503021", false, true,  U_INVALID_FORMAT_ERROR, 16, 2147483647, U_ZERO_ERROR, 16, 9999990000503020.0, U_ZERO_ERROR, 16, "9999990000503021" },
    { "en", u"1000000.5",        false, false, U_ZERO_ERROR,            9,    1000000, U_ZERO_ERROR,  9,          1000000.5, U_ZERO_ERROR,  9, "1.0000005E+6"},
    { "en", u"1000000.5",        false, true,  U_ZERO_ERROR,            7,    1000000, U_ZERO_ERROR,  7,          1000000.0, U_ZERO_ERROR,  7, "1000000" },
    { "en", u"123.5",            false, false, U_ZERO_ERROR,            5,        123, U_ZERO_ERROR,  5,              123.5, U_ZERO_ERROR,  5, "123.5" },
    { "en", u"123.5",            false, true,  U_ZERO_ERROR,            3,        123, U_ZERO_ERROR,  3,              123.0, U_ZERO_ERROR,  3, "123" },
    { NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0.0, 0, 0, NULL }
};

static void TestParseCases(void) {
    const ParseCaseItem* itemPtr;
    for (itemPtr = parseCaseItems; itemPtr->locale != NULL; itemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat* unumDec = unum_open(UNUM_DECIMAL, NULL, 0, itemPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("unum_open UNUM_DECIMAL fails for locale %s: %s\n", itemPtr->locale, u_errorName(status));
            continue;
        }
        int32_t intValue, parsePos, dclen;
        double doubValue;
        char decstr[32];
        unum_setAttribute(unumDec, UNUM_LENIENT_PARSE, itemPtr->lenient);
        unum_setAttribute(unumDec, UNUM_PARSE_INT_ONLY, itemPtr->intOnly);

        parsePos = 0;
        status = U_ZERO_ERROR;
        intValue = unum_parse(unumDec, itemPtr->text, -1, &parsePos, &status);
        if (status != itemPtr->intStatus || parsePos != itemPtr->intPos || intValue != itemPtr->intValue) {
            char btext[32];
            u_austrcpy(btext, itemPtr->text);
            log_err("locale %s, text \"%s\", lenient %d, intOnly %d;\n      parse        expected status %s, pos %d, value %d;\n                   got   %s, %d, %d\n",
                    itemPtr->locale, btext, itemPtr->lenient, itemPtr->intOnly,
                    u_errorName(itemPtr->intStatus), itemPtr->intPos, itemPtr->intValue,
                    u_errorName(status), parsePos, intValue);
        }

        parsePos = 0;
        status = U_ZERO_ERROR;
        doubValue = unum_parseDouble(unumDec, itemPtr->text, -1, &parsePos, &status);
        if (status != itemPtr->doubStatus || parsePos != itemPtr->doubPos || doubValue != itemPtr->doubValue) {
            char btext[32];
            u_austrcpy(btext, itemPtr->text);
            log_err("locale %s, text \"%s\", lenient %d, intOnly %d;\n      parseDouble  expected status %s, pos %d, value %.1f;\n                   got   %s, %d, %.1f\n",
                    itemPtr->locale, btext, itemPtr->lenient, itemPtr->intOnly,
                    u_errorName(itemPtr->doubStatus), itemPtr->doubPos, itemPtr->doubValue,
                    u_errorName(status), parsePos, doubValue);
        }

        parsePos = 0;
        status = U_ZERO_ERROR;
        decstr[0] = 0;
        dclen = unum_parseDecimal(unumDec, itemPtr->text, -1, &parsePos, decstr, 32, &status);
        (void)dclen;
        if (status != itemPtr->decStatus || parsePos != itemPtr->decPos || uprv_strcmp(decstr,itemPtr->decString) != 0) {
            char btext[32];
            u_austrcpy(btext, itemPtr->text);
            log_err("locale %s, text \"%s\", lenient %d, intOnly %d;\n      parseDecimal expected status %s, pos %d, str \"%s\";\n                   got   %s, %d, \"%s\"\n",
                    itemPtr->locale, btext, itemPtr->lenient, itemPtr->intOnly,
                    u_errorName(itemPtr->decStatus), itemPtr->decPos, itemPtr->decString,
                    u_errorName(status), parsePos, decstr);
        }

        unum_close(unumDec);
    }
}

typedef struct {
    const char*        descrip;
    const char*        locale;
    UNumberFormatStyle style;
    int32_t            minInt;
    int32_t            minFrac;
    int32_t            maxFrac;
    double             roundIncr;
    const UChar*       expPattern;
    double             valueToFmt;
    const UChar*       expFormat;
} SetMaxFracAndRoundIncrItem;

static const SetMaxFracAndRoundIncrItem maxFracAndRoundIncrItems[] = {
    // descrip                     locale   style         mnI mnF mxF rdInc   expPat         value  expFmt
    { "01 en_US DEC 1/0/3/0.0",    "en_US", UNUM_DECIMAL,  1,  0,  3, 0.0,    u"#,##0.###",  0.128, u"0.128" },
    { "02 en_US DEC 1/0/1/0.0",    "en_US", UNUM_DECIMAL,  1,  0,  1, 0.0,    u"#,##0.#",    0.128, u"0.1"   },
    { "03 en_US DEC 1/0/1/0.01",   "en_US", UNUM_DECIMAL,  1,  0,  1, 0.01,   u"#,##0.#",    0.128, u"0.1"   },
    { "04 en_US DEC 1/1/1/0.01",   "en_US", UNUM_DECIMAL,  1,  1,  1, 0.01,   u"#,##0.0",    0.128, u"0.1"   },
    { "05 en_US DEC 1/0/1/0.1",    "en_US", UNUM_DECIMAL,  1,  0,  1, 0.1,    u"#,##0.1",    0.128, u"0.1"   }, // use incr
    { "06 en_US DEC 1/1/1/0.1",    "en_US", UNUM_DECIMAL,  1,  1,  1, 0.1,    u"#,##0.1",    0.128, u"0.1"   }, // use incr

    { "10 en_US DEC 1/0/1/0.02",   "en_US", UNUM_DECIMAL,  1,  0,  1, 0.02,   u"#,##0.#",    0.128, u"0.1"   },
    { "11 en_US DEC 1/0/2/0.02",   "en_US", UNUM_DECIMAL,  1,  0,  2, 0.02,   u"#,##0.02",   0.128, u"0.12"  }, // use incr
    { "12 en_US DEC 1/0/3/0.02",   "en_US", UNUM_DECIMAL,  1,  0,  3, 0.02,   u"#,##0.02#",  0.128, u"0.12"  }, // use incr
    { "13 en_US DEC 1/1/1/0.02",   "en_US", UNUM_DECIMAL,  1,  1,  1, 0.02,   u"#,##0.0",    0.128, u"0.1"   },
    { "14 en_US DEC 1/1/2/0.02",   "en_US", UNUM_DECIMAL,  1,  1,  2, 0.02,   u"#,##0.02",   0.128, u"0.12"  }, // use incr
    { "15 en_US DEC 1/1/3/0.02",   "en_US", UNUM_DECIMAL,  1,  1,  3, 0.02,   u"#,##0.02#",  0.128, u"0.12"  }, // use incr
    { "16 en_US DEC 1/2/2/0.02",   "en_US", UNUM_DECIMAL,  1,  2,  2, 0.02,   u"#,##0.02",   0.128, u"0.12"  }, // use incr
    { "17 en_US DEC 1/2/3/0.02",   "en_US", UNUM_DECIMAL,  1,  2,  3, 0.02,   u"#,##0.02#",  0.128, u"0.12"  }, // use incr
    { "18 en_US DEC 1/3/3/0.02",   "en_US", UNUM_DECIMAL,  1,  3,  3, 0.02,   u"#,##0.020",  0.128, u"0.120" }, // use incr

    { "20 en_US DEC 1/1/1/0.0075", "en_US", UNUM_DECIMAL,  1,  1,  1, 0.0075, u"#,##0.0",    0.019, u"0.0"    },
    { "21 en_US DEC 1/1/2/0.0075", "en_US", UNUM_DECIMAL,  1,  1,  2, 0.0075, u"#,##0.0075", 0.004, u"0.0075" }, // use incr
    { "22 en_US DEC 1/1/2/0.0075", "en_US", UNUM_DECIMAL,  1,  1,  2, 0.0075, u"#,##0.0075", 0.019, u"0.0225" }, // use incr
    { "23 en_US DEC 1/1/3/0.0075", "en_US", UNUM_DECIMAL,  1,  1,  3, 0.0075, u"#,##0.0075", 0.004, u"0.0075" }, // use incr
    { "24 en_US DEC 1/1/3/0.0075", "en_US", UNUM_DECIMAL,  1,  1,  3, 0.0075, u"#,##0.0075", 0.019, u"0.0225" }, // use incr
    { "25 en_US DEC 1/2/2/0.0075", "en_US", UNUM_DECIMAL,  1,  2,  2, 0.0075, u"#,##0.0075", 0.004, u"0.0075" }, // use incr
    { "26 en_US DEC 1/2/2/0.0075", "en_US", UNUM_DECIMAL,  1,  2,  2, 0.0075, u"#,##0.0075", 0.019, u"0.0225" }, // use incr
    { "27 en_US DEC 1/2/3/0.0075", "en_US", UNUM_DECIMAL,  1,  2,  3, 0.0075, u"#,##0.0075", 0.004, u"0.0075" }, // use incr
    { "28 en_US DEC 1/2/3/0.0075", "en_US", UNUM_DECIMAL,  1,  2,  3, 0.0075, u"#,##0.0075", 0.019, u"0.0225" }, // use incr
    { "29 en_US DEC 1/3/3/0.0075", "en_US", UNUM_DECIMAL,  1,  3,  3, 0.0075, u"#,##0.0075", 0.004, u"0.0075" }, // use incr
    { "2A en_US DEC 1/3/3/0.0075", "en_US", UNUM_DECIMAL,  1,  3,  3, 0.0075, u"#,##0.0075", 0.019, u"0.0225" }, // use incr

#if APPLE_ICU_CHANGES
// rdar://
    // Additions for rdar://51452216
    { "30 en_US DEC 1/0/1/0.01",   "en_US", UNUM_DECIMAL,  1,  1,  3, 0.001,   u"#,##0.001", 1.23456789, u"1.235" },
    { "31 en_US DEC 1/1/1/0.01",   "en_US", UNUM_DECIMAL,  1,  1,  3, 0.001f,  u"#,##0.001", 1.23456789, u"1.235" },
#endif  // APPLE_ICU_CHANGES

    { NULL, NULL, UNUM_IGNORE, 0, 0, 0, 0.0, NULL, 0.0, NULL }
};

// The following is copied from C++ number_patternstring.cpp for this C test.
//
// Determine whether a given roundingIncrement should be ignored for formatting
// based on the current maxFrac value (maximum fraction digits). For example a
// roundingIncrement of 0.01 should be ignored if maxFrac is 1, but not if maxFrac
// is 2 or more. Note that roundingIncrements are rounded in significance, so
// a roundingIncrement of 0.006 is treated like 0.01 for this determination, i.e.
// it should not be ignored if maxFrac is 2 or more (but a roundingIncrement of
// 0.005 is treated like 0.001 for significance). This is the reason for the
// initial doubling below.
// roundIncr must be non-zero
static UBool ignoreRoundingIncrement(double roundIncr, int32_t maxFrac) {
    if (maxFrac < 0) {
        return false;
    }
    int32_t frac = 0;
    roundIncr *= 2.0;
    for (frac = 0; frac <= maxFrac && roundIncr <= 1.0; frac++, roundIncr *= 10.0);
    return (frac > maxFrac);
}

#if APPLE_ICU_CHANGES
// rdar://
#else
enum { kBBufMax = 128 };
#endif  // APPLE_ICU_CHANGES
static void TestSetMaxFracAndRoundIncr(void) {
    const SetMaxFracAndRoundIncrItem* itemPtr;
    for (itemPtr = maxFracAndRoundIncrItems; itemPtr->descrip != NULL; itemPtr++) {
        UChar ubuf[kUBufMax];
        char  bbufe[kBBufMax];
        char  bbufg[kBBufMax];
        int32_t ulen;
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat* unf = unum_open(itemPtr->style, NULL, 0, itemPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("locale %s: unum_open style %d fails with %s\n", itemPtr->locale, itemPtr->style, u_errorName(status));
            continue;
        }

        unum_setAttribute(unf, UNUM_MIN_INTEGER_DIGITS, itemPtr->minInt);
        unum_setAttribute(unf, UNUM_MIN_FRACTION_DIGITS, itemPtr->minFrac);
        unum_setAttribute(unf, UNUM_MAX_FRACTION_DIGITS, itemPtr->maxFrac);
        unum_setDoubleAttribute(unf, UNUM_ROUNDING_INCREMENT, itemPtr->roundIncr);

        UBool roundIncrUsed = (itemPtr->roundIncr != 0.0 && !ignoreRoundingIncrement(itemPtr->roundIncr, itemPtr->maxFrac));

        int32_t minInt = unum_getAttribute(unf, UNUM_MIN_INTEGER_DIGITS);
        if (minInt != itemPtr->minInt) {
            log_err("test %s: unum_getAttribute UNUM_MIN_INTEGER_DIGITS, expected %d, got %d\n",
                    itemPtr->descrip, itemPtr->minInt, minInt);
        }
        int32_t minFrac = unum_getAttribute(unf, UNUM_MIN_FRACTION_DIGITS);
        if (minFrac != itemPtr->minFrac) {
            log_err("test %s: unum_getAttribute UNUM_MIN_FRACTION_DIGITS, expected %d, got %d\n",
                    itemPtr->descrip, itemPtr->minFrac, minFrac);
        }
        // If incrementRounding is used, maxFrac is set equal to minFrac
        int32_t maxFrac = unum_getAttribute(unf, UNUM_MAX_FRACTION_DIGITS);
        // If incrementRounding is used, maxFrac is set equal to minFrac
        int32_t expMaxFrac = (roundIncrUsed)? itemPtr->minFrac: itemPtr->maxFrac;
        if (maxFrac != expMaxFrac) {
            log_err("test %s: unum_getAttribute UNUM_MAX_FRACTION_DIGITS, expected %d, got %d\n",
                    itemPtr->descrip, expMaxFrac, maxFrac);
        }
        double roundIncr = unum_getDoubleAttribute(unf, UNUM_ROUNDING_INCREMENT);
        // If incrementRounding is not used, roundIncr is set to 0.0
        double expRoundIncr = (roundIncrUsed)? itemPtr->roundIncr: 0.0;
#if APPLE_ICU_CHANGES
// rdar://112745117 (SEED: NSNumberFormatter not rounding correctly using roundingIncrement & usesSignificantDigits)
        if (fabs(roundIncr - expRoundIncr) >= 0.001) {
#else
        if (roundIncr != expRoundIncr) {
#endif // APPLE_ICU_CHANGES
            log_err("test %s: unum_getDoubleAttribute UNUM_ROUNDING_INCREMENT, expected %f, got %f\n",
                    itemPtr->descrip, expRoundIncr, roundIncr);
        }

        status = U_ZERO_ERROR;
        ulen = unum_toPattern(unf, false, ubuf, kUBufMax, &status);
        (void)ulen;
        if ( U_FAILURE(status) ) {
            log_err("test %s: unum_toPattern fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expPattern)!=0) {
            u_austrcpy(bbufe, itemPtr->expPattern);
            u_austrcpy(bbufg, ubuf);
            log_err("test %s: unum_toPattern expect \"%s\", get \"%s\"\n", itemPtr->descrip, bbufe, bbufg);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unf, itemPtr->valueToFmt, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("test %s: unum_formatDouble fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expFormat)!=0) {
            u_austrcpy(bbufe, itemPtr->expFormat);
            u_austrcpy(bbufg, ubuf);
            log_err("test %s: unum_formatDouble expect \"%s\", get \"%s\"\n", itemPtr->descrip, bbufe, bbufg);
        }

        unum_close(unf);
    }
}

static void TestIgnorePadding(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_PATTERN_DECIMAL, NULL, 0, "en_US", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL for en_US and NULL pattern fails:%s\n", u_errorName(status));
    } else {
        unum_setAttribute(unum, UNUM_GROUPING_USED, 0);
        unum_setAttribute(unum, UNUM_FORMAT_WIDTH, 0);
        unum_setTextAttribute(unum, UNUM_PADDING_CHARACTER, u"*", 1, &status);
        if (U_FAILURE(status)) {
            log_err("unum_setTextAttribute UNUM_PADDING_CHARACTER to '*' fails: %s\n", u_errorName(status));
        } else {
            unum_setAttribute(unum, UNUM_PADDING_POSITION, 0);
            unum_setAttribute(unum, UNUM_MIN_INTEGER_DIGITS, 0);
            unum_setAttribute(unum, UNUM_MAX_INTEGER_DIGITS, 8);
            unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 0);
            unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 0);

            UChar ubuf[kUBufMax];
            int32_t ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
            if (U_FAILURE(status)) {
                log_err("unum_toPattern fails: %s\n", u_errorName(status));
            } else {
                char bbuf[kBBufMax];
                if (ulen > 0 && ubuf[0]==u'*') {
                    ubuf[kUBufMax-1] = 0; // ensure zero termination
                    u_austrncpy(bbuf, ubuf, kBBufMax);
                    log_err("unum_toPattern result should ignore padding but get %s\n", bbuf);
                }
                unum_applyPattern(unum, false, ubuf, ulen, NULL, &status);
                if (U_FAILURE(status)) {
                    log_err("unum_applyPattern fails: %s\n", u_errorName(status));
                } else {
                    ulen = unum_formatDecimal(unum, "24", -1, ubuf, kUBufMax, NULL, &status);
                    if (U_FAILURE(status)) {
                        log_err("unum_formatDecimal fails: %s\n", u_errorName(status));
                    } else if (u_strcmp(ubuf, u"24") != 0) {
                        ubuf[kUBufMax-1] = 0; // ensure zero termination
                        u_austrncpy(bbuf, ubuf, kBBufMax);
                        log_err("unum_formatDecimal result expect 24 but get %s\n", bbuf);
                    }
                }
            }
        }
        unum_close(unum);
    }
}

static void TestSciNotationMaxFracCap(void) {
    static const UChar* pat1 = u"#.##E+00;-#.##E+00";
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_PATTERN_DECIMAL, pat1, -1, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL with scientific pattern for \"en_US\" fails with %s\n", u_errorName(status));
    } else {
        double value;
        UChar ubuf[kUBufMax];
        char bbuf[kBBufMax];
        int32_t ulen;

        unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 0);
        unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 2147483647);
        ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
        if ( U_SUCCESS(status) ) {
            u_austrncpy(bbuf, ubuf, kUBufMax);
            log_info("unum_toPattern (%d): %s\n", ulen, bbuf);
        }

        for (value = 10.0; value < 1000000000.0; value *= 10.0) {
            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, value, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble value %.1f status %s\n", value, u_errorName(status));
            } else if (u_strncmp(ubuf,u"1E+0",4) != 0) {
                u_austrncpy(bbuf, ubuf, kUBufMax);
                log_err("unum_formatDouble value %.1f expected result to begin with 1E+0, got %s\n", value, bbuf);
            }
        }
        unum_close(unum);
    }
}

static void TestMinIntMinFracZero(void) {
#if APPLE_ICU_CHANGES
// rdar://
    UChar ubuf[kUBufMax];
    char  bbuf[kBBufMax];
    int minInt, minFrac, maxFrac, ulen;
#endif  // APPLE_ICU_CHANGES

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_DECIMAL, NULL, 0, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_DECIMAL for en_US fails with %s\n", u_errorName(status));
    } else {
#if APPLE_ICU_CHANGES
// rdar://
// move these to top
#else
        UChar ubuf[kUBufMax];
        char  bbuf[kBBufMax];
        int minInt, minFrac, ulen;
#endif  // APPLE_ICU_CHANGES

        unum_setAttribute(unum, UNUM_MIN_INTEGER_DIGITS, 0);
        unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 0);
        minInt = unum_getAttribute(unum, UNUM_MIN_INTEGER_DIGITS);
        minFrac = unum_getAttribute(unum, UNUM_MIN_FRACTION_DIGITS);
        if (minInt != 0 || minFrac != 0) {
            log_err("after setting minInt=minFrac=0, get minInt %d, minFrac %d\n", minInt, minFrac);
        }

        ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_toPattern fails with %s\n", u_errorName(status));
        } else if (ulen < 3 || u_strstr(ubuf, u"#.#")==NULL) {
            u_austrncpy(bbuf, ubuf, kUBufMax);
            log_info("after setting minInt=minFrac=0, expect pattern to contain \"#.#\", but get (%d): \"%s\"\n", ulen, bbuf);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unum, 10.0, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_formatDouble 10.0 ulen %d fails with %s\n", ulen, u_errorName(status));
        } else if (u_strcmp(ubuf, u"10") != 0) {
            u_austrncpy(bbuf, ubuf, kUBufMax);
            log_err("unum_formatDouble 10.0 expected \"10\", got \"%s\"\n", bbuf);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unum, 0.9, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_formatDouble 0.9 ulen %d fails with %s\n", ulen, u_errorName(status));
        } else if (u_strcmp(ubuf, u".9") != 0) {
            u_austrncpy(bbuf, ubuf, kUBufMax);
            log_err("unum_formatDouble 0.9 expected \".9\", got \"%s\"\n", bbuf);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unum, 0.0, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_formatDouble 0.0 ulen %d fails with %s\n", ulen, u_errorName(status));
        } else if (u_strcmp(ubuf, u"0") != 0) {
            u_austrncpy(bbuf, ubuf, kUBufMax);
            log_err("unum_formatDouble 0.0 expected \"0\", got \"%s\"\n", bbuf);
        }

        unum_close(unum);
        status = U_ZERO_ERROR;
        unum = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("unum_open UNUM_CURRENCY for en_US fails with %s\n", u_errorName(status));
        } else {
            unum_setAttribute(unum, UNUM_MIN_INTEGER_DIGITS, 0);
            unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 0);
            minInt = unum_getAttribute(unum, UNUM_MIN_INTEGER_DIGITS);
            minFrac = unum_getAttribute(unum, UNUM_MIN_FRACTION_DIGITS);
            if (minInt != 0 || minFrac != 0) {
                log_err("after setting CURRENCY minInt=minFrac=0, get minInt %d, minFrac %d\n", minInt, minFrac);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, 10.0, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble (CURRENCY) 10.0 ulen %d fails with %s\n", ulen, u_errorName(status));
            } else if (u_strcmp(ubuf, u"$10") != 0) {
                u_austrncpy(bbuf, ubuf, kUBufMax);
                log_err("unum_formatDouble (CURRENCY) 10.0 expected \"$10\", got \"%s\"\n", bbuf);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, 0.9, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble (CURRENCY) 0.9 ulen %d fails with %s\n", ulen, u_errorName(status));
            } else if (u_strcmp(ubuf, u"$.9") != 0) {
                u_austrncpy(bbuf, ubuf, kUBufMax);
                log_err("unum_formatDouble (CURRENCY) 0.9 expected \"$.9\", got \"%s\"\n", bbuf);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, 0.0, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble (CURRENCY) 0.0 ulen %d fails with %s\n", ulen, u_errorName(status));
            } else if (u_strcmp(ubuf, u"$0") != 0) {
                u_austrncpy(bbuf, ubuf, kUBufMax);
                log_err("unum_formatDouble (CURRENCY) 0.0 expected \"$0\", got \"%s\"\n", bbuf);
            }

            unum_close(unum);
        }
    }
#if APPLE_ICU_CHANGES
// rdar://
   // addition for rdar://57291456
    status = U_ZERO_ERROR;
    unum = unum_open(UNUM_PATTERN_DECIMAL, NULL, 0, "en_IE", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_DECIMAL for en_US fails with %s\n", u_errorName(status));
    } else {
        unum_setAttribute(unum, UNUM_MIN_INTEGER_DIGITS, 0);
        unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 1);
        minInt = unum_getAttribute(unum, UNUM_MIN_INTEGER_DIGITS);
        maxFrac = unum_getAttribute(unum, UNUM_MAX_FRACTION_DIGITS);
        if (minInt != 0 || maxFrac != 1) {
            log_err("after setting minInt=0, maxFrac=1, get minInt %d, maxFrac %d\n", minInt, maxFrac);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unum, 0.0, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_formatDouble (maxFrac 1) 0.0 ulen %d fails with %s\n", ulen, u_errorName(status));
        } else if (u_strcmp(ubuf, u".0") != 0) {
            u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("unum_formatDouble (maxFrac 1) 0.0 expected \".0\", got \"%s\"\n", bbuf);
        }

        unum_close(unum);
    }
#endif  // APPLE_ICU_CHANGES
}

static void Test21479_ExactCurrency(void) {
#if APPLE_ICU_CHANGES
// rdar://
#else
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_CURRENCY for en_US fails with %s\n", u_errorName(status));
        goto cleanup;
    }
    unum_setTextAttribute(nf, UNUM_CURRENCY_CODE, u"EUR", -1, &status);
    UChar result[40];
    unum_formatDecimal(nf, "987654321000000000000001", -1, result, 40, NULL, &status);
    if (!assertSuccess("Formatting currency decimal", &status)) {
        goto cleanup;
    }
    assertUEquals("", u"€987,654,321,000,000,000,000,001.00", result);

    cleanup:
    unum_close(nf);
#endif  // APPLE_ICU_CHANGES
}

static void Test22088_Ethiopic(void) {
    const struct TestCase {
        const char* localeID;
        UNumberFormatStyle style;
        const UChar* expectedResult;
    } testCases[] = {
        { "am_ET@numbers=ethi",        UNUM_DEFAULT,          u"፻፳፫" },
        { "am_ET@numbers=ethi",        UNUM_NUMBERING_SYSTEM, u"፻፳፫" },
        { "am_ET@numbers=traditional", UNUM_DEFAULT,          u"፻፳፫" },
        { "am_ET@numbers=traditional", UNUM_NUMBERING_SYSTEM, u"፻፳፫" },
        { "am_ET",                     UNUM_NUMBERING_SYSTEM, u"123" },    // make sure default for Ethiopic still works
        { "en_US",                     UNUM_NUMBERING_SYSTEM, u"123" },    // make sure non-algorithmic default still works
        { "ar_SA",                     UNUM_NUMBERING_SYSTEM, u"١٢٣" },    // make sure non-algorithmic default still works
        // NOTE: There are NO locales in ICU 72 whose default numbering system is algorithmic!
    };
    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        char errorMessage[200];
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(testCases[i].style, NULL, 0, testCases[i].localeID, NULL, &err);

        snprintf(errorMessage, 200, "Creation of number formatter for %s failed", testCases[i].localeID);
        if (assertSuccess(errorMessage, &err)) {
            UChar result[200];
            
            unum_formatDouble(nf, 123, result, 200, NULL, &err);
            snprintf(errorMessage, 200, "Formatting of number for %s failed", testCases[i].localeID);
            if (assertSuccess(errorMessage, &err)) {
                snprintf(errorMessage, 200, "Wrong result for %s", testCases[i].localeID);
                assertUEquals(errorMessage, testCases[i].expectedResult, result);
            }
        }
        unum_close(nf);
   }
}

static void TestChangingRuleset(void) {
    const struct TestCase {
        const char* localeID;
        const UChar* rulesetName;
        const UChar* expectedResult;
    } testCases[] = {
        { "en_US",               NULL,            u"123" },
        { "en_US",               u"%roman-upper", u"CXXIII" },
        { "en_US",               u"%ethiopic",    u"፻፳፫" },
        { "en_US@numbers=roman", NULL,            u"CXXIII" },
        { "en_US@numbers=ethi",  NULL,            u"፻፳፫" },
        { "am_ET",               NULL,            u"123" },
        { "am_ET",               u"%ethiopic",    u"፻፳፫" },
        { "am_ET@numbers=ethi",  NULL,            u"፻፳፫" },
    };
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        char errorMessage[200];
        const char* rulesetNameString = (testCases[i].rulesetName != NULL) ? austrdup(testCases[i].rulesetName) : "NULL";
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(UNUM_NUMBERING_SYSTEM, NULL, 0, testCases[i].localeID, NULL, &err);
        
        snprintf(errorMessage, 200, "Creating of number formatter for %s failed", testCases[i].localeID);
        if (assertSuccess(errorMessage, &err)) {
            if (testCases[i].rulesetName != NULL) {
                unum_setTextAttribute(nf, UNUM_DEFAULT_RULESET, testCases[i].rulesetName, -1, &err);
                snprintf(errorMessage, 200, "Changing formatter for %s's default ruleset to %s failed", testCases[i].localeID, rulesetNameString);
                assertSuccess(errorMessage, &err);
            }
            
            if (U_SUCCESS(err)) {
                UChar result[200];
                
                unum_formatDouble(nf, 123, result, 200, NULL, &err);
                snprintf(errorMessage, 200, "Formatting of number with %s/%s failed", testCases[i].localeID, rulesetNameString);
                if (assertSuccess(errorMessage, &err)) {
                    snprintf(errorMessage, 200, "Wrong result for %s/%s", testCases[i].localeID, rulesetNameString);
                    assertUEquals(errorMessage, testCases[i].expectedResult, result);
                }
            }
        }
        unum_close(nf);
    }
}

static void TestParseWithEmptyCurr(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("unum_open UNUM_CURRENCY for \"en_US\" fails with %s\n", u_errorName(status));
    } else {
        unum_setSymbol(unum, UNUM_CURRENCY_SYMBOL, u"", 0, &status);
        if (U_FAILURE(status)) {
            log_err("unum_setSymbol UNUM_CURRENCY_SYMBOL u\"\" fails with %s\n", u_errorName(status));
        } else {
            char bbuf[kBBufMax] = { 0 };
            UChar curr[4] = { 0 };
            int32_t ppos, blen;
            double val;
            const UChar* text = u"3";

            status = U_ZERO_ERROR;
            ppos = 0;
            blen = unum_parseDecimal(unum, text, -1, &ppos, bbuf, kBBufMax, &status);
            if (U_FAILURE(status)) {
                log_err("unum_parseDecimal u\"3\" with empty curr symbol fails with %s, ppos %d\n", u_errorName(status), ppos);
            } else if (ppos != 1 || blen != 1 || bbuf[0] != '3') {
                log_err("unum_parseDecimal expect ppos 1, blen 1, str 3; get %d, %d, %s\n", ppos, blen, bbuf);
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDouble(unum, text, -1, &ppos, &status);
            if (U_FAILURE(status)) {
                log_err("unum_parseDouble u\"3\" with empty curr symbol fails with %s, ppos %d\n", u_errorName(status), ppos);
            } else if (ppos != 1 || val != 3.0) {
                log_err("unum_parseDouble expect ppos 1, val 3.0; get %d, %.2f\n", ppos, val);
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDoubleCurrency(unum, text, -1, &ppos, curr, &status);
            if (U_SUCCESS(status)) {
                log_err("unum_parseDoubleCurrency u\"3\" with empty curr symbol succeeds, get ppos %d, val %.2f\n", ppos, val);
            }
        }
        unum_close(unum);
    }

    //                              "¤#,##0.00" "¤ #,##0.00" "#,##0.00 ¤" "#,##,##0.00¤"
    static const char* locales[] = {"en_US",    "nb_NO",     "cs_CZ",     "bn_BD",       NULL };
    const char ** localesPtr = locales;
    const char* locale;
    while ((locale = *localesPtr++) != NULL) {
        status = U_ZERO_ERROR;
        unum = unum_open(UNUM_CURRENCY, NULL, 0, locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("locale %s unum_open UNUM_CURRENCY fails with %s\n", locale, u_errorName(status));
        } else {
            UChar ubuf[kUBufMax];
            int32_t ppos, ulen;
            const double posValToUse = 37.0;
            const double negValToUse = -3.0;
            double val;

            status = U_ZERO_ERROR;
            unum_setSymbol(unum, UNUM_CURRENCY_SYMBOL, u"", 0, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_setSymbol UNUM_CURRENCY_SYMBOL u\"\" fails with %s, skipping\n", locale, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, posValToUse, ubuf, kUBufMax, NULL, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_formatDouble %.1f fails with %s, skipping\n", locale, posValToUse, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDouble(unum, ubuf, ulen, &ppos, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_parseDouble fails with %s, ppos %d, expect %.1f\n", locale, u_errorName(status), ppos, posValToUse);
            } else if (ppos != ulen || val != posValToUse) {
                log_err("locale %s unum_parseDouble expect ppos %d, val %.1f; get %d, %.2f\n", locale, ulen, posValToUse, ppos, val);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, negValToUse, ubuf, kUBufMax, NULL, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_formatDouble %.1f fails with %s, skipping\n", locale, negValToUse, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDouble(unum, ubuf, ulen, &ppos, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_parseDouble fails with %s, ppos %d, expect %.1f\n", locale, u_errorName(status), ppos, negValToUse);
            } else if (ppos != ulen || val != negValToUse) {
                log_err("locale %s unum_parseDouble expect ppos %d, val %.1f; get %d, %.2f\n", locale, ulen, negValToUse, ppos, val);
            }

            status = U_ZERO_ERROR;
            unum_applyPattern(unum, false, u"#,##0.00¤", -1, NULL, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s unum_applyPattern \"#,##0.00¤\" fails with %s, skipping\n", locale, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, posValToUse, ubuf, kUBufMax, NULL, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s with \"#,##0.00¤\" unum_formatDouble %.1f fails with %s, skipping\n", locale, posValToUse, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDouble(unum, ubuf, ulen, &ppos, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s with \"#,##0.00¤\" unum_parseDouble fails with %s, ppos %d, expect %.1f\n", locale, u_errorName(status), ppos, posValToUse);
            } else if (ppos != ulen || val != posValToUse) {
                log_err("locale %s with \"#,##0.00¤\" unum_parseDouble expect ppos %d, val %.1f; get %d, %.2f\n", locale, ulen, posValToUse, ppos, val);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, negValToUse, ubuf, kUBufMax, NULL, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s with \"#,##0.00¤\" unum_formatDouble %.1f fails with %s, skipping\n", locale, negValToUse, u_errorName(status));
                continue;
            }

            status = U_ZERO_ERROR;
            ppos = 0;
            val = unum_parseDouble(unum, ubuf, ulen, &ppos, &status);
            if (U_FAILURE(status)) {
                log_err("locale %s with \"#,##0.00¤\" unum_parseDouble fails with %s, ppos %d, expect %.1f\n", locale, u_errorName(status), ppos, negValToUse);
            } else if (ppos != ulen || val != negValToUse) {
                log_err("locale %s with \"#,##0.00¤\" unum_parseDouble expect ppos %d, val %.1f; get %d, %.2f\n", locale, ulen, negValToUse, ppos, val);
            }

            unum_close(unum);
        }
    }
}

static void TestDuration(void) {
    // NOTE: at the moment, UNUM_DURATION is still backed by a set of RBNF rules, which don't handle
    // showing fractional seconds.  This test should be updated or replaced
    // when https://unicode-org.atlassian.net/browse/ICU-22487 is fixed.
    double values[] = { 34, 34.5, 1234, 1234.2, 1234.7, 1235, 8434, 8434.5 };
    const UChar* expectedResults[] = { u"34 sec.", u"34 sec.", u"20:34", u"20:34", u"20:35", u"20:35", u"2:20:34", u"2:20:34" };
    
    UErrorCode err = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(UNUM_DURATION, NULL, 0, "en_US", NULL, &err);
    
    if (assertSuccess("Failed to create duration formatter", &err)) {
        UChar actualResult[200];
        
        for (int32_t i = 0; i < UPRV_LENGTHOF(values); i++) {
            unum_formatDouble(nf, values[i], actualResult, 200, NULL, &err);
            if (assertSuccess("Error formatting duration", &err)) {
                assertUEquals("Wrong formatting result", expectedResults[i], actualResult);
            }
        }
        unum_close(nf);
    }
}

#if APPLE_ICU_CHANGES
// rdar://
static const NumParseTestItem altnumParseTests[] = {
    /* name        loc                   lenent src       start val end  status */
    { "zh@hd,50dL","zh@numbers=hanidec", true,  ustr_zh50d,  0,  50,  2, U_USING_DEFAULT_WARNING },
    { "zh@hd,05aL","zh@numbers=hanidec", true,  ustr_zh05a,  0,   5,  2, U_USING_DEFAULT_WARNING },
    { "zh@hd,05dL","zh@numbers=hanidec", true,  ustr_zh05d,  0,   5,  2, U_USING_DEFAULT_WARNING },
    { NULL,        NULL,                 false, NULL,        0,   0,  0, 0 } /* terminator */
};

static void TestParseAltNum(void)
{
    const NumParseTestItem * testPtr;
    for (testPtr = altnumParseTests; testPtr->testname != NULL; ++testPtr) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t	value, position = testPtr->startPos;
        UNumberFormat *nf = unum_open(UNUM_DECIMAL, NULL, 0, testPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "unum_open fails for UNUM_DECIMAL with locale %s, status %s\n", testPtr->locale, myErrorName(status));
            continue;
        }
        unum_setAttribute(nf, UNUM_LENIENT_PARSE, testPtr->lenient);
        value = unum_parse(nf, testPtr->source, -1, &position, &status);
        if ( value != testPtr->value || position != testPtr->endPos || status != testPtr->status ) {
            log_err("unum_parse DECIMAL, locale %s, testname %s, startPos %d: for value / endPos / status, expected %d / %d / %s, got %d / %d / %s\n",
                    testPtr->locale, testPtr->testname, testPtr->startPos,
                    testPtr->value, testPtr->endPos, myErrorName(testPtr->status),
                    value, position, myErrorName(status) );
        }
        unum_close(nf);
    }
}

typedef struct {
    const char*  locale;
    double       value;
    const UChar* formatLimitPrecision;
    const UChar* formatFullPrecision;
} FormatPrecisionItem;

static const FormatPrecisionItem formatPrecisionItems[] = {
    { "en_US", 0.33333333 - 0.00333333, u"0.33", u"0.32999999999999996" },
    { "en_US", 0.07 * 100.0,            u"7",    u"7.000000000000001"   },
    { NULL, 0.0, NULL, NULL }
};

static const UChar* patternTestPrecision = u"#0.################################################################################"; // 80 fraction places

// Currently Apple only
static void TestFormatPrecision(void) {
    const FormatPrecisionItem* itemPtr;
    for (itemPtr = formatPrecisionItems; itemPtr->locale != NULL; itemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UParseError perr;
        UNumberFormat *unum = unum_open(UNUM_PATTERN_DECIMAL, patternTestPrecision, -1, itemPtr->locale, &perr, &status);
        if (U_FAILURE(status)) {
            log_data_err("unum_open UNUM_PATTERN_DECIMAL fails for locale %s: %s\n", itemPtr->locale, u_errorName(status));
            continue;
        }
        UChar ubuf[kUBufSize];
        int32_t ulen;
        UFieldPosition fpos;
        UBool formatFullPrecision;

        formatFullPrecision = (UBool)unum_getAttribute(unum, UNUM_FORMAT_WITH_FULL_PRECISION);
        if (formatFullPrecision) {
            log_err("unum_getAttribute, default for UNUM_FORMAT_WITH_FULL_PRECISION is not false\n");
        } else {
            fpos.field = UNUM_DECIMAL_SEPARATOR_FIELD;
            fpos.beginIndex = 0;
            fpos.endIndex = 0;
            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, itemPtr->value, ubuf, kUBufSize, &fpos, &status);
            if (U_FAILURE(status)) {
                log_err("unum_formatDouble locale %s val %.20f limit precision fails with %s\n", itemPtr->locale, itemPtr->value, u_errorName(status));
            } else if (ulen != u_strlen(itemPtr->formatLimitPrecision) || u_strcmp(ubuf, itemPtr->formatLimitPrecision) != 0) {
                char bbufe[kBBufSize];
                char bbufg[kBBufSize];
                u_strToUTF8(bbufe, kBBufSize, NULL, itemPtr->formatLimitPrecision, -1, &status);
                u_strToUTF8(bbufg, kBBufSize, NULL, ubuf, ulen, &status);
                log_err("unum_formatDouble locale %s val %.20f limit precision, expect %s, get %s\n", itemPtr->locale, itemPtr->value, bbufe, bbufg);
            }
        }

        unum_setAttribute(unum, UNUM_FORMAT_WITH_FULL_PRECISION, true);
        formatFullPrecision = (UBool)unum_getAttribute(unum, UNUM_FORMAT_WITH_FULL_PRECISION);
        if (!formatFullPrecision) {
            log_err("unum_getAttribute, after set UNUM_FORMAT_WITH_FULL_PRECISION is not true\n");
        } else {
            fpos.field = UNUM_DECIMAL_SEPARATOR_FIELD;
            fpos.beginIndex = 0;
            fpos.endIndex = 0;
            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, itemPtr->value, ubuf, kUBufSize, &fpos, &status);
            if (U_FAILURE(status)) {
                log_err("unum_formatDouble locale %s val %.20f full  precision fails with %s\n", itemPtr->locale, itemPtr->value, u_errorName(status));
            } else if (ulen != u_strlen(itemPtr->formatFullPrecision) || u_strcmp(ubuf, itemPtr->formatFullPrecision) != 0) {
                char bbufe[kBBufSize];
                char bbufg[kBBufSize];
                u_strToUTF8(bbufe, kBBufSize, NULL, itemPtr->formatFullPrecision, -1, &status);
                u_strToUTF8(bbufg, kBBufSize, NULL, ubuf, ulen, &status);
                log_err("unum_formatDouble locale %s val %.20f full  precision, expect %s, get %s\n", itemPtr->locale, itemPtr->value, bbufe, bbufg);
            }
        }

        unum_close(unum);
    }
}

// rdar://52538227
static void TestSetSigDigAndRoundIncr(void) {
#if APPLE_ICU_CHANGES
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_PATTERN_DECIMAL, u"#", 1, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL # for \"en_US\" fails with %s\n", u_errorName(status));
    } else {
        static const double value = 1.034000;
        UChar ubuf[kUBufMax];
        char bbuf[kBBufMax];
        int32_t ulen;

        unum_setAttribute(unum, UNUM_MAX_INTEGER_DIGITS, 42);
        unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 0);
        unum_setAttribute(unum, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP); // =6
        unum_setDoubleAttribute(unum, UNUM_ROUNDING_INCREMENT, 0.01);
        unum_setAttribute(unum, UNUM_SIGNIFICANT_DIGITS_USED, 1);
        unum_setAttribute(unum, UNUM_MAX_SIGNIFICANT_DIGITS, 5);
        unum_setAttribute(unum, UNUM_MIN_SIGNIFICANT_DIGITS, 1);

        log_info("unum_getAttribute minSig %d maxSig %d sigUsed %d\n",
            unum_getAttribute(unum,UNUM_MIN_SIGNIFICANT_DIGITS), unum_getAttribute(unum,UNUM_MAX_SIGNIFICANT_DIGITS),
            unum_getAttribute(unum,UNUM_SIGNIFICANT_DIGITS_USED));
        log_info("unum_getDoubleAttribute UNUM_ROUNDING_INCREMENT %f\n", unum_getDoubleAttribute(unum,UNUM_SIGNIFICANT_DIGITS_USED));
        ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
        u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
        if ( U_SUCCESS(status) ) {
            log_info("unum_toPattern (%d): %s\n", ulen, bbuf);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unum, value, ubuf, kUBufMax, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_formatDouble value %.1f status %s\n", value, u_errorName(status));
        } else if (u_strcmp(ubuf,u"1.03") != 0) {
            u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("unum_formatDouble value %.1f expected 1.03, got %s\n", value, bbuf);
        }
        unum_close(unum);
    }
#endif  // APPLE_ICU_CHANGES
}

// rdar://46755430
typedef struct {
    const char*  descrip;
    const char*  locale;
    const UChar* setPosPrefix;
    const UChar* setPosSuffix;
    const UChar* setNegPrefix;
    const UChar* setNegSuffix;
    const UChar* expPosPrefix;
    const UChar* expPosSuffix;
    const UChar* expNegPrefix;
    const UChar* expNegSuffix;
    const UChar* expPattern;
    double       value;
    const UChar* expPosFormat;
    const UChar* expNegFormat;
} SetAffixOnCurrFmtItem;

static const SetAffixOnCurrFmtItem affixOnCurrFmtItems[] = {
    // descrip               loc   sPP   sPS   sNP    sNS   ePP   ePS  eNP    eNS  ePattern                   value  ePosFmt     eNegFmt
    { "01 en set no affix ", "en", NULL, NULL, NULL,  NULL, u"¤", u"", u"-¤", u"", u"¤#,##0.00",              123.4, u"¤123.40", u"-¤123.40" },
    { "02 en set +  prefix", "en", u"$", NULL, NULL,  NULL, u"$", u"", u"-¤", u"", u"$#,##0.00;-¤#,##0.00",   123.4, u"$123.40", u"-¤123.40" },
    { "03 en set +- prefix", "en", u"$", NULL, u"-$", NULL, u"$", u"", u"-$", u"", u"$#,##0.00;'-'$#,##0.00", 123.4, u"$123.40", u"-$123.40" },
    { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0.0,NULL,NULL }
};

// rdar://46755430
static void TestSetAffixOnCurrFmt(void) {
    const SetAffixOnCurrFmtItem* itemPtr;
    for (itemPtr = affixOnCurrFmtItems; itemPtr->descrip != NULL; itemPtr++) {
        UChar ubuf[kUBufMax];
        char  bbufe[kBBufMax];
        char  bbufg[kBBufMax];
        int32_t ulen;
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat* unf = unum_open(UNUM_CURRENCY, NULL, 0, itemPtr->locale, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("locale %s: unum_open UNUM_CURRENCY fails with %s\n", itemPtr->locale, u_errorName(status));
            continue;
        }

        if (itemPtr->setPosPrefix) {
            status = U_ZERO_ERROR;
            unum_setTextAttribute(unf, UNUM_POSITIVE_PREFIX, itemPtr->setPosPrefix, -1, &status);
            if (U_FAILURE(status)) {
                log_err("test %s: unum_setTextAttribute UNUM_POSITIVE_PREFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
            }
        }
        if (itemPtr->setPosSuffix) {
            status = U_ZERO_ERROR;
            unum_setTextAttribute(unf, UNUM_POSITIVE_SUFFIX, itemPtr->setPosSuffix, -1, &status);
            if (U_FAILURE(status)) {
                log_err("test %s: unum_setTextAttribute UNUM_POSITIVE_SUFFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
            }
        }
        if (itemPtr->setNegPrefix) {
            status = U_ZERO_ERROR;
            unum_setTextAttribute(unf, UNUM_NEGATIVE_PREFIX, itemPtr->setNegPrefix, -1, &status);
            if (U_FAILURE(status)) {
                log_err("test %s: unum_setTextAttribute UNUM_NEGATIVE_PREFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
            }
        }
        if (itemPtr->setNegSuffix) {
            status = U_ZERO_ERROR;
            unum_setTextAttribute(unf, UNUM_NEGATIVE_SUFFIX, itemPtr->setNegSuffix, -1, &status);
            if (U_FAILURE(status)) {
                log_err("test %s: unum_setTextAttribute UNUM_NEGATIVE_SUFFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
            }
        }

        status = U_ZERO_ERROR;
        ulen = unum_getTextAttribute(unf, UNUM_POSITIVE_PREFIX, ubuf, kUBufMax, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_getTextAttribute UNUM_POSITIVE_PREFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expPosPrefix)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expPosPrefix, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_getTextAttribute UNUM_POSITIVE_PREFIX expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }
        status = U_ZERO_ERROR;
        ulen = unum_getTextAttribute(unf, UNUM_POSITIVE_SUFFIX, ubuf, kUBufMax, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_getTextAttribute UNUM_POSITIVE_SUFFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expPosSuffix)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expPosSuffix, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_getTextAttribute UNUM_POSITIVE_SUFFIX expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }
        status = U_ZERO_ERROR;
        ulen = unum_getTextAttribute(unf, UNUM_NEGATIVE_PREFIX, ubuf, kUBufMax, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_getTextAttribute UNUM_NEGATIVE_PREFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expNegPrefix)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expNegPrefix, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_getTextAttribute UNUM_NEGATIVE_PREFIX expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }
        status = U_ZERO_ERROR;
        ulen = unum_getTextAttribute(unf, UNUM_NEGATIVE_SUFFIX, ubuf, kUBufMax, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_getTextAttribute UNUM_NEGATIVE_SUFFIX fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expNegSuffix)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expNegSuffix, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_getTextAttribute UNUM_NEGATIVE_SUFFIX expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }
        status = U_ZERO_ERROR;
        ulen = unum_toPattern(unf, false, ubuf, kUBufMax, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_toPattern fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expPattern)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expPattern, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_toPattern expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }

        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unf, itemPtr->value, ubuf, kUBufMax, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_formatDouble positive fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expPosFormat)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expPosFormat, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_formatDouble positive expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }
        status = U_ZERO_ERROR;
        ulen = unum_formatDouble(unf, -1.0*itemPtr->value, ubuf, kUBufMax, NULL, &status);
        if (U_FAILURE(status)) {
            log_err("test %s: unum_formatDouble negative fails with %s\n", itemPtr->descrip, u_errorName(status));
        } else if (u_strcmp(ubuf,itemPtr->expNegFormat)!=0) {
            u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expNegFormat, -1, &status);
            u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
            log_err("test %s: unum_formatDouble negative expect %s, get %s\n", itemPtr->descrip, bbufe, bbufg);
        }

        unum_close(unf);
    }
}

/// rdar://50113359
static const UChar* pat1 = u"#.##E+00;-#.##E+00";
static void TestSciNotationNumbers(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_PATTERN_DECIMAL, NULL, 0, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL with null pattern for \"en_US\" fails with %s\n", u_errorName(status));
    } else {
        unum_applyPattern(unum, false, pat1, u_strlen(pat1), NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_applyPattern fails with %s\n", u_errorName(status));
        } else {
            double value;
            UChar ubuf[kUBufMax];
            char bbuf[kBBufMax];
            int32_t ulen;

            unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 0);
            unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 2147483647);
            log_info("unum_getAttribute minInt %d maxInt %d minFrac %d maxFrac %d\n",
                unum_getAttribute(unum,UNUM_MIN_INTEGER_DIGITS), unum_getAttribute(unum,UNUM_MAX_INTEGER_DIGITS),
                unum_getAttribute(unum,UNUM_MIN_FRACTION_DIGITS), unum_getAttribute(unum,UNUM_MAX_FRACTION_DIGITS));
            ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
            u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
            if ( U_SUCCESS(status) ) {
                log_info("unum_toPattern (%d): %s\n", ulen, bbuf);
            }

            for (value = 10.0; value < 1000000000.0; value *= 10.0) {
                status = U_ZERO_ERROR;
                ulen = unum_formatDouble(unum, value, ubuf, kUBufMax, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("unum_formatDouble value %.1f status %s\n", value, u_errorName(status));
                } else if (u_strncmp(ubuf,u"1E+0",4) != 0) {
                    u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
                    log_err("unum_formatDouble value %.1f expected result to begin with 1E+0, got %s\n", value, bbuf);
                }
            }
        }
        unum_close(unum);
    }
}

// rdar://51601250
static const UChar* patFmt3Exp = u"0.000E+00";
static void TestSciNotationPrecision(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unum = unum_open(UNUM_PATTERN_DECIMAL, NULL, 0, "en_US", NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL with null pattern for \"en_US\" fails with %s\n", u_errorName(status));
    } else {
        unum_applyPattern(unum, false, pat1, u_strlen(pat1), NULL, &status);
        if ( U_FAILURE(status) ) {
            log_err("unum_applyPattern fails with %s\n", u_errorName(status));
        } else {
            double value;
            UChar ubuf[kUBufMax];
            char bexp[kBBufMax];
            char bget[kBBufMax];
            int32_t ulen;

            unum_setAttribute(unum, UNUM_MIN_FRACTION_DIGITS, 3);
            unum_setAttribute(unum, UNUM_MAX_FRACTION_DIGITS, 3);
            unum_setAttribute(unum, UNUM_GROUPING_USED, 0);

            status = U_ZERO_ERROR;
            ulen = unum_toPattern(unum, false, ubuf, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_toPattern fails with %s\n", u_errorName(status));
            } else if (u_strcmp(ubuf,patFmt3Exp) != 0) {
                u_strToUTF8(bget, kBBufMax, NULL, ubuf, ulen, &status);
                log_info("unum_toPattern, get \"%s\"\n", bget);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unum, 0.0, ubuf, ulen, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble fails with %s\n", u_errorName(status));
            } else if (u_strcmp(ubuf,patFmt3Exp) != 0) {
                u_strToUTF8(bexp, kBBufMax, NULL, patFmt3Exp, -1, &status);
                u_strToUTF8(bget, kBBufMax, NULL, ubuf, ulen, &status);
                log_err("unum_formatDouble error, expect \"%s\", get \"%s\"\n", bexp, bget);
            }
        }
        unum_close(unum);
    }
}

// rdar://49808819
static const char* minGroupLocale = "pt_PT"; // has minimumGroupingDigits{"2"}
typedef struct {
    double value;
    const char* string;
    const UChar* expectDecimal;
    const UChar* expectCurrency;
} TestMinGroupItem;
static const TestMinGroupItem minGroupItems[] = {
    { 123.0,      "123.0",      u"123,00",        u"123,00 €" },
    { 1234.0,     "1234.0",     u"1234,00",       u"1234,00 €" },
    { 12345.0,    "12345.0",    u"12 345,00",     u"12 345,00 €" },
    { 123456.0,   "123456.0",   u"123 456,00",    u"123 456,00 €" },
    { 1234567.0,  "1234567.0",  u"1 234 567,00",  u"1 234 567,00 €" },
    { 12345678.0, "12345678.0", u"12 345 678,00", u"12 345 678,00 €" },
    { 0.0, NULL, NULL, NULL } // terminator
};
static void TestMinimumGrouping(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* unumd = unum_open(UNUM_DECIMAL, NULL, 0, minGroupLocale, NULL, &status);
    UNumberFormat* unumc = unum_open(UNUM_CURRENCY, NULL, 0, minGroupLocale, NULL, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("unum_open UNUM_PATTERN_DECIMAL/CURRENCY  for %s fails with %s\n", minGroupLocale, u_errorName(status));
    } else {
        const TestMinGroupItem* itemPtr;
        unum_setAttribute(unumd, UNUM_MIN_FRACTION_DIGITS, 2);
        for (itemPtr = minGroupItems; itemPtr->value != 0.0; itemPtr++) {
            UChar ubuf[kUBufMax];
            char bbufe[kBBufMax];
            char bbufg[kBBufMax];
            int32_t ulen;

            status = U_ZERO_ERROR;
            ulen = unum_formatDecimal(unumd, itemPtr->string, -1, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDecimal DEC for locale %s, value %.1f fails with %s\n", minGroupLocale, itemPtr->value, u_errorName(status));
            } else if (u_strcmp(ubuf, itemPtr->expectDecimal) != 0) {
                u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expectDecimal, -1, &status);
                u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
                log_err("unum_formatDecimal DEC for locale %s, value %.1f, expected \"%s\" but got \"%s\"\n",
                        minGroupLocale, itemPtr->value, bbufe, bbufg);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unumd, itemPtr->value, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble  DEC for locale %s, value %.1f fails with %s\n", minGroupLocale, itemPtr->value, u_errorName(status));
            } else if (u_strcmp(ubuf, itemPtr->expectDecimal) != 0) {
                u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expectDecimal, -1, &status);
                u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
                log_err("unum_formatDouble  DEC for locale %s, value %.1f, expected \"%s\" but got \"%s\"\n",
                        minGroupLocale, itemPtr->value, bbufe, bbufg);
            }

            status = U_ZERO_ERROR;
            ulen = unum_formatDouble(unumc, itemPtr->value, ubuf, kUBufMax, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_formatDouble  CUR for locale %s, value %.1f fails with %s\n", minGroupLocale, itemPtr->value, u_errorName(status));
            } else if (u_strcmp(ubuf, itemPtr->expectCurrency) != 0) {
                u_strToUTF8(bbufe, kBBufMax, NULL, itemPtr->expectCurrency, -1, &status);
                u_strToUTF8(bbufg, kBBufMax, NULL, ubuf, ulen, &status);
                log_err("unum_formatDouble  CUR for locale %s, value %.1f, expected \"%s\" but got \"%s\"\n",
                        minGroupLocale, itemPtr->value, bbufe, bbufg);
            }
        }
        unum_close(unumc);
        unum_close(unumd);
    }
}

// rdar://49120648
static const char* const numSysLocales[] = {
    "en_US", "en_US@numbers=arab", "en_US@numbers=roman", "en_US@numbers=grek", "en_US@numbers=hebr",
    "zh@numbers=hanidec", "zh_Hant@numbers=traditional", "en@numbers=finance", NULL
};
static void TestNumberSystemsMultiplier(void) {
    const char* const* localesPtr = numSysLocales;
    const char* locale;
    while ((locale = *localesPtr++) != NULL) {
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat *unum = unum_open(UNUM_DECIMAL, NULL, 0, locale, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("unum_open UNUM_DECIMAL for %s fails with %s\n", locale, u_errorName(status));
        } else {
            int32_t multiplier = unum_getAttribute(unum, UNUM_MULTIPLIER);
            log_info("for locale %s with UNUM_DECIMAL, UNUM_MULTIPLIER is %d\n", locale, multiplier);
            unum_close(unum);
        }
    }
}

// rdar://39156484
typedef struct {
    const char*  locale;
    const UChar* stringToParse;
    double       expectValue;
    int32_t      expectPos;
} ParseScientificItem;
static const ParseScientificItem parseSciItems[] = {
    { "en_US",              u"4E\u200E+05", 400000.0, 6 },
    { "en_US",              u"4E+05",       400000.0, 5 },
    { "en_US",              u"4E\u200E-02", 0.04,     6 },
    { "en_US",              u"4E-02",       0.04,     5 },
    { "he_IL",              u"4E\u200E+05", 400000.0, 6 },
    { "he_IL",              u"4E+05",       400000.0, 5 },
    { "he_IL",              u"4E\u200E-02", 0.04,     6 },
    { "he_IL",              u"4E-02",       0.04,     5 },
    { "en@numbers=arabext", u"\u06F4\u00D7\u06F1\u06F0^\u200E+\u200E\u06F0\u06F5", 400000.0, 10 },
    { "en@numbers=arabext", u"\u06F4\u00D7\u06F1\u06F0^+\u06F0\u06F5",             400000.0, 8 },
    { NULL,NULL, 0.0, 0 } // terminator
};
static void TestParseScientific(void) {
    const ParseScientificItem* itemPtr;
    for (itemPtr = parseSciItems; itemPtr->locale != NULL; itemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat *unum = unum_open(UNUM_SCIENTIFIC, NULL, 0, itemPtr->locale, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("unum_open UNUM_SCIENTIFIC for %s fails with %s\n", itemPtr->locale, u_errorName(status));
        } else {
            int32_t parsePos = 0;
            double result = unum_parseDouble(unum, itemPtr->stringToParse, -1, &parsePos, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_parseDouble for %s fails with %s\n", itemPtr->locale, u_errorName(status));
            } else if (result != itemPtr->expectValue || parsePos != itemPtr->expectPos) {
                log_err("unum_parseDouble for %s, expected result %.1f pos %d, got %.1f %d\n",
                        itemPtr->locale, itemPtr->expectValue, itemPtr->expectPos, result, parsePos);
            }
            unum_close(unum);
        }
    }
}

// rdar://51985640
typedef struct {
    const char*  locale;
    const UChar* expCurrCode;
} LocaleCurrCodeItem;
static const LocaleCurrCodeItem currCodeItems[] = {
    { "en",              u""    }, // curr ICU has "XXX"
    { "en@currency=EUR", u"EUR" },
    { "en_US",           u"USD" },
    { "en_ZZ",           u"XAG" },
    { "_US",             u"USD" },
    { "_ZZ",             u"XAG" },
    { "us",              u""    }, // curr ICU has "XXX"
    { "",                u""    }, // curr ICU has "XXX"
    { NULL,              NULL   }
};
static void TestCurrForUnkRegion(void) {
    const LocaleCurrCodeItem* itemPtr;
    for (itemPtr = currCodeItems; itemPtr->locale != NULL; itemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat* unum = unum_open(UNUM_CURRENCY, NULL, 0, itemPtr->locale, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("unum_open UNUM_CURRENCY for %s fails with %s\n", itemPtr->locale, u_errorName(status));
        } else {
            UChar ubuf[kUBufMax];
            int32_t ulen = unum_getTextAttribute(unum, UNUM_CURRENCY_CODE, ubuf, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_getTextAttribute UNUM_CURRENCY_CODE for %s fails with %s\n", itemPtr->locale, u_errorName(status));
            } else if (u_strcmp(ubuf,itemPtr->expCurrCode)!=0) {
                char  bexp[kBBufMax];
                char  bget[kBBufMax];
                u_strToUTF8(bexp, kBBufMax, NULL, itemPtr->expCurrCode, -1, &status);
                u_strToUTF8(bget, kBBufMax, NULL, ubuf, ulen, &status);
                log_err("unum_getTextAttribute UNUM_CURRENCY_CODE error for %s, expect \"%s\" but get \"%s\"\n", itemPtr->locale, bexp, bget);
            }
            status = U_ZERO_ERROR;
            unum_setTextAttribute(unum, UNUM_CURRENCY_CODE, u"XXX", 3, &status);
            if ( U_FAILURE(status) ) {
                log_err("unum_setTextAttribute UNUM_CURRENCY_CODE for %s fails with %s\n", itemPtr->locale, u_errorName(status));
            } else {
                ulen = unum_getTextAttribute(unum, UNUM_CURRENCY_CODE, ubuf, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("unum_getTextAttribute UNUM_CURRENCY_CODE XXX for %s fails with %s\n", itemPtr->locale, u_errorName(status));
                } else if (u_strcmp(ubuf,u"XXX")!=0) {
                    char  bget[kBBufMax];
                    u_strToUTF8(bget, kBBufMax, NULL, ubuf, ulen, &status);
                    log_err("unum_getTextAttribute UNUM_CURRENCY_CODE error for %s, expect XXX but get \"%s\"\n", itemPtr->locale, bget);
                }
            }
            unum_close(unum);
        }
    }
}

// rdar://51672521
#include <stdio.h>
#if U_PLATFORM_IS_DARWIN_BASED
#include <unistd.h>
#include <mach/mach_time.h>
#define GET_START() mach_absolute_time()
#define GET_DURATION(start, info) ((mach_absolute_time() - start) * info.numer)/info.denom
#elif !U_PLATFORM_HAS_WIN32_API
#include <unistd.h>
#include "putilimp.h"
#define GET_START() (uint64_t)uprv_getUTCtime()
#define GET_DURATION(start, info) ((uint64_t)uprv_getUTCtime() - start)
#else
#include "putilimp.h"
#define GET_START() (unsigned long long)uprv_getUTCtime()
#define GET_DURATION(start, info) ((unsigned long long)uprv_getUTCtime() - start)
#endif

// rdar://51672521
static void TestFormatDecPerf(void) {
    static const char* locales[] =
        { "en_US", "ar_EG", "ar_EG@numbers=latn", "zh_CN@numbers=traditional", NULL };
    static const UNumberFormatStyle styles[] =
        { UNUM_DECIMAL, UNUM_CURRENCY, UNUM_PATTERN_DECIMAL, UNUM_FORMAT_STYLE_COUNT };
    static const char* values[] =
        { "123.4", "234.5", "345.6", "456.7", NULL };
    static const UChar* pattern = u"#";
#if U_PLATFORM_IS_DARWIN_BASED
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
#endif
    const char** localesPtr = locales;
    const char* locale;
    while ((locale = *localesPtr++) != NULL) {
        const UNumberFormatStyle* stylesPtr = styles;
        UNumberFormatStyle style;
        while ((style = *stylesPtr++) < UNUM_FORMAT_STYLE_COUNT) {
#if !U_PLATFORM_HAS_WIN32_API
            uint64_t start, duration;
#else
            unsigned long long start, duration;
#endif
            UErrorCode status = U_ZERO_ERROR;
            const UChar* patternPtr = (style == UNUM_PATTERN_DECIMAL)? pattern: NULL;
            int32_t patternLen = (style == UNUM_PATTERN_DECIMAL)? u_strlen(pattern): 0;
            UNumberFormat *unum;

            start = GET_START();
            unum = unum_open(style, patternPtr, patternLen, locale, NULL, &status);
            duration = GET_DURATION(start, info);
            log_info("== start locale %s, style %d\n", locale, (int)style);
            if ( U_FAILURE(status) ) {
                log_data_err("unum_open fails with %s\n", u_errorName(status));
            } else {
                log_info("unum_open nsec %5llu\n", duration);
                const char** valuesPtr = values;
                const char* value;
                while ((value = *valuesPtr++) != NULL) {
                    UChar ubuf[kUBufMax];
                    char bbuf[kBBufMax];
                    int32_t ulen;

                    status = U_ZERO_ERROR;
                    start = GET_START();
                    ulen = unum_formatDecimal(unum, value, strlen(value), ubuf, kUBufMax, NULL, &status);
                    duration = GET_DURATION(start, info);
                    u_strToUTF8(bbuf, kBBufMax, NULL, ubuf, ulen, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("unum_formatDecimal %s fails with %s\n", value, u_errorName(status));
                    } else {
                        log_info("unum_formatDecimal %s, nsec %5llu, result %s\n", value, duration, bbuf);
                    }
                }
                unum_close(unum);
            }
        }
    }
}

// rdar://54886964 and rdar://62544359

static void TestCountryFallback(void) {
    // Locales to check fallback behavior for-- the first column (subscript % 3 == 0)
    // is the locale to test with; the second column (subscript % 3 == 1) is the locale
    // whose number-formatting properties we should be matching when we use the test locale,
    // and the third column (subscript % 3 == 2) is the locale we should be inheriting
    // certain language-dependent symbols from (generally, the locale you'd normally get
    // as a fallback).
    char* testLocales[] = {
        // locales we still have synthetic resources for that were working okay to begin with
//        "en_BG", "bg_BG", "en_150", // en_BG exists and has "0.00 ¤" -- en_ 150 has "#,##0.00 ¤"
        "en_CN", "zh_CN", "en_001",
        "en_CZ", "cs_CZ", "en_CZ",
        "en_JP", "ja_JP", "en_001@currency=JPY",
        "en_KR", "ko_KR", "en_001@currency=KRW",
        "en_TH", "th_TH", "en_001",
        "en_TW", "zh_TW", "en_001",

        // locales that were okay except for currency-pattern issues
        // (note that we have to specify the language locale as en, not en_150-- the en_150 currency format is wrong too)
        "en_EE", "et_EE", "en",
        "en_LT", "lt_LT", "en_LT",
        "en_LV", "lv_LV", "en",
        "en_PT", "pt_PT", "en",
        "en_SK", "sk_SK", "en_SK",
        "en_FR", "fr_FR", "en_FR",

        // locales we still have synthetic resources for that we had to modify to match the country-fallback results
        "en_AL", "sq_AL", "en_150@currency=ALL",
        "en_AR", "es_AR", "en_AR",
        "en_BD", "bn_BD@numbers=latn", "en_BD",
//        "en_BR", "pt_BR", "en_001", // en_BR exists and has "¤ #,##0.00" -- en_001 has "¤#,##0.00"
        "en_CL", "es_CL", "en_CL",
        "en_CO", "es_CO", "en_CO",
        "en_GR", "el_GR", "en_150",
        "en_HU", "hu_HU", "en_150",
        "en_ID", "id_ID", "en_001@currency=IDR",
//        "en_MM", "my_MM@numbers=latn", "en_001", // en_MM exists and has "#,##0 ¤" -- en_001 "¤#,##0.00"
//        "en_MV", "dv_MV", "en_001", // en_MV exists and has "¤ #,##0.00" -- en_001 has "¤#,##0.00"
        "en_MX", "es_MX", "en_001",
        "en_RU", "ru_RU", "en_RU",
        "en_TR", "tr_TR", "en_TR",
        "en_UA", "uk_UA", "en_150",
        "es_AG", "en_AG", "es_AG",
        "es_BB", "en_BB", "es_BB",
        "es_BM", "en_BM", "es_BM",
        "es_BQ", "nl_BQ", "es_BQ",
        "es_BS", "en_BS", "es_BS",
        "es_CA", "en_CA", "es_CA",
        "es_CW", "nl_CW", "es_CW",
        "es_DM", "en_DM", "es_DM",
        "es_GD", "en_GD", "es_GD",
        "es_GY", "en_GY", "es_GY",
        "es_HT", "fr_HT", "es_HT",
        "es_KN", "en_KN", "es_KN",
        "es_KY", "en_KY", "es_KY",
        "es_LC", "en_LC", "es_LC",
        "es_TC", "en_TC", "es_TC",
        "es_TT", "en_TT", "es_TT",
        "es_VC", "en_VC", "es_VC",
        "es_VG", "en_VG", "es_VG",
        "es_VI", "en_VI", "es_VI",

        // locales we used to have synthetic resources for but don't anymore
        "en_AD", "ca_AD", "en_150",
        "en_BA", "bs_BA", "en_150",
        "en_ES", "es_ES", "en_150",
        "en_HR", "hr_HR", "en_150",
        "en_IS", "is_IS", "en_150@currency=ISK",
        "en_IT", "it_IT", "en_150",
        "en_LU", "fr_LU", "en_150",
        "en_ME", "sr_ME", "en_150",
        "en_NO", "nb_NO", "en_NO",
        "en_PL", "pl_PL", "en_150",
        "en_RO", "ro_RO", "en_150",
        "en_RS", "sr_RS", "en_150@currency=RSD",
//        "en_SA", "ar_SA@numbers=latn", "en", // en_SA exists and has "¤ #,##0.00" -- en_001 has "¤#,##0.00"
        "es_AI", "en_AI", "es_419",
        "es_AW", "nl_AW", "es_419",
        "es_BL", "fr_BL", "es_419",
        "es_FK", "en_FK", "es_419",
        "es_GF", "fr_GF", "es_419",
        "es_GL", "kl_GL", "es_419",
        "es_GP", "fr_GP", "es_419",
        "es_MF", "fr_MF", "es_419",
        "es_MQ", "fr_MQ", "es_419",
        "es_MS", "en_MS", "es_419",
        "es_PM", "fr_PM", "es_419",
        "es_SR", "nl_SR", "es_419",
        "es_SX", "en_SX", "es_419",

        // locales we have synthetic resources for, but they come from open-source ICU
        "en_DE", "de_DE", "en_DE",
        "en_ER", "ti_ER", "en_001",
        "en_GH", "ak_GH", "en_001",
        "en_HK", "zh_HK", "en_001",
        "en_ME", "sr_ME", "en_150",
        "en_MO", "zh_MO", "en_001",
        "en_MT", "mt_MT", "en_001",
        "en_MY", "ms_MY", "en_001",
//        "en_NL", "nl_NL", "en_150", // en_NL exists and has "¤ #,##0.00;¤ -#,##0.00" -- en_150 has "#,##0.00 ¤"
        "en_PH", "fil_PH", "en_001",
        "en_ZW", "sn_ZW", "en_001",
        "es_US", "en_US", "es_US",
        
        // locales where we're adding a NEW synthetic resource bundle, to PREVENT us from falling back by country
        "en_BN", "en_001", "en_001", // rdar://69272236
        
        // locales specifically mentioned in Radars
        "fr_US", "en_US", "fr",     // rdar://54886964
//        "en_TH", "th_TH", "en",   // rdar://29299919 (listed above)
//        "en_BG", "bg_BG", "en",   // rdar://29299919 (listed above)
        "en_LI", "de_LI", "en_150", // rdar://29299919
        "en_MC", "fr_MC", "en_150", // rdar://29299919
        "en_MD", "ro_MD", "en_150", // rdar://29299919
        "en_VA", "it_VA", "en_150", // rdar://29299919
        "fr_GB", "en_GB", "fr",     // rdar://36020946
        "fr_CN", "zh_CN", "fr",     // rdar://50083902
        "es_IE", "en_IE", "es",     // rdar://58733843
        "en_LB", "ar_LB@numbers=latn", "en_001@currency=LBP", // rdar://66609948

        // tests for situations where the default number system is different depending on whether you
        // fall back by language or by country:
//        "en_SA", "ar_SA@numbers=latn", "en", // (listed above)
        "ar_US", "ar@numbers=latn", "ar@numbers=latn", // rdar://116185298
        "en_SA@numbers=arab", "ar_SA", "en@numbers=arab",
        "ar_US@numbers=latn", "en_US", "ar@numbers=latn",

        // tests for situations where the original locale ID specifies a script:
        "sr_Cyrl_SA", "ar_SA@numbers=latn", "sr_Cyrl",
        "ru_Cyrl_BA", "bs_Cyrl_BA", "ru",

        // a few additional arbitrary combinations:
        "ja_US", "en_US", "ja",
        "fr_DE", "de_DE", "fr",
        "de_FR", "fr_FR", "de",
        "es_TW", "zh_TW", "es",

        // not in the resources, but to exercise the new ures_openWithCountryFallback() logic (AQ's default language is "und")
        "fr_AQ", "en", "fr",

        // test to make sure that nothing goes wrong if language and country fallback both lead to the same resource
        // (This won't happen for any "real" locales, because ICU has resources for all of them, but we can fake it with
        // a nonexistent country code such as QQ.)
        "en_QQ", "en", "en"
    };

    for (int32_t i = 0; i < (sizeof(testLocales) / sizeof(char*)); i += 3) {
        const char* testLocale = testLocales[i];
        const char* fallbackLocale = testLocales[i + 1];
        const char* languageLocale = testLocales[i + 2];
        char errorMessage[200];
        UErrorCode err = U_ZERO_ERROR;

        // Check that the decimal, percentage, currency, and scientific formatting patterns
        // for the test locale are the same as those for the fallback locale.
        for (UNumberFormatStyle style = UNUM_DECIMAL; style <= UNUM_SCIENTIFIC; ++style) {
            err = U_ZERO_ERROR;
            UNumberFormat* testFormatter = unum_open(style, NULL, -1, testLocale, NULL, &err);
            UNumberFormat* fallbackFormatter = unum_open(style, NULL, -1, fallbackLocale, NULL, &err);
            UNumberFormat* languageLocaleFormatter = unum_open(style, NULL, -1, languageLocale, NULL, &err);

            sprintf(errorMessage, "Error creating formatters for %s and %s", testLocale, fallbackLocale);
            if (assertSuccess(errorMessage, &err)) {
                UChar testPattern[100];
                UChar fallbackPattern[100];

                unum_toPattern(testFormatter, false, testPattern, 100, &err);
                if (style == UNUM_PERCENT || style == UNUM_CURRENCY) {
                    unum_toPattern(languageLocaleFormatter, false, fallbackPattern, 100, &err);
                } else {
                    unum_toPattern(fallbackFormatter, false, fallbackPattern, 100, &err);
                }

                if (assertSuccess("Error getting number format patterns", &err)) {
                    sprintf(errorMessage, "In %s, formatting pattern for style %d doesn't match", testLocale, style);
                    assertUEquals(errorMessage, fallbackPattern, testPattern);
                }
            }
            unum_close(testFormatter);
            unum_close(fallbackFormatter);
            unum_close(languageLocaleFormatter);
        }

        // Check that all of the number formatting symbols for the test locale (except for the
        // currency, exponential, infinity, and NaN symbols) are the same as those for the fallback locale.
        UNumberFormat* testFormatter = unum_open(UNUM_DECIMAL, NULL, -1, testLocale, NULL, &err);
        UNumberFormat* fallbackFormatter = unum_open(UNUM_DECIMAL, NULL, -1, fallbackLocale, NULL, &err);
        UNumberFormat* languageLocaleFormatter = unum_open(UNUM_DECIMAL, NULL, -1, languageLocale, NULL, &err);
        if (assertSuccess("Error creating formatters", &err)) {
            for (UNumberFormatSymbol symbol = UNUM_DECIMAL_SEPARATOR_SYMBOL; symbol < UNUM_FORMAT_SYMBOL_COUNT; ++symbol) {
                UBool compareToFallbackLocale = true;
                switch (symbol) {
                    case UNUM_CURRENCY_SYMBOL:
                    case UNUM_INTL_CURRENCY_SYMBOL:
                    case UNUM_MONETARY_SEPARATOR_SYMBOL:
                    case UNUM_MONETARY_GROUPING_SEPARATOR_SYMBOL:
                    case UNUM_APPROXIMATELY_SIGN_SYMBOL:
                        // we don't do anything special with currency here, and the approximately-equal
                        // symbol is internal (and I'm not sure what it's doing)
                        continue;
                    case UNUM_PLUS_SIGN_SYMBOL:
                    case UNUM_MINUS_SIGN_SYMBOL:
                    case UNUM_PERCENT_SYMBOL:
                    case UNUM_EXPONENTIAL_SYMBOL:
                    case UNUM_INFINITY_SYMBOL:
                    case UNUM_NAN_SYMBOL:
                        // unlike most number symbols, these should follow the language
                        compareToFallbackLocale = false;
                        break;
                    default:
                        // everything else follows the country
                        compareToFallbackLocale = true;
                        break;
                }

                UChar testSymbol[50];
                UChar fallbackSymbol[50];

                err = U_ZERO_ERROR;
                unum_getSymbol(testFormatter, symbol, testSymbol, 50, &err);
                if (compareToFallbackLocale) {
                    unum_getSymbol(fallbackFormatter, symbol, fallbackSymbol, 50, &err);
                } else {
                    unum_getSymbol(languageLocaleFormatter, symbol, fallbackSymbol, 50, &err);
                }

                if (assertSuccess("Error getting number format symbol", &err)) {
                    sprintf(errorMessage, "In %s, formatting symbol #%d doesn't match the fallback", testLocale, symbol);
                    assertUEquals(errorMessage, fallbackSymbol, testSymbol);
                }
            }
        }
        unum_close(testFormatter);
        unum_close(fallbackFormatter);
        unum_close(languageLocaleFormatter);
    }
}

static void _checkForRTLRun(const char* locale, UBiDi* bidi, UChar* text, UErrorCode* err) {
    // Helper function used by TestCurrencyBidiOrdering() below.
    //
    // We want to make sure that the currency symbol in a formatted currency value in the Hebrew locale
    // always appears to the left of the value itself-- the original bug had the currency symbol
    // displaying to the right when the value was negative and the currency symbol consisted of Latin
    // letters.  This was happening because the space between the number and the currency was having
    // its directionality resolved to LTR because the nearest strong-directionality characters on
    // either side of it were both LTR (there was a LRM before the minus sign, and the letters in the
    // currency symbol were strong LTR).  This caused the entire formatted value (except for the RLM at
    // the very beginning) to be part of the same LTR run.
    //
    // To verify that we're not doing that, we use UBiDi to resolve bidi levels in the formatted result
    // and check to make sure there's at least one RTL run somewhere in the result (other than the RLM at
    // the very beginning).  The RTL run will have an embedding level of 1, so a set of levels with a 1
    // anywhere other than the first character passes the test; a set of levels with all 2s fails.
    //
    // rdar://106059271 Not sure some aspects of this test are valid when we have the following:
    // - A locale like ar_AE that uses 'latn' digits by default
    // - A currency like PEN which in that locale uses the ISO 4217 code "PEN" (so Latn letters)
    // - A negative value, since the '- is preceded by a LRM.
    // Then the valid result is u"\u200F\u200E-1,234.56\u00A0PEN" which has no RTL chars after the first.
    //
    ubidi_setPara(bidi, text, -1, UBIDI_DEFAULT_LTR, NULL, err);
    if (assertSuccess("Error resolving bidi levels", err)) {
        uint32_t textLen = u_strlen(text);
        bool sawLTR = false;

        for (int32_t i = 1; !sawLTR && i < textLen; i++) {
            if (ubidi_getLevelAt(bidi, i) == 1) {
                sawLTR = true;
            }
        }
        if (!sawLTR) {
            log_err("No resolved LTR runs in locale %s, text \"%s\"\n", locale, austrdup(text));
        }
    }
}

static void _doCurrencyBidiOrderingTest(const char* locale, UChar* result, UBiDi* bidi, UErrorCode* err) {
    UNumberFormat* nf = unum_open(UNUM_CURRENCY, NULL, 0, locale, NULL, err);
    unum_formatDouble(nf, 1234.56, result, 50, NULL, err);
    _checkForRTLRun(locale, bidi, result, err);
    unum_formatDouble(nf, -1234.56, result, 50, NULL, err);
    if (uprv_strcmp(locale, "ar_AE@currency=PEN") != 0) { // rdar://106059271
        _checkForRTLRun(locale, bidi, result, err);
    }
    unum_close(nf);
}

static void TestCurrencyBidiOrdering(void) {
    // see explanation of this test in _checkForRTLRun() above
    UErrorCode err = U_ZERO_ERROR;
    UBiDi* bidi = ubidi_open();
    UChar result[50];
    
    // tests for rdar://76160456
    _doCurrencyBidiOrderingTest("he_IL", result, bidi, &err);
    _doCurrencyBidiOrderingTest("he_IL@currency=JMD", result, bidi, &err);

    // tests for rdar://57790549 and rdar://60348009
    _doCurrencyBidiOrderingTest("ar_AE", result, bidi, &err);
    _doCurrencyBidiOrderingTest("ar_AE@currency=PEN", result, bidi, &err);
    _doCurrencyBidiOrderingTest("ar_AE@currency=USD", result, bidi, &err);
    _doCurrencyBidiOrderingTest("ar_SA", result, bidi, &err);
    _doCurrencyBidiOrderingTest("ar_SA@currency=PEN", result, bidi, &err);
    _doCurrencyBidiOrderingTest("ar_SA@currency=USD", result, bidi, &err);

    ubidi_close(bidi);
}

static void TestPrecisionOverMultipleCalls(void) {
    UErrorCode err = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(UNUM_DECIMAL, NULL, 0, "en_CH", NULL, &err);
    unum_setAttribute(nf, UNUM_MAX_INTEGER_DIGITS, 2000000000);
    unum_setAttribute(nf, UNUM_MIN_INTEGER_DIGITS, 1);
    unum_setAttribute(nf, UNUM_MIN_FRACTION_DIGITS, 0);
    unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, 340);
    unum_setAttribute(nf, UNUM_LENIENT_PARSE, 0);
    unum_setAttribute(nf, UNUM_MINIMUM_GROUPING_DIGITS, 1);
    unum_setAttribute(nf, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);

    UChar result[100];
    unum_formatDouble(nf, -6.6000000000000005, result, 100, NULL, &err);
    assertSuccess("Failure creating number formatter", &err);
    assertUEquals("Wrong result (1st try)", u"-6.6", result);

    unum_formatDouble(nf, -6.6000000000000005, result, 100, NULL, &err);
    assertUEquals("Wrong result (2nd try)", u"-6.6", result);

    unum_formatDouble(nf, -6.6000000000000005, result, 100, NULL, &err);
    assertUEquals("Wrong result (3rd try)", u"-6.6", result);

    unum_formatDouble(nf, -6.6000000000000005, result, 100, NULL, &err);
    assertUEquals("Wrong result (4th try)", u"-6.6", result);

    unum_formatDouble(nf, -6.6000000000000005, result, 100, NULL, &err);
    assertUEquals("Wrong result (5th try)", u"-6.6", result);

    unum_close(nf);
}

static void TestCurrencySymbol(void) {
    // rdar://79879611, rdar://77057954, others as noted below
    UChar* testCases[] = {
        u"en_US", u"$1,234.56",     // baseline
        u"ar_SA", u"\u200f\u0661\u066c\u0662\u0663\u0664\u066b\u0665\u0666\u00a0\u0631.\u0633.\u200f", // baseline
        u"en_SA", u"SAR 1,234.56",  // don't use native currency symbol because it's letters in a different script
        u"ar_US", u"\u200f1,234.56\u00a0$", // rdar://116185298
        u"en_IL", u"₪1,234.56",     // can use native currency symbol because its script code is "common"
        u"he_US", u"\u200f1,234.56\u00a0\u200f$",   // can use native currency symbol because its script code is "common"
        u"th_TH", u"฿1,234.56",     // baseline
        u"fr_TH", u"1,234.56 ฿",    // can use native currency symbol because its gc is Sc (rdar://79879611)
        u"km_KH", u"1,234.56៛",     // baseline
        u"en_KH", u"៛1,234.56",     // can use native currency symbol because its gc is Sc (rdar://79879611)
        // tests for rdar://77057954 (wrong symbol for CNY when region is JP (should get ¥, got 元)
        u"zh_JP@currency=CNY", u"CN¥1,234.56",
        u"zh_Hans_JP@currency=CNY", u"CN¥1,234.56",
        u"zh_Hant_JP@currency=CNY", u"CN¥1,234.56", // per rdar://77057954 this is the desired value 
        u"ja_JP@currency=CNY", u"元 1,234.56",
        u"en_JP@currency=CNY", u"CN¥1,234.56",
        // rdar://88513120
        u"en_CV", u"​ 1 234$56", // decimal separator = actual currency symbol, set currency symbol to ZWSP
        // rdar://97100652
        u"zh-Hant_US@currency=USD", u"$1,234.56",
        // rdar://107639369
        u"en_NL", u"€ 1.234,56",
        u"en_BE", u"€ 1.234,56",
        // rdar://107636645
        u"en_AT", u"€ 1.234,56",
        u"zh-Hant_US@currency=USD", u"$1,234.56",
        // rdar://107676538, fixed in rdar://108767828
        u"fr_CH@currency=CHF", u"CHF 1 234,56",
        // rdar://107711452
        u"en_ID@currency=IDR", u"Rp 1.235", // IDR has digits="0"
        // rdar://102824421
        u"es_VE", u"Bs.D 1.234,56" // want default to be VED (Bs.D), not VES (Bs.S)
    };
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i += 2) {
        char* locale = austrdup(testCases[i]);
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(UNUM_CURRENCY, NULL, 0, locale, NULL, &err);
        UChar result[50];
        unum_formatDouble(nf, 1234.56, result, 50, NULL, &err);
        
        if (assertSuccess(locale, &err)) {
            assertUEquals(locale, testCases[i + 1], result);
        }
        unum_close(nf);
    }
}

// rdar://107276400, rdar://106566783, and rdar://115839570
static void TestRgSubtag(void) {
    UChar* testData[] = {
        u"fr_FR",                        u"CAD", u"1234.56", u"1 234,56 $CA",
        u"fr_CA" ,                       u"CAD", u"1234.56", u"1 234,56 $",
        u"fr_FR@rg=CAzzzz",              u"CAD", u"1234.56", u"1 234,56 $",
        u"fr_FR",                        u"",    u"1234.56", u"1 234,56 €",
        u"fr_CA" ,                       u"",    u"1234.56", u"1 234,56 $",
        u"fr_FR@rg=CAzzzz",              u"",    u"1234.56", u"1 234,56 $",
        u"fr_FR@currency=CAD",           u"",    u"1234.56", u"1 234,56 $CA",
        u"fr_CA@currency=CAD" ,          u"",    u"1234.56", u"1 234,56 $",
        u"fr_FR@rg=CAzzzz;currency=CAD", u"",    u"1234.56", u"1 234,56 $",
        
        u"en_US",                        u"USD", u"10.99",   u"$10.99",
        u"en_US@rg=USzzzz",              u"USD", u"10.99",   u"$10.99",
        u"es_MX",                        u"USD", u"10.99",   u"USD 10.99",
        u"es_US",                        u"USD", u"10.99",   u"$10.99",
        u"es_MX@rg=USzzzz",              u"USD", u"10.99",   u"$10.99",

        u"ar_SA",                        u"USD", u"10.99",   u"\u200f١٠٫٩٩\u00a0US$",
        u"ar_SA@rg=USzzzz",              u"USD", u"10.99",   u"\u200f10.99\u00a0$", // rdar://116185298
        u"ar_US",                        u"USD", u"10.99",   u"\u200f10.99\u00a0$", // rdar://116185298
        u"ar_US@rg=USzzzz;numbers=latn", u"USD", u"7.99",    u"\u200f7.99\u00a0$",
        u"ar_US@numbers=latn",           u"USD", u"7.99",    u"\u200f7.99\u00a0$",
        
        u"ru_RU@rg=USzzzz",              u"USD", u"10.99",   u"10.99\u00a0$",
        u"ru_US",                        u"USD", u"10.99",   u"10.99\u00a0$",
        u"zh_CN@rg=USzzzz",              u"USD", u"10.99",   u"$10.99",
        u"zh_Hans_US",                   u"USD", u"10.99",   u"$10.99",
        u"fr_FR@rg=USzzzz",              u"USD", u"10.99",   u"10.99\u00a0$",
        u"fr_US",                        u"USD", u"10.99",   u"10.99\u00a0$",
        u"ko_KR@rg=USzzzz",              u"USD", u"10.99",   u"$10.99",
        u"ko_US",                        u"USD", u"10.99",   u"$10.99",
        u"pt_BR@rg=USzzzz",              u"USD", u"10.99",   u"$\u00a010.99",
//        u"pt-BR_US",                     u"USD", u"10.99",   u"$\u00a010.99", // this locale syntax doesn't work in ICU4C
        u"vi_VN@rg=USzzzz",              u"USD", u"10.99",   u"10.99\u00a0$",
        u"vi_US",                        u"USD", u"10.99",   u"10.99\u00a0$",
        u"zh_TW@rg=USzzzz",              u"USD", u"10.99",   u"$10.99",
        u"zh_Hant_US",                   u"USD", u"10.99",   u"$10.99",
        
        // test for rdar://115839570
        u"en_DE",                        u"EUR", u"1234.56", u"1.234,56 €",
        u"en_US@rg=DEzzzz",              u"EUR", u"1234.56", u"1.234,56 €",
    };
    
    UErrorCode err = U_ZERO_ERROR;
    UChar result[200];
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testData); i+= 4) {
        char* locale = austrdup(testData[i]);
        UChar* currencyCode = testData[i + 1];
        double amount = atof(austrdup(testData[i + 2]));
        UChar* expectedResult = testData[i + 3];
        
        err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(UNUM_CURRENCY, NULL, 0, locale, NULL, &err);
        if (u_strlen(currencyCode) > 0) {
            unum_setTextAttribute(nf, UNUM_CURRENCY_CODE, currencyCode, -1, &err);
        }
        
        char errorMessage[100];
        snprintf(errorMessage, 100, "Failed to create number formatter for %s", locale);
        if (assertSuccess(errorMessage, &err)) {
            unum_formatDouble(nf, amount, result, 200, NULL, &err);
            if (assertSuccess("Error formatting", &err)) {
                snprintf(errorMessage, 100, "Wrong result for %s", locale);
                assertUEquals(errorMessage, expectedResult, result);
            }
        }
        unum_close(nf);
    }
}

// rdar://108506710
static void TestThousandsSeparator(void) {
    const char* locales[] = { "en_US", "zh-Hans", "zh_US", "zh-Hans_US" };
    
    // save off the regular default locale before changing it, then change it to en_US_POSIX
    // (we know en_US_POSIX has a different decimal format than most of the other locales)
    char oldDefaultLocale[ULOC_FULLNAME_CAPACITY];
    UErrorCode locErr = U_ZERO_ERROR;
    uprv_strcpy(oldDefaultLocale, uloc_getDefault());
    uloc_setDefault("en_US_POSIX", &locErr);
    if (!assertSuccess("Failed to set default locale", &locErr)) {
        return;
    }
    
    // do the actual test, which is really testing resource-bundle fallback more than number formatting
    for (int32_t i = 0; i < UPRV_LENGTHOF(locales); i++) {
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(UNUM_DECIMAL, NULL, 0, locales[i], NULL, &err);
        
        UChar pattern[200];
        unum_toPattern(nf, false, pattern, 200, &err);
        if (assertSuccess("Failure formatting", &err)) {
            assertUEquals("Wrong pattern", u"#,##0.###", pattern);
        }
        unum_close(nf);
    }
    
    // finally set the default locale back to what it was
    uloc_setDefault(oldDefaultLocale, &locErr);
}

// rdar://112745117
static void TestSigDigVsRounding(void) {
    UErrorCode err = U_ZERO_ERROR;
    UChar result[200];
    
    UNumberFormat* nf = unum_open(UNUM_PATTERN_DECIMAL, NULL, 0, "en_US", NULL, &err);
    unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, 0);
    unum_setAttribute(nf, UNUM_SIGNIFICANT_DIGITS_USED, 1);
    unum_setAttribute(nf, UNUM_MIN_SIGNIFICANT_DIGITS, 0);
    unum_setAttribute(nf, UNUM_MAX_SIGNIFICANT_DIGITS, 5);
    unum_setAttribute(nf, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
    unum_setDoubleAttribute(nf, UNUM_ROUNDING_INCREMENT, 0.01);
    
    unum_formatDouble(nf, 14.54, result, 200, NULL, &err);
    
    assertUEquals("Wrong result", u"14.54", result);
    
    unum_close(nf);
}

// rdar://116185298
static void TestDefaultArabicNumberingSystem(void) {
    const UChar* testCases[] = {
        u"ar",    u"1,234.56",
        u"ar_SA", u"١٬٢٣٤٫٥٦",
        u"ar_AE", u"1,234.56",
        u"ar_GB", u"1,234.56",
    };
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i += 2) {
        char* locale = austrdup(testCases[i]);
        const UChar* expectedResult = testCases[i + 1];
        
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormat* nf = unum_open(UNUM_DECIMAL, NULL, 0, locale, NULL, &err);
        
        UChar result[100];
        unum_formatDouble(nf, 1234.56, result, 100, NULL, &err);
        if (assertSuccess(locale, &err)) {
            assertUEquals(locale, expectedResult, result);
        }
        unum_close(nf);
    }
}

static void TestMinGroupingDigits(void) {
    UChar result[256];
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat *numFmt = unum_open(UNUM_DECIMAL, NULL, 0, "es", NULL, &status);
    unum_format(numFmt, 1234, result, 256, NULL, &status);
    assertUEquals("default", u"1234", result);
    unum_format(numFmt, 12345, result, 256, NULL, &status);
    assertUEquals("default", u"12.345", result);

    unum_applyPattern(numFmt, false, u"#", -1, NULL, &status); // <---???
    unum_format(numFmt, 1234, result, 256, NULL, &status);
    assertUEquals("pattern", u"1234", result);
    unum_format(numFmt, 12345, result, 256, NULL, &status);
    assertUEquals("pattern", u"12345", result);

    unum_setAttribute(numFmt, UNUM_GROUPING_USED, true);
    unum_setAttribute(numFmt, UNUM_MINIMUM_GROUPING_DIGITS, 1);
    unum_format(numFmt, 1234, result, 256, NULL, &status);
    assertUEquals("min grouping 1", u"1.234", result);
    unum_format(numFmt, 12345, result, 256, NULL, &status);
    assertUEquals("min grouping 1", u"12.345", result);

    unum_setAttribute(numFmt, UNUM_MINIMUM_GROUPING_DIGITS, UNUM_MINIMUM_GROUPING_DIGITS_AUTO);
    unum_format(numFmt, 1234, result, 256, NULL, &status);
    assertUEquals("min grouping UNUM_MINIMUM_GROUPING_DIGITS_AUTO", u"1234", result);
    unum_format(numFmt, 12345, result, 256, NULL, &status);
    assertUEquals("min grouping UNUM_MINIMUM_GROUPING_DIGITS_AUTO", u"12.345", result);

    unum_close(numFmt);
}


#endif  // APPLE_ICU_CHANGES

#endif /* #if !UCONFIG_NO_FORMATTING */
