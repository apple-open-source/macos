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
 * compiler/back-ends/c-gen/gen-print.c - routines for printing C hierachical print routines
 *
 * Mike Sample
 * 92/04
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/Attic/gen-print.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-print.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:42  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:43:18  rj
 * file name has been shortened for redundant part: c-gen/gen-c-print -> c-gen/gen-print.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:23:43  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:28  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "define.h"
#include "rules.h"
#include "type-info.h"
#include "str-util.h"
#include "util.h"
#include "gen-print.h"

static char *returnTypeG = "void";
static char *valueArgNameG = "v";
static char *fileTypeNameG = "FILE*";
static char *indentTypeNameG = "unsigned short int";
static CRules *genPrintCRulesG;
/* non-exported prototypes */

static void PrintCPrintPrototype PROTO ((FILE *hdr, TypeDef *td));
static void PrintCPrintDeclaration PROTO ((FILE *src, TypeDef *td));
static void PrintCPrintDefine PROTO ((FILE *hdr, TypeDef *td));
static void PrintCPrintLocals PROTO ((FILE *src,TypeDef *td));
/*
static void PrintCPrintElmts PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *elmts, char *varName));
*/
static void PrintCChoiceElmtPrint PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *elmts, NamedType *e, char *varName));


static void PrintCElmtPrintWithIndent PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *elmts, NamedType *e, char *varName, int allOpt));

static void PrintCChoicePrintRoutine PROTO ((FILE *src, FILE *hdr, CRules *r, ModuleList *mods, Module *m, TypeDef *td));

static void PrintCSetPrintRoutine  PROTO ((FILE *src, FILE *hdr, CRules *r, ModuleList *mods, Module *m, TypeDef *td));
static void PrintCSeqPrintRoutine  PROTO ((FILE *src, FILE *hdr, CRules *r, ModuleList *mods, Module *m, TypeDef *td));
static void PrintCSeqOfPrintRoutine PROTO ((FILE *src, FILE *hdr, CRules *r, ModuleList *mods, Module *m, TypeDef *td));
static void PrintCSetOfPrintRoutine PROTO ((FILE *src, FILE *hdr, CRules *r, ModuleList *mods, Module *m, TypeDef *td));



void
PrintCPrinter PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    if ((td->cTypeDefInfo == NULL) || !(td->cTypeDefInfo->genPrintRoutine))
        return;

    genPrintCRulesG = r;
    switch (td->type->basicType->choiceId)
    {
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
            PrintCPrintDefine (hdr, td);
            fprintf (hdr, "\n\n");
            break;

        case BASICTYPE_SETOF:
            PrintCSetOfPrintRoutine (src, hdr, r, mods, m, td);
            break;

        case BASICTYPE_SEQUENCEOF:
            PrintCSeqOfPrintRoutine (src, hdr, r, mods, m, td);
            break;

        case BASICTYPE_CHOICE:
            PrintCChoicePrintRoutine (src, hdr, r, mods, m, td);
            break;

        case BASICTYPE_SET:
            PrintCSetPrintRoutine (src, hdr, r, mods, m, td);
            break;


        case BASICTYPE_SEQUENCE:
            PrintCSeqPrintRoutine (src, hdr, r, mods, m, td);
            break;

        default:
            break;
    }
}  /*  PrintCPrint */


/*
 * Prints prototype for encode routine in hdr file
 */
static void
PrintCPrintPrototype PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi = td->cTypeDefInfo;
    fprintf (hdr,"%s %s PROTO ((%s f, %s *v, %s indent));\n", returnTypeG, ctdi->printRoutineName, fileTypeNameG, ctdi->cTypeName, indentTypeNameG);

}  /*  PrintCPrintPrototype */



/*
 * Prints declarations of encode routine for the given type def
 */
