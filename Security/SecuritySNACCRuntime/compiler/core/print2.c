/*
 * compiler/core/print.c
 *
 *  These routines are for printing the information from a Module
 *  Data strucuture in ASN.1 form.
 *
 *  Useful for debugging the parser and seeing changes caused by
 *  normalization and sorting.
 *
 * Mike Sample
 * Feb 28/91
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/print2.c,v 1.1 2001/06/20 21:27:58 dmitch Exp $
 * $Log: print2.c,v $
 * Revision 1.1  2001/06/20 21:27:58  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:52  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.6  1997/02/28 13:39:55  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.5  1995/08/17 14:58:57  rj
 * minor typographic change
 *
 * Revision 1.4  1995/07/25  19:41:42  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1994/10/08  03:48:53  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.2  1994/09/01  00:42:16  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:32  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "lib-types.h"
#include "print.h"


static int  indentCountG;
static int  indentG = 0;
static int  indentStepG =  4;

#define INDENT(f, i)\
    for (indentCountG = 0; indentCountG < (i); indentCountG++)\
        fputc (' ', (f))\

/*
 * Prints the given Module *, mod, to the given FILE *f in
 * ASN.1 format
 */
void
PrintModule PARAMS ((f, mod),
    FILE *f _AND_
    Module *mod)
{

   if (mod->status == MOD_ERROR)
   {
       fprintf (f, "WARNING: this module contains errors\n");
       fprintf (f,"(probably some type/value is referenced but is not defined or imported)\n");
       fprintf (f,"The prog. may croak, cross your fingers!\n");
   }


   fprintf (f, "%s ",mod->modId->name);
   PrintOid (f, mod->modId->oid);

   fprintf (f, "\nDEFINITIONS ");

   if (mod->tagDefault == EXPLICIT_TAGS)
       fprintf (f, "EXPLICIT TAGS");

   else if (mod->tagDefault == IMPLICIT_TAGS)
       fprintf (f, "IMPLICIT TAGS");
   else
       fprintf (f, "\n\n -- compiler error unknown tag default");


   fprintf (f, " ::=\nBEGIN\n\n");



   PrintExports (f, mod);

   PrintImportLists (f, mod->imports);

   PrintTypeDefs (f, mod->typeDefs);
   PrintValueDefs (f, mod->valueDefs);

   fprintf (f, "END\n");

}  /* PrintModule */


void
PrintExports PARAMS ((f, m),
    FILE *f _AND_
    Module *m)
{
    TypeDef *td;
    ValueDef *vd;
    int first;

    if (m->exportStatus == EXPORTS_ALL)
    {
        fprintf (f, "\n\n-- exports everything\n\n");
    }
    else if  (m->exportStatus == EXPORTS_NOTHING)
    {
        fprintf (f, "\n\nEXPORTS   -- exports nothing\n\n");
    }
    else
    {
        fprintf (f, "\n\nEXPORTS\n");
        first = 1;
        FOR_EACH_LIST_ELMT (td, m->typeDefs)
            if (td->exported)
            {
                if (!first)
                    fprintf (f,", ");
                fprintf (f, "%s", td->definedName);
                first = 0;
            }

        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
            if (vd->exported)
            {
                if (!first)
                    fprintf (f,", ");
                fprintf (f, "%s", vd->definedName);
                first = 0;
            }

        fprintf (f, "\n;\n\n");
    }
}  /* PrintExports */



void
PrintOid PARAMS ((f, oid),
    FILE *f _AND_
    OID *oid)
{
    int i;

    if (oid == NULL)
       return;

    fprintf (f, "{ ");
    for (;  oid != NULL; oid = oid->next)
    {
        /*
         *  value ref to an integer or if first elmt in
         *  oid can ref other oid value
         *  { id-asdc }
         */
        if (oid->valueRef != NULL)
            PrintValue (f, NULL, NULL, oid->valueRef);

        /*
         * just  "arcNum" format
         *  { 2 }
         */
        else if (oid->arcNum != NULL_OID_ARCNUM)
            fprintf (f, "%d", oid->arcNum);


        fprintf (f, " ");
    }
    fprintf (f, "}");

}  /* PrintOid */



void
PrintImportElmt PARAMS ((f, impElmt),
    FILE *f _AND_
    ImportElmt *impElmt)
{
     fprintf (f, "%s",impElmt->name);
}   /* PrintImportElmt */


void
PrintImportElmts PARAMS ((f, impElmtList),
    FILE *f _AND_
    ImportElmtList *impElmtList)
{
    ImportElmt *ie;
    ImportElmt *last;

    if ((impElmtList == NULL) || (LIST_EMPTY (impElmtList)))
        return;

    last = (ImportElmt*)LAST_LIST_ELMT (impElmtList);
    FOR_EACH_LIST_ELMT (ie, impElmtList)
    {
        PrintImportElmt (f, ie);

        if (ie != last)
            fprintf (f, ", ");
    }

}  /* PrintImportElmts */



void
PrintImportLists PARAMS ((f, impLists),
    FILE *f _AND_
    ImportModuleList *impLists)
{
   ImportModule *impMod;

   if (impLists == NULL)
   {
       fprintf (f,"\n\n-- imports nothing\n\n");
       return;
   }

   fprintf (f, "IMPORTS\n\n");
   FOR_EACH_LIST_ELMT (impMod, impLists)
   {
       PrintImportElmts (f, impMod->importElmts);

       fprintf (f, "\n    FROM %s ", impMod->modId->name);

       PrintOid (f, impMod->modId->oid);

       fprintf (f, "\n\n\n");
   }
   fprintf (f, ";\n\n\n");

}  /* PrintImportLists */



void
PrintTypeDefs PARAMS ((f, typeDefs),
    FILE *f _AND_
    TypeDefList *typeDefs)
{
    TypeDef *td;

    FOR_EACH_LIST_ELMT (td, typeDefs)
    {
        if (td->type->basicType->choiceId == BASICTYPE_MACRODEF)
            PrintMacroDef (f, td);
        else
        {
            fprintf (f,"-- %s notes: ", td->definedName);

            if (td->recursive)
                fprintf (f,"recursive, ");
            else
                fprintf (f,"not recursive, ");

            if (td->exported)
                fprintf (f,"exported,\n");
            else
                fprintf (f,"not exported,\n");

            fprintf (f,"-- locally refd %d times, ", td->localRefCount);
            fprintf (f,"import refd %d times\n", td->importRefCount);


            fprintf (f, "%s ::= ", td->definedName);
            PrintType (f, td, td->type);
        }
        fprintf (f, "\n\n\n");
    }
}  /* PrintTypeDefs */




void
PrintType PARAMS ((f, head, t),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t)
{
    Tag *tag;
    Tag *lastTag;

    if (t == NULL)
        return;

    lastTag = NULL;
    FOR_EACH_LIST_ELMT (tag, t->tags)
    {



        if (! ((tag->tclass == UNIV) &&
               (tag->code == LIBTYPE_GET_UNIV_TAG_CODE (t->basicType->choiceId))))
        {
            PrintTag (f, tag);
            fprintf (f, " ");
        }
        lastTag = tag;
    }

    /*
     * check type has been implicitly tagged
     */
    if (t->implicit)
        fprintf (f, "IMPLICIT ");

    PrintBasicType (f, head, t, t->basicType);


    /*
     * sequences of and set of print subtypes a special way
     * so ignore them here
     */
    if ((t->subtypes != NULL) &&
        (t->basicType->choiceId != BASICTYPE_SETOF) &&
        (t->basicType->choiceId != BASICTYPE_SEQUENCEOF))
    {
        fprintf (f," ");
        PrintSubtype (f, head, t, t->subtypes);
    }


    if (t->defaultVal != NULL)
    {
        fprintf (f, " DEFAULT ");
        if (t->defaultVal->fieldName != NULL)
            fprintf (f, "%s ", t->defaultVal->fieldName);
        PrintValue (f, NULL, t, t->defaultVal->value);
    }

    else if (t->optional)
        fprintf (f, " OPTIONAL");


#ifdef DEBUG
    fprintf (f, "  -- lineNo = %d --", t->lineNo);
#endif

}  /* PrintType */


