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
 * compiler/back_ends/idl_gen/gen_vals.c - prints ASN.1 values in IDL format
 *
 * MS 92
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/idl-gen/gen-vals.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-vals.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:29  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:45  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.1  1997/01/01 20:25:36  rj
 * first draft
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "oid.h"
#include "mem.h"
#include "define.h"
#include "lib-types.h"
#include "str-util.h"
#include "snacc-util.h"
#include "rules.h"
#include "gen-vals.h"

/* non-exported routines' prototypes */

static void PrintIDLValueDefsName PROTO ((FILE *f, IDLRules *r, ValueDef *v));




void
PrintIDLValueDef PARAMS ((idl, r, v),
    FILE	*idl _AND_
    IDLRules	*r _AND_
    ValueDef	*v)
{
    /* just do oid's, ints and bools for now */
    if ((v->value->basicValue->choiceId != BASICVALUE_OID) &&
        (v->value->basicValue->choiceId != BASICVALUE_INTEGER) &&
        (v->value->basicValue->choiceId != BASICVALUE_BOOLEAN))
        return;

    /*
     * put instantiation in idl file
     */
    fprintf (idl, "  const ");
    PrintIDLValuesClass (idl, r, v->value);
    fprintf (idl, " ");
    PrintIDLValueDefsName (idl, r, v);
    fprintf (idl, " = ");
    PrintIDLValueInstatiation (idl, r, v->value);
    fprintf (idl, ";\n\n");

}  /* PrintIDLValueDef */


static void
PrintIDLValueDefsName PARAMS ((f, r, v),
    FILE *f _AND_
    IDLRules *r _AND_
    ValueDef *v)
{
    char *cName;
    cName = Asn1ValueName2CValueName (v->definedName);
    fprintf (f, "%s", cName);
    Free (cName);
}

void
PrintIDLValuesClass PARAMS ((f, r, v),
    FILE *f _AND_
    IDLRules *r _AND_
    Value *v)
{
    /* needs work - just do ints bools and oid's for now */
    switch (v->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_OID].typeName);
            break;

        case BASICVALUE_INTEGER:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_INTEGER].typeName);
            break;

        case BASICVALUE_BOOLEAN:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_BOOLEAN].typeName);
            break;

        default:
           break;
    }
}


void
PrintIDLValueInstatiation PARAMS ((f, r, v),
    FILE *f _AND_
    IDLRules *r _AND_
    Value *v)
{
    /* needs work - just do oids, ints and bools for now */
    switch (v->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            PrintIDLOidValue (f, r, v->basicValue->a.oid);
            break;

        case BASICVALUE_INTEGER:
            PrintIDLIntValue (f, r, v->basicValue->a.integer);
            break;

        case BASICVALUE_BOOLEAN:
            fprintf (f, v->basicValue->a.boolean ? "TRUE" : "FALSE");
            break;

        default:
           break;
    }
}



/*
 * given an AOID, c++ AOID constructors params are produced.
 * This is used for turning ASN.1 OBJECT ID values
 * into usable c++ values.
 *
 * eg for the oid { 0 1 2 } (in AOID format)
 *   (0,1,2)
 * is produced.
 */
void
PrintIDLOidValue PARAMS ((f, r, v),
    FILE *f _AND_
    IDLRules *r _AND_
    AsnOid *v)
{
    unsigned short int firstArcNum;
    unsigned long int arcNum;
    int i;

    fprintf (f, "(");

    /* un-munge first two arc numbers */
    for (arcNum = 0, i=0; (i < v->octetLen) && (v->octs[i] & 0x80);i++)
        arcNum = (arcNum << 7) + (v->octs[i] & 0x7f);

    arcNum = (arcNum << 7) + (v->octs[i] & 0x7f);
    i++;
    firstArcNum = arcNum/40;
    if (firstArcNum > 2)
        firstArcNum = 2;

    fprintf (f, "%u, %u", firstArcNum, arcNum - (firstArcNum * 40));

    for (; i < v->octetLen; )
    {
        for (arcNum = 0; (i < v->octetLen) && (v->octs[i] & 0x80);i++)
            arcNum = (arcNum << 7) + (v->octs[i] & 0x7f);

        arcNum = (arcNum << 7) + (v->octs[i] & 0x7f);
        i++;

        fprintf (f, ", %u", arcNum);
    }
    fprintf (f, ")");

} /* PrintIDLOidValue */



void
PrintIDLIntValue PARAMS ((f, r, v),
    FILE *f _AND_
    IDLRules *r _AND_
    AsnInt v)
{
    fprintf (f, "%d", v);

} /* PrintIDLIntValue */
