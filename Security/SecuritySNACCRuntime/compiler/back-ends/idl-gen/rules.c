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
 * compiler/back_ends/idl_gen/rules.c - initialized c rule structure
 *           inits a table that contains info about
 *           converting each ASN.1 type to an IDL type
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/idl-gen/Attic/rules.c,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: rules.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:29  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:45  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.1  1997/01/01 20:25:38  rj
 * first draft
 *
 * Revision 1.3  1994/10/08  03:47:49  rj
 */

#include "asn-incl.h"
#include "asn1module.h"
#include "rules.h"


IDLRules idlRulesG =
{
    4,

    "",
    "_T",

    "Choice",
    "a",
    "ChoiceUnion",
    FALSE,
    {
        {
            BASICTYPE_UNKNOWN,
            "???",
            FALSE,
            FALSE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            TRUE,
            "NOT_NULL",
            "unknown"
        },
        {
            BASICTYPE_BOOLEAN,
            "BOOLEAN",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "bool"
        },
        {
            BASICTYPE_INTEGER,
            "INTEGER",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "integer"
        },
        {
            BASICTYPE_BITSTRING,
            "BitString",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "bits"
        },
        {
            BASICTYPE_OCTETSTRING,
            "OctetString",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "octs"
        },
        {
            BASICTYPE_NULL,
            "NULL",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "null"
        },
        {
            BASICTYPE_OID,
            "ObjectIdentifier",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "oid"
        },
        {
            BASICTYPE_REAL,
            "REAL",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "real"
        },
        {
            BASICTYPE_ENUMERATED,
            "???",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "enumeration"
        },
        {
            BASICTYPE_SEQUENCE,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            TRUE,
            "NOT_NULL",
            "seq"
        },
        {
            BASICTYPE_SEQUENCEOF,
            "AsnList",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "seqOf"
        },
        {
            BASICTYPE_SET,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            FALSE,
            "NOT_NULL",
            "set"
        },
        {
            BASICTYPE_SETOF,
            "AsnList",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "setOf"
        },
        {
            BASICTYPE_CHOICE,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            FALSE,
            "NOT_NULL",
            "choice"
        },
        {
            BASICTYPE_SELECTION,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "foo"
        },
        {
            BASICTYPE_COMPONENTSOF,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "bar"
        },
        {
            BASICTYPE_ANY,
            "any",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "any"
        },
        {
            BASICTYPE_ANYDEFINEDBY,
            "any",
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "anyDefBy"
        },
        {
            BASICTYPE_LOCALTYPEREF,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "foo"
        },
        {
            BASICTYPE_IMPORTTYPEREF,
            NULL,
            FALSE,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "bar"
        },
        {
            BASICTYPE_MACROTYPE,
            NULL,
            FALSE,
            FALSE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "foo"
        },
        {
            BASICTYPE_MACRODEF,
            NULL,
            FALSE,
            FALSE,
            FALSE,
            TRUE,
            TRUE,
            FALSE,
            TRUE,
            "NOT_NULL",
            "foo"
        }
    }
};