void
PrintBasicType PARAMS ((f, head, t, bt),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt)
{
    switch (bt->choiceId)
    {

        case BASICTYPE_SEQUENCE:
            fprintf (f, "SEQUENCE\n");
            INDENT (f, indentG);
            fprintf (f,"{\n");
            indentG += indentStepG;
            INDENT (f, indentG);
            PrintElmtTypes (f, head, t, bt->a.sequence);
            indentG -= indentStepG;
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f, "}");
            break;

        case BASICTYPE_SET:
            fprintf (f, "SET\n");
            INDENT (f, indentG);
            fprintf (f,"{\n");
            indentG += indentStepG;
            INDENT (f, indentG);
            PrintElmtTypes (f, head, t, bt->a.set);
            indentG -= indentStepG;
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f, "}");
            break;

        case BASICTYPE_CHOICE:
            fprintf (f, "CHOICE\n");
            INDENT (f, indentG);
            fprintf (f,"{\n");
            indentG += indentStepG;
            INDENT (f, indentG);
            PrintElmtTypes (f, head, t, bt->a.choice);
            indentG -= indentStepG;
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f, "}");
            break;



        case BASICTYPE_SEQUENCEOF:
            fprintf (f, "SEQUENCE ");
            if (t->subtypes != NULL)
            {
                PrintSubtype (f, head, t, t->subtypes);
                fprintf (f," ");
            }
            fprintf (f, "OF ");
            PrintType (f, head, bt->a.sequenceOf);
            break;

        case BASICTYPE_SETOF:
            fprintf (f, "SET ");
            if (t->subtypes != NULL)
            {
                PrintSubtype (f, head, t, t->subtypes);
                fprintf (f," ");
            }
            fprintf (f, "OF ");
            PrintType (f, head, bt->a.setOf);
            break;


        case BASICTYPE_SELECTION:
            fprintf (f, "%s < ", bt->a.selection->fieldName);
            PrintType (f, head, bt->a.selection->typeRef);
            break;




        case BASICTYPE_COMPONENTSOF:
            fprintf (f, "COMPONENTS OF ");
            PrintType (f, NULL, bt->a.componentsOf);
            break;



        case BASICTYPE_ANYDEFINEDBY:
            fprintf (f, "ANY DEFINED BY %s", bt->a.anyDefinedBy->fieldName);
            break;


        case BASICTYPE_LOCALTYPEREF:
            fprintf (f, "%s", bt->a.localTypeRef->typeName);
            break;

        case BASICTYPE_IMPORTTYPEREF:
            /* attempt to keep special scoping, ie modname.type forms */
            if (bt->a.importTypeRef->moduleName != NULL)
                fprintf (f,"%s.", bt->a.importTypeRef->moduleName);
            fprintf (f, "%s", bt->a.importTypeRef->typeName);
            break;


        case BASICTYPE_UNKNOWN:
            fprintf (f, "unknown type !?!");
            break;

        case BASICTYPE_BOOLEAN:
            fprintf (f, "BOOLEAN");
            break;


        case BASICTYPE_INTEGER:
            fprintf (f, "INTEGER");
            if ((bt->a.integer != NULL) && !LIST_EMPTY (bt->a.integer))
            {
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "{\n");
                indentG += indentStepG;
                PrintNamedElmts (f, head, t, bt->a.integer);
                indentG -= indentStepG;
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "}");
            }
            break;


        case BASICTYPE_BITSTRING:
            fprintf (f, "BIT STRING");
            if ((bt->a.bitString != NULL) && !LIST_EMPTY (bt->a.bitString))
            {
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "{\n");
                indentG += indentStepG;
                PrintNamedElmts (f, head, t, bt->a.bitString);
                indentG -= indentStepG;
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "}");
            }
            break;

        case BASICTYPE_OCTETSTRING:
            fprintf (f, "OCTET STRING");
            break;

        case BASICTYPE_NULL:
            fprintf (f, "NULL");
            break;

        case BASICTYPE_OID:
            fprintf (f, "OBJECT IDENTIFIER");
            break;

        case BASICTYPE_REAL:
            fprintf (f, "REAL");
            break;

        case BASICTYPE_ENUMERATED:
            fprintf (f, "ENUMERATED");
            if ((bt->a.enumerated != NULL) && !LIST_EMPTY (bt->a.enumerated))
            {
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "{\n");
                indentG += indentStepG;
                PrintNamedElmts (f, head, t, bt->a.enumerated);
                indentG -= indentStepG;
                fprintf (f, "\n");
                INDENT (f, indentG);
                fprintf (f, "}");
            }
            break;

        case BASICTYPE_ANY:
            fprintf (f, "ANY");
            break;

        case BASICTYPE_MACROTYPE:
            switch (bt->a.macroType->choiceId)
            {
        case MACROTYPE_ROSOPERATION:
        case MACROTYPE_ASNABSTRACTOPERATION:
            PrintRosOperationMacroType (f, head, t, bt, bt->a.macroType->a.rosOperation);
            break;

        case MACROTYPE_ROSERROR:
        case MACROTYPE_ASNABSTRACTERROR:
            PrintRosErrorMacroType (f, head, t, bt, bt->a.macroType->a.rosError);
            break;

        case MACROTYPE_ROSBIND:
        case MACROTYPE_ROSUNBIND:
            PrintRosBindMacroType (f, head, t, bt, bt->a.macroType->a.rosBind);
            break;

        case MACROTYPE_ROSASE:
            PrintRosAseMacroType (f, head, t, bt, bt->a.macroType->a.rosAse);
            break;

        case MACROTYPE_MTSASEXTENSIONS:
            PrintMtsasExtensionsMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtensions);
            break;

        case MACROTYPE_MTSASEXTENSION:
            PrintMtsasExtensionMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtension);
            break;

        case MACROTYPE_MTSASEXTENSIONATTRIBUTE:
            PrintMtsasExtensionAttributeMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtensionAttribute);
            break;

        case MACROTYPE_MTSASTOKEN:
            PrintMtsasTokenMacroType (f, head, t, bt, bt->a.macroType->a.mtsasToken);
            break;

        case MACROTYPE_MTSASTOKENDATA:
            PrintMtsasTokenDataMacroType (f, head, t, bt, bt->a.macroType->a.mtsasTokenData);
            break;

        case MACROTYPE_MTSASSECURITYCATEGORY:
            PrintMtsasSecurityCategoryMacroType (f, head, t, bt, bt->a.macroType->a.mtsasSecurityCategory);
            break;

        case MACROTYPE_ASNOBJECT:
            PrintAsnObjectMacroType (f, head, t, bt, bt->a.macroType->a.asnObject);
            break;

        case MACROTYPE_ASNPORT:
            PrintAsnPortMacroType (f, head, t, bt, bt->a.macroType->a.asnPort);
            break;

        case MACROTYPE_ASNABSTRACTBIND:
        case MACROTYPE_ASNABSTRACTUNBIND:
            PrintAsnAbstractBindMacroType (f, head, t, bt, bt->a.macroType->a.asnAbstractBind);
            break;

        case MACROTYPE_AFALGORITHM:
            PrintAfAlgorithmMacroType (f, head, t, bt, bt->a.macroType->a.afAlgorithm);
            break;

        case MACROTYPE_AFENCRYPTED:
            PrintAfEncryptedMacroType (f, head, t, bt, bt->a.macroType->a.afEncrypted);
            break;

        case MACROTYPE_AFSIGNED:
            PrintAfSignedMacroType (f, head, t, bt, bt->a.macroType->a.afSigned);
            break;

        case MACROTYPE_AFSIGNATURE:
            PrintAfSignatureMacroType (f, head, t, bt, bt->a.macroType->a.afSignature);
            break;

        case MACROTYPE_AFPROTECTED:
            PrintAfProtectedMacroType (f, head, t, bt, bt->a.macroType->a.afProtected);
            break;

        case MACROTYPE_SNMPOBJECTTYPE:
            PrintSnmpObjectTypeMacroType (f, head, t, bt, bt->a.macroType->a.snmpObjectType);
            break;

        default:
            fprintf (f, "< unknown macro type id ?! >");

    } /* end macro type switch */
    break;

        /*
         * @MACRO@ add new macro printers above this point
         */

        case BASICTYPE_MACRODEF:
            /*
             * printing this should be handled in PrintTypeDefs
             */
            break;


        default:
            fprintf (f, "< unknown type id ?! >");

    }
}  /* PrintBasicType */



void
PrintElmtType PARAMS ((f, head, t, nt),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    NamedType *nt)
{
    if (nt->fieldName != NULL)
        fprintf (f, "%s ", nt->fieldName);

    PrintType (f, head, nt->type);

}  /* PrintElmtType */

void
PrintElmtTypes PARAMS ((f, head, t, e),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    NamedTypeList *e)
{
    NamedType *nt;
    NamedType *last;

    if ((e == NULL) || LIST_EMPTY (e))
        return;

    last = (NamedType*)LAST_LIST_ELMT (e);
    FOR_EACH_LIST_ELMT (nt, e)
    {

        PrintElmtType (f, head, t, nt);
        if (nt != last)
        {
            fprintf (f, ",\n");
            INDENT (f, indentG);
        }
    }
}  /* PrintElmtTypes */




