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
 * tbl_dec.c - type table decoder.
 *
 *
 * Mike Sample
 *
 * Copyright (C) 1993 Michael Sample
 *            and the University of British Columbia
 *
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

int  TagsMatch PROTO ((TBLType *tblT, AsnTag asnTag));
void TblPopTagsAndLens PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, unsigned long int *bytesDecoded, ENV_TYPE env));

void TblDecodeTagsAndLens PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, ENV_TYPE env));

int CountMandatoryElmts PROTO ((TBLType *tblT));


typedef struct TagNLen
{
    AsnTag tag;
    AsnLen len;
    unsigned int size; /* encoded len for this tag len pair */
} TagNLen;

#define TL_STK_SIZE 128
static TagNLen tlStkG[TL_STK_SIZE];
static int nextFreeTLG = 0;

#define PUSH_TL(t,l,sz, env)\
  { if (nextFreeTLG >= TL_STK_SIZE)\
      longjmp (env, -1000);\
    tlStkG[nextFreeTLG].tag = t;\
    tlStkG[nextFreeTLG].len = l;\
    tlStkG[nextFreeTLG++].size = sz; }

#define POP_TL(env)\
  { nextFreeTLG--;\
    if (nextFreeTLG < 0)\
        longjmp (env, -1001);}

#define LAST_TAG() (tlStkG[nextFreeTLG-1].tag)
#define LAST_LEN() (tlStkG[nextFreeTLG-1].len)
#define LAST_SIZE() (tlStkG[nextFreeTLG-1].size)



AVal*
TblDecode PARAMS ((tbl, modName, typeName, b, bytesDecoded),
    TBL *tbl _AND_
    char *modName _AND_
    char *typeName _AND_
    BUF_TYPE b _AND_
    unsigned long int *bytesDecoded)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;
    ENV_TYPE env;
    AVal *retVal;
    int val;

    tblTd = TblFindTypeDef (tbl, modName, typeName, &tblMod);
    if (tblTd == NULL)
    {
        TblError ("TblDecode: Could not find a type definition with the given module and name");
        return NULL;
    }
    *bytesDecoded = 0;

    if ((val = setjmp (env)) == 0)
    {
        retVal = TblDecodeType (tblTd->type, b, FALSE, bytesDecoded, env);
    }
    else
        retVal = NULL;

    if (val != 0)
        fprintf (stderr,"ack! longjmp error number: %d\n", val);

    return retVal;
}  /* TblDecode p*/


