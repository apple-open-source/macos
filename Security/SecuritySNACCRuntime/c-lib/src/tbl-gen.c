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
#include <stdio.h>
#include "tbl-gen.h"

typedef struct TagNLen
{
    AsnTag tag;
    AsnLen len;
    unsigned int size; /* encoded len for this tag len pair */
} TagNLen;

#define TL_STK_SIZE 128

typedef struct STDEDecoder
{
    TBL* tbl;
    BUF_TYPE b;
    ENV_TYPE env;
    TagNLen tlStk[TL_STK_SIZE];
    int nTlStk;
    int rewindsize;
    TdeTypeProc typeproc;
    TdeSimpleProc simpleproc;
    TdeExcProc excproc;
} * TDEDecoder;


#define TDEEXCEPTION(dec,code,p1,p2,p3) if ((dec)->excproc) if ((*(dec)->excproc)(code,p1,p2,p3)) longjmp((dec)->env,-236)

#define TDEERRORMSG(dec,msg) TDEEXCEPTION(dec,TDEERROR,msg,NULL,NULL)

#define TDEWARNUNEXPECTED(dec,type,elmtType) TDEEXCEPTION(dec,TDEUNEXPECTED,type,elmtType,NULL)
#define TDEWARNNONOPTIONAL(dec,type,elmtType) TDEEXCEPTION(dec,TDENONOPTIONAL,type,elmtType,NULL)
#define TDEWARNMANDATORY(dec,type) TDEEXCEPTION(dec,TDEMANDATORY,type,NULL,NULL)
#define TDEWARNCONSTRAINT(dec,type,cons,val) TDEEXCEPTION(dec,TDECONSTRAINT,type,cons,&val)
#define TDEWARNNOMATCH(dec,type,typetag,tag) TDEEXCEPTION(dec,TDENOMATCH,type,&typetag,&tag)

#define TDEINFOEOC(dec) TDEEXCEPTION(dec,TDEEOC,NULL,NULL,NULL)
#define TDEINFOPEEKTAG(dec,tag) TDEEXCEPTION(dec,TDEPEEKTAG,&tag,NULL,NULL)
#define TDEINFOPUSHTAG(dec,tag,len,size) TDEEXCEPTION(dec,TDEPUSHTAG,&tag,&len,&size)

#define TDETYPE(dec,type,val,begin) if (dec->typeproc) if ((*dec->typeproc)(type,val,begin)) longjmp(dec->env,-234)
#define TDESIMPLE(dec,tag,octs,begin) if (dec->simpleproc) if ((*dec->simpleproc)(tag,octs,begin)) longjmp(dec->env,-235)

#define LAST_TAG() (dec->tlStk[dec->nTlStk-1-dec->rewindsize].tag)
#define LAST_LEN() (dec->tlStk[dec->nTlStk-1-dec->rewindsize].len)
#define LAST_SIZE() (dec->tlStk[dec->nTlStk-1-dec->rewindsize].size)

AsnTag
TDEPeekTag PARAMS ((dec),
    TDEDecoder dec)
{
    AsnTag tag;
    if (dec->rewindsize)
    	tag = dec->tlStk[dec->nTlStk-dec->rewindsize].tag;
    else
    	tag = PeekTag(dec->b,dec->env);
    TDEINFOPEEKTAG(dec,tag);
    return tag;
}

AsnTag
TDEPushTag PARAMS ((dec),
    TDEDecoder dec)
{
    if (dec->rewindsize)
    	dec->rewindsize--;
    else
    {
        unsigned long encSize = 0;
        if (dec->nTlStk >= TL_STK_SIZE)
            longjmp (dec->env, -1000);
    	dec->tlStk[dec->nTlStk].tag = BDecTag (dec->b, &encSize, dec->env);
	dec->tlStk[dec->nTlStk].len = BDecLen (dec->b, &encSize, dec->env);
	dec->tlStk[dec->nTlStk++].size = encSize;
	TDEINFOPUSHTAG(dec,LAST_TAG(),LAST_LEN(),LAST_SIZE());
    }
    return LAST_TAG();
}

void 
TDEDoPop PARAMS ((dec),
    TDEDecoder dec)
{
    dec->nTlStk--;
    if (dec->nTlStk < 0)
        longjmp (dec->env, -1001);
}

