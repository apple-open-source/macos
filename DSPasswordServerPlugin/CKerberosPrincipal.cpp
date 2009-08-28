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

#include "CKerberosPrincipal.h"
#include "KerberosInterface.h"
#include "PSUtilitiesDefs.h"

#include <syslog.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILE_HEADER "kdb5_util load_dump version 5\n"

PWSFKerberosPrincipalList::PWSFKerberosPrincipalList()
	: mBuffer(NULL), mPrincipalList(0), mPrincipalArray(0), mPrincipalArrayCount(0)
{
}

PWSFKerberosPrincipalList::~PWSFKerberosPrincipalList()
{
	while (mPrincipalList != NULL)
		delete mPrincipalList;
	
	if (mBuffer != NULL)
		free(mBuffer);
	
	if ( mPrincipalArray != NULL )
		free( mPrincipalArray );
}


//----------------------------------------------------------------------------------------------------
//	ReadAllPrincipalsFromDB
//
//	Returns: 0=ok, -1=err
//
//	<inAfterThisModDate>	 ->		0=read all records, or toss records earlier than the time.
//	<inMaxPrincipals>		 ->		0=no limit, or stop reading when linked list hits this count.
//----------------------------------------------------------------------------------------------------

int PWSFKerberosPrincipalList::ReadAllPrincipalsFromDB(time_t inAfterThisModDate, long inMaxPrincipals)
{
	PWSFKerberosPrincipal* current;
	char* ptr;
	const char* args[5];
	size_t result;
	long princCount = 0;
	struct stat sb;
	size_t dumpLen = 0;
	char tempFileName[50];
	int fd;
	
	strcpy(tempFileName, "/var/db/krb5kdc/KerbDumpFileXXXXX");
	mktemp(tempFileName);

	args[0] = "kdb5_util";
	args[1] = "dump";
	args[2] = (const char *)tempFileName;
	args[3] = NULL;

	result = pwsf_LaunchTaskWithIO(kKDBUtilLocalFilePath, (char * const *)args, NULL, NULL, 0, NULL);
	result = lstat( tempFileName, &sb );
	if ( result == 0 )
		dumpLen = (size_t)sb.st_size;
	
	if ( (result != 0) || (dumpLen <= (long)sizeof(FILE_HEADER)) )
	{
		unlink(tempFileName);
		return -1;
	}
	
	mBuffer = (char*)malloc(dumpLen + 1);
	if ( mBuffer == NULL ) {
		return -1;
	}
	
	fd = open(tempFileName, O_RDONLY);
	if ( fd == -1 ) {
		free( mBuffer );
		mBuffer = NULL;
		return -1;
	}
	result = read(fd, mBuffer, dumpLen);
	close(fd);
	unlink(tempFileName);
	strcat(tempFileName, ".dump_ok");
	unlink(tempFileName);
	if (result != dumpLen)
		return -1;
	mBuffer[dumpLen] = 0;
	
	ptr = mBuffer;
	current = new PWSFKerberosPrincipal(ptr, this);
	ptr = current->ParseBuffer(true);
	if (!current->IsPolicy() && (current->GetRecordModDate() < inAfterThisModDate))
		delete current;
	else
		princCount++;

	while (*ptr != 0)
	{
		current = new PWSFKerberosPrincipal(ptr, this);
		ptr = current->ParseBuffer(false);
		
		if (!current->IsPolicy() && (current->GetRecordModDate() < inAfterThisModDate))
			delete current;
		else
			princCount++;
		
		if ( inMaxPrincipals > 0 && princCount >= inMaxPrincipals )
			break;
	}
	
	// build a ptr table for b-search
	mPrincipalArray = (PWSFKerberosPrincipal **) calloc( princCount + 1, sizeof(PWSFKerberosPrincipal *) );
	if ( mPrincipalArray != NULL )
	{
		int32_t index = 0;
		
		for ( current = mPrincipalList; current != NULL && index < princCount; current = current->GetNext() )
			mPrincipalArray[index++] = current;
		
		mPrincipalArrayCount = princCount;
	}
	
	return 0;
}

