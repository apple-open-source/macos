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
 * compiler/back-ends/c-gen/gen-enc.c - routines for printing c encoders from type trees
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/gen-enc.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-enc.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:42  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:42:24  rj
 * file name has been shortened for redundant part: c-gen/gen-c-enc -> c-gen/gen-enc.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:23:10  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:24  rj
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
#include "tag-util.h"
#include "snacc-util.h"
#include "gen-enc.h"



static int moduleImplicitTagsG;
static CRules *genEncCRulesG;
extern char *valueArgNameG;

char *encodedLenVarNameG = "totalLen";
char *itemLenNameG = "itemLen";
char *listComponentNameG = "component";
char *listLenNameG = "listLen";
char *returnTypeG = "AsnLen";
extern char *bufTypeNameG;
extern char *lenTypeNameG;
extern char *tagTypeNameG;
extern char *envTypeNameG;


/* non-exported prototypes */

static void PrintCBerEncoderPrototype PROTO ((FILE *hdr, TypeDef *td));
static void PrintCBerEncoderDeclaration PROTO ((FILE *src, TypeDef *td));
static void PrintCBerEncoderDefine PROTO ((FILE *src, TypeDef *td));

static void PrintCBerEncoderLocals PROTO ((FILE *src, TypeDef *td));

static void PrintCBerElmtsEncodeCode PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *e, int level, char *varName));
static void PrintCBerElmtEncodeCode PROTO ((FILE *src, TypeDef *td, Type *parent, NamedType *e, int level, char *varName));

static void PrintCBerListEncoderCode PROTO ((FILE *src, TypeDef *td, Type *t, int level, char *varName));
static void PrintCBerChoiceEncodeCode PROTO ((FILE *src, TypeDef *td, Type *t, int level, char *varName));

static void PrintCTagAndLenEncodingCode PROTO ((FILE *src, TypeDef *td, Type *t));

static void PrintEocEncoders PROTO ((FILE *src, TypeDef *td, Type *t));

static void PrintCLenEncodingCode PROTO ((FILE *f, int isCons, int isShort));

static void PrintCTagAndLenList PROTO ((FILE *src, Type *t,TagList *tg));




