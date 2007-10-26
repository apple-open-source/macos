/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/hash.c,v 1.5 2003/04/27 18:17:32 tsi Exp $ */

#include "lisp/hash.h"

/* A simple hash-table implementation
 * TODO: implement SXHASH and WITH-HASH-TABLE-ITERATOR
 * May need a rewrite for better performance, and will
 * need a rewrite if images/bytecode saved on disk.
 */

#define	GET_HASH	1
#define PUT_HASH	2
#define REM_HASH	3

/*
 * Prototypes
 */
static unsigned long LispHashKey(LispObj*, int);
static LispObj *LispHash(LispBuiltin*, int);
static void LispRehash(LispHashTable*);
static void LispFreeHashEntries(LispHashEntry*, long);

/*
 * Initialization
 */
extern LispObj *Oeq, *Oeql, *Oequal, *Oequalp;

/*  Hash tables will have one of these sizes, unless the user
 * specified a very large size */
static long some_primes[] = {
       5,   11,   17,   23,
      31,   47,   71,   97,
     139,  199,  307,  401,
     607,  809, 1213, 1619,
    2437, 3251, 4889, 6521
};

/*
 * Implementation
 */
static unsigned long
LispHashKey(LispObj *object, int function)
{
    mpi *bigi;
    char *string;
    long i, length;
    unsigned long key = ((unsigned long)object) >> 4;

    /* Must be the same object for EQ */
    if (function == FEQ)
	goto hash_key_done;

    if (function == FEQUALP) {
	switch (OBJECT_TYPE(object)) {
	    case LispSChar_t:
		key = (unsigned long)toupper(SCHAR_VALUE(object));
		goto hash_key_done;
	    case LispString_t:
		string = THESTR(object);
		length = STRLEN(object);
		if (length > 32)
		    length = 32;
		for (i = 0, key = 0; i < length; i++)
		    key = (key << 1) ^ toupper(string[i]);
		goto hash_key_done;
	    default:
		break;
	}
    }

    /* Function is EQL, EQUAL or EQUALP */
    switch (OBJECT_TYPE(object)) {
	case LispFixnum_t:
	case LispSChar_t:
	    key = (unsigned long)FIXNUM_VALUE(object);
	    goto hash_key_done;
	case LispInteger_t:
	    key = (unsigned long)INT_VALUE(object);
	    goto hash_key_done;
	case LispRatio_t:
	    key = (object->data.ratio.numerator << 16) ^
		   object->data.ratio.denominator;
	    goto hash_key_done;
	case LispDFloat_t:
	    key = (unsigned long)DFLOAT_VALUE(object);
	    break;
	case LispComplex_t:
	    key = (LispHashKey(object->data.complex.imag, function) << 16) ^
		   LispHashKey(object->data.complex.real, function);
	    goto hash_key_done;
	case LispBignum_t:
	    bigi = object->data.mp.integer;
	    length = bigi->size;
	    if (length > 8)
		length = 8;
	    key = bigi->sign;
	    for (i = 0; i < length; i++)
		key = (key << 8) ^ bigi->digs[i];
	    goto hash_key_done;
	case LispBigratio_t:
	    bigi = mpr_num(object->data.mp.ratio);
	    length = bigi->size;
	    if (length > 4)
		length = 4;
	    key = bigi->sign;
	    for (i = 0; i < length; i++)
		key = (key << 4) ^ bigi->digs[i];
	    bigi = mpr_den(object->data.mp.ratio);
	    length = bigi->size;
	    if (length > 4)
		length = 4;
	    for (i = 0; i < length; i++)
		key = (key << 4) ^ bigi->digs[i];
	    goto hash_key_done;
	default:
	    break;
    }

    /* Anything else must be the same object for EQL */
    if (function == FEQL)
	goto hash_key_done;

    switch (OBJECT_TYPE(object)) {
	case LispString_t:
	    string = THESTR(object);
	    length = STRLEN(object);
	    if (length > 32)
		length = 32;
	    for (i = 0, key = 0; i < length; i++)
		key = (key << 1) ^ string[i];
	    break;
	case LispCons_t:
	    key = (LispHashKey(CAR(object), function) << 16) ^
		   LispHashKey(CDR(object), function);
	    break;
	case LispQuote_t:
	case LispBackquote_t:
	case LispPathname_t:
	    key = LispHashKey(object->data.pathname, function);
	    break;
	case LispRegex_t:
	    key = LispHashKey(object->data.regex.pattern, function);
	    break;
	default:
	    break;
    }

hash_key_done:
    return (key);
}

