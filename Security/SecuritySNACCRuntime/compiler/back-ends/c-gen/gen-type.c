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
 * compiler/back-ends/c-gen/gen-type.c - routines for printing c types from  ASN.1 from type trees
 *
 * Mike Sample
 * 91/09/26
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/gen-type.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-type.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:43  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:43:44  rj
 * file name has been shortened for redundant part: c-gen/gen-c-type -> c-gen/gen-type.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:23:58  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:31  rj
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
#include "gen-type.h"


/* non-exported prototypes */

static void PrintCType PROTO ((FILE *f,  CRules *r, Module *m, TypeDef *td, Type *parent, Type *t));

static void PrintCStructElmts PROTO ((FILE *f, CRules *r, Module *m, TypeDef *td, Type *parent, Type *t));


static void PrintCChoiceIdEnum PROTO ((FILE *f, CRules *r, Module *m, TypeDef *td, Type *parent, Type *t));

static void PrintCChoiceUnion  PROTO ((FILE *f, CRules *r, Module *m, TypeDef *td, Type *parent, Type *t));

static void PrintCChoiceTypeDef PROTO ((FILE *f, CRules *r, Module *m, TypeDef *td));

static void PrintTypeComment PROTO ((FILE *f, TypeDef *head, Type *t));


static void PrintPreTypeDefStuff PROTO ((FILE *f, CRules *r, Module *m, TypeDef *td, Type *parent, Type *t));



void
PrintCTypeDef PARAMS ((f, r, m, td),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td)
{
    CTRI *ctri;
    CTDI *ctdi;
    Type *t;

    ctdi = td->cTypeDefInfo;
    if ((ctdi == NULL) || (!ctdi->genTypeDef))
        return;

    t = td->type;
    ctri = t->cTypeRefInfo;

    PrintPreTypeDefStuff (f, r, m, td, NULL, t);

    switch (ctri->cTypeId)
    {
        case C_TYPEREF:
        case C_LIB:
        case C_ANY:
        case C_ANYDEFINEDBY:
        case C_LIST:
            fprintf (f, "typedef ");
            PrintCType (f, r, m, td, NULL, t);
            fprintf (f, " %s;", ctdi->cTypeName);
            PrintTypeComment (f, td, t);
            fprintf (f, "\n\n");
            break;


        case C_CHOICE:
            PrintCChoiceTypeDef (f, r,  m, td);
            break;

        case C_STRUCT:
            fprintf (f, "typedef ");
            fprintf (f,"%s %s", "struct", t->cTypeRefInfo->cTypeName);
            PrintTypeComment (f, td, t);
            fprintf (f,"\n{\n");
            PrintCStructElmts (f, r, m, td, NULL, t);
            fprintf (f, "} %s;", ctdi->cTypeName);
            fprintf (f, "\n\n");
            break;

        default:
            break;
        /* else do nothing - some unprocessed or unknown type (macros etc) */
    }

}  /* PrintCTypeDef */



static void
PrintCType PARAMS ((f, r, m, td, parent, t),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    CTRI *ctri;
    CNamedElmt *n;

    ctri = t->cTypeRefInfo;

    if (ctri == NULL)
        return;


    switch (ctri->cTypeId)
    {
        case C_TYPEREF:
            /*
             * put struct in front of def if
             * defined from a struct type (set/seq/choice)
             * but only if not a ref of a ref
             */
            if ((t->basicType->a.localTypeRef->link->type->cTypeRefInfo->cTypeId == C_STRUCT)||
                 (t->basicType->a.localTypeRef->link->type->cTypeRefInfo->cTypeId == C_CHOICE))
            {
                fprintf (f,"struct ");
            }

            fprintf (f,"%s", ctri->cTypeName);

            if (ctri->isPtr)
                fprintf (f,"*");
            break;

        case C_ANY:
            fprintf (f,"/* ANY- Fix Me ! */\n");
        case C_ANYDEFINEDBY:
        case C_LIST:
        case C_LIB:
            fprintf (f,"%s", ctri->cTypeName);
            /*
             * print enum constant defs
             */
            if ((ctri->cNamedElmts != NULL) &&
                (t->basicType->choiceId  == BASICTYPE_ENUMERATED))
            {
                fprintf (f, "\n    {\n");

                FOR_EACH_LIST_ELMT (n, ctri->cNamedElmts)
                {
                    fprintf (f,"        %s = %d", n->name, n->value);
                    if (n != (CNamedElmt*)LAST_LIST_ELMT (ctri->cNamedElmts))
                        fprintf (f,",");

                    fprintf (f,"\n");
                }
                fprintf (f, "    }");
            }

            if (ctri->isPtr)
               fprintf (f,"*");
            break;

       default:
           break;
             /* nothing */
    }

}  /* PrintCType */


