/*
 * compiler/core/do_macros.c
 *
 * Runs through type and value def lists and does any processing nec.
 * for any macro encountered.
 *
 * Processing could consist of making stubs for OPERATION macro etc.
 * What is done is very environment dependent.
 *
 * You should change this file to match your environment.
 *
 * Any Type Defs hidden in a MACRO Type are popped into the normal
 * type def list and REFERENCED from the macro (instead of being
 * defined there)
 *
 * SNMP Objectype macro fills the ANY Ref lists so the id to ANY
 * type hash table is filled.
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
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/do-macros.c,v 1.1 2001/06/20 21:27:56 dmitch Exp $
 * $Log: do-macros.c,v $
 * Revision 1.1  2001/06/20 21:27:56  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:47  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:23  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:32:28  rj
 * snacc_config.h removed; do_macros.h includet.
 *
 * Revision 1.1  1994/08/28  09:49:03  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>
#include <ctype.h>  /* for islower/toupper */

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "define.h"
#include "str-util.h"
#include "normalize.h"
#include "snacc-util.h"
#include "do-macros.h"

void AddAnyRefByOid PROTO ((AnyRefList **arl, char *enumIdName, AsnOid *oid));

void AddAnyRefByInt PROTO ((AnyRefList **arl, char *enumIdName, AsnInt intId));


void ProcessMacrosInTypeDef PROTO ((Module *m, TypeDef *td));

void ProcessMacrosInValueDef PROTO ((Module *m, ValueDef *vd));

void ProcessMacrosInType PROTO ((Module *m, TypeDef *td, Type *t, ValueDef *v));

void ProcessMacrosInElmtTypes PROTO ((Module *m, TypeDef *td, NamedTypeList *e, ValueDef *v));

void ProcessMacrosInBasicType PROTO ((Module  *m, TypeDef *td, Type *type, BasicType *bt, ValueDef *v));

void DefineType PROTO ((Module *m, TypeDef *td, Type *t, char *name));

void ProcessRosOperationMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, RosOperationMacroType *op, ValueDef *v));

void ProcessRosErrorMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, RosErrorMacroType *err, ValueDef *v));

void ProcessRosBindMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, RosBindMacroType *bind, ValueDef *v));

void ProcessRosAseMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, RosAseMacroType *ase, ValueDef *v));

void ProcessRosAcMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, RosAcMacroType *ac, ValueDef *v));

void ProcessMtsasExtensionsMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasExtensionsMacroType *exts, ValueDef *v));

void ProcessMtsasExtensionMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasExtensionMacroType *ext, ValueDef *v));

void ProcessMtsasExtensionAttributeMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasExtensionAttributeMacroType *ext, ValueDef *v));

void ProcessMtsasTokenMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasTokenMacroType *tok, ValueDef *v));

void ProcessMtsasTokenDataMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasTokenDataMacroType *tok, ValueDef *v));

void ProcessMtsasSecurityCategoryMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, MtsasSecurityCategoryMacroType *sec, ValueDef *v));

void ProcessAsnObjectMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, AsnObjectMacroType *obj, ValueDef *v));

void ProcessAsnPortMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, AsnPortMacroType *p, ValueDef *v));

void ProcessAsnAbstractBindMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, AsnAbstractBindMacroType *bind, ValueDef *v));

void ProcessSnmpObjectTypeMacroType PROTO ((Module *m, TypeDef *td, Type *t, BasicType *bt, SnmpObjectTypeMacroType *bind, ValueDef *v));

/*
static TypeDef  *snmpObjectSyntaxesG = NULL;
*/

/*
 * Hunts for macros in TypeDefs or ValueDefs and
 * might do something with them.
 */
void
ProcessMacros PARAMS ((m),
    Module *m)
{
    TypeDef *td;
    TypeDef **tmpTypeDefHndl;
    ValueDef *vd;

    /*
     * go through each type in typeList
     */
    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        ProcessMacrosInTypeDef (m, td);
    }

    /*
     *  go through each value in valueList and link
     */
    FOR_EACH_LIST_ELMT (vd, m->valueDefs)
    {
        ProcessMacrosInValueDef (m, vd);
    }

    /* add snmp object syntaxes choice to typedef list */
