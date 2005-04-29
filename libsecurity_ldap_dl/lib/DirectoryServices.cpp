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

#include <stdarg.h>
#include "DirectoryServices.h"
#include <string.h>



static void CheckResult (long result)
{
	if (result != eDSNoErr)
	{
		throw DirectoryServiceException (result);
	}
}



DSBuffer::DSBuffer (DirectoryService& ds, unsigned long size) : mDirectoryService (ds)
{
	mBuffer = dsDataBufferAllocate (ds, size);
	mFinger = 0;
}



void DSBuffer::AppendBytes (const void* bytes, unsigned long size)
{
	u_int8_t* data = (u_int8_t*) bytes;
	
	memcpy (mBuffer->fBufferData + mBuffer->fBufferLength, data, size);
	mBuffer->fBufferLength += size;
}



DSBuffer::~DSBuffer ()
{
	dsDataBufferDeAllocate (mDirectoryService, mBuffer);
}



void DSAttributeEntry::Clear ()
{
	if (mAttributeEntry)
	{
		dsDeallocAttributeEntry (mDirectoryService, mAttributeEntry);
	}
	
	if (mAttributeValueListRef)
	{
		dsCloseAttributeValueList (mAttributeValueListRef);
	}
}



DSAttributeEntry::~DSAttributeEntry ()
{
	Clear ();
}



void DSAttributeEntry::GetAttributeValue (int which, DSAttributeValueEntry &value)
{
	tDirStatus result = dsGetAttributeValue (*mDataNode, *mRecordList, which, mAttributeValueListRef, &value.mValueEntry);
	CheckResult (result);
}



DSAttributeValueEntry::~DSAttributeValueEntry () {if (mValueEntry != NULL) dsDeallocAttributeValueEntry (mDirectoryService, mValueEntry);}



DSRecordInternal::~DSRecordInternal ()
{
	dsCloseAttributeValueList (mAttributes);
	dsDeallocRecordEntry (mDirectoryService, mEntry);
}



void DSRecordInternal::GetAttributeEntry (int which, DSAttributeEntry &attributeInfo)
{
	attributeInfo.mDataNode = &mDirNode;
	attributeInfo.mRecordList = &mRecordList;
	attributeInfo.Clear ();
	tDirStatus result = dsGetAttributeEntry (mDirNode, mRecordList, mAttributes, which, &attributeInfo.mAttributeValueListRef, &attributeInfo.mAttributeEntry);
	CheckResult (result);
}



std::string DSRecordInternal::GetRecordName ()
{
	char* outName;
	tDirStatus result = dsGetRecordNameFromEntry (mEntry, &outName);
	CheckResult (result);
	
	std::string strResult (outName);
	free (outName);
	return strResult;
}



void DSRecordList::GetRecordEntry (int which, DSRecord &record)
{
	tRecordEntryPtr recordEntryPtr;
	tAttributeListRef attributeListRef = 0;

	tDirStatus result = dsGetRecordEntry (mDirectoryService, mBuffer, which, &attributeListRef, &recordEntryPtr);
	CheckResult (result);
	
	if (record.mInternalRecord != NULL) // is there an exsisting record?
	{
		delete record.mInternalRecord;
	}
	
	record.mInternalRecord = new DSRecordInternal (mDirectoryService, mNode, *this, attributeListRef, recordEntryPtr);
}



DSDataNode::DSDataNode (DirectoryService& ds, const char* string) : mDirectoryService (ds)
{
	mNodePtr = dsDataNodeAllocateString (ds, string);
}



DSDataNode::DSDataNode (DirectoryService& ds, void* data, size_t length) : mDirectoryService (ds)
{
	mNodePtr = dsDataNodeAllocateBlock (ds, length, length, data);
}



DSDataNode::~DSDataNode ()
{
	dsDataNodeDeAllocate (mDirectoryService, mNodePtr);
}



DSNode::DSNode (DirectoryService& ds) : mNodeReference (0), mDirectoryService (ds)
{
}



DSNode::DSNode (const DSNode& node) : mDirectoryService (node.mDirectoryService)
{
	CloneDSNode (node);
}



DSNode::~DSNode ()
{
}



void DSNode::operator = (const DSNode& node)
{
	CloneDSNode (node);
}



void DSNode::CloneDSNode (const DSNode& node)
{
	mDirectoryService = node.mDirectoryService;
	mNodeReference  = node.mNodeReference;
	mNodeName = node.mNodeName;
}



void DSNode::SetNodeName (const std::string& name)
{
	mNodeName = name;
}



const std::string& DSNode::GetNodeName ()
{
	return mNodeName;
}



void DSNode::Connect ()
{
	DSDataList dataList (mDirectoryService);
	dataList.BuildFromPath (mNodeName.c_str ());
	long status = dsOpenDirNode (mDirectoryService, dataList, &mNodeReference);
	CheckResult (status);
}



void DSNode::Disconnect ()
{
	long status = dsCloseDirNode (mNodeReference);
	CheckResult (status);
}



