/*
 * compiler/core/normalize.c
 *
 * 1. swap COMPONENTS OF for actual types
 *     - do this since save lots of special case handling in
 *       code generation
 *
 * 2. change SEQUENCE OF/SET OF (type def (not ref))
 *    to     SEQUENCE OF/SEQ OF (type ref)
 *           and add type def for orig.
 *     - do this since OF type are AsnList
 *
 * 3. change CHOICE defs within other constructed types
 *      into CHOICE refs
 *        - makes code production easier. can be changed
 *          with some work
 *
 * 4. change SEQUENCE/SET defs within other constructed types
 *      into SEQUENCE/SET refs
 *        - makes code production easier. can be changed
 *          with some work (allocation in decode is wrong
 *                          - isPtr set incorrectly)
 *
 * 5. change SELECTION types to the actual field from the choice
 *
 * 6. convert Linked oid's with value refs into a ENC_OID's
 *    so values can be easily defined in C/C++. MS 92/03/01
 *
 * 7. if IMPLICIT-TAGS is specified, mark type references
 *    as implicit, if the ref'd type is not CHOICE or ANY.
 *    (Extra tags on primitives (ie not references) are already
 *     removed in the parsing step (asn1.yacc)).
 *
 * 8.  SET OF/SEQ OF defs nested in other SETs/SEQ/CHOICEs/SET OF/SEQ OF
 *     types are moved to separate type defs - added 08/92 to support
 *     C++ lists more easily.
 *
 * 9.  INTEGERs with named elmts and ENUM defs nested in other
 * SETs/SEQ/CHOICEs/SET OF/SEQ OF types are moved to separate type
 * defs - added 08/92 to support C++ class hierarchy better.
 *
 * ******** 10 is no longer done - in fact it was stupid for ******
 * ******** ANY DEFINED BY types                MS 09/92     ******
 * 10.  Move ANY and ANY DEFINED BY type defs nested in SET/SEQ/CHOICE/SET OF
 * /SEQ OF to a separate definition - this should make fixing the
 * produced code simpler.
 *
 * Mike Sample
 * 91/12/12
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/core/normalize.c,v 1.1 2001/06/20 21:27:58 dmitch Exp $
 * $Log: normalize.c,v $
 * Revision 1.1  2001/06/20 21:27:58  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:50  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:40  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:40:56  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:23  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "oid.h"
#include "lib-types.h"
#include "snacc-util.h"
#include "normalize.h"

#define LIST_ELMT_SUFFIX "ListElmt"
#define CHOICE_SUFFIX "Choice"
#define SET_SUFFIX "Set"
#define SEQ_SUFFIX "Seq"
#define SETOF_SUFFIX "SetOf"
#define SEQOF_SUFFIX "SeqOf"
#define INT_SUFFIX "Int"
#define ENUM_SUFFIX "Enum"
#define BITS_SUFFIX "Bits"
#define ANY_SUFFIX "Any"

long int oidRecursionCountG = 0;

void NormalizeTypeDef PROTO ((Module *m, TypeDef *td));

void NormalizeType PROTO ((Module *m, TypeDef *td, Type *parent, NamedTypeList *e, Type *t));

void NormalizeElmtTypes PROTO ((Module *m, TypeDef *td, Type *parent, NamedTypeList *e));

void NormalizeBasicType PROTO ((Module  *m, TypeDef *td, Type *parent, NamedTypeList *e, Type *type, BasicType *bt));

TypeDef *AddListElmtTypeDef PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt));

TypeDef *AddConsTypeDef PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, char *suffix));

void NormalizeValueDef PROTO ((Module *m, ValueDef *vd));

int  FlattenLinkedOid PROTO ((OID *o, char *asn1FileName, AsnInt lineNo, int quiet));

/*
 * looks through the given module and performs the operations
 * mentioned above
 */