/*
    tmpTypeDefHndl = (TypeDef**) AsnListAppend (m->typeDefs);
    *tmpTypeDefHndl =  snmpObjectSyntaxesG;
    snmpObjectSyntaxesG = NULL;
*/


}   /* ProcessMacros */

/*
 * Given an AnyRefList, char string for an enum Id,
 *  and an OBJECT IDENTIFIER,
 * this routine puts the id and oid into the AnyRefList.
 * When the code is generated, the AnyInit routine for
 * the module to which the typeDef that owns the given AnyRefList
 * belongs, calls a routine that will cause the given oid to
 * hash to the TypeDef that owns the AnyRefList.
 * The enumId value at runtime is used for simple determination of
 * the ANY type by the user.
 */
void
AddAnyRefByOid PARAMS ((arl, enumId, oid),
    AnyRefList **arl _AND_
    char *enumId _AND_
    AsnOid *oid)
{
    AnyRef **anyRefHndl;

    if (*arl == NULL)
        *arl = AsnListNew (sizeof (void*));

    anyRefHndl = (AnyRef**)AsnListAppend (*arl);
    *anyRefHndl = MT (AnyRef);

    (*anyRefHndl)->anyIdName = Malloc (strlen (enumId)+1);
    strcpy ((*anyRefHndl)->anyIdName, enumId);

    (*anyRefHndl)->id = MT (OidOrInt);
    (*anyRefHndl)->id->choiceId = OIDORINT_OID;
    (*anyRefHndl)->id->a.oid = MT (AsnOid);
    (*anyRefHndl)->id->a.oid->octs = Malloc (oid->octetLen);
    memcpy ((*anyRefHndl)->id->a.oid->octs, oid->octs, oid->octetLen);
    (*anyRefHndl)->id->a.oid->octetLen =  oid->octetLen;

} /* AddAnyRefByOid */

/*
 * Like AddAnyRefByOid except that an int maps to the type def
 * instead of an OBJECT IDENTIFIER
 */
void
AddAnyRefByInt PARAMS ((arl, enumId, intId),
    AnyRefList **arl _AND_
    char *enumId _AND_
    AsnInt intId)
{
    AnyRef **anyRefHndl;

    if (*arl == NULL)
        *arl = AsnListNew (sizeof (void*));

    anyRefHndl = (AnyRef**)AsnListAppend (*arl);
    *anyRefHndl = MT (AnyRef);

    (*anyRefHndl)->anyIdName = Malloc (strlen (enumId)+1);
    strcpy ((*anyRefHndl)->anyIdName, enumId);
    (*anyRefHndl)->id = MT (OidOrInt);
    (*anyRefHndl)->id->choiceId = OIDORINT_INTID;
    (*anyRefHndl)->id->a.intId = intId;

} /* AddAnyRefByInt */


void
ProcessMacrosInValueDef PARAMS ((m, vd),
    Module *m _AND_
    ValueDef *vd)
{
   if (vd == NULL)
       return;

   /* turn linked oid's into encoded oids */
    if (vd->value->basicValue->choiceId == BASICVALUE_LINKEDOID)
        NormalizeValue (m, vd, vd->value, FALSE);

   ProcessMacrosInType (m, NULL, vd->value->type, vd);

} /* ProcessMacrosInValueDef */



void
ProcessMacrosInTypeDef PARAMS ((m,td),
    Module *m _AND_
    TypeDef *td)
{
   if (td == NULL)
       return;

   ProcessMacrosInType (m, td, td->type, NULL);

} /* ProcessMacrosInTypeDef */



