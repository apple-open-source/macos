/*
 * hashtable.c - hash tables
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zsh.h"

/********************************/
/* Generic Hash Table functions */
/********************************/

/* Generic hash function */

/**/
unsigned
hasher(char *str)
{
    unsigned hashval = 0;

    while (*str)
	hashval += (hashval << 5) + *(unsigned char *)str++;

    return hashval;
}

/* Get a new hash table */

/**/
HashTable
newhashtable(int size)
{
    HashTable ht;

    ht = (HashTable) zcalloc(sizeof *ht);
    ht->nodes = (HashNode *) zcalloc(size * sizeof(HashNode));
    ht->hsize = size;
    ht->ct = 0;
    return ht;
}

/* Add a node to a hash table.                          *
 * nam is the key to use in hashing.  nodeptr points    *
 * to the node to add.  If there is already a node in   *
 * the table with the same key, it is first freed, and  *
 * then the new node is added.  If the number of nodes  *
 * is now greater than twice the number of hash values, *
 * the table is then expanded.                          */

/**/
void
addhashnode(HashTable ht, char *nam, void *nodeptr)
{
    unsigned hashval;
    HashNode hn, hp, hq;

    hn = (HashNode) nodeptr;
    hn->nam = nam;

    hashval = ht->hash(hn->nam) % ht->hsize;
    hp = ht->nodes[hashval];

    /* check if this is the first node for this hash value */
    if (!hp) {
	hn->next = NULL;
	ht->nodes[hashval] = hn;
	if (++ht->ct == ht->hsize * 2)
	    expandhashtable(ht);
	return;
    }

    /* else check if the first node contains the same key */
    if (!strcmp(hp->nam, hn->nam)) {
	hn->next = hp->next;
	ht->nodes[hashval] = hn;
	ht->freenode(hp);
	return;
    }

    /* else run through the list and check all the keys */
    hq = hp;
    hp = hp->next;
    for (; hp; hq = hp, hp = hp->next) {
	if (!strcmp(hp->nam, hn->nam)) {
	    hn->next = hp->next;
	    hq->next = hn;
	    ht->freenode(hp);
	    return;
	}
    }

    /* else just add it at the front of the list */
    hn->next = ht->nodes[hashval];
    ht->nodes[hashval] = hn;
    if (++ht->ct == ht->hsize * 2)
        expandhashtable(ht);
}

/* Get an enabled entry in a hash table.  *
 * If successful, it returns a pointer to *
 * the hashnode.  If the node is DISABLED *
 * or isn't found, it returns NULL        */

/**/
HashNode
gethashnode(HashTable ht, char *nam)
{
    unsigned hashval;
    HashNode hp;

    hashval = ht->hash(nam) % ht->hsize;
    for (hp = ht->nodes[hashval]; hp; hp = hp->next) {
	if (!strcmp(hp->nam, nam)) {
	    if (hp->flags & DISABLED)
		return NULL;
	    else
		return hp;
	}
    }
    return NULL;
}

/* Get an entry in a hash table.  It will *
 * ignore the DISABLED flag and return a  *
 * pointer to the hashnode if found, else *
 * it returns NULL.                       */

/**/
HashNode
gethashnode2(HashTable ht, char *nam)
{
    unsigned hashval;
    HashNode hp;

    hashval = ht->hash(nam) % ht->hsize;
    for (hp = ht->nodes[hashval]; hp; hp = hp->next) {
	if (!strcmp(hp->nam, nam))
	    return hp;
    }
    return NULL;
}

/* Remove an entry from a hash table.           *
 * If successful, it removes the node from the  *
 * table and returns a pointer to it.  If there *
 * is no such node, then it returns NULL        */

/**/
HashNode
removehashnode(HashTable ht, char *nam)
{
    unsigned hashval;
    HashNode hp, hq;

    hashval = ht->hash(nam) % ht->hsize;
    hp = ht->nodes[hashval];

    /* if no nodes at this hash value, return NULL */
    if (!hp)
	return NULL;

    /* else check if the key in the first one matches */
    if (!strcmp(hp->nam, nam)) {
	ht->nodes[hashval] = hp->next;
	ht->ct--;
	return hp;
    }

    /* else run through the list and check the rest of the keys */
    hq = hp;
    hp = hp->next;
    for (; hp; hq = hp, hp = hp->next) {
	if (!strcmp(hp->nam, nam)) {
	    hq->next = hp->next;
	    ht->ct--;
	    return hp;
	}
    }

    /* else it is not in the list, so return NULL */
    return NULL;
}

/* Disable a node in a hash table */

/**/
void
disablehashnode(HashNode hn, int flags)
{
    hn->flags |= DISABLED;
}

/* Enable a node in a hash table */

/**/
void
enablehashnode(HashNode hn, int flags)
{
    hn->flags &= ~DISABLED;
}

/* Compare two hash table entries by name */

