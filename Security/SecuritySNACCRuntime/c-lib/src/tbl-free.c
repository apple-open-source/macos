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
 * tbl_free.c - frees data structs returned by type table driven decoder.
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


void
TblFree PARAMS ((tbl, modName, typeName, v),
    TBL *tbl _AND_
    char *modName _AND_
    char *typeName _AND_
    AVal *v)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;

    tblTd = TblFindTypeDef (tbl, modName, typeName, &tblMod);
    if (tblTd == NULL)
    {
        TblError ("TblFree: Could not find a type definition with the given module and name");
    }

    TblFreeType (tblTd->type, v);
}  /* TblDecode p*/


void
TblFreeType PARAMS ((tblT, v),
    TBLType *tblT _AND_
    AVal *v)
{
    AVal *elmtVPtr;
    unsigned int currElmt;
    TBLType *listElmtType;
    TBLType *structElmtType;
    TBLType *choiceElmtType;
    AChoiceVal *cVal;
    AStructVal *sVal;
    AsnList *lVal;
    void *tmp;


    switch (tblT->typeId)
    {
      case TBL_TYPEREF:
          TblFreeType (tblT->content->a.typeRef->typeDefPtr->type, v);
          break;

      case TBL_SEQUENCE:
      case TBL_SET:
          sVal = (AStructVal*)v;
          currElmt = 0;
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT (structElmtType, tblT->content->a.elmts)
          {
              if (!((structElmtType->optional) && (sVal[currElmt] == NULL)))
                  TblFreeType (structElmtType, sVal[currElmt]);
              currElmt++;
          }
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          Asn1Free (v);
          break;


      case TBL_SEQUENCEOF:
      case TBL_SETOF:
          listElmtType = FIRST_LIST_ELMT (tblT->content->a.elmts);
          lVal  = (AsnList*)v;
          FOR_EACH_LIST_ELMT (elmtVPtr, lVal)
          {
              TblFreeType (listElmtType, elmtVPtr);
          }
          AsnListFree (lVal);
        break;

      case TBL_CHOICE:
          cVal = (AChoiceVal*)v;
          choiceElmtType = (TBLType*)GetAsnListElmt (tblT->content->a.elmts, cVal->choiceId);
          TblFreeType (choiceElmtType, cVal->val);
          Asn1Free (cVal);
        break;

      case TBL_BOOLEAN:
          FreeAsnBool ((AsnBool*)v);
          Asn1Free (v);
        break;

      case TBL_INTEGER:
      case TBL_ENUMERATED:
          FreeAsnInt ((AsnInt*)v);
          Asn1Free (v);
        break;

      case TBL_BITSTRING:
          FreeAsnBits ((AsnBits*)v);
          Asn1Free (v);
        break;

      case TBL_OCTETSTRING:
          FreeAsnOcts ((AsnOcts*)v);
          Asn1Free (v);
        break;

      case TBL_NULL:
          FreeAsnNull ((AsnNull*)v);
          Asn1Free (v);
        break;

      case TBL_OID:
          FreeAsnOid ((AsnOid*)v);
          Asn1Free (v);
        break;

      case TBL_REAL:
          FreeAsnReal ((AsnReal*)v);
          Asn1Free (v);
        break;

      default:
         break;
    }

}  /* TblFreeType */

#endif /* TTBL */
