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

#include "PartialRelation.h"
#include "TableRelation.h"
#include "CommonCode.h"


PartialRelation::PartialRelation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns, columnInfoLoader *theColumnInfo)  :
	Relation (recordType), mNumberOfColumns (numberOfColumns)
{
	if (mNumberOfColumns == 0) {
		mColumnInfo = NULL;
		return;
	}
	
	mColumnInfo = new columnInfo[mNumberOfColumns];
	for (int i = 0; i < mNumberOfColumns; ++i) {
		mColumnInfo[i].mColumnName = new StringValue (theColumnInfo[i].mColumnName);
		mColumnInfo[i].mColumnID = theColumnInfo[i].mColumnID;
		mColumnInfo[i].mColumnFormat = theColumnInfo[i].mColumnFormat;
	}
}



PartialRelation::~PartialRelation ()
{
	if (mColumnInfo != NULL) {
		for (int i = 0; i < mNumberOfColumns; ++i)
			delete mColumnInfo[i].mColumnName;		
		delete mColumnInfo;
	}
}


StringValue *PartialRelation::GetColumnName (int i)
{
	return mColumnInfo[i].mColumnName;
}


int PartialRelation::GetNumberOfColumns ()
{
	return mNumberOfColumns;
}



uint32 PartialRelation::GetColumnIDs (int i)
{
	return mColumnInfo[i].mColumnID;
}



int PartialRelation::GetColumnNumber (const char* columnName)
{
	// look for a column name that matches this columnName.  If not, throw an exception
 	for (int i = 0; i < mNumberOfColumns; ++i) {
		const char *s = mColumnInfo[i].mColumnName->GetRawValue();
		if (strncmp(s, columnName, strlen(s)) == 0)
 			return i;
 	}
	return -1;
}



int PartialRelation::GetColumnNumber (uint32 columnID)
{
	for (int i = 0; i < mNumberOfColumns; ++i) {
		if (mColumnInfo[i].mColumnID == columnID)
			return i;
	}
	return -1;
}