/**/
int
hnamcmp(const void *ap, const void *bp)
{
    HashNode a = *(HashNode *)ap;
    HashNode b = *(HashNode *)bp;
    return ztrcmp((unsigned char *) a->nam, (unsigned char *) b->nam);
}

/* Scan the nodes in a hash table and execute scanfunc on nodes based on the flags *
 * that are set/unset.  scanflags is passed unchanged to scanfunc (if executed).   *
 *                                                                                 *
 * If sorted = 1, then sort entries of hash table before scanning.                 *
 * If sorted = 0, don't sort entries before scanning.                              *
 * If (flags1 > 0), then execute func on a node only if these flags are set.       *
 * If (flags2 > 0), then execute func on a node only if these flags are NOT set.   *
 * The conditions above for flags1/flags2 must both be true.                       */

/**/
void
scanhashtable(HashTable ht, int sorted, int flags1, int flags2, ScanFunc scanfunc, int scanflags)
{
    HashNode hn, *hnsorttab, *htp;
    int i;

    if (sorted) {
	hnsorttab = (HashNode *) zalloc(ht->ct * sizeof(HashNode));

	for (htp = hnsorttab, i = 0; i < ht->hsize; i++)
	    for (hn = ht->nodes[i]; hn; hn = hn->next)
		*htp++ = hn;

	qsort((void *) & hnsorttab[0], ht->ct, sizeof(HashNode), hnamcmp);

	/* Ignore the flags */
	if (!flags1 && !flags2) {
	    for (htp = hnsorttab, i = 0; i < ht->ct; i++, htp++)
		scanfunc(*htp, scanflags);
	} else if (flags1 && !flags2) {
	/* Only exec scanfunc if flags1 are set */
	    for (htp = hnsorttab, i = 0; i < ht->ct; i++, htp++)
		if ((*htp)->flags & flags1)
		    scanfunc(*htp, scanflags);
	} else if (!flags1 && flags2) {
	/* Only exec scanfunc if flags2 are NOT set */
	    for (htp = hnsorttab, i = 0; i < ht->ct; i++, htp++)
		if (!((*htp)->flags & flags2))
		    scanfunc(*htp, scanflags);
	} else {
	/* Only exec scanfun if flags1 are set, and flags2 are NOT set */
	    for (htp = hnsorttab, i = 0; i < ht->ct; i++, htp++)
		if (((*htp)->flags & flags1) && !((*htp)->flags & flags2))
		    scanfunc(*htp, scanflags);
	}
	free(hnsorttab);
	return;
    }

    /* Don't sort, just use hash order. */

    /* Ignore the flags */
    if (!flags1 && !flags2) {
	for (i = 0; i < ht->hsize; i++)
	    for (hn = ht->nodes[i]; hn; hn = hn->next)
		scanfunc(hn, scanflags);
	return;
    }

    /* Only exec scanfunc if flags1 are set */
    if (flags1 && !flags2) {
	for (i = 0; i < ht->hsize; i++)
	    for (hn = ht->nodes[i]; hn; hn = hn->next)
		if (hn->flags & flags1)
		    scanfunc(hn, scanflags);
	return;
    }

    /* Only exec scanfunc if flags2 are NOT set */
    if (!flags1 && flags2) {
	for (i = 0; i < ht->hsize; i++)
	    for (hn = ht->nodes[i]; hn; hn = hn->next)
		if (!(hn->flags & flags2))
		    scanfunc(hn, scanflags);
	return;
    }

    /* Only exec scanfun if flags1 are set, and flags2 are NOT set */
    for (i = 0; i < ht->hsize; i++)
	for (hn = ht->nodes[i]; hn; hn = hn->next)
	    if ((hn->flags & flags1) && !(hn->flags & flags2))
		scanfunc(hn, scanflags);
}


/* Scan all nodes in a hash table and executes scanfunc on the *
 * nodes which meet all the following criteria:                *
 * The hash key must match the glob pattern given by `com'.    *
 * If (flags1 > 0), then all flags in flags1 must be set.      *
 * If (flags2 > 0), then all flags in flags2 must NOT be set.  *
 *                                                             *
 * scanflags is passed unchanged to scanfunc (if executed).    *
 * The return value if the number of matches.                  */

