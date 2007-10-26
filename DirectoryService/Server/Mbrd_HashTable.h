/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
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

#ifndef __Mbrd_HashTable_h__
#define	__Mbrd_HashTable_h__		1

#include "Mbrd_UserGroup.h"

typedef struct HashTable
{
	struct UserGroup**   fTable;
	long	fTableSize;
	long	fNumEntries;
	long	fKeyOffset;
	long	fKeySize;
	long	fKeyIsPtr;
	long	fCountReference;
} HashTable;

__BEGIN_DECLS

HashTable* CreateHash(int offset, int size, long isPtr, int countReference);
void ReleaseHash(HashTable* hash);

unsigned int ComputeHashFromItem(HashTable* hash, void* item);
void AddToHash(HashTable* hash, UserGroup* item);
UserGroup* HashLookup(HashTable* hash, void* data);
void RemoveFromHash(HashTable* hash, UserGroup* item);

void MergeHashEntries(HashTable* destination, HashTable* source);
int GetHashEntries(HashTable* hash, UserGroup** itemArray, long itemArraySize);

__END_DECLS

#endif
