/*
 * compiler/core/link_types.c
 *
 *  Links type references. Also increments 'refCount' in a TypeDef
 *
 *  Does type checking when linking SELECTION and COMPONENTS OF types
 *
 * MS
 * 91/09/04
 * Completely Rewritten for new ModuleList data structure (ASN.1 based)
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/link-types.c,v 1.1 2001/06/20 21:27:57 dmitch Exp $
 * $Log: link-types.c,v $
 * Revision 1.1  2001/06/20 21:27:57  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:49  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:36  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:38:30  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:17  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <ctype.h>
#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "snacc-util.h"
#include "link-types.h"

extern Module *usefulTypeModG;

/* non-exported prototypes */

void TypeLinkImportLists PROTO ((ModuleList *m));

void TypeLinkTypeDef PROTO ((ModuleList *m, Module *currMod, TypeDef *head));

void TypeLinkElmtTypes PROTO ((ModuleList *m, Module *currMod, TypeDef *head, NamedTypeList *e));

void TypeLinkElmtType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, NamedType *n));

void TypeLinkType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *currType));

void TypeLinkBasicType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *type, BasicType *bt));

void TypeLinkSubtypes PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *currType, Subtype *s));

void TypeLinkSubtypeValue PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *currType, SubtypeValue *s));

void TypeLinkNamedElmts PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, ValueDefList *v));



void TypeLinkValueDef PROTO ((ModuleList *m, Module *currMod, ValueDef *v));


void TypeLinkValue PROTO ((ModuleList *m, Module *currMod, ValueDef *head, Type *valuesType, Value *v));

void TypeLinkRosOperationMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, RosOperationMacroType *op));


void TypeLinkRosErrorMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, RosErrorMacroType *err));


void TypeLinkRosBindMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, RosBindMacroType *bind));


void TypeLinkRosAseMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, RosAseMacroType *ase));

void TypeLinkRosAcMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, RosAcMacroType *ac));

void TypeLinkMtsasExtensionsMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionsMacroType *exts));

void TypeLinkMtsasExtensionMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionMacroType *ext));

void TypeLinkMtsasExtensionAttributeMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionAttributeMacroType *ext));

void TypeLinkMtsasTokenMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasTokenMacroType *tok));

void TypeLinkMtsasTokenDataMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasTokenDataMacroType *tok));

void TypeLinkMtsasSecurityCategoryMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, MtsasSecurityCategoryMacroType *sec));

void TypeLinkAsnObjectMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, AsnObjectMacroType *obj));

void TypeLinkAsnPortMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, AsnPortMacroType *p));

void TypeLinkAsnAbstractBindMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, AsnAbstractBindMacroType *bind));

void TypeLinkSnmpObjectTypeMacroType PROTO ((ModuleList *m, Module *currMod, TypeDef *head, Type *t, BasicType *bt, SnmpObjectTypeMacroType *ot));


/* end of prototypes */




static char *asn1SrcFileNameG;

/*
 * returns 0 if no link error occured,
 * otherwise returns a value < 0.
 * Processing should not continue if an error is returned
 */
int
LinkTypeRefs PARAMS ((m),
    ModuleList *m)
{
    Module   *currMod;
    TypeDef  *td;
    ValueDef *vd;
    int linkErr = 0;

    /*
     * link imported types/values to their definition if
     * the defining module is in the modulelist
     */
    TypeLinkImportLists (m);


    /* link useful types */
    if (usefulTypeModG != NULL)
    {
        FOR_EACH_LIST_ELMT (td, usefulTypeModG->typeDefs)
        {
            TypeLinkTypeDef (m, usefulTypeModG, td);
        }

        FOR_EACH_LIST_ELMT (vd, usefulTypeModG->valueDefs)
        {
            TypeLinkValueDef (m, usefulTypeModG, vd);
        }

        if (usefulTypeModG->status != MOD_ERROR)
            usefulTypeModG->status = MOD_OK;
        else
            linkErr = -1;
    }

    /*
     * go through types, values & macros of  each module
     */
    FOR_EACH_LIST_ELMT (currMod, m)
    {
        asn1SrcFileNameG = currMod->asn1SrcFileName;

        /*
         * go through each type in typeList and link as nec
         */
        FOR_EACH_LIST_ELMT (td, currMod->typeDefs)
        {
            TypeLinkTypeDef (m, currMod, td);
        }


        /*
         *  go through each value in valueList and link as nec
         */
        FOR_EACH_LIST_ELMT (vd, currMod->valueDefs)
        {
            TypeLinkValueDef (m, currMod, vd);
        }

        if (currMod->status != MOD_ERROR)
            currMod->status = MOD_OK;
        else
            linkErr = -1;
    }

    return linkErr;

} /* LinkRefs */


