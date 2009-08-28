/*
***************************************************************************
* Copyright (C) 2006-2008 Apple Inc. All Rights Reserved.                 *
***************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "rbtok.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
#include "rbbidata.h"

U_NAMESPACE_BEGIN


#if defined(__GNUC__) && (__GNUC__ >= 4)
#pragma GCC optimization_level 3
#endif

static const int16_t START_STATE = 1;     // The state number of the starting state
static const int16_t STOP_STATE  = 0;     // The state-transition value indicating "stop"

int32_t RuleBasedTokenizer::tokenize(int32_t maxTokens, RuleBasedTokenRange *outTokenRanges, unsigned long *outTokenFlags)
{
    RuleBasedTokenRange *outTokenLimit = outTokenRanges + maxTokens;
    RuleBasedTokenRange *outTokenP = outTokenRanges;
    int32_t             state;
    int16_t             category;
    
    const RBBIStateTableRow  *row;
    const RBBIStateTableRow  *const startRow = fStartRow;

    int32_t             lastAcceptingState = 0;
    UChar32             c = 0;
    signed long         prev;
    signed long         result;
    const char         *const tableData       = fData->fForwardTable->fTableData;
    const uint32_t            tableRowLen     = fData->fForwardTable->fRowLen;
    UText *text = fText;

    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPuts("Handle Next   pos   char  state category");
        }
    #endif

    fLastStatusIndexValid = FALSE;

    // if we're already at the end of the text, return DONE.
    prev = (signed long)UTEXT_GETNATIVEINDEX(text);
    
    // loop until we reach the end of the text or transition to state 0
    //
    const UTrie         *trie = &fData->fTrie;
    while (outTokenP < outTokenLimit) {
        c               = UTEXT_NEXT32(text);
        if (c == U_SENTINEL)
        {
            goto exitTokenizer;
        }
        //  Set the initial state for the state machine
        state = START_STATE;
        row = startRow;
            
        // if we have cached break positions and we're still in the range
        // covered by them, just move one step forward in the cache
        if (fCachedBreakPositions != NULL) {
            if (fPositionInCache < fNumCachedBreakPositions - 1) {
                ++fPositionInCache;
                result = fCachedBreakPositions[fPositionInCache];
                goto emitToken;
            }
            else {
                reset();
            }
        }

        while (c != U_SENTINEL) {
            //
            // Get the char category.  An incoming category of 1 or 2 means that
            //      we are preset for doing the beginning or end of input, and
            //      that we shouldn't get a category from an actual text input character.
            //
                // look up the current character's character category, which tells us
                // which column in the state table to look at.
                // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
                //        not the size of the character going in, which is a UChar32.
                //
                if (__builtin_expect((c < 0x100), 1))
                    category = fLatin1Cat[c];
                else
                    UTRIE_GET16(trie, c, category);
    
                // Check the dictionary bit in the character's category.
                //    Counter is only used by dictionary based iterators (subclasses).
                //    Chars that need to be handled by a dictionary have a flag bit set
                //    in their category values.
                //
                if (__builtin_expect((category & 0x4000) != 0, 0))  {
                    fDictionaryCharCount++;
                    //  And off the dictionary flag bit.
                    category &= ~0x4000;
                }
    
            #ifdef RBBI_DEBUG
                if (fTrace) {
                    RBBIDebugPrintf("             %4d   ", utext_getNativeIndex(fText));
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
            state = row->fNextState[category];
            row = (const RBBIStateTableRow *) (tableData + tableRowLen * state);
    
            if (row->fAccepting == -1) {
                // Match found, common case.
                    result = (signed long)UTEXT_GETNATIVEINDEX(text);
                //fLastRuleStatusIndex = row->fTagIdx;   // Remember the break status (tag) values.
                //lastStatusRow = row;
                lastAcceptingState = state;
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
                c = UTEXT_NEXT32(text);
        }

        if (fDictionaryCharCount > 0) {
            result = (signed long) checkDictionary(prev, (int32_t) result, FALSE);
        }

emitToken:
        // The state machine is done.  Check whether it found a match...
    
        // Leave the iterator at our result position.
        UTEXT_SETNATIVEINDEX(text, result);

        RuleBasedTokenRange range = {(signed long)prev, (signed long) (result-prev)};
        int32_t flags = fStateFlags[lastAcceptingState];

        if (flags == -1)
            goto skipToken;

        *outTokenP++ = range;
        if (outTokenFlags)
        {
            *outTokenFlags++ = (unsigned long) flags;
        }

        if (flags & 0x40000000)
            goto exitTokenizer;

skipToken:
        prev = result;
    }
    
exitTokenizer:
    return (outTokenP - outTokenRanges);
}

#if defined (__GNUC__) && (__GNUC__ >= 4)
#pragma GCC optimization_level reset
#endif

void
RuleBasedTokenizer::init()
{
    const RBBIStateTable *statetable = fData->fForwardTable;
    setBreakType(UBRK_WORD);
    fStartRow = (const RBBIStateTableRow *)
        (statetable->fTableData + (statetable->fRowLen * START_STATE));
    UChar i;
    const UTrie         *trie = &fData->fTrie;
    int16_t category;
    fLatin1Cat = new int16_t[256];
    for (i = 0; i < 256; ++i)
    {
        //UTRIE_GET16(trie, i, category);
        //fLatin1Cat[i] = category;
        fLatin1Cat[i] = _UTRIE_GET_RAW(trie, index, 0, i);
    }
    fStateFlags = new int32_t[statetable->fNumStates];
    for (i = 0; i < statetable->fNumStates; ++i)
    {
        const RBBIStateTableRow *row = (const RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * i));
        int32_t flags = 0;
        if (row->fAccepting == -1)
        {
            const int32_t *vals = (fData->fRuleStatusTable) + (row->fTagIdx);
            const int32_t *valLimit = vals + 1;
            valLimit += *vals++;
            while (vals < valLimit)
            {
                int32_t val = *vals++;
                if (val == 0)
                {
                    break;
                }
                else if (val > 0)
                {
                    flags |= val;
                }
                else
                {
                    flags = val;
                    break;
                }
            }
        }
        fStateFlags[i] = flags;
    }
}

RuleBasedTokenizer::RuleBasedTokenizer(const UnicodeString &rules, UParseError &parseErr, UErrorCode &err)
    : RuleBasedBreakIterator(rules, parseErr, err)
{
    init();
}

RuleBasedTokenizer::RuleBasedTokenizer(uint8_t *data, UErrorCode &status)
    : RuleBasedBreakIterator((RBBIDataHeader *)data, status)
{
    init();
}

RuleBasedTokenizer::RuleBasedTokenizer(const uint8_t *data, enum EDontAdopt, UErrorCode &status)
    : RuleBasedBreakIterator((const RBBIDataHeader *)data, RuleBasedBreakIterator::kDontAdopt, status)
{
    init();
}

RuleBasedTokenizer::~RuleBasedTokenizer() {
    delete [] fStateFlags;
    delete [] fLatin1Cat;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
