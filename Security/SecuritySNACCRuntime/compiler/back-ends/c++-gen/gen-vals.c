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
 * compiler/back_ends/c++_gen/gen_vals.c - prints ASN.1 values in c++ format
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/c++-gen/gen-vals.c,v 1.3 2001/06/25 21:51:10 dmitch Exp $
 * $Log: gen-vals.c,v $
 * Revision 1.3  2001/06/25 21:51:10  dmitch
 * Avoid instantiating AsnInt constants; use #define instead. Partial fix for Radar 2664258.
 *
 * Revision 1.2  2001/06/20 21:30:32  dmitch
 * Per SNACC_OIDS_AS_DEFINES #define, optionally define OIDs as #defines in the header rather than as statically initialized objects in the .cpp file.
 *
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:40  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1995/07/25 18:20:05  rj
 * use true/false instead of AsnBool::true/false.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1994/10/08  03:47:57  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.2  1994/09/01  00:17:22  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:05  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
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

static void PrintCxxValueDefsName PROTO ((FILE *f, CxxRules *r, ValueDef *v));




void
PrintCxxValueDef PARAMS ((src, r, v),
    FILE *src _AND_
    CxxRules *r _AND_
    ValueDef *v)
{
    /* just do oid's, ints and bools for now */
    if (
		#if		!SNACC_OIDS_AS_DEFINES
		(v->value->basicValue->choiceId != BASICVALUE_OID) &&
		#endif	/* SNACC_OIDS_AS_DEFINES */
		#if		!SNACC_INTS_AS_DEFINES
        (v->value->basicValue->choiceId != BASICVALUE_INTEGER) &&
		#endif	/* !SNACC_INTS_AS_DEFINES */
        (v->value->basicValue->choiceId != BASICVALUE_BOOLEAN))
        return;

    /*
     * put instantiation in src file
     */
    fprintf (src, "const ");
    PrintCxxValuesClass (src, r, v->value);
    fprintf (src, " ");
    PrintCxxValueDefsName (src, r, v);
    fprintf (src, " ");
    PrintCxxValueInstatiation (src, r, v->value);
    fprintf (src, ";\n\n");


}  /* PrintCxxValueDef */

void
PrintCxxValueExtern PARAMS ((hdr, r, v),
    FILE *hdr _AND_
    CxxRules *r _AND_
    ValueDef *v)
{
    /* just do oid's, ints and bools for now */
    if ((v->value->basicValue->choiceId != BASICVALUE_OID) &&
        (v->value->basicValue->choiceId != BASICVALUE_INTEGER) &&
        (v->value->basicValue->choiceId != BASICVALUE_BOOLEAN))
        return;

	#if		SNACC_OIDS_AS_DEFINES
	/*
	 * Special case for OIDs. The format is 
	 *
	 *   #define oidname_arc  x,y,z,...
	 *   #define oidname AsnOid(oidname_arc)
	 */
    if (v->value->basicValue->choiceId == BASICVALUE_OID) {
		/* first just the arc */
		fprintf (hdr, "#define ");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "_arc ");
		PrintCxxValueInstatiation (hdr, r, v->value);
		fprintf (hdr, "\n");
		
		/* now the declaration using the arc */
		fprintf (hdr, "#define ");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "     ");
		PrintCxxValuesClass (hdr, r, v->value);
		fprintf (hdr, "(");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "_arc)\n");

		return;
	}
	#endif	/* SNACC_OIDS_AS_DEFINES */
	
	#if		SNACC_INTS_AS_DEFINES
	/*
	 * Special case for AsnInt. The format is 
	 *
	 *   #define oidname_val  x
	 *   #define oidname AsnInt(oidname_val)
	 */
    if (v->value->basicValue->choiceId == BASICVALUE_INTEGER) {
		/* first just the integer value */
		fprintf (hdr, "#define ");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "_val %d\n", v->value->basicValue->a.integer);
		
		/* now the declaration using the val */
		fprintf (hdr, "#define ");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "     ");
		PrintCxxValuesClass (hdr, r, v->value);
		fprintf (hdr, "(");
		PrintCxxValueDefsName (hdr, r, v);
		fprintf (hdr, "_val)\n");

		return;
	}
	#endif	/* SNACC_OIDS_AS_DEFINES */

    /*
     * put extern declaration in hdr file
     */
    fprintf (hdr, "extern const ");
    PrintCxxValuesClass (hdr, r, v->value);
    fprintf (hdr, " ");
    PrintCxxValueDefsName (hdr, r, v);
    fprintf (hdr, ";\n");

}  /* PrintCxxValueExtern */


static void
PrintCxxValueDefsName PARAMS ((f, r, v),
    FILE *f _AND_
    CxxRules *r _AND_
    ValueDef *v)
{
    char *cName;
    cName = Asn1ValueName2CValueName (v->definedName);
    fprintf (f, "%s", cName);
    Free (cName);
}

void
PrintCxxValuesClass PARAMS ((f, r, v),
    FILE *f _AND_
    CxxRules *r _AND_
    Value *v)
{
    /* needs work - just do ints bools and oid's for now */
    switch (v->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_OID].className);
            break;

        case BASICVALUE_INTEGER:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_INTEGER].className);
            break;

        case BASICVALUE_BOOLEAN:
            fprintf (f, "%s", r->typeConvTbl[BASICTYPE_BOOLEAN].className);
            break;

        default:
           break;
    }
}


void
PrintCxxValueInstatiation PARAMS ((f, r, v),
    FILE *f _AND_
    CxxRules *r _AND_
    Value *v)
{
    /* needs work - just do oids, ints and bools for now */
    switch (v->basicValue->choiceId)
    {
        case BASICVALUE_OID:
            PrintCxxOidValue (f, r, v->basicValue->a.oid);
            break;

        case BASICVALUE_INTEGER:
            PrintCxxIntValue (f, r, v->basicValue->a.integer);
            break;

        case BASICVALUE_BOOLEAN:
	    fprintf (f, "(%s)", v->basicValue->a.boolean ? "true" : "false");
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
 *
 * The parentheses are omitted in the SNACC_OIDS_AS_DEFINES case; we're just printing
 * the arc which can be used as an arg elsewhere. 

 */
void
PrintCxxOidValue PARAMS ((f, r, v),
    FILE *f _AND_
    CxxRules *r _AND_
    AsnOid *v)
{
    unsigned short int firstArcNum;
    unsigned long int arcNum;
    int i;

	#if !SNACC_OIDS_AS_DEFINES
    fprintf (f, "(");
	#endif
	
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
	
	#if	!SNACC_OIDS_AS_DEFINES
    fprintf (f, ")");
	#endif

} /* PrintCxxOidValue */



void
PrintCxxIntValue PARAMS ((f, r, v),
    FILE *f _AND_
    CxxRules *r _AND_
    AsnInt v)
{
    fprintf (f, "(%d)", v);
} /* PrintCxxIntValue */