int PWSFKerberosPrincipalList::WriteAllPrincipalsToDB()
{
	PWSFKerberosPrincipal* current = mPrincipalList;
	char tempFileName[50];
	int status;
	
	if (mPrincipalList == NULL)
		return 0;
	
	strcpy(tempFileName, "/var/db/krb5kdc/KerbLoadFileXXXXX");
	int fd = mkstemp(tempFileName);
	write(fd, FILE_HEADER, sizeof(FILE_HEADER) - 1);
	int count = 0;
	while (current != NULL)
	{
		current->WritePrincipalToTempFile(fd);
		count++;
		current = current->GetNext();
	}
	close(fd);
	
	const char* args[5];
	
	args[0] = "kdb5_util";
	args[1] = "load";
	args[2] = "-update";
	args[3] = (const char *)tempFileName;
	args[4] = NULL;

	status = pwsf_LaunchTaskWithIO(kKDBUtilLocalFilePath, (char * const *)args, NULL, NULL, 0, NULL);
	unlink(tempFileName);

	return status;
}


PWSFKerberosPrincipal* PWSFKerberosPrincipalList::GetPrincipalByName(char* principalName)
{
	PWSFKerberosPrincipal* current = mPrincipalList;
	char *name = NULL;
	char firstChar;
	long index = 0;
	long upper = 0;
	long lower = 0;
	long delta = 0;
	int deltaHitZero = 0;
	int compare = 0;
	
	if (principalName == NULL || *principalName == '\0')
		return NULL;
	
	firstChar = *principalName;

	if ( mPrincipalArray != NULL )
	{
		lower = 0;
		upper = mPrincipalArrayCount - 1;
		
		index = ((upper - lower) / 2);
		do
		{
			current = mPrincipalArray[index];
			if ( current != NULL )
				name = current->GetName();
			if ( name != NULL )
			{
				compare = strcmp( name, principalName );
				if ( compare == 0 )
					return current;
				else
				if ( compare < 0 ) {
					upper = index;
					delta = ((upper - lower) / 2);
					index -= MAX(delta, 1);
				}
				else
				if ( compare > 0 ) {
					lower = index;
					delta = ((upper - lower) / 2);
					index += MAX(delta, 1);
				}
				
				if ( delta == 0 )
				{
					if ( deltaHitZero )
						return NULL;
					else
						deltaHitZero = 1;
				}
			}
		}
		while ( index >= 0 && index < mPrincipalArrayCount );
		
		return NULL;
	}
	else
	{
		while (current != NULL)
		{
			name = current->GetName();
			if (name != NULL && *name == firstChar && strcmp(name + 1, principalName + 1) == 0)
				break;
			current = current->GetNext();
		}
	}
	
	return current;
}


PWSFKerberosPrincipal* PWSFKerberosPrincipalList::GetPrincipalByIndex(int index)
{
	int i;
	PWSFKerberosPrincipal* current = mPrincipalList;
	
	for (i = 0; (i < index) && (current != NULL); i++)
		current = current->GetNext();
		
	return current;
}

PWSFKerberosPrincipal* PWSFKerberosPrincipalList::ReadPrincipalFromFile(FILE* file, int recordSize)
{
	return PWSFKerberosPrincipal::ReadPrincipalFromFile(file, recordSize, this);
}

PWSFKerberosPrincipal* PWSFKerberosPrincipalList::ReadPrincipalFromData(unsigned char *record, int recordSize)
{
	return PWSFKerberosPrincipal::ReadPrincipalFromData(record, recordSize, this);
}


