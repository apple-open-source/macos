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
 * compiler/back_ends/c++_gen/gen_any.c
 *
 *     prints Routine to initialize the ANY Hash table.  The
 *     ANY Hash table maps the OBJECT IDENTIFIERS or INTEGERS
 *     to the correct decoding routines.
 *
 *     Also prints an enum to identify each ANY mapping.
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
 * INSERT_VDA_COMMENTS
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/c++-gen/gen-any.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-any.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:39  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1995/07/25 18:19:11  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1994/10/08  03:47:53  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.2  1994/09/01  01:06:31  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:47:58  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "define.h"
#include "str-util.h"
#include "rules.h"
#include "gen-vals.h"
#include "lib-types.h"
#include "gen-any.h"

static int anyEnumValG = 0;


void PrintCxxAnyEnum PROTO ((FILE *hdr, Module *m, CxxRules *r));

void PrintCxxAnyHashInitRoutine PROTO ((FILE *src, FILE *hdr, ModuleList *mods, Module *m, CxxRules *r));


void
PrintCxxAnyCode PARAMS ((src, hdr, r, mods, m),
    FILE *src _AND_
    FILE *hdr _AND_
    CxxRules *r _AND_
    ModuleList *mods _AND_
    Module *m)
{

    if (!m->hasAnys)
        return;

    PrintCxxAnyEnum (hdr, m, r);
    PrintCxxAnyHashInitRoutine (src, hdr, mods, m, r);

}  /* PrintAnyCode */



void
PrintCxxAnyEnum PARAMS ((hdr, m, r),
    FILE *hdr _AND_
    Module *m _AND_
    CxxRules *r)
{
    TypeDef *td;
    AnyRef *ar;
    AnyRefList *arl;
    int firstPrinted = TRUE;
    int i;
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

#ifndef VDADER_RULES
    if (firstPrinted) /* none have been printed */
        fprintf (hdr,"/* NO INTEGER or OBJECT IDENTIFIER to ANY type relationships were defined (via MACROs or other mechanism) */\n ??? \n");
#endif

    fprintf (hdr,"\n} %sAnyId;\n\n\n", modName);
    Free (modName);

}  /* PrintAnyEnum */


void
PrintCxxAnyHashInitRoutine PARAMS ((src, hdr, mods, m, r),
    FILE *src _AND_
    FILE *hdr _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    CxxRules *r)
{
    TypeDef *td;
    AnyRefList *arl;
    AnyRef *ar;
    CxxTDI *cxxtdi;
    int i;
    int j;
    enum BasicTypeChoiceId typeId;
    int installedSomeHashes = FALSE;


#ifndef VDADER_RULES
    /* print InitAny class src file */
    fprintf (src,"// this class will automatically intialize the any hash tbl\n");
    fprintf (src,"class InitAny\n");
    fprintf (src,"{\n");
    fprintf (src,"  public:\n");
    fprintf (src,"    InitAny();\n");
    fprintf (src,"};\n\n");

    fprintf (src,"static InitAny anyInitalizer;\n");

    /* print constructor method that build hash tbl to src file*/
    fprintf (src,"InitAny::InitAny()\n");
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
                    fprintf (src,"    %s oid%d", r->typeConvTbl[BASICTYPE_OID].className, i++);
                    PrintCxxOidValue (src, r, ar->id->a.oid);
                    fprintf (src,";\n");
                }
                else if (ar->id->choiceId == OIDORINT_INTID)
                {
                    fprintf (src,"    %s int%d", r->typeConvTbl[BASICTYPE_INTEGER].className, i++);
                    PrintCxxIntValue (src, r, ar->id->a.intId);
                    fprintf (src,";\n");
                }
            }
        }
    }


    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (td->anyRefs != NULL)
        {
            cxxtdi = td->cxxTypeDefInfo;
            FOR_EACH_LIST_ELMT (ar, td->anyRefs)
            {
                installedSomeHashes = TRUE;
                if (ar->id->choiceId == OIDORINT_OID)
                {
                    fprintf (src,"    %s oid%d", r->typeConvTbl[BASICTYPE_OID].className, i++);
                    PrintCxxOidValue (src, r, ar->id->a.oid);
                    fprintf (src,";\n");
                }
                else if (ar->id->choiceId == OIDORINT_INTID)
                {
                    fprintf (src,"    %s int%d", r->typeConvTbl[BASICTYPE_INTEGER].className, i++);
                    PrintCxxIntValue (src, r, ar->id->a.intId);
                    fprintf (src,";\n");
                }
            }
        }
    }


    /* now print hash init calls */
    i = 0;
    for (j = BASICTYPE_BOOLEAN; j < BASICTYPE_MACRODEF; j++)
    {
        arl = LIBTYPE_GET_ANY_REFS (j);
        if (arl != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, arl)
            {
                if (ar->id->choiceId == OIDORINT_OID)
                    fprintf (src,"    AsnAny::InstallAnyByOid (oid%d, %s, new %s);\n", i++, ar->anyIdName, r->typeConvTbl[j].className);

                else
                    fprintf (src,"    AsnAny::InstallAnyByInt (int%d, %s, new %s);\n", i++, ar->anyIdName, r->typeConvTbl[j].className);

            }
        }
    }

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        if (td->anyRefs != NULL)
        {
            FOR_EACH_LIST_ELMT (ar, td->anyRefs)
            {
                cxxtdi = td->cxxTypeDefInfo;

                if (ar->id->choiceId == OIDORINT_OID)
                    fprintf (src,"    AsnAny::InstallAnyByOid (oid%d, %s, new %s);\n", i++, ar->anyIdName, cxxtdi->className);

                else
                    fprintf (src,"    AsnAny::InstallAnyByInt (int%d, %s, new %s);\n", i++, ar->anyIdName, cxxtdi->className);

            }
        }
    }

    if (!installedSomeHashes)
    {
        fprintf (src,"    /* Since no INTEGER/OID to ANY type relations were defined\n");
        fprintf (src,"     * (usually done via MACROs) you must manually do the code\n");
        fprintf (src,"     * to fill the hash tbl.\n");
        fprintf (src,"     * if the ids are INTEGER use the following:\n");
        fprintf (src,"     * AsnAny::InstallAnyByInt (3, ??_ANY_ID, new <className>);\n");
        fprintf (src,"     * if the ids are OBJECT IDENTIFIERs use the following:\n");
        fprintf (src,"     * AsnAny::InstallAnyByOid (OidValue, ??_ANY_ID, new <className>);\n");
        fprintf (src,"     * put the ??_ANY_IDs in the AnyId enum.\n\n");
        fprintf (src,"     * For example if you have some thing like\n");
        fprintf (src,"     * T1 ::= SEQUENCE { id INTEGER, ANY DEFINED BY id }\n");
        fprintf (src,"     * and the id 1 maps to the type BOOLEAN use the following:\n");
        fprintf (src,"     * AsnAny::InstallAnyByInt (1, SOMEBOOL_ANY_ID, new AsnBool);\n");
        fprintf (src,"     */\n ???????\n");  /* generate compile error */
        fprintf (src,"    /* VDADER_RULES is selected UPDATE THIS COMMENT\n");
        fprintf (src,"     */\n");
    }


    fprintf (src,"}  /* InitAny::InitAny */\n\n\n");
#endif

}  /* PrintAnyHashInitRoutine */
