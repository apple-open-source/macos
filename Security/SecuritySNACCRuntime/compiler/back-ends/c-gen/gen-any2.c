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
 * compiler/back-ends/c-gen/gen-any.c
 *
 *     prints Routine to initialize the ANY Hash table.  The
 *     ANY Hash table maps the OBJECT IDENTIFIERS or INTEGERS
 *     to the correct encoding/decoding etc routines.
 *
 *     Also prints an enum to identify each ANY mapping.
 *
 *     if the given module has no ANY or ANY DEFINED BY  types
 *     nothing is printed.
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/gen-any2.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-any2.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:41  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:33:43  rj
 * file name has been shortened for redundant part: c-gen/gen-c-any -> c-gen/gen-any.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:21:15  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:48:15  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "rules.h"
#include "define.h"
#include "str-util.h"
#include "gen-vals.h"
#include "lib-types.h"
#include "gen-any.h"

int anyEnumValG = 0;


void PrintCAnyEnum PROTO ((FILE *hdr, Module *m, CRules *r));

void PrintCAnyHashInitRoutine PROTO ((FILE *src, FILE *hdr, ModuleList *mods, Module *m, CRules *r));




void
PrintCAnyCode PARAMS ((src, hdr, r, mods, m),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m)
{

    if (!m->hasAnys)
        return;

    PrintCAnyEnum (hdr, m, r);
    PrintCAnyHashInitRoutine (src, hdr, mods, m, r);

}  /* PrintAnyCode */



void
PrintCAnyEnum PARAMS ((hdr, m, r),
    FILE *hdr _AND_
    Module *m _AND_
    CRules *r)
{
    TypeDef *td;
    AnyRef *ar;
    AnyRefList *arl;
    int i;
    int firstPrinted = TRUE;
    char *modName;

    modName = Asn1TypeName2CTypeName (m->modId->name);

    fprintf (hdr,"typedef enum %sAnyId\n", modName);
    fprintf (hdr,"{\n");

    /* do any lib types */
    for (i = BASICTYPE_BOOLEAN; i < BASICTYPE_MACRODEF; i++)
    {
        arl = LIBTYPE_GET_ANY_REFS (i);
        if (arl != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, arl)
            {
                if (!firstPrinted)
                    fprintf (hdr,",\n");
                fprintf (hdr,"    %s = %d", ar->anyIdName, anyEnumValG++);
                firstPrinted = FALSE;
            }
        }
    }

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (td->anyRefs != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, td->anyRefs)
            {
                if (!firstPrinted)
                    fprintf (hdr,",\n");
                fprintf (hdr,"    %s = %d", ar->anyIdName, anyEnumValG++);
                firstPrinted = FALSE;
            }
        }
    }
    if (firstPrinted) /* none have been printed */
        fprintf (hdr,"/* NO INTEGER or OBJECT IDENTIFIER to ANY type relationships were defined (via MACROs or other mechanism) */\n???\n");

    fprintf (hdr,"} %sAnyId;\n\n\n", modName);
    Free (modName);

}  /* PrintAnyEnum */