/*
 * goes through import lists of each module making sure each
 * imported type is in the referenced module.  Will flag
 * errors if the imported type cannot be found or is not
 * exported by the referenced module.
 */
void
TypeLinkImportLists PARAMS ((m),
    ModuleList *m)
{
    Module *currMod;
    TypeDef *t;
    ValueDef *v;
    ImportModule *currImpList;
    ImportElmt *currImpElmt;
    Module *impRefMod;


    /* Link each modules imports */
    FOR_EACH_LIST_ELMT (currMod, m)
    {
        /*
         * Link each import list in the currMod.
         * (there is an import list for every module
         * imported from by this module
         */
        FOR_EACH_LIST_ELMT (currImpList, currMod->imports)
        {
            /* lookup ref'd module by it's name and oid (if any) */
            impRefMod = LookupModule (m, currImpList->modId->name, currImpList->modId->oid);

            if (impRefMod == NULL)
            {
                /*
                 * The needed module is not available.
                 * Let user know and set fatal error
                 */
                currMod->status = MOD_ERROR;
                PrintErrLoc (currMod->asn1SrcFileName, currImpList->lineNo);
                fprintf (stderr,"ERROR - cannot locate IMPORT module \"%s\", ", currImpList->modId->name);

                fprintf (stderr,"so the following types/values are missing:\n");
                FOR_EACH_LIST_ELMT (currImpElmt, currImpList->importElmts)
                {
                    fprintf (stderr,"        "); /* indent */
                    if (currImpElmt->privateScope)
                        fprintf (stderr,"%s.", currImpList->modId->name);
                    fprintf (stderr,"%s\n", currImpElmt->name);
                }
                fprintf (stderr,"\n");
                /*
                 * go onto next import list in this module
                 * to report more errors if any
                 */
                continue;
            }

            /*
             * go through each import elements and look for the
             * the referenced type in the ref'd module
             */
            FOR_EACH_LIST_ELMT (currImpElmt, currImpList->importElmts)
            {
                /*
                 * only do types (types have uppercase first letter)
                 */
                if  (!isupper (currImpElmt->name[0]))
                    continue;

                /* look for the type in the ref'd module */
                t = LookupType (impRefMod->typeDefs, currImpElmt->name);

                if (t != NULL)
                {
                    if (!t->exported)
                    {
                        currMod->status = MOD_ERROR;
                        PrintErrLoc (currMod->asn1SrcFileName, currImpElmt->lineNo);
                        fprintf (stderr,"ERROR - \"%s\" module imports \"%s\", which is not exported from module \"%s\".\n", currMod->modId->name, currImpElmt->name, impRefMod->modId->name);
                    }

                    /* set as ref'd if imported by someone */
                    t->importRefCount++;
                    currImpElmt->resolvedRef =
                        (ImportElmtChoice*)Malloc (sizeof (ImportElmtChoice));
                    currImpElmt->resolvedRef->choiceId = IMPORTELMTCHOICE_TYPE;
                    currImpElmt->resolvedRef->a.type = t;

                }
                else /* type not found in ref'd module */
                {
                    currMod->status = MOD_ERROR;
                    PrintErrLoc (currMod->asn1SrcFileName, currImpElmt->lineNo);
                    fprintf (stderr,"ERROR - \"%s\" is imported from module \"%s\" by module \"%s\", but is not defined in the referenced module\n", currImpElmt->name, impRefMod->modId->name, currMod->modId->name);
                }

            }
        }
    }
} /* TypeLinkImportLists */


/*
 * given a type def, it goes through the entire typedef
 * (aggregate parts if any) and links refs
 */
void
TypeLinkTypeDef PARAMS ((m, currMod, head),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head)
{
    if (head == NULL)
        return;

    TypeLinkType (m, currMod, head, head->type);
}  /* LinkTypeDef */