void
PrintValueDefs PARAMS ((f, vList),
    FILE *f _AND_
    ValueDefList *vList)
{
    ValueDef *v;
    FOR_EACH_LIST_ELMT (v, vList)
    {
        PrintValueDef (f, v);
    }
}  /* PrintValueDefs */


void
PrintValueDef PARAMS ((f, v),
    FILE *f _AND_
    ValueDef *v)
{
    fprintf (f, "%s ", v->definedName);

    if (v->value->type != NULL)
        PrintType (f, NULL, v->value->type);
    else
        /* just go by valueType */
        PrintTypeById (f, v->value->valueType);

    fprintf (f, " ::= ");
    indentG += indentStepG;
    PrintValue (f, v, v->value->type, v->value);
    fprintf (f, "\n\n");
    indentG -= indentStepG;
}  /* PrintValueDef */


void
PrintValue PARAMS ((f, head, valuesType, v),
    FILE *f _AND_
    ValueDef *head _AND_
    Type *valuesType _AND_
    Value *v)
{
    if (v == NULL)
        return;

    PrintBasicValue (f, head, valuesType, v, v->basicValue);

}  /* PrintValue */


void
PrintBasicValue PARAMS ((f, head, valuesType, v, bv),
    FILE *f _AND_
    ValueDef *head _AND_
    Type *valuesType _AND_
    Value *v _AND_
    BasicValue *bv)
{
    if (v == NULL)
        return;


    switch (bv->choiceId)
    {
        case BASICVALUE_UNKNOWN:
            fprintf (f, "<unknown value>");
            break;

        case BASICVALUE_EMPTY:
            fprintf (f,"{ }");
            break;

        case BASICVALUE_INTEGER:
            fprintf (f, "%d", bv->a.integer);
            break;

        case BASICVALUE_SPECIALINTEGER:
            if (bv->a.specialInteger == MAX_INT)
                fprintf (f, "MAX");
            else
                fprintf (f, "MIN");

            break;

        case BASICVALUE_BOOLEAN:
            if (bv->a.boolean)
                fprintf (f,"TRUE");
            else
                fprintf (f,"FALSE");
            break;

        case BASICVALUE_REAL:
            fprintf (f, "%f", bv->a.real);
            break;

        case BASICVALUE_SPECIALREAL:
            if (bv->a.specialReal == PLUS_INFINITY_REAL)
                fprintf (f, "PLUS INFINITY");
            else
                fprintf (f, "MINUS INFINITY");

            break;

        case BASICVALUE_ASCIITEXT:
            fprintf (f, "\"%s\"", bv->a.asciiText->octs);
            break;

        case BASICVALUE_ASCIIHEX:
            fprintf (f, "\"%s\"", bv->a.asciiHex->octs);
            break;

        case BASICVALUE_ASCIIBITSTRING:
            fprintf (f, "\"%s\"", bv->a.asciiBitString->octs);
            break;

        case BASICVALUE_OID:
            PrintEncodedOid (f, bv->a.oid);
            break;

        case BASICVALUE_LINKEDOID:
            PrintOid (f, bv->a.linkedOid);
            break;

        case BASICVALUE_BERVALUE:
            fprintf (f,"<PrintBerValue not coded yet");
            break;

        case BASICVALUE_PERVALUE:
            fprintf (f,"<PrintPerValue not coded yet");
            break;

        case BASICVALUE_NAMEDVALUE:
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f,"{\n");
            indentG += indentStepG;
            PrintElmtValue (f, head, v, bv->a.namedValue);
            indentG -= indentStepG;
            fprintf (f,"\n");
            INDENT (f, indentG);
            fprintf (f,"}");
            break;

        case BASICVALUE_NULL:
            fprintf (f,"NULL");
            break;

        case BASICVALUE_LOCALVALUEREF:
            fprintf (f, "%s", bv->a.localValueRef->valueName);
            break;

        case BASICVALUE_IMPORTVALUEREF:
            fprintf (f, "%s", bv->a.importValueRef->valueName);
            break;

        case BASICVALUE_VALUENOTATION:
            fprintf (f, "-- snacc warning: can't parse this value yet --");
            fprintf (f, "%s", bv->a.valueNotation->octs);
            break;


        default:
           fprintf (stderr,"PrintBasicValue: ERROR - unknown value type\n");
    }

}  /* PrintBasicValue */


void
PrintElmtValue PARAMS ((f, head, v, nv),
    FILE *f _AND_
    ValueDef *head _AND_
    Value *v _AND_
    NamedValue *nv)
{
    if (nv->fieldName != NULL)
        fprintf (f, "%s ", nv->fieldName);

    PrintValue (f, NULL, NULL,  nv->value);
}  /* PrintElmtValue */


void
PrintElmtValues PARAMS ((f, head, v, e),
    FILE *f _AND_
    ValueDef *head _AND_
    Value *v _AND_
    NamedValueList *e)
{
    NamedValue *nv;
    NamedValue *last;

    if ((e == NULL) || LIST_EMPTY (e))
        return;

    last = (NamedValue*)LAST_LIST_ELMT (e);
    FOR_EACH_LIST_ELMT (nv, e)
    {
        PrintElmtValue (f, head, v, nv);
        if (nv != last)
        {
            fprintf (f, ",\n");
            INDENT (f, indentG);
        }
    }
}  /* PrintElmtValues */


void
PrintTypeById PARAMS ((f, typeId),
    FILE *f _AND_
    int typeId)
{
    switch (typeId)
    {
        case BASICTYPE_UNKNOWN:
            fprintf (f, "UNKNOWN");
            break;

        case BASICTYPE_BOOLEAN:
            fprintf (f, "BOOLEAN");
            break;

        case BASICTYPE_INTEGER:
            fprintf (f, "INTEGER");
            break;

        case BASICTYPE_BITSTRING:
            fprintf (f, "BIT STRING");
            break;

        case BASICTYPE_OCTETSTRING:
            fprintf (f, "OCTET STRING");
            break;


        case BASICTYPE_NULL:
            fprintf (f, "NULL");
            break;

        case BASICTYPE_SEQUENCE:
            fprintf (f, "SEQUENCE");
            break;

        case BASICTYPE_SEQUENCEOF:
            fprintf (f, "SEQUENCE OF");
            break;

        case BASICTYPE_SET:
            fprintf (f, "SET");
            break;

        case BASICTYPE_SETOF:
            fprintf (f, "SET OF");
            break;

        case BASICTYPE_CHOICE:
            fprintf (f, "CHOICE");
            break;

        case BASICTYPE_SELECTION:
            fprintf (f, "SELECTION");
            break;

        case BASICTYPE_ANY:
            fprintf (f, "ANY");
            break;

        case BASICTYPE_ANYDEFINEDBY:
            fprintf (f, "ANY DEFINED BY");
            break;

        case BASICTYPE_OID:
            fprintf (f, "OBJECT IDENTIFIER");
            break;

        case BASICTYPE_ENUMERATED:
            fprintf (f, "ENUMERATED");
            break;

        case BASICTYPE_REAL:
            fprintf (f, "REAL");
            break;

        case BASICTYPE_COMPONENTSOF:
            fprintf (f, "COMPONENTS OF");
            break;

        default:
            fprintf (f, "ERROR - %d is an unknown type id\n", typeId);
    }
}  /* PrintTypeById */