/**/
int
scanmatchtable(HashTable ht, Comp com, int flags1, int flags2, ScanFunc scanfunc, int scanflags)
{
    HashNode hn;
    int i, match = 0;

    /* ignore the flags */
    if (!flags1 && !flags2) {
	for (i = 0; i < ht->hsize; i++) {
	    for (hn = ht->nodes[i]; hn; hn = hn->next) {
		if (domatch(hn->nam, com, 0)) {
		    scanfunc(hn, scanflags);
		    match++;
		}
	    }
	}
	return match;
    }

    /* flags in flags1 must be set */
    if (flags1 && !flags2) {
	for (i = 0; i < ht->hsize; i++) {
	    for (hn = ht->nodes[i]; hn; hn = hn->next) {
		if (domatch(hn->nam, com, 0) && (hn->flags & flags1)) {
		    scanfunc(hn, scanflags);
		    match++;
		}
	    }
	}
	return match;
    }

    /* flags in flags2 must NOT be set */
    if (!flags1 && flags2) {
	for (i = 0; i < ht->hsize; i++) {
	    for (hn = ht->nodes[i]; hn; hn = hn->next) {
		if (domatch(hn->nam, com, 0) && !(hn->flags & flags2)) {
		    scanfunc(hn, scanflags);
		    match++;
		}
	    }
	}
	return match;
    }

    /* flags in flags1 must be set,    *
     * flags in flags2 must NOT be set */
    for (i = 0; i < ht->hsize; i++) {
	for (hn = ht->nodes[i]; hn; hn = hn->next) {
	    if (domatch(hn->nam, com, 0) && (hn->flags & flags1) && !(hn->flags & flags2)) {
		scanfunc(hn, scanflags);
		match++;
	    }
	}
    }
    return match;
}


/* Expand hash tables when they get too many entries. *
 * The new size is 4 times the previous size.         */

/**/
void
expandhashtable(HashTable ht)
{
    struct hashnode **onodes, **ha, *hn, *hp;
    int i, osize;

    osize = ht->hsize;
    onodes = ht->nodes;

    ht->hsize = osize * 4;
    ht->nodes = (HashNode *) zcalloc(ht->hsize * sizeof(HashNode));
    ht->ct = 0;

    /* scan through the old list of nodes, and *
     * rehash them into the new list of nodes  */
    for (i = 0, ha = onodes; i < osize; i++, ha++) {
	for (hn = *ha; hn;) {
	    hp = hn->next;
	    ht->addnode(ht, hn->nam, hn);
	    hn = hp;
	}
    }
    zfree(onodes, osize * sizeof(HashNode));
}

/* Empty the hash table and resize it if necessary */

/**/
void
emptyhashtable(HashTable ht, int newsize)
{
    struct hashnode **ha, *hn, *hp;
    int i;

    /* free all the hash nodes */
    ha = ht->nodes;
    for (i = 0; i < ht->hsize; i++, ha++) {
	for (hn = *ha; hn;) {
	    hp = hn->next;
	    ht->freenode(hn);
	    hn = hp;
	}
    }

    /* If new size desired is different from current size, *
     * we free it and allocate a new nodes array.          */
    if (ht->hsize != newsize) {
	zfree(ht->nodes, ht->hsize * sizeof(HashNode));
	ht->nodes = (HashNode *) zcalloc(newsize * sizeof(HashNode));
	ht->hsize = newsize;
    } else {
	/* else we just re-zero the current nodes array */
	memset(ht->nodes, 0, newsize * sizeof(HashNode));
    }

    ht->ct = 0;
}

/* Print info about hash table */

#ifdef ZSH_HASH_DEBUG

#define MAXDEPTH 7

/**/
void
printhashtabinfo(HashTable ht)
{
    HashNode hn;
    int chainlen[MAXDEPTH + 1];
    int i, tmpcount, total;

    printf("name of table   : %s\n",   ht->tablename);
    printf("size of nodes[] : %d\n",   ht->hsize);
    printf("number of nodes : %d\n\n", ht->ct);

    memset(chainlen, 0, sizeof(chainlen));

    /* count the number of nodes just to be sure */
    total = 0;
    for (i = 0; i < ht->hsize; i++) {
	tmpcount = 0;
	for (hn = ht->nodes[i]; hn; hn = hn->next)
	    tmpcount++;
	if (tmpcount >= MAXDEPTH)
	    chainlen[MAXDEPTH]++;
	else
	    chainlen[tmpcount]++;
	total += tmpcount;
    }

    for (i = 0; i < MAXDEPTH; i++)
	printf("number of hash values with chain of length %d  : %4d\n", i, chainlen[i]);
    printf("number of hash values with chain of length %d+ : %4d\n", MAXDEPTH, chainlen[MAXDEPTH]);
    printf("total number of nodes                         : %4d\n", total);
}
#endif

/********************************/
/* Command Hash Table Functions */
/********************************/

/* size of the initial cmdnamtab hash table */
#define INITIAL_CMDNAMTAB 201

/* Create a new command hash table */
 
/**/
void
createcmdnamtable(void)
{
    cmdnamtab = newhashtable(INITIAL_CMDNAMTAB);

    cmdnamtab->hash        = hasher;
    cmdnamtab->emptytable  = emptycmdnamtable;
    cmdnamtab->filltable   = fillcmdnamtable;
    cmdnamtab->addnode     = addhashnode;
    cmdnamtab->getnode     = gethashnode2;
    cmdnamtab->getnode2    = gethashnode2;
    cmdnamtab->removenode  = removehashnode;
    cmdnamtab->disablenode = NULL;
    cmdnamtab->enablenode  = NULL;
    cmdnamtab->freenode    = freecmdnamnode;
    cmdnamtab->printnode   = printcmdnamnode;
#ifdef ZSH_HASH_DEBUG
    cmdnamtab->printinfo   = printhashtabinfo;
    cmdnamtab->tablename   = ztrdup("cmdnamtab");
#endif

    pathchecked = path;
}

