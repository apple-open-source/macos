// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CBIAPTS.C
*
* Modification History:
*        Name                     Description
*     Madhu Katragadda              Creation
*********************************************************************************/
/*C API TEST FOR BREAKITERATOR */
/**
* This is an API test.  It doesn't test very many cases, and doesn't
* try to test the full functionality.  It just calls each function in the class and
* verifies that it works on a basic level.
**/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include <stdlib.h>
#include <string.h>
#include "unicode/uloc.h"
#include "unicode/ubrk.h"
#include "unicode/ustring.h"
#include "unicode/ucnv.h"
#include "unicode/utext.h"
#include "cintltst.h"
#include "cbiapts.h"
#include "cmemory.h"

#define TEST_ASSERT_SUCCESS(status) {if (U_FAILURE(status)) { \
log_data_err("Failure at file %s, line %d, error = %s (Are you missing data?)\n", __FILE__, __LINE__, u_errorName(status));}}

#define TEST_ASSERT(expr) {if ((expr)==FALSE) { \
log_data_err("Test Failure at file %s, line %d (Are you missing data?)\n", __FILE__, __LINE__);}}

#define APPLE_ADDITIONS 1

#if !UCONFIG_NO_FILE_IO
static void TestBreakIteratorSafeClone(void);
#endif
static void TestBreakIteratorRules(void);
static void TestBreakIteratorRuleError(void);
static void TestBreakIteratorStatusVec(void);
static void TestBreakIteratorUText(void);
static void TestBreakIteratorTailoring(void);
static void TestBreakIteratorRefresh(void);
static void TestBug11665(void);
static void TestBreakIteratorSuppressions(void);
#if APPLE_ADDITIONS
static void TestRuleBasedTokenizer(void);
#endif

void addBrkIterAPITest(TestNode** root);

void addBrkIterAPITest(TestNode** root)
{
#if !UCONFIG_NO_FILE_IO
    addTest(root, &TestBreakIteratorCAPI, "tstxtbd/cbiapts/TestBreakIteratorCAPI");
    addTest(root, &TestBreakIteratorSafeClone, "tstxtbd/cbiapts/TestBreakIteratorSafeClone");
    addTest(root, &TestBreakIteratorUText, "tstxtbd/cbiapts/TestBreakIteratorUText");
#endif
    addTest(root, &TestBreakIteratorRules, "tstxtbd/cbiapts/TestBreakIteratorRules");
    addTest(root, &TestBreakIteratorRuleError, "tstxtbd/cbiapts/TestBreakIteratorRuleError");
    addTest(root, &TestBreakIteratorStatusVec, "tstxtbd/cbiapts/TestBreakIteratorStatusVec");
    addTest(root, &TestBreakIteratorTailoring, "tstxtbd/cbiapts/TestBreakIteratorTailoring");
    addTest(root, &TestBreakIteratorRefresh, "tstxtbd/cbiapts/TestBreakIteratorRefresh");
    addTest(root, &TestBug11665, "tstxtbd/cbiapts/TestBug11665");
#if !UCONFIG_NO_FILTERED_BREAK_ITERATION
    addTest(root, &TestBreakIteratorSuppressions, "tstxtbd/cbiapts/TestBreakIteratorSuppressions");
#endif
#if APPLE_ADDITIONS
    addTest(root, &TestRuleBasedTokenizer, "tstxtbd/cbiapts/TestRuleBasedTokenizer");
#endif
}

#define CLONETEST_ITERATOR_COUNT 2

/*
 *   Utility function for converting char * to UChar * strings, to
 *     simplify the test code.   Converted strings are put in heap allocated
 *     storage.   A hook (probably a local in the caller's code) allows all
 *     strings converted with that hook to be freed with a single call.
 */
typedef struct StringStruct {
        struct StringStruct   *link;
        UChar                 str[1];
    } StringStruct;


static UChar* toUChar(const char *src, void **freeHook) {
    /* Structure of the memory that we allocate on the heap */

    int32_t    numUChars;
    int32_t    destSize;
    UChar      stackBuf[2000 + sizeof(void *)/sizeof(UChar)];
    StringStruct  *dest;
    UConverter *cnv;

    UErrorCode status = U_ZERO_ERROR;
    if (src == NULL) {
        return NULL;
    };

    cnv = ucnv_open(NULL, &status);
    if(U_FAILURE(status) || cnv == NULL) {
        return NULL;
    }
    ucnv_reset(cnv);
    numUChars = ucnv_toUChars(cnv,
                  stackBuf,
                  2000,
                  src, -1,
                  &status);

    destSize = (numUChars+1) * sizeof(UChar) + sizeof(struct StringStruct);
    dest = (StringStruct *)malloc(destSize);
    if (dest != NULL) {
        if (status == U_BUFFER_OVERFLOW_ERROR || status == U_STRING_NOT_TERMINATED_WARNING) {
            ucnv_toUChars(cnv, dest->str, numUChars+1, src, -1, &status);
        } else if (status == U_ZERO_ERROR) {
            u_strcpy(dest->str, stackBuf);
        } else {
            free(dest);
            dest = NULL;
        }
    }

    ucnv_reset(cnv); /* be good citizens */
    ucnv_close(cnv);
    if (dest == NULL) {
        return NULL;
    }

    dest->link = (StringStruct*)(*freeHook);
    *freeHook = dest;
    return dest->str;
}

static void freeToUCharStrings(void **hook) {
    StringStruct  *s = *(StringStruct **)hook;
    while (s != NULL) {
        StringStruct *next = s->link;
        free(s);
        s = next;
    }
}


