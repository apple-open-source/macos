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
 * compiler/back-ends/c-gen/gen-vals.c - prints ASN.1 values in C format
 *
 *
 * MS Feb 92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/gen-vals.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-vals.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1  2000/05/10 21:35:01  rmurphy
 * Adding back in base code files which had been moved to "2" versions.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:43  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1997/05/07 14:59:31  wan
 * Fixed bug in C value string generation.
 *
 * Revision 1.3  1995/07/25 18:44:12  rj
 * file name has been shortened for redundant part: c-gen/gen-c-vals -> c-gen/gen-vals.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:24:18  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:33  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "oid.h"
#include "asn1module.h"
#include "mem.h"
#include "define.h"
#include "lib-types.h"
#include "rules.h"
#include "type-info.h"
#include "str-util.h"
#include "snacc-util.h"
#include "util.h"
#include "kwd.h"
#include "gen-vals.h"

/* non-exported routines' prototypes */

static void PrintValueDefsName PROTO ((FILE *f, CRules *r, ValueDef *v));
static void PrintValueDefsType PROTO ((FILE *f, CRules *r, ValueDef *v));
static void PrintValueInstatiation PROTO ((FILE *f, CRules *r, ValueDef *v));




void
PrintCValueDef PARAMS ((src, r, v),
    FILE *src _AND_
    CRules *r _AND_
    ValueDef *v)
{
    /* just do oid's, ints and bools for now */
    if ((v->value->basicValue->choiceId != BASICVALUE_OID) &&
        (v->value->basicValue->choiceId != BASICVALUE_INTEGER) &&
        (v->value->basicValue->choiceId != BASICVALUE_BOOLEAN))
        return;

    /*
     * put instantiation in src file
     */
    PrintValueDefsType (src, r, v);
    fprintf (src," ");
    PrintValueDefsName (src, r, v);
    fprintf (src," = ");
    PrintValueInstatiation (src, r, v);
    fprintf (src,";\n\n");

}  /* PrintCValueDef */

void
PrintCValueExtern PARAMS ((hdr, r, v),
    FILE *hdr _AND_
    CRules *r _AND_
    ValueDef *v)
{
    /* just do oid's, ints and bools for now */
    if ((v->value->basicValue->choiceId != BASICVALUE_OID) &&
        (v->value->basicValue->choiceId != BASICVALUE_INTEGER) &&
        (v->value->basicValue->choiceId != BASICVALUE_BOOLEAN))
        return;

    /*
     * put extern declaration in hdr file
     */
    fprintf (hdr,"extern ");
    PrintValueDefsType (hdr, r, v);
    fprintf (hdr," ");
    PrintValueDefsName (hdr, r, v);
    fprintf (hdr,";\n");

}  /* PrintCValueExtern */


static void
PrintValueDefsName PARAMS ((f, r, v),
    FILE *f _AND_
    CRules *r _AND_
    ValueDef *v)
{
    char *cName;
    cName = Asn1ValueName2CValueName (v->definedName);
    fprintf (f, "%s", cName);
    Free (cName);
}

static void
PrintValueDefsType PARAMS ((f, r, v),
    FILE *f _AND_
    CRules *r _AND_
    ValueDef *v)
{
    /* needs work - just do ints bools and oid's for now */
    switch (v->value->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_OID].cTypeName);
            break;

        case BASICVALUE_INTEGER:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_INTEGER].cTypeName);
            break;

        case BASICVALUE_BOOLEAN:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_BOOLEAN].cTypeName);
            break;

        default:
           break;
    }
}


static void
PrintValueInstatiation PARAMS ((f, r, v),
    FILE *f _AND_
    CRules *r _AND_
    ValueDef *v)
{
    /* needs work - just do ints, bools and oids for now */
    switch (v->value->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            PrintCOidValue (f, r, v->value->basicValue->a.oid);
            break;

        case BASICVALUE_INTEGER:
            fprintf (f, "%d", v->value->basicValue->a.integer);
            break;

        case BASICVALUE_BOOLEAN:
            if (v->value->basicValue->a.boolean)
                fprintf (f, "TRUE");
            else
               fprintf (f, "FALSE");
            break;

        default:
           break;
    }


}



/*
 * given an AOID, a c value is produced.
 * This is used for turning ASN.1 OBJECT ID values
 * into usable c values.
 *
 * eg for the oid { 0 1 2 } (in AOID format)
 *
 * {
 *     2,
 *     "\1\2"
 * }
 * is produced.
 */
void
PrintCOidValue PARAMS ((f, r, oid),
    FILE *f _AND_
    CRules *r _AND_
    AsnOid *oid)
{
    int i;

    fprintf (f, "{ ");
    fprintf (f, "%d, ",oid->octetLen);
    fprintf (f, "\"");

    /* print encoded oid string in C's 'octal' escape format */
    for (i = 0; i < oid->octetLen; i++)
        fprintf (f, "\\%o", (unsigned char) oid->octs[i]);
    fprintf (f, "\"");
    fprintf (f, " }");

} /* PrintCOidValue */