void
PrintCAnyHashInitRoutine PARAMS ((src, hdr, mods, m, r),
    FILE *src _AND_
    FILE *hdr _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    CRules *r)
{
    TypeDef *td;
    AnyRef *ar;
    AnyRefList *arl;
    char *modName;
    CTDI *ctdi;
    int i,j;
    enum BasicTypeChoiceId typeId;
    char *encRoutineName;
    char *decRoutineName;
    char *freeRoutineName;
    char *printRoutineName;
    int installedSomeHashes = FALSE;

    /* print proto in hdr file */
    modName = Asn1TypeName2CTypeName (m->modId->name);
    fprintf (hdr,"void InitAny%s();\n\n", modName);

    /* print routine to src file */
    fprintf (src,"void\nInitAny%s()\n", modName);
    fprintf (src,"{\n");

    /* first print value for OID's */
    /* do any lib types first */
    i = 0;
    for (j = BASICTYPE_BOOLEAN; j < BASICTYPE_MACRODEF; j++)
    {
        arl = LIBTYPE_GET_ANY_REFS (j);
        if (arl != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, arl)
            {
                installedSomeHashes = TRUE;
                if (ar->id->choiceId == OIDORINT_OID)
                {
                    fprintf (src,"    %s oid%d =", r->typeConvTbl[BASICTYPE_OID].cTypeName, i++);
                    PrintCOidValue (src, r, ar->id->a.oid);
                    fprintf (src,";\n");
                }
            }
        }
    }

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (td->anyRefs != NULL)
        {
            ctdi = td->cTypeDefInfo;
            FOR_EACH_LIST_ELMT (ar, td->anyRefs)
            {
                installedSomeHashes = TRUE;
                if (ar->id->choiceId == OIDORINT_OID)
                {
                    fprintf (src,"    %s oid%d =", r->typeConvTbl[BASICTYPE_OID].cTypeName, i++);
                    PrintCOidValue (src, r, ar->id->a.oid);
                    fprintf (src,";\n");
                }
            }
        }
    }

    fprintf (src,"\n\n");

    /* now print hash init calls */
    i = 0;

    /* do lib types first */
    for (j = BASICTYPE_BOOLEAN; j < BASICTYPE_MACRODEF; j++)
    {
        arl = LIBTYPE_GET_ANY_REFS (j);
        if (arl != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, arl)
            {

                encRoutineName = r->typeConvTbl[j].encodeRoutineName;
                decRoutineName = r->typeConvTbl[j].decodeRoutineName;
                printRoutineName = r->typeConvTbl[j].printRoutineName;

                /*
                 * use NULL free routine for types that
                 * have empyt macros for their free routines
                 * (since the any hash tbl needs the addr of the routine)
                 */
                switch (j)
                {
                    case BASICTYPE_BOOLEAN:
                    case BASICTYPE_INTEGER:
                    case BASICTYPE_NULL:
                    case BASICTYPE_REAL:
                    case BASICTYPE_ENUMERATED:
                        freeRoutineName = "NULL";
                        break;
                    default:
                        freeRoutineName =  r->typeConvTbl[j].freeRoutineName;
                }

                if (ar->id->choiceId == OIDORINT_OID)
                    fprintf (src,"    InstallAnyByOid (%s, &oid%d, sizeof (%s), (EncodeFcn) B%s, (DecodeFcn)B%s, (FreeFcn)%s, (PrintFcn)%s);\n\n", ar->anyIdName, i++,  r->typeConvTbl[j].cTypeName, encRoutineName, decRoutineName, freeRoutineName, printRoutineName);
                else
                    fprintf (src,"    InstallAnyByInt (%s, %d, sizeof (%s), (EncodeFcn) B%s, (DecodeFcn)B%s, (FreeFcn)%s, (PrintFcn)%s);\n\n", ar->anyIdName, ar->id->a.intId, r->typeConvTbl[j].cTypeName, encRoutineName, decRoutineName, freeRoutineName, printRoutineName);
            }
        }
    }

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (td->anyRefs != NULL)
        {
            ctdi = td->cTypeDefInfo;
            FOR_EACH_LIST_ELMT (ar, td->anyRefs)
            {
                typeId = GetBuiltinType (td->type);

                encRoutineName = ctdi->encodeRoutineName;
                decRoutineName = ctdi->decodeRoutineName;
                printRoutineName = ctdi->printRoutineName;

                /*
                 * use NULL free routine for types that
                 * have empyt macros for their free routines
                 * (since the any hash tbl needs the addr of the routine)
                 */
                switch (typeId)
                {
                    case BASICTYPE_BOOLEAN:
                    case BASICTYPE_INTEGER:
                    case BASICTYPE_NULL:
                    case BASICTYPE_REAL:
                    case BASICTYPE_ENUMERATED:
                        freeRoutineName = "NULL";
                        break;
                    default:
                        freeRoutineName = ctdi->freeRoutineName;
                }

                if (ar->id->choiceId == OIDORINT_OID)
                    fprintf (src,"    InstallAnyByOid (%s, &oid%d, sizeof (%s), (EncodeFcn) B%s, (DecodeFcn)B%s, (FreeFcn)%s, (PrintFcn)%s);\n\n", ar->anyIdName, i++, ctdi->cTypeName, encRoutineName, decRoutineName, freeRoutineName, printRoutineName);
                else
                    fprintf (src,"    InstallAnyByInt (%s, %d, sizeof (%s), (EncodeFcn) B%s, (DecodeFcn)B%s, (FreeFcn)%s, (PrintFcn)%s);\n\n", ar->anyIdName, ar->id->a.intId, ctdi->cTypeName, encRoutineName, decRoutineName, freeRoutineName, printRoutineName);
            }
        }
    }


    if (!installedSomeHashes)
    {
        fprintf (src,"    /* Since no INTEGER/OID to ANY type relations were defined\n");
        fprintf (src,"     * (usually done via MACROs) you must manually do the code\n");
        fprintf (src,"     * to fill the hash tbl.\n");
        fprintf (src,"     * if the ids are INTEGER use the following:\n");
        fprintf (src,"     * InstallAnyByInt (??_ANY_ID, intVal, sizeof (Foo), (EncodeFcn) BEncFoo, (DecodeFcn)BDecFoo, (FreeFcn)FreeFoo, (PrintFcn)PrintFoo);\n");
        fprintf (src,"     * if the ids are OBJECT IDENTIFIERs use the following:\n");
        fprintf (src,"     *     InstallAnyByOid (??_ANY_ID, oidVal, sizeof (Foo), (EncodeFcn) BEncFoo, (DecodeFcn)BDecFoo, (FreeFcn)FreeFoo, (PrintFcn)PrintFoo);\n");
        fprintf (src,"     * put the ??_ANY_IDs in the AnyId enum.\n\n");
        fprintf (src,"     * For example if you have some thing like\n");
        fprintf (src,"     * T1 ::= SEQUENCE { id INTEGER, ANY DEFINED BY id }\n");
        fprintf (src,"     * and the id 1 maps to the type BOOLEAN use the following:\n");
        fprintf (src,"     * InstallAnyByInt (SOMEBOOL_ANY_ID, 1, sizeof (AsnBool), (EncodeFcn) BEncAsnBool, (DecodeFcn)BDecAsnBool, (FreeFcn)NULL, (PrintFcn)PrintAsnBool);;\n");
        fprintf (src,"     */\n ???????\n");  /* generate compile error */
    }


    fprintf (src,"}  /* InitAny%s */\n\n\n", modName);

    Free (modName);

}  /* PrintAnyHashInitRoutine */