#if !UCONFIG_NO_FILE_IO
static void TestBreakIteratorCAPI()
{
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *word, *sentence, *line, *character, *b, *bogus;
    int32_t start,pos,end,to;
    int32_t i;
    int32_t count = 0;

    UChar text[50];

    /* Note:  the adjacent "" are concatenating strings, not adding a \" to the
       string, which is probably what whoever wrote this intended.  Don't fix,
       because it would throw off the hard coded break positions in the following
       tests. */
    u_uastrcpy(text, "He's from Africa. ""Mr. Livingston, I presume?"" Yeah");


/*test ubrk_open()*/
    log_verbose("\nTesting BreakIterator open functions\n");

    /* Use french for fun */
    word         = ubrk_open(UBRK_WORD, "en_US", text, u_strlen(text), &status);
    if(status == U_FILE_ACCESS_ERROR) {
        log_data_err("Check your data - it doesn't seem to be around\n");
        return;
    } else if(U_FAILURE(status)){
        log_err_status(status, "FAIL: Error in ubrk_open() for word breakiterator: %s\n", myErrorName(status));
    }
    else{
        log_verbose("PASS: Successfully opened  word breakiterator\n");
    }

    sentence     = ubrk_open(UBRK_SENTENCE, "en_US", text, u_strlen(text), &status);
    if(U_FAILURE(status)){
        log_err_status(status, "FAIL: Error in ubrk_open() for sentence breakiterator: %s\n", myErrorName(status));
        return;
    }
    else{
        log_verbose("PASS: Successfully opened  sentence breakiterator\n");
    }

    line         = ubrk_open(UBRK_LINE, "en_US", text, u_strlen(text), &status);
    if(U_FAILURE(status)){
        log_err("FAIL: Error in ubrk_open() for line breakiterator: %s\n", myErrorName(status));
        return;
    }
    else{
        log_verbose("PASS: Successfully opened  line breakiterator\n");
    }

    character     = ubrk_open(UBRK_CHARACTER, "en_US", text, u_strlen(text), &status);
    if(U_FAILURE(status)){
        log_err("FAIL: Error in ubrk_open() for character breakiterator: %s\n", myErrorName(status));
        return;
    }
    else{
        log_verbose("PASS: Successfully opened  character breakiterator\n");
    }
    /*trying to open an illegal iterator*/
    bogus     = ubrk_open((UBreakIteratorType)5, "en_US", text, u_strlen(text), &status);
    if(bogus != NULL) {
        log_err("FAIL: expected NULL from opening an invalid break iterator.\n");
    }
    if(U_SUCCESS(status)){
        log_err("FAIL: Error in ubrk_open() for BOGUS breakiterator. Expected U_ILLEGAL_ARGUMENT_ERROR\n");
    }
    if(U_FAILURE(status)){
        if(status != U_ILLEGAL_ARGUMENT_ERROR){
            log_err("FAIL: Error in ubrk_open() for BOGUS breakiterator. Expected U_ILLEGAL_ARGUMENT_ERROR\n Got %s\n", myErrorName(status));
        }
    }
    status=U_ZERO_ERROR;


/* ======= Test ubrk_countAvialable() and ubrk_getAvialable() */

    log_verbose("\nTesting ubrk_countAvailable() and ubrk_getAvailable()\n");
    count=ubrk_countAvailable();
    /* use something sensible w/o hardcoding the count */
    if(count < 0){
        log_err("FAIL: Error in ubrk_countAvialable() returned %d\n", count);
    }
    else{
        log_verbose("PASS: ubrk_countAvialable() successful returned %d\n", count);
    }
    for(i=0;i<count;i++)
    {
        log_verbose("%s\n", ubrk_getAvailable(i));
        if (ubrk_getAvailable(i) == 0)
            log_err("No locale for which breakiterator is applicable\n");
        else
            log_verbose("A locale %s for which breakiterator is applicable\n",ubrk_getAvailable(i));
    }

/*========Test ubrk_first(), ubrk_last()...... and other functions*/

    log_verbose("\nTesting the functions for word\n");
    start = ubrk_first(word);
    if(start!=0)
        log_err("error ubrk_start(word) did not return 0\n");
    log_verbose("first (word = %d\n", (int32_t)start);
       pos=ubrk_next(word);
    if(pos!=4)
        log_err("error ubrk_next(word) did not return 4\n");
    log_verbose("next (word = %d\n", (int32_t)pos);
    pos=ubrk_following(word, 4);
    if(pos!=5)
        log_err("error ubrl_following(word,4) did not return 6\n");
    log_verbose("next (word = %d\n", (int32_t)pos);
    end=ubrk_last(word);
    if(end!=49)
        log_err("error ubrk_last(word) did not return 49\n");
    log_verbose("last (word = %d\n", (int32_t)end);

    pos=ubrk_previous(word);
    log_verbose("%d   %d\n", end, pos);

    pos=ubrk_previous(word);
    log_verbose("%d \n", pos);

    if (ubrk_isBoundary(word, 2) != FALSE) {
        log_err("error ubrk_isBoundary(word, 2) did not return FALSE\n");
    }
    pos=ubrk_current(word);
    if (pos != 4) {
        log_err("error ubrk_current() != 4 after ubrk_isBoundary(word, 2)\n");
    }
    if (ubrk_isBoundary(word, 4) != TRUE) {
        log_err("error ubrk_isBoundary(word, 4) did not return TRUE\n");
    }



    log_verbose("\nTesting the functions for character\n");
    ubrk_first(character);
    pos = ubrk_following(character, 5);
    if(pos!=6)
       log_err("error ubrk_following(character,5) did not return 6\n");
    log_verbose("Following (character,5) = %d\n", (int32_t)pos);
    pos=ubrk_following(character, 18);
    if(pos!=19)
       log_err("error ubrk_following(character,18) did not return 19\n");
    log_verbose("Followingcharacter,18) = %d\n", (int32_t)pos);
    pos=ubrk_preceding(character, 22);
    if(pos!=21)
       log_err("error ubrk_preceding(character,22) did not return 21\n");
    log_verbose("preceding(character,22) = %d\n", (int32_t)pos);


    log_verbose("\nTesting the functions for line\n");
    pos=ubrk_first(line);
    if(pos != 0)
        log_err("error ubrk_first(line) returned %d, expected 0\n", (int32_t)pos);
    pos = ubrk_next(line);
    pos=ubrk_following(line, 18);
    if(pos!=22)
        log_err("error ubrk_following(line) did not return 22\n");
    log_verbose("following (line) = %d\n", (int32_t)pos);


    log_verbose("\nTesting the functions for sentence\n");
    ubrk_first(sentence);
    pos = ubrk_current(sentence);
    log_verbose("Current(sentence) = %d\n", (int32_t)pos);
       pos = ubrk_last(sentence);
    if(pos!=49)
        log_err("error ubrk_last for sentence did not return 49\n");
    log_verbose("Last (sentence) = %d\n", (int32_t)pos);
    ubrk_first(sentence);
    to = ubrk_following( sentence, 0 );
    if (to == 0) log_err("ubrk_following returned 0\n");
    to = ubrk_preceding( sentence, to );
    if (to != 0) log_err("ubrk_preceding didn't return 0\n");
    if (ubrk_first(sentence)!=ubrk_current(sentence)) {
        log_err("error in ubrk_first() or ubrk_current()\n");
    }


    /*---- */
    /*Testing ubrk_open and ubrk_close()*/
   log_verbose("\nTesting open and close for us locale\n");
    b = ubrk_open(UBRK_WORD, "fr_FR", text, u_strlen(text), &status);
    if (U_FAILURE(status)) {
        log_err("ubrk_open for word returned NULL: %s\n", myErrorName(status));
    }
    ubrk_close(b);

    /* Test setText and setUText */
    {
        UChar s1[] = {0x41, 0x42, 0x20, 0};
        UChar s2[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0};
        UText *ut = NULL;
        UBreakIterator *bb;
        int j;

        log_verbose("\nTesting ubrk_setText() and ubrk_setUText()\n");
        status = U_ZERO_ERROR;
        bb = ubrk_open(UBRK_WORD, "en_US", NULL, 0, &status);
        TEST_ASSERT_SUCCESS(status);
        ubrk_setText(bb, s1, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        ubrk_first(bb);
        j = ubrk_next(bb);
        TEST_ASSERT(j == 2);
        ut = utext_openUChars(ut, s2, -1, &status);
        ubrk_setUText(bb, ut, &status);
        TEST_ASSERT_SUCCESS(status);
        j = ubrk_next(bb);
        TEST_ASSERT(j == 5);

        ubrk_close(bb);
        utext_close(ut);
    }

    ubrk_close(word);
    ubrk_close(sentence);
    ubrk_close(line);
    ubrk_close(character);
}

static void TestBreakIteratorSafeClone(void)
{
    UChar text[51];     /* Keep this odd to test for 64-bit memory alignment */
                        /*  NOTE:  This doesn't reliably force mis-alignment of following items. */
    uint8_t buffer [CLONETEST_ITERATOR_COUNT] [U_BRK_SAFECLONE_BUFFERSIZE];
    int32_t bufferSize = U_BRK_SAFECLONE_BUFFERSIZE;

    UBreakIterator * someIterators [CLONETEST_ITERATOR_COUNT];
    UBreakIterator * someClonedIterators [CLONETEST_ITERATOR_COUNT];

    UBreakIterator * brk;
    UErrorCode status = U_ZERO_ERROR;
    int32_t start,pos;
    int32_t i;

    /*Testing ubrk_safeClone */

    /* Note:  the adjacent "" are concatenating strings, not adding a \" to the
       string, which is probably what whoever wrote this intended.  Don't fix,
       because it would throw off the hard coded break positions in the following
       tests. */
    u_uastrcpy(text, "He's from Africa. ""Mr. Livingston, I presume?"" Yeah");

    /* US & Thai - rule-based & dictionary based */
    someIterators[0] = ubrk_open(UBRK_WORD, "en_US", text, u_strlen(text), &status);
    if(!someIterators[0] || U_FAILURE(status)) {
      log_data_err("Couldn't open en_US word break iterator - %s\n", u_errorName(status));
      return;
    }

    someIterators[1] = ubrk_open(UBRK_WORD, "th_TH", text, u_strlen(text), &status);
    if(!someIterators[1] || U_FAILURE(status)) {
      log_data_err("Couldn't open th_TH word break iterator - %s\n", u_errorName(status));
      return;
    }

    /* test each type of iterator */
    for (i = 0; i < CLONETEST_ITERATOR_COUNT; i++)
    {

        /* Check the various error & informational states */

        /* Null status - just returns NULL */
        if (NULL != ubrk_safeClone(someIterators[i], buffer[i], &bufferSize, NULL))
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with null status\n");
        }
        /* error status - should return 0 & keep error the same */
        status = U_MEMORY_ALLOCATION_ERROR;
        if (NULL != ubrk_safeClone(someIterators[i], buffer[i], &bufferSize, &status) || status != U_MEMORY_ALLOCATION_ERROR)
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with incoming error status\n");
        }
        status = U_ZERO_ERROR;

        /* Null buffer size pointer is ok */
        if (NULL == (brk = ubrk_safeClone(someIterators[i], buffer[i], NULL, &status)) || U_FAILURE(status))
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with null bufferSize pointer\n");
        }
        ubrk_close(brk);
        status = U_ZERO_ERROR;

        /* buffer size pointer is 0 - fill in pbufferSize with a size */
        bufferSize = 0;
        if (NULL != ubrk_safeClone(someIterators[i], buffer[i], &bufferSize, &status) ||
                U_FAILURE(status) || bufferSize <= 0)
        {
            log_err("FAIL: Cloned Iterator failed a sizing request ('preflighting')\n");
        }
        /* Verify our define is large enough  */
        if (U_BRK_SAFECLONE_BUFFERSIZE < bufferSize)
        {
          log_err("FAIL: Pre-calculated buffer size is too small - %d but needed %d\n", U_BRK_SAFECLONE_BUFFERSIZE, bufferSize);
        }
        /* Verify we can use this run-time calculated size */
        if (NULL == (brk = ubrk_safeClone(someIterators[i], buffer[i], &bufferSize, &status)) || U_FAILURE(status))
        {
            log_err("FAIL: Iterator can't be cloned with run-time size\n");
        }
        if (brk)
            ubrk_close(brk);
        /* size one byte too small - should allocate & let us know */
        if (bufferSize > 1) {
            --bufferSize;
        }
        if (NULL == (brk = ubrk_safeClone(someIterators[i], NULL, &bufferSize, &status)) || status != U_SAFECLONE_ALLOCATED_WARNING)
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with too-small buffer size\n");
        }
        if (brk)
            ubrk_close(brk);
        status = U_ZERO_ERROR;
        bufferSize = U_BRK_SAFECLONE_BUFFERSIZE;

        /* Null buffer pointer - return Iterator & set error to U_SAFECLONE_ALLOCATED_ERROR */
        if (NULL == (brk = ubrk_safeClone(someIterators[i], NULL, &bufferSize, &status)) || status != U_SAFECLONE_ALLOCATED_WARNING)
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with null buffer pointer\n");
        }
        if (brk)
            ubrk_close(brk);
        status = U_ZERO_ERROR;

        /* Mis-aligned buffer pointer. */
        {
            char  stackBuf[U_BRK_SAFECLONE_BUFFERSIZE+sizeof(void *)];

            brk = ubrk_safeClone(someIterators[i], &stackBuf[1], &bufferSize, &status);
            if (U_FAILURE(status) || brk == NULL) {
                log_err("FAIL: Cloned Iterator failed with misaligned buffer pointer\n");
            }
            if (status == U_SAFECLONE_ALLOCATED_WARNING) {
                log_verbose("Cloned Iterator allocated when using a mis-aligned buffer.\n");
            }
            if (brk)
                ubrk_close(brk);
        }


        /* Null Iterator - return NULL & set U_ILLEGAL_ARGUMENT_ERROR */
        if (NULL != ubrk_safeClone(NULL, buffer[i], &bufferSize, &status) || status != U_ILLEGAL_ARGUMENT_ERROR)
        {
            log_err("FAIL: Cloned Iterator failed to deal correctly with null Iterator pointer\n");
        }
        status = U_ZERO_ERROR;

        /* Do these cloned Iterators work at all - make a first & next call */
        bufferSize = U_BRK_SAFECLONE_BUFFERSIZE;
        someClonedIterators[i] = ubrk_safeClone(someIterators[i], buffer[i], &bufferSize, &status);

        start = ubrk_first(someClonedIterators[i]);
        if(start!=0)
            log_err("error ubrk_start(clone) did not return 0\n");
        pos=ubrk_next(someClonedIterators[i]);
        if(pos!=4)
            log_err("error ubrk_next(clone) did not return 4\n");

        ubrk_close(someClonedIterators[i]);
        ubrk_close(someIterators[i]);
    }
}
#endif


