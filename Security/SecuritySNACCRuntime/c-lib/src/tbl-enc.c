/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#ifdef TTBL

/*
 * tbl_enc.c - type table encoder
 *
 *
 * Mike Sample
 *
 * Copyright (C) 1993 Michael Sample
 *            and the University of British Columbia
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */


#include <stdio.h>
#include "tbl-incl.h"

/* protos for non exported routines */

/*
int TblEncodePrimTagsAndLens PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, unsigned long int *bytesEncoded));

int TblEncodeConsTagsAndLens PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, unsigned long int *bytesEncoded));
*/

int TblEncodeTagsAndLens PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, unsigned long int *bytesEncoded));

AsnLen TblEncTag PROTO ((BUF_TYPE b, TBLTag *tag));




/*
 * Encode value v as though it is of type modName.typeName.
 * bytesEncoded is set the actual number of bytes in the
 * encode value.
 * returns less than zero if an error occurs otherwise
 * returns 0 for success.
 */
int
TblEncode PARAMS ((tbl, modName, typeName, b, v, bytesEncoded),
    TBL *tbl _AND_
    char *modName _AND_
    char *typeName _AND_
    BUF_TYPE b _AND_
    AVal *v _AND_
    unsigned long int *bytesEncoded)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;
    int retVal;

    tblTd = TblFindTypeDef (tbl, modName, typeName, &tblMod);
    if (tblTd == NULL)
    {
        TblError ("TblEncode: Could not find a type definition with the given module and name");
        return -1;
    }
    *bytesEncoded = 0;
    retVal = TblEncodeType (tblTd->type, b, v, FALSE, bytesEncoded);

    if (BufWriteError (b))
        retVal = -1;

    return retVal;
}  /* TblEncode */


/*
 * returns less than zero if an error occurs
 */