PWSFKerberosPrincipal::PWSFKerberosPrincipal()
:	mBuffer(NULL), mOtherBuffer(NULL), mOwnsBuffer(true), mBufSize(0), mListOwner(NULL), mNext(NULL), mPrevious(NULL)
{
	bzero( mEntries, sizeof(mEntries) );	
}

PWSFKerberosPrincipal::PWSFKerberosPrincipal(char* buffer, PWSFKerberosPrincipalList* owner)
:	mBuffer(buffer), mOtherBuffer(NULL), mOwnsBuffer(false), mBufSize(0), mListOwner(owner), mNext(NULL), mPrevious(NULL)
{
	bzero( mEntries, sizeof(mEntries) );
	
	mNext = owner->GetPrincipalByIndex(0);
	if (mNext != NULL)
		mNext->mPrevious = this;
	owner->SetFirst(this);
}

PWSFKerberosPrincipal::PWSFKerberosPrincipal(PWSFKerberosPrincipalList* owner)
:	mBuffer(NULL), mOtherBuffer(NULL), mOwnsBuffer(true), mBufSize(0), mListOwner(owner), mNext(NULL), mPrevious(NULL)
{
	bzero( mEntries, sizeof(mEntries) );
	
	mNext = owner->GetPrincipalByIndex(0);
	if (mNext != NULL)
		mNext->mPrevious = this;
	owner->SetFirst(this);
}

PWSFKerberosPrincipal::~PWSFKerberosPrincipal()
{
	if (mBuffer != NULL && mOwnsBuffer)
		free(mBuffer);
		
	if (mListOwner != NULL)
	{
		if (mNext != NULL)
			mNext->mPrevious = mPrevious;
		
		if (mPrevious == NULL)
			mListOwner->SetFirst(mNext);
		else
			mPrevious->mNext = mNext;
	}
	
	if ( mOtherBuffer != NULL )
	{
		free( mOtherBuffer );
		mOtherBuffer = NULL;
	}
}

bool PWSFKerberosPrincipal::IsPolicy()
{
	char *entryZero;
	
	if ( (entryZero = mEntries[0]) == NULL )
		return false;
	
	// There typically aren't that many policies in the db
	// so it's worth optimizing the compare for the 'princ' case.
	return ( (*((unsigned long *)entryZero) == 'poli') &&
			 (strcmp(entryZero, "policy") == 0) );
}

char* PWSFKerberosPrincipal::GetName()
{
	if (IsPolicy())
		return mEntries[1];
	else
		return mEntries[6];
}

void PWSFKerberosPrincipal::CopyLastLogin(PWSFKerberosPrincipal* otherRecord)
{
	mEntries[12] = GetEntryCopy(otherRecord, 12);
}

int PWSFKerberosPrincipal::GetFirstKeyEntryIndex()
{
	if (IsPolicy())
		return -1;
	
	if ( mEntries[3] == NULL )
		return -1;
	
	int numTags = (int)strtol(mEntries[3], NULL, 16);
	int theIndex = 15 + (3 * numTags);
	
	// need some kind of safety
	if ( theIndex > kKerberosPrincipalMaxEntries )
		theIndex = -1;
	
	return theIndex;
}


int PWSFKerberosPrincipal::GetPasswordModDateEntryIndex()
{
	if (IsPolicy())
		return -1;
	
	if ( mEntries[3] == NULL )
		return -1;
	
	int numTags = (int)strtol(mEntries[3], NULL, 16);
		
	if ( 15 + (3 * (numTags-1)) > kKerberosPrincipalMaxEntries )
		return -1;
	
	for (int i = 0; i < numTags; i++)
	{
		if (mEntries[15 + (3 * i)] == NULL)
			return -1;
			
		if (strtol(mEntries[15 + (3 * i)], NULL, 16) == 1)
			return (15 + (3 * i) + 2);
	}
	
	return -1;
}