/*
 * given a type t, this routine goes through the components of
 * the type and links any type references
 */
void
TypeLinkType PARAMS ((m, currMod, head, t),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t)
{
    if (t == NULL)
        return;

    /* like main type information */
    TypeLinkBasicType (m, currMod, head, t, t->basicType);

    /* link any type references in the subtypes (if any)*/
    TypeLinkSubtypes (m, currMod, head, t, t->subtypes);

    /* like type refs in the default value (if any) */
    if (t->defaultVal != NULL)
        TypeLinkValue (m, currMod, NULL, t,  t->defaultVal->value);

}  /* TypeLinkType */


/*
 * given a sequence of NamedTypes (components of a SET, SEQ or
 * CHOICE etc), this links any type refs in each one.
 */
void
TypeLinkElmtTypes PARAMS ((m, currMod, head, e),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    NamedTypeList *e)
{
    NamedType *n;
    FOR_EACH_LIST_ELMT (n, e)
    {
        TypeLinkElmtType (m, currMod, head, n);
    }
}  /* TypeLinkElmtTypes */


void
TypeLinkElmtType PARAMS ((m, currMod, head, n),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    NamedType *n)
{
    if (n != NULL)
        TypeLinkType (m, currMod, head, n->type);
}

/*
 * given a BasicType, this links any type refs that are
 * part of it.
 */