static LispObj *
LispHash(LispBuiltin *builtin, int code)
{
    LispHashEntry *entry;
    LispHashTable *hash;
    unsigned long key;
    LispObj *result;
    int found;
    long i;

    LispObj *okey, *hash_table, *value;

    if (code == REM_HASH)
	value = NIL;
    else {
	value = ARGUMENT(2);
	if (value == UNSPEC)
	    value = NIL;
    }
    hash_table = ARGUMENT(1);
    okey = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    /* get hash entry */
    hash = hash_table->data.hash.table;
    key = LispHashKey(okey, hash->function) % hash->num_entries;
    entry = hash->entries + key;

    /* search entry in the hash table */
    if (entry->count == 0)
	i = 0;
    else {
	if (hash->function == FEQ) {
	    for (i = entry->cache; i >= 0; i--) {
		if (entry->keys[i] == okey)
		    goto found_key;
	    }
	    for (i = entry->cache + 1; i < entry->count; i++) {
		if (entry->keys[i] == okey)
		    break;
	    }
	}
	else {
	    for (i = entry->cache; i >= 0; i--) {
		if (LispObjectCompare(entry->keys[i], okey,
				      hash->function) == T)
		    goto found_key;
	    }
	    for (i = entry->cache + 1; i < entry->count; i++) {
		if (LispObjectCompare(entry->keys[i], okey,
				      hash->function) == T)
		    break;
	    }
	}
    }

found_key:
    result = value;
    if ((found = i < entry->count) == 0)
	i = entry->count;

    switch (code) {
	case GET_HASH:
	    RETURN_COUNT = 1;
	    if (found) {
		RETURN(0) = T;
		entry->cache = i;
		result = entry->values[i];
	    }
	    else
		RETURN(0) = NIL;
	    break;
	case PUT_HASH:
	    entry->cache = i;
	    if (found)
		/* Just replace current entry */
		entry->values[i] = value;
	    else {
		if ((i % 4) == 0) {
		    LispObj **keys, **values;

		    keys = realloc(entry->keys, sizeof(LispObj*) * (i + 4));
		    if (keys == NULL)
			LispDestroy("out of memory");
		    values = realloc(entry->values, sizeof(LispObj*) * (i + 4));
		    if (values == NULL) {
			free(keys);
			LispDestroy("out of memory");
		    }
		    entry->keys = keys;
		    entry->values = values;
		}
		entry->keys[i] = okey;
		entry->values[i] = value;
		++entry->count;
		++hash->count;
		if (hash->count > hash->rehash_threshold * hash->num_entries)
		    LispRehash(hash);
	    }
	    break;
	case REM_HASH:
	    if (found) {
		result = T;
		--entry->count;
		--hash->count;
		if (i < entry->count) {
		    memmove(entry->keys + i, entry->keys + i + 1,
			    (entry->count - i) * sizeof(LispObj*));
		    memmove(entry->values + i, entry->values + i + 1,
			    (entry->count - i) * sizeof(LispObj*));
		}
		if (entry->cache && entry->cache == entry->count)
		    --entry->cache;
	    }
	    break;
    }

    return (result);
}

static void
LispRehash(LispHashTable *hash)
{
    unsigned long key;
    LispHashEntry *entries, *nentry, *entry, *last;
    long i, size = hash->num_entries * hash->rehash_size;

    for (i = 0; i < sizeof(some_primes) / sizeof(some_primes[0]); i++)
	if (some_primes[i] >= size) {
	    size = some_primes[i];
	    break;
	}

    entries = calloc(1, sizeof(LispHashEntry) * size);
    if (entries == NULL)
	goto out_of_memory;

    for (entry = hash->entries, last = entry + hash->num_entries;
	 entry < last; entry++) {
	for (i = 0; i < entry->count; i++) {
	    key = LispHashKey(entry->keys[i], hash->function) % size;
	    nentry = entries + key;
	    if ((nentry->count % 4) == 0) {
		LispObj **keys, **values;

		keys = realloc(nentry->keys, sizeof(LispObj*) *
			       (nentry->count + 4));
		if (keys == NULL)
		    goto out_of_memory;
		values = realloc(nentry->values, sizeof(LispObj*) *
				 (nentry->count + 4));
		if (values == NULL) {
		    free(keys);
		    goto out_of_memory;
		}
		nentry->keys = keys;
		nentry->values = values;
	    }
	    nentry->keys[nentry->count] = entry->keys[i];
	    nentry->values[nentry->count] = entry->values[i];
	    ++nentry->count;

	}
    }
    LispFreeHashEntries(hash->entries, hash->num_entries);
    hash->entries = entries;
    hash->num_entries = size;
    return;

out_of_memory:
    if (entries)
	LispFreeHashEntries(entries, size);
    LispDestroy("out of memory");
}