void
PrintTag PARAMS ((f, tag),
    FILE *f _AND_
    Tag *tag)
{
    char *name=NULL;

    if (tag->tclass == UNIV)
    {
        switch (tag->code)
        {
           case BOOLEAN_TAG_CODE: name = "BOOLEAN";
                        break;
           case INTEGER_TAG_CODE: name = "INTEGER";
                        break;
           case BITSTRING_TAG_CODE: name = "BITSTRING";
                        break;
           case OCTETSTRING_TAG_CODE: name = "OCTETSTRING";
                        break;
           case NULLTYPE_TAG_CODE: name = "NULL TYPE";
                        break;
           case OID_TAG_CODE: name = "OBJECT ID";
                        break;
           case OD_TAG_CODE: name = "OBEJECT DESCRIPTOR";
                        break;
           case EXTERNAL_TAG_CODE: name = "EXTERNAL";
                        break;
           case REAL_TAG_CODE: name = "REAL";
                        break;
           case ENUM_TAG_CODE: name = "ENUMERATED";
                        break;
           case SEQ_TAG_CODE: name = "SEQUENCE";
                        break;
           case SET_TAG_CODE: name = "SET";
                        break;
           case NUMERICSTRING_TAG_CODE: name = "NUMERIC STRING";
                        break;
           case PRINTABLESTRING_TAG_CODE: name = "PRINTABLE STRING";
                        break;
           case TELETEXSTRING_TAG_CODE: name = "TELETEX STRING";
                        break;
           case VIDEOTEXSTRING_TAG_CODE: name = "VIDEOTEX STRING";
                        break;
           case IA5STRING_TAG_CODE: name = "IA5 STRING";
                        break;
           case UTCTIME_TAG_CODE:  name = "UTC TIME";
                        break;
           case GENERALIZEDTIME_TAG_CODE: name = "GENERALIZED TIME";
                        break;
           case GRAPHICSTRING_TAG_CODE: name = "GRAPHIC STRING";
                        break;
           case VISIBLESTRING_TAG_CODE: name = "VISIBLE STRING";
                        break;
           case GENERALSTRING_TAG_CODE: name = "GENERAL STRING";
                        break;

           default: name = "UNKNOWN UNIVERSAL TYPE";
        }
        fprintf (f, "[UNIVERSAL %d]", tag->code);
    }
    else  if (tag->tclass  == APPL)
    {
        fprintf (f, "[APPLICATION %d]", tag->code);
    }
    else  if (tag->tclass  == PRIV)
    {
        fprintf (f, "[PRIVATE %d]", tag->code);
    }
    else  if (tag->tclass  == CNTX)
    {
        fprintf (f, "[%d]", tag->code);
    }

    if (tag->explicit)
        fprintf (f, " EXPLICIT");

}  /* PrintTag */


void
PrintSubtype PARAMS ((f, head, t, s),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    Subtype *s)
{
    Subtype *tmpS;
    Subtype *last;

    if (s == NULL)
        return;

/*     fprintf (f, "("); */

    switch (s->choiceId)
    {
        case SUBTYPE_SINGLE:
            PrintSubtypeValue (f, head, t, s->a.single);
        break;

        case SUBTYPE_AND:
            FOR_EACH_LIST_ELMT (tmpS, s->a.and)
            {
                fprintf (f, "(");
                PrintSubtype (f, head, t, tmpS);
                fprintf (f, ")");
            }
        break;


        case SUBTYPE_OR:
            if ((s->a.or != NULL) && !LIST_EMPTY (s->a.or))
                last = (Subtype*)LAST_LIST_ELMT (s->a.or);
            FOR_EACH_LIST_ELMT (tmpS, s->a.or)
            {
                fprintf (f, "(");
                PrintSubtype (f, head, t, tmpS);
                fprintf (f, ")");
                if (tmpS != last)
                    fprintf (f, " | ");
            }
        break;

        case SUBTYPE_NOT:
            fprintf (f, "NOT (");
        PrintSubtype (f, head, t, s->a.not);
        fprintf (f, ")");
        break;

        default:
        fprintf (stderr, "PrintSubtype: ERROR - unknown Subtypes choiceId\n");
        break;
    }

/*     fprintf (f, ")"); */


}  /* PrintSubtype */



void
PrintSubtypeValue PARAMS ((f, head, t, s),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    SubtypeValue *s)
{
    if (s == NULL)
        return;

    switch (s->choiceId)
    {
        case SUBTYPEVALUE_SINGLEVALUE:
            PrintValue (f, NULL, NULL, s->a.singleValue);
            break;

        case SUBTYPEVALUE_CONTAINED:
            fprintf (f, "<PrintContainedSubtype not coded yet\n");
            break;

        case SUBTYPEVALUE_VALUERANGE:
            PrintValue (f, NULL, NULL, s->a.valueRange->lowerEndValue);
            if (!s->a.valueRange->lowerEndInclusive)
                fprintf (f, " >");
            fprintf (f,"..");
            if (!s->a.valueRange->upperEndInclusive)
                fprintf (f, "< ");
            PrintValue (f, NULL, NULL, s->a.valueRange->upperEndValue);
            break;


        case SUBTYPEVALUE_PERMITTEDALPHABET:
            fprintf (f,"FROM ");
            PrintSubtype (f, head, t, s->a.permittedAlphabet);
            break;

        case SUBTYPEVALUE_SIZECONSTRAINT:
            fprintf (f,"SIZE ");
            PrintSubtype (f, head, t, s->a.sizeConstraint);
            break;

        case SUBTYPEVALUE_INNERSUBTYPE:
            PrintInnerSubtype (f, head, t, s->a.innerSubtype);
            break;

        default:
           fprintf (stderr, "PrintSubtype: ERROR - unknown Subtype choiceId\n");
            break;
    }
}   /* PrintSubtype */


void
PrintInnerSubtype PARAMS ((f, head, t, i),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    InnerSubtype *i)
{
    Constraint *constraint;
    if (i->constraintType == SINGLE_CT)
    {
        fprintf (f,"WITH COMPONENT ");
        constraint = *(Constraint**)AsnListFirst (i->constraints);
        PrintSubtype (f, head, t, constraint->valueConstraints);
    }
    else
    {
        fprintf (f, "WITH COMPONENTS\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;
        if (i->constraintType == PARTIAL_CT)
        {
            INDENT (f, indentG);
            fprintf (f, "...,\n");
        }
        PrintMultipleTypeConstraints (f, head, t, i->constraints);
        indentG -= indentStepG;
        fprintf (f, "\n");
        INDENT (f, indentG);
        fprintf (f, "}");

    }
}  /* PrintInnerSubtype */



void
PrintMultipleTypeConstraints PARAMS ((f, head, t, cList),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    ConstraintList *cList)
{
    Constraint *c;
    Constraint *last;

    if ((cList == NULL) || LIST_EMPTY (cList))
        return;

    last  = (Constraint*)LAST_LIST_ELMT (cList);
    FOR_EACH_LIST_ELMT (c, cList)
    {
        if (c->fieldRef != NULL)
        {
            INDENT (f, indentG);
            fprintf (f, "%s ", c->fieldRef);
        }


        PrintSubtype (f, head, t, c->valueConstraints);

        if (c->presenceConstraint == ABSENT_CT)
            fprintf (f, " ABSENT");
        if (c->presenceConstraint == PRESENT_CT)
            fprintf (f, " PRESENT");
        if (c->presenceConstraint == OPTIONAL_CT)
            fprintf (f, " OPTIONAL");

        if (c != last)
            fprintf (f, ",\n");

    }
} /* PrintMultipleTypeConstraints */



void
PrintNamedElmts PARAMS ((f, head, t, n),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    ValueDefList *n)
{
    ValueDef *vd;
    ValueDef *last;

    if ((n == NULL) || LIST_EMPTY (n))
        return;

    last = (ValueDef*)LAST_LIST_ELMT (n);
    FOR_EACH_LIST_ELMT (vd, n)
    {
        INDENT (f, indentG);
        fprintf (f, "%s (", vd->definedName);
        PrintValue (f, NULL, NULL, vd->value);
        fprintf (f,")");
        if (vd != last)
            fprintf (f,",\n");
    }
}  /* PrintNamedElmts */




void
PrintRosOperationMacroType PARAMS ((f, head, t, bt, op),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosOperationMacroType *op)
{
    TypeOrValue *tOrV;
    TypeOrValue *last;

    if (bt->a.macroType->choiceId == MACROTYPE_ROSOPERATION)
        fprintf (f, "OPERATION");
    else
        fprintf (f, "ABSTRACT-OPERATION");

    indentG += indentStepG;
    if (op->arguments != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "ARGUMENT\n");
        indentG += indentStepG;

        INDENT (f, indentG);

        if (op->arguments->fieldName != NULL)
            fprintf (f, "%s ", op->arguments->fieldName);

        PrintType (f, head, op->arguments->type);
        indentG -= indentStepG;
    }

    if (op->result != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "RESULT\n");
        indentG += indentStepG;

        INDENT (f, indentG);

        if (op->arguments->fieldName != NULL)
            fprintf (f, "%s ", op->arguments->fieldName);

        PrintType (f, head, op->result->type);
        indentG -= indentStepG;
    }

    if ((op->errors == NULL) || (!LIST_EMPTY (op->errors)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "ERRORS\n");
        INDENT (f, indentG);
        fprintf (f,"{\n");
        indentG += indentStepG;

        last = (TypeOrValue*)LAST_LIST_ELMT (op->errors);
        FOR_EACH_LIST_ELMT (tOrV, op->errors)
        {
            INDENT (f, indentG);
            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV  != last)
                fprintf (f, ",\n");

        }
        indentG -= indentStepG;
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    if ((op->linkedOps != NULL) && (!LIST_EMPTY (op->linkedOps)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "LINKED\n");
        INDENT (f, indentG);
        fprintf (f,"{\n");
        indentG += indentStepG;

        last = (TypeOrValue*)LAST_LIST_ELMT (op->linkedOps);
        FOR_EACH_LIST_ELMT (tOrV, op->linkedOps)
        {
            INDENT (f, indentG);
            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV != last)
                fprintf (f, ",\n");
        }
        indentG -= indentStepG;
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, " }");
    }

    indentG -= indentStepG;

} /* PrintRosOperationMacroType */



