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
 * tbl_util.c - type table utilities.
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
#include "sbuf.h"

/* non -exported routine protos */
void TblLinkIndexes PROTO ((TBL *tbl));
void TblLinkTypeRefs PROTO ((TBL *tbl, TBLType *tblT));

void TblFixTags PROTO ((TBL *tbl));
void TblFixTypeTags PROTO ((TBLType *tblT));
void TblSetTagForms PROTO ((TBLType *t));


/*
 * opens given filename, determines its size, allocs a block
 * of that size and reads the file into it. returns a pointer
 * to this block.  Prints an err msgs is something screwed up
 * and returns NULL.  Sets the size param to the size of the file.
 */
char*
LoadFile PARAMS ((fileName, size),
    char *fileName _AND_
    unsigned long int *size)
{
    FILE *f;
    unsigned long int fsize;
    char *fileData;

    f = fopen (fileName, "r");

    if (f == NULL)
    {
        Asn1Error("Could not open file for reading.\n");
        return NULL;
    }

    fseek (f, 0, 2);    /* seek to end */
    fsize = ftell (f);  /* get size of file */
    fseek (f, 0, 0);    /* seek to beginning */

    *size = fsize;
    fileData = (char *) malloc (fsize);

    if (fileData == NULL)
    {
        Asn1Error("Not enough memory to read in file.\n");
        return NULL;
    }

    if (fread (fileData, sizeof (char), fsize, f) != fsize)
    {
        free (fileData);
        fileData = NULL;
        Asn1Error("Trouble reading file.\n");
    }

    fclose (f);
    return fileData;
}  /* LoadFile */


TBL*
LoadTblFile PARAMS ((tblFileName),
    char *tblFileName)
{
    SBuf sb;
    SBuf *sbPtr;
    GenBuf gb;
    TBL *tbl;
    unsigned long int fsize;
    char *fileData;
    AsnLen decodedLen;
    ENV_TYPE env;
    int val;


    fileData = LoadFile (tblFileName, &fsize);
    if (fileData == NULL)
        return NULL;

    SBufInstallData (&sb, fileData, fsize);
    SBufResetInReadMode (&sb);
    PutSBufInGenBuf (&sb, &gb);

    decodedLen = 0;

    tbl = (TBL*)Asn1Alloc (sizeof (TBL));

    if ((val = setjmp (env)) == 0)
        BDecTBL (&gb, tbl, &decodedLen, env);
    else
        return NULL;

    /* convert the typeDefIndexes into real pointers */
    TblLinkIndexes (tbl);

    TblFixTags (tbl);

    free (fileData); /* malloc'd in LoadFile */

    return tbl;
}


/*
 * just use slow individual lookup instead of creating a table
 * (a conversion tbl could be built during decoding)
 */
void
TblLinkIndexes PARAMS ((tbl),
    TBL *tbl)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;

    FOR_EACH_LIST_ELMT (tblMod, tbl->modules)
    {
        FOR_EACH_LIST_ELMT (tblTd, tblMod->typeDefs)
        {
            /* go through the types looking for TBLTypeRefs */
            TblLinkTypeRefs (tbl, tblTd->type);
        }
    }
} /* TBLLinkIndexes */


/*
 * set tags forms and include encoded version to improve
 * decoding and encoding performance.
 */
void
TblFixTags PARAMS ((tbl),
    TBL *tbl)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;

    FOR_EACH_LIST_ELMT (tblMod, tbl->modules)
    {
        FOR_EACH_LIST_ELMT (tblTd, tblMod->typeDefs)
        {
            TblFixTypeTags (tblTd->type);
        }
    }
} /* TBLFixTags */



/*
 * recursively descends type looking for typeDefIds in type refs
 * to convert to the type defs actual ptr
 *
 * Also sets the form field for each tag. (this speeds up enc/dec).
 * Note that the form bit is not in the encoded version of a TBLTag.
 */
void
TblLinkTypeRefs PARAMS ((tbl, tblT),
    TBL *tbl _AND_
    TBLType *tblT)
{
    TBLType *tblElmtT;
    void *tmp;

    switch (tblT->typeId)
    {
      case TBL_BOOLEAN:
      case TBL_INTEGER:
      case TBL_BITSTRING:
      case TBL_OCTETSTRING:
      case TBL_NULL:
      case TBL_OID:
      case TBL_REAL:
      case TBL_ENUMERATED:
          /* not contained type refs so return */
          break;

      case TBL_SEQUENCE:
      case TBL_SET:
      case TBL_SEQUENCEOF:
      case TBL_SETOF:
      case TBL_CHOICE:
          /* look for contained type refs */
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT (tblElmtT, tblT->content->a.elmts)
          {
              TblLinkTypeRefs (tbl, tblElmtT);
          }
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          break;

      case TBL_TYPEREF:
          /* convert type def index into a pointer to the type def */
          tblT->content->a.typeRef->typeDefPtr =
              TblFindTypeDefByIndex (tbl, tblT->content->a.typeRef->typeDef);
          break;
    }
} /* TblLinkTypeRefs */
void
TblFixTypeTags PARAMS ((tblT),
    TBLType *tblT)
{
    void *tmp;
    TBLType *tblElmtT;

    TblSetTagForms (tblT);
    switch (tblT->typeId)
    {
      case TBL_SEQUENCE:
      case TBL_SET:
      case TBL_SEQUENCEOF:
      case TBL_SETOF:
      case TBL_CHOICE:
          /* fix tags in elmt types */
          tmp = CURR_LIST_NODE (tblT->content->a.elmts);
          FOR_EACH_LIST_ELMT (tblElmtT, tblT->content->a.elmts)
          {
              TblFixTypeTags (tblElmtT);
          }
          SET_CURR_LIST_NODE (tblT->content->a.elmts, tmp);
          break;

      default:
        break;
   }
}

