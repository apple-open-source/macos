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
 * compiler/back-ends/c-gen/gen-dec.c - routines for printing C decoders from type trees
 *
 *   The type tree has already been run through the c type generator
 *   (type-info.c).  Types that the type generator didn't know how
 *   to handle (or didn't want/need to handle eg macros) get the
 *   C_NO_TYPE label and are ignored for code generation.
 *
 *   NOTE: this is a real rats nest - it sort of evolved.  It was
 *         written assuming SETs/SEQ/CHOICE etc could be nested
 *         hence all the crap about 'levels'.
 *
 * Mike Sample
 * 91/10/23
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/gen-dec.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: gen-dec.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:41  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1997/02/28 13:39:54  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.3  1995/07/25 18:41:01  rj
 * file name has been shortened for redundant part: c-gen/gen-c-dec -> c-gen/gen-dec.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:22:06  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:20  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "define.h"
#include "lib-types.h"
#include "rules.h"
#include "type-info.h"
#include "str-util.h"
#include "snacc-util.h"
#include "util.h"
#include "tag-util.h"
#include "gen-dec.h"


static CRules *genDecCRulesG;
char *valueArgNameG = "v";
static long int *longJmpValG;
static char *decodedLenVarNameG = "totalElmtsLen";
static char *itemLenVarNameG = "elmtLen";
static char *mecVarNameG = "mandatoryElmtCount";
static char *tagIdVarNameG = "tagId";
char *bufTypeNameG  = "BUF_TYPE";
char *lenTypeNameG = "AsnLen";
char *tagTypeNameG = "AsnTag";
char *envTypeNameG = "ENV_TYPE";



/* non-exported prototypes */

static void PrintCBerDecoderPrototype PROTO ((FILE *hdr, TypeDef *td));
static void PrintCBerDecoderDeclaration PROTO ((FILE *src, TypeDef *td));
static void PrintCBerDecoderDefine PROTO ((FILE *src, TypeDef *td));

static int RecCountVariableLevels PROTO ((Type *t));
static int CountVariableLevels PROTO ((Type *t));
static void PrintCBerDecoderLocals PROTO ((FILE *src, TypeDef *td));
static void PrintCBerListDecoderLocals PROTO ((FILE *src));

static void PrintCBerSetDecodeCode PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *e, int elmtLevel, int totalLevel, int tagLevel, char *varName));

static void PrintCBerSeqDecodeCode PROTO ((FILE *src, TypeDef *td, Type *parent, NamedTypeList *e, int elmtLevel, int totalLevel, int tagLevel, char *varName));

static void PrintCBerListDecoderCode PROTO ((FILE *src, TypeDef *td, Type *t, int elmtLevel, int totalLevel, int tagLevel, char *varName));

static void PrintCBerChoiceDecodeCode PROTO ((FILE *src, TypeDef *td, Type *t, int elmtLevel, int totalLevel, int tagLevel, char *varName));

static void PrintCLenDecodingCode PROTO ((FILE *f));

static void PrintCBerDecoderIncludes PROTO ((FILE *src, Module *m, ModuleList *mods));

static void PrintCBerElmtDecodeCode PROTO ((FILE *src, TypeDef *td, Type *parent, Type *t, int elmtLevel, int totalLevel, int tagLevel, char *parnetVarName, char *elmtVarName, int stoleChoiceTags));


void
PrintCBerDecoder PARAMS ((src, hdr, r, m,  td, longJmpVal),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    long int *longJmpVal)
{
    int i;
    enum BasicTypeChoiceId typeId;
    int elmtLevel;
    CTDI *ctdi;
    Tag *tag;
    char *classStr;
    char *formStr;
    int stoleChoiceTags;
    TagList *tags;

    ctdi = td->cTypeDefInfo;
    if (!ctdi->genDecodeRoutine)
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
        fprintf(hdr,"#define B%s B%s\n", td->cTypeDefInfo->decodeRoutineName, td->type->cTypeRefInfo->decodeRoutineName);
/*
         fprintf(hdr,"#define B%s(b, v, bytesDecoded, env) B%s(b, v, bytesDecoded, env)\n", td->cTypeDefInfo->decodeRoutineName, td->type->cTypeRefInfo->decodeRoutineName);
*/
        return;
    }



    typeId = GetBuiltinType (td->type);

    /* print proto type to hdr file */
    fprintf (hdr, "void B%s PROTO ((%s b, %s *result, %s *bytesDecoded, %s env));\n", ctdi->decodeRoutineName, bufTypeNameG, ctdi->cTypeName, lenTypeNameG, envTypeNameG);

    /* print routine in src */
    fprintf (src,"void B%s PARAMS ((b, result, bytesDecoded, env),\n", ctdi->decodeRoutineName);
    fprintf (src,"%s b _AND_\n", bufTypeNameG);
    fprintf (src,"%s *result _AND_\n", ctdi->cTypeName);
    fprintf (src,"%s *bytesDecoded _AND_\n", lenTypeNameG);
    fprintf (src,"%s env)\n", envTypeNameG);
    fprintf (src,"{\n");
    fprintf (src,"    %s tag;\n", tagTypeNameG);

    /* print extra locals for redundant lengths */
    tags = GetTags (td->type, &stoleChoiceTags);
    for (i = 1; !stoleChoiceTags && (i <= LIST_COUNT (tags)); i++)
        fprintf (src,"    %s elmtLen%d;\n", lenTypeNameG, i);

    /* add extra len for choice */
    if (typeId == BASICTYPE_CHOICE)
        fprintf (src,"    %s elmtLen%d;\n", lenTypeNameG, i);

    fprintf (src,"\n");

    /* decode tag/length pairs */
    elmtLevel = 0;
    if (!stoleChoiceTags)
    {
        FOR_EACH_LIST_ELMT (tag, tags)
        {
            classStr = Class2ClassStr (tag->tclass);
            if (tag->form == ANY_FORM)
                formStr = Form2FormStr (PRIM);
            else
                formStr = Form2FormStr (tag->form);

            fprintf (src,"    if (((tag = BDecTag (b, bytesDecoded, env)) != \n");

            if (tag->tclass == UNIV)
            {
                fprintf (src,"MAKE_TAG_ID (%s, %s, %s))", classStr, formStr, Code2UnivCodeStr (tag->code));
                if (tag->form == ANY_FORM)
                    fprintf (src,"&&\n         (tag != MAKE_TAG_ID (%s, %s, %s)))\n", classStr, Form2FormStr (CONS), Code2UnivCodeStr (tag->code));
                else
                    fprintf (src,")\n");
            }
            else
            {
                fprintf (src,"MAKE_TAG_ID (%s, %s, %d))", classStr, formStr, tag->code);
                if (tag->form == ANY_FORM)
                    fprintf (src,"&&\n        (tag != MAKE_TAG_ID (%s, %s, %d)))\n", classStr, Form2FormStr (CONS), tag->code);
                else
                    fprintf (src,")\n");

            }
            fprintf (src,"    {\n");
            fprintf (src,"        Asn1Error (\"B%s: ERROR - wrong tag\\n\");\n", ctdi->decodeRoutineName);
            fprintf (src,"        longjmp (env, %d);\n", (*longJmpVal)--);
            fprintf (src,"    }\n");

            fprintf (src,"    elmtLen%d = BDecLen (b, bytesDecoded, env);\n", ++elmtLevel);
        }
    }

    /* for choices always decode first tag of the choice's content */
    if (typeId == BASICTYPE_CHOICE)
    {
        fprintf (src,"    tag = BDecTag (b, bytesDecoded, env);\n");
        fprintf (src,"    elmtLen%d = BDecLen (b, bytesDecoded, env);\n", ++elmtLevel);
    }

    if ((typeId != BASICTYPE_ANY) && (typeId != BASICTYPE_ANYDEFINEDBY))
        fprintf (src,"    B%sContent (b, tag, elmtLen%d, result, bytesDecoded, env);\n", ctdi->decodeRoutineName, elmtLevel);
    else
        fprintf (src,"    B%s (b, result, bytesDecoded, env);\n", ctdi->decodeRoutineName, elmtLevel);


    /* grab any EOCs that match redundant, indef lengths */
    for (i = elmtLevel-1; i > 0; i--)
    {
        fprintf (src,"    if (elmtLen%d == INDEFINITE_LEN)\n", i);
        fprintf (src,"        BDecEoc (b, bytesDecoded, env);\n");
    }


    fprintf (src,"}  /* B%s */\n\n", ctdi->decodeRoutineName);

    FreeTags (tags);
}  /*  PrintCBerDecoder */