/**/
void
emptycmdnamtable(HashTable ht)
{
    emptyhashtable(ht, INITIAL_CMDNAMTAB);
    pathchecked = path;
}

/* Add all commands in a given directory *
 * to the command hashtable.             */

/**/
void
hashdir(char **dirp)
{
    Cmdnam cn;
    DIR *dir;
    char *fn;

    if (isrelative(*dirp) || !(dir = opendir(unmeta(*dirp))))
	return;

    while ((fn = zreaddir(dir))) {
	/* Ignore `.' and `..'. */
	if (fn[0] == '.' &&
	    (fn[1] == '\0' ||
	     (fn[1] == '.' && fn[2] == '\0')))
	    continue;
	if (!cmdnamtab->getnode(cmdnamtab, fn)) {
	    cn = (Cmdnam) zcalloc(sizeof *cn);
	    cn->flags = 0;
	    cn->u.name = dirp;
	    cmdnamtab->addnode(cmdnamtab, ztrdup(fn), cn);
	}
    }
    closedir(dir);
}

/* Go through user's PATH and add everything to *
 * the command hashtable.                       */

/**/
void
fillcmdnamtable(HashTable ht)
{
    char **pq;
 
    for (pq = pathchecked; *pq; pq++)
	hashdir(pq);

    pathchecked = pq;
}

/**/
void
freecmdnamnode(HashNode hn)
{
    Cmdnam cn = (Cmdnam) hn;
 
    zsfree(cn->nam);
    if (cn->flags & HASHED)
	zsfree(cn->u.cmd);
 
    zfree(cn, sizeof(struct cmdnam));
}

/* Print an element of the cmdnamtab hash table (external command) */
 
/**/
void
printcmdnamnode(HashNode hn, int printflags)
{
    Cmdnam cn = (Cmdnam) hn;

    if ((printflags & PRINT_WHENCE_CSH) || (printflags & PRINT_WHENCE_SIMPLE)) {
	if (cn->flags & HASHED) {
	    zputs(cn->u.cmd, stdout);
	    putchar('\n');
	} else {
	    zputs(*(cn->u.name), stdout);
	    putchar('/');
	    zputs(cn->nam, stdout);
	    putchar('\n');
	}
	return;
    }

    if (printflags & PRINT_WHENCE_VERBOSE) {
	if (cn->flags & HASHED) {
	    nicezputs(cn->nam, stdout);
	    printf(" is hashed to ");
	    nicezputs(cn->u.cmd, stdout);
	    putchar('\n');
	} else {
	    nicezputs(cn->nam, stdout);
	    printf(" is ");
	    nicezputs(*(cn->u.name), stdout);
	    putchar('/');
	    nicezputs(cn->nam, stdout);
	    putchar('\n');
	}
	return;
    }

    if (cn->flags & HASHED) {
	quotedzputs(cn->nam, stdout);
	putchar('=');
	quotedzputs(cn->u.cmd, stdout);
	putchar('\n');
    } else {
	quotedzputs(cn->nam, stdout);
	putchar('=');
	quotedzputs(*(cn->u.name), stdout);
	putchar('/');
	quotedzputs(cn->nam, stdout);
	putchar('\n');
    }
}

/***************************************/
/* Shell Function Hash Table Functions */
/***************************************/

/**/
void
createshfunctable(void)
{
    shfunctab = newhashtable(7);

    shfunctab->hash        = hasher;
    shfunctab->emptytable  = NULL;
    shfunctab->filltable   = NULL;
    shfunctab->addnode     = addhashnode;
    shfunctab->getnode     = gethashnode;
    shfunctab->getnode2    = gethashnode2;
    shfunctab->removenode  = removeshfuncnode;
    shfunctab->disablenode = disableshfuncnode;
    shfunctab->enablenode  = enableshfuncnode;
    shfunctab->freenode    = freeshfuncnode;
    shfunctab->printnode   = printshfuncnode;
#ifdef ZSH_HASH_DEBUG
    shfunctab->printinfo   = printhashtabinfo;
    shfunctab->tablename   = ztrdup("shfunctab");
#endif
}

/* Remove an entry from the shell function hash table.   *
 * It checks if the function is a signal trap and if so, *
 * it will disable the trapping of that signal.          */

/**/
HashNode
removeshfuncnode(HashTable ht, char *nam)
{
    HashNode hn;

    if ((hn = removehashnode(shfunctab, nam))) {
	if (!strncmp(hn->nam, "TRAP", 4))
	    unsettrap(getsignum(hn->nam + 4));
	return hn;
    } else
	return NULL;
}

