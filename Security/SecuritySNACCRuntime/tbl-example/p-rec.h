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
 *  type definitions for the values that the snacc table
 *  decoder will return and snacc table encoder expects
 *  for the type table that contains the following modules:
 *  
 *      P-REC
 *
 *  UBC snacc written by Mike Sample
 *  NOTE: This is a machine generated file 
 *  NOTE2: Table routines don't use this so feel free to make changes
 *       that do not affect the structure. Changing field 
 *       and type names is fine.
 */


/* start of module P-REC's definitions */

typedef AsnInt EmployeeNumber;

typedef struct Name
{
    IA5String* givenName;
    IA5String* initial;
    IA5String* familyName;
} Name;

typedef IA5String Date;

typedef struct ChildInformation
{
    Name* field0;
    Date* dateOfBirth;
} ChildInformation;

typedef AsnList PersonnelRecordSeqOf;

typedef struct PersonnelRecord
{
    Name* field0;
    IA5String* title;
    EmployeeNumber* field1;
    Date* dateOfHire;
    Name* nameOfSpouse;
    PersonnelRecordSeqOf* children;
} PersonnelRecord;