void
PrintCBerContentDecoder PARAMS ((src, hdr, r, m,  td, longJmpVal),
    FILE *src _AND_
    FILE *hdr _AND_
    CRules *r _AND_
    Module *m _AND_
    TypeDef *td _AND_
    long int *longJmpVal)
{
    NamedType *e;
    CTDI *ctdi;
    CTypeId rhsTypeId;  /* cTypeId of the type that defined this typedef */
    Type *t;
    BER_FORM form;

    longJmpValG = longJmpVal;

    genDecCRulesG = r;

    ctdi =  td->cTypeDefInfo;
    if ((ctdi == NULL) || (td->type->cTypeRefInfo == NULL))
    {
        fprintf (stderr,"PrintCBerDecoder: ERROR - no type info\n");
        return;
    }

    if (!ctdi->genDecodeRoutine)
        return;

    rhsTypeId = td->type->cTypeRefInfo->cTypeId;
    switch (rhsTypeId)
    {
        /*
         * type refs or primitive types are
         * defined as calls to the referenced type
         */
        case C_ANY:
            fprintf (hdr, "/* ANY - Fix Me! */\n");
        case C_ANYDEFINEDBY:
            fprintf(hdr, "#define B%s B%s\n", td->cTypeDefInfo->decodeRoutineName, td->type->cTypeRefInfo->decodeRoutineName);

/*
            fprintf(hdr, "#define B%s( b, tagId, elmtLen, v, bytesDecoded, env)  ", td->cTypeDefInfo->decodeRoutineName);
            fprintf (hdr, "B%s (b, tagId, elmtLen, v, bytesDecoded, env)", td->type->cTypeRefInfo->decodeRoutineName);
*/
            fprintf (hdr,"\n\n");
            break;

        case C_LIB:
        case C_TYPEREF:
            PrintCBerDecoderDefine (hdr, td);
            fprintf (hdr,"\n\n");
            break;


        case C_CHOICE:
            PrintCBerDecoderPrototype (hdr, td);
            fprintf (hdr,"\n\n");
            PrintCBerDecoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerDecoderLocals (src, td);
            fprintf (src,"\n\n");
            PrintCBerChoiceDecodeCode (src, td, td->type, FIRST_LEVEL-1, FIRST_LEVEL,FIRST_LEVEL-1, valueArgNameG);

            fprintf (src, "    (*bytesDecoded) += totalElmtsLen1;\n");
            fprintf (src,"}  /* B%sContent */",td->cTypeDefInfo->decodeRoutineName);
            fprintf (src,"\n\n");
            break;

        case C_STRUCT:
            PrintCBerDecoderPrototype (hdr, td);
            fprintf (hdr,"\n\n");
            PrintCBerDecoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerDecoderLocals (src, td);
            fprintf (src,"\n\n");
            if (td->type->basicType->choiceId == BASICTYPE_SET)
                PrintCBerSetDecodeCode (src, td, td->type, td->type->basicType->a.set, FIRST_LEVEL-1, FIRST_LEVEL, FIRST_LEVEL-1, valueArgNameG);
            else
                PrintCBerSeqDecodeCode (src, td, td->type, td->type->basicType->a.sequence, FIRST_LEVEL-1, FIRST_LEVEL, FIRST_LEVEL-1, valueArgNameG);

            fprintf (src, "    (*bytesDecoded) += totalElmtsLen1;\n");
            fprintf (src,"}  /* B%sContent */",td->cTypeDefInfo->decodeRoutineName);
            fprintf (src,"\n\n");
            break;


        case C_LIST:
            PrintCBerDecoderPrototype (hdr, td);
            fprintf (hdr,"\n\n");

            PrintCBerDecoderDeclaration (src, td);
            fprintf (src,"{\n");
            PrintCBerDecoderLocals (src, td);
            fprintf (src,"\n\n");
            PrintCBerListDecoderCode (src, td, td->type,  FIRST_LEVEL-1, FIRST_LEVEL, FIRST_LEVEL-1, valueArgNameG);

            fprintf (src, "    (*bytesDecoded) += totalElmtsLen1;\n");
            fprintf (src,"}  /* B%sContent */",td->cTypeDefInfo->decodeRoutineName);
            fprintf (src,"\n\n");
            break;

        case C_NO_TYPE:
/*            fprintf (src,"< sorry, unsupported type >\n\n"); */
            return; /* dont' print newlines */
            break;

        default:
            fprintf (stderr,"PrintCBerContentDecoder: ERROR - unknown c type id\n");
            return;
            break;
    }

}  /*  PrintCBerContentDecoder */




/*
 * Prints prototype for decode routine in hdr file
 */

static void
PrintCBerDecoderPrototype PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi =  td->cTypeDefInfo;
    fprintf (hdr,"void B%sContent PROTO ((%s b, %s tagId%d, %s elmtLen%d, %s *v, %s *bytesDecoded, %s env));\n", ctdi->decodeRoutineName, bufTypeNameG, tagTypeNameG, FIRST_LEVEL-1, lenTypeNameG, FIRST_LEVEL-1, ctdi->cTypeName,lenTypeNameG, envTypeNameG);

}  /*  PrintCBerDecoderPrototype */



/*
 * Prints declarations of decode routine for the given type def
 */
static void
PrintCBerDecoderDeclaration PARAMS ((src,td),
    FILE *src _AND_
    TypeDef *td)
{
    CTDI *ctdi;

    ctdi =  td->cTypeDefInfo;
    fprintf (src,"void\n");
    fprintf (src,"B%sContent PARAMS ((b, tagId%d, elmtLen%d, v, bytesDecoded, env),\n", ctdi->decodeRoutineName, FIRST_LEVEL -1,  FIRST_LEVEL -1);
    fprintf (src,"%s b _AND_\n", bufTypeNameG);
    fprintf (src,"%s tagId%d _AND_\n", tagTypeNameG, FIRST_LEVEL -1);
    fprintf (src,"%s elmtLen%d _AND_\n", lenTypeNameG, FIRST_LEVEL -1);
    fprintf (src,"%s *v _AND_\n", ctdi->cTypeName);
    fprintf (src,"%s *bytesDecoded _AND_\n", lenTypeNameG);
    fprintf (src,"%s env)\n", envTypeNameG);

}  /*  PrintCBerDecoderDeclaration */



/*
 * makes a define for type refs or primitive type renaming
 * EG:
 * TypeX ::= INTEGER --> #define BerDecodeTypeX(b,v) BerDecodeInteger(b,v)
 * TypeX ::= TypeY --> #define BerDecodeTypeX(b,v) BerDecodeTypeY(b,v)
 */
static void
PrintCBerDecoderDefine PARAMS ((hdr, td),
    FILE *hdr _AND_
    TypeDef *td)
{
    fprintf(hdr, "#define B%sContent B%sContent", td->cTypeDefInfo->decodeRoutineName, td->type->cTypeRefInfo->decodeRoutineName);

/*
    fprintf(hdr, "#define B%sContent( b, tagId, elmtLen, v, bytesDecoded, env)  ", td->cTypeDefInfo->decodeRoutineName);
    fprintf (hdr, "B%sContent (b, tagId, elmtLen, v, bytesDecoded, env)", td->type->cTypeRefInfo->decodeRoutineName);
*/
}  /*  PrintCBerDecoderDefine */



/*
 * used to figure out local variables to declare
 */
static int
RecCountVariableLevels PARAMS ((t),
    Type *t)
{
    CTRI *ctri;
    int maxLevels = 0;
    NamedType *e;
    TagList *tl;
    int tagCount;
    int typeCount;
    void *tmp;
    enum BasicTypeChoiceId typeId;

    ctri = t->cTypeRefInfo;
    typeId = GetBuiltinType (t);

    /* embedded struct/choices aren't really an issue any more */
    if ((ctri->cTypeId == C_STRUCT) ||
        (ctri->cTypeId == C_CHOICE))
    {
        maxLevels = 1;

        tagCount = CountTags (t);

        tmp = (void*)CURR_LIST_NODE (t->basicType->a.set);
        FOR_EACH_LIST_ELMT (e, t->basicType->a.set)
        {
            if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
                continue;

            typeCount = RecCountVariableLevels (e->type);

            if (typeCount > maxLevels)
                maxLevels = typeCount;
        }
        SET_CURR_LIST_NODE (t->basicType->a.set, tmp);
        return maxLevels + tagCount;
    }
    else if (ctri->cTypeId == C_LIST)
    {
        return CountTags (t) +RecCountVariableLevels (t->basicType->a.setOf);
    }
    else if (typeId == BASICTYPE_CHOICE)
        return CountTags (t) +1;
    else if ((typeId == BASICTYPE_ANY) || (typeId == BASICTYPE_ANYDEFINEDBY))
        return CountTags (t) +1;
    else
        return CountTags (t);

}  /* RecCountVariableLevels */



