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


/*
 * compiler/back-ends/c-gen/type-info.c  - fills in c type information
 *
 * MS 91/92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/Attic/type-info.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: type-info.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:44  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:47:45  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:26:44  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:42  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <ctype.h>
#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "snacc-util.h"
#include "define.h"
#include "str-util.h"
#include "rules.h"
#include "type-info.h"


extern Module *usefulTypeModG;
static DefinedObj *definedNamesG;
/*
 *  All Typedefs, union,struct & enum Tags, and definedvalues (enum consts)
 *  are assumed to share the same name space - this list is used to
 *  assure uniqueness. (actually 4 name spaces in C - see pg 227 KR 2nd Ed)
 */


/* unexported prototypes */

void FillCTypeDefInfo PROTO ((CRules *r,  Module *m,  TypeDef *td));

static void FillCFieldNames PROTO ((CRules *r, NamedTypeList *firstSibling));

static void FillCTypeRefInfo PROTO ((CRules *r,  Module *m,  TypeDef *head, Type *t, CTypeId parentTypeId));

static void FillCStructElmts PROTO ((CRules *r,  Module *m,  TypeDef *head, NamedTypeList *t));

static void FillCChoiceElmts PROTO ((CRules *r, Module *m, TypeDef *head, NamedTypeList *first));

static int IsCPtr PROTO ((CRules *r, TypeDef *td, Type *t, CTypeId parentTypeId));


void ParseTypeDefAttribs PROTO ((CTDI *ctdi, AttributeList *attrList));
void ParseTypeRefAttribs PROTO ((CTRI *ctri, AttributeList *attrList));
void ParseAttr PROTO ((char *str, int *startLoc, char **attrName, char **attrValue));

int ParseBool PROTO ((char *str, int *result));
int ParseInt PROTO ((char *str, int *result));
int ParseCTypeId PROTO ((char *str, int *result));

void FillCTDIDefaults PROTO ((CRules *r, CTDI *ctdi, TypeDef *td));


/*
 *  allocates and fills all the "cTypeDefInfo" for each type def
 *  and "cTypeRefInfo" for each type in the given modules.
 *  Also does the useful types module if it is not null.
 */
void
FillCTypeInfo PARAMS ((r, modList),
    CRules *r _AND_
    ModuleList *modList)
{
    TypeDef *td;
    Module *m;

    /*
     * go through each module's type defs and fill
     * in the C type and enc/dec routines etc
     */
    definedNamesG = NewObjList();

    /* do useful types first */
    if (usefulTypeModG != NULL)
    {
        FOR_EACH_LIST_ELMT (td, usefulTypeModG->typeDefs)
            FillCTypeDefInfo (r, usefulTypeModG, td);
    }

    FOR_EACH_LIST_ELMT (m, modList)
    {
        FOR_EACH_LIST_ELMT (td, m->typeDefs)
            FillCTypeDefInfo (r, m, td);
    }

    /*
     * now that type def info is filled in
     * set up set/seq/list/choice elements that ref
     * those definitions
     */

    /* do useful types first */
    if (usefulTypeModG != NULL)
    {
        FOR_EACH_LIST_ELMT (td, usefulTypeModG->typeDefs)
            FillCTypeRefInfo (r, usefulTypeModG, td, td->type, C_TYPEDEF);
    }

    FOR_EACH_LIST_ELMT (m, modList)
    {
        FOR_EACH_LIST_ELMT (td, m->typeDefs)
            FillCTypeRefInfo (r, m, td, td->type, C_TYPEDEF);
    }

    /*
     * modules compiled together (ie one call to snacc with
     * multiple args) likely to be C compiled together so
     * need a unique routines/types/defines/enum values
     * since assuming they share same name space.
     *  All Typedefs, union, struct & enum Tags, and defined values
     * (enum consts), #define names
     *  are assumed to share the same name space
     */

    FreeDefinedObjs (&definedNamesG);

}  /* FillCTypeInfo */


/*
 *  allocates and fills structure holding C type definition information
 *  fo the given ASN.1 type definition.  Does not fill CTRI for contained
 *  types etc.
 */