void
TypeLinkBasicType PARAMS ((m, currMod, head, type, bt),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *type _AND_
    BasicType *bt)
{
    TypeDef *tmpTypeDef;
    TypeDefList *tmpTypeDefs;
    Type *tmpType;
    Module *tmpMod;
    NamedType *tmpElmtType;
    ImportElmt *impElmt;
    ImportModule *impMod;
    int implicitRef;

    if (bt == NULL)
        return;

    switch (bt->choiceId)
    {

        case BASICTYPE_SEQUENCE:
        case BASICTYPE_SET:
        case BASICTYPE_CHOICE:
            TypeLinkElmtTypes (m, currMod, head, bt->a.set);
            break;

        case BASICTYPE_SEQUENCEOF:
        case BASICTYPE_SETOF:
            TypeLinkType (m, currMod, head, bt->a.setOf);
            break;

        case BASICTYPE_SELECTION:
            TypeLinkType (m, currMod, head, bt->a.selection->typeRef);

            /*
             * check that elmt type is CHOICE
             * and set up link  (if resolved)
             */
            tmpType = bt->a.selection->typeRef;
            if ((tmpType->basicType->choiceId == BASICTYPE_IMPORTTYPEREF) ||
                (tmpType->basicType->choiceId == BASICTYPE_LOCALTYPEREF))
            {
                tmpTypeDef = tmpType->basicType->a.importTypeRef->link;
                if (tmpTypeDef == NULL) /* unlinked import or local type */
                {
                    currMod->status = MOD_ERROR;
                    return;
                }
            }
            else
            {
                PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                fprintf (stderr,"ERROR - selection type defines type instead of referencing CHOICE field.\n");
                currMod->status = MOD_ERROR;
                return;
            }

            /*
             * selections types must reference choice types
             */
            tmpType = ParanoidGetType (tmpTypeDef->type);
            if (tmpType->basicType->choiceId != BASICTYPE_CHOICE)
            {
                PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                fprintf (stderr,"ERROR - SELECTION types must reference a CHOICE type\n");
                currMod->status = MOD_ERROR;
                return;
            }

            /*
             * find field ref'd by selection
             */
            tmpElmtType = LookupFieldInType (tmpTypeDef->type, bt->a.selection->fieldName);
            if (tmpElmtType == NULL)
            {
                PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                fprintf (stderr,"ERROR - selection type's reference field name \"%s\" is not in CHOICE \"%s\".\n", bt->a.selection->fieldName, tmpTypeDef->definedName);
                currMod->status = MOD_ERROR;
                return;
            }

            bt->a.selection->link = tmpElmtType;
            break;


        case BASICTYPE_COMPONENTSOF:
            TypeLinkType (m, currMod, head, bt->a.componentsOf);
            /* error checks done in normalize.c */
            break;


        case BASICTYPE_ANYDEFINEDBY:
            /*
             *  set the link to the defining field if not already linked
             */
            if (bt->a.anyDefinedBy->link == NULL)
            {
                /*
                 * get set or seq that holds this any def'd by
                 */
                tmpType = GetParent (head->type, type);

                if (tmpType == NULL)
                {
                    PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                    fprintf (stderr," ERROR - could not find parent type for linking ANY DEFINED BY\n");
                }

                /*
                 * find "defining" field
                 */
                tmpElmtType = LookupFieldInType (tmpType, bt->a.anyDefinedBy->fieldName);

                if (tmpElmtType == NULL)
                {
                    currMod->status = MOD_ERROR;
                    PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                    fprintf (stderr," ERROR - could not find identifier field \"%s\" in type \"%s\" for linking ANY DEFINED BY\n", bt->a.anyDefinedBy->fieldName, head->definedName);
                }

                bt->a.anyDefinedBy->link = tmpElmtType;
            }
            break;


        case BASICTYPE_LOCALTYPEREF:
            /*
             * Remember: the parser sets any typeref it encounters
             * to LOCALTYPE_REF, so some Localtyperefs may be import
             * type refs.
             */
            /*
             * First, look in this module's type defs and create a
             * resolvedLocalTypeRef if it's there.
             */
            if ((tmpTypeDef = LookupType (currMod->typeDefs, bt->a.localTypeRef->typeName)) != NULL)
            {
                /*
                 * locally defined type
                 */
                tmpTypeDef->localRefCount++;

                bt->a.localTypeRef->link = tmpTypeDef;
                bt->a.localTypeRef->module = currMod;
                break; /* finished here */
            }
            else /* not locally defined type */
                bt->choiceId = BASICTYPE_IMPORTTYPEREF;
               /*  !!!!!! fall through !!!!!!!! */

        case BASICTYPE_IMPORTTYPEREF:

            /* This handles "modname.type" type refs. */
            if (bt->a.importTypeRef->moduleName != NULL)
            {
                /*
                 * Lookup the import list maintained in this module
                 * from the named module.  (the parser generates
                 * an import list from Foo module for "Foo.Bar" style
                 * import refs)
                 */
                impMod = LookupImportModule (currMod, bt->a.importTypeRef->moduleName);

                if (impMod == NULL) /* whoa, compiler error */
                {
                    currMod->status = MOD_ERROR;
                    fprintf (stderr,"Compiler Error: \"%s.%s\" typeref - no import list defined from module \"%s\"\n", bt->a.importTypeRef->moduleName, bt->a.importTypeRef->typeName, bt->a.importTypeRef->moduleName);

                    return;
                }
                impElmt = LookupImportElmtInImportElmtList (impMod->importElmts, bt->a.importTypeRef->typeName);

                if (impElmt == NULL) /* whoa, compiler error again */
                {
                    currMod->status = MOD_ERROR;
                    fprintf (stderr,"Compiler Error: \"%s.%s\" typeref - no import element defined for type \"%s\"\n", bt->a.importTypeRef->moduleName, bt->a.importTypeRef->typeName, bt->a.importTypeRef->typeName);

                    return;
                }
                /*
                 * should already be resolved unless could not find
                 * the import for some reason
                 */
                if (impElmt->resolvedRef != NULL)
                {
                   if (impElmt->resolvedRef->choiceId != IMPORTELMTCHOICE_TYPE)
                        fprintf (stderr,"Linker Warning: import TYPE ref \"%s\" resolves with an imported VALUE\n", impElmt->name);

                   bt->a.importTypeRef->link = impElmt->resolvedRef->a.type;
                   bt->a.importTypeRef->link->importRefCount++;
                   bt->a.importTypeRef->module = impMod->moduleRef;
                }
                else
                {
                    /* print loc of refs to unresolved imports */
                    PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                    fprintf (stderr,"reference to unresolved imported type \"%s\"\n", impElmt->name);
                }

            }
            else    /* not a "modname.type" type ref */
            {
                impElmt = LookupImportElmtInModule (currMod, bt->a.importTypeRef->typeName, &impMod);

                /*
                 * privateScope one's should only resolve with one's
                 * non-null module names (see last if) (mod.type form)
                 */
                if ((impElmt != NULL) && (!impElmt->privateScope))
                {
                    /*
                     * should already be resolved unless could not find
                     * the import for some reason
                     */
                    if (impElmt->resolvedRef != NULL)
                    {
                        if (impElmt->resolvedRef->choiceId != IMPORTELMTCHOICE_TYPE)
                            fprintf (stderr,"Linker Warning: import TYPE ref \"%s\" resolves with an imported VALUE\n", impElmt->name);

                        bt->a.importTypeRef->link = impElmt->resolvedRef->a.type;
                        bt->a.importTypeRef->link->importRefCount++;
                        bt->a.importTypeRef->module = impMod->moduleRef;
                    }
                    else
                    {
                        /* print loc of refs to unresolved imports */
                        PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                        fprintf (stderr,"reference to unresolved imported type \"%s\"\n", impElmt->name);
                    }
                }
                /*
                 * Since not locally defined or imported,
                 * look in useful types module (if any).
                 * Useful type references are treated as imported
                 * type references (from the useful types module)
                 */
                else if ((usefulTypeModG != NULL) && (tmpTypeDef = LookupType (usefulTypeModG->typeDefs, bt->a.localTypeRef->typeName)) != NULL)
                {
                    bt->a.importTypeRef->link = tmpTypeDef;
                    bt->a.importTypeRef->module = usefulTypeModG;
                }
                else  /* impElmt == NULL */
                {
                    /*
                     *  Type not defined locally, imported or
                     *  in useful types module.
                     */
                    currMod->status = MOD_ERROR;
                    PrintErrLoc (currMod->asn1SrcFileName, type->lineNo);
                    fprintf (stderr,"ERROR - type \"%s\" is referenced but not defined or imported.\n", bt->a.importTypeRef->typeName);
                }
            }
            break;


        /*
         * these types may optionally have named elmts
         */
        case BASICTYPE_INTEGER:
        case BASICTYPE_BITSTRING:
        case BASICTYPE_ENUMERATED:
            TypeLinkNamedElmts (m, currMod, head, type, bt->a.integer);
            break;


        /*
         * these types have no extra info and cause no linking action
         */
        case BASICTYPE_UNKNOWN:
        case BASICTYPE_BOOLEAN:
        case BASICTYPE_OCTETSTRING:
        case BASICTYPE_NULL:
        case BASICTYPE_OID:
        case BASICTYPE_REAL:
        case BASICTYPE_ANY:
        case BASICTYPE_MACRODEF:
            break;

        case BASICTYPE_MACROTYPE:
            switch (bt->a.macroType->choiceId)
            {
                case MACROTYPE_ROSOPERATION:
                case MACROTYPE_ASNABSTRACTOPERATION:
                    TypeLinkRosOperationMacroType (m, currMod, head, type, bt, bt->a.macroType->a.rosOperation);
                    break;

                case MACROTYPE_ROSERROR:
                case MACROTYPE_ASNABSTRACTERROR:
                    TypeLinkRosErrorMacroType (m, currMod, head, type, bt, bt->a.macroType->a.rosError);
                    break;

                case MACROTYPE_ROSBIND:
                case MACROTYPE_ROSUNBIND:
                    TypeLinkRosBindMacroType (m, currMod, head, type, bt, bt->a.macroType->a.rosBind);
                    break;

                case MACROTYPE_ROSASE:
                    TypeLinkRosAseMacroType (m, currMod, head, type, bt, bt->a.macroType->a.rosAse);
                    break;

                case MACROTYPE_MTSASEXTENSIONS:
                    TypeLinkMtsasExtensionsMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasExtensions);
                    break;

                case MACROTYPE_MTSASEXTENSION:
                    TypeLinkMtsasExtensionMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasExtension);
                    break;

                case MACROTYPE_MTSASEXTENSIONATTRIBUTE:
                    TypeLinkMtsasExtensionAttributeMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasExtensionAttribute);
                    break;

                case MACROTYPE_MTSASTOKEN:
                    TypeLinkMtsasTokenMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasToken);
                    break;

                case MACROTYPE_MTSASTOKENDATA:
                    TypeLinkMtsasTokenDataMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasTokenData);
                    break;

                case MACROTYPE_MTSASSECURITYCATEGORY:
                    TypeLinkMtsasSecurityCategoryMacroType (m, currMod, head, type, bt, bt->a.macroType->a.mtsasSecurityCategory);
                    break;

                case MACROTYPE_ASNOBJECT:
                    TypeLinkAsnObjectMacroType (m, currMod, head, type, bt, bt->a.macroType->a.asnObject);
                    break;

                case MACROTYPE_ASNPORT:
                    TypeLinkAsnPortMacroType (m, currMod, head, type, bt, bt->a.macroType->a.asnPort);
                    break;

                case MACROTYPE_ASNABSTRACTBIND:
                case MACROTYPE_ASNABSTRACTUNBIND:
                    TypeLinkAsnAbstractBindMacroType (m, currMod, head, type, bt, bt->a.macroType->a.asnAbstractBind);
                    break;

                case MACROTYPE_AFALGORITHM:
                case MACROTYPE_AFENCRYPTED:
                case MACROTYPE_AFPROTECTED:
                case MACROTYPE_AFSIGNATURE:
                case MACROTYPE_AFSIGNED:
                    TypeLinkType (m, currMod, head, bt->a.macroType->a.afAlgorithm);
                    break;

                case MACROTYPE_SNMPOBJECTTYPE:
                    TypeLinkSnmpObjectTypeMacroType (m, currMod, head, type, bt, bt->a.macroType->a.snmpObjectType);
                    break;

                default:
                    fprintf (stderr, "TypeLinkBasicType: ERROR - unknown macro type id!\n");
            }
            break;

        default:
            fprintf (stderr, "TypeLinkBasicType: ERROR - unknown basic type id!\n");
    }

}  /* LinkBasicType */