/*
 * returns the number of variable contexts needed for
 * decoding the contents of this type.  Does not consider tags on this type.
 */
static int
CountVariableLevels PARAMS ((t),
    Type *t)
{
    CTRI *ctri;
    int maxLevels = 0;
    NamedType *e;
    TagList *tl;
    int tagCount;
    int typeCount;
    void *tmp;

    ctri =  t->cTypeRefInfo;

    if ((ctri->cTypeId == C_STRUCT) ||
        (ctri->cTypeId == C_CHOICE))
    {
        maxLevels = 1;
        tmp = (void*)CURR_LIST_NODE (t->basicType->a.set);
        FOR_EACH_LIST_ELMT (e, t->basicType->a.set)
        {
            if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
                continue;

            typeCount = RecCountVariableLevels (e->type);

            /* add extra level since must decode key tag in choice */
            if (GetBuiltinType (e->type) == BASICTYPE_CHOICE)
                typeCount++;

            if (typeCount > maxLevels)
                maxLevels = typeCount;
        }
        SET_CURR_LIST_NODE (t->basicType->a.set, tmp);
        return maxLevels;
    }
    else if (ctri->cTypeId == C_LIST)
        return RecCountVariableLevels (t->basicType->a.setOf);
    else if ((ctri->cTypeId == C_ANY) ||
             (ctri->cTypeId == C_ANYDEFINEDBY))
        return 1;
    else
        return 0;
}  /* CountVariableLevels */



/*
 * prints local vars for constructed types (set/seq/choice)
 */
static void
PrintCBerDecoderLocals PARAMS ((src,td),
    FILE *src _AND_
    TypeDef *td)
{
    int levels;
    int i;

    levels = CountVariableLevels (td->type);

    fprintf (src, "    int seqDone = FALSE;\n");

    for (i = 0; i < levels; i++)
    {
        fprintf (src, "    %s totalElmtsLen%d = 0;\n", lenTypeNameG, i + FIRST_LEVEL);
        fprintf (src, "    %s elmtLen%d;\n", lenTypeNameG, i + FIRST_LEVEL);
        fprintf (src, "    %s tagId%d;\n", tagTypeNameG, i + FIRST_LEVEL);
        if (i == 0)
            fprintf (src, "    int mandatoryElmtCount%d = 0;\n", i + FIRST_LEVEL);
    }

}  /*  PrintCBerDecoderLocals */


/*
 * given the Type *(t) of an elmt in a set/seq/choice/list,
 * prints decoding code.
 *   elmtVarName is string ptr ref to field being decoded
 *     eg "(&personnelRecord.name)"
 *   stoleChoiceTags is as returned by GetTags
 *
 * elmtLevel - last elmtLen# var that is valid/used (has a len)
 * totalLevel - totalElmtsLen# to be used for running total of dec bytes
 * tagIdLevel - last tagId# var that is valid/used (contains a tag)
 */
static void
PrintCBerElmtDecodeCode PARAMS ((src, td, parent, t, elmtLevel, totalLevel, tagLevel, parentVarName, elmtVarName, stoleChoiceTags),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *t _AND_
    int elmtLevel _AND_
    int totalLevel _AND_
    int tagLevel _AND_
    char *parentVarName _AND_
    char *elmtVarName _AND_
    int stoleChoiceTags)
{
    CTRI *ctri;
    Type *tmpType;
    char idVarRef[MAX_VAR_REF];
    NamedType *idNamedType;
    enum BasicTypeChoiceId tmpTypeId;

    ctri =  t->cTypeRefInfo;

    /* check if meant to be encoded */
    if (!ctri->isEncDec)
        return;

    tmpType = GetType (t);

    if (tmpType->basicType->choiceId == BASICTYPE_ANY)
    {
        fprintf (src,"/* ANY - Fix Me ! */\n");
        fprintf (src,"    SetAnyTypeBy???(%s, ???);\n", elmtVarName);
        fprintf (src,"    B%s (b, %s, &%s%d, env);\n", ctri->decodeRoutineName, elmtVarName, decodedLenVarNameG, totalLevel);
    }
    else if (tmpType->basicType->choiceId == BASICTYPE_ANYDEFINEDBY)
    {
        /* get type of 'defining' field (int/enum/oid)*/
        idNamedType = t->basicType->a.anyDefinedBy->link;
        tmpTypeId = GetBuiltinType (idNamedType->type);

        if (tmpTypeId == BASICTYPE_OID)
        {
            MakeVarPtrRef (genDecCRulesG, td, parent, idNamedType->type, parentVarName, idVarRef);
            fprintf (src, "    SetAnyTypeByOid (%s, %s);\n", elmtVarName, idVarRef);
        }
        else
        {
            /* want to ref int by value not ptr */
            MakeVarValueRef (genDecCRulesG, td, parent, idNamedType->type, parentVarName, idVarRef);
            fprintf (src, "    SetAnyTypeByInt (%s, %s);\n", elmtVarName, idVarRef);
        }
        fprintf (src,"    B%s (b, %s, &%s%d, env);\n", ctri->decodeRoutineName, elmtVarName, decodedLenVarNameG, totalLevel);
    }
    else switch (ctri->cTypeId)
    {
        case C_LIB:
        case C_TYPEREF:
            /*
             * choices and octet/bit str types need tagId argument
             */
            if ((tmpType->basicType->choiceId == BASICTYPE_CHOICE) &&
                !stoleChoiceTags)
            {
                /*
                 * strip off top tag of choice in not already done
                 * since choice decoders assume you are passing in
                 * their top tag
                 */
                fprintf (src, "    %s%d = BDecTag (b, &%s%d, env);\n", tagIdVarNameG, ++tagLevel, decodedLenVarNameG, totalLevel);
                fprintf (src, "    %s%d = BDecLen (b, &%s%d, env);\n", itemLenVarNameG, ++elmtLevel, decodedLenVarNameG, totalLevel);
            }
            fprintf (src,"    B%sContent (b, %s%d, %s%d, %s, &%s%d, env);\n", ctri->decodeRoutineName, tagIdVarNameG, tagLevel, itemLenVarNameG, elmtLevel, elmtVarName, decodedLenVarNameG, totalLevel);

	    /* From ftp://ftp.cs.ubc.ca/pub/local/src/snacc/bugs-in-1.1 */
	    if ((tmpType->basicType->choiceId == BASICTYPE_CHOICE)
		    && !stoleChoiceTags)
	    {
		fprintf(src,"    if (elmtLen%d == INDEFINITE_LEN)\n", elmtLevel-1);
		fprintf(src,"        BDecEoc(b, &totalElmtsLen%d, env);\n", totalLevel);
	    }

        break;


        /*
         * NOTE: the CHOICE, STRUCT and LIST switch clauses won't
         * fire due to the current 'normalization'
         * (see normalize.c)
         */

        case C_CHOICE:
                /*
                 * strip off top tag of choice in not already done
                 * since choice decoders assume you are passing in
                 * their top tag
                 */
            if (!stoleChoiceTags)
            {
                fprintf (src, "    %s%d = BDecTag (b, &%s%d, env);\n\n", tagIdVarNameG, ++tagLevel, decodedLenVarNameG, totalLevel);

                fprintf (src, "    %s%d = BDecLen (b, &%s%d, env);\n", itemLenVarNameG, ++elmtLevel, decodedLenVarNameG, totalLevel);
            }
            PrintCBerChoiceDecodeCode (src, td, t, elmtLevel, totalLevel+1, tagLevel, elmtVarName);
        break;


        case C_STRUCT:
            if (t->basicType->choiceId == BASICTYPE_SET)
                PrintCBerSetDecodeCode (src, td, t, t->basicType->a.set, elmtLevel, totalLevel+1, tagLevel, elmtVarName);
            else
            {
                PrintCBerSeqDecodeCode (src, td, t, t->basicType->a.sequence, elmtLevel,totalLevel+1, tagLevel, elmtVarName);
                fprintf (src,"    seqDone = FALSE;\n");
            }
            fprintf (src,"             %s%d += %s%d;\n", decodedLenVarNameG, totalLevel, decodedLenVarNameG, totalLevel+1);
        break;


        case C_LIST:
            PrintCBerListDecoderCode (src, td, t,  elmtLevel, totalLevel+1, tagLevel, elmtVarName);
            fprintf (src,"\n\n");
            fprintf (src,"             %s%d += %s%d;\n", decodedLenVarNameG, totalLevel, decodedLenVarNameG, totalLevel+1);
        break;


        case C_NO_TYPE:
            break;

        default:
            fprintf (stderr,"PrintCBerElmtDecodeCode: ERROR - unknown c type id\n");
        break;
    }

} /* PrintCBerElmtDecodeCode */


