// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
***************************************************************************
*   Copyright (C) 1999-2016 International Business Machines Corporation
*   and others. All rights reserved.
***************************************************************************
*/
//
//  file:  rbbi.cpp  Contains the implementation of the rule based break iterator
//                   runtime engine and the API implementation for
//                   class RuleBasedBreakIterator
//

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include <cinttypes>

#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "unicode/uchriter.h"
#include "unicode/uclean.h"
#include "unicode/udata.h"
#if APPLE_ICU_CHANGES
// rdar://51193810 for line break, remap locale delimiters that are QU to OP/CL as appropriate
#include "unicode/ulocdata.h"
#endif // APPLE_ICU_CHANGES

#include "brkeng.h"
#include "ucln_cmn.h"
#include "cmemory.h"
#include "cstring.h"
#include "localsvc.h"
#include "rbbidata.h"
#include "rbbi_cache.h"
#include "rbbirb.h"
#include "uassert.h"
#include "umutex.h"
#include "uvectr32.h"

#ifdef RBBI_DEBUG
static UBool gTrace = false;
#endif

U_NAMESPACE_BEGIN

// The state number of the starting state
constexpr int32_t START_STATE = 1;

// The state-transition value indicating "stop"
constexpr int32_t STOP_STATE = 0;


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedBreakIterator)


//=======================================================================
// constructors
//=======================================================================

/**
 * Constructs a RuleBasedBreakIterator that uses the already-created
 * tables object that is passed in as a parameter.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status)
 : RuleBasedBreakIterator(&status)
{
    fData = new RBBIDataWrapper(data, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if (fData->fForwardTable->fLookAheadResultsSize > 0) {
        fLookAheadMatches = static_cast<int32_t *>(
            uprv_malloc(fData->fForwardTable->fLookAheadResultsSize * sizeof(int32_t)));
        if (fLookAheadMatches == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
}

//-------------------------------------------------------------------------------
//
//   Constructor   from a UDataMemory handle to precompiled break rules
//                 stored in an ICU data file. This construcotr is private API,
//                 only for internal use.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(UDataMemory* udm, UBool isPhraseBreaking,
        UErrorCode &status) : RuleBasedBreakIterator(udm, status)
{
    fIsPhraseBreaking = isPhraseBreaking;
}

//
//  Construct from precompiled binary rules (tables).  This constructor is public API,
//  taking the rules as a (const uint8_t *) to match the type produced by getBinaryRules().
//
RuleBasedBreakIterator::RuleBasedBreakIterator(const uint8_t *compiledRules,
                       uint32_t       ruleLength,
                       UErrorCode     &status)
 : RuleBasedBreakIterator(&status)
{
    if (U_FAILURE(status)) {
        return;
    }
    if (compiledRules == nullptr || ruleLength < sizeof(RBBIDataHeader)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const RBBIDataHeader *data = (const RBBIDataHeader *)compiledRules;
    if (data->fLength > ruleLength) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    fData = new RBBIDataWrapper(data, RBBIDataWrapper::kDontAdopt, status);
    if (U_FAILURE(status)) {return;}
    if(fData == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if (fData->fForwardTable->fLookAheadResultsSize > 0) {
        fLookAheadMatches = static_cast<int32_t *>(
            uprv_malloc(fData->fForwardTable->fLookAheadResultsSize * sizeof(int32_t)));
        if (fLookAheadMatches == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
}


//-------------------------------------------------------------------------------
//
//   Constructor   from a UDataMemory handle to precompiled break rules
//                 stored in an ICU data file.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(UDataMemory* udm, UErrorCode &status)
 : RuleBasedBreakIterator(&status)
{
    fData = new RBBIDataWrapper(udm, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if (fData->fForwardTable->fLookAheadResultsSize > 0) {
        fLookAheadMatches = static_cast<int32_t *>(
            uprv_malloc(fData->fForwardTable->fLookAheadResultsSize * sizeof(int32_t)));
        if (fLookAheadMatches == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
}



//-------------------------------------------------------------------------------
//
//   Constructor       from a set of rules supplied as a string.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator( const UnicodeString  &rules,
                                                UParseError          &parseError,
                                                UErrorCode           &status)
 : RuleBasedBreakIterator(&status)
{
    if (U_FAILURE(status)) {return;}
    RuleBasedBreakIterator *bi = (RuleBasedBreakIterator *)
        RBBIRuleBuilder::createRuleBasedBreakIterator(rules, &parseError, status);
    // Note:  This is a bit awkward.  The RBBI ruleBuilder has a factory method that
    //        creates and returns a complete RBBI.  From here, in a constructor, we
    //        can't just return the object created by the builder factory, hence
    //        the assignment of the factory created object to "this".
    if (U_SUCCESS(status)) {
        *this = *bi;
        delete bi;
    }
}


//-------------------------------------------------------------------------------
//
// Default Constructor.      Create an empty shell that can be set up later.
//                           Used when creating a RuleBasedBreakIterator from a set
//                           of rules.
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator()
 : RuleBasedBreakIterator(nullptr)
{
}

/**
 * Simple Constructor with an error code.
 * Handles common initialization for all other constructors.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(UErrorCode *status) {
    UErrorCode ec = U_ZERO_ERROR;
    if (status == nullptr) {
        status = &ec;
    }
    utext_openUChars(&fText, nullptr, 0, status);
    LocalPointer<DictionaryCache> lpDictionaryCache(new DictionaryCache(this, *status), *status);
    LocalPointer<BreakCache> lpBreakCache(new BreakCache(this, *status), *status);
    if (U_FAILURE(*status)) {
        fErrorCode = *status;
        return;
    }
    fDictionaryCache = lpDictionaryCache.orphan();
    fBreakCache = lpBreakCache.orphan();

#ifdef RBBI_DEBUG
    static UBool debugInitDone = false;
    if (debugInitDone == false) {
        char *debugEnv = getenv("U_RBBIDEBUG");
        if (debugEnv && uprv_strstr(debugEnv, "trace")) {
            gTrace = true;
        }
        debugInitDone = true;
    }
#endif

#if APPLE_ICU_CHANGES
// rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
// (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
    fLatin1Cat            = nullptr;
    // rdar://51193810 for line break, remap locale delimiters that are QU to OP/CL as appropriate
    fCatOverrides         = nullptr;
    fCatOverrideCount     = 0;
#endif // APPLE_ICU_CHANGES
}


//-------------------------------------------------------------------------------
//
//   Copy constructor.  Will produce a break iterator with the same behavior,
//                      and which iterates over the same text, as the one passed in.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(const RuleBasedBreakIterator& other)
: RuleBasedBreakIterator()
{
    *this = other;
}


/**
 * Destructor
 */
