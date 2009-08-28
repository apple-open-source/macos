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



/*
	PartialRelation.h
	
	This class provides common support for writing relations.
*/

class PartialRelation : public Relation
{
protected:
	int mNumberOfColumns;												// number of columns (attributes) this relation supports
	Value** mColumnNames;												// names of these columns
	CSSM_DB_ATTRIBUTE_FORMAT* mColumnFormat;							// formats of these columns
	uint32* mColumnIDs;													// I.D.'s for these columns

public:
	PartialRelation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns);
																		// pass in the relation ID and number of columns
	virtual ~PartialRelation ();

	virtual Tuple* GetColumnNames ();									// returns an array of Tuples representing the column names
	virtual int GetNumberOfColumns ();									// returns the number of columns
	int GetColumnNumber (const char* columnName);						// returns the column number corresponding to the name
	int GetColumnNumber (uint32 columnID);								// returns the column number corresponding to the ID
	CSSM_DB_ATTRIBUTE_FORMAT GetColumnFormat (int i) {return mColumnFormat[i];}
																		// returns the format of a column
	
	void SetColumnNames (const char* column0, ...);							// sets the names of columns.
	void SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT column0, ...);		// set the formats of columns.
	void SetColumnIDs (uint32 column0, ...);							// set the column id's
	uint32* GetColumnIDs ();											// gets the column id's
};



#endif