void
PrintCBerEncoder PARAMS ((src, hdr, r, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td)
{
    enum BasicTypeChoiceId typeId;
    int elmtLevel;
    CTDI *ctdi;
    TagList *tags;
    Tag *tag;
    char *formStr;
    char *classStr;
    int tagLen;
    int stoleChoiceTags;


    ctdi = td->cTypeDefInfo;
    if (!ctdi->genEncodeRoutine)
        return;

    /*
     *  if is type that refs another pdu type or lib type
     *  without generating a new type via tagging or named elmts
     *  print define to the hdr file
     * (a type is a pdu by default if it is ref'd by an ANY)
     */
    if (!IsNewType (td->type)  &&
          (!IsTypeRef (td->type) ||
           (IsTypeRef (td->type) &&
            (td->type->basicType->a.localTypeRef->link->cTypeDefInfo->isPdu ||
             ((td->type->basicType->a.localTypeRef->link->anyRefs != NULL) &&
              !LIST_EMPTY (td->type->basicType->a.localTypeRef->link->anyRefs))))))
    {
        fprintf(hdr,"#define B%s B%s\n", td->cTypeDefInfo->encodeRoutineName, td->type->cTypeRefInfo->encodeRoutineName);
/*
        fprintf(hdr,"#define B%s(b, v, bytesDecoded, env) B%s(b, v, bytesDecoded, env)\n", td->cTypeDefInfo->encodeRoutineName, td->type->cTypeRefInfo->encodeRoutineName);
  */
        return;
    }

    typeId = GetBuiltinType (td->type);

    /* print proto to hdr file */
    fprintf (hdr,"%s B%s PROTO ((%s b, %s *v));\n\n", lenTypeNameG, ctdi->encodeRoutineName, bufTypeNameG, ctdi->cTypeName);

    /* print routine to src file */
    fprintf (src,"%s B%s PARAMS ((b, v),\n", lenTypeNameG, ctdi->encodeRoutineName);
    fprintf (src,"%s b _AND_\n",bufTypeNameG);
    fprintf (src,"%s *v)\n",ctdi->cTypeName);
    fprintf (src,"{\n");
    fprintf (src,"    %s l;\n", lenTypeNameG);

    PrintEocEncoders (src, td, td->type);

    fprintf (src,"    l = B%sContent (b, v);\n", ctdi->encodeRoutineName);

    /* encode each tag/len pair if any */
    tags = GetTags (td->type, &stoleChoiceTags);
    if (! stoleChoiceTags)
    {
        FOR_EACH_LIST_ELMT_RVS (tag, tags)
        {
            classStr = Class2ClassStr (tag->tclass);

            if (tag->form == ANY_FORM)
                tag->form = PRIM;
            formStr = Form2FormStr (tag->form);
            tagLen = TagByteLen (tag->code);


            if (tag->form == CONS)
                fprintf (src,"    l += BEncConsLen (b, l);\n");
            else
                fprintf (src,"    l += BEncDefLen (b, l);\n");

            if (tag->tclass == UNIV)
                fprintf (src,"    l += BEncTag%d (b, %s, %s, %s);\n", tagLen, classStr, formStr, Code2UnivCodeStr (tag->code));
            else
                fprintf (src,"    l += BEncTag%d (b, %s, %s, %d);\n", tagLen, classStr, formStr, tag->code);
        }
    }
    fprintf (src,"    return l;\n");
    fprintf (src,"} /* B%s */\n\n", ctdi->encodeRoutineName);

    FreeTags (tags);
}  /*  PrintCBerEncoder */

void
PrintCBerContentEncoder PARAMS ((src, hdr, r, m, td),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td)
{
    NamedType *e;
    CTDI *ctdi;
    CTypeId rhsTypeId;  /* cTypeId of the type that defined this typedef */

    genEncCRulesG = r;

    ctdi =  td->cTypeDefInfo;
    if (!ctdi->genEncodeRoutine)
        return;

    rhsTypeId = td->type->cTypeRefInfo->cTypeId;
    switch (rhsTypeId)
    {
        case C_ANY:
            fprintf (hdr, "/* ANY - Fix Me! */\n");

            /*
             * Note - ANY's don't have the 'Content' suffix cause they
             * encode their tags and lengths
             */
            fprintf(hdr, "#define B%s B%s\n", td->cTypeDefInfo->encodeRoutineName, td->type->cTypeRefInfo->encodeRoutineName);

/*
            fprintf(hdr, "#define B%s( b, v)  ",td->cTypeDefInfo->encodeRoutineName);
            fprintf (hdr, "B%s (b, v)", td->type->cTypeRefInfo->encodeRoutineName);
*/


            break;

        case C_LIB:
        case C_TYPEREF:
            PrintCBerEncoderDefine (hdr, td);
            fprintf (hdr,"\n\n");
            break;

        case C_CHOICE:
            PrintCBerEncoderPrototype (hdr, td);
            PrintCBerEncoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerEncoderLocals (src, td);
            fprintf (src,"\n\n");
            PrintCBerChoiceEncodeCode (src, td, td->type, FIRST_LEVEL, valueArgNameG);
            fprintf (src,"    return %s;\n\n", encodedLenVarNameG);
            fprintf (src,"}  /* B%sContent */",td->cTypeDefInfo->encodeRoutineName);
            fprintf (hdr,"\n\n");
            fprintf (src,"\n\n");
            break;

        case C_STRUCT:
            PrintCBerEncoderPrototype (hdr, td);
            PrintCBerEncoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerEncoderLocals (src, td);
            fprintf (src,"\n\n");
            PrintCBerElmtsEncodeCode (src, td, td->type, td->type->basicType->a.set, FIRST_LEVEL, valueArgNameG);
            fprintf (src,"    return %s;\n\n", encodedLenVarNameG);
            fprintf (src,"}  /* B%sContent */",td->cTypeDefInfo->encodeRoutineName);
            fprintf (hdr,"\n\n");
            fprintf (src,"\n\n");
            break;


        case C_LIST:
            PrintCBerEncoderPrototype (hdr, td);
            fprintf (hdr,"\n\n");

            PrintCBerEncoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerEncoderLocals (src, td);
            fprintf (src,"\n\n");
            PrintCBerListEncoderCode (src, td, td->type, FIRST_LEVEL, valueArgNameG);
            fprintf (src,"    return %s;\n\n", listLenNameG);
            fprintf (src,"}  /* B%sContent */", td->cTypeDefInfo->encodeRoutineName);
            fprintf (src,"\n\n");
            break;

        case C_NO_TYPE:
/*            fprintf (src," sorry, unsupported type \n\n"); */
            break;

        default:
            fprintf (stderr,"PrintCBerEncoder: ERROR - unknown c type id\n");
            break;
    }

}  /*  PrintCBerContentEncoder */



/*
 * Prints prototype for encode routine in hdr file
 */
static void
PrintCBerEncoderPrototype PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi =  td->cTypeDefInfo;
    fprintf (hdr,"%s B%sContent PROTO ((%s b, %s *v));", returnTypeG, ctdi->encodeRoutineName, bufTypeNameG, ctdi->cTypeName);

}  /*  PrintCBerEncoderPrototype */