/*
//  Open a break iterator from char * rules.  Take care of conversion
//     of the rules and error checking.
*/
static UBreakIterator * testOpenRules(char *rules) {
    UErrorCode      status       = U_ZERO_ERROR;
    UChar          *ruleSourceU  = NULL;
    void           *strCleanUp   = NULL;
    UParseError     parseErr;
    UBreakIterator *bi;

    ruleSourceU = toUChar(rules, &strCleanUp);

    bi = ubrk_openRules(ruleSourceU,  -1,     /*  The rules  */
                        NULL,  -1,            /*  The text to be iterated over. */
                        &parseErr, &status);

    if (U_FAILURE(status)) {
        log_data_err("FAIL: ubrk_openRules: ICU Error \"%s\" (Are you missing data?)\n", u_errorName(status));
        bi = 0;
    };
    freeToUCharStrings(&strCleanUp);
    return bi;

}

/*
 *  TestBreakIteratorRules - Verify that a break iterator can be created from
 *                           a set of source rules.
 */
static void TestBreakIteratorRules() {
    /*  Rules will keep together any run of letters not including 'a', OR
     *             keep together 'abc', but only when followed by 'def', OTHERWISE
     *             just return one char at a time.
     */
    char         rules[]  = "abc/def{666};\n   [\\p{L} - [a]]* {2};  . {1};";
    /*                        0123456789012345678 */
    char         data[]   =  "abcdex abcdefgh-def";     /* the test data string                     */
    char         breaks[] =  "**    **  *    **  *";    /*  * the expected break positions          */
    char         tags[]   =  "01    21  6    21  2";    /*  expected tag values at break positions  */
    int32_t      tagMap[] = {0, 1, 2, 3, 4, 5, 666};

    UChar       *uData;
    void        *freeHook = NULL;
    UErrorCode   status   = U_ZERO_ERROR;
    int32_t      pos;
    int          i;

    UBreakIterator *bi = testOpenRules(rules);
    if (bi == NULL) {return;}
    uData = toUChar(data, &freeHook);
    ubrk_setText(bi,  uData, -1, &status);

    pos = ubrk_first(bi);
    for (i=0; i<sizeof(breaks); i++) {
        if (pos == i && breaks[i] != '*') {
            log_err("FAIL: unexpected break at position %d found\n", pos);
            break;
        }
        if (pos != i && breaks[i] == '*') {
            log_err("FAIL: expected break at position %d not found.\n", i);
            break;
        }
        if (pos == i) {
            int32_t tag, expectedTag;
            tag = ubrk_getRuleStatus(bi);
            expectedTag = tagMap[tags[i]&0xf];
            if (tag != expectedTag) {
                log_err("FAIL: incorrect tag value.  Position = %d;  expected tag %d, got %d",
                    pos, expectedTag, tag);
                break;
            }
            pos = ubrk_next(bi);
        }
    }

    /* #12914 add basic sanity test for ubrk_getBinaryRules, ubrk_openBinaryRules */
    /* Underlying functionality checked in C++ rbbiapts.cpp TestRoundtripRules */
    status = U_ZERO_ERROR;
    int32_t rulesLength = ubrk_getBinaryRules(bi, NULL, 0, &status); /* preflight */
    if (U_FAILURE(status)) {
        log_err("FAIL: ubrk_getBinaryRules preflight err: %s", u_errorName(status));
    } else {
        uint8_t* binaryRules = (uint8_t*)uprv_malloc(rulesLength);
        if (binaryRules == NULL) {
            log_err("FAIL: unable to malloc rules buffer, size %u", rulesLength);
        } else {
            rulesLength = ubrk_getBinaryRules(bi, binaryRules, rulesLength, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: ubrk_getBinaryRules err: %s", u_errorName(status));
            } else {
                UBreakIterator* bi2 = ubrk_openBinaryRules(binaryRules, rulesLength, uData, -1, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: ubrk_openBinaryRules err: %s", u_errorName(status));
                } else {
                    int32_t maxCount = sizeof(breaks); /* fail-safe test limit */
                    int32_t pos2 = ubrk_first(bi2);
                    pos = ubrk_first(bi);
                    do {
                        if (pos2 != pos) {
                            log_err("FAIL: interator from ubrk_openBinaryRules does not match original, get pos = %d instead of %d", pos2, pos);
                        }
                        pos2 = ubrk_next(bi2);
                        pos = ubrk_next(bi);
                    } while ((pos != UBRK_DONE || pos2 != UBRK_DONE) && maxCount-- > 0);
                    
                    ubrk_close(bi2);
                }
            }
            uprv_free(binaryRules);
        }
    }

    freeToUCharStrings(&freeHook);
    ubrk_close(bi);
}

static void TestBreakIteratorRuleError() {
/*
 *  TestBreakIteratorRuleError -   Try to create a BI from rules with syntax errors,
 *                                 check that the error is reported correctly.
 */
    char            rules[]  = "           #  This is a rule comment on line 1\n"
                               "[:L:];     # this rule is OK.\n"
                               "abcdefg);  # Error, mismatched parens\n";
    UChar          *uRules;
    void           *freeHook = NULL;
    UErrorCode      status   = U_ZERO_ERROR;
    UParseError     parseErr;
    UBreakIterator *bi;

    uRules = toUChar(rules, &freeHook);
    bi = ubrk_openRules(uRules,  -1,          /*  The rules  */
                        NULL,  -1,            /*  The text to be iterated over. */
                        &parseErr, &status);
    if (U_SUCCESS(status)) {
        log_err("FAIL: construction of break iterator succeeded when it should have failed.\n");
        ubrk_close(bi);
    } else {
        if (parseErr.line != 3 || parseErr.offset != 8) {
            log_data_err("FAIL: incorrect error position reported. Got line %d, char %d, expected line 3, char 7 (Are you missing data?)\n",
                parseErr.line, parseErr.offset);
        }
    }
    freeToUCharStrings(&freeHook);
}