static void
PrintCPrintDeclaration PARAMS ((src, td),
    FILE *src _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi =  td->cTypeDefInfo;
    fprintf (src,"%s\n%s PARAMS ((f, v, indent),\n%s f _AND_\n%s *v _AND_\n%s indent)\n", returnTypeG, ctdi->printRoutineName, fileTypeNameG, ctdi->cTypeName, indentTypeNameG);

}  /*  PrintCPrintDeclaration */




static void
PrintCPrintDefine PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    fprintf(hdr, "#define %s %s", td->cTypeDefInfo->printRoutineName, td->type->cTypeRefInfo->printRoutineName);
/*
    fprintf(hdr, "#define %s(f, v, indent)  ", td->cTypeDefInfo->printRoutineName);
    fprintf (hdr, "%s (f, v, indent)", td->type->cTypeRefInfo->printRoutineName);
*/
}  /*  PrintCPrintDefine */




static void
PrintCPrintLocals PARAMS ((src, td),
    FILE *src _AND_
    TypeDef *td)
{
    /* none yet */
}  /*  PrintCPrintLocals */


/*
static void
PrintCPrintElmts PARAMS ((src, td, parent, elmts, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    char *varName)
{
    NamedType *e;


    FOR_EACH_LIST_ELMT (e, elmts)
        PrintCElmtPrint (src, td, parent, elmts, e, varName);
}  PrintCBerElmtsEncodeCode */



/*
 * Prints code for printing a CHOICE element
 *
 */
static void
PrintCChoiceElmtPrint PARAMS ((src, td, parent, elmts,  e, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    NamedType *e _AND_
    char *varName)
{
    CTRI *ctri;
    char elmtVarRef[MAX_VAR_REF];
    Type *tmpType;
    int inTailOpts;

    ctri =  e->type->cTypeRefInfo;


    /* build ref to the elmt */
    MakeVarPtrRef (genPrintCRulesG, td, parent, e->type, varName, elmtVarRef);

    if (e->fieldName != NULL)
    {
        fprintf (src,"    fprintf (f,\"%s \");\n", e->fieldName);
        fprintf (src,"    %s (f, %s, indent + stdIndentG);\n", e->type->cTypeRefInfo->printRoutineName, elmtVarRef);
    }
    else
    {
        fprintf (src,"    %s (f, %s, indent + stdIndentG);\n", e->type->cTypeRefInfo->printRoutineName, elmtVarRef);
    }

}  /*  PrintCChoiceElmtPrint */

/*
 * Prints code for printing an elmt of a SEQ or SET
 *
 * Does funny things to print commas correctly
 * eg for the following type
 * Foo ::= SET
 * {
 *      A,                   --> print   A ",\n"
 *      B,                               B ",\n"
 *      C OPTIONAL,                      C ",\n" if C present
 *      D,                               D ",\n"
 *      E,                               E ",\n"
 *      F,                               F       <- nothing after last non-opt
 *                                                  before tail opts.
 *      G OPTIONAL,                      ",\n" G
 *      H OPTIONAL                       ",\n" H "\n"
 * }

 */
static void
PrintCElmtPrintWithIndent PARAMS ((src, td, parent, elmts, e, varName, allOpt),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    NamedType *e _AND_
    char *varName _AND_
    int allOpt)
{
    CTRI *ctri;
    char elmtVarRef[MAX_VAR_REF];
    Type *tmpType;
    int inTailOpts;

    ctri =  e->type->cTypeRefInfo;

    /* this assumes the elmts->curr == e */
    inTailOpts = IsTailOptional (elmts);

    /* build ref to the elmt */
    MakeVarPtrRef (genPrintCRulesG, td, parent, e->type, varName, elmtVarRef);

    /* if optional then put in NULL check */
    if (e->type->optional || (e->type->defaultVal != NULL))
        fprintf (src, "    if (%s (%s))\n    {\n", ctri->optTestRoutineName, elmtVarRef);

    if (allOpt)
    {
        if (e != FIRST_LIST_ELMT (elmts))
        {
            fprintf (src, "    if (!nonePrinted)\n");
            fprintf (src, "        fprintf (f,\",\\n\");\n");
        }
        fprintf (src, "    nonePrinted = FALSE;\n");
    }
    else if ((inTailOpts) && (e != FIRST_LIST_ELMT (elmts)))
        fprintf (src, "    fprintf (f,\",\\n\");\n");

    fprintf (src,"    Indent (f, indent + stdIndentG);\n");

    if (e->fieldName != NULL)
        fprintf (src,"    fprintf (f,\"%s \");\n", e->fieldName);

    fprintf (src,"    %s (f, %s, indent + stdIndentG);\n", e->type->cTypeRefInfo->printRoutineName, elmtVarRef);

    if ((e != LAST_LIST_ELMT (elmts)) &&
         (!inTailOpts) &&
         (!NextIsTailOptional (elmts)))
        fprintf (src,"    fprintf (f, \",\\n\");\n");


    /* write closing brkt for NULL check for optional elmts */
    if (e->type->optional || (e->type->defaultVal != NULL))
        fprintf (src, "    }\n");

    if (e == LAST_LIST_ELMT (elmts))
        fprintf (src,"    fprintf (f,\"\\n\");\n");

}  /*  PrintCElmtPrintWithIndent */


static void
PrintCChoicePrintRoutine PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;

    PrintCPrintPrototype (hdr,td);

    PrintCPrintDeclaration (src, td);
    fprintf (src,"{\n");
    PrintCPrintLocals (src,td);
    fprintf (src,"    switch (%s->%s)\n", valueArgNameG, td->type->cTypeRefInfo->choiceIdEnumFieldName);
    fprintf (src,"    {\n");

    FOR_EACH_LIST_ELMT (e, td->type->basicType->a.choice)
    {
        fprintf (src,"      case %s:\n",e->type->cTypeRefInfo->choiceIdSymbol);
        fprintf (src,"      ");
        PrintCChoiceElmtPrint (src, td, td->type, td->type->basicType->a.choice, e, valueArgNameG);
        fprintf (src,"          break;\n\n");
    }
    fprintf (src,"    }\n");
/*    fprintf (src,"    fprintf (f,\"\\n\");\n"); */

    fprintf (src,"}  /* %s */\n\n", td->cTypeDefInfo->printRoutineName);

} /* PrintCChoicePrintRoutine */