void
PrintRosErrorMacroType PARAMS ((f, head, t, bt, err),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosErrorMacroType *err)
{
    if (bt->a.macroType->choiceId == MACROTYPE_ROSERROR)
        fprintf (f,"ERROR\n");
    else
        fprintf (f,"ABSTRACT-ERROR\n");

    indentG += indentStepG;

    if (err->parameter != NULL)
    {
        INDENT (f, indentG);
        fprintf (f,"PARAMETER ");
        indentG += indentStepG;
        PrintElmtType (f, head, t, err->parameter);
        indentG -= indentStepG;
    }
    indentG -= indentStepG;

}  /* PrintRosErrorMacroType */


void
PrintRosBindMacroType PARAMS ((f, head, t, bt, bind),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosBindMacroType *bind)
{
    if (bt->a.macroType->choiceId == MACROTYPE_ROSBIND)
        fprintf (f,"BIND");
    else
        fprintf (f,"UNBIND");

    indentG += indentStepG;

    if (bind->argument != NULL)
    {
        fprintf (f, "\n");
        INDENT (f, indentG);
        fprintf (f,"ARGUMENT\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintElmtType (f, head, t, bind->argument);
        indentG -= indentStepG;
    }

    if (bind->result != NULL)
    {
        fprintf (f, "\n");
        INDENT (f, indentG);
        fprintf (f,"RESULT\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintElmtType (f, head, t, bind->result);
        indentG -= indentStepG;
    }

    if (bind->error != NULL)
    {
        fprintf (f, "\n");
        INDENT (f, indentG);
        if (bt->a.macroType->choiceId == MACROTYPE_ROSBIND)
            fprintf (f,"BIND-ERROR\n");
        else
            fprintf (f,"UNBIND-ERROR\n");

        indentG += indentStepG;
        INDENT (f, indentG);
        PrintElmtType (f, head, t, bind->error);
        indentG -= indentStepG;
    }

    indentG -= indentStepG;

}  /* PrintRosBindMacroType */


void
PrintRosAseMacroType PARAMS ((f, head, t, bt, ase),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAseMacroType *ase)
{
    Value *v;
    Value *last;

    fprintf (f, "APPLICATION-SERVICE-ELEMENT");
    indentG += indentStepG;

    if ((ase->operations != NULL)&& (!LIST_EMPTY (ase->operations)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f,"OPERATIONS\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");

        indentG += indentStepG;

        last = (Value*)LAST_LIST_ELMT (ase->operations);
        FOR_EACH_LIST_ELMT (v, ase->operations)
        {
            INDENT (f, indentG);
            PrintValue (f, NULL, t, v);
            if (v != last)
                fprintf (f, ",\n");
        }
        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    else  /* either suuplier invokes or consumer invokes will be valid */
    {
        if ((ase->consumerInvokes != NULL) && (!LIST_EMPTY (ase->consumerInvokes)))
        {
            fprintf (f,"\n");
            INDENT (f, indentG);
            fprintf (f,"CONSUMER INVOKES\n");
            INDENT (f, indentG);
            fprintf (f, "{\n");

            indentG += indentStepG;
            last = (Value*) LAST_LIST_ELMT (ase->consumerInvokes);
            FOR_EACH_LIST_ELMT (v, ase->consumerInvokes)
            {
                INDENT (f, indentG);
                PrintValue (f, NULL, t, v);
                if (v != last)
                    fprintf (f, ",\n");
            }
            fprintf (f, "\n");
            indentG -= indentStepG;
            INDENT (f, indentG);
            fprintf (f, "}");
        }
        if ((ase->operations != NULL) && (!LIST_EMPTY (ase->operations)))
        {
            fprintf (f,"\n");
            INDENT (f, indentG);
            fprintf (f,"SUPPLIER INVOKES\n");
            INDENT (f, indentG);
            fprintf (f, "{\n");

            indentG += indentStepG;
            last = (Value*)LAST_LIST_ELMT (ase->supplierInvokes);
            FOR_EACH_LIST_ELMT (v, ase->supplierInvokes)
            {
                INDENT (f, indentG);
                PrintValue (f, NULL, t, v);
                if (v != last)
                    fprintf (f, ",\n");
            }
            fprintf (f, "\n");
            indentG -= indentStepG;
            INDENT (f, indentG);
            fprintf (f, "}");
        }
    }
    indentG -= indentStepG;

}  /* PrintRosAseMacrType */




void
PrintRosAcMacroType PARAMS ((f, head, t, bt, ac),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAcMacroType *ac)
{
    Value *v;
    Value *last;
    OID *oid;
    OID *lastOid;

    fprintf (f, "APPLICATION-CONTEXT");
    indentG += indentStepG;

    /*
     * print non Ros Elements
     */
    fprintf (f,"\n");
    INDENT (f, indentG);
    fprintf (f,"APPLICATION-SERVICE-ELEMENTS\n");
    INDENT (f, indentG);
    fprintf (f, "{\n");

    indentG += indentStepG;
    if ((ac->nonRoElements == NULL) && (!LIST_EMPTY (ac->nonRoElements)))
        last = (Value*)LAST_LIST_ELMT (ac->nonRoElements);
    FOR_EACH_LIST_ELMT (v, ac->nonRoElements)
    {
        INDENT (f, indentG);
        PrintValue (f, NULL, t, v);
        if (v != last)
            fprintf (f, ",\n");
    }
    fprintf (f, "}\n");

    /*
     * Print Bind Type
     */
    INDENT (f, indentG);
    fprintf (f,"BIND\n");
    INDENT (f, indentG);
    PrintType (f, head, ac->bindMacroType);
    fprintf (f, "\n");

    /*
     * Print unbind Type
     */
    INDENT (f, indentG);
    fprintf (f,"UNBIND\n");
    INDENT (f, indentG);
    PrintType (f, head, ac->unbindMacroType);


    if (ac->remoteOperations != NULL)
    {
        fprintf (f, "\n");
        INDENT (f, indentG);
        fprintf (f,"REMOTE OPERATIONS { ");
        PrintValue (f, NULL, t, ac->remoteOperations);
        fprintf (f, " }");

        if ((ac->operationsOf != NULL) && (!LIST_EMPTY (ac->operationsOf)))
        {
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f,"OPERATIONS OF\n");
            INDENT (f, indentG);
            fprintf (f, "{\n");

            indentG += indentStepG;
            last = (Value*)LAST_LIST_ELMT (ac->operationsOf);
            FOR_EACH_LIST_ELMT (v,  ac->operationsOf)
            {
                INDENT (f, indentG);
                PrintValue (f, NULL, t, v);
                if (v != last)
                    fprintf (f, ",\n");
            }
            fprintf (f, "\n");
            indentG -= indentStepG;
            INDENT (f, indentG);
            fprintf (f, "}");
        }

        if ((ac->initiatorConsumerOf != NULL) && (!LIST_EMPTY (ac->initiatorConsumerOf)))
        {
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f,"INITIATOR CONSUMER OF\n");
            INDENT (f, indentG);
            fprintf (f, "{\n");

            indentG += indentStepG;
            last = (Value*)LAST_LIST_ELMT (ac->initiatorConsumerOf);
            FOR_EACH_LIST_ELMT (v, ac->initiatorConsumerOf)
            {
                INDENT (f, indentG);
                PrintValue (f, NULL, t, v);
                if (v != last)
                    fprintf (f, ",\n");
            }
            fprintf (f, "\n");
            indentG -= indentStepG;
            INDENT (f, indentG);
            fprintf (f, "}");
        }

        if ((ac->responderConsumerOf != NULL) && (!LIST_EMPTY (ac->responderConsumerOf)))
        {
            fprintf (f, "\n");
            INDENT (f, indentG);
            fprintf (f,"RESPONDER CONSUMER OF\n");
            INDENT (f, indentG);
            fprintf (f, "{\n");

            indentG += indentStepG;
            last = (Value*)LAST_LIST_ELMT (ac->responderConsumerOf);
            FOR_EACH_LIST_ELMT (v, ac->responderConsumerOf)
            {
                INDENT (f, indentG);
                PrintValue (f, NULL, t, v);
                if (v != last)
                    fprintf (f, ",\n");
            }
            fprintf (f, "\n");
            indentG -= indentStepG;
            INDENT (f, indentG);
            fprintf (f, "}");
        }
    }

    fprintf (f,"\n");
    INDENT (f, indentG);
    fprintf (f,"ABSTRACT SYNTAXES\n");
    INDENT (f, indentG);
    fprintf (f, "{\n");

    if ((ac->abstractSyntaxes != NULL) && (!LIST_EMPTY (ac->abstractSyntaxes)))
    lastOid = (OID*)LAST_LIST_ELMT (ac->abstractSyntaxes);
    FOR_EACH_LIST_ELMT (oid, ac->abstractSyntaxes)
    {
        INDENT (f, indentG);
        PrintOid (f, oid);
        if (oid != lastOid)
            fprintf (f, ",\n");
    }
    fprintf (f, "\n");
    indentG -= indentStepG;
    INDENT (f, indentG);
    fprintf (f, "}");

    indentG -= indentStepG;

}  /* PrintRosAcMacroType */


void
PrintMtsasExtensionsMacroType PARAMS ((f, head, t, bt, exts),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionsMacroType *exts)
{
    Value *v;
    Value *last;

    fprintf (f, "EXTENSIONS CHOSEN FROM");

    INDENT (f, indentG);
    fprintf (f, "{\n");

    indentG += indentStepG;
    if ((exts->extensions == NULL) && (!LIST_EMPTY (exts->extensions)))
        last = (Value*)LAST_LIST_ELMT (exts->extensions);
    FOR_EACH_LIST_ELMT (v, exts->extensions)
    {
        INDENT (f, indentG);
        PrintValue (f, NULL, t, v);
        if (v != last)
            fprintf (f, ",\n");
    }
    fprintf (f, "\n");
    indentG -= indentStepG;
    INDENT (f, indentG);
    fprintf (f, "}");

}  /* PrintMtsasExtensionsMacroType */


void
PrintMtsasExtensionMacroType PARAMS ((f, head, t, bt, ext),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionMacroType *ext)
{

    fprintf (f, "EXTENSION");

    indentG += indentStepG;
    if (ext->elmtType != NULL)
    {
        fprintf (f, "\n");
        INDENT (f, indentG);
        PrintElmtType (f, head, t, ext->elmtType);

        if (ext->defaultValue != NULL)
        {
            fprintf (f, " DEFAULT ");
            PrintValue (f, NULL, t, ext->defaultValue);
        }
    }

    if ((ext->criticalForSubmission != NULL) ||
         (ext->criticalForTransfer != NULL) ||
         (ext->criticalForDelivery != NULL))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "CRITICAL FOR ");

        if (ext->criticalForSubmission != NULL)
        {
            fprintf (f, "SUBMISSION");
            if ((ext->criticalForTransfer != NULL) ||
                (ext->criticalForDelivery != NULL))
                fprintf (f,", ");
        }

        if (ext->criticalForTransfer != NULL)
        {
            fprintf (f, "TRANSFER, ");
            if  (ext->criticalForDelivery != NULL)
                fprintf (f,", ");
        }

        if (ext->criticalForDelivery != NULL)
            fprintf (f, "DELIVERY");

    }

    indentG -= indentStepG;

}  /* PrintMtsasExtensionMacroType */




