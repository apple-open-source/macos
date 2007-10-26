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

#ifndef __DIRECTORY_SERVICES__
#define __DIRECTORY_SERVICES__


/*
	DirectoryServices.h
	
	These classes represent underlying Open Directory data structures and calls.  See the Open Directory documentation
	for additional information
*/



#include <DirectoryService/DirectoryService.h>
#include <list>
#include <vector>
#include <string>



class DirectoryServiceException
{
protected:
	long mResult;

public:
	DirectoryServiceException (long result) : mResult (result) {}
	
	long GetResult () {return mResult;}
};



class DSNode;
class DSNodeList;
class DirectoryService;


class DSBuffer
{
protected:
	DirectoryService& mDirectoryService;
	tDataBufferPtr mBuffer;
	unsigned long mFinger;

public:
	DSBuffer (DirectoryService &ds, unsigned long size);
	~DSBuffer ();
	void AppendBytes (const void* bytes, unsigned long size);
	operator tDataBufferPtr () {return mBuffer;}
};



class DSRecordList;
class DSAttributeValueEntry;

class DSAttributeEntry
{
protected:
	DirectoryService &mDirectoryService;
	tAttributeEntry *mAttributeEntry;
	tAttributeValueListRef mAttributeValueListRef;
	DSNode *mDataNode;
	DSRecordList *mRecordList;

	friend class DSRecordInternal;

	void Clear ();

public:
	DSAttributeEntry (DirectoryService &ds) : mDirectoryService (ds), mAttributeEntry (NULL), mAttributeValueListRef (0) {}
	~DSAttributeEntry ();
	
	unsigned long GetValueCount () {return mAttributeEntry->fAttributeValueCount;}
	void GetAttributeValue (int which, DSAttributeValueEntry &value);
	std::string GetSignature () {return std::string (mAttributeEntry->fAttributeSignature.fBufferData, mAttributeEntry->fAttributeSignature.fBufferLength);}
};



class DSRecordInternal
{
protected:
	DirectoryService &mDirectoryService;
	DSNode &mDirNode;
	DSRecordList &mRecordList;
	tAttributeListRef mAttributes;
	tRecordEntryPtr mEntry;

public:
	DSRecordInternal (DirectoryService &ds, DSNode &node, DSRecordList &recordList, tAttributeListRef attributeList, tRecordEntryPtr recordEntry) :
					  mDirectoryService (ds), mDirNode (node), mRecordList (recordList), mAttributes (attributeList), mEntry (recordEntry) {}

	~DSRecordInternal ();

	void GetAttributeEntry (int which, DSAttributeEntry &attributeInfo);
	unsigned long GetAttributeCount () {return mEntry->fRecordAttributeCount;}
	std::string GetRecordName ();
	DSNode& GetNode () {return mDirNode;}
	DirectoryService& GetDirectoryService () {return mDirectoryService;}
};

	
	
class DSAttributeValueEntry
{
protected:
	DirectoryService &mDirectoryService;
	tAttributeValueEntry* mValueEntry;
	friend class DSRecordInternal;
	friend class DSAttributeEntry;

public:
	DSAttributeValueEntry (DirectoryService &ds) : mDirectoryService (ds), mValueEntry (NULL) {}
	~DSAttributeValueEntry ();
	
	unsigned long GetAttributeID () {return mValueEntry->fAttributeValueID;}
	char* GetData () {return mValueEntry->fAttributeValueData.fBufferData;}
	unsigned long GetLength () {return mValueEntry->fAttributeValueData.fBufferLength;}
};



class DSRecord
{
protected:
	friend class DSRecordList;
	DSRecordInternal *mInternalRecord;

public:
	DSRecord () : mInternalRecord (NULL) {}
	~DSRecord () {if (mInternalRecord != NULL) delete mInternalRecord;}
	void GetAttributeEntry (int which, DSAttributeEntry &attributeInfo) {mInternalRecord->GetAttributeEntry (which, attributeInfo);}
	unsigned long GetAttributeCount () {return mInternalRecord->GetAttributeCount ();}
	std::string GetRecordName () {return mInternalRecord->GetRecordName ();}
	DSNode& GetNode () {return mInternalRecord->GetNode ();}
};



class DSRecordList : public DSBuffer
{
protected:
	DSNode &mNode;

public:
	DSRecordList (DirectoryService &ds, DSNode &node, unsigned long size) : DSBuffer (ds, size), mNode (node) {}
	
	void GetRecordEntry (int which, DSRecord &record);
};



class DSDataNode
{
protected:
	DirectoryService &mDirectoryService;
	tDataNodePtr mNodePtr;

public:
	DSDataNode (DirectoryService& ds, const char* string);
	DSDataNode (DirectoryService& ds, void* data, size_t length);
	~DSDataNode ();

	operator tDataNodePtr () {return mNodePtr;}
};



class DSContext
{
protected:
	tContextData mContext;
	tDirReference mReference;

public:
	DSContext () : mContext (NULL), mReference (0) {}
	~DSContext ();
	
	operator tContextData* () {return &mContext;}
	void SetReference (tDirReference ref) {mReference = ref;}
	bool Empty () {return mContext == NULL;}
};



class DSDataList
{
protected:
	DirectoryService& mDirectoryService;
	tDataListPtr mDataList;

public:
	DSDataList (DirectoryService& ds) : mDirectoryService (ds), mDataList (NULL) {}
	~DSDataList ();
	
	operator tDataListPtr* () {return &mDataList;}
	operator tDataListPtr () {return mDataList;}
	
	void BuildFromPath (const char* path, const char* separator = "/");
	void BuildFromStrings (const char* string1, ...);
	void AppendString (const char* string);
};



class DSNode
{
protected:
	tDirNodeReference mNodeReference;
	std::string mNodeName;
	DirectoryService& mDirectoryService;

	void CloneDSNode (const DSNode& node);

public:
	DSNode (DirectoryService &ds);
	DSNode (const DSNode& node);
	~DSNode ();
	void operator = (const DSNode& node);

	operator tDirNodeReference () {return mNodeReference;}

	void SetNodeName (const std::string& name);
	const std::string& GetNodeName ();

	void Connect ();
	void Disconnect ();
	void NativeAuthentication (const char* userName, const char* password);
	void GetRecordList (DSRecordList &recordList, DSDataList &recordNameList, tDirPatternMatch patternMatch,
						DSDataList &recordTypeList, DSDataList &attributeTypeList, dsBool attributeInfoOnly,
						unsigned long &recordCount, DSContext &context);
	void Search (DSRecordList &recordList, DSDataList &recordTypeList, DSDataNode &attributeType,
				 tDirPatternMatch patternMatch, DSDataNode &patternToMatch, unsigned long &recordCount,
				 DSContext &context);
};



class DSNodeList
{
protected:
	std::vector<DSNode> mNodeList;

public:
	DSNodeList () {}
	~DSNodeList () {}
	
	void push_back (const DSNode& node);
	
	size_t size ();
	DSNode& operator[] (int n);
};




class DSNodeName : public DSDataList
{
protected:
	char* mNodeName;

public:
	DSNodeName (DirectoryService &ds) : DSDataList (ds),  mNodeName (NULL) {}
	~DSNodeName ();
	
	char* GetPath (const char* separator = "/");
};



class DirectoryService
{
protected:
	tDirReference mDirReference;
	DSNodeList mNodeList;

	void CreateNodeList (const char* nodeListFilter);

public:
	DirectoryService (const char* nodeListFilter);
	~DirectoryService ();

	DSNodeList& GetNodeList ();
	operator tDirReference () {return mDirReference;}
};




#endif