/*
 * Prints declarations of encode routine for the given type def
 */
static void
PrintCBerEncoderDeclaration PARAMS ((src, td),
    FILE *src _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi =  td->cTypeDefInfo;
    fprintf (src,"%s\nB%sContent PARAMS ((b, v),\n%s b _AND_\n%s *v)\n", returnTypeG, ctdi->encodeRoutineName, bufTypeNameG, ctdi->cTypeName);

}  /*  PrintCBerEncoderDeclaration */




/*
 * makes a define for type refs or primitive type renaming
 * EG:
 * TypeX ::= INTEGER --> #define BerEncodeTypeX(b,v) BerEncodeInteger(b,v)
 * TypeX ::= TypeY --> #define BerEncodeTypeX(b,v) BerEncodeTypeY(b,v)
 */
static void
PrintCBerEncoderDefine PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    fprintf(hdr, "#define B%sContent B%sContent", td->cTypeDefInfo->encodeRoutineName, td->type->cTypeRefInfo->encodeRoutineName);

/*
    fprintf(hdr, "#define B%sContent( b, v)  ",td->cTypeDefInfo->encodeRoutineName);
    fprintf (hdr, "B%sContent (b, v)", td->type->cTypeRefInfo->encodeRoutineName);
*/
}  /*  PrintCBerEncoderDefine */




static void
PrintCBerEncoderLocals PARAMS ((src, td),
    FILE *src _AND_
    TypeDef *td)
{
    fprintf (src, "    AsnLen %s = 0;\n", encodedLenVarNameG);
    fprintf (src, "    AsnLen %s;\n", itemLenNameG);
    fprintf (src, "    AsnLen %s;\n", listLenNameG);
    fprintf (src, "    void *%s;", listComponentNameG);

}  /*  PrintCBerEncoderLocals */



/*
 * runs through elmts backwards and prints
 * encoding code for each one
 */
