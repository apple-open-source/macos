/*
***************************************************************************
* Copyright (C) 2006-2008 Apple Inc. All Rights Reserved.                 *
***************************************************************************
*/

#ifndef RBTOK_H
#define RBTOK_H

#include "unicode/utypes.h"

/**
 * \file
 * \brief C++ API: Rule Based Tokenizer
 */

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/urbtok.h"
#include "unicode/rbbi.h"
#include "unicode/parseerr.h"


U_NAMESPACE_BEGIN

/** @internal */
struct RBBIDataHeader;
struct RBBIStateTableRow;


/**
 *
 * A subclass of RuleBasedBreakIterator that adds tokenization functionality.

 * <p>This class is for internal use only by Apple Computer, Inc.</p>
 *
 */
class U_COMMON_API RuleBasedTokenizer : public RuleBasedBreakIterator {

private:
    /**
     * The row corresponding to the start state
     * @internal
     */
    const RBBIStateTableRow *fStartRow;

    /**
     * The merged flag results for accepting states
     * @internal
     */
    int32_t *fStateFlags;

    /**
     * Character categories for the Latin1 subset of Unicode
     * @internal
     */
    int16_t *fLatin1Cat;

public:
    /**
     * Construct a RuleBasedTokenizer from a set of rules supplied as a string.
     * @param rules The break rules to be used.
     * @param parseError  In the event of a syntax error in the rules, provides the location
     *                    within the rules of the problem.
     * @param status Information on any errors encountered.
     * @internal
     */
    RuleBasedTokenizer(const UnicodeString &rules, UParseError &parseErr, UErrorCode &status);

    /**
     * Constructor from a flattened set of RBBI data in uprv_malloc'd memory.
     *             RulesBasedBreakIterators built from a custom set of rules
     *             are created via this constructor; the rules are compiled
     *             into memory, then the break iterator is constructed here.
     *
     *             The break iterator adopts the memory, and will
     *             free it when done.
     * @internal
     */
    RuleBasedTokenizer(uint8_t *data, UErrorCode &status);

    /**
     * Constructor from a flattened set of RBBI data in umemory which need not
     *             be malloced (e.g. it may be a memory-mapped file, etc.).
       *
     *             This version does not adopt the memory, and does not
     *             free it when done.
     * @internal
     */
    enum EDontAdopt {
        kDontAdopt
    };
    RuleBasedTokenizer(const uint8_t *data, enum EDontAdopt dontAdopt, UErrorCode &status);

    /**
     * Destructor
     *  @internal
     */
    virtual ~RuleBasedTokenizer();

    /**
     * Fetch the next set of tokens.
     * @param maxTokens The maximum number of tokens to return.
     * @param outTokenRanges Pointer to output array of token ranges.
     * @param outTokenFlags (optional) pointer to output array of token flags.
     * @internal
     */
    int32_t tokenize(int32_t maxTokens, RuleBasedTokenRange *outTokenRanges, unsigned long *outTokenFlags);

private:
    /**
      * Common initialization function, used by constructors.
      * @internal
      */
    void init();
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

#endif
