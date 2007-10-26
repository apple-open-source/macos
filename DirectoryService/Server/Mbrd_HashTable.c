/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "Mbrd_HashTable.h"
#include <stdlib.h>
#include <strings.h>

HashTable* CreateHash(int offset, int size, long isPtr, int countReference)
{
	HashTable* result = (HashTable*)malloc(sizeof(HashTable));
	if (result == NULL) return NULL;
	
	result->fTable = NULL;
	result->fTableSize = 0;
	result->fNumEntries = 0;
	result->fKeyOffset = offset;
	result->fKeySize = size;
	result->fKeyIsPtr = isPtr;
	result->fCountReference = countReference;
	
	return result;
}

void ReleaseHash(HashTable* hash)
{
	int i;
	
	if (hash == NULL) return;
	
	if (hash->fTable != NULL)
	{
		for (i = 0; i < hash->fTableSize; i++)
		{
			if (hash->fTable[i] != NULL)
				if (hash->fCountReference) hash->fTable[i]->fRefCount--;
		}
		free(hash->fTable);
		hash->fTable = NULL;
	}
	
	free(hash);
}

unsigned int ComputeHash(HashTable* hash, void* key)
{
	unsigned int hashOffset;
	int i;
	unsigned char* keyPtr = (unsigned char*)key;
	int keySize = 0;
	
	if (key == NULL)
		return 0;
	if (hash == NULL)
		return 0;
	
	keySize = hash->fKeySize;
	
	hashOffset = 0;
	if (keySize == 0)
		keySize = strlen((char*)key);
	for (i = 0; i < keySize; i++)
		hashOffset = (hashOffset ^ keyPtr[i] ^ (keyPtr[i] << 1) ^ (keyPtr[i] << 8)) + (keyPtr[i] << (keyPtr[i] % 7));
	hashOffset = hashOffset % (hash->fTableSize - 1);
	
	return hashOffset;
}

unsigned int ComputeHashFromItem(HashTable* hash, void* item)
{
	void* key;
	if (hash->fKeyIsPtr)
		key = *(void**)((char*)item + hash->fKeyOffset);
	else
		key = (char*)item + hash->fKeyOffset;
	return ComputeHash(hash, key);
}

int IsItemEqualToKey(HashTable* hash, void* item, void* key)
{
	void* key2;
	int keySize = hash->fKeySize;

	if (hash->fKeyIsPtr)
		key2 = *(void**)((char*)item + hash->fKeyOffset);
	else
		key2 = (char*)item + hash->fKeyOffset;
	
	if (key2 == NULL)
		return 0;
	
	if (keySize == 0)
		return (strcmp(key2, key) == 0);

	return (memcmp(key2, key, hash->fKeySize) == 0);
}

void ExpandTable(HashTable* hash)
{
	int i;
	int oldSize = hash->fTableSize;
	int newSize = oldSize * 2;
	if (newSize == 0)
		newSize = 256;
	UserGroup** newTable = (UserGroup**)malloc(newSize * sizeof(void*));
	if (newTable == NULL) return;
	bzero(newTable, newSize * sizeof(void*));

	hash->fTableSize = newSize;

	if (hash->fTable != NULL)
	{
		for (i = 0; i < oldSize; i++)
		{
			if (hash->fTable[i] != NULL)
			{
				void* item = hash->fTable[i];
				int newHashOffset = ComputeHashFromItem(hash, item);
				while (newTable[newHashOffset] != NULL)
					newHashOffset = (newHashOffset + 1) % newSize;
				
				newTable[newHashOffset] = item;
			}
		}
		
		free(hash->fTable);
		hash->fTable = NULL;
	}
	hash->fTable = newTable;
}

void AddToHash(HashTable* hash, UserGroup* item)
{
	unsigned int hashOffset;
	
	if (hash == NULL) return;
	
	if (item == NULL) return;
	
	if (hash->fNumEntries * 3 >= hash->fTableSize * 2)
		ExpandTable(hash);
		
	if (hash->fNumEntries >= hash->fTableSize - 50) return;
	
	hashOffset = ComputeHashFromItem(hash, item);
	
	while (hash->fTable[hashOffset] != NULL)
	{
		if (hash->fTable[hashOffset] == item)
			return;
		hashOffset = (hashOffset + 1) % hash->fTableSize;
	}
	
	hash->fTable[hashOffset] = item;
	if (hash->fCountReference) item->fRefCount++;
	hash->fNumEntries++;
}

UserGroup* HashLookup(HashTable* hash, void* data)
{
	unsigned int hashOffset;
	
	if (hash == NULL) return NULL;
	
	if ( hash->fTable == NULL) return NULL;
	
	hashOffset = ComputeHash(hash, data);
	while ((hash->fTable[hashOffset] != NULL) && !IsItemEqualToKey(hash, hash->fTable[hashOffset], data))
		hashOffset = (hashOffset + 1) % hash->fTableSize;

	if (hash->fTable[hashOffset] != NULL)
		TouchItem(hash->fTable[hashOffset]);

	return hash->fTable[hashOffset];
}

void RemoveFromHash(HashTable* hash, UserGroup* item)
{
	unsigned int hashOffset;
	
	if (hash == NULL) return;
	
	if ( hash->fTable == NULL) return;
	
	hashOffset = ComputeHashFromItem(hash, item);
	while ((hash->fTable[hashOffset] != NULL) && (hash->fTable[hashOffset] != item))
		hashOffset = (hashOffset + 1) % hash->fTableSize;


	if (hash->fTable[hashOffset] != NULL)
	{
		if (hash->fCountReference) hash->fTable[hashOffset]->fRefCount--;
		hash->fTable[hashOffset] = NULL;
		hash->fNumEntries--;
	}
	
	hashOffset = (hashOffset + 1) % hash->fTableSize;
	while (hash->fTable[hashOffset] != NULL)
	{
		unsigned int origOffset = ComputeHashFromItem(hash, hash->fTable[hashOffset]);
		if (origOffset != hashOffset)
		{
			// we need to shuffle this item
			UserGroup* itemToShuffle = hash->fTable[hashOffset];
			if (hash->fCountReference) itemToShuffle->fRefCount--;
			hash->fTable[hashOffset] = NULL;
			hash->fNumEntries--;
			AddToHash(hash, itemToShuffle);
		}
		hashOffset = (hashOffset + 1) % hash->fTableSize;
	}
}

void MergeHashEntries(HashTable* destination, HashTable* source)
{
	int i;
	
	if (source->fTable != NULL)
	{
		for (i = 0; i < source->fTableSize; i++)
		{
			if (source->fTable[i] != NULL)
			{
				UserGroup* item = source->fTable[i];
				AddToHash(destination, item);
			}
		}
	}
}

int GetHashEntries(HashTable* hash, UserGroup** itemArray, long itemArraySize)
{
	int i;
	int numResults = 0;
	
	if (hash == NULL) return 0;
	
	if ( hash->fTable == NULL) return 0;
	
	for (i = 0; i < hash->fTableSize; i++)
	{
		if (hash->fTable[i] != NULL)
		{
			itemArray[numResults++] = hash->fTable[i];
			if (numResults >= itemArraySize) break;
		}
	}
	
	return numResults;
}