void
NormalizeModule PARAMS ((m),
    Module *m)
{
    TypeDef *td;
    ValueDef *vd;

    /*
     * go through each type in typeList
     */
    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        NormalizeTypeDef (m, td);
    }

    /*
     *  go through each value for types?
     */
    FOR_EACH_LIST_ELMT (vd, m->valueDefs)
    {
        NormalizeValueDef (m, vd);
    }

}   /* NormalizeModule */


void
NormalizeTypeDef PARAMS ((m, td),
    Module *m _AND_
    TypeDef *td)
{
    if (td == NULL)
        return;

    NormalizeType (m, td, NULL, NULL, td->type);

} /* NormalizeTypeDef */


void
NormalizeType PARAMS ((m, td, parent, e, t),
 Module *m _AND_
 TypeDef *td _AND_
 Type *parent _AND_
 NamedTypeList *e _AND_
 Type *t)
{
    enum BasicTypeChoiceId typeId;
    Type *tmpType;
    Tag *lastTag;

    if (t == NULL)
        return;

    NormalizeBasicType (m, td, parent, e,  t, t->basicType);

    /*
     * make type refs implicit if IMPLICIT-TAGS specified and
     * ref'd type is OK for implicit tagging.
     * Tag removal work is done in parsing (yacc).
     */
    if ((m->tagDefault == IMPLICIT_TAGS))
    {
        tmpType = ParanoidGetType (t);
        typeId = tmpType->basicType->choiceId;

        if ((t->tags != NULL) && (!LIST_EMPTY (t->tags)))
            lastTag = (Tag*)LAST_LIST_ELMT (t->tags);
        else
            lastTag = NULL;

        /*
         * only mark as implicit if
         *  1. This type has a tag in it's taglist
         *  2. This type is a reference to another type
         *  3. the referenced type is not an untagged CHOICE, ANY or
         *     ANY DEFINED BY (just need to check that it has
         *     tags since all other types have tags)
         */
        if (((lastTag != NULL) && !(lastTag->explicit)) &&
            ((t->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
             (t->basicType->choiceId == BASICTYPE_IMPORTTYPEREF)) &&
            (CountTags (t->basicType->a.localTypeRef->link->type) != 0))
        {
            t->implicit = TRUE;
        }
    }

} /* NormalizeType */



void
NormalizeElmtTypes PARAMS ((m, td, parent, e),
    Module *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *e)
{
    NamedType *nt;

    FOR_EACH_LIST_ELMT (nt, e)
    {
        NormalizeType (m, td, parent, e, nt->type);
    }
}  /* NormalizeElmtTypes */


/*
 * this is where most of the action happens
 * assumes that "e"'s curr ptr is namedtype that holds "type"
 */
void
NormalizeBasicType PARAMS ((m, td, parent, e, type, bt),
    Module  *m _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    NamedTypeList *e _AND_
    Type *type _AND_
    BasicType *bt)
{
    int i, numElmtsAdded;
    NamedType *newElmt;
    NamedType **newElmtHndl;
    NamedType *nt;
    NamedTypeList *elmts;
    NamedType *origNext;
    Type *compType;
    Type *parentType;
    TypeDef *newDef;
    BasicType *tmpBasicType;
    TagList *tags;
    Tag *tag;
    Tag **tagHndl;

    if (bt == NULL)
        return;

    switch (bt->choiceId)
    {

        case BASICTYPE_COMPONENTSOF:
            /*
             * copy elmts of COMPONENTS OF type into this type
             */
            if (parent == NULL)
            {
                PrintErrLoc (m->asn1SrcFileName, type->lineNo);
                fprintf (stderr,"ERROR - COMPONENTS OF must be a SET or SEQUENCE element\n");
                m->status = MOD_ERROR;
                return;
            }

        compType = ParanoidGetType (bt->a.componentsOf);
        parentType = ParanoidGetType (parent);

        /* COMPONENTS OF must be nested in a SET or SEQUENCE type */
        if ((parentType->basicType->choiceId != BASICTYPE_SET) &&
            (parentType->basicType->choiceId != BASICTYPE_SEQUENCE))
        {
            PrintErrLoc (m->asn1SrcFileName, type->lineNo);
            fprintf (stderr,"ERROR - COMPONENTS OF must be a SET or SEQUENCE element\n");
            m->status = MOD_ERROR;
            return;
        }

            /* COMPONENTS OF in a SET must ref a SET and vice versa for SEQ */
            if (((parentType->basicType->choiceId == BASICTYPE_SET) &&
                 (compType->basicType->choiceId != BASICTYPE_SET)) ||
                ((parentType->basicType->choiceId == BASICTYPE_SEQUENCE) &&
                (compType->basicType->choiceId != BASICTYPE_SEQUENCE)))
            {
                PrintErrLoc (m->asn1SrcFileName, type->lineNo);
                fprintf (stderr,"ERROR - COMPONENTS OF in a SET must reference a SET type and  COMPONENTS OF in SEQUENCE must reference a SEQUENCE type\n");
                type->basicType = compType->basicType;
                m->status = MOD_ERROR;
                return;
            }

            /*
             * replace "COMPONENTS OF" with elmts from ref'd set
             */
            elmts = compType->basicType->a.set;

            if (elmts == NULL)
                break;

            /*
             * add new list elmts that  point to elmts
             * of type ref'd by COMPONENTS OF
             */
            FOR_EACH_LIST_ELMT (nt, elmts)
            {
                newElmtHndl = (NamedType**)AsnListAdd (e);
                *newElmtHndl = nt;
            }

            /*
             * Set e list's curr  ptr to first of of the
             * newly added components.
             * Do this so NormalizeElmtTypes will do the
             * newly added ones as well
             */
            numElmtsAdded = AsnListCount (elmts);
            for (i = 0; i < numElmtsAdded; i++)
                AsnListPrev (e);

            /* remove the componets of ref since elmts copied in */
            AsnListRemove (e);

            break;


        case BASICTYPE_SELECTION:
            /*
             * first normalize the CHOICE that is selected from
             * - this will be done twice to the CHOICE but nothing
             * bad should happen.  The main reason for 'normalizing'
             * the CHOICE first is to strip tags from the choice elmts
             * if IMPLICIT-TAGS is set.
             * NOTE: this call assumes that import/local type refs
             * both use the 'TypeRef' struct and that a selection references
             * a CHOICE by name (not definition)
             */
            NormalizeType (m, type->basicType->a.selection->typeRef->basicType->a.localTypeRef->link, NULL, NULL, type->basicType->a.selection->typeRef->basicType->a.localTypeRef->link->type);

            /*
             * use SELECTION field name if this is an elmt type with no
             * field name.
             */
            if ((e != NULL) &&
                (((NamedType*) e->curr->data)->fieldName == NULL))
                ((NamedType*) e->curr->data)->fieldName =
                    type->basicType->a.selection->link->fieldName;

            /*
             * replace SELECTION type with refd type.
             * must append the named CHOICE field's tags to
             * any existing tags on this SELECTION type.
             */
            tmpBasicType =  type->basicType->a.selection->link->type->basicType;
            tags = type->basicType->a.selection->link->type->tags;


            FOR_EACH_LIST_ELMT (tag, tags)
            {
                if (!(((m->tagDefault == IMPLICIT_TAGS) || (type->implicit)) &&
                    (tag == (Tag*)FIRST_LIST_ELMT (tags))))
                {
                    tagHndl = (Tag**) AsnListAppend (type->tags);
                    *tagHndl = tag;
                }
                type->implicit = FALSE;
            }

            if (type->basicType->a.selection->link->type->implicit)
                type->implicit = TRUE;

            Free (type->basicType->a.selection->fieldName);
            Free (type->basicType->a.selection->typeRef->basicType);
            Free (type->basicType->a.selection->typeRef);
            type->basicType = tmpBasicType;

        break;



        case BASICTYPE_SEQUENCEOF:
        case BASICTYPE_SETOF:
            /* convert def inside other type into a ref */
            if (td->type != type)
            {
                if (bt->choiceId == BASICTYPE_SETOF)
                    newDef = AddConsTypeDef (m, td, type, bt, SETOF_SUFFIX);
                else
                    newDef = AddConsTypeDef (m, td, type, bt, SEQOF_SUFFIX);

                NormalizeType (m, newDef, NULL, NULL, newDef->type);
            }
            else
                NormalizeType (m, td, type, NULL, type->basicType->a.setOf);
            break;

            /*  NOT NEEDED ANY MORE
             * convert typdef after SET OF/SEQ OF to type REFS
            switch (bt->a.setOf->basicType->choiceId)
            {
                case BASICTYPE_SEQUENCE:
                case BASICTYPE_SET:
                case BASICTYPE_CHOICE:
                case BASICTYPE_SEQUENCEOF:
                case BASICTYPE_SETOF:
                case BASICTYPE_COMPONENTSOF:
                    newDef = AddListElmtTypeDef (m, td, type, bt);
                    NormalizeType (m, newDef, NULL, NULL, newDef->type);
                break;

                default:
                    NormalizeType (m, td, NULL, NULL, bt->a.setOf);
                break;
            }
            */
            break;


        case BASICTYPE_CHOICE:
            /*
             * change CHOICE defs embedded in other types
             * into type refs
             */
            if (td->type != type)
            {
                newDef = AddConsTypeDef (m, td, type, bt, CHOICE_SUFFIX);
                NormalizeType (m, newDef, NULL, NULL, newDef->type);
            }
            else
                NormalizeElmtTypes (m, td, type, bt->a.set);

            break;


        case BASICTYPE_SEQUENCE:
            /*
             * change SEQ defs embedded in other types
             * into type refs
             */
            if (td->type != type)
            {
                newDef = AddConsTypeDef (m, td, type, bt, SEQ_SUFFIX);
                NormalizeType (m, newDef, NULL, NULL, newDef->type);
            }
            else
                NormalizeElmtTypes (m, td, type, bt->a.sequence);
            break;


        case BASICTYPE_SET:
            /*
             * change SET defs embedded in other types
             * into type refs
             */
            if (td->type != type)
            {
                newDef = AddConsTypeDef (m, td, type, bt, SET_SUFFIX);
                NormalizeType (m, newDef, NULL, NULL, newDef->type);
            }
            else
                NormalizeElmtTypes (m, td, type, bt->a.set);
            break;


        case BASICTYPE_INTEGER:
            /* if they have named elements convert this def into a ref */
            if ((td->type != type) && (bt->a.integer !=  NULL) &&
                (!LIST_EMPTY (bt->a.integer)))
            {
                newDef = AddConsTypeDef (m, td, type, bt, INT_SUFFIX);
            }
            break;

        case BASICTYPE_ENUMERATED:
            /* if they have named elements convert this def into a ref */
            if ((td->type != type) && (bt->a.enumerated !=  NULL) &&
                (!LIST_EMPTY (bt->a.enumerated)))
            {
                newDef = AddConsTypeDef (m, td, type, bt, ENUM_SUFFIX);
            }
            break;

        case BASICTYPE_BITSTRING:
            /* if they have named elements convert this def into a ref */
            if ((td->type != type) && (bt->a.bitString !=  NULL) &&
                (!LIST_EMPTY (bt->a.bitString)))
            {
                newDef = AddConsTypeDef (m, td, type, bt, BITS_SUFFIX);
            }
            break;

        case BASICTYPE_ANY:
        case BASICTYPE_ANYDEFINEDBY:
            m->hasAnys = TRUE;
            /*  NO LONGER DONE
             * change ANY defs embedded in other types
             * into type refs

            if (td->type != type)
                newDef = AddConsTypeDef (m, td, type, bt, ANY_SUFFIX);
             */
            break;

        default:
          /* the rest are not processed */
        break;
    }
}  /* NormalizeBasicType */




/*
 * given a set of/seq of type t within typedef td, change the
 * set of /seq of elmt type def  into a type ref and
 * add a type def for the elmt at the top level.
 */
TypeDef*
AddListElmtTypeDef PARAMS ((m, td, t, bt),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt)
{
    TypeDef *newDef;
    TypeDef **typeDefHndl;
    int end;
    int digit;

    /*
     * make new  type def
     */
    newDef = (TypeDef*)Malloc (sizeof (TypeDef));
    newDef->exported = FALSE;
    newDef->type = bt->a.setOf;
    /*
     * make name for new type
     * Foo ::= SET OF SEQUENCE {...}
     *  -->
     * FooListElmt ::=  SEQUENCE {...}
     * Foo ::=  SET OF FooListElmt
     */
    newDef->definedName =
        Malloc (strlen (td->definedName) +
               strlen (LIST_ELMT_SUFFIX) + 4);

    strcpy (newDef->definedName, td->definedName);
    strcat (newDef->definedName, LIST_ELMT_SUFFIX);
    end = strlen (newDef->definedName);
    digit  = 1;
    while (LookupType (m->typeDefs, newDef->definedName) != NULL)
    {
        newDef->definedName[end] = '\0';
        AppendDigit (newDef->definedName, digit++);
    }

    /*
     * now put new type at head of list
     */
    typeDefHndl = (TypeDef**)AsnListPrepend (m->typeDefs);
    *typeDefHndl = newDef;



    /*
     * replace SET OF/SEQ OF body with type ref
     */
    bt->a.setOf = (Type*)Malloc (sizeof (Type));
    bt->a.setOf->optional = FALSE;
    bt->a.setOf->implicit = FALSE;
    bt->a.setOf->lineNo = t->lineNo;
    bt->a.setOf->basicType = (BasicType*)Malloc (sizeof (BasicType));
    bt->a.setOf->basicType->choiceId = BASICTYPE_LOCALTYPEREF;
    bt->a.setOf->basicType->a.localTypeRef = (TypeRef*)Malloc (sizeof (TypeRef));
    bt->a.setOf->basicType->a.localTypeRef->link = newDef;
    bt->a.setOf->basicType->a.localTypeRef->typeName =
        newDef->definedName;
    bt->a.setOf->basicType->a.localTypeRef->moduleName = NULL;


    return newDef;

} /* AddListElmtTypeDefs */



/*
 * given a CHOICE/SET/SEQ/etc type t within typedef td, make t into a ref
 * to a new  top level typdef of the CHOICE/SET/SEQ
 */
TypeDef*
AddConsTypeDef PARAMS ((m, td, t, bt, suffix),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    char *suffix)
{
    TypeDef *newDef;
    TypeDef **typeDefHndl;
    Tag **tmpPtr;
    Tag  *lastTag;
    int end;
    int digit;

    /*
     * make new  type def
     */
    newDef = (TypeDef*)Malloc (sizeof (TypeDef));
    newDef->exported = FALSE;
    newDef->recursive = FALSE;
    newDef->localRefCount = 1;
    newDef->type = (Type*)Malloc (sizeof (Type));
    newDef->type->optional = FALSE;
    newDef->type->lineNo = t->lineNo;
    newDef->type->basicType = bt;

    /*
     * make name for new choice/SET/SEQ
     * Foo ::= SEQUENCE { .., bar CHOICE { ...}, ..}
     *  -->
     * FooChoice ::=  CHOICE { ...}
     * Foo ::=  SEQUENCE { .., bar FooChoice, .. }
     */
    newDef->definedName =
        Malloc (strlen (td->definedName) +
               strlen (suffix) + 4);

    strcpy (newDef->definedName, td->definedName);
    strcat (newDef->definedName, suffix);
    end = strlen (newDef->definedName);
    digit  = 1;

    /* keep name unique */
    while (LookupType (m->typeDefs, newDef->definedName) != NULL)
    {
        newDef->definedName[end] = '\0';
        AppendDigit (newDef->definedName, digit++);
    }

    /*
     * now put new type at head of list
     */
    typeDefHndl = (TypeDef**)AsnListPrepend (m->typeDefs);
    *typeDefHndl = newDef;

    /*
     * what to do with tags? Use default universal type on
     * newly defined type and adjust (new) reference's tags
     * appropriately
     *
     *  NOTE: may be simpler just to move all the tags to the
     *        new def.
     */

    newDef->type->tags = (TagList*)AsnListNew (sizeof (void*));
    if (LIBTYPE_GET_UNIV_TAG_CODE ((newDef->type->basicType->choiceId))
        != NO_TAG_CODE)
    {
         tmpPtr = (Tag**)AsnListAppend (newDef->type->tags);
         *tmpPtr = (Tag*)Malloc (sizeof (Tag));
         (*tmpPtr)->tclass = UNIV;
         (*tmpPtr)->code = LIBTYPE_GET_UNIV_TAG_CODE ((newDef->type->basicType->choiceId));


    /* adjust tags of new ref to new def */
    if ((t->tags != NULL) && (!LIST_EMPTY (t->tags)))
    {
        lastTag = (Tag*)LAST_LIST_ELMT (t->tags);
        if ((lastTag->tclass == UNIV) &&
            (lastTag->code ==
             LIBTYPE_GET_UNIV_TAG_CODE ((newDef->type->basicType->choiceId))))
        {
            /* zap it since same as default universal tag */
            SET_CURR_LIST_NODE (t->tags, LAST_LIST_NODE (t->tags));
            AsnListRemove (t->tags);
            t->implicit = FALSE;
        }
        else
        {
            t->implicit = TRUE; /* this will probably already be true */
        }
    }

     }
    /*
     * replace embeded CHOICE/SET/SEQ def with ref to newly defined type
     */
    t->basicType = (BasicType*)Malloc (sizeof (BasicType));
    t->basicType->choiceId = BASICTYPE_LOCALTYPEREF;
    t->basicType->a.localTypeRef = (TypeRef*)Malloc (sizeof (TypeRef));
    t->basicType->a.localTypeRef->link = newDef;
    t->basicType->a.localTypeRef->typeName =
        newDef->definedName;
    t->basicType->a.localTypeRef->moduleName = NULL;


    return newDef;

} /* AddConsTypeDef */


void
NormalizeValueDef PARAMS ((m, vd),
    Module *m _AND_
    ValueDef *vd)
{
    NormalizeValue (m, vd, vd->value, FALSE);
}

void
NormalizeValue PARAMS ((m, vd, v, quiet),
    Module *m _AND_
    ValueDef *vd _AND_
    Value *v _AND_
    int quiet)
{
    AsnOid *eoid;
    OID *o;
    OID *tmp;
    int eLen;

    /*
     * convert linked oids into ENC_OID's
     */
    if (v->basicValue->choiceId == BASICVALUE_LINKEDOID)
    {
        if (!FlattenLinkedOid (v->basicValue->a.linkedOid, m->asn1SrcFileName, v->lineNo, quiet))
            return;
        eLen = EncodedOidLen (v->basicValue->a.linkedOid);
        eoid = MT (AsnOid);
        eoid->octetLen = eLen;
        eoid->octs = (char*)Malloc (eLen);
        BuildEncodedOid (v->basicValue->a.linkedOid, eoid);

        /* free linked  oid */
        for (o = v->basicValue->a.linkedOid; o != NULL; )
        {
            tmp = o->next;
            Free (o);
            o = tmp;
        }
        v->basicValue->choiceId = BASICVALUE_OID;
        v->basicValue->a.oid = eoid;
    }
}


/*
 * replaces value refs with the value's number if poss
 * returns TRUE if successfully done.
 * returns FALSE if a value ref could not be traced
 *  (false should not happen if the value link succeeded)
 * "quiet" parameter allows err msg to be turned off
 *  which prevents cascading errors by other oid's that
 *  reference a bad oid.
 */
int
FlattenLinkedOid PARAMS ((o, asn1FileName, lineNo, quiet),
    OID *o _AND_
    char *asn1FileName _AND_
    AsnInt lineNo _AND_
    int quiet)
{
    OID *firstElmt;
    OID *refdOid;
    OID *tmpOid;
    OID **nextOid;
    Value *val;
    Value *valRef;

    if (oidRecursionCountG > 100)
    {
        PrintErrLoc (asn1FileName, lineNo);
        fprintf (stderr,"ERROR - recursive OBJECT IDENTIFIER value.\n");
        return FALSE;
    }

    firstElmt = o;

    for (; o != NULL; o = o->next)
    {
        valRef = o->valueRef;
        if ((valRef == NULL) || (o->arcNum != NULL_OID_ARCNUM))
            continue; /* no linking nec for this one */

        val = GetValue (o->valueRef);

        /*
         * if the very first component is an oid val ref
         * then insert that value
         */
        if ((o == firstElmt) && (val->basicValue->choiceId == BASICVALUE_OID))
        {
            UnbuildEncodedOid (val->basicValue->a.oid, &refdOid);
            for (tmpOid = refdOid; tmpOid->next != NULL; tmpOid = tmpOid->next)
		;
            tmpOid->next = o->next;
            memcpy (firstElmt, refdOid, sizeof (OID));
            Free (refdOid); /* free first component of OID since copied */
        }

        else if ((o == firstElmt) && (val->basicValue->choiceId == BASICVALUE_LINKEDOID))
        {
            oidRecursionCountG++;
            if (!FlattenLinkedOid (val->basicValue->a.linkedOid, asn1FileName, lineNo, TRUE))
            {
                oidRecursionCountG--;
                return FALSE;
            }
            oidRecursionCountG--;

            nextOid = &refdOid;
            for (tmpOid = val->basicValue->a.linkedOid;
                tmpOid != NULL; tmpOid = tmpOid->next)
            {
                *nextOid = (OID*)Malloc (sizeof (OID));
                (*nextOid)->arcNum = tmpOid->arcNum;
                nextOid = &(*nextOid)->next;
            }
            (*nextOid) = o->next;
            memcpy (firstElmt, refdOid, sizeof (OID));
            Free (refdOid); /* since copied into firstElmt */
        }

        else if ((val->basicValue->choiceId == BASICVALUE_INTEGER))
        {
            o->arcNum = val->basicValue->a.integer;
            if ((o->arcNum < 0) && !quiet)
            {
                PrintErrLoc (asn1FileName, lineNo);
                fprintf (stderr,"ERROR - OBJECT IDENTIFIER arc values cannot be negative.\n");
            }
        }
        else /* bad arc value type */
        {
            if (!quiet)
            {
                PrintErrLoc (asn1FileName, lineNo);
                fprintf (stderr,"ERROR - type mismatch for an arc value. Values ref'd from an OBJECT IDENTIFIER value must be either an OBJECT IDENTIFIER (first oid elmt only) or INTEGER value (this may be reported twice!)\n");
            }
            return FALSE;
        }

        /* free mem assoc with value ref */
        Free (valRef->basicValue->a.localValueRef->valueName);
        Free (valRef->basicValue->a.localValueRef);
        Free (valRef->basicValue);
        Free (valRef);
        o->valueRef = NULL;
    }
    return TRUE;
} /* FlattenLinkedOid */
