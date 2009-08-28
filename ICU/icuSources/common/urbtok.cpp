/*
*****************************************************************************************
* Copyright (C) 2006-2008 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/urbtok.h"

#include "rbtok.h"
#include "unicode/ustring.h"
#include "rbbidata.h"
#include "cmemory.h"
#include "ucmndata.h"

U_NAMESPACE_USE

U_CAPI UBreakIterator* U_EXPORT2
urbtok_openRules(const UChar     *rules,
               int32_t         rulesLength,
               UParseError     *parseErr,
               UErrorCode      *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return 0;
    }

    BreakIterator *result = 0;
    UnicodeString ruleString(rules, rulesLength);
    result = new RuleBasedTokenizer(ruleString, *parseErr, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }

    UBreakIterator *uBI = (UBreakIterator *)result;
    return uBI;
}

U_CAPI UBreakIterator* U_EXPORT2
urbtok_openBinaryRules(const uint8_t *rules,
               UErrorCode      *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return 0;
    }

    uint32_t length = ((const RBBIDataHeader *)rules)->fLength;
    uint8_t *ruleCopy = (uint8_t *) uprv_malloc(length);
    if (ruleCopy == 0)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }
    // Copy the rules so they can be adopted by the tokenizer
    uprv_memcpy(ruleCopy, rules, length);
    BreakIterator *result = 0;
    result = new RuleBasedTokenizer(ruleCopy, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }

    UBreakIterator *uBI = (UBreakIterator *)result;
    return uBI;
}

U_CAPI UBreakIterator* U_EXPORT2
urbtok_openBinaryRulesNoCopy(const uint8_t *rules,
               UErrorCode      *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return 0;
    }

    BreakIterator *result = 0;
    result = new RuleBasedTokenizer(rules, RuleBasedTokenizer::kDontAdopt, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }

    UBreakIterator *uBI = (UBreakIterator *)result;
    return uBI;
}

U_CAPI uint32_t U_EXPORT2
urbtok_getBinaryRules(UBreakIterator      *bi,
                uint8_t             *buffer,
                uint32_t            buffSize,
                UErrorCode          *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return 0;
    }

    uint32_t length;
    const uint8_t *rules = ((RuleBasedBreakIterator *)bi)->getBinaryRules(length);
    if (buffer != 0)
    {
        if (length > buffSize)
        {
            *status = U_BUFFER_OVERFLOW_ERROR;
        }
        else
        {
            uprv_memcpy(buffer, rules, length);
        }
    }
    return length;
}

U_CAPI int32_t U_EXPORT2
urbtok_tokenize(UBreakIterator      *bi,
               int32_t              maxTokens,
               RuleBasedTokenRange  *outTokens,
               unsigned long        *outTokenFlags)
{
    return ((RuleBasedTokenizer *)bi)->tokenize(maxTokens, outTokens, outTokenFlags);
}

U_CAPI void U_EXPORT2
urbtok_swapBinaryRules(const uint8_t *rules,
               uint8_t          *buffer,
               UBool            inIsBigEndian,
               UBool            outIsBigEndian,
               UErrorCode       *status)
{
    DataHeader *outH = NULL;
    int32_t outLength = 0;
    UDataSwapper *ds = udata_openSwapper(inIsBigEndian, U_CHARSET_FAMILY, outIsBigEndian, U_CHARSET_FAMILY, status);
    
    if (status == NULL || U_FAILURE(*status)){
        return;
    }
    
    uint32_t length = ds->readUInt32(((const RBBIDataHeader *)rules)->fLength);
    uint32_t totalLength = sizeof(DataHeader) + length;

    DataHeader *dh = (DataHeader *)uprv_malloc(totalLength);
    if (dh == 0)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto closeSwapper;
    }
    outH = (DataHeader *)uprv_malloc(totalLength);
    if (outH == 0)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(dh);
        goto closeSwapper;
    }
    dh->dataHeader.headerSize = ds->readUInt16(sizeof(DataHeader));
    dh->dataHeader.magic1 = 0xda;
    dh->dataHeader.magic2 = 0x27;
    dh->info.size = ds->readUInt16(sizeof(UDataInfo));
    dh->info.reservedWord = 0;
    dh->info.isBigEndian = inIsBigEndian;
    dh->info.charsetFamily = U_CHARSET_FAMILY;
    dh->info.sizeofUChar = U_SIZEOF_UCHAR;
    dh->info.reservedByte = 0;
    uprv_memcpy(dh->info.dataFormat, "Brk ", sizeof(dh->info.dataFormat));
    uprv_memcpy(dh->info.formatVersion, ((const RBBIDataHeader *)rules)->fFormatVersion, sizeof(dh->info.formatVersion));
    dh->info.dataVersion[0] = 4;        // Unicode version
    dh->info.dataVersion[1] = 1;
    dh->info.dataVersion[2] = 0;
    dh->info.dataVersion[3] = 0;
    uprv_memcpy(((uint8_t*)dh) + sizeof(DataHeader), rules, length);
    
    outLength = ubrk_swap(ds, dh, totalLength, outH, status);
    if (U_SUCCESS(*status) && outLength != totalLength)   // something went horribly wrong
    {
        *status = U_INVALID_FORMAT_ERROR;
    }

    if (U_SUCCESS(*status))
    {
        uprv_memcpy(buffer, ((uint8_t *)outH) + sizeof(DataHeader), length);
    }
    uprv_free(outH);
    uprv_free(dh);

closeSwapper:
    udata_closeSwapper(ds);
}


#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