void
TDEPopTag PARAMS ((dec, bytesDecoded),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded)
{
    if (LAST_LEN()==INDEFINITE_LEN)
    {
        BDecEoc (dec->b, bytesDecoded, dec->env);
        TDEINFOEOC(dec);
    }
    else if (*bytesDecoded != LAST_LEN())
    {
        TDEERRORMSG(dec,"Lost BER synchronisation");
        longjmp (dec->env, -1003);
    }
    (*bytesDecoded) += LAST_SIZE();
    TDEDoPop(dec);
}

void
TDECheckConstraint PARAMS ((dec, type, constraint, value),
    TDEDecoder dec _AND_
    TBLType* type _AND_
    TBLRange* constraint _AND_
    AsnInt value)
{
    if (constraint && (value<constraint->from || value>constraint->to))
    	TDEWARNCONSTRAINT(dec,type,constraint,value);
}

int
TDEInTag PARAMS ((dec, bytesDecodedInTag),
    TDEDecoder dec _AND_
    unsigned long int bytesDecodedInTag)
{
    return LAST_LEN()==INDEFINITE_LEN? !PeekEoc(dec->b): (bytesDecodedInTag<LAST_LEN());
}

int
TDECountMandatoryElmts PARAMS ((type),
    TBLType *type)
{
    TBLType *elmtType;
    int count = 0;
    FOR_EACH_LIST_ELMT (elmtType, type->content->a.elmts)
    {
        if (!elmtType->optional)
            count++;
    }
    return count;
}

void 
TDESimpleDecode PARAMS ((dec, bytesDecoded),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded)
{
    AsnTag tag = TDEPushTag(dec);
    unsigned long int bytesDecodedInTag = 0;

    BER_CLASS tclass = TAG_ID_CLASS(tag);
    BER_FORM form = TAG_ID_FORM(tag);
    unsigned long int code = tag & 0x1FFFFFFF;
    BER_UNIV_CODE bcode;
    if (tclass==UNIV)
	bcode = code>>24;
    else
	bcode = OCTETSTRING_TAG_CODE;
	
    if (TAG_IS_CONS(tag))
    {
    	TDESIMPLE(dec,tag,NULL,1);
        while (TDEInTag(dec,bytesDecodedInTag))
        {
    	    TDESimpleDecode (dec, &bytesDecodedInTag);
        }
        TDESIMPLE(dec,tag,NULL,0);
    }
    else
    {
    	PrintableString v;
    	switch (bcode)
    	{
    	case INTEGER_TAG_CODE:
    	case OCTETSTRING_TAG_CODE:
    	default:
    	    v.octetLen = LAST_LEN();
    	    v.octs = Asn1Alloc(v.octetLen);
    	    BufCopy(v.octs,dec->b,v.octetLen);
    	    TDESIMPLE(dec,tag,&v,1);
    	    Asn1Free(v.octs);
    	    break;
    	}
    	bytesDecodedInTag += LAST_LEN();
    }
    TDEPopTag(dec,&bytesDecodedInTag);
    *bytesDecoded += bytesDecodedInTag;
}

int
TDEPushTagsAndLens PARAMS ((dec, type, implicit),
    TDEDecoder dec _AND_
    TBLType *type _AND_
    int implicit)
{
    AsnTag tag;
    AsnLen len;
    AsnLen encSize;
    TBLTag *tblTag;
    int fullMatch = TRUE;
    int origTLG = dec->nTlStk;
    int origRewindsize = dec->rewindsize;

    if ((type->tagList == NULL) || (LIST_EMPTY (type->tagList)))
        return TRUE;

    SET_CURR_LIST_NODE (type->tagList, FIRST_LIST_NODE (type->tagList));
    if (implicit)
    {
       SET_CURR_LIST_NODE (type->tagList, NEXT_LIST_NODE (type->tagList));
    }

    FOR_REST_LIST_ELMT (tblTag, type->tagList)
    {
    	tag = TDEPushTag(dec);
        if (!TagsEquiv (tag, tblTag))
        {
            /*
             * Whoops! The expected tags do not completely fit! So what to do?
             *
             * This is a complicated situation since might have already read some
             * tags from the buffer (and pushed), but now we should return  failure
             * AND REWIND TO THE STATE WE WERE IN WHEN CALLED,
             * so that future PeekTag and then TblDecodeTagsAndLens calls start
             * off there again!
             * 
             * The idea is to modify PeekTag and this routine to first check
             * whether there is information pending that was read already.
             *
             * Luckily, this can not happen recursively, only in sequence:
             * ... -> ...
             * ... -> Tags fit -> Tags fit -> ...
             * ... -> Tags fit -> Tags fit -> ...
             *                    Tags fit -> ...
             *                    Tags don't fit -<
             *                    Tags don't fit -<
             *                    Complete subtype decoding remaining tags in simple manner
             *                 <-
             *        Tags don't fit -<
             *        Tags fit -> ...
             *        Complete subtype decoding remaining tags in simple manner
             *        <-
             * ...
             */
            fullMatch = FALSE;
            dec->rewindsize = origRewindsize + dec->nTlStk - origTLG;
            TDEWARNNOMATCH(dec,type,tblTag->encTag,tag);
            break;
        }
    }
    if (fullMatch)
    	dec->rewindsize = 0;
    return fullMatch;
}

