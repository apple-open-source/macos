/*
 * compiler/core/print.h
 *
 * These are the prototypes for the typetree printing
 * routines.  Attempts to convert a typetree back into its original
 * ASN.1 def.
 *
 * Mike Sample
 * Mar 3/91
 *
 * Rewritten 91/09/05
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/print.h,v 1.1 2001/06/20 21:27:58 dmitch Exp $
 * $Log: print.h,v $
 * Revision 1.1  2001/06/20 21:27:58  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:52  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1994/10/08 03:48:56  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:49:33  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */




void PrintModule PROTO ((FILE *f, Module *mod));

void PrintExports PROTO ((FILE *f, Module *m));

void PrintOid PROTO ((FILE *f, OID *oid));

void PrintImportElmt PROTO ((FILE *f, ImportElmt *impElmt));

void PrintImportLists PROTO ((FILE *f, ImportModuleList *impLists));

void PrintTypeDefs PROTO ((FILE *f, TypeDefList *typeDefs));

void PrintType PROTO ((FILE *f, TypeDef *head, Type *t));

void PrintBasicType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt));

void PrintElmtType PROTO ((FILE *f, TypeDef *head, Type *t, NamedType *nt));

void PrintElmtTypes PROTO ((FILE *f, TypeDef *head, Type *t, NamedTypeList *e));

void PrintValueDefs PROTO ((FILE *f, ValueDefList *v));

void PrintValueDef PROTO ((FILE *f, ValueDef *v));

void PrintValue PROTO ((FILE *f, ValueDef *head, Type *valuesType, Value *v));

void PrintBasicValue PROTO ((FILE *f, ValueDef *head, Type *valuesType, Value *v, BasicValue *bv));

void PrintElmtValue PROTO ((FILE *f, ValueDef *head, Value *v, NamedValue *nv));

void PrintElmtValues PROTO ((FILE *f, ValueDef *head, Value *v, NamedValueList *e));

void PrintTag PROTO ((FILE *f, Tag *tag));

void PrintSubtype PROTO ((FILE *f, TypeDef *head, Type *t, Subtype *s));

void PrintSubtypeValue PROTO ((FILE *f, TypeDef *head, Type *t, SubtypeValue *s));

void PrintNamedElmts PROTO ((FILE *f, TypeDef *head, Type *t, ValueDefList *n));

void PrintInnerSubtype PROTO ((FILE *f, TypeDef *head, Type *t, InnerSubtype *i));

void PrintMultipleTypeConstraints PROTO ((FILE *f, TypeDef *head, Type *t, ConstraintList *c));

void PrintTypeById PROTO ((FILE *f, int typeId));


void PrintRosOperationMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, RosOperationMacroType *op));

void PrintRosErrorMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, RosErrorMacroType *err));

void PrintRosBindMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, RosBindMacroType *bind));

void PrintRosAseMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, RosAseMacroType *ase));

void PrintRosAcMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, RosAcMacroType *ac));

void PrintMtsasExtensionsMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionsMacroType *exts));

void PrintMtsasExtensionMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionMacroType *ext));

void PrintMtsasExtensionAttributeMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasExtensionAttributeMacroType *ext));

void PrintMtsasTokenMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasTokenMacroType *tok));

void PrintMtsasTokenDataMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasTokenDataMacroType *tok));

void PrintMtsasSecurityCategoryMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, MtsasSecurityCategoryMacroType *sec));

void PrintAsnObjectMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, AsnObjectMacroType *obj));

void PrintAsnPortMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, AsnPortMacroType *p));

void PrintAsnAbstractBindMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, AsnAbstractBindMacroType *bind));

void PrintAfAlgorithmMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, Type *alg));

void PrintAfEncryptedMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, Type *encrypt));

void PrintAfSignedMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, Type *sign));

void PrintAfSignatureMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, Type *sig));

void PrintAfProtectedMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, Type *p));

void PrintSnmpObjectTypeMacroType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt, SnmpObjectTypeMacroType *ot));

void PrintMacroDef PROTO ((FILE *f, TypeDef *head));

void PrintEncodedOid PROTO ((FILE *f, AsnOid *eoid));


void SpecialPrintType PROTO ((FILE *f, TypeDef *head, Type *t));

void SpecialPrintBasicType PROTO ((FILE *f, TypeDef *head, Type *t, BasicType *bt));

void SpecialPrintNamedElmts PROTO ((FILE *f, TypeDef *head, Type *t));