void
PrintMtsasExtensionAttributeMacroType PARAMS ((f, head, t, bt, ext),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionAttributeMacroType *ext)
{

    fprintf (f, "EXTENSION-ATTRIBUTE");
    if (ext->type != NULL)
    {
        fprintf (f, "\n");
        indentG += indentStepG;
        INDENT (f, indentG);

        PrintType (f, head, ext->type);
        indentG -= indentStepG;
    }

}  /* PrintMtsasExtensionAttributeMacroType */



void
PrintMtsasTokenMacroType PARAMS ((f, head, t, bt, tok),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenMacroType *tok)
{

    fprintf (f, "TOKEN");
    if (tok->type != NULL)
    {
        fprintf (f, "\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintType (f, head, tok->type);
        indentG -= indentStepG;
    }

}  /* PrintMtsasTokenMacro */


void
PrintMtsasTokenDataMacroType PARAMS ((f, head, t, bt, tok),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenDataMacroType *tok)
{

    fprintf (f, "TOKEN-DATA");
    if (tok->type != NULL)
    {
        fprintf (f, "\n");
        indentG += indentStepG;
        INDENT (f, indentG);

        PrintType (f, head, tok->type);
        indentG -= indentStepG;
    }

}  /* PrintMtsasTokenDataMacro */


void
PrintMtsasSecurityCategoryMacroType PARAMS ((f, head, t, bt, sec),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasSecurityCategoryMacroType *sec)
{

    fprintf (f, "SECURITY-CATEGORY");
    if (sec->type != NULL)
    {
        fprintf (f, "\n");
        indentG += indentStepG;
        INDENT (f, indentG);

        PrintType (f, head, sec->type);
        indentG -= indentStepG;
    }

}  /* PrintMtsasSecurityCategoryMacroType */



void
PrintAsnObjectMacroType PARAMS ((f, head, t, bt, obj),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnObjectMacroType *obj)
{
    AsnPort *ap;
    AsnPort *last;

    fprintf (f, "OBJECT");

    indentG += indentStepG;

    if ((obj->ports != NULL) && !LIST_EMPTY (obj->ports))
    {

        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "PORTS\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;

        last = (AsnPort*)LAST_LIST_ELMT (obj->ports);
        FOR_EACH_LIST_ELMT (ap, obj->ports)
        {
            INDENT (f, indentG);
            PrintValue (f, NULL, t, ap->portValue);

            if (ap->portType == CONSUMER_PORT)
                fprintf (f, " [C]");
            else if (ap->portType == SUPPLIER_PORT)
                fprintf (f, " [S]");

            if (ap != last)
                fprintf (f, ",\n");
        }
        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }
    indentG -= indentStepG;

}  /* PrintAsnObjectMacroType */



void
PrintAsnPortMacroType PARAMS ((f, head, t, bt, p),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnPortMacroType *p)
{
    TypeOrValue *tOrV;
    TypeOrValue *last;

    fprintf (f, "PORT");
    indentG += indentStepG;
    if ((p->abstractOps != NULL) && (!LIST_EMPTY (p->abstractOps)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "ABSTRACT OPERATIONS\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;

        last = (TypeOrValue*)LAST_LIST_ELMT (p->abstractOps);
        FOR_EACH_LIST_ELMT (tOrV, p->abstractOps)
        {
            INDENT (f, indentG);

            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV != last)
                fprintf (f, ",\n");
        }
        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    if ((p->consumerInvokes != NULL) && (!LIST_EMPTY (p->consumerInvokes)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "CONSUMER INVOKES\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;

        last = (TypeOrValue*)LAST_LIST_ELMT (p->consumerInvokes);
        FOR_EACH_LIST_ELMT (tOrV, p->consumerInvokes)
        {
            INDENT (f, indentG);

            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV != last)
                fprintf (f, ",\n");
        }
        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    if ((p->supplierInvokes != NULL) && (!LIST_EMPTY (p->supplierInvokes)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f, "SUPPLIER INVOKES\n");
        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;

        last = (TypeOrValue*)LAST_LIST_ELMT (p->supplierInvokes);
        FOR_EACH_LIST_ELMT (tOrV, p->supplierInvokes)

        {
            INDENT (f, indentG);

            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV != last)
                fprintf (f, ",\n");
        }
        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    indentG -= indentStepG;

}  /* PrintAsnPortMacroType */




void
PrintAsnAbstractBindMacroType PARAMS ((f, head, t, bt, bind),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnAbstractBindMacroType *bind)
{
    AsnPort *ap;
    AsnPort *last;

    if (bt->a.macroType->choiceId == MACROTYPE_ASNABSTRACTBIND)
        fprintf (f, "ABSTRACT-BIND");
    else
        fprintf (f, "ABSTRACT-UNBIND");

    indentG += indentStepG;

    if ((bind->ports != NULL) && (!LIST_EMPTY (bind->ports)))
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        if (bt->a.macroType->choiceId == MACROTYPE_ASNABSTRACTBIND)
            fprintf (f, "TO\n");
        else
            fprintf (f, "FROM\n");

        INDENT (f, indentG);
        fprintf (f, "{\n");
        indentG += indentStepG;

        last = (AsnPort*)LAST_LIST_ELMT (bind->ports);
        FOR_EACH_LIST_ELMT (ap, bind->ports)
        {
            INDENT (f, indentG);
            PrintValue (f, NULL, t, ap->portValue);

            if (ap->portType == CONSUMER_PORT)
                fprintf (f, " [C]");
            else if (ap->portType == SUPPLIER_PORT)
                fprintf (f, " [S]");

            if (ap != last)
                fprintf (f, ",\n");
        }

        fprintf (f, "\n");
        indentG -= indentStepG;
        INDENT (f, indentG);
        fprintf (f, "}");
    }

    if (bind->type != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        PrintType (f, head, bind->type);
    }

    indentG -= indentStepG;

}  /* PrintAsnAbstractBindMacroType */



void
PrintAfAlgorithmMacroType PARAMS ((f, head, t, bt, alg),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    Type *alg)
{
    indentG += indentStepG;
    fprintf (f, "ALGORITHM PARAMETER ");
    PrintType (f, head, alg);
    indentG -= indentStepG;
}  /* PrintAfAlgorithmMacroType */


void
PrintAfEncryptedMacroType PARAMS ((f, head, t, bt, encrypt),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    Type *encrypt)
{
    indentG += indentStepG;
    fprintf (f, "ENCRYPTED ");
    PrintType (f, head, encrypt);
    indentG -= indentStepG;
}  /* PrintAfEncryptedMacroType */


void
PrintAfSignedMacroType PARAMS ((f, head, t, bt, sign),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    Type *sign)
{
    indentG += indentStepG;
    fprintf (f, "SIGNED ");
    PrintType (f, head, sign);
    indentG -= indentStepG;
}  /* PrintAfSignedMacroType */


void
PrintAfSignatureMacroType PARAMS ((f, head, t, bt, sig),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    Type *sig)
{
    indentG += indentStepG;
    fprintf (f, "SIGNATURE ");
    PrintType (f, head, sig);
    indentG -= indentStepG;
}  /* PrintAfSignatureMacroType */


void
PrintAfProtectedMacroType PARAMS ((f, head, t, bt, p),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    Type *p)
{
    indentG += indentStepG;
    fprintf (f, "PROTECTED ");
    PrintType (f, head, p);
    indentG -= indentStepG;
}  /* PrintAfMacroType */


void
PrintSnmpObjectTypeMacroType PARAMS ((f, head, t, bt, ot),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    SnmpObjectTypeMacroType *ot)
{
    TypeOrValue *tOrV;
    TypeOrValue *last;

    fprintf (f, "OBJECT-TYPE\n");
    indentG += indentStepG;
    INDENT (f,indentG);
    fprintf (f,"SYNTAX ");
    indentG += indentStepG;
    PrintType (f, head, ot->syntax);
    indentG -= indentStepG;

    fprintf (f,"\n");
    INDENT (f,indentG);
    fprintf (f,"ACCESS ");
    switch (ot->access)
    {
        case SNMP_READ_ONLY:
            fprintf (f,"read-only");
            break;

        case SNMP_READ_WRITE:
            fprintf (f,"read-write");
            break;

        case SNMP_WRITE_ONLY:
            fprintf (f,"write-only");
            break;

        case SNMP_NOT_ACCESSIBLE:
            fprintf (f,"not-accessible");
            break;

        default:
            fprintf (f," < ?? unknown access type ?? >");
    }

    fprintf (f,"\n");
    INDENT (f, indentG);
    fprintf (f,"STATUS ");
    switch (ot->status)
    {
        case SNMP_MANDATORY:
            fprintf (f,"mandatory");
            break;

        case SNMP_OPTIONAL:
            fprintf (f,"optional");
            break;

        case SNMP_OBSOLETE:
            fprintf (f,"obsolete");
            break;

        case SNMP_DEPRECATED:
            fprintf (f,"deprecated");
            break;

        default:
            fprintf (f," < ?? unknown status type ?? >");
    }

    if (ot->description != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f,"DESCRIPTION\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintValue (f, NULL, t, ot->description);
        indentG -= indentStepG;
    }

    if (ot->reference != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f,"REFERENCE\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintValue (f, NULL, t, ot->reference);
        indentG -= indentStepG;
    }

    if (ot->index != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f,"INDEX\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        last = (TypeOrValue*)LAST_LIST_ELMT (ot->index);
        FOR_EACH_LIST_ELMT (tOrV, ot->index)
        {
            INDENT (f, indentG);
            if (tOrV->choiceId == TYPEORVALUE_TYPE)
                PrintType (f, head, tOrV->a.type);
            else
                PrintValue (f, NULL, t, tOrV->a.value);

            if (tOrV  != last)
                fprintf (f, ",\n");
        }
        indentG -= indentStepG;
    }

    if (ot->defVal != NULL)
    {
        fprintf (f,"\n");
        INDENT (f, indentG);
        fprintf (f,"DEFVAL\n");
        indentG += indentStepG;
        INDENT (f, indentG);
        PrintValue (f, NULL, t, ot->defVal);
        indentG -= indentStepG;
    }

    fprintf (f,"\n");

    indentG -= indentStepG;
}  /* PrintSnmpObjectTypeMacroType */


/*
 * @MACRO@ add new macro print routines above this point
 */

void
PrintMacroDef PARAMS ((f, head),
    FILE *f _AND_
    TypeDef *head)
{
    char *s;

    fprintf (f,"\n--  Note: snacc does not use macro defs to extend the compiler.");
    fprintf (f,"\n--  All macros that are understood have been hand coded.");
    fprintf (f,"\n--  The macro def body is kept as a string only.\n\n");

    s =  head->type->basicType->a.macroDef;

    fprintf (f, "%s MACRO ::=\n", head->definedName);
    fprintf (f, "%s", s);

}  /* PrintMacroDef */



void
PrintEncodedOid PARAMS ((f, eoid),
    FILE *f _AND_
    AsnOid *eoid)
{
    int i;
    int arcNum;
    int firstArcNum;
    int secondArcNum;

    if (eoid == NULL)
       return;

    fprintf (f, "{ ");

    for (arcNum = 0, i=0; (i < eoid->octetLen) && (eoid->octs[i] & 0x80);i++)
        arcNum = (arcNum << 7) + (eoid->octs[i] & 0x7f);

    arcNum = (arcNum << 7) + (eoid->octs[i] & 0x7f);
    i++;

    firstArcNum = arcNum / 40;
    if (firstArcNum > 2)
        firstArcNum = 2;

    secondArcNum = arcNum - (firstArcNum * 40);

    fprintf (f, "%d ", firstArcNum);
    fprintf (f, "%d ", secondArcNum);
    for (; i < eoid->octetLen; )
    {
        for (arcNum = 0; (i < eoid->octetLen) && (eoid->octs[i] & 0x80);i++)
            arcNum = (arcNum << 7) + (eoid->octs[i] & 0x7f);

        arcNum = (arcNum << 7) + (eoid->octs[i] & 0x7f);
        i++;

        fprintf (f, "%d ", arcNum);
    }

    fprintf (f, "}");

}  /* PrintEncodedOid */



/*
 * this just prints  a short form of the given type.  It
 * does not print the components of a constructed type
 * such as a SEQUENCE
 * This is used by the header file generators to annotate
 * the C/C++ types
 */
void
SpecialPrintBasicType PARAMS ((f, head, t, bt),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt)
{
    switch (bt->choiceId)
    {

        case BASICTYPE_SEQUENCE:
            fprintf (f, "SEQUENCE");
            break;

        case BASICTYPE_SET:
            fprintf (f, "SET");
            break;

        case BASICTYPE_CHOICE:
            fprintf (f, "CHOICE");
            break;



        case BASICTYPE_SEQUENCEOF:
            fprintf (f, "SEQUENCE ");
            if (t->subtypes != NULL)
            {
                PrintSubtype (f, head, t, t->subtypes);
                fprintf (f," ");
            }
            fprintf (f, "OF ");
            SpecialPrintType (f, head, t->basicType->a.sequenceOf);
            break;

        case BASICTYPE_SETOF:
            fprintf (f, "SET ");
            if (t->subtypes != NULL)
            {
                PrintSubtype (f, head, t, t->subtypes);
                fprintf (f," ");
            }
            fprintf (f, "OF ");
            SpecialPrintType (f, head, t->basicType->a.sequenceOf);
            break;


        case BASICTYPE_SELECTION:
            fprintf (f, "%s < ", bt->a.selection->fieldName);
            PrintType (f, head, bt->a.selection->typeRef);
            break;




        case BASICTYPE_COMPONENTSOF:
            fprintf (f, "COMPONENTS OF ");
            PrintType (f, NULL, bt->a.componentsOf);
            break;



        case BASICTYPE_ANYDEFINEDBY:
            fprintf (f, "ANY DEFINED BY %s", bt->a.anyDefinedBy->fieldName);
            break;


        case BASICTYPE_LOCALTYPEREF:
            fprintf (f, "%s", bt->a.localTypeRef->typeName);
            break;

        case BASICTYPE_IMPORTTYPEREF:
            fprintf (f, "%s", bt->a.importTypeRef->typeName);
            break;


        case BASICTYPE_UNKNOWN:
            fprintf (f, "unknown type !?!");
            break;

        case BASICTYPE_BOOLEAN:
            fprintf (f, "BOOLEAN");
            break;


        case BASICTYPE_INTEGER:
            fprintf (f, "INTEGER");
            if ((bt->a.integer != NULL) && !LIST_EMPTY (bt->a.integer))
                SpecialPrintNamedElmts (f, head, t);
            break;


        case BASICTYPE_BITSTRING:
            fprintf (f, "BIT STRING");
            if ((bt->a.bitString != NULL) && !LIST_EMPTY (bt->a.bitString))
                SpecialPrintNamedElmts (f, head, t);
            break;

        case BASICTYPE_OCTETSTRING:
            fprintf (f, "OCTET STRING");
            break;

        case BASICTYPE_NULL:
            fprintf (f, "NULL");
            break;

        case BASICTYPE_OID:
            fprintf (f, "OBJECT IDENTIFIER");
            break;

        case BASICTYPE_REAL:
            fprintf (f, "REAL");
            break;

        case BASICTYPE_ENUMERATED:
            fprintf (f, "ENUMERATED");
            if ((bt->a.enumerated != NULL) && !LIST_EMPTY (bt->a.enumerated))
                SpecialPrintNamedElmts (f, head, t);

            break;

        case BASICTYPE_ANY:
            fprintf (f, "ANY");
            break;

        case BASICTYPE_MACROTYPE:
            switch (bt->a.macroType->choiceId)
            {
        case MACROTYPE_ROSOPERATION:
        case MACROTYPE_ASNABSTRACTOPERATION:
            PrintRosOperationMacroType (f, head, t, bt, bt->a.macroType->a.rosOperation);
            break;

        case MACROTYPE_ROSERROR:
        case MACROTYPE_ASNABSTRACTERROR:
            PrintRosErrorMacroType (f, head, t, bt, bt->a.macroType->a.rosError);
            break;

        case MACROTYPE_ROSBIND:
        case MACROTYPE_ROSUNBIND:
            PrintRosBindMacroType (f, head, t, bt, bt->a.macroType->a.rosBind);
            break;

        case MACROTYPE_ROSASE:
            PrintRosAseMacroType (f, head, t, bt, bt->a.macroType->a.rosAse);
            break;

        case MACROTYPE_MTSASEXTENSIONS:
            PrintMtsasExtensionsMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtensions);
            break;

        case MACROTYPE_MTSASEXTENSION:
            PrintMtsasExtensionMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtension);
            break;

        case MACROTYPE_MTSASEXTENSIONATTRIBUTE:
            PrintMtsasExtensionAttributeMacroType (f, head, t, bt, bt->a.macroType->a.mtsasExtensionAttribute);
            break;

        case MACROTYPE_MTSASTOKEN:
            PrintMtsasTokenMacroType (f, head, t, bt, bt->a.macroType->a.mtsasToken);
            break;

        case MACROTYPE_MTSASTOKENDATA:
            PrintMtsasTokenDataMacroType (f, head, t, bt, bt->a.macroType->a.mtsasTokenData);
            break;

        case MACROTYPE_MTSASSECURITYCATEGORY:
            PrintMtsasSecurityCategoryMacroType (f, head, t, bt, bt->a.macroType->a.mtsasSecurityCategory);
            break;

        case MACROTYPE_ASNOBJECT:
            PrintAsnObjectMacroType (f, head, t, bt, bt->a.macroType->a.asnObject);
            break;

        case MACROTYPE_ASNPORT:
            PrintAsnPortMacroType (f, head, t, bt, bt->a.macroType->a.asnPort);
            break;

        case MACROTYPE_ASNABSTRACTBIND:
        case MACROTYPE_ASNABSTRACTUNBIND:
            PrintAsnAbstractBindMacroType (f, head, t, bt, bt->a.macroType->a.asnAbstractBind);
            break;

        case MACROTYPE_AFALGORITHM:
            PrintAfAlgorithmMacroType (f, head, t, bt, bt->a.macroType->a.afAlgorithm);
            break;

        case MACROTYPE_AFENCRYPTED:
            PrintAfEncryptedMacroType (f, head, t, bt, bt->a.macroType->a.afEncrypted);
            break;

        case MACROTYPE_AFSIGNED:
            PrintAfSignedMacroType (f, head, t, bt, bt->a.macroType->a.afSigned);
            break;

        case MACROTYPE_AFSIGNATURE:
            PrintAfSignatureMacroType (f, head, t, bt, bt->a.macroType->a.afSignature);
            break;

        case MACROTYPE_AFPROTECTED:
            PrintAfProtectedMacroType (f, head, t, bt, bt->a.macroType->a.afProtected);
            break;

        case MACROTYPE_SNMPOBJECTTYPE:
            PrintSnmpObjectTypeMacroType (f, head, t, bt, bt->a.macroType->a.snmpObjectType);
            break;

        default:
            fprintf (f, "< unknown macro type id ?! >");

    } /* end macro type switch */
    break;

        /*
         * @MACRO@ add new macro printers above this point
         */

        case BASICTYPE_MACRODEF:
            /*
             * printing this should be handled in PrintTypeDefs
             */
            break;


        default:
            fprintf (f, "< unknown type id ?! >");

    }
}  /* SpecialPrintBasicType */