void
TDEPopTagsAndLens PARAMS ((dec, bytesDecoded, type, implicit),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded _AND_
    TBLType *type _AND_
    int implicit)
{
    TBLTag *tblTag;
    if (dec->rewindsize)
    	TDEERRORMSG(dec,"Still rewinding at end of tag");
    FOR_EACH_LIST_ELMT_RVS (tblTag, type->tagList)
    {
        if (implicit && (tblTag == FIRST_LIST_ELMT (type->tagList)))
            break;
    	TDEPopTag(dec,bytesDecoded);
    }
}

int 
TDETagsMatch PARAMS ((type, asnTag),
    TBLType *type _AND_
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
    for (tmpTblT = type; ((tmpTblT->typeId == TBL_TYPEREF) &&
               ((tmpTblT->tagList == NULL) || LIST_EMPTY (tmpTblT->tagList)));
         )
         tmpTblT = tmpTblT->content->a.typeRef->typeDefPtr->type;

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
	int result;
        tblTag =  FIRST_LIST_ELMT (tmpTblT->tagList);
        result = TagsEquiv (asnTag, tblTag);
	return result;
    }
}

int
TDEDecodeType PARAMS ((dec, bytesDecoded, type, implicit, constraint),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded _AND_
    TBLType *type _AND_
    int implicit _AND_
    TBLRange* constraint)
{
    AVal *elmtVPtr;
    unsigned long int tmpBytesDecoded = 0;
    unsigned int currElmt;
    TBLType *elmtType;
    AVal *retVal = NULL;
    AVal *eleVal;
    AVal **tmpHndl;
    AsnTag asnTag;
    int i, mandatoryCount, mandatoryElmts;
    int implicitRef;
    void *tmp;
    AsnInt value;
    char* constraintmsg = NULL;
    int elmtfound;
    int indefinite;
    
    if (!TDEPushTagsAndLens (dec, type, implicit))
        return FALSE;
        
#if TTBL>1
    if (!constraint)
	constraint = type->constraint;
#endif

    TDETYPE(dec,type,NULL,1);

    switch (type->typeId)
	{
	  case TBL_TYPEREF:
              /*
               * carry over implicit ref if goes
               * through typeref with no tags
               */
              implicitRef = type->content->a.typeRef->implicit ||
        	  (implicit &&
        	   ((type->tagList == NULL) || LIST_EMPTY (type->tagList)));

              if (!TDEDecodeType (dec, &tmpBytesDecoded,
            	    type->content->a.typeRef->typeDefPtr->type,
              	    implicitRef, constraint))
              {
        	  TDEWARNUNEXPECTED(dec,type,type->content->a.typeRef->typeDefPtr->type);
        	  TDESimpleDecode(dec, &tmpBytesDecoded);
              }
              break;

	  case TBL_SEQUENCE:
              /* go fwd though elmt type list */
              tmp = CURR_LIST_NODE (type->content->a.elmts);
              FOR_EACH_LIST_ELMT (elmtType, type->content->a.elmts)
              {
                  elmtfound = FALSE;
        	  while (!elmtfound 
			  && TDEInTag(dec,tmpBytesDecoded)
			  && TDETagsMatch (elmtType, TDEPeekTag (dec)))
                      elmtfound = TDEDecodeType (dec,&tmpBytesDecoded,
                      	    elmtType, FALSE, NULL);
        	  if (!elmtfound && !elmtType->optional)
                      TDEWARNNONOPTIONAL(dec,type,elmtType);
              }
              SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
              
              /* process remaining stuff in sequence */
              while (TDEInTag(dec,tmpBytesDecoded))
        	  {
        	  TDEWARNUNEXPECTED(dec,type,NULL);
        	  TDESimpleDecode(dec, &tmpBytesDecoded);
        	  }
              break;

	  case TBL_SET:
              mandatoryCount = 0;
              mandatoryElmts = TDECountMandatoryElmts (type);
              while (TDEInTag(dec,tmpBytesDecoded))
        	  {
                  asnTag = TDEPeekTag (dec);
                  elmtfound = FALSE;
                 /* find elmt that matches the peeked tag */
                  tmp = CURR_LIST_NODE (type->content->a.elmts);
                  FOR_EACH_LIST_ELMT (elmtType,
                      	  type->content->a.elmts)
                  {
                      if (TDETagsMatch (elmtType, asnTag))
                      {
                          elmtfound = TRUE;
                          break;
                      }
                  }
                  SET_CURR_LIST_NODE (type->content->a.elmts, tmp);

                  /* didn't find a match */
                  if (!elmtfound || !TDEDecodeType (dec, &tmpBytesDecoded, 
                    	     elmtType, FALSE, NULL))
                      {
                      TDEWARNUNEXPECTED(dec,type,elmtfound?elmtType:NULL);
                      TDESimpleDecode(dec, &tmpBytesDecoded);
                      }
                  else
                      {
                      if (!elmtType->optional)
                      	  mandatoryCount++;
                      }

        	  }
              if (mandatoryCount != mandatoryElmts)
              	  TDEWARNMANDATORY(dec,type);
            break;


	  case TBL_SEQUENCEOF:
	  case TBL_SETOF:
              elmtType = FIRST_LIST_ELMT (type->content->a.elmts);
	      constraintmsg = "Size of SEQUENCE/SET OF";
	      value = 0;

              while (TDEInTag(dec,tmpBytesDecoded))
              {
                  if (!TDEDecodeType (dec, &tmpBytesDecoded, elmtType, 
                      	      FALSE,NULL))
                      {
                      TDEWARNUNEXPECTED(dec,type,elmtType);
                      TDESimpleDecode(dec, &tmpBytesDecoded);
                      }
                  else
                      value++;
              }
            break;

	  case TBL_CHOICE:
              elmtfound = FALSE;
	      if (TDEInTag(dec,tmpBytesDecoded)) 
	      {
		  asnTag = TDEPeekTag (dec);
		  /* find elmt that matches the peeked tag */
		  tmp = CURR_LIST_NODE (type->content->a.elmts);
		  FOR_EACH_LIST_ELMT (elmtType, type->content->a.elmts)
		  {
		      if (TDETagsMatch (elmtType, asnTag))
		      {
			  elmtfound = TRUE;
			  break;
		      }
		  }
	      }
              SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
              if (!elmtfound || !TDEDecodeType (dec, &tmpBytesDecoded, 
                    	 elmtType, FALSE, NULL))
                  {
                  TDEWARNUNEXPECTED(dec,type,elmtfound?elmtType:NULL);
                  TDESimpleDecode(dec, &tmpBytesDecoded);
                  }
            break;

	  case TBL_BOOLEAN:
              retVal = Asn1Alloc (sizeof (AsnBool));
              BDecAsnBoolContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnBool*) retVal, &tmpBytesDecoded, dec->env);
            break;

	  case TBL_INTEGER:
	  case TBL_ENUMERATED:
              retVal = Asn1Alloc (sizeof (AsnInt));
              BDecAsnIntContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnInt*) retVal, &tmpBytesDecoded, dec->env);
	      constraintmsg = "INTEGER/ENUMERATED";
	      value = *(AsnInt*)retVal;
            break;

	  case TBL_BITSTRING:
              retVal = Asn1Alloc (sizeof (AsnBits));
              BDecAsnBitsContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnBits*) retVal, &tmpBytesDecoded, dec->env);
            break;

	  case TBL_OCTETSTRING:
              retVal = Asn1Alloc (sizeof (AsnOcts));
              BDecAsnOctsContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnOcts*) retVal, &tmpBytesDecoded, dec->env);
	      constraintmsg = "Length of OCTET STRING";
	      value = ((AsnOcts*)retVal)->octetLen;
	   break;

	  case TBL_NULL:
              retVal = Asn1Alloc (sizeof (AsnNull));
              BDecAsnNullContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnNull*) retVal, &tmpBytesDecoded, dec->env);
            break;

	  case TBL_OID:
              retVal = Asn1Alloc (sizeof (AsnOid));
              BDecAsnOidContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnOid*) retVal, &tmpBytesDecoded, dec->env);
            break;

	  case TBL_REAL:
              retVal = Asn1Alloc (sizeof (AsnReal));
              BDecAsnRealContent (dec->b, LAST_TAG(), LAST_LEN(),
		    (AsnReal*) retVal, &tmpBytesDecoded, dec->env);
	   break;

	  default:
             retVal = NULL;
             break;
	}

    TDETYPE(dec,type,retVal,0);
    if (retVal)
    	Asn1Free(retVal);

    if (constraintmsg)
	TDECheckConstraint(dec,type,constraint,value);

    TDEPopTagsAndLens (dec, &tmpBytesDecoded, type, implicit);
    (*bytesDecoded) += tmpBytesDecoded;
    return TRUE;
}