/*
 * Prints code for decoding the elmts of SET
 */
static void
PrintCBerSetDecodeCode PARAMS ((src, td, parent, elmts, elmtLevel, totalLevel, tagLevel, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    int elmtLevel _AND_
    int totalLevel _AND_
    int tagLevel _AND_
    char *varName)
{
    NamedType *e;
    CTRI *ctri;
    TagList *tags;
    Tag *tag;
    TagList *tl;
    enum BasicTypeChoiceId builtinType;
    char *classStr;
    char *formStr;
    char *codeStr;
    int   mandatoryCount = 0;
    int   i;
    char  tmpVarName[MAX_VAR_REF];
    int   stoleChoiceTags;
    char *routineName;
    int initialTagLevel;
    int initialElmtLevel;


    initialTagLevel = tagLevel;
    initialElmtLevel = elmtLevel;


    routineName = td->cTypeDefInfo->decodeRoutineName;

    if ((elmts == NULL) || LIST_EMPTY (elmts)) /* empty set */
    {
        fprintf (src,"    if (elmtLen%d == INDEFINITE_LEN)\n", elmtLevel);
        fprintf (src,"    {\n");
        fprintf (src,"        BDecEoc (b, &totalElmtsLen%d, env);\n", totalLevel);
        fprintf (src,"    }\n");
        fprintf (src,"    else if (elmtLen%d != 0)\n", elmtLevel);
        fprintf (src,"    {\n");
        fprintf (src,"         Asn1Error (\"Expected an empty SET\\n\");\n");
        fprintf (src,"         longjmp (env, %d);\n",(*longJmpValG)--);

        fprintf (src,"    }\n");

/* forget about possible extension types for now
        fprintf (src,"    if (elmtLen%d == INDEFINITE_LEN)\n", elmtLevel);
        fprintf (src,"    {\n");
        fprintf (src,"        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", ++tagLevel, totalLevel);

        fprintf (src,"        if (tagId%d == EOC_TAG_ID)\n", tagLevel);
        fprintf (src,"            BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
        fprintf (src,"        else\n");
        fprintf (src,"            BerDiscardElmt (b, &totalElmtsLen%d, env);\n\n",totalLevel);
        fprintf (src,"    }\n");
        fprintf (src,"    else\n");
        fprintf (src,"    {\n");
        fprintf (src,"        BufSkip (b, elmtLen%d);\n", elmtLevel);
        fprintf (src,"        totalElmtsLen%d += elmtLen%d;\n", totalLevel, elmtLevel);
        fprintf (src,"    }\n");
*/
        return;
    }


    fprintf (src, "for ( ; (totalElmtsLen%d < elmtLen%d) || (elmtLen%d == INDEFINITE_LEN);)\n", totalLevel, elmtLevel, elmtLevel);
    fprintf (src, "{\n");
    fprintf (src, "    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", ++tagLevel, totalLevel);
    fprintf (src, "    if ((tagId%d == EOC_TAG_ID) && (elmtLen%d == INDEFINITE_LEN))\n", tagLevel, elmtLevel);
    fprintf (src, "    {\n");
    fprintf (src, "        BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
    fprintf (src, "        break; /* got EOC so can exit this SET's for loop*/\n");
    fprintf (src, "    }\n");

    fprintf (src, "    elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);

    fprintf (src, "    switch (tagId%d)\n", tagLevel);
    fprintf (src, "    {\n");

    FOR_EACH_LIST_ELMT (e, elmts)
    {

        elmtLevel = initialElmtLevel+1;
        tagLevel = initialTagLevel+1;
        if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
        {
            fprintf (src, "< ERROR - no c type information - prob unsuported type>\n");
            continue;
        }

        ctri = e->type->cTypeRefInfo;

        /* check if meant to be encoded */
        if (!ctri->isEncDec)
            continue;

        tags  = GetTags (e->type, &stoleChoiceTags);
        builtinType = GetBuiltinType (e->type);

        if ((tags == NULL) || LIST_EMPTY (tags))
        {
            if ((builtinType != BASICTYPE_ANY) &&
                (builtinType != BASICTYPE_ANYDEFINEDBY))
                fprintf (src, "<What? no tag on a SetElmt?>\n");
            else
            {
                fprintf (src,"       /* ANY - Fix Me ! */\n");
                fprintf (src,"       case MAKE_TAG_ID (?,?,?):\n");
            }
        }
        else
        {
            tag = (Tag*)FIRST_LIST_ELMT (tags);
            classStr = Class2ClassStr (tag->tclass);
            codeStr = Code2UnivCodeStr (tag->code);
            formStr = Form2FormStr (tag->form);

            if (tag->tclass == UNIV)
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (CONS), codeStr);
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (PRIM), codeStr);
                }
                else
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
            }
            else
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (CONS), tag->code);

                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (PRIM), tag->code);
                }
                else
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, formStr, tag->code);
            }

            AsnListFirst (tags);
            AsnListNext (tags); /* set curr to 2nd tag */
            FOR_REST_LIST_ELMT (tag, tags)
            {

                codeStr = Code2UnivCodeStr (tag->code);
                classStr = Class2ClassStr (tag->tclass);
                formStr = Form2FormStr (tag->form);

                if (stoleChoiceTags)
                {
                    if (tag->tclass == UNIV)
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (CONS), tag->code);

                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (PRIM), tag->code);
                        }
                        fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
                    }
                    else
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (CONS), tag->code);

                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (PRIM), tag->code);
                        }
                        fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, formStr, tag->code);
                    }
                }
                else
                {
                    tagLevel = initialTagLevel+2;
                    if (tag->form == ANY_FORM)
                    {
                        fprintf (src,"    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n", tagLevel, totalLevel);
                        if (tag->tclass == UNIV)
                        {
                            fprintf (src,"if ((tagId%d != MAKE_TAG_ID (%s, %s, %s)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                            fprintf (src,"   (tagId%d != MAKE_TAG_ID (%s, %s, %s)))\n", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                        }
                        else
                        {
                            fprintf (src,"if ((tagId%d != MAKE_TAG_ID (%s, %s, %d)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                            fprintf (src,"   (tagId%d != MAKE_TAG_ID (%s, %s, %d)))\n", tagLevel, classStr, Form2FormStr (CONS), tag->code);
                        }

                    }
                    else
                    {
                        if (tag->tclass == UNIV)
                            fprintf (src,"if (BDecTag (b, &totalElmtsLen%d, env) != MAKE_TAG_ID (%s, %s, %s))\n", totalLevel, classStr, formStr, codeStr);
                        else
                            fprintf (src,"if (BDecTag (b, &totalElmtsLen%d, env) != MAKE_TAG_ID (%s, %s, %d))\n", totalLevel, classStr, formStr, tag->code);
                    }

                    fprintf (src,"    {\n");
                    fprintf (src,"         Asn1Error (\"Unexpected Tag\\n\");\n");
                    fprintf (src,"         longjmp (env, %d);\n", (*longJmpValG)--);
                    fprintf (src,"    }\n\n");
                    fprintf (src,"elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
                }
            }
        }

        MakeVarPtrRef (genDecCRulesG, td, parent, e->type, varName, tmpVarName);

        /*
         * allocate mem for decoding result
         */
        PrintElmtAllocCode (src, e->type, tmpVarName);

        PrintCBerElmtDecodeCode (src, td, parent, e->type, elmtLevel, totalLevel, tagLevel, varName, tmpVarName, stoleChoiceTags);

        /*
         * must check for another EOC for ANYs
         * Since the any decode routines
         * decode their own first tag/len pair
         */
        if ((builtinType == BASICTYPE_ANY) ||
            (builtinType == BASICTYPE_ANYDEFINEDBY))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);
        /*
         * must check for another EOC for tagged CHOICEs
         * since the choice decoder routines do not check
         * for an EOC on the choice's overall length -
         * they are only passed the tag/len of the choice's
         * component.
         */
        else if ((builtinType == BASICTYPE_CHOICE) && !(stoleChoiceTags) &&
                ((tags != NULL) && !LIST_EMPTY (tags)))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);

        else
            PrintEocDecoders (src, elmtLevel-1, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);

        if ((!e->type->optional) && (e->type->defaultVal == NULL))
        {
            mandatoryCount++;
            fprintf (src, "    mandatoryElmtCount%d++;\n", totalLevel);
        }

        FreeTags (tags);

        fprintf (src,"    break;\n\n");
    }  /* end for */

    fprintf (src, "    default:\n");
    fprintf (src, "        Asn1Error (\"B%sContent: ERROR - Unexpected tag in SET\\n\");\n", routineName);
    fprintf (src, "        longjmp (env, %d);\n",(*longJmpValG)--);
    fprintf (src, "        break;\n");

