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
 * compiler/back-ends/c-gen/util.c  - utilities for generating C encoders and decoders
 *
 *  MS 91/11/04
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/util.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: util.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:44  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:48:38  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:26:52  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:44  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "rules.h"
#include "snacc-util.h"
#include "util.h"


void
MakeVarPtrRef PARAMS ((r, td, parent, fieldType, parentVarName, newVarName),
    CRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *fieldType _AND_
    char *parentVarName _AND_
    char *newVarName)
{
    CTRI *ctri;

    ctri = fieldType->cTypeRefInfo;

    /* always put in brackets to save future referencing hassles */
    strcpy (newVarName, "(");

    /* make ref'd field into a ptr by taking it's addr if nec */
    if (!ctri->isPtr)
        strcat (newVarName, "&");

    /* start with ref to parent */
    strcat (newVarName, parentVarName);

    /* ref this field */
    if ((td->type == parent) || (parent->cTypeRefInfo->isPtr))
        strcat (newVarName, "->");
    else
        strcat (newVarName, ".");

    /* ref choice union field if nec */
    if (parent->basicType->choiceId == BASICTYPE_CHOICE)
    {
        strcat (newVarName, r->choiceUnionFieldName);
        strcat (newVarName, ".");
    }

    strcat (newVarName, ctri->cFieldName);
    strcat (newVarName, ")");

}  /* MakeVarPtrRef */




void
MakeVarValueRef PARAMS ((r, td, parent, fieldType, parentVarName, newVarName),
    CRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *fieldType _AND_
    char *parentVarName _AND_
    char *newVarName)
{
    CTRI *ctri;

    ctri = fieldType->cTypeRefInfo;

    /* always put in brackets to save future referencing hassles */
    strcpy (newVarName, "(");

    /* make ref'd field into a value by de-referencing if nec */
    if (ctri->isPtr)
        strcat (newVarName, "*");

    /* start with ref to parent */
    strcat (newVarName, parentVarName);

    /* ref this field */
    if ((td->type == parent) || (parent->cTypeRefInfo->isPtr))
        strcat (newVarName, "->");
    else
        strcat (newVarName, ".");

    /* ref choice union field if nec */
    if (parent->basicType->choiceId == BASICTYPE_CHOICE)
    {
        strcat (newVarName, r->choiceUnionFieldName);
        strcat (newVarName, ".");
    }

    strcat (newVarName, ctri->cFieldName);
    strcat (newVarName, ")");

}  /* MakeVarValueRef */

void
MakeChoiceIdValueRef PARAMS ((r, td, parent, fieldType, parentVarName, newVarName),
    CRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *fieldType _AND_
    char *parentVarName _AND_
    char *newVarName)
{
    CTRI *ctri;

    ctri = fieldType->cTypeRefInfo;

    /* always put in brackets to save future referencing hassles */
    strcpy (newVarName, "(");

    /* start with ref to parent */
    strcat (newVarName, parentVarName);

    /* ref this field */
    if ((td->type == parent) || (parent->cTypeRefInfo->isPtr))
        strcat (newVarName, "->");
    else
        strcat (newVarName, ".");

    strcat (newVarName, parent->cTypeRefInfo->choiceIdEnumFieldName);
    strcat (newVarName, ")");

}  /* MakeChoiceIdValueRef */


void
PrintElmtAllocCode PARAMS ((src, type, varRefPtrName),
    FILE *src _AND_
    Type *type _AND_
    char *varRefPtrName)
{
    CTRI *ctri1;
    CTRI *ctri2;
    Type *t;

    t = GetType (type);
    ctri1 =  type->cTypeRefInfo;
    ctri2 =  t->cTypeRefInfo;
    if (ctri1->isPtr)
    {
        if (ctri2->cTypeId == C_LIST)
           fprintf (src, "    %s = AsnListNew (sizeof (char*));\n", varRefPtrName);
        else
           fprintf (src, "    %s = (%s*) Asn1Alloc (sizeof (%s));\n", varRefPtrName, ctri1->cTypeName, ctri1->cTypeName);
         fprintf (src,"    CheckAsn1Alloc (%s, env);\n", varRefPtrName);
    }

} /* PrintElmtAllocCode */


/*
 * prints code to decode EOCs for the lengths that go with extra tagging
 * maxLenLevel - the highest used length variable (ie 2 for elmtLen2)
 * minLenLevel - the lowest valid length variable (ie 0 for elmtLen0)
 * lenBaseVarName - len var name sans number (ie elmtLen for elmtLen2)
 * totalLevel - current level for the running total
 * totalBaseName - total var name sans number
 *                        (ie totalElmtLen for totalElmtLen1)
 */
void
PrintEocDecoders PARAMS ((f, maxLenLevel, minLenLevel, lenBaseVarName, totalLevel, totalBaseVarName),
    FILE *f _AND_
    int maxLenLevel _AND_
    int minLenLevel _AND_
    char *lenBaseVarName _AND_
    int totalLevel _AND_
    char *totalBaseVarName)
{
    int i;
    for (i = maxLenLevel; i > minLenLevel; i--)
    {
        fprintf (f,"    if (%s%d == INDEFINITE_LEN)\n", lenBaseVarName, i);
        fprintf (f,"        BDecEoc (b, &%s%d, env);\n", totalBaseVarName, totalLevel);
    }
} /* PrintEocDeocoders */
