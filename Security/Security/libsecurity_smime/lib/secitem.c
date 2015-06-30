/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Support routines for SECItem data structure.
 */

#include "secitem.h"
#include <security_asn1/seccomon.h>
#include <security_asn1/secerr.h>

SECItem *
SECITEM_AllocItem(PRArenaPool *arena, SECItem *item, size_t len)
{
    SECItem *result = NULL;
    void *mark = NULL;

    if (arena != NULL) {
	mark = PORT_ArenaMark(arena);
    }

    if (item == NULL) {
	if (arena != NULL) {
	    result = PORT_ArenaZAlloc(arena, sizeof(SECItem));
	} else {
	    result = PORT_ZAlloc(sizeof(SECItem));
	}
	if (result == NULL) {
	    goto loser;
	}
    } else {
	PORT_Assert(item->Data == NULL);
	result = item;
    }

    result->Length = len;
    if (len) {
	if (arena != NULL) {
	    result->Data = PORT_ArenaAlloc(arena, len);
	} else {
	    result->Data = PORT_Alloc(len);
	}
    }

    if (mark) {
	PORT_ArenaUnmark(arena, mark);
    }
    return(result);

loser:
    if ( arena != NULL ) {
	if (mark) {
	    PORT_ArenaRelease(arena, mark);
	}
	if (item != NULL) {
	    item->Data = NULL;
	    item->Length = 0;
	}
    } else {
	if (result != NULL) {
	    SECITEM_FreeItem(result, (item == NULL) ? PR_TRUE : PR_FALSE);
	}
    }
    return(NULL);
}

SECStatus
SECITEM_ReallocItem(PRArenaPool *arena, SECItem *item, unsigned int oldlen,
		    unsigned int newlen)
{
    PORT_Assert(item != NULL);
    if (item == NULL) {
	/* XXX Set error.  But to what? */
	return SECFailure;
    }

    /*
     * If no old length, degenerate to just plain alloc.
     */
    if (oldlen == 0) {
	PORT_Assert(item->Data == NULL || item->Length == 0);
	if (newlen == 0) {
	    /* Nothing to do.  Weird, but not a failure.  */
	    return SECSuccess;
	}
	item->Length = newlen;
	if (arena != NULL) {
	    item->Data = PORT_ArenaAlloc(arena, newlen);
	} else {
	    item->Data = PORT_Alloc(newlen);
	}
    } else {
	if (arena != NULL) {
	    item->Data = PORT_ArenaGrow(arena, item->Data, oldlen, newlen);
	} else {
	    item->Data = PORT_Realloc(item->Data, newlen);
	}
    }

    if (item->Data == NULL) {
	return SECFailure;
    }

    return SECSuccess;
}

SECComparison
SECITEM_CompareItem(const SECItem *a, const SECItem *b)
{
    CSSM_SIZE m;
    SECComparison rv;

    m = ( ( a->Length < b->Length ) ? a->Length : b->Length );
    
    rv = (SECComparison) PORT_Memcmp(a->Data, b->Data, m);
    if (rv) {
	return rv;
    }
    if (a->Length < b->Length) {
	return SECLessThan;
    }
    if (a->Length == b->Length) {
	return SECEqual;
    }
    return SECGreaterThan;
}

Boolean
SECITEM_ItemsAreEqual(const SECItem *a, const SECItem *b)
{
    if (a->Length != b->Length)
        return PR_FALSE;
    if (!a->Length)
    	return PR_TRUE;
    if (!a->Data || !b->Data) {
        /* avoid null pointer crash. */
		return (Boolean)(a->Data == b->Data);
    }
    return (Boolean)!PORT_Memcmp(a->Data, b->Data, a->Length);
}

SECItem *
SECITEM_DupItem(const SECItem *from)
{
    return SECITEM_ArenaDupItem(NULL, from);
}

SECItem *
SECITEM_ArenaDupItem(PRArenaPool *arena, const SECItem *from)
{
    SECItem *to;

    if ( from == NULL ) {
		return(NULL);
    }

    if ( arena != NULL ) {
		to = (SECItem *)PORT_ArenaAlloc(arena, sizeof(SECItem));
    } else {
		to = (SECItem *)PORT_Alloc(sizeof(SECItem));
    }
    if ( to == NULL ) {
		return(NULL);
    }

    if ( arena != NULL ) {
		to->Data = (unsigned char *)PORT_ArenaAlloc(arena, from->Length);
    } else {
		to->Data = (unsigned char *)PORT_Alloc(from->Length);
    }
    if ( to->Data == NULL ) {
		PORT_Free(to);
		return(NULL);
    }

    to->Length = from->Length;
    // to->type = from->type;
    if ( to->Length ) {
		PORT_Memcpy(to->Data, from->Data, to->Length);
    }

    return(to);
}

SECStatus
SECITEM_CopyItem(PRArenaPool *arena, SECItem *to, const SECItem *from)
{
    // to->type = from->type;
    if (from->Data && from->Length) {
	if ( arena ) {
	    to->Data = (unsigned char*) PORT_ArenaAlloc(arena, from->Length);
	} else {
	    to->Data = (unsigned char*) PORT_Alloc(from->Length);
	}
	
	if (!to->Data) {
	    return SECFailure;
	}
	PORT_Memcpy(to->Data, from->Data, from->Length);
	to->Length = from->Length;
    } else {
	to->Data = 0;
	to->Length = 0;
    }
    return SECSuccess;
}

void
SECITEM_FreeItem(SECItem *zap, Boolean freeit)
{
    if (zap) {
	PORT_Free(zap->Data);
	zap->Data = 0;
	zap->Length = 0;
	if (freeit) {
	    PORT_Free(zap);
	}
    }
}

void
SECITEM_ZfreeItem(SECItem *zap, Boolean freeit)
{
    if (zap) {
	PORT_ZFree(zap->Data, zap->Length);
	zap->Data = 0;
	zap->Length = 0;
	if (freeit) {
	    PORT_ZFree(zap, sizeof(SECItem));
	}
    }
}


/* these reroutines were taken from pkix oid.c, which is supposed to
 * replace this file some day */
/*
 * This is the hash function.  We simply XOR the encoded form with
 * itself in sizeof(PLHashNumber)-byte chunks.  Improving this
 * routine is left as an excercise for the more mathematically
 * inclined student.
 */
PLHashNumber PR_CALLBACK
SECITEM_Hash ( const void *key)
{
    const SECItem *item = (const SECItem *)key;
    PLHashNumber rv = 0;

    PRUint8 *data = (PRUint8 *)item->Data;
    PRUint32 i;
    PRUint8 *rvc = (PRUint8 *)&rv;

    for( i = 0; i < item->Length; i++ ) {
        rvc[ i % sizeof(rv) ] ^= *data;
        data++;
    }

    return rv;
}

/*
 * This is the key-compare function.  It simply does a lexical
 * comparison on the item data.  This does not result in
 * quite the same ordering as the "sequence of numbers" order,
 * but heck it's only used internally by the hash table anyway.
 */
PRIntn PR_CALLBACK
SECITEM_HashCompare ( const void *k1, const void *k2)
{
    const SECItem *i1 = (const SECItem *)k1;
    const SECItem *i2 = (const SECItem *)k2;

    return SECITEM_ItemsAreEqual(i1,i2);
}