static void
PrintCBerElmtsEncodeCode PARAMS ((src, td, parent, elmts, level, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    int level _AND_
    char *varName)
{
    NamedType *e;

    if (elmts == NULL)
    {
        fprintf (src,"/* ERROR? - expected elmts for this type*/\n");
        return;
    }

    /*
     * remember! encoding "backwards" so recursively traverse
     * list backwards
     */
    FOR_EACH_LIST_ELMT_RVS (e, elmts)
    {
        PrintCBerElmtEncodeCode (src, td, parent, e, level, varName);
    }

}  /* PrintCBerElmtsEncodeCode */



/*
 * Prints code for encoding the elmts of a SEQ or SET
 */
static void
PrintCBerElmtEncodeCode PARAMS ((src, td, parent, e, level, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedType *e _AND_
    int level _AND_
    char *varName)
{
    CTRI *ctri;
    char elmtVarRef[MAX_VAR_REF];
    char idVarRef[MAX_VAR_REF];
    enum BasicTypeChoiceId tmpTypeId;
    Type *tmpType;
    NamedType *idNamedType;

    if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
        return;

    ctri =  e->type->cTypeRefInfo;

    /* check if meant to be encoded */
    if (!ctri->isEncDec)
        return;


    MakeVarPtrRef (genEncCRulesG, td, parent, e->type, varName, elmtVarRef);

    if (e->type->optional || (e->type->defaultVal != NULL))
        fprintf (src, "    if (%s (%s))\n    {\n", ctri->optTestRoutineName, elmtVarRef);

    PrintEocEncoders (src, td, e->type);

    switch (ctri->cTypeId)
    {
        case C_ANYDEFINEDBY:

            /* get type of 'defining' field (int/enum/oid)*/
            idNamedType = e->type->basicType->a.anyDefinedBy->link;
            tmpTypeId = GetBuiltinType (idNamedType->type);

            if (tmpTypeId == BASICTYPE_OID)
            {
                MakeVarPtrRef (genEncCRulesG, td, parent, idNamedType->type, varName, idVarRef);
                fprintf (src, "    SetAnyTypeByOid (%s, %s);\n", elmtVarRef, idVarRef);
            }
            else
            {
                /* want to ref int by value not ptr */
                MakeVarValueRef (genEncCRulesG, td, parent, idNamedType->type, varName, idVarRef);
                fprintf (src, "    SetAnyTypeByInt (%s, %s);\n", elmtVarRef, idVarRef);
            }

            /* ANY's enc's do tag and len so zap the Content suffix */
            fprintf (src, "    %s = B%s (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
            break;

        case C_TYPEREF:
            tmpType = GetType (e->type);

             /* NOTE: ANY DEFINED BY must be directly in the parent (not ref)*/
            if (tmpType->cTypeRefInfo->cTypeId != C_ANY)
            {
                fprintf (src, "    %s = B%sContent (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
                break;
            }
            else  /* fall through */

        case C_ANY:
            /* ANY's enc's do tag and len so zap the Content suffix */
            fprintf (src,"     /* ANY - Fix Me! */\n");
            fprintf (src, "    SetAnyTypeBy???(%s, ???);\n", elmtVarRef);
            fprintf (src, "    %s = B%s (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
            break;


        case C_LIB:
            fprintf (src, "    %s = B%sContent (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
            break;

        case C_CHOICE:
            PrintCBerChoiceEncodeCode (src, td, e->type, level+1, elmtVarRef);
            break;

        case C_STRUCT:
            PrintCBerElmtsEncodeCode (src, td, e->type, e->type->basicType->a.set, level+1, elmtVarRef);
            break;

        case C_LIST:
            PrintCBerListEncoderCode (src, td, e->type, level+1, elmtVarRef);
            fprintf (src, "    %s = %s;\n", itemLenNameG, listLenNameG);
            fprintf (src,"\n\n");
            break;

        case C_NO_TYPE:
            break;

        default:
            fprintf (stderr,"PrintCBerElmtEncodeCode: ERROR - unknown c type id\n");
            break;
    }

    if (ctri->cTypeId != C_ANY) /* ANY's do their own tag/lens */
    {
        PrintCTagAndLenEncodingCode (src, td, e->type);
        fprintf (src,"\n    %s += %s;\n", encodedLenVarNameG, itemLenNameG);
    }

    if (e->type->optional || (e->type->defaultVal != NULL))
        fprintf (src, "    }\n");

    fprintf (src,"\n");

}  /*  PrintCBerElmtEncodeCode */




/*
 * Generates code for internally defined lists
 * eg:
 * TypeX = SET { foo INTEGER, bar SEQUENCE OF INTEGER } -->
 * BerEncodeTypeX (b, v)
 * {
 *    ...
 *         listLen = 0;
 *         FOR_EACH_LIST_ELMT (component, v->bar)
 *         {
 *              itemLen = BerEncodeInteger (b, (int*) component);
 *              itemLen+= EncodeLen (b, itemLen)
 *              itemLen += ENCODE_TAG (b, INTEGER_TAG);
 *              listLen += itemLen;
 *         }
 *    ...
 * }
 */
static void
PrintCBerListEncoderCode PARAMS ((src, td, t, level, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t _AND_
    int level _AND_
    char *varName)
{
    CTRI *ctri;
    char *elmtVarRef = "component";
    Type *tmpType;
    enum BasicTypeChoiceId tmpTypeId;
    TypeDef *idNamedType;


    ctri =  t->basicType->a.setOf->cTypeRefInfo;

    if (ctri == NULL)
        return;

    fprintf (src, "        listLen = 0;\n");
    fprintf (src, "    FOR_EACH_LIST_ELMT_RVS (component, %s)\n", varName);
    fprintf (src, "    {\n");

    PrintEocEncoders (src, td, t->basicType->a.setOf);

    /*
     * need extra case here for SET OF typedef not just SET OF typeref
     */
    switch (ctri->cTypeId)
    {

        case C_TYPEREF:
            tmpType = GetType (t->basicType->a.setOf);

             /* NOTE: ANY DEFINED BY must be directly in the parent (not ref)*/
            if (tmpType->cTypeRefInfo->cTypeId != C_ANY)
            {
                fprintf (src, "    %s = B%sContent (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
                break;
            }
            else  /* fall through */

        case C_ANY:
            /* ANY's enc's do tag and len so zap the Content suffix */
            fprintf (src,"     /* ANY - Fix Me! */\n");
            fprintf (src, "    SetAnyTypeBy???(%s, ???);\n", elmtVarRef);
            fprintf (src, "    %s = B%s (b, %s);\n", itemLenNameG, ctri->encodeRoutineName, elmtVarRef);
            break;



        default:
            fprintf (src, "    %s = B%sContent (b, (%s*) %s);\n", itemLenNameG, ctri->encodeRoutineName, ctri->cTypeName, elmtVarRef);
            break;

    }

    PrintCTagAndLenEncodingCode (src, td, t->basicType->a.setOf);
    fprintf (src,"\n");
    fprintf (src, "        %s += %s;\n", listLenNameG, itemLenNameG);
    fprintf (src, "    }\n");

}  /*  PrintCBerListEncoderCode */



static void
PrintCBerChoiceEncodeCode PARAMS ((src, td, t, level, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t _AND_
    int level _AND_
    char *varName)
{
    NamedType *e;
    CTRI *ctri;
    void *tmp;

    ctri =  t->cTypeRefInfo;

    fprintf (src,"    switch (%s->%s)\n    {\n", varName, ctri->choiceIdEnumFieldName);

    FOR_EACH_LIST_ELMT (e, t->basicType->a.choice)
    {
        tmp = (void*)CURR_LIST_NODE (t->basicType->a.choice);

        if (e->type == NULL)
            continue;

        ctri =  e->type->cTypeRefInfo;

        if (ctri != NULL)
            fprintf (src, "       case %s:\n", ctri->choiceIdSymbol);
        else
            fprintf (src, "       case ????:\n");



        PrintCBerElmtEncodeCode (src, td, t, e, level+1, varName);
        fprintf (src,"    break;\n\n");

        SET_CURR_LIST_NODE (t->basicType->a.choice, tmp);
    }

    fprintf (src, "    }\n");
}  /* PrintCBerChoiceEncodeCode */



/*
 *  prints DecodeBerEocIfNec (b) for each constructed len
 *  assoc with given type
 */
static void
PrintEocEncoders PARAMS ((src, td, t),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t)
{
    TagList *tl;
    Tag *tag;
    int consTagCount;
    int stoleChoiceTags;

    /*
     * get all the tags on this type
     */
    tl = (TagList*) GetTags (t, &stoleChoiceTags);

    /*
     * leave choice elmt tag enc to encoding routine
     */

    if (!stoleChoiceTags)
    {
        FOR_EACH_LIST_ELMT (tag, tl)
        {
            if (tag->form == CONS)
                fprintf (src,"    BEncEocIfNec (b);\n");
        }
    }
/*
    consTagCount = 0;
    if (!stoleChoiceTags)
    {
        FOR_EACH_LIST_ELMT (tag, tl)
            consTagCount++;
    }

    if (IsPrimitiveByDefOrRef (t))
        consTagCount--;

    for (; consTagCount > 0; consTagCount--)
        fprintf (src,"    BEncEocIfNec (b);\n");

*/

    FreeTags (tl);

}  /* PrintEocEncoders */


/*
 *  Recursively walks throught type refs printing lower lvl tags
 *  first (since encoding is done backwards).
 *
 */
static void
PrintCTagAndLenEncodingCode PARAMS ((src, td, t),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t)
{
    TagList *tl;
    int stoleChoiceTags;

    /*
     * get all the tags on this type
     */
    tl = (TagList*) GetTags (t, &stoleChoiceTags);

    /*
     * leave choice elmt tag enc to encoding routine
     */
    if (!stoleChoiceTags)
        PrintCTagAndLenList (src, t, tl);

    FreeTags (tl);

}  /* PrintCTagAndLenEncodingCode */



/*
 * prints last tag's encoding code first
 */
static void
PrintCTagAndLenList PARAMS ((src, t, tagList),
    FILE *src _AND_
    Type *t _AND_
    TagList *tagList)
{
    char *classStr;
    char *formStr;
    char *codeStr;
    Tag *tg;
    Tag *last;
    int tagLen;
    enum BasicTypeChoiceId typesType;
    int isShort;

    if ((tagList == NULL) || LIST_EMPTY (tagList))
        return;

    /*
     * efficiency hack - use simple length (1 byte)
     * encoded for type (almost) guaranteed to have
     * encoded lengths of 0 <= len <= 127
     */
    typesType = GetBuiltinType (t);
    if ((typesType == BASICTYPE_BOOLEAN) ||
        (typesType == BASICTYPE_INTEGER) ||
        (typesType == BASICTYPE_NULL) ||
        (typesType == BASICTYPE_REAL) ||
        (typesType == BASICTYPE_ENUMERATED))
        isShort = 1;
    else
        isShort = 0;

    /*
     * since encoding backward encode tags backwards
     */
    last = (Tag*)LAST_LIST_ELMT (tagList);
    FOR_EACH_LIST_ELMT_RVS (tg, tagList)
    {
        classStr = Class2ClassStr (tg->tclass);

        if (tg->form == CONS)
        {
            formStr = Form2FormStr (CONS);
            PrintCLenEncodingCode (src, TRUE, isShort);
        }
        else /* PRIM or ANY_FORM */
        {
            formStr = Form2FormStr (PRIM);
            PrintCLenEncodingCode (src, FALSE, isShort);
        }

/*       GetTags sets the form bit correctly now
        if (IsPrimitiveByDefOrRef (t) && (tg == last))
        {
            formStr = Form2FormStr (PRIM);
            PrintCLenEncodingCode (src, FALSE, isShort);
        }
        else
        {
            formStr = Form2FormStr (CONS);
            PrintCLenEncodingCode (src, TRUE, isShort);
        }
  */

        fprintf (src,"\n");

        if (tg->code < 31)
            tagLen = 1;
        else if (tg->code < 128)
            tagLen = 2;
        else if (tg->code < 16384)
            tagLen = 3;
        else if (tg->code < 2097152)
            tagLen = 4;
        else
            tagLen = 5;

        fprintf (src,"    %s += BEncTag%d (b, %s, %s, %d);\n", itemLenNameG, tagLen, classStr, formStr, tg->code);
    }

}  /* PrintCTagAndLenList */

/*
 * prints length encoding code.  Primitives always use
 * definite length and constructors get "ConsLen"
 * which can be configured at compile to to be indefinite
 * or definite.  Primitives can also be "short" (isShort is true)
 * in which case a fast macro is used to write the length.
 * Types for which isShort apply are: boolean, null and
 * (almost always) integer and reals
 */
static void
PrintCLenEncodingCode PARAMS ((f, isCons, isShort),
    FILE *f _AND_
    int isCons _AND_
    int isShort)
{
   /* fprintf (f, "    BER_ENCODE_DEF_LEN (b, itemLen, itemLen);"); */
    if (isCons)
        fprintf (f, "    itemLen += BEncConsLen (b, itemLen);");
    else
    {
        if (isShort)
        {
            fprintf (f, "    BEncDefLenTo127 (b, itemLen);\n");
            fprintf (f, "    itemLen++;");
        }
        else
            fprintf (f, "    itemLen += BEncDefLen (b, itemLen);");
    }
}  /* PrintCLenEncodingCode */