/* Disable an entry in the shell function hash table.    *
 * It checks if the function is a signal trap and if so, *
 * it will disable the trapping of that signal.          */

/**/
void
disableshfuncnode(HashNode hn, int flags)
{
    hn->flags |= DISABLED;
    if (!strncmp(hn->nam, "TRAP", 4)) {
	int signum = getsignum(hn->nam + 4);
	sigtrapped[signum] &= ~ZSIG_FUNC;
	sigfuncs[signum] = NULL;
	unsettrap(signum);
    }
}

/* Re-enable an entry in the shell function hash table.  *
 * It checks if the function is a signal trap and if so, *
 * it will re-enable the trapping of that signal.        */

/**/
void
enableshfuncnode(HashNode hn, int flags)
{
    Shfunc shf = (Shfunc) hn;
    int signum;

    shf->flags &= ~DISABLED;
    if (!strncmp(shf->nam, "TRAP", 4)) {
	signum = getsignum(shf->nam + 4);
	if (signum != -1) {
	    settrap(signum, shf->funcdef);
	    sigtrapped[signum] |= ZSIG_FUNC;
	}
    }
}

/**/
void
freeshfuncnode(HashNode hn)
{
    Shfunc shf = (Shfunc) hn;

    zsfree(shf->nam);
    if (shf->funcdef)
	freestruct(shf->funcdef);
    zfree(shf, sizeof(struct shfunc));
}

/* Print a shell function */
 
/**/
void
printshfuncnode(HashNode hn, int printflags)
{
    Shfunc f = (Shfunc) hn;
    char *t;
 
    if ((printflags & PRINT_NAMEONLY) ||
	((printflags & PRINT_WHENCE_SIMPLE) &&
	!(printflags & PRINT_WHENCE_FUNCDEF))) {
	zputs(f->nam, stdout);
	putchar('\n');
	return;
    }
 
    if ((printflags & PRINT_WHENCE_VERBOSE) &&
	!(printflags & PRINT_WHENCE_FUNCDEF)) {
	nicezputs(f->nam, stdout);
	printf(" is a shell function\n");
	return;
    }
 
    if (f->flags & PM_UNDEFINED)
	printf("undefined ");
    if (f->flags & PM_TAGGED)
	printf("traced ");
    if (!f->funcdef) {
	nicezputs(f->nam, stdout);
	printf(" () { }\n");
	return;
    }
 
    t = getpermtext((void *) dupstruct((void *) f->funcdef));
    quotedzputs(f->nam, stdout);
    printf(" () {\n\t");
    zputs(t, stdout);
    printf("\n}\n");
    zsfree(t);
}

/****************************************/
/* Builtin Command Hash Table Functions */
/****************************************/

/**/
void
createbuiltintable(void)
{
    Builtin bn;

    builtintab = newhashtable(85);

    builtintab->hash        = hasher;
    builtintab->emptytable  = NULL;
    builtintab->filltable   = NULL;
    builtintab->addnode     = addhashnode;
    builtintab->getnode     = gethashnode;
    builtintab->getnode2    = gethashnode2;
    builtintab->removenode  = NULL;
    builtintab->disablenode = disablehashnode;
    builtintab->enablenode  = enablehashnode;
    builtintab->freenode    = NULL;
    builtintab->printnode   = printbuiltinnode;
#ifdef ZSH_HASH_DEBUG
    builtintab->printinfo   = printhashtabinfo;
    builtintab->tablename   = ztrdup("builtintab");
#endif

    for (bn = builtins; bn->nam; bn++)
	builtintab->addnode(builtintab, bn->nam, bn);
}

/* Print a builtin */

/**/
void
printbuiltinnode(HashNode hn, int printflags)
{
    Builtin bn = (Builtin) hn;

    if (printflags & PRINT_WHENCE_CSH) {
	printf("%s: shell built-in command\n", bn->nam);
	return;
    }

    if (printflags & PRINT_WHENCE_VERBOSE) {
	printf("%s is a shell builtin\n", bn->nam);
	return;
    }

    /* default is name only */
    printf("%s\n", bn->nam);
}

/**************************************/
/* Reserved Word Hash Table Functions */
/**************************************/

/* Build the hash table containing zsh's reserved words. */

/**/
void
createreswdtable(void)
{
    Reswd rw;

    reswdtab = newhashtable(23);

    reswdtab->hash        = hasher;
    reswdtab->emptytable  = NULL;
    reswdtab->filltable   = NULL;
    reswdtab->addnode     = addhashnode;
    reswdtab->getnode     = gethashnode;
    reswdtab->getnode2    = gethashnode2;
    reswdtab->removenode  = NULL;
    reswdtab->disablenode = disablehashnode;
    reswdtab->enablenode  = enablehashnode;
    reswdtab->freenode    = NULL;
    reswdtab->printnode   = printreswdnode;
#ifdef ZSH_HASH_DEBUG
    reswdtab->printinfo   = printhashtabinfo;
    reswdtab->tablename   = ztrdup("reswdtab");
#endif

    for (rw = reswds; rw->nam; rw++)
	reswdtab->addnode(reswdtab, rw->nam, rw);
}