time_t PWSFKerberosPrincipal::GetRecordModDate()
{
	if (IsPolicy())
		return 0;
	
	if ( mEntries[3] == NULL )
		return 0;
	
	int numTags = (int)strtol(mEntries[3], NULL, 16);
	int i;
	int index = 0;
	time_t modDate = 0;
	
	if ( 15 + (3 * (numTags-1)) > kKerberosPrincipalMaxEntries )
		return 0;
	
	for (i = 0; i < numTags; i++)
	{
		if (mEntries[15 + (3 * i)] == NULL)
			return 0;
		
		if (strtol(mEntries[15 + (3 * i)], NULL, 16) == 2)
		{
			index = (15 + (3 * i) + 2);
			break;
		}
	}
	
	if (index != 0)
	{
		char* modDateString = mEntries[index];
		for (i = 6; i >= 0; i-=2)
		{
			int hiNibble = (modDateString[i] >= 'a') ? modDateString[i] - 'a' + 10 : modDateString[i] - '0';
			int loNibble = (modDateString[i+1] >= 'a') ? modDateString[i+1] - 'a' + 10 : modDateString[i+1] - '0';
			modDate = (modDate << 8) + (hiNibble << 4) + loNibble;
		}
	}
	
	return modDate;
}

void PWSFKerberosPrincipal::CopyPassword(PWSFKerberosPrincipal* otherRecord)
{
	int modDateIndex = GetPasswordModDateEntryIndex();
	int otherModDateIndex = otherRecord->GetPasswordModDateEntryIndex();
	
	// copy the password changed date
	if ((modDateIndex != -1) && (otherModDateIndex != -1))
			mEntries[modDateIndex] = GetEntryCopy(otherRecord, otherModDateIndex);
			
	// copy all of the key hash data
	int index = GetFirstKeyEntryIndex();
	int otherIndex = otherRecord->GetFirstKeyEntryIndex();
	
	if ( index != -1 && otherIndex != -1 )
	{
		do
		{
			mEntries[index++] = GetEntryCopy(otherRecord, otherIndex);
		} while (otherRecord->mEntries[otherIndex++] != NULL);
	}
	
	// copy the number of keys
	mEntries[4] = GetEntryCopy(otherRecord, 4);
}

char* PWSFKerberosPrincipal::ParseBuffer(bool skipLine)
{
	char* cur = mBuffer;
	int curEntry;

	if ( mBuffer == NULL )
		return NULL;
	
	if (skipLine)
	{
		// skip first line
		while (*cur != '\n') {
			if (*cur == '\0')
				return NULL;
			cur++;
		}
		cur++;
	}
	
	mEntries[0] = cur;
	curEntry = 1;
	while (*cur != '\n' && *cur != '\0')
	{
		if (*cur == '\t')
		{
			*cur = '\0';
			cur++;
			mEntries[curEntry++] = cur;
		}
		
		cur++;
	}
	
	mEntries[curEntry] = NULL;
	
	if (*cur == '\0')
		return cur;
	
	*cur = '\0';
	if (mBufSize == 0)
		mBufSize = cur - mBuffer;
	
	return cur+1;
}

int PWSFKerberosPrincipal::GetPrincipalData(unsigned char **outData, size_t *outDataLen)
{
	int i;
	size_t len = 0;
	size_t datalen = 0;
	unsigned char *dataPtr = NULL;
	unsigned char *tptr = NULL;
	
	// first calculate length
	datalen += (sizeof(FILE_HEADER) - 1);
	i = 0;
	while (mEntries[i] != NULL)
		datalen += strlen(mEntries[i++]) + 1;
	
	dataPtr = (unsigned char *) malloc( datalen );
	if ( dataPtr == NULL )
		return 0;
	
	tptr = dataPtr;
	memcpy(tptr, FILE_HEADER, sizeof(FILE_HEADER) - 1);
	tptr += sizeof(FILE_HEADER) - 1;
	
	i = 0;
	len = strlen(mEntries[i]);
	memcpy(tptr, mEntries[i], len);
	tptr += len;
	
	i++;
	while(mEntries[i] != NULL)
	{
		*tptr++ = '\t';
		len = strlen(mEntries[i]);
		memcpy(tptr, mEntries[i], len);
		tptr += len;
		i++;
	}
	*tptr++ = '\n';
	
	*outData = dataPtr;
	*outDataLen = datalen;
	
//	REPLOG1("Wrote principal %s to File", GetName());
	return 1;
}

