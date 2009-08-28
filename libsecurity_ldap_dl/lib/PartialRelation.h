/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __PARTIAL_RELATION__
#define __PARTIAL_RELATION__



#include "Relation.h"

typedef struct columnInfoLoader {
	uint32						mColumnID;
	const char					*mColumnName;
	CSSM_DB_ATTRIBUTE_FORMAT	mColumnFormat;
} columnInfoLoader;

typedef struct columnInfo {
	uint32						mColumnID;
	StringValue					*mColumnName;
	CSSM_DB_ATTRIBUTE_FORMAT	mColumnFormat;
} columnInfo;

/*
	PartialRelation.h
	
	This class provides common support for writing relations.
*/

class PartialRelation : public Relation
{
protected:
	int mNumberOfColumns;												// number of columns (attributes) this relation supports
	columnInfo *mColumnInfo;

public:
	PartialRelation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns, columnInfoLoader *theColumnInfo);
																		// pass in the relation ID and number of columns
	virtual ~PartialRelation ();

	virtual StringValue *GetColumnName (int i);									// returns an array of Tuples representing the column names
	virtual int GetNumberOfColumns ();									// returns the number of columns
	int GetColumnNumber (const char* columnName);						// returns the column number corresponding to the name
	int GetColumnNumber (uint32 columnID);								// returns the column number corresponding to the ID
	
	CSSM_DB_ATTRIBUTE_FORMAT GetColumnFormat (int i) {return mColumnInfo[i].mColumnFormat;} // returns the format of a column
	uint32 GetColumnIDs (int i);											// gets the column id's
};



#endif