/*
 * this just prints  a short form of the given type.  It
 * does not print the components of a constructed type
 * such as a SEQUENCE
 * This is used by the header file generators to annotate
 * the C types
 */
void
SpecialPrintType PARAMS ((f, head, t),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t)
{
    Tag *tag;
    Tag *lastTag;

    if (t == NULL)
        return;

    lastTag = NULL;
    FOR_EACH_LIST_ELMT (tag, t->tags)
    {
        if (!(tag->tclass == UNIV && tag->code == LIBTYPE_GET_UNIV_TAG_CODE (t->basicType->choiceId)))
        {
            PrintTag (f, tag);
            fprintf (f, " ");
        }
        lastTag = tag;
    }

    /*
     * check type has been implicitly tagged
     */
    if (t->implicit)
        fprintf (f, "IMPLICIT ");

    SpecialPrintBasicType (f, head, t, t->basicType);


    /*
     * sequences of and set of print subtypes a special way
     * so ignore them here
     */
    if ((t->subtypes != NULL) &&
        (t->basicType->choiceId != BASICTYPE_SETOF) &&
        (t->basicType->choiceId != BASICTYPE_SEQUENCEOF))
    {
        fprintf (f," ");
        PrintSubtype (f, head, t, t->subtypes);
    }


    if (t->defaultVal != NULL)
    {
        fprintf (f, " DEFAULT ");
        if (t->defaultVal->fieldName != NULL)
            fprintf (f, "%s ", t->defaultVal->fieldName);
        PrintValue (f, NULL, t, t->defaultVal->value);
    }

    else if (t->optional)
        fprintf (f, " OPTIONAL");


#ifdef DEBUG
    fprintf (f, "  -- lineNo = %d", t->lineNo);
    fprintf (f, " --");
#endif

}  /* SpecialPrintType */


/*
 * This is used by the header file generators to annotate
 * the C/C++ types.  This version prints the C version of the
 * enum/bits elmt names to make sure the programmer can use
 * the correct defines/enum constants.
 * NOTE: this can only be called after the CTRI infor is filled in
 * so the C/C++ names can be accessed
 */
void
SpecialPrintNamedElmts PARAMS ((f, head, t),
    FILE *f _AND_
    TypeDef *head _AND_
    Type *t)
{
    CNamedElmt *last;
    CNamedElmt *cne;
    CNamedElmts *n = NULL;

    if (t->cTypeRefInfo != NULL)
        n = t->cTypeRefInfo->cNamedElmts;

    if ((n == NULL) && (t->cxxTypeRefInfo != NULL))
        n = t->cxxTypeRefInfo->namedElmts;


    if ((n == NULL) || LIST_EMPTY (n))
        return;

    fprintf (f," { ");
    last = (CNamedElmt*)LAST_LIST_ELMT (n);
    FOR_EACH_LIST_ELMT (cne, n)
    {
        fprintf (f, "%s (%d)", cne->name, cne->value);
        if (cne != last)
            fprintf (f,", ");
    }
    fprintf (f," } ");
}  /* SpecialPrintNamedElmts */