RuleBasedBreakIterator::~RuleBasedBreakIterator() {
    if (fCharIter != &fSCharIter) {
        // fCharIter was adopted from the outside.
        delete fCharIter;
    }
    fCharIter = nullptr;

    utext_close(&fText);

    if (fData != nullptr) {
        fData->removeReference();
        fData = nullptr;
    }
    delete fBreakCache;
    fBreakCache = nullptr;

    delete fDictionaryCache;
    fDictionaryCache = nullptr;

    delete fLanguageBreakEngines;
    fLanguageBreakEngines = nullptr;

    delete fUnhandledBreakEngine;
    fUnhandledBreakEngine = nullptr;

    uprv_free(fLookAheadMatches);
    fLookAheadMatches = nullptr;

#if APPLE_ICU_CHANGES
// rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
// (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
    delete [] fLatin1Cat;
    fLatin1Cat = nullptr;

    // rdar://51193810 for line break, remap locale delimiters that are QU to OP/CL as appropriate
    delete [] fCatOverrides;
    fCatOverrides = nullptr;
    fCatOverrideCount = 0;
#endif // APPLE_ICU_CHANGES
}

/**
 * Assignment operator.  Sets this iterator to have the same behavior,
 * and iterate over the same text, as the one passed in.
 * TODO: needs better handling of memory allocation errors.
 */
RuleBasedBreakIterator&
RuleBasedBreakIterator::operator=(const RuleBasedBreakIterator& that) {
    if (this == &that) {
        return *this;
    }
    BreakIterator::operator=(that);
#if APPLE_ICU_CHANGES
// rdar://36667210 Add ubrk_setLineWordOpts to programmatically set @lw options, add lw=keep-hangul support via keyword or function
    fLineWordOpts = that.fLineWordOpts;
#endif // APPLE_ICU_CHANGES

    if (fLanguageBreakEngines != nullptr) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = nullptr;   // Just rebuild for now
    }
    // TODO: clone fLanguageBreakEngines from "that"
    UErrorCode status = U_ZERO_ERROR;
    utext_clone(&fText, &that.fText, false, true, &status);

    if (fCharIter != &fSCharIter) {
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

    if (that.fCharIter != nullptr && that.fCharIter != &that.fSCharIter) {
        // This is a little bit tricky - it will initially appear that
        //  this->fCharIter is adopted, even if that->fCharIter was
        //  not adopted.  That's ok.
        fCharIter = that.fCharIter->clone();
    }
    fSCharIter = that.fSCharIter;
    if (fCharIter == nullptr) {
        fCharIter = &fSCharIter;
    }

    if (fData != nullptr) {
        fData->removeReference();
        fData = nullptr;
    }
    if (that.fData != nullptr) {
        fData = that.fData->addReference();
    }

    uprv_free(fLookAheadMatches);
    fLookAheadMatches = nullptr;
    if (fData && fData->fForwardTable->fLookAheadResultsSize > 0) {
        fLookAheadMatches = static_cast<int32_t *>(
            uprv_malloc(fData->fForwardTable->fLookAheadResultsSize * sizeof(int32_t)));
    }

#if APPLE_ICU_CHANGES
// rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
// (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
    delete [] fLatin1Cat;
    fLatin1Cat = NULL;

    // rdar://51193810 for line break, remap locale delimiters that are QU to OP/CL as appropriate
    delete [] fCatOverrides;
    fCatOverrides = NULL;
    fCatOverrideCount = that.fCatOverrideCount;
    if (fCatOverrideCount != 0) {
        fCatOverrides = new CategoryOverride[fCatOverrideCount];
        for (int32_t orItem = 0; orItem < fCatOverrideCount; ++orItem) {
            fCatOverrides[orItem] = that.fCatOverrides[orItem];
        }
    }
#endif // APPLE_ICU_CHANGES

    fPosition = that.fPosition;
    fRuleStatusIndex = that.fRuleStatusIndex;
    fDone = that.fDone;

    // TODO: both the dictionary and the main cache need to be copied.
    //       Current position could be within a dictionary range. Trying to continue
    //       the iteration without the caches present would go to the rules, with
    //       the assumption that the current position is on a rule boundary.
    fBreakCache->reset(fPosition, fRuleStatusIndex);
    fDictionaryCache->reset();

    return *this;
}

#if APPLE_ICU_CHANGES
// rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
// (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
// rdar://70367682 #163,dab0491810.. Integrate open-source ICU 68.2 into Apple ICU sources
void RuleBasedBreakIterator::initLatin1Cat(void) {
    fLatin1Cat = new uint16_t[256];
    for (UChar32 c = 0; c < 256; ++c) {
        fLatin1Cat[c] = ucptrie_get(fData->fTrie, c);
    }
}