AVal*
TblDecodeType PARAMS ((tblT, b, implicit, bytesDecoded, env),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    unsigned long int *bytesDecoded _AND_
    ENV_TYPE env)
{
    AVal *elmtVPtr;
    unsigned long int tmpBytesDecoded = 0;
    unsigned int currElmt;
    TBLType *listElmtType;
    TBLType *structElmtType;
    TBLType *choiceElmtType;
    AChoiceVal *cVal;
    AStructVal *sVal;
    AVal *retVal;
    AVal **tmpHndl;
    AsnTag asnTag;
    int i, mandatoryCount, mandatoryElmts;
    int implicitRef;
    void *tmp;


    TblDecodeTagsAndLens (tblT, b, implicit, env);

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

          retVal = TblDecodeType (tblT->content->a.typeRef->typeDefPtr->type, b, implicitRef, &tmpBytesDecoded, env);
          break;

      case TBL_SEQUENCE:
          /* go fwd though elmt type list */
          currElmt = 0;
          sVal = (AStructVal*) Asn1Alloc (sizeof (AVal*)*
                                        LIST_COUNT (tblT->content->a.elmts));
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT (structElmtType, tblT->content->a.elmts)
          {
              if (TagsMatch (structElmtType, PeekTag (b,env)))
              {
                  sVal[currElmt] = TblDecodeType (structElmtType, b, FALSE, &tmpBytesDecoded, env);
              }
              else if (!structElmtType->optional)
                  longjmp (env,-1008);

              currElmt++;
          }
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          retVal = (AVal*) sVal;
          break;

      case TBL_SET:
          sVal = (AStructVal*) Asn1Alloc (sizeof (AVal*)*
                                        LIST_COUNT (tblT->content->a.elmts));
          mandatoryCount = 0;
          mandatoryElmts = CountMandatoryElmts (tblT);
          if (LAST_LEN() == INDEFINITE_LEN)
              while (!PeekEoc (b))
              {
                  asnTag = PeekTag (b,env);
                  currElmt = 0;
                  /* find elmt that matches the peeked tag */
                  FOR_EACH_LIST_ELMT (structElmtType, tblT->content->a.elmts)
                  {
                      if (TagsMatch (structElmtType, asnTag))
                          break;
                      currElmt++;
                  }

                  /* didn't find a match */
                  if (currElmt >= LIST_COUNT (tblT->content->a.elmts))
                      longjmp (env,-1009);

                  if (!structElmtType->optional)
                      mandatoryCount++;

                  sVal[currElmt] = TblDecodeType (structElmtType, b, FALSE, &tmpBytesDecoded, env);
              }
          else
              while (tmpBytesDecoded < LAST_LEN())
              {
                  asnTag = PeekTag (b,env);
                  currElmt = 0;
                  /* find elmt that matches the peeked tag */
                  FOR_EACH_LIST_ELMT (structElmtType, tblT->content->a.elmts)
                  {
                      if (TagsMatch (structElmtType, asnTag))
                          break;
                      currElmt++;
                  }

                  if (currElmt >= LIST_COUNT (tblT->content->a.elmts))
                      longjmp (env, -1007);

                  if (!structElmtType->optional)
                      mandatoryCount++;

                  sVal[currElmt] = TblDecodeType (structElmtType, b, FALSE, &tmpBytesDecoded, env);
              }
          if (mandatoryCount != mandatoryElmts)
              longjmp (env,-1006);
          else
              retVal = sVal;

        break;


      case TBL_SEQUENCEOF:
      case TBL_SETOF:
          retVal = (AsnList*)Asn1Alloc (sizeof (AsnList));
          listElmtType = FIRST_LIST_ELMT (tblT->content->a.elmts);

          if (LAST_LEN() == INDEFINITE_LEN)
              while (!PeekEoc (b))
              {
                  elmtVPtr = TblDecodeType (listElmtType, b, FALSE, &tmpBytesDecoded, env);
                  tmpHndl = AsnListAppend ((AsnList*)retVal);
                  *tmpHndl = elmtVPtr;
              }
          else
              while (tmpBytesDecoded < LAST_LEN())
              {
                  elmtVPtr = TblDecodeType (listElmtType, b, FALSE, &tmpBytesDecoded, env);
                  tmpHndl = AsnListAppend ((AsnList*)retVal);
                  *tmpHndl = elmtVPtr;
              }

        break;

      case TBL_CHOICE:
          retVal = cVal = (AChoiceVal*) Asn1Alloc (sizeof (AChoiceVal));
          asnTag = PeekTag (b,env);
          i = 0;
          /* find elmt that matches the peeked tag */
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT (choiceElmtType, tblT->content->a.elmts)
          {
              if (TagsMatch (choiceElmtType, asnTag))
              {
                  cVal->choiceId = i;
                  break;
              }
              i++;
          }
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          cVal->val = TblDecodeType (choiceElmtType, b, FALSE, &tmpBytesDecoded, env);
        break;

      case TBL_BOOLEAN:
          retVal = Asn1Alloc (sizeof (AsnBool));
          BDecAsnBoolContent (b, LAST_TAG(), LAST_LEN(), (AsnBool*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_INTEGER:
      case TBL_ENUMERATED:
          retVal = Asn1Alloc (sizeof (AsnInt));
          BDecAsnIntContent (b, LAST_TAG(), LAST_LEN(), (AsnInt*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_BITSTRING:
          retVal = Asn1Alloc (sizeof (AsnBits));
          BDecAsnBitsContent (b, LAST_TAG(), LAST_LEN(), (AsnBits*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_OCTETSTRING:
          retVal = Asn1Alloc (sizeof (AsnOcts));
          BDecAsnOctsContent (b, LAST_TAG(), LAST_LEN(), (AsnOcts*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_NULL:
          retVal = Asn1Alloc (sizeof (AsnNull));
          BDecAsnNullContent (b, LAST_TAG(), LAST_LEN(), (AsnNull*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_OID:
          retVal = Asn1Alloc (sizeof (AsnOid));
          BDecAsnOidContent (b, LAST_TAG(), LAST_LEN(), (AsnOid*) retVal, &tmpBytesDecoded, env);
        break;

      case TBL_REAL:
          retVal = Asn1Alloc (sizeof (AsnReal));
          BDecAsnRealContent (b, LAST_TAG(), LAST_LEN(), (AsnReal*) retVal, &tmpBytesDecoded, env);
        break;

      default:
         retVal = NULL;
         break;
    }

    TblPopTagsAndLens (tblT, b, implicit, &tmpBytesDecoded, env);

    (*bytesDecoded) += tmpBytesDecoded;

    return retVal;

}  /* TblDecodeType */


void
TblDecodeTagsAndLens PARAMS ((tblT, b, implicit, env),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    ENV_TYPE env)
{
    AsnTag tag;
    AsnLen len;
    AsnLen encSize;
    TBLTag *tblTag;

    if ((tblT->tagList == NULL) || (LIST_EMPTY (tblT->tagList)))
        return;

    SET_CURR_LIST_NODE (tblT->tagList, FIRST_LIST_NODE (tblT->tagList));
    if (implicit)
    {
       SET_CURR_LIST_NODE (tblT->tagList, NEXT_LIST_NODE (tblT->tagList));
    }


    FOR_REST_LIST_ELMT (tblTag, tblT->tagList)
    {
        encSize = 0;
        tag = BDecTag (b, &encSize, env);
        len = BDecLen (b, &encSize, env);

        if (!TagsEquiv (tag, tblTag))
            longjmp (env, -1002);

        PUSH_TL (tag, len, encSize, env);
    }
} /* TblDecodeTagsAndLens */

/*
 * bytesDecoded should hold the length of the content that
 * was just decoded.  This verifies the lengths as it pops
 * them off the stack.  Also decodes EOCs.
 */
void
TblPopTagsAndLens PARAMS ((tblT, b, implicit, bytesDecoded, env),
    TBLType *tblT _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    unsigned long int *bytesDecoded _AND_
    ENV_TYPE env)
{
    TBLTag *tblTag;

    FOR_EACH_LIST_ELMT_RVS (tblTag, tblT->tagList)
    {
        if (implicit && (tblTag == FIRST_LIST_ELMT (tblT->tagList)))
            break;

        if (LAST_LEN() == INDEFINITE_LEN)
            BDecEoc (b, bytesDecoded, env);
        else if (*bytesDecoded != LAST_LEN())
            longjmp (env, -1003);

        (*bytesDecoded) += LAST_SIZE();
        POP_TL (env);
    }
} /* TblPopTagsAndLens */


int TagsMatch PARAMS ((tblT, asnTag),
    TBLType *tblT _AND_
    AsnTag asnTag)
{
    TBLType *tmpTblT;
    TBLType *elmtTblT;
    TBLTag *tblTag;
    void *tmp;

    /*
     * skip through type refs until encounter first tag or
     * untagged CHOICE (only TYPEREFs and CHOICEs can
     * have empty tag lists).
     */
    for (tmpTblT = tblT; ((tmpTblT->typeId == TBL_TYPEREF) &&
               ((tmpTblT->tagList == NULL) || LIST_EMPTY (tmpTblT->tagList)));
         tmpTblT = tmpTblT->content->a.typeRef->typeDefPtr->type);


    /*
     * if untagged CHOICE must check for a match with the first tag
     * of each component of the CHOICE
     */
    if ((tmpTblT->typeId == TBL_CHOICE) &&
        ((tmpTblT->tagList == NULL) || LIST_EMPTY (tmpTblT->tagList)))
    {
        tmp = CURR_LIST_NODE (tmpTblT->content->a.elmts);
        FOR_EACH_LIST_ELMT (elmtTblT, tmpTblT->content->a.elmts)
        {
            /*
             * remember the elmt type can be an untagged choice too
             * so call TagsMatch again.
             */
            if (TagsMatch (elmtTblT, asnTag))
            {
                SET_CURR_LIST_NODE (tmpTblT->content->a.elmts, tmp);
                return TRUE;  /* match in choice */
            }
        }
        SET_CURR_LIST_NODE (tmpTblT->content->a.elmts, tmp);
        return FALSE; /* no match in choice */
    }
    else /* is type other than untagged choice or type ref */
    {
        tblTag =  FIRST_LIST_ELMT (tmpTblT->tagList);
        return TagsEquiv (asnTag, tblTag);
    }
} /* TagsMatch */


int
CountMandatoryElmts PARAMS ((tblT),
    TBLType *tblT)
{
    TBLType *tblElmtT;
    int count = 0;
    FOR_EACH_LIST_ELMT (tblElmtT, tblT->content->a.elmts)
    {
        if (!tblElmtT->optional)
            count++;
    }
    return count;
} /* CountMandatoryElmts */

#endif /* TTBL */