int PWSFKerberosPrincipal::WritePrincipalToFile(FILE* file)
{
	int i;
	int len = 0;
	
	// first calculate length
	len += (int)(sizeof(FILE_HEADER) - 1);
	i = 0;
	while(mEntries[i] != NULL)
		len += (int)strlen(mEntries[i++]) + 1;
	
	len = htonl(len);
	i = (int)fwrite(&len, sizeof(len), 1, file);
	if (i != 1)
		return i;
	
	fprintf(file, FILE_HEADER);
	
	i = 0;
	fprintf(file, "%s", mEntries[i++]);
	while(mEntries[i] != NULL)
	{
		fprintf(file, "\t%s", mEntries[i++]);
	}
	fprintf(file, "\n");
	
	return 1;
}

int PWSFKerberosPrincipal::WritePrincipalToTempFile(int fd)
{
	size_t len = 0;
	int i;
	
	// first calculate length
	i = 0;
	while (mEntries[i] != NULL)
		len += strlen(mEntries[i++]) + 1;
	
	if (len == 0)
		return 0;
	
	char* outBuf = (char*)malloc(len + 1);
	char* cur = outBuf;
	memcpy(cur, mEntries[0], strlen(mEntries[0]));
	cur += strlen(mEntries[0]);
	i = 1;
	while(mEntries[i] != NULL)
	{
		*cur++ = '\t';
		memcpy(cur, mEntries[i], strlen(mEntries[i]));
		cur += strlen(mEntries[i++]);
	}
	*cur++ = '\n';
	*cur = '\0';
	
	write(fd, outBuf, strlen(outBuf));
	
	free(outBuf);

	return 1;
}

void PWSFKerberosPrincipal::WritePrincipalToDB()
{
	char tempFileName[50];
	
	strcpy(tempFileName, "/var/db/krb5kdc/KerbLoadFileXXXXX");
	int fd = mkstemp(tempFileName);
	write(fd, FILE_HEADER, sizeof(FILE_HEADER) - 1);
	WritePrincipalToTempFile(fd);
	close(fd);
	
	const char* args[5];
	
	args[0] = "kdb5_util";
	args[1] = "load";
	args[2] = "-update";
	args[3] = (const char *)tempFileName;
	args[4] = NULL;

	pwsf_LaunchTaskWithIO(kKDBUtilLocalFilePath, (char * const *)args, NULL, NULL, 0, NULL);
	unlink(tempFileName);
}


int PWSFKerberosPrincipal::ReadPrincipalFromDB(char* principalName, PWSFKerberosPrincipal** outKerbPrinc)
{
	PWSFKerberosPrincipal* reply = new PWSFKerberosPrincipal;
	const char* args[5];
	int result = 0;
	
	if ( principalName == NULL || principalName[0] == '\0' )
		return -1;
	
	if ( outKerbPrinc == NULL )
		return -1;
	*outKerbPrinc = NULL;
	
	reply->mBuffer = (char*)malloc(2001);
	if ( reply->mBuffer == NULL )
		return -1;
	
	reply->mBufSize = 2000;
	
	args[0] = "kdb5_util";
	args[1] = "dump";
	args[2] = "-";
	args[3] = (const char *)principalName;
	args[4] = NULL;

	result = pwsf_LaunchTaskWithIO(kKDBUtilLocalFilePath, (char * const *)args, NULL, reply->mBuffer, reply->mBufSize, NULL);
	if ( (result == 0) && (strlen(reply->mBuffer) > sizeof(FILE_HEADER)) )
	{
		reply->ParseBuffer(true);
	}
	else
	{
		delete reply;
		return result;
	}
	
	if ((reply->GetName() == NULL) || (strcmp(reply->GetName(), principalName) != 0))
	{
		// this is to fix a problem where policies are always dumped even if princ doesn't exist
		delete reply;
		reply = NULL;
	}
	
	*outKerbPrinc = reply;
	
	return result;
}