static void
LispFreeHashEntries(LispHashEntry *entries, long num_entries)
{
    LispHashEntry *entry, *last;

    for (entry = entries, last = entry + num_entries; entry < last; entry++) {
	free(entry->keys);
	free(entry->values);
    }
    free(entries);
}

void
LispFreeHashTable(LispHashTable *hash)
{
    LispFreeHashEntries(hash->entries, hash->num_entries);
    free(hash);
}

LispObj *
Lisp_Clrhash(LispBuiltin *builtin)
/*
 clrhash hash-table
 */
{
    LispHashTable *hash;
    LispHashEntry *entry, *last;

    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    hash = hash_table->data.hash.table;
    for (entry = hash->entries, last = entry + hash->num_entries;
	entry < last; entry++) {
	free(entry->keys);
	free(entry->values);
	entry->keys = entry->values = NULL;
	entry->count = entry->cache = 0;
    }
    hash->count = 0;

    return (hash_table);
}

LispObj *
Lisp_Gethash(LispBuiltin *builtin)
/*
 gethash key hash-table &optional default
 */
{
    return (LispHash(builtin, GET_HASH));
}

LispObj *
Lisp_HashTableP(LispBuiltin *builtin)
/*
 hash-table-p object
 */
{
    LispObj *object = ARGUMENT(0);

    return (HASHTABLEP(object) ? T : NIL);
}

LispObj *
Lisp_HashTableCount(LispBuiltin *builtin)
/*
 hash-table-count hash-table
 */
{
    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    return (FIXNUM(hash_table->data.hash.table->count));
}

LispObj *
Lisp_HashTableRehashSize(LispBuiltin *builtin)
/*
 hash-table-rehash-size hash-table
 */
{
    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    return (DFLOAT(hash_table->data.hash.table->rehash_size));
}

LispObj *
Lisp_HashTableRehashThreshold(LispBuiltin *builtin)
/*
 hash-table-rehash-threshold hash-table
 */
{
    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    return (DFLOAT(hash_table->data.hash.table->rehash_threshold));
}

LispObj *
Lisp_HashTableSize(LispBuiltin *builtin)
/*
 hash-table-size hash-table
 */
{
    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    return (FIXNUM(hash_table->data.hash.table->num_entries));
}

LispObj *
Lisp_HashTableTest(LispBuiltin *builtin)
/*
 hash-table-test hash-table
 */
{
    LispObj *hash_table = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    return (hash_table->data.hash.test);
}

LispObj *
Lisp_Maphash(LispBuiltin *builtin)
/*
 maphash function hash-table
 */
{
    long i;
    LispHashEntry *entry, *last;

    LispObj *function, *hash_table;

    hash_table = ARGUMENT(1);
    function = ARGUMENT(0);

    CHECK_HASHTABLE(hash_table);

    for (entry = hash_table->data.hash.table->entries,
	 last = entry + hash_table->data.hash.table->num_entries;
	 entry < last; entry++) {
	for (i = 0; i < entry->count; i++)
	    APPLY2(function, entry->keys[i], entry->values[i]);
    }

    return (NIL);
}