static void
PrintCSetPrintRoutine PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;
    int allOpt;

    PrintCPrintPrototype (hdr,td);

    PrintCPrintDeclaration (src, td);
    fprintf (src,"{\n");
    PrintCPrintLocals (src,td);

    allOpt = AllElmtsOptional (td->type->basicType->a.set);
    /*
     * print extra local variable so commas are handled correctly
     * when all elements are optional
     */
    if (allOpt)
        fprintf (src,"    int nonePrinted = TRUE;\n\n");

    fprintf (src,"    if (%s == NULL)\n", valueArgNameG);
    fprintf (src,"        return;\n\n");

    fprintf (src,"    fprintf (f,\"{ -- SET --\\n\");\n\n");


    FOR_EACH_LIST_ELMT (e, td->type->basicType->a.set)
    {
        PrintCElmtPrintWithIndent (src, td, td->type, td->type->basicType->a.set, e, valueArgNameG, allOpt);
    }
    fprintf (src,"    Indent (f, indent);\n");
    fprintf (src,"    fprintf (f,\"}\");\n");

    fprintf (src,"}  /* %s */\n\n", td->cTypeDefInfo->printRoutineName);

} /* PrintCSetPrintRoutine */



static void
PrintCSeqPrintRoutine PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;
    int allOpt;

    PrintCPrintPrototype (hdr,td);

    PrintCPrintDeclaration (src, td);
    fprintf (src,"{\n");
    PrintCPrintLocals (src,td);

    allOpt = AllElmtsOptional (td->type->basicType->a.set);
    /*
     * print extra local variable so commas are handled correctly
     * when all elements are optional
     */
    if (allOpt)
        fprintf (src,"    int nonePrinted = TRUE;\n\n");

    fprintf (src,"    if (%s == NULL)\n", valueArgNameG);
    fprintf (src,"        return;\n\n");

    fprintf (src,"    fprintf (f,\"{ -- SEQUENCE --\\n\");\n\n");

    FOR_EACH_LIST_ELMT (e, td->type->basicType->a.sequence)
    {
        PrintCElmtPrintWithIndent (src, td, td->type, td->type->basicType->a.sequence, e, valueArgNameG, allOpt);
    }
    fprintf (src,"    Indent (f, indent);\n");
    fprintf (src,"    fprintf (f,\"}\");\n");

    fprintf (src,"}  /* %s */\n\n", td->cTypeDefInfo->printRoutineName);
} /* PrintCSeqPrintRoutine */