PWSFKerberosPrincipal* PWSFKerberosPrincipal::ReadPrincipalFromFile(FILE* file, int recordSize, PWSFKerberosPrincipalList* listOwner)
{
	PWSFKerberosPrincipal* reply;
	size_t readCount;
	
	// sanity check -- principals are typically ~300 bytes.
	// if the record size is greater than 50K, then the sync
	// file is probably bad.
	if ( recordSize < 0 || recordSize > 50 * 1024 )
		return NULL;
	
	if (listOwner != NULL)
		reply = new PWSFKerberosPrincipal(listOwner);
	else
		reply = new PWSFKerberosPrincipal;
	
	if ( reply != NULL )
	{
		reply->mBufSize = recordSize;
		reply->mBuffer = (char*)malloc(reply->mBufSize + 1);
		if ( reply->mBuffer == NULL ) {
			delete reply;
			return NULL;
		}
		readCount = fread( reply->mBuffer, reply->mBufSize, 1, file );
		if ( readCount != 1 ) {
			delete reply;
			return NULL;
		}
		reply->mBuffer[reply->mBufSize] = '\0';
		reply->ParseBuffer( true );
	}
	
	return reply;
}

PWSFKerberosPrincipal *
PWSFKerberosPrincipal::ReadPrincipalFromData(unsigned char *record, int recordSize, PWSFKerberosPrincipalList* listOwner)
{
	PWSFKerberosPrincipal* reply;
	
	// sanity check -- principals are typically ~300 bytes.
	// if the record size is greater than 50K, then the sync
	// file is probably bad.
	if ( recordSize < 0 || recordSize > 50 * 1024 ) {
		//ERRLOG1( kLogMeta, "Discarded attempt to read long principal from file (length = %d)", recordSize);
		return NULL;
	}
	
	if ( listOwner != NULL )
		reply = new PWSFKerberosPrincipal( listOwner );
	else
		reply = new PWSFKerberosPrincipal;
	
	if ( reply != NULL )
	{
		reply->mBufSize = recordSize;
		reply->mBuffer = (char*)malloc(reply->mBufSize + 1);
		if ( reply->mBuffer == NULL ) {
			delete reply;
			return NULL;
		}
		memcpy( reply->mBuffer, record, recordSize );
		reply->mBuffer[reply->mBufSize] = '\0';
		reply->ParseBuffer( true );
	}
	
	return reply;
}

char* PWSFKerberosPrincipal::GetEntryCopy(PWSFKerberosPrincipal* otherRecord, int index)
{
	if (otherRecord == NULL || otherRecord->mEntries[index] == NULL)
		return NULL;
	
	// if the buffer exists, make sure it is big enough
	if ( mOtherBuffer != NULL && mOtherBufSize < otherRecord->mBufSize + 1 )
	{
		free( mOtherBuffer );
		mOtherBuffer = NULL;
	}
	if ( mOtherBuffer == NULL )
	{
		mOtherBufSize = otherRecord->mBufSize + 1;
		mOtherBuffer = (char *) malloc( mOtherBufSize );
	}
	if ( mOtherBuffer == NULL )
		return NULL;
	
	memcpy( mOtherBuffer, otherRecord->mBuffer, otherRecord->mBufSize + 1 );
	
	int offset = (int)(otherRecord->mEntries[index] - otherRecord->mBuffer);
	return mOtherBuffer + offset;
}
