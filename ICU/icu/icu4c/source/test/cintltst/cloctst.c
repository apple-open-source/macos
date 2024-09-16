// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*****************************************************************************
*
* File CLOCTST.C
*
* Modification History:
*        Name                     Description 
*     Madhu Katragadda            Ported for C API
******************************************************************************
*/
#include "cloctst.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"
#include "uparse.h"
#include "uresimp.h"
#include "uassert.h"
#if APPLE_ICU_CHANGES
// rdar://
#include "cmemory.h"
#endif  // APPLE_ICU_CHANGES

#include "unicode/putil.h"
#include "unicode/ubrk.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/udat.h"
#include "unicode/uloc.h"
#include "unicode/umsg.h"
#include "unicode/ures.h"
#include "unicode/uset.h"
#include "unicode/ustring.h"
#include "unicode/utypes.h"
#include "unicode/ulocdata.h"
#include "unicode/uldnames.h"
#include "unicode/parseerr.h" /* may not be included with some uconfig switches */
#include "udbgutil.h"
#if APPLE_ICU_CHANGES
// rdar://
#if !U_PLATFORM_HAS_WIN32_API
#include "unicode/ualoc.h" /* Apple-specific */
#endif
#endif  // APPLE_ICU_CHANGES

static void TestNullDefault(void);
static void TestNonexistentLanguageExemplars(void);
static void TestLocDataErrorCodeChaining(void);
static void TestLocDataWithRgTag(void);
static void TestLanguageExemplarsFallbacks(void);
static void TestDisplayNameBrackets(void);
static void TestIllegalArgumentWhenNoDataWithNoSubstitute(void);
static void Test21157CorrectTerminating(void);

static void TestUnicodeDefines(void);

static void TestIsRightToLeft(void);
static void TestBadLocaleIDs(void);
static void TestBug20370(void);
static void TestBug20321UnicodeLocaleKey(void);

static void TestUsingDefaultWarning(void);
static void TestExcessivelyLongIDs(void);
#if !UCONFIG_NO_FORMATTING
static void TestUldnNameVariants(void);
#endif

#if APPLE_ICU_CHANGES
// rdar://
static void TestCanonicalForm(void);
static void TestRootUndEmpty(void);
#if !U_PLATFORM_HAS_WIN32_API
// These test functionality that does not exist in the AAS/Windows version of Apple ICU
static void TestGetLanguagesForRegion(void);
static void TestGetRegionsForLanguage(void);
static void TestGetAppleParent(void);
static void TestAppleLocalizationsToUsePerf(void);
static void TestAppleLocalizationsToUse(void);
#endif
static void TestNorwegianDisplayNames(void);
static void TestSpecificDisplayNames(void);
static void TestChinaNamesNotResolving(void);
static void TestMvskokeAndLushootseedDisplayNames(void); // rdar://123393073
#endif  // APPLE_ICU_CHANGES

void PrintDataTable();

/*---------------------------------------------------
  table of valid data
 --------------------------------------------------- */
#define LOCALE_SIZE 9
#define LOCALE_INFO_SIZE 28

static const char* const rawData2[LOCALE_INFO_SIZE][LOCALE_SIZE] = {
    /* language code */
    {   "en",   "fr",   "ca",   "el",   "no",   "zh",   "de",   "es",  "ja"    },
    /* script code */
    {   "",     "",     "",     "",     "",     "", "", "", ""  },
    /* country code */
    {   "US",   "FR",   "ES",   "GR",   "NO",   "CN", "DE", "", "JP"    },
    /* variant code */
    {   "",     "",     "",     "",     "NY",   "", "", "", ""      },
    /* full name */
    {   "en_US",    "fr_FR",    "ca_ES",    
        "el_GR",    "no_NO_NY", "zh_Hans_CN", 
        "de_DE@collation=phonebook", "es@collation=traditional",  "ja_JP@calendar=japanese" },
    /* ISO-3 language */
    {   "eng",  "fra",  "cat",  "ell",  "nor",  "zho", "deu", "spa", "jpn"   },
    /* ISO-3 country */
    {   "USA",  "FRA",  "ESP",  "GRC",  "NOR",  "CHN", "DEU", "", "JPN"   },
    /* LCID */
    {   "409", "40c", "403", "408", "814",  "804", "10407", "40a", "411"     },

    /* display language (English) */
    {   "English",  "French",   "Catalan", "Greek",    "Norwegian", "Chinese", "German", "Spanish", "Japanese"    },
    /* display script code (English) */
    {   "",     "",     "",     "",     "",     "Simplified Han", "", "", ""       },
    /* display country (English) */
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
    {   "United States",    "France",   "Spain",  "Greece",   "Norway", "China mainland", "Germany", "", "Japan"       },
#else
    {   "United States",    "France",   "Spain",  "Greece",   "Norway", "China", "Germany", "", "Japan"       },
#endif  // APPLE_ICU_CHANGES
    /* display variant (English) */
    {   "",     "",     "",     "",     "NY",  "", "", "", ""       },
    /* display name (English) */
    {   "English (United States)", "French (France)", "Catalan (Spain)", 
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
        "Greek (Greece)", "Norwegian (Norway, NY)", "Chinese, Simplified (China mainland)", 
#else
        "Greek (Greece)", "Norwegian (Norway, NY)", "Chinese (Simplified, China)", 
#endif  // APPLE_ICU_CHANGES
        "German (Germany, Sort Order=Phonebook Sort Order)", "Spanish (Sort Order=Traditional Sort Order)", "Japanese (Japan, Calendar=Japanese Calendar)" },

    /* display language (French) */
    {   "anglais",  "fran\\u00E7ais",   "catalan", "grec",    "norv\\u00E9gien",    "chinois", "allemand", "espagnol", "japonais"     },
    /* display script code (French) */
    {   "",     "",     "",     "",     "",     "sinogrammes simplifi\\u00e9s", "", "", ""         },
    /* display country (French) */
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
    {   "\\u00C9tats-Unis",    "France",   "Espagne",  "Gr\\u00E8ce",   "Norv\\u00E8ge",    "Chine continentale", "Allemagne", "", "Japon"       },
#else
    {   "\\u00C9tats-Unis",    "France",   "Espagne",  "Gr\\u00E8ce",   "Norv\\u00E8ge",    "Chine", "Allemagne", "", "Japon"       },
#endif  // APPLE_ICU_CHANGES
    /* display variant (French) */
    {   "",     "",     "",     "",     "NY",   "", "", "", ""       },
    /* display name (French) */
    {   "anglais (\\u00C9tats-Unis)", "fran\\u00E7ais (France)", "catalan (Espagne)", 
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
        "grec (Gr\\u00E8ce)", "norv\\u00E9gien (Norv\\u00E8ge, NY)",  "chinois simplifi\\u00e9 (Chine continentale)", 
#else
        "grec (Gr\\u00E8ce)", "norv\\u00E9gien (Norv\\u00E8ge, NY)",  "chinois (simplifi\\u00e9, Chine)", 
#endif  // APPLE_ICU_CHANGES
        "allemand (Allemagne, ordre de tri=ordre de l\\u2019annuaire)", "espagnol (ordre de tri=ordre traditionnel)", "japonais (Japon, calendrier=calendrier japonais)" },

    /* display language (Catalan) */
    {   "angl\\u00E8s", "franc\\u00E8s", "catal\\u00E0", "grec",  "noruec", "xin\\u00E8s", "alemany", "espanyol", "japon\\u00E8s"    },
    /* display script code (Catalan) */
    {   "",     "",     "",     "",     "",     "han simplificat", "", "", ""         },
    /* display country (Catalan) */
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
    {   "Estats Units", "Fran\\u00E7a", "Espanya",  "Gr\\u00E8cia", "Noruega",  "Xina continental", "Alemanya", "", "Jap\\u00F3"    },
#else
    {   "Estats Units", "Fran\\u00E7a", "Espanya",  "Gr\\u00E8cia", "Noruega",  "Xina", "Alemanya", "", "Jap\\u00F3"    },
#endif  // APPLE_ICU_CHANGES
    /* display variant (Catalan) */
    {   "", "", "",                    "", "NY",    "", "", "", ""    },
    /* display name (Catalan) */
    {   "angl\\u00E8s (Estats Units)", "franc\\u00E8s (Fran\\u00E7a)", "catal\\u00E0 (Espanya)", 
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
    "grec (Gr\\u00E8cia)", "noruec (Noruega, NY)", "xin\\u00E8s simplificat (Xina continental)", 
#else
    "grec (Gr\\u00E8cia)", "noruec (Noruega, NY)", "xin\\u00E8s (simplificat, Xina)", 
#endif  // APPLE_ICU_CHANGES
    "alemany (Alemanya, ordre=ordre de la guia telef\\u00F2nica)", "espanyol (ordre=ordre tradicional)", "japon\\u00E8s (Jap\\u00F3, calendari=calendari japon\\u00e8s)" },

    /* display language (Greek) */
    {
        "\\u0391\\u03b3\\u03b3\\u03bb\\u03b9\\u03ba\\u03ac",
        "\\u0393\\u03b1\\u03bb\\u03bb\\u03b9\\u03ba\\u03ac",
        "\\u039a\\u03b1\\u03c4\\u03b1\\u03bb\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac",
        "\\u0395\\u03bb\\u03bb\\u03b7\\u03bd\\u03b9\\u03ba\\u03ac",
        "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03b9\\u03ba\\u03ac",
        "\\u039A\\u03B9\\u03BD\\u03B5\\u03B6\\u03B9\\u03BA\\u03AC", 
        "\\u0393\\u03B5\\u03C1\\u03BC\\u03B1\\u03BD\\u03B9\\u03BA\\u03AC", 
        "\\u0399\\u03C3\\u03C0\\u03B1\\u03BD\\u03B9\\u03BA\\u03AC", 
        "\\u0399\\u03B1\\u03C0\\u03C9\\u03BD\\u03B9\\u03BA\\u03AC"   
    },
    /* display script code (Greek) */

    {   "",     "",     "",     "",     "", "\\u0391\\u03c0\\u03bb\\u03bf\\u03c0\\u03bf\\u03b9\\u03b7\\u03bc\\u03ad\\u03bd\\u03bf \\u03a7\\u03b1\\u03bd", "", "", "" },
    /* display country (Greek) */
    {
        "\\u0397\\u03BD\\u03C9\\u03BC\\u03AD\\u03BD\\u03B5\\u03C2 \\u03A0\\u03BF\\u03BB\\u03B9\\u03C4\\u03B5\\u03AF\\u03B5\\u03C2",
        "\\u0393\\u03b1\\u03bb\\u03bb\\u03af\\u03b1",
        "\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03af\\u03b1",
        "\\u0395\\u03bb\\u03bb\\u03ac\\u03b4\\u03b1",
        "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03af\\u03b1",
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
        "\\u039A\\u03AF\\u03BD\\u03B1 \\u03B7\\u03C0\\u03B5\\u03B9\\u03C1\\u03C9\\u03C4\\u03B9\\u03BA\\u03AE", 
#else
        "\\u039A\\u03AF\\u03BD\\u03B1", 
#endif  // APPLE_ICU_CHANGES
        "\\u0393\\u03B5\\u03C1\\u03BC\\u03B1\\u03BD\\u03AF\\u03B1", 
        "", 
        "\\u0399\\u03B1\\u03C0\\u03C9\\u03BD\\u03AF\\u03B1"   
    },
    /* display variant (Greek) */
    {   "", "", "", "", "NY", "", "", "", ""    }, /* TODO: currently there is no translation for NY in Greek fix this test when we have it */
    /* display name (Greek) */
    {
        "\\u0391\\u03b3\\u03b3\\u03bb\\u03b9\\u03ba\\u03ac (\\u0397\\u03BD\\u03C9\\u03BC\\u03AD\\u03BD\\u03B5\\u03C2 \\u03A0\\u03BF\\u03BB\\u03B9\\u03C4\\u03B5\\u03AF\\u03B5\\u03C2)",
        "\\u0393\\u03b1\\u03bb\\u03bb\\u03b9\\u03ba\\u03ac (\\u0393\\u03b1\\u03bb\\u03bb\\u03af\\u03b1)",
        "\\u039a\\u03b1\\u03c4\\u03b1\\u03bb\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac (\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03af\\u03b1)",
        "\\u0395\\u03bb\\u03bb\\u03b7\\u03bd\\u03b9\\u03ba\\u03ac (\\u0395\\u03bb\\u03bb\\u03ac\\u03b4\\u03b1)",
        "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03b9\\u03ba\\u03ac (\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03af\\u03b1, NY)",
#if APPLE_ICU_CHANGES
//rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
        "\\u0391\\u03c0\\u03bb\\u03bf\\u03c0\\u03bf\\u03b9\\u03b7\\u03bc\\u03ad\\u03bd\\u03b1 \\u039A\\u03B9\\u03BD\\u03B5\\u03B6\\u03B9\\u03BA\\u03AC (\\u039A\\u03AF\\u03BD\\u03B1 \\u03B7\\u03C0\\u03B5\\u03B9\\u03C1\\u03C9\\u03C4\\u03B9\\u03BA\\u03AE)",
#else
        "\\u039A\\u03B9\\u03BD\\u03B5\\u03B6\\u03B9\\u03BA\\u03AC (\\u0391\\u03c0\\u03bb\\u03bf\\u03c0\\u03bf\\u03b9\\u03b7\\u03bc\\u03ad\\u03bd\\u03bf, \\u039A\\u03AF\\u03BD\\u03B1)",
#endif  // APPLE_ICU_CHANGES
        "\\u0393\\u03b5\\u03c1\\u03bc\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac (\\u0393\\u03b5\\u03c1\\u03bc\\u03b1\\u03bd\\u03af\\u03b1, \\u03a3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2=\\u03a3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2 \\u03c4\\u03b7\\u03bb\\u03b5\\u03c6\\u03c9\\u03bd\\u03b9\\u03ba\\u03bf\\u03cd \\u03ba\\u03b1\\u03c4\\u03b1\\u03bb\\u03cc\\u03b3\\u03bf\\u03c5)",
        "\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac (\\u03a3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2=\\u03a0\\u03b1\\u03c1\\u03b1\\u03b4\\u03bf\\u03c3\\u03b9\\u03b1\\u03ba\\u03ae \\u03c3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2)",
        "\\u0399\\u03b1\\u03c0\\u03c9\\u03bd\\u03b9\\u03ba\\u03ac (\\u0399\\u03b1\\u03c0\\u03c9\\u03bd\\u03af\\u03b1, \\u0397\\u03bc\\u03b5\\u03c1\\u03bf\\u03bb\\u03cc\\u03b3\\u03b9\\u03bf=\\u0399\\u03b1\\u03c0\\u03c9\\u03bd\\u03b9\\u03ba\\u03cc \\u03b7\\u03bc\\u03b5\\u03c1\\u03bf\\u03bb\\u03cc\\u03b3\\u03b9\\u03bf)"
    }
};

static UChar*** dataTable=0;
enum {
    ENGLISH = 0,
    FRENCH = 1,
    CATALAN = 2,
    GREEK = 3,
    NORWEGIAN = 4
};

enum {
    LANG = 0,
    SCRIPT = 1,
    CTRY = 2,
    VAR = 3,
    NAME = 4,
    LANG3 = 5,
    CTRY3 = 6,
    LCID = 7,
    DLANG_EN = 8,
    DSCRIPT_EN = 9,
    DCTRY_EN = 10,
    DVAR_EN = 11,
    DNAME_EN = 12,
    DLANG_FR = 13,
    DSCRIPT_FR = 14,
    DCTRY_FR = 15,
    DVAR_FR = 16,
    DNAME_FR = 17,
    DLANG_CA = 18,
    DSCRIPT_CA = 19,
    DCTRY_CA = 20,
    DVAR_CA = 21,
    DNAME_CA = 22,
    DLANG_EL = 23,
    DSCRIPT_EL = 24,
    DCTRY_EL = 25,
    DVAR_EL = 26,
    DNAME_EL = 27
};

#define TESTCASE(name) addTest(root, &name, "tsutil/cloctst/" #name)

void addLocaleTest(TestNode** root);

void addLocaleTest(TestNode** root)
{
    TESTCASE(TestObsoleteNames); /* srl- move */
    TESTCASE(TestBasicGetters);
    TESTCASE(TestNullDefault);
    TESTCASE(TestPrefixes);
    TESTCASE(TestSimpleResourceInfo);
    TESTCASE(TestDisplayNames);
    TESTCASE(TestGetDisplayScriptPreFlighting21160);
    TESTCASE(TestGetAvailableLocales);
    TESTCASE(TestGetAvailableLocalesByType);
    TESTCASE(TestDataDirectory);
#if !UCONFIG_NO_FILE_IO && !UCONFIG_NO_LEGACY_CONVERSION
    TESTCASE(TestISOFunctions);
#endif
    TESTCASE(TestISO3Fallback);
    TESTCASE(TestUninstalledISO3Names);
    TESTCASE(TestSimpleDisplayNames);
    TESTCASE(TestVariantParsing);
    TESTCASE(TestKeywordVariants);
    TESTCASE(TestKeywordVariantParsing);
    TESTCASE(TestCanonicalization);
#if APPLE_ICU_CHANGES
// rdar://
    TESTCASE(TestCanonicalForm);
#endif  // APPLE_ICU_CHANGES
    TESTCASE(TestCanonicalizationBuffer);
    TESTCASE(TestKeywordSet);
    TESTCASE(TestKeywordSetError);
    TESTCASE(TestDisplayKeywords);
    TESTCASE(TestCanonicalization21749StackUseAfterScope);
    TESTCASE(TestDisplayKeywordValues);
    TESTCASE(TestGetBaseName);
#if !UCONFIG_NO_FILE_IO
    TESTCASE(TestGetLocale);
#endif
    TESTCASE(TestDisplayNameWarning);
    TESTCASE(Test21157CorrectTerminating);
    TESTCASE(TestNonexistentLanguageExemplars);
    TESTCASE(TestLocDataErrorCodeChaining);
    TESTCASE(TestLocDataWithRgTag);
    TESTCASE(TestLanguageExemplarsFallbacks);
    TESTCASE(TestCalendar);
    TESTCASE(TestDateFormat);
    TESTCASE(TestCollation);
    TESTCASE(TestULocale);
    TESTCASE(TestUResourceBundle);
    TESTCASE(TestDisplayName); 
    TESTCASE(TestAcceptLanguage); 
    TESTCASE(TestGetLocaleForLCID);
    TESTCASE(TestOrientation);
    TESTCASE(TestLikelySubtags);
    TESTCASE(TestToLanguageTag);
    TESTCASE(TestBug20132);
    TESTCASE(TestBug20149);
    TESTCASE(TestCDefaultLocale);
    TESTCASE(TestForLanguageTag);
    TESTCASE(TestLangAndRegionCanonicalize);
    TESTCASE(TestTrailingNull);
    TESTCASE(TestUnicodeDefines);
    TESTCASE(TestEnglishExemplarCharacters);
    TESTCASE(TestDisplayNameBrackets);
    TESTCASE(TestIllegalArgumentWhenNoDataWithNoSubstitute);
    TESTCASE(TestIsRightToLeft);
    TESTCASE(TestToUnicodeLocaleKey);
    TESTCASE(TestToLegacyKey);
    TESTCASE(TestToUnicodeLocaleType);
    TESTCASE(TestToLegacyType);
    TESTCASE(TestBadLocaleIDs);
    TESTCASE(TestBug20370);
    TESTCASE(TestBug20321UnicodeLocaleKey);
    TESTCASE(TestUsingDefaultWarning);
    TESTCASE(TestBug21449InfiniteLoop);
    TESTCASE(TestExcessivelyLongIDs);
#if !UCONFIG_NO_FORMATTING
    TESTCASE(TestUldnNameVariants);
#endif
#if APPLE_ICU_CHANGES
// rdar://
    TESTCASE(TestRootUndEmpty);
#if !U_PLATFORM_HAS_WIN32_API
// These test functionality that does not exist in the AAS/Windows version of Apple ICU
    TESTCASE(TestGetLanguagesForRegion);
    TESTCASE(TestGetRegionsForLanguage);
    TESTCASE(TestGetAppleParent);
    TESTCASE(TestAppleLocalizationsToUsePerf); // must be the first test to call ualoc_localizationsToUse
    TESTCASE(TestAppleLocalizationsToUse);
#endif
    TESTCASE(TestNorwegianDisplayNames);
    TESTCASE(TestSpecificDisplayNames);
    TESTCASE(TestChinaNamesNotResolving);
    TESTCASE(TestMvskokeAndLushootseedDisplayNames); // rdar://123393073
#endif  // APPLE_ICU_CHANGES
}


/* testing uloc(), uloc_getName(), uloc_getLanguage(), uloc_getVariant(), uloc_getCountry() */
static void TestBasicGetters() {
    int32_t i;
    int32_t cap;
    UErrorCode status = U_ZERO_ERROR;
    char *testLocale = 0;
    char *temp = 0, *name = 0;
    log_verbose("Testing Basic Getters\n");
    for (i = 0; i < LOCALE_SIZE; i++) {
        testLocale=(char*)malloc(sizeof(char) * (strlen(rawData2[NAME][i])+1));
        strcpy(testLocale,rawData2[NAME][i]);

        log_verbose("Testing   %s  .....\n", testLocale);
        cap=uloc_getLanguage(testLocale, NULL, 0, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR){
            status=U_ZERO_ERROR;
            temp=(char*)malloc(sizeof(char) * (cap+1));
            uloc_getLanguage(testLocale, temp, cap+1, &status);
        }
        if(U_FAILURE(status)){
            log_err("ERROR: in uloc_getLanguage  %s\n", myErrorName(status));
        }
        if (0 !=strcmp(temp,rawData2[LANG][i]))    {
            log_err("  Language code mismatch: %s versus  %s\n", temp, rawData2[LANG][i]);
        }


        cap=uloc_getCountry(testLocale, temp, cap, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR){
            status=U_ZERO_ERROR;
            temp=(char*)realloc(temp, sizeof(char) * (cap+1));
            uloc_getCountry(testLocale, temp, cap+1, &status);
        }
        if(U_FAILURE(status)){
            log_err("ERROR: in uloc_getCountry  %s\n", myErrorName(status));
        }
        if (0 != strcmp(temp, rawData2[CTRY][i])) {
            log_err(" Country code mismatch:  %s  versus   %s\n", temp, rawData2[CTRY][i]);

          }

        cap=uloc_getVariant(testLocale, temp, cap, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR){
            status=U_ZERO_ERROR;
            temp=(char*)realloc(temp, sizeof(char) * (cap+1));
            uloc_getVariant(testLocale, temp, cap+1, &status);
        }
        if(U_FAILURE(status)){
            log_err("ERROR: in uloc_getVariant  %s\n", myErrorName(status));
        }
        if (0 != strcmp(temp, rawData2[VAR][i])) {
            log_err("Variant code mismatch:  %s  versus   %s\n", temp, rawData2[VAR][i]);
        }

        cap=uloc_getName(testLocale, NULL, 0, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR){
            status=U_ZERO_ERROR;
            name=(char*)malloc(sizeof(char) * (cap+1));
            uloc_getName(testLocale, name, cap+1, &status);
        } else if(status==U_ZERO_ERROR) {
          log_err("ERROR: in uloc_getName(%s,NULL,0,..), expected U_BUFFER_OVERFLOW_ERROR!\n", testLocale);
        }
        if(U_FAILURE(status)){
            log_err("ERROR: in uloc_getName   %s\n", myErrorName(status));
        }
        if (0 != strcmp(name, rawData2[NAME][i])){
            log_err(" Mismatch in getName:  %s  versus   %s\n", name, rawData2[NAME][i]);
        }

        free(temp);
        free(name);

        free(testLocale);
    }
}

static void TestNullDefault() {
    UErrorCode status = U_ZERO_ERROR;
    char original[ULOC_FULLNAME_CAPACITY];

    uprv_strcpy(original, uloc_getDefault());
    uloc_setDefault("qq_BLA", &status);
    if (uprv_strcmp(uloc_getDefault(), "qq_BLA") != 0) {
        log_err(" Mismatch in uloc_setDefault:  qq_BLA  versus   %s\n", uloc_getDefault());
    }
    uloc_setDefault(NULL, &status);
    if (uprv_strcmp(uloc_getDefault(), original) != 0) {
        log_err(" uloc_setDefault(NULL, &status) didn't get the default locale back!\n");
    }

    {
    /* Test that set & get of default locale work, and that
     * default locales are cached and reused, and not overwritten.
     */
        const char *n_en_US;
        const char *n_fr_FR;
        const char *n2_en_US;
        
        status = U_ZERO_ERROR;
        uloc_setDefault("en_US", &status);
        n_en_US = uloc_getDefault();
        if (strcmp(n_en_US, "en_US") != 0) {
            log_err("Wrong result from uloc_getDefault().  Expected \"en_US\", got \"%s\"\n", n_en_US);
        }
        
        uloc_setDefault("fr_FR", &status);
        n_fr_FR = uloc_getDefault();
        if (strcmp(n_en_US, "en_US") != 0) {
            log_err("uloc_setDefault altered previously default string."
                "Expected \"en_US\", got \"%s\"\n",  n_en_US);
        }
        if (strcmp(n_fr_FR, "fr_FR") != 0) {
            log_err("Wrong result from uloc_getDefault().  Expected \"fr_FR\", got %s\n",  n_fr_FR);
        }
        
        uloc_setDefault("en_US", &status);
        n2_en_US = uloc_getDefault();
        if (strcmp(n2_en_US, "en_US") != 0) {
            log_err("Wrong result from uloc_getDefault().  Expected \"en_US\", got \"%s\"\n", n_en_US);
        }
        if (n2_en_US != n_en_US) {
            log_err("Default locale cache failed to reuse en_US locale.\n");
        }
        
        if (U_FAILURE(status)) {
            log_err("Failure returned from uloc_setDefault - \"%s\"\n", u_errorName(status));
        }
        
    }
    uloc_setDefault(original, &status);
    if (U_FAILURE(status)) {
        log_err("Failed to change the default locale back to %s\n", original);
    }
    
}
/* Test the i- and x- and @ and . functionality 
*/

#define PREFIXBUFSIZ 128

static void TestPrefixes() {
    int row = 0;
    int n;
    const char *loc, *expected;
    
    static const char * const testData[][7] =
    {
        /* NULL canonicalize() column means "expect same as getName()" */
        {"sv", "", "FI", "AL", "sv-fi-al", "sv_FI_AL", NULL},
        {"en", "", "GB", "", "en-gb", "en_GB", NULL},
        {"i-hakka", "", "MT", "XEMXIJA", "i-hakka_MT_XEMXIJA", "i-hakka_MT_XEMXIJA", NULL},
        {"i-hakka", "", "CN", "", "i-hakka_CN", "i-hakka_CN", NULL},
        {"i-hakka", "", "MX", "", "I-hakka_MX", "i-hakka_MX", NULL},
        {"x-klingon", "", "US", "SANJOSE", "X-KLINGON_us_SANJOSE", "x-klingon_US_SANJOSE", NULL},
        {"hy", "", "", "AREVMDA", "hy_AREVMDA", "hy__AREVMDA", "hyw"},
        {"de", "", "", "1901", "de-1901", "de__1901", NULL},
        {"mr", "", "", "", "mr.utf8", "mr.utf8", "mr"},
        {"de", "", "TV", "", "de-tv.koi8r", "de_TV.koi8r", "de_TV"},
        {"x-piglatin", "", "ML", "", "x-piglatin_ML.MBE", "x-piglatin_ML.MBE", "x-piglatin_ML"},  /* Multibyte English */
        {"i-cherokee", "","US", "", "i-Cherokee_US.utf7", "i-cherokee_US.utf7", "i-cherokee_US"},
        {"x-filfli", "", "MT", "FILFLA", "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA"},
        {"no", "", "NO", "NY", "no-no-ny.utf32@B", "no_NO_NY.utf32@B", "no_NO_NY_B"},
        {"no", "", "NO", "",  "no-no.utf32@B", "no_NO.utf32@B", "no_NO_B"},
        {"no", "", "",   "NY", "no__ny", "no__NY", NULL},
        {"no", "", "",   "", "no@ny", "no@ny", "no__NY"},
        {"el", "Latn", "", "", "el-latn", "el_Latn", NULL},
        {"en", "Cyrl", "RU", "", "en-cyrl-ru", "en_Cyrl_RU", NULL},
        {"qq", "Qqqq", "QQ", "QQ", "qq_Qqqq_QQ_QQ", "qq_Qqqq_QQ_QQ", NULL},
        {"qq", "Qqqq", "", "QQ", "qq_Qqqq__QQ", "qq_Qqqq__QQ", NULL},
        {"ab", "Cdef", "GH", "IJ", "ab_cdef_gh_ij", "ab_Cdef_GH_IJ", NULL}, /* total garbage */

        // Before ICU 64, ICU locale canonicalization had some additional mappings.
        // They were removed for ICU-20187 "drop support for long-obsolete locale ID variants".
        // The following now use standard canonicalization.
        {"zh", "Hans", "", "PINYIN", "zh-Hans-pinyin", "zh_Hans__PINYIN", "zh_Hans__PINYIN"},
        {"zh", "Hant", "TW", "STROKE", "zh-hant_TW_STROKE", "zh_Hant_TW_STROKE", "zh_Hant_TW_STROKE"},

        {NULL,NULL,NULL,NULL,NULL,NULL,NULL}
    };
    
    static const char * const testTitles[] = {
        "uloc_getLanguage()",
        "uloc_getScript()",
        "uloc_getCountry()",
        "uloc_getVariant()",
        "name",
        "uloc_getName()",
        "uloc_canonicalize()"
    };
    
    char buf[PREFIXBUFSIZ];
    int32_t len;
    UErrorCode err;
    
    
    for(row=0;testData[row][0] != NULL;row++) {
        loc = testData[row][NAME];
        log_verbose("Test #%d: %s\n", row, loc);
        
        err = U_ZERO_ERROR;
        len=0;
        buf[0]=0;
        for(n=0;n<=(NAME+2);n++) {
            if(n==NAME) continue;
            
            for(len=0;len<PREFIXBUFSIZ;len++) {
                buf[len] = '%'; /* Set a tripwire.. */
            }
            len = 0;
            
            switch(n) {
            case LANG:
                len = uloc_getLanguage(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            case SCRIPT:
                len = uloc_getScript(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            case CTRY:
                len = uloc_getCountry(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            case VAR:
                len = uloc_getVariant(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            case NAME+1:
                len = uloc_getName(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            case NAME+2:
                len = uloc_canonicalize(loc, buf, PREFIXBUFSIZ, &err);
                break;
                
            default:
                strcpy(buf, "**??");
                len=4;
            }
            
            if(U_FAILURE(err)) {
                log_err("#%d: %s on %s: err %s\n",
                    row, testTitles[n], loc, u_errorName(err));
            } else {
                log_verbose("#%d: %s on %s: -> [%s] (length %d)\n",
                    row, testTitles[n], loc, buf, len);
                
                if(len != (int32_t)strlen(buf)) {
                    log_err("#%d: %s on %s: -> [%s] (length returned %d, actual %d!)\n",
                        row, testTitles[n], loc, buf, len, strlen(buf)+1);
                    
                }
                
                /* see if they smashed something */
                if(buf[len+1] != '%') {
                    log_err("#%d: %s on %s: -> [%s] - wrote [%X] out ofbounds!\n",
                        row, testTitles[n], loc, buf, buf[len+1]);
                }
                
                expected = testData[row][n];
                if (expected == NULL && n == (NAME+2)) {
                    /* NULL expected canonicalize() means "expect same as getName()" */
                    expected = testData[row][NAME+1];
                }
                if(strcmp(buf, expected)) {
                    log_err("#%d: %s on %s: -> [%s] (expected '%s'!)\n",
                        row, testTitles[n], loc, buf, expected);
                    
                }
            }
        }
    }
}


/* testing uloc_getISO3Language(), uloc_getISO3Country(),  */
static void TestSimpleResourceInfo() {
    int32_t i;
    char* testLocale = 0;
    UChar* expected = 0;
    
    const char* temp;
    char            temp2[20];
    testLocale=(char*)malloc(sizeof(char) * 1);
    expected=(UChar*)malloc(sizeof(UChar) * 1);
    
    setUpDataTable();
    log_verbose("Testing getISO3Language and getISO3Country\n");
    for (i = 0; i < LOCALE_SIZE; i++) {
        
        testLocale=(char*)realloc(testLocale, sizeof(char) * (u_strlen(dataTable[NAME][i])+1));
        u_austrcpy(testLocale, dataTable[NAME][i]);
        
        log_verbose("Testing   %s ......\n", testLocale);
        
        temp=uloc_getISO3Language(testLocale);
        expected=(UChar*)realloc(expected, sizeof(UChar) * (strlen(temp) + 1));
        u_uastrcpy(expected,temp);
        if (0 != u_strcmp(expected, dataTable[LANG3][i])) {
            log_err("  ISO-3 language code mismatch:  %s versus  %s\n",  austrdup(expected),
                austrdup(dataTable[LANG3][i]));
        }
        
        temp=uloc_getISO3Country(testLocale);
        expected=(UChar*)realloc(expected, sizeof(UChar) * (strlen(temp) + 1));
        u_uastrcpy(expected,temp);
        if (0 != u_strcmp(expected, dataTable[CTRY3][i])) {
            log_err("  ISO-3 Country code mismatch:  %s versus  %s\n",  austrdup(expected),
                austrdup(dataTable[CTRY3][i]));
        }
        snprintf(temp2, sizeof(temp2), "%x", (int)uloc_getLCID(testLocale));
        if (strcmp(temp2, rawData2[LCID][i]) != 0) {
            log_err("LCID mismatch: %s versus %s\n", temp2 , rawData2[LCID][i]);
        }
    }
    
    free(expected);
    free(testLocale);
    cleanUpDataTable();
}

/* if len < 0, we convert until we hit UChar 0x0000, which is not output. will add trailing null
 * if there's room but won't be included in result.  result < 0 indicates an error.
 * Returns the number of chars written (not those that would be written if there's enough room.*/
static int32_t UCharsToEscapedAscii(const UChar* utext, int32_t len, char* resultChars, int32_t buflen) {
    static const struct {
        char escapedChar;
        UChar sourceVal;
    } ESCAPE_MAP[] = {
        /*a*/ {'a', 0x07},
        /*b*/ {'b', 0x08},
        /*e*/ {'e', 0x1b},
        /*f*/ {'f', 0x0c},
        /*n*/ {'n', 0x0a},
        /*r*/ {'r', 0x0d},
        /*t*/ {'t', 0x09},
        /*v*/ {'v', 0x0b}
    };
    static const int32_t ESCAPE_MAP_LENGTH = UPRV_LENGTHOF(ESCAPE_MAP);
    static const char HEX_DIGITS[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    int32_t i, j;
    int32_t resultLen = 0;
    const int32_t limit = len<0 ? buflen : len; /* buflen is long enough to hit the buffer limit */
    const int32_t escapeLimit1 = buflen-2;
    const int32_t escapeLimit2 = buflen-6;
    UChar uc;

    if(utext==NULL || resultChars==NULL || buflen<0) {
        return -1;
    }

    for(i=0;i<limit && resultLen<buflen;++i) {
        uc=utext[i];
        if(len<0 && uc==0) {
            break;
        }
        if(uc<0x20) {
            for(j=0;j<ESCAPE_MAP_LENGTH && uc!=ESCAPE_MAP[j].sourceVal;j++) {
            }
            if(j<ESCAPE_MAP_LENGTH) {
                if(resultLen>escapeLimit1) {
                    break;
                }
                resultChars[resultLen++]='\\';
                resultChars[resultLen++]=ESCAPE_MAP[j].escapedChar;
                continue;
            }
        } else if(uc<0x7f) {
            u_austrncpy(resultChars + resultLen, &uc, 1);
            resultLen++;
            continue;
        }

        if(resultLen>escapeLimit2) {
            break;
        }

        /* have to escape the uchar */
        resultChars[resultLen++]='\\';
        resultChars[resultLen++]='u';
        resultChars[resultLen++]=HEX_DIGITS[(uc>>12)&0xff];
        resultChars[resultLen++]=HEX_DIGITS[(uc>>8)&0xff];
        resultChars[resultLen++]=HEX_DIGITS[(uc>>4)&0xff];
        resultChars[resultLen++]=HEX_DIGITS[uc&0xff];
    }

    if(resultLen<buflen) {
        resultChars[resultLen] = 0;
    }

    return resultLen;
}

/*
 * Jitterbug 2439 -- markus 20030425
 *
 * The lookup of display names must not fall back through the default
 * locale because that yields useless results.
 */
static void TestDisplayNames()
{
    UChar buffer[100];
    UErrorCode errorCode=U_ZERO_ERROR;
    int32_t length;
    log_verbose("Testing getDisplayName for different locales\n");

    log_verbose("  In locale = en_US...\n");
    doTestDisplayNames("en_US", DLANG_EN);
    log_verbose("  In locale = fr_FR....\n");
    doTestDisplayNames("fr_FR", DLANG_FR);
    log_verbose("  In locale = ca_ES...\n");
    doTestDisplayNames("ca_ES", DLANG_CA);
    log_verbose("  In locale = gr_EL..\n");
    doTestDisplayNames("el_GR", DLANG_EL);

    /* test that the default locale has a display name for its own language */
    errorCode=U_ZERO_ERROR;
    length=uloc_getDisplayLanguage(NULL, NULL, buffer, UPRV_LENGTHOF(buffer), &errorCode);
    /* check <=3 to reject getting the language code as a display name */
    if(U_FAILURE(errorCode) || (length<=3 && buffer[0]<=0x7f)) {
        const char* defaultLocale = uloc_getDefault();
        for (int32_t i = 0, count = uloc_countAvailable(); i < count; i++) {
            /* Only report error if the default locale is in the available list */
            if (uprv_strcmp(defaultLocale, uloc_getAvailable(i)) == 0) {
                log_data_err(
                    "unable to get a display string for the language of the "
                    "default locale - %s (Are you missing data?)\n",
                    u_errorName(errorCode));
                break;
            }
        }
    }

    /* test that we get the language code itself for an unknown language, and a default warning */
    errorCode=U_ZERO_ERROR;
    length=uloc_getDisplayLanguage("qq", "rr", buffer, UPRV_LENGTHOF(buffer), &errorCode);
    if(errorCode!=U_USING_DEFAULT_WARNING || length!=2 || buffer[0]!=0x71 || buffer[1]!=0x71) {
        log_err("error getting the display string for an unknown language - %s\n", u_errorName(errorCode));
    }
    
    /* test that we get a default warning for a display name where one component is unknown (4255) */
    errorCode=U_ZERO_ERROR;
    length=uloc_getDisplayName("qq_US_POSIX", "en_US", buffer, UPRV_LENGTHOF(buffer), &errorCode);
    if(errorCode!=U_USING_DEFAULT_WARNING) {
        log_err("error getting the display name for a locale with an unknown language - %s\n", u_errorName(errorCode));
    }

    {
        int32_t i;
        static const char *aLocale = "es@collation=traditional;calendar=japanese";
        static const char *testL[] = { "en_US", 
            "fr_FR", 
            "ca_ES",
            "el_GR" };
        static const char *expect[] = { "Spanish (Calendar=Japanese Calendar, Sort Order=Traditional Sort Order)", /* note sorted order of keywords */
            "espagnol (calendrier=calendrier japonais, ordre de tri=ordre traditionnel)",
            "espanyol (calendari=calendari japon\\u00e8s, ordre=ordre tradicional)",
            "\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac (\\u0397\\u03bc\\u03b5\\u03c1\\u03bf\\u03bb\\u03cc\\u03b3\\u03b9\\u03bf=\\u0399\\u03b1\\u03c0\\u03c9\\u03bd\\u03b9\\u03ba\\u03cc \\u03b7\\u03bc\\u03b5\\u03c1\\u03bf\\u03bb\\u03cc\\u03b3\\u03b9\\u03bf, \\u03a3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2=\\u03a0\\u03b1\\u03c1\\u03b1\\u03b4\\u03bf\\u03c3\\u03b9\\u03b1\\u03ba\\u03ae \\u03c3\\u03b5\\u03b9\\u03c1\\u03ac \\u03c4\\u03b1\\u03be\\u03b9\\u03bd\\u03cc\\u03bc\\u03b7\\u03c3\\u03b7\\u03c2)" };
        UChar *expectBuffer;

        for(i=0;i<UPRV_LENGTHOF(testL);i++) {
            errorCode = U_ZERO_ERROR;
            uloc_getDisplayName(aLocale, testL[i], buffer, UPRV_LENGTHOF(buffer), &errorCode);
            if(U_FAILURE(errorCode)) {
                log_err("FAIL in uloc_getDisplayName(%s,%s,..) -> %s\n", aLocale, testL[i], u_errorName(errorCode));
            } else {
                expectBuffer = CharsToUChars(expect[i]);
                if(u_strcmp(buffer,expectBuffer)) {
                    log_data_err("FAIL in uloc_getDisplayName(%s,%s,..) expected '%s' got '%s' (Are you missing data?)\n", aLocale, testL[i], expect[i], austrdup(buffer));
                } else {
                    log_verbose("pass in uloc_getDisplayName(%s,%s,..) got '%s'\n", aLocale, testL[i], expect[i]);
                }
                free(expectBuffer);
            }
        }
    }

    /* test that we properly preflight and return data when there's a non-default pattern,
       see ticket #8262. */
    {
        int32_t i;
        static const char *locale="az_Cyrl";
        static const char *displayLocale="ja";
        static const char *expectedChars =
#if APPLE_ICU_CHANGES
// rdar://
                "\\u30a2\\u30bc\\u30eb\\u30d0\\u30a4\\u30b8\\u30e3\\u30f3\\u8a9e"
                "\\uff08\\u30ad\\u30ea\\u30eb\\u6587\\u5b57\\uff09";
#else
                "\\u30a2\\u30bc\\u30eb\\u30d0\\u30a4\\u30b8\\u30e3\\u30f3\\u8a9e "
                "(\\u30ad\\u30ea\\u30eb\\u6587\\u5b57)";
#endif  // APPLE_ICU_CHANGES
        UErrorCode ec=U_ZERO_ERROR;
        UChar result[256];
        int32_t len;
        int32_t preflightLen=uloc_getDisplayName(locale, displayLocale, NULL, 0, &ec);
        /* inconvenient semantics when preflighting, this condition is expected... */
        if(ec==U_BUFFER_OVERFLOW_ERROR) {
            ec=U_ZERO_ERROR;
        }
        len=uloc_getDisplayName(locale, displayLocale, result, UPRV_LENGTHOF(result), &ec);
        if(U_FAILURE(ec)) {
            log_err("uloc_getDisplayName(%s, %s...) returned error: %s",
                    locale, displayLocale, u_errorName(ec));
        } else {
            UChar *expected=CharsToUChars(expectedChars);
            int32_t expectedLen=u_strlen(expected);

            if(len!=expectedLen) {
                log_data_err("uloc_getDisplayName(%s, %s...) returned string of length %d, expected length %d",
                        locale, displayLocale, len, expectedLen);
            } else if(preflightLen!=expectedLen) {
                log_err("uloc_getDisplayName(%s, %s...) returned preflight length %d, expected length %d",
                        locale, displayLocale, preflightLen, expectedLen);
            } else if(u_strncmp(result, expected, len)) {
                int32_t cap=len*6+1;  /* worst case + space for trailing null */
                char* resultChars=(char*)malloc(cap);
                int32_t resultCharsLen=UCharsToEscapedAscii(result, len, resultChars, cap);
                if(resultCharsLen<0 || resultCharsLen<cap-1) {
                    log_err("uloc_getDisplayName(%s, %s...) mismatch", locale, displayLocale);
                } else {
                    log_err("uloc_getDisplayName(%s, %s...) returned '%s' but expected '%s'",
                            locale, displayLocale, resultChars, expectedChars);
                }
                free(resultChars);
                resultChars=NULL;
            } else {
                /* test all buffer sizes */
                for(i=len+1;i>=0;--i) {
                    len=uloc_getDisplayName(locale, displayLocale, result, i, &ec);
                    if(ec==U_BUFFER_OVERFLOW_ERROR) {
                        ec=U_ZERO_ERROR;
                    }
                    if(U_FAILURE(ec)) {
                        log_err("using buffer of length %d returned error %s", i, u_errorName(ec));
                        break;
                    }
                    if(len!=expectedLen) {
                        log_err("with buffer of length %d, expected length %d but got %d", i, expectedLen, len);
                        break;
                    }
                    /* There's no guarantee about what's in the buffer if we've overflowed, in particular,
                     * we don't know that it's been filled, so no point in checking. */
                }
            }

            free(expected);
        }
    }
}

/**
 * ICU-21160 test the pre-flighting call to uloc_getDisplayScript returns the actual length needed
 * for the result buffer.
 */
static void TestGetDisplayScriptPreFlighting21160()
{
    const char* locale = "und-Latn";
    const char* inlocale = "de";

    UErrorCode ec = U_ZERO_ERROR;
    UChar* result = NULL;
    int32_t length = uloc_getDisplayScript(locale, inlocale, NULL, 0, &ec) + 1;
    ec = U_ZERO_ERROR;
    result=(UChar*)malloc(sizeof(UChar) * length);
    length = uloc_getDisplayScript(locale, inlocale, result, length, &ec);
    if (U_FAILURE(ec)) {
        log_err("uloc_getDisplayScript length %d returned error %s", length, u_errorName(ec));
    }
    free(result);
}

/* test for uloc_getAvailable()  and uloc_countAvailable()*/
static void TestGetAvailableLocales()
{

    const char *locList;
    int32_t locCount,i;

    log_verbose("Testing the no of available locales\n");
    locCount=uloc_countAvailable();
    if (locCount == 0)
        log_data_err("countAvailable() returned an empty list!\n");

    /* use something sensible w/o hardcoding the count */
    else if(locCount < 0){
        log_data_err("countAvailable() returned a wrong value!= %d\n", locCount);
    }
    else{
        log_info("Number of locales returned = %d\n", locCount);
    }
    for(i=0;i<locCount;i++){
        locList=uloc_getAvailable(i);

        log_verbose(" %s\n", locList);
    }
}

static void TestGetAvailableLocalesByType() {
    UErrorCode status = U_ZERO_ERROR;

    UEnumeration* uenum = uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
    assertSuccess("Constructing the UEnumeration", &status);

    assertIntEquals("countAvailable() should be same in old and new methods",
        uloc_countAvailable(),
        uenum_count(uenum, &status));

    for (int32_t i = 0; i < uloc_countAvailable(); i++) {
        const char* old = uloc_getAvailable(i);
        int32_t len = 0;
        const char* new = uenum_next(uenum, &len, &status);
        assertEquals("Old and new strings should equal", old, new);
        assertIntEquals("String length should be correct", uprv_strlen(old), len);
    }
    assertPtrEquals("Should get nullptr on the last string",
        NULL, uenum_next(uenum, NULL, &status));

    uenum_close(uenum);

    uenum = uloc_openAvailableByType(ULOC_AVAILABLE_ONLY_LEGACY_ALIASES, &status);
    UBool found_he = false;
    UBool found_iw = false;
    const char* loc;
    while ((loc = uenum_next(uenum, NULL, &status))) {
        if (uprv_strcmp("he", loc) == 0) {
            found_he = true;
        }
        if (uprv_strcmp("iw", loc) == 0) {
            found_iw = true;
        }
    }
    assertTrue("Should NOT have found he amongst the legacy/alias locales", !found_he);
    assertTrue("Should have found iw amongst the legacy/alias locales", found_iw);
    uenum_close(uenum);

    uenum = uloc_openAvailableByType(ULOC_AVAILABLE_WITH_LEGACY_ALIASES, &status);
    found_he = false;
    found_iw = false;
    const UChar* uloc; // test the UChar conversion
    int32_t count = 0;
    while ((uloc = uenum_unext(uenum, NULL, &status))) {
        if (u_strcmp(u"iw", uloc) == 0) {
            found_iw = true;
        }
        if (u_strcmp(u"he", uloc) == 0) {
            found_he = true;
        }
        count++;
    }
    assertTrue("Should have found he amongst all locales", found_he);
    assertTrue("Should have found iw amongst all locales", found_iw);
    assertIntEquals("Should return as many strings as claimed",
        count, uenum_count(uenum, &status));

    // Reset the enumeration and it should still work
    uenum_reset(uenum, &status);
    count = 0;
    while ((loc = uenum_next(uenum, NULL, &status))) {
        count++;
    }
    assertIntEquals("After reset, should return as many strings as claimed",
        count, uenum_count(uenum, &status));

    uenum_close(uenum);

    assertSuccess("No errors should have occurred", &status);
}

/* test for u_getDataDirectory, u_setDataDirectory, uloc_getISO3Language */
static void TestDataDirectory()
{

    char            oldDirectory[512];
    const char     *temp,*testValue1,*testValue2,*testValue3;
    const char path[40] ="d:\\icu\\source\\test\\intltest" U_FILE_SEP_STRING; /*give the required path */

    log_verbose("Testing getDataDirectory()\n");
    temp = u_getDataDirectory();
    strcpy(oldDirectory, temp);

    testValue1=uloc_getISO3Language("en_US");
    log_verbose("first fetch of language retrieved  %s\n", testValue1);

    if (0 != strcmp(testValue1,"eng")){
        log_err("Initial check of ISO3 language failed: expected \"eng\", got  %s \n", testValue1);
    }

    /*defining the path for DataDirectory */
    log_verbose("Testing setDataDirectory\n");
    u_setDataDirectory( path );
    if(strcmp(path, u_getDataDirectory())==0)
        log_verbose("setDataDirectory working fine\n");
    else
        log_err("Error in setDataDirectory. Directory not set correctly - came back as [%s], expected [%s]\n", u_getDataDirectory(), path);

    testValue2=uloc_getISO3Language("en_US");
    log_verbose("second fetch of language retrieved  %s \n", testValue2);

    u_setDataDirectory(oldDirectory);
    testValue3=uloc_getISO3Language("en_US");
    log_verbose("third fetch of language retrieved  %s \n", testValue3);

    if (0 != strcmp(testValue3,"eng")) {
       log_err("get/setDataDirectory() failed: expected \"eng\", got \" %s  \" \n", testValue3);
    }
}



/*=========================================================== */

static UChar _NUL=0;

static void doTestDisplayNames(const char* displayLocale, int32_t compareIndex)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t i;
    int32_t maxresultsize;

    const char *testLocale;


    UChar  *testLang  = 0;
    UChar  *testScript  = 0;
    UChar  *testCtry = 0;
    UChar  *testVar = 0;
    UChar  *testName = 0;


    UChar*  expectedLang = 0;
    UChar*  expectedScript = 0;
    UChar*  expectedCtry = 0;
    UChar*  expectedVar = 0;
    UChar*  expectedName = 0;

setUpDataTable();

    for(i=0;i<LOCALE_SIZE; ++i)
    {
        testLocale=rawData2[NAME][i];

        log_verbose("Testing.....  %s\n", testLocale);

        maxresultsize=0;
        maxresultsize=uloc_getDisplayLanguage(testLocale, displayLocale, NULL, maxresultsize, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            testLang=(UChar*)malloc(sizeof(UChar) * (maxresultsize+1));
            uloc_getDisplayLanguage(testLocale, displayLocale, testLang, maxresultsize + 1, &status);
        }
        else
        {
            testLang=&_NUL;
        }
        if(U_FAILURE(status)){
            log_err("Error in getDisplayLanguage()  %s\n", myErrorName(status));
        }

        maxresultsize=0;
        maxresultsize=uloc_getDisplayScript(testLocale, displayLocale, NULL, maxresultsize, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            testScript=(UChar*)malloc(sizeof(UChar) * (maxresultsize+1));
            uloc_getDisplayScript(testLocale, displayLocale, testScript, maxresultsize + 1, &status);
        }
        else
        {
            testScript=&_NUL;
        }
        if(U_FAILURE(status)){
            log_err("Error in getDisplayScript()  %s\n", myErrorName(status));
        }

        maxresultsize=0;
        maxresultsize=uloc_getDisplayCountry(testLocale, displayLocale, NULL, maxresultsize, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            testCtry=(UChar*)malloc(sizeof(UChar) * (maxresultsize+1));
            uloc_getDisplayCountry(testLocale, displayLocale, testCtry, maxresultsize + 1, &status);
        }
        else
        {
            testCtry=&_NUL;
        }
        if(U_FAILURE(status)){
            log_err("Error in getDisplayCountry()  %s\n", myErrorName(status));
        }

        maxresultsize=0;
        maxresultsize=uloc_getDisplayVariant(testLocale, displayLocale, NULL, maxresultsize, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            testVar=(UChar*)malloc(sizeof(UChar) * (maxresultsize+1));
            uloc_getDisplayVariant(testLocale, displayLocale, testVar, maxresultsize + 1, &status);
        }
        else
        {
            testVar=&_NUL;
        }
        if(U_FAILURE(status)){
                log_err("Error in getDisplayVariant()  %s\n", myErrorName(status));
        }

        maxresultsize=0;
        maxresultsize=uloc_getDisplayName(testLocale, displayLocale, NULL, maxresultsize, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR)
        {
            status=U_ZERO_ERROR;
            testName=(UChar*)malloc(sizeof(UChar) * (maxresultsize+1));
            uloc_getDisplayName(testLocale, displayLocale, testName, maxresultsize + 1, &status);
        }
        else
        {
            testName=&_NUL;
        }
        if(U_FAILURE(status)){
            log_err("Error in getDisplayName()  %s\n", myErrorName(status));
        }

        expectedLang=dataTable[compareIndex][i];
        if(u_strlen(expectedLang)== 0)
            expectedLang=dataTable[DLANG_EN][i];

        expectedScript=dataTable[compareIndex + 1][i];
        if(u_strlen(expectedScript)== 0)
            expectedScript=dataTable[DSCRIPT_EN][i];

        expectedCtry=dataTable[compareIndex + 2][i];
        if(u_strlen(expectedCtry)== 0)
            expectedCtry=dataTable[DCTRY_EN][i];

        expectedVar=dataTable[compareIndex + 3][i];
        if(u_strlen(expectedVar)== 0)
            expectedVar=dataTable[DVAR_EN][i];

        expectedName=dataTable[compareIndex + 4][i];
        if(u_strlen(expectedName) == 0)
            expectedName=dataTable[DNAME_EN][i];

        if (0 !=u_strcmp(testLang,expectedLang))  {
            log_data_err(" Display Language mismatch: got %s expected %s displayLocale=%s (Are you missing data?)\n", austrdup(testLang), austrdup(expectedLang), displayLocale);
        }

        if (0 != u_strcmp(testScript,expectedScript))   {
            log_data_err(" Display Script mismatch: got %s expected %s displayLocale=%s (Are you missing data?)\n", austrdup(testScript), austrdup(expectedScript), displayLocale);
        }

        if (0 != u_strcmp(testCtry,expectedCtry))   {
            log_data_err(" Display Country mismatch: got %s expected %s displayLocale=%s (Are you missing data?)\n", austrdup(testCtry), austrdup(expectedCtry), displayLocale);
        }

        if (0 != u_strcmp(testVar,expectedVar))    {
            log_data_err(" Display Variant mismatch: got %s expected %s displayLocale=%s (Are you missing data?)\n", austrdup(testVar), austrdup(expectedVar), displayLocale);
        }

        if(0 != u_strcmp(testName, expectedName))    {
            log_data_err(" Display Name mismatch: got %s expected %s displayLocale=%s (Are you missing data?)\n", austrdup(testName), austrdup(expectedName), displayLocale);
        }

        if(testName!=&_NUL) {
            free(testName);
        }
        if(testLang!=&_NUL) {
            free(testLang);
        }
        if(testScript!=&_NUL) {
            free(testScript);
        }
        if(testCtry!=&_NUL) {
            free(testCtry);
        }
        if(testVar!=&_NUL) {
            free(testVar);
        }
    }
cleanUpDataTable();
}

/*------------------------------
 * TestDisplayNameBrackets
 */

typedef struct {
    const char * displayLocale;
    const char * namedRegion;
    const char * namedLocale;
    const char * regionName;
#if APPLE_ICU_CHANGES
// rdar://
    const char * ulocLocaleName;
    const char * uldnLocaleName;
#else
    const char * localeName;
#endif  // APPLE_ICU_CHANGES
} DisplayNameBracketsItem;

static const DisplayNameBracketsItem displayNameBracketsItems[] = {
#if APPLE_ICU_CHANGES
// rdar://
    { "en", "CC", "en_CC",      "Cocos (Keeling) Islands",  "English (Cocos [Keeling] Islands)",  "English (Cocos [Keeling] Islands)" },
    { "en", "MM", "my_MM",      "Myanmar (Burma)",          "Burmese (Myanmar [Burma])",          "Burmese (Myanmar)"                 },
    { "en", "MM", "my_Mymr_MM", "Myanmar (Burma)",          "Burmese (Myanmar, Myanmar [Burma])", "Burmese (Myanmar, Myanmar)"        },
    { "zh", "CC", "en_CC",      "\\u79D1\\u79D1\\u65AF\\uFF08\\u57FA\\u6797\\uFF09\\u7FA4\\u5C9B",
                                "\\u82F1\\u8BED\\uFF08\\u79D1\\u79D1\\u65AF\\uFF3B\\u57FA\\u6797\\uFF3D\\u7FA4\\u5C9B\\uFF09",
                                "\\u82F1\\u8BED\\uFF08\\u79D1\\u79D1\\u65AF\\uFF3B\\u57FA\\u6797\\uFF3D\\u7FA4\\u5C9B\\uFF09"         },
    { "zh", "CG", "fr_CG",      "\\u521A\\u679C\\uFF08\\u5E03\\uFF09",
                                "\\u6CD5\\u8BED\\uFF08\\u521A\\u679C\\uFF3B\\u5E03\\uFF3D\\uFF09",
                                "\\u6CD5\\u8BED\\uFF08\\u521A\\u679C\\uFF3B\\u5E03\\uFF3D\\uFF09"                                     },
    { NULL, NULL, NULL,         NULL,                       NULL,                                  NULL                               }
#else
    { "en", "CC", "en_CC",      "Cocos (Keeling) Islands",  "English (Cocos [Keeling] Islands)"  },
    { "en", "MM", "my_MM",      "Myanmar (Burma)",          "Burmese (Myanmar [Burma])"          },
    { "en", "MM", "my_Mymr_MM", "Myanmar (Burma)",          "Burmese (Myanmar, Myanmar [Burma])" },
    { "zh", "CC", "en_CC",      "\\u79D1\\u79D1\\u65AF\\uFF08\\u57FA\\u6797\\uFF09\\u7FA4\\u5C9B", "\\u82F1\\u8BED\\uFF08\\u79D1\\u79D1\\u65AF\\uFF3B\\u57FA\\u6797\\uFF3D\\u7FA4\\u5C9B\\uFF09" },
    { "zh", "CG", "fr_CG",      "\\u521A\\u679C\\uFF08\\u5E03\\uFF09",                             "\\u6CD5\\u8BED\\uFF08\\u521A\\u679C\\uFF3B\\u5E03\\uFF3D\\uFF09" },
    { NULL, NULL, NULL,         NULL,                       NULL                                 }
#endif  // APPLE_ICU_CHANGES
};

enum { kDisplayNameBracketsMax = 128 };

static void TestDisplayNameBrackets()
{
    const DisplayNameBracketsItem * itemPtr = displayNameBracketsItems;
    for (; itemPtr->displayLocale != NULL; itemPtr++) {
        ULocaleDisplayNames * uldn;
        UErrorCode status;
        UChar expectRegionName[kDisplayNameBracketsMax];
#if APPLE_ICU_CHANGES
// rdar://
        UChar expectUlocLocaleName[kDisplayNameBracketsMax];
        UChar expectUldnLocaleName[kDisplayNameBracketsMax];
#else
        UChar expectLocaleName[kDisplayNameBracketsMax];
#endif  // APPLE_ICU_CHANGES
        UChar getName[kDisplayNameBracketsMax];
        int32_t ulen;
        
        (void) u_unescape(itemPtr->regionName, expectRegionName, kDisplayNameBracketsMax);
#if APPLE_ICU_CHANGES
// rdar://
        (void) u_unescape(itemPtr->ulocLocaleName, expectUlocLocaleName, kDisplayNameBracketsMax);
        (void) u_unescape(itemPtr->uldnLocaleName, expectUldnLocaleName, kDisplayNameBracketsMax);
#else
        (void) u_unescape(itemPtr->localeName, expectLocaleName, kDisplayNameBracketsMax);
#endif  // APPLE_ICU_CHANGES

        status = U_ZERO_ERROR;
        ulen = uloc_getDisplayCountry(itemPtr->namedLocale, itemPtr->displayLocale, getName, kDisplayNameBracketsMax, &status);
        if ( U_FAILURE(status) || u_strcmp(getName, expectRegionName) != 0 ) {
            log_data_err("uloc_getDisplayCountry for displayLocale %s and namedLocale %s returns unexpected name or status %s\n", itemPtr->displayLocale, itemPtr->namedLocale, myErrorName(status));
        }

        status = U_ZERO_ERROR;
        ulen = uloc_getDisplayName(itemPtr->namedLocale, itemPtr->displayLocale, getName, kDisplayNameBracketsMax, &status);
#if APPLE_ICU_CHANGES
// rdar://
        if ( U_FAILURE(status) || u_strcmp(getName, expectUlocLocaleName) != 0 ) {
#else
        if ( U_FAILURE(status) || u_strcmp(getName, expectLocaleName) != 0 ) {
#endif  // APPLE_ICU_CHANGES
            log_data_err("uloc_getDisplayName for displayLocale %s and namedLocale %s returns unexpected name or status %s\n", itemPtr->displayLocale, itemPtr->namedLocale, myErrorName(status));
        }
#if APPLE_ICU_CHANGES
// rdar://
        if ( U_FAILURE(status) ) {
            log_data_err("uloc_getDisplayName for displayLocale %s and namedLocale %-10s returns unexpected status %s\n", itemPtr->displayLocale, itemPtr->namedLocale, myErrorName(status));
        } else if ( u_strcmp(getName, expectUlocLocaleName) != 0 ) {
            char bbuf[128];
            u_strToUTF8(bbuf, 128, NULL, getName, ulen, &status);
            log_data_err("uloc_getDisplayName for displayLocale %s and namedLocale %-10s returns unexpected name (len %d): \"%s\"\n", itemPtr->displayLocale, itemPtr->namedLocale, ulen, bbuf);
        }
#endif  // APPLE_ICU_CHANGES

#if !UCONFIG_NO_FORMATTING
        status = U_ZERO_ERROR;
        uldn = uldn_open(itemPtr->displayLocale, ULDN_STANDARD_NAMES, &status);
        if (U_SUCCESS(status)) {
            status = U_ZERO_ERROR;
            ulen = uldn_regionDisplayName(uldn, itemPtr->namedRegion, getName, kDisplayNameBracketsMax, &status);
            if ( U_FAILURE(status) || u_strcmp(getName, expectRegionName) != 0 ) {
                log_data_err("uldn_regionDisplayName for displayLocale %s and namedRegion %s returns unexpected name or status %s\n", itemPtr->displayLocale, itemPtr->namedRegion, myErrorName(status));
            }

            status = U_ZERO_ERROR;
            ulen = uldn_localeDisplayName(uldn, itemPtr->namedLocale, getName, kDisplayNameBracketsMax, &status);
#if APPLE_ICU_CHANGES
// rdar://
            if ( U_FAILURE(status) ) {
                log_data_err("uldn_localeDisplayName for displayLocale %s and namedLocale %-10s returns unexpected status %s\n", itemPtr->displayLocale, itemPtr->namedLocale, myErrorName(status));
            } else if ( u_strcmp(getName, expectUldnLocaleName) != 0 ) {
                char bbuf[128];
                u_strToUTF8(bbuf, 128, NULL, getName, ulen, &status);
                log_data_err("uldn_localeDisplayName for displayLocale %s and namedLocale %-10s returns unexpected name (len %d): \"%s\"\n", itemPtr->displayLocale, itemPtr->namedLocale, ulen, bbuf);
            }
#else
            if ( U_FAILURE(status) || u_strcmp(getName, expectLocaleName) != 0 ) {
                log_data_err("uldn_localeDisplayName for displayLocale %s and namedLocale %s returns unexpected name or status %s\n", itemPtr->displayLocale, itemPtr->namedLocale, myErrorName(status));
            }
#endif  // APPLE_ICU_CHANGES

            uldn_close(uldn);
        } else {
            log_data_err("uldn_open fails for displayLocale %s, status=%s\n", itemPtr->displayLocale, u_errorName(status));
        }
#endif
    (void)ulen;   /* Suppress variable not used warning */
    }
}

/*------------------------------
 * TestIllegalArgumentWhenNoDataWithNoSubstitute
 */

static void TestIllegalArgumentWhenNoDataWithNoSubstitute()
{
#if !UCONFIG_NO_FORMATTING
    UErrorCode status = U_ZERO_ERROR;
    UChar getName[kDisplayNameBracketsMax];
    UDisplayContext contexts[] = {
        UDISPCTX_NO_SUBSTITUTE,
    };
    ULocaleDisplayNames* ldn = uldn_openForContext("en", contexts, 1, &status);

    uldn_localeDisplayName(ldn, "efg", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_localeDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_languageDisplayName(ldn, "zz", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_languageDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_scriptDisplayName(ldn, "Aaaa", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_scriptDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_regionDisplayName(ldn, "KK", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_regionDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_variantDisplayName(ldn, "ZZ", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_variantDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_keyDisplayName(ldn, "zz", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_keyDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    status = U_ZERO_ERROR;
    uldn_keyValueDisplayName(ldn, "ca", "zz", getName, kDisplayNameBracketsMax, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("FAIL uldn_keyValueDisplayName should return U_ILLEGAL_ARGUMENT_ERROR "
                "while no resource under UDISPCTX_NO_SUBSTITUTE");
    }

    uldn_close(ldn);
#endif
}

/*------------------------------
 * TestISOFunctions
 */

#if !UCONFIG_NO_FILE_IO && !UCONFIG_NO_LEGACY_CONVERSION
/* test for uloc_getISOLanguages, uloc_getISOCountries */
static void TestISOFunctions()
{
    const char* const* str=uloc_getISOLanguages();
    const char* const* str1=uloc_getISOCountries();
    const char* test;
    const char *key = NULL;
    int32_t count = 0, skipped = 0;
    int32_t expect;
    UResourceBundle *res;
    UResourceBundle *subRes;
    UErrorCode status = U_ZERO_ERROR;

    /*  test getISOLanguages*/
    /*str=uloc_getISOLanguages(); */
    log_verbose("Testing ISO Languages: \n");

    /* use structLocale - this data is no longer in root */
    res = ures_openDirect(loadTestData(&status), "structLocale", &status);
    subRes = ures_getByKey(res, "Languages", NULL, &status);
    if (U_FAILURE(status)) {
        log_data_err("There is an error in structLocale's ures_getByKey(\"Languages\"), status=%s\n", u_errorName(status));
        return;
    }

    expect = ures_getSize(subRes);
    for(count = 0; *(str+count) != 0; count++)
    {
        key = NULL;
        test = *(str+count);
        status = U_ZERO_ERROR;

        do {
            /* Skip over language tags. This API only returns language codes. */
            skipped += (key != NULL);
            ures_getNextString(subRes, NULL, &key, &status);
        }
        while (key != NULL && strchr(key, '_'));

        if(key == NULL)
            break;
        /* TODO: Consider removing sh, which is deprecated */
        if(strcmp(key,"root") == 0 || strcmp(key,"Fallback") == 0 || strcmp(key,"sh") == 0) {
            ures_getNextString(subRes, NULL, &key, &status);
            skipped++;
        }
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
        /* This code only works on ASCII machines where the keys are stored in ASCII order */
        if(strcmp(test,key)) {
            /* The first difference usually implies the place where things get out of sync */
            log_err("FAIL Language diff at offset %d, \"%s\" != \"%s\"\n", count, test, key);
        }
#endif

        if(!strcmp(test,"in"))
            log_err("FAIL getISOLanguages() has obsolete language code %s\n", test);
        if(!strcmp(test,"iw"))
            log_err("FAIL getISOLanguages() has obsolete language code %s\n", test);
        if(!strcmp(test,"ji"))
            log_err("FAIL getISOLanguages() has obsolete language code %s\n", test);
        if(!strcmp(test,"jw"))
            log_err("FAIL getISOLanguages() has obsolete language code %s\n", test);
        if(!strcmp(test,"sh"))
            log_err("FAIL getISOLanguages() has obsolete language code %s\n", test);
    }

    expect -= skipped; /* Ignore the skipped resources from structLocale */

    if(count!=expect) {
        log_err("There is an error in getISOLanguages, got %d, expected %d (as per structLocale)\n", count, expect);
    }

    subRes = ures_getByKey(res, "Countries", subRes, &status);
    log_verbose("Testing ISO Countries");
    skipped = 0;
    expect = ures_getSize(subRes) - 1; /* Skip ZZ */
    for(count = 0; *(str1+count) != 0; count++)
    {
        key = NULL;
        test = *(str1+count);
        do {
            /* Skip over numeric UN tags. This API only returns ISO-3166 codes. */
            skipped += (key != NULL);
            ures_getNextString(subRes, NULL, &key, &status);
        }
        while (key != NULL && strlen(key) != 2);

        if(key == NULL)
            break;
        /* TODO: Consider removing CS, which is deprecated */
        while(strcmp(key,"QO") == 0 || strcmp(key,"QU") == 0 || strcmp(key,"CS") == 0) {
            ures_getNextString(subRes, NULL, &key, &status);
            skipped++;
        }
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
        /* This code only works on ASCII machines where the keys are stored in ASCII order */
        if(strcmp(test,key)) {
            /* The first difference usually implies the place where things get out of sync */
            log_err("FAIL Country diff at offset %d, \"%s\" != \"%s\"\n", count, test, key);
        }
#endif
        if(!strcmp(test,"FX"))
            log_err("FAIL getISOCountries() has obsolete country code %s\n", test);
        if(!strcmp(test,"YU"))
            log_err("FAIL getISOCountries() has obsolete country code %s\n", test);
        if(!strcmp(test,"ZR"))
            log_err("FAIL getISOCountries() has obsolete country code %s\n", test);
    }

    ures_getNextString(subRes, NULL, &key, &status);
    if (strcmp(key, "ZZ") != 0) {
        log_err("ZZ was expected to be the last entry in structLocale, but got %s\n", key);
    }
#if U_CHARSET_FAMILY==U_EBCDIC_FAMILY
    /* On EBCDIC machines, the numbers are sorted last. Account for those in the skipped value too. */
    key = NULL;
    do {
        /* Skip over numeric UN tags. uloc_getISOCountries only returns ISO-3166 codes. */
        skipped += (key != NULL);
        ures_getNextString(subRes, NULL, &key, &status);
    }
    while (U_SUCCESS(status) && key != NULL && strlen(key) != 2);
#endif
    expect -= skipped; /* Ignore the skipped resources from structLocale */
    if(count!=expect)
    {
        log_err("There is an error in getISOCountries, got %d, expected %d \n", count, expect);
    }
    ures_close(subRes);
    ures_close(res);
}
#endif

static void setUpDataTable()
{
    int32_t i,j;
    dataTable = (UChar***)(calloc(sizeof(UChar**),LOCALE_INFO_SIZE));

    for (i = 0; i < LOCALE_INFO_SIZE; i++) {
        dataTable[i] = (UChar**)(calloc(sizeof(UChar*),LOCALE_SIZE));
        for (j = 0; j < LOCALE_SIZE; j++){
            dataTable[i][j] = CharsToUChars(rawData2[i][j]);
        }
    }
}

static void cleanUpDataTable()
{
    int32_t i,j;
    if(dataTable != NULL) {
        for (i=0; i<LOCALE_INFO_SIZE; i++) {
            for(j = 0; j < LOCALE_SIZE; j++) {
                free(dataTable[i][j]);
            }
            free(dataTable[i]);
        }
        free(dataTable);
    }
    dataTable = NULL;
}

/**
 * @bug 4011756 4011380
 */
static void TestISO3Fallback()
{
    const char* test="xx_YY";

    const char * result;

    result = uloc_getISO3Language(test);

    /* Conform to C API usage  */

    if (!result || (result[0] != 0))
       log_err("getISO3Language() on xx_YY returned %s instead of \"\"");

    result = uloc_getISO3Country(test);

    if (!result || (result[0] != 0))
        log_err("getISO3Country() on xx_YY returned %s instead of \"\"");
}

/**
 * @bug 4118587
 */
static void TestSimpleDisplayNames()
{
  /*
     This test is different from TestDisplayNames because TestDisplayNames checks
     fallback behavior, combination of language and country names to form locale
     names, and other stuff like that.  This test just checks specific language
     and country codes to make sure we have the correct names for them.
  */
    char languageCodes[] [4] = { "he", "id", "iu", "ug", "yi", "za", "419" };
    const char* languageNames [] = { "Hebrew", "Indonesian", "Inuktitut", "Uyghur", "Yiddish",
                               "Zhuang", "419" };
    const char* inLocale [] = { "en_US", "zh_Hant"};
    UErrorCode status=U_ZERO_ERROR;

    int32_t i;
    int32_t localeIndex = 0;
    for (i = 0; i < 7; i++) {
        UChar *testLang=0;
        UChar *expectedLang=0;
        int size=0;
        
        if (i == 6) {
            localeIndex = 1; /* Use the second locale for the rest of the test. */
        }
        
        size=uloc_getDisplayLanguage(languageCodes[i], inLocale[localeIndex], NULL, size, &status);
        if(status==U_BUFFER_OVERFLOW_ERROR) {
            status=U_ZERO_ERROR;
            testLang=(UChar*)malloc(sizeof(UChar) * (size + 1));
            uloc_getDisplayLanguage(languageCodes[i], inLocale[localeIndex], testLang, size + 1, &status);
        }
        expectedLang=(UChar*)malloc(sizeof(UChar) * (strlen(languageNames[i])+1));
        u_uastrcpy(expectedLang, languageNames[i]);
        if (u_strcmp(testLang, expectedLang) != 0)
            log_data_err("Got wrong display name for %s : Expected \"%s\", got \"%s\".\n",
                    languageCodes[i], languageNames[i], austrdup(testLang));
        free(testLang);
        free(expectedLang);
    }

}

/**
 * @bug 4118595
 */
static void TestUninstalledISO3Names()
{
  /* This test checks to make sure getISO3Language and getISO3Country work right
     even for locales that are not installed (and some installed ones). */
    static const char iso2Languages [][4] = {     "am", "ba", "fy", "mr", "rn",
                                        "ss", "tw", "zu", "sr" };
    static const char iso3Languages [][5] = {     "amh", "bak", "fry", "mar", "run",
                                        "ssw", "twi", "zul", "srp" };
    static const char iso2Countries [][6] = {     "am_AF", "ba_BW", "fy_KZ", "mr_MO", "rn_MN",
                                        "ss_SB", "tw_TC", "zu_ZW", "sr_XK" };
    static const char iso3Countries [][4] = {     "AFG", "BWA", "KAZ", "MAC", "MNG",
                                        "SLB", "TCA", "ZWE", "XKK" };
    int32_t i;

    for (i = 0; i < 9; i++) {
      UErrorCode err = U_ZERO_ERROR;
      const char *test;
      test = uloc_getISO3Language(iso2Languages[i]);
      if(strcmp(test, iso3Languages[i]) !=0 || U_FAILURE(err))
         log_err("Got wrong ISO3 code for %s : Expected \"%s\", got \"%s\". %s\n",
                     iso2Languages[i], iso3Languages[i], test, myErrorName(err));
    }
    for (i = 0; i < 9; i++) {
      UErrorCode err = U_ZERO_ERROR;
      const char *test;
      test = uloc_getISO3Country(iso2Countries[i]);
      if(strcmp(test, iso3Countries[i]) !=0 || U_FAILURE(err))
         log_err("Got wrong ISO3 code for %s : Expected \"%s\", got \"%s\". %s\n",
                     iso2Countries[i], iso3Countries[i], test, myErrorName(err));
    }
}


static void TestVariantParsing()
{
    static const char* en_US_custom="en_US_De Anza_Cupertino_California_United States_Earth";
    static const char* dispName="English (United States, DE ANZA_CUPERTINO_CALIFORNIA_UNITED STATES_EARTH)";
    static const char* dispVar="DE ANZA_CUPERTINO_CALIFORNIA_UNITED STATES_EARTH";
    static const char* shortVariant="fr_FR_foo";
    static const char* bogusVariant="fr_FR__foo";
    static const char* bogusVariant2="fr_FR_foo_";
    static const char* bogusVariant3="fr_FR__foo_";


    UChar displayVar[100];
    UChar displayName[100];
    UErrorCode status=U_ZERO_ERROR;
    UChar* got=0;
    int32_t size=0;
    size=uloc_getDisplayVariant(en_US_custom, "en_US", NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayVariant(en_US_custom, "en_US", got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    u_uastrcpy(displayVar, dispVar);
    if(u_strcmp(got,displayVar)!=0) {
        log_err("FAIL: getDisplayVariant() Wanted %s, got %s\n", dispVar, austrdup(got));
    }
    size=0;
    size=uloc_getDisplayName(en_US_custom, "en_US", NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayName(en_US_custom, "en_US", got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    u_uastrcpy(displayName, dispName);
    if(u_strcmp(got,displayName)!=0) {
        if (status == U_USING_DEFAULT_WARNING) {
            log_data_err("FAIL: getDisplayName() got %s. Perhaps you are missing data?\n", u_errorName(status));
        } else {
            log_err("FAIL: getDisplayName() Wanted %s, got %s\n", dispName, austrdup(got));
        }
    }

    size=0;
    status=U_ZERO_ERROR;
    size=uloc_getDisplayVariant(shortVariant, NULL, NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayVariant(shortVariant, NULL, got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    if(strcmp(austrdup(got),"FOO")!=0) {
        log_err("FAIL: getDisplayVariant()  Wanted: foo  Got: %s\n", austrdup(got));
    }
    size=0;
    status=U_ZERO_ERROR;
    size=uloc_getDisplayVariant(bogusVariant, NULL, NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayVariant(bogusVariant, NULL, got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    if(strcmp(austrdup(got),"_FOO")!=0) {
        log_err("FAIL: getDisplayVariant()  Wanted: _FOO  Got: %s\n", austrdup(got));
    }
    size=0;
    status=U_ZERO_ERROR;
    size=uloc_getDisplayVariant(bogusVariant2, NULL, NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayVariant(bogusVariant2, NULL, got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    if(strcmp(austrdup(got),"FOO_")!=0) {
        log_err("FAIL: getDisplayVariant()  Wanted: FOO_  Got: %s\n", austrdup(got));
    }
    size=0;
    status=U_ZERO_ERROR;
    size=uloc_getDisplayVariant(bogusVariant3, NULL, NULL, size, &status);
    if(status==U_BUFFER_OVERFLOW_ERROR) {
        status=U_ZERO_ERROR;
        got=(UChar*)realloc(got, sizeof(UChar) * (size+1));
        uloc_getDisplayVariant(bogusVariant3, NULL, got, size + 1, &status);
    }
    else {
        log_err("FAIL: Didn't get U_BUFFER_OVERFLOW_ERROR\n");
    }
    if(strcmp(austrdup(got),"_FOO_")!=0) {
        log_err("FAIL: getDisplayVariant()  Wanted: _FOO_  Got: %s\n", austrdup(got));
    }
    free(got);
}


static void TestObsoleteNames(void)
{
    int32_t i;
    UErrorCode status = U_ZERO_ERROR;
    char buff[256];

    static const struct
    {
        char locale[9];
        char lang3[4];
        char lang[4];
        char ctry3[4];
        char ctry[4];
    } tests[] =
    {
        { "eng_USA", "eng", "en", "USA", "US" },
        { "kok",  "kok", "kok", "", "" },
        { "in",  "ind", "in", "", "" },
        { "id",  "ind", "id", "", "" }, /* NO aliasing */
        { "sh",  "srp", "sh", "", "" },
        { "zz_CS",  "", "zz", "SCG", "CS" },
        { "zz_FX",  "", "zz", "FXX", "FX" },
        { "zz_RO",  "", "zz", "ROU", "RO" },
        { "zz_TP",  "", "zz", "TMP", "TP" },
        { "zz_TL",  "", "zz", "TLS", "TL" },
        { "zz_ZR",  "", "zz", "ZAR", "ZR" },
        { "zz_FXX",  "", "zz", "FXX", "FX" }, /* no aliasing. Doesn't go to PS(PSE). */
        { "zz_ROM",  "", "zz", "ROU", "RO" },
        { "zz_ROU",  "", "zz", "ROU", "RO" },
        { "zz_ZAR",  "", "zz", "ZAR", "ZR" },
        { "zz_TMP",  "", "zz", "TMP", "TP" },
        { "zz_TLS",  "", "zz", "TLS", "TL" },
        { "zz_YUG",  "", "zz", "YUG", "YU" },
        { "mlt_PSE", "mlt", "mt", "PSE", "PS" },
        { "iw", "heb", "iw", "", "" },
        { "ji", "yid", "ji", "", "" },
        { "jw", "jaw", "jw", "", "" },
        { "sh", "srp", "sh", "", "" },
        { "", "", "", "", "" }
    };

    for(i=0;tests[i].locale[0];i++)
    {
        const char *locale;

        locale = tests[i].locale;
        log_verbose("** %s:\n", locale);

        status = U_ZERO_ERROR;
        if(strcmp(tests[i].lang3,uloc_getISO3Language(locale)))
        {
            log_err("FAIL: uloc_getISO3Language(%s)==\t\"%s\",\t expected \"%s\"\n",
                locale,  uloc_getISO3Language(locale), tests[i].lang3);
        }
        else
        {
            log_verbose("   uloc_getISO3Language()==\t\"%s\"\n",
                uloc_getISO3Language(locale) );
        }

        status = U_ZERO_ERROR;
        uloc_getLanguage(locale, buff, 256, &status);
        if(U_FAILURE(status))
        {
            log_err("FAIL: error getting language from %s\n", locale);
        }
        else
        {
            if(strcmp(buff,tests[i].lang))
            {
                log_err("FAIL: uloc_getLanguage(%s)==\t\"%s\"\t expected \"%s\"\n",
                    locale, buff, tests[i].lang);
            }
            else
            {
                log_verbose("  uloc_getLanguage(%s)==\t%s\n", locale, buff);
            }
        }
        if(strcmp(tests[i].lang3,uloc_getISO3Language(locale)))
        {
            log_err("FAIL: uloc_getISO3Language(%s)==\t\"%s\",\t expected \"%s\"\n",
                locale,  uloc_getISO3Language(locale), tests[i].lang3);
        }
        else
        {
            log_verbose("   uloc_getISO3Language()==\t\"%s\"\n",
                uloc_getISO3Language(locale) );
        }

        if(strcmp(tests[i].ctry3,uloc_getISO3Country(locale)))
        {
            log_err("FAIL: uloc_getISO3Country(%s)==\t\"%s\",\t expected \"%s\"\n",
                locale,  uloc_getISO3Country(locale), tests[i].ctry3);
        }
        else
        {
            log_verbose("   uloc_getISO3Country()==\t\"%s\"\n",
                uloc_getISO3Country(locale) );
        }

        status = U_ZERO_ERROR;
        uloc_getCountry(locale, buff, 256, &status);
        if(U_FAILURE(status))
        {
            log_err("FAIL: error getting country from %s\n", locale);
        }
        else
        {
            if(strcmp(buff,tests[i].ctry))
            {
                log_err("FAIL: uloc_getCountry(%s)==\t\"%s\"\t expected \"%s\"\n",
                    locale, buff, tests[i].ctry);
            }
            else
            {
                log_verbose("  uloc_getCountry(%s)==\t%s\n", locale, buff);
            }
        }
    }

    if (uloc_getLCID("iw_IL") != uloc_getLCID("he_IL")) {
        log_err("he,iw LCID mismatch: %X versus %X\n", uloc_getLCID("iw_IL"), uloc_getLCID("he_IL"));
    }

    if (uloc_getLCID("iw") != uloc_getLCID("he")) {
        log_err("he,iw LCID mismatch: %X versus %X\n", uloc_getLCID("iw"), uloc_getLCID("he"));
    }

#if 0

    i = uloc_getLanguage("kok",NULL,0,&icu_err);
    if(U_FAILURE(icu_err))
    {
        log_err("FAIL: Got %s trying to do uloc_getLanguage(kok)\n", u_errorName(icu_err));
    }

    icu_err = U_ZERO_ERROR;
    uloc_getLanguage("kok",r1_buff,12,&icu_err);
    if(U_FAILURE(icu_err))
    {
        log_err("FAIL: Got %s trying to do uloc_getLanguage(kok, buff)\n", u_errorName(icu_err));
    }

    r1_addr = (char *)uloc_getISO3Language("kok");

    icu_err = U_ZERO_ERROR;
    if (strcmp(r1_buff,"kok") != 0)
    {
        log_err("FAIL: uloc_getLanguage(kok)==%s not kok\n",r1_buff);
        line--;
    }
    r1_addr = (char *)uloc_getISO3Language("in");
    i = uloc_getLanguage(r1_addr,r1_buff,12,&icu_err);
    if (strcmp(r1_buff,"id") != 0)
    {
        printf("uloc_getLanguage error (%s)\n",r1_buff);
        line--;
    }
    r1_addr = (char *)uloc_getISO3Language("sh");
    i = uloc_getLanguage(r1_addr,r1_buff,12,&icu_err);
    if (strcmp(r1_buff,"sr") != 0)
    {
        printf("uloc_getLanguage error (%s)\n",r1_buff);
        line--;
    }

    r1_addr = (char *)uloc_getISO3Country("zz_ZR");
    strcpy(p1_buff,"zz_");
    strcat(p1_buff,r1_addr);
    i = uloc_getCountry(p1_buff,r1_buff,12,&icu_err);
    if (strcmp(r1_buff,"ZR") != 0)
    {
        printf("uloc_getCountry error (%s)\n",r1_buff);
        line--;
    }
    r1_addr = (char *)uloc_getISO3Country("zz_FX");
    strcpy(p1_buff,"zz_");
    strcat(p1_buff,r1_addr);
    i = uloc_getCountry(p1_buff,r1_buff,12,&icu_err);
    if (strcmp(r1_buff,"FX") != 0)
    {
        printf("uloc_getCountry error (%s)\n",r1_buff);
        line--;
    }

#endif

}

static void TestKeywordVariants(void) 
{
    static const struct {
        const char *localeID;
        const char *expectedLocaleID;           /* uloc_getName */
        const char *expectedLocaleIDNoKeywords; /* uloc_getBaseName */
        const char *expectedCanonicalID;        /* uloc_canonicalize */
        const char *expectedKeywords[10];
        int32_t numKeywords;
        UErrorCode expectedStatus; /* from uloc_openKeywords */
    } testCases[] = {
        {
            "de_DE@  currency = euro; C o ll A t i o n   = Phonebook   ; C alen dar = buddhist   ", 
            "de_DE@calendar=buddhist;collation=Phonebook;currency=euro", 
            "de_DE",
            "de_DE@calendar=buddhist;collation=Phonebook;currency=euro", 
            {"calendar", "collation", "currency"},
            3,
            U_ZERO_ERROR
        },
        {
            "de_DE@euro",
            "de_DE@euro",
            "de_DE@euro",   /* we probably should strip off the POSIX style variant @euro see #11690 */
            "de_DE_EURO",
            {"","","","","","",""},
            0,
            U_INVALID_FORMAT_ERROR /* must have '=' after '@' */
        },
        {
            "de_DE@euro;collation=phonebook",   /* The POSIX style variant @euro cannot be combined with key=value? */
            "de_DE", /* getName returns de_DE - should be INVALID_FORMAT_ERROR? */
            "de_DE", /* getBaseName returns de_DE - should be INVALID_FORMAT_ERROR? see #11690 */
            "de_DE", /* canonicalize returns de_DE - should be INVALID_FORMAT_ERROR? */
            {"","","","","","",""},
            0,
            U_INVALID_FORMAT_ERROR
        },
        {
            "de_DE@collation=",
            0, /* expected getName to fail */
            "de_DE", /* getBaseName returns de_DE - should be INVALID_FORMAT_ERROR? see #11690 */
            0, /* expected canonicalize to fail */
            {"","","","","","",""},
            0,
            U_INVALID_FORMAT_ERROR /* must have '=' after '@' */
        }
    };
    UErrorCode status = U_ZERO_ERROR;
    
    int32_t i = 0, j = 0;
    int32_t resultLen = 0;
    char buffer[256];
    UEnumeration *keywords;
    int32_t keyCount = 0;
    const char *keyword = NULL;
    int32_t keywordLen = 0;
    
    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        status = U_ZERO_ERROR;
        *buffer = 0;
        keywords = uloc_openKeywords(testCases[i].localeID, &status);
        
        if(status != testCases[i].expectedStatus) {
            log_err("Expected to uloc_openKeywords(\"%s\") => status %s. Got %s instead\n", 
                    testCases[i].localeID,
                    u_errorName(testCases[i].expectedStatus), u_errorName(status));
        }
        status = U_ZERO_ERROR;
        if(keywords) {
            if((keyCount = uenum_count(keywords, &status)) != testCases[i].numKeywords) {
                log_err("Expected to get %i keywords, got %i\n", testCases[i].numKeywords, keyCount);
            }
            if(keyCount) {
                j = 0;
                while((keyword = uenum_next(keywords, &keywordLen, &status))) {
                    if(strcmp(keyword, testCases[i].expectedKeywords[j]) != 0) {
                        log_err("Expected to get keyword value %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                    }
                    j++;
                }
                j = 0;
                uenum_reset(keywords, &status);
                while((keyword = uenum_next(keywords, &keywordLen, &status))) {
                    if(strcmp(keyword, testCases[i].expectedKeywords[j]) != 0) {
                        log_err("Expected to get keyword value %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                    }
                    j++;
                }
            }
            uenum_close(keywords);
        }

        status = U_ZERO_ERROR;
        resultLen = uloc_getName(testCases[i].localeID, buffer, 256, &status);
        (void)resultLen;
        U_ASSERT(resultLen < 256);
        if (U_SUCCESS(status)) {
            if (testCases[i].expectedLocaleID == 0) {
                log_err("Expected uloc_getName(\"%s\") to fail; got \"%s\"\n",
                        testCases[i].localeID, buffer);
            } else if (uprv_strcmp(testCases[i].expectedLocaleID, buffer) != 0) {
                log_err("Expected uloc_getName(\"%s\") => \"%s\"; got \"%s\"\n",
                        testCases[i].localeID, testCases[i].expectedLocaleID, buffer);
            }
        } else {
            if (testCases[i].expectedLocaleID != 0) {
                log_err("Expected uloc_getName(\"%s\") => \"%s\"; but returned error: %s\n",
                        testCases[i].localeID, testCases[i].expectedLocaleID, buffer, u_errorName(status));
            }
        }

        status = U_ZERO_ERROR;
        resultLen = uloc_getBaseName(testCases[i].localeID, buffer, 256, &status);
        U_ASSERT(resultLen < 256);
        if (U_SUCCESS(status)) {
            if (testCases[i].expectedLocaleIDNoKeywords == 0) {
                log_err("Expected uloc_getBaseName(\"%s\") to fail; got \"%s\"\n",
                        testCases[i].localeID, buffer);
            } else if (uprv_strcmp(testCases[i].expectedLocaleIDNoKeywords, buffer) != 0) {
                log_err("Expected uloc_getBaseName(\"%s\") => \"%s\"; got \"%s\"\n",
                        testCases[i].localeID, testCases[i].expectedLocaleIDNoKeywords, buffer);
            }
        } else {
            if (testCases[i].expectedLocaleIDNoKeywords != 0) {
                log_err("Expected uloc_getBaseName(\"%s\") => \"%s\"; but returned error: %s\n",
                        testCases[i].localeID, testCases[i].expectedLocaleIDNoKeywords, buffer, u_errorName(status));
            }
        }

        status = U_ZERO_ERROR;
        resultLen = uloc_canonicalize(testCases[i].localeID, buffer, 256, &status);
        U_ASSERT(resultLen < 256);
        if (U_SUCCESS(status)) {
            if (testCases[i].expectedCanonicalID == 0) {
                log_err("Expected uloc_canonicalize(\"%s\") to fail; got \"%s\"\n",
                        testCases[i].localeID, buffer);
            } else if (uprv_strcmp(testCases[i].expectedCanonicalID, buffer) != 0) {
                log_err("Expected uloc_canonicalize(\"%s\") => \"%s\"; got \"%s\"\n",
                        testCases[i].localeID, testCases[i].expectedCanonicalID, buffer);
            }
        } else {
            if (testCases[i].expectedCanonicalID != 0) {
                log_err("Expected uloc_canonicalize(\"%s\") => \"%s\"; but returned error: %s\n",
                        testCases[i].localeID, testCases[i].expectedCanonicalID, buffer, u_errorName(status));
            }
        }
    }
}

static void TestKeywordVariantParsing(void) 
{
    static const struct {
        const char *localeID;
        const char *keyword;
        const char *expectedValue; /* NULL if failure is expected */
    } testCases[] = {
        { "de_DE@  C o ll A t i o n   = Phonebook   ", "c o ll a t i o n", NULL }, /* malformed key name */
        { "de_DE", "collation", ""},
        { "de_DE@collation=PHONEBOOK", "collation", "PHONEBOOK" },
        { "de_DE@currency = euro; CoLLaTion   = PHONEBOOk", "collatiON", "PHONEBOOk" },
    };
    
    UErrorCode status;
    int32_t i = 0;
    int32_t resultLen = 0;
    char buffer[256];
    
    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        *buffer = 0;
        status = U_ZERO_ERROR;
        resultLen = uloc_getKeywordValue(testCases[i].localeID, testCases[i].keyword, buffer, 256, &status);
        (void)resultLen;    /* Suppress set but not used warning. */
        if (testCases[i].expectedValue) {
            /* expect success */
            if (U_FAILURE(status)) {
                log_err("Expected to extract \"%s\" from \"%s\" for keyword \"%s\". Instead got status %s\n",
                    testCases[i].expectedValue, testCases[i].localeID, testCases[i].keyword, u_errorName(status));
            } else if (uprv_strcmp(testCases[i].expectedValue, buffer) != 0) {
                log_err("Expected to extract \"%s\" from \"%s\" for keyword \"%s\". Instead got \"%s\"\n",
                    testCases[i].expectedValue, testCases[i].localeID, testCases[i].keyword, buffer);
            }
        } else if (U_SUCCESS(status)) {
            /* expect failure */
            log_err("Expected failure but got success from \"%s\" for keyword \"%s\". Got \"%s\"\n",
                testCases[i].localeID, testCases[i].keyword, buffer);
            
        }
    }
}

static const struct {
  const char *l; /* locale */
  const char *k; /* kw */
  const char *v; /* value */
  const char *x; /* expected */
} kwSetTestCases[] = {
#if 1
  { "en_US", "calendar", "japanese", "en_US@calendar=japanese" },
  { "en_US@", "calendar", "japanese", "en_US@calendar=japanese" },
  { "en_US@calendar=islamic", "calendar", "japanese", "en_US@calendar=japanese" },
  { "en_US@calendar=slovakian", "calendar", "gregorian", "en_US@calendar=gregorian" }, /* don't know what this means, but it has the same # of chars as gregorian */
  { "en_US@calendar=gregorian", "calendar", "japanese", "en_US@calendar=japanese" },
  { "de", "Currency", "CHF", "de@currency=CHF" },
  { "de", "Currency", "CHF", "de@currency=CHF" },

  { "en_US@collation=phonebook", "calendar", "japanese", "en_US@calendar=japanese;collation=phonebook" },
  { "en_US@calendar=japanese", "collation", "phonebook", "en_US@calendar=japanese;collation=phonebook" },
  { "de@collation=phonebook", "Currency", "CHF", "de@collation=phonebook;currency=CHF" },
  { "en_US@calendar=gregorian;collation=phonebook", "calendar", "japanese", "en_US@calendar=japanese;collation=phonebook" },
  { "en_US@calendar=slovakian;collation=phonebook", "calendar", "gregorian", "en_US@calendar=gregorian;collation=phonebook" }, /* don't know what this means, but it has the same # of chars as gregorian */
  { "en_US@calendar=slovakian;collation=videobook", "collation", "phonebook", "en_US@calendar=slovakian;collation=phonebook" }, /* don't know what this means, but it has the same # of chars as gregorian */
  { "en_US@calendar=islamic;collation=phonebook", "calendar", "japanese", "en_US@calendar=japanese;collation=phonebook" },
  { "de@collation=phonebook", "Currency", "CHF", "de@collation=phonebook;currency=CHF" },
#endif
#if 1
  { "mt@a=0;b=1;c=2;d=3", "c","j", "mt@a=0;b=1;c=j;d=3" },
  { "mt@a=0;b=1;c=2;d=3", "x","j", "mt@a=0;b=1;c=2;d=3;x=j" },
  { "mt@a=0;b=1;c=2;d=3", "a","f", "mt@a=f;b=1;c=2;d=3" },
  { "mt@a=0;aa=1;aaa=3", "a","x", "mt@a=x;aa=1;aaa=3" },
  { "mt@a=0;aa=1;aaa=3", "aa","x", "mt@a=0;aa=x;aaa=3" },
  { "mt@a=0;aa=1;aaa=3", "aaa","x", "mt@a=0;aa=1;aaa=x" },
  { "mt@a=0;aa=1;aaa=3", "a","yy", "mt@a=yy;aa=1;aaa=3" },
  { "mt@a=0;aa=1;aaa=3", "aa","yy", "mt@a=0;aa=yy;aaa=3" },
  { "mt@a=0;aa=1;aaa=3", "aaa","yy", "mt@a=0;aa=1;aaa=yy" },
#endif
#if 1
  /* removal tests */
  /* 1. removal of item at end */
  { "de@collation=phonebook;currency=CHF", "currency",   "", "de@collation=phonebook" },
  { "de@collation=phonebook;currency=CHF", "currency", NULL, "de@collation=phonebook" },
  /* 2. removal of item at beginning */
  { "de@collation=phonebook;currency=CHF", "collation", "", "de@currency=CHF" },
  { "de@collation=phonebook;currency=CHF", "collation", NULL, "de@currency=CHF" },
  /* 3. removal of an item not there */
  { "de@collation=phonebook;currency=CHF", "calendar", NULL, "de@collation=phonebook;currency=CHF" },
  /* 4. removal of only item */
  { "de@collation=phonebook", "collation", NULL, "de" },
#endif
  { "de@collation=phonebook", "Currency", "CHF", "de@collation=phonebook;currency=CHF" },
  /* cases with legal extra spacing */
  /*31*/{ "en_US@ calendar = islamic", "calendar", "japanese", "en_US@calendar=japanese" },
  /*32*/{ "en_US@ calendar = gregorian ; collation = phonebook", "calendar", "japanese", "en_US@calendar=japanese;collation=phonebook" },
  /*33*/{ "en_US@ calendar = islamic", "currency", "CHF", "en_US@calendar=islamic;currency=CHF" },
  /*34*/{ "en_US@ currency = CHF", "calendar", "japanese", "en_US@calendar=japanese;currency=CHF" },
  /* cases in which setKeywordValue expected to fail (implied by NULL for expected); locale need not be canonical */
  /*35*/{ "en_US@calendar=gregorian;", "calendar", "japanese", NULL },
  /*36*/{ "en_US@calendar=gregorian;=", "calendar", "japanese", NULL },
  /*37*/{ "en_US@calendar=gregorian;currency=", "calendar", "japanese", NULL },
  /*38*/{ "en_US@=", "calendar", "japanese", NULL },
  /*39*/{ "en_US@=;", "calendar", "japanese", NULL },
  /*40*/{ "en_US@= ", "calendar", "japanese", NULL },
  /*41*/{ "en_US@ =", "calendar", "japanese", NULL },
  /*42*/{ "en_US@ = ", "calendar", "japanese", NULL },
  /*43*/{ "en_US@=;calendar=gregorian", "calendar", "japanese", NULL },
  /*44*/{ "en_US@= calen dar = gregorian", "calendar", "japanese", NULL },
  /*45*/{ "en_US@= calendar = greg orian", "calendar", "japanese", NULL },
  /*46*/{ "en_US@=;cal...endar=gregorian", "calendar", "japanese", NULL },
  /*47*/{ "en_US@=;calendar=greg...orian", "calendar", "japanese", NULL },
  /*48*/{ "en_US@calendar=gregorian", "cale ndar", "japanese", NULL },
  /*49*/{ "en_US@calendar=gregorian", "calendar", "japa..nese", NULL },
  /* cases in which getKeywordValue and setKeyword expected to fail (implied by NULL for value and expected) */
  /*50*/{ "en_US@=", "calendar", NULL, NULL },
  /*51*/{ "en_US@=;", "calendar", NULL, NULL },
  /*52*/{ "en_US@= ", "calendar", NULL, NULL },
  /*53*/{ "en_US@ =", "calendar", NULL, NULL },
  /*54*/{ "en_US@ = ", "calendar", NULL, NULL },
  /*55*/{ "en_US@=;calendar=gregorian", "calendar", NULL, NULL },
  /*56*/{ "en_US@= calen dar = gregorian", "calendar", NULL, NULL },
  /*57*/{ "en_US@= calendar = greg orian", "calendar", NULL, NULL },
  /*58*/{ "en_US@=;cal...endar=gregorian", "calendar", NULL, NULL },
  /*59*/{ "en_US@=;calendar=greg...orian", "calendar", NULL, NULL },
  /*60*/{ "en_US@calendar=gregorian", "cale ndar", NULL, NULL },
};


static void TestKeywordSet(void)
{
    int32_t i = 0;
    int32_t resultLen = 0;
    char buffer[1024];

    char cbuffer[1024];

    for(i = 0; i < UPRV_LENGTHOF(kwSetTestCases); i++) {
      UErrorCode status = U_ZERO_ERROR;
      memset(buffer,'%',1023);
      strcpy(buffer, kwSetTestCases[i].l);

      if (kwSetTestCases[i].x != NULL) {
        uloc_canonicalize(kwSetTestCases[i].l, cbuffer, 1023, &status);
        if(strcmp(buffer,cbuffer)) {
          log_verbose("note: [%d] wasn't canonical, should be: '%s' not '%s'. Won't check for canonicity in output.\n", i, cbuffer, buffer);
        }
        /* sanity check test case results for canonicity */
        uloc_canonicalize(kwSetTestCases[i].x, cbuffer, 1023, &status);
        if(strcmp(kwSetTestCases[i].x,cbuffer)) {
          log_err("%s:%d: ERROR: kwSetTestCases[%d].x = '%s', should be %s (must be canonical)\n", __FILE__, __LINE__, i, kwSetTestCases[i].x, cbuffer);
        }

        status = U_ZERO_ERROR;
        resultLen = uloc_setKeywordValue(kwSetTestCases[i].k, kwSetTestCases[i].v, buffer, 1023, &status);
        if(U_FAILURE(status)) {
          log_err("Err on test case %d for setKeywordValue: got error %s\n", i, u_errorName(status));
        } else if(strcmp(buffer,kwSetTestCases[i].x) || ((int32_t)strlen(buffer)!=resultLen)) {
          log_err("FAIL: #%d setKeywordValue: %s + [%s=%s] -> %s (%d) expected %s (%d)\n", i, kwSetTestCases[i].l, kwSetTestCases[i].k,
                  kwSetTestCases[i].v, buffer, resultLen, kwSetTestCases[i].x, strlen(buffer));
        } else {
          log_verbose("pass: #%d: %s + [%s=%s] -> %s\n", i, kwSetTestCases[i].l, kwSetTestCases[i].k, kwSetTestCases[i].v,buffer);
        }

        if (kwSetTestCases[i].v != NULL && kwSetTestCases[i].v[0] != 0) {
          status = U_ZERO_ERROR;
          resultLen = uloc_getKeywordValue(kwSetTestCases[i].x, kwSetTestCases[i].k, buffer, 1023, &status);
          if(U_FAILURE(status)) {
            log_err("Err on test case %d for getKeywordValue: got error %s\n", i, u_errorName(status));
          } else if (resultLen != (int32_t)uprv_strlen(kwSetTestCases[i].v) || uprv_strcmp(buffer, kwSetTestCases[i].v) != 0) {
            log_err("FAIL: #%d getKeywordValue: got %s (%d) expected %s (%d)\n", i, buffer, resultLen,
                    kwSetTestCases[i].v, uprv_strlen(kwSetTestCases[i].v));
          }
        }
      } else {
        /* test cases expected to result in error */
        status = U_ZERO_ERROR;
        resultLen = uloc_setKeywordValue(kwSetTestCases[i].k, kwSetTestCases[i].v, buffer, 1023, &status);
        if(U_SUCCESS(status)) {
          log_err("Err on test case %d for setKeywordValue: expected to fail but succeeded, got %s (%d)\n", i, buffer, resultLen);
        }

        if (kwSetTestCases[i].v == NULL) {
          status = U_ZERO_ERROR;
          strcpy(cbuffer, kwSetTestCases[i].l);
          resultLen = uloc_getKeywordValue(cbuffer, kwSetTestCases[i].k, buffer, 1023, &status);
          if(U_SUCCESS(status)) {
            log_err("Err on test case %d for getKeywordValue: expected to fail but succeeded\n", i);
          }
        }
      }
    }
}

static void TestKeywordSetError(void)
{
    char buffer[1024];
    UErrorCode status;
    int32_t res;
    int32_t i;
    int32_t blen;

    /* 0-test whether an error condition modifies the buffer at all */
    blen=0;
    i=0;
    memset(buffer,'%',1023);
    status = U_ZERO_ERROR;
    res = uloc_setKeywordValue(kwSetTestCases[i].k, kwSetTestCases[i].v, buffer, blen, &status);
    if(status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("expected illegal err got %s\n", u_errorName(status));
        return;
    }
    /*  if(res!=strlen(kwSetTestCases[i].x)) {
    log_err("expected result %d got %d\n", strlen(kwSetTestCases[i].x), res);
    return;
    } */
    if(buffer[blen]!='%') {
        log_err("Buffer byte %d was modified: now %c\n", blen, buffer[blen]);
        return;
    }
    log_verbose("0-buffer modify OK\n");

    for(i=0;i<=2;i++) {
        /* 1- test a short buffer with growing text */
        blen=(int32_t)strlen(kwSetTestCases[i].l)+1;
        memset(buffer,'%',1023);
        strcpy(buffer,kwSetTestCases[i].l);
        status = U_ZERO_ERROR;
        res = uloc_setKeywordValue(kwSetTestCases[i].k, kwSetTestCases[i].v, buffer, blen, &status);
        if(status != U_BUFFER_OVERFLOW_ERROR) {
            log_err("expected buffer overflow on buffer %d got %s, len %d (%s + [%s=%s])\n", blen, u_errorName(status), res, kwSetTestCases[i].l, kwSetTestCases[i].k, kwSetTestCases[i].v);
            return;
        }
        if(res!=(int32_t)strlen(kwSetTestCases[i].x)) {
            log_err("expected result %d got %d\n", strlen(kwSetTestCases[i].x), res);
            return;
        }
        if(buffer[blen]!='%') {
            log_err("Buffer byte %d was modified: now %c\n", blen, buffer[blen]);
            return;
        }
        log_verbose("1/%d-buffer modify OK\n",i);
    }

    for(i=3;i<=4;i++) {
        /* 2- test a short buffer - text the same size or shrinking   */
        blen=(int32_t)strlen(kwSetTestCases[i].l)+1;
        memset(buffer,'%',1023);
        strcpy(buffer,kwSetTestCases[i].l);
        status = U_ZERO_ERROR;
        res = uloc_setKeywordValue(kwSetTestCases[i].k, kwSetTestCases[i].v, buffer, blen, &status);
        if(status != U_ZERO_ERROR) {
            log_err("expected zero error got %s\n", u_errorName(status));
            return;
        }
        if(buffer[blen+1]!='%') {
            log_err("Buffer byte %d was modified: now %c\n", blen+1, buffer[blen+1]);
            return;
        }
        if(res!=(int32_t)strlen(kwSetTestCases[i].x)) {
            log_err("expected result %d got %d\n", strlen(kwSetTestCases[i].x), res);
            return;
        }
        if(strcmp(buffer,kwSetTestCases[i].x) || ((int32_t)strlen(buffer)!=res)) {
            log_err("FAIL: #%d: %s + [%s=%s] -> %s (%d) expected %s (%d)\n", i, kwSetTestCases[i].l, kwSetTestCases[i].k,
                kwSetTestCases[i].v, buffer, res, kwSetTestCases[i].x, strlen(buffer));
        } else {
            log_verbose("pass: #%d: %s + [%s=%s] -> %s\n", i, kwSetTestCases[i].l, kwSetTestCases[i].k, kwSetTestCases[i].v,
                buffer);
        }
        log_verbose("2/%d-buffer modify OK\n",i);
    }
}

static int32_t _canonicalize(int32_t selector, /* 0==getName, 1==canonicalize */
                             const char* localeID,
                             char* result,
                             int32_t resultCapacity,
                             UErrorCode* ec) {
    /* YOU can change this to use function pointers if you like */
    switch (selector) {
    case 0:
        return uloc_getName(localeID, result, resultCapacity, ec);
    case 1:
        return uloc_canonicalize(localeID, result, resultCapacity, ec);
#if APPLE_ICU_CHANGES
// rdar://
    case 2:
        return ualoc_canonicalForm(localeID, result, resultCapacity, ec);
#endif  // APPLE_ICU_CHANGES
    default:
        return -1;
    }
}

static void TestCanonicalization(void)
{
    static const struct {
        const char *localeID;    /* input */
        const char *getNameID;   /* expected getName() result */
        const char *canonicalID; /* expected canonicalize() result */
    } testCases[] = {
        { "ca_ES-with-extra-stuff-that really doesn't make any sense-unless-you're trying to increase code coverage",
          "ca_ES_WITH_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_YOU'RE TRYING TO INCREASE CODE COVERAGE",
          "ca_ES_WITH_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_YOU'RE TRYING TO INCREASE CODE COVERAGE"},
        { "zh@collation=pinyin", "zh@collation=pinyin", "zh@collation=pinyin" },
        { "zh_CN@collation=pinyin", "zh_CN@collation=pinyin", "zh_CN@collation=pinyin" },
        { "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin" },
        { "en_US_POSIX", "en_US_POSIX", "en_US_POSIX" }, 
        { "hy_AM_REVISED", "hy_AM_REVISED", "hy_AM_REVISED" }, 
        { "no_NO_NY", "no_NO_NY", "no_NO_NY" /* not: "nn_NO" [alan ICU3.0] */ },
        { "no@ny", "no@ny", "no__NY" /* not: "nn" [alan ICU3.0] */ }, /* POSIX ID */
        { "no-no.utf32@B", "no_NO.utf32@B", "no_NO_B" /* not: "nb_NO_B" [alan ICU3.0] */ }, /* POSIX ID */
        { "qz-qz@Euro", "qz_QZ@Euro", "qz_QZ_EURO" }, /* qz-qz uses private use iso codes */
        { "en-BOONT", "en__BOONT", "en__BOONT" }, /* registered name */
        { "de-1901", "de__1901", "de__1901" }, /* registered name */
        { "de-1906", "de__1906", "de__1906" }, /* registered name */

        /* posix behavior that used to be performed by getName */
        { "mr.utf8", "mr.utf8", "mr" },
        { "de-tv.koi8r", "de_TV.koi8r", "de_TV" },
        { "x-piglatin_ML.MBE", "x-piglatin_ML.MBE", "x-piglatin_ML" },
        { "i-cherokee_US.utf7", "i-cherokee_US.utf7", "i-cherokee_US" },
        { "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA" },
        { "no-no-ny.utf8@B", "no_NO_NY.utf8@B", "no_NO_NY_B" /* not: "nn_NO" [alan ICU3.0] */ }, /* @ ignored unless variant is empty */

        /* fleshing out canonicalization */
        /* trim space and sort keywords, ';' is separator so not present at end in canonical form */
        { "en_Hant_IL_VALLEY_GIRL@ currency = EUR; calendar = Japanese ;", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR" },
        /* already-canonical ids are not changed */
        { "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR" },
        /* norwegian is just too weird, if we handle things in their full generality */
        { "no-Hant-GB_NY@currency=$$$", "no_Hant_GB_NY@currency=$$$", "no_Hant_GB_NY@currency=$$$" /* not: "nn_Hant_GB@currency=$$$" [alan ICU3.0] */ },

        /* test cases reflecting internal resource bundle usage */
        { "root@kw=foo", "root@kw=foo", "root@kw=foo" },
        { "@calendar=gregorian", "@calendar=gregorian", "@calendar=gregorian" },
        { "ja_JP@calendar=Japanese", "ja_JP@calendar=Japanese", "ja_JP@calendar=Japanese" },
        { "ja_JP", "ja_JP", "ja_JP" },

        /* test case for "i-default" */
        { "i-default", "en@x=i-default", "en@x=i-default" },

        // Before ICU 64, ICU locale canonicalization had some additional mappings.
        // They were removed for ICU-20187 "drop support for long-obsolete locale ID variants".
        // The following now use standard canonicalization.
        { "ca_ES_PREEURO", "ca_ES_PREEURO", "ca_ES_PREEURO" },
        { "de_AT_PREEURO", "de_AT_PREEURO", "de_AT_PREEURO" },
        { "de_DE_PREEURO", "de_DE_PREEURO", "de_DE_PREEURO" },
        { "de_LU_PREEURO", "de_LU_PREEURO", "de_LU_PREEURO" },
        { "el_GR_PREEURO", "el_GR_PREEURO", "el_GR_PREEURO" },
        { "en_BE_PREEURO", "en_BE_PREEURO", "en_BE_PREEURO" },
        { "en_IE_PREEURO", "en_IE_PREEURO", "en_IE_PREEURO" },
        { "es_ES_PREEURO", "es_ES_PREEURO", "es_ES_PREEURO" },
        { "eu_ES_PREEURO", "eu_ES_PREEURO", "eu_ES_PREEURO" },
        { "fi_FI_PREEURO", "fi_FI_PREEURO", "fi_FI_PREEURO" },
        { "fr_BE_PREEURO", "fr_BE_PREEURO", "fr_BE_PREEURO" },
        { "fr_FR_PREEURO", "fr_FR_PREEURO", "fr_FR_PREEURO" },
        { "fr_LU_PREEURO", "fr_LU_PREEURO", "fr_LU_PREEURO" },
        { "ga_IE_PREEURO", "ga_IE_PREEURO", "ga_IE_PREEURO" },
        { "gl_ES_PREEURO", "gl_ES_PREEURO", "gl_ES_PREEURO" },
        { "it_IT_PREEURO", "it_IT_PREEURO", "it_IT_PREEURO" },
        { "nl_BE_PREEURO", "nl_BE_PREEURO", "nl_BE_PREEURO" },
        { "nl_NL_PREEURO", "nl_NL_PREEURO", "nl_NL_PREEURO" },
        { "pt_PT_PREEURO", "pt_PT_PREEURO", "pt_PT_PREEURO" },
        { "de__PHONEBOOK", "de__PHONEBOOK", "de__PHONEBOOK" },
        { "en_GB_EURO", "en_GB_EURO", "en_GB_EURO" },
        { "en_GB@EURO", "en_GB@EURO", "en_GB_EURO" }, /* POSIX ID */
        { "es__TRADITIONAL", "es__TRADITIONAL", "es__TRADITIONAL" },
        { "hi__DIRECT", "hi__DIRECT", "hi__DIRECT" },
        { "ja_JP_TRADITIONAL", "ja_JP_TRADITIONAL", "ja_JP_TRADITIONAL" },
        { "th_TH_TRADITIONAL", "th_TH_TRADITIONAL", "th_TH_TRADITIONAL" },
        { "zh_TW_STROKE", "zh_TW_STROKE", "zh_TW_STROKE" },
        { "zh__PINYIN", "zh__PINYIN", "zh__PINYIN" },
        { "zh_CN_STROKE", "zh_CN_STROKE", "zh_CN_STROKE" },
        { "sr-SP-Cyrl", "sr_SP_CYRL", "sr_SP_CYRL" }, /* .NET name */
        { "sr-SP-Latn", "sr_SP_LATN", "sr_SP_LATN" }, /* .NET name */
        { "sr_YU_CYRILLIC", "sr_YU_CYRILLIC", "sr_YU_CYRILLIC" }, /* Linux name */
        { "uz-UZ-Cyrl", "uz_UZ_CYRL", "uz_UZ_CYRL" }, /* .NET name */
        { "uz-UZ-Latn", "uz_UZ_LATN", "uz_UZ_LATN" }, /* .NET name */
        { "zh-CHS", "zh_CHS", "zh_CHS" }, /* .NET name */
        { "zh-CHT", "zh_CHT", "zh_CHT" }, /* .NET name This may change back to zh_Hant */
        /* PRE_EURO and EURO conversions don't affect other keywords */
        { "es_ES_PREEURO@CALendar=Japanese", "es_ES_PREEURO@calendar=Japanese", "es_ES_PREEURO@calendar=Japanese" },
        { "es_ES_EURO@SHOUT=zipeedeedoodah", "es_ES_EURO@shout=zipeedeedoodah", "es_ES_EURO@shout=zipeedeedoodah" },
        /* currency keyword overrides PRE_EURO and EURO currency */
        { "es_ES_PREEURO@currency=EUR", "es_ES_PREEURO@currency=EUR", "es_ES_PREEURO@currency=EUR" },
        { "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP" },
    };

    static const char* label[] = { "getName", "canonicalize" };

    UErrorCode status = U_ZERO_ERROR;
    int32_t i, j, resultLen = 0, origResultLen;
    char buffer[256];
    
    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        for (j=0; j<2; ++j) {
            const char* expected = (j==0) ? testCases[i].getNameID : testCases[i].canonicalID;
            *buffer = 0;
            status = U_ZERO_ERROR;

            if (expected == NULL) {
                expected = uloc_getDefault();
            }

            /* log_verbose("testing %s -> %s\n", testCases[i], testCases[i].canonicalID); */
            origResultLen = _canonicalize(j, testCases[i].localeID, NULL, 0, &status);
            if (status != U_BUFFER_OVERFLOW_ERROR) {
                log_err("FAIL: uloc_%s(%s) => %s, expected U_BUFFER_OVERFLOW_ERROR\n",
                        label[j], testCases[i].localeID, u_errorName(status));
                continue;
            }
            status = U_ZERO_ERROR;
            resultLen = _canonicalize(j, testCases[i].localeID, buffer, sizeof(buffer), &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: uloc_%s(%s) => %s, expected U_ZERO_ERROR\n",
                        label[j], testCases[i].localeID, u_errorName(status));
                continue;
            }
            if(uprv_strcmp(expected, buffer) != 0) {
                log_err("FAIL: uloc_%s(%s) => \"%s\", expected \"%s\"\n",
                        label[j], testCases[i].localeID, buffer, expected);
            } else {
                log_verbose("Ok: uloc_%s(%s) => \"%s\"\n",
                            label[j], testCases[i].localeID, buffer);
            }
            if (resultLen != (int32_t)strlen(buffer)) {
                log_err("FAIL: uloc_%s(%s) => len %d, expected len %d\n",
                        label[j], testCases[i].localeID, resultLen, strlen(buffer));
            }
            if (origResultLen != resultLen) {
                log_err("FAIL: uloc_%s(%s) => preflight len %d != actual len %d\n",
                        label[j], testCases[i].localeID, origResultLen, resultLen);
            }
        }
    }
}

#if APPLE_ICU_CHANGES
// rdar://
static void TestCanonicalForm(void)
{
    static const struct {
        const char *localeID;    /* input */
        const char *canonicalID; /* expected uloc_canonicalize() result */
        const char *canonicalFormID; /* expected ualoc_canonicalForm() result */
    } testCases[] = {
        { NULL,       "en_US",       "en_US" }, // NULL  maps to default locale
        { "",         "",            "" }, // rdar://114361374 revert to current open ICU behavior for mapping ""
        { "nb",       "nb",          "nb" },
        { "nn",       "nn",          "nn" },
        { "no",       "no",          "no" },
        { "no_bok",   "no_BOK",      "no_BOK" },
        { "no_nyn",   "no_NYN",      "no_NYN" },
        { "no_NO_NY", "no_NO_NY",    "no_NO_NY" },
        { "no@ny",    "no__NY",      "no__NY" },
        { "iw",       "iw",          "he" },
        { "tl",       "tl",          "fil" },
        { "prs",      "prs",         "fa_AF" },
        { "swc",      "swc",         "sw_CD" },
        { "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP" },
    };

    static const char* label[] = { "canonicalize", "canonicalForm" };

    UErrorCode status = U_ZERO_ERROR;
    int32_t i, j, resultLen = 0, origResultLen;
    char buffer[256];

    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        for (j=0; j<2; ++j) {
            const char* expected = (j==0) ? testCases[i].canonicalID : testCases[i].canonicalFormID;
            *buffer = 0;
            status = U_ZERO_ERROR;

            if (expected == NULL) {
                expected = uloc_getDefault();
            }

            origResultLen = _canonicalize(j+1, testCases[i].localeID, NULL, 0, &status);
            if (*expected != 0 && status != U_BUFFER_OVERFLOW_ERROR) { // rdar://114361374 handle zero-length expected result
                log_err("FAIL: uloc_%s(%s) => %s, expected U_BUFFER_OVERFLOW_ERROR\n",
                        label[j], testCases[i].localeID, u_errorName(status));
                continue;
            }
            status = U_ZERO_ERROR;
            resultLen = _canonicalize(j+1, testCases[i].localeID, buffer, sizeof(buffer), &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: uloc_%s(%s) => %s, expected U_ZERO_ERROR\n",
                        label[j], testCases[i].localeID, u_errorName(status));
                continue;
            }
            if(uprv_strcmp(expected, buffer) != 0) {
                log_err("FAIL: uloc_%s(%s) => \"%s\", expected \"%s\"\n",
                        label[j], testCases[i].localeID, buffer, expected);
            } else {
                log_verbose("Ok: uloc_%s(%s) => \"%s\"\n",
                            label[j], testCases[i].localeID, buffer);
            }
            if (resultLen != (int32_t)strlen(buffer)) {
                log_err("FAIL: uloc_%s(%s) => len %d, expected len %d\n",
                        label[j], testCases[i].localeID, resultLen, strlen(buffer));
            }
            if (origResultLen != resultLen) {
                log_err("FAIL: uloc_%s(%s) => preflight len %d != actual len %d\n",
                        label[j], testCases[i].localeID, origResultLen, resultLen);
            }
        }
    }
}
#endif  // APPLE_ICU_CHANGES

static void TestCanonicalizationBuffer(void)
{
    UErrorCode status = U_ZERO_ERROR;
    char buffer[256];

    // ULOC_FULLNAME_CAPACITY == 157 (uloc.h)
    static const char name[] =
        "zh@x"
        "=foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-barz"
    ;
    static const size_t len = sizeof(name) - 1;  // Without NUL terminator.

    int32_t reslen = uloc_canonicalize(name, buffer, (int32_t)len, &status);

    if (U_FAILURE(status)) {
        log_err("FAIL: uloc_canonicalize(%s) => %s, expected !U_FAILURE()\n",
                name, u_errorName(status));
        return;
    }

    if (reslen != len) {
        log_err("FAIL: uloc_canonicalize(%s) => \"%i\", expected \"%u\"\n",
                name, reslen, len);
        return;
    }

    if (uprv_strncmp(name, buffer, len) != 0) {
        log_err("FAIL: uloc_canonicalize(%s) => \"%.*s\", expected \"%s\"\n",
                name, reslen, buffer, name);
        return;
    }
}

static void TestCanonicalization21749StackUseAfterScope(void)
{
    UErrorCode status = U_ZERO_ERROR;
    char buffer[256];
    const char* input = "- _";
    uloc_canonicalize(input, buffer, -1, &status);
    if (U_SUCCESS(status)) {
        log_err("FAIL: uloc_canonicalize(%s) => %s, expected U_FAILURE()\n",
                input, u_errorName(status));
        return;
    }

    // ICU-22475 test that we don't free an internal buffer twice.
    status = U_ZERO_ERROR;
    uloc_canonicalize("ti-defaultgR-lS-z-UK-0P", buffer, UPRV_LENGTHOF(buffer), &status);
}

static void TestDisplayKeywords(void)
{
    int32_t i;

    static const struct {
        const char *localeID;
        const char *displayLocale;
        UChar displayKeyword[200];
    } testCases[] = {
        {   "ca_ES@currency=ESP",         "de_AT", 
            {0x0057, 0x00e4, 0x0068, 0x0072, 0x0075, 0x006e, 0x0067, 0x0000}, 
        },
        {   "ja_JP@calendar=japanese",         "de", 
            { 0x004b, 0x0061, 0x006c, 0x0065, 0x006e, 0x0064, 0x0065, 0x0072, 0x0000}
        },
        {   "de_DE@collation=traditional",       "de_DE", 
            {0x0053, 0x006f, 0x0072, 0x0074, 0x0069, 0x0065, 0x0072, 0x0075, 0x006e, 0x0067, 0x0000}
        },
    };
    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode status = U_ZERO_ERROR;
        const char* keyword =NULL;
        int32_t keywordLen = 0;
        int32_t keywordCount = 0;
        UChar *displayKeyword=NULL;
        int32_t displayKeywordLen = 0;
        UEnumeration* keywordEnum = uloc_openKeywords(testCases[i].localeID, &status);
        for(keywordCount = uenum_count(keywordEnum, &status); keywordCount > 0 ; keywordCount--){
              if(U_FAILURE(status)){
                  log_err("uloc_getKeywords failed for locale id: %s with error : %s \n", testCases[i].localeID, u_errorName(status)); 
                  break;
              }
              /* the uenum_next returns NUL terminated string */
              keyword = uenum_next(keywordEnum, &keywordLen, &status);
              /* fetch the displayKeyword */
              displayKeywordLen = uloc_getDisplayKeyword(keyword, testCases[i].displayLocale, displayKeyword, displayKeywordLen, &status);
              if(status==U_BUFFER_OVERFLOW_ERROR){
                  status = U_ZERO_ERROR;
                  displayKeywordLen++; /* for null termination */
                  displayKeyword = (UChar*) malloc(displayKeywordLen * U_SIZEOF_UCHAR);
                  displayKeywordLen = uloc_getDisplayKeyword(keyword, testCases[i].displayLocale, displayKeyword, displayKeywordLen, &status);
                  if(U_FAILURE(status)){
                      log_err("uloc_getDisplayKeyword filed for keyword : %s in locale id: %s for display locale: %s \n", testCases[i].localeID, keyword, testCases[i].displayLocale, u_errorName(status)); 
                      free(displayKeyword);
                      break; 
                  }
                  if(u_strncmp(displayKeyword, testCases[i].displayKeyword, displayKeywordLen)!=0){
                      if (status == U_USING_DEFAULT_WARNING) {
                          log_data_err("uloc_getDisplayKeyword did not get the expected value for keyword : %s in locale id: %s for display locale: %s . Got error: %s. Perhaps you are missing data?\n", testCases[i].localeID, keyword, testCases[i].displayLocale, u_errorName(status));
                      } else {
                          log_err("uloc_getDisplayKeyword did not get the expected value for keyword : %s in locale id: %s for display locale: %s \n", testCases[i].localeID, keyword, testCases[i].displayLocale);
                      }
                      free(displayKeyword);
                      break; 
                  }
              }else{
                  log_err("uloc_getDisplayKeyword did not return the expected error. Error: %s\n", u_errorName(status));
              }
              
              free(displayKeyword);

        }
        uenum_close(keywordEnum);
    }
}

static void TestDisplayKeywordValues(void){
    int32_t i;

    static const struct {
        const char *localeID;
        const char *displayLocale;
        
        #if APPLE_ICU_CHANGES
            // rdar://107558312
        	const UChar displayKeywordValue[500];
        #else
        	UChar displayKeywordValue[500];
        #endif  // APPLE_ICU_CHANGES
        
    } testCases[] = {
        {   "ca_ES@currency=ESP",         "de_AT", 
            {0x0053, 0x0070, 0x0061, 0x006e, 0x0069, 0x0073, 0x0063, 0x0068, 0x0065, 0x0020, 0x0050, 0x0065, 0x0073, 0x0065, 0x0074, 0x0061, 0x0000}
        },
        {   "de_AT@currency=ATS",         "fr_FR", 
            {0x0073, 0x0063, 0x0068, 0x0069, 0x006c, 0x006c, 0x0069, 0x006e, 0x0067, 0x0020, 0x0061, 0x0075, 0x0074, 0x0072, 0x0069, 0x0063, 0x0068, 0x0069, 0x0065, 0x006e, 0x0000}
        },
        {   "de_DE@currency=DEM",         "it", 
            {0x006d, 0x0061, 0x0072, 0x0063, 0x006f, 0x0020, 0x0074, 0x0065, 0x0064, 0x0065, 0x0073, 0x0063, 0x006f, 0x0000}
        },
        {   "el_GR@currency=GRD",         "en",    
            {0x0047, 0x0072, 0x0065, 0x0065, 0x006b, 0x0020, 0x0044, 0x0072, 0x0061, 0x0063, 0x0068, 0x006d, 0x0061, 0x0000}
        },
        {   "eu_ES@currency=ESP",         "it_IT", 
            {0x0070, 0x0065, 0x0073, 0x0065, 0x0074, 0x0061, 0x0020, 0x0073, 0x0070, 0x0061, 0x0067, 0x006e, 0x006f, 0x006c, 0x0061, 0x0000}
        },
        {   "de@collation=phonebook",     "es",    
            {0x006F, 0x0072, 0x0064, 0x0065, 0x006E, 0x0020, 0x0064, 0x0065, 0x0020, 0x006C, 0x0069, 0x0073, 0x0074, 0x00ED, 0x006E, 0x0020, 0x0074, 0x0065, 0x006C, 0x0065, 0x0066, 0x00F3, 0x006E, 0x0069, 0x0063, 0x006F, 0x0000}
        },
        { "de_DE@collation=phonebook",  "es", 
          {0x006F, 0x0072, 0x0064, 0x0065, 0x006E, 0x0020, 0x0064, 0x0065, 0x0020, 0x006C, 0x0069, 0x0073, 0x0074, 0x00ED, 0x006E, 0x0020, 0x0074, 0x0065, 0x006C, 0x0065, 0x0066, 0x00F3, 0x006E, 0x0069, 0x0063, 0x006F, 0x0000}
        },
        { "es_ES@collation=traditional","de", 
          {0x0054, 0x0072, 0x0061, 0x0064, 0x0069, 0x0074, 0x0069, 0x006f, 0x006e, 0x0065, 0x006c, 0x006c, 0x0065, 0x0020, 0x0053, 0x006f, 0x0072, 0x0074, 0x0069, 0x0065, 0x0072, 0x0075, 0x006E, 0x0067, 0x0000}
        },
        { "ja_JP@calendar=japanese",    "de", 
           {0x004a, 0x0061, 0x0070, 0x0061, 0x006e, 0x0069, 0x0073, 0x0063, 0x0068, 0x0065, 0x0072, 0x0020, 0x004b, 0x0061, 0x006c, 0x0065, 0x006e, 0x0064, 0x0065, 0x0072, 0x0000}
        },
        #if APPLE_ICU_CHANGES
		// rdar://107558312
        { "@calendar=islamic",	"en", u"Hijri Calendar" },
        { "@calendar=islamic",	"de", u"Hidschra-Kalender" },
        { "@calendar=islamic",	"ar", u"التقويم الهجري" },
        { "@calendar=islamic",	"ru", u"календарь Хиджра" },        
        #endif  // APPLE_ICU_CHANGES
    };
    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode status = U_ZERO_ERROR;
        const char* keyword =NULL;
        int32_t keywordLen = 0;
        int32_t keywordCount = 0;
        UChar *displayKeywordValue = NULL;
        int32_t displayKeywordValueLen = 0;
        UEnumeration* keywordEnum = uloc_openKeywords(testCases[i].localeID, &status);
        for(keywordCount = uenum_count(keywordEnum, &status); keywordCount > 0 ; keywordCount--){
              if(U_FAILURE(status)){
                  log_err("uloc_getKeywords failed for locale id: %s in display locale: % with error : %s \n", testCases[i].localeID, testCases[i].displayLocale, u_errorName(status)); 
                  break;
              }
              /* the uenum_next returns NUL terminated string */
              keyword = uenum_next(keywordEnum, &keywordLen, &status);
              
              /* fetch the displayKeywordValue */
              displayKeywordValueLen = uloc_getDisplayKeywordValue(testCases[i].localeID, keyword, testCases[i].displayLocale, displayKeywordValue, displayKeywordValueLen, &status);
              if(status==U_BUFFER_OVERFLOW_ERROR){
                  status = U_ZERO_ERROR;
                  displayKeywordValueLen++; /* for null termination */
                  displayKeywordValue = (UChar*)malloc(displayKeywordValueLen * U_SIZEOF_UCHAR);
                  displayKeywordValueLen = uloc_getDisplayKeywordValue(testCases[i].localeID, keyword, testCases[i].displayLocale, displayKeywordValue, displayKeywordValueLen, &status);
                  if(U_FAILURE(status)){
                      log_err("uloc_getDisplayKeywordValue failed for keyword : %s in locale id: %s for display locale: %s with error : %s \n", testCases[i].localeID, keyword, testCases[i].displayLocale, u_errorName(status)); 
                      free(displayKeywordValue);
                      break; 
                  }
                  if(u_strncmp(displayKeywordValue, testCases[i].displayKeywordValue, displayKeywordValueLen)!=0){
                      if (status == U_USING_DEFAULT_WARNING) {
                          log_data_err("uloc_getDisplayKeywordValue did not return the expected value keyword : %s in locale id: %s for display locale: %s with error : %s Perhaps you are missing data\n", testCases[i].localeID, keyword, testCases[i].displayLocale, u_errorName(status)); 
                      } else {
                          log_err("uloc_getDisplayKeywordValue did not return the expected value keyword : %s in locale id: %s for display locale: %s with error : %s \n", testCases[i].localeID, keyword, testCases[i].displayLocale, u_errorName(status)); 
                      }
                      free(displayKeywordValue);
                      break;   
                  }
              }else{
                  log_err("uloc_getDisplayKeywordValue did not return the expected error. Error: %s\n", u_errorName(status));
              }
              free(displayKeywordValue);
        }
        uenum_close(keywordEnum);
    }
    {   
        /* test a multiple keywords */
        UErrorCode status = U_ZERO_ERROR;
        const char* keyword =NULL;
        int32_t keywordLen = 0;
        int32_t keywordCount = 0;
        const char* localeID = "es@collation=phonebook;calendar=buddhist;currency=DEM";
        const char* displayLocale = "de";
        static const UChar expected[][50] = {
            {0x0042, 0x0075, 0x0064, 0x0064, 0x0068, 0x0069, 0x0073, 0x0074, 0x0069, 0x0073, 0x0063, 0x0068, 0x0065, 0x0072, 0x0020, 0x004b, 0x0061, 0x006c, 0x0065, 0x006e, 0x0064, 0x0065, 0x0072, 0x0000},

            {0x0054, 0x0065, 0x006c, 0x0065, 0x0066, 0x006f, 0x006e, 0x0062, 0x0075, 0x0063, 0x0068, 0x002d, 0x0053, 0x006f, 0x0072, 0x0074, 0x0069, 0x0065, 0x0072, 0x0075, 0x006e, 0x0067, 0x0000},
            {0x0044, 0x0065, 0x0075, 0x0074, 0x0073, 0x0063, 0x0068, 0x0065, 0x0020, 0x004d, 0x0061, 0x0072, 0x006b, 0x0000},
        };

        UEnumeration* keywordEnum = uloc_openKeywords(localeID, &status);

        for(keywordCount = 0; keywordCount < uenum_count(keywordEnum, &status) ; keywordCount++){
              UChar *displayKeywordValue = NULL;
              int32_t displayKeywordValueLen = 0;
              if(U_FAILURE(status)){
                  log_err("uloc_getKeywords failed for locale id: %s in display locale: % with error : %s \n", localeID, displayLocale, u_errorName(status)); 
                  break;
              }
              /* the uenum_next returns NUL terminated string */
              keyword = uenum_next(keywordEnum, &keywordLen, &status);
              
              /* fetch the displayKeywordValue */
              displayKeywordValueLen = uloc_getDisplayKeywordValue(localeID, keyword, displayLocale, displayKeywordValue, displayKeywordValueLen, &status);
              if(status==U_BUFFER_OVERFLOW_ERROR){
                  status = U_ZERO_ERROR;
                  displayKeywordValueLen++; /* for null termination */
                  displayKeywordValue = (UChar*)malloc(displayKeywordValueLen * U_SIZEOF_UCHAR);
                  displayKeywordValueLen = uloc_getDisplayKeywordValue(localeID, keyword, displayLocale, displayKeywordValue, displayKeywordValueLen, &status);
                  if(U_FAILURE(status)){
                      log_err("uloc_getDisplayKeywordValue failed for keyword : %s in locale id: %s for display locale: %s with error : %s \n", localeID, keyword, displayLocale, u_errorName(status)); 
                      free(displayKeywordValue);
                      break; 
                  }
                  if(u_strncmp(displayKeywordValue, expected[keywordCount], displayKeywordValueLen)!=0){
                      if (status == U_USING_DEFAULT_WARNING) {
                          log_data_err("uloc_getDisplayKeywordValue did not return the expected value keyword : %s in locale id: %s for display locale: %s  got error: %s. Perhaps you are missing data?\n", localeID, keyword, displayLocale, u_errorName(status));
                      } else {
                          log_err("uloc_getDisplayKeywordValue did not return the expected value keyword : %s in locale id: %s for display locale: %s \n", localeID, keyword, displayLocale);
                      }
                      free(displayKeywordValue);
                      break;   
                  }
              }else{
                  log_err("uloc_getDisplayKeywordValue did not return the expected error. Error: %s\n", u_errorName(status));
              }
              free(displayKeywordValue);
        }
        uenum_close(keywordEnum);
    
    }
    {
        /* Test non existent keywords */
        UErrorCode status = U_ZERO_ERROR;
        const char* localeID = "es";
        const char* displayLocale = "de";
        UChar *displayKeywordValue = NULL;
        int32_t displayKeywordValueLen = 0;
        
        /* fetch the displayKeywordValue */
        displayKeywordValueLen = uloc_getDisplayKeywordValue(localeID, "calendar", displayLocale, displayKeywordValue, displayKeywordValueLen, &status);
        if(U_FAILURE(status)) {
          log_err("uloc_getDisplaykeywordValue returned error status %s\n", u_errorName(status));
        } else if(displayKeywordValueLen != 0) {
          log_err("uloc_getDisplaykeywordValue returned %d should be 0 \n", displayKeywordValueLen);
        }
    }
}


static void TestGetBaseName(void) {
    static const struct {
        const char *localeID;
        const char *baseName;
    } testCases[] = {
        { "de_DE@  C o ll A t i o n   = Phonebook   ", "de_DE" },
        { "de@currency = euro; CoLLaTion   = PHONEBOOk", "de" },
        { "ja@calendar = buddhist", "ja" }
    };

    int32_t i = 0, baseNameLen = 0;
    char baseName[256];
    UErrorCode status = U_ZERO_ERROR;

    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        baseNameLen = uloc_getBaseName(testCases[i].localeID, baseName, 256, &status);
        (void)baseNameLen;    /* Suppress set but not used warning. */
        if(strcmp(testCases[i].baseName, baseName)) {
            log_err("For locale \"%s\" expected baseName \"%s\", but got \"%s\"\n",
                testCases[i].localeID, testCases[i].baseName, baseName);
            return;
        }
    }
}

static void TestTrailingNull(void) {
  const char* localeId = "zh_Hans";
  UChar buffer[128]; /* sufficient for this test */
  int32_t len;
  UErrorCode status = U_ZERO_ERROR;
  int i;

  len = uloc_getDisplayName(localeId, localeId, buffer, 128, &status);
  if (len > 128) {
    log_err("buffer too small");
    return;
  }

  for (i = 0; i < len; ++i) {
    if (buffer[i] == 0) {
      log_err("name contained null");
      return;
    }
  }
}

/* Jitterbug 4115 */
static void TestDisplayNameWarning(void) {
    UChar name[256];
    int32_t size;
    UErrorCode status = U_ZERO_ERROR;
    
    size = uloc_getDisplayLanguage("qqq", "kl", name, UPRV_LENGTHOF(name), &status);
    (void)size;    /* Suppress set but not used warning. */
    if (status != U_USING_DEFAULT_WARNING) {
        log_err("For language \"qqq\" in locale \"kl\", expecting U_USING_DEFAULT_WARNING, but got %s\n",
            u_errorName(status));
    }
}


/**
 * Compare two locale IDs.  If they are equal, return 0.  If `string'
 * starts with `prefix' plus an additional element, that is, string ==
 * prefix + '_' + x, then return 1.  Otherwise return a value < 0.
 */
static UBool _loccmp(const char* string, const char* prefix) {
    int32_t slen = (int32_t)uprv_strlen(string),
            plen = (int32_t)uprv_strlen(prefix);
    int32_t c = uprv_strncmp(string, prefix, plen);
    /* 'root' is less than everything */
    if (uprv_strcmp(prefix, "root") == 0) {
        return (uprv_strcmp(string, "root") == 0) ? 0 : 1;
    }
    if (c) return -1; /* mismatch */
    if (slen == plen) return 0;
    if (string[plen] == '_') return 1;
    return -2; /* false match, e.g. "en_USX" cmp "en_US" */
}

static void _checklocs(const char* label,
                       const char* req,
                       const char* valid,
                       const char* actual) {
    /* We want the valid to be strictly > the bogus requested locale,
       and the valid to be >= the actual. */
    if (_loccmp(req, valid) > 0 &&
        _loccmp(valid, actual) >= 0) {
        log_verbose("%s; req=%s, valid=%s, actual=%s\n",
                    label, req, valid, actual);
    } else {
        log_err("FAIL: %s; req=%s, valid=%s, actual=%s\n",
                label, req, valid, actual);
    }
}

static void TestGetLocale(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UParseError pe;
    UChar EMPTY[1] = {0};

    /* === udat === */
#if !UCONFIG_NO_FORMATTING
    {
        UDateFormat *obj;
        const char *req = "en_US_REDWOODSHORES", *valid, *actual;
        obj = udat_open(UDAT_DEFAULT, UDAT_DEFAULT,
                        req,
                        NULL, 0,
                        NULL, 0, &ec);
        if (U_FAILURE(ec)) {
            log_data_err("udat_open failed.Error %s\n", u_errorName(ec));
            return;
        }
        valid = udat_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = udat_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("udat_getLocaleByType() failed\n");
            return;
        }
        _checklocs("udat", req, valid, actual);
        udat_close(obj);
    }
#endif

    /* === ucal === */
#if !UCONFIG_NO_FORMATTING
    {
        UCalendar *obj;
        const char *req = "fr_FR_PROVENCAL", *valid, *actual;
        obj = ucal_open(NULL, 0,
                        req,
                        UCAL_GREGORIAN,
                        &ec);
        if (U_FAILURE(ec)) {
            log_err("ucal_open failed with error: %s\n", u_errorName(ec));
            return;
        }
        valid = ucal_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = ucal_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("ucal_getLocaleByType() failed\n");
            return;
        }
        _checklocs("ucal", req, valid, actual);
        ucal_close(obj);
    }
#endif

    /* === unum === */
#if !UCONFIG_NO_FORMATTING
    {
        UNumberFormat *obj;
        const char *req = "zh_Hant_TW_TAINAN", *valid, *actual;
        obj = unum_open(UNUM_DECIMAL,
                        NULL, 0,
                        req,
                        &pe, &ec);
        if (U_FAILURE(ec)) {
            log_err("unum_open failed\n");
            return;
        }
        valid = unum_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = unum_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("unum_getLocaleByType() failed\n");
            return;
        }
        _checklocs("unum", req, valid, actual);
        unum_close(obj);
    }
#endif

    /* === umsg === */
#if 0
    /* commented out by weiv 01/12/2005. umsg_getLocaleByType is to be removed */
#if !UCONFIG_NO_FORMATTING
    {
        UMessageFormat *obj;
        const char *req = "ja_JP_TAKAYAMA", *valid, *actual;
        UBool test;
        obj = umsg_open(EMPTY, 0,
                        req,
                        &pe, &ec);
        if (U_FAILURE(ec)) {
            log_err("umsg_open failed\n");
            return;
        }
        valid = umsg_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = umsg_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("umsg_getLocaleByType() failed\n");
            return;
        }
        /* We want the valid to be strictly > the bogus requested locale,
           and the valid to be >= the actual. */
        /* TODO MessageFormat is currently just storing the locale it is given.
           As a result, it will return whatever it was given, even if the
           locale is invalid. */
        test = (_cmpversion("3.2") <= 0) ?
            /* Here is the weakened test for 3.0: */
            (_loccmp(req, valid) >= 0) :
            /* Here is what the test line SHOULD be: */
            (_loccmp(req, valid) > 0);

        if (test &&
            _loccmp(valid, actual) >= 0) {
            log_verbose("umsg; req=%s, valid=%s, actual=%s\n", req, valid, actual);
        } else {
            log_err("FAIL: umsg; req=%s, valid=%s, actual=%s\n", req, valid, actual);
        }
        umsg_close(obj);
    }
#endif
#endif

    /* === ubrk === */
#if !UCONFIG_NO_BREAK_ITERATION
    {
        UBreakIterator *obj;
        const char *req = "ar_KW_ABDALI", *valid, *actual;
        obj = ubrk_open(UBRK_WORD,
                        req,
                        EMPTY,
                        0,
                        &ec);
        if (U_FAILURE(ec)) {
            log_err("ubrk_open failed. Error: %s \n", u_errorName(ec));
            return;
        }
        valid = ubrk_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = ubrk_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("ubrk_getLocaleByType() failed\n");
            return;
        }
        _checklocs("ubrk", req, valid, actual);
        ubrk_close(obj);
    }
#endif

    /* === ucol === */
#if !UCONFIG_NO_COLLATION
    {
        UCollator *obj;
        const char *req = "es_AR_BUENOSAIRES", *valid, *actual;
        obj = ucol_open(req, &ec);
        if (U_FAILURE(ec)) {
            log_err("ucol_open failed - %s\n", u_errorName(ec));
            return;
        }
        valid = ucol_getLocaleByType(obj, ULOC_VALID_LOCALE, &ec);
        actual = ucol_getLocaleByType(obj, ULOC_ACTUAL_LOCALE, &ec);
        if (U_FAILURE(ec)) {
            log_err("ucol_getLocaleByType() failed\n");
            return;
        }
        _checklocs("ucol", req, valid, actual);
        ucol_close(obj);
    }
#endif
}
static void TestEnglishExemplarCharacters(void) {
    UErrorCode status = U_ZERO_ERROR;
    int i;
    USet *exSet = NULL;
    UChar testChars[] = {
        0x61,   /* standard */
        0xE1,   /* auxiliary */
        0x41,   /* index */
        0x2D    /* punctuation */
    };
    ULocaleData *uld = ulocdata_open("en", &status);
    if (U_FAILURE(status)) {
        log_data_err("ulocdata_open() failed : %s - (Are you missing data?)\n", u_errorName(status));
        return;
    }

    for (i = 0; i < ULOCDATA_ES_COUNT; i++) {
        exSet = ulocdata_getExemplarSet(uld, exSet, 0, (ULocaleDataExemplarSetType)i, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "ulocdata_getExemplarSet() for type %d failed\n", i);
            status = U_ZERO_ERROR;
            continue;
        }
        if (!uset_contains(exSet, (UChar32)testChars[i])) {
            log_err("Character U+%04X is not included in exemplar type %d\n", testChars[i], i);
        }
    }

    uset_close(exSet);
    ulocdata_close(uld);
}

static void TestNonexistentLanguageExemplars(void) {
    /* JB 4068 - Nonexistent language */
    UErrorCode ec = U_ZERO_ERROR;
    ULocaleData *uld = ulocdata_open("qqq",&ec);
    if (ec != U_USING_DEFAULT_WARNING) {
        log_err_status(ec, "Exemplar set for \"qqq\", expecting U_USING_DEFAULT_WARNING, but got %s\n",
            u_errorName(ec));
    }
    uset_close(ulocdata_getExemplarSet(uld, NULL, 0, ULOCDATA_ES_STANDARD, &ec));
    ulocdata_close(uld);
}

static void TestLocDataErrorCodeChaining(void) {
    UErrorCode ec = U_USELESS_COLLATOR_ERROR;
    ulocdata_open(NULL, &ec);
    ulocdata_getExemplarSet(NULL, NULL, 0, ULOCDATA_ES_STANDARD, &ec);
    ulocdata_getDelimiter(NULL, ULOCDATA_DELIMITER_COUNT, NULL, -1, &ec);
    ulocdata_getMeasurementSystem(NULL, &ec);
    ulocdata_getPaperSize(NULL, NULL, NULL, &ec);
    if (ec != U_USELESS_COLLATOR_ERROR) {
        log_err("ulocdata API changed the error code to %s\n", u_errorName(ec));
    }
}

typedef struct {
    const char*        locale;
    UMeasurementSystem measureSys;
} LocToMeasureSys;

static const LocToMeasureSys locToMeasures[] = {
    { "fr_FR",            UMS_SI },
    { "en",               UMS_US },
    { "en_GB",            UMS_UK },
    { "fr_FR@rg=GBZZZZ",  UMS_UK },
    { "en@rg=frzzzz",     UMS_SI },
    { "en_GB@rg=USZZZZ",  UMS_US },
    { NULL, (UMeasurementSystem)0 } /* terminator */
};

static void TestLocDataWithRgTag(void) {
    const  LocToMeasureSys* locToMeasurePtr = locToMeasures;
    for (; locToMeasurePtr->locale != NULL; locToMeasurePtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UMeasurementSystem measureSys = ulocdata_getMeasurementSystem(locToMeasurePtr->locale, &status);
        if (U_FAILURE(status)) {
            log_data_err("ulocdata_getMeasurementSystem(\"%s\", ...) failed: %s - Are you missing data?\n",
                        locToMeasurePtr->locale, u_errorName(status));
        } else if (measureSys != locToMeasurePtr->measureSys) {
            log_err("ulocdata_getMeasurementSystem(\"%s\", ...), expected %d, got %d\n",
                        locToMeasurePtr->locale, (int) locToMeasurePtr->measureSys, (int)measureSys);
        }
    }
}

static void TestLanguageExemplarsFallbacks(void) {
    /* Test that en_US fallsback, but en doesn't fallback. */
    UErrorCode ec = U_ZERO_ERROR;
    ULocaleData *uld = ulocdata_open("en_US",&ec);
    uset_close(ulocdata_getExemplarSet(uld, NULL, 0, ULOCDATA_ES_STANDARD, &ec));
    if (ec != U_USING_FALLBACK_WARNING) {
        log_err_status(ec, "Exemplar set for \"en_US\", expecting U_USING_FALLBACK_WARNING, but got %s\n",
            u_errorName(ec));
    }
    ulocdata_close(uld);
    ec = U_ZERO_ERROR;
    uld = ulocdata_open("en",&ec);
    uset_close(ulocdata_getExemplarSet(uld, NULL, 0, ULOCDATA_ES_STANDARD, &ec));
    if (ec != U_ZERO_ERROR) {
        log_err_status(ec, "Exemplar set for \"en\", expecting U_ZERO_ERROR, but got %s\n",
            u_errorName(ec));
    }
    ulocdata_close(uld);
}

static const char *acceptResult(UAcceptResult uar) {
    return  udbg_enumName(UDBG_UAcceptResult, uar);
}

static void TestAcceptLanguage(void) {
    UErrorCode status = U_ZERO_ERROR;
    UAcceptResult outResult;
    UEnumeration *available;
    char tmp[200];
    int i;
    int32_t rc = 0;

    struct { 
        int32_t httpSet;       /**< Which of http[] should be used? */
        const char *icuSet;    /**< ? */
        const char *expect;    /**< The expected locale result */
        UAcceptResult res;     /**< The expected error code */
        UErrorCode expectStatus; /**< expected status */
    } tests[] = { 
        /*0*/{ 0, NULL, "mt_MT", ULOC_ACCEPT_VALID, U_ZERO_ERROR},
        /*1*/{ 1, NULL, "en", ULOC_ACCEPT_VALID, U_ZERO_ERROR},
        /*2*/{ 2, NULL, "en_GB", ULOC_ACCEPT_FALLBACK, U_ZERO_ERROR},
        /*3*/{ 3, NULL, "", ULOC_ACCEPT_FAILED, U_ZERO_ERROR},
        /*4*/{ 4, NULL, "es", ULOC_ACCEPT_VALID, U_ZERO_ERROR},
        /*5*/{ 5, NULL, "zh", ULOC_ACCEPT_FALLBACK, U_ZERO_ERROR},  /* XF */
        /*6*/{ 6, NULL, "ja", ULOC_ACCEPT_FALLBACK, U_ZERO_ERROR},  /* XF */
        /*7*/{ 7, NULL, "zh", ULOC_ACCEPT_FALLBACK, U_ZERO_ERROR},  /* XF */
        /*8*/{ 8, NULL, "", ULOC_ACCEPT_FAILED, U_ILLEGAL_ARGUMENT_ERROR },  /*  */
        /*9*/{ 9, NULL, "", ULOC_ACCEPT_FAILED, U_ILLEGAL_ARGUMENT_ERROR },  /*  */
       /*10*/{10, NULL, "", ULOC_ACCEPT_FAILED, U_ILLEGAL_ARGUMENT_ERROR },  /*  */
       /*11*/{11, NULL, "", ULOC_ACCEPT_FAILED, U_ILLEGAL_ARGUMENT_ERROR },  /*  */
    };
    const int32_t numTests = UPRV_LENGTHOF(tests);
    static const char *http[] = {
        /*0*/ "mt-mt, ja;q=0.76, en-us;q=0.95, en;q=0.92, en-gb;q=0.89, fr;q=0.87, "
              "iu-ca;q=0.84, iu;q=0.82, ja-jp;q=0.79, mt;q=0.97, de-de;q=0.74, de;q=0.71, "
              "es;q=0.68, it-it;q=0.66, it;q=0.63, vi-vn;q=0.61, vi;q=0.58, "
              "nl-nl;q=0.55, nl;q=0.53, th-th-traditional;q=0.01",
        /*1*/ "ja;q=0.5, en;q=0.8, tlh",
        /*2*/ "en-wf, de-lx;q=0.8",
        /*3*/ "mga-ie;q=0.9, sux",
        /*4*/ "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, "
              "xxx-yyy;q=0.01, xxx-yyy;q=0.01, xxx-yyy;q=0.01, xx-yy;q=0.1, "
              "es",
        /*5*/ "zh-xx;q=0.9, en;q=0.6",
        /*6*/ "ja-JA",
        /*7*/ "zh-xx;q=0.9",
       /*08*/ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", // 156
       /*09*/ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB", // 157 (this hits U_STRING_NOT_TERMINATED_WARNING )
       /*10*/ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABC", // 158
       /*11*/ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
              "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", // 163 bytes
    };

    for(i=0;i<numTests;i++) {
        outResult = -3;
        status=U_ZERO_ERROR;
        log_verbose("test #%d: http[%s], ICU[%s], expect %s, %s\n", 
            i, http[tests[i].httpSet], tests[i].icuSet, tests[i].expect, acceptResult(tests[i].res));

        available = ures_openAvailableLocales(tests[i].icuSet, &status);
        tmp[0]=0;
        rc = uloc_acceptLanguageFromHTTP(tmp, 199, &outResult,
                                         http[tests[i].httpSet], available, &status);
        (void)rc;    /* Suppress set but not used warning. */
        uenum_close(available);
        log_verbose(" got %s, %s [%s]\n",
                    tmp[0]?tmp:"(EMPTY)", acceptResult(outResult), u_errorName(status));
        if(status != tests[i].expectStatus) {
          log_err_status(status,
                         "FAIL: expected status %s but got %s\n",
                         u_errorName(tests[i].expectStatus),
                         u_errorName(status));
        } else if(U_SUCCESS(tests[i].expectStatus)) {
            /* don't check content if expected failure */
            if(outResult != tests[i].res) {
            log_err_status(status, "FAIL: #%d: expected outResult of %s but got %s\n", i, 
                acceptResult( tests[i].res), 
                acceptResult( outResult));
            log_info("test #%d: http[%s], ICU[%s], expect %s, %s\n", 
                     i, http[tests[i].httpSet], tests[i].icuSet,
                     tests[i].expect,acceptResult(tests[i].res));
            }
            if((outResult>0)&&uprv_strcmp(tmp, tests[i].expect)) {
              log_err_status(status,
                             "FAIL: #%d: expected %s but got %s\n",
                             i, tests[i].expect, tmp);
              log_info("test #%d: http[%s], ICU[%s], expect %s, %s\n", 
                       i, http[tests[i].httpSet], tests[i].icuSet, tests[i].expect, acceptResult(tests[i].res));
            }
        }
    }

    // API coverage
    status = U_ZERO_ERROR;
    static const char *const supported[] = { "en-US", "en-GB", "de-DE", "ja-JP" };
    const char * desired[] = { "de-LI", "en-IN", "zu", "fr" };
    available = uenum_openCharStringsEnumeration(supported, UPRV_LENGTHOF(supported), &status);
    tmp[0]=0;
    rc = uloc_acceptLanguage(tmp, 199, &outResult, desired, UPRV_LENGTHOF(desired), available, &status);
    if (U_FAILURE(status) || rc != 5 || uprv_strcmp(tmp, "de_DE") != 0 || outResult == ULOC_ACCEPT_FAILED) {
        log_err("uloc_acceptLanguage() failed to do a simple match\n");
    }
    uenum_close(available);
}

static const char* LOCALE_ALIAS[][2] = {
    {"in", "id"},
    {"in_ID", "id_ID"},
    {"iw", "he"},
    {"iw_IL", "he_IL"},
    {"ji", "yi"},
    {"en_BU", "en_MM"},
    {"en_DY", "en_BJ"},
    {"en_HV", "en_BF"},
    {"en_NH", "en_VU"},
    {"en_RH", "en_ZW"},
    {"en_TP", "en_TL"},
    {"en_ZR", "en_CD"}
};
static UBool isLocaleAvailable(UResourceBundle* resIndex, const char* loc){
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = 0;
    ures_getStringByKey(resIndex, loc,&len, &status);
    if(U_FAILURE(status)){
        return false; 
    }
    return true;
}

static void TestCalendar() {
#if !UCONFIG_NO_FORMATTING
    int i;
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *resIndex = ures_open(NULL,"res_index", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "Could not open res_index.res. Exiting. Error: %s\n", u_errorName(status));
        return;
    }
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {
        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        UCalendar* c1 = NULL;
        UCalendar* c2 = NULL;

        /*Test function "getLocale(ULocale.VALID_LOCALE)"*/
        const char* l1 = ucal_getLocaleByType(c1, ULOC_VALID_LOCALE, &status);
        const char* l2 = ucal_getLocaleByType(c2, ULOC_VALID_LOCALE, &status);

        if(!isLocaleAvailable(resIndex, newLoc)){
            continue;
        }
        c1 = ucal_open(NULL, -1, oldLoc, UCAL_GREGORIAN, &status);
        c2 = ucal_open(NULL, -1, newLoc, UCAL_GREGORIAN, &status);

        if (strcmp(newLoc,l1)!=0 || strcmp(l1,l2)!=0 || status!=U_ZERO_ERROR) {
            log_err("The locales are not equal!.Old: %s, New: %s \n", oldLoc, newLoc);
        }
        log_verbose("ucal_getLocaleByType old:%s   new:%s\n", l1, l2);
        ucal_close(c1);
        ucal_close(c2);
    }
    ures_close(resIndex);
#endif
}

static void TestDateFormat() {
#if !UCONFIG_NO_FORMATTING
    int i;
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *resIndex = ures_open(NULL,"res_index", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "Could not open res_index.res. Exiting. Error: %s\n", u_errorName(status));
        return;
    }
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {
        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        UDateFormat* df1 = NULL;
        UDateFormat* df2 = NULL;
        const char* l1 = NULL;
        const char* l2 = NULL;

        if(!isLocaleAvailable(resIndex, newLoc)){
            continue;
        }
        df1 = udat_open(UDAT_FULL, UDAT_FULL,oldLoc, NULL, 0, NULL, -1, &status);
        df2 = udat_open(UDAT_FULL, UDAT_FULL,newLoc, NULL, 0, NULL, -1, &status);
        if(U_FAILURE(status)){
            log_err("Creation of date format failed  %s\n", u_errorName(status));
            return;
        }        
        /*Test function "getLocale"*/
        l1 = udat_getLocaleByType(df1, ULOC_VALID_LOCALE, &status);
        l2 = udat_getLocaleByType(df2, ULOC_VALID_LOCALE, &status);
        if(U_FAILURE(status)){
            log_err("Fetching the locale by type failed.  %s\n", u_errorName(status));
        }
        if (strcmp(newLoc,l1)!=0 || strcmp(l1,l2)!=0) {
            log_err("The locales are not equal!.Old: %s, New: %s \n", oldLoc, newLoc);
        }
        log_verbose("udat_getLocaleByType old:%s   new:%s\n", l1, l2);
        udat_close(df1);
        udat_close(df2);
    }
    ures_close(resIndex);
#endif
}

static void TestCollation() {
#if !UCONFIG_NO_COLLATION
    int i;
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *resIndex = ures_open(NULL,"res_index", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "Could not open res_index.res. Exiting. Error: %s\n", u_errorName(status));
        return;
    }
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {
        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        UCollator* c1 = NULL;
        UCollator* c2 = NULL;
        const char* l1 = NULL;
        const char* l2 = NULL;

        status = U_ZERO_ERROR;
        if(!isLocaleAvailable(resIndex, newLoc)){
            continue;
        }
        if(U_FAILURE(status)){
            log_err("Creation of collators failed  %s\n", u_errorName(status));
            return;
        }
        c1 = ucol_open(oldLoc, &status);
        c2 = ucol_open(newLoc, &status);
        l1 = ucol_getLocaleByType(c1, ULOC_VALID_LOCALE, &status);
        l2 = ucol_getLocaleByType(c2, ULOC_VALID_LOCALE, &status);
        if(U_FAILURE(status)){
            log_err("Fetching the locale names failed failed  %s\n", u_errorName(status));
        }        
        if (strcmp(newLoc,l1)!=0 || strcmp(l1,l2)!=0) {
            log_err("The locales are not equal!.Old: %s, New: %s \n", oldLoc, newLoc);
        }
        log_verbose("ucol_getLocaleByType old:%s   new:%s\n", l1, l2);
        ucol_close(c1);
        ucol_close(c2);
    }
    ures_close(resIndex);
#endif
}

typedef struct OrientationStructTag {
    const char* localeId;
    ULayoutType character;
    ULayoutType line;
} OrientationStruct;

static const char* ULayoutTypeToString(ULayoutType type)
{
    switch(type)
    {
    case ULOC_LAYOUT_LTR:
        return "ULOC_LAYOUT_LTR";
        break;
    case ULOC_LAYOUT_RTL:
        return "ULOC_LAYOUT_RTL";
        break;
    case ULOC_LAYOUT_TTB:
        return "ULOC_LAYOUT_TTB";
        break;
    case ULOC_LAYOUT_BTT:
        return "ULOC_LAYOUT_BTT";
        break;
    case ULOC_LAYOUT_UNKNOWN:
        break;
    }

    return "Unknown enum value for ULayoutType!";
}

static void  TestOrientation()
{
    static const OrientationStruct toTest [] = {
        { "ar", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "aR", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ar_Arab", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "fa", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "Fa", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "he", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ps", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ur", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "UR", ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "en", ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB }
#if APPLE_ICU_CHANGES
// rdar://
        ,
        // Additional Apple tests for rdar://51447187
        { "sd",       ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "sd_Arab",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "sd_Deva",  ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "mid",      ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB }, // rdar://104625921
        { "mni_Beng", ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "mni_Mtei", ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "sat_Deva", ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "sat_Olck", ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "ks",       ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ks_Arab",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ks_Aran",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ks_Deva",  ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "pa",       ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "pa_Guru",  ULOC_LAYOUT_LTR, ULOC_LAYOUT_TTB },
        { "pa_Arab",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "pa_Aran",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ur",       ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ur_Arab",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
        { "ur_Aran",  ULOC_LAYOUT_RTL, ULOC_LAYOUT_TTB },
#endif  // APPLE_ICU_CHANGES
    };

    size_t i = 0;
    for (; i < UPRV_LENGTHOF(toTest); ++i) {
        UErrorCode statusCO = U_ZERO_ERROR;
        UErrorCode statusLO = U_ZERO_ERROR;
        const char* const localeId = toTest[i].localeId;
        const ULayoutType co = uloc_getCharacterOrientation(localeId, &statusCO);
        const ULayoutType expectedCO = toTest[i].character;
        const ULayoutType lo = uloc_getLineOrientation(localeId, &statusLO);
        const ULayoutType expectedLO = toTest[i].line;
        if (U_FAILURE(statusCO)) {
            log_err_status(statusCO,
                "  unexpected failure for uloc_getCharacterOrientation(), with localId \"%s\" and status %s\n",
                localeId,
                u_errorName(statusCO));
        }
        else if (co != expectedCO) {
            log_err(
                "  unexpected result for uloc_getCharacterOrientation(), with localeId \"%s\". Expected %s but got result %s\n",
                localeId,
                ULayoutTypeToString(expectedCO),
                ULayoutTypeToString(co));
        }
        if (U_FAILURE(statusLO)) {
            log_err_status(statusLO,
                "  unexpected failure for uloc_getLineOrientation(), with localId \"%s\" and status %s\n",
                localeId,
                u_errorName(statusLO));
        }
        else if (lo != expectedLO) {
            log_err(
                "  unexpected result for uloc_getLineOrientation(), with localeId \"%s\". Expected %s but got result %s\n",
                localeId,
                ULayoutTypeToString(expectedLO),
                ULayoutTypeToString(lo));
        }
    }
}

static void  TestULocale() {
    int i;
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *resIndex = ures_open(NULL,"res_index", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "Could not open res_index.res. Exiting. Error: %s\n", u_errorName(status));
        return;
    }
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {
        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        UChar name1[256], name2[256];
        char names1[256], names2[256];
        int32_t capacity = 256;

        status = U_ZERO_ERROR;
        if(!isLocaleAvailable(resIndex, newLoc)){
            continue;
        }
        uloc_getDisplayName(oldLoc, ULOC_US, name1, capacity, &status);
        if(U_FAILURE(status)){
            log_err("uloc_getDisplayName(%s) failed %s\n", oldLoc, u_errorName(status));
        }

        uloc_getDisplayName(newLoc, ULOC_US, name2, capacity, &status);
        if(U_FAILURE(status)){
            log_err("uloc_getDisplayName(%s) failed %s\n", newLoc, u_errorName(status));
        }

        if (u_strcmp(name1, name2)!=0) {
            log_err("The locales are not equal!.Old: %s, New: %s \n", oldLoc, newLoc);
        }
        u_austrcpy(names1, name1);
        u_austrcpy(names2, name2);
        log_verbose("uloc_getDisplayName old:%s   new:%s\n", names1, names2);
    }
    ures_close(resIndex);

}

static void TestUResourceBundle() {
    const char* us1;
    const char* us2;

    UResourceBundle* rb1 = NULL;
    UResourceBundle* rb2 = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int i;
    UResourceBundle *resIndex = NULL;
    if(U_FAILURE(status)){
        log_err("Could not open res_index.res. Exiting. Error: %s\n", u_errorName(status));
        return;
    }
    resIndex = ures_open(NULL,"res_index", &status);
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {

        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        if(!isLocaleAvailable(resIndex, newLoc)){
            continue;
        }
        rb1 = ures_open(NULL, oldLoc, &status);
        if (U_FAILURE(status)) {
            log_err("ures_open(%s) failed %s\n", oldLoc, u_errorName(status));
        }

        us1 = ures_getLocaleByType(rb1, ULOC_ACTUAL_LOCALE, &status);

        status = U_ZERO_ERROR;
        rb2 = ures_open(NULL, newLoc, &status);
        if (U_FAILURE(status)) {
            log_err("ures_open(%s) failed %s\n", oldLoc, u_errorName(status));
        } 
        us2 = ures_getLocaleByType(rb2, ULOC_ACTUAL_LOCALE, &status);

        if (strcmp(us1,newLoc)!=0 || strcmp(us1,us2)!=0 ) {
            log_err("The locales are not equal!.Old: %s, New: %s \n", oldLoc, newLoc);
        }

        log_verbose("ures_getStringByKey old:%s   new:%s\n", us1, us2);
        ures_close(rb1);
        rb1 = NULL;
        ures_close(rb2);
        rb2 = NULL;
    }
    ures_close(resIndex);
}

static void TestDisplayName() {
    
    UChar oldCountry[256] = {'\0'};
    UChar newCountry[256] = {'\0'};
    UChar oldLang[256] = {'\0'};
    UChar newLang[256] = {'\0'};
    char country[256] ={'\0'}; 
    char language[256] ={'\0'};
    int32_t capacity = 256;
    int i =0;
    int j=0;
    for (i=0; i<UPRV_LENGTHOF(LOCALE_ALIAS); i++) {
        const char* oldLoc = LOCALE_ALIAS[i][0];
        const char* newLoc = LOCALE_ALIAS[i][1];
        UErrorCode status = U_ZERO_ERROR;
        int32_t available = uloc_countAvailable();

        for(j=0; j<available; j++){
            
            const char* dispLoc = uloc_getAvailable(j);
            int32_t oldCountryLen = uloc_getDisplayCountry(oldLoc,dispLoc, oldCountry, capacity, &status);
            int32_t newCountryLen = uloc_getDisplayCountry(newLoc, dispLoc, newCountry, capacity, &status);
            int32_t oldLangLen = uloc_getDisplayLanguage(oldLoc, dispLoc, oldLang, capacity, &status);
            int32_t newLangLen = uloc_getDisplayLanguage(newLoc, dispLoc, newLang, capacity, &status );
            
            int32_t countryLen = uloc_getCountry(newLoc, country, capacity, &status);
            int32_t langLen  = uloc_getLanguage(newLoc, language, capacity, &status);
            /* there is a display name for the current country ID */
            if(countryLen != newCountryLen ){
                if(u_strncmp(oldCountry,newCountry,oldCountryLen)!=0){
                    log_err("uloc_getDisplayCountry() failed for %s in display locale %s \n", oldLoc, dispLoc);
                }
            }
            /* there is a display name for the current lang ID */
            if(langLen!=newLangLen){
                if(u_strncmp(oldLang,newLang,oldLangLen)){
                    log_err("uloc_getDisplayLanguage() failed for %s in display locale %s \n", oldLoc, dispLoc);                }
            }
        }
    }
}

static void TestGetLocaleForLCID() {
    int32_t i, length, lengthPre;
    const char* testLocale = 0;
    UErrorCode status = U_ZERO_ERROR;
    char            temp2[40], temp3[40];
    uint32_t lcid;
    
    lcid = uloc_getLCID("en_US");
    if (lcid != 0x0409) {
        log_err("  uloc_getLCID(\"en_US\") = %d, expected 0x0409\n", lcid);
    }
    
    lengthPre = uloc_getLocaleForLCID(lcid, temp2, 4, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        log_err("  unexpected result from uloc_getLocaleForLCID with small buffer: %s\n", u_errorName(status));
    }
    else {
        status = U_ZERO_ERROR;
    }
    
    length = uloc_getLocaleForLCID(lcid, temp2, UPRV_LENGTHOF(temp2), &status);
    if (U_FAILURE(status)) {
        log_err("  unexpected result from uloc_getLocaleForLCID(0x0409): %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    }
    
    if (length != lengthPre) {
        log_err("  uloc_getLocaleForLCID(0x0409): returned length %d does not match preflight length %d\n", length, lengthPre);
    }
    
    length = uloc_getLocaleForLCID(0x12345, temp2, UPRV_LENGTHOF(temp2), &status);
    if (U_SUCCESS(status)) {
        log_err("  unexpected result from uloc_getLocaleForLCID(0x12345): %s, status %s\n", temp2, u_errorName(status));
    }
    status = U_ZERO_ERROR;
    
    log_verbose("Testing getLocaleForLCID vs. locale data\n");
    for (i = 0; i < LOCALE_SIZE; i++) {
        
        testLocale=rawData2[NAME][i];
        
        log_verbose("Testing   %s ......\n", testLocale);
        
        sscanf(rawData2[LCID][i], "%x", &lcid);
        length = uloc_getLocaleForLCID(lcid, temp2, UPRV_LENGTHOF(temp2), &status);
        if (U_FAILURE(status)) {
            log_err("  unexpected failure of uloc_getLocaleForLCID(%#04x), status %s\n", lcid, u_errorName(status));
            status = U_ZERO_ERROR;
            continue;
        }
        
        if (length != (int32_t)uprv_strlen(temp2)) {
            log_err("  returned length %d not correct for uloc_getLocaleForLCID(%#04x), expected %d\n", length, lcid, uprv_strlen(temp2));
        }
        
        /* Compare language, country, script */
        length = uloc_getLanguage(temp2, temp3, UPRV_LENGTHOF(temp3), &status);
        if (U_FAILURE(status)) {
            log_err("  couldn't get language in uloc_getLocaleForLCID(%#04x) = %s, status %s\n", lcid, temp2, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strcmp(temp3, rawData2[LANG][i]) && !(uprv_strcmp(temp3, "nn") == 0 && uprv_strcmp(rawData2[VAR][i], "NY") == 0)) {
            log_err("  language doesn't match expected %s in in uloc_getLocaleForLCID(%#04x) = %s\n", rawData2[LANG][i], lcid, temp2);
        }
        
        length = uloc_getScript(temp2, temp3, UPRV_LENGTHOF(temp3), &status);
        if (U_FAILURE(status)) {
            log_err("  couldn't get script in uloc_getLocaleForLCID(%#04x) = %s, status %s\n", lcid, temp2, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strcmp(temp3, rawData2[SCRIPT][i])) {
            log_err("  script doesn't match expected %s in in uloc_getLocaleForLCID(%#04x) = %s\n", rawData2[SCRIPT][i], lcid, temp2);
        }
        
        length = uloc_getCountry(temp2, temp3, UPRV_LENGTHOF(temp3), &status);
        if (U_FAILURE(status)) {
            log_err("  couldn't get country in uloc_getLocaleForLCID(%#04x) = %s, status %s\n", lcid, temp2, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strlen(rawData2[CTRY][i]) && uprv_strcmp(temp3, rawData2[CTRY][i])) {
            log_err("  country doesn't match expected %s in in uloc_getLocaleForLCID(%#04x) = %s\n", rawData2[CTRY][i], lcid, temp2);
        }
    }
    
}

const char* const basic_maximize_data[][2] = {
  {
    "zu_Zzzz_Zz",
    "zu_Latn_ZA",
  }, {
    "ZU_Zz",
    "zu_Latn_ZA"
  }, {
    "zu_LATN",
    "zu_Latn_ZA"
  }, {
    "en_Zz",
    "en_Latn_US"
  }, {
    "en_us",
    "en_Latn_US"
  }, {
    "en_Kore",
    "en_Kore_US"
  }, {
    "en_Kore_Zz",
    "en_Kore_US"
  }, {
    "en_Kore_ZA",
    "en_Kore_ZA"
  }, {
    "en_Kore_ZA_POSIX",
    "en_Kore_ZA_POSIX"
  }, {
    "en_Gujr",
    "en_Gujr_US"
  }, {
    "en_ZA",
    "en_Latn_ZA"
  }, {
    "en_Gujr_Zz",
    "en_Gujr_US"
  }, {
    "en_Gujr_ZA",
    "en_Gujr_ZA"
  }, {
    "en_Gujr_ZA_POSIX",
    "en_Gujr_ZA_POSIX"
  }, {
    "en_US_POSIX_1901",
    "en_Latn_US_POSIX_1901"
  }, {
    "en_Latn__POSIX_1901",
    "en_Latn_US_POSIX_1901"
  }, {
    "en__POSIX_1901",
    "en_Latn_US_POSIX_1901"
  }, {
    "de__POSIX_1901",
    "de_Latn_DE_POSIX_1901"
  }, {
    "en_US_BOSTON",
    "en_Latn_US_BOSTON"
  }, {
    "th@calendar=buddhist",
    "th_Thai_TH@calendar=buddhist"
  }, {
    "ar_ZZ",
    "ar_Arab_EG"
  }, {
    "zh",
    "zh_Hans_CN"
  }, {
    "zh_TW",
    "zh_Hant_TW"
  }, {
    "zh_HK",
    "zh_Hant_HK"
  }, {
    "zh_Hant",
    "zh_Hant_TW"
  }, {
    "zh_Zzzz_CN",
    "zh_Hans_CN"
  }, {
    "und_US",
    "en_Latn_US"
  }, {
    "und_HK",
    "zh_Hant_HK"
  }, {
    "zzz",
    ""
  }, {
     "de_u_co_phonebk",
     "de_Latn_DE@collation=phonebook"
  }, {
     "de_Latn_u_co_phonebk",
      "de_Latn_DE@collation=phonebook"
  }, {
     "de_Latn_DE_u_co_phonebk",
      "de_Latn_DE@collation=phonebook"
  }, {
    "_Arab@em=emoji",
    "ar_Arab_EG@em=emoji"
  }, {
    "_Latn@em=emoji",
    "en_Latn_US@em=emoji"
  }, {
    "_Latn_DE@em=emoji",
    "de_Latn_DE@em=emoji"
  }, {
    "_Zzzz_DE@em=emoji",
    "de_Latn_DE@em=emoji"
  }, {
    "_DE@em=emoji",
    "de_Latn_DE@em=emoji"
#if APPLE_ICU_CHANGES
// rdar://
  }, { // start Apple tests for rdar://47494884
    "ur",
    "ur_Aran_PK"
  }, {
    "ks",
    "ks_Aran_IN"
  }, {
    "und_Aran_PK",
    "ur_Aran_PK"
  }, {
    "und_Aran_IN",
    "ks_Aran_IN"
  }, {
    "ur_PK",
    "ur_Aran_PK"
  }, {
    "ks_IN",
    "ks_Aran_IN"
  }, {
    "ur_Arab",
    "ur_Arab_PK"
  }, {
    "ks_Arab",
    "ks_Arab_IN"
  }, { // start Apple tests for rdar://54153189
    "mni",
    "mni_Beng_IN"
  }, {
    "mni_Beng",
    "mni_Beng_IN"
  }, {
    "mni_Mtei",
    "mni_Mtei_IN"
  }, {
    "sat",
    "sat_Olck_IN"
  }, {
    "sat_Olck",
    "sat_Olck_IN"
  }, {
    "sat_Deva",
    "sat_Deva_IN"
#endif  // APPLE_ICU_CHANGES
  }
};

const char* const basic_minimize_data[][2] = {
  {
    "en_Latn_US",
    "en"
  }, {
    "en_Latn_US_POSIX_1901",
    "en__POSIX_1901"
  }, {
    "EN_Latn_US_POSIX_1901",
    "en__POSIX_1901"
  }, {
    "en_Zzzz_US_POSIX_1901",
    "en__POSIX_1901"
  }, {
    "de_Latn_DE_POSIX_1901",
    "de__POSIX_1901"
  }, {
    "zzz",
    ""
  }, {
    "en_Latn_US@calendar=gregorian",
    "en@calendar=gregorian"
  }
};

const char* const full_data[][3] = {
  {
    /*   "FROM", */
    /*   "ADD-LIKELY", */
    /*   "REMOVE-LIKELY" */
    /* }, { */
    "aa",
    "aa_Latn_ET",
    "aa"
  }, {
    "af",
    "af_Latn_ZA",
    "af"
#if APPLE_ICU_CHANGES
// rdar://
  }, {
    "ain",
    "ain_Kana_JP",
    "ain"
#endif  // APPLE_ICU_CHANGES
  }, {
    "ak",
    "ak_Latn_GH",
    "ak"
  }, {
    "am",
    "am_Ethi_ET",
    "am"
  }, {
    "ar",
    "ar_Arab_EG",
    "ar"
  }, {
    "as",
    "as_Beng_IN",
    "as"
  }, {
    "az",
    "az_Latn_AZ",
    "az"
  }, {
    "be",
    "be_Cyrl_BY",
    "be"
  }, {
    "bg",
    "bg_Cyrl_BG",
    "bg"
  }, {
    "bn",
    "bn_Beng_BD",
    "bn"
  }, {
    "bo",
    "bo_Tibt_CN",
    "bo"
  }, {
    "bs",
    "bs_Latn_BA",
    "bs"
  }, {
    "ca",
    "ca_Latn_ES",
    "ca"
  }, {
    "ch",
    "ch_Latn_GU",
    "ch"
  }, {
    "chk",
    "chk_Latn_FM",
    "chk"
  }, {
    "cs",
    "cs_Latn_CZ",
    "cs"
  }, {
    "cy",
    "cy_Latn_GB",
    "cy"
  }, {
    "da",
    "da_Latn_DK",
    "da"
  }, {
    "de",
    "de_Latn_DE",
    "de"
  }, {
    "dv",
    "dv_Thaa_MV",
    "dv"
  }, {
    "dz",
    "dz_Tibt_BT",
    "dz"
  }, {
    "ee",
    "ee_Latn_GH",
    "ee"
  }, {
    "el",
    "el_Grek_GR",
    "el"
  }, {
    "en",
    "en_Latn_US",
    "en"
  }, {
    "es",
    "es_Latn_ES",
    "es"
  }, {
    "et",
    "et_Latn_EE",
    "et"
  }, {
    "eu",
    "eu_Latn_ES",
    "eu"
  }, {
    "fa",
    "fa_Arab_IR",
    "fa"
  }, {
    "fi",
    "fi_Latn_FI",
    "fi"
  }, {
    "fil",
    "fil_Latn_PH",
    "fil"
  }, {
    "fo",
    "fo_Latn_FO",
    "fo"
  }, {
    "fr",
    "fr_Latn_FR",
    "fr"
  }, {
    "fur",
    "fur_Latn_IT",
    "fur"
  }, {
    "ga",
    "ga_Latn_IE",
    "ga"
  }, {
    "gaa",
    "gaa_Latn_GH",
    "gaa"
  }, {
    "gl",
    "gl_Latn_ES",
    "gl"
  }, {
    "gn",
    "gn_Latn_PY",
    "gn"
  }, {
    "gu",
    "gu_Gujr_IN",
    "gu"
  }, {
    "ha",
    "ha_Latn_NG",
    "ha"
  }, {
    "haw",
    "haw_Latn_US",
    "haw"
  }, {
    "he",
    "he_Hebr_IL",
    "he"
  }, {
    "hi",
    "hi_Deva_IN",
    "hi"
  }, {
    "hr",
    "hr_Latn_HR",
    "hr"
  }, {
    "ht",
    "ht_Latn_HT",
    "ht"
  }, {
    "hu",
    "hu_Latn_HU",
    "hu"
  }, {
    "hy",
    "hy_Armn_AM",
    "hy"
  }, {
    "id",
    "id_Latn_ID",
    "id"
  }, {
    "ig",
    "ig_Latn_NG",
    "ig"
  }, {
    "ii",
    "ii_Yiii_CN",
    "ii"
  }, {
    "is",
    "is_Latn_IS",
    "is"
  }, {
    "it",
    "it_Latn_IT",
    "it"
  }, {
    "ja",
    "ja_Jpan_JP",
    "ja"
  }, {
    "ka",
    "ka_Geor_GE",
    "ka"
  }, {
    "kaj",
    "kaj_Latn_NG",
    "kaj"
  }, {
    "kam",
    "kam_Latn_KE",
    "kam"
  }, {
    "kk",
    "kk_Cyrl_KZ",
    "kk"
  }, {
    "kl",
    "kl_Latn_GL",
    "kl"
  }, {
    "km",
    "km_Khmr_KH",
    "km"
  }, {
    "kn",
    "kn_Knda_IN",
    "kn"
  }, {
    "ko",
    "ko_Kore_KR",
    "ko"
  }, {
    "kok",
    "kok_Deva_IN",
    "kok"
  }, {
    "kpe",
    "kpe_Latn_LR",
    "kpe"
  }, {
    "ku",
    "ku_Latn_TR",
    "ku"
  }, {
    "ky",
    "ky_Cyrl_KG",
    "ky"
  }, {
    "la",
    "la_Latn_VA",
    "la"
  }, {
    "ln",
    "ln_Latn_CD",
    "ln"
  }, {
    "lo",
    "lo_Laoo_LA",
    "lo"
  }, {
    "lt",
    "lt_Latn_LT",
    "lt"
  }, {
    "lv",
    "lv_Latn_LV",
    "lv"
  }, {
    "mg",
    "mg_Latn_MG",
    "mg"
  }, {
    "mh",
    "mh_Latn_MH",
    "mh"
  }, {
    "mk",
    "mk_Cyrl_MK",
    "mk"
  }, {
    "ml",
    "ml_Mlym_IN",
    "ml"
  }, {
    "mn",
    "mn_Cyrl_MN",
    "mn"
  }, {
    "mr",
    "mr_Deva_IN",
    "mr"
  }, {
    "ms",
    "ms_Latn_MY",
    "ms"
#if APPLE_ICU_CHANGES
// rdar://
  }, { // rdar://27943264
    "ms_ID",
    "ms_Latn_ID",
    "ms_ID"
  }, { // rdar://27943264
    "ms_Arab",
    "ms_Arab_MY",
    "ms_Arab"
#endif  // APPLE_ICU_CHANGES
  }, {
    "mt",
    "mt_Latn_MT",
    "mt"
  }, {
    "my",
    "my_Mymr_MM",
    "my"
  }, {
    "na",
    "na_Latn_NR",
    "na"
  }, {
    "ne",
    "ne_Deva_NP",
    "ne"
  }, {
    "niu",
    "niu_Latn_NU",
    "niu"
  }, {
    "nl",
    "nl_Latn_NL",
    "nl"
  }, {
    "nn",
    "nn_Latn_NO",
    "nn"
  }, {
    "no",
    "no_Latn_NO",
    "no"
  }, {
    "nr",
    "nr_Latn_ZA",
    "nr"
  }, {
    "nso",
    "nso_Latn_ZA",
    "nso"
  }, {
    "ny",
    "ny_Latn_MW",
    "ny"
  }, {
    "om",
    "om_Latn_ET",
    "om"
  }, {
    "or",
    "or_Orya_IN",
    "or"
  }, {
    "pa",
    "pa_Guru_IN",
    "pa"
#if APPLE_ICU_CHANGES
// rdar://50687287 #20 add names for ks/pa/ur_Arab using script Naskh, force their use, remove redundant parens
  }, {
    "pa_Arab",
    "pa_Arab_PK",
    "pa_Arab"
  }, {
    "pa_Aran",
    "pa_Aran_PK",
    "pa_PK"
  }, {
    "pa_PK",
    "pa_Aran_PK",
    "pa_PK"
#else
  }, {
    "pa_Arab",
    "pa_Arab_PK",
    "pa_PK"
  }, {
    "pa_PK",
    "pa_Arab_PK",
    "pa_PK"
#endif  // APPLE_ICU_CHANGES
  }, {
    "pap",
    "pap_Latn_CW",
    "pap"
  }, {
    "pau",
    "pau_Latn_PW",
    "pau"
  }, {
    "pl",
    "pl_Latn_PL",
    "pl"
  }, {
    "ps",
    "ps_Arab_AF",
    "ps"
  }, {
    "pt",
    "pt_Latn_BR",
    "pt"
#if APPLE_ICU_CHANGES
// rdar://
  }, {
    "rhg",
    "rhg_Rohg_MM",
    "rhg"
#endif  // APPLE_ICU_CHANGES
  }, {
    "rn",
    "rn_Latn_BI",
    "rn"
  }, {
    "ro",
    "ro_Latn_RO",
    "ro"
  }, {
    "ru",
    "ru_Cyrl_RU",
    "ru"
  }, {
    "rw",
    "rw_Latn_RW",
    "rw"
  }, {
    "sa",
    "sa_Deva_IN",
    "sa"
  }, {
    "se",
    "se_Latn_NO",
    "se"
  }, {
    "sg",
    "sg_Latn_CF",
    "sg"
  }, {
    "si",
    "si_Sinh_LK",
    "si"
  }, {
    "sid",
    "sid_Latn_ET",
    "sid"
  }, {
    "sk",
    "sk_Latn_SK",
    "sk"
  }, {
    "sl",
    "sl_Latn_SI",
    "sl"
  }, {
    "sm",
    "sm_Latn_WS",
    "sm"
  }, {
    "so",
    "so_Latn_SO",
    "so"
  }, {
    "sq",
    "sq_Latn_AL",
    "sq"
  }, {
    "sr",
    "sr_Cyrl_RS",
    "sr"
  }, {
    "ss",
    "ss_Latn_ZA",
    "ss"
  }, {
    "st",
    "st_Latn_ZA",
    "st"
  }, {
    "sv",
    "sv_Latn_SE",
    "sv"
  }, {
    "sw",
    "sw_Latn_TZ",
    "sw"
#if APPLE_ICU_CHANGES
// rdar://
  }, {
    "syr",
    "syr_Syrc_IQ",
    "syr"
#endif  // APPLE_ICU_CHANGES
  }, {
    "ta",
    "ta_Taml_IN",
    "ta"
  }, {
    "te",
    "te_Telu_IN",
    "te"
  }, {
    "tet",
    "tet_Latn_TL",
    "tet"
  }, {
    "tg",
    "tg_Cyrl_TJ",
    "tg"
  }, {
    "th",
    "th_Thai_TH",
    "th"
  }, {
    "ti",
    "ti_Ethi_ET",
    "ti"
  }, {
    "tig",
    "tig_Ethi_ER",
    "tig"
  }, {
    "tk",
    "tk_Latn_TM",
    "tk"
  }, {
    "tkl",
    "tkl_Latn_TK",
    "tkl"
  }, {
    "tn",
    "tn_Latn_ZA",
    "tn"
  }, {
    "to",
    "to_Latn_TO",
    "to"
  }, {
    "tpi",
    "tpi_Latn_PG",
    "tpi"
  }, {
    "tr",
    "tr_Latn_TR",
    "tr"
  }, {
    "ts",
    "ts_Latn_ZA",
    "ts"
  }, {
    "tt",
    "tt_Cyrl_RU",
    "tt"
  }, {
    "tvl",
    "tvl_Latn_TV",
    "tvl"
  }, {
    "ty",
    "ty_Latn_PF",
    "ty"
  }, {
    "uk",
    "uk_Cyrl_UA",
    "uk"
  }, {
    "und",
    "en_Latn_US",
    "en"
  }, {
    "und_AD",
    "ca_Latn_AD",
    "ca_AD"
  }, {
    "und_AE",
    "ar_Arab_AE",
    "ar_AE"
  }, {
    "und_AF",
    "fa_Arab_AF",
    "fa_AF"
  }, {
    "und_AL",
    "sq_Latn_AL",
    "sq"
  }, {
    "und_AM",
    "hy_Armn_AM",
    "hy"
  }, {
    "und_AO",
    "pt_Latn_AO",
    "pt_AO"
  }, {
    "und_AR",
    "es_Latn_AR",
    "es_AR"
  }, {
    "und_AS",
    "sm_Latn_AS",
    "sm_AS"
  }, {
    "und_AT",
    "de_Latn_AT",
    "de_AT"
  }, {
    "und_AW",
    "nl_Latn_AW",
    "nl_AW"
  }, {
    "und_AX",
    "sv_Latn_AX",
    "sv_AX"
  }, {
    "und_AZ",
    "az_Latn_AZ",
    "az"
  }, {
    "und_Arab",
    "ar_Arab_EG",
    "ar"
  }, {
    "und_Arab_IN",
    "ur_Arab_IN",
#if APPLE_ICU_CHANGES
// rdar://
    "ur_Arab_IN" // rdar://47494884
#else
    "ur_IN"
#endif  // APPLE_ICU_CHANGES
  }, {
    "und_Arab_PK",
    "ur_Arab_PK",
#if APPLE_ICU_CHANGES
// rdar://
    "ur_Arab", // rdar://47494884
#else
    "ur"
#endif  // APPLE_ICU_CHANGES
  }, {
    "und_Arab_SN",
    "ar_Arab_SN",
    "ar_SN"
  }, {
    "und_Armn",
    "hy_Armn_AM",
    "hy"
  }, {
    "und_BA",
    "bs_Latn_BA",
    "bs"
  }, {
    "und_BD",
    "bn_Beng_BD",
    "bn"
  }, {
    "und_BE",
    "nl_Latn_BE",
    "nl_BE"
  }, {
    "und_BF",
    "fr_Latn_BF",
    "fr_BF"
  }, {
    "und_BG",
    "bg_Cyrl_BG",
    "bg"
  }, {
    "und_BH",
    "ar_Arab_BH",
    "ar_BH"
  }, {
    "und_BI",
    "rn_Latn_BI",
    "rn"
  }, {
    "und_BJ",
    "fr_Latn_BJ",
    "fr_BJ"
  }, {
    "und_BN",
    "ms_Latn_BN",
    "ms_BN"
  }, {
    "und_BO",
    "es_Latn_BO",
    "es_BO"
  }, {
    "und_BR",
    "pt_Latn_BR",
    "pt"
  }, {
    "und_BT",
    "dz_Tibt_BT",
    "dz"
  }, {
    "und_BY",
    "be_Cyrl_BY",
    "be"
  }, {
    "und_Beng",
    "bn_Beng_BD",
    "bn"
  }, {
    "und_Beng_IN",
    "bn_Beng_IN",
    "bn_IN"
  }, {
    "und_CD",
    "sw_Latn_CD",
    "sw_CD"
  }, {
    "und_CF",
    "fr_Latn_CF",
    "fr_CF"
  }, {
    "und_CG",
    "fr_Latn_CG",
    "fr_CG"
  }, {
    "und_CH",
    "de_Latn_CH",
    "de_CH"
  }, {
    "und_CI",
    "fr_Latn_CI",
    "fr_CI"
  }, {
    "und_CL",
    "es_Latn_CL",
    "es_CL"
  }, {
    "und_CM",
    "fr_Latn_CM",
    "fr_CM"
  }, {
    "und_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_CO",
    "es_Latn_CO",
    "es_CO"
  }, {
    "und_CR",
    "es_Latn_CR",
    "es_CR"
  }, {
    "und_CU",
    "es_Latn_CU",
    "es_CU"
  }, {
    "und_CV",
    "pt_Latn_CV",
    "pt_CV"
  }, {
    "und_CY",
    "el_Grek_CY",
    "el_CY"
  }, {
    "und_CZ",
    "cs_Latn_CZ",
    "cs"
  }, {
    "und_Cher",
    "chr_Cher_US",
    "chr"
  }, {
    "und_Cyrl",
    "ru_Cyrl_RU",
    "ru"
  }, {
    "und_Cyrl_KZ",
    "ru_Cyrl_KZ",
    "ru_KZ"
  }, {
    "und_DE",
    "de_Latn_DE",
    "de"
  }, {
    "und_DJ",
    "aa_Latn_DJ",
    "aa_DJ"
  }, {
    "und_DK",
    "da_Latn_DK",
    "da"
  }, {
    "und_DO",
    "es_Latn_DO",
    "es_DO"
  }, {
    "und_DZ",
    "ar_Arab_DZ",
    "ar_DZ"
  }, {
    "und_Deva",
    "hi_Deva_IN",
    "hi"
  }, {
    "und_EC",
    "es_Latn_EC",
    "es_EC"
  }, {
    "und_EE",
    "et_Latn_EE",
    "et"
  }, {
    "und_EG",
    "ar_Arab_EG",
    "ar"
  }, {
    "und_EH",
    "ar_Arab_EH",
    "ar_EH"
  }, {
    "und_ER",
    "ti_Ethi_ER",
    "ti_ER"
  }, {
    "und_ES",
    "es_Latn_ES",
    "es"
  }, {
    "und_ET",
    "am_Ethi_ET",
    "am"
  }, {
    "und_Ethi",
    "am_Ethi_ET",
    "am"
  }, {
    "und_Ethi_ER",
    "ti_Ethi_ER",
    "ti_ER"
  }, {
    "und_FI",
    "fi_Latn_FI",
    "fi"
  }, {
    "und_FM",
    "en_Latn_FM",
    "en_FM"
  }, {
    "und_FO",
    "fo_Latn_FO",
    "fo"
  }, {
    "und_FR",
    "fr_Latn_FR",
    "fr"
  }, {
    "und_GA",
    "fr_Latn_GA",
    "fr_GA"
  }, {
    "und_GE",
    "ka_Geor_GE",
    "ka"
  }, {
    "und_GF",
    "fr_Latn_GF",
    "fr_GF"
  }, {
    "und_GL",
    "kl_Latn_GL",
    "kl"
  }, {
    "und_GN",
    "fr_Latn_GN",
    "fr_GN"
  }, {
    "und_GP",
    "fr_Latn_GP",
    "fr_GP"
  }, {
    "und_GQ",
    "es_Latn_GQ",
    "es_GQ"
  }, {
    "und_GR",
    "el_Grek_GR",
    "el"
  }, {
    "und_GT",
    "es_Latn_GT",
    "es_GT"
  }, {
    "und_GU",
    "en_Latn_GU",
    "en_GU"
  }, {
    "und_GW",
    "pt_Latn_GW",
    "pt_GW"
  }, {
    "und_Geor",
    "ka_Geor_GE",
    "ka"
  }, {
    "und_Grek",
    "el_Grek_GR",
    "el"
  }, {
    "und_Gujr",
    "gu_Gujr_IN",
    "gu"
  }, {
    "und_Guru",
    "pa_Guru_IN",
    "pa"
  }, {
    "und_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "und_HN",
    "es_Latn_HN",
    "es_HN"
  }, {
    "und_HR",
    "hr_Latn_HR",
    "hr"
  }, {
    "und_HT",
    "ht_Latn_HT",
    "ht"
  }, {
    "und_HU",
    "hu_Latn_HU",
    "hu"
  }, {
    "und_Hani",
    "zh_Hani_CN",
    "zh_Hani"
  }, {
    "und_Hans",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_Hant",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Hebr",
    "he_Hebr_IL",
    "he"
  }, {
    "und_IL",
    "he_Hebr_IL",
    "he"
  }, {
    "und_IN",
    "hi_Deva_IN",
    "hi"
  }, {
    "und_IQ",
    "ar_Arab_IQ",
    "ar_IQ"
  }, {
    "und_IR",
    "fa_Arab_IR",
    "fa"
  }, {
    "und_IS",
    "is_Latn_IS",
    "is"
  }, {
    "und_IT",
    "it_Latn_IT",
    "it"
  }, {
    "und_JO",
    "ar_Arab_JO",
    "ar_JO"
  }, {
    "und_JP",
    "ja_Jpan_JP",
    "ja"
  }, {
    "und_Jpan",
    "ja_Jpan_JP",
    "ja"
  }, {
    "und_KG",
    "ky_Cyrl_KG",
    "ky"
  }, {
    "und_KH",
    "km_Khmr_KH",
    "km"
  }, {
    "und_KM",
    "ar_Arab_KM",
    "ar_KM"
  }, {
    "und_KP",
    "ko_Kore_KP",
    "ko_KP"
  }, {
    "und_KR",
    "ko_Kore_KR",
    "ko"
  }, {
    "und_KW",
    "ar_Arab_KW",
    "ar_KW"
  }, {
    "und_KZ",
    "ru_Cyrl_KZ",
    "ru_KZ"
  }, {
    "und_Khmr",
    "km_Khmr_KH",
    "km"
  }, {
    "und_Knda",
    "kn_Knda_IN",
    "kn"
  }, {
    "und_Kore",
    "ko_Kore_KR",
    "ko"
  }, {
    "und_LA",
    "lo_Laoo_LA",
    "lo"
  }, {
    "und_LB",
    "ar_Arab_LB",
    "ar_LB"
  }, {
    "und_LI",
    "de_Latn_LI",
    "de_LI"
  }, {
    "und_LK",
    "si_Sinh_LK",
    "si"
  }, {
    "und_LS",
    "st_Latn_LS",
    "st_LS"
  }, {
    "und_LT",
    "lt_Latn_LT",
    "lt"
  }, {
    "und_LU",
    "fr_Latn_LU",
    "fr_LU"
  }, {
    "und_LV",
    "lv_Latn_LV",
    "lv"
  }, {
    "und_LY",
    "ar_Arab_LY",
    "ar_LY"
  }, {
    "und_Laoo",
    "lo_Laoo_LA",
    "lo"
  }, {
    "und_Latn_ES",
    "es_Latn_ES",
    "es"
  }, {
    "und_Latn_ET",
    "en_Latn_ET",
    "en_ET"
  }, {
    "und_Latn_GB",
    "en_Latn_GB",
    "en_GB"
  }, {
    "und_Latn_GH",
    "ak_Latn_GH",
    "ak"
  }, {
    "und_Latn_ID",
    "id_Latn_ID",
    "id"
  }, {
    "und_Latn_IT",
    "it_Latn_IT",
    "it"
  }, {
    "und_Latn_NG",
    "en_Latn_NG",
    "en_NG"
  }, {
    "und_Latn_TR",
    "tr_Latn_TR",
    "tr"
  }, {
    "und_Latn_ZA",
    "en_Latn_ZA",
    "en_ZA"
  }, {
    "und_MA",
    "ar_Arab_MA",
    "ar_MA"
  }, {
    "und_MC",
    "fr_Latn_MC",
    "fr_MC"
  }, {
    "und_MD",
    "ro_Latn_MD",
    "ro_MD"
  }, {
    "und_ME",
    "sr_Latn_ME",
    "sr_ME"
  }, {
    "und_MG",
    "mg_Latn_MG",
    "mg"
  }, {
    "und_MH",
    "en_Latn_MH",
    "en_MH"
  }, {
    "und_MK",
    "mk_Cyrl_MK",
    "mk"
  }, {
    "und_ML",
    "bm_Latn_ML",
    "bm"
  }, {
    "und_MM",
    "my_Mymr_MM",
    "my"
  }, {
    "und_MN",
    "mn_Cyrl_MN",
    "mn"
  }, {
    "und_MO",
    "zh_Hant_MO",
    "zh_MO"
  }, {
    "und_MQ",
    "fr_Latn_MQ",
    "fr_MQ"
  }, {
    "und_MR",
    "ar_Arab_MR",
    "ar_MR"
  }, {
    "und_MT",
    "mt_Latn_MT",
    "mt"
  }, {
    "und_MV",
    "dv_Thaa_MV",
    "dv"
  }, {
    "und_MW",
    "en_Latn_MW",
    "en_MW"
  }, {
    "und_MX",
    "es_Latn_MX",
    "es_MX"
  }, {
    "und_MY",
    "ms_Latn_MY",
    "ms"
  }, {
    "und_MZ",
    "pt_Latn_MZ",
    "pt_MZ"
  }, {
    "und_Mlym",
    "ml_Mlym_IN",
    "ml"
  }, {
    "und_Mymr",
    "my_Mymr_MM",
    "my"
  }, {
    "und_NC",
    "fr_Latn_NC",
    "fr_NC"
  }, {
    "und_NE",
    "ha_Latn_NE",
    "ha_NE"
  }, {
    "und_NG",
    "en_Latn_NG",
    "en_NG"
  }, {
    "und_NI",
    "es_Latn_NI",
    "es_NI"
  }, {
    "und_NL",
    "nl_Latn_NL",
    "nl"
  }, {
    "und_NO",
    "nb_Latn_NO",
    "nb"
  }, {
    "und_NP",
    "ne_Deva_NP",
    "ne"
  }, {
    "und_NR",
    "en_Latn_NR",
    "en_NR"
  }, {
    "und_NU",
    "en_Latn_NU",
    "en_NU"
  }, {
    "und_OM",
    "ar_Arab_OM",
    "ar_OM"
  }, {
    "und_Orya",
    "or_Orya_IN",
    "or"
  }, {
    "und_PA",
    "es_Latn_PA",
    "es_PA"
  }, {
    "und_PE",
    "es_Latn_PE",
    "es_PE"
  }, {
    "und_PF",
    "fr_Latn_PF",
    "fr_PF"
  }, {
    "und_PG",
    "tpi_Latn_PG",
    "tpi"
  }, {
    "und_PH",
    "fil_Latn_PH",
    "fil"
  }, {
    "und_PL",
    "pl_Latn_PL",
    "pl"
  }, {
    "und_PM",
    "fr_Latn_PM",
    "fr_PM"
  }, {
    "und_PR",
    "es_Latn_PR",
    "es_PR"
  }, {
    "und_PS",
    "ar_Arab_PS",
    "ar_PS"
  }, {
    "und_PT",
    "pt_Latn_PT",
    "pt_PT"
  }, {
    "und_PW",
    "pau_Latn_PW",
    "pau"
  }, {
    "und_PY",
    "gn_Latn_PY",
    "gn"
  }, {
    "und_QA",
    "ar_Arab_QA",
    "ar_QA"
  }, {
    "und_RE",
    "fr_Latn_RE",
    "fr_RE"
  }, {
    "und_RO",
    "ro_Latn_RO",
    "ro"
  }, {
    "und_RS",
    "sr_Cyrl_RS",
    "sr"
  }, {
    "und_RU",
    "ru_Cyrl_RU",
    "ru"
  }, {
    "und_RW",
    "rw_Latn_RW",
    "rw"
  }, {
    "und_SA",
    "ar_Arab_SA",
    "ar_SA"
  }, {
    "und_SD",
    "ar_Arab_SD",
    "ar_SD"
  }, {
    "und_SE",
    "sv_Latn_SE",
    "sv"
  }, {
    "und_SG",
    "en_Latn_SG",
    "en_SG"
  }, {
    "und_SI",
    "sl_Latn_SI",
    "sl"
  }, {
    "und_SJ",
    "nb_Latn_SJ",
    "nb_SJ"
  }, {
    "und_SK",
    "sk_Latn_SK",
    "sk"
  }, {
    "und_SM",
    "it_Latn_SM",
    "it_SM"
  }, {
    "und_SN",
    "fr_Latn_SN",
    "fr_SN"
  }, {
    "und_SO",
    "so_Latn_SO",
    "so"
  }, {
    "und_SR",
    "nl_Latn_SR",
    "nl_SR"
  }, {
    "und_ST",
    "pt_Latn_ST",
    "pt_ST"
  }, {
    "und_SV",
    "es_Latn_SV",
    "es_SV"
  }, {
    "und_SY",
    "ar_Arab_SY",
    "ar_SY"
  }, {
    "und_Sinh",
    "si_Sinh_LK",
    "si"
  }, {
    "und_TD",
    "fr_Latn_TD",
    "fr_TD"
  }, {
    "und_TG",
    "fr_Latn_TG",
    "fr_TG"
  }, {
    "und_TH",
    "th_Thai_TH",
    "th"
  }, {
    "und_TJ",
    "tg_Cyrl_TJ",
    "tg"
  }, {
    "und_TK",
    "tkl_Latn_TK",
    "tkl"
  }, {
    "und_TL",
    "pt_Latn_TL",
    "pt_TL"
  }, {
    "und_TM",
    "tk_Latn_TM",
    "tk"
  }, {
    "und_TN",
    "ar_Arab_TN",
    "ar_TN"
  }, {
    "und_TO",
    "to_Latn_TO",
    "to"
  }, {
    "und_TR",
    "tr_Latn_TR",
    "tr"
  }, {
    "und_TV",
    "tvl_Latn_TV",
    "tvl"
  }, {
    "und_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Taml",
    "ta_Taml_IN",
    "ta"
  }, {
    "und_Telu",
    "te_Telu_IN",
    "te"
  }, {
    "und_Thaa",
    "dv_Thaa_MV",
    "dv"
  }, {
    "und_Thai",
    "th_Thai_TH",
    "th"
  }, {
    "und_Tibt",
    "bo_Tibt_CN",
    "bo"
  }, {
    "und_UA",
    "uk_Cyrl_UA",
    "uk"
  }, {
    "und_UY",
    "es_Latn_UY",
    "es_UY"
  }, {
    "und_UZ",
    "uz_Latn_UZ",
    "uz"
  }, {
    "und_VA",
    "it_Latn_VA",
    "it_VA"
  }, {
    "und_VE",
    "es_Latn_VE",
    "es_VE"
  }, {
    "und_VN",
    "vi_Latn_VN",
    "vi"
  }, {
    "und_VU",
    "bi_Latn_VU",
    "bi"
  }, {
    "und_WF",
    "fr_Latn_WF",
    "fr_WF"
  }, {
    "und_WS",
    "sm_Latn_WS",
    "sm"
  }, {
    "und_YE",
    "ar_Arab_YE",
    "ar_YE"
  }, {
    "und_YT",
    "fr_Latn_YT",
    "fr_YT"
  }, {
    "und_Yiii",
    "ii_Yiii_CN",
    "ii"
  }, {
    "ur",
#if APPLE_ICU_CHANGES
// rdar://
    "ur_Aran_PK", // rdar://47494884
#else
    "ur_Arab_PK",
#endif  // APPLE_ICU_CHANGES
    "ur"
  }, {
    "uz",
    "uz_Latn_UZ",
    "uz"
  }, {
    "uz_AF",
    "uz_Arab_AF",
    "uz_AF"
  }, {
    "uz_Arab",
    "uz_Arab_AF",
    "uz_AF"
  }, {
    "ve",
    "ve_Latn_ZA",
    "ve"
  }, {
    "vi",
    "vi_Latn_VN",
    "vi"
  }, {
    "wal",
    "wal_Ethi_ET",
    "wal"
  }, {
    "wo",
    "wo_Latn_SN",
    "wo"
  }, {
    "xh",
    "xh_Latn_ZA",
    "xh"
  }, {
    "yo",
    "yo_Latn_NG",
    "yo"
  }, {
    "zh",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "zh_Hani",
    "zh_Hani_CN", /* changed due to cldrbug 6204, may be an error */
    "zh_Hani", /* changed due to cldrbug 6204, may be an error */
  }, {
    "zh_Hant",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zh_MO",
    "zh_Hant_MO",
    "zh_MO"
  }, {
    "zh_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zu",
    "zu_Latn_ZA",
    "zu"
  }, {
    "und",
    "en_Latn_US",
    "en"
  }, {
    "und_ZZ",
    "en_Latn_US",
    "en"
  }, {
    "und_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "und_AQ",
    "en_Latn_AQ",
    "en_AQ"
  }, {
    "und_Zzzz",
    "en_Latn_US",
    "en"
  }, {
    "und_Zzzz_ZZ",
    "en_Latn_US",
    "en"
  }, {
    "und_Zzzz_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_Zzzz_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Zzzz_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "und_Zzzz_AQ",
    "en_Latn_AQ",
    "en_AQ"
  }, {
    "und_Latn",
    "en_Latn_US",
    "en"
  }, {
    "und_Latn_ZZ",
    "en_Latn_US",
    "en"
  }, {
    "und_Latn_CN",
    "za_Latn_CN",
    "za"
  }, {
    "und_Latn_TW",
    "trv_Latn_TW",
    "trv"
  }, {
    "und_Latn_HK",
    "en_Latn_HK",
    "en_HK"
  }, {
    "und_Latn_AQ",
    "en_Latn_AQ",
    "en_AQ"
  }, {
    "und_Hans",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_Hans_ZZ",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_Hans_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "und_Hans_TW",
    "zh_Hans_TW",
    "zh_Hans_TW"
  }, {
    "und_Hans_HK",
    "zh_Hans_HK",
    "zh_Hans_HK"
  }, {
    "und_Hans_AQ",
    "zh_Hans_AQ",
    "zh_AQ"
  }, {
    "und_Hant",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Hant_ZZ",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Hant_CN",
    "zh_Hant_CN",
    "zh_Hant_CN"
  }, {
    "und_Hant_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "und_Hant_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "und_Hant_AQ",
    "zh_Hant_AQ",
    "zh_Hant_AQ"
  }, {
    "und_Moon",
    "en_Moon_US",
    "en_Moon"
  }, {
    "und_Moon_ZZ",
    "en_Moon_US",
    "en_Moon"
  }, {
    "und_Moon_CN",
    "zh_Moon_CN",
    "zh_Moon"
  }, {
    "und_Moon_TW",
    "zh_Moon_TW",
    "zh_Moon_TW"
  }, {
    "und_Moon_HK",
    "zh_Moon_HK",
    "zh_Moon_HK"
  }, {
    "und_Moon_AQ",
    "en_Moon_AQ",
    "en_Moon_AQ"
  }, {
    "es",
    "es_Latn_ES",
    "es"
  }, {
    "es_ZZ",
    "es_Latn_ES",
    "es"
  }, {
    "es_CN",
    "es_Latn_CN",
    "es_CN"
  }, {
    "es_TW",
    "es_Latn_TW",
    "es_TW"
  }, {
    "es_HK",
    "es_Latn_HK",
    "es_HK"
  }, {
    "es_AQ",
    "es_Latn_AQ",
    "es_AQ"
  }, {
    "es_Zzzz",
    "es_Latn_ES",
    "es"
  }, {
    "es_Zzzz_ZZ",
    "es_Latn_ES",
    "es"
  }, {
    "es_Zzzz_CN",
    "es_Latn_CN",
    "es_CN"
  }, {
    "es_Zzzz_TW",
    "es_Latn_TW",
    "es_TW"
  }, {
    "es_Zzzz_HK",
    "es_Latn_HK",
    "es_HK"
  }, {
    "es_Zzzz_AQ",
    "es_Latn_AQ",
    "es_AQ"
  }, {
    "es_Latn",
    "es_Latn_ES",
    "es"
  }, {
    "es_Latn_ZZ",
    "es_Latn_ES",
    "es"
  }, {
    "es_Latn_CN",
    "es_Latn_CN",
    "es_CN"
  }, {
    "es_Latn_TW",
    "es_Latn_TW",
    "es_TW"
  }, {
    "es_Latn_HK",
    "es_Latn_HK",
    "es_HK"
  }, {
    "es_Latn_AQ",
    "es_Latn_AQ",
    "es_AQ"
  }, {
    "es_Hans",
    "es_Hans_ES",
    "es_Hans"
  }, {
    "es_Hans_ZZ",
    "es_Hans_ES",
    "es_Hans"
  }, {
    "es_Hans_CN",
    "es_Hans_CN",
    "es_Hans_CN"
  }, {
    "es_Hans_TW",
    "es_Hans_TW",
    "es_Hans_TW"
  }, {
    "es_Hans_HK",
    "es_Hans_HK",
    "es_Hans_HK"
  }, {
    "es_Hans_AQ",
    "es_Hans_AQ",
    "es_Hans_AQ"
  }, {
    "es_Hant",
    "es_Hant_ES",
    "es_Hant"
  }, {
    "es_Hant_ZZ",
    "es_Hant_ES",
    "es_Hant"
  }, {
    "es_Hant_CN",
    "es_Hant_CN",
    "es_Hant_CN"
  }, {
    "es_Hant_TW",
    "es_Hant_TW",
    "es_Hant_TW"
  }, {
    "es_Hant_HK",
    "es_Hant_HK",
    "es_Hant_HK"
  }, {
    "es_Hant_AQ",
    "es_Hant_AQ",
    "es_Hant_AQ"
  }, {
    "es_Moon",
    "es_Moon_ES",
    "es_Moon"
  }, {
    "es_Moon_ZZ",
    "es_Moon_ES",
    "es_Moon"
  }, {
    "es_Moon_CN",
    "es_Moon_CN",
    "es_Moon_CN"
  }, {
    "es_Moon_TW",
    "es_Moon_TW",
    "es_Moon_TW"
  }, {
    "es_Moon_HK",
    "es_Moon_HK",
    "es_Moon_HK"
  }, {
    "es_Moon_AQ",
    "es_Moon_AQ",
    "es_Moon_AQ"
  }, {
    "zh",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_ZZ",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zh_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "zh_AQ",
    "zh_Hans_AQ",
    "zh_AQ"
#if APPLE_ICU_CHANGES
// rdar://
  }, {
    "zh_MY",
    "zh_Hans_MY",
    "zh_MY"
#endif  // APPLE_ICU_CHANGES
  }, {
    "zh_Zzzz",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_Zzzz_ZZ",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_Zzzz_CN",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_Zzzz_TW",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zh_Zzzz_HK",
    "zh_Hant_HK",
    "zh_HK"
  }, {
    "zh_Zzzz_AQ",
    "zh_Hans_AQ",
    "zh_AQ"
  }, {
    "zh_Latn",
    "zh_Latn_CN",
    "zh_Latn"
  }, {
    "zh_Latn_ZZ",
    "zh_Latn_CN",
    "zh_Latn"
  }, {
    "zh_Latn_CN",
    "zh_Latn_CN",
    "zh_Latn"
  }, {
    "zh_Latn_TW",
    "zh_Latn_TW",
    "zh_Latn_TW"
  }, {
    "zh_Latn_HK",
    "zh_Latn_HK",
    "zh_Latn_HK"
  }, {
    "zh_Latn_AQ",
    "zh_Latn_AQ",
    "zh_Latn_AQ"
  }, {
    "zh_Hans",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_Hans_ZZ",
    "zh_Hans_CN",
    "zh"
  }, {
    "zh_Hans_TW",
    "zh_Hans_TW",
    "zh_Hans_TW"
  }, {
    "zh_Hans_HK",
    "zh_Hans_HK",
    "zh_Hans_HK"
  }, {
    "zh_Hans_AQ",
    "zh_Hans_AQ",
    "zh_AQ"
  }, {
    "zh_Hant",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zh_Hant_ZZ",
    "zh_Hant_TW",
    "zh_TW"
  }, {
    "zh_Hant_CN",
    "zh_Hant_CN",
    "zh_Hant_CN"
  }, {
    "zh_Hant_AQ",
    "zh_Hant_AQ",
    "zh_Hant_AQ"
  }, {
    "zh_Moon",
    "zh_Moon_CN",
    "zh_Moon"
  }, {
    "zh_Moon_ZZ",
    "zh_Moon_CN",
    "zh_Moon"
  }, {
    "zh_Moon_CN",
    "zh_Moon_CN",
    "zh_Moon"
  }, {
    "zh_Moon_TW",
    "zh_Moon_TW",
    "zh_Moon_TW"
  }, {
    "zh_Moon_HK",
    "zh_Moon_HK",
    "zh_Moon_HK"
  }, {
    "zh_Moon_AQ",
    "zh_Moon_AQ",
    "zh_Moon_AQ"
  }, {
    "art",
    "",
    ""
  }, {
    "art_ZZ",
    "",
    ""
  }, {
    "art_CN",
    "",
    ""
  }, {
    "art_TW",
    "",
    ""
  }, {
    "art_HK",
    "",
    ""
  }, {
    "art_AQ",
    "",
    ""
  }, {
    "art_Zzzz",
    "",
    ""
  }, {
    "art_Zzzz_ZZ",
    "",
    ""
  }, {
    "art_Zzzz_CN",
    "",
    ""
  }, {
    "art_Zzzz_TW",
    "",
    ""
  }, {
    "art_Zzzz_HK",
    "",
    ""
  }, {
    "art_Zzzz_AQ",
    "",
    ""
  }, {
    "art_Latn",
    "",
    ""
  }, {
    "art_Latn_ZZ",
    "",
    ""
  }, {
    "art_Latn_CN",
    "",
    ""
  }, {
    "art_Latn_TW",
    "",
    ""
  }, {
    "art_Latn_HK",
    "",
    ""
  }, {
    "art_Latn_AQ",
    "",
    ""
  }, {
    "art_Hans",
    "",
    ""
  }, {
    "art_Hans_ZZ",
    "",
    ""
  }, {
    "art_Hans_CN",
    "",
    ""
  }, {
    "art_Hans_TW",
    "",
    ""
  }, {
    "art_Hans_HK",
    "",
    ""
  }, {
    "art_Hans_AQ",
    "",
    ""
  }, {
    "art_Hant",
    "",
    ""
  }, {
    "art_Hant_ZZ",
    "",
    ""
  }, {
    "art_Hant_CN",
    "",
    ""
  }, {
    "art_Hant_TW",
    "",
    ""
  }, {
    "art_Hant_HK",
    "",
    ""
  }, {
    "art_Hant_AQ",
    "",
    ""
  }, {
    "art_Moon",
    "",
    ""
  }, {
    "art_Moon_ZZ",
    "",
    ""
  }, {
    "art_Moon_CN",
    "",
    ""
  }, {
    "art_Moon_TW",
    "",
    ""
  }, {
    "art_Moon_HK",
    "",
    ""
  }, {
    "art_Moon_AQ",
    "",
    ""
  }, {
    "de@collation=phonebook",
    "de_Latn_DE@collation=phonebook",
    "de@collation=phonebook"
  }
};

typedef struct errorDataTag {
    const char* tag;
    const char* expected;
    UErrorCode uerror;
    int32_t  bufferSize;
} errorData;

const errorData maximizeErrors[] = {
    {
        "enfueiujhytdf",
        NULL,
        U_ILLEGAL_ARGUMENT_ERROR,
        -1
    },
    {
        "en_THUJIOGIURJHGJFURYHFJGURYYYHHGJURHG",
        NULL,
        U_ILLEGAL_ARGUMENT_ERROR,
        -1
    },
    {
        "en_THUJIOGIURJHGJFURYHFJGURYYYHHGJURHG",
        NULL,
        U_ILLEGAL_ARGUMENT_ERROR,
        -1
    },
    {
        "en_Latn_US_POSIX@currency=EURO",
        "en_Latn_US_POSIX@currency=EURO",
        U_BUFFER_OVERFLOW_ERROR,
        29
    },
    {
        "en_Latn_US_POSIX@currency=EURO",
        "en_Latn_US_POSIX@currency=EURO",
        U_STRING_NOT_TERMINATED_WARNING,
        30
    }
};

const errorData minimizeErrors[] = {
    {
        "enfueiujhytdf",
        NULL,
        U_ILLEGAL_ARGUMENT_ERROR,
        -1
    },
    {
        "en_THUJIOGIURJHGJFURYHFJGURYYYHHGJURHG",
        NULL,
        U_ILLEGAL_ARGUMENT_ERROR,
        -1
    },
    {
        "en_Latn_US_POSIX@currency=EURO",
        "en__POSIX@currency=EURO",
        U_BUFFER_OVERFLOW_ERROR,
        22
    },
    {
        "en_Latn_US_POSIX@currency=EURO",
        "en__POSIX@currency=EURO",
        U_STRING_NOT_TERMINATED_WARNING,
        23
    }
};

static int32_t getExpectedReturnValue(const errorData* data)
{
    if (data->uerror == U_BUFFER_OVERFLOW_ERROR ||
        data->uerror == U_STRING_NOT_TERMINATED_WARNING)
    {
        return (int32_t)strlen(data->expected);
    }
    else
    {
        return -1;
    }
}

static int32_t getBufferSize(const errorData* data, int32_t actualSize)
{
    if (data->expected == NULL)
    {
        return actualSize;
    }
    else if (data->bufferSize < 0)
    {
        return (int32_t)strlen(data->expected) + 1;
    }
    else
    {
        return data->bufferSize;
    }
}

static void TestLikelySubtags()
{
    char buffer[ULOC_FULLNAME_CAPACITY + ULOC_KEYWORD_AND_VALUES_CAPACITY + 1];
    int32_t i = 0;

    for (; i < UPRV_LENGTHOF(basic_maximize_data); ++i)
    {
        UErrorCode status = U_ZERO_ERROR;
        const char* const minimal = basic_maximize_data[i][0];
        const char* const maximal = basic_maximize_data[i][1];

        /* const int32_t length = */
            uloc_addLikelySubtags(
                minimal,
                buffer,
                sizeof(buffer),
                &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "  unexpected failure of uloc_addLikelySubtags(), minimal \"%s\" status %s\n", minimal, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strlen(maximal) == 0) {
            if (uprv_stricmp(minimal, buffer) != 0) {
                log_err("  unexpected maximal value \"%s\" in uloc_addLikelySubtags(), minimal \"%s\" = \"%s\"\n", maximal, minimal, buffer);
            }
        }
        else if (uprv_stricmp(maximal, buffer) != 0) {
            log_err("  maximal doesn't match expected %s in uloc_addLikelySubtags(), minimal \"%s\" = %s\n", maximal, minimal, buffer);
        }
    }

    for (i = 0; i < UPRV_LENGTHOF(basic_minimize_data); ++i) {

        UErrorCode status = U_ZERO_ERROR;
        const char* const maximal = basic_minimize_data[i][0];
        const char* const minimal = basic_minimize_data[i][1];

        /* const int32_t length = */
            uloc_minimizeSubtags(
                maximal,
                buffer,
                sizeof(buffer),
                &status);

        if (U_FAILURE(status)) {
            log_err_status(status, "  unexpected failure of uloc_MinimizeSubtags(), maximal \"%s\" status %s\n", maximal, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strlen(minimal) == 0) {
            if (uprv_stricmp(maximal, buffer) != 0) {
                log_err("  unexpected minimal value \"%s\" in uloc_minimizeSubtags(), maximal \"%s\" = \"%s\"\n", minimal, maximal, buffer);
            }
        }
        else if (uprv_stricmp(minimal, buffer) != 0) {
            log_err("  minimal doesn't match expected %s in uloc_MinimizeSubtags(), maximal \"%s\" = %s\n", minimal, maximal, buffer);
        }
    }

    for (i = 0; i < UPRV_LENGTHOF(full_data); ++i) {

        UErrorCode status = U_ZERO_ERROR;
        const char* const minimal = full_data[i][0];
        const char* const maximal = full_data[i][1];

        /* const int32_t length = */
            uloc_addLikelySubtags(
                minimal,
                buffer,
                sizeof(buffer),
                &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "  unexpected failure of uloc_addLikelySubtags(), minimal \"%s\" status \"%s\"\n", minimal, u_errorName(status));
            status = U_ZERO_ERROR;
        }
        else if (uprv_strlen(maximal) == 0) {
            if (uprv_stricmp(minimal, buffer) != 0) {
                log_err("  unexpected maximal value \"%s\" in uloc_addLikelySubtags(), minimal \"%s\" = \"%s\"\n", maximal, minimal, buffer);
            }
        }
        else if (uprv_stricmp(maximal, buffer) != 0) {
            log_err("  maximal doesn't match expected \"%s\" in uloc_addLikelySubtags(), minimal \"%s\" = \"%s\"\n", maximal, minimal, buffer);
        }
    }

    for (i = 0; i < UPRV_LENGTHOF(full_data); ++i) {

        UErrorCode status = U_ZERO_ERROR;
        const char* const maximal = full_data[i][1];
        const char* const minimal = full_data[i][2];

        if (strlen(maximal) > 0) {

            /* const int32_t length = */
                uloc_minimizeSubtags(
                    maximal,
                    buffer,
                    sizeof(buffer),
                    &status);

            if (U_FAILURE(status)) {
                log_err_status(status, "  unexpected failure of uloc_minimizeSubtags(), maximal \"%s\" status %s\n", maximal, u_errorName(status));
                status = U_ZERO_ERROR;
            }
            else if (uprv_strlen(minimal) == 0) {
                if (uprv_stricmp(maximal, buffer) != 0) {
                    log_err("  unexpected minimal value \"%s\" in uloc_minimizeSubtags(), maximal \"%s\" = \"%s\"\n", minimal, maximal, buffer);
                }
            }
            else if (uprv_stricmp(minimal, buffer) != 0) {
                log_err("  minimal doesn't match expected %s in uloc_MinimizeSubtags(), maximal \"%s\" = %s\n", minimal, maximal, buffer);
            }
        }
    }

    for (i = 0; i < UPRV_LENGTHOF(maximizeErrors); ++i) {

        UErrorCode status = U_ZERO_ERROR;
        const char* const minimal = maximizeErrors[i].tag;
        const char* const maximal = maximizeErrors[i].expected;
        const UErrorCode expectedStatus = maximizeErrors[i].uerror;
        const int32_t expectedLength = getExpectedReturnValue(&maximizeErrors[i]);
        const int32_t bufferSize = getBufferSize(&maximizeErrors[i], sizeof(buffer));

        const int32_t length =
            uloc_addLikelySubtags(
                minimal,
                buffer,
                bufferSize,
                &status);

        if (status == U_ZERO_ERROR) {
            log_err("  unexpected U_ZERO_ERROR for uloc_addLikelySubtags(), minimal \"%s\" expected status %s\n", minimal, u_errorName(expectedStatus));
            status = U_ZERO_ERROR;
        }
        else if (status != expectedStatus) {
            log_err_status(status, "  unexpected status for uloc_addLikelySubtags(), minimal \"%s\" expected status %s, but got %s\n", minimal, u_errorName(expectedStatus), u_errorName(status));
        }
        else if (length != expectedLength) {
            log_err("  unexpected length for uloc_addLikelySubtags(), minimal \"%s\" expected length %d, but got %d\n", minimal, expectedLength, length);
        }
        else if (status == U_BUFFER_OVERFLOW_ERROR || status == U_STRING_NOT_TERMINATED_WARNING) {
            if (uprv_strnicmp(maximal, buffer, bufferSize) != 0) {
                log_err("  maximal doesn't match expected %s in uloc_addLikelySubtags(), minimal \"%s\" = %*s\n",
                    maximal, minimal, (int)sizeof(buffer), buffer);
            }
        }
    }

    for (i = 0; i < UPRV_LENGTHOF(minimizeErrors); ++i) {

        UErrorCode status = U_ZERO_ERROR;
        const char* const maximal = minimizeErrors[i].tag;
        const char* const minimal = minimizeErrors[i].expected;
        const UErrorCode expectedStatus = minimizeErrors[i].uerror;
        const int32_t expectedLength = getExpectedReturnValue(&minimizeErrors[i]);
        const int32_t bufferSize = getBufferSize(&minimizeErrors[i], sizeof(buffer));

        const int32_t length =
            uloc_minimizeSubtags(
                maximal,
                buffer,
                bufferSize,
                &status);

        if (status == U_ZERO_ERROR) {
            log_err("  unexpected U_ZERO_ERROR for uloc_minimizeSubtags(), maximal \"%s\" expected status %s\n", maximal, u_errorName(expectedStatus));
            status = U_ZERO_ERROR;
        }
        else if (status != expectedStatus) {
            log_err_status(status, "  unexpected status for uloc_minimizeSubtags(), maximal \"%s\" expected status %s, but got %s\n", maximal, u_errorName(expectedStatus), u_errorName(status));
        }
        else if (length != expectedLength) {
            log_err("  unexpected length for uloc_minimizeSubtags(), maximal \"%s\" expected length %d, but got %d\n", maximal, expectedLength, length);
        }
        else if (status == U_BUFFER_OVERFLOW_ERROR || status == U_STRING_NOT_TERMINATED_WARNING) {
            if (uprv_strnicmp(minimal, buffer, bufferSize) != 0) {
                log_err("  minimal doesn't match expected \"%s\" in uloc_minimizeSubtags(), minimal \"%s\" = \"%*s\"\n",
                    minimal, maximal, (int)sizeof(buffer), buffer);
            }
        }
    }
}

const char* const locale_to_langtag[][3] = {
    {"",            "und",          "und"},
    {"en",          "en",           "en"},
    {"en_US",       "en-US",        "en-US"},
    {"iw_IL",       "he-IL",        "he-IL"},
    {"sr_Latn_SR",  "sr-Latn-SR",   "sr-Latn-SR"},
    {"en__POSIX",   "en-u-va-posix", "en-u-va-posix"},
    {"en_POSIX",    "en-u-va-posix", "en-u-va-posix"},
    {"en_US_POSIX_VAR", "en-US-posix-x-lvariant-var", NULL},  /* variant POSIX_VAR is processed as regular variant */
    {"en_US_VAR_POSIX", "en-US-x-lvariant-var-posix", NULL},  /* variant VAR_POSIX is processed as regular variant */
    {"en_US_POSIX@va=posix2",   "en-US-u-va-posix2",  "en-US-u-va-posix2"},           /* if keyword va=xxx already exists, variant POSIX is simply dropped */
    {"en_US_POSIX@ca=japanese",  "en-US-u-ca-japanese-va-posix", "en-US-u-ca-japanese-va-posix"},
    {"und_555",     "und-555",      "und-555"},
    {"123",         "und",          NULL},
    {"%$#&",        "und",          NULL},
    {"_Latn",       "und-Latn",     "und-Latn"},
    {"_DE",         "und-DE",       "und-DE"},
    {"und_FR",      "und-FR",       "und-FR"},
    {"th_TH_TH",    "th-TH-x-lvariant-th", NULL},
    {"bogus",       "bogus",        "bogus"},
    {"foooobarrr",  "und",          NULL},
    {"aa_BB_CYRL",  "aa-BB-x-lvariant-cyrl", NULL},
    {"en_US_1234",  "en-US-1234",   "en-US-1234"},
    {"en_US_VARIANTA_VARIANTB", "en-US-varianta-variantb",  "en-US-varianta-variantb"},
    {"en_US_VARIANTB_VARIANTA", "en-US-varianta-variantb",  "en-US-varianta-variantb"}, /* ICU-20478 */
    {"ja__9876_5432",   "ja-5432-9876", "ja-5432-9876"}, /* ICU-20478 */
    {"sl__ROZAJ_BISKE_1994",   "sl-1994-biske-rozaj", "sl-1994-biske-rozaj"}, /* ICU-20478 */
    {"en__SCOUSE_FONIPA",   "en-fonipa-scouse", "en-fonipa-scouse"}, /* ICU-20478 */
    {"zh_Hant__VAR",    "zh-Hant-x-lvariant-var", NULL},
    {"es__BADVARIANT_GOODVAR",  "es-goodvar",   NULL},
    {"en@calendar=gregorian",   "en-u-ca-gregory",  "en-u-ca-gregory"},
    {"de@collation=phonebook;calendar=gregorian",   "de-u-ca-gregory-co-phonebk",   "de-u-ca-gregory-co-phonebk"},
    {"th@numbers=thai;z=extz;x=priv-use;a=exta",   "th-a-exta-u-nu-thai-z-extz-x-priv-use", "th-a-exta-u-nu-thai-z-extz-x-priv-use"},
    {"en@timezone=America/New_York;calendar=japanese",    "en-u-ca-japanese-tz-usnyc",    "en-u-ca-japanese-tz-usnyc"},
    {"en@timezone=US/Eastern",  "en-u-tz-usnyc",    "en-u-tz-usnyc"},
    {"en@x=x-y-z;a=a-b-c",  "en-x-x-y-z",   NULL},
    {"it@collation=badcollationtype;colStrength=identical;cu=usd-eur", "it-u-cu-usd-eur-ks-identic",  NULL},
    {"en_US_POSIX", "en-US-u-va-posix", "en-US-u-va-posix"},
    {"en_US_POSIX@calendar=japanese;currency=EUR","en-US-u-ca-japanese-cu-eur-va-posix", "en-US-u-ca-japanese-cu-eur-va-posix"},
    {"@x=elmer",    "und-x-elmer",      "und-x-elmer"},
    {"en@x=elmer",  "en-x-elmer",   "en-x-elmer"},
    {"@x=elmer;a=exta", "und-a-exta-x-elmer",   "und-a-exta-x-elmer"},
    {"en_US@attribute=attr1-attr2;calendar=gregorian", "en-US-u-attr1-attr2-ca-gregory", "en-US-u-attr1-attr2-ca-gregory"},
    /* #12671 */
    {"en@a=bar;attribute=baz",  "en-a-bar-u-baz",   "en-a-bar-u-baz"},
    {"en@a=bar;attribute=baz;x=u-foo",  "en-a-bar-u-baz-x-u-foo",   "en-a-bar-u-baz-x-u-foo"},
    {"en@attribute=baz",    "en-u-baz", "en-u-baz"},
    {"en@attribute=baz;calendar=islamic-civil", "en-u-baz-ca-islamic-civil",    "en-u-baz-ca-islamic-civil"},
    {"en@a=bar;calendar=islamic-civil;x=u-foo", "en-a-bar-u-ca-islamic-civil-x-u-foo",  "en-a-bar-u-ca-islamic-civil-x-u-foo"},
    {"en@a=bar;attribute=baz;calendar=islamic-civil;x=u-foo",   "en-a-bar-u-baz-ca-islamic-civil-x-u-foo",  "en-a-bar-u-baz-ca-islamic-civil-x-u-foo"},
    {"en@9=efg;a=baz",    "en-9-efg-a-baz", "en-9-efg-a-baz"},

    // Before ICU 64, ICU locale canonicalization had some additional mappings.
    // They were removed for ICU-20187 "drop support for long-obsolete locale ID variants".
    // The following now uses standard canonicalization.
    {"az_AZ_CYRL", "az-AZ-x-lvariant-cyrl", NULL},


    /* ICU-20310 */
    {"en-u-kn-true",   "en-u-kn", "en-u-kn"},
    {"en-u-kn",   "en-u-kn", "en-u-kn"},
    {"de-u-co-yes",   "de-u-co", "de-u-co"},
    {"de-u-co",   "de-u-co", "de-u-co"},
    {"de@collation=yes",   "de-u-co", "de-u-co"},
    {"cmn-hans-cn-u-ca-t-ca-x-t-u",   "cmn-Hans-CN-t-ca-u-ca-x-t-u", "cmn-Hans-CN-t-ca-u-ca-x-t-u"},
    {NULL,          NULL,           NULL}
};

static void TestToLanguageTag(void) {
    char langtag[256];
    int32_t i;
    UErrorCode status;
    int32_t len;
    const char *inloc;
    const char *expected;

    for (i = 0; locale_to_langtag[i][0] != NULL; i++) {
        inloc = locale_to_langtag[i][0];

        /* testing non-strict mode */
        status = U_ZERO_ERROR;
        langtag[0] = 0;
        expected = locale_to_langtag[i][1];

        len = uloc_toLanguageTag(inloc, langtag, sizeof(langtag), false, &status);
        (void)len;    /* Suppress set but not used warning. */
        if (U_FAILURE(status)) {
            if (expected != NULL) {
                log_err("Error returned by uloc_toLanguageTag for locale id [%s] - error: %s\n",
                    inloc, u_errorName(status));
            }
        } else {
            if (expected == NULL) {
                log_err("Error should be returned by uloc_toLanguageTag for locale id [%s], but [%s] is returned without errors\n",
                    inloc, langtag);
            } else if (uprv_strcmp(langtag, expected) != 0) {
                log_data_err("uloc_toLanguageTag returned language tag [%s] for input locale [%s] - expected: [%s]. Are you missing data?\n",
                    langtag, inloc, expected);
            }
        }

        /* testing strict mode */
        status = U_ZERO_ERROR;
        langtag[0] = 0;
        expected = locale_to_langtag[i][2];

        len = uloc_toLanguageTag(inloc, langtag, sizeof(langtag), true, &status);
        if (U_FAILURE(status)) {
            if (expected != NULL) {
                log_data_err("Error returned by uloc_toLanguageTag {strict} for locale id [%s] - error: %s Are you missing data?\n",
                    inloc, u_errorName(status));
            }
        } else {
            if (expected == NULL) {
                log_err("Error should be returned by uloc_toLanguageTag {strict} for locale id [%s], but [%s] is returned without errors\n",
                    inloc, langtag);
            } else if (uprv_strcmp(langtag, expected) != 0) {
                log_err("uloc_toLanguageTag {strict} returned language tag [%s] for input locale [%s] - expected: [%s]\n",
                    langtag, inloc, expected);
            }
        }
    }
}

static void TestBug20132(void) {
    char langtag[256];
    UErrorCode status;
    int32_t len;

    static const char inloc[] = "en-C";
    static const char expected[] = "en-x-lvariant-c";
    const int32_t expected_len = (int32_t)uprv_strlen(expected);

    /* Before ICU-20132 was fixed, calling uloc_toLanguageTag() with a too small
     * buffer would not immediately return the buffer size actually needed, but
     * instead require several iterations before getting the correct size. */

    status = U_ZERO_ERROR;
    len = uloc_toLanguageTag(inloc, langtag, 1, false, &status);

    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        log_data_err("Error returned by uloc_toLanguageTag for locale id [%s] - error: %s Are you missing data?\n",
            inloc, u_errorName(status));
    }

    if (len != expected_len) {
        log_err("Bad length returned by uloc_toLanguageTag for locale id [%s]: %i != %i\n", inloc, len, expected_len);
    }

    status = U_ZERO_ERROR;
    len = uloc_toLanguageTag(inloc, langtag, expected_len, false, &status);

    if (U_FAILURE(status)) {
        log_data_err("Error returned by uloc_toLanguageTag for locale id [%s] - error: %s Are you missing data?\n",
            inloc, u_errorName(status));
    }

    if (len != expected_len) {
        log_err("Bad length returned by uloc_toLanguageTag for locale id [%s]: %i != %i\n", inloc, len, expected_len);
    } else if (uprv_strncmp(langtag, expected, expected_len) != 0) {
        log_data_err("uloc_toLanguageTag returned language tag [%.*s] for input locale [%s] - expected: [%s]. Are you missing data?\n",
            len, langtag, inloc, expected);
    }
}

#define FULL_LENGTH -1
static const struct {
    const char  *bcpID;
    const char  *locID;
    int32_t     len;
} langtag_to_locale[] = {
    {"en",                  "en",                   FULL_LENGTH},
    {"en-us",               "en_US",                FULL_LENGTH},
    {"und-US",              "_US",                  FULL_LENGTH},
    {"und-latn",            "_Latn",                FULL_LENGTH},
    {"en-US-posix",         "en_US_POSIX",          FULL_LENGTH},
    {"de-de_euro",          "de",                   2},
    {"kok-IN",              "kok_IN",               FULL_LENGTH},
    {"123",                 "",                     0},
    {"en_us",               "",                     0},
    {"en-latn-x",           "en_Latn",              7},
    {"art-lojban",          "jbo",                  FULL_LENGTH},
    {"zh-hakka",            "hak",                  FULL_LENGTH},
    {"zh-cmn-CH",           "cmn_CH",               FULL_LENGTH},
    {"zh-cmn-CH-u-co-pinyin", "cmn_CH@collation=pinyin", FULL_LENGTH},
    {"xxx-yy",              "xxx_YY",               FULL_LENGTH},
    {"fr-234",              "fr_234",               FULL_LENGTH},
    {"i-default",           "en@x=i-default",       FULL_LENGTH},
    {"i-test",              "",                     0},
    {"ja-jp-jp",            "ja_JP",                5},
    {"bogus",               "bogus",                FULL_LENGTH},
    {"boguslang",           "",                     0},
    {"EN-lATN-us",          "en_Latn_US",           FULL_LENGTH},
    {"und-variant-1234",    "__1234_VARIANT",       FULL_LENGTH}, /* ICU-20478 */
    {"ja-9876-5432",    "ja__5432_9876",       FULL_LENGTH}, /* ICU-20478 */
    {"en-US-varianta-variantb",    "en_US_VARIANTA_VARIANTB",       FULL_LENGTH}, /* ICU-20478 */
    {"en-US-variantb-varianta",    "en_US_VARIANTA_VARIANTB",       FULL_LENGTH}, /* ICU-20478 */
    {"sl-rozaj-1994-biske",    "sl__1994_BISKE_ROZAJ",       FULL_LENGTH}, /* ICU-20478 */
    {"sl-biske-1994-rozaj",    "sl__1994_BISKE_ROZAJ",       FULL_LENGTH}, /* ICU-20478 */
    {"sl-1994-rozaj-biske",    "sl__1994_BISKE_ROZAJ",       FULL_LENGTH}, /* ICU-20478 */
    {"sl-rozaj-biske-1994",    "sl__1994_BISKE_ROZAJ",       FULL_LENGTH}, /* ICU-20478 */
    {"en-fonipa-scouse",    "en__FONIPA_SCOUSE",       FULL_LENGTH}, /* ICU-20478 */
    {"en-scouse-fonipa",    "en__FONIPA_SCOUSE",       FULL_LENGTH}, /* ICU-20478 */
    {"und-varzero-var1-vartwo", "__VARZERO",        11},
    {"en-u-ca-gregory",     "en@calendar=gregorian",    FULL_LENGTH},
    {"en-U-cu-USD",         "en@currency=usd",      FULL_LENGTH},
    {"en-US-u-va-posix",    "en_US_POSIX",          FULL_LENGTH},
    {"en-us-u-ca-gregory-va-posix", "en_US_POSIX@calendar=gregorian",   FULL_LENGTH},
    {"en-us-posix-u-va-posix",   "en_US_POSIX@va=posix",    FULL_LENGTH},
    {"en-us-u-va-posix2",        "en_US@va=posix2",         FULL_LENGTH},
    {"en-us-vari1-u-va-posix",   "en_US_VARI1@va=posix",    FULL_LENGTH},
    {"ar-x-1-2-3",          "ar@x=1-2-3",           FULL_LENGTH},
    {"fr-u-nu-latn-cu-eur", "fr@currency=eur;numbers=latn", FULL_LENGTH},
    {"de-k-kext-u-co-phonebk-nu-latn",  "de@collation=phonebook;k=kext;numbers=latn",   FULL_LENGTH},
    {"ja-u-cu-jpy-ca-jp",   "ja@calendar=yes;currency=jpy;jp=yes",  FULL_LENGTH},
    {"en-us-u-tz-usnyc",    "en_US@timezone=America/New_York",  FULL_LENGTH},
    {"und-a-abc-def",       "und@a=abc-def",        FULL_LENGTH},
    {"zh-u-ca-chinese-x-u-ca-chinese",  "zh@calendar=chinese;x=u-ca-chinese",   FULL_LENGTH},
    {"x-elmer",             "@x=elmer",             FULL_LENGTH},
    {"en-US-u-attr1-attr2-ca-gregory", "en_US@attribute=attr1-attr2;calendar=gregorian",    FULL_LENGTH},
    {"sr-u-kn",             "sr@colnumeric=yes",    FULL_LENGTH},
    {"de-u-kn-co-phonebk",  "de@collation=phonebook;colnumeric=yes",    FULL_LENGTH},
    {"en-u-attr2-attr1-kn-kb",  "en@attribute=attr1-attr2;colbackwards=yes;colnumeric=yes", FULL_LENGTH},
    {"ja-u-ijkl-efgh-abcd-ca-japanese-xx-yyy-zzz-kn",   "ja@attribute=abcd-efgh-ijkl;calendar=japanese;colnumeric=yes;xx=yyy-zzz",  FULL_LENGTH},
    {"de-u-xc-xphonebk-co-phonebk-ca-buddhist-mo-very-lo-extensi-xd-that-de-should-vc-probably-xz-killthebuffer",
     "de@calendar=buddhist;collation=phonebook;de=should;lo=extensi;mo=very;vc=probably;xc=xphonebk;xd=that;xz=yes", 91},
    {"de-1901-1901", "de__1901", 7},
    {"de-DE-1901-1901", "de_DE_1901", 10},
    {"en-a-bbb-a-ccc", "en@a=bbb", 8},
    /* #12761 */
    {"en-a-bar-u-baz",      "en@a=bar;attribute=baz",   FULL_LENGTH},
    {"en-a-bar-u-baz-x-u-foo",  "en@a=bar;attribute=baz;x=u-foo",   FULL_LENGTH},
    {"en-u-baz",            "en@attribute=baz",     FULL_LENGTH},
    {"en-u-baz-ca-islamic-civil",   "en@attribute=baz;calendar=islamic-civil",  FULL_LENGTH},
    {"en-a-bar-u-ca-islamic-civil-x-u-foo", "en@a=bar;calendar=islamic-civil;x=u-foo",  FULL_LENGTH},
    {"en-a-bar-u-baz-ca-islamic-civil-x-u-foo", "en@a=bar;attribute=baz;calendar=islamic-civil;x=u-foo",    FULL_LENGTH},
    {"und-Arab-u-em-emoji", "_Arab@em=emoji", FULL_LENGTH},
    {"und-Latn-u-em-emoji", "_Latn@em=emoji", FULL_LENGTH},
    {"und-Latn-DE-u-em-emoji", "_Latn_DE@em=emoji", FULL_LENGTH},
    {"und-Zzzz-DE-u-em-emoji", "_Zzzz_DE@em=emoji", FULL_LENGTH},
    {"und-DE-u-em-emoji", "_DE@em=emoji", FULL_LENGTH},
    // #20098
    {"hant-cmn-cn", "hant", 4},
    {"zh-cmn-TW", "cmn_TW", FULL_LENGTH},
    {"zh-x_t-ab", "zh", 2},
    {"zh-hans-cn-u-ca-x_t-u", "zh_Hans_CN@calendar=yes", 15},
    /* #20140 dupe keys in U-extension */
    {"zh-u-ca-chinese-ca-gregory", "zh@calendar=chinese", FULL_LENGTH},
    {"zh-u-ca-gregory-co-pinyin-ca-chinese", "zh@calendar=gregorian;collation=pinyin", FULL_LENGTH},
    {"de-latn-DE-1901-u-co-phonebk-co-pinyin-ca-gregory", "de_Latn_DE_1901@calendar=gregorian;collation=phonebook", FULL_LENGTH},
    {"th-u-kf-nu-thai-kf-false", "th@colcasefirst=yes;numbers=thai", FULL_LENGTH},
    /* #9562 IANA language tag data update */
    {"en-gb-oed", "en_GB_OXENDICT", FULL_LENGTH},
    {"i-navajo", "nv", FULL_LENGTH},
    {"i-navajo-a-foo", "nv@a=foo", FULL_LENGTH},
    {"i-navajo-latn-us", "nv_Latn_US", FULL_LENGTH},
    {"sgn-br", "bzs", FULL_LENGTH},
    {"sgn-br-u-co-phonebk", "bzs@collation=phonebook", FULL_LENGTH},
    {"ja-latn-hepburn-heploc", "ja_Latn__ALALC97", FULL_LENGTH},
    {"ja-latn-hepburn-heploc-u-ca-japanese", "ja_Latn__ALALC97@calendar=japanese", FULL_LENGTH},
    {"en-a-bcde-0-fgh", "en@0=fgh;a=bcde", FULL_LENGTH},
};

static void TestForLanguageTag(void) {
    char locale[256];
    int32_t i;
    UErrorCode status;
    int32_t parsedLen;
    int32_t expParsedLen;

    for (i = 0; i < UPRV_LENGTHOF(langtag_to_locale); i++) {
        status = U_ZERO_ERROR;
        locale[0] = 0;
        expParsedLen = langtag_to_locale[i].len;
        if (expParsedLen == FULL_LENGTH) {
            expParsedLen = (int32_t)uprv_strlen(langtag_to_locale[i].bcpID);
        }
        uloc_forLanguageTag(langtag_to_locale[i].bcpID, locale, sizeof(locale), &parsedLen, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "Error returned by uloc_forLanguageTag for language tag [%s] - error: %s\n",
                langtag_to_locale[i].bcpID, u_errorName(status));
        } else {
            if (uprv_strcmp(langtag_to_locale[i].locID, locale) != 0) {
                log_data_err("uloc_forLanguageTag returned locale [%s] for input language tag [%s] - expected: [%s]\n",
                    locale, langtag_to_locale[i].bcpID, langtag_to_locale[i].locID);
            }
            if (parsedLen != expParsedLen) {
                log_err("uloc_forLanguageTag parsed length of %d for input language tag [%s] - expected parsed length: %d\n",
                    parsedLen, langtag_to_locale[i].bcpID, expParsedLen);
            }
        }
    }
}

static const struct {
    const char  *input;
    const char  *canonical;
} langtag_to_canonical[] = {
    {"de-DD", "de-DE"},
    {"de-DD-u-co-phonebk", "de-DE-u-co-phonebk"},
    {"jw-id", "jv-ID"},
    {"jw-id-u-ca-islamic-civil", "jv-ID-u-ca-islamic-civil"},
    {"mo-md", "ro-MD"},
    {"my-bu-u-nu-mymr", "my-MM-u-nu-mymr"},
    {"yuu-ru", "yug-RU"},
};


static void TestLangAndRegionCanonicalize(void) {
    char locale[256];
    char canonical[256];
    int32_t i;
    UErrorCode status;
    for (i = 0; i < UPRV_LENGTHOF(langtag_to_canonical); i++) {
        status = U_ZERO_ERROR;
        const char* input = langtag_to_canonical[i].input;
        uloc_forLanguageTag(input, locale, sizeof(locale), NULL, &status);
        uloc_toLanguageTag(locale, canonical, sizeof(canonical), true, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "Error returned by uloc_forLanguageTag or uloc_toLanguageTag "
                           "for language tag [%s] - error: %s\n", input, u_errorName(status));
        } else {
            const char* expected_canonical = langtag_to_canonical[i].canonical;
            if (uprv_strcmp(expected_canonical, canonical) != 0) {
                log_data_err("input language tag [%s] is canonicalized to [%s] - expected: [%s]\n",
                    input, canonical, expected_canonical);
            }
        }
    }
}

static void TestToUnicodeLocaleKey(void)
{
    /* $IN specifies the result should be the input pointer itself */
    static const char* DATA[][2] = {
        {"calendar",    "ca"},
        {"CALEndar",    "ca"},  /* difference casing */
        {"ca",          "ca"},  /* bcp key itself */
        {"kv",          "kv"},  /* no difference between legacy and bcp */
        {"foo",         NULL},  /* unknown, bcp ill-formed */
        {"ZZ",          "$IN"}, /* unknown, bcp well-formed -  */
        {NULL,          NULL}
    };

    int32_t i;
    for (i = 0; DATA[i][0] != NULL; i++) {
        const char* keyword = DATA[i][0];
        const char* expected = DATA[i][1];
        const char* bcpKey = NULL;

        bcpKey = uloc_toUnicodeLocaleKey(keyword);
        if (expected == NULL) {
            if (bcpKey != NULL) {
                log_err("toUnicodeLocaleKey: keyword=%s => %s, expected=NULL\n", keyword, bcpKey);
            }
        } else if (bcpKey == NULL) {
            log_data_err("toUnicodeLocaleKey: keyword=%s => NULL, expected=%s\n", keyword, expected);
        } else if (uprv_strcmp(expected, "$IN") == 0) {
            if (bcpKey != keyword) {
                log_err("toUnicodeLocaleKey: keyword=%s => %s, expected=%s(input pointer)\n", keyword, bcpKey, keyword);
            }
        } else if (uprv_strcmp(bcpKey, expected) != 0) {
            log_err("toUnicodeLocaleKey: keyword=%s => %s, expected=%s\n", keyword, bcpKey, expected);
        }
    }
}

static void TestBug20321UnicodeLocaleKey(void)
{
    // key = alphanum alpha ;
    static const char* invalid[] = {
        "a0",
        "00",
        "a@",
        "0@",
        "@a",
        "@a",
        "abc",
        "0bc",
    };
    for (int i = 0; i < UPRV_LENGTHOF(invalid); i++) {
        const char* bcpKey = NULL;
        bcpKey = uloc_toUnicodeLocaleKey(invalid[i]);
        if (bcpKey != NULL) {
            log_err("toUnicodeLocaleKey: keyword=%s => %s, expected=NULL\n", invalid[i], bcpKey);
        }
    }
    static const char* valid[] = {
        "aa",
        "0a",
    };
    for (int i = 0; i < UPRV_LENGTHOF(valid); i++) {
        const char* bcpKey = NULL;
        bcpKey = uloc_toUnicodeLocaleKey(valid[i]);
        if (bcpKey == NULL) {
            log_err("toUnicodeLocaleKey: keyword=%s => NULL, expected!=NULL\n", valid[i]);
        }
    }
}

static void TestToLegacyKey(void)
{
    /* $IN specifies the result should be the input pointer itself */
    static const char* DATA[][2] = {
        {"kb",          "colbackwards"},
        {"kB",          "colbackwards"},    /* different casing */
        {"Collation",   "collation"},   /* keyword itself with different casing */
        {"kv",          "kv"},  /* no difference between legacy and bcp */
        {"foo",         "$IN"}, /* unknown, bcp ill-formed */
        {"ZZ",          "$IN"}, /* unknown, bcp well-formed */
        {"e=mc2",       NULL},  /* unknown, bcp/legacy ill-formed */
        {NULL,          NULL}
    };

    int32_t i;
    for (i = 0; DATA[i][0] != NULL; i++) {
        const char* keyword = DATA[i][0];
        const char* expected = DATA[i][1];
        const char* legacyKey = NULL;

        legacyKey = uloc_toLegacyKey(keyword);
        if (expected == NULL) {
            if (legacyKey != NULL) {
                log_err("toLegacyKey: keyword=%s => %s, expected=NULL\n", keyword, legacyKey);
            }
        } else if (legacyKey == NULL) {
            log_err("toLegacyKey: keyword=%s => NULL, expected=%s\n", keyword, expected);
        } else if (uprv_strcmp(expected, "$IN") == 0) {
            if (legacyKey != keyword) {
                log_err("toLegacyKey: keyword=%s => %s, expected=%s(input pointer)\n", keyword, legacyKey, keyword);
            }
        } else if (uprv_strcmp(legacyKey, expected) != 0) {
            log_data_err("toUnicodeLocaleKey: keyword=%s, %s, expected=%s\n", keyword, legacyKey, expected);
        }
    }
}

static void TestToUnicodeLocaleType(void)
{
    /* $IN specifies the result should be the input pointer itself */
    static const char* DATA[][3] = {
        {"tz",              "Asia/Kolkata",     "inccu"},
        {"calendar",        "gregorian",        "gregory"},
        {"ca",              "gregorian",        "gregory"},
        {"ca",              "Gregorian",        "gregory"},
        {"ca",              "buddhist",         "buddhist"},
        {"Calendar",        "Japanese",         "japanese"},
        {"calendar",        "Islamic-Civil",    "islamic-civil"},
        {"calendar",        "islamicc",         "islamic-civil"},   /* bcp type alias */
        {"colalternate",    "NON-IGNORABLE",    "noignore"},
        {"colcaselevel",    "yes",              "true"},
        {"rg",              "GBzzzz",           "$IN"},
        {"tz",              "america/new_york", "usnyc"},
        {"tz",              "Asia/Kolkata",     "inccu"},
        {"timezone",        "navajo",           "usden"},
        {"ca",              "aaaa",             "$IN"},     /* unknown type, well-formed type */
        {"ca",              "gregory-japanese-islamic", "$IN"}, /* unknown type, well-formed type */
        {"zz",              "gregorian",        NULL},      /* unknown key, ill-formed type */
        {"co",              "foo-",             NULL},      /* unknown type, ill-formed type */
        {"variableTop",     "00A0",             "$IN"},     /* valid codepoints type */
        {"variableTop",     "wxyz",             "$IN"},     /* invalid codepoints type - return as is for now */
        {"kr",              "space-punct",      "space-punct"}, /* valid reordercode type */
        {"kr",              "digit-spacepunct", NULL},      /* invalid (bcp ill-formed) reordercode type */
        {NULL,              NULL,               NULL}
    };

    int32_t i;
    for (i = 0; DATA[i][0] != NULL; i++) {
        const char* keyword = DATA[i][0];
        const char* value = DATA[i][1];
        const char* expected = DATA[i][2];
        const char* bcpType = NULL;

        bcpType = uloc_toUnicodeLocaleType(keyword, value);
        if (expected == NULL) {
            if (bcpType != NULL) {
                log_err("toUnicodeLocaleType: keyword=%s, value=%s => %s, expected=NULL\n", keyword, value, bcpType);
            }
        } else if (bcpType == NULL) {
            log_data_err("toUnicodeLocaleType: keyword=%s, value=%s => NULL, expected=%s\n", keyword, value, expected);
        } else if (uprv_strcmp(expected, "$IN") == 0) {
            if (bcpType != value) {
                log_err("toUnicodeLocaleType: keyword=%s, value=%s => %s, expected=%s(input pointer)\n", keyword, value, bcpType, value);
            }
        } else if (uprv_strcmp(bcpType, expected) != 0) {
            log_data_err("toUnicodeLocaleType: keyword=%s, value=%s => %s, expected=%s\n", keyword, value, bcpType, expected);
        }
    }
}

static void TestToLegacyType(void)
{
    /* $IN specifies the result should be the input pointer itself */
    static const char* DATA[][3] = {
        {"calendar",        "gregory",          "gregorian"},
        {"ca",              "gregory",          "gregorian"},
        {"ca",              "Gregory",          "gregorian"},
        {"ca",              "buddhist",         "buddhist"},
        {"Calendar",        "Japanese",         "japanese"},
        {"calendar",        "Islamic-Civil",    "islamic-civil"},
        {"calendar",        "islamicc",         "islamic-civil"},   /* bcp type alias */
        {"colalternate",    "noignore",         "non-ignorable"},
        {"colcaselevel",    "true",             "yes"},
        {"rg",              "gbzzzz",           "gbzzzz"},
        {"tz",              "usnyc",            "America/New_York"},
        {"tz",              "inccu",            "Asia/Calcutta"},
        {"timezone",        "usden",            "America/Denver"},
        {"timezone",        "usnavajo",         "America/Denver"},  /* bcp type alias */
        {"colstrength",     "quarternary",      "quaternary"},  /* type alias */
        {"ca",              "aaaa",             "$IN"}, /* unknown type */
        {"calendar",        "gregory-japanese-islamic", "$IN"}, /* unknown type, well-formed type */
        {"zz",              "gregorian",        "$IN"}, /* unknown key, bcp ill-formed type */
        {"ca",              "gregorian-calendar",   "$IN"}, /* known key, bcp ill-formed type */
        {"co",              "e=mc2",            NULL},  /* known key, ill-formed bcp/legacy type */
        {"variableTop",     "00A0",             "$IN"},     /* valid codepoints type */
        {"variableTop",     "wxyz",             "$IN"},    /* invalid codepoints type - return as is for now */
        {"kr",              "space-punct",      "space-punct"}, /* valid reordercode type */
        {"kr",              "digit-spacepunct", "digit-spacepunct"},    /* invalid reordercode type, but ok for legacy syntax */
        {NULL,              NULL,               NULL}
    };

    int32_t i;
    for (i = 0; DATA[i][0] != NULL; i++) {
        const char* keyword = DATA[i][0];
        const char* value = DATA[i][1];
        const char* expected = DATA[i][2];
        const char* legacyType = NULL;

        legacyType = uloc_toLegacyType(keyword, value);
        if (expected == NULL) {
            if (legacyType != NULL) {
                log_err("toLegacyType: keyword=%s, value=%s => %s, expected=NULL\n", keyword, value, legacyType);
            }
        } else if (legacyType == NULL) {
            log_err("toLegacyType: keyword=%s, value=%s => NULL, expected=%s\n", keyword, value, expected);
        } else if (uprv_strcmp(expected, "$IN") == 0) {
            if (legacyType != value) {
                log_err("toLegacyType: keyword=%s, value=%s => %s, expected=%s(input pointer)\n", keyword, value, legacyType, value);
            }
        } else if (uprv_strcmp(legacyType, expected) != 0) {
            log_data_err("toLegacyType: keyword=%s, value=%s => %s, expected=%s\n", keyword, value, legacyType, expected);
        } else {
            log_verbose("toLegacyType: keyword=%s, value=%s => %s\n", keyword, value, legacyType);
        }
    }
}



static void test_unicode_define(const char *namech, char ch,
                                const char *nameu, UChar uch)
{
    UChar asUch[1];
    asUch[0]=0;
    log_verbose("Testing whether %s[\\x%02x,'%c'] == %s[U+%04X]\n",
                namech, ch,(int)ch, nameu, (int) uch);
    u_charsToUChars(&ch, asUch, 1);
    if(asUch[0] != uch) {
        log_err("FAIL:  %s[\\x%02x,'%c'] maps to U+%04X, but %s = U+%04X\n",
                namech, ch, (int)ch, (int)asUch[0], nameu, (int)uch);
    } else {
        log_verbose(" .. OK, == U+%04X\n", (int)asUch[0]);
    }
}

static void checkTerminating(const char* locale, const char* inLocale)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t preflight_length = uloc_getDisplayName(
        locale, inLocale, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        log_err("uloc_getDisplayName(%s, %s) preflight failed",
                locale, inLocale);
    }
    UChar buff[256];
    const UChar sentinel1 = 0x6C38; // 永- a Han unicode as sentinel.
    const UChar sentinel2 = 0x92D2; // 鋒- a Han unicode as sentinel.

    // 1. Test when we set the maxResultSize to preflight_length + 1.
    // Set sentinel1 in the buff[preflight_length-1] to check it will be
    // replaced with display name.
    buff[preflight_length-1] = sentinel1;
    // Set sentinel2 in the buff[preflight_length] to check it will be
    // replaced by null.
    buff[preflight_length] = sentinel2;
    // It should be properly null terminated at buff[preflight_length].
    status = U_ZERO_ERROR;
    int32_t length = uloc_getDisplayName(
        locale, inLocale, buff, preflight_length + 1, &status);
    const char* result = U_SUCCESS(status) ?
        aescstrdup(buff, length) : "(undefined when failure)";
    if (length != preflight_length) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length+1 returns "
                "length %d different from preflight length %d. Returns '%s'\n",
                locale, inLocale, length, preflight_length, result);
    }
    if (U_ZERO_ERROR != status) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length+1 should "
                "set status to U_ZERO_ERROR but got %d %s. Returns %s\n",
                locale, inLocale, status, myErrorName(status), result);
    }
    if (buff[length-1] == sentinel1) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length+1 does "
                "not change memory in the end of buffer while it should. "
                "Returns %s\n",
                locale, inLocale, result);
    }
    if (buff[length] != 0x0000) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length+1 should "
                "null terminate at buff[length] but does not %x. Returns %s\n",
                locale, inLocale, buff[length], result);
    }

    // 2. Test when we only set the maxResultSize to preflight_length.

    // Set sentinel1 in the buff[preflight_length-1] to check it will be
    // replaced with display name.
    buff[preflight_length-1] = sentinel1;
    // Set sentinel2 in the buff[preflight_length] to check it won't be replaced
    // by null.
    buff[preflight_length] = sentinel2;
    status = U_ZERO_ERROR;
    length = uloc_getDisplayName(
        locale, inLocale, buff, preflight_length, &status);
    result = U_SUCCESS(status) ?
        aescstrdup(buff, length) : "(undefined when failure)";

    if (length != preflight_length) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length return "
                "length %d different from preflight length %d. Returns '%s'\n",
                locale, inLocale, length, preflight_length, result);
    }
    if (U_STRING_NOT_TERMINATED_WARNING != status) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length should "
                "set status to U_STRING_NOT_TERMINATED_WARNING but got %d %s. "
                "Returns %s\n",
                locale, inLocale, status, myErrorName(status), result);
    }
    if (buff[length-1] == sentinel1) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length does not "
                "change memory in the end of buffer while it should. Returns "
                "'%s'\n",
                locale, inLocale, result);
    }
    if (buff[length] != sentinel2) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length change "
                "memory beyond maxResultSize to %x. Returns '%s'\n",
                locale, inLocale, buff[length], result);
    }
    if (buff[preflight_length - 1] == 0x0000) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length null "
                "terminated while it should not. Return '%s'\n",
                locale, inLocale, result);
    }

    // 3. Test when we only set the maxResultSize to preflight_length-1.
    // Set sentinel1 in the buff[preflight_length-1] to check it will not be
    // replaced with display name.
    buff[preflight_length-1] = sentinel1;
    // Set sentinel2 in the buff[preflight_length] to check it won't be replaced
    // by null.
    buff[preflight_length] = sentinel2;
    status = U_ZERO_ERROR;
    length = uloc_getDisplayName(
        locale, inLocale, buff, preflight_length - 1, &status);
    result = U_SUCCESS(status) ?
        aescstrdup(buff, length) : "(undefined when failure)";

    if (length != preflight_length) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length-1 return "
                "length %d different from preflight length %d. Returns '%s'\n",
                locale, inLocale, length, preflight_length, result);
    }
    if (U_BUFFER_OVERFLOW_ERROR != status) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length-1 should "
                "set status to U_BUFFER_OVERFLOW_ERROR but got %d %s. "
                "Returns %s\n",
                locale, inLocale, status, myErrorName(status), result);
    }
    if (buff[length-1] != sentinel1) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length-1 should "
                "not change memory in beyond the maxResultSize. Returns '%s'\n",
                locale, inLocale, result);
    }
    if (buff[length] != sentinel2) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length-1 change "
                "memory beyond maxResultSize to %x. Returns '%s'\n",
                locale, inLocale, buff[length], result);
    }
    if (buff[preflight_length - 2] == 0x0000) {
        log_err("uloc_getDisplayName(%s, %s) w/ maxResultSize=length-1 null "
                "terminated while it should not. Return '%s'\n",
                locale, inLocale, result);
    }
}

static void Test21157CorrectTerminating(void) {
    checkTerminating("fr", "fr");
    checkTerminating("fr_BE", "fr");
    checkTerminating("fr_Latn_BE", "fr");
    checkTerminating("fr_Latn", "fr");
    checkTerminating("fr", "fr");
    checkTerminating("fr-CN", "fr");
    checkTerminating("fr-Hant-CN", "fr");
    checkTerminating("fr-Hant", "fr");
    checkTerminating("zh-u-co-pinyin", "fr");
}

#define TEST_UNICODE_DEFINE(x,y) test_unicode_define(#x, (char)(x), #y, (UChar)(y))

static void TestUnicodeDefines(void) {
  TEST_UNICODE_DEFINE(ULOC_KEYWORD_SEPARATOR, ULOC_KEYWORD_SEPARATOR_UNICODE);
  TEST_UNICODE_DEFINE(ULOC_KEYWORD_ASSIGN, ULOC_KEYWORD_ASSIGN_UNICODE);
  TEST_UNICODE_DEFINE(ULOC_KEYWORD_ITEM_SEPARATOR, ULOC_KEYWORD_ITEM_SEPARATOR_UNICODE);
}

static void TestIsRightToLeft() {
    // API test only. More test cases in intltest/LocaleTest.
    if(uloc_isRightToLeft("root") || !uloc_isRightToLeft("EN-HEBR")) {
        log_err("uloc_isRightToLeft() failed");
    }
    // ICU-22466 Make sure no crash when locale is bogus
    uloc_isRightToLeft(
        "uF-Vd_u-VaapoPos-u1-Pos-u1-Pos-u1-Pos-u1-oPos-u1-Pufu1-PuosPos-u1-Pos-u1-Pos-u1-Pzghu1-Pos-u1-PoP-u1@osus-u1");
    uloc_isRightToLeft("-Xa");
}

typedef struct {
    const char * badLocaleID;
    const char * displayLocale;
    const char * expectedName;
    UErrorCode   expectedStatus;
} BadLocaleItem;

static const BadLocaleItem badLocaleItems[] = {
#if APPLE_ICU_CHANGES
// rdar://
    { "-9223372036854775808", "en", "9223372036854775808", U_USING_DEFAULT_WARNING },
#else
    { "-9223372036854775808", "en", "Unknown language (9223372036854775808)", U_USING_DEFAULT_WARNING },
#endif  // APPLE_ICU_CHANGES
    /* add more in the future */
    { NULL, NULL, NULL, U_ZERO_ERROR } /* terminator */
};

enum { kUBufDispNameMax = 128, kBBufDispNameMax = 256 };

static void TestBadLocaleIDs() {
    const BadLocaleItem* itemPtr;
    for (itemPtr = badLocaleItems; itemPtr->badLocaleID != NULL; itemPtr++) {
        UChar ubufExpect[kUBufDispNameMax], ubufGet[kUBufDispNameMax];
        UErrorCode status = U_ZERO_ERROR;
        int32_t ulenExpect = u_unescape(itemPtr->expectedName, ubufExpect, kUBufDispNameMax);
        int32_t ulenGet = uloc_getDisplayName(itemPtr->badLocaleID, itemPtr->displayLocale, ubufGet, kUBufDispNameMax, &status);
        if (status != itemPtr->expectedStatus ||
                (U_SUCCESS(status) && (ulenGet != ulenExpect || u_strncmp(ubufGet, ubufExpect, ulenExpect) != 0))) {
            char bbufExpect[kBBufDispNameMax], bbufGet[kBBufDispNameMax];
            u_austrncpy(bbufExpect, ubufExpect, ulenExpect);
            u_austrncpy(bbufGet, ubufGet, ulenGet);
            log_err("FAIL: For localeID %s, displayLocale %s, calling uloc_getDisplayName:\n"
                    "    expected status %-26s, name (len %2d): %s\n"
                    "    got      status %-26s, name (len %2d): %s\n",
                    itemPtr->badLocaleID, itemPtr->displayLocale,
                    u_errorName(itemPtr->expectedStatus), ulenExpect, bbufExpect,
                    u_errorName(status), ulenGet, bbufGet );
        }
    }
}

// Test case for ICU-20370.
// The issue shows as an Address Sanitizer failure.
static void TestBug20370() {
    const char *localeID = "x-privatebutreallylongtagfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobar";
    uint32_t lcid = uloc_getLCID(localeID);
    if (lcid != 0) {
        log_err("FAIL: Expected LCID value of 0 for invalid localeID input.");
    }
}

// Test case for ICU-20149
// Handle the duplicate U extension attribute
static void TestBug20149() {
    const char *localeID = "zh-u-foo-foo-co-pinyin";
    char locale[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t parsedLen;
    locale[0] = '\0';
    uloc_forLanguageTag(localeID, locale, sizeof(locale), &parsedLen, &status);
    if (U_FAILURE(status) ||
        0 !=strcmp("zh@attribute=foo;collation=pinyin", locale)) {
        log_err("ERROR: in uloc_forLanguageTag %s return %s\n", myErrorName(status), locale);
    }
}

#if !UCONFIG_NO_FORMATTING
typedef enum UldnNameType {
    TEST_ULDN_LOCALE,
    TEST_ULDN_LANGUAGE,
    TEST_ULDN_SCRIPT,
    TEST_ULDN_REGION,
    TEST_ULOC_LOCALE,   // only valid with optStdMidLong
    TEST_ULOC_LANGUAGE, // only valid with optStdMidLong
    TEST_ULOC_SCRIPT,   // only valid with optStdMidLong
    TEST_ULOC_REGION,   // only valid with optStdMidLong
} UldnNameType;

typedef struct {
    const char * localeToName; // NULL to terminate a list of these
    UldnNameType nameType;
    const UChar * expectResult;
} UldnItem;

typedef struct {
    const char *            displayLocale;
    const UDisplayContext * displayOptions; // set of 3 UDisplayContext items
    const UldnItem *        testItems;
    int32_t                 countItems;
} UldnLocAndOpts;

static const UDisplayContext optStdMidLong[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL};
static const UDisplayContext optStdMidShrt[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_SHORT};
#if APPLE_ICU_CHANGES
// rdar://
static const UDisplayContext optStdMidVrnt[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_VARIANT};
static const UDisplayContext optStdMidPrc[3]  = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_PRC}; // rdar://115264744
static const UDisplayContext optStdLstPrc[3]  = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_PRC}; // rdar://120926070
#endif  // APPLE_ICU_CHANGES
static const UDisplayContext optDiaMidLong[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL};
static const UDisplayContext optDiaMidShrt[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_SHORT};

#if APPLE_ICU_CHANGES
// rdar://
static const UDisplayContext optStdBegLong[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL}; 
static const UDisplayContext optStdBegShrt[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_SHORT}; 
static const UDisplayContext optDiaBegLong[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL}; 
static const UDisplayContext optDiaBegShrt[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_SHORT}; 

static const UDisplayContext optStdLstLong[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL}; 
static const UDisplayContext optStdLstShrt[3] = {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_SHORT}; 
static const UDisplayContext optDiaLstLong[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL}; 
static const UDisplayContext optDiaLstShrt[3] = {UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_SHORT}; 
#endif  // APPLE_ICU_CHANGES

static const UldnItem en_StdMidLong[] = {
#if APPLE_ICU_CHANGES
// rdar://
	{ "en_US",                  TEST_ULDN_LOCALE, u"English (US)" },
	{ "en_US_POSIX",            TEST_ULDN_LOCALE, u"English (US, Computer)" },
	{ "en_US@calendar=chinese", TEST_ULDN_LOCALE, u"English (US, Chinese Calendar)" },
	{ "en_CA",                  TEST_ULDN_LOCALE, u"English (Canada)" },
#else
	{ "en_US",                  TEST_ULDN_LOCALE, u"English (United States)" },
#endif  // APPLE_ICU_CHANGES
	{ "en",                     TEST_ULDN_LANGUAGE, u"English" },
#if APPLE_ICU_CHANGES
// rdar://
#else
	{ "en_US",                  TEST_ULOC_LOCALE, u"English (United States)" },
#endif  // APPLE_ICU_CHANGES
	{ "en_US",                  TEST_ULOC_LANGUAGE, u"English" },
	{ "en",                     TEST_ULOC_LANGUAGE, u"English" },
#if APPLE_ICU_CHANGES
// rdar://
	{ "pt",                     TEST_ULDN_LOCALE, u"Portuguese" },
	{ "pt_BR",                  TEST_ULDN_LOCALE, u"Portuguese (Brazil)" },
	{ "pt_PT",                  TEST_ULDN_LOCALE, u"Portuguese (Portugal)" },
#endif  // APPLE_ICU_CHANGES
	// https://unicode-org.atlassian.net/browse/ICU-20870
	{ "fa_AF",                  TEST_ULDN_LOCALE, u"Persian (Afghanistan)" },
	{ "prs",                    TEST_ULDN_LOCALE, u"Dari" },
	{ "prs_AF",                 TEST_ULDN_LOCALE, u"Dari (Afghanistan)" },
	{ "prs_TJ",                 TEST_ULDN_LOCALE, u"Dari (Tajikistan)" },
	{ "prs",                    TEST_ULDN_LANGUAGE, u"Dari" },
	{ "prs",                    TEST_ULOC_LANGUAGE, u"Dari" },
	// https://unicode-org.atlassian.net/browse/ICU-21742
	{ "ji",                     TEST_ULDN_LOCALE, u"Yiddish" },
#if APPLE_ICU_CHANGES
// rdar://
	{ "ji_US",                  TEST_ULDN_LOCALE, u"Yiddish (US)" },
#else
	{ "ji_US",                  TEST_ULDN_LOCALE, u"Yiddish (United States)" },
#endif  // APPLE_ICU_CHANGES
	{ "ji",                     TEST_ULDN_LANGUAGE, u"Yiddish" },
	{ "ji_US",                  TEST_ULOC_LOCALE, u"Yiddish (United States)" },
	{ "ji",                     TEST_ULOC_LANGUAGE, u"Yiddish" },
	// https://unicode-org.atlassian.net/browse/ICU-11563
	{ "mo",                     TEST_ULDN_LOCALE, u"Romanian" },
	{ "mo_MD",                  TEST_ULDN_LOCALE, u"Romanian (Moldova)" },
	{ "mo",                     TEST_ULDN_LANGUAGE, u"Romanian" },
	{ "mo_MD",                  TEST_ULOC_LOCALE, u"Romanian (Moldova)" },
	{ "mo",                     TEST_ULOC_LANGUAGE, u"Romanian" },
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
// rdar://50687287 #20 add names for ks/pa/ur_Arab using script Naskh, force their use, remove redundant parens
// rdar://68351139 change names for region IO per GA, Legal, ET
	// Apple additions
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" },                  // rdar://50750364
	{ "zh_Hans_CN",             TEST_ULDN_LOCALE, u"Chinese, Simplified (China mainland)" }, // rdar://50750364
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"Chinese, Traditional" },                 // rdar://50750364
	{ "zh_Hant_HK",             TEST_ULDN_LOCALE, u"Chinese, Traditional (Hong Kong)" },     // rdar://50750364
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"Cantonese, Simplified" },                // rdar://50750364
	{ "yue_Hans_CN",            TEST_ULDN_LOCALE, u"Cantonese, Simplified (China mainland)" }, // rdar://50750364
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"Cantonese, Traditional" },               // rdar://50750364
	{ "yue_Hant_HK",            TEST_ULDN_LOCALE, u"Cantonese, Traditional (Hong Kong)" },   // rdar://50750364
	{ "zh_Hans@calendar=chinese",     TEST_ULDN_LOCALE, u"Chinese, Simplified (Chinese Calendar)" },                  // rdar://50750364
	{ "zh_Hans_CN@calendar=chinese",  TEST_ULDN_LOCALE, u"Chinese, Simplified (China mainland, Chinese Calendar)" }, // rdar://50750364
	{ "zh_Hant@calendar=chinese",     TEST_ULDN_LOCALE, u"Chinese, Traditional (Chinese Calendar)" },                 // rdar://50750364
	{ "zh_Hant_HK@calendar=chinese",  TEST_ULDN_LOCALE, u"Chinese, Traditional (Hong Kong, Chinese Calendar)" },     // rdar://50750364
	{ "yue_Hans@calendar=chinese",    TEST_ULDN_LOCALE, u"Cantonese, Simplified (Chinese Calendar)" },                // rdar://50750364
	{ "yue_Hans_CN@calendar=chinese", TEST_ULDN_LOCALE, u"Cantonese, Simplified (China mainland, Chinese Calendar)" }, // rdar://50750364
	{ "yue_Hant@calendar=chinese",    TEST_ULDN_LOCALE, u"Cantonese, Traditional (Chinese Calendar)" },               // rdar://50750364
	{ "yue_Hant_HK@calendar=chinese", TEST_ULDN_LOCALE, u"Cantonese, Traditional (Hong Kong, Chinese Calendar)" },   // rdar://50750364
	{ "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong)" },
	{ "Latn",                   TEST_ULDN_SCRIPT, u"Latin" },
	{ "Hans",                   TEST_ULDN_SCRIPT, u"Simplified Han" },
	{ "Hant",                   TEST_ULDN_SCRIPT, u"Traditional Han" },
	{ "US",                     TEST_ULDN_REGION, u"United States" },
	{ "CA",                     TEST_ULDN_REGION, u"Canada" },
	{ "GB",                     TEST_ULDN_REGION, u"United Kingdom" },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
	{ "MO",                     TEST_ULDN_REGION, u"Macao" },
	{ "IO",                     TEST_ULDN_REGION, u"Chagos Archipelago" }, // rdar://68351139 change names for region IO per GA, Legal, ET
	{ "ps_Arab",                TEST_ULDN_LOCALE, u"Pashto (Arabic)" },
	{ "ps_Arab_AF",             TEST_ULDN_LOCALE, u"Pashto (Arabic, Afghanistan)" },
	{ "ks_Arab",                TEST_ULDN_LOCALE, u"Kashmiri (Naskh)" }, // rdar://50687287
	{ "ks_Aran",                TEST_ULDN_LOCALE, u"Kashmiri (Nastaliq)" }, // rdar://47494884
	{ "ks_Arab_IN",             TEST_ULDN_LOCALE, u"Kashmiri (Naskh, India)" }, // rdar://50687287
	{ "ks_Aran_IN",             TEST_ULDN_LOCALE, u"Kashmiri (Nastaliq, India)" }, // rdar://47494884
	{ "pa_Arab",                TEST_ULDN_LOCALE, u"Punjabi (Naskh)" }, // rdar://50687287
	{ "pa_Aran",                TEST_ULDN_LOCALE, u"Punjabi (Nastaliq)" }, // rdar://50687287
	{ "pa_Arab_PK",             TEST_ULDN_LOCALE, u"Punjabi (Naskh, Pakistan)" }, // rdar://50687287
	{ "pa_Aran_PK",             TEST_ULDN_LOCALE, u"Punjabi (Nastaliq, Pakistan)" }, // rdar://50687287
	{ "ur_Arab",                TEST_ULDN_LOCALE, u"Urdu (Naskh)" }, // rdar://50687287
	{ "ur_Aran",                TEST_ULDN_LOCALE, u"Urdu (Nastaliq)" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULDN_LOCALE, u"Urdu (Naskh, Pakistan)" }, // rdar://50687287
	{ "ur_Aran_PK",             TEST_ULDN_LOCALE, u"Urdu (Nastaliq, Pakistan)" }, // rdar://47494884
	{ "ps_Arab@calendar=islamic",    TEST_ULDN_LOCALE, u"Pashto (Arabic, Hijri Calendar)" },
	{ "ps_Arab_AF@calendar=islamic", TEST_ULDN_LOCALE, u"Pashto (Arabic, Afghanistan, Hijri Calendar)" },
	{ "ks_Arab@calendar=islamic",    TEST_ULDN_LOCALE, u"Kashmiri (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "ks_Aran@calendar=islamic",    TEST_ULDN_LOCALE, u"Kashmiri (Nastaliq, Hijri Calendar)" }, // rdar://47494884
	{ "ks_Arab_IN@calendar=islamic", TEST_ULDN_LOCALE, u"Kashmiri (Naskh, India, Hijri Calendar)" }, // rdar://50687287
	{ "ks_Aran_IN@calendar=islamic", TEST_ULDN_LOCALE, u"Kashmiri (Nastaliq, India, Hijri Calendar)" }, // rdar://47494884
	{ "pa_Arab@calendar=islamic",    TEST_ULDN_LOCALE, u"Punjabi (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Aran@calendar=islamic",    TEST_ULDN_LOCALE, u"Punjabi (Nastaliq, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Arab_PK@calendar=islamic", TEST_ULDN_LOCALE, u"Punjabi (Naskh, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Aran_PK@calendar=islamic", TEST_ULDN_LOCALE, u"Punjabi (Nastaliq, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Arab@calendar=islamic",    TEST_ULDN_LOCALE, u"Urdu (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Aran@calendar=islamic",    TEST_ULDN_LOCALE, u"Urdu (Nastaliq, Hijri Calendar)" }, // rdar://47494884
	{ "ur_Arab_PK@calendar=islamic", TEST_ULDN_LOCALE, u"Urdu (Naskh, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Aran_PK@calendar=islamic", TEST_ULDN_LOCALE, u"Urdu (Nastaliq, Pakistan, Hijri Calendar)" }, // rdar://47494884
	{ "Arab",                   TEST_ULDN_SCRIPT, u"Arabic" },
	{ "Aran",                   TEST_ULDN_SCRIPT, u"Nastaliq" }, // rdar://47494884
	{ "Qaag",                   TEST_ULDN_SCRIPT, u"Zawgyi" },   // rdar://51471316
	{ "my_Qaag",                TEST_ULDN_LOCALE, u"Burmese (Zawgyi)" }, // rdar://51471316

	{ "zh_Hans",                TEST_ULOC_LOCALE, u"Chinese, Simplified" },                  // rdar://51418203
	{ "zh_Hans_CN",             TEST_ULOC_LOCALE, u"Chinese, Simplified (China mainland)" }, // rdar://51418203
	{ "zh_Hant",                TEST_ULOC_LOCALE, u"Chinese, Traditional" },                 // rdar://51418203
	{ "zh_Hant_HK",             TEST_ULOC_LOCALE, u"Chinese, Traditional (Hong Kong)" },     // rdar://51418203
	{ "yue_Hans",               TEST_ULOC_LOCALE, u"Cantonese, Simplified" },                // rdar://51418203
	{ "yue_Hans_CN",            TEST_ULOC_LOCALE, u"Cantonese, Simplified (China mainland)" }, // rdar://51418203
	{ "yue_Hant",               TEST_ULOC_LOCALE, u"Cantonese, Traditional" },               // rdar://51418203
	{ "yue_Hant_HK",            TEST_ULOC_LOCALE, u"Cantonese, Traditional (Hong Kong)" },   // rdar://51418203
	{ "zh_Hans@calendar=chinese",     TEST_ULOC_LOCALE, u"Chinese, Simplified (Chinese Calendar)" },                  // rdar://50750364
	{ "zh_Hans_CN@calendar=chinese",  TEST_ULOC_LOCALE, u"Chinese, Simplified (China mainland, Chinese Calendar)" }, // rdar://50750364
	{ "zh_Hant@calendar=chinese",     TEST_ULOC_LOCALE, u"Chinese, Traditional (Chinese Calendar)" },                 // rdar://50750364
	{ "zh_Hant_HK@calendar=chinese",  TEST_ULOC_LOCALE, u"Chinese, Traditional (Hong Kong, Chinese Calendar)" },     // rdar://50750364
	{ "yue_Hans@calendar=chinese",    TEST_ULOC_LOCALE, u"Cantonese, Simplified (Chinese Calendar)" },                // rdar://50750364
	{ "yue_Hans_CN@calendar=chinese", TEST_ULOC_LOCALE, u"Cantonese, Simplified (China mainland, Chinese Calendar)" }, // rdar://50750364
	{ "yue_Hant@calendar=chinese",    TEST_ULOC_LOCALE, u"Cantonese, Traditional (Chinese Calendar)" },               // rdar://50750364
	{ "yue_Hant_HK@calendar=chinese", TEST_ULOC_LOCALE, u"Cantonese, Traditional (Hong Kong, Chinese Calendar)" },   // rdar://50750364
	{ "ks_Arab",                TEST_ULOC_LOCALE, u"Kashmiri (Naskh)" }, // rdar://51418203
	{ "ks_Aran",                TEST_ULOC_LOCALE, u"Kashmiri (Nastaliq)" }, // rdar://47494884
	{ "ks_Arab_IN",             TEST_ULOC_LOCALE, u"Kashmiri (Naskh, India)" }, // rdar://51418203
	{ "ks_Aran_IN",             TEST_ULOC_LOCALE, u"Kashmiri (Nastaliq, India)" }, // rdar://47494884
	{ "pa_Arab",                TEST_ULOC_LOCALE, u"Punjabi (Naskh)" }, // rdar://51418203
	{ "pa_Aran",                TEST_ULOC_LOCALE, u"Punjabi (Nastaliq)" }, // rdar://51418203
	{ "pa_Arab_PK",             TEST_ULOC_LOCALE, u"Punjabi (Naskh, Pakistan)" }, // rdar://51418203
	{ "pa_Aran_PK",             TEST_ULOC_LOCALE, u"Punjabi (Nastaliq, Pakistan)" }, // rdar://51418203
	{ "ur_Arab",                TEST_ULOC_LOCALE, u"Urdu (Naskh)" }, // rdar://51418203
	{ "ur_Aran",                TEST_ULOC_LOCALE, u"Urdu (Nastaliq)" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULOC_LOCALE, u"Urdu (Naskh, Pakistan)" }, // rdar://51418203
	{ "ur_Aran_PK",             TEST_ULOC_LOCALE, u"Urdu (Nastaliq, Pakistan)" }, // rdar://47494884
	{ "ks_Arab@calendar=islamic",    TEST_ULOC_LOCALE, u"Kashmiri (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "ks_Aran@calendar=islamic",    TEST_ULOC_LOCALE, u"Kashmiri (Nastaliq, Hijri Calendar)" }, // rdar://47494884
	{ "ks_Arab_IN@calendar=islamic", TEST_ULOC_LOCALE, u"Kashmiri (Naskh, India, Hijri Calendar)" }, // rdar://50687287
	{ "ks_Aran_IN@calendar=islamic", TEST_ULOC_LOCALE, u"Kashmiri (Nastaliq, India, Hijri Calendar)" }, // rdar://47494884
	{ "pa_Arab@calendar=islamic",    TEST_ULOC_LOCALE, u"Punjabi (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Aran@calendar=islamic",    TEST_ULOC_LOCALE, u"Punjabi (Nastaliq, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Arab_PK@calendar=islamic", TEST_ULOC_LOCALE, u"Punjabi (Naskh, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "pa_Aran_PK@calendar=islamic", TEST_ULOC_LOCALE, u"Punjabi (Nastaliq, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Arab@calendar=islamic",    TEST_ULOC_LOCALE, u"Urdu (Naskh, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Aran@calendar=islamic",    TEST_ULOC_LOCALE, u"Urdu (Nastaliq, Hijri Calendar)" }, // rdar://47494884
	{ "ur_Arab_PK@calendar=islamic", TEST_ULOC_LOCALE, u"Urdu (Naskh, Pakistan, Hijri Calendar)" }, // rdar://50687287
	{ "ur_Aran_PK@calendar=islamic", TEST_ULOC_LOCALE, u"Urdu (Nastaliq, Pakistan, Hijri Calendar)" }, // rdar://47494884
	{ "my_Qaag",                TEST_ULOC_LOCALE, u"Burmese (Zawgyi)" }, // rdar://51471316
	{ "cic_Latn",               TEST_ULDN_LOCALE, u"Chickasaw" }, // rdar://105748418
	{ "cho_Latn",               TEST_ULDN_LOCALE, u"Choctaw" }, // rdar://105748418
	{ "apw_Latn",               TEST_ULDN_LOCALE, u"Apache, Western" }, // rdar://94490599
	{ "ber",                    TEST_ULDN_LOCALE, u"Amazigh" }, // rdar://104877633
	{ "hmn",                    TEST_ULDN_LOCALE, u"Hmong" }, // rdar://108866340
    { "inh",                    TEST_ULDN_LOCALE, u"Ingush" }, // rdar://109529736
    { "osa",                    TEST_ULDN_LOCALE, u"Osage" }, // rdar://111138831
	{ "mic",                    TEST_ULDN_LOCALE, u"Mi'kmaw" }, // rdar://104877633
	{ "mid",                    TEST_ULDN_LOCALE, u"Mandaic" }, // rdar://104877633
	{ "nnp",                    TEST_ULDN_LOCALE, u"Wancho" }, // rdar://104877633
	{ "pqm",                    TEST_ULDN_LOCALE, u"Wolastoqey" }, // rdar://104877633
	{ "rej",                    TEST_ULDN_LOCALE, u"Rejang" }, // rdar://104877633
    { "sjd",                    TEST_ULDN_LOCALE, u"Kildin Sámi" }, // rdar://117588343
    { "sje",                    TEST_ULDN_LOCALE, u"Pite Sámi" }, // rdar://117588343
    { "sju",                    TEST_ULDN_LOCALE, u"Ume Sámi" }, // rdar://117588343

	// tests for rdar://63655841 #173 en/fr language names for Canadian Aboriginal Peoples TV
	// & rdar://79400781 #240 Add en/fr display names for language tce
	{ "atj",                    TEST_ULOC_LOCALE, u"Atikamekw" },
	{ "bla",                    TEST_ULOC_LOCALE, u"Siksiká" },
	{ "cr",                     TEST_ULOC_LOCALE, u"Cree" }, // macrolang
	{ "cwd",                    TEST_ULOC_LOCALE, u"Woods Cree" }, // distinct name for primary sub-lang
	{ "crj",                    TEST_ULOC_LOCALE, u"Southern East Cree" }, // other sub-lang
	{ "csw",                    TEST_ULOC_LOCALE, u"Swampy Cree" }, // other sub-lang
	{ "hai",                    TEST_ULOC_LOCALE, u"Haida" }, // macrolang
	{ "hdn",                    TEST_ULOC_LOCALE, u"Northern Haida" }, // distinct name for primary sub-lang
	{ "hax",                    TEST_ULOC_LOCALE, u"Southern Haida" }, // other sub-lang
	{ "iu",                     TEST_ULOC_LOCALE, u"Inuktitut" }, // macrolang
	{ "ike",                    TEST_ULOC_LOCALE, u"Eastern Canadian Inuktitut" }, // distinct name for primary sub-lang
	{ "ikt",                    TEST_ULOC_LOCALE, u"Western Canadian Inuktitut" }, // other sub-lang
	{ "oj",                     TEST_ULOC_LOCALE, u"Ojibwa" }, // macrolang
	{ "ojg",                    TEST_ULOC_LOCALE, u"Eastern Ojibwa" }, // distinct name for primary sub-lang
	{ "ojb",                    TEST_ULOC_LOCALE, u"Northwestern Ojibwa" }, // other sub-lang
	{ "ojs",                    TEST_ULOC_LOCALE, u"Oji-Cree" }, // other sub-lang
	{ "ttm",                    TEST_ULOC_LOCALE, u"Northern Tutchone" },
	{ "tce",                    TEST_ULOC_LOCALE, u"Southern Tutchone" },
	// tests for rdar://74820154, rdar://75064267, rdar://89394823 
	{ "ain",                    TEST_ULOC_LOCALE, u"Ainu" },
	{ "am",                     TEST_ULOC_LOCALE, u"Amharic" },
	{ "ff_Adlm",                TEST_ULOC_LOCALE, u"Fula (Adlam)" },
	{ "ig",                     TEST_ULOC_LOCALE, u"Igbo" },
	{ "nv",                     TEST_ULOC_LOCALE, u"Navajo" },
	{ "rhg",                    TEST_ULOC_LOCALE, u"Rohingya" },
	{ "rhg_Rohg",               TEST_ULOC_LOCALE, u"Rohingya (Hanifi)" },
	{ "syr",                    TEST_ULOC_LOCALE, u"Assyrian" }, // rdar://76764332 #220
	{ "ti",                     TEST_ULOC_LOCALE, u"Tigrinya" },
	{ "Rohg",                   TEST_ULDN_SCRIPT, u"Hanifi Rohingya" },
	{ "apw",                    TEST_ULOC_LOCALE, u"Apache, Western" },
    // baseline tests for rdar://76655165
    { "ky",                     TEST_ULDN_LANGUAGE, u"Kyrgyz" },
    { "ps",                     TEST_ULDN_LANGUAGE, u"Pashto" },
    { "Arab",                   TEST_ULDN_SCRIPT,   u"Arabic" },
    { "CD",                     TEST_ULDN_REGION,   u"Congo - Kinshasa" },
    { "CG",                     TEST_ULDN_REGION,   u"Congo - Brazzaville" },
    { "SZ",                     TEST_ULDN_REGION,   u"Eswatini" },
    { "GB",                     TEST_ULDN_REGION,   u"United Kingdom" },
    { "FR",                     TEST_ULDN_REGION,   u"France" },
    { "ps_Arab_AF",             TEST_ULOC_LOCALE,   u"Pashto (Arabic, Afghanistan)" },
    { "en_SZ",                  TEST_ULOC_LOCALE,   u"English (Eswatini)" },
    { "en_GB",                  TEST_ULOC_LOCALE,   u"English (United Kingdom)" },
    { "fr_CG",                  TEST_ULOC_LOCALE,   u"French (Congo - Brazzaville)" },
    { "fr_FR",                  TEST_ULOC_LOCALE,   u"French (France)" },
	// tests for rdar://80374611 #295 Add en names for languages mdh,otk,oui to enable locale use in Xcode
	{ "mdh",                    TEST_ULOC_LOCALE, u"Maguindanaon" },
	{ "otk",                    TEST_ULOC_LOCALE, u"Old Turkish" },
	{ "oui",                    TEST_ULOC_LOCALE, u"Old Uighur" },
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // (compare with values under en_StdMidPrc[] below)
    { "zh_HK",                  TEST_ULOC_LOCALE, u"Chinese (Hong Kong)" },
    { "zh_MO",                  TEST_ULOC_LOCALE, u"Chinese (Macao)" },
    { "zh_TW",                  TEST_ULOC_LOCALE, u"Chinese (Taiwan)" },
    { "zh_CN",                  TEST_ULOC_LOCALE, u"Chinese (China mainland)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
    { "MO",                     TEST_ULDN_REGION, u"Macao" },
    { "TW",                     TEST_ULDN_REGION, u"Taiwan" },
    { "CN",                     TEST_ULDN_REGION, u"China mainland" },
    { "en_HK",                  TEST_ULOC_LOCALE, u"English (Hong Kong)" },
    { "en_MO",                  TEST_ULOC_LOCALE, u"English (Macao)" },
    { "en_TW",                  TEST_ULOC_LOCALE, u"English (Taiwan)" },
    { "en_CN",                  TEST_ULOC_LOCALE, u"English (China mainland)" },
    { "en_HK",                  TEST_ULOC_REGION, u"Hong Kong" },
    { "en_MO",                  TEST_ULOC_REGION, u"Macao" },
    { "en_TW",                  TEST_ULOC_REGION, u"Taiwan" },
    { "en_CN",                  TEST_ULOC_REGION, u"China mainland" },
#endif  // APPLE_ICU_CHANGES
};

static const UldnItem en_StdMidShrt[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"English (US)" },
	{ "en",                     TEST_ULDN_LANGUAGE, u"English" },
#if APPLE_ICU_CHANGES
// rdar://
	{ "en_US_POSIX",            TEST_ULDN_LOCALE, u"English (US, Computer)" },
	{ "en_US@calendar=chinese", TEST_ULDN_LOCALE, u"English (US, Chinese Calendar)" },
	{ "en_CA",                  TEST_ULDN_LOCALE, u"English (Canada)" },
	{ "pt",                     TEST_ULDN_LOCALE, u"Portuguese" },
	{ "pt_BR",                  TEST_ULDN_LOCALE, u"Portuguese (Brazil)" },
	{ "pt_PT",                  TEST_ULDN_LOCALE, u"Portuguese (Portugal)" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" }, // rdar://50750364
	{ "zh_Hant_HK",             TEST_ULDN_LOCALE, u"Chinese, Traditional (Hong Kong)" }, // rdar://50750364
	{ "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong)" },
	{ "Latn",                   TEST_ULDN_SCRIPT, u"Latin" },
	{ "Hans",                   TEST_ULDN_SCRIPT, u"Simplified Han" },
	{ "Hant",                   TEST_ULDN_SCRIPT, u"Traditional Han" },
	{ "US",                     TEST_ULDN_REGION, u"US" },
	{ "CA",                     TEST_ULDN_REGION, u"Canada" },
	{ "GB",                     TEST_ULDN_REGION, u"UK" },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
#endif  // APPLE_ICU_CHANGES
};

#if APPLE_ICU_CHANGES
// rdar://
static const UldnItem en_StdMidVrnt[] = {
    // tests for rdar://76655165
    { "ky",                     TEST_ULDN_LANGUAGE, u"Kirghiz" },
    // rdar://108460253 - Update variant name for 'ii' to "Sichuan Yi; Nuosu”
    { "ii",                     TEST_ULDN_LANGUAGE, u"Sichuan Yi; Nuosu" },
    { "ps",                     TEST_ULDN_LANGUAGE, u"Pushto" },
    { "Arab",                   TEST_ULDN_SCRIPT,   u"Perso-Arabic" },
    { "CD",                     TEST_ULDN_REGION,   u"Congo (DRC)" },
    { "CG",                     TEST_ULDN_REGION,   u"Congo (Republic)" },
    { "SZ",                     TEST_ULDN_REGION,   u"Swaziland" },
    { "GB",                     TEST_ULDN_REGION,   u"United Kingdom" },
    { "FR",                     TEST_ULDN_REGION,   u"France" },
    { "ps_Arab_AF",             TEST_ULDN_LOCALE,   u"Pushto (Perso-Arabic, Afghanistan)" },
    { "en_SZ",                  TEST_ULDN_LOCALE,   u"English (Swaziland)" },
    { "en_GB",                  TEST_ULDN_LOCALE,   u"English (UK)" },
    { "fr_CG",                  TEST_ULDN_LOCALE,   u"French (Congo [Republic])" },
    { "fr_FR",                  TEST_ULDN_LOCALE,   u"French (France)" },
};
#endif  // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://
static const UldnItem en_StdMidPrc[] = {
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // (compare with values under en_StdMidLong[] above)
    { "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong [China])" },
    { "zh_MO",                  TEST_ULDN_LOCALE, u"Chinese (Macao [China])" },
    { "zh_TW",                  TEST_ULDN_LOCALE, u"Chinese (Taiwan [China])" },
    { "zh_CN",                  TEST_ULDN_LOCALE, u"Chinese (China)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong (China)" },
    { "MO",                     TEST_ULDN_REGION, u"Macao (China)" },
    { "TW",                     TEST_ULDN_REGION, u"Taiwan (China)" },
    { "CN",                     TEST_ULDN_REGION, u"China" },
};

static const UldnItem en_StdLstPrc[] = {
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // and rdar://120926070 (No tests exist for UI capitalization and PRC length)
    // Should get the same results with UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU and UDISPCTX_LENGTH_PRC
    // as those above for UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE and UDISPCTX_LENGTH_PRC
    { "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong [China])" },
    { "zh_MO",                  TEST_ULDN_LOCALE, u"Chinese (Macao [China])" },
    { "zh_TW",                  TEST_ULDN_LOCALE, u"Chinese (Taiwan [China])" },
    { "zh_CN",                  TEST_ULDN_LOCALE, u"Chinese (China)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong (China)" },
    { "MO",                     TEST_ULDN_REGION, u"Macao (China)" },
    { "TW",                     TEST_ULDN_REGION, u"Taiwan (China)" },
    { "CN",                     TEST_ULDN_REGION, u"China" },
};

static const UldnItem en_StdLstLong[] = {
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // (compare with values under en_StdMidLong[] above)
    { "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong)" },
    { "zh_MO",                  TEST_ULDN_LOCALE, u"Chinese (Macao)" },
    { "zh_TW",                  TEST_ULDN_LOCALE, u"Chinese (Taiwan)" },
    { "zh_CN",                  TEST_ULDN_LOCALE, u"Chinese (China mainland)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
    { "MO",                     TEST_ULDN_REGION, u"Macao" },
    { "TW",                     TEST_ULDN_REGION, u"Taiwan" },
    { "CN",                     TEST_ULDN_REGION, u"China mainland" },
};
#endif  // APPLE_ICU_CHANGES

static const UldnItem en_DiaMidLong[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"American English" },
	{ "fa_AF",                  TEST_ULDN_LOCALE, u"Dari" },
	{ "prs",                    TEST_ULDN_LOCALE, u"Dari" },
	{ "prs_AF",                 TEST_ULDN_LOCALE, u"Dari (Afghanistan)" },
	{ "prs_TJ",                 TEST_ULDN_LOCALE, u"Dari (Tajikistan)" },
	{ "prs",                    TEST_ULDN_LANGUAGE, u"Dari" },
	{ "mo",                     TEST_ULDN_LOCALE, u"Romanian" },
	{ "mo",                     TEST_ULDN_LANGUAGE, u"Romanian" },
#if APPLE_ICU_CHANGES
// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
// rdar:// 
	{ "en_US_POSIX",            TEST_ULDN_LOCALE, u"American English (Computer)" },
	{ "en_US@calendar=chinese", TEST_ULDN_LOCALE, u"American English (Chinese Calendar)" },
	{ "en_CA",                  TEST_ULDN_LOCALE, u"Canadian English" },
	{ "pt",                     TEST_ULDN_LOCALE, u"Portuguese" },
	{ "pt_BR",                  TEST_ULDN_LOCALE, u"Brazilian Portuguese" },
	{ "pt_PT",                  TEST_ULDN_LOCALE, u"European Portuguese" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" }, // rdar://50750364
	{ "zh_Hant_HK",             TEST_ULDN_LOCALE, u"Chinese, Traditional (Hong Kong)" }, //rdar://50750364
	{ "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong)" },
	{ "Latn",                   TEST_ULDN_SCRIPT, u"Latin" },
	{ "Hans",                   TEST_ULDN_SCRIPT, u"Simplified Han" },
	{ "Hant",                   TEST_ULDN_SCRIPT, u"Traditional Han" },
	{ "US",                     TEST_ULDN_REGION, u"United States" },
	{ "CA",                     TEST_ULDN_REGION, u"Canada" },
	{ "GB",                     TEST_ULDN_REGION, u"United Kingdom" },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
#endif  // APPLE_ICU_CHANGES
};

static const UldnItem en_DiaMidShrt[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"US English" },
#if APPLE_ICU_CHANGES
// rdar://
	{ "en_US_POSIX",            TEST_ULDN_LOCALE, u"US English (Computer)" },
	{ "en_US@calendar=chinese", TEST_ULDN_LOCALE, u"US English (Chinese Calendar)" },
	{ "en_CA",                  TEST_ULDN_LOCALE, u"Canadian English" },
	{ "pt",                     TEST_ULDN_LOCALE, u"Portuguese" },
	{ "pt_BR",                  TEST_ULDN_LOCALE, u"Brazilian Portuguese" },
	{ "pt_PT",                  TEST_ULDN_LOCALE, u"European Portuguese" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" }, // rdar://50750364
	{ "zh_Hant_HK",             TEST_ULDN_LOCALE, u"Chinese, Traditional (Hong Kong)" }, // rdar://50750364
	{ "zh_HK",                  TEST_ULDN_LOCALE, u"Chinese (Hong Kong)" },
	{ "Latn",                   TEST_ULDN_SCRIPT, u"Latin" },
	{ "Hans",                   TEST_ULDN_SCRIPT, u"Simplified Han" },
	{ "Hant",                   TEST_ULDN_SCRIPT, u"Traditional Han" },
	{ "US",                     TEST_ULDN_REGION, u"US" },
	{ "CA",                     TEST_ULDN_REGION, u"Canada" },
	{ "GB",                     TEST_ULDN_REGION, u"UK" },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
#endif  // APPLE_ICU_CHANGES
};

#if APPLE_ICU_CHANGES
// rdar://
static const UldnItem en_CA_DiaMidLong[] = { // rdar://60916890
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"Cantonese, Simplified" },
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"Cantonese, Traditional" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" },
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"Chinese, Traditional" },
	{ "ar_001",                 TEST_ULDN_LOCALE, u"Arabic (Modern Standard)" },
	{ "de_AT",                  TEST_ULDN_LOCALE, u"Austrian German" },
	{ "es_419",                 TEST_ULDN_LOCALE, u"Latin American Spanish" },
	{ "fr_CA",                  TEST_ULDN_LOCALE, u"Canadian French" },
	{ "en_GB",                  TEST_ULDN_LOCALE, u"British English" },
	{ "en_US",                  TEST_ULDN_LOCALE, u"American English" },
};

static const UldnItem en_CA_DiaMidShrt[] = { // rdar://60916890
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"Cantonese, Simplified" },
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"Cantonese, Traditional" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" },
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"Chinese, Traditional" },
	{ "ar_001",                 TEST_ULDN_LOCALE, u"Arabic (Modern Standard)" },
	{ "de_AT",                  TEST_ULDN_LOCALE, u"Austrian German" },
	{ "es_419",                 TEST_ULDN_LOCALE, u"Latin American Spanish" },
	{ "fr_CA",                  TEST_ULDN_LOCALE, u"Canadian French" },
	{ "en_GB",                  TEST_ULDN_LOCALE, u"UK English" },
	{ "en_US",                  TEST_ULDN_LOCALE, u"US English" },
};

static const UldnItem en_GB_DiaMidLong[] = { // rdar://60916890
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"Cantonese, Simplified" },
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"Cantonese, Traditional" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" },
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"Chinese, Traditional" },
	{ "ar_001",                 TEST_ULDN_LOCALE, u"Modern Standard Arabic" },
	{ "de_AT",                  TEST_ULDN_LOCALE, u"Austrian German" },
	{ "es_419",                 TEST_ULDN_LOCALE, u"Latin American Spanish" },
	{ "fr_CA",                  TEST_ULDN_LOCALE, u"Canadian French" },
	{ "en_GB",                  TEST_ULDN_LOCALE, u"British English" },
	{ "en_US",                  TEST_ULDN_LOCALE, u"American English" },
};

static const UldnItem en_GB_DiaMidShrt[] = { // rdar://60916890
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"Cantonese, Simplified" },
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"Cantonese, Traditional" },
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"Chinese, Simplified" },
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"Chinese, Traditional" },
	{ "ar_001",                 TEST_ULDN_LOCALE, u"Modern Standard Arabic" },
	{ "de_AT",                  TEST_ULDN_LOCALE, u"Austrian German" },
	{ "es_419",                 TEST_ULDN_LOCALE, u"Latin American Spanish" },
	{ "fr_CA",                  TEST_ULDN_LOCALE, u"Canadian French" },
	{ "en_GB",                  TEST_ULDN_LOCALE, u"UK English" },
	{ "en_US",                  TEST_ULDN_LOCALE, u"US English" },
};

static const UldnItem en_IN_StdMidLong[] = {
	{ "bn",                     TEST_ULDN_LOCALE, u"Bangla" },
	{ "bn_Beng",                TEST_ULDN_LOCALE, u"Bangla (Bangla)" },
	{ "or",                     TEST_ULDN_LOCALE, u"Odia" },
	{ "or_Orya",                TEST_ULDN_LOCALE, u"Odia (Odia)" },
};

// rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
static const UldnItem fi_StdMidLong[] = {
	{ "en",                     TEST_ULDN_LOCALE, u"englanti" },
};

//rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
static const UldnItem fi_StdBegLong[] = {
	{ "en",                     TEST_ULDN_LOCALE, u"Englanti" },
};

// rdar://47499172 a7fdc2124c.. Update loc names for CN,HK,MO; delete redundant short names for them
// rdar://68351139 change names for region IO per GA, Legal, ET
static const UldnItem fr_StdMidLong[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"anglais (É.-U.)" },
	{ "US",                     TEST_ULDN_REGION, u"États-Unis" },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
	{ "MO",                     TEST_ULDN_REGION, u"Macao" },
	{ "IO",                     TEST_ULDN_REGION, u"Archipel des Chagos" }, // rdar://68351139 change names for region IO per GA, Legal, ET
	// tests for rdar://63655841 #173 en/fr language names for Canadian Aboriginal Peoples TV
	// & rdar://79400781 #240 (Add en/fr display names for language tce)
	{ "atj",                    TEST_ULDN_LOCALE, u"atikamekw" },
	{ "cho",                    TEST_ULDN_LOCALE, u"choctaw" },
	{ "cic",                    TEST_ULDN_LOCALE, u"chicacha" },	
	{ "cr",                     TEST_ULDN_LOCALE, u"cree" }, // macrolang
	{ "cwd",                    TEST_ULDN_LOCALE, u"cri des bois" }, // distinct name for primary sub-lang
	{ "crj",                    TEST_ULDN_LOCALE, u"cri de l’Est (dialecte du Sud)" }, // other sub-lang
	{ "csw",                    TEST_ULDN_LOCALE, u"cri des marais" }, // other sub-lang
	{ "hai",                    TEST_ULDN_LOCALE, u"haïda" }, // macrolang
	{ "hdn",                    TEST_ULDN_LOCALE, u"haïda du Nord" }, // distinct name for primary sub-lang
	{ "hax",                    TEST_ULDN_LOCALE, u"haïda du Sud" }, // other sub-lang
	{ "iu",                     TEST_ULDN_LOCALE, u"inuktitut" }, // macrolang
	{ "ike",                    TEST_ULDN_LOCALE, u"inuktitut de l’Est canadien" }, // distinct name for primary sub-lang
	{ "ikt",                    TEST_ULDN_LOCALE, u"inuktitut de l’Ouest canadien" }, // other sub-lang
	{ "oj",                     TEST_ULDN_LOCALE, u"ojibwa" }, // macrolang
	{ "ojg",                    TEST_ULDN_LOCALE, u"ojibwa de l'Est" }, // distinct name for primary sub-lang
	{ "ojb",                    TEST_ULDN_LOCALE, u"ojibwé du Nord-Ouest" }, // other sub-lang
	{ "ojs",                    TEST_ULDN_LOCALE, u"oji-cri" }, // other sub-lang
	{ "ttm",                    TEST_ULDN_LOCALE, u"tutchone du Nord" },
	{ "tce",                    TEST_ULDN_LOCALE, u"tutchone du Sud" },
	// tests for rdar://75064267, rdar://89394823 
	{ "rhg",                    TEST_ULOC_LOCALE, u"rohingya" },
	{ "Adlm",                   TEST_ULDN_SCRIPT, u"adlam" },
	{ "Rohg",                   TEST_ULDN_SCRIPT, u"hanifi" },
	{ "Syrc",                   TEST_ULDN_SCRIPT, u"syriaque" },
	{ "syr",                    TEST_ULOC_LOCALE, u"soureth" },
	{ "apw",                    TEST_ULOC_LOCALE, u"apache occidental" },
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // (compare with values under en_StdMidLong above and fr_StdMidPrc[] below-- in this case, we
    // should get the same results for "Long" and "Prc" because French doesn't have special names
    // for use in the PRC)
    { "zh_HK",                  TEST_ULOC_LOCALE, u"chinois (Hong Kong)" },
    { "zh_MO",                  TEST_ULOC_LOCALE, u"chinois (Macao)" },
    { "zh_TW",                  TEST_ULOC_LOCALE, u"chinois (Taïwan)" },
    { "zh_CN",                  TEST_ULOC_LOCALE, u"chinois (Chine continentale)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
    { "MO",                     TEST_ULDN_REGION, u"Macao" },
    { "TW",                     TEST_ULDN_REGION, u"Taïwan" },
    { "CN",                     TEST_ULDN_REGION, u"Chine continentale" },
};

static const UldnItem fr_StdMidShrt[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"anglais (É.-U.)" },
	{ "US",                     TEST_ULDN_REGION, u"É.-U." },
	{ "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
};
    
#if APPLE_ICU_CHANGES
// rdar://
static const UldnItem fr_StdMidPrc[] = {
    // tests for rdar://115264744 (Sub-TLF: China geopolitical location display via ICU)
    // (compare with values under fr_StdMidLong[] above-- in this case, we should get the same
    // results for "Long" and "Prc" because French doesn't have special names for use in the PRC)
    { "zh_HK",                  TEST_ULOC_LOCALE, u"chinois (Hong Kong)" },
    { "zh_MO",                  TEST_ULOC_LOCALE, u"chinois (Macao)" },
    { "zh_TW",                  TEST_ULOC_LOCALE, u"chinois (Taïwan)" },
    { "zh_CN",                  TEST_ULOC_LOCALE, u"chinois (Chine continentale)" },
    { "HK",                     TEST_ULDN_REGION, u"Hong Kong" },
    { "MO",                     TEST_ULDN_REGION, u"Macao" },
    { "TW",                     TEST_ULDN_REGION, u"Taïwan" },
    { "CN",                     TEST_ULDN_REGION, u"Chine continentale" },
};
#endif  // APPLE_ICU_CHANGES

static const UldnItem fr_StdBegLong[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"Anglais (É.-U.)" },
};

static const UldnItem fr_StdLstLong[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"Anglais (É.-U.)" },
	{ "PS",                     TEST_ULDN_REGION, u"Territoires palestiniens" },
    { "Hans",                   TEST_ULDN_SCRIPT, u"sinogrammes simplifiés" },      // rdar://70285470
    { "Hant",                   TEST_ULDN_SCRIPT, u"sinogrammes traditionnels" }    // rdar://70285470
};

static const UldnItem fr_DiaMidLong[] = {
	{ "en_US",                  TEST_ULDN_LOCALE, u"anglais américain" },
};

static const UldnItem ca_StdLstLong[] = {
	{ "PS",                     TEST_ULDN_REGION,   u"Territoris palestins" },
    { "SZ",                     TEST_ULDN_REGION,   u"Eswatini" },             // rdar://65139631
    { "az",                     TEST_ULDN_LANGUAGE, u"àzeri" },                // rdar://107286939
};

static const UldnItem nb_StdMidLong[] = {
	{ "ur_Arab",                TEST_ULDN_LOCALE, u"urdu (naskh)" },
	// tests for rdar://75064267, rdar://89394823, rdar://91073737 #298 Add/update more localized names for apw, Western Apache
	{ "rhg",                    TEST_ULOC_LOCALE, u"rohingya" },
	{ "Adlm",                   TEST_ULDN_SCRIPT, u"adlam" },
	{ "Rohg",                   TEST_ULDN_SCRIPT, u"hanifi" },
	{ "Syrc",                   TEST_ULDN_SCRIPT, u"syrisk" },
	{ "apw",                    TEST_ULOC_LOCALE, u"apache, vestlig" },
    { "sjd",                    TEST_ULOC_LOCALE, u"kildinsamisk" }, // rdar://117588343
    { "sje",                    TEST_ULOC_LOCALE, u"pitesamisk" }, // rdar://117588343
    { "sju",                    TEST_ULOC_LOCALE, u"umesamisk" }, // rdar://117588343
};

// rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
static const UldnItem ru_StdMidLong[] = {
	{ "cic",                    TEST_ULDN_LOCALE, u"чикасо" },
	{ "cic_Latn",               TEST_ULDN_LOCALE, u"чикасо" },
};

static const UldnItem ur_StdMidLong[] = {
	{ "ps_Arab",                TEST_ULDN_LOCALE, u"پشتو (عربی)" },
	{ "ps_Arab_AF",             TEST_ULDN_LOCALE, u"پشتو (عربی،افغانستان)" },
	{ "ur_Aran",                TEST_ULDN_LOCALE, u"اردو (نستعلیق)" }, // rdar://47494884
	{ "ur_Arab",                TEST_ULDN_LOCALE, u"اردو (نسخ)" }, // rdar://50687287
	{ "ur_Aran_PK",             TEST_ULDN_LOCALE, u"اردو (نستعلیق،پاکستان)" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULDN_LOCALE, u"اردو (نسخ،پاکستان)" }, // rdar://50687287

	{ "ps_Arab",                TEST_ULOC_LOCALE, u"پشتو (عربی)" },
	{ "ps_Arab_AF",             TEST_ULOC_LOCALE, u"پشتو (عربی،افغانستان)" },
	{ "ur_Aran",                TEST_ULOC_LOCALE, u"اردو (نستعلیق)" },     // rdar://47494884
	{ "ur_Arab",                TEST_ULOC_LOCALE, u"اردو (نسخ)" },         // rdar://51418203
	{ "ur_Aran_PK",             TEST_ULOC_LOCALE, u"اردو (نستعلیق،پاکستان)" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULOC_LOCALE, u"اردو (نسخ،پاکستان)" }, // rdar://51418203
};

static const UldnItem pa_Arab_StdMidLong[] = {
	{ "pa_Aran",                TEST_ULDN_LOCALE, u"پنجابی (نستعلیق)" }, // rdar://47494884
	{ "pa_Arab",                TEST_ULDN_LOCALE, u"پنجابی (نسخ)" }, // rdar://50687287
	{ "pa_Aran_PK",             TEST_ULDN_LOCALE, u"پنجابی (نستعلیق, پاکستان)" }, // rdar://47494884
	{ "pa_Arab_PK",             TEST_ULDN_LOCALE, u"پنجابی (نسخ, پاکستان)" }, // rdar://50687287

	{ "pa_Aran",                TEST_ULOC_LOCALE, u"پنجابی (نستعلیق)" },      // rdar://51418203
	{ "pa_Arab",                TEST_ULOC_LOCALE, u"پنجابی (نسخ)" },          // rdar://51418203
	{ "pa_Aran_PK",             TEST_ULOC_LOCALE, u"پنجابی (نستعلیق, پاکستان)" }, // rdar://51418203
	{ "pa_Arab_PK",             TEST_ULOC_LOCALE, u"پنجابی (نسخ, پاکستان)" }, // rdar://51418203
};

static const UldnItem zh_StdMidLong[] = {
	{ "zh_Hans",                TEST_ULDN_LOCALE, u"简体中文" },           // rdar://50750364
	{ "zh_Hans_CN",             TEST_ULDN_LOCALE, u"简体中文（中国大陆）" }, // rdar://50750364
	{ "zh_Hant",                TEST_ULDN_LOCALE, u"繁体中文" },           // rdar://50750364
	{ "zh_Hant_HK",             TEST_ULDN_LOCALE, u"繁体中文（香港）" },    // rdar://50750364
	{ "yue_Hans",               TEST_ULDN_LOCALE, u"简体粤语" },           // rdar://50750364
	{ "yue_Hans_CN",            TEST_ULDN_LOCALE, u"简体粤语（中国大陆）" }, // rdar://50750364
	{ "yue_Hant",               TEST_ULDN_LOCALE, u"繁体粤语" },           // rdar://50750364
	{ "yue_Hant_HK",            TEST_ULDN_LOCALE, u"繁体粤语（香港）" },    // rdar://50750364
	{ "ps_Arab",                TEST_ULDN_LOCALE, u"普什图语（阿拉伯文）" },
	{ "ps_Arab_AF",             TEST_ULDN_LOCALE, u"普什图语（阿拉伯文，阿富汗）" },
	{ "ur_Aran",                TEST_ULDN_LOCALE, u"乌尔都语（波斯体）" }, // rdar://47494884
	{ "ur_Arab",                TEST_ULDN_LOCALE, u"乌尔都语（誊抄体）" }, // rdar://50687287
	{ "ur_Aran_PK",             TEST_ULDN_LOCALE, u"乌尔都语（波斯体，巴基斯坦）" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULDN_LOCALE, u"乌尔都语（誊抄体，巴基斯坦）" }, // rdar://50687287

	{ "zh_Hans",                TEST_ULOC_LOCALE, u"简体中文" },           // rdar://51418203
	{ "zh_Hans_CN",             TEST_ULOC_LOCALE, u"简体中文（中国大陆）" }, // rdar://51418203
	{ "zh_Hant",                TEST_ULOC_LOCALE, u"繁体中文" },           // rdar://51418203
	{ "zh_Hant_HK",             TEST_ULOC_LOCALE, u"繁体中文（香港）" },    // rdar://51418203
	{ "yue_Hans",               TEST_ULOC_LOCALE, u"简体粤语" },           // rdar://51418203
	{ "yue_Hans_CN",            TEST_ULOC_LOCALE, u"简体粤语（中国大陆）" }, // rdar://51418203
	{ "yue_Hant",               TEST_ULOC_LOCALE, u"繁体粤语" },           // rdar://51418203
	{ "yue_Hant_HK",            TEST_ULOC_LOCALE, u"繁体粤语（香港）" },    // rdar://51418203
	{ "ur_Aran",                TEST_ULOC_LOCALE, u"乌尔都语（波斯体）" }, // rdar://47494884
	{ "ur_Arab",                TEST_ULOC_LOCALE, u"乌尔都语（誊抄体）" }, // rdar://51418203
	{ "ur_Aran_PK",             TEST_ULOC_LOCALE, u"乌尔都语（波斯体，巴基斯坦）" }, // rdar://47494884
	{ "ur_Arab_PK",             TEST_ULOC_LOCALE, u"乌尔都语（誊抄体，巴基斯坦）" }, // rdar://51418203

	{ "HK",                     TEST_ULDN_REGION, u"香港" },
	{ "MO",                     TEST_ULDN_REGION, u"澳门" },
    
    { "ars",                    TEST_ULDN_LANGUAGE, u"阿拉伯语（内志）" },  // rdar://69728925
	{ "ii",                     TEST_ULDN_LANGUAGE, u"凉山彝语" },  // rdar://107440844,rdar://108460253
    { "pqm",                    TEST_ULDN_LANGUAGE, u"沃拉斯托基语" }, // rdar://107600615
    { "se",                     TEST_ULDN_LANGUAGE, u"北萨米语" }, // rdar://117588343

	// tests for rdar://75064267, rdar://89394823, rdar://59762233, rdar://91073737 #298 Add/update more localized names for apw, Western Apache
	{ "rhg",                    TEST_ULOC_LOCALE, u"罗兴亚语" },
	{ "Adlm",                   TEST_ULDN_SCRIPT, u"阿德拉姆文" },
	{ "Rohg",                   TEST_ULDN_SCRIPT, u"哈乃斐文" },
	{ "Syrc",                   TEST_ULDN_SCRIPT, u"叙利亚文" },
	{ "apw",                    TEST_ULOC_LOCALE, u"阿帕奇语（西方）" },
	{ "guc",                    TEST_ULOC_LOCALE, u"瓦尤语" },
	{ "nnp",                    TEST_ULOC_LOCALE, u"万秋语" }, // rdar://108037783
	{ "Wcho",                   TEST_ULDN_SCRIPT, u"万秋文" }, // rdar://108037783
    
    // tests for rdar://125648144
    { "blo",                   TEST_ULDN_LANGUAGE, u"阿尼语" },
    { "kxv",                   TEST_ULDN_LANGUAGE, u"库维语" },
    { "vmw",                   TEST_ULDN_LANGUAGE, u"马库阿语" },
    { "xnr",                   TEST_ULDN_LANGUAGE, u"康格里语" }, 
};

static const UldnItem zh_Hant_StdMidLong[] = {
	{ "HK",                     TEST_ULDN_REGION, u"香港" },
	{ "MO",                     TEST_ULDN_REGION, u"澳門" },
    
    { "ars",                    TEST_ULDN_LANGUAGE, u"阿拉伯文（內志）" },  // rdar://69728925
	{ "apw",                    TEST_ULOC_LOCALE, u"阿帕切文（西部）" }, // rdar://89394823, rdar://91073737 #298 Add/update more localized names for apw, Western Apache
};

static const UldnItem zh_Hant_HK_StdMidLong[] = {
	{ "HK",                     TEST_ULDN_REGION, u"香港" },
	{ "MO",                     TEST_ULDN_REGION, u"澳門" },
};

static const UldnItem yue_StdMidLong[] = {
	{ "HK",                     TEST_ULDN_REGION, u"香港" },
	{ "MO",                     TEST_ULDN_REGION, u"澳門" },
	{ "ii",                     TEST_ULDN_LANGUAGE, u"涼山彝文" },  // rdar://107440844,rdar://108460253
    { "pqm",                    TEST_ULDN_LANGUAGE, u"沃拉斯托基语" }, // rdar://107600615
};

static const UldnItem yue_Hans_StdMidLong[] = {
	{ "zh_Hans_CN",             TEST_ULOC_LOCALE, u"简体中文（中国大陆）" }, // rdar://53136228
	{ "zh_Hant_HK",             TEST_ULOC_LOCALE, u"繁体中文（香港）" },    // rdar://53136228
	{ "yue_Hans_CN",            TEST_ULOC_LOCALE, u"简体粤语（中国大陆）" }, // rdar://53136228
	{ "yue_Hant_HK",            TEST_ULOC_LOCALE, u"繁体粤语（香港）" },    // rdar://53136228
	{ "HK",                     TEST_ULDN_REGION, u"香港" },
	{ "MO",                     TEST_ULDN_REGION, u"澳门" },
	{ "apw",                    TEST_ULOC_LOCALE, u"阿帕奇语（西方）" }, // rdar://89394823, rdar://91073737 #298 Add/update more localized names for apw, Western Apache
};

static const UldnItem nds_StdMidLong[] = { // rdar://64703717
	{ "nds",                   TEST_ULOC_LOCALE, u"Neddersass’sch" },
};

static const UldnItem hi_StdMidLong[] = { // rdar://53653337
	{ "Aran",                   TEST_ULDN_SCRIPT, u"नस्तालीक़" },
	// tests for rdar://75064267, rdar://89394823, rdar://59762233, rdar://91073737 #298 Add/update more localized names for apw, Western Apache
	{ "rhg",                    TEST_ULOC_LOCALE, u"रोहिंग्या" },
	{ "Adlm",                   TEST_ULDN_SCRIPT, u"ऐडलम" },
	{ "Rohg",                   TEST_ULDN_SCRIPT, u"हनिफ़ि" },
	{ "Syrc",                   TEST_ULDN_SCRIPT, u"सिरिएक" },
	{ "apw",                    TEST_ULOC_LOCALE, u"अपाची, पश्चिमी" },
	{ "guc",                    TEST_ULOC_LOCALE, u"वायु" },
    { "smj",                    TEST_ULOC_LOCALE, u"लूले सामी" }, // rdar://117588343
	{ "JM",                     TEST_ULDN_REGION, u"जमैका"}, // rdar://52174281, which supersedes rdar://16850007
};

static const UldnItem hi_Latn_StdMidLong[] = { // rdar://53216112
	{ "en",                     TEST_ULDN_LOCALE, u"English" },
	{ "hi_Deva",                TEST_ULDN_LOCALE, u"Hindi (Devanagari)" },
	{ "hi_Latn",                TEST_ULDN_LOCALE, u"Hindi (Latin)" },
	{ "hi_Latn_IN",             TEST_ULDN_LOCALE, u"Hindi (Latin, Bharat)" }, // rdar://125016053
};

static const UldnItem mni_Beng_StdMidLong[] = { // rdar://54153189
	{ "mni_Beng",               TEST_ULDN_LOCALE, u"মৈতৈলোন্ (বাংলা)" },
	{ "mni_Mtei",               TEST_ULDN_LOCALE, u"মৈতৈলোন্ (মীতৈ ময়েক)" },
};

static const UldnItem mni_Mtei_StdMidLong[] = { // rdar://54153189
	{ "mni_Beng",               TEST_ULDN_LOCALE, u"ꯃꯤꯇꯩꯂꯣꯟ (ꯕꯪꯂꯥ)" },
	{ "mni_Mtei",               TEST_ULDN_LOCALE, u"ꯃꯤꯇꯩꯂꯣꯟ (ꯃꯤꯇꯩ ꯃꯌꯦꯛ)" },
};

static const UldnItem sat_Olck_StdMidLong[] = { // rdar://54153189
	{ "sat_Olck",               TEST_ULDN_LOCALE, u"ᱥᱟᱱᱛᱟᱲᱤ (ᱚᱞ ᱪᱤᱠᱤ)" },
	{ "sat_Deva",               TEST_ULDN_LOCALE, u"ᱥᱟᱱᱛᱟᱲᱤ (ᱫᱮᱣᱟᱱᱟᱜᱟᱨᱤ)" },
};

static const UldnItem sat_Deva_StdMidLong[] = { // rdar://54153189
	{ "sat_Olck",               TEST_ULDN_LOCALE, u"सानताड़ी (अल चीकी)" },
	{ "sat_Deva",               TEST_ULDN_LOCALE, u"सानताड़ी (देवानागारी)" },
};

static const UldnItem en_AU_StdBegLong[] = { // rdar://69835367
    { "019",               TEST_ULDN_REGION, u"Americas" },
    { "002",               TEST_ULDN_REGION, u"Africa" },
    { "150",               TEST_ULDN_REGION, u"Europe" },
    { "142",               TEST_ULDN_REGION, u"Asia" },
    { "009",               TEST_ULDN_REGION, u"Oceania" },
};

static const UldnItem es_MX_StdBegLong[] = { // rdar://69835367
    { "019",                TEST_ULDN_REGION, u"América" },
    { "002",                TEST_ULDN_REGION, u"África" },
    { "150",                TEST_ULDN_REGION, u"Europa" },
    { "142",                TEST_ULDN_REGION, u"Asia" },
    { "009",                TEST_ULDN_REGION, u"Oceanía" },
	{ "HK",                 TEST_ULDN_REGION, u"Hong-Kong" },
	{ "MO",                 TEST_ULDN_REGION, u"Macao" },
	{ "guc",                TEST_ULDN_LANGUAGE, u"Wayú" }, // rdar://59762233
};

static const UldnItem ms_StdMidLong[] = {
	{ "SZ",                 TEST_ULDN_REGION, u"Eswatini" },
	{ "wuu",                TEST_ULDN_LOCALE, u"Dialek Shanghai" },
	{ "HK",                 TEST_ULDN_REGION, u"Hong Kong" },
	{ "MO",                 TEST_ULDN_REGION, u"Macau" },
	// rdar://59762233
	{ "arc",                TEST_ULDN_LOCALE, u"Aramia" },
	{ "car",                TEST_ULDN_LOCALE, u"Carib" },
	{ "guc",                TEST_ULDN_LOCALE, u"Wayuu" },
	{ "vls",                TEST_ULDN_LOCALE, u"Flemish Barat" },
};

static const UldnItem ain_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "ain",                TEST_ULOC_LOCALE, u"アイヌ・イタㇰ" },
};

static const UldnItem am_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "am",                 TEST_ULOC_LOCALE, u"አማርኛ" },
};

static const UldnItem ff_Adlm_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "ff_Adlm",            TEST_ULOC_LOCALE, u"𞤆𞤵𞤤𞤢𞤪 (𞤀𞤁𞤂𞤢𞤃)" },
};

static const UldnItem ig_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "ig",                 TEST_ULOC_LOCALE, u"Igbo" },
};

static const UldnItem kk_StdMidLong[] = { // rdar://95015661
    { "AT",                 TEST_ULDN_REGION,   u"Аустрия" },
    { "AU",                 TEST_ULDN_REGION,   u"Аустралия" },
    { "053",                TEST_ULDN_REGION,   u"Аустралазия" },
    { "de_AT",              TEST_ULOC_LOCALE,   u"неміс тілі (Аустрия)" },
    { "en_AU",              TEST_ULOC_LOCALE,   u"ағылшын тілі (Аустралия)" },
	{ "cic",                TEST_ULDN_LOCALE,   u"чикасо тілі" }, // rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
    // rdar://95777359
    { "yue_Hans",           TEST_ULOC_LOCALE, u"кантон тілі (жеңілдетілген жазу)" },
    { "yue_Hant",           TEST_ULOC_LOCALE, u"кантон тілі (дәстүрлі жазу)" },
    { "Hans",               TEST_ULDN_SCRIPT, u"жеңілдетілген қытай иероглифы" },
    { "Hant",               TEST_ULDN_SCRIPT, u"дәстүрлі қытай иероглифы" },
    { "en_ZA",              TEST_ULOC_LOCALE, u"ағылшын тілі (Оңтүстік Африка)" },
};

static const UldnItem kk_DiaMidLong[] = { // rdar://95015661
    { "de_AT",              TEST_ULDN_LOCALE,   u"аустриялық неміс тілі" },
    { "en_AU",              TEST_ULDN_LOCALE,   u"аустралиялық ағылшын тілі" },
};

static const UldnItem nv_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "nv",                 TEST_ULOC_LOCALE, u"Diné Bizaad" },
};

static const UldnItem rhg_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "rhg",                TEST_ULOC_LOCALE, u"𐴌𐴗𐴥𐴝𐴙𐴚𐴒𐴙𐴝" },
};

static const UldnItem syr_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "syr",                TEST_ULOC_LOCALE, u"ܣܘܪܝܝܐ" },
};

static const UldnItem ti_StdMidLong[] = { // Apple <rdar:/74820154>
	{ "ti",                TEST_ULOC_LOCALE, u"ትግርኛ" },
};
    
static const UldnItem ar_StdMidLong[] = { // rdar://123305986 ([TextInputUI]: AB: DawnE21E212: <spelling issue> Slight different spelling for Cantonese)
    { "yue",               TEST_ULOC_LANGUAGE, u"الكانتونية" },
    { "yue",               TEST_ULDN_LANGUAGE, u"الكانتونية" },
};
#endif  // APPLE_ICU_CHANGES

static const UldnItem ro_StdMidLong[] = { // https://unicode-org.atlassian.net/browse/ICU-11563
	{ "mo",                     TEST_ULDN_LOCALE, u"română" },
	{ "mo_MD",                  TEST_ULDN_LOCALE, u"română (Republica Moldova)" },
	{ "mo",                     TEST_ULDN_LANGUAGE, u"română" },
	{ "mo_MD",                  TEST_ULOC_LOCALE, u"română (Republica Moldova)" },
	{ "mo",                     TEST_ULOC_LANGUAGE, u"română" },
};

static const UldnItem yi_StdMidLong[] = { // https://unicode-org.atlassian.net/browse/ICU-21742
	{ "ji",                     TEST_ULDN_LOCALE, u"ייִדיש" },
#if APPLE_ICU_CHANGES
// rdar://
	{ "ji_US",                  TEST_ULDN_LOCALE, u"ייִדיש (פֿ\"ש)" },
#else
	{ "ji_US",                  TEST_ULDN_LOCALE, u"ייִדיש (פֿאַראייניגטע שטאַטן)" },
#endif  // APPLE_ICU_CHANGES
	{ "ji",                     TEST_ULDN_LANGUAGE, u"ייִדיש" },
	{ "ji_US",                  TEST_ULOC_LOCALE, u"ייִדיש (פֿאַראייניגטע שטאַטן)" },
	{ "ji",                     TEST_ULOC_LANGUAGE, u"ייִדיש" },
};

static const UldnItem zh_DiaMidLong[] = {
    // zh and zh_Hant both have dialect names for the following in ICU 73
    { "ar_001",                 TEST_ULDN_LOCALE, u"现代标准阿拉伯语" },
    { "nl_BE",                  TEST_ULDN_LOCALE, u"弗拉芒语" },
    { "ro_MD",                  TEST_ULDN_LOCALE, u"摩尔多瓦语" },
    // zh has dialect names for the following in ICU 73
    { "en_AU",                  TEST_ULDN_LOCALE, u"澳大利亚英语" },
    { "en_CA",                  TEST_ULDN_LOCALE, u"加拿大英语" },
    { "en_GB",                  TEST_ULDN_LOCALE, u"英国英语" },
    { "en_US",                  TEST_ULDN_LOCALE, u"美国英语" },
    { "es_419",                 TEST_ULDN_LOCALE, u"拉丁美洲西班牙语" },
    { "es_ES",                  TEST_ULDN_LOCALE, u"欧洲西班牙语" },
    { "es_MX",                  TEST_ULDN_LOCALE, u"墨西哥西班牙语" },
    { "fr_CA",                  TEST_ULDN_LOCALE, u"加拿大法语" },
    { "fr_CH",                  TEST_ULDN_LOCALE, u"瑞士法语" },
};

static const UldnItem zh_Hant_DiaMidLong[] = {
    // zh and zh_Hant both have dialect names for the following in ICU 73
    { "ar_001",                 TEST_ULDN_LOCALE, u"現代標準阿拉伯文" },
    { "nl_BE",                  TEST_ULDN_LOCALE, u"法蘭德斯文" },
    { "ro_MD",                  TEST_ULDN_LOCALE, u"摩爾多瓦文" },
    // zh_Hant no dialect names for the following in ICU-73,
    // use standard name
#if APPLE_ICU_CHANGES // rdar://
    { "en_AU",                  TEST_ULDN_LOCALE, u"澳洲英文" },
    { "en_CA",                  TEST_ULDN_LOCALE, u"加拿大英文" },
    { "en_GB",                  TEST_ULDN_LOCALE, u"英式英文" },
    { "en_US",                  TEST_ULDN_LOCALE, u"美式英文" },
#else
    { "en_AU",                  TEST_ULDN_LOCALE, u"英文（澳洲）" },
    { "en_CA",                  TEST_ULDN_LOCALE, u"英文（加拿大）" },
    { "en_GB",                  TEST_ULDN_LOCALE, u"英文（英國）" },
    { "en_US",                  TEST_ULDN_LOCALE, u"英文（美國）" },
#endif // APPLE_ICU_CHANGES
    { "es_419",                 TEST_ULDN_LOCALE, u"西班牙文（拉丁美洲）" },
    { "es_ES",                  TEST_ULDN_LOCALE, u"西班牙文（西班牙）" },
    { "es_MX",                  TEST_ULDN_LOCALE, u"西班牙文（墨西哥）" },
#if APPLE_ICU_CHANGES // rdar://
    { "fr_CA",                  TEST_ULDN_LOCALE, u"加拿大法文" },
    { "fr_CH",                  TEST_ULDN_LOCALE, u"瑞士法文" },
#else
    { "fr_CA",                  TEST_ULDN_LOCALE, u"法文（加拿大）" },
    { "fr_CH",                  TEST_ULDN_LOCALE, u"法文（瑞士）" },
#endif // APPLE_ICU_CHANGES
};

#if APPLE_ICU_CHANGES
// rdar://
static const UldnItem apw_StdMidLong[] = { // rdar://89394823 rdar://94490599
	{ "apw",                TEST_ULOC_LOCALE, u"Nṉee biyátiʼ" },
	{ "Latn",               TEST_ULDN_SCRIPT, u"Latin" },
	{ "apw_Latn",           TEST_ULDN_LOCALE, u"Nṉee biyátiʼ" },
};

static const UldnItem ber_StdMidLong[] = { // rdar://104877633
	{ "ber",                TEST_ULOC_LOCALE, u"ⴰⵎⴰⵣⵉⵖ" },
};

static const UldnItem cic_StdMidLong[] = { // rdar://100483742 rdar://100483742
	{ "cic",                TEST_ULOC_LOCALE, u"Chikashshanompaʼ" },
	{ "Latn",               TEST_ULDN_SCRIPT, u"Latin" },
	{ "cic_Latn",           TEST_ULDN_LOCALE, u"Chikashshanompaʼ" },
};

static const UldnItem cho_StdMidLong[] = { // rdar://103954992 rdar://100483742
	{ "cho",                TEST_ULOC_LOCALE, u"Chahta" },
	{ "Latn",               TEST_ULDN_SCRIPT, u"Latin" },
	{ "cho_Latn",           TEST_ULDN_LOCALE, u"Chahta" },
};

static const UldnItem dz_StdMidLong[] = { // rdar://89394823
	{ "dz",                TEST_ULOC_LOCALE, u"རྫོང་ཁ་" },
};

static const UldnItem el_StdMidLong[] = { // rdar://84190308, rdar://89394823, rdar://59762233
	{ "NL",                 TEST_ULDN_REGION, u"Κάτω Χώρες" },
	{ "apw",                TEST_ULOC_LOCALE, u"Απάτσι, Δυτικοί" },
	{ "guc",                TEST_ULOC_LOCALE, u"Ουαγού" },
    { "sjd",                TEST_ULOC_LOCALE, u"Κίλντιν Σάμι" }, // rdar://117588343
    { "sje",                TEST_ULOC_LOCALE, u"Πίτε Σάμι" }, // rdar://117588343
    { "sju",                TEST_ULOC_LOCALE, u"Ούμε Σάμι" }, // rdar://117588343
};

static const UldnItem hmn_StdMidLong[] = { // rdar://104877633
	{ "hmn",                TEST_ULOC_LOCALE, u"𖬌𖬣𖬵" },
};

static const UldnItem inh_StdMidLong[] = { // rdar://109529736
    { "inh",                TEST_ULOC_LOCALE, u"ГӀалгӀай мотт" },
};
    
static const UldnItem sjd_StdMidLong[] = { // rdar://117588343
    { "sjd",                TEST_ULOC_LOCALE, u"Кӣллт са̄мь кӣлл" },
};
        
static const UldnItem sje_StdMidLong[] = { // rdar://117588343
    { "sje",                TEST_ULOC_LOCALE, u"Bidumsámegiella" },
};
        
static const UldnItem sju_StdMidLong[] = { // rdar://117588343
    { "sju",                TEST_ULOC_LOCALE, u"Ubmejesámiengiälla" },
};
        
static const UldnItem osa_StdMidLong[] = { // rdar://109529736
    { "osa",                TEST_ULOC_LOCALE, u"𐓏𐓘𐓻𐓘𐓻𐓟" },
};


static const UldnItem it_StdMidLong[] = { // rdar://85413154
	{ "SH",                 TEST_ULDN_REGION, u"Sant'Elena" },
	{ "CI",                 TEST_ULDN_REGION, u"Côte d'Ivoire" },
	{ "qug@currency=ECS",   TEST_ULOC_LOCALE, u"quechua dell'altopiano del Chimborazo (Valuta=sucre dell'Ecuador)" },
};

static const UldnItem mic_StdMidLong[] = { // rdar://104877633
	{ "mic",                TEST_ULOC_LOCALE, u"Lʼnuiʼsuti" },
};

static const UldnItem mid_StdMidLong[] = { // rdar://104877633
	{ "mid",                TEST_ULOC_LOCALE, u"ࡌࡀࡍࡃࡀࡉࡀ" },
};

static const UldnItem nnp_StdMidLong[] = { // rdar://104877633
	{ "nnp",                TEST_ULOC_LOCALE, u"𞋒𞋀𞋉𞋃𞋕" },
};

static const UldnItem pqm_StdMidLong[] = { // rdar://104877633
	{ "pqm",                TEST_ULOC_LOCALE, u"Wolastoqey" },
};

static const UldnItem rej_StdMidLong[] = { // rdar://104877633
	{"rej",                TEST_ULOC_LOCALE, u"Baso Hejang" },
};

static const UldnItem rej_Rjng_StdMidLong[] = { // rdar://104877633
	{ "rej",                TEST_ULOC_LOCALE, u"ꤷꤼꥋ ꤽꥍꤺꥏ" },
};

static const UldnItem sm_StdMidLong[] = { // rdar://89394823
	{ "sm",                TEST_ULOC_LOCALE, u"Gagana faʻa Sāmoa" },
};

static const UldnItem no_StdLstLong[] = { // rdar://81296782
    { "en",                 TEST_ULDN_LANGUAGE, u"Engelsk" },
    { "sv",                 TEST_ULDN_LANGUAGE, u"Svensk" },
    { "no",                 TEST_ULDN_LANGUAGE, u"Norsk" },
    { "fr",                 TEST_ULDN_LANGUAGE, u"Fransk" },
    { "ja",                 TEST_ULDN_LANGUAGE, u"Japansk" },
    { "zh",                 TEST_ULDN_LANGUAGE, u"Kinesisk" },
};
#endif  // APPLE_ICU_CHANGES

static const UldnLocAndOpts uldnLocAndOpts[] = {
    { "en", optStdMidLong,      en_StdMidLong,      UPRV_LENGTHOF(en_StdMidLong) },
    { "en", optStdMidShrt,      en_StdMidShrt,      UPRV_LENGTHOF(en_StdMidShrt) },
    { "en", optDiaMidLong,      en_DiaMidLong,      UPRV_LENGTHOF(en_DiaMidLong) },
    { "en", optDiaMidShrt,      en_DiaMidShrt,      UPRV_LENGTHOF(en_DiaMidShrt) },
    { "ro", optStdMidLong,      ro_StdMidLong,      UPRV_LENGTHOF(ro_StdMidLong) },
    { "yi", optStdMidLong,      yi_StdMidLong,      UPRV_LENGTHOF(yi_StdMidLong) },
    { "zh", optDiaMidLong,      zh_DiaMidLong,      UPRV_LENGTHOF(zh_DiaMidLong) },
    { "zh_Hant", optDiaMidLong, zh_Hant_DiaMidLong, UPRV_LENGTHOF(zh_Hant_DiaMidLong) },
#if APPLE_ICU_CHANGES
//// rdar://
    // Apple additions
    { "en", optStdMidVrnt, en_StdMidVrnt, UPRV_LENGTHOF(en_StdMidVrnt) },
    { "en", optStdMidPrc, en_StdMidPrc, UPRV_LENGTHOF(en_StdMidPrc) }, // rdar://115264744
    { "en", optStdLstLong, en_StdLstLong, UPRV_LENGTHOF(en_StdLstLong) }, // rdar://115264744
    { "en", optStdLstPrc, en_StdLstPrc, UPRV_LENGTHOF(en_StdLstPrc) }, // rdar://120926070
    { "en_CA", optDiaMidLong, en_CA_DiaMidLong, UPRV_LENGTHOF(en_CA_DiaMidLong) }, // rdar://60916890
    { "en_CA", optDiaMidShrt, en_CA_DiaMidShrt, UPRV_LENGTHOF(en_CA_DiaMidShrt) }, // rdar://60916890
    { "en_GB", optDiaMidLong, en_GB_DiaMidLong, UPRV_LENGTHOF(en_GB_DiaMidLong) }, // rdar://60916890
    { "en_GB", optDiaMidShrt, en_GB_DiaMidShrt, UPRV_LENGTHOF(en_GB_DiaMidShrt) }, // rdar://60916890
    { "en_IN", optStdMidLong, en_IN_StdMidLong, UPRV_LENGTHOF(en_IN_StdMidLong) }, // rdar://65959353
    { "ber", optStdMidLong, ber_StdMidLong, UPRV_LENGTHOF(ber_StdMidLong) }, // rdar://104877633
	{ "fi", optStdMidLong, fi_StdMidLong, UPRV_LENGTHOF(fi_StdMidLong) }, // rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
	{ "fi", optStdLstLong, fi_StdBegLong, UPRV_LENGTHOF(fi_StdBegLong) }, // rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
    { "fr", optStdMidLong, fr_StdMidLong, UPRV_LENGTHOF(fr_StdMidLong) },
    { "fr", optStdMidPrc, fr_StdMidPrc, UPRV_LENGTHOF(fr_StdMidPrc) }, // rdar://115264744
    { "fr", optStdMidShrt, fr_StdMidShrt, UPRV_LENGTHOF(fr_StdMidShrt) },
    { "fr", optStdBegLong, fr_StdBegLong, UPRV_LENGTHOF(fr_StdBegLong) },
    { "fr", optStdLstLong, fr_StdLstLong, UPRV_LENGTHOF(fr_StdLstLong) },
    { "fr_CA", optStdLstLong, fr_StdLstLong, UPRV_LENGTHOF(fr_StdLstLong) },
    { "fr", optDiaMidLong, fr_DiaMidLong, UPRV_LENGTHOF(fr_DiaMidLong) },
    { "ca", optStdLstLong, ca_StdLstLong, UPRV_LENGTHOF(ca_StdLstLong) },
	{ "cho", optStdMidLong, cho_StdMidLong, UPRV_LENGTHOF(cho_StdMidLong) },
	{ "cic", optStdMidLong, cic_StdMidLong, UPRV_LENGTHOF(cic_StdMidLong) },
	{ "hmn", optStdMidLong, hmn_StdMidLong, UPRV_LENGTHOF(hmn_StdMidLong) }, // rdar://104877633
    { "inh", optStdMidLong, inh_StdMidLong, UPRV_LENGTHOF(inh_StdMidLong) }, // rdar://109529736
    { "sjd", optStdMidLong, sjd_StdMidLong, UPRV_LENGTHOF(inh_StdMidLong) }, // rdar://117588343
    { "sje", optStdMidLong, sje_StdMidLong, UPRV_LENGTHOF(inh_StdMidLong) }, // rdar://117588343
    { "sju", optStdMidLong, sju_StdMidLong, UPRV_LENGTHOF(inh_StdMidLong) }, // rdar://117588343
    { "osa", optStdMidLong, osa_StdMidLong, UPRV_LENGTHOF(osa_StdMidLong) }, // rdar://111138831
    { "nb", optStdMidLong, nb_StdMidLong, UPRV_LENGTHOF(nb_StdMidLong) }, // rdar://65008672
	{ "ru", optStdMidLong, ru_StdMidLong, UPRV_LENGTHOF(ru_StdMidLong) }, // rdar://102511347 Incorrect character case for language names in dictionary settings; some tests for 'cic'
    { "ur", optStdMidLong,      ur_StdMidLong,      UPRV_LENGTHOF(ur_StdMidLong) },
    { "ur_Arab", optStdMidLong, ur_StdMidLong,      UPRV_LENGTHOF(ur_StdMidLong) },
    { "ur_Aran", optStdMidLong, ur_StdMidLong,      UPRV_LENGTHOF(ur_StdMidLong) },
    { "pa_Arab", optStdMidLong, pa_Arab_StdMidLong, UPRV_LENGTHOF(pa_Arab_StdMidLong) },
    { "pa_Aran", optStdMidLong, pa_Arab_StdMidLong, UPRV_LENGTHOF(pa_Arab_StdMidLong) },
    { "ru", optStdMidLong,      ru_StdMidLong,      UPRV_LENGTHOF(ru_StdMidLong) },
    { "zh", optStdMidLong,      zh_StdMidLong,        UPRV_LENGTHOF(zh_StdMidLong) },
    { "zh_Hant", optStdMidLong, zh_Hant_StdMidLong,   UPRV_LENGTHOF(zh_Hant_StdMidLong) },
    { "zh_Hant_HK", optStdMidLong, zh_Hant_HK_StdMidLong, UPRV_LENGTHOF(zh_Hant_HK_StdMidLong) },
    { "yue", optStdMidLong,      yue_StdMidLong,      UPRV_LENGTHOF(yue_StdMidLong) },
    { "yue_Hans", optStdMidLong, yue_Hans_StdMidLong, UPRV_LENGTHOF(yue_Hans_StdMidLong) },
    { "nds", optStdMidLong, nds_StdMidLong, UPRV_LENGTHOF(nds_StdMidLong) },
    { "hi", optStdMidLong, hi_StdMidLong, UPRV_LENGTHOF(hi_StdMidLong) },
    { "hi_Latn", optStdMidLong, hi_Latn_StdMidLong, UPRV_LENGTHOF(hi_Latn_StdMidLong) },
    { "mic", optStdMidLong, mic_StdMidLong, UPRV_LENGTHOF(mic_StdMidLong) }, // rdar://104877633
    { "mid", optStdMidLong, mid_StdMidLong, UPRV_LENGTHOF(mid_StdMidLong) }, // rdar://104877633
    { "mni_Beng", optStdMidLong, mni_Beng_StdMidLong, UPRV_LENGTHOF(mni_Beng_StdMidLong) },
    { "mni_Mtei", optStdMidLong, mni_Mtei_StdMidLong, UPRV_LENGTHOF(mni_Mtei_StdMidLong) },
    { "sat_Olck", optStdMidLong, sat_Olck_StdMidLong, UPRV_LENGTHOF(sat_Olck_StdMidLong) },
    { "sat_Deva", optStdMidLong, sat_Deva_StdMidLong, UPRV_LENGTHOF(sat_Deva_StdMidLong) },
    { "en_AU", optStdBegLong, en_AU_StdBegLong, UPRV_LENGTHOF(en_AU_StdBegLong) }, // rdar://69835367
    { "es_MX", optStdBegLong, es_MX_StdBegLong, UPRV_LENGTHOF(es_MX_StdBegLong) }, // rdar://69835367
    { "ms", optStdMidLong, ms_StdMidLong, UPRV_LENGTHOF(ms_StdMidLong) }, // rdar://66990121
    { "ain", optStdMidLong, ain_StdMidLong, UPRV_LENGTHOF(ain_StdMidLong) }, // rdar:/74820154
    { "am", optStdMidLong, am_StdMidLong, UPRV_LENGTHOF(am_StdMidLong) }, // rdar:/74820154
    { "ff_Adlm", optStdMidLong, ff_Adlm_StdMidLong, UPRV_LENGTHOF(ff_Adlm_StdMidLong) }, // rdar:/74820154
    { "ig", optStdMidLong, ig_StdMidLong, UPRV_LENGTHOF(ig_StdMidLong) }, // rdar:/74820154
    { "nv", optStdMidLong, nv_StdMidLong, UPRV_LENGTHOF(nv_StdMidLong) }, // rdar:/74820154
    { "nnp", optStdMidLong, nnp_StdMidLong, UPRV_LENGTHOF(nnp_StdMidLong) }, // rdar://104877633
    { "pqm", optStdMidLong, pqm_StdMidLong, UPRV_LENGTHOF(pqm_StdMidLong) }, // rdar://104877633
    { "rhg", optStdMidLong, rhg_StdMidLong, UPRV_LENGTHOF(rhg_StdMidLong) }, // rdar:/74820154
    { "rej", optStdMidLong, rej_StdMidLong, UPRV_LENGTHOF(rej_StdMidLong) }, // rdar://104877633
    { "rej_Rjng", optStdMidLong, rej_Rjng_StdMidLong, UPRV_LENGTHOF(rej_Rjng_StdMidLong) }, // rdar://104877633	
    { "syr", optStdMidLong, syr_StdMidLong, UPRV_LENGTHOF(syr_StdMidLong) }, // rdar:/74820154
    { "ti", optStdMidLong, ti_StdMidLong, UPRV_LENGTHOF(ti_StdMidLong) }, // rdar:/74820154
    { "apw", optStdMidLong, apw_StdMidLong, UPRV_LENGTHOF(apw_StdMidLong) }, // rdar:/89394823
    { "dz", optStdMidLong, dz_StdMidLong, UPRV_LENGTHOF(dz_StdMidLong) }, // rdar:/89394823
    { "sm", optStdMidLong, sm_StdMidLong, UPRV_LENGTHOF(sm_StdMidLong) }, // rdar:/89394823
    { "el", optStdMidLong, el_StdMidLong, UPRV_LENGTHOF(el_StdMidLong) }, // rdar:/84190308
    { "it", optStdMidLong, it_StdMidLong, UPRV_LENGTHOF(it_StdMidLong) }, // rdar:/85413154
    { "no", optStdLstLong, no_StdLstLong, UPRV_LENGTHOF(no_StdLstLong) }, // rdar://81296782
    { "kk", optStdMidLong, kk_StdMidLong, UPRV_LENGTHOF(kk_StdMidLong) }, // rdar://95015661
    { "kk", optDiaMidLong, kk_DiaMidLong, UPRV_LENGTHOF(kk_DiaMidLong) }, // rdar://95015661
    { "ar", optStdMidLong, ar_StdMidLong, UPRV_LENGTHOF(ar_StdMidLong) }, // rdar://123305986
#endif  // APPLE_ICU_CHANGES
    { NULL, NULL, NULL, 0 }
};

enum { kUNameBuf = 128, kBNameBuf = 256 };

static void TestUldnNameVariants() {
    const UldnLocAndOpts * uloPtr;
    for (uloPtr = uldnLocAndOpts; uloPtr->displayLocale != NULL; uloPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames * uldn = uldn_openForContext(uloPtr->displayLocale, (UDisplayContext*)uloPtr->displayOptions, 3, &status);
        if (U_FAILURE(status)) {
            log_data_err("uldn_openForContext fails, displayLocale %s, contexts %03X %03X %03X: %s - Are you missing data?\n",
                    uloPtr->displayLocale, uloPtr->displayOptions[0], uloPtr->displayOptions[1], uloPtr->displayOptions[2],
                    u_errorName(status) );
            continue;
        }
        // API coverage: Expect to get back the dialect handling which is
        // the first item in the displayOptions test data.
        UDialectHandling dh = uldn_getDialectHandling(uldn);
        UDisplayContext dhContext = (UDisplayContext)dh;  // same numeric values
        if (dhContext != uloPtr->displayOptions[0]) {
            log_err("uldn_getDialectHandling()=%03X != expected UDisplayContext %03X\n",
                    dhContext, uloPtr->displayOptions[0]);
        }
        const UldnItem * itemPtr = uloPtr->testItems;
        int32_t itemCount = uloPtr->countItems;
        for (; itemCount-- > 0; itemPtr++) {
            UChar uget[kUNameBuf];
            int32_t ulenget, ulenexp;
            const char* typeString;
            status = U_ZERO_ERROR;
            switch (itemPtr->nameType) {
                case TEST_ULDN_LOCALE:
                    ulenget = uldn_localeDisplayName(uldn, itemPtr->localeToName, uget, kUNameBuf, &status);
                    typeString = "uldn_localeDisplayName";
                    break;
                case TEST_ULDN_LANGUAGE:
                    ulenget = uldn_languageDisplayName(uldn, itemPtr->localeToName, uget, kUNameBuf, &status);
                    typeString = "uldn_languageDisplayName";
                  break;
                case TEST_ULDN_SCRIPT:
                    ulenget = uldn_scriptDisplayName(uldn, itemPtr->localeToName, uget, kUNameBuf, &status);
                    typeString = "uldn_scriptDisplayName";
                    break;
                case TEST_ULDN_REGION:
                    ulenget = uldn_regionDisplayName(uldn, itemPtr->localeToName, uget, kUNameBuf, &status);
                    typeString = "uldn_regionDisplayName";
                    break;
                case TEST_ULOC_LOCALE:
                    ulenget = uloc_getDisplayName(itemPtr->localeToName, uloPtr->displayLocale, uget, kUNameBuf, &status);
                    typeString = "uloc_getDisplayName";
                    break;
                case TEST_ULOC_LANGUAGE:
                    ulenget = uloc_getDisplayLanguage(itemPtr->localeToName, uloPtr->displayLocale, uget, kUNameBuf, &status);
                    typeString = "uloc_getDisplayLanguage";
                    break;
                case TEST_ULOC_SCRIPT:
                    ulenget = uloc_getDisplayScript(itemPtr->localeToName, uloPtr->displayLocale, uget, kUNameBuf, &status);
                    typeString = "uloc_getDisplayScript";
                    break;
                case TEST_ULOC_REGION:
                    ulenget = uloc_getDisplayCountry(itemPtr->localeToName, uloPtr->displayLocale, uget, kUNameBuf, &status);
                    typeString = "uloc_getDisplayCountry";
                    break;
                default:
                    continue;
            }
            if (U_FAILURE(status)) {
                log_data_err("%s fails, displayLocale %s, contexts %03X %03X %03X, localeToName %s: %s\n",
                        typeString, uloPtr->displayLocale, uloPtr->displayOptions[0], uloPtr->displayOptions[1], uloPtr->displayOptions[2],
                        itemPtr->localeToName, u_errorName(status) );
                continue;
            }
            ulenexp = u_strlen(itemPtr->expectResult);
            if (ulenget != ulenexp || u_strncmp(uget, itemPtr->expectResult, ulenexp) != 0) {
                char bexp[kBNameBuf], bget[kBNameBuf];
                u_strToUTF8(bexp, kBNameBuf, NULL, itemPtr->expectResult, ulenexp, &status);
                u_strToUTF8(bget, kBNameBuf, NULL, uget, ulenget, &status);
                log_data_err("%s fails, displayLocale %s, contexts %03X %03X %03X, localeToName %s:\n    expect %2d: %s\n    get    %2d: %s\n",
                        typeString, uloPtr->displayLocale, uloPtr->displayOptions[0], uloPtr->displayOptions[1], uloPtr->displayOptions[2],
                        itemPtr->localeToName, ulenexp, bexp, ulenget, bget );
            }
        }

        uldn_close(uldn);
    }
}
#endif

static void TestUsingDefaultWarning() {
    UChar buff[256];
    char errorOutputBuff[256];
    UErrorCode status = U_ZERO_ERROR;
    const char* language = "jJj";
    int32_t length = uloc_getDisplayLanguage(language, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"jjj") != 0 ||
        length != 3) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayLanguage %s return len:%d %s with status %d %s\n",
                language, length, errorOutputBuff, status, myErrorName(status));
    }

    status = U_ZERO_ERROR;
    const char* script = "und-lALA";
    length = uloc_getDisplayScript(script, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"Lala") != 0 ||
        length != 4) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayScript %s return len:%d %s with status %d %s\n",
                script, length, errorOutputBuff, status, myErrorName(status));
    }

    status = U_ZERO_ERROR;
    const char* region = "und-wt";
    length = uloc_getDisplayCountry(region, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"WT") != 0 ||
        length != 2) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayCountry %s return len:%d %s with status %d %s\n",
                region, length, errorOutputBuff, status, myErrorName(status));
    }

    status = U_ZERO_ERROR;
    const char* variant = "und-abcde";
    length = uloc_getDisplayVariant(variant, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"ABCDE") != 0 ||
        length != 5) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayVariant %s return len:%d %s with status %d %s\n",
                variant, length, errorOutputBuff, status, myErrorName(status));
    }

    status = U_ZERO_ERROR;
    const char* keyword = "postCODE";
    length = uloc_getDisplayKeyword(keyword, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"postCODE") != 0 ||
        length != 8) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayKeyword %s return len:%d %s with status %d %s\n",
                keyword, length, errorOutputBuff, status, myErrorName(status));
    }

    status = U_ZERO_ERROR;
    const char* keyword_value = "de_DE@postCode=fOObAR";
    length = uloc_getDisplayKeywordValue(keyword_value, keyword, "de", buff, 256, &status);
    if (status != U_USING_DEFAULT_WARNING ||
        u_strcmp(buff, u"fOObAR") != 0 ||
        length != 6) {
        u_UCharsToChars(buff, errorOutputBuff, length+1);
        log_err("ERROR: in uloc_getDisplayKeywordValue %s %s return len:%d %s with status %d %s\n",
                keyword_value, keyword, length, errorOutputBuff, status, myErrorName(status));
      }
}

// Test case for ICU-20575
// This test checks if the environment variable LANG is set, 
// and if so ensures that both C and C.UTF-8 cause ICU's default locale to be en_US_POSIX.
static void TestCDefaultLocale() {
    const char *defaultLocale = uloc_getDefault();
    char *env_var = getenv("LANG");
    if (env_var == NULL) {
      log_verbose("Skipping TestCDefaultLocale test, as the LANG variable is not set.");
      return;
    }
    if (getenv("LC_ALL") != NULL) {
      log_verbose("Skipping TestCDefaultLocale test, as the LC_ALL variable is set.");
      return;
    }
    if ((strcmp(env_var, "C") == 0 || strcmp(env_var, "C.UTF-8") == 0) && strcmp(defaultLocale, "en_US_POSIX") != 0) {
      log_err("The default locale for LANG=%s should be en_US_POSIX, not %s\n", env_var, defaultLocale);
    }
}

// Test case for ICU-21449
static void TestBug21449InfiniteLoop() {
    UErrorCode status = U_ZERO_ERROR;
    const char* invalidLocaleId = RES_PATH_SEPARATOR_S;

    // The issue causes an infinite loop to occur when looking up a non-existent resource for the invalid locale ID,
    // so the test is considered passed if the call to the API below returns anything at all.
    uloc_getDisplayLanguage(invalidLocaleId, invalidLocaleId, NULL, 0, &status);
}

// rdar://79296849 and https://unicode-org.atlassian.net/browse/ICU-21639
static void TestExcessivelyLongIDs(void) {
    const char* reallyLongID =
        "de-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric"
        "-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix";
    char minimizedID[ULOC_FULLNAME_CAPACITY];
    char maximizedID[ULOC_FULLNAME_CAPACITY];
    int32_t actualMinimizedLength = 0;
    int32_t actualMaximizedLength = 0;
    UErrorCode err = U_ZERO_ERROR;
    
    actualMinimizedLength = uloc_minimizeSubtags(reallyLongID, minimizedID, ULOC_FULLNAME_CAPACITY, &err);
    assertTrue("uloc_minimizeSubtags() with too-small buffer didn't fail as expected",
            U_FAILURE(err) && actualMinimizedLength > ULOC_FULLNAME_CAPACITY);
    
    err = U_ZERO_ERROR;
    actualMaximizedLength = uloc_addLikelySubtags(reallyLongID, maximizedID, ULOC_FULLNAME_CAPACITY, &err);
    assertTrue("uloc_addLikelySubtags() with too-small buffer didn't fail as expected",
            U_FAILURE(err) && actualMaximizedLength > ULOC_FULLNAME_CAPACITY);
    
    err = U_ZERO_ERROR;
    char* realMinimizedID = (char*)uprv_malloc(actualMinimizedLength + 1);
    uloc_minimizeSubtags(reallyLongID, realMinimizedID, actualMinimizedLength + 1, &err);
    if (assertSuccess("uloc_minimizeSubtags() failed", &err)) {
        assertEquals("Wrong result from uloc_minimizeSubtags()",
                     "de__POSIX@colstrength=primary;currency=eur;em=default;hours=h23;lb=strict;"
                         "lw=normal;measure=metric;numbers=latn;rg=atzzzz;sd=atat1;ss=none;timezone=Europe/Vienna",
                     realMinimizedID);
    }
    uprv_free(realMinimizedID);

    char* realMaximizedID = (char*)uprv_malloc(actualMaximizedLength + 1);
    uloc_addLikelySubtags(reallyLongID, realMaximizedID, actualMaximizedLength + 1, &err);
    if (assertSuccess("uloc_addLikelySubtags() failed", &err)) {
        assertEquals("Wrong result from uloc_addLikelySubtags()",
                     "de_Latn_DE_POSIX@colstrength=primary;currency=eur;em=default;hours=h23;lb=strict;"
                         "lw=normal;measure=metric;numbers=latn;rg=atzzzz;sd=atat1;ss=none;timezone=Europe/Vienna",
                     realMaximizedID);
    }
    uprv_free(realMaximizedID);
}

#if APPLE_ICU_CHANGES
// rdar://
#define ULOC_UND_TESTNUM 9

static const char* for_empty[ULOC_UND_TESTNUM] = { // ""
    "",                 // uloc_getName
    "",                 // uloc_getLanguage
    "en_Latn_US",       // uloc_addLikelySubtags // rdar://114361374 revert to current open ICU behavior for mapping ""
    "en",               // uloc_minimizeSubtags // rdar://114361374 revert to current open ICU behavior for mapping ""
    "",                 // uloc_canonicalize // rdar://114361374 revert to current open ICU behavior for mapping ""
    "",                 // uloc_getParent
    "und",              // uloc_toLanguageTag
    "",                 // uloc_getDisplayName in en
    "",                 // uloc_getDisplayLanguage in en
};
static const char* for_root[ULOC_UND_TESTNUM] = { // "root"
    "root",             // uloc_getName
    "root",             // uloc_getLanguage
    "root",             // uloc_addLikelySubtags
    "root",             // uloc_minimizeSubtags
    "root",             // uloc_canonicalize
    "",                 // uloc_getParent
    "root",             // uloc_toLanguageTag
    "Root",             // uloc_getDisplayName in en
    "Root",             // uloc_getDisplayLanguage in en
};
static const char* for_und[ULOC_UND_TESTNUM] = { // "und"
    "und",              // uloc_getName
    "und",              // uloc_getLanguage
    "en_Latn_US",       // uloc_addLikelySubtags
    "en",               // uloc_minimizeSubtags
    "und",              // uloc_canonicalize
    "",                 // uloc_getParent
    "und",              // uloc_toLanguageTag
    "Unknown language", // uloc_getDisplayName in en
    "Unknown language", // uloc_getDisplayLanguage in en
};
static const char* for_und_ZZ[ULOC_UND_TESTNUM] = { // "und_ZZ"
    "und_ZZ",           // uloc_getName
    "und",              // uloc_getLanguage
    "en_Latn_US",       // uloc_addLikelySubtags
    "en",               // uloc_minimizeSubtags
    "und_ZZ",           // uloc_canonicalize
    "und",              // uloc_getParent
    "und-ZZ",           // uloc_toLanguageTag
    "Unknown language (Unknown Region)", // uloc_getDisplayName in en
    "Unknown language", // uloc_getDisplayLanguage in en
};
static const char* for_empty_ZZ[ULOC_UND_TESTNUM] = { // "_ZZ"
    "_ZZ",              // uloc_getName
    "",                 // uloc_getLanguage
    "en_Latn_US",       // uloc_addLikelySubtags
    "en",               // uloc_minimizeSubtags
    "_ZZ",              // uloc_canonicalize
    "",                 // uloc_getParent
    "und-ZZ",           // uloc_toLanguageTag
    "Unknown Region",   // uloc_getDisplayName in en
    "",                 // uloc_getDisplayLanguage in en
};

typedef struct {
    const char * locale;
    const char ** expResults;
} RootUndEmptyItem;

static const RootUndEmptyItem rootUndEmptryItems[] = {
    { "",       for_empty },
    { "root",   for_root },
    { "und",    for_und },
    { "und_ZZ", for_und_ZZ },
    { "_ZZ",    for_empty_ZZ },
    { NULL, NULL }
};

enum { kULocMax = 64, kBLocMax = 128 };

static void TestRootUndEmpty() {
    const RootUndEmptyItem* itemPtr;
    for (itemPtr = rootUndEmptryItems; itemPtr->locale != NULL; itemPtr++) {
        const char* loc = itemPtr->locale;
        const char** expResultsPtr = itemPtr->expResults;
        const char* bexp;
        char bget[kBLocMax];
        UChar uexp[kULocMax];
        UChar uget[kULocMax];
        int32_t ulen, blen;
        UErrorCode status;

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_getName(loc, bget, kBLocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_getName status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_getName expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_getLanguage(loc, bget, kBLocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_getLanguage status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_getLanguage expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_addLikelySubtags(loc, bget, kBLocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_addLikelySubtags status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_addLikelySubtags expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_minimizeSubtags(loc, bget, kBLocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_minimizeSubtags status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_minimizeSubtags expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_canonicalize(loc, bget, kBLocMax, &status);
         if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_canonicalize status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_canonicalize expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_getParent(loc, bget, kBLocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_getParent status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_getParent expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        blen = uloc_toLanguageTag(loc, bget, kBLocMax, true, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_toLanguageTag status: %s\n", loc, u_errorName(status) );
        } else if (uprv_strcmp(bget, bexp) != 0) {
            log_err("loc \"%s\", uloc_toLanguageTag expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }

        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        u_unescape(bexp, uexp, kULocMax);
        uexp[kULocMax-1] = 0; // force zero term
        ulen = uloc_getDisplayName(loc, "en", uget, kULocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_getDisplayName en status: %s\n", loc, u_errorName(status) );
        } else if (u_strcmp(uget, uexp) != 0) {
            u_austrncpy(bget, uget, kBLocMax);
            bget[kBLocMax-1] = 0;
            log_err("loc \"%s\", uloc_getDisplayName en expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }
        
        status = U_ZERO_ERROR;
        bexp = *expResultsPtr++;
        u_unescape(bexp, uexp, kULocMax);
        uexp[kULocMax-1] = 0; // force zero term
        ulen = uloc_getDisplayLanguage(loc, "en", uget, kULocMax, &status);
        if (U_FAILURE(status)) {
            log_err("loc \"%s\", uloc_getDisplayLanguage en status: %s\n", loc, u_errorName(status) );
        } else if (u_strcmp(uget, uexp) != 0) {
            u_austrncpy(bget, uget, kBLocMax);
            bget[kBLocMax-1] = 0;
            log_err("loc \"%s\", uloc_getDisplayLanguage en expect \"%s\", get \"%s\"\n", loc, bexp, bget );
        }
   }
}


#if !U_PLATFORM_HAS_WIN32_API
/* Apple-specific, test for Apple-specific function ualoc_getAppleParent */
static const char* localesAndAppleParent[] = {
    "en",               "root",
    "en-US",            "en",
    "en-CA",            "en",
    "en-CN",            "en",
    "en-JP",            "en",
    "en-TW",            "en",
    "en-001",           "en",
    "en_001",           "en",
    "en-150",           "en_GB",
    "en-GB",            "en_001",
    "en_GB",            "en_001",
    "en-AU",            "en_GB",
    "en-BE",            "en_150",
    "en-DG",            "en_GB",
    "en-FK",            "en_GB",
    "en-GG",            "en_GB",
    "en-GI",            "en_GB",
    "en-HK",            "en_GB",
    "en-IE",            "en_GB",
    "en-IM",            "en_GB",
    "en-IN",            "en_GB",
    "en-IO",            "en_GB",
    "en-JE",            "en_GB",
    "en-JM",            "en_GB",
    "en-MO",            "en_GB",
    "en-MT",            "en_GB",
    "en-MV",            "en_GB",
    "en-NZ",            "en_AU",
    "en-PK",            "en_GB",
    "en-SG",            "en_GB",
    "en-SH",            "en_GB",
    "en-VG",            "en_GB",
    "es",               "root",
    "es-ES",            "es",
    "es-419",           "es",
    "es_419",           "es",
    "es-MX",            "es_419",
    "es-AR",            "es_419",
    "es-BR",            "es_419",
    "es-BZ",            "es_419",
    "es-AG",            "es_419",
    "es-AW",            "es_419",
    "es-CA",            "es_419",
    "es-CW",            "es_419",
    "es-SX",            "es_419",
    "es-TT",            "es_419",
    "fr",               "root",
    "fr-CA",            "fr",
    "fr-CH",            "fr",
    "haw",              "root",
    "nl",               "root",
    "nl-BE",            "nl",
    "pt",               "root",
    "pt-BR",            "pt",
    "pt-PT",            "pt",
    "pt-MO",            "pt_PT",
    "pt-CH",            "pt_PT",
    "pt-GQ",            "pt_PT",
    "pt-LU",            "pt_PT",
    "sr",               "root",
    "sr-Cyrl",          "sr",
    "sr-Latn",          "root",
    "tlh",              "root",
    "yue",              "yue_HK",
    "yue_Hans",         "yue_CN",
    "yue_Hant",         "yue_HK",
    "zh_CN",            "root",
    "zh-CN",            "root",
    "zh",               "zh_CN",
    "zh-Hans",          "zh",
    "zh_TW",            "root",
    "zh-TW",            "root",
    "zh-Hant",          "zh_TW",
    "zh_HK",            "zh_Hant_HK",
    "zh-HK",            "zh_Hant_HK",
    "zh_Hant",          "zh_TW",
    "zh-Hant-HK",       "zh_Hant",
    "zh_Hant_HK",       "zh_Hant",
    "zh-Hant-MO",       "zh_Hant_HK",
    "zh-Hans-HK",       "zh_Hans",
    "root",             "root",
    "en-Latn",          "en",
    "en-Latn-US",       "en_Latn",
    "en_US_POSIX",      "en_US",
    "en_Latn_US_POSIX", "en_Latn_US",
    "en-u-ca-hebrew",   "root",
    "en@calendar=hebrew", "root",
    "en_@calendar=hebrew", "root",
    "en-",              "root",
    "en_",              "root",
    "Default@2x",       "root",
    "default",          "root",
    NULL /* terminator */
};

static void TestGetAppleParent() {
    const char **localesPtr = localesAndAppleParent;
    const char * locale;
    while ((locale = *localesPtr++) != NULL) {
        const char * expectParent = *localesPtr++;
        UErrorCode status = U_ZERO_ERROR;
        char getParent[ULOC_FULLNAME_CAPACITY];
        int32_t plen = ualoc_getAppleParent(locale, getParent, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getAppleParent input \"%s\", status %s\n", locale, u_errorName(status));
        } else if (uprv_strcmp(expectParent, getParent) != 0) {
            log_err("FAIL: ualoc_getAppleParent input \"%s\", expected parent \"%s\", got parent \"%s\"\n", locale, expectParent, getParent);
        }
    }
}

/* Apple-specific, test for Apple-specific function ualoc_getLanguagesForRegion */
enum { kUALanguageEntryMin = 10, kUALanguageEntryMax = 20 };

static void TestGetLanguagesForRegion() {
    UALanguageEntry entries[kUALanguageEntryMax];
    int32_t entryCount;
    UErrorCode status;
    const char * region;

    status = U_ZERO_ERROR;
    region = "CN";
    entryCount = ualoc_getLanguagesForRegion(region, 0.001, entries, kUALanguageEntryMin, &status);
    if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, status %s\n", region, u_errorName(status));
    } else {
        // Expect approximately:
        // zh_Hans 0.90 UALANGSTATUS_OFFICIAL
        // wuu 0.06 Wu
        // yue_Hans 0.052
        // hsn 0.029 Xiang
        // hak 0.023 Hakka
        // nan 0.019 Minnan
        // gan 0.017 Gan
        // ii  0.006 Yi
        // ug_Arab 0.0055 Uighur UALANGSTATUS_REGIONAL_OFFICIAL
        // ...at least 4 more with fractions >= 0.001
        if (entryCount < kUALanguageEntryMin) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, entryCount %d is too small\n", region, entryCount);
        } else {
            UALanguageEntry* entryPtr = entries;
            if (uprv_strcmp(entryPtr->languageCode, "zh_Hans") != 0 || entryPtr->userFraction < 0.8 || entryPtr->userFraction > 1.0 || entryPtr->status != UALANGSTATUS_OFFICIAL) {
                log_err("FAIL: ualoc_getLanguagesForRegion %s, invalid entries[0] { %s, %.3f, %d }\n", region, entryPtr->languageCode, entryPtr->userFraction, (int)entryPtr->status);
            }
            for (entryPtr++; entryPtr < entries + kUALanguageEntryMin && uprv_strcmp(entryPtr->languageCode, "ug_Arab") != 0; entryPtr++)
                ;
            if (entryPtr < entries + kUALanguageEntryMin) {
                // we found ug_Arab, make sure it has correct status
                if (entryPtr->status != UALANGSTATUS_REGIONAL_OFFICIAL) {
                    log_err("FAIL: ualoc_getLanguagesForRegion %s, ug_Arab had incorrect status %d\n", (int)entryPtr->status);
                }
            } else {
                // did not find ug_Arab
                log_err("FAIL: ualoc_getLanguagesForRegion %s, entries did not include ug_Arab\n", region);
            }
        }
    }

    status = U_ZERO_ERROR;
    region = "CA";
    entryCount = ualoc_getLanguagesForRegion(region, 0.001, entries, kUALanguageEntryMin, &status);
    if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, status %s\n", region, u_errorName(status));
    } else {
        // Expect approximately:
        // en 0.86 UALANGSTATUS_OFFICIAL
        // fr 0.22 UALANGSTATUS_OFFICIAL
        // ...
        if (entryCount < 2) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, entryCount %d is too small\n", region, entryCount);
        } else {
            if (uprv_strcmp(entries[0].languageCode, "en") != 0 || entries[0].userFraction < 0.7 || entries[0].userFraction > 1.0 || entries[0].status != UALANGSTATUS_OFFICIAL) {
                log_err("FAIL: ualoc_getLanguagesForRegion %s, invalid entries[0] { %s, %.3f, %d }\n", region, entries[0].languageCode, entries[0].userFraction, (int)entries[0].status);
            }
            if (uprv_strcmp(entries[1].languageCode, "fr") != 0 || entries[1].userFraction < 0.1 || entries[1].userFraction > 1.0 || entries[1].status != UALANGSTATUS_OFFICIAL) {
                log_err("FAIL: ualoc_getLanguagesForRegion %s, invalid entries[1] { %s, %.3f, %d }\n", region, entries[1].languageCode, entries[1].userFraction, (int)entries[1].status);
            }
        }
    }

    status = U_ZERO_ERROR;
    region = "IN";
    entryCount = ualoc_getLanguagesForRegion(region, 0.001, NULL, 0, &status);
    if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, status %s\n", region, u_errorName(status));
    } else {
        if (entryCount < 40) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, entryCount %d is too small\n", region, entryCount);
        }
    }

    status = U_ZERO_ERROR;
    region = "FO";
    entryCount = ualoc_getLanguagesForRegion(region, 0.001, entries, kUALanguageEntryMin, &status);
    if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, status %s\n", region, u_errorName(status));
    } else {
        // Expect approximately:
        // fo 0.95 UALANGSTATUS_OFFICIAL
        // ...
        if (entryCount < 1) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, entryCount %d is too small\n", region, entryCount);
        } else {
            if (uprv_strcmp(entries[0].languageCode, "fo") != 0 || entries[0].userFraction < 0.90 || entries[0].userFraction > 0.98 || entries[0].status != UALANGSTATUS_OFFICIAL) {
                log_err("FAIL: ualoc_getLanguagesForRegion %s, invalid entries[0] { %s, %.3f, %d }\n", region, entries[0].languageCode, entries[0].userFraction, (int)entries[0].status);
            }
        }
    }

    status = U_ZERO_ERROR;
    region = "ID";
    entryCount = ualoc_getLanguagesForRegion(region, 0.01, entries, kUALanguageEntryMax, &status);
    if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s, status %s\n", region, u_errorName(status));
    } else {
        // Expect about 12 entries, the last should be for ms_Arab with a fraction ≈ 0.012 // rdar://27943264 and subsequent changes
        if (entryCount < 10) {
            log_err("FAIL: ualoc_getLanguagesForRegion %s with minFraction=0.01, entryCount %d is too small\n", region, entryCount);
        } else {
            if (uprv_strcmp(entries[entryCount-1].languageCode, "ms_Arab") != 0 || entries[entryCount-1].userFraction >= 0.02) {
                log_err("FAIL: ualoc_getLanguagesForRegion %s, invalid entries[entryCount-1] { %s, %.3f, %d }\n",
                    region, entries[entryCount-1].languageCode, entries[entryCount-1].userFraction, (int)entries[entryCount-1].status);
            }
        }
    }
}

static void TestGetRegionsForLanguage() {
    int32_t regionCount = 0;
    UARegionEntry regions[50];
    
    // test preflighting
    UErrorCode err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0, NULL, 0, &err);
    if (assertSuccess("Preflighting with no threshold failed", &err)) {
        assertIntEquals("Preflighting with no threshold returned wrong region count", 38, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0.5, NULL, 0, &err);
    if (assertSuccess("Preflighting with threshold of 0.5 failed", &err)) {
        assertIntEquals("Preflighting with threshold of 0.5 returned wrong region count", 21, regionCount);
    }
    
    // test zero capacity
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0, regions, 0, &err);
    if (assertSuccess("Passing capacity of 0 failed", &err)) {
        assertIntEquals("Passing capacity of 0 returned nonzero region count", 0, regionCount);
    }
    
    // test top 10
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0, regions, 10, &err);
    if (assertSuccess("Passing capacity of 10 failed", &err)) {
        assertIntEquals("Passing capacity of 10 returned wrong region count", 10, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertTrue("Wrong first region population share", regions[0].userFraction == 1.0);
        assertIntEquals("Wrong first region status", regions[0].status, UALANGSTATUS_OFFICIAL);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertTrue("Wrong second region population share", regions[1].userFraction == 1.0);
        assertIntEquals("Wrong second region status", regions[1].status, UALANGSTATUS_OFFICIAL);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
        assertTrue("Wrong tenth region population share", regions[9].userFraction == 0.86);
        assertIntEquals("Wrong tenth region status", regions[9].status, UALANGSTATUS_OFFICIAL);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0.5, regions, 10, &err);
    if (assertSuccess("Passing capacity of 10 and threshold of 0.5 failed", &err)) {
        assertIntEquals("Passing capacity of 10 and threshold of 0.5 returned wrong region count", 10, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
    }
    
    // test top 25
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0.5, regions, 25, &err);
    if (assertSuccess("Passing capacity of 25 and threshold of 0.5 failed", &err)) {
        assertIntEquals("Passing capacity of 25 and threshold of 0.5 rreturned wrong region count", 21, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertTrue("Wrong first region population share", regions[0].userFraction == 1.0);
        assertIntEquals("Wrong first region status", regions[0].status, UALANGSTATUS_OFFICIAL);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
        assertEquals("Wrong 21st region", "SD", regions[20].regionCode);
        assertTrue("Wrong 21st region population share", regions[20].userFraction == 0.61);
        assertIntEquals("Wrong 21st region status", regions[20].status, UALANGSTATUS_OFFICIAL);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0, regions, 25, &err);
    if (assertSuccess("Passing capacity of 25 failed", &err)) {
        assertIntEquals("Passing capacity of 25 returned wrong region count", 25, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
        assertEquals("Wrong 21st region", "SD", regions[20].regionCode);
        assertEquals("Wrong 25th region", "TD", regions[24].regionCode);
        assertTrue("Wrong 25th region population share", regions[24].userFraction == 0.16999999999999998);
        assertIntEquals("Wrong 25th region status", regions[24].status, UALANGSTATUS_OFFICIAL);
    }

    // test full list
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0.5, regions, 50, &err);
    if (assertSuccess("Passing capacity of 50 and threshold of 0.5 failed", &err)) {
        assertIntEquals("Passing capacity of 50 and threshold of 0.5 rreturned wrong region count", 21, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertTrue("Wrong first region population share", regions[0].userFraction == 1.0);
        assertIntEquals("Wrong first region status", regions[0].status, UALANGSTATUS_OFFICIAL);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
        assertEquals("Wrong 21st region", "SD", regions[20].regionCode);
        assertTrue("Wrong 21st region population share", regions[20].userFraction == 0.61);
        assertIntEquals("Wrong 21st region status", regions[20].status, UALANGSTATUS_OFFICIAL);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ar", 0, regions, 50, &err);
    if (assertSuccess("Passing capacity of 50 failed", &err)) {
        assertIntEquals("Passing capacity of 50 returned wrong region count", 38, regionCount);
        assertEquals("Wrong first region", "EH", regions[0].regionCode);
        assertEquals("Wrong second region", "JO", regions[1].regionCode);
        assertEquals("Wrong tenth region", "LB", regions[9].regionCode);
        assertEquals("Wrong 21st region", "SD", regions[20].regionCode);
        assertEquals("Wrong 25th region", "TD", regions[24].regionCode);
        assertTrue("Wrong 25th region population share", regions[24].userFraction == 0.16999999999999998);
        assertIntEquals("Wrong 25th region status", regions[24].status, UALANGSTATUS_OFFICIAL);
        assertEquals("Wrong 36th region", "NG", regions[35].regionCode);
        assertTrue("Wrong 36th region population share", regions[35].userFraction == 0.00071);
        assertIntEquals("Wrong 36th region status", regions[35].status, UALANGSTATUS_UNSPECIFIED);
    }
    
    // test language ID canonicalization
    // bsaeline values
    err = U_ZERO_ERROR;
    int32_t simplifiedChineseRegionCount = ualoc_getRegionsForLanguage("zh", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh'", 8, simplifiedChineseRegionCount);
    }
    err = U_ZERO_ERROR;
    int32_t traditionalChineseRegionCount = ualoc_getRegionsForLanguage("zh_Hant", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh_Hant' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh_Hant'", 15, traditionalChineseRegionCount);
    }
    // weird capitalization or punctuation
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ZH", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'ZH' failed", &err)) {
        assertIntEquals("Wrong region count for 'ZH'", simplifiedChineseRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-HANT", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-HANT' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-HANT'", traditionalChineseRegionCount, regionCount);
    }
    // redundant script code
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-hans", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-hans' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-hans'", simplifiedChineseRegionCount, regionCount);
    }
    // region code without script code
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-cn", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-cn' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-cn'", simplifiedChineseRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-tw", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-tw' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-tw'", traditionalChineseRegionCount, regionCount);
    }
    // both region code and script code
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-hans-cn", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-hans-cn' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-hans-cn'", simplifiedChineseRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-hant-tw", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-hant-tw' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-hant-tw'", traditionalChineseRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-hans-tw", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-hans-tw' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-hans-tw'", simplifiedChineseRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("zh-hant-cn", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'zh-hant-cn' failed", &err)) {
        assertIntEquals("Wrong region count for 'zh-hant-cn'", traditionalChineseRegionCount, regionCount);
    }
    // extra locale-ID parameters
    err = U_ZERO_ERROR;
    int32_t thaiRegionCount = ualoc_getRegionsForLanguage("th", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'th' failed", &err)) {
        assertIntEquals("Wrong region count for 'th'", 1, thaiRegionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("th_Thai_TH@calendar=buddhist", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'th_Thai_TH@calendar=buddhist' failed", &err)) {
        assertIntEquals("Wrong region count for 'th_Thai_TH@calendar=buddhist'", thaiRegionCount, regionCount);
    }
    // aliased locale ID
    err = U_ZERO_ERROR;
    int32_t norwegianRegionCount = ualoc_getRegionsForLanguage("nb", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'nb' failed", &err)) {
        assertIntEquals("Wrong region count for 'nb'", 2, norwegianRegionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("nb_NO", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'nb_NO' failed", &err)) {
        assertIntEquals("Wrong region count for 'nb_NO'", norwegianRegionCount, regionCount);
    }
    // locale IDs with variant codes
    err = U_ZERO_ERROR;
    int32_t englishRegionCount = ualoc_getRegionsForLanguage("en", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'en' failed", &err)) {
        assertIntEquals("Wrong region count for 'en'", 50, englishRegionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("en_US_POSIX", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'en_US_POSIX' failed", &err)) {
        assertIntEquals("Wrong region count for 'en_US_POSIX'", englishRegionCount, regionCount);
    }
    err = U_ZERO_ERROR;
    int32_t japaneseRegionCount = ualoc_getRegionsForLanguage("ja", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'ja' failed", &err)) {
        assertIntEquals("Wrong region count for 'ja'", 3, japaneseRegionCount);
    }
    err = U_ZERO_ERROR;
    regionCount = ualoc_getRegionsForLanguage("ja_JP_TRADITIONAL", 0, regions, 50, &err);
    if (assertSuccess("Getting regions for 'ja_JP_TRADITIONAL' failed", &err)) {
        assertIntEquals("Wrong region count for 'ja_JP_TRADITIONAL'", japaneseRegionCount, regionCount);
    }
}


#if U_PLATFORM_IS_DARWIN_BASED
#include <unistd.h>
#include <mach/mach_time.h>
#define GET_START() mach_absolute_time()
#define GET_DURATION(start, info) ((mach_absolute_time() - start) * info.numer)/info.denom
#else
// Here we already know !U_PLATFORM_HAS_WIN32_API is true
// So the following is for Linux
#include <unistd.h>
#include "putilimp.h"
#define GET_START() (uint64_t)uprv_getUTCtime()
#define GET_DURATION(start, info) ((uint64_t)uprv_getUTCtime() - start)
#endif

enum { kMaxLocsToUse = 8 };
static void TestAppleLocalizationsToUsePerf() { // rdar://62458844&63471438
    static const char* preferredLangs[] = {"zh_Hans","pt_BR","fr_BE"};
    static const char* availableLocs[] = {"ar","de","en","en_AU","en_GB","es","es_419","fr","fr_CA","it","ja","ko","nl","no","pt","pt_PT","ru","zh_CN","zh_HK","zh_TW"};
    const char * locsToUse[kMaxLocsToUse];
    UErrorCode status;
    int32_t numLocsToUse;
    uint64_t start, duration;
#if U_PLATFORM_IS_DARWIN_BASED
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
#endif

    log_info("sleeping 10 sec to check heap before...\n");
    sleep(10);
    status = U_ZERO_ERROR;
    start = GET_START();
    numLocsToUse = ualoc_localizationsToUse(preferredLangs, UPRV_LENGTHOF(preferredLangs),
                                            availableLocs, UPRV_LENGTHOF(availableLocs),
                                            locsToUse, kMaxLocsToUse, &status);
    duration = GET_DURATION(start, info);
    log_info("ualoc_localizationsToUse first  use nsec %llu; status %s, numLocsToUse %d: %s ...\n",
                                                duration, u_errorName(status), numLocsToUse, (numLocsToUse>=0)? locsToUse[0]: "");
    log_info("sleeping 10 sec to check heap after...\n");
    sleep(10);

    status = U_ZERO_ERROR;
    start = GET_START();
    numLocsToUse = ualoc_localizationsToUse(preferredLangs, UPRV_LENGTHOF(preferredLangs),
                                            availableLocs, UPRV_LENGTHOF(availableLocs),
                                            locsToUse, kMaxLocsToUse, &status);
    duration = GET_DURATION(start, info);
    log_info("ualoc_localizationsToUse second use nsec %llu; status %s, numLocsToUse %d\n",
                                                duration, u_errorName(status), numLocsToUse);
}

/* data for TestAppleLocalizationsToUse */

typedef struct {
    const char * const *locs;
    int32_t             locCount;
} AppleLocsAndCount;

enum { kNumLocSets = 6 };

typedef struct {
    const char * language;
    const char ** expLocsForSets[kNumLocSets];
} LangAndExpLocs;


static const char * appleLocs1[] = {
    "Arabic",
    "Danish",
    "Dutch",
    "English",
    "Finnish",
    "French",
    "German",
    "Italian",
    "Japanese",
    "Korean",
    "Norwegian",
    "Polish",
    "Portuguese",
    "Russian",
    "Spanish",
    "Swedish",
    "Thai",
    "Turkish",
    "ca",
    "cs",
    "el",
    "he",
    "hr",
    "hu",
    "id",
    "ms",
    "ro",
    "sk",
    "uk",
    "vi",
    "zh_CN", "zh_TW",
};

static const char * appleLocs2[] = {
    "ar",
    "ca",
    "cs",
    "da",
    "de",
    "el",
    "en", "en_AU", "en_GB",
    "es", "es_MX",
    "fi",
    "fr", "fr_CA",
    "he",
    "hr",
    "hu",
    "id",
    "it",
    "ja",
    "ko",
    "ms",
    "nl",
    "no",
    "pl",
    "pt", "pt_PT",
    "ro",
    "ru",
    "sk",
    "sv",
    "th",
    "tr",
    "uk",
    "vi",
    "zh_CN", "zh_HK", "zh_TW",
};

static const char * appleLocs3[] = {
    "ar",
    "ca",
    "cs",
    "da",
    "de",
    "el",
    "en", "en_AU", "en_CA", "en_GB",
    "es", "es_419",
    "fi",
    "fr", "fr_CA", "fr_FR",
    "he",
    "hr",
    "hu",
    "id",
    "it", "it_CH", // rdar://35829322
    "ja",
    "ko",
    "ms",
    "nb",
    "nl",
    "pl",
    "pt", "pt_BR", "pt_PT",
    "ro",
    "ru",
    "sk",
    "sv",
    "th",
    "tr",
    "uk",
    "vi",
    "zh_CN", "zh_HK", "zh_MO", "zh_TW",
};

static const char * appleLocs4[] = {
    "en", "en_AU", "en_CA", "en_GB", "en_IN", "en_US",
    "es", "es_419", "es_MX",
    "fr", "fr_CA", "fr_CH", "fr_FR",
    "it", "it_CH", "it_IT", // rdar://35829322
    "nl", "nl_BE", "nl_NL",
    "pt", "pt_BR",
    "ro", "ro_MD", "ro_RO",
    "zh_Hans", "zh_Hant", "zh_Hant_HK",
};

static const char * appleLocs5[] = {
    "en", "en_001", "en_AU", "en_GB",
    "es", "es_ES", "es_MX",
    "zh_CN", "zh_Hans", "zh_Hant", "zh_TW",
    "yi",
    "fil",
    "haw",
    "tlh",
    "sr",
    "sr-Latn",
};

// list 6
static const char * appleLocs6[] = {
    "en", "en_001", "en_150", "en_AU", "en_GB",
    "es", "es_419", "es_ES", "es_MX",
    "zh_CN", "zh_Hans", "zh_Hant", "zh_Hant_HK", "zh_HK", "zh_TW",
    "iw",
    "in",
    "mo",
    "tl",
};

static const AppleLocsAndCount locAndCountEntries[kNumLocSets] = {
    { appleLocs1, UPRV_LENGTHOF(appleLocs1) },
    { appleLocs2, UPRV_LENGTHOF(appleLocs2) },
    { appleLocs3, UPRV_LENGTHOF(appleLocs3) },
    { appleLocs4, UPRV_LENGTHOF(appleLocs4) },
    { appleLocs5, UPRV_LENGTHOF(appleLocs5) },
    { appleLocs6, UPRV_LENGTHOF(appleLocs6) },
};


static const char* l1_ar[]          = { "ar", NULL };
static const char* l1_Ara[]         = { "Arabic", NULL };
static const char* l1_ca[]          = { "ca", NULL };
static const char* l1_cs[]          = { "cs", NULL };
static const char* l1_da[]          = { "da", NULL };
static const char* l1_Dan[]         = { "Danish", NULL };
static const char* l1_de[]          = { "de", NULL };
static const char* l1_Ger[]         = { "German", NULL };
static const char* l1_el[]          = { "el", NULL };
static const char* l1_en[]          = { "en", NULL };
static const char* l1_Eng[]         = { "English", NULL };
static const char* l2_en_001_[]     = { "en_001", "en", NULL };
static const char* l2_en_CA_[]      = { "en_CA", "en", NULL };
static const char* l2_en_GB_[]      = { "en_GB", "en", NULL };
static const char* l2_en_US_[]      = { "en_US", "en", NULL };
static const char* l2_en_GB_Eng[]   = { "en_GB", "English", NULL };
static const char* l3_en_GB001_[]   = { "en_GB", "en_001", "en", NULL };
static const char* l3_en_AUGB_[]    = { "en_AU", "en_GB", "en", NULL };
static const char* l3_en_INGB_[]    = { "en_IN", "en_GB", "en", NULL };
static const char* l4_en_150GB001_[] = { "en_150", "en_GB", "en_001", "en", NULL };
static const char* l4_en_AUGB001_[] = { "en_AU", "en_GB", "en_001", "en", NULL };
static const char* l1_es[]          = { "es", NULL };
static const char* l1_Spa[]         = { "Spanish", NULL };
static const char* l2_es_419_[]     = { "es_419", "es", NULL };
static const char* l2_es_ES_[]      = { "es_ES", "es", NULL };
static const char* l2_es_MX_[]      = { "es_MX", "es", NULL };
static const char* l2_es_MX_Spa[]   = { "es_MX", "Spanish", NULL };
static const char* l3_es_MX419_[]   = { "es_MX", "es_419", "es", NULL };
static const char* l1_fi[]          = { "fi", NULL };
static const char* l1_Fin[]         = { "Finnish", NULL };
static const char* l1_fil[]         = { "fil", NULL };
static const char* l1_tl[]          = { "tl", NULL };
static const char* l1_fr[]          = { "fr", NULL };
static const char* l1_Fre[]         = { "French", NULL };
static const char* l2_fr_CA_[]      = { "fr_CA", "fr", NULL };
static const char* l2_fr_CH_[]      = { "fr_CH", "fr", NULL };
static const char* l2_fr_FR_[]      = { "fr_FR", "fr", NULL };
static const char* l1_haw[]         = { "haw", NULL };
static const char* l1_he[]          = { "he", NULL };
static const char* l1_hr[]          = { "hr", NULL };
static const char* l1_hu[]          = { "hu", NULL };
static const char* l1_id[]          = { "id", NULL };
static const char* l1_in[]          = { "in", NULL };
static const char* l1_it[]          = { "it", NULL };
static const char* l2_it_CH[]       = { "it_CH", "it", NULL }; // rdar://35829322
static const char* l2_it_IT[]       = { "it_IT", "it", NULL }; // rdar://35829322
static const char* l1_Ita[]         = { "Italian", NULL };
static const char* l1_ja[]          = { "ja", NULL };
static const char* l1_Japn[]        = { "Japanese", NULL };
static const char* l1_ko[]          = { "ko", NULL };
static const char* l1_Kor[]         = { "Korean", NULL };
static const char* l1_ms[]          = { "ms", NULL };
static const char* l1_nb[]          = { "nb", NULL };
static const char* l1_no[]          = { "no", NULL };
static const char* l1_Nor[]         = { "Norwegian", NULL };
static const char* l2_no_NO_[]      = { "no_NO", "no", NULL };
static const char* l1_nl[]          = { "nl", NULL };
static const char* l1_Dut[]         = { "Dutch", NULL };
static const char* l2_nl_BE_[]      = { "nl_BE", "nl", NULL };
static const char* l1_pl[]          = { "pl", NULL };
static const char* l1_Pol[]         = { "Polish", NULL };
static const char* l1_pt[]          = { "pt", NULL };
static const char* l1_pt_PT[]       = { "pt_PT", NULL };
static const char* l1_Port[]        = { "Portuguese", NULL };
static const char* l2_pt_BR_[]      = { "pt_BR", "pt", NULL };
static const char* l2_pt_PT_[]      = { "pt_PT", "pt", NULL };
static const char* l1_ro[]          = { "ro", NULL };
static const char* l2_ro_MD_[]      = { "ro_MD", "ro", NULL };
static const char* l1_mo[]          = { "mo", NULL };
static const char* l1_ru[]          = { "ru", NULL };
static const char* l1_Rus[]         = { "Russian", NULL };
static const char* l1_sk[]          = { "sk", NULL };
static const char* l1_sr[]          = { "sr", NULL };
static const char* l1_srLatn[]      = { "sr-Latn", NULL };
static const char* l1_sv[]          = { "sv", NULL };
static const char* l1_Swe[]         = { "Swedish", NULL };
static const char* l1_th[]          = { "th", NULL };
static const char* l1_Thai[]        = { "Thai", NULL };
static const char* l1_tlh[]         = { "tlh", NULL };
static const char* l1_tr[]          = { "tr", NULL };
static const char* l1_Tur[]         = { "Turkish", NULL };
static const char* l1_uk[]          = { "uk", NULL };
static const char* l1_vi[]          = { "vi", NULL };
static const char* l1_yi[]          = { "yi", NULL };
static const char* l1_iw[]          = { "iw", NULL };
static const char* l1_zh_CN[]       = { "zh_CN", NULL };
static const char* l1_zh_TW[]       = { "zh_TW", NULL };
static const char* l1_zh_HK[]       = { "zh_HK", NULL };
static const char* l1_zh_Hans[]     = { "zh_Hans", NULL };
static const char* l1_zh_Hant[]     = { "zh_Hant", NULL };
static const char* l1_zhHant[]      = { "zh-Hant", NULL };
static const char* l2_zh_HKTW[]     = { "zh_HK", "zh_TW", NULL };
static const char* l2_zh_Hant_HK_[] = { "zh_Hant_HK", "zh_Hant", NULL };
static const char* l2_zh_CN_Hans[]  = { "zh_CN", "zh_Hans", NULL };
static const char* l2_zh_TW_Hant[]  = { "zh_TW", "zh_Hant", NULL };
static const char* l2_zh_TW_Hant_[]  = { "zh_TW", "zh-Hant", NULL };
static const char* l3_zh_MOHKTW[]   = { "zh_MO", "zh_HK", "zh_TW", NULL };
static const char* l3_zh_HK_HantHK_Hant[] = { "zh_HK", "zh_Hant_HK", "zh_Hant", NULL };
static const char* l3_zh_HKTW_Hant[] = { "zh_Hant_HK", "zh_HK", "zh_TW", "zh_Hant", NULL };
static const char* l3_zh_HantHKHKTW_Hant[] = { "zh_HK", "zh_Hant_HK", "zh_TW", "zh_Hant", NULL };

static const LangAndExpLocs appleLangAndLoc[] = {
//    language\    result for appleLocs1      appleLocs2      appleLocs3      appleLocs4      appleLocs5      appleLocs6
    { "zh",                 { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "zh-Hans",            { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "zh-Hant",            { l1_zh_TW,       l1_zh_TW,       l1_zh_TW,       l1_zh_Hant,     l1_zh_Hant,     l1_zh_Hant     } },
    { "zh-Hans-CN",         { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l2_zh_CN_Hans,  l2_zh_CN_Hans  } },
    { "zh-Hans-SG",         { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "zh-Hant-TW",         { l1_zh_TW,       l1_zh_TW,       l1_zh_TW,       l1_zh_Hant,     l2_zh_TW_Hant,  l2_zh_TW_Hant  } },
    { "zh-Hant-HK",         { l1_zh_TW,       l2_zh_HKTW,     l2_zh_HKTW,     l2_zh_Hant_HK_, l1_zh_Hant,     l3_zh_HKTW_Hant} },
    { "zh-Hant-MO",         { l1_zh_TW,       l2_zh_HKTW,     l3_zh_MOHKTW,   l2_zh_Hant_HK_, l1_zh_Hant,     l3_zh_HKTW_Hant} },
    { "zh-Hans-HK",         { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "zh-CN",              { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l2_zh_CN_Hans,  l2_zh_CN_Hans  } },
    { "zh-SG",              { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "zh-TW",              { l1_zh_TW,       l1_zh_TW,       l1_zh_TW,       l1_zh_Hant,     l2_zh_TW_Hant,  l2_zh_TW_Hant  } },
    { "zh-HK",              { l1_zh_TW,       l2_zh_HKTW,     l2_zh_HKTW,     l2_zh_Hant_HK_, l1_zh_Hant,  l3_zh_HantHKHKTW_Hant } },
    { "zh-MO",              { l1_zh_TW,       l2_zh_HKTW,     l3_zh_MOHKTW,   l2_zh_Hant_HK_, l1_zh_Hant,  l3_zh_HKTW_Hant} },
    { "yue-CN",             { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } }, // rdar://30671866 should 5/6 be zh_CN?
    { "yue-HK",             { l1_zh_TW,       l2_zh_HKTW,     l2_zh_HKTW,     l2_zh_Hant_HK_, l1_zh_Hant,     l3_zh_HKTW_Hant } },
    { "yue-Hans",           { l1_zh_CN,       l1_zh_CN,       l1_zh_CN,       l1_zh_Hans,     l1_zh_Hans,     l1_zh_Hans     } },
    { "yue-Hant",           { l1_zh_TW,       l2_zh_HKTW,     l2_zh_HKTW,     l2_zh_Hant_HK_, l1_zh_Hant,     l3_zh_HKTW_Hant } },
    { "en",                 { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-US",              { l1_Eng,         l1_en,          l1_en,          l2_en_US_,      l1_en,          l1_en          } },
    { "en_US",              { l1_Eng,         l1_en,          l1_en,          l2_en_US_,      l1_en,          l1_en          } },
    { "en-CN",              { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-JP",              { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-TW",              { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-TR",              { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-001",             { l1_Eng,         l1_en,          l1_en,          l1_en,          l2_en_001_,     l2_en_001_     } },
    { "en-CA",              { l1_Eng,         l1_en,          l2_en_CA_,      l2_en_CA_,      l1_en,          l1_en          } },
    { "en-IL",              { l1_Eng,         l1_en,          l1_en,          l1_en,          l2_en_001_,     l2_en_001_     } },
    { "en-GB",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-IN",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l3_en_INGB_,    l3_en_GB001_,   l3_en_GB001_   } },
    { "en-BD",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-LK",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-GG",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-HK",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-IE",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-JM",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-MO",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-MT",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-PK",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-SG",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-VG",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-ZA",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l3_en_GB001_   } },
    { "en-AU",              { l1_Eng,         l3_en_AUGB_,    l3_en_AUGB_,    l3_en_AUGB_,    l4_en_AUGB001_, l4_en_AUGB001_ } },
    { "en-NZ",              { l1_Eng,         l3_en_AUGB_,    l3_en_AUGB_,    l3_en_AUGB_,    l4_en_AUGB001_, l4_en_AUGB001_ } },
    { "en-WS",              { l1_Eng,         l3_en_AUGB_,    l3_en_AUGB_,    l3_en_AUGB_,    l4_en_AUGB001_, l4_en_AUGB001_ } },
    { "en-150",             { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l4_en_150GB001_ } },
    { "en-FR",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l4_en_150GB001_ } },
    { "en-BE",              { l1_Eng,         l2_en_GB_,      l2_en_GB_,      l2_en_GB_,      l3_en_GB001_,   l4_en_150GB001_ } },
    { "en-Latn",            { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-Latn-US",         { l1_Eng,         l1_en,          l1_en,          l1_en,/*TODO*/  l1_en,          l1_en          } },
    { "en-US-POSIX",        { l1_Eng,         l1_en,          l1_en,          l2_en_US_,      l1_en,          l1_en          } },
    { "en-Latn-US-POSIX",   { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-u-ca-hebrew",     { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en@calendar=hebrew", { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en-",                { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "en_",                { l1_Eng,         l1_en,          l1_en,          l1_en,          l1_en,          l1_en          } },
    { "es",                 { l1_Spa,         l1_es,          l1_es,          l1_es,          l1_es,          l1_es          } },
    { "es-ES",              { l1_Spa,         l1_es,          l1_es,          l1_es,          l2_es_ES_,      l2_es_ES_      } },
    { "es-419",             { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-MX",              { l1_Spa,         l2_es_MX_,      l2_es_419_,     l3_es_MX419_,   l2_es_MX_,      l3_es_MX419_   } },
    { "es-AR",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-BO",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } }, // rdar://34459988
    { "es-BR",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-BZ",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-AG",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-AW",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-CA",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-CW",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-SX",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-TT",              { l1_Spa,         l1_es,          l2_es_419_,     l2_es_419_,     l1_es,          l2_es_419_     } },
    { "es-Latn",            { l1_Spa,         l1_es,          l1_es,          l1_es,          l1_es,          l1_es          } },
    { "es-Latn-MX",         { l1_Spa,         l1_es,          l1_es,          l1_es,          l1_es,          l1_es          } },
    { "pt",                 { l1_Port,        l1_pt,          l1_pt,          l1_pt,          NULL,           NULL  } },
    { "pt-BR",              { l1_Port,        l1_pt,          l2_pt_BR_,      l2_pt_BR_,      NULL,           NULL  } },
    { "pt-PT",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "pt-MO",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "pt-CH",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "pt-FR",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "pt-GQ",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "pt-LU",              { l1_Port,        l2_pt_PT_,      l2_pt_PT_,      l1_pt,          NULL,           NULL  } },
    { "fr",                 { l1_Fre,         l1_fr,          l1_fr,          l1_fr,          NULL,           NULL  } },
    { "fr-FR",              { l1_Fre,         l1_fr,          l2_fr_FR_,      l2_fr_FR_,      NULL,           NULL  } },
    { "fr-CA",              { l1_Fre,         l2_fr_CA_,      l2_fr_CA_,      l2_fr_CA_,      NULL,           NULL  } },
    { "fr-CH",              { l1_Fre,         l1_fr,          l1_fr,          l2_fr_CH_,      NULL,           NULL  } },
    { "ar",                 { l1_Ara,         l1_ar,          l1_ar,          NULL,           NULL,           NULL  } },
    { "da",                 { l1_Dan,         l1_da,          l1_da,          NULL,           NULL,           NULL  } },
    { "nl",                 { l1_Dut,         l1_nl,          l1_nl,          l1_nl,          NULL,           NULL  } },
    { "nl-BE",              { l1_Dut,         l1_nl,          l1_nl,          l2_nl_BE_,      NULL,           NULL  } },
    { "fi",                 { l1_Fin,         l1_fi,          l1_fi,          NULL,           NULL,           NULL  } },
    { "de",                 { l1_Ger,         l1_de,          l1_de,          NULL,           NULL,           NULL  } },
    { "it",                 { l1_Ita,         l1_it,          l1_it,          l1_it,          NULL,           NULL  } },
    { "it_CH",              { l1_Ita,         l1_it,          l2_it_CH,       l2_it_CH,       NULL,           NULL  } }, // rdar://35829322
    { "it_IT",              { l1_Ita,         l1_it,          l1_it,          l2_it_IT,       NULL,           NULL  } }, // rdar://35829322
    { "it_VA",              { l1_Ita,         l1_it,          l1_it,          l1_it,          NULL,           NULL  } }, // rdar://35829322
    { "ja",                 { l1_Japn,        l1_ja,          l1_ja,          NULL,           NULL,           NULL  } },
    { "ko",                 { l1_Kor,         l1_ko,          l1_ko,          NULL,           NULL,           NULL  } },
    { "nb",                 { l1_Nor,         l1_no,          l1_nb,          NULL,           NULL,           NULL  } },
    { "no",                 { l1_Nor,         l1_no,          l1_nb,          NULL,           NULL,           NULL  } },
    { "pl",                 { l1_Pol,         l1_pl,          l1_pl,          NULL,           NULL,           NULL  } },
    { "ru",                 { l1_Rus,         l1_ru,          l1_ru,          NULL,           NULL,           NULL  } },
    { "sv",                 { l1_Swe,         l1_sv,          l1_sv,          NULL,           NULL,           NULL  } },
    { "th",                 { l1_Thai,        l1_th,          l1_th,          NULL,           NULL,           NULL  } },
    { "tr",                 { l1_Tur,         l1_tr,          l1_tr,          NULL,           NULL,           NULL  } },
    { "ca",                 { l1_ca,          l1_ca,          l1_ca,          NULL,           NULL,           NULL  } },
    { "cs",                 { l1_cs,          l1_cs,          l1_cs,          NULL,           NULL,           NULL  } },
    { "el",                 { l1_el,          l1_el,          l1_el,          NULL,           NULL,           NULL  } },
    { "he",                 { l1_he,          l1_he,          l1_he,          NULL,           NULL,           l1_iw } },
    { "iw",                 { l1_he,          l1_he,          l1_he,          NULL,           NULL,           l1_iw } },
    { "hr",                 { l1_hr,          l1_hr,          l1_hr,          NULL,           NULL,           NULL  } },
    { "hu",                 { l1_hu,          l1_hu,          l1_hu,          NULL,           NULL,           NULL  } },
    { "id",                 { l1_id,          l1_id,          l1_id,          NULL,           NULL,           l1_in } },
    { "in",                 { l1_id,          l1_id,          l1_id,          NULL,           NULL,           l1_in } },
    { "ms",                 { l1_ms,          l1_ms,          l1_ms,          NULL,           NULL,           NULL  } },
    { "ro",                 { l1_ro,          l1_ro,          l1_ro,          l1_ro,          NULL,           l1_mo } },
    { "mo",                 { l1_ro,          l1_ro,          l1_ro,          l1_ro,          NULL,           l1_mo } },
    { "sk",                 { l1_sk,          l1_sk,          l1_sk,          NULL,           NULL,           NULL  } },
    { "uk",                 { l1_uk,          l1_uk,          l1_uk,          NULL,           NULL,           NULL  } },
    { "vi",                 { l1_vi,          l1_vi,          l1_vi,          NULL,           NULL,           NULL  } },
    { "yi",                 { NULL,           NULL,           NULL,           NULL,           l1_yi,          NULL  } },
    { "ji",                 { NULL,           NULL,           NULL,           NULL,           l1_yi,          NULL  } },
    { "fil",                { NULL,           NULL,           NULL,           NULL,           l1_fil,         l1_tl } },
    { "tl",                 { NULL,           NULL,           NULL,           NULL,           l1_fil,         l1_tl } },
    { "haw",                { NULL,           NULL,           NULL,           NULL,           l1_haw,         NULL  } },
    { "sr",                 { NULL,           NULL,           NULL,           NULL,           l1_sr,          NULL  } },
    { "sr-Cyrl",            { NULL,           NULL,           NULL,           NULL,           l1_sr,          NULL  } },
    { "sr-Latn",            { NULL,           NULL,           NULL,           NULL,           l1_srLatn,      NULL  } },
    { "tlh",                { NULL,           NULL,           NULL,           NULL,           l1_tlh,         NULL  } },
    { "Default@2x",         { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "default",            { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "root",               { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "",                   { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "_US",                { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "-US",                { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "-u-ca-hebrew",       { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "-u-ca-hebrew",       { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
    { "@calendar=hebrew",   { NULL,           NULL,           NULL,           NULL,           NULL,           NULL  } },
};
enum { kNumAppleLangAndLoc = UPRV_LENGTHOF(appleLangAndLoc) };

/* tests from rdar://21518031 */

static const char * appleLocsA1[] = { "en", "fr", "no", "zh-Hant" };
static const char * appleLocsA2[] = { "en", "fr", "nb", "zh_TW", "zh_CN", "zh-Hant" };
static const char * appleLocsA3[] = { "en", "en_IN", "en_GB", "fr", "de", "zh_TW" };
static const char * appleLocsA4[] = { "Spanish", "es_MX", "English", "en_GB" };
static const char * appleLocsA5[] = { "en", "fr", "de", "pt", "pt_PT" };
static const char * appleLocsA6[] = { "en", "no", "no_NO", "pt_PT" };

static const AppleLocsAndCount locAndCountEntriesA[kNumLocSets] = {
    { appleLocsA1, UPRV_LENGTHOF(appleLocsA1) },
    { appleLocsA2, UPRV_LENGTHOF(appleLocsA2) },
    { appleLocsA3, UPRV_LENGTHOF(appleLocsA3) },
    { appleLocsA4, UPRV_LENGTHOF(appleLocsA4) },
    { appleLocsA5, UPRV_LENGTHOF(appleLocsA5) },
    { appleLocsA6, UPRV_LENGTHOF(appleLocsA6) },
};

static const LangAndExpLocs appleLangAndLocA[] = {
//    language\    result for appleLocsA1     appleLocsA2         appleLocsA3     appleLocsA4     appleLocsA5     appleLocsA6
    { "zh-Hant",            { l1_zhHant,/*0*/ l1_zhHant,/*zh_TW*/ l1_zh_TW,       NULL,           NULL,           NULL        } },
    { "zh_Hant",            { l1_zhHant,      l1_zhHant,/*zh_TW*/ l1_zh_TW,       NULL,           NULL,           NULL        } },
    { "zh_HK",              { l1_zhHant,      l1_zhHant,/*zh_TW*/ l1_zh_TW,       NULL,           NULL,           NULL        } },
    { "en_IN",              { l1_en,          l1_en,              l3_en_INGB_,    l2_en_GB_Eng,   l1_en,          l1_en       } },
    { "es_MX",              { NULL,           NULL,               NULL,           l2_es_MX_Spa,   NULL,           NULL        } },
    { "pt_PT",              { NULL,           NULL,               NULL,           NULL,           l2_pt_PT_,      l1_pt_PT    } },
    { "pt",                 { NULL,           NULL,               NULL,           NULL,           l1_pt,          l1_pt_PT    } },
    { "no",                 { l1_no,          l1_nb,              NULL,           NULL,           NULL,           l1_no       } },
    { "no_NO",              { l1_no,          l1_nb,              NULL,           NULL,           NULL,           l2_no_NO_   } },
    { "nb",                 { l1_no,          l1_nb,              NULL,           NULL,           NULL,           l2_no_NO_   } },
    { "nb_NO",              { l1_no,          l1_nb,              NULL,           NULL,           NULL,           l2_no_NO_   } },
};
enum { kNumAppleLangAndLocA = UPRV_LENGTHOF(appleLangAndLocA) };

/* tests from log attached to 21682790 */

static const char * appleLocsB1[] = {
    "ar",       "Base",     "ca",       "cs",
    "da",       "Dutch",    "el",       "English",
    "es_MX",    "fi",       "French",   "German",
    "he",       "hr",       "hu",       "id",
    "Italian",  "Japanese", "ko",       "ms",
    "no",       "pl",       "pt",       "pt_PT",
    "ro",       "ru",       "sk",       "Spanish",
    "sv",       "th",       "tr",       "uk",
    "vi",       "zh_CN",    "zh_TW"
};

static const char * appleLocsB2[] = {
    "ar",                   "ca",       "cs",
    "da",       "Dutch",    "el",       "English",
    "es_MX",    "fi",       "French",   "German",
    "he",       "hr",       "hu",       "id",
    "Italian",  "Japanese", "ko",       "ms",
    "no",       "pl",       "pt",       "pt_PT",
    "ro",       "ru",       "sk",       "Spanish",
    "sv",       "th",       "tr",       "uk",
    "vi",       "zh_CN",    "zh_TW"
};

static const char * appleLocsB3[] = {
    "ar",       "ca",       "cs",       "da",
    "de",       "el",       "en",       "es",
    "es_MX",    "fi",       "French",   "he",
    "hr",       "hu",       "id",       "Italian",
    "ja",       "ko",       "ms",       "nl",
    "no",       "pl",       "pt",       "pt_PT",
    "ro",       "ru",       "sk",       "sv",
    "th",       "tr",       "uk",       "vi",
    "zh_CN",    "zh_TW"
};

static const char * appleLocsB4[] = {
    "ar",       "ca",       "cs",       "da",
    "de",       "el",       "en",       "es",
    "es_MX",    "fi",       "fr",       "he",
    "hr",       "hu",       "id",       "it",
    "ja",       "ko",       "ms",       "nl",
    "no",       "pl",       "pt",       "pt_PT",
    "ro",       "ru",       "sk",       "sv",
    "th",       "tr",       "uk",       "vi",
    "zh_CN",     "zh_TW"
};

static const char * appleLocsB5[] = { "en" };

static const char * appleLocsB6[] = { "English" };

static const AppleLocsAndCount locAndCountEntriesB[kNumLocSets] = {
    { appleLocsB1, UPRV_LENGTHOF(appleLocsB1) },
    { appleLocsB2, UPRV_LENGTHOF(appleLocsB2) },
    { appleLocsB3, UPRV_LENGTHOF(appleLocsB3) },
    { appleLocsB4, UPRV_LENGTHOF(appleLocsB4) },
    { appleLocsB5, UPRV_LENGTHOF(appleLocsB5) },
    { appleLocsB6, UPRV_LENGTHOF(appleLocsB6) },
};

static const LangAndExpLocs appleLangAndLocB[] = {
//    language\    result for appleLocsB1     appleLocsB2         appleLocsB3     appleLocsB4     appleLocsB5     appleLocsB6
// Prefs 1, logged with sets B1-B3
    { "en",                 { l1_Eng,         l1_Eng,             l1_en,          l1_en,          l1_en,          l1_Eng      } },
    { "es",                 { l1_Spa,         l1_Spa,             l1_es,          l1_es,          NULL,           NULL        } },
// Prefs 2, logged with sets B1-B6
    { "English",            { l1_Eng,         l1_Eng,             l1_en,          l1_en,          l1_en,          l1_Eng      } },
    { "Spanish",            { l1_Spa,         l1_Spa,             l1_es,          l1_es,          NULL,           NULL        } },
};
enum { kNumAppleLangAndLocB = UPRV_LENGTHOF(appleLangAndLocB) };

typedef struct {
    const AppleLocsAndCount * locAndCountEntriesPtr;
    const LangAndExpLocs *    appleLangAndLocPtr;
    int32_t                   appleLangAndLocCount;
} AppleLocToUseTestSet;

static const AppleLocToUseTestSet altuTestSets[] = {
    { locAndCountEntries,  appleLangAndLoc,  kNumAppleLangAndLoc },
    { locAndCountEntriesA, appleLangAndLocA, kNumAppleLangAndLocA },
    { locAndCountEntriesB, appleLangAndLocB, kNumAppleLangAndLocB },
    { NULL, NULL, 0 }
};

/* tests for multiple prefs sets */

static const char * appleLocsM1[] = { "en", "en_GB", "pt", "pt_PT", "zh_CN", "zh_Hant" };
static const char * prefLangsM1[] = { "tlh", "zh_HK", "zh_SG", "zh_Hans", "pt_BR", "pt_PT", "en_IN", "en" };
static const char * locsToUseM1[] = { "zh_Hant" };

// Tests from first pass at rdar://22012864, 2015-11-18

static const char * appleLocsM2[] = { "fr-FR", "en-US", "en-GB" };
static const char * prefLangsM2[] = { "fr-CH" };
static const char * locsToUseM2[] = { "fr-FR" };

static const char * appleLocsM3[] = { "es-es", "fr-fr" };
static const char * prefLangsM3[] = { "fr-US", "fr", "en-US" };
static const char * locsToUseM3[] = { "fr-fr" };

static const char * appleLocsM4[] = { "es-es", "fr-fr", "fr" };
static const char * prefLangsM4[] = { "fr-US", "fr", "en-US" };
static const char * locsToUseM4[] = { "fr" };

// Tests from second pass at rdar://22012864, 2015-12-08
// Per Karan M
static const char * appleLocsM5[] = { "en-US", "fr-FR", "de-DE", "es-ES", "es-419", "pt-PT", "pt-BR", "zh-CN", "zh-TW", "zh-HK", "ja-JP", "ko-KR" };
static const char * prefLangsM5[] = { "fr-US", "en-US" };
static const char * locsToUseM5[] = { "fr-FR" };
// Per Peter E; expected result changed from "en-US" to "de-CH" per rdar://26559053
static const char * appleLocsM6[] = { "de-CH", "en-US" };
static const char * prefLangsM6[] = { "de-DE", "en-US" };
static const char * locsToUseM6[] = { "de-CH" };
// The following is used for M7-MD
static const char * appleLocsMx[] = { "de-DE", "en-AU", "es-ES", "fr-FR", "hi-IN", "pt-BR", "zh-HK", "zh-TW" };
// Per Karan M
static const char * prefLangsM7[] = { "fr-ES", "en-AU" };
static const char * locsToUseM7[] = { "fr-FR" };
// Per Karan M
static const char * prefLangsM8[] = { "de-IT", "en-AU" };
static const char * locsToUseM8[] = { "de-DE" };
// Per Karan M
static const char * prefLangsM9[] = { "hi-US", "en-AU" };
static const char * locsToUseM9[] = { "hi-IN" };
// Per Karan M
static const char * prefLangsMA[] = { "en-IN", "zh-HK" };
static const char * locsToUseMA[] = { "en-AU" };
// Per Karan M
static const char * prefLangsMB[] = { "pt-PT", "en-AU" };
static const char * locsToUseMB[] = { "en-AU" };
// per Paul B:
static const char * prefLangsMC[] = { "pt-PT", "ar" };
static const char * locsToUseMC[] = { "pt-BR" };
// Per Karan M
static const char * prefLangsMD[] = { "zh-CN", "en-AU" };
static const char * locsToUseMD[] = { "en-AU" };
// Per Karan M
static const char * appleLocsME[] = { "de-DE", "en-AU", "es-ES", "fr-FR", "hi-IN", "pt-BR", "zh-CN", "zh-HK" };
static const char * prefLangsME[] = { "zh-TW", "en-AU" };
static const char * locsToUseME[] = { "zh-HK" };
// Per Peter E in diagnosis for rdar://22012864 and rdar://23815194
static const char * appleLocsMF[] = { "en", "en-GB", "fr", "es" };
static const char * prefLangsMF[] = { "en-IN", "en-GB", "de", "fr" };
static const char * locsToUseMF[] = { "en-GB", "en" };
// Per Karan M in rdar://23982460
static const char * appleLocsMG[] = { "zh-Hans", "zh-Hant", "zh-HK" };
static const char * prefLangsMG[] = { "zh-Hans-US", "zh-HK", "en-US" };
static const char * locsToUseMG[] = { "zh-Hans" };
// Per rdar://25903891
static const char * appleLocsMH[] = { "zh-TW", "zh-CN", "zh-HK" };
static const char * prefLangsMH[] = { "zh-Hans-HK", "zh-HK", "en" };
static const char * locsToUseMH[] = { "zh-CN" };
// Per rdar://26559053
static const char * appleLocsMI[] = { "unk", "en-US", "ar-SA" };
static const char * prefLangsMI[] = { "ar-US" };
static const char * locsToUseMI[] = { "ar-SA" };
// Per rdar://30501523 - first for comparison with zh, then real test
static const char * appleLocsMJ[] = { "zh-CN", "en-US" };
static const char * prefLangsMJ[] = { "zh", "zh_AC" };
static const char * locsToUseMJ[] = { "zh-CN" };
static const char * appleLocsMK[] = { "yue-CN", "en-US" };
static const char * prefLangsMK[] = { "yue", "yue_AC" };
static const char * locsToUseMK[] = {  };
// NOTE: Test MK was changed to expect "yue" NOT to match "yue-CN"-- the default script for "yue" is "Hant", and we don't
// allow cross-script matching (we believe our original fix for rdar://30501523 was mistaken).  Tests MK1a and MK1b are
// added to make sure that "yue-CN" DOES still match "yue_CN" and "yue_Hans", even with the change for rdar://66938404.
static const char * prefLangsMK1a[] = { "yue", "yue_Hans" };
static const char * prefLangsMK1b[] = { "yue", "yue_CN" };
static const char * locsToUseMK1[] = { "yue-CN" };
// Per rdar://30433534
static const char * appleLocsML[] = { "nl_NL", "es_MX", "fr_FR", "zh_TW", "it_IT", "vi_VN", "fr_CH", "es_CL",
                                      "en_ZA", "ko_KR", "ca_ES", "ro_RO", "en_PH", "en_CA", "en_SG", "en_IN",
                                      "en_NZ", "it_CH", "fr_CA", "da_DK", "de_AT", "pt_BR", "yue_CN", "zh_CN",
                                      "sv_SE", "es_ES", "ar_SA", "hu_HU", "fr_BE", "en_GB", "ja_JP", "zh_HK",
                                      "fi_FI", "tr_TR", "nb_NO", "en_ID", "en_SA", "pl_PL", "ms_MY", "cs_CZ",
                                      "el_GR", "id_ID", "hr_HR", "en_AE", "he_IL", "ru_RU", "wuu_CN", "de_DE",
                                      "de_CH", "en_AU", "nl_BE", "th_TH", "pt_PT", "sk_SK", "en_US", "en_IE",
                                      "es_CO", "uk_UA", "es_US" };
static const char * prefLangsML[] = { "en-JP" };
static const char * locsToUseML[] = { "en_US" };
// Per rdar://30671866
static const char * prefLangsML1[] = { "yue-CN", "zh-CN" };
static const char * locsToUseML1[] = { "yue_CN" }; // should we also get "zh-CN" as a second option?
static const char * prefLangsML2[] = { "yue-Hans", "zh-Hans" };
static const char * locsToUseML2[] = { "yue_CN" }; // should we also get "zh-CN" as a second option?
// Per rdar://32421203
static const char * appleLocsMM1[] = { "pt-PT" };
static const char * appleLocsMM2[] = { "pt-BR" };
static const char * appleLocsMM3[] = { "pt-PT", "pt-BR" };
static const char * appleLocsMM4[] = { "en", "pt-PT" };
static const char * appleLocsMM5[] = { "en", "pt-BR" };
static const char * appleLocsMM6[] = { "en", "pt-PT", "pt-BR" };
static const char * prefLangsMM1[] = { "pt-PT" };
static const char * prefLangsMM2[] = { "pt-BR" };
static const char * prefLangsMM3[] = { "pt" };
static const char * prefLangsMM4[] = { "pt-PT", "en" };
static const char * prefLangsMM5[] = { "pt-BR", "en" };
static const char * prefLangsMM6[] = { "pt", "en" };
static const char * locsToUseMMptPT[] = { "pt-PT" };
static const char * locsToUseMMptBR[] = { "pt-BR" };
static const char * locsToUseMMen[]   = { "en" };
// Per rdar://32658828
static const char * appleLocsMN[]   = { "en-US", "en-GB" };
static const char * prefLangsMN1[]  = { "en-KR" };
static const char * prefLangsMN2[]  = { "en-SA" };
static const char * prefLangsMN3[]  = { "en-TW" };
static const char * prefLangsMN4[]  = { "en-JP" };
static const char * locsToUseMN_U[] = { "en-US" };
// Per rdar://36010857
static const char * appleLocsMO[]   = { "Dutch", "French", "German", "Italian", "Japanese", "Spanish",
                                        "ar", "ca", "cs", "da", "el", "en_AU", "en_GB", "en_IN",
                                        "es_419", "fi", "fr_CA", "he", "hi", "hr", "hu", "id", "ko",
                                        "ms", "no", "pl", "pt", "pt_PT", "ro", "ru", "sk", "sv",
                                        "th", "tr", "uk", "vi", "zh_CN", "zh_HK", "zh_TW" };
static const char * prefLangsMO1[]  = { "en-US" };
static const char * locsToUseMO1[]  = { "en_GB" };
// Per rdar://47494729
static const char * appleLocsMP[]   = { "en-IN", "hi-IN" };
static const char * prefLangsMP[]   = { "hi-Latn-IN", "en-IN" };
static const char * locsToUseMP[]   = { "en-IN" };
// Per rdar://34459988&35829322
static const char * appleLocsMQa[]   = { "en_AU", "en_IE", "en_IN", "en_SA", "en_UK", "en_US", "es_AR", "es_CO", "es_ES", "es_MX", "fr_CA", "fr_FR", "it_CH", "it_IT", "zh_CN", "zh_HK", "zh_TW" };
static const char * appleLocsMQb[]   = { "en_AU", "en_IE", "en_IN", "en_SA", "en_UK", "en", "es_AR", "es_CO", "es", "es_MX", "fr_CA", "fr", "it_CH", "it", "zh_CN", "zh_HK", "zh_TW" };
static const char * prefLangsMQ1[]  = { "es-BO" };
static const char * locsToUseMQ1[]  = { "es_MX" };
static const char * prefLangsMQ2[]  = { "it-VA" };
static const char * locsToUseMQ2a[]  = { "it_IT" };
static const char * locsToUseMQ2b[]  = { "it" };
// Per rdar://50913699
static const char * appleLocsMRa[]   = { "en", "hi" };
static const char * appleLocsMRb[]   = { "en", "hi", "hi_Latn" };
static const char * prefLangsMRx[]   = { "hi_Latn_IN", "en_IN", "hi_IN" };
static const char * prefLangsMRy[]   = { "hi_Latn", "en", "hi" };
static const char * locsToUseMRa[]   = { "en" };
static const char * locsToUseMRb[]   = { "hi_Latn", "en" };
// For rdar://50280505
static const char * appleLocsMSa[]   = { "en", "en_GB" };
static const char * appleLocsMSb[]   = { "en", "en_GB", "en_AU" };
static const char * prefLangsMSx[]   = { "en_NZ" };
static const char * prefLangsMSy[]   = { "en_NZ", "en_AU" };
static const char * locsToUseMSa[]   = { "en_GB", "en" };
static const char * locsToUseMSb[]   = { "en_AU", "en_GB", "en" };
// For rdar://55885283
static const char * appleLocsMT[]   = { "ca-ES", "fi", "nl", "en-US", "hu", "pt-BR", "pl-PL", "it",
                                        "ru", "el", "el-GR", "ca", "de-DE", "sv-SE", "tr", "pl",
                                        "sv", "tr-TR", "da", "en", "nb", "pt-PT", "nb-NO",
                                        "es-ES@collation=traditional", "sl-SI", "cs", "hu-HU",
                                        "cs-CZ", "sk", "sl", "de", "da-DK", "es-MX", "vi", "nl-NL",
                                        "es", "fi-FI", "fr", "it-IT", "es-ES", "fr-CA", "vi-VN",
                                        "pt", "sk-SK", "eu-ES", "ru-RU", "eu", "fr-FR", "unk" };
static const char * prefLangsMTa[]   = { "en" };
static const char * prefLangsMTb[]   = { "he" };
static const char * locsToUseMTa[]   = { "en" };
static const char * locsToUseMTb[]   = { };
// For rdar://64350332
static const char * appleLocsMU[]    = { "hi", "en-IN" };
static const char * prefLangsMU[]    = { "en-US" };
static const char * locsToUseMU[]    = { "en-IN" };
// For rdar://59520369
static const char * appleLocsMVa[]    = { "zh", "zh-Hans", "zh-Hant" };
static const char * appleLocsMVb[]    = { "zh-Hans", "zh", "zh-Hant" };
static const char * prefLangsMVa[]    = { "zh-Hans-US" };
static const char * prefLangsMVc[]    = { "zh-Hans" };
static const char * locsToUseMV[]     = { "zh-Hans", "zh" };
// comment in Developer Forums, not made into a Radar
static const char * appleLocsMW[]     = { "fr-CA", "pt-BR", "es-MX", "en-US", "en" };
static const char * prefLangsMW[]     = { "es-ES" };
static const char * locsToUseMW[]     = { "es-MX" };
// For rdar://64811575
static const char * appleLocsMX[]     = { "en", "fr", "de", "zh_CN", "zh_TW", "zh-Hant"};
static const char * prefLangsMX[]     = { "zh_HK" };
static const char * locsToUseMX[]     = { "zh-Hant" };
// For rdar://59520369
static const char * appleLocsMYa[]    = { "en", "ars", "ar" };
static const char * appleLocsMYb[]    = { "en", "ar" };
static const char * appleLocsMYc[]    = { "en", "ars" };
static const char * prefLangsMYa[]    = { "ars_SA" };
static const char * prefLangsMYb[]    = { "ar_SA" };
static const char * locsToUseMYa[]    = { "ars", "ar" };
static const char * locsToUseMYb[]    = { "ars" };
static const char * locsToUseMYc[]    = { "ar" };
// For rdar://59520369
static const char * appleLocsMZa[]    = { "en", "wuu-Hans", "zh-Hans" };
static const char * appleLocsMZb[]    = { "en", "zh-Hans" };
static const char * appleLocsMZc[]    = { "en", "wuu-Hans" };
static const char * prefLangsMZa[]    = { "wuu_CN" };
static const char * prefLangsMZb[]    = { "zh_CN" };
static const char * locsToUseMZa[]    = { "wuu-Hans", "zh-Hans" };
static const char * locsToUseMZb[]    = { "wuu-Hans" };
static const char * locsToUseMZc[]    = { "zh-Hans" };
// For rdar://64916132
static const char * appleLocsMAA[]    = { "fr-CH", "ja-JP", "fr-CA" };
static const char * prefLangsMAA[]    = { "fr-FR", "ja-JP", "fr-CA" };
static const char * locsToUseMAA[]    = { "fr-CA" };
// For rdar://65843542
static const char * appleLocsMAB[]    = { "en", "yue", "yue-Hans", "zh-CN", "zh-HK" };
static const char * prefLangsMAB[]    = { "zh-Hans-US" };
static const char * locsToUseMAB[]    = { "zh-CN" };
// For rdar://66729600
static const char * appleLocsMAC[]    = { "en", "en-AU", "en-GB"};
static const char * prefLangsMAC[]    = { "en-US", "en-GB" };
static const char * locsToUseMAC[]    = { "en" };
static const char * appleLocsMAD[]    = { "en", "zh-CN", "en-AU" };
static const char * prefLangsMAD[]    = { "en-CN", "zh-CN", "en-AU" };
static const char * locsToUseMAD[]    = { "en" };
// For rdar://66403688
static const char * appleLocsMAE[]    = { "unk", "zh-Hant", "yue" };
static const char * prefLangsMAE[]    = { "zh-Hans" };
static const char * locsToUseMAE[]    = {  };
// For rdar://68146613
static const char * appleLocsMAF[]    = { "zxx", "en_HK", "en_MO" };
static const char * prefLangsMAF[]    = { "th_TH" };
static const char * locsToUseMAF[]    = { "zxx" };
// For rdar://69272236
static const char * appleLocsMAG[]    = { "en_US", "en_GB" };
static const char * prefLangsMAG[]    = { "en_BN" };
static const char * locsToUseMAG[]    = { "en_GB" };
// For rdar://67469388
static const char * appleLocsMAH[]    = { "en", "zh-CN", "zh-TW", "zh-HK" };
static const char * prefLangsMAHa[]   = { "yue-Hant-HK" };
static const char * prefLangsMAHb[]   = { "zh-Hant-HK" };
static const char * locsToUseMAH[]    = { "zh-HK", "zh-TW" };
// For rdar://70677637 (and rdar://69336571)
static const char * appleLocsMAIa[]    = { "zh-Hans", "en_GB" };
static const char * appleLocsMAIb[]    = { "zh-Hans", "en", "en_GB" };
static const char * prefLangsMAI[]     = { "hi-Latn" };
static const char * locsToUseMAIa[]    = { "en_GB" };
static const char * locsToUseMAIb[]    = { "en_GB", "en" };
// For rdar://79163271
static const char * appleLocsMAJa[]    = { "en", "tl" };
static const char * appleLocsMAJb[]    = { "en", "tl_PH" };
static const char * appleLocsMAJc[]    = { "en", "fil" };
static const char * appleLocsMAJd[]    = { "en", "fil_PH" };
static const char * prefLangsMAJa[]    = { "tl" };
static const char * prefLangsMAJb[]    = { "tl_PH" };
static const char * prefLangsMAJc[]    = { "fil" };
static const char * prefLangsMAJd[]    = { "fil_PH" };
static const char * locsToUseMAJa[]    = { "tl" };
static const char * locsToUseMAJb[]    = { "tl_PH" };
static const char * locsToUseMAJc[]    = { "fil" };
static const char * locsToUseMAJd[]    = { "fil_PH" };
static const char * appleLocsMAK[]     = { "en_US", "sr_Latn_RS" };
static const char * prefLangsMAK[]     = { "sh_RS" };
static const char * locsToUseMAK[]     = { "sr_Latn_RS" };
static const char * appleLocsMAL[]     = { "en_US", "fa_AF" };
static const char * prefLangsMAL[]     = { "prs_AF" };
static const char * locsToUseMAL[]     = { "fa_AF" };
// For rdar://99195843
static const char * appleLocsMAM[]    = { "en", "no" };
static const char * prefLangsMAMa[]   = { "nn" };
static const char * prefLangsMAMb[]   = { "nb" };
static const char * locsToUseMAM[]    = { "no" };
// For rdar://120006679
static const char * appleLocsMAN[]    = { "ar", "bg", "cs", "en-AU", "en-GB", "en-IN", "hi", "ja", "pt", "zh-Hans", "zh-Hant" };
static const char * prefLangsMAN[]    = { "en", "en-GB", "en-AU", "en-IN", "zh-Hans", "zh-Hant", "zh-HK", "ja", "es", "es-419", "fr", "fr-CA", "de", "ru", "pt-BR", "pt-PT", "it", "ko", "tr", "nl", "ar", "th", "sv", "da", "vi", "nb", "pl", "fi", "id", "he", "el", "ro", "hu", "cs", "ca", "sk", "uk", "hr", "ms", "hi", "kk-Cyrl", "bg", };
static const char * locsToUseMAN[]    = { "en-GB" };


typedef struct {
    const char *  name;
    const char ** availLocs;
    int32_t       availLocsCount;
    const char ** prefLangs;
    int32_t       prefLangsCount;
    const char ** locsToUse;
    int32_t       locsToUseCount;
} MultiPrefTest;

static const MultiPrefTest multiTestSets[] = {
    { "M1",  appleLocsM1,  UPRV_LENGTHOF(appleLocsM1), prefLangsM1, UPRV_LENGTHOF(prefLangsM1), locsToUseM1, UPRV_LENGTHOF(locsToUseM1) },
    //
    { "M2",  appleLocsM2,  UPRV_LENGTHOF(appleLocsM2), prefLangsM2, UPRV_LENGTHOF(prefLangsM2), locsToUseM2, UPRV_LENGTHOF(locsToUseM2) },
    { "M3",  appleLocsM3,  UPRV_LENGTHOF(appleLocsM3), prefLangsM3, UPRV_LENGTHOF(prefLangsM3), locsToUseM3, UPRV_LENGTHOF(locsToUseM3) },
    { "M4",  appleLocsM4,  UPRV_LENGTHOF(appleLocsM4), prefLangsM4, UPRV_LENGTHOF(prefLangsM4), locsToUseM4, UPRV_LENGTHOF(locsToUseM4) },
    //
    { "M5",  appleLocsM5,  UPRV_LENGTHOF(appleLocsM5), prefLangsM5, UPRV_LENGTHOF(prefLangsM5), locsToUseM5, UPRV_LENGTHOF(locsToUseM5) },
    { "M6",  appleLocsM6,  UPRV_LENGTHOF(appleLocsM6), prefLangsM6, UPRV_LENGTHOF(prefLangsM6), locsToUseM6, UPRV_LENGTHOF(locsToUseM6) },
    { "M7",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsM7, UPRV_LENGTHOF(prefLangsM7), locsToUseM7, UPRV_LENGTHOF(locsToUseM7) },
    { "M8",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsM8, UPRV_LENGTHOF(prefLangsM8), locsToUseM8, UPRV_LENGTHOF(locsToUseM8) },
    { "M9",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsM9, UPRV_LENGTHOF(prefLangsM9), locsToUseM9, UPRV_LENGTHOF(locsToUseM9) },
    { "MA",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsMA, UPRV_LENGTHOF(prefLangsMA), locsToUseMA, UPRV_LENGTHOF(locsToUseMA) },
    { "MB",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsMB, UPRV_LENGTHOF(prefLangsMB), locsToUseMB, UPRV_LENGTHOF(locsToUseMB) },
    { "MC",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsMC, UPRV_LENGTHOF(prefLangsMC), locsToUseMC, UPRV_LENGTHOF(locsToUseMC) },
    { "MD",  appleLocsMx,  UPRV_LENGTHOF(appleLocsMx), prefLangsMD, UPRV_LENGTHOF(prefLangsMD), locsToUseMD, UPRV_LENGTHOF(locsToUseMD) },
    { "ME",  appleLocsME,  UPRV_LENGTHOF(appleLocsME), prefLangsME, UPRV_LENGTHOF(prefLangsME), locsToUseME, UPRV_LENGTHOF(locsToUseME) },
    { "MF",  appleLocsMF,  UPRV_LENGTHOF(appleLocsMF), prefLangsMF, UPRV_LENGTHOF(prefLangsMF), locsToUseMF, UPRV_LENGTHOF(locsToUseMF) },
    { "MG",  appleLocsMG,  UPRV_LENGTHOF(appleLocsMG), prefLangsMG, UPRV_LENGTHOF(prefLangsMG), locsToUseMG, UPRV_LENGTHOF(locsToUseMG) },
    { "MH",  appleLocsMH,  UPRV_LENGTHOF(appleLocsMH), prefLangsMH, UPRV_LENGTHOF(prefLangsMH), locsToUseMH, UPRV_LENGTHOF(locsToUseMH) },
    { "MI",  appleLocsMI,  UPRV_LENGTHOF(appleLocsMI), prefLangsMI, UPRV_LENGTHOF(prefLangsMI), locsToUseMI, UPRV_LENGTHOF(locsToUseMI) },
    { "MJ",  appleLocsMJ,  UPRV_LENGTHOF(appleLocsMJ), prefLangsMJ, UPRV_LENGTHOF(prefLangsMJ), locsToUseMJ, UPRV_LENGTHOF(locsToUseMJ) },
    { "MK",  appleLocsMK,  UPRV_LENGTHOF(appleLocsMK), prefLangsMK, UPRV_LENGTHOF(prefLangsMK), locsToUseMK, UPRV_LENGTHOF(locsToUseMK) },
    { "MK1a",  appleLocsMK,  UPRV_LENGTHOF(appleLocsMK), prefLangsMK1a, UPRV_LENGTHOF(prefLangsMK1a), locsToUseMK1, UPRV_LENGTHOF(locsToUseMK1) },
    { "MK1b",  appleLocsMK,  UPRV_LENGTHOF(appleLocsMK), prefLangsMK1b, UPRV_LENGTHOF(prefLangsMK1b), locsToUseMK1, UPRV_LENGTHOF(locsToUseMK1) },
    { "ML",  appleLocsML,  UPRV_LENGTHOF(appleLocsML), prefLangsML, UPRV_LENGTHOF(prefLangsML), locsToUseML, UPRV_LENGTHOF(locsToUseML) },
    { "ML1",  appleLocsML,  UPRV_LENGTHOF(appleLocsML), prefLangsML1, UPRV_LENGTHOF(prefLangsML1), locsToUseML1, UPRV_LENGTHOF(locsToUseML1) },
    { "ML2",  appleLocsML,  UPRV_LENGTHOF(appleLocsML), prefLangsML2, UPRV_LENGTHOF(prefLangsML2), locsToUseML2, UPRV_LENGTHOF(locsToUseML2) },
    { "MM11",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM21",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM31",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM41",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM51",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM61",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM1, UPRV_LENGTHOF(prefLangsMM1), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM12",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM22",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM32",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM42",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM52",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM62",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM2, UPRV_LENGTHOF(prefLangsMM2), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM13",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM23",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM33",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM43",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM53",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM63",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM3, UPRV_LENGTHOF(prefLangsMM3), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM14",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM24",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM34",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM44",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM54",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMen,   UPRV_LENGTHOF(locsToUseMMen)   }, // want en, see rdar://22012864
    { "MM64",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM4, UPRV_LENGTHOF(prefLangsMM4), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM15",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM25",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM35",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM45",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM55",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM65",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM5, UPRV_LENGTHOF(prefLangsMM5), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM16",  appleLocsMM1,  UPRV_LENGTHOF(appleLocsMM1), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM26",  appleLocsMM2,  UPRV_LENGTHOF(appleLocsMM2), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM36",  appleLocsMM3,  UPRV_LENGTHOF(appleLocsMM3), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM46",  appleLocsMM4,  UPRV_LENGTHOF(appleLocsMM4), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptPT, UPRV_LENGTHOF(locsToUseMMptPT) },
    { "MM56",  appleLocsMM5,  UPRV_LENGTHOF(appleLocsMM5), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MM66",  appleLocsMM6,  UPRV_LENGTHOF(appleLocsMM6), prefLangsMM6, UPRV_LENGTHOF(prefLangsMM6), locsToUseMMptBR, UPRV_LENGTHOF(locsToUseMMptBR) },
    { "MN1",   appleLocsMN,   UPRV_LENGTHOF(appleLocsMN),  prefLangsMN1, UPRV_LENGTHOF(prefLangsMN1), locsToUseMN_U,   UPRV_LENGTHOF(locsToUseMN_U) },
    { "MN2",   appleLocsMN,   UPRV_LENGTHOF(appleLocsMN),  prefLangsMN2, UPRV_LENGTHOF(prefLangsMN2), locsToUseMN_U,   UPRV_LENGTHOF(locsToUseMN_U) },
    { "MN3",   appleLocsMN,   UPRV_LENGTHOF(appleLocsMN),  prefLangsMN3, UPRV_LENGTHOF(prefLangsMN3), locsToUseMN_U,   UPRV_LENGTHOF(locsToUseMN_U) },
    { "MN4",   appleLocsMN,   UPRV_LENGTHOF(appleLocsMN),  prefLangsMN4, UPRV_LENGTHOF(prefLangsMN4), locsToUseMN_U,   UPRV_LENGTHOF(locsToUseMN_U) },
    { "MO",    appleLocsMO,   UPRV_LENGTHOF(appleLocsMO),  prefLangsMO1, UPRV_LENGTHOF(prefLangsMO1), locsToUseMO1,    UPRV_LENGTHOF(locsToUseMO1) },
    { "MP",    appleLocsMP,   UPRV_LENGTHOF(appleLocsMP),  prefLangsMP,  UPRV_LENGTHOF(prefLangsMP),  locsToUseMP,     UPRV_LENGTHOF(locsToUseMP) },
    { "MQ1a",  appleLocsMQa,  UPRV_LENGTHOF(appleLocsMQa), prefLangsMQ1, UPRV_LENGTHOF(prefLangsMQ1), locsToUseMQ1,    UPRV_LENGTHOF(locsToUseMQ1) },
//  { "MQ1b",  appleLocsMQb,  UPRV_LENGTHOF(appleLocsMQb), prefLangsMQ1, UPRV_LENGTHOF(prefLangsMQ1), locsToUseMQ1,    UPRV_LENGTHOF(locsToUseMQ1) }, // still to do for rdar://34459988
    { "MQ2a",  appleLocsMQa,  UPRV_LENGTHOF(appleLocsMQa), prefLangsMQ2, UPRV_LENGTHOF(prefLangsMQ2), locsToUseMQ2a,   UPRV_LENGTHOF(locsToUseMQ2a) },
    { "MQ2b",  appleLocsMQb,  UPRV_LENGTHOF(appleLocsMQb), prefLangsMQ2, UPRV_LENGTHOF(prefLangsMQ2), locsToUseMQ2b,   UPRV_LENGTHOF(locsToUseMQ2b) },
    { "MRa",   appleLocsMRa,  UPRV_LENGTHOF(appleLocsMRa), prefLangsMRx, UPRV_LENGTHOF(prefLangsMRx), locsToUseMRa,    UPRV_LENGTHOF(locsToUseMRa) },
    { "MRb",   appleLocsMRb,  UPRV_LENGTHOF(appleLocsMRb), prefLangsMRx, UPRV_LENGTHOF(prefLangsMRx), locsToUseMRb,    UPRV_LENGTHOF(locsToUseMRb) },
    { "MRa",   appleLocsMRa,  UPRV_LENGTHOF(appleLocsMRa), prefLangsMRy, UPRV_LENGTHOF(prefLangsMRy), locsToUseMRa,    UPRV_LENGTHOF(locsToUseMRa) },
    { "MRb",   appleLocsMRb,  UPRV_LENGTHOF(appleLocsMRb), prefLangsMRy, UPRV_LENGTHOF(prefLangsMRy), locsToUseMRb,    UPRV_LENGTHOF(locsToUseMRb) },
    { "MSax",  appleLocsMSa,  UPRV_LENGTHOF(appleLocsMSa), prefLangsMSx, UPRV_LENGTHOF(prefLangsMSx), locsToUseMSa,    UPRV_LENGTHOF(locsToUseMSa) },
    { "MSay",  appleLocsMSa,  UPRV_LENGTHOF(appleLocsMSa), prefLangsMSy, UPRV_LENGTHOF(prefLangsMSy), locsToUseMSa,    UPRV_LENGTHOF(locsToUseMSa) },
    { "MSbx",  appleLocsMSb,  UPRV_LENGTHOF(appleLocsMSb), prefLangsMSx, UPRV_LENGTHOF(prefLangsMSx), locsToUseMSb,    UPRV_LENGTHOF(locsToUseMSb) },
    { "MSby",  appleLocsMSb,  UPRV_LENGTHOF(appleLocsMSb), prefLangsMSy, UPRV_LENGTHOF(prefLangsMSy), locsToUseMSb,    UPRV_LENGTHOF(locsToUseMSb) },
    { "MTa",   appleLocsMT,   UPRV_LENGTHOF(appleLocsMT),  prefLangsMTa, UPRV_LENGTHOF(prefLangsMTa), locsToUseMTa,    UPRV_LENGTHOF(locsToUseMTa) },
    { "MTb",   appleLocsMT,   UPRV_LENGTHOF(appleLocsMT),  prefLangsMTb, UPRV_LENGTHOF(prefLangsMTb), locsToUseMTb,    UPRV_LENGTHOF(locsToUseMTb) },
    { "MU",    appleLocsMU,   UPRV_LENGTHOF(appleLocsMU),  prefLangsMU,  UPRV_LENGTHOF(prefLangsMU),  locsToUseMU,     UPRV_LENGTHOF(locsToUseMU)  }, // rdar://64350332
    { "MVa",   appleLocsMVa,  UPRV_LENGTHOF(appleLocsMVa), prefLangsMVa, UPRV_LENGTHOF(prefLangsMVa), locsToUseMV,     UPRV_LENGTHOF(locsToUseMV)  }, // rdar://59520369
    { "MVb",   appleLocsMVb,  UPRV_LENGTHOF(appleLocsMVb), prefLangsMVa, UPRV_LENGTHOF(prefLangsMVa), locsToUseMV,     UPRV_LENGTHOF(locsToUseMV)  }, // rdar://59520369
    { "MVc",   appleLocsMVa,  UPRV_LENGTHOF(appleLocsMVa), prefLangsMVc, UPRV_LENGTHOF(prefLangsMVc), locsToUseMV,     UPRV_LENGTHOF(locsToUseMV)  }, // rdar://59520369
    { "MW",   appleLocsMW,  UPRV_LENGTHOF(appleLocsMW), prefLangsMW, UPRV_LENGTHOF(prefLangsMW), locsToUseMW,     UPRV_LENGTHOF(locsToUseMW)  }, // rdar://59520369
    { "MX",   appleLocsMX,  UPRV_LENGTHOF(appleLocsMX), prefLangsMX, UPRV_LENGTHOF(prefLangsMX), locsToUseMX,     UPRV_LENGTHOF(locsToUseMW)  }, // rdar://64811575
    { "MYaa",   appleLocsMYa,  UPRV_LENGTHOF(appleLocsMYa), prefLangsMYa, UPRV_LENGTHOF(prefLangsMYa), locsToUseMYa,     UPRV_LENGTHOF(locsToUseMYa)  }, // rdar://64497611
    { "MYba",   appleLocsMYb,  UPRV_LENGTHOF(appleLocsMYb), prefLangsMYa, UPRV_LENGTHOF(prefLangsMYa), locsToUseMYc,     UPRV_LENGTHOF(locsToUseMYc)  }, // rdar://64497611
    { "MYca",   appleLocsMYc,  UPRV_LENGTHOF(appleLocsMYc), prefLangsMYa, UPRV_LENGTHOF(prefLangsMYa), locsToUseMYb,     UPRV_LENGTHOF(locsToUseMYb)  }, // rdar://64497611
    { "MYab",   appleLocsMYa,  UPRV_LENGTHOF(appleLocsMYa), prefLangsMYb, UPRV_LENGTHOF(prefLangsMYb), locsToUseMYc,     UPRV_LENGTHOF(locsToUseMYc)  }, // rdar://64497611
    { "MYbb",   appleLocsMYb,  UPRV_LENGTHOF(appleLocsMYb), prefLangsMYb, UPRV_LENGTHOF(prefLangsMYb), locsToUseMYc,     UPRV_LENGTHOF(locsToUseMYc)  }, // rdar://64497611
    { "MYcb",   appleLocsMYc,  UPRV_LENGTHOF(appleLocsMYc), prefLangsMYb, UPRV_LENGTHOF(prefLangsMYb), locsToUseMYb,     UPRV_LENGTHOF(locsToUseMYb)  }, // rdar://64497611
    { "MZaa",   appleLocsMZa,  UPRV_LENGTHOF(appleLocsMZa), prefLangsMZa, UPRV_LENGTHOF(prefLangsMZa), locsToUseMZa,     UPRV_LENGTHOF(locsToUseMZa)  }, // rdar://64497611
    { "MZba",   appleLocsMZb,  UPRV_LENGTHOF(appleLocsMZb), prefLangsMZa, UPRV_LENGTHOF(prefLangsMZa), locsToUseMZc,     UPRV_LENGTHOF(locsToUseMZc)  }, // rdar://64497611
    { "MZca",   appleLocsMZc,  UPRV_LENGTHOF(appleLocsMZc), prefLangsMZa, UPRV_LENGTHOF(prefLangsMZa), locsToUseMZb,     UPRV_LENGTHOF(locsToUseMZb)  }, // rdar://64497611
    { "MZab",   appleLocsMZa,  UPRV_LENGTHOF(appleLocsMZa), prefLangsMZb, UPRV_LENGTHOF(prefLangsMZb), locsToUseMZc,     UPRV_LENGTHOF(locsToUseMZc)  }, // rdar://64497611
    { "MZbb",   appleLocsMZb,  UPRV_LENGTHOF(appleLocsMZb), prefLangsMZb, UPRV_LENGTHOF(prefLangsMZb), locsToUseMZc,     UPRV_LENGTHOF(locsToUseMZc)  }, // rdar://64497611
    { "MZcb",   appleLocsMZc,  UPRV_LENGTHOF(appleLocsMZc), prefLangsMZb, UPRV_LENGTHOF(prefLangsMZb), locsToUseMZb,     UPRV_LENGTHOF(locsToUseMZb)  }, // rdar://64497611
    { "MAA",   appleLocsMAA,  UPRV_LENGTHOF(appleLocsMAA), prefLangsMAA, UPRV_LENGTHOF(prefLangsMAA), locsToUseMAA,     UPRV_LENGTHOF(locsToUseMAA)  }, // rdar://64916132
    { "MAB",   appleLocsMAB,  UPRV_LENGTHOF(appleLocsMAB), prefLangsMAB, UPRV_LENGTHOF(prefLangsMAB), locsToUseMAB,     UPRV_LENGTHOF(locsToUseMAB)  }, // rdar://65843542
    { "MAC",   appleLocsMAC,  UPRV_LENGTHOF(appleLocsMAC), prefLangsMAC, UPRV_LENGTHOF(prefLangsMAC), locsToUseMAC,     UPRV_LENGTHOF(locsToUseMAC)  }, // rdar://66729600
    { "MAD",   appleLocsMAD,  UPRV_LENGTHOF(appleLocsMAD), prefLangsMAD, UPRV_LENGTHOF(prefLangsMAD), locsToUseMAD,     UPRV_LENGTHOF(locsToUseMAD)  }, // rdar://66729600
    { "MAE",   appleLocsMAE,  UPRV_LENGTHOF(appleLocsMAE), prefLangsMAE, UPRV_LENGTHOF(prefLangsMAE), locsToUseMAE,     UPRV_LENGTHOF(locsToUseMAE)  }, // rdar://66403688
    { "MAF",   appleLocsMAF,  UPRV_LENGTHOF(appleLocsMAF), prefLangsMAF, UPRV_LENGTHOF(prefLangsMAF), locsToUseMAF,     UPRV_LENGTHOF(locsToUseMAF)  }, // rdar://68146613
    { "MAG",   appleLocsMAG,  UPRV_LENGTHOF(appleLocsMAG), prefLangsMAG, UPRV_LENGTHOF(prefLangsMAG), locsToUseMAG,     UPRV_LENGTHOF(locsToUseMAG)  }, // rdar://69272236
    { "MAHa",  appleLocsMAH,  UPRV_LENGTHOF(appleLocsMAH), prefLangsMAHa, UPRV_LENGTHOF(prefLangsMAHa), locsToUseMAH,     UPRV_LENGTHOF(locsToUseMAH)  }, // rdar://67469388
    { "MAHb",  appleLocsMAH,  UPRV_LENGTHOF(appleLocsMAH), prefLangsMAHb, UPRV_LENGTHOF(prefLangsMAHb), locsToUseMAH,     UPRV_LENGTHOF(locsToUseMAH)  }, // rdar://67469388
    { "MAIa",   appleLocsMAIa,  UPRV_LENGTHOF(appleLocsMAIa), prefLangsMAI, UPRV_LENGTHOF(prefLangsMAI), locsToUseMAIa,     UPRV_LENGTHOF(locsToUseMAIa)  }, // rdar://70677637
    { "MAIb",   appleLocsMAIb,  UPRV_LENGTHOF(appleLocsMAIb), prefLangsMAI, UPRV_LENGTHOF(prefLangsMAI), locsToUseMAIb,     UPRV_LENGTHOF(locsToUseMAIb)  }, // rdar://70677637
    { "MAJaa",   appleLocsMAJa,  UPRV_LENGTHOF(appleLocsMAJa), prefLangsMAJa, UPRV_LENGTHOF(prefLangsMAJa), locsToUseMAJa,     UPRV_LENGTHOF(locsToUseMAJa)  }, // rdar://79163271
    { "MAJab",   appleLocsMAJa,  UPRV_LENGTHOF(appleLocsMAJa), prefLangsMAJb, UPRV_LENGTHOF(prefLangsMAJb), locsToUseMAJa,     UPRV_LENGTHOF(locsToUseMAJa)  }, // rdar://79163271
    { "MAJac",   appleLocsMAJa,  UPRV_LENGTHOF(appleLocsMAJa), prefLangsMAJc, UPRV_LENGTHOF(prefLangsMAJc), locsToUseMAJa,     UPRV_LENGTHOF(locsToUseMAJa)  }, // rdar://79163271
    { "MAJad",   appleLocsMAJa,  UPRV_LENGTHOF(appleLocsMAJa), prefLangsMAJd, UPRV_LENGTHOF(prefLangsMAJd), locsToUseMAJa,     UPRV_LENGTHOF(locsToUseMAJa)  }, // rdar://79163271
    { "MAJba",   appleLocsMAJb,  UPRV_LENGTHOF(appleLocsMAJb), prefLangsMAJa, UPRV_LENGTHOF(prefLangsMAJa), locsToUseMAJb,     UPRV_LENGTHOF(locsToUseMAJb)  }, // rdar://79163271
    { "MAJbb",   appleLocsMAJb,  UPRV_LENGTHOF(appleLocsMAJb), prefLangsMAJb, UPRV_LENGTHOF(prefLangsMAJb), locsToUseMAJb,     UPRV_LENGTHOF(locsToUseMAJb)  }, // rdar://79163271
    { "MAJbc",   appleLocsMAJb,  UPRV_LENGTHOF(appleLocsMAJb), prefLangsMAJc, UPRV_LENGTHOF(prefLangsMAJc), locsToUseMAJb,     UPRV_LENGTHOF(locsToUseMAJb)  }, // rdar://79163271
    { "MAJbd",   appleLocsMAJb,  UPRV_LENGTHOF(appleLocsMAJb), prefLangsMAJd, UPRV_LENGTHOF(prefLangsMAJd), locsToUseMAJb,     UPRV_LENGTHOF(locsToUseMAJb)  }, // rdar://79163271
    { "MAJca",   appleLocsMAJc,  UPRV_LENGTHOF(appleLocsMAJc), prefLangsMAJa, UPRV_LENGTHOF(prefLangsMAJa), locsToUseMAJc,     UPRV_LENGTHOF(locsToUseMAJc)  }, // rdar://79163271
    { "MAJcb",   appleLocsMAJc,  UPRV_LENGTHOF(appleLocsMAJc), prefLangsMAJb, UPRV_LENGTHOF(prefLangsMAJb), locsToUseMAJc,     UPRV_LENGTHOF(locsToUseMAJc)  }, // rdar://79163271
    { "MAJcc",   appleLocsMAJc,  UPRV_LENGTHOF(appleLocsMAJc), prefLangsMAJc, UPRV_LENGTHOF(prefLangsMAJc), locsToUseMAJc,     UPRV_LENGTHOF(locsToUseMAJc)  }, // rdar://79163271
    { "MAJcd",   appleLocsMAJc,  UPRV_LENGTHOF(appleLocsMAJc), prefLangsMAJd, UPRV_LENGTHOF(prefLangsMAJd), locsToUseMAJc,     UPRV_LENGTHOF(locsToUseMAJc)  }, // rdar://79163271
    { "MAJda",   appleLocsMAJd,  UPRV_LENGTHOF(appleLocsMAJd), prefLangsMAJa, UPRV_LENGTHOF(prefLangsMAJa), locsToUseMAJd,     UPRV_LENGTHOF(locsToUseMAJd)  }, // rdar://79163271
    { "MAJdb",   appleLocsMAJd,  UPRV_LENGTHOF(appleLocsMAJd), prefLangsMAJb, UPRV_LENGTHOF(prefLangsMAJb), locsToUseMAJd,     UPRV_LENGTHOF(locsToUseMAJd)  }, // rdar://79163271
    { "MAJdc",   appleLocsMAJd,  UPRV_LENGTHOF(appleLocsMAJd), prefLangsMAJc, UPRV_LENGTHOF(prefLangsMAJc), locsToUseMAJd,     UPRV_LENGTHOF(locsToUseMAJd)  }, // rdar://79163271
    { "MAJdd",   appleLocsMAJd,  UPRV_LENGTHOF(appleLocsMAJd), prefLangsMAJd, UPRV_LENGTHOF(prefLangsMAJd), locsToUseMAJd,     UPRV_LENGTHOF(locsToUseMAJd)  }, // rdar://79163271
    { "MAK",     appleLocsMAK,   UPRV_LENGTHOF(appleLocsMAK),  prefLangsMAK,  UPRV_LENGTHOF(prefLangsMAK),  locsToUseMAK,     UPRV_LENGTHOF(locsToUseMAK)  },  // rdar://79163271
    { "MAL",     appleLocsMAL,   UPRV_LENGTHOF(appleLocsMAL),  prefLangsMAL,  UPRV_LENGTHOF(prefLangsMAL),  locsToUseMAL,     UPRV_LENGTHOF(locsToUseMAL)  },  // rdar://79163271
    { "MAMa",    appleLocsMAM,   UPRV_LENGTHOF(appleLocsMAM),  prefLangsMAMa,  UPRV_LENGTHOF(prefLangsMAMa),  locsToUseMAM,     UPRV_LENGTHOF(locsToUseMAM)  },  // rdar://99195843
    { "MAMb",    appleLocsMAM,   UPRV_LENGTHOF(appleLocsMAM),  prefLangsMAMb,  UPRV_LENGTHOF(prefLangsMAMb),  locsToUseMAM,     UPRV_LENGTHOF(locsToUseMAM)  },  // rdar://99195843
    { "MAN" ,    appleLocsMAN,   UPRV_LENGTHOF(appleLocsMAN),  prefLangsMAN,  UPRV_LENGTHOF(prefLangsMAN),  locsToUseMAN,     UPRV_LENGTHOF(locsToUseMAN)  },  // rdar://120006679

    { NULL, NULL, 0, NULL, 0, NULL, 0 }
};


/* general enums */

enum { kMaxLocalizationsToUse = 8, kPrintArrayBufSize = 128 };

// array, array of pointers to strings to print
// count, count of array elements, may be -1 if array is terminated by a NULL entry
// buf, buffer into which to put concatenated strings
// bufSize, length of buf
static void printStringArray(const char **array, int32_t count, char *buf, int32_t bufSize) {
    char * bufPtr = buf;
    const char * curEntry;
    int32_t idx, countMax = bufSize/16;
    if (count < 0 || count > countMax) {
        count = countMax;
    }
    for (idx = 0; idx < count && (curEntry = *array++) != NULL; idx++) {
        int32_t len = sprintf(bufPtr, "%s\"%.12s\"", (idx > 0)? ", ": "", curEntry);
        if (len <= 0) {
            break;
        }
        bufPtr += len;
    }
    *bufPtr = 0; /* ensure termination */
}

static UBool equalStringArrays(const char **array1, int32_t count1, const char **array2, int32_t count2) {
    const char ** array1Ptr = array1;
    const char ** array2Ptr = array2;
    int32_t idx;
    if (count1 < 0) {
        count1 = 0;
        while (*array1Ptr++ != NULL) {
            count1++;
        }
    }
    if (count2 < 0) {
        count2 = 0;
        while (*array2Ptr++ != NULL) {
            count2++;
        }
    }
    if (count1 != count2) {
        return false;
    }
    for (idx = 0; idx < count1; idx++) {
        if (uprv_strcmp(array1[idx], array2[idx]) != 0) {
            return false;
        }
    }
    return true;
}

static void TestAppleLocalizationsToUse() {
    const AppleLocToUseTestSet * testSetPtr;
    const MultiPrefTest * multiSetPtr;
    const char * locsToUse[kMaxLocalizationsToUse];
    int32_t numLocsToUse;
    UErrorCode status;
    char printExpected[kPrintArrayBufSize];
    char printActual[kPrintArrayBufSize];

    for (testSetPtr = altuTestSets; testSetPtr->locAndCountEntriesPtr != NULL; testSetPtr++) {
        int32_t iLocSet, iLang;

        for (iLocSet = 0; iLocSet < kNumLocSets; iLocSet++) {
            for (iLang = 0; iLang < testSetPtr->appleLangAndLocCount; iLang++) {
                const char * language = testSetPtr->appleLangAndLocPtr[iLang].language;
                const char ** expLocsForSet = testSetPtr->appleLangAndLocPtr[iLang].expLocsForSets[iLocSet];
                status = U_ZERO_ERROR;

                numLocsToUse = ualoc_localizationsToUse(&language, 1,
                                                        testSetPtr->locAndCountEntriesPtr[iLocSet].locs, testSetPtr->locAndCountEntriesPtr[iLocSet].locCount,
                                                        locsToUse, kMaxLocalizationsToUse, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: ualoc_localizationsToUse testSet %d, locSet %d, lang %s, status %s\n",
                            testSetPtr-altuTestSets, iLocSet+1, language, u_errorName(status));
                } else if (numLocsToUse == 0 && expLocsForSet != NULL) {
                    printStringArray(expLocsForSet, -1, printExpected, kPrintArrayBufSize);
                    log_err("FAIL: ualoc_localizationsToUse testSet %d, locSet %d, lang %s, expect {%s}, get no results\n",
                            testSetPtr-altuTestSets, iLocSet+1, language, printExpected);
                } else if (numLocsToUse > 0 && expLocsForSet == NULL) {
                    printStringArray(locsToUse, numLocsToUse, printActual, kPrintArrayBufSize);
                    log_err("FAIL: ualoc_localizationsToUse testSet %d, locSet %d, lang %s, expect no results, get {%s}\n",
                            testSetPtr-altuTestSets, iLocSet+1, language, printActual);
                } else if (numLocsToUse > 0 && !equalStringArrays(expLocsForSet, -1, locsToUse, numLocsToUse)) {
                    printStringArray(expLocsForSet, -1, printExpected, kPrintArrayBufSize);
                    printStringArray(locsToUse, numLocsToUse, printActual, kPrintArrayBufSize);
                    log_err("FAIL: ualoc_localizationsToUse testSet %d, locSet %d, lang %s:\n            expect {%s}\n            get    {%s}\n",
                            testSetPtr-altuTestSets, iLocSet+1, language, printExpected, printActual);
                }
            }
        }
    }

   for (multiSetPtr = multiTestSets; multiSetPtr->name != NULL; multiSetPtr++) {
        status = U_ZERO_ERROR;
        numLocsToUse = ualoc_localizationsToUse(multiSetPtr->prefLangs, multiSetPtr->prefLangsCount, multiSetPtr->availLocs, multiSetPtr->availLocsCount, locsToUse, kMaxLocalizationsToUse, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: ualoc_localizationsToUse appleLocs%s, langs prefLangs%s, status %s\n", multiSetPtr->name, multiSetPtr->name, u_errorName(status));
        } else if (!equalStringArrays(multiSetPtr->locsToUse, multiSetPtr->locsToUseCount, locsToUse, numLocsToUse)) {
            printStringArray(multiSetPtr->locsToUse, multiSetPtr->locsToUseCount, printExpected, kPrintArrayBufSize);
            printStringArray(locsToUse, numLocsToUse, printActual, kPrintArrayBufSize);
            log_err("FAIL: ualoc_localizationsToUse appleLocs%s, langs prefLangs%s:\n            expect {%s}\n            get    {%s}\n",
                    multiSetPtr->name, multiSetPtr->name, printExpected, printActual);
        }
   }
}

#endif

// rdar://63313283
static void TestNorwegianDisplayNames(void) {
    UChar bokmalInBokmal[50];
    UChar bokmalInNynorsk[50];
    UChar nynorskInBokmal[50];
    UChar nynorskInNynorsk[50];
    UErrorCode err = U_ZERO_ERROR;
    
    uloc_getDisplayLanguage("nb", "nb", bokmalInBokmal, 50, &err);
    uloc_getDisplayLanguage("nb", "nn", bokmalInNynorsk, 50, &err);
    uloc_getDisplayLanguage("nn", "nb", nynorskInBokmal, 50, &err);
    uloc_getDisplayLanguage("nn", "nn", nynorskInNynorsk, 50, &err);
    
    if (assertSuccess("Error getting display names", &err)) {
        assertUEquals("Bokmal and Nynorsk don't have the same names for Bokmal", bokmalInBokmal, bokmalInNynorsk);
        assertUEquals("Bokmal and Nynorsk don't have the same names for Nynorsk", nynorskInBokmal, nynorskInNynorsk);
    }
}

static void TestSpecificDisplayNames(void) {
    
    // first column is locale whose name we want, second column is language to display it in, third column is expected result
    static const char* testLanguages[] = {
        "wuu",   "sv",      "shanghainesiska",   // rdar://66154565
        "mi",    "en",      "Māori",             // rdar://75300243
        "mi",    "en_NZ",   "Māori",             // rdar://75300243
        "hi_IN", "hi_Latn", "Hindi (Bharat)",    // rdar://125016053 (obsoletes: rdar://80522845)
        "nv",    "hi",      "नावाहो",              // rdar://82306232 (CLDR 42 also updated)
        "es_PA", "it",      "spagnolo (Panama)", // rdar://85413128
        "ii",    "en",      "Liangshan Yi",      // rdar://108460253
        "hmn_Hmng","en",    "Hmong (Pahawh)",    // rdar://108866340
        "hmn",    "en",     "Hmong",             // rdar://108866340
        "zgh_Tfng","en",    "Tamazight, Standard Moroccan (Tifinagh)",      // rdar://108866340
        "zgh",   "en",      "Tamazight, Standard Moroccan",                 // rdar://108866340
        "ii",    "yue-Hans","凉山彝语",            // rdar://108460253
        "ii",    "yue",     "涼山彝文",            // rdar://108460253
        "ii",    "zh-Hant", "涼山彝文",            // rdar://108460253
        "gu",    "ms",      "Gujarat",            // rdar://105577613
    };
    int32_t testLanguagesLength = UPRV_LENGTHOF(testLanguages);
    
    for (int32_t i = 0; i < testLanguagesLength; i += 3) {
        UErrorCode err = U_ZERO_ERROR;
        UChar displayName[200];
        char displayNameUTF8[200];
        uloc_getDisplayName(testLanguages[i], testLanguages[i + 1], displayName, 200, &err);
        
        if (assertSuccess("Error getting display language", &err)) {
            u_strToUTF8(displayNameUTF8, 200, NULL, displayName, -1, &err);
            if (assertSuccess("Error translating display name to UTF-8", &err)) {
                assertEquals("Display name mismatch", testLanguages[i + 2], displayNameUTF8);
            }
        }
    }
}

static void TestChinaNamesNotResolving(void) { // rdar://121879891
    UChar result[256] = {'\0'};
    UErrorCode status = U_ZERO_ERROR;
    UErrorCode expected_status = U_USING_DEFAULT_WARNING;
    char *localeToName;

    char * localesToName[5] = {
        "en_HK",
        "en_MO",
        "en_TW",
        "en_CN",
        "en_US"
    };
    
    for(int i=0; i < 5; i++) {
        status = U_ZERO_ERROR;
        localeToName = localesToName[i];
        uloc_getDisplayCountry(localeToName, "", result, 256, &status);
        if (status != expected_status) {
            log_data_err("uloc_getDisplayCountry for empty displayLocale and namedLocale %s returns unexpected status %s\n", localeToName, myErrorName(status));
        }
    }
}

// rdar://123393073
static void TestMvskokeAndLushootseedDisplayNames(void) {
    // first column is language in which to display
    // second column is that language's display name for Lushootseed (lut)
    // third column is that language's display name Mvskoke (mus)
    static const char* testLanguages[] = {
        "ar", "لوشوتسيد", "مسكوكي",
        "bg", "Лушуцид", "Мускоги",
        "ca", "lushootseed", "mvskoke",
        "cs", "lushootseedská", "mvskoke",
        "da", "lushootseed", "mvskoke",
        "de", "Lushootseed", "Mvskoke",
        "el", "Λάσουτσιντ", "Μοσκόγκι",
        "en", "Lushootseed", "Mvskoke",
        "en_AU", "Lushootseed", "Mvskoke",
        "en_GB", "Lushootseed", "Mvskoke",
        "es", "lushootseed", "mvskoke",
        "es_419", "lushootseed", "mvskoke",
        "fi", "lushootseed", "mvskoke",
        "fr", "lushootseed", "mvskoke",
        "fr_CA", "lushootseed", "mvskoke",
        "he", "לושוטסיד", "מוסקוגי",
        "hi", "लुशुत्सीद", "मस्कोगी",
        "hr", "lushootseed", "mvskoke",
        "hu", "lushootseed", "muszkoki",
        "id", "Lushootseed", "Mvskoke",
        "it", "lushootseed", "mvskoke", // rdar://126883239
        "ja", "ルシュツィード語", "マスコギ語",
        "kk", "лушуцид тілі", "маскоги тілі",
        "ko", "루슈트시드어", "머스코기어",
        "lt", "lašutsidų", "mvskokų",
        "lut", "dxʷləšucid · txʷəlšucid · xʷəlšucid", "mus", // yes, all three forms for lut x lut, and no entry for "mus"
        "ms", "Lushootseed", "Mvskoke",
        "mus", "lut", "Mvskoke", // yes, no entry for "lut"
        "nl", "Lushootseed", "Mvskoke",
        "no", "lushootseed", "mvskoke",
        "pl", "lushootseed", "mvskoke", // rdar://126883239
        "pt_BR", "lushootseed", "mvskoke",
        "pt_PT", "lushootseed", "mvskoke",
        "ro", "lushootseed", "mvskoke",
        "ru", "лушицид", "маскоги",
        "sk", "lushootseedské", "mvskoke",
        "sl", "lushootseed", "mvskoke",
        "sv", "lushootseed", "mvskoke",
        "th", "ลูชูตซีด", "มัสคีกี",
        "tr", "Lushootseed", "Mvskoke",
        "uk", "лушуцид", "маскогі",
        "vi", "Tiếng Lushootseed", "Tiếng Mvskoke",
        "yue_CN", "卢绍锡德语", "马斯科吉语",
        "zh_CN", "卢绍锡德语", "马斯科吉语",
        "zh_HK", "魯蘇奇文", "馬斯科吉文",
        "zh_TW", "魯蘇奇文", "馬斯科吉文",
    };
    int32_t testLanguagesLength = UPRV_LENGTHOF(testLanguages);
    
    for (int32_t i = 0; i < testLanguagesLength; i += 3) {
        UErrorCode err = U_ZERO_ERROR;
        UChar displayName[200];
        char displayNameUTF8[200];
        char errorMessage[200];

        uloc_getDisplayName("lut", testLanguages[i], displayName, 200, &err);
        if (assertSuccess("Error getting display language", &err)) {
            u_strToUTF8(displayNameUTF8, 200, NULL, displayName, -1, &err);
            if (assertSuccess("Error translating display name to UTF-8", &err)) {
                snprintf(errorMessage, sizeof(errorMessage), "Display name mismatch for '%s' x 'lut'", testLanguages[i]);
                assertEquals(errorMessage, testLanguages[i + 1], displayNameUTF8);
            }
        }

        uloc_getDisplayName("mus", testLanguages[i], displayName, 200, &err);
        if (assertSuccess("Error getting display language", &err)) {
            u_strToUTF8(displayNameUTF8, 200, NULL, displayName, -1, &err);
            if (assertSuccess("Error translating display name to UTF-8", &err)) {
                snprintf(errorMessage, sizeof(errorMessage), "Display name mismatch for '%s' x 'mus'", testLanguages[i]);
                assertEquals(errorMessage, testLanguages[i + 2], displayNameUTF8);
            }
        }

    }
}
#endif  // APPLE_ICU_CHANGES
