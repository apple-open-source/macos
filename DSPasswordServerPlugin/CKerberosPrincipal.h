/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#define kKerberosPrincipalMaxEntries		200

class PWSFKerberosPrincipal;

class PWSFKerberosPrincipalList
{
public:
	PWSFKerberosPrincipalList();
	virtual ~PWSFKerberosPrincipalList();
	
	int ReadAllPrincipalsFromDB(time_t inAfterThisModDate, long inMaxPrincipals = 0);
	int WriteAllPrincipalsToDB();
	
	PWSFKerberosPrincipal* GetPrincipalByName(char* principalName);
	PWSFKerberosPrincipal* GetPrincipalByIndex(int index);
	PWSFKerberosPrincipal* ReadPrincipalFromFile(FILE* file, int recordSize);
	PWSFKerberosPrincipal* ReadPrincipalFromData(unsigned char *record, int recordSize);
	
	void SetFirst(PWSFKerberosPrincipal* listHead) { mPrincipalList = listHead; }
	
private:
	char* mBuffer;
	
	PWSFKerberosPrincipal* mPrincipalList;
	PWSFKerberosPrincipal** mPrincipalArray;
	long mPrincipalArrayCount;
};

class PWSFKerberosPrincipal
{
public:
	PWSFKerberosPrincipal();
	PWSFKerberosPrincipal(char* buffer, PWSFKerberosPrincipalList* owner);
	PWSFKerberosPrincipal(PWSFKerberosPrincipalList* owner);
	~PWSFKerberosPrincipal();
	
	char* GetName();

	void SetLastLogin(time_t time);
	void CopyPassword(PWSFKerberosPrincipal* otherRecord);
	void CopyLastLogin(PWSFKerberosPrincipal* otherRecord);
	
	int GetFirstKeyEntryIndex();
	int GetPasswordModDateEntryIndex();
	
	time_t GetRecordModDate();
	
	int GetPrincipalData(unsigned char **outData, size_t *outDataLen);
	int WritePrincipalToFile(FILE* file);
	int WritePrincipalToTempFile(int fd);
	void WritePrincipalToDB();
	
//	int GetNameHashSlot();
	
	static int ReadPrincipalFromDB(char* principalName, PWSFKerberosPrincipal** outKerbPrinc);
	static PWSFKerberosPrincipal* ReadPrincipalFromFile(FILE* file, int recordSize, PWSFKerberosPrincipalList* list = NULL);
	static PWSFKerberosPrincipal* ReadPrincipalFromData(unsigned char *record, int recordSize, PWSFKerberosPrincipalList* list = NULL);
	
	char* ParseBuffer(bool skipLine);
	
	bool IsPolicy();
	
	PWSFKerberosPrincipal* GetNext() { return mNext; }

private:
	char* GetEntryCopy(PWSFKerberosPrincipal* otherRecord, int index);
	
	char* mBuffer;
	char* mOtherBuffer;
	bool mOwnsBuffer;
	size_t mBufSize;
	size_t mOtherBufSize;
	char* mEntries[kKerberosPrincipalMaxEntries];
	
	PWSFKerberosPrincipalList* mListOwner;
	PWSFKerberosPrincipal* mNext;
	PWSFKerberosPrincipal* mPrevious;
//	PWSFKerberosPrincipal* mHashNext;
};