/*
 * resolve any type/value refs in the subtypes (if any)
 */
void
TypeLinkSubtypes PARAMS ((m, currMod, head, currType, s),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *currType _AND_
    Subtype *s)
{
    Subtype *sElmt;

    if (s == NULL)
        return;

    switch (s->choiceId)
    {
        case SUBTYPE_SINGLE:
            TypeLinkSubtypeValue (m, currMod, head, currType, s->a.single);
        break;

        case SUBTYPE_AND:
        case SUBTYPE_OR:
        case SUBTYPE_NOT:
            FOR_EACH_LIST_ELMT (sElmt, s->a.and)
            {
                TypeLinkSubtypes (m, currMod, head, currType, sElmt);
            }
        break;

        default:
            fprintf (stderr, "TypeLinkSubtypes: ERROR - unknown Subtype id\n");
        break;
    }
}  /* TypeLinkSubtypes */




/*
 * link any type referenced in the value parts of subtypes
 */
void
TypeLinkSubtypeValue PARAMS ((m, currMod, head, currType, s),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *currType _AND_
    SubtypeValue *s)
{
    Constraint *constraint;

    if (s == NULL)
        return;

    switch (s->choiceId)
    {
        case SUBTYPEVALUE_SINGLEVALUE:
            TypeLinkValue (m, currMod, NULL, currType, s->a.singleValue);
            break;

        case SUBTYPEVALUE_CONTAINED:
            TypeLinkType (m, currMod, head, s->a.contained);
            break;

        case SUBTYPEVALUE_VALUERANGE:
            TypeLinkValue (m, currMod, NULL, currType, s->a.valueRange->lowerEndValue);
            TypeLinkValue (m, currMod, NULL, currType, s->a.valueRange->upperEndValue);
            break;

        case SUBTYPEVALUE_PERMITTEDALPHABET:
            TypeLinkSubtypes (m, currMod, head, currType, s->a.permittedAlphabet);
           break;

        case SUBTYPEVALUE_SIZECONSTRAINT:
            TypeLinkSubtypes (m, currMod, head, currType, s->a.sizeConstraint);
            break;

        case SUBTYPEVALUE_INNERSUBTYPE:
            FOR_EACH_LIST_ELMT (constraint, s->a.innerSubtype->constraints)
            {
                TypeLinkSubtypes (m, currMod, head, currType, constraint->valueConstraints);
            }
            break;

        default:
            fprintf (stderr,"TypeLinkSubtype: ERROR - unknown subtype choiceId\n");
    }

} /* TypeLinkSubtype */