void
TblSetTagForms PARAMS ((tblT),
    TBLType *tblT)
{
    TBLTag *tblTag;
    TBLType *tmpTblT;
    int numTags;
    TBLTypeId tid;
    BER_FORM form;

    if (tblT->tagList == NULL)
        return;

    numTags = LIST_COUNT (tblT->tagList);

    /*
     * get real type id (skip through type refs)
     * count total number of tags too.
     */
    for (tmpTblT = tblT; tmpTblT->typeId == TBL_TYPEREF; tmpTblT = tmpTblT->content->a.typeRef->typeDefPtr->type)
    {
	if (tmpTblT->tagList)
	    numTags += LIST_COUNT (tmpTblT->tagList);
        if (tmpTblT->content->a.typeRef->implicit)
            numTags--;
    }
    tid = tmpTblT->typeId;

    /* only traverse this types tags */
    FOR_EACH_LIST_ELMT (tblTag, tblT->tagList)
    {
        if (numTags > 1)
            form = tblTag->form = CONS;
        else
            switch (tid)
            {
              case TBL_SEQUENCE:
              case TBL_SET:
              case TBL_SEQUENCEOF:
              case TBL_SETOF:
              case TBL_CHOICE:
                  form = tblTag->form = CONS;
                  break;

              case TBL_OCTETSTRING:
              case TBL_BITSTRING:
                  tblTag->form = ANY_FORM;
                  form = PRIM; /* store as prim (for encoder - always prim) */
                  break;

              default:
                  form = tblTag->form = PRIM;
                  break;
          }

        tblTag->encTag = MAKE_TAG_ID (TblTagClassToBer (tblTag->tclass), form, tblTag->code);
        numTags--;
    }
} /* TblSetTagForms */



TBLTypeDef*
TblFindTypeDef PARAMS ((tbl, modName, typeName, tblModHndl),
    TBL *tbl _AND_
    char *modName _AND_
    char *typeName _AND_
    TBLModule **tblModHndl)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;
    void *tmp;

    /* look in named module only if given */
    if (modName != NULL)
    {
        tblMod = TblFindModule (tbl, modName);
        *tblModHndl = tblMod;
        if (tblMod == NULL)
            return NULL;

        return TblFindTypeDefInMod (tblMod, typeName);
    }
    else /* look in all modules and return first instance */
    {
        tmp = CURR_LIST_NODE (tbl->modules);
        FOR_EACH_LIST_ELMT (tblMod, tbl->modules)
        {
            tblTd = TblFindTypeDefInMod (tblMod, typeName);
            if (tblTd != NULL)
            {
                *tblModHndl = tblMod;
                SET_CURR_LIST_NODE (tbl->modules, tmp);
                return tblTd;
            }
        }
        SET_CURR_LIST_NODE (tbl->modules, tmp);
    }
    return NULL; /* not found */
} /* TblFindTypeDef */


TBLTypeDef*
TblFindTypeDefInMod PARAMS ((tblMod, typeName),
    TBLModule *tblMod _AND_
    char *typeName)
{
    TBLTypeDef *tblTd;
    void *tmp;

    tmp = CURR_LIST_NODE (tblMod->typeDefs);
    FOR_EACH_LIST_ELMT (tblTd, tblMod->typeDefs)
    {
        if (strcmp (tblTd->typeName.octs, typeName) == 0)
        {
            SET_CURR_LIST_NODE (tblMod->typeDefs, tmp);
            return tblTd;
        }
    }
    SET_CURR_LIST_NODE (tblMod->typeDefs, tmp);
    return NULL;
} /* TblFindTypeDefInMod */


TBLTypeDef*
TblFindTypeDefByIndex PARAMS ((tbl, id),
    TBL *tbl _AND_
    TBLTypeDefId id)
{
    TBLModule *tblMod;
    TBLTypeDef *tblTd;
    void *tmp1;
    void *tmp2;

     /* look in all modules and return typedef with given id */
    tmp1 = CURR_LIST_NODE (tbl->modules);
    FOR_EACH_LIST_ELMT (tblMod, tbl->modules)
    {
        tmp2 = CURR_LIST_NODE (tblMod->typeDefs);
        FOR_EACH_LIST_ELMT (tblTd, tblMod->typeDefs)
        {
            if (tblTd->typeDefId == id)
            {
                SET_CURR_LIST_NODE (tblMod->typeDefs, tmp2);
                SET_CURR_LIST_NODE (tbl->modules, tmp1);
                return tblTd;
            }
        }
        SET_CURR_LIST_NODE (tblMod->typeDefs, tmp2);
    }
    SET_CURR_LIST_NODE (tbl->modules, tmp1);

    return NULL;
} /* TblFindTypeDefByIndex */


TBLModule*
TblFindModule PARAMS ((tbl, modName),
    TBL *tbl _AND_
    char *modName)
{
    TBLModule *tblMod;
    void *tmp;

    tmp = CURR_LIST_NODE (tbl->modules);
    FOR_EACH_LIST_ELMT (tblMod, tbl->modules)
    {
        if (strcmp (tblMod->name.octs, modName) == 0)
        {
            SET_CURR_LIST_NODE (tbl->modules, tmp);
            return tblMod;
        }
    }
    SET_CURR_LIST_NODE (tbl->modules, tmp);
    return NULL;

} /* TblFindModule */

#endif /* TTBL */