// rdar://51193810 for line break, remap locale delimiters that are QU to OP/CL as appropriate
enum {
    kUDelimBuf = 3,   // maximum UTF16 length of delimiter to get (1 for all delimiters in ICU 66)
    kUDelimCount = 4, // maximum number of category overrides for delimiters
    kPrototypeForOP = 0x007B, // prototype character for linebreak class OP (in Unicode 13)
    kPrototypeForCL = 0x007D, // prototype character for linebreak class CL (in Unicode 13)
    // rdar://63475386 U+2019 apostrophe should remain linebreak class QU
    kTrueApostrophe = 0x2019, // U+2019 true apostrophe, glottal stop
};
void RuleBasedBreakIterator::setCategoryOverrides(Locale locale) {
    delete [] fCatOverrides;
    fCatOverrides = NULL;
    fCatOverrideCount = 0;

    // rdar://66836891 da, skip remapping line break class for delimiters
    if (uprv_strcmp(locale.getLanguage(),"da") == 0) { // rdar://66836891
        return; // skip all remapping; U+201C/U+201D and U+2018/U+2019 can be open or close
    }
    UErrorCode status = U_ZERO_ERROR;
    ULocaleData* uldata = ulocdata_open(locale.getName(), &status);
    if (U_SUCCESS(status)) {
        static const ULocaleDataDelimiterType delimTypes[][2] = {
            { ULOCDATA_QUOTATION_START, ULOCDATA_QUOTATION_END },
            { ULOCDATA_ALT_QUOTATION_START, ULOCDATA_ALT_QUOTATION_END }
        };
        CategoryOverride catOverrides[kUDelimCount];
        int32_t catOverrideCount = 0;

        for (int32_t delimIndex = 0; delimIndex < UPRV_LENGTHOF(delimTypes); delimIndex++) {
            UChar32 quotOpen = 0, quotClose = 0;
            UChar uDelim[kUDelimBuf];
            int32_t uDelimLen;

            // TODO: Currently we assume all delimiters in CLDR data are single BMP characters.
            // That is currently true but we should at least expand this in the future to handle single
            // UTF32 characters.
            status = U_ZERO_ERROR;
            uDelimLen = ulocdata_getDelimiter(uldata, delimTypes[delimIndex][0], uDelim, kUDelimBuf, &status);
            if (U_SUCCESS(status) && uDelimLen==1) {
                quotOpen = uDelim[0];
            }
            status = U_ZERO_ERROR;
            uDelimLen = ulocdata_getDelimiter(uldata, delimTypes[delimIndex][1], uDelim, kUDelimBuf, &status);
            if (U_SUCCESS(status) && uDelimLen==1) {
                quotClose = uDelim[0];
                if (quotClose == 0x201C) {
                    // rdar://67787054&67804156&73179567 Simplify/generalize linebreak check for not remapping quotes.
                    // 0x201C can be ambiguous with mix of English usage (where it is open double quote),
                    // so do not remap it from QU (affects e.g. de,hr,ru,...). However in these cases,
                    // if 0x201D != quotOpen then it is unambiguously close if used (per English usage) and
                    // can be remapped (fr_CA and ur use 0x201D as quotOpen). We do not need to separately
                    // check for 0x201D != quotOpen here since quotOpen != quotClose is checked below before
                    // any remapping.
                    quotClose = 0x201D;
                }
                if (quotClose == 0x2018) {
                    // rdar://67787054&67804156&73179567 Simplify/generalize linebreak check for not remapping quotes.
                    // Similarly 0x2018 can be ambiguous with mix of English usage (where it is open),
                    // so do not remap it from QU (affects e.g. de,...). But here we cannot say that
                    // 0x2019 (TrueApostrophe) is unambiguously close if used since it could be apostrophe,
                    // so we do not want to remap that from QU either.
                    quotClose = 0; // no remapping, since linebreak class of 0 is not U_LB_QUOTATION
                }
            }
            if (quotOpen != quotClose) { // if they are the same we cannot distinguish OP and CL !
                // rdar://63475386 U+2019 apostrophe should remain linebreak class QU
                // only remap the classes for characters that currently have linebreak class QU
                // and are not U+2019 (true apostrophe / glottal stop); need to wait to check here
                // so that the test quotOpen != quotClose is valid.
                if (u_getIntPropertyValue(quotOpen, UCHAR_LINE_BREAK) == U_LB_QUOTATION && quotOpen != kTrueApostrophe) {
                    catOverrides[catOverrideCount].c = quotOpen;
                    catOverrides[catOverrideCount++].category = ucptrie_get(fData->fTrie, kPrototypeForOP);
                }
                if (u_getIntPropertyValue(quotClose, UCHAR_LINE_BREAK) == U_LB_QUOTATION && quotClose != kTrueApostrophe) {
                    catOverrides[catOverrideCount].c = quotClose;
                    catOverrides[catOverrideCount++].category = ucptrie_get(fData->fTrie, kPrototypeForCL);
                }
            }
        }
        ulocdata_close(uldata);

        if (catOverrideCount > 0) {
            fCatOverrideCount = catOverrideCount;
            fCatOverrides = new CategoryOverride[catOverrideCount];
            for (int32_t orItem = 0; orItem < catOverrideCount; ++orItem) {
                fCatOverrides[orItem] = catOverrides[orItem];
            }
        }
    }
}
#endif // APPLE_ICU_CHANGES

//-----------------------------------------------------------------------------
//
//    clone - Returns a newly-constructed RuleBasedBreakIterator with the same
//            behavior, and iterating over the same text, as this one.
//            Virtual function: does the right thing with subclasses.
//
//-----------------------------------------------------------------------------
RuleBasedBreakIterator*
RuleBasedBreakIterator::clone() const {
    return new RuleBasedBreakIterator(*this);
}

/**
 * Equality operator.  Returns true if both BreakIterators are of the
 * same class, have the same behavior, and iterate over the same text.
 */
bool
RuleBasedBreakIterator::operator==(const BreakIterator& that) const {
    if (typeid(*this) != typeid(that)) {
        return false;
    }
    if (this == &that) {
        return true;
    }

    // The base class BreakIterator carries no state that participates in equality,
    // and does not implement an equality function that would otherwise be
    // checked at this point.

    const RuleBasedBreakIterator& that2 = static_cast<const RuleBasedBreakIterator&>(that);
#if APPLE_ICU_CHANGES
// rdar://36667210 Add ubrk_setLineWordOpts to programmatically set @lw options, add lw=keep-hangul support via keyword or function
    if (that2.fLineWordOpts != fLineWordOpts) {
        return false;
    }
#endif // APPLE_ICU_CHANGES

    if (!utext_equals(&fText, &that2.fText)) {
        // The two break iterators are operating on different text,
        //   or have a different iteration position.
        //   Note that fText's position is always the same as the break iterator's position.
        return false;
    }

    if (!(fPosition == that2.fPosition &&
            fRuleStatusIndex == that2.fRuleStatusIndex &&
            fDone == that2.fDone)) {
        return false;
    }

    if (that2.fData == fData ||
        (fData != nullptr && that2.fData != nullptr && *that2.fData == *fData)) {
            // The two break iterators are using the same rules.
            return true;
        }
    return false;
}

/**
 * Compute a hash code for this BreakIterator
 * @return A hash code
 */