/*
 * go through named elements of INTEGER/ENUMERATED/BOOLEAN
 * and link any type refs in the values
 */
void
TypeLinkNamedElmts PARAMS ((m, currMod, head, t, v),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    ValueDefList *v)
{
    ValueDef *vd;
    FOR_EACH_LIST_ELMT (vd, v)
    {
        TypeLinkValue (m, currMod, vd, vd->value->type, vd->value);
    }

}  /* TypeLinkNamedElmts */



/*
 * only use this for 'real' value defs
 * ie those in the value def list - not ones for namedElmts
 * since infinitite recursion can result from the
 * attempt to link the values type which will try to link
 * this value again.
*/
void
TypeLinkValueDef PARAMS ((m, currMod, v),
    ModuleList *m _AND_
    Module *currMod _AND_
    ValueDef *v)
{
    if (v == NULL)
        return;

    TypeLinkType (m, currMod, NULL, v->value->type);

    if ((v->value->valueType == BASICTYPE_UNKNOWN) &&
        (v->value->type != NULL))
        v->value->valueType = v->value->type->basicType->choiceId;

}  /* TypeLinkValueDef */



/*
 * link any type refs associated with the given value.
 * also sets the values type field with the given
 * 'valuesType' Type.
 */
void
TypeLinkValue PARAMS ((m, currMod, head, valuesType, v),
    ModuleList *m _AND_
    Module *currMod _AND_
    ValueDef *head _AND_
    Type *valuesType _AND_
    Value *v)
{

    if (v == NULL)
        return;

    v->type = valuesType;
/*    TypeLinkType (m, currMod, NULL, v->typeRef); */

    if ((v->valueType == BASICTYPE_UNKNOWN) && (valuesType != NULL))
        v->valueType = valuesType->basicType->choiceId;

}  /* TypeLinkValue */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkRosOperationMacroType PARAMS ((m, currMod, head, t, bt, op),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosOperationMacroType *op)
{
    TypeOrValue *tOrV;

    if (op->arguments != NULL)
        TypeLinkType (m, currMod, head, op->arguments->type);

    if (op->result != NULL)
        TypeLinkType (m, currMod, head, op->result->type);

    /*
     *  go through errors (if any) and link types/values
     */
    FOR_EACH_LIST_ELMT (tOrV,  op->errors)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }

    /*
     *  go through linked operations (if any) and
     *  link types/values
     */
    FOR_EACH_LIST_ELMT (tOrV,  op->linkedOps)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }
}  /* TypeLinkRosOperationMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkRosErrorMacroType PARAMS ((m, currMod, head, t, bt, err),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosErrorMacroType *err)
{
    if ((err != NULL) && (err->parameter != NULL))
    {
        TypeLinkType (m, currMod, head, err->parameter->type);
    }
}   /* TypeLinkRosErrorMacroType */