/* Print a reserved word */

/**/
void
printreswdnode(HashNode hn, int printflags)
{
    Reswd rw = (Reswd) hn;

    if (printflags & PRINT_WHENCE_CSH) {
	printf("%s: shell reserved word\n", rw->nam);
	return;
    }

    if (printflags & PRINT_WHENCE_VERBOSE) {
	printf("%s is a reserved word\n", rw->nam);
	return;
    }

    /* default is name only */
    printf("%s\n", rw->nam);
}

/********************************/
/* Aliases Hash Table Functions */
/********************************/

/* Create new hash table for aliases */

/**/
void
createaliastable(void)
{
    aliastab = newhashtable(23);

    aliastab->hash        = hasher;
    aliastab->emptytable  = NULL;
    aliastab->filltable   = NULL;
    aliastab->addnode     = addhashnode;
    aliastab->getnode     = gethashnode;
    aliastab->getnode2    = gethashnode2;
    aliastab->removenode  = removehashnode;
    aliastab->disablenode = disablehashnode;
    aliastab->enablenode  = enablehashnode;
    aliastab->freenode    = freealiasnode;
    aliastab->printnode   = printaliasnode;
#ifdef ZSH_HASH_DEBUG
    aliastab->printinfo   = printhashtabinfo;
    aliastab->tablename   = ztrdup("aliastab");
#endif

    /* add the default aliases */
    aliastab->addnode(aliastab, ztrdup("run-help"), createaliasnode(ztrdup("man"), 0));
    aliastab->addnode(aliastab, ztrdup("which-command"), createaliasnode(ztrdup("whence"), 0));
}

/* Create a new alias node */

/**/
Alias
createaliasnode(char *txt, int flags)
{
    Alias al;

    al = (Alias) zcalloc(sizeof *al);
    al->flags = flags;
    al->text = txt;
    al->inuse = 0;
    return al;
}

/**/
void
freealiasnode(HashNode hn)
{
    Alias al = (Alias) hn;
 
    zsfree(al->nam);
    zsfree(al->text);
    zfree(al, sizeof(struct alias));
}

/* Print an alias */

/**/
void
printaliasnode(HashNode hn, int printflags)
{
    Alias a = (Alias) hn;

    if (printflags & PRINT_NAMEONLY) {
	zputs(a->nam, stdout);
	putchar('\n');
	return;
    }

    if (printflags & PRINT_WHENCE_SIMPLE) {
	zputs(a->text, stdout);
	putchar('\n');
	return;
    }

    if (printflags & PRINT_WHENCE_CSH) {
	nicezputs(a->nam, stdout);
	if (a->flags & ALIAS_GLOBAL)
	    printf(": globally aliased to ");
	else
	    printf(": aliased to ");
	nicezputs(a->text, stdout);
	putchar('\n');
	return;
    }

    if (printflags & PRINT_WHENCE_VERBOSE) {
	nicezputs(a->nam, stdout);
	if (a->flags & ALIAS_GLOBAL)
	    printf(" is a global alias for ");
	else
	    printf(" is an alias for ");
	nicezputs(a->text, stdout);
	putchar('\n');
	return;
    }

    if (printflags & PRINT_LIST) {
	printf("alias ");
	if (a->flags & ALIAS_GLOBAL)
	    printf("-g ");

	/* If an alias begins with `-', then we must output `-- ' *
	 * first, so that it is not interpreted as an option.     */
	if(a->nam[0] == '-')
	    printf("-- ");
    }

    quotedzputs(a->nam, stdout);
    putchar('=');
    quotedzputs(a->text, stdout);
    putchar('\n');
}

/**********************************/
/* Parameter Hash Table Functions */
/**********************************/

/**/
void
freeparamnode(HashNode hn)
{
    Param pm = (Param) hn;
 
    zsfree(pm->nam);
    zfree(pm, sizeof(struct param));
}

/* Print a parameter */