int
TDEDecodeSpecific PARAMS ((dec, bytesDecoded, type),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded _AND_
    TBLType* type)
{
    int val;
    
    *bytesDecoded = 0;
    dec->rewindsize = 0;
    dec->nTlStk = 0;
    
    if ((val = setjmp (dec->env)) == 0)
    {
        TDEDecodeType (dec, bytesDecoded, type, FALSE, NULL);
        return TRUE;
    }
    return FALSE;
}

int
TDEDecodeUnknown PARAMS ((dec, bytesDecoded),
    TDEDecoder dec _AND_
    unsigned long int *bytesDecoded)
{
    TBLModule *tblMod = NULL;
    TBLTypeDef *tblTd = NULL;
    
    *bytesDecoded = 0;

    FOR_EACH_LIST_ELMT (tblMod, dec->tbl->modules)
	break;
    if (!tblMod)
    {
        TDEERRORMSG (dec,"No module in grammar");
	return FALSE;
    }

    FOR_EACH_LIST_ELMT_RVS (tblTd, tblMod->typeDefs)
	break;
    if (!tblTd)
    {
        TDEERRORMSG (dec,"No type in first module of grammar");
        return FALSE;
    }
    
    return TDEDecodeSpecific (dec, bytesDecoded, tblTd->type);
}