LispObj *
Lisp_MakeHashTable(LispBuiltin *builtin)
/*
 make-hash-table &key test size rehash-size rehash-threshold initial-contents
 */
{
    int function = FEQL;
    unsigned long i, isize, xsize;
    double drsize, drthreshold;
    LispHashTable *hash_table;
    LispObj *cons, *result;

    LispObj *test, *size, *rehash_size, *rehash_threshold, *initial_contents;

    initial_contents = ARGUMENT(4);
    rehash_threshold = ARGUMENT(3);
    rehash_size = ARGUMENT(2);
    size = ARGUMENT(1);
    test = ARGUMENT(0);

    if (test != UNSPEC) {
	if (FUNCTIONP(test))
	    test = test->data.atom->object;
	if (test == Oeq)
	    function = FEQ;
	else if (test == Oeql)
	    function = FEQL;
	else if (test == Oequal)
	    function = FEQUAL;
	else if (test == Oequalp)
	    function = FEQUALP;
	else
	    LispDestroy("%s: :TEST must be EQ, EQL, EQUAL, "
			"or EQUALP, not %s", STRFUN(builtin), STROBJ(test));
    }
    else
	test = Oeql;

    if (size != UNSPEC) {
	CHECK_INDEX(size);
	isize = FIXNUM_VALUE(size);
    }
    else
	isize = 1;

    if (rehash_size != UNSPEC) {
	CHECK_DFLOAT(rehash_size);
	if (DFLOAT_VALUE(rehash_size) <= 1.0)
	    LispDestroy("%s: :REHASH-SIZE must a float > 1, not %s",
			STRFUN(builtin), STROBJ(rehash_size));
	drsize = DFLOAT_VALUE(rehash_size);
    }
    else
	drsize = 1.5;

    if (rehash_threshold != UNSPEC) {
	CHECK_DFLOAT(rehash_threshold);
	if (DFLOAT_VALUE(rehash_threshold) < 0.0 ||
	    DFLOAT_VALUE(rehash_threshold) > 1.0)
	    LispDestroy("%s: :REHASH-THRESHOLD must a float "
			"in the range 0.0 - 1.0, not %s",
			STRFUN(builtin), STROBJ(rehash_threshold));
	drthreshold = DFLOAT_VALUE(rehash_threshold);
    }
    else
	drthreshold = 0.75;

    if (initial_contents == UNSPEC)
	initial_contents = NIL;
    CHECK_LIST(initial_contents);
    for (xsize = 0, cons = initial_contents;
	 CONSP(cons);
	 xsize++, cons = CDR(cons))
	CHECK_CONS(CAR(cons));

    if (xsize > isize)
	isize = xsize;

    for (i = 0; i < sizeof(some_primes) / sizeof(some_primes[0]); i++)
	if (some_primes[i] >= isize) {
	    isize = some_primes[i];
	    break;
	}

    hash_table = LispMalloc(sizeof(LispHashTable));
    hash_table->entries = LispCalloc(1, sizeof(LispHashEntry) * isize);
    hash_table->num_entries = isize;
    hash_table->count = 0;
    hash_table->function = function;
    hash_table->rehash_size = drsize;
    hash_table->rehash_threshold = drthreshold;

    result = LispNew(NIL, NIL);
    result->type = LispHashTable_t;
    result->data.hash.table = hash_table;
    result->data.hash.test = test;

    LispMused(hash_table);
    LispMused(hash_table->entries);

    if (initial_contents != UNSPEC) {
	unsigned long key;
	LispHashEntry *entry;

	for (cons = initial_contents; CONSP(cons); cons = CDR(cons)) {
	    key = LispHashKey(CAAR(cons), function) % isize;
	    entry = hash_table->entries + key;

	    if ((entry->count % 4) == 0) {
		LispObj **keys, **values;

		keys = realloc(entry->keys, sizeof(LispObj*) * (i + 4));
		if (keys == NULL)
		    LispDestroy("out of memory");
		values = realloc(entry->values, sizeof(LispObj*) * (i + 4));
		if (values == NULL) {
		    free(keys);
		    LispDestroy("out of memory");
		}
		entry->keys = keys;
		entry->values = values;
	    }
	    entry->keys[entry->count] = CAAR(cons);
	    entry->values[entry->count] = CDAR(cons);
	    ++entry->count;
	}
	hash_table->count = xsize;
    }

    return (result);
}

LispObj *
Lisp_Remhash(LispBuiltin *builtin)
/*
 remhash key hash-table
 */
{
    return (LispHash(builtin, REM_HASH));
}

LispObj *
Lisp_XeditPuthash(LispBuiltin *builtin)
/*
 lisp::puthash key hash-table value
 */
{
    return (LispHash(builtin, PUT_HASH));
}