/**/
void
printparamnode(HashNode hn, int printflags)
{
#ifdef ZSH_64_BIT_TYPE
    static char llbuf[DIGBUFSIZE];
#endif
    Param p = (Param) hn;
    char *t, **u;

    if (p->flags & PM_UNSET)
	return;

    /* Print the attributes of the parameter */
    if (printflags & PRINT_TYPE) {
	if (p->flags & PM_INTEGER)
	    printf("integer ");
	if (p->flags & PM_ARRAY)
	    printf("array ");
	if (p->flags & PM_LEFT)
	    printf("left justified %d ", p->ct);
	if (p->flags & PM_RIGHT_B)
	    printf("right justified %d ", p->ct);
	if (p->flags & PM_RIGHT_Z)
	    printf("zero filled %d ", p->ct);
	if (p->flags & PM_LOWER)
	    printf("lowercase ");
	if (p->flags & PM_UPPER)
	    printf("uppercase ");
	if (p->flags & PM_READONLY)
	    printf("readonly ");
	if (p->flags & PM_TAGGED)
	    printf("tagged ");
	if (p->flags & PM_EXPORTED)
	    printf("exported ");
    }

    if (printflags & PRINT_NAMEONLY) {
	zputs(p->nam, stdout);
	putchar('\n');
	return;
    }

    /* How the value is displayed depends *
     * on the type of the parameter       */
    quotedzputs(p->nam, stdout);
    putchar('=');
    switch (PM_TYPE(p->flags)) {
    case PM_SCALAR:
	/* string: simple output */
	if (p->gets.cfn && (t = p->gets.cfn(p)))
	    quotedzputs(t, stdout);
	putchar('\n');
	break;
    case PM_INTEGER:
	/* integer */
#ifdef ZSH_64_BIT_TYPE
	convbase(llbuf, p->gets.ifn(p), 0);
	puts(llbuf);
#else
	printf("%ld\n", p->gets.ifn(p));
#endif
	break;
    case PM_ARRAY:
	/* array */
	putchar('(');
	u = p->gets.afn(p);
	if(*u) {
	    quotedzputs(*u++, stdout);
	    while (*u) {
		putchar(' ');
		quotedzputs(*u++, stdout);
	    }
	}
	printf(")\n");
	break;
    }
}

/****************************************/
/* Named Directory Hash Table Functions */
/****************************************/

/* size of the initial name directory hash table */
#define INITIAL_NAMEDDIR 201

/* != 0 if all the usernames have already been *
 * added to the named directory hash table.    */
int allusersadded;

/* Create new hash table for named directories */

/**/
void
createnameddirtable(void)
{
    nameddirtab = newhashtable(INITIAL_NAMEDDIR);

    nameddirtab->hash        = hasher;
    nameddirtab->emptytable  = emptynameddirtable;
    nameddirtab->filltable   = fillnameddirtable;
    nameddirtab->addnode     = addnameddirnode;
    nameddirtab->getnode     = gethashnode;
    nameddirtab->getnode2    = gethashnode2;
    nameddirtab->removenode  = removenameddirnode;
    nameddirtab->disablenode = NULL;
    nameddirtab->enablenode  = NULL;
    nameddirtab->freenode    = freenameddirnode;
    nameddirtab->printnode   = printnameddirnode;
#ifdef ZSH_HASH_DEBUG
    nameddirtab->printinfo   = printhashtabinfo;
    nameddirtab->tablename   = ztrdup("nameddirtab");
#endif

    allusersadded = 0;
    finddir(NULL);		/* clear the finddir cache */
}

/* Empty the named directories table */

/**/
void
emptynameddirtable(HashTable ht)
{
    emptyhashtable(ht, INITIAL_NAMEDDIR);
    allusersadded = 0;
    finddir(NULL);		/* clear the finddir cache */
}

/* Add all the usernames in the password file/database *
 * to the named directories table.                     */

/**/
void
fillnameddirtable(HashTable ht)
{
    if (!allusersadded) {
	struct passwd *pw;
 
	setpwent();
 
	/* loop through the password file/database *
	 * and add all entries returned.           */
	while ((pw = getpwent()) && !errflag)
	    adduserdir(ztrdup(pw->pw_name), pw->pw_dir, ND_USERNAME, 1);
 
	endpwent();
	allusersadded = 1;
    }
    return;
}

/* Add an entry to the named directory hash *
 * table, clearing the finddir() cache and  *
 * initialising the `diff' member.          */

/**/
void
addnameddirnode(HashTable ht, char *nam, void *nodeptr)
{
    Nameddir nd = (Nameddir) nodeptr;

    nd->diff = strlen(nd->dir) - strlen(nam);
    finddir(NULL);		/* clear the finddir cache */
    addhashnode(ht, nam, nodeptr);
}

/* Remove an entry from the named directory  *
 * hash table, clearing the finddir() cache. */

/**/
HashNode
removenameddirnode(HashTable ht, char *nam)
{
    HashNode hn = removehashnode(ht, nam);

    if(hn)
	finddir(NULL);		/* clear the finddir cache */
    return hn;
}

/* Free up the memory used by a named directory hash node. */

/**/
void
freenameddirnode(HashNode hn)
{
    Nameddir nd = (Nameddir) hn;
 
    zsfree(nd->nam);
    zsfree(nd->dir);
    zfree(nd, sizeof(struct nameddir));
}

/* Print a named directory */

/**/
void
printnameddirnode(HashNode hn, int printflags)
{
    Nameddir nd = (Nameddir) hn;

    if (printflags & PRINT_NAMEONLY) {
	zputs(nd->nam, stdout);
	putchar('\n');
	return;
    }

    quotedzputs(nd->nam, stdout);
    putchar('=');
    quotedzputs(nd->dir, stdout);
    putchar('\n');
}