/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkRosBindMacroType PARAMS ((m, currMod, head, t, bt, bind),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosBindMacroType *bind)
{
    if (bind != NULL)
    {
        TypeLinkElmtType (m, currMod, head, bind->argument);
        TypeLinkElmtType (m, currMod, head, bind->result);
        TypeLinkElmtType (m, currMod, head, bind->error);
    }
}   /* TypeLinkRosBindMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkRosAseMacroType PARAMS ((m, currMod, head, t, bt, ase),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAseMacroType *ase)
{
    Value *v;

    FOR_EACH_LIST_ELMT (v, ase->operations)
        TypeLinkValue (m, currMod, NULL, t, v);


    FOR_EACH_LIST_ELMT (v, ase->consumerInvokes)
        TypeLinkValue (m, currMod, NULL, t, v);


    FOR_EACH_LIST_ELMT (v, ase->supplierInvokes)
        TypeLinkValue (m, currMod, NULL, t, v);

}  /* TypeLinkRosAseMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkRosAcMacroType PARAMS ((m, currMod, head, t, bt, ac),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    RosAcMacroType *ac)
{
    Value *v;
    OID *oid;

    FOR_EACH_LIST_ELMT (v,  ac->nonRoElements)
        TypeLinkValue (m, currMod, NULL, t, v);


    TypeLinkType (m, currMod, head, ac->bindMacroType);
    TypeLinkType (m, currMod, head, ac->unbindMacroType);

    FOR_EACH_LIST_ELMT (v, ac->operationsOf)
        TypeLinkValue (m, currMod, NULL, t, v);


    FOR_EACH_LIST_ELMT (v, ac->initiatorConsumerOf)
        TypeLinkValue (m, currMod, NULL, t, v);


    FOR_EACH_LIST_ELMT (v, ac->responderConsumerOf)
        TypeLinkValue (m, currMod, NULL, t, v);

}  /* TypeLinkRosAcMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasExtensionsMacroType PARAMS ((m, currMod, head, t, bt, exts),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionsMacroType *exts)
{
    Value *v;

    FOR_EACH_LIST_ELMT (v, exts->extensions)
        TypeLinkValue (m, currMod, NULL, t, v);

}  /* TypeLinkMtsasExtensionsMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasExtensionMacroType PARAMS ((m, currMod, head, t, bt, ext),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionMacroType *ext)
{
    TypeLinkElmtType (m, currMod, head, ext->elmtType);
    TypeLinkValue (m, currMod, NULL, t, ext->defaultValue);

}  /* TypeLinkMtsasExtensionMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasExtensionAttributeMacroType PARAMS ((m, currMod, head, t, bt, ext),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasExtensionAttributeMacroType *ext)
{

    if (ext != NULL)
        TypeLinkType (m, currMod, head, ext->type);

}  /* TypeLinkMtsasExtensionAttributeMacroType */