void DSNode::NativeAuthentication (const char* userName, const char* password)
{
	long nameLength = strlen (userName);
	long passwordLength = strlen (password);
	long messageLength = 2 * sizeof (long) + nameLength + passwordLength;
	
	DSDataNode authType (mDirectoryService, kDSStdAuthNodeNativeClearTextOK);
	DSBuffer message (mDirectoryService, messageLength);
	message.AppendBytes (&nameLength, sizeof (nameLength));
	message.AppendBytes (userName, nameLength);
	message.AppendBytes (&passwordLength, sizeof (passwordLength));
	message.AppendBytes (password, passwordLength);
	
	DSBuffer response (mDirectoryService, 512);
	DSContext continueData (mDirectoryService);

	long result = dsDoDirNodeAuth (mDirectoryService, authType, true, message, response, continueData);
	CheckResult (result);
}



void DSNode::GetRecordList (DSRecordList &recordList, DSDataList &recordNameList, tDirPatternMatch patternMatch,
							DSDataList &recordTypeList, DSDataList &attributeTypeList, dsBool attributeInfoOnly,
							unsigned long &recordCount, DSContext &context)
{
	tDirStatus result = dsGetRecordList (mNodeReference, recordList, recordNameList, patternMatch,
										 recordTypeList, attributeTypeList, attributeInfoOnly, &recordCount,
										 context);
	CheckResult (result);
}



void DSNode::Search (DSRecordList &recordList, DSDataList &recordTypeList, DSDataNode &attributeType,
					 tDirPatternMatch patternMatch, DSDataNode &patternToMatch, unsigned long &recordCount,
					 DSContext &context)
{
	tDirStatus result = dsDoAttributeValueSearch (mNodeReference, recordList, recordTypeList, attributeType,
												  patternMatch, patternToMatch, &recordCount, context);
	CheckResult (result);
}



DSNodeName::~DSNodeName ()
{
	if (mNodeName != NULL)
	{
		free (mNodeName);
	}
}



char* DSNodeName::GetPath (const char* separator)
{
	mNodeName = dsGetPathFromList (mDirectoryService, mDataList, separator);
	return mNodeName;
}



DirectoryService::DirectoryService (const char* nodeListFilter) : mDirReference (0)
{
	// make a directory services reference
	long result = dsOpenDirService (&mDirReference);
	CheckResult (result);
	
	CreateNodeList (nodeListFilter);
}



DirectoryService::~DirectoryService ()
{
	if (mDirReference != 0)
	{
		dsCloseDirService (mDirReference);
	}
}



DSNodeList& DirectoryService::GetNodeList ()
{
	return mNodeList;
}



void DirectoryService::CreateNodeList (const char* nodeListFilter)
{
	// prefix with the length of our filter
	int filterLen;
	if (nodeListFilter == NULL)
	{
		filterLen = 0;
	}
	else
	{
		filterLen = strlen (nodeListFilter);
	}
	
	// figure out the number of DSNodes to get
	unsigned long nodeCount;
	long result = dsGetDirNodeCount (mDirReference, &nodeCount);
	if (nodeCount == 0)
	{
		return;
	}
	
	CheckResult (result);
	bool done = false;

	while (!done)
	{
		DSBuffer buffer (*this, 20 * 1024);
		DSContext context (*this);
		
		unsigned long bufferCount;

		result = dsGetDirNodeList (mDirReference, buffer, &bufferCount, context);

		// get the returned nodes
		while (!done)
		{
			unsigned long i;
			for (i = 1; i <= bufferCount; ++i) // weird, ordinal rather than cardinal.
			{
				DSNode n (*this);
				DSNodeName name (*this);
				
				result = dsGetDirNodeName (mDirReference, buffer, i, name);
				CheckResult (result);
				
				// add the node to the list if it matches the filter
				const char* path = name.GetPath ();
				if (filterLen == 0 || strncmp (path, nodeListFilter, filterLen) == 0)
				{
					n.SetNodeName (name.GetPath ());
					mNodeList.push_back (n);
				}
			}
			
			if (context.Empty ())
			{
				done = true;
			}
			else
			{
				result = dsGetDirNodeList (mDirReference, buffer, &bufferCount, context);
				CheckResult (result);
			}
		}
	}
}



void DSNodeList::push_back (const DSNode& node)
{
	mNodeList.push_back (node);
}



size_t DSNodeList::size ()
{
	return mNodeList.size ();
}



DSNode& DSNodeList::operator[] (int n)
{
	return mNodeList[n];
}



DSContext::~DSContext () {if (mContext != NULL) dsReleaseContinueData (mDirectoryService, mContext);}



DSDataList::~DSDataList ()
{
	if (mDataList != NULL)
	{
		dsDataListDeallocate (mDirectoryService, mDataList);
		free (mDataList);
	}
}



void DSDataList::BuildFromPath (const char* path, const char* separator)
{
	mDataList = dsBuildFromPath (mDirectoryService, path, separator);
}



void DSDataList::BuildFromStrings (const char* string1, ...)
{
	va_list args;
	va_start (args, string1);

	mDataList = dsDataListAllocate (mDirectoryService);
	tDirStatus result = dsBuildListFromStringsAllocV (mDirectoryService, mDataList, string1, args);
	CheckResult (result);
}



void DSDataList::AppendString (const char* string)
{
	if (mDataList == NULL)
	{
		mDataList = dsDataListAllocate (mDirectoryService);
	}
	
	tDirStatus result = dsAppendStringToListAlloc (mDirectoryService, mDataList, string);
	CheckResult (result);
}

