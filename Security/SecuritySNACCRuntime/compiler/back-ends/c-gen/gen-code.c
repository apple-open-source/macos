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
 * compiler/back-ends/c-gen/gen-code.c - generate C hdr and src files
 *
 * Assumes you have called FillCTypeInfo
 *
 * MS 92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/Attic/gen-code.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-code.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1  2000/05/10 21:35:01  rmurphy
 * Adding back in base code files which had been moved to "2" versions.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:41  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1995/07/25 18:39:46  rj
 * file name has been shortened for redundant part: c-gen/gen-c-code -> c-gen/gen-code.
 *
 * PrintConditionalIncludeOpen() and PrintConditionalIncludeClose() moved to back-ends/cond.c
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1995/02/18  12:50:53  rj
 * typo fixed.
 *
 * Revision 1.2  1994/09/01  00:21:54  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:17  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "print.h"
#include "rules.h"
#include "type-info.h"
#include "util.h"
#include "cond.h"
#include "gen-type.h"
#include "gen-enc.h"
#include "gen-dec.h"
#include "gen-vals.h"
#include "gen-free.h"
#include "gen-print.h"
#include "gen-any.h"
#include "gen-code.h"

/* unexported prototypes */
static void PrintCSrcComment PROTO ((FILE *src, Module *m));
static void PrintCSrcIncludes PROTO ((FILE *src, Module *m, ModuleList *mods));
static void PrintCHdrComment PROTO ((FILE *hdr, Module *m));

/*
 * Fills the hdr file with the C type and encode/decode prototypes
 * Fills the src file with the encoded/decode routine definitions
 */
void
PrintCCode PARAMS ((src, hdr, mods, m, r, longJmpVal, printTypes, printValues, printEncoders, printDecoders, printPrinters, printFree),
    FILE *src _AND_
    FILE *hdr _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    CRules *r _AND_
    long int longJmpVal _AND_
    int printTypes _AND_
    int printValues _AND_
    int printEncoders _AND_
    int printDecoders _AND_
    int printPrinters _AND_
    int printFree)
{
    TypeDef *td;
    ValueDef *vd;

    PrintCSrcComment (src, m);
    PrintCSrcIncludes (src, m, mods);

    PrintCHdrComment (hdr, m);
    PrintConditionalIncludeOpen (hdr, m->cHdrFileName);

    fprintf (hdr,"\n\n");
    fprintf (src,"\n\n");


    if (printValues)
    {
        /* put value defs at beginning of .c file */
        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
        {
            PrintCValueDef (src, r, vd);
        }
    }

    PrintCAnyCode (src, hdr, r, mods, m);

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (printTypes)
            PrintCTypeDef (hdr, r, m, td);

        /* for PDU type or types ref'd with ANY/ANY DEF BY */
        if (printEncoders &&
            ((td->anyRefs != NULL) || td->cTypeDefInfo->isPdu))
            PrintCBerEncoder (src, hdr, r, m, td);

        /* for PDU type or types ref'd with ANY/ANY DEF BY */
        if (printDecoders &&
            ((td->anyRefs != NULL) || td->cTypeDefInfo->isPdu))
            PrintCBerDecoder (src, hdr, r, m, td, &longJmpVal);

        if (printEncoders)
            PrintCBerContentEncoder (src, hdr, r, m, td);

        if (printDecoders)
            PrintCBerContentDecoder (src, hdr, r, m, td, &longJmpVal);


        if (printPrinters)
            PrintCPrinter (src, hdr, r, mods, m, td);

        if (printFree)
            PrintCFree (src, hdr, r, mods, m, td);

        /* only print new lines for normal types */
        switch (td->type->basicType->choiceId)
        {
            case BASICTYPE_SEQUENCEOF:  /* list types */
            case BASICTYPE_SETOF:
            case BASICTYPE_CHOICE:
            case BASICTYPE_SET:
            case BASICTYPE_SEQUENCE:
                fprintf (src, "\n\n\n");
                /* fall through */

            case BASICTYPE_IMPORTTYPEREF:  /* type references */
            case BASICTYPE_LOCALTYPEREF:
            case BASICTYPE_BOOLEAN:  /* library type */
            case BASICTYPE_REAL:  /* library type */
            case BASICTYPE_OCTETSTRING:  /* library type */
            case BASICTYPE_NULL:  /* library type */
            case BASICTYPE_OID:  /* library type */
            case BASICTYPE_INTEGER:  /* library type */
            case BASICTYPE_BITSTRING:  /* library type */
            case BASICTYPE_ENUMERATED:  /* library type */
            case BASICTYPE_ANYDEFINEDBY:  /* ANY types */
            case BASICTYPE_ANY:
                fprintf (hdr, "\n\n\n");
                break;
        }

    }

    if (printValues)
    {
        /* put value externs at end of .h file */
        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
        {
            PrintCValueExtern (hdr, r, vd);
        }
    }

    PrintConditionalIncludeClose (hdr, m->cHdrFileName);

} /* PrintCCode */


static void
PrintCSrcComment PARAMS ((src, m),
    FILE *src _AND_
    Module *m)
{
    long int t;

    t = time (0);
    fprintf (src, "/*\n");
    fprintf (src, " *    %s\n *\n", m->cSrcFileName);
    fprintf (src, " *    \"%s\" ASN.1 module encode/decode/print/free C src.\n *\n", m->modId->name);
    fprintf (src, " *    This file was generated by snacc on %s *\n", ctime (&t));
    fprintf (src, " *    UBC snacc written by Mike Sample\n *\n");
    fprintf (src, " *    NOTE: This is a machine generated file - editing not recommended\n");
    fprintf (src, " */\n\n\n");

} /* PrintSrcComment */



static void
PrintCSrcIncludes PARAMS ((src, m, mods),
    FILE *src _AND_
    Module *m _AND_
    ModuleList *mods)
{
    void *tmp;
    Module *impMod;

    /*
     * include snacc runtime library related hdrs
     */
    fprintf (src, "\n#include \"asn-incl.h\"\n");

    /*
     * print out include files in same order of the module
     * list. every module in the list includes the others and it's
     * own .h
     */
    tmp = (void*)CURR_LIST_NODE (mods);
    FOR_EACH_LIST_ELMT (impMod, mods)
    {
        fprintf (src, "#include \"%s\"\n", impMod->cHdrFileName);
    }
    SET_CURR_LIST_NODE (mods, tmp);

}  /* PrintCSrcIncludes */


static void
PrintCHdrComment PARAMS ((f, m),
    FILE *f _AND_
    Module *m)
{
    long int t;

    t = time (0);
    fprintf (f, "/*\n");
    fprintf (f, " *    %s\n *\n", m->cHdrFileName);
    fprintf (f, " *    \"%s\" ASN.1 module C type definitions and prototypes\n *\n", m->modId->name);
    fprintf (f, " *    This .h file was generated by snacc on %s *\n", ctime (&t));
    fprintf (f, " *    UBC snacc written compiler by Mike Sample\n *\n");
    fprintf (f, " *    NOTE: This is a machine generated file--editing not recommended\n");
    fprintf (f, " */\n\n\n");
} /* PrintCHdrComment */