/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasTokenMacroType PARAMS ((m, currMod, head, t, bt, tok),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenMacroType *tok)
{
    if (tok != NULL)
        TypeLinkType (m, currMod, head, tok->type);

}  /* TypeLinkMtsasTokenMacroType */

/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasTokenDataMacroType PARAMS ((m, currMod, head, t, bt, tok),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasTokenDataMacroType *tok)
{
    if (tok != NULL)
        TypeLinkType (m, currMod, head, tok->type);

}  /* TypeLinkMtsasTokenDataMacroType */

/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkMtsasSecurityCategoryMacroType PARAMS ((m, currMod, head, t, bt, sec),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    MtsasSecurityCategoryMacroType *sec)
{

    if (sec != NULL)
        TypeLinkType (m, currMod, head, sec->type);

}  /* TypeLinkMtsasSecurityCategoryMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkAsnObjectMacroType PARAMS ((m, currMod, head, t, bt, obj),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnObjectMacroType *obj)
{
    AsnPort *ap;

    FOR_EACH_LIST_ELMT (ap, obj->ports)
        TypeLinkValue (m, currMod, NULL, t, ap->portValue);

}  /* TypeLinkAsnObjectMacroType */

/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkAsnPortMacroType PARAMS ((m, currMod, head, t, bt, p),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnPortMacroType *p)
{
    TypeOrValue *tOrV;

    FOR_EACH_LIST_ELMT (tOrV, p->abstractOps)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }


    FOR_EACH_LIST_ELMT (tOrV, p->supplierInvokes)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }


    FOR_EACH_LIST_ELMT (tOrV, p->consumerInvokes)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }

}  /* TypeLinkAsnPortMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkAsnAbstractBindMacroType PARAMS ((m, currMod, head, t, bt, bind),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    AsnAbstractBindMacroType *bind)
{
    AsnPort *ap;

    FOR_EACH_LIST_ELMT (ap, bind->ports)
        TypeLinkValue (m, currMod, NULL, t, ap->portValue);

}  /* TypeLinkAsnBindMacroType */


/*
 * link any type refs in this macro's parsed data struct
 */
void
TypeLinkSnmpObjectTypeMacroType PARAMS ((m, currMod, head, t, bt, ot),
    ModuleList *m _AND_
    Module *currMod _AND_
    TypeDef *head _AND_
    Type *t _AND_
    BasicType *bt _AND_
    SnmpObjectTypeMacroType *ot)
{
    TypeOrValue *tOrV;

    TypeLinkType (m, currMod, head, ot->syntax);
    TypeLinkValue (m, currMod, NULL, t, ot->description);
    TypeLinkValue (m, currMod, NULL, t, ot->reference);
    TypeLinkValue (m, currMod, NULL, t, ot->defVal);

    FOR_EACH_LIST_ELMT (tOrV, ot->index)
    {
        if (tOrV->choiceId == TYPEORVALUE_TYPE)
            TypeLinkType (m, currMod, head, tOrV->a.type);
        else
            TypeLinkValue (m, currMod, NULL, t, tOrV->a.value);
    }

}  /* TypeLinkSnmpObjectTypeMacroType */