/********************************/
/* Compctl Hash Table Functions */
/********************************/

/**/
void
createcompctltable(void)
{
    compctltab = newhashtable(23);

    compctltab->hash        = hasher;
    compctltab->emptytable  = NULL;
    compctltab->filltable   = NULL;
    compctltab->addnode     = addhashnode;
    compctltab->getnode     = gethashnode2;
    compctltab->getnode2    = gethashnode2;
    compctltab->removenode  = removehashnode;
    compctltab->disablenode = NULL;
    compctltab->enablenode  = NULL;
    compctltab->freenode    = freecompctlp;
    compctltab->printnode   = printcompctlp;
#ifdef ZSH_HASH_DEBUG
    compctltab->printinfo   = printhashtabinfo;
    compctltab->tablename   = ztrdup("compctltab");
#endif
}

/**/
void
freecompctl(Compctl cc)
{
    if (cc == &cc_default ||
 	cc == &cc_first ||
	cc == &cc_compos ||
	--cc->refc > 0)
	return;

    zsfree(cc->keyvar);
    zsfree(cc->glob);
    zsfree(cc->str);
    zsfree(cc->func);
    zsfree(cc->explain);
    zsfree(cc->prefix);
    zsfree(cc->suffix);
    zsfree(cc->hpat);
    zsfree(cc->subcmd);
    if (cc->cond)
	freecompcond(cc->cond);
    if (cc->ext) {
	Compctl n, m;

	n = cc->ext;
	do {
	    m = (Compctl) (n->next);
	    freecompctl(n);
	    n = m;
	}
	while (n);
    }
    if (cc->xor && cc->xor != &cc_default)
	freecompctl(cc->xor);
    zfree(cc, sizeof(struct compctl));
}

/**/
void
freecompctlp(HashNode hn)
{
    Compctlp ccp = (Compctlp) hn;

    zsfree(ccp->nam);
    freecompctl(ccp->cc);
    zfree(ccp, sizeof(struct compctlp));
}

/***********************************************************/
/* Emacs Multi-Character Key Bindings Hash Table Functions */
/***********************************************************/

/* size of the initial hashtable for multi-character *
 * emacs key bindings.                               */
#define INITIAL_EMKEYBINDTAB 67

/**/
void
createemkeybindtable(void)
{
    emkeybindtab = newhashtable(INITIAL_EMKEYBINDTAB);

    emkeybindtab->hash        = hasher;
    emkeybindtab->emptytable  = emptyemkeybindtable;
    emkeybindtab->filltable   = NULL;
    emkeybindtab->addnode     = addhashnode;
    emkeybindtab->getnode     = gethashnode2;
    emkeybindtab->getnode2    = gethashnode2;
    emkeybindtab->removenode  = removehashnode;
    emkeybindtab->disablenode = NULL;
    emkeybindtab->enablenode  = NULL;
    emkeybindtab->freenode    = freekeynode;

    /* need to combine printbinding and printfuncbinding for this */
    emkeybindtab->printnode   = NULL;
#ifdef ZSH_HASH_DEBUG
    emkeybindtab->printinfo   = printhashtabinfo;
    emkeybindtab->tablename   = ztrdup("emkeybindtab");
#endif
}

/**/
void
emptyemkeybindtable(HashTable ht)
{
    emptyhashtable(ht, INITIAL_EMKEYBINDTAB);
}

/********************************************************/
/* Vi Multi-Character Key Bindings Hash Table Functions */
/********************************************************/

/* size of the initial hash table for *
 * multi-character vi key bindings.   */
#define INITIAL_VIKEYBINDTAB 20

/**/
void
createvikeybindtable(void)
{
    vikeybindtab = newhashtable(INITIAL_VIKEYBINDTAB);

    vikeybindtab->hash        = hasher;
    vikeybindtab->emptytable  = emptyvikeybindtable;
    vikeybindtab->filltable   = NULL;
    vikeybindtab->addnode     = addhashnode;
    vikeybindtab->getnode     = gethashnode2;
    vikeybindtab->getnode2    = gethashnode2;
    vikeybindtab->removenode  = removehashnode;
    vikeybindtab->disablenode = NULL;
    vikeybindtab->enablenode  = NULL;
    vikeybindtab->freenode    = freekeynode;

    /* need to combine printbinding and printfuncbinding for this */
    vikeybindtab->printnode   = NULL;
#ifdef ZSH_HASH_DEBUG
    vikeybindtab->printinfo   = printhashtabinfo;
    vikeybindtab->tablename   = ztrdup("vikeybindtab");
#endif
}

/**/
void
emptyvikeybindtable(HashTable ht)
{
    emptyhashtable(ht, INITIAL_VIKEYBINDTAB);
}