/*
    fprintf (src, "        Asn1Warning (\"B%sContent: Warning - unexpected tag in SET, discarding elmt\\n\");\n", routineName);
    fprintf (src, "        BerDiscardElmt (b, &totalElmtsLen%d, env);\n\n", totalLevel);
*/

    fprintf (src, "        } /* end switch */\n");
    fprintf (src, "    } /* end for */\n");

    fprintf (src, "    if (mandatoryElmtCount%d != %d)\n", totalLevel, mandatoryCount);

    fprintf (src, "    {\n");
    fprintf (src, "        Asn1Error (\"B%sContent: ERROR - non-optional elmt missing from SET\\n\");\n", routineName);
    fprintf (src, "        longjmp (env, %d);\n",(*longJmpValG)--);
    fprintf (src, "    }\n");

}  /*  PrintCBerSetDecodeCode */




/*
 * Prints code for decoding the elmts of a SEQUENCE
 */
static void
PrintCBerSeqDecodeCode PARAMS ((src, td, parent, elmts, elmtLevel, totalLevel, tagLevel, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *elmts _AND_
    int elmtLevel _AND_
    int totalLevel _AND_
    int tagLevel _AND_
    char *varName)
{
    CTRI *ctri;
    CTDI *ctdi;
    NamedType *e;
    NamedType *tmpElmt;
    NamedType *last;
    TagList *tags;
    Tag *tag;
    Tag *lastTag;
    enum BasicTypeChoiceId builtinType;
    enum BasicTypeChoiceId tmpTypeId;
    char *classStr;
    BER_FORM form;
    char *formStr;
    char *codeStr;
    int   i;
    char  tmpVarName[MAX_VAR_REF];
    int   stoleChoiceTags;
    char *routineName;
    int   inTailOptElmts = FALSE;
    int   initialElmtLevel;
    int   initialTagLevel;


    initialTagLevel = tagLevel;
    initialElmtLevel = elmtLevel;


    routineName = td->cTypeDefInfo->decodeRoutineName;

    if ((elmts == NULL) || LIST_EMPTY (elmts)) /* empty seq */
    {
        fprintf (src,"    if (elmtLen%d == INDEFINITE_LEN)\n", elmtLevel);
        fprintf (src,"    {\n");
        fprintf (src,"        BDecEoc (b, &totalElmtsLen%d, env);\n", totalLevel);
        fprintf (src,"    }\n");
        fprintf (src,"    else if (elmtLen%d != 0)\n", elmtLevel);
        fprintf (src,"    {\n");
        fprintf (src,"         Asn1Error (\"Expected an empty SEQUENCE\\n\");\n");
        fprintf (src,"         longjmp (env, %d);\n",(*longJmpValG)--);

        fprintf (src,"    }\n");

/*
        forget about extended types for now
        fprintf (src,"        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", tagLevel+1, totalLevel);
        fprintf (src,"    {\n");
        fprintf (src,"        if (tagId%d == EOC_TAG_ID)\n", tagLevel+1);
        fprintf (src,"            BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
        fprintf (src,"        else\n");
        fprintf (src,"            BerDiscardElmt (b, &totalElmtsLen%d, env);\n\n",totalLevel);
        fprintf (src,"    }\n");
        fprintf (src,"    else \n");
        fprintf (src,"    {\n");
        fprintf (src,"        BufSkip (b, elmtLen%d);\n", elmtLevel);
        fprintf (src,"        totalElmtsLen%d += elmtLen%d\n", totalLevel, elmtLevel);
        fprintf (src,"    }\n");
*/
        return;
    }

    /*
     * must set list curr since IsTailOptional checks from curr pt
     * onward
     */
    AsnListFirst (elmts);
    inTailOptElmts = IsTailOptional (elmts);
    e = (NamedType*)FIRST_LIST_ELMT (elmts);
    tmpTypeId = GetBuiltinType (e->type);

    /*
     * print code to decode the first tag
     */
    tagLevel++;
    if (!inTailOptElmts)
    {
        if (((tmpTypeId == BASICTYPE_ANY) ||
             (tmpTypeId == BASICTYPE_ANYDEFINEDBY)) &&
            (CountTags (e->type) == 0))
        {
            if ((e->type->optional) && (e != (NamedType*)LAST_LIST_ELMT (elmts)))
            {
                /* let this cause a compile error in the generated code */
                fprintf (src,"<untagged optional ANY - you must fix this>\n");
            }
        }
        else
            fprintf (src, "    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", tagLevel, totalLevel);
    }
    else
    {
        fprintf (src, "    if ((elmtLen%d != INDEFINITE_LEN) && (totalElmtsLen%d == elmtLen%d))\n", elmtLevel, totalLevel, elmtLevel);
        fprintf (src, "        seqDone = TRUE;\n");
        fprintf (src, "    else\n");
        fprintf (src, "    {\n");

        if (((tmpTypeId == BASICTYPE_ANY) ||
             (tmpTypeId == BASICTYPE_ANYDEFINEDBY)) &&
            (CountTags (e->type) == 0))
        {
            if ((e->type->optional) && (e != (NamedType*)LAST_LIST_ELMT (elmts)))
            {
                /* let this cause a compile error in the generated code */
                fprintf (src,"<untagged optional ANY - you must fix this>\n");
            }
        }
        else
            fprintf (src, "        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", tagLevel, totalLevel);
        fprintf (src,"         if ((elmtLen%d == INDEFINITE_LEN) && (tagId%d == EOC_TAG_ID))\n", elmtLevel, tagLevel);
        fprintf (src, "        {\n");
        fprintf (src, "            BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
        fprintf (src, "            seqDone = TRUE;\n");
        fprintf (src, "        }\n");
        fprintf (src, "    }\n\n");
    }

    last = (NamedType*)LAST_LIST_ELMT (elmts);
    FOR_EACH_LIST_ELMT (e, elmts)
    {
        elmtLevel = initialElmtLevel;
        tagLevel = initialTagLevel+1;

        if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
        {
            fprintf (src, "< ERROR - no c type information - prob unsuported type>\n");
            continue;
        }

        ctri = e->type->cTypeRefInfo;

        /* check if meant to be encoded */
        if (!ctri->isEncDec)
            continue;

        tags  = GetTags (e->type, &stoleChoiceTags);
        builtinType = GetBuiltinType (e->type);


        if ((tags == NULL) || LIST_EMPTY (tags))
        {
            if ((builtinType != BASICTYPE_ANY) &&
                (builtinType != BASICTYPE_ANYDEFINEDBY))
                fprintf (src, "<What? no tag on a SetElmt?>\n");

            if (inTailOptElmts)
            {
                fprintf (src,"    if (!seqDone)");
            }
            /* always enclose elmt decoder in block */
            fprintf (src,"    {\n");

/*
             else
            {
                fprintf (src,"    if (tagId%d == MAKE_TAG_ID (?, ?, ?))\n", tagLevel);
                fprintf (src,"    {\n");
            }
*/
        }
        else  /* has tags */
        {
            tag = (Tag*)FIRST_LIST_ELMT (tags);

            classStr = Class2ClassStr (tag->tclass);
            codeStr = Code2UnivCodeStr (tag->code);
            formStr = Form2FormStr (tag->form);



            if (inTailOptElmts)
                fprintf (src,"    if ((!seqDone) && (");
            else
                fprintf (src,"    if ((");

            if (tag->tclass == UNIV)
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %s)) ||\n", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                }
                else
                    fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, formStr, codeStr);
            }
            else
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %d)) ||\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, Form2FormStr (CONS), tag->code);
                }
                else
                    fprintf (src,"(tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, formStr, tag->code);
            }

            if (!stoleChoiceTags)
            {
                fprintf (src,"))\n");
                fprintf (src, "    {\n");
                fprintf (src,"    elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
            }

            AsnListFirst (tags);
            AsnListNext (tags);

            FOR_REST_LIST_ELMT (tag, tags)
            {
                classStr = Class2ClassStr (tag->tclass);
                codeStr = Code2UnivCodeStr (tag->code);
                formStr = Form2FormStr (tag->form);


                if (stoleChoiceTags)
                {
                    fprintf (src," ||\n");
                    if (tag->tclass == UNIV)
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"     (tagId%d ==MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                            fprintf (src,"||\n    (tagId%d == MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                        }
                        else
                            fprintf (src,"     (tagId%d ==MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, formStr, codeStr);
                    }
                    else
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"    (tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                            fprintf (src,"||\n    (tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, Form2FormStr (CONS), tag->code);
                        }
                        else
                            fprintf (src,"    (tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, formStr, tag->code);
                    }
                }
                else
                {

                    tagLevel = initialTagLevel + 2;
                    fprintf (src, "        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", tagLevel, totalLevel);
                    if (tag->tclass == UNIV)
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"    if ((tagId%d != MAKE_TAG_ID (%s, %s, %s)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                            fprintf (src,"       (tagId%d != MAKE_TAG_ID (%s, %s, %s)))\n", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                        }
                        else
                            fprintf (src,"    if (tagId%d != MAKE_TAG_ID (%s, %s, %s))\n", tagLevel, classStr, formStr, codeStr);
                    }
                    else
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"    if ((tagId%d != MAKE_TAG_ID (%s, %s, %d)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                            fprintf (src,"        (tagId%d != MAKE_TAG_ID (%s, %s, %d)))\n", tagLevel, classStr, Form2FormStr (CONS), tag->code);
                        }
                        else
                            fprintf (src,"    if (tagId%d != MAKE_TAG_ID (%s, %s, %d))\n", tagLevel, classStr, formStr, tag->code);
                    }


                    fprintf (src,"    {\n");
                    fprintf (src,"         Asn1Error (\"Unexpected Tag\\n\");\n");
                    fprintf (src,"         longjmp (env, %d);\n",(*longJmpValG)--);
                    fprintf (src,"    }\n\n");
                    fprintf (src,"    elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
                }
            } /* end tag list for */

            if (stoleChoiceTags)
            {
                fprintf (src,"))\n");
                fprintf (src, "    {\n");
                fprintf (src, "        elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
            }
        }


        MakeVarPtrRef (genDecCRulesG, td, parent, e->type, varName, tmpVarName);

        /*
         * allocate mem for decoding result
         */
        PrintElmtAllocCode (src, e->type, tmpVarName);

        PrintCBerElmtDecodeCode (src, td, parent, e->type, elmtLevel, totalLevel, tagLevel, varName, tmpVarName, stoleChoiceTags);

        /*
         * must check for another EOC for ANYs
         * Since the any decode routines
         * decode their own first tag/len pair
         */
        if ((builtinType == BASICTYPE_ANY) ||
            (builtinType == BASICTYPE_ANYDEFINEDBY))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);
        /*
         * must check for another EOC for tagged CHOICEs
         * since the choice decoder routines do not check
         * for an EOC on the choice's overall length -
         * they are only passed the tag/len of the choice's
         * component.
         */
        else if ((builtinType == BASICTYPE_CHOICE) && (!stoleChoiceTags) &&
                ((tags != NULL) && !LIST_EMPTY (tags)))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);

        else
            PrintEocDecoders (src, elmtLevel-1, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);


        /*  could check cons len vs decode len here */

        if (!inTailOptElmts)
        {
            /*
             * determine whether next elmt in Seq is start
             * of tailing optionals
             */
            AsnListNext (elmts);
            inTailOptElmts = IsTailOptional (elmts);
            AsnListPrev (elmts);
        }

        /*
         * print code for getting the next tag
         */
        tmpTypeId = GetBuiltinType (e->type);

        if  (e != last)
        {
            tmpElmt = (NamedType*)NEXT_LIST_ELMT (elmts);
            tmpTypeId = GetBuiltinType (tmpElmt->type);
            if (!inTailOptElmts)
            {
                if (((tmpTypeId == BASICTYPE_ANY) ||
                     (tmpTypeId == BASICTYPE_ANYDEFINEDBY)) &&
                    (CountTags (tmpElmt->type) == 0))
                {
                    if ((e->type->optional) ||
                        ((tmpElmt->type->optional) && (tmpElmt != last)))
                    {
                        /* let this cause a compile error in the gen'd code */
                        fprintf (src,"  <problems with untagged ANY that is optional or follows an optional sequence element - you must fix this>\n");
                    }
                    /* don't get a tag since ANY's decode their own */
                }
                else
                    fprintf (src, "    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n", initialTagLevel+1, totalLevel);
            }
            else
            {
                fprintf (src, "    if ((elmtLen%d != INDEFINITE_LEN) && (totalElmtsLen%d == elmtLen%d))\n", initialElmtLevel, totalLevel, initialElmtLevel);
                fprintf (src, "        seqDone = TRUE;\n");
                fprintf (src, "    else\n");
                fprintf (src, "    {\n");
                if (((tmpTypeId == BASICTYPE_ANY) ||
                     (tmpTypeId == BASICTYPE_ANYDEFINEDBY)) &&
                    (CountTags (tmpElmt->type) == 0))
                {
                    if ((e->type->optional) ||
                        ((tmpElmt->type->optional) && (tmpElmt != last)))
                    {
                        /* let this cause a compile error in the gen'd code */
                        fprintf (src,"  <problems with untagged ANY that is optional or follows an optional sequence element - you must fix this>\n");

                    }

                    /* peek ahead for first octet of eoc */
                    fprintf (src,"         tagId%d = BufPeekByte (b);\n", initialTagLevel+1);
                    fprintf (src,"         if ((elmtLen%d == INDEFINITE_LEN) && (tagId%d == EOC_TAG_ID))\n", initialElmtLevel, initialTagLevel+1);
                    fprintf (src, "        {\n");
                    fprintf (src, "            BDecEoc (b, &totalElmtsLen%d, env);\n", totalLevel);
                    fprintf (src, "            seqDone = TRUE;\n");
                    fprintf (src, "        }\n");
                }
                else
                {
                    fprintf (src, "        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", initialTagLevel+1, totalLevel);
                    fprintf (src,"         if ((elmtLen%d == INDEFINITE_LEN) && (tagId%d == EOC_TAG_ID))\n", initialElmtLevel, initialTagLevel+1);
                    fprintf (src, "        {\n");
                    fprintf (src, "            BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
                    fprintf (src, "            seqDone = TRUE;\n");
                    fprintf (src, "        }\n");
                }
                fprintf (src, "    }\n");
            }
        }
        else /* for last elmt only */
        {
            fprintf (src,"        seqDone = TRUE;\n");
            fprintf (src,"        if (elmtLen%d == INDEFINITE_LEN)\n", initialElmtLevel);
            fprintf (src,"            BDecEoc (b, &totalElmtsLen%d, env);\n", totalLevel);
            fprintf (src,"        else if (totalElmtsLen%d != elmtLen%d)\n", totalLevel, initialElmtLevel);
            fprintf (src,"            longjmp (env, %d);\n",(*longJmpValG)--);
        }

        /*
         * close (tag check/seqDone test) if block and
         * print else clause to handle missing non-optional elmt
         * errors
         */
        tmpTypeId = GetBuiltinType (e->type);
        if (((tmpTypeId == BASICTYPE_ANYDEFINEDBY) ||
             (tmpTypeId == BASICTYPE_ANY)) &&
            (CountTags (e->type) == 0))
        {
            /* close if stmt block */
            fprintf (src,"    }\n");
        }
        else if (!e->type->optional && (e->type->defaultVal == NULL))
        {

            fprintf (src, "    }\n"); /* end of tag check if */
            fprintf (src, "    else\n");
            fprintf (src, "        longjmp (env, %d);\n", (*longJmpValG)--);
        }
        else
        {
            fprintf (src, "    }\n"); /* end of tag check if */
        }

        fprintf (src,"\n\n");
        FreeTags (tags);
    }


    /*
     * print code to make sure that truly finished with sequence
     */

    fprintf (src,"    if (!seqDone)\n");
    fprintf (src, "        longjmp (env, %d);\n\n", (*longJmpValG)--);

}  /*  PrintCBerSeqDecodeCode */