/*
*   TestsBreakIteratorStatusVals()   Test the ubrk_getRuleStatusVec() funciton
*/
static void TestBreakIteratorStatusVec() {
    #define RULE_STRING_LENGTH 200
    UChar          rules[RULE_STRING_LENGTH];

    #define TEST_STRING_LENGTH 25
    UChar           testString[TEST_STRING_LENGTH];
    UBreakIterator *bi        = NULL;
    int32_t         pos       = 0;
    int32_t         vals[10];
    int32_t         numVals;
    UErrorCode      status    = U_ZERO_ERROR;

    u_uastrncpy(rules,  "[A-N]{100}; \n"
                             "[a-w]{200}; \n"
                             "[\\p{L}]{300}; \n"
                             "[\\p{N}]{400}; \n"
                             "[0-5]{500}; \n"
                              "!.*;\n", RULE_STRING_LENGTH);
    u_uastrncpy(testString, "ABC", TEST_STRING_LENGTH);


    bi = ubrk_openRules(rules, -1, testString, -1, NULL, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(bi != NULL);

    /* The TEST_ASSERT above should change too... */
    if (bi != NULL) {
        pos = ubrk_next(bi);
        TEST_ASSERT(pos == 1);

        memset(vals, -1, sizeof(vals));
        numVals = ubrk_getRuleStatusVec(bi, vals, 10, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(numVals == 2);
        TEST_ASSERT(vals[0] == 100);
        TEST_ASSERT(vals[1] == 300);
        TEST_ASSERT(vals[2] == -1);

        numVals = ubrk_getRuleStatusVec(bi, vals, 0, &status);
        TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
        TEST_ASSERT(numVals == 2);
    }

    ubrk_close(bi);
}


/*
 *  static void TestBreakIteratorUText(void);
 *
 *         Test that ubrk_setUText() is present and works for a simple case.
 */
static void TestBreakIteratorUText(void) {
    const char *UTF8Str = "\x41\xc3\x85\x5A\x20\x41\x52\x69\x6E\x67";  /* c3 85 is utf-8 for A with a ring on top */
                      /*   0  1   2 34567890  */

    UErrorCode      status = U_ZERO_ERROR;
    UBreakIterator *bi     = NULL;
    int32_t         pos    = 0;


    UText *ut = utext_openUTF8(NULL, UTF8Str, -1, &status);
    TEST_ASSERT_SUCCESS(status);

    bi = ubrk_open(UBRK_WORD, "en_US", NULL, 0, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Failure at file %s, line %d, error = %s\n", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    ubrk_setUText(bi, ut, &status);
    if (U_FAILURE(status)) {
        log_err("Failure at file %s, line %d, error = %s\n", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    pos = ubrk_first(bi);
    TEST_ASSERT(pos == 0);

    pos = ubrk_next(bi);
    TEST_ASSERT(pos == 4);

    pos = ubrk_next(bi);
    TEST_ASSERT(pos == 5);

    pos = ubrk_next(bi);
    TEST_ASSERT(pos == 10);

    pos = ubrk_next(bi);
    TEST_ASSERT(pos == UBRK_DONE);
    ubrk_close(bi);
    utext_close(ut);
}

/*
 *  static void TestBreakIteratorTailoring(void);
 *
 *         Test break iterator tailorings from CLDR data.
 */

/* Thai/Lao grapheme break tailoring */
static const UChar thTest[] = { 0x0020, 0x0E40, 0x0E01, 0x0020,
                                0x0E01, 0x0E30, 0x0020, 0x0E01, 0x0E33, 0x0020, 0 };
/*in Unicode 6.1 en should behave just like th for this*/
/*static const int32_t thTestOffs_enFwd[] = {  1,      3,  4,      6,  7,      9, 10 };*/
static const int32_t thTestOffs_thFwd[] = {  1,  2,  3,  4,  5,  6,  7,      9, 10 };
/*static const int32_t thTestOffs_enRev[] = {  9,      7,  6,      4,  3,      1,  0 };*/
static const int32_t thTestOffs_thRev[] = {  9,      7,  6,  5,  4,  3,  2,  1,  0 };

/* Hebrew line break tailoring, for cldrbug 3028 */
static const UChar heTest[] = { 0x0020, 0x002D, 0x0031, 0x0032, 0x0020,
                                0x0061, 0x002D, 0x006B, 0x0020,
                                0x0061, 0x0300, 0x2010, 0x006B, 0x0020,
                                0x05DE, 0x05D4, 0x002D, 0x0069, 0x0020,
                                0x05D1, 0x05BC, 0x2010, 0x0047, 0x0020, 0 };
/*in Unicode 6.1 en should behave just like he for this*/
/*static const int32_t heTestOffs_enFwd[] = {  1,  5,  7,  9, 12, 14, 17, 19, 22, 24 };*/
static const int32_t heTestOffs_heFwd[] = {  1,  5,  7,  9, 12, 14,     19,     24 };
/*static const int32_t heTestOffs_enRev[] = { 22, 19, 17, 14, 12,  9,  7,  5,  1,  0 };*/
static const int32_t heTestOffs_heRev[] = {     19,     14, 12,  9,  7,  5,  1,  0 };

/* Finnish line break tailoring, for cldrbug 3029 */
static const UChar fiTest[] = { /* 00 */ 0x0020, 0x002D, 0x0031, 0x0032, 0x0020,
                                /* 05 */ 0x0061, 0x002D, 0x006B, 0x0020,
                                /* 09 */ 0x0061, 0x0300, 0x2010, 0x006B, 0x0020,
                                /* 14 */ 0x0061, 0x0020, 0x002D, 0x006B, 0x0020,
                                /* 19 */ 0x0061, 0x0300, 0x0020, 0x2010, 0x006B, 0x0020, 0 };
static const int32_t fiTestOffs_enFwd[] =  {  1,  5,  7,  9, 12, 14, 16, 17, 19, 22, 23, 25 };
static const int32_t fiTestOffs_fiFwd[] =  {  1,  5,  7,  9, 12, 14, 16,     19, 22,     25 };
static const int32_t fiTestOffs_enRev[] =  { 23, 22, 19, 17, 16, 14, 12,  9,  7,  5,  1,  0 };
static const int32_t fiTestOffs_fiRev[] =  {     22, 19,     16, 14, 12,  9,  7,  5,  1,  0 };

/* Khmer dictionary-based work break, for ICU ticket #8329 */
static const UChar kmTest[] = { /* 00 */ 0x179F, 0x17BC, 0x1798, 0x1785, 0x17C6, 0x178E, 0x17B6, 0x1799, 0x1796, 0x17C1,
                                /* 10 */ 0x179B, 0x1794, 0x1793, 0x17D2, 0x178F, 0x17B7, 0x1785, 0x178A, 0x17BE, 0x1798,
                                /* 20 */ 0x17D2, 0x1794, 0x17B8, 0x17A2, 0x1792, 0x17B7, 0x179F, 0x17D2, 0x178B, 0x17B6,
                                /* 30 */ 0x1793, 0x17A2, 0x179A, 0x1796, 0x17D2, 0x179A, 0x17C7, 0x1782, 0x17BB, 0x178E,
                                /* 40 */ 0x178A, 0x179B, 0x17CB, 0x1796, 0x17D2, 0x179A, 0x17C7, 0x17A2, 0x1784, 0x17D2,
                                /* 50 */ 0x1782, 0 };
static const int32_t kmTestOffs_kmFwd[] =  {  3, /*8,*/ 11, 17, 23, 31, /*33,*/  40,  43, 51 }; /* TODO: Investigate failure to break at offset 8 */
static const int32_t kmTestOffs_kmRev[] =  { 43,  40,   /*33,*/ 31, 23, 17, 11, /*8,*/ 3,  0 };


/* Korean keepAll vs Normal */
static const UChar koTest[] = { /* 00 */ 0xBAA8, 0xB4E0, 0x0020, 0xC778, 0xB958, 0x0020, 0xAD6C, 0xC131, 0xC6D0, 0xC758,
                                /* 10 */ 0x0020, 0xCC9C, 0xBD80, 0xC758, 0x0020, 0xC874, 0xC5C4, 0xC131, 0xACFC, 0x0020,
                                /* 20 */ 0xB3D9, 0xB4F1, 0xD558, 0xACE0, 0x0020, 0xC591, 0xB3C4, 0xD560, 0 };
static const int32_t koTestOffs_koKeepFwd[] =  {   3,  6, 11, 15, 20, 25, 28 };
static const int32_t koTestOffs_koKeepRev[] =  {  25, 20, 15, 11,  6,  3,  0 };
static const int32_t koTestOffs_koNormFwd[] =  {  1,  3,  4,  6,  7,  8,  9, 11, 12, 13, 15, 16, 17, 18, 20, 21, 22, 23, 25, 26, 27, 28 };
static const int32_t koTestOffs_koNormRev[] =  { 27, 26, 25, 23, 22, 21, 20, 18, 17, 16, 15, 13, 12, 11,  9,  8,  7,  6,  4,  3,  1,  0 };

typedef struct {
    const char * locale;
    UBreakIteratorType type;
    const UChar * test;
    const int32_t * offsFwd;
    const int32_t * offsRev;
    int32_t numOffsets;
} RBBITailoringTest;

static const RBBITailoringTest tailoringTests[] = {
    { "en",            UBRK_CHARACTER, thTest, thTestOffs_thFwd, thTestOffs_thRev, UPRV_LENGTHOF(thTestOffs_thFwd) },
    { "en_US_POSIX",   UBRK_CHARACTER, thTest, thTestOffs_thFwd, thTestOffs_thRev, UPRV_LENGTHOF(thTestOffs_thFwd) },
    { "en",            UBRK_LINE,      heTest, heTestOffs_heFwd, heTestOffs_heRev, UPRV_LENGTHOF(heTestOffs_heFwd) },
    { "he",            UBRK_LINE,      heTest, heTestOffs_heFwd, heTestOffs_heRev, UPRV_LENGTHOF(heTestOffs_heFwd) },
    { "en",            UBRK_LINE,      fiTest, fiTestOffs_enFwd, fiTestOffs_enRev, UPRV_LENGTHOF(fiTestOffs_enFwd) },
    { "fi",            UBRK_LINE,      fiTest, fiTestOffs_fiFwd, fiTestOffs_fiRev, UPRV_LENGTHOF(fiTestOffs_fiFwd) },
    { "km",            UBRK_WORD,      kmTest, kmTestOffs_kmFwd, kmTestOffs_kmRev, UPRV_LENGTHOF(kmTestOffs_kmFwd) },
    { "ko",            UBRK_LINE,      koTest, koTestOffs_koNormFwd, koTestOffs_koNormRev, UPRV_LENGTHOF(koTestOffs_koNormFwd) },
    { "ko@lw=keepall", UBRK_LINE,      koTest, koTestOffs_koKeepFwd, koTestOffs_koKeepRev, UPRV_LENGTHOF(koTestOffs_koKeepFwd) },
    { "ko@lw=normal",  UBRK_LINE,      koTest, koTestOffs_koNormFwd, koTestOffs_koNormRev, UPRV_LENGTHOF(koTestOffs_koNormFwd) },
    { NULL, 0, NULL, NULL, NULL, 0 },
};

static void TestBreakIteratorTailoring(void) {
    const RBBITailoringTest * testPtr;
    for (testPtr = tailoringTests; testPtr->locale != NULL; ++testPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UBreakIterator* ubrkiter = ubrk_open(testPtr->type, testPtr->locale, testPtr->test, -1, &status);
        if ( U_SUCCESS(status) ) {
            int32_t offset, offsindx;
            UBool foundError;

            foundError = FALSE;
            ubrk_first(ubrkiter);
            for (offsindx = 0; (offset = ubrk_next(ubrkiter)) != UBRK_DONE; ++offsindx) {
                if (!foundError && offsindx >= testPtr->numOffsets) {
                    log_err("FAIL: locale %s, break type %d, ubrk_next expected UBRK_DONE, got %d\n",
                            testPtr->locale, testPtr->type, offset);
                    foundError = TRUE;
                } else if (!foundError && offset != testPtr->offsFwd[offsindx]) {
                    log_err("FAIL: locale %s, break type %d, ubrk_next expected %d, got %d\n",
                            testPtr->locale, testPtr->type, testPtr->offsFwd[offsindx], offset);
                    foundError = TRUE;
                }
            }
            if (!foundError && offsindx < testPtr->numOffsets) {
                log_err("FAIL: locale %s, break type %d, ubrk_next expected %d, got UBRK_DONE\n",
                        testPtr->locale, testPtr->type, testPtr->offsFwd[offsindx]);
            }

            foundError = FALSE;
            ubrk_last(ubrkiter);
            for (offsindx = 0; (offset = ubrk_previous(ubrkiter)) != UBRK_DONE; ++offsindx) {
                if (!foundError && offsindx >= testPtr->numOffsets) {
                    log_err("FAIL: locale %s, break type %d, ubrk_previous expected UBRK_DONE, got %d\n",
                            testPtr->locale, testPtr->type, offset);
                    foundError = TRUE;
                } else if (!foundError && offset != testPtr->offsRev[offsindx]) {
                    log_err("FAIL: locale %s, break type %d, ubrk_previous expected %d, got %d\n",
                            testPtr->locale, testPtr->type, testPtr->offsRev[offsindx], offset);
                    foundError = TRUE;
                }
            }
            if (!foundError && offsindx < testPtr->numOffsets) {
                log_err("FAIL: locale %s, break type %d, ubrk_previous expected %d, got UBRK_DONE\n",
                        testPtr->locale, testPtr->type, testPtr->offsRev[offsindx]);
            }

            ubrk_close(ubrkiter);
        } else {
            log_err_status(status, "FAIL: locale %s, break type %d, ubrk_open status: %s\n", testPtr->locale, testPtr->type, u_errorName(status));
        }
    }
}


static void TestBreakIteratorRefresh(void) {
    /*
     *  RefreshInput changes out the input of a Break Iterator without
     *    changing anything else in the iterator's state.  Used with Java JNI,
     *    when Java moves the underlying string storage.   This test
     *    runs a ubrk_next() repeatedly, moving the text in the middle of the sequence.
     *    The right set of boundaries should still be found.
     */
    UChar testStr[]  = {0x20, 0x41, 0x20, 0x42, 0x20, 0x43, 0x20, 0x44, 0x0};  /* = " A B C D"  */
    UChar movedStr[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  0};
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *bi;
    UText ut1 = UTEXT_INITIALIZER;
    UText ut2 = UTEXT_INITIALIZER;

    bi = ubrk_open(UBRK_LINE, "en_US", NULL, 0, &status);
    TEST_ASSERT_SUCCESS(status);
    if (U_FAILURE(status)) {
        return;
    }

    utext_openUChars(&ut1, testStr, -1, &status);
    TEST_ASSERT_SUCCESS(status);
    ubrk_setUText(bi, &ut1, &status);
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        /* Line boundaries will occur before each letter in the original string */
        TEST_ASSERT(1 == ubrk_next(bi));
        TEST_ASSERT(3 == ubrk_next(bi));

        /* Move the string, kill the original string.  */
        u_strcpy(movedStr, testStr);
        u_memset(testStr, 0x20, u_strlen(testStr));
        utext_openUChars(&ut2, movedStr, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        ubrk_refreshUText(bi, &ut2, &status);
        TEST_ASSERT_SUCCESS(status);

        /* Find the following matches, now working in the moved string. */
        TEST_ASSERT(5 == ubrk_next(bi));
        TEST_ASSERT(7 == ubrk_next(bi));
        TEST_ASSERT(8 == ubrk_next(bi));
        TEST_ASSERT(UBRK_DONE == ubrk_next(bi));
        TEST_ASSERT_SUCCESS(status);

        utext_close(&ut1);
        utext_close(&ut2);
    }
    ubrk_close(bi);
}


static void TestBug11665(void) {
    // The problem was with the incorrect breaking of Japanese text beginning
    // with Katakana characters when no prior Japanese or Chinese text had been
    // encountered.
    //
    // Tested here in cintltst, rather than in intltest, because only cintltst
    // tests have the ability to reset ICU, which is needed to get the bug
    // to manifest itself.

    static UChar japaneseText[] = {0x30A2, 0x30EC, 0x30EB, 0x30AE, 0x30FC, 0x6027, 0x7D50, 0x819C, 0x708E};
    int32_t boundaries[10] = {0};
    UBreakIterator *bi = NULL;
    int32_t brk;
    int32_t brkIdx = 0;
    int32_t totalBreaks = 0;
    UErrorCode status = U_ZERO_ERROR;

    ctest_resetICU();
    bi = ubrk_open(UBRK_WORD, "en_US", japaneseText, UPRV_LENGTHOF(japaneseText), &status);
    TEST_ASSERT_SUCCESS(status);
    if (!bi) {
        return;
    }
    for (brk=ubrk_first(bi); brk != UBRK_DONE; brk=ubrk_next(bi)) {
        boundaries[brkIdx] = brk;
        if (++brkIdx >= UPRV_LENGTHOF(boundaries) - 1) {
            break;
        }
    }
    if (brkIdx <= 2 || brkIdx >= UPRV_LENGTHOF(boundaries)) {
        log_err("%s:%d too few or many breaks found.\n", __FILE__, __LINE__);
    } else {
        totalBreaks = brkIdx;
        brkIdx = 0;
        for (brk=ubrk_first(bi); brk != UBRK_DONE; brk=ubrk_next(bi)) {
            if (brk != boundaries[brkIdx]) {
                log_err("%s:%d Break #%d differs between first and second iteration.\n", __FILE__, __LINE__, brkIdx);
                break;
            }
            if (++brkIdx >= UPRV_LENGTHOF(boundaries) - 1) {
                log_err("%s:%d Too many breaks.\n", __FILE__, __LINE__);
                break;
            }
        }
        if (totalBreaks != brkIdx) {
            log_err("%s:%d Number of breaks differ between first and second iteration.\n", __FILE__, __LINE__);
        }
    }
    ubrk_close(bi);
}

/*
 * expOffset is the set of expected offsets, ending with '-1'.
 * "Expected expOffset -1" means "expected the end of the offsets"
 */

static const char testSentenceSuppressionsEn[]  = "Mr. Jones comes home. Dr. Smith Ph.D. is out. In the U.S.A. it is hot.";
static const int32_t testSentSuppFwdOffsetsEn[] = { 22, 46, 70, -1 };         /* With suppressions */
static const int32_t testSentFwdOffsetsEn[]     = {  4, 22, 26, 46, 70, -1 }; /* Without suppressions */
static const int32_t testSentSuppRevOffsetsEn[] = { 46, 22,  0, -1 };         /* With suppressions */
static const int32_t testSentRevOffsetsEn[]     = { 46, 26, 22,  4,  0, -1 }; /* Without suppressions */

static const char testSentenceSuppressionsDe[]  = "Wenn ich schon h\\u00F6re zu Guttenberg kommt evtl. zur\\u00FCck.";
//                                                "Wenn ich schon höre zu Guttenberg kommt evtl. zurück."
static const int32_t testSentSuppFwdOffsetsDe[] = { 53, -1 };       /* With suppressions */
static const int32_t testSentFwdOffsetsDe[]     = { 53, -1 };       /* Without suppressions; no break in evtl. zur due to casing */
static const int32_t testSentSuppRevOffsetsDe[] = {  0, -1 };       /* With suppressions */
static const int32_t testSentRevOffsetsDe[]     = {  0, -1 };       /* Without suppressions */

static const char testSentenceSuppressionsEs[]  = "Te esperamos todos los miercoles en Bravo 416, Col. El Pueblo a las 7 PM.";
static const int32_t testSentSuppFwdOffsetsEs[] = { 73, -1 };       /* With suppressions */
static const int32_t testSentFwdOffsetsEs[]     = { 52, 73, -1 };   /* Without suppressions */
static const int32_t testSentSuppRevOffsetsEs[] = {  0, -1 };       /* With suppressions */
static const int32_t testSentRevOffsetsEs[]     = { 52,  0, -1 };   /* Without suppressions */

static const char testSentenceSuppressionsE1[]  = "Add or detract. The world will little note.";
static const char testSentenceSuppressionsE1u[] = "ADD OR DETRACT. THE WORLD WILL LITTLE NOTE.";
static const int32_t testSentFwdOffsetsE1[]     = { 16, 43, -1 };   /* Suppressions and case should make no difference */
static const int32_t testSentRevOffsetsE1[]     = { 16,  0, -1 };   /* Suppressions and case should make no difference */

static const char testSentenceSuppressionsE2[]  = "Coming up, the sprints at NCAA. Are you watching?";
static const char testSentenceSuppressionsE2u[] = "COMING UP, THE SPRINTS AT NCAA. ARE YOU WATCHING?";
static const int32_t testSentFwdOffsetsE2[]     = { 32, 49, -1 };   /* Suppressions and case should make no difference */
static const int32_t testSentRevOffsetsE2[]     = { 32,  0, -1 };   /* Suppressions and case should make no difference */

static const char testSentenceSuppressionsFr[]  = "Tr\\u00E8s bonne prise de parole de M. Junod, municipal \\u00E0 la culture de Lausanne.";
//                                                "Très bonne prise de parole de M. Junod, municipal à la culture de Lausanne."
static const int32_t testSentFwdOffsetsFr[]     = { 33, 75, -1 };   /* Without suppressions */
static const int32_t testSentSuppFwdOffsetsFr[] = { 75, -1 };       /* With suppressions */
static const int32_t testSentRevOffsetsFr[]     = { 33,  0, -1 };   /* Without suppressions */
static const int32_t testSentSuppRevOffsetsFr[] = {  0, -1 };       /* With suppressions */

static const char testSentenceSuppressionsE3[]  = "G8 countries e.g. U.K., Japan. Sanctions i.e. restrictions. Test E. Xx G. Xx I. Xx.";
static const char testSentenceSuppressionsE3u[] = "G8 COUNTRIES E.G. U.K., JAPAN. SANCTIONS I.E. RESTRICTIONS. TEST E. XX G. XX I. XX.";
static const int32_t testSentSuppFwdOffsetsE3[] = { 31, 60, 83, -1 };                 /* With suppressions */
static const int32_t testSentSuppRevOffsetsE3[] = { 60, 31,  0, -1 };                 /* With suppressions */
static const int32_t testSentFwdOffsetsE3[]     = { 18, 31, 60, 68, 74, 80, 83, -1 }; /* Without suppressions */
static const int32_t testSentRevOffsetsE3[]     = { 80, 74, 68, 60, 31, 18,  0, -1 }; /* Without suppressions */
static const int32_t testSentFwdOffsetsE3u[]    = { 18, 31, 46, 60, 68, 74, 80, 83, -1 }; /* Without suppressions */
static const int32_t testSentRevOffsetsE3u[]    = { 80, 74, 68, 60, 46, 31, 18,  0, -1 }; /* Without suppressions */

enum { kTextULenMax = 128, kTextBLenMax = 192 };

typedef struct {
    const char * locale;
    const char * text;
    const int32_t * expFwdOffsets;
    const int32_t * expRevOffsets;
} TestBISuppressionsItem;

static const TestBISuppressionsItem testBISuppressionsItems[] = {
    { "en@ss=standard", testSentenceSuppressionsEn, testSentSuppFwdOffsetsEn, testSentSuppRevOffsetsEn },
    { "en",             testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     },
    { "en_CA",             testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     },
    { "en_CA@ss=standard", testSentenceSuppressionsEn, testSentSuppFwdOffsetsEn, testSentSuppRevOffsetsEn },
    { "fr@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     },
    { "af@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     }, /* no brkiter data => nosuppressions? */
    { "af_ZA@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     }, /* no brkiter data => nosuppressions? */
    { "zh@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     }, /* brkiter data, no suppressions data => no suppressions */
    { "zh_Hant@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn, testSentRevOffsetsEn    }, /* brkiter data, no suppressions data => no suppressions */
    { "fi@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     }, /* brkiter data, no suppressions data => no suppressions */
    { "ja@ss=standard", testSentenceSuppressionsEn, testSentFwdOffsetsEn,     testSentRevOffsetsEn     }, /* brkiter data, no suppressions data => no suppressions */
    { "de@ss=standard", testSentenceSuppressionsDe, testSentSuppFwdOffsetsDe, testSentSuppRevOffsetsDe },
    { "de",             testSentenceSuppressionsDe, testSentFwdOffsetsDe,     testSentRevOffsetsDe     },
    { "es@ss=standard", testSentenceSuppressionsEs, testSentSuppFwdOffsetsEs, testSentSuppRevOffsetsEs },
    { "es",             testSentenceSuppressionsEs, testSentFwdOffsetsEs,     testSentRevOffsetsEs     },
    { "en",             testSentenceSuppressionsE1,  testSentFwdOffsetsE1,    testSentRevOffsetsE1     },
    { "en@ss=standard", testSentenceSuppressionsE1,  testSentFwdOffsetsE1,    testSentRevOffsetsE1     },
    { "en",             testSentenceSuppressionsE1u, testSentFwdOffsetsE1,    testSentRevOffsetsE1     },
    { "en@ss=standard", testSentenceSuppressionsE1u, testSentFwdOffsetsE1,    testSentRevOffsetsE1     },
    { "en",             testSentenceSuppressionsE2,  testSentFwdOffsetsE2,    testSentRevOffsetsE2     },
    { "en@ss=standard", testSentenceSuppressionsE2,  testSentFwdOffsetsE2,    testSentRevOffsetsE2     },
    { "en",             testSentenceSuppressionsE2u, testSentFwdOffsetsE2,    testSentRevOffsetsE2     },
    { "en@ss=standard", testSentenceSuppressionsE2u, testSentFwdOffsetsE2,    testSentRevOffsetsE2     },
    { "fr",             testSentenceSuppressionsFr, testSentFwdOffsetsFr,     testSentRevOffsetsFr     },
    { "fr@ss=standard", testSentenceSuppressionsFr, testSentSuppFwdOffsetsFr, testSentSuppRevOffsetsFr },
    { "en@ss=standard", testSentenceSuppressionsE3, testSentSuppFwdOffsetsE3, testSentSuppRevOffsetsE3 },
    { "en",             testSentenceSuppressionsE3, testSentFwdOffsetsE3,     testSentRevOffsetsE3     },
    { "en@ss=standard", testSentenceSuppressionsE3u, testSentSuppFwdOffsetsE3, testSentSuppRevOffsetsE3 },
    { "en",             testSentenceSuppressionsE3u, testSentFwdOffsetsE3u,    testSentRevOffsetsE3u    },
    { NULL, NULL, NULL }
};

static void TestBreakIteratorSuppressions(void) {
    const TestBISuppressionsItem * itemPtr;

    for (itemPtr = testBISuppressionsItems; itemPtr->locale != NULL; itemPtr++) {
        UChar textU[kTextULenMax + 1];
        char  textB[kTextBLenMax];
        int32_t textULen = u_unescape(itemPtr->text, textU, kTextULenMax);
        textU[kTextULenMax] = 0; // ensure zero termination
        UErrorCode status = U_ZERO_ERROR;
        UBreakIterator *bi = ubrk_open(UBRK_SENTENCE, itemPtr->locale, textU, textULen, &status);
        log_verbose("#%d: %s\n", (itemPtr-testBISuppressionsItems), itemPtr->locale);
        if (U_SUCCESS(status)) {
            int32_t offset, start;
            const int32_t * expOffsetPtr;
            const int32_t * expOffsetStart;
            u_austrcpy(textB, textU);

            expOffsetStart = expOffsetPtr = itemPtr->expFwdOffsets;
            ubrk_first(bi);
            for (; (offset = ubrk_next(bi)) != UBRK_DONE && *expOffsetPtr >= 0; expOffsetPtr++) {
                if (offset != *expOffsetPtr) {
                    log_err("FAIL: ubrk_next loc \"%s\", expected %d, got %d, text \"%s\"\n",
                            itemPtr->locale, *expOffsetPtr, offset, textB);
                }
            }
            if (offset != UBRK_DONE || *expOffsetPtr >= 0) {
                log_err("FAIL: ubrk_next loc \"%s\", expected UBRK_DONE & expOffset -1, got %d and %d, text \"%s\"\n",
                        itemPtr->locale, offset, *expOffsetPtr, textB);
            }

            expOffsetStart = expOffsetPtr = itemPtr->expFwdOffsets;
            start = ubrk_first(bi) + 1;
            for (; (offset = ubrk_following(bi, start)) != UBRK_DONE && *expOffsetPtr >= 0; expOffsetPtr++) {
                if (offset != *expOffsetPtr) {
                    log_err("FAIL: ubrk_following(%d) loc \"%s\", expected %d, got %d, text \"%s\"\n",
                            start, itemPtr->locale, *expOffsetPtr, offset, textB);
                }
                start = *expOffsetPtr + 1;
            }
            if (offset != UBRK_DONE || *expOffsetPtr >= 0) {
                log_err("FAIL: ubrk_following(%d) loc \"%s\", expected UBRK_DONE & expOffset -1, got %d and %d, text \"%s\"\n",
                        start, itemPtr->locale, offset, *expOffsetPtr, textB);
            }

            expOffsetStart = expOffsetPtr = itemPtr->expRevOffsets;
            offset = ubrk_last(bi);
            log_verbose("___ @%d ubrk_last\n", offset);
            if(offset == 0) {
              log_err("FAIL: ubrk_last loc \"%s\" unexpected %d\n", itemPtr->locale, offset);
            }
            for (; (offset = ubrk_previous(bi)) != UBRK_DONE && *expOffsetPtr >= 0; expOffsetPtr++) {
                if (offset != *expOffsetPtr) {
                    log_err("FAIL: ubrk_previous loc \"%s\", expected %d, got %d, text \"%s\"\n",
                            itemPtr->locale, *expOffsetPtr, offset, textB);
                } else {
                    log_verbose("[%d] @%d ubrk_previous()\n", (expOffsetPtr - expOffsetStart), offset);
                }
            }
            if (offset != UBRK_DONE || *expOffsetPtr >= 0) {
                log_err("FAIL: ubrk_previous loc \"%s\", expected UBRK_DONE & expOffset[%d] -1, got %d and %d, text \"%s\"\n",
                        itemPtr->locale, expOffsetPtr - expOffsetStart, offset, *expOffsetPtr, textB);
            }

            expOffsetStart = expOffsetPtr = itemPtr->expRevOffsets;
            start = ubrk_last(bi) - 1;
            for (; (offset = ubrk_preceding(bi, start)) != UBRK_DONE && *expOffsetPtr >= 0; expOffsetPtr++) {
                if (offset != *expOffsetPtr) {
                    log_err("FAIL: ubrk_preceding(%d) loc \"%s\", expected %d, got %d, text \"%s\"\n",
                            start, itemPtr->locale, *expOffsetPtr, offset, textB);
                }
                start = *expOffsetPtr - 1;
            }
            if (start >=0 && (offset != UBRK_DONE || *expOffsetPtr >= 0)) {
                log_err("FAIL: ubrk_preceding loc(%d) \"%s\", expected UBRK_DONE & expOffset -1, got %d and %d, text \"%s\"\n",
                        start, itemPtr->locale, offset, *expOffsetPtr, textB);
            }

            ubrk_close(bi);
        } else {
            log_data_err("FAIL: ubrk_open(UBRK_SENTENCE, \"%s\", ...) status %s (Are you missing data?)\n",
                    itemPtr->locale, u_errorName(status));
        }
    }
}

#if APPLE_ADDITIONS
#include <stdio.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include "unicode/urbtok.h"
#include "cstring.h"

static const char testRulesFilePath[] = "../testdata/word_urbTokTest.txt";
static const UChar textToTokenize[] = {
/*
"Short phrase! Another (with parens); done.\n
At 4:00, tea-time.\n
He wouldn't've wanted y'all to ... come at 3:30pm for $3 coffee @funman :)\n
x3:30 -- x1.0"
*/
    0x53,0x68,0x6F,0x72,0x74,0x20,0x70,0x68,0x72,0x61,0x73,0x65,0x21,0x20,
    0x41,0x6E,0x6F,0x74,0x68,0x65,0x72,0x20,0x28,0x77,0x69,0x74,0x68,0x20,0x70,0x61,0x72,0x65,0x6E,0x73,0x29,0x3B,0x20,0x64,0x6F,0x6E,0x65,0x2E,0x0A,
    0x41,0x74,0x20,0x34,0x3A,0x30,0x30,0x2C,0x20,0x74,0x65,0x61,0x2D,0x74,0x69,0x6D,0x65,0x2E,0x0A,
    0x48,0x65,0x20,0x77,0x6F,0x75,0x6C,0x64,0x6E,0x27,0x74,0x27,0x76,0x65,0x20,0x77,0x61,0x6E,0x74,0x65,0x64,0x20,
    0x79,0x27,0x61,0x6C,0x6C,0x20,0x74,0x6F,0x20,0x2E,0x2E,0x2E,0x20, 0x63,0x6F,0x6D,0x65,0x20,0x61,0x74,0x20,
    0x33,0x3A,0x33,0x30,0x70,0x6D,0x20,0x66,0x6F,0x72,0x20,0x24,0x33,0x20,0x63,0x6F,0x66,0x66,0x65,0x65,0x20,
    0x40,0x66,0x75,0x6E,0x6D,0x61,0x6E,0x20,0x3A,0x29,0x0A,
    0x78,0x33,0x3A,0x33,0x30,0x20,0x2D,0x2D,0x20,0x78,0x31,0x2E,0x30,0
};
typedef struct {
    RuleBasedTokenRange token;
    unsigned long       flags;
} RBTokResult;
static const RBTokResult expectedResults[] = { // 66 tokens
    { {  0, 5 }, 0xC8 },  // Short
    { {  5, 1 }, 0x01 },  // _sp_
    { {  6, 6 }, 0xC8 },  // phrase
    { { 12, 1 }, 0x00 },  // !
    { { 13, 1 }, 0x01 },  // _sp_
    { { 14, 7 }, 0xC8 },  // Another
    { { 21, 1 }, 0x01 },  // _sp_
    { { 22, 1 }, 0x00 },  // (
    { { 23, 4 }, 0xC8 },  // with
    { { 27, 1 }, 0x01 },  // _sp_
    { { 28, 6 }, 0xC8 },  // parens
    { { 34, 1 }, 0x00 },  // )
    { { 35, 1 }, 0x00 },  // ;
    { { 36, 1 }, 0x01 },  // _sp_
    { { 37, 4 }, 0xC8 },  // done
    { { 41, 1 }, 0x14 },  // .
    { { 42, 1 }, 0x00 },  // _nl_

    { { 43, 2 }, 0xC8 },  // At
    { { 45, 1 }, 0x01 },  // _sp_
    { { 46, 4 }, 0x76 },  // 4:00       ** here RBBI has x64
    { { 50, 1 }, 0x00 },  // ,
    { { 51, 1 }, 0x01 },  // _sp_
    { { 52, 3 }, 0xC8 },  // tea
    { { 55, 1 }, 0x15 },  // -
    { { 56, 4 }, 0xC8 },  // time
    { { 60, 1 }, 0x14 },  // .
    { { 61, 1 }, 0x00 },  // _nl_

    { { 62, 2 }, 0xC8 },  // He
    { { 64, 1 }, 0x01 },  // _sp_
    { { 65, 8 }, 0xCA },  // wouldn't
    { { 73, 1 }, 0x16 },  // '
    { { 74, 2 }, 0xC8 },  // ve
    { { 76, 1 }, 0x01 },  // _sp_
    { { 77, 6 }, 0xC8 },  // wanted
    { { 83, 1 }, 0x01 },  // _sp_
    { { 84, 5 }, 0xCA },  // y'all
    { { 89, 1 }, 0x01 },  // _sp_
    { { 90, 2 }, 0xC8 },  // to
    { { 92, 1 }, 0x01 },  // _sp_
    { { 93, 3 }, 0x3C },  // ...        ** here RBBI has 0x28
    { { 96, 1 }, 0x01 },  // _sp_
    { { 97, 4 }, 0xC8 },  // come
    { { 101, 1 }, 0x01 },  // _sp_
    { { 102, 2 }, 0xC8 },  // at
    { { 104, 1 }, 0x01 },  // _sp_
    { { 105, 6 }, 0xC8 },  // 3:30pm
    { { 111, 1 }, 0x01 },  // _sp_
    { { 112, 3 }, 0xC8 },  // for
    { { 115, 1 }, 0x01 },  // _sp_
    { { 116, 1 }, 0x00 },  // $
    { { 117, 1 }, 0x64 },  // 3
    { { 118, 1 }, 0x01 },  // _sp_
    { { 119, 6 }, 0xC8 },  // coffee
    { { 125, 1 }, 0x01 },  // _sp_
    { { 126, 7 }, 0xDF },  // @funman   ** here RBBI has 0xC8
    { { 133, 1 }, 0x01 },  // _sp_
    { { 134, 2 }, 0x20 },  // :)
    { { 136, 1 }, 0x00 },  // _nl_

    // ** incorrect ranges (and flags) currently produced by RBTok
    { { 137, 2 }, 0xEC },  // x3
    { { 139, 1 }, 0x00 },  // :
    { { 140, 2 }, 0x64 },  // 30
    // ** for the above, RBBI has
    //{ { 137, 1 }, 0x64 },  // x
    //{ { 138, 4 }, 0x64 },  // 3:30
    //
    { { 142, 1 }, 0x01 },  // _sp_
    { { 143, 2 }, 0x3D },  // --        ** here RBBI has 0x28
    { { 145, 1 }, 0x01 },  //  _sp_
    { { 146, 2 }, 0xEC },  // x1        ** here RBBI has 0xC8
    { { 148, 1 }, 0x14 },  // .
    { { 149, 1 }, 0x64 },  // 0
};
enum {
    kNumTokensExpected = UPRV_LENGTHOF(expectedResults), // 66
    kMaxTokens = 96
};

static void TestRuleBasedTokenizer(void) {
    FILE * testRulesFile;
    char * testRulesUTF8Buf;
    UChar* testRulesUTF16Buf = NULL;
    long testRulesFileSize, testRulesFileRead = 0;
    long testRulesUTF8Offset = 0;
    int32_t testRulesUTF16Size;
    UErrorCode status = U_ZERO_ERROR;

    testRulesFile = fopen(testRulesFilePath, "r");
    if (testRulesFile == NULL) {
        log_data_err("FAIL: fopen fails for: %s\n", testRulesFilePath);
        return;
    }
    fseek(testRulesFile, 0, SEEK_END);
    testRulesFileSize = ftell(testRulesFile);
    rewind(testRulesFile);

    testRulesUTF8Buf = (char *)uprv_malloc(testRulesFileSize);
    if (testRulesUTF8Buf != NULL) {
        testRulesFileRead = fread(testRulesUTF8Buf, 1, testRulesFileSize, testRulesFile);
    }
    fclose(testRulesFile);
    if (testRulesUTF8Buf == NULL) {
        log_data_err("FAIL: uprv_malloc fails for testRulesUTF8Buf[%ld]\n", testRulesFileSize);
        return;
    }
    if (testRulesFileRead < testRulesFileSize) {
        log_data_err("FAIL: fread fails for %s, read %ld of %ld\n", testRulesFile, testRulesFileRead, testRulesFileSize);
        uprv_free(testRulesUTF8Buf);
        return;
    }
    /* done with file, UTF8 rules in testRulesUTF8Buf. Handle UTF8 BOM: */
    if (uprv_strncmp(testRulesUTF8Buf, "\xEF\xBB\xBF", 3) == 0) {
        testRulesUTF8Offset = 3;
        testRulesFileSize -= testRulesUTF8Offset;
    }

    u_strFromUTF8(NULL, 0, &testRulesUTF16Size, testRulesUTF8Buf+testRulesUTF8Offset, testRulesFileSize, &status); /* preflight */
    if (status == U_BUFFER_OVERFLOW_ERROR) { /* expected for preflight */
        status = U_ZERO_ERROR;
    }
    if (U_FAILURE(status)) {
        log_data_err("FAIL: u_strFromUTF8 preflight fails: %s\n", u_errorName(status));
    } else {
        testRulesUTF16Buf = (UChar *)uprv_malloc(testRulesUTF16Size*sizeof(UChar));
        if (testRulesUTF16Buf == NULL) {
            log_data_err("FAIL: uprv_malloc fails for testRulesUTF16Buf[%ld]\n", testRulesUTF16Size*sizeof(UChar));
        } else {
            u_strFromUTF8(testRulesUTF16Buf, testRulesUTF16Size, &testRulesUTF16Size, testRulesUTF8Buf+testRulesUTF8Offset, testRulesFileSize, &status);
        }
    }
    uprv_free(testRulesUTF8Buf);
    if (testRulesUTF16Buf == NULL) {
        return;
    }
    if (U_FAILURE(status)) {
        log_data_err("FAIL: u_strFromUTF8 fails: %s\n", u_errorName(status));
    } else {
	    mach_timebase_info_data_t info;
	    uint64_t start, duration;
        UParseError parseErr;
        UBreakIterator *brkFromRules;

        mach_timebase_info(&info);

        start = mach_absolute_time();
        brkFromRules = urbtok_openRules(testRulesUTF16Buf, testRulesUTF16Size, &parseErr, &status);
        duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
        if (U_FAILURE(status)) {
            log_err("FAIL: urbtok_openRules status: %s, line %d, col %d\n", u_errorName(status), parseErr.line, parseErr.offset);
        } else {
            uint8_t *rulesBinaryBuf;
            uint32_t rulesBinarySize;
            log_info("urbtok_openRules nsec %llu\n", duration);
            rulesBinarySize = urbtok_getBinaryRules(brkFromRules, NULL, 0, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: urbtok_getBinaryRules preflight status: %s, rulesBinarySize %u\n", u_errorName(status), rulesBinarySize);
            } else {
                rulesBinaryBuf = (uint8_t *)uprv_malloc(rulesBinarySize);
                if (rulesBinaryBuf == NULL) {
                    log_data_err("FAIL: uprv_malloc fails for rulesBinaryBuf[%ld]\n", rulesBinarySize);
                } else {
                    start = mach_absolute_time();
                    rulesBinarySize = urbtok_getBinaryRules(brkFromRules, rulesBinaryBuf, rulesBinarySize, &status);
                    duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
                    if (U_FAILURE(status)) {
                        log_err("FAIL: urbtok_getBinaryRules status: %s, rulesBinarySize %u\n", u_errorName(status), rulesBinarySize);
                    } else {
                        UBreakIterator *brkFromBinary;
                        log_info("urbtok_getBinaryRules nsec %llu\n", duration);
                        start = mach_absolute_time();
                        brkFromBinary = urbtok_openBinaryRules(rulesBinaryBuf, &status);
                        duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
                        if (U_FAILURE(status)) {
                            log_err("FAIL: urbtok_openBinaryRules status: %s\n", u_errorName(status));
                        } else {
                            RuleBasedTokenRange tokens[kMaxTokens];
                            unsigned long       flags[kMaxTokens];
                            int32_t iToken, numTokens = 0;

                            log_info("urbtok_openBinaryRules nsec %llu\n", duration);

                            status = U_ZERO_ERROR;
                            ubrk_setText(brkFromRules, textToTokenize, -1, &status);
                            if (U_FAILURE(status)) {
                                log_err("FAIL: ubrk_setText brkFromRules status: %s\n", u_errorName(status));
                            } else {
                                start = mach_absolute_time();
                                numTokens = urbtok_tokenize(brkFromRules, kMaxTokens, tokens, flags);
                                duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
                                UBool fail = (numTokens != kNumTokensExpected);
                                for (iToken = 0; !fail && iToken < numTokens; iToken++) {
                                    if (  tokens[iToken].location != expectedResults[iToken].token.location ||
                                          tokens[iToken].length   != expectedResults[iToken].token.length   ||
                                          flags[iToken]           != expectedResults[iToken].flags ) {
                                        fail = TRUE;
                                    }
                                }
                                if (fail) {
                                    log_err("FAIL: urbtok_tokenize bulk brkFromRules expected %d tokens, got %d\n", kNumTokensExpected, numTokens);
                                    printf("# expect          get\n");
                                    printf("# loc len flags   loc len flags\n");
                                    int32_t maxTokens = (numTokens >= kNumTokensExpected)? numTokens: kNumTokensExpected;
                                    for (iToken = 0; iToken < maxTokens; iToken++) {
                                        if (iToken < kNumTokensExpected) {
                                            printf("  %3ld %3ld 0x%03lX", expectedResults[iToken].token.location,
                                                expectedResults[iToken].token.length, expectedResults[iToken].flags);
                                        } else {
                                            printf("             ");
                                        }
                                        if (iToken < numTokens) {
                                            printf("   %3ld %3ld 0x%03lX\n",  tokens[iToken].location, tokens[iToken].length, flags[iToken] );
                                        } else {
                                            printf("\n");
                                        }
                                    }
                                } else {
                                    log_info("Latn urbtok_tokenize bulk brkFromRules nsec %llu\n", duration);
                                }
                            }

                            status = U_ZERO_ERROR;
                            ubrk_setText(brkFromBinary, textToTokenize, -1, &status);
                            if (U_FAILURE(status)) {
                                log_err("FAIL: ubrk_setText brkFromBinary status: %s\n", u_errorName(status));
                            } else {
                                start = mach_absolute_time();
                                numTokens = urbtok_tokenize(brkFromBinary, kMaxTokens, tokens, flags);
                                duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
                                UBool fail = (numTokens != kNumTokensExpected);
                                for (iToken = 0; !fail && iToken < numTokens; iToken++) {
                                    if (  tokens[iToken].location != expectedResults[iToken].token.location ||
                                          tokens[iToken].length   != expectedResults[iToken].token.length   ||
                                          flags[iToken]           != expectedResults[iToken].flags ) {
                                        fail = TRUE;
                                    }
                                }
                                if (fail) {
                                    log_err("FAIL: urbtok_tokenize bulk brkFromBinary expected %d tokens, got %d\n", kNumTokensExpected, numTokens);
                                    printf("# expect          get\n");
                                    printf("# loc len flags   loc len flags\n");
                                    int32_t maxTokens = (numTokens >= kNumTokensExpected)? numTokens: kNumTokensExpected;
                                    for (iToken = 0; iToken < maxTokens; iToken++) {
                                        if (iToken < kNumTokensExpected) {
                                            printf("  %3ld %3ld 0x%03lX", expectedResults[iToken].token.location,
                                                expectedResults[iToken].token.length, expectedResults[iToken].flags);
                                        } else {
                                            printf("             ");
                                        }
                                        if (iToken < numTokens) {
                                            printf("   %3ld %3ld 0x%03lX\n",  tokens[iToken].location, tokens[iToken].length, flags[iToken] );
                                        } else {
                                            printf("\n");
                                        }
                                    }
                                } else {
                                     log_info("Latn urbtok_tokenize bulk brkFromBinary nsec %llu\n", duration);
                               }
                            }

                            status = U_ZERO_ERROR;
                            ubrk_setText(brkFromBinary, textToTokenize, -1, &status);
                            if (U_FAILURE(status)) {
                                log_err("FAIL: ubrk_setText brkFromBinary status: %s\n", u_errorName(status));
                            } else {
                                RuleBasedTokenRange *tokenLimit = tokens + kMaxTokens;
                                RuleBasedTokenRange *tokenP;
                                unsigned long *flagsP;

                                start = mach_absolute_time();
						        for (tokenP = tokens, flagsP = flags; tokenP < tokenLimit && urbtok_tokenize(brkFromBinary, 1, tokenP, flagsP) == 1; tokenP++, flagsP++) {
						            ;
						        }
                                numTokens = tokenP - tokens;
                                duration = ((mach_absolute_time() - start) * info.numer)/info.denom;
                                UBool fail = (numTokens != kNumTokensExpected);
                                for (iToken = 0; !fail && iToken < numTokens; iToken++) {
                                    if (  tokens[iToken].location != expectedResults[iToken].token.location ||
                                          tokens[iToken].length   != expectedResults[iToken].token.length   ||
                                          flags[iToken]           != expectedResults[iToken].flags ) {
                                        fail = TRUE;
                                    }
                                }
                                if (fail) {
                                    log_err("FAIL: urbtok_tokenize loop brkFromBinary expected %d tokens, got %d\n", kNumTokensExpected, numTokens);
                                    printf("# expect          get\n");
                                    printf("# loc len flags   loc len flags\n");
                                    int32_t maxTokens = (numTokens >= kNumTokensExpected)? numTokens: kNumTokensExpected;
                                    for (iToken = 0; iToken < maxTokens; iToken++) {
                                        if (iToken < kNumTokensExpected) {
                                            printf("  %3ld %3ld 0x%03lX", expectedResults[iToken].token.location,
                                                expectedResults[iToken].token.length, expectedResults[iToken].flags);
                                        } else {
                                            printf("             ");
                                        }
                                        if (iToken < numTokens) {
                                            printf("   %3ld %3ld 0x%03lX\n",  tokens[iToken].location, tokens[iToken].length, flags[iToken] );
                                        } else {
                                            printf("\n");
                                        }
                                    }
                                } else {
                                     log_info("Latn urbtok_tokenize loop brkFromBinary nsec %llu\n", duration);
                               }
                            }

                            ubrk_close(brkFromBinary);
                        }
                    }
                    uprv_free(rulesBinaryBuf);
                }
            }
            ubrk_close(brkFromRules);
        }
    }
    uprv_free(testRulesUTF16Buf);
}
#endif


#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