int
TblEncodeType PARAMS ((tblT, b, v, implicit, bytesEncoded),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    AVal *v _AND_
    int implicit _AND_
    unsigned long int *bytesEncoded)
{
    AVal *elmtV;
    AsnList *lVal;
    int retVal = 0;
    unsigned long int tmpBytesEncoded = 0;
    unsigned int currElmt;
    TBLType *listElmtType;
    TBLType *structElmtType;
    TBLType *choiceElmtType;
    AChoiceVal *cVal;
    AStructVal *sVal;
    int implicitRef;
    void *tmp;

    switch (tblT->typeId)
    {
      case TBL_TYPEREF:

          /*
           * carry over implicit ref if goes
           * through typeref with no tags
           */
          implicitRef = tblT->content->a.typeRef->implicit ||
              (implicit &&
               ((tblT->tagList == NULL) || LIST_EMPTY (tblT->tagList)));

          retVal = TblEncodeType (tblT->content->a.typeRef->typeDefPtr->type, b, v, implicitRef, &tmpBytesEncoded);
          break;

      case TBL_SEQUENCE:
      case TBL_SET:
          /* rvs though list value and list type def */
          currElmt = LIST_COUNT (tblT->content->a.elmts)-1;
          sVal = (AStructVal*)v;
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT_RVS (structElmtType, tblT->content->a.elmts)
          {
              elmtV = sVal[currElmt--];
              if (!(structElmtType->optional && (elmtV == NULL)))
              {
                  retVal = TblEncodeType (structElmtType, b, elmtV, FALSE, &tmpBytesEncoded);
                  if (retVal < 0)
                      break; /* exit for loop */
              }
          }
          /* restore list curr in case recursive type */
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          break;

      case TBL_SEQUENCEOF:
      case TBL_SETOF:
          lVal = (AsnList*)v;
          listElmtType = FIRST_LIST_ELMT (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT_RVS (elmtV, lVal)
          {
              retVal = TblEncodeType (listElmtType, b, elmtV, FALSE, &tmpBytesEncoded);
              if (retVal < 0)
                  break; /* exit for loop */
          }
        break;

      case TBL_CHOICE:
          cVal = (AChoiceVal*) v;
          choiceElmtType = (TBLType*)GetAsnListElmt (tblT->content->a.elmts, cVal->choiceId);
          retVal = TblEncodeType (choiceElmtType, b, cVal->val, FALSE, &tmpBytesEncoded);
        break;

      case TBL_BOOLEAN:
          tmpBytesEncoded += BEncAsnBoolContent (b, (AsnBool*)v);
        break;

      case TBL_INTEGER:
      case TBL_ENUMERATED:
          tmpBytesEncoded += BEncAsnIntContent (b, (AsnInt*)v);
        break;

      case TBL_BITSTRING:
          tmpBytesEncoded += BEncAsnBitsContent (b, (AsnBits*)v);
        break;

      case TBL_OCTETSTRING:
          tmpBytesEncoded += BEncAsnOctsContent (b, (AsnOcts*)v);
        break;

      case TBL_NULL:
          tmpBytesEncoded += BEncAsnNullContent (b, (AsnNull*)v);
        break;

      case TBL_OID:
          tmpBytesEncoded += BEncAsnOidContent (b, (AsnOid*)v);
        break;

      case TBL_REAL:
          tmpBytesEncoded += BEncAsnRealContent (b, (AsnReal*)v);
        break;

      default:
         retVal = -1;
    }

    if (retVal >= 0)
        retVal = TblEncodeTagsAndLens (tblT, b, implicit, &tmpBytesEncoded);

    (*bytesEncoded) += tmpBytesEncoded;

    return retVal;

}  /* TblEncodeTd */


int
TblEncodeTagsAndLens PARAMS ((tblT, b, implicit, bytesEncoded),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    unsigned long int *bytesEncoded)
{
    TBLTag *tag;

    FOR_EACH_LIST_ELMT_RVS (tag, tblT->tagList)
    {
        if (!(implicit && (tag == FIRST_LIST_ELMT (tblT->tagList))))
        {
            if (tag->form == CONS)
                (*bytesEncoded) += BEncConsLen (b, *bytesEncoded);
            else /* ANY_FORM or PRIM */
                (*bytesEncoded) += BEncDefLen (b, *bytesEncoded);

            (*bytesEncoded) += TblEncTag (b, tag);
        }
    }
    return 0; /* no errors */
} /* TblEncodeTagsAndLens */

/*
int
TblEncodePrimTagsAndLens PARAMS ((tblT, b, implicit, bytesEncoded),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    unsigned long int *bytesEncoded)
{
    TBLTag *tag;
    int tagsEncoded;
    int numTags;

    if ((tblT->tagList == NULL) ||(LIST_EMPTY (tblT->tagList)))
        untaggedPrimG = TRUE;
    else
    {
        numTags = LIST_COUNT (tblT->tagList);
        if ((numTags != 1) || !implicit)
        {
            untaggedPrimG = FALSE;
            tag = FIRST_LIST_ELMT (tblT->tagList);
            (*bytesEncoded) += BEncDefLen (b, *bytesEncoded);
            (*bytesEncoded) += TblEncTag (b, tag->tclass, PRIM, tag->code);
            tagsEncoded = 1;
            SET_CURR_LIST_NODE (tblT->tagList, LAST_LIST_NODE (tblT->tagList)->prev);
            FOR_REST_LIST_ELMT_RVS (tag, tblT->tagList)
            {
                if (implicit && (tagsEncoded == (numTags -1)))
                    break;
                (*bytesEncoded) += BEncConsLen (b, *bytesEncoded);
                (*bytesEncoded) += TblEncTag (b, tag->tclass, CONS, tag->code);
                tagsEncoded++;
            }
        }
        else
            untaggedPrimG = TRUE;
    }
    return 0;
} TblEncodeTagsAndLens */


/*
 * write encoded version of tag stored in the tag to
 * the bufer
 */
AsnLen TblEncTag PARAMS ((b, tag),
    BUF_TYPE b _AND_
    TBLTag *tag)
{
    AsnTag shifted;
    unsigned char octet;
    AsnLen encLen = 0;
    int i;

    for (i = 0; i < sizeof (AsnTag); i++)
    {
        shifted = (tag->encTag >> (i * 8));
	octet =  shifted & 0xff;
        if (octet || i<sizeof(AsnTag)-2 && (shifted & 0x8000))
        {
            encLen++;
            BufPutByteRvs (b, octet);
        }

    }
    return encLen;
} /* TblEncTag */


/* OLD NOT USED ANY MORE
  returns encoded length of tag
AsnLen TblEncTag PARAMS ((b, tclass, form, code),
    BUF_TYPE b _AND_
    TBLTagClass tclass _AND_
    BER_FORM form _AND_
    AsnInt code)
{
    AsnLen retVal;
    BER_CLASS  bclass;

    bclass = TblTagClassToBer (tclass);

     warning: keep the BEncTagX calls in braces ({}) cause macros
    if (code < 31)
    {
        retVal = BEncTag1 (b, bclass, form, code);
    }
    else if (code < 128)
    {
        retVal = BEncTag2 (b, bclass, form, code);
    }
    else if (code < 16384)
    {
        retVal = BEncTag3 (b, bclass, form, code);
    }
    else if (code < 2097152)
    {
        retVal = BEncTag4 (b, bclass, form, code);
    }
    else
    {
        retVal = BEncTag5 (b, bclass, form, code);
    }
    return retVal;
}
*/

#endif /* TTBL */