/*
 * Generates code for internally defined lists
 * eg:
 * TypeX = SET { foo INTEGER, bar SEQUENCE OF INTEGER } -->
 * BerDecodeTypeX (b, len, v, bytesDecoded, env)
 * {
 *    ...
 *         listLen1 = BerDecodeLen (b, &totalElmtsLen, env);
 *         retVal->bar = NewList();
 *         for ( ; totalElmtsLen1 < listLen1 || listLen1== INDEFINITE_LEN;)
 *         {
 *              tagId1 = BerDecodeTag (b, &totalElmtsLen1, env);
 *               check for EOC
 *              elmtLen1 = BerDecodeLen (b, &totalElmtsLen1, env)
 *              tmpInt = Asn1Alloc (sizeof (int));
 *              BerDecodeInteger (b, elmtLen1, tmpInt, &totalElmtsLen1, env);
 *              AppendList (retVal->bar, tmpInt);
 *         }
 *         totalElmtsLen += totalElmtsLen1;
 *    ...
 * }
 */
static void
PrintCBerListDecoderCode PARAMS ((src, td, list, elmtLevel, totalLevel, tagLevel, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *list _AND_
    int elmtLevel _AND_
    int totalLevel _AND_
    int tagLevel _AND_
 char *varName)
{
    CTRI *ctri;
    TagList *tags;
    Tag  *tag;
    Tag  *lastTag;
    enum BasicTypeChoiceId builtinType;
    char *classStr;
    BER_FORM form;
    char *formStr;
    char *codeStr;
    int   mandatoryCount = 0;
    int   i;
    char tmpVarName[MAX_VAR_REF];
    int  stoleChoiceTags;
    char *routineName;
    int initialTagLevel;
    int initialElmtLevel;
    int taglessAny;

    initialTagLevel = tagLevel;
    initialElmtLevel = elmtLevel;


    routineName = td->cTypeDefInfo->decodeRoutineName;
    ctri = list->basicType->a.setOf->cTypeRefInfo;
    tags  = GetTags (list->basicType->a.setOf, &stoleChoiceTags);
    builtinType = GetBuiltinType (list->basicType->a.setOf);

    taglessAny = (((tags == NULL) || LIST_EMPTY (tags)) &&
                  ((builtinType == BASICTYPE_ANY) ||
                   (builtinType == BASICTYPE_ANYDEFINEDBY)));

    fprintf (src, "    for (totalElmtsLen%d = 0; (totalElmtsLen%d < elmtLen%d) || (elmtLen%d == INDEFINITE_LEN);)\n", totalLevel, totalLevel, elmtLevel, elmtLevel);
    fprintf (src, "    {\n");
    fprintf (src,"        %s **tmpVar;\n", ctri->cTypeName);

    if (taglessAny)
    {
        fprintf (src, "    tagId%d = BufPeekByte (b);\n\n", ++tagLevel);
        fprintf (src, "    if ((tagId%d == EOC_TAG_ID) && (elmtLen%d == INDEFINITE_LEN))\n", tagLevel, elmtLevel);
        fprintf (src, "    {\n");
        fprintf (src, "        BDecEoc (b, &totalElmtsLen%d, env);\n", totalLevel);
        fprintf (src, "        break; /* got EOC so can exit this SET OF/SEQ OF's for loop*/\n");
        fprintf (src, "    }\n");
    }
    else
    {
        fprintf (src, "    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", ++tagLevel, totalLevel);
        fprintf (src, "    if ((tagId%d == EOC_TAG_ID) && (elmtLen%d == INDEFINITE_LEN))\n", tagLevel, elmtLevel);
        fprintf (src, "    {\n");
        fprintf (src, "        BDEC_2ND_EOC_OCTET (b, &totalElmtsLen%d, env)\n", totalLevel);
        fprintf (src, "        break; /* got EOC so can exit this SET OF/SEQ OF's for loop*/\n");
        fprintf (src, "    }\n");
    }


    if ((tags == NULL) || LIST_EMPTY (tags))
    {
        if (!taglessAny)
            fprintf (src, "<What? no tag on a SET OF/SEQ OF Elmt?>\n");
/*
        else
        {
            fprintf (src,"    if (tagId%d == MAKE_TAG_ID (?, ?, ?))",tagLevel);
            fprintf (src,"    {\n");
        }
*/

    }
    else if (!stoleChoiceTags) /* choice decoder will check tag */
    {
        tag = (Tag*)FIRST_LIST_ELMT (tags);
        classStr = Class2ClassStr (tag->tclass);
        codeStr = Code2UnivCodeStr (tag->code);
        formStr = Form2FormStr (tag->form);

        if (tag->tclass == UNIV)
        {
            if (tag->form == ANY_FORM)
            {
                fprintf (src,"    if ((tagId%d == MAKE_TAG_ID (%s, %s, %s)) ||", tagLevel, classStr, Form2FormStr (PRIM), codeStr);

                fprintf (src,"         (tagId%d == MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, Form2FormStr (CONS), codeStr);
            }
            else
                fprintf (src,"    if ((tagId%d == MAKE_TAG_ID (%s, %s, %s))", tagLevel, classStr, formStr, codeStr);
        }
        else
        {
            if (tag->form == ANY_FORM)
            {
                fprintf (src,"    if ((tagId%d == MAKE_TAG_ID (%s, %s, %d)) ||\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                fprintf (src,"       (tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, Form2FormStr (CONS), tag->code);
            }
            else
                fprintf (src,"    if ((tagId%d == MAKE_TAG_ID (%s, %s, %d))", tagLevel, classStr, formStr, tag->code);
        }

        fprintf (src,")\n");
        fprintf (src, "    {\n");
        fprintf (src, "        elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);

        AsnListFirst (tags);
        AsnListNext (tags);
        FOR_REST_LIST_ELMT (tag, tags)
        {
            tagLevel = initialTagLevel+2;
            fprintf (src, "        tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n\n", tagLevel, totalLevel);
            classStr = Class2ClassStr (tag->tclass);
            codeStr = Code2UnivCodeStr (tag->code);
            formStr = Form2FormStr (tag->form);

            if (tag->tclass == UNIV)
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"    if ((tagId%d != MAKE_TAG_ID (%s, %s, %s)) ||\n", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                    fprintf (src,"        (tagId%d != MAKE_TAG_ID (%s, %s, %s)))\n", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                }
                else
                    fprintf (src,"    if (tagId%d != MAKE_TAG_ID (%s, %s, %s))\n", tagLevel, classStr, formStr, codeStr);
            }
            else
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"    if ((tagId%d != MAKE_TAG_ID (%s, %s, %d)) ||\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                    fprintf (src,"        (tagId%d != MAKE_TAG_ID (%s, %s, %d)))\n", tagLevel, classStr, Form2FormStr (CONS), tag->code);

                }
                else
                    fprintf (src,"    if (tagId%d != MAKE_TAG_ID (%s, %s, %d))\n", tagLevel, classStr, formStr, tag->code);
            }


            fprintf (src,"    {\n");
            fprintf (src,"         Asn1Error (\"Unexpected Tag\\n\");\n");
            fprintf (src,"         longjmp (env, %d);\n", (*longJmpValG)--);
            fprintf (src,"    }\n\n");
            fprintf (src,"    elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
        }
    }
    if (stoleChoiceTags)
    {
        fprintf (src, "        elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
    }



    strcpy (tmpVarName, "(*tmpVar)");
    fprintf (src,"    tmpVar = (%s**) AsnListAppend (%s);\n", ctri->cTypeName, varName);
    fprintf (src, "    %s = (%s*) Asn1Alloc (sizeof (%s));\n", tmpVarName, ctri->cTypeName, ctri->cTypeName);

    fprintf (src,"    CheckAsn1Alloc (%s, env);\n", tmpVarName);
    PrintCBerElmtDecodeCode (src, td, list, list->basicType->a.setOf, elmtLevel, totalLevel, tagLevel, varName, tmpVarName, stoleChoiceTags);

    /*
     * must check for another EOC for ANYs
     * Since the any decode routines
     * decode their own first tag/len pair
     */
    if ((builtinType == BASICTYPE_ANY) ||
        (builtinType == BASICTYPE_ANYDEFINEDBY))
        PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);
        /*
         * must check for another EOC for tagged CHOICEs
         * since the choice decoder routines do not check
         * for an EOC on the choice's overall length -
         * they are only passed the tag/len of the choice's
         * component.
         */
    else if ((builtinType == BASICTYPE_CHOICE) && (!stoleChoiceTags) &&
             ((tags != NULL) && !LIST_EMPTY (tags)))
        PrintEocDecoders (src, elmtLevel, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);

    else
        PrintEocDecoders (src, elmtLevel-1, initialElmtLevel, itemLenVarNameG, totalLevel, decodedLenVarNameG);


    if ((!stoleChoiceTags) && (!taglessAny))
    {
        fprintf (src, "    }  /* end of tag check if */\n");
        fprintf (src, "    else  /* wrong tag */\n");
        fprintf (src,"    {\n");
        fprintf (src,"         Asn1Error (\"Unexpected Tag\\n\");\n");
        fprintf (src,"         longjmp (env, %d);\n", (*longJmpValG)--);
        fprintf (src,"    }\n");
    }
    fprintf (src, "    } /* end of for */\n\n");

    FreeTags (tags);

}  /*  PrintCBerListDecodeCode */



/*
 * t is the choice type pointer
 */
static void
PrintCBerChoiceDecodeCode PARAMS ((src, td, t, elmtLevel, totalLevel, tagLevel, varName),
    FILE *src _AND_
    TypeDef *td _AND_
    Type *t _AND_
    int elmtLevel _AND_
    int totalLevel _AND_
    int tagLevel _AND_
    char *varName)
{
    NamedType *e;
    CTRI *ctri;
    TagList *tags;
    Tag *tag;
    Tag *lastTag;
    enum BasicTypeChoiceId builtinType;
    char *classStr;
    BER_FORM form;
    char *formStr;
    char *codeStr;
    int   mandatoryCount = 0;
    int   i;
    char  tmpVarName[MAX_VAR_REF];
    char  choiceIdVarName[MAX_VAR_REF];
    CTRI *parentCtri;
    int   stoleChoiceTags;
    void *tmp;
    int initialTagLevel;
    int initialElmtLevel;

    initialTagLevel = tagLevel;
    initialElmtLevel = elmtLevel;

    parentCtri = t->cTypeRefInfo;


    fprintf (src, "    switch (tagId%d)\n", tagLevel);
    fprintf (src, "    {\n");


    FOR_EACH_LIST_ELMT (e,  t->basicType->a.choice)
    {
        /* hack ! remember curr loc cause called routine hacks it */
        tmp = (void*)CURR_LIST_NODE (t->basicType->a.choice);

        tagLevel = initialTagLevel;
        elmtLevel = initialElmtLevel;

        if ((e->type == NULL) || (e->type->cTypeRefInfo == NULL))
        {
            fprintf (src, "< ERROR - no c type information - prob unsuported type>\n");
            continue;
        }

        ctri =  e->type->cTypeRefInfo;

        tags  = GetTags (e->type, &stoleChoiceTags);
        builtinType = GetBuiltinType (e->type);

        if ((tags == NULL) || LIST_EMPTY (tags))
        {
            if ((builtinType != BASICTYPE_ANY) &&
                (builtinType != BASICTYPE_ANYDEFINEDBY))
                fprintf (src, "<What? no tag on a CHOICE elmt?>\n");
            else
            {
                fprintf (src, "    /* You must hand code ANY type refs */\n");
                fprintf (src,"       case MAKE_TAG_ID (?, ?, ?):\n");

            }

        }
        else
        {
            tag = (Tag*)FIRST_LIST_ELMT (tags);
            classStr = Class2ClassStr (tag->tclass);
            codeStr = Code2UnivCodeStr (tag->code);
            formStr = Form2FormStr (tag->form);

            if (tag->tclass == UNIV)
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (PRIM), codeStr);
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (CONS), codeStr);
                }
                else
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
            }
            else
            {
                if (tag->form == ANY_FORM)
                {
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (PRIM), tag->code);
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (CONS), tag->code);
                }
                else
                    fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, formStr, tag->code);
            }


	    AsnListFirst (tags);
	    AsnListNext (tags); /* set curr ptr to 2nd elmt */
            FOR_REST_LIST_ELMT (tag, tags)
            {
                classStr = Class2ClassStr (tag->tclass);
                codeStr = Code2UnivCodeStr (tag->code);
                formStr = Form2FormStr (tag->form);


                if (stoleChoiceTags)
                {
                    if (tag->tclass == UNIV)
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (PRIM), codeStr);
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, Form2FormStr (CONS), codeStr);
                        }
                        else
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %s):\n", classStr, formStr, codeStr);
                    }
                    else
                    {
                        if (tag->form == ANY_FORM)
                        {
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (PRIM), tag->code);
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, Form2FormStr (CONS), tag->code);
                        }
                        else
                            fprintf (src,"       case MAKE_TAG_ID (%s, %s, %d):\n", classStr, formStr, tag->code);
                    }
                }
                else
                {
                    tagLevel = initialTagLevel +1;
                    if (tag->form == ANY_FORM)
                    {
                        fprintf (src,"    tagId%d = BDecTag (b, &totalElmtsLen%d, env);\n", tagLevel, totalLevel);
                        if (tag->tclass == UNIV)
                        {
                            fprintf (src,"if ((tagId%d != MAKE_TAG_ID (%s, %s, %s)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), codeStr);
                            fprintf (src,"   (tagId%d != MAKE_TAG_ID (%s, %s, %s)))\n", tagLevel, classStr, Form2FormStr (CONS), codeStr);
                        }
                        else
                        {
                            fprintf (src,"if ((tagId%d != MAKE_TAG_ID (%s, %s, %d)) &&\n", tagLevel, classStr, Form2FormStr (PRIM), tag->code);
                            fprintf (src,"   (tagId%d != MAKE_TAG_ID (%s, %s, %d)))\n", tagLevel, classStr, Form2FormStr (CONS), tag->code);
                        }

                    }
                    else
                    {
                        if (tag->tclass == UNIV)
                            fprintf (src,"if (BDecTag (b, &totalElmtsLen%d, env) != MAKE_TAG_ID (%s, %s, %s))\n", totalLevel, classStr, formStr, codeStr);
                        else
                            fprintf (src,"if (BDecTag (b, &totalElmtsLen%d, env) != MAKE_TAG_ID (%s, %s, %d))\n", totalLevel, classStr, formStr, tag->code);
                    }

                    fprintf (src,"    {\n");
                    fprintf (src,"         Asn1Error (\"Unexpected Tag\\n\");\n");
                    fprintf (src,"         longjmp (env, %d);\n", (*longJmpValG)--);
                    fprintf (src,"    }\n\n");
                    fprintf (src,"    elmtLen%d = BDecLen (b, &totalElmtsLen%d, env);\n", ++elmtLevel, totalLevel);
                }
            }
        }


        MakeChoiceIdValueRef (genDecCRulesG, td, t, e->type, varName, choiceIdVarName);
        fprintf (src, "        %s = %s;\n", choiceIdVarName, ctri->choiceIdSymbol);

        MakeVarPtrRef (genDecCRulesG, td, t, e->type, varName, tmpVarName);

        PrintElmtAllocCode (src, e->type, tmpVarName);

        PrintCBerElmtDecodeCode (src, td, t, e->type, elmtLevel, totalLevel, tagLevel, varName, tmpVarName, stoleChoiceTags);

        /*
         * this is slightly diff from set/seq since
         * no loop checking for eoc (set) and no next elmt (seq)
         * so should check elmtLen0 for EOC if nec
         * (therefore (initialElmtLevel-1) instead of initialElmtLevel)
         *
         * must check for another EOC for ANYs
         * Since the any decode routines
         * decode their own first tag/len pair
         */
        if ((builtinType == BASICTYPE_ANY) ||
            (builtinType == BASICTYPE_ANYDEFINEDBY))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel-1, itemLenVarNameG, totalLevel, decodedLenVarNameG);
        /*
         * must check for another EOC for tagged CHOICEs
         * since the choice decoder routines do not check
         * for an EOC on the choice's overall length -
         * they are only passed the tag/len of the choice's
         * component.
         */
        else if ((builtinType == BASICTYPE_CHOICE) && (!stoleChoiceTags) &&
                ((tags != NULL) && !LIST_EMPTY (tags)))
            PrintEocDecoders (src, elmtLevel, initialElmtLevel-1, itemLenVarNameG, totalLevel, decodedLenVarNameG);

        else
            PrintEocDecoders (src, elmtLevel-1, initialElmtLevel-1, itemLenVarNameG, totalLevel, decodedLenVarNameG);


        FreeTags (tags);

        fprintf (src,"    break;\n\n");

        /* reset curr list node to value remember at beg of loop */
        SET_CURR_LIST_NODE (t->basicType->a.choice, tmp);
    }  /* end for */

    fprintf (src,"    default:\n");
    fprintf (src,"        Asn1Error (\"ERROR - unexpected tag in CHOICE\\n\");\n");
    fprintf (src,"        longjmp (env, %d);\n",(*longJmpValG)--);
    fprintf (src,"        break;\n");

    fprintf (src, "    } /* end switch */\n");

}  /* PrintCBerChoiceDecodeCode */



static void
PrintCLenDecodingCode PARAMS ((f),
    FILE *f)
{
    fprintf (f, "    itemLen += BDecDefLen (b, itemLen);");
}  /* PrintCLenDecodingCode */
