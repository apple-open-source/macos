/*
 * pswdict.c
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

/***********/
/* Imports */
/***********/

#include <stdlib.h>

#include "pswtypes.h"
#include "pswdict.h"
#include "psw.h"

#ifdef XENVIRONMENT
#include <X11/Xos.h>
#else
#include <string.h>
#endif

/********************/
/* Types */
/********************/

typedef struct _t_EntryRec {
  struct _t_EntryRec *next;
  char *name;
  PSWDictValue value;
} EntryRec, *Entry;

 /* The concrete definition for a dictionary */
typedef struct _t_PSWDictRec {
  int nEntries;
  Entry *entries;
} PSWDictRec;

PSWDict atoms;

/**************************/
/* Procedure Declarations */
/**************************/

/* Creates and returns a new dictionary. nEntries is a hint. */
PSWDict CreatePSWDict(int nEntries)
{
  PSWDict d = (PSWDict)psw_calloc(sizeof(PSWDictRec), 1);
  d->nEntries = nEntries;
  d->entries = (Entry *)psw_calloc(sizeof(EntryRec), d->nEntries);
  return d;
}

/* Destroys a dictionary */
void DestroyPSWDict(PSWDict dict)
{
  free(dict->entries);
  free(dict);
}

static int Hash(char *name, int nEntries)
{
  register int val = 0;
  while (*name) val += *name++;
  if (val < 0) val = -val;
  return (val % nEntries);
}

static Entry Probe(PSWDict d, int x, char *name)
{
  register Entry e;
  for (e = (d->entries)[x]; e; e = e->next) {
    if (strcmp(name, e->name) == 0) break;
    }
  return e;
}

static Entry PrevProbe(Entry *prev, PSWDict d, int x, char *name)
{
  register Entry e;
  *prev = NULL;
  for (e = (d->entries)[x]; e; e = e->next) {
    if (strcmp(name, e->name) == 0) break;
    *prev = e;
    }
  return e;
}

/* -1 => not found */
PSWDictValue PSWDictLookup(PSWDict dict, char *name)
{
  Entry e;
  e = Probe(dict, Hash(name, dict->nEntries), name);
  if (e == NULL) return -1;
  return e->value;
}

/*  0 => normal return (not found)
   -1 => found. If found, value is replaced. */
PSWDictValue PSWDictEnter(PSWDict dict, char *name, PSWDictValue value)
{
  Entry e;
  int x = Hash(name, dict->nEntries);
  e = Probe(dict, x, name);
  if (e) {
    e->value = value;
    return -1;
    }
  e = (Entry)psw_calloc(sizeof(EntryRec), 1);
  e->next = (dict->entries)[x]; (dict->entries)[x] = e;
  e->value = value;
  e->name = MakeAtom(name);
  return 0;
}

/* -1 => not found. If found, value is returned. */
PSWDictValue PSWDictRemove(PSWDict dict, char *name)
{
  Entry e, prev;
  PSWDictValue value;
  int x = Hash(name, dict->nEntries);

  e = PrevProbe(&prev, dict, x, name);
  if (e == NULL) return -1;
  value = e->value;
  if (prev == NULL) (dict->entries)[x] = e->next; else prev->next = e->next;
  free(e);
  return value;
}

PSWAtom MakeAtom(char *name)
{
  Entry e;
  int x = Hash(name, 511);
  char *newname;

  if (atoms == NULL) atoms = CreatePSWDict(511);
  e = Probe(atoms, x, name);
  if (e == NULL) {
    e = (Entry)psw_calloc(sizeof(EntryRec), 1);
    e->next = (atoms->entries)[x]; (atoms->entries)[x] = e;
    e->value = 0;
    newname = psw_malloc(strlen(name)+1);
    strcpy(newname, name);
    e->name = newname;
    }
  return e->name;
}
