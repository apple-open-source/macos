/*
 * dpsdict.c
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dps/dpsdict.c,v 1.3 2000/09/26 15:57:00 tsi Exp $ */

/***********/
/* Imports */
/***********/

#include <stdlib.h>
#include <string.h>

#include "dpsint.h"

/********************/
/* Types */
/********************/

typedef struct _EntryRec {
  struct _EntryRec *next;
  char *name;
  PSWDictValue value;
  } EntryRec, *Entry;

 /* The concrete definition for a dictionary */
 typedef struct _PSWDictRec {
  integer nEntries;
  Entry *entries;
  } PSWDictRec;

static PSWDict atoms;

/**************************/
/* Procedure Declarations */
/**************************/

/* Creates and returns a new dictionary. nEntries is a hint. */
PSWDict DPSCreatePSWDict(integer nEntries)
{
  PSWDict d = (PSWDict)DPScalloc(sizeof(PSWDictRec), 1);
  d->nEntries = nEntries;
  d->entries = (Entry *)DPScalloc(sizeof(EntryRec), d->nEntries);
  return d;
}

/* Destroys a dictionary */
void DPSDestroyPSWDict(PSWDict dict)
{
  integer links = dict->nEntries;
  Entry   next;
  Entry   prev;
  
  while (links > 0)
    {
    next = (dict->entries)[links];
    while (next != NIL)
      {
      prev = next;
      next = next->next;
      free (prev);
      }
    links--;
    }
  free(dict->entries);
  free(dict);
}

static integer Hash(char *name, integer nEntries)
{
  register integer val = 0;
  while (*name) val += *name++;
  if (val < 0) val = -val;
  return (val % nEntries);
}

static Entry Probe(PSWDict d, integer x, char *name)
{
  register Entry e;
  for (e = (d->entries)[x]; e != NIL; e = e->next) {
    if (strcmp(name, e->name) == 0) break;
    }
  return e;
}

static Entry PrevProbe(Entry *prev, PSWDict d, integer x, char *name)
{
  register Entry e;
  *prev = NIL;
  for (e = (d->entries)[x]; e != NIL; e = e->next) {
    if (strcmp(name, e->name) == 0) break;
    *prev = e;
    }
  return e;
}

/* -1 => not found */
PSWDictValue DPSWDictLookup(PSWDict dict, char *name)
{
  Entry e;
  e = Probe(dict, Hash(name, dict->nEntries), name);
  if (e == NIL) return -1;
  return e->value;
}

/*  0 => normal return (not found)
   -1 => found. If found, value is replaced. */
PSWDictValue DPSWDictEnter(PSWDict dict, char *name, PSWDictValue value)
{
  Entry e;
  integer x = Hash(name, dict->nEntries);
  e = Probe(dict, x, name);
  if (e != NIL) {
    e->value = value;
    return -1;
    }
  e = (Entry)DPScalloc(sizeof(EntryRec), 1);
  e->next = (dict->entries)[x]; (dict->entries)[x] = e;
  e->value = value;
  e->name = name; /* MakeAtom(name); */
  return 0;
}

/* -1 => not found. If found, value is returned. */
PSWDictValue DPSWDictRemove(PSWDict dict, char *name)
{
  Entry e, prev;
  PSWDictValue value;
  integer x = Hash(name, dict->nEntries);

  e = PrevProbe(&prev, dict, x, name);
  if (e == NIL) return -1;
  value = e->value;
  if (prev == NIL) (dict->entries)[x] = e->next; else prev->next = e->next;
  free(e);
  return value;
}

Atom DPSMakeAtom(char *name)
{
  Entry e;
  integer x = Hash(name, 511);
  char *newname;

  if (atoms == NIL) atoms = DPSCreatePSWDict(511);
  e = Probe(atoms, x, name);
  if (e == NIL) {
    e = (Entry)DPScalloc(sizeof(EntryRec), 1);
    e->next = (atoms->entries)[x]; (atoms->entries)[x] = e;
    e->value = 0;
    newname = (char *)DPScalloc(strlen(name)+1, 1);
    strcpy(newname, name);
    e->name = newname;
    }
  return (Atom) e->name;
}