static void
PrintCSetOfPrintRoutine PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;

    PrintCPrintPrototype (hdr,td);

    PrintCPrintDeclaration (src, td);
    fprintf (src,"{\n");
    PrintCPrintLocals (src,td);

    fprintf (src,"    %s *tmp;\n", td->type->basicType->a.setOf->cTypeRefInfo->cTypeName);

    fprintf (src,"    if (%s == NULL)\n", valueArgNameG);
    fprintf (src,"        return;\n");

    fprintf (src,"    fprintf (f,\"{ -- SET OF -- \\n\");\n");

    fprintf (src,"    FOR_EACH_LIST_ELMT (tmp, %s)\n", valueArgNameG);
    fprintf (src,"    {\n");
    fprintf (src,"        Indent (f, indent+ stdIndentG);\n");
    fprintf (src,"        %s (f, tmp, indent + stdIndentG);\n", td->type->basicType->a.setOf->cTypeRefInfo->printRoutineName);
    fprintf (src,"        if (tmp != (%s*)LAST_LIST_ELMT (%s))\n", td->type->basicType->a.setOf->cTypeRefInfo->cTypeName, valueArgNameG);
    fprintf (src,"            fprintf (f,\",\\n\");\n");
    fprintf (src,"    }\n");
    fprintf (src,"    fprintf (f,\"\\n\");\n");
    fprintf (src,"    Indent (f, indent);\n");
    fprintf (src,"    fprintf (f,\"}\");\n");

    fprintf (src,"}  /* %s */\n\n", td->cTypeDefInfo->printRoutineName);

} /* PrintCSetOfPrintRoutine */

static void
PrintCSeqOfPrintRoutine PARAMS ((src, hdr, r, mods, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;

    PrintCPrintPrototype (hdr,td);

    PrintCPrintDeclaration (src, td);
    fprintf (src,"{\n");
    PrintCPrintLocals (src,td);

    fprintf (src,"    %s *tmp;\n", td->type->basicType->a.setOf->cTypeRefInfo->cTypeName);

    fprintf (src,"    if (%s == NULL)\n", valueArgNameG);
    fprintf (src,"        return;\n");

    fprintf (src,"    fprintf (f,\"{ -- SEQUENCE OF -- \\n\");\n");

    fprintf (src,"    FOR_EACH_LIST_ELMT (tmp, %s)\n", valueArgNameG);
    fprintf (src,"    {\n");
    fprintf (src,"        Indent (f, indent+ stdIndentG);\n");
    fprintf (src,"        %s (f, tmp, indent + stdIndentG);\n", td->type->basicType->a.setOf->cTypeRefInfo->printRoutineName);
    fprintf (src,"        if (tmp != (%s*)LAST_LIST_ELMT (%s))\n", td->type->basicType->a.setOf->cTypeRefInfo->cTypeName, valueArgNameG);
    fprintf (src,"            fprintf (f,\",\\n\");\n");
    fprintf (src,"    }\n");
    fprintf (src,"    fprintf (f,\"\\n\");\n");
    fprintf (src,"    Indent (f, indent);\n");
    fprintf (src,"    fprintf (f,\"}\");\n");

    fprintf (src,"}  /* %s */\n\n", td->cTypeDefInfo->printRoutineName);

} /* PrintCSeqOfPrintRoutine */