static void
PrintCStructElmts PARAMS ((f, r, m, td, parent, t),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    CTRI *ctri;
    NamedType *et;
    NamedTypeList *elmts;

    elmts = t->basicType->a.sequence;

    if ((elmts == NULL) || (LIST_EMPTY (elmts)))
    {
        fprintf (f, "    char unused; /* empty ASN1 SET/SEQ - not used */\n");
    }

    FOR_EACH_LIST_ELMT (et, elmts)
    {

        ctri =  et->type->cTypeRefInfo;
        fprintf (f,"    ");  /* cheap, fixed indent */
        PrintCType (f, r, m, td, t, et->type);
        fprintf (f, " %s;", ctri->cFieldName);
        PrintTypeComment (f, td, et->type);
        fprintf (f, "\n");
    }
}  /* PrintCStructElmts */



static void
PrintCChoiceIdEnum PARAMS ((f, r, m, td, parent, t),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    NamedType *et;
    NamedType *last;
    CTRI *ctri;

    ctri =  t->cTypeRefInfo;
    fprintf (f, "    enum %s\n    {\n", ctri->choiceIdEnumName);

    if ((t->basicType->a.choice != NULL) &&
        !(LIST_EMPTY (t->basicType->a.choice)))
        last = (NamedType*)LAST_LIST_ELMT (t->basicType->a.choice);

    FOR_EACH_LIST_ELMT (et, t->basicType->a.choice)
    {
        ctri =  et->type->cTypeRefInfo;
        fprintf (f,"        %s", ctri->choiceIdSymbol);
        if (et == last)
            fprintf (f, "\n");
        else
            fprintf (f, ",\n");
    }

    ctri =  t->cTypeRefInfo;
    fprintf (f, "    } %s;", ctri->choiceIdEnumFieldName);

}  /* PrintCChoiceIdEnum */


static void
PrintCChoiceUnion PARAMS ((f, r, m, td, parent, t),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    CTRI *ctri;
    ctri = t->cTypeRefInfo;

    fprintf (f,"    union %s\n    {\n",  ctri->cTypeName);
    PrintCStructElmts (f, r, m, td, parent, t);
    fprintf (f, "    }");
}  /* PrintCChoiceUnion */


static void
PrintCChoiceTypeDef PARAMS ((f, r, m, td),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td)
{
    CTRI *ctri;
    char *choiceName;
    Type *t;

    t = td->type;
    ctri =  t->cTypeRefInfo;
    choiceName = td->cTypeDefInfo->cTypeName;

    fprintf (f, "typedef ");
    fprintf (f, "struct %s", choiceName);
    PrintTypeComment (f, td, t);
    fprintf (f,"\n{\n");
    PrintCChoiceIdEnum (f, r, m, td, NULL, t);
    fprintf (f,"\n");
    PrintCChoiceUnion (f, r, m, td, NULL, t);
    fprintf (f, " %s;", ctri->cFieldName);
    fprintf (f,"\n} %s;\n\n", choiceName);
}  /* PrintCChoiceDef */



/*
 * used to print snippet of the defining ASN.1  after the
 * C type.
 */
static void
PrintTypeComment PARAMS ((f, td, t),
    FILE *f _AND_
    TypeDef *td _AND_
    Type *t)
{
    fprintf (f," /* ");
    SpecialPrintType (f, td, t);
    fprintf (f," */");
}



/*
 * print any #defines for integers/bits with named elements
 * (currenly only the first option will fire due to the
 *  steps taken in normalize.c)
 */
static void
PrintPreTypeDefStuff PARAMS ((f, r, m, td, parent, t),
    FILE *f _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t)
{
    CTRI *ctri;
    NamedType *et;
    CNamedElmt *n;

    ctri = td->type->cTypeRefInfo;

    /*
     * print defined stmts for non enumerated type with named elmts
     */
    if ((ctri->cNamedElmts != NULL) &&
        (t->basicType->choiceId  != BASICTYPE_ENUMERATED))
    {
        FOR_EACH_LIST_ELMT (n, ctri->cNamedElmts)
        {
            fprintf(f, "\n#define %s %d", n->name, n->value);
        }
        fprintf (f, "\n\n");
    }

    else if ((t->basicType->choiceId == BASICTYPE_SET) ||
             (t->basicType->choiceId == BASICTYPE_SEQUENCE) ||
             (t->basicType->choiceId == BASICTYPE_CHOICE))
    {

        FOR_EACH_LIST_ELMT (et, t->basicType->a.set)
            PrintPreTypeDefStuff (f, r, m, td, t, et->type);
    }

    else if ((t->basicType->choiceId == BASICTYPE_SETOF) ||
             (t->basicType->choiceId == BASICTYPE_SEQUENCEOF))
    {
        PrintPreTypeDefStuff (f, r,  m, td, t, t->basicType->a.setOf);
    }
}  /* PrintPreTypeDefStuff */
