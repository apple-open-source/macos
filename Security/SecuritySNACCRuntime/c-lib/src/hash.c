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
 * This was borrowed from Don Acton and Terry Coatta's Raven Code.
 * It has been modified somewhat.
 *    - Mike Sample 92
 *
 * This is a set or routines that implements an extensible hashing
 * algorithm. At the moment it assumes that all the hash codes are unique
 * (ie. there are no collisions). For the way hash codes are currently being
 * supplied this is not a bad assumption.
 * The extensible hashing routine used is based on a multiway tree with
 * each node in the tree being a fixed array of (2^n) size. At a given
 * level, i, in the tree with the first level being level 0, bits
 * i*n through i*n through (i+1)*n-1 are used as the index into the table.
 * Each entry in the table is either NULL (unused) or a pointer to an
 * object of type entry. The entry contains all the information about a
 * hash entry. The entry also contains a field indicating whether or not this
 * is a leaf node. If an entry isn't a leaf node then it references a table at
 * at the next level and not a value. With the current implementation
 * a 32 hash value is used and table sizes are 256. The algorithm used
 * here is the same as the one used in the Set class of the Raven
 * class system.
 *
 * Copyright (C) 1992 University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/src/hash.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: hash.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:32  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1997/02/28 13:39:51  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.2  1995/07/27 09:05:54  rj
 * use memzero that is defined in .../snacc.h to use either memset or bzero.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:46:06  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "hash.h"


/*
 *
 * From sdbm, an ndbm work-alike hashed database library
 * Author: oz@nexus.yorku.ca
 * Status: public domain.
 *
 * polynomial conversion ignoring overflows
 * [this seems to work remarkably well, in fact better
 * then the ndbm hash function. Replace at your own risk]
 * use: 65599   nice.
 *      65587   even better.
 *
 * [In one experiment, this function hashed 84165 symbols (English words
 * plus symbol table values) with no collisions. -bjb]
 *
 */

Hash
MakeHash PARAMS ((str, len),
    char *str _AND_
    unsigned long int len)
{
    register Hash n;
    n = 0;

#define HASHC   n = *str++ + 65587 * n

    if (len > 0)
    {
        int loop;
        loop = (len + 8 - 1) >> 3;
        switch (len & (8 - 1))
	{
          case 0:
            do
	    {
                HASHC;
              case 7: HASHC;
              case 6: HASHC;
              case 5: HASHC;
              case 4: HASHC;
              case 3: HASHC;
              case 2: HASHC;
              case 1: HASHC;
	    } while (--loop);
	}
    }
    return n;
}

/* Creates and clears a new hash slot */
static HashSlot*
NewHashSlot()
{
  HashSlot *foo;

  foo = (HashSlot *) malloc (sizeof (HashSlot));
  if (foo == NULL)
      return NULL;
  memzero (foo, sizeof (HashSlot));
  return foo;
}

/* Create a new cleared hash table */
static Table*
NewTable()
{
  Table *new_table;

  new_table = (Table *) malloc (sizeof (Table));
  if (new_table == NULL)
      return NULL;
  memzero (new_table, sizeof (Table));
  return new_table;
}

/* This routine is used to initialize the hash tables. When it is called
 * it returns a value which is used to identify which hash table
 * a particular request is to operate on.
 */
Table*
InitHash()
{
  Table *table;
  table = NewTable();
  if (table == NULL)
      return 0;
  else
      return table;
}

/* When a hash collision occurs at a leaf slot this routine is called to
 * split the entry and add a new level to the tree at this point.
 */
static int
SplitAndInsert PARAMS ((entry, element, hash_value),
    HashSlot *entry _AND_
    void *element _AND_
    Hash hash_value)
{

  if (((entry->table = NewTable()) == NULL) ||
      !Insert (entry->table, entry->value, entry->hash >> INDEXSHIFT) ||
      !Insert (entry->table, element, hash_value >> INDEXSHIFT))
    return FALSE;

  entry->leaf = FALSE;
  return TRUE;
}

/* This routine takes a hash table identifier, an element (value) and the
 * coresponding hash value for that element and enters it into the table
 * assuming it isn't already there.
 */
int
Insert PARAMS ((table, element, hash_value),
    Table *table _AND_
    void *element _AND_
    Hash hash_value)
{
  HashSlot *entry;

  entry = (HashSlot *) (*table)[hash_value & INDEXMASK];

  if (entry == NULL) {
    /* Need to add this element here */
    entry = NewHashSlot();
    if (entry == NULL)
        return FALSE;
    entry->leaf = TRUE;
    entry->value = element;
    entry->hash = hash_value;
    (*table)[hash_value & INDEXMASK] = (void*)entry;
    return TRUE;
  }

  if (hash_value == entry->hash)
      return TRUE;

  if (entry->leaf)
      return SplitAndInsert (entry, element, hash_value);

  return Insert (entry->table, element, hash_value >> INDEXSHIFT);
}


/* This routine looks to see if a particular hash value is already stored in
 * the table. It returns true if it is and false otherwise.
 */
int
CheckFor PARAMS ((table, hash),
    Table *table _AND_
    Hash hash)
{
  HashSlot *entry;

  entry = (HashSlot *) table[hash & INDEXMASK];

  if (entry == NULL)
      return FALSE;
  if (entry->leaf)
      return entry->hash == hash;
  return CheckFor (entry->table, hash >> INDEXSHIFT);
}

/* In addition to checking for a hash value in the tree this function also
 * returns the coresponding element value into the space pointed to by
 * the value parameter. If the hash value isn't found FALSE is returned
 * the the space pointed to by value is not changed.
 */
int
CheckForAndReturnValue PARAMS ((table, hash, value),
    Table *table _AND_
    Hash hash _AND_
    void **value)
{
  HashSlot *entry;
  entry = (HashSlot *) (*table)[hash & INDEXMASK];

  if (entry == NULL)
      return FALSE;

  if (entry->leaf)
  {
      if (entry->hash == hash)
      {
          *value = entry->value;
          return TRUE;
      }
      else
          return FALSE;
  }
  return CheckForAndReturnValue (entry->table, hash >> INDEXSHIFT, value);
}