void
FillCTypeDefInfo PARAMS ((r, m, td),
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td)
{
    int digit;
    int len;
    char *tmpName;
    CTDI *ctdi;

    /*
     * if CTDI is present this type def has already been 'filled'
     */
    if (td->cTypeDefInfo != NULL)
        return;

    ctdi = td->cTypeDefInfo = MT (CTDI);
    ctdi->cTypeId = C_TYPEDEF;

    /* get default type def attributes from table for type on rhs of ::= */

    FillCTDIDefaults (r, ctdi, td);


    /*
     * if defined by a ref to another type definition fill in that type
     * def's CTDI so can inherit (actully completly replace default
     * attributes) from it
     */
    if ((td->type->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
        (td->type->basicType->choiceId == BASICTYPE_IMPORTTYPEREF))
    {
        /*
         * Fill in CTDI for defining type if nec.
         * this works for importTypeRef as well since both a.localTypeRef
         * and a.importTypeRef are of type TypeRef
         */
        FillCTypeDefInfo (r, td->type->basicType->a.localTypeRef->module, td->type->basicType->a.localTypeRef->link);

        memcpy (ctdi, td->type->basicType->a.localTypeRef->link->cTypeDefInfo, sizeof (CTDI));
    }


    /*
     * Zap default names for routines/type with NULL so
     * can determine if the --snacc attributes specified any
     */
    ctdi->cTypeName = NULL;
    ctdi->printRoutineName = NULL;
    ctdi->encodeRoutineName = NULL;
    ctdi->decodeRoutineName = NULL;
    ctdi->freeRoutineName = NULL;


    /*
     * check for any "--snacc" attributes that overide the current
     * ctdi fields
     */
    ParseTypeDefAttribs (ctdi, td->attrList);


    /*
     * generate c typename for this  type def if not given by
     * --snacc attributes
     */
    if (ctdi->cTypeName == NULL)
    {
        tmpName = Asn1TypeName2CTypeName (td->definedName);
        len = strlen (tmpName);
        ctdi->cTypeName = Malloc (len + r->maxDigitsToAppend + 1);
        strcpy (ctdi->cTypeName, tmpName);
        Free (tmpName);

       /*
        * make sure c type def name is unique
        * (no need to check if cTypeName was specified by --snacc attribs)
        */
       MakeCStrUnique (definedNamesG, ctdi->cTypeName,r->maxDigitsToAppend, 1);
       DefineObj (&definedNamesG, ctdi->cTypeName);
    }


    /*
     * make names for encoder,decoder, print and free routines
     * (if not already set by --snacc attributes
     */
    if (ctdi->encodeRoutineName == NULL)
    {
        ctdi->encodeRoutineName =
            Malloc (strlen (ctdi->cTypeName) + strlen (r->encodeRoutineBaseName)
                   + 1);
        strcpy (ctdi->encodeRoutineName, r->encodeRoutineBaseName);
        strcat (ctdi->encodeRoutineName, ctdi->cTypeName);
    }

    if (ctdi->decodeRoutineName == NULL)
    {
        ctdi->decodeRoutineName =
            Malloc (strlen (ctdi->cTypeName) + strlen (r->decodeRoutineBaseName) + 1);
        strcpy (ctdi->decodeRoutineName, r->decodeRoutineBaseName);
        strcat (ctdi->decodeRoutineName, ctdi->cTypeName);
    }

    if (ctdi->printRoutineName == NULL)
    {
        ctdi->printRoutineName =
            Malloc (strlen (ctdi->cTypeName) + strlen (r->printRoutineBaseName) + 1);
        strcpy (ctdi->printRoutineName, r->printRoutineBaseName);
        strcat (ctdi->printRoutineName, ctdi->cTypeName);
    }

    if (ctdi->freeRoutineName == NULL)
    {
        ctdi->freeRoutineName =
            Malloc (strlen (ctdi->cTypeName) + strlen (r->freeRoutineBaseName) + 1);
        strcpy (ctdi->freeRoutineName, r->freeRoutineBaseName);
        strcat (ctdi->freeRoutineName, ctdi->cTypeName);
    }

}  /* FillCTypeDefInfo */


static void
FillCTypeRefInfo PARAMS ((r, m, head, t, parentTypeId),
    CRules *r _AND_
    Module *m _AND_
    TypeDef *head _AND_
    Type *t _AND_
    CTypeId parentTypeId)
{
    char	*typeStr;
    CTRI	*ctri;
    CTDI	*tmpCtdi;
    ValueDef	*namedElmt;
    CNamedElmt	*cne;
    CNamedElmt	**cneHndl;
    char	*elmtName;
    char	*listName;
    char	*choiceName;
    char	*unionName;
    Type	*tmpT;
    int		len, digit;
    enum BasicTypeChoiceId basicTypeId;

    /*
     * you must check for cycles yourself before calling this
     */
    if (t->cTypeRefInfo == NULL)
    {
        ctri = MT (CTRI);
        t->cTypeRefInfo = ctri;
    }
    else
        ctri =  t->cTypeRefInfo;

    basicTypeId = t->basicType->choiceId;

    tmpCtdi = &r->typeConvTbl[basicTypeId];

    /* get base type def info from the conversion table in the rules */
    /* if the cTypeId is C_LIB, nothing more needs to be done */
    ctri->cTypeId = tmpCtdi->cTypeId;
    ctri->cTypeName = tmpCtdi->cTypeName;
    ctri->optTestRoutineName = tmpCtdi->optTestRoutineName;
    ctri->printRoutineName = tmpCtdi->printRoutineName;
    ctri->encodeRoutineName = tmpCtdi->encodeRoutineName;
    ctri->decodeRoutineName = tmpCtdi->decodeRoutineName;
    ctri->freeRoutineName = tmpCtdi->freeRoutineName;
    ctri->isEncDec = tmpCtdi->isEncDec;


    if (ctri->cTypeId == C_ANY)
    {
        fprintf (stderr,"Warning - generated code for the \"ANY\" type in type \"%s\" will need modification by YOU.", head->definedName);
        fprintf (stderr,"  The source files will have a \"/* ANY - Fix Me! */\" comment before related code.\n\n");
     }

    /*
     * convert named elmts to C.
     * check for name conflict with other defined Types/Names/Values
     */
    if ((basicTypeId == BASICTYPE_INTEGER || basicTypeId == BASICTYPE_ENUMERATED || basicTypeId == BASICTYPE_BITSTRING) && !(LIST_EMPTY (t->basicType->a.integer)))
    {
        ctri->cNamedElmts = AsnListNew (sizeof (void *));
        FOR_EACH_LIST_ELMT (namedElmt, t->basicType->a.integer)
        {
            cneHndl = (CNamedElmt **)AsnListAppend (ctri->cNamedElmts);
            cne = *cneHndl = MT (CNamedElmt);
            elmtName = Asn1ValueName2CValueName (namedElmt->definedName);
            len = strlen (elmtName);
            cne->name = Malloc (len + 1 + r->maxDigitsToAppend);
            strcpy (cne->name, elmtName);
            Free (elmtName); /* not very efficient */

            if (namedElmt->value->basicValue->choiceId == BASICVALUE_INTEGER)
                cne->value = namedElmt->value->basicValue->a.integer;
            else
            {
                fprintf (stderr,"Warning: unlinked defined value using -9999999\n");
                cne->value = -9999999;
            }

            if (r->capitalizeNamedElmts)
                Str2UCase (cne->name, len);

            /*
             * append digits until there is not name conflict
             * if nec
             */
            MakeCStrUnique (definedNamesG, cne->name, r->maxDigitsToAppend, 1);
            DefineObj (&definedNamesG, cne->name);
        }
    }

    /*
     *  Fill in c type name, routines, ptr attibutes etc
     */
    if (r->typeConvTbl[basicTypeId].cTypeId == C_TYPEREF)
    {
        /*
         * don't do this anymore - it cause problems since FillTypeDef
         * changes name ie ORName -> ORName1 and other type use new name
         *
         * don't define type or print/enc/dec/free routines
         * if typedef name is the same as the defining type ref name
         * in P2: ORName ::= P1.ORName
        if ((parentTypeId == C_TYPEDEF) &&
            (strcmp (head->definedName, t->basicType->a.localTypeRef->typeName)
             == 0))
        {
            tmpCtdi = head->cTypeDefInfo;
            tmpCtdi->genPrintRoutine = FALSE;
            tmpCtdi->genEncodeRoutine = FALSE;
            tmpCtdi->genDecodeRoutine = FALSE;
            tmpCtdi->genFreeRoutine = FALSE;
            tmpCtdi->genTypeDef = FALSE;
        }
         */

        /*
         * grab type name from link (link is the def of the
         * the ref'd type)
         */
        if (t->basicType->a.localTypeRef->link != NULL)
        {
            /* inherit attributes from referenced type */
            tmpCtdi=  t->basicType->a.localTypeRef->link->cTypeDefInfo;
            ctri->cTypeName = tmpCtdi->cTypeName;
            ctri->printRoutineName  = tmpCtdi->printRoutineName;
            ctri->encodeRoutineName = tmpCtdi->encodeRoutineName;
            ctri->decodeRoutineName = tmpCtdi->decodeRoutineName;
            ctri->freeRoutineName = tmpCtdi->freeRoutineName;
            ctri->isEncDec = tmpCtdi->isEncDec;
            ctri->optTestRoutineName = tmpCtdi->optTestRoutineName;

        }
        else
        {
            /*
             * guess type and routine names
             */
            fprintf (stderr,"Assuming C Type and Routine names for unresolved type ref \"%s\"\n",t->basicType->a.localTypeRef->typeName);

            ctri->cTypeName = Asn1TypeName2CTypeName (t->basicType->a.localTypeRef->typeName);

            ctri->printRoutineName = Malloc (strlen (r->printRoutineBaseName) + strlen (ctri->cTypeName) + 1);
            strcpy (ctri->printRoutineName, r->printRoutineBaseName);
            strcat (ctri->printRoutineName, ctri->cTypeName);

            ctri->encodeRoutineName = Malloc (strlen (r->encodeRoutineBaseName)+ strlen (ctri->cTypeName) +  1);
            strcpy (ctri->encodeRoutineName, r->encodeRoutineBaseName);
            strcat (ctri->encodeRoutineName, ctri->cTypeName);

            ctri->decodeRoutineName = Malloc (strlen (r->decodeRoutineBaseName)+ strlen (ctri->cTypeName) + 1);
            strcpy (ctri->decodeRoutineName, r->decodeRoutineBaseName);
            strcat (ctri->decodeRoutineName, ctri->cTypeName);

            ctri->freeRoutineName = Malloc (strlen (ctri->cTypeName) + strlen (r->freeRoutineBaseName) + 1);
            strcpy (ctri->freeRoutineName, r->freeRoutineBaseName);
            strcat (ctri->freeRoutineName, ctri->cTypeName);
        }

    }

    else if (r->typeConvTbl[basicTypeId].cTypeId == C_LIST)
    {
        /*
         * List  types (SET OF/ SEQ OF)
         * fill in component type
         */

        FillCTypeRefInfo (r, m, head, t->basicType->a.setOf, C_LIST);
    }

    else if (r->typeConvTbl[basicTypeId].cTypeId == C_CHOICE)
    {
        /*
         * Choice - set up choice Id elmt names, choiceid enum name
         * choiceid enum fieldName, choice union name.
         * this will only be the first type in the typedef
         * ie will not be embedded (those are turned into type
         * refs in nomalize.c)
         */

        /*
         * make union name (tag) from enclosing typedefs name plus "Choice"
         * put in the cTypeName part. (the typeDef name is already unique
         * but make sure union tag/name does not conflict with other types)
         */
        len = strlen (head->cTypeDefInfo->cTypeName);
        unionName = (char*) Malloc (len + strlen (r->choiceUnionDefSuffix) + r->maxDigitsToAppend + 1);
        strcpy (unionName, head->cTypeDefInfo->cTypeName);
        strcat (unionName, r->choiceUnionDefSuffix);
        MakeCStrUnique (definedNamesG, unionName,  r->maxDigitsToAppend, 1);
        DefineObj (&definedNamesG, unionName);
        ctri->cTypeName = unionName;

        ctri->choiceIdEnumName = Malloc (len + strlen (r->choiceIdEnumSuffix) + r->maxDigitsToAppend + 1);
        strcpy (ctri->choiceIdEnumName, head->cTypeDefInfo->cTypeName);
        strcat (ctri->choiceIdEnumName, r->choiceIdEnumSuffix);
        MakeCStrUnique (definedNamesG, ctri->choiceIdEnumName,  r->maxDigitsToAppend, 1);
        DefineObj (&definedNamesG, ctri->choiceIdEnumName);

        ctri->choiceIdEnumFieldName = r->choiceIdFieldName; /* "choiceId" */
        ctri->cFieldName = r->choiceUnionFieldName;         /* "a" */

        /*
         * must fill field names BEFORE filling choice elmts
         * (allows better naming for choice ids
         */
        FillCFieldNames (r, t->basicType->a.choice);
        FillCChoiceElmts (r, m, head, t->basicType->a.choice);

    }

    else if (r->typeConvTbl[basicTypeId].cTypeId == C_STRUCT)
    {
        /*
         * SETs and SEQUENCEs
         */

        /*
         * make struct name (tag) (the typeDef name is already unique)
         * the same as the enclosing typeDef
         */
        unionName = Malloc (strlen (head->cTypeDefInfo->cTypeName) +1);
        strcpy (unionName, head->cTypeDefInfo->cTypeName);
        ctri->cTypeName = unionName;

        FillCStructElmts (r, m, head, t->basicType->a.set);
        FillCFieldNames (r, t->basicType->a.set);
    }

    /*
     * figure out whether this is a ptr based on the enclosing
     * type (if any) and optionality/default
     */
    ctri->isPtr = IsCPtr (r, head, t, parentTypeId);

    /* let user overide any defaults with the --snacc attributes */
    ParseTypeRefAttribs (ctri, t->attrList);


}  /* FillCTypeRefInfo */



static void
FillCStructElmts PARAMS ((r, m, head, elmts),
    CRules *r _AND_
    Module *m _AND_
    TypeDef *head _AND_
    NamedTypeList *elmts)
{
    NamedType *et;

    FOR_EACH_LIST_ELMT (et, elmts)
    {
        FillCTypeRefInfo (r, m, head, et->type, C_STRUCT);
    }

}  /* FillCStructElmts */



/*
 *  Figures out non-conflicting enum names for the
 *  choice id's
 */
static void
FillCChoiceElmts PARAMS ((r, m, head, elmts),
    CRules *r _AND_
    Module *m _AND_
    TypeDef *head _AND_
    NamedTypeList *elmts)
{
    NamedType *et;
    int idCount = 0;
    CTRI *ctri;
    char *firstName;
    char *secondName;
    int   len;

    /*
     * fill in type info for elmt types first
     */
    FOR_EACH_LIST_ELMT (et, elmts)
        FillCTypeRefInfo (r, m, head, et->type, C_CHOICE);

    /*
     * set choiceId Symbol & value
     * eg
     *  Car ::= CHOICE {          typedef struct Car {
     *     chev ChevCar,   ->         enum CarChoiceId {
     *     ford FordCar                  CAR_CHEV,  <- typename_fieldName
     *     toyota ToyotaCar              CAR_FORD,
     *     }                             CAR_TOYOTA } choiceId;
     *                                union CarChoiceUnion {
     *                                      ChevCar *chev;
     *                                      FordCar *ford;
     *                                      ToyotaCar *toyota; } a;
     *                               }
     */
    FOR_EACH_LIST_ELMT (et, elmts)
    {
        ctri =  et->type->cTypeRefInfo;

        if (ctri == NULL)
            continue; /* wierd type */

        ctri->choiceIdValue = idCount++;

        firstName = Asn1TypeName2CTypeName (head->cTypeDefInfo->cTypeName);
        secondName = ctri->cFieldName;
        ctri->choiceIdSymbol = Malloc (strlen (firstName) + strlen (secondName) + 2 + r->maxDigitsToAppend);
        strcpy (ctri->choiceIdSymbol, firstName);
        strcat (ctri->choiceIdSymbol, "_");
        strcat (ctri->choiceIdSymbol, secondName);
        Free (firstName);
        len = strlen (ctri->choiceIdSymbol);

        if (r->capitalizeNamedElmts)
            Str2UCase (ctri->choiceIdSymbol, len);

        MakeCStrUnique (definedNamesG, ctri->choiceIdSymbol, r->maxDigitsToAppend, 0);
        DefineObj (&definedNamesG, ctri->choiceIdSymbol);
    }

}  /* FillCChoiceElmts */


/*
 * takes a list of "sibling" (eg same level in a structure)
 * ElmtTypes and fills sets up the c field names in
 * the CTypeRefInfo struct
 */
static void
FillCFieldNames PARAMS ((r, elmts),
    CRules *r _AND_
    NamedTypeList *elmts)
{
    NamedType  *et;
    CTRI *ctri;
    DefinedObj *fieldNames;
    int        len, num, digit, i, tmpLen;
    char      *tmpName;
    char      *asn1FieldName;
    char      *cFieldName;

    fieldNames = NewObjList();

    /*
     * Initialize fieldname data
     * allocate (if nec) and fill in CTRI fieldname if poss
     * from asn1 field name.  leave blank otherwise
     */
    FOR_EACH_LIST_ELMT (et, elmts)
    {
        ctri =  et->type->cTypeRefInfo;
        if (ctri == NULL)
        {
            ctri = MT (CTRI);
            et->type->cTypeRefInfo = ctri;
        }
        if (et->fieldName != NULL)
        {
            asn1FieldName = et->fieldName;
            ctri->cFieldName = Asn1FieldName2CFieldName (asn1FieldName);
            DefineObj (&fieldNames, ctri->cFieldName);
        }
    }


    FOR_EACH_LIST_ELMT (et, elmts)
    {
        ctri =  et->type->cTypeRefInfo;

        /*
         * generate field names for those without them
         */
        if (ctri->cFieldName == NULL)
        {
            if ((et->type->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
                 (et->type->basicType->choiceId == BASICTYPE_IMPORTTYPEREF))
            {
                /*
                 * take ref'd type name as field name
                 * convert first let to lower case
                 */
                tmpName = et->type->basicType->a.localTypeRef->link->cTypeDefInfo->cTypeName;
                tmpName =  Asn1TypeName2CTypeName (tmpName);
                cFieldName = Malloc (strlen (tmpName) + r->maxDigitsToAppend +1);
                strcpy (cFieldName, tmpName);
                Free (tmpName);
                if (isupper (cFieldName[0]))
                    cFieldName[0] = tolower (cFieldName[0]);
            }
            else
            {
                /*
                 * get default field name for this type
                 */
                tmpName = r->typeConvTbl[et->type->basicType->choiceId].defaultFieldName;
                cFieldName = Malloc (strlen (tmpName) + r->maxDigitsToAppend +1);
                strcpy (cFieldName, tmpName);

                if (isupper (cFieldName[0]))
                    cFieldName[0] = tolower (cFieldName[0]);
            }


            MakeCStrUnique (fieldNames, cFieldName, r->maxDigitsToAppend, 1);

            DefineObj (&fieldNames, cFieldName);
            ctri->cFieldName = cFieldName;
        }
    }
    FreeDefinedObjs (&fieldNames);
}  /* FillCFieldNames */



/*
 * returns true if this c type for this type should be
 * be ref'd as a ptr
 */
static int
IsCPtr PARAMS ((r, td, t, parentCTypeId),
    CRules *r _AND_
    TypeDef *td _AND_
    Type *t _AND_
    CTypeId parentCTypeId)
{
    CTDI *ctdi;
    int retVal = FALSE;

    /*
     * inherit ptr attriubutes from ref'd type if any
     * otherwise grab lib c type def from the CRules
     */
    if ((t->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
        (t->basicType->choiceId == BASICTYPE_IMPORTTYPEREF))
    {
        ctdi = t->basicType->a.localTypeRef->link->cTypeDefInfo;
    }
    else
        ctdi = &r->typeConvTbl[GetBuiltinType (t)];

    if ((parentCTypeId == C_TYPEDEF) && (ctdi->isPtrForTypeDef))
        retVal = TRUE;

    else if ((parentCTypeId == C_STRUCT) && (ctdi->isPtrForTypeRef))
        retVal = TRUE;

    else if ((parentCTypeId == C_CHOICE) && (ctdi->isPtrInChoice))
        retVal = TRUE;

    else if (((t->optional) || (t->defaultVal != NULL)) && (ctdi->isPtrForOpt))
        retVal = TRUE;

    return retVal;
}  /* IsCPtr */



#define BAD_VALUE(attrValue, attrType)\
    fprintf (stderr,"Warning: ignoring attribute with improper value (%s/%s)\n",attrType, attrValue)

/*
 * attrList is a list of strings that hold attribute value
 * pairs.  A list is used in case the attr/value pairs are
 * given in multiple ASN.1 comments around the type.
 */
void ParseTypeDefAttribs PARAMS ((ctdi, attrList),
    CTDI *ctdi _AND_
    AttributeList *attrList)
{
    char *attrName;
    char *attrValue;
    int loc;
    MyString attr;
    int result;

    if (attrList == NULL)
        return;

    FOR_EACH_LIST_ELMT (attr, attrList)
    {
        loc = 0;  /* loc is location to start/continue parse from */

        while (1)
        {
            ParseAttr (attr, &loc, &attrName, &attrValue);

            if (attrName == NULL)
               break;

            if (strcmp (attrName, "asn1TypeId") == 0)
            {
               if (ParseTypeId (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->asn1TypeId = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "cTypeId") == 0)
            {
               if (ParseCTypeId (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->cTypeId = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "cTypeName") == 0)
                ctdi->cTypeName = attrValue;

            else if (strcmp (attrName, "isPdu") == 0)
            {
               if (ParseBool (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->isPdu = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "isPtrForTypeDef") == 0)
            {
               if (ParseBool (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->isPtrForTypeDef = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "isPtrForTypeRef") == 0)
            {
               if (ParseBool (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->isPtrForTypeRef = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "isPtrInChoice") == 0)
            {
               if (ParseBool (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->isPtrInChoice = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "isPtrForOpt") == 0)
            {
               if (ParseBool (attrValue, &result) < 0)
                   BAD_VALUE (attrValue, attrName);
               else
                   ctdi->isPtrForOpt = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "optTestRoutineName") == 0)
                ctdi->optTestRoutineName = attrValue;

            else if (strcmp (attrName, "defaultFieldName") == 0)
                ctdi->defaultFieldName = attrValue;

            else if (strcmp (attrName, "printRoutineName") == 0)
                ctdi->printRoutineName = attrValue;

            else if (strcmp (attrName, "encodeRoutineName") == 0)
                ctdi->encodeRoutineName = attrValue;

            else if (strcmp (attrName, "decodeRoutineName") == 0)
                ctdi->decodeRoutineName = attrValue;

            else if (strcmp (attrName, "freeRoutineName") == 0)
                ctdi->freeRoutineName = attrValue;

            else if (strcmp (attrName, "isEncDec") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                {
                    ctdi->isEncDec = result;
                }
                Free (attrValue);
            }
            else if (strcmp (attrName, "genTypeDef") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                {
                    ctdi->genTypeDef = result;
                }
                Free (attrValue);
            }
            else if (strcmp (attrName, "genPrintRoutine") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctdi->genPrintRoutine = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "genEncodeRoutine") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctdi->genEncodeRoutine = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "genDecodeRoutine") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctdi->genDecodeRoutine = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "genFreeRoutine") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctdi->genFreeRoutine = result;
                Free (attrValue);
            }

            else
                fprintf (stderr,"Warning: ignoring unrecognized type def attribute value pair (%s/%s)\n", attrName, attrValue);

        } /* end while */
    } /* end for */

} /* ParseTypeDefAttribs */


void ParseTypeRefAttribs PARAMS ((ctri, attrList),
    CTRI *ctri _AND_
    AttributeList *attrList)
{
    char *attrName;
    char *attrValue;
    int loc;
    int result;
    MyString attr;

    if (attrList == NULL)
        return;

    FOR_EACH_LIST_ELMT (attr, attrList)
    {
        loc = 0;  /* loc is location to start/continue parse from */

        while (1)
        {
            ParseAttr (attr, &loc, &attrName, &attrValue);

            if (attrName == NULL)
               break;

            if (strcmp (attrName, "cTypeId") == 0)
            {
                if  (ParseCTypeId (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctri->cTypeId = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "cTypeName") == 0)
                ctri->cTypeName = attrValue;

            else if (strcmp (attrName, "cFieldName") == 0)
                ctri->cFieldName = attrValue;

            else if (strcmp (attrName, "isPtr") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctri->isPtr = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "choiceIdValue") == 0)
            {
                if  (ParseInt (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctri->choiceIdValue = result;
                Free (attrValue);
            }
            else if (strcmp (attrName, "choiceIdSymbol") == 0)
                    ctri->choiceIdSymbol = attrValue;

            else if (strcmp (attrName, "choiceIdEnumName") == 0)
                    ctri->choiceIdEnumName = attrValue;

            else if (strcmp (attrName, "choiceIdEnumFieldName") == 0)
                    ctri->choiceIdEnumFieldName = attrValue;

            else if (strcmp (attrName, "optTestRoutineName") == 0)
                    ctri->optTestRoutineName = attrValue;

            else if (strcmp (attrName, "printRoutineName") == 0)
                    ctri->printRoutineName = attrValue;

            else if (strcmp (attrName, "encodeRoutineName") == 0)
                    ctri->encodeRoutineName = attrValue;

            else if (strcmp (attrName, "decodeRoutineName") == 0)
                    ctri->decodeRoutineName = attrValue;

            else if (strcmp (attrName, "isEncDec") == 0)
            {
                if  (ParseBool (attrValue, &result) < 0)
                    BAD_VALUE (attrValue, attrName);
                else
                    ctri->isEncDec = result;
                Free (attrValue);
            }

            else
                fprintf (stderr,"Warning: ignoring unrecognized type ref attribute value pair (%s/%s)\n", attrName, attrValue);


        } /* end while/ per comment */
    } /* end per att str */
} /* ParseTypeRefAttribs */


int
ParseBool PARAMS ((str, result),
    char *str _AND_
    int *result)
{
   if (strcmp (str,"TRUE")==0)
   {
       *result = TRUE;
       return 0;
   }

   if (strcmp (str,"FALSE")==0)
   {
       *result = FALSE;
       return 0;
   }
   return -1;
}

int
ParseInt PARAMS ((str, result),
    char *str _AND_
    int *result)
{
    *result = atoi (str);
    return 0;
}

int
ParseCTypeId PARAMS ((str, result),
    char *str _AND_
    int *result)
{
    if (strcmp (str,"C_CHOICE"))
    {
        *result = C_CHOICE;
        return 0;
    }
    if (strcmp (str,"C_LIST"))
    {
        *result = C_LIST;
        return 0;
    }
    if (strcmp (str,"C_ANY"))
    {
        *result = C_ANY;
        return 0;
    }
    if (strcmp (str,"C_ANYDEFINEDBY"))
    {
        *result = C_ANYDEFINEDBY;
        return 0;
    }
    if (strcmp (str,"C_LIB"))
    {
        *result = C_LIB;
        return 0;
    }
    if (strcmp (str,"C_STRUCT"))
    {
        *result = C_STRUCT;
        return 0;
    }
    if (strcmp (str,"C_TYPEREF"))
    {
        *result = C_TYPEREF;
        return 0;
    }
    if (strcmp (str,"C_TYPEDEF"))
    {
        *result = C_TYPEDEF;
        return 0;
    }
    if (strcmp (str,"C_NO_TYPE"))
    {
        *result = C_NO_TYPE;
        return 0;
    }
    return -1;
}

int
ParseTypeId PARAMS ((str, result),
    char *str _AND_
    int *result)
{
    if (strcmp (str,"UNKNOWN"))
    {
        *result = BASICTYPE_UNKNOWN;
        return 0;
    }
    if (strcmp (str,"BOOLEAN"))
    {
        *result = BASICTYPE_BOOLEAN;
        return 0;
    }
    if (strcmp (str,"INTEGER"))
    {
        *result = BASICTYPE_INTEGER;
        return 0;
    }
    if (strcmp (str,"BITSTRING"))
    {
        *result = BASICTYPE_BITSTRING;
        return 0;
    }
    if (strcmp (str,"OCTETSTRING"))
    {
        *result = BASICTYPE_OCTETSTRING;
        return 0;
    }
    if (strcmp (str,"NULL"))
    {
        *result = BASICTYPE_NULL;
        return 0;
    }
    if (strcmp (str,"OID"))
    {
        *result = BASICTYPE_OID;
        return 0;
    }
    if (strcmp (str,"REAL"))
    {
        *result = BASICTYPE_REAL;
        return 0;
    }
    if (strcmp (str,"ENUMERATED"))
    {
        *result = BASICTYPE_ENUMERATED;
        return 0;
    }
    if (strcmp (str,"SEQUENCE"))
    {
        *result = BASICTYPE_SEQUENCE;
        return 0;
    }
    if (strcmp (str,"SEQUENCEOF"))
    {
        *result = BASICTYPE_SEQUENCEOF;
        return 0;
    }
    if (strcmp (str,"SET"))
    {
        *result = BASICTYPE_SET;
        return 0;
    }
    if (strcmp (str,"SETOF"))
    {
        *result = BASICTYPE_SETOF;
        return 0;
    }
    if (strcmp (str,"CHOICE"))
    {
        *result = BASICTYPE_CHOICE;
        return 0;
    }
    if (strcmp (str,"ANY"))
    {
        *result = BASICTYPE_ANY;
        return 0;
    }
    if (strcmp (str,"ANYDEFINEDBY"))
    {
        *result = BASICTYPE_ANYDEFINEDBY;
        return 0;
    }
    if (strcmp (str,"LOCALTYPEREF"))
    {
        *result = BASICTYPE_LOCALTYPEREF;
        return 0;
    }
    if (strcmp (str,"IMPORTYPEREF"))
    {
        *result = BASICTYPE_IMPORTTYPEREF;
        return 0;
    }
    return -1;
} /* ParseTypeId */


/*
 * read attribute value pair from given str starting
 * at str[loc].  Allocate and return attibute value
 * in the attrValue  parameter. The attribute name is
 * returned in the attrName parameter - do not free this
 * as it is statically defined and overwritten with
 * each call to ParseAttr.
 * str must be NULL terminated.
 *
 */
void
ParseAttr PARAMS ((str, startLoc, attrName, attrValue),
    char *str _AND_
    int *startLoc _AND_
    char **attrName _AND_
    char **attrValue)
{
    int len;
    int loc;
    int attrNameStart;
    int attrNameEnd;
    int attrValueStart;
    int attrValueEnd;
    static char retAttrName[200];
    char *retAttrValue;

    loc = *startLoc;

    len = strlen (str)-1;

    /* skip whitespc */
    for (; (loc <= len)  && str[loc] == ' '; loc++)
	;

    if (loc >= len)
    {
        *attrName = NULL;
        *attrValue = NULL;
        return;
    }

    attrNameStart = loc;

    for (; (loc <= len) && str[loc] != ':'; loc++)
	;

    if (loc > len)
    {
        *attrName = NULL;
        *attrValue = NULL;
        return;
    }

    attrNameEnd = loc-1;

    loc++; /* skip: */

    /* check for and skip "  */
    if (str[loc++] != '"')
    {
        *attrName = NULL;
        *attrValue = NULL;
        fprintf (stderr,"ERROR in snacc comment attribute string \"%s\".  Missing quote at beggining of field value\n",str);
        return;
    }

    attrValueStart = loc;

    for (; (loc <= len) && str[loc] != '"'; loc++)
	;

    attrValueEnd = loc-1;

    if ((loc > len) || (str[attrValueStart-1] != '"'))
    {
        *attrName = NULL;
        *attrValue = NULL;
        fprintf (stderr,"Parsing Error after position %d in snacc attribute string \"%s\".\n",*startLoc, str);
        return;
    }

    *startLoc = loc + 1;
    retAttrValue = (char*) Malloc (attrValueEnd - attrValueStart + 2);
    strncpy (retAttrName, &str[attrNameStart], attrNameEnd-attrNameStart+1);
    strncpy (retAttrValue, &str[attrValueStart], attrValueEnd-attrValueStart+1);
    retAttrValue[attrValueEnd-attrValueStart+1] = '\0';
    retAttrName[attrNameEnd-attrNameStart+1] = '\0';

    *attrName = retAttrName;
    *attrValue = retAttrValue;
} /* ParseAttr */



/* fill given ctdi with defaults from table for given typedef */
void
FillCTDIDefaults PARAMS ((r, ctdi, td),
    CRules *r _AND_
    CTDI *ctdi _AND_
    TypeDef *td)
{
    CTDI *tblCtdi;
    int typeIndex;

    typeIndex = GetBuiltinType (td->type);

    if (typeIndex < 0)
        return;

    tblCtdi = &r->typeConvTbl[typeIndex];

    memcpy (ctdi, tblCtdi, sizeof (CTDI));

}