int32_t
RuleBasedBreakIterator::hashCode() const {
    int32_t   hash = 0;
    if (fData != nullptr) {
        hash = fData->hashCode();
    }
    return hash;
}


void RuleBasedBreakIterator::setText(UText *ut, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fBreakCache->reset();
    fDictionaryCache->reset();
    utext_clone(&fText, ut, false, true, &status);

    // Set up a dummy CharacterIterator to be returned if anyone
    //   calls getText().  With input from UText, there is no reasonable
    //   way to return a characterIterator over the actual input text.
    //   Return one over an empty string instead - this is the closest
    //   we can come to signaling a failure.
    //   (GetText() is obsolete, this failure is sort of OK)
    fSCharIter.setText(u"", 0);

    if (fCharIter != &fSCharIter) {
        // existing fCharIter was adopted from the outside.  Delete it now.
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

    this->first();
}


UText *RuleBasedBreakIterator::getUText(UText *fillIn, UErrorCode &status) const {
    UText *result = utext_clone(fillIn, &fText, false, true, &status);
    return result;
}


//=======================================================================
// BreakIterator overrides
//=======================================================================

/**
 * Return a CharacterIterator over the text being analyzed.
 */
CharacterIterator&
RuleBasedBreakIterator::getText() const {
    return *fCharIter;
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::adoptText(CharacterIterator* newText) {
    // If we are holding a CharacterIterator adopted from a
    //   previous call to this function, delete it now.
    if (fCharIter != &fSCharIter) {
        delete fCharIter;
    }

    fCharIter = newText;
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->reset();
    fDictionaryCache->reset();
    if (newText==nullptr || newText->startIndex() != 0) {
        // startIndex !=0 wants to be an error, but there's no way to report it.
        // Make the iterator text be an empty string.
        utext_openUChars(&fText, nullptr, 0, &status);
    } else {
        utext_openCharacterIterator(&fText, newText, &status);
    }
    this->first();
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::setText(const UnicodeString& newText) {
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->reset();
    fDictionaryCache->reset();
    utext_openConstUnicodeString(&fText, &newText, &status);

    // Set up a character iterator on the string.
    //   Needed in case someone calls getText().
    //  Can not, unfortunately, do this lazily on the (probably never)
    //  call to getText(), because getText is const.
    fSCharIter.setText(newText.getBuffer(), newText.length());

    if (fCharIter != &fSCharIter) {
        // old fCharIter was adopted from the outside.  Delete it.
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

    this->first();
}


/**
 *  Provide a new UText for the input text.  Must reference text with contents identical
 *  to the original.
 *  Intended for use with text data originating in Java (garbage collected) environments
 *  where the data may be moved in memory at arbitrary times.
 */
RuleBasedBreakIterator &RuleBasedBreakIterator::refreshInputText(UText *input, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (input == nullptr) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    int64_t pos = utext_getNativeIndex(&fText);
    //  Shallow read-only clone of the new UText into the existing input UText
    utext_clone(&fText, input, false, true, &status);
    if (U_FAILURE(status)) {
        return *this;
    }
    utext_setNativeIndex(&fText, pos);
    if (utext_getNativeIndex(&fText) != pos) {
        // Sanity check.  The new input utext is supposed to have the exact same
        // contents as the old.  If we can't set to the same position, it doesn't.
        // The contents underlying the old utext might be invalid at this point,
        // so it's not safe to check directly.
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return *this;
}


/**
 * Sets the current iteration position to the beginning of the text, position zero.
 * @return The new iterator position, which is zero.
 */
int32_t RuleBasedBreakIterator::first() {
    UErrorCode status = U_ZERO_ERROR;
    if (!fBreakCache->seek(0)) {
        fBreakCache->populateNear(0, status);
    }
    fBreakCache->current();
    U_ASSERT(fPosition == 0);
    return 0;
}

/**
 * Sets the current iteration position to the end of the text.
 * @return The text's past-the-end offset.
 */
int32_t RuleBasedBreakIterator::last() {
    int32_t endPos = (int32_t)utext_nativeLength(&fText);
    UBool endShouldBeBoundary = isBoundary(endPos);      // Has side effect of setting iterator position.
    (void)endShouldBeBoundary;
    U_ASSERT(endShouldBeBoundary);
    U_ASSERT(fPosition == endPos);
    return endPos;
}

/**
 * Advances the iterator either forward or backward the specified number of steps.
 * Negative values move backward, and positive values move forward.  This is
 * equivalent to repeatedly calling next() or previous().
 * @param n The number of steps to move.  The sign indicates the direction
 * (negative is backwards, and positive is forwards).
 * @return The character offset of the boundary position n boundaries away from
 * the current one.
 */
int32_t RuleBasedBreakIterator::next(int32_t n) {
    int32_t result = 0;
    if (n > 0) {
        for (; n > 0 && result != UBRK_DONE; --n) {
            result = next();
        }
    } else if (n < 0) {
        for (; n < 0 && result != UBRK_DONE; ++n) {
            result = previous();
        }
    } else {
        result = current();
    }
    return result;
}

/**
 * Advances the iterator to the next boundary position.
 * @return The position of the first boundary after this one.
 */
int32_t RuleBasedBreakIterator::next() {
    fBreakCache->next();
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Move the iterator backwards, to the boundary preceding the current one.
 *
 *         Starts from the current position within fText.
 *         Starting position need not be on a boundary.
 *
 * @return The position of the boundary position immediately preceding the starting position.
 */
int32_t RuleBasedBreakIterator::previous() {
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->previous(status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Sets the iterator to refer to the first boundary position following
 * the specified position.
 * @param startPos The position from which to begin searching for a break position.
 * @return The position of the first break after the current position.
 */
int32_t RuleBasedBreakIterator::following(int32_t startPos) {
    // if the supplied position is before the beginning, return the
    // text's starting offset
    if (startPos < 0) {
        return first();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8. Or it may be beyond the end of the text.
    utext_setNativeIndex(&fText, startPos);
    startPos = (int32_t)utext_getNativeIndex(&fText);

    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->following(startPos, status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Sets the iterator to refer to the last boundary position before the
 * specified position.
 * @param offset The position to begin searching for a break from.
 * @return The position of the last boundary before the starting position.
 */
int32_t RuleBasedBreakIterator::preceding(int32_t offset) {
    if (offset > utext_nativeLength(&fText)) {
        return last();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.

    utext_setNativeIndex(&fText, offset);
    int32_t adjustedOffset = static_cast<int32_t>(utext_getNativeIndex(&fText));

    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->preceding(adjustedOffset, status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Returns true if the specified position is a boundary position.  As a side
 * effect, leaves the iterator pointing to the first boundary position at
 * or after "offset".
 *
 * @param offset the offset to check.
 * @return True if "offset" is a boundary position.
 */
UBool RuleBasedBreakIterator::isBoundary(int32_t offset) {
    // out-of-range indexes are never boundary positions
    if (offset < 0) {
        first();       // For side effects on current position, tag values.
        return false;
    }

    // Adjust offset to be on a code point boundary and not beyond the end of the text.
    // Note that isBoundary() is always false for offsets that are not on code point boundaries.
    // But we still need the side effect of leaving iteration at the following boundary.

    utext_setNativeIndex(&fText, offset);
    int32_t adjustedOffset = static_cast<int32_t>(utext_getNativeIndex(&fText));

    bool result = false;
    UErrorCode status = U_ZERO_ERROR;
    if (fBreakCache->seek(adjustedOffset) || fBreakCache->populateNear(adjustedOffset, status)) {
        result = (fBreakCache->current() == offset);
    }

    if (result && adjustedOffset < offset && utext_char32At(&fText, offset) == U_SENTINEL) {
        // Original offset is beyond the end of the text. Return false, it's not a boundary,
        // but the iteration position remains set to the end of the text, which is a boundary.
        return false;
    }
    if (!result) {
        // Not on a boundary. isBoundary() must leave iterator on the following boundary.
        // Cache->seek(), above, left us on the preceding boundary, so advance one.
        next();
    }
    return result;
}


/**
 * Returns the current iteration position.
 * @return The current iteration position.
 */
int32_t RuleBasedBreakIterator::current() const {
    return fPosition;
}


//=======================================================================
// implementation
//=======================================================================

//
// RBBIRunMode  -  the state machine runs an extra iteration at the beginning and end
//                 of user text.  A variable with this enum type keeps track of where we
//                 are.  The state machine only fetches user input while in the RUN mode.
//
enum RBBIRunMode {
    RBBI_START,     // state machine processing is before first char of input
    RBBI_RUN,       // state machine processing is in the user text
    RBBI_END        // state machine processing is after end of user text.
};


// Wrapper functions to select the appropriate handleNext() or handleSafePrevious()
// instantiation, based on whether an 8 or 16 bit table is required.
//
// These Trie access functions will be inlined within the handleNext()/Previous() instantions.
static inline uint16_t TrieFunc8(const UCPTrie *trie, UChar32 c) {
    return UCPTRIE_FAST_GET(trie, UCPTRIE_8, c);
}

static inline uint16_t TrieFunc16(const UCPTrie *trie, UChar32 c) {
    return UCPTRIE_FAST_GET(trie, UCPTRIE_16, c);
}

int32_t RuleBasedBreakIterator::handleNext() {
    const RBBIStateTable *statetable = fData->fForwardTable;
    bool use8BitsTrie = ucptrie_getValueWidth(fData->fTrie) == UCPTRIE_VALUE_BITS_8;
    if (statetable->fFlags & RBBI_8BITS_ROWS) {
        if (use8BitsTrie) {
            return handleNext<RBBIStateTableRow8, TrieFunc8>();
        } else {
            return handleNext<RBBIStateTableRow8, TrieFunc16>();
        }
    } else {
        if (use8BitsTrie) {
            return handleNext<RBBIStateTableRow16, TrieFunc8>();
        } else {
            return handleNext<RBBIStateTableRow16, TrieFunc16>();
        }
    }
}

#if APPLE_ICU_CHANGES
// rdar://70367682 #163,dab0491810.. Integrate open-source ICU 68.2 into Apple ICU sources
int32_t RuleBasedBreakIterator::handleNextInternal() {
    const RBBIStateTable *statetable = fData->fForwardTable;
    bool use8BitsTrie = ucptrie_getValueWidth(fData->fTrie) == UCPTRIE_VALUE_BITS_8;
    if (statetable->fFlags & RBBI_8BITS_ROWS) {
        if (use8BitsTrie) {
            return handleNextInternal<RBBIStateTableRow8, TrieFunc8>();
        } else {
            return handleNextInternal<RBBIStateTableRow8, TrieFunc16>();
        }
    } else {
        if (use8BitsTrie) {
            return handleNextInternal<RBBIStateTableRow16, TrieFunc8>();
        } else {
            return handleNextInternal<RBBIStateTableRow16, TrieFunc16>();
        }
    }
}
#endif // APPLE_ICU_CHANGES

int32_t RuleBasedBreakIterator::handleSafePrevious(int32_t fromPosition) {
    const RBBIStateTable *statetable = fData->fReverseTable;
    bool use8BitsTrie = ucptrie_getValueWidth(fData->fTrie) == UCPTRIE_VALUE_BITS_8;
    if (statetable->fFlags & RBBI_8BITS_ROWS) {
        if (use8BitsTrie) {
            return handleSafePrevious<RBBIStateTableRow8, TrieFunc8>(fromPosition);
        } else {
            return handleSafePrevious<RBBIStateTableRow8, TrieFunc16>(fromPosition);
        }
    } else {
        if (use8BitsTrie) {
            return handleSafePrevious<RBBIStateTableRow16, TrieFunc8>(fromPosition);
        } else {
            return handleSafePrevious<RBBIStateTableRow16, TrieFunc16>(fromPosition);
        }
    }
}


//-----------------------------------------------------------------------------------
//
//  handleNext()
//     Run the state machine to find a boundary
//
//-----------------------------------------------------------------------------------
template <typename RowType, RuleBasedBreakIterator::PTrieFunc trieFunc>
int32_t RuleBasedBreakIterator::handleNext() {
#if APPLE_ICU_CHANGES
// rdar://36667210 Add ubrk_setLineWordOpts to programmatically set @lw options, add lw=keep-hangul support via keyword or function
// rdar://70367682 #163,dab0491810.. Integrate open-source ICU 68.2 into Apple ICU sources
    int32_t result = handleNextInternal<RowType, trieFunc>();
    while (fLineWordOpts != UBRK_LINEWORD_NORMAL) {
        UChar32 prevChr = utext_char32At(&fText, result-1);
        UChar32 currChr = utext_char32At(&fText, result);
        if (currChr == U_SENTINEL || prevChr == U_SENTINEL) {
            break;
        }
        if (fLineWordOpts == UBRK_LINEWORD_KEEP_HANGUL) {
            UErrorCode status = U_ZERO_ERROR;
            if (uscript_getScript(currChr, &status) != USCRIPT_HANGUL || uscript_getScript(prevChr, &status) != USCRIPT_HANGUL) {
                break;
            }
        } else {
            if (!u_isalpha(currChr) || !u_isalpha(prevChr)) {
                break;
            }
        }
        int32_t nextResult = handleNextInternal<RowType, trieFunc>();
        if (nextResult <= result) {
            break;
        }
        result = nextResult;
    }
    return result;
}

template <typename RowType, RuleBasedBreakIterator::PTrieFunc trieFunc>
int32_t RuleBasedBreakIterator::handleNextInternal() {
#endif // APPLE_ICU_CHANGES
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;

    RowType             *row;
    UChar32             c;
    int32_t             result             = 0;
    int32_t             initialPosition    = 0;
    const RBBIStateTable *statetable       = fData->fForwardTable;
    const char         *tableData          = statetable->fTableData;
    uint32_t            tableRowLen        = statetable->fRowLen;
    uint32_t            dictStart          = statetable->fDictCategoriesStart;
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPuts("Handle Next   pos   char  state category");
        }
    #endif

    // handleNext always sets the break tag value.
    // Set the default for it.
    fRuleStatusIndex = 0;

    fDictionaryCharCount = 0;

    // if we're already at the end of the text, return DONE.
    initialPosition = fPosition;
    UTEXT_SETNATIVEINDEX(&fText, initialPosition);
    result          = initialPosition;
    c               = UTEXT_NEXT32(&fText);
    if (c==U_SENTINEL) {
        fDone = true;
        return UBRK_DONE;
    }

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RowType *)
            //(statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the end of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
#if APPLE_ICU_CHANGES
// rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
// (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
// rdar://51193810 delimiter category overrides, max of 4
            if (fLatin1Cat!=NULL && c<0x100) {
                category = fLatin1Cat[c]; // fast Latin1 class lookup used for urbtok
            } else {
                UBool didOverride = false;
                for (int32_t orItem = 0; orItem < fCatOverrideCount; ++orItem) {
                    if (c == fCatOverrides[orItem].c) {
                        category = fCatOverrides[orItem].category;
                        didOverride = true;
                        break;
                    }
                }
                if (!didOverride) {
                    category = trieFunc(fData->fTrie, c);
                    fDictionaryCharCount += (category >= dictStart);
                }
            }
#else
            category = trieFunc(fData->fTrie, c);
            fDictionaryCharCount += (category >= dictStart);
#endif // APPLE_ICU_CHANGES
        }

        #ifdef RBBI_DEBUG
            if (gTrace) {
                RBBIDebugPrintf("             %4" PRId64 "   ", utext_getNativeIndex(&fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //

        // fNextState is a variable-length array.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RowType *)
            // (statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


        uint16_t accepting = row->fAccepting;
        if (accepting == ACCEPTING_UNCONDITIONAL) {
            // Match found, common case.
            if (mode != RBBI_START) {
                result = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
            }
            fRuleStatusIndex = row->fTagsIdx;   // Remember the break status (tag) values.
        } else if (accepting > ACCEPTING_UNCONDITIONAL) {
            // Lookahead match is completed.
            U_ASSERT(accepting < fData->fForwardTable->fLookAheadResultsSize);
            int32_t lookaheadResult = fLookAheadMatches[accepting];
            if (lookaheadResult >= 0) {
                fRuleStatusIndex = row->fTagsIdx;
                fPosition = lookaheadResult;
                return lookaheadResult;
            }
        }

        // If we are at the position of the '/' in a look-ahead (hard break) rule;
        // record the current position, to be returned later, if the full rule matches.
        // TODO: Move this check before the previous check of fAccepting.
        //       This would enable hard-break rules with no following context.
        //       But there are line break test failures when trying this. Investigate.
        //       Issue ICU-20837
        uint16_t rule = row->fLookAhead;
        U_ASSERT(rule == 0 || rule > ACCEPTING_UNCONDITIONAL);
        U_ASSERT(rule == 0 || rule < fData->fForwardTable->fLookAheadResultsSize);
        if (rule > ACCEPTING_UNCONDITIONAL) {
            int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
            fLookAheadMatches[rule] = pos;
        }

        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

        // Advance to the next character.
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = UTEXT_NEXT32(&fText);
        } else {
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }
    }

    // The state machine is done.  Check whether it found a match...

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        utext_setNativeIndex(&fText, initialPosition);
        utext_next32(&fText);
        result = (int32_t)utext_getNativeIndex(&fText);
        fRuleStatusIndex = 0;
    }

    // Leave the iterator at our result position.
    fPosition = result;
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


//-----------------------------------------------------------------------------------
//
//  handleSafePrevious()
//
//      Iterate backwards using the safe reverse rules.
//      The logic of this function is similar to handleNext(), but simpler
//      because the safe table does not require as many options.
//
//-----------------------------------------------------------------------------------
template <typename RowType, RuleBasedBreakIterator::PTrieFunc trieFunc>
int32_t RuleBasedBreakIterator::handleSafePrevious(int32_t fromPosition) {

    int32_t             state;
    uint16_t            category        = 0;
    RowType            *row;
    UChar32             c;
    int32_t             result          = 0;

    const RBBIStateTable *stateTable = fData->fReverseTable;
    UTEXT_SETNATIVEINDEX(&fText, fromPosition);
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPuts("Handle Previous   pos   char  state category");
        }
    #endif

    // if we're already at the start of the text, return DONE.
    if (fData == nullptr || UTEXT_GETNATIVEINDEX(&fText)==0) {
        return BreakIterator::DONE;
    }

    //  Set the initial state for the state machine
    c = UTEXT_PREVIOUS32(&fText);
    state = START_STATE;
    row = (RowType *)
            (stateTable->fTableData + (stateTable->fRowLen * state));

    // loop until we reach the start of the text or transition to state 0
    //
    for (; c != U_SENTINEL; c = UTEXT_PREVIOUS32(&fText)) {

        // look up the current character's character category, which tells us
        // which column in the state table to look at.
        //
        //  Off the dictionary flag bit. For reverse iteration it is not used.
        category = trieFunc(fData->fTrie, c);

        #ifdef RBBI_DEBUG
            if (gTrace) {
                RBBIDebugPrintf("             %4d   ", (int32_t)utext_getNativeIndex(&fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //
        // fNextState is a variable-length array.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RowType *)
            (stateTable->fTableData + (stateTable->fRowLen * state));

        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // Transition to state zero means we have found a safe point.
            break;
        }
    }

    // The state machine is done.  Check whether it found a match...
    result = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


//-------------------------------------------------------------------------------
//
//   getRuleStatus()   Return the break rule tag associated with the current
//                     iterator position.  If the iterator arrived at its current
//                     position by iterating forwards, the value will have been
//                     cached by the handleNext() function.
//
//-------------------------------------------------------------------------------

int32_t  RuleBasedBreakIterator::getRuleStatus() const {

    // fLastRuleStatusIndex indexes to the start of the appropriate status record
    //                                                 (the number of status values.)
    //   This function returns the last (largest) of the array of status values.
    int32_t  idx = fRuleStatusIndex + fData->fRuleStatusTable[fRuleStatusIndex];
    int32_t  tagVal = fData->fRuleStatusTable[idx];

    return tagVal;
}


int32_t RuleBasedBreakIterator::getRuleStatusVec(
             int32_t *fillInVec, int32_t capacity, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }

    int32_t  numVals = fData->fRuleStatusTable[fRuleStatusIndex];
    int32_t  numValsToCopy = numVals;
    if (numVals > capacity) {
        status = U_BUFFER_OVERFLOW_ERROR;
        numValsToCopy = capacity;
    }
    int i;
    for (i=0; i<numValsToCopy; i++) {
        fillInVec[i] = fData->fRuleStatusTable[fRuleStatusIndex + i + 1];
    }
    return numVals;
}

#if APPLE_ICU_CHANGES
// rdar://35946337 (Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough)
int32_t RuleBasedBreakIterator::tokenize(int32_t maxTokens, RuleBasedTokenRange *outTokenRanges, unsigned long *outTokenFlags)
{
    if (fDone) {
        return 0;
    }
    RuleBasedTokenRange *outTokenLimit = outTokenRanges + maxTokens;
    RuleBasedTokenRange *outTokenP = outTokenRanges;
    int32_t lastOffset = fPosition;
    while (outTokenP < outTokenLimit) {
        // rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough
        // (Fold populateFollowing() into tokenize(); add faster category lookup for Latin1 as in rbtok)
        // start portion from inlining populateFollowing()
        int32_t pos = 0;
        int32_t ruleStatusIdx = 0;
        int32_t startPos = fPosition;

        if (fDictionaryCache->following(startPos, &pos, &ruleStatusIdx)) {
            fPosition = pos;
            fRuleStatusIndex = ruleStatusIdx;
        } else {
            pos = handleNextInternal(); // sets fRuleStatusIndex for the pos it returns, updates fPosition
            if (pos == UBRK_DONE) {
                // fDone = true; already set by handleNextInternal
                break;
            }
            // Use current result from handleNextInternal(), including fRuleStatusIndex,
            // unless overridden by dictionary subdivisions
            fPosition = pos;
            if (fDictionaryCharCount > 0) {
                // The text segment obtained from the rules includes dictionary characters.
                // Subdivide it, with subdivided results going into the dictionary cache.
                fDictionaryCache->populateDictionary(startPos, pos, fRuleStatusIndex, fRuleStatusIndex);
                if (fDictionaryCache->following(startPos, &pos, &ruleStatusIdx)) {
                    fPosition = pos;
                    fRuleStatusIndex = ruleStatusIdx;
                }
            }
        }
        // end portion from inlining populateFollowing()
        // rdar://35946337 Rewrite urbtok_tokenize & other urbtok_ interfaces to work with new RBBI but be fast enough:
        //   Fix urbtok_tokenize to skip tokens whose flag value is -1; set flags entries to union of all getRuleStatusVec values
        int32_t flagCount = fData->fRuleStatusTable[fRuleStatusIndex];
        const int32_t* flagPtr = fData->fRuleStatusTable + fRuleStatusIndex + flagCount;
        int32_t flagSet = *flagPtr; // if -1 then skip token
        if (flagSet != -1) {
            outTokenP->location = lastOffset;
            outTokenP++->length = fPosition - lastOffset;
            if (outTokenFlags) {
                // flagSet should be the OR of all flags returned by getRuleStatusVec;
                // here we collect from high-order to low-order.
                while (--flagCount > 0) {
                   flagSet |=  *--flagPtr;
                }
                *outTokenFlags++ = (unsigned long)flagSet;
            }
        }
        lastOffset = fPosition;
    }
    return (outTokenP - outTokenRanges);
}
#endif // APPLE_ICU_CHANGES

//-------------------------------------------------------------------------------
//
//   getBinaryRules        Access to the compiled form of the rules,
//                         for use by build system tools that save the data
//                         for standard iterator types.
//
//-------------------------------------------------------------------------------
const uint8_t  *RuleBasedBreakIterator::getBinaryRules(uint32_t &length) {
    const uint8_t  *retPtr = nullptr;
    length = 0;

    if (fData != nullptr) {
        retPtr = (const uint8_t *)fData->fHeader;
        length = fData->fHeader->fLength;
    }
    return retPtr;
}


RuleBasedBreakIterator *RuleBasedBreakIterator::createBufferClone(
        void * /*stackBuffer*/, int32_t &bufferSize, UErrorCode &status) {
    if (U_FAILURE(status)){
        return nullptr;
    }

    if (bufferSize == 0) {
        bufferSize = 1;  // preflighting for deprecated functionality
        return nullptr;
    }

    BreakIterator *clonedBI = clone();
    if (clonedBI == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        status = U_SAFECLONE_ALLOCATED_WARNING;
    }
    return (RuleBasedBreakIterator *)clonedBI;
}

U_NAMESPACE_END


static icu::UStack *gLanguageBreakFactories = nullptr;
static const icu::UnicodeString *gEmptyString = nullptr;
static icu::UInitOnce gLanguageBreakFactoriesInitOnce {};
static icu::UInitOnce gRBBIInitOnce {};
static icu::ICULanguageBreakFactory *gICULanguageBreakFactory = nullptr;

/**
 * Release all static memory held by breakiterator.
 */
U_CDECL_BEGIN
UBool U_CALLCONV rbbi_cleanup() {
    delete gLanguageBreakFactories;
    gLanguageBreakFactories = nullptr;
    delete gEmptyString;
    gEmptyString = nullptr;
    gLanguageBreakFactoriesInitOnce.reset();
    gRBBIInitOnce.reset();
    return true;
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV _deleteFactory(void *obj) {
    delete (icu::LanguageBreakFactory *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static void U_CALLCONV rbbiInit() {
    gEmptyString = new UnicodeString();
    ucln_common_registerCleanup(UCLN_COMMON_RBBI, rbbi_cleanup);
}

static void U_CALLCONV initLanguageFactories(UErrorCode& status) {
    U_ASSERT(gLanguageBreakFactories == nullptr);
    gLanguageBreakFactories = new UStack(_deleteFactory, nullptr, status);
    if (gLanguageBreakFactories != nullptr && U_SUCCESS(status)) {
        LocalPointer<ICULanguageBreakFactory> factory(new ICULanguageBreakFactory(status), status);
        if (U_SUCCESS(status)) {
            gICULanguageBreakFactory = factory.orphan();
            gLanguageBreakFactories->push(gICULanguageBreakFactory, status);
#ifdef U_LOCAL_SERVICE_HOOK
            LanguageBreakFactory *extra = (LanguageBreakFactory *)uprv_svc_hook("languageBreakFactory", &status);
            if (extra != nullptr) {
                gLanguageBreakFactories->push(extra, status);
            }
#endif
        }
    }
    ucln_common_registerCleanup(UCLN_COMMON_RBBI, rbbi_cleanup);
}

void ensureLanguageFactories(UErrorCode& status) {
    umtx_initOnce(gLanguageBreakFactoriesInitOnce, &initLanguageFactories, status);
}

static const LanguageBreakEngine*
getLanguageBreakEngineFromFactory(UChar32 c, const char* locale)
{
    UErrorCode status = U_ZERO_ERROR;
    ensureLanguageFactories(status);
    if (U_FAILURE(status)) return nullptr;

    int32_t i = gLanguageBreakFactories->size();
    const LanguageBreakEngine *lbe = nullptr;
    while (--i >= 0) {
        LanguageBreakFactory *factory = (LanguageBreakFactory *)(gLanguageBreakFactories->elementAt(i));
        lbe = factory->getEngineFor(c, locale);
        if (lbe != nullptr) {
            break;
        }
    }
    return lbe;
}


//-------------------------------------------------------------------------------
//
//  getLanguageBreakEngine  Find an appropriate LanguageBreakEngine for the
//                          the character c.
//
//-------------------------------------------------------------------------------
const LanguageBreakEngine *
RuleBasedBreakIterator::getLanguageBreakEngine(UChar32 c, const char* locale) {
    const LanguageBreakEngine *lbe = nullptr;
    UErrorCode status = U_ZERO_ERROR;

    if (fLanguageBreakEngines == nullptr) {
        fLanguageBreakEngines = new UStack(status);
        if (fLanguageBreakEngines == nullptr || U_FAILURE(status)) {
            delete fLanguageBreakEngines;
            fLanguageBreakEngines = 0;
            return nullptr;
        }
    }

    int32_t i = fLanguageBreakEngines->size();
    while (--i >= 0) {
        lbe = (const LanguageBreakEngine *)(fLanguageBreakEngines->elementAt(i));
        if (lbe->handles(c, locale)) {
            return lbe;
        }
    }

    // No existing dictionary took the character. See if a factory wants to
    // give us a new LanguageBreakEngine for this character.
    lbe = getLanguageBreakEngineFromFactory(c, locale);

    // If we got one, use it and push it on our stack.
    if (lbe != nullptr) {
        fLanguageBreakEngines->push((void *)lbe, status);
        // Even if we can't remember it, we can keep looking it up, so
        // return it even if the push fails.
        return lbe;
    }

    // No engine is forthcoming for this character. Add it to the
    // reject set. Create the reject break engine if needed.
    if (fUnhandledBreakEngine == nullptr) {
        fUnhandledBreakEngine = new UnhandledEngine(status);
        if (U_SUCCESS(status) && fUnhandledBreakEngine == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        // Put it last so that scripts for which we have an engine get tried
        // first.
        fLanguageBreakEngines->insertElementAt(fUnhandledBreakEngine, 0, status);
        // If we can't insert it, or creation failed, get rid of it
        U_ASSERT(!fLanguageBreakEngines->hasDeleter());
        if (U_FAILURE(status)) {
            delete fUnhandledBreakEngine;
            fUnhandledBreakEngine = 0;
            return nullptr;
        }
    }

    // Tell the reject engine about the character; at its discretion, it may
    // add more than just the one character.
    fUnhandledBreakEngine->handleCharacter(c);

    return fUnhandledBreakEngine;
}

#ifndef U_HIDE_DRAFT_API
void U_EXPORT2 RuleBasedBreakIterator::registerExternalBreakEngine(
                  ExternalBreakEngine* toAdopt, UErrorCode& status) {
    LocalPointer<ExternalBreakEngine> engine(toAdopt, status);
    if (U_FAILURE(status)) return;
    ensureLanguageFactories(status);
    if (U_FAILURE(status)) return;
    gICULanguageBreakFactory->addExternalEngine(engine.orphan(), status);
}
#endif  /* U_HIDE_DRAFT_API */


void RuleBasedBreakIterator::dumpCache() {
    fBreakCache->dumpCache();
}

void RuleBasedBreakIterator::dumpTables() {
    fData->printData();
}

/**
 * Returns the description used to create this iterator
 */

const UnicodeString&
RuleBasedBreakIterator::getRules() const {
    if (fData != nullptr) {
        return fData->getRuleSourceString();
    } else {
        umtx_initOnce(gRBBIInitOnce, &rbbiInit);
        return *gEmptyString;
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