struct STDEDecoder sdec;

void
TDEErrorHandler PARAMS ((str, severity),
    char* str _AND_
    int severity)
{
    TDEERRORMSG(&sdec,str);
}

int 
TdeDecodeSpecific PARAMS ((tbl, b, type, bytesDecoded, typeproc, simpleproc, excproc),
    TBL *tbl _AND_
    BUF_TYPE b _AND_
    TBLType* type _AND_
    unsigned long int *bytesDecoded _AND_
    TdeTypeProc typeproc _AND_
    TdeSimpleProc simpleproc _AND_
    TdeExcProc excproc)
{
    int result;
    Asn1ErrorHandler former = Asn1InstallErrorHandler(TDEErrorHandler);
    sdec.tbl = tbl;
    sdec.b = b;
    sdec.typeproc = typeproc;
    sdec.simpleproc = simpleproc;
    sdec.excproc = excproc;
    result = TDEDecodeSpecific(&sdec,bytesDecoded,type);
    Asn1InstallErrorHandler(former);
    return result;
}

int 
TdeDecode PARAMS ((tbl, b, bytesDecoded, typeproc, simpleproc, excproc),
    TBL *tbl _AND_
    BUF_TYPE b _AND_
    unsigned long int *bytesDecoded _AND_
    TdeTypeProc typeproc _AND_
    TdeSimpleProc simpleproc _AND_
    TdeExcProc excproc)
{
    int result;
    Asn1ErrorHandler former = Asn1InstallErrorHandler(TDEErrorHandler);
    sdec.tbl = tbl;
    sdec.b = b;
    sdec.typeproc = typeproc;
    sdec.simpleproc = simpleproc;
    sdec.excproc = excproc;
    result = TDEDecodeUnknown(&sdec,bytesDecoded);
    Asn1InstallErrorHandler(former);
    return result;
}
#endif