void
ProcessMacrosInType PARAMS ((m, td,t, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    ValueDef *v)
{
   if (t == NULL)
       return;

   ProcessMacrosInBasicType (m, td, t, t->basicType, v);

} /* ProcessMacrosInTypeDef */



void
ProcessMacrosInElmtTypes PARAMS ((m, td, e, v),
    Module *m _AND_
    TypeDef *td _AND_
    NamedTypeList *e _AND_
    ValueDef *v)
{
    NamedType *nt;
    FOR_EACH_LIST_ELMT (nt, e)
    {
        ProcessMacrosInType (m, td, nt->type, v);
    }
}  /* ProcessElmtTypes */



void
ProcessMacrosInBasicType PARAMS ((m,td,type,bt, v),
    Module  *m _AND_
    TypeDef *td _AND_
    Type *type _AND_
    BasicType *bt _AND_
    ValueDef *v)
{

    if (bt == NULL)
        return;

    switch (bt->choiceId)
    {

        case BASICTYPE_SEQUENCE:
        case BASICTYPE_SET:
        case BASICTYPE_CHOICE:
            ProcessMacrosInElmtTypes (m, td, bt->a.set,v);
            break;



        case BASICTYPE_SEQUENCEOF:
        case BASICTYPE_SETOF:
            ProcessMacrosInType (m, td, bt->a.setOf, v);
            break;



        case BASICTYPE_MACROTYPE:
            switch (bt->a.macroType->choiceId)
            {
        case MACROTYPE_ASNABSTRACTOPERATION:
        case MACROTYPE_ROSOPERATION:

            ProcessRosOperationMacroType (m, td, type, bt, bt->a.macroType->a.rosOperation, v);
            break;

        case MACROTYPE_ROSERROR:
        case MACROTYPE_ASNABSTRACTERROR:
            ProcessRosErrorMacroType (m, td, type, bt, bt->a.macroType->a.rosError, v);
             break;

        case MACROTYPE_ROSBIND:
        case MACROTYPE_ROSUNBIND:
            ProcessRosBindMacroType (m, td, type, bt, bt->a.macroType->a.rosBind,v);
             break;

        case MACROTYPE_ROSASE:
            ProcessRosAseMacroType (m, td, type, bt, bt->a.macroType->a.rosAse,v);
             break;

        case MACROTYPE_MTSASEXTENSIONS:
            ProcessMtsasExtensionsMacroType (m, td, type, bt, bt->a.macroType->a.mtsasExtensions,v);
            break;

        case MACROTYPE_MTSASEXTENSION:
            ProcessMtsasExtensionMacroType (m, td, type, bt, bt->a.macroType->a.mtsasExtension,v);
            break;

        case MACROTYPE_MTSASEXTENSIONATTRIBUTE:
            ProcessMtsasExtensionAttributeMacroType (m, td, type, bt, bt->a.macroType->a.mtsasExtensionAttribute,v);
            break;

        case MACROTYPE_MTSASTOKEN:
            ProcessMtsasTokenMacroType (m, td, type, bt, bt->a.macroType->a.mtsasToken,v);
            break;

        case MACROTYPE_MTSASTOKENDATA:
            ProcessMtsasTokenDataMacroType (m, td, type, bt, bt->a.macroType->a.mtsasTokenData,v);
            break;

        case MACROTYPE_MTSASSECURITYCATEGORY:
            ProcessMtsasSecurityCategoryMacroType (m, td, type, bt, bt->a.macroType->a.mtsasSecurityCategory,v);
            break;

        case MACROTYPE_ASNOBJECT:
            ProcessAsnObjectMacroType (m, td, type, bt, bt->a.macroType->a.asnObject,v);
            break;

        case MACROTYPE_ASNPORT:
            ProcessAsnPortMacroType (m, td, type, bt, bt->a.macroType->a.asnPort,v);
            break;

        case MACROTYPE_ASNABSTRACTBIND:
        case MACROTYPE_ASNABSTRACTUNBIND:
            ProcessAsnAbstractBindMacroType (m, td, type, bt, bt->a.macroType->a.asnAbstractBind,v);
            break;

        case MACROTYPE_AFALGORITHM:
        case MACROTYPE_AFENCRYPTED:
        case MACROTYPE_AFPROTECTED:
        case MACROTYPE_AFSIGNATURE:
        case MACROTYPE_AFSIGNED:
            break;

        case MACROTYPE_SNMPOBJECTTYPE:
            ProcessSnmpObjectTypeMacroType (m, td, type, bt, bt->a.macroType->a.snmpObjectType,v);
            break;

        default:
           /* ignore any others */
        break;
    }

        default:
          /* the rest do not need processing */

        break;
    }
}  /* ProcessMacrosInBasicType */


/*
 * Given a Type referenced in a macro, makes up a name and defines
 * the type iff the type is not a simple type ref or library type.
 * Returns the typedef of the type given type. (may be new may
 * be from the typeref if t was a local or import type ref)
 */
void
DefineType PARAMS ((m, td, t, name),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    char *name)
{
   int digit;
   TypeDef *newDef;
   TypeDef **tmpTypeDefHndl;
   Type *tmpType;
   TypeDef *retVal;

   if (IsNewType (t))
   {
       newDef = (TypeDef*)Malloc (sizeof (TypeDef));
       newDef->exported = FALSE;
       newDef->type = (Type*)Malloc (sizeof (Type));
       memcpy (newDef->type, t, sizeof (Type));


       newDef->definedName = Malloc (strlen (name)+4);
       strcpy (newDef->definedName, name);

       if (islower (newDef->definedName[0]))
           newDef->definedName[0] =
               toupper (newDef->definedName[0]);


       /* set up unique type name for new type */
       for (digit = 0;
           (LookupType (m->typeDefs, newDef->definedName) != NULL);
           digit++)
           AppendDigit (newDef->definedName, digit);

       /*
        * now put new typedef  at head of list
        */
       tmpTypeDefHndl = (TypeDef**)AsnListPrepend (m->typeDefs);
       *tmpTypeDefHndl = newDef;

       /* convert macro's type def into a ref */

       SetupType (&tmpType, BASICTYPE_LOCALTYPEREF, 0);
       memcpy (t, tmpType, sizeof (Type));
       Free (tmpType);
       t->implicit = FALSE;
       t->basicType->a.localTypeRef = (TypeRef*)Malloc (sizeof (TypeRef));
       t->basicType->a.localTypeRef->link = newDef;
       t->basicType->a.localTypeRef->module = m;
       t->basicType->a.localTypeRef->typeName =
           newDef->definedName;

    }
} /* DefineType */


void
ProcessRosOperationMacroType PARAMS ((m, td, t, bt, op, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosOperationMacroType *op _AND_
    ValueDef *v)
{

    if (op->arguments != NULL)
        DefineType (m, td, op->arguments->type, v->definedName);

    if (op->result != NULL)
        DefineType (m, td, op->result->type, v->definedName);

}  /* ProcessRosOperationMacroType */



void
ProcessRosErrorMacroType PARAMS ((m, td, t, bt, err, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosErrorMacroType *err _AND_
    ValueDef *v)
{
    if ((err != NULL) && (err->parameter != NULL))
        DefineType (m, td, err->parameter->type, v->definedName);

}   /* ProcessRosErrorMacroType */


void
ProcessRosBindMacroType PARAMS ((m, td, t, bt, bind, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosBindMacroType *bind _AND_
    ValueDef *v)
{
    if (bind != NULL)
    {
        DefineType (m, td, bind->argument->type, v->definedName);
        DefineType (m, td, bind->result->type, v->definedName);
        DefineType (m, td, bind->error->type, v->definedName);
    }
}   /* ProcessRosBindMacroType */


void
ProcessRosAseMacroType PARAMS ((m, td, t, bt, ase, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAseMacroType *ase _AND_
    ValueDef *v)
{

}  /* ProcessRosAseMacroType */



void
ProcessRosAcMacroType PARAMS ((m, td, t, bt, ac, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAcMacroType *ac  _AND_
    ValueDef *v)
{

}  /* ProcessRosAcMacroType */



void
ProcessMtsasExtensionsMacroType PARAMS ((m, td, t, bt, exts, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionsMacroType *exts _AND_
    ValueDef *v)
{

}  /* ProcessMtsasExtensionsMacroType */


void
ProcessMtsasExtensionMacroType PARAMS ((m, td, t, bt, ext, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionMacroType *ext _AND_
    ValueDef *v)
{

}  /* ProcessMtsasExtensionMacroType */


void
ProcessMtsasExtensionAttributeMacroType PARAMS ((m, td, t, bt, ext, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionAttributeMacroType *ext _AND_
    ValueDef *v)
{

}  /* ProcessMtsasExtensionAttributeMacroType */


void
ProcessMtsasTokenMacroType PARAMS ((m, td, t, bt, tok, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenMacroType *tok _AND_
    ValueDef *v)
{

}  /* ProcessMtsasTokenMacroType */


void
ProcessMtsasTokenDataMacroType PARAMS ((m, td, t, bt, tok, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenDataMacroType *tok _AND_
    ValueDef *v)
{


}  /* ProcessMtsasTokenDataMacroType */


void
ProcessMtsasSecurityCategoryMacroType PARAMS ((m, td, t, bt, sec, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasSecurityCategoryMacroType *sec _AND_
    ValueDef *v)
{

}  /* ProcessMtsasSecurityCategoryMacroType */



void
ProcessAsnObjectMacroType PARAMS ((m, td, t, bt, obj, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnObjectMacroType *obj _AND_
    ValueDef *v)
{

}  /* ProcessAsnObjectMacroType */


void
ProcessAsnPortMacroType PARAMS ((m, td, t, bt, p, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnPortMacroType *p _AND_
    ValueDef *v)
{

}  /* ProcessAsnPortMacroType */



void
ProcessAsnAbstractBindMacroType PARAMS ((m, td, t, bt, bind, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnAbstractBindMacroType *bind _AND_
    ValueDef *v)
{

}  /* ProcessAsnBindMacroType */


void
ProcessSnmpObjectTypeMacroType  PARAMS ((m, td, t, bt, ot, v),
    Module *m _AND_
    TypeDef *td _AND_
    Type *t _AND_
    BasicType *bt _AND_
    SnmpObjectTypeMacroType *ot _AND_
    ValueDef *v)
{
    NamedType *nt;
    NamedType **tmpNtHndl;
    char anyId[256];
    AnyRefList **arlHndl;

    if ((ot != NULL) && (ot->syntax != NULL))
         DefineType (m, td, ot->syntax, v->definedName);

    /*
     * add ANY ref stuff to type ref'd by this macro so it is
     * included in the ANY hash table.
     */

    /*
     * do this since the SNMP spec doesn't have an ANY type
     * but uses the mechanism. (SNMP uses an OCTET STRING
     * where the 'ANY' value is
     */
    m->hasAnys = TRUE;

    strcpy (anyId, v->definedName);
/*    Str2UCase (anyId, strlen (anyId)); */
    strcat (anyId, "_ANY_ID");

    arlHndl = GetAnyRefListHndl (ot->syntax);


    if (v->value->basicValue->choiceId == BASICVALUE_OID)
        AddAnyRefByOid (arlHndl, anyId, v->value->basicValue->a.oid);

    /*  integer types are not allowed, but relax constraints anyway */
    else
        AddAnyRefByInt (arlHndl, anyId, v->value->basicValue->a.integer);



    /* make a choice with all the object type elmts */
    /*   USING THE ANY HASH TABLE NOW
    if (snmpObjectSyntaxesG == NULL)
    {
        snmpObjectSyntaxesG = (TypeDef*) Malloc (sizeof (TypeDef));
        SetupType (&snmpObjectSyntaxesG->type, BASICTYPE_CHOICE, 0);
        snmpObjectSyntaxesG->type->basicType->a.choice =
            AsnListNew (sizeof (void*));
        snmpObjectSyntaxesG->definedName = "SnmpOpaqueTypes";

    }
    */

    /*  NOT DONE ANYMORE
     * make each field in the choice the same as the object
     * types SYNTAX field type (adjusted by Define type)
     * make choice field name same as OBJ-TYPE value Defs name
     *
     * NOTE - using ptrs to type/fieldname, not duplicating them
     * this may cause freeing probs
     */
    /*
    nt = MT (NamedType);
    nt->fieldName = v->definedName;
    nt->type = ot->syntax;

    tmpNtHndl = (NamedType**)
        AsnListAppend (snmpObjectSyntaxesG->type->basicType->a.choice);
    *tmpNtHndl = nt;
    */

}  /* ProcessSnmpObjectTypeMacro */
