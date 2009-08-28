/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*     fastcp.c
       Original - Bertrand Jan 92
       Updated for use in app_makefiles - Mike Monegan Apr 92
       Ported to NT - Mike Monegan May 95
       Ported to Solaris & HPUX - Mike Monegan Jan 96
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/mman.h>	# for mmap()
#ifdef __APPLE__
#include <libc.h>
#include <sys/dir.h>
#include <errno.h>
typedef struct direct DIRENT;
#endif

#if defined(sun) || defined(hpux)
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
typedef struct dirent DIRENT;

extern const char * sys_errlist[];                        // Sun & HP Bug
#ifdef sun
extern int utimes(const char *file, struct timeval *tvp); // Sun Bug
#endif
#endif

#define IS_DIRECTORY(st_mode)	(((st_mode) & S_IFMT) == S_IFDIR)
#define IS_LINK(st_mode)	(((st_mode) & S_IFMT) == S_IFLNK)
#define IS_REGULAR(st_mode)	(((st_mode) & S_IFMT) == S_IFREG)

#define PROGRAM_NAME "fastcp"

static void fastcp1(const char *source, const char *dest);
static void copy_destmissing(const char *source, struct stat *sstat, const char *dest);
static void blast(const char *dest);

/************************ hashtable ***************/

unsigned _StrHash (const void *data) {
    register unsigned	hash = 0;
    register unsigned char	*s = (unsigned char *) data;
    /* unsigned to avoid a sign-extend */
    /* unroll the loop */
    if (s) for (; ; ) { 
	if (*s == '\0') break;
	hash ^= (unsigned) *s++;
	if (*s == '\0') break;
	hash ^= (unsigned) *s++ << 8;
	if (*s == '\0') break;
	hash ^= (unsigned) *s++ << 16;
	if (*s == '\0') break;
	hash ^= (unsigned) *s++ << 24;
	}
    return hash;
}
    
int _StrIsEqual (const void *data1, const void *data2) {
    if (data1 == data2) return 1;
    if (! data1) return ! strlen ((char *) data2);
    if (! data2) return ! strlen ((char *) data1);
    if (((char *) data1)[0] != ((char *) data2)[0]) return 0;
    return (strcmp ((char *) data1, (char *) data2)) ? 0 : 1;
}
    
typedef struct {
    unsigned			count;
    unsigned			nbBuckets;
    void			*buckets;
   } NXHashTable;
    /* private data structure; may change */
    
extern NXHashTable *NXCreateHashTable (void);

extern void NXFreeHashTable (NXHashTable *table);
    /* calls free for each data, and recovers table */
	
extern void *NXHashGet (NXHashTable *table, const void *data);
    /* return original table data or NULL.
    Example of use when the hashed data is a struct containing the key,
    and when the callee only has a key:
	MyStruct	pseudo;
	MyStruct	*original;
	pseudo.key = myKey;
	original = NXHashGet (myTable, &pseudo)
    */
	
extern void *NXHashInsert (NXHashTable *table, const void *data);
    /* previous data or NULL is returned. */
	
extern void *NXHashRemove (NXHashTable *table, const void *data);
    /* previous data or NULL is returned */
	
/* Iteration over all elements of a table consists in setting up an iteration state and then to progress until all entries have been visited.  An example of use for counting elements in a table is:
    unsigned	count = 0;
    MyData	*data;
    NXHashState	state = NXInitHashState(table);
    while (NXNextHashState(table, &state, &data)) {
	count++;
    }
*/

typedef struct {int i; int j;} NXHashState;
    /* callers should not rely on actual contents of the struct */

extern NXHashState NXInitHashState(NXHashTable *table);

extern int NXNextHashState(NXHashTable *table, NXHashState *state, void **data);
    /* returns 0 when all elements have been visited */
/* In order to improve efficiency, buckets contain a pointer to an array or directly the data when the array size is 1 */
typedef union {
    const void	*one;
    const void	**many;
    } oneOrMany;
    /* an optimization consists of storing directly data when count = 1 */
    
typedef struct	{
    unsigned 	count; 
    oneOrMany	elements;
    } HashBucket;
    /* private data structure; may change */
    
#define	PTRSIZE		sizeof(void *)

#define	ALLOCTABLE()	((NXHashTable *) malloc(sizeof (NXHashTable)))
#define	ALLOCBUCKETS(nb)((HashBucket *) calloc(nb, sizeof (HashBucket)))
#define	ALLOCPAIRS(nb) ((const void **) calloc(nb, sizeof (void *)))

/* iff necessary this modulo can be optimized since the nbBuckets is of the form 2**n-1 */
#define	BUCKETOF(table, data) (((HashBucket *)table->buckets)+(_StrHash(data) % table->nbBuckets))

#define ISEQUAL(table, data1, data2) ((data1 == data2) || _StrIsEqual(data1, data2))
	/* beware of double evaluation */
	
/*************************************************************************
 *
 *	Global data and bootstrap
 *	
 *************************************************************************/
 
void NXNoEffectFree (const void *info, void *data) {};

int NXPtrIsEqual (const void *info, const void *data1, const void *data2) {
    return data1 == data2;
    };

/*************************************************************************
 *
 *	On z'y va
 *	
 *************************************************************************/

NXHashTable *NXCreateHashTable(void) {
    NXHashTable			*table;
    table = ALLOCTABLE();
    table->count = 0;
    table->nbBuckets = 1;
    table->buckets = ALLOCBUCKETS(table->nbBuckets);
    return table;
}

static void freeBuckets (NXHashTable *table, int freeObjects) {
    unsigned		i = table->nbBuckets;
    HashBucket		*buckets = (HashBucket *) table->buckets;
    
    while (i--) {
	if (buckets->count) {
	    if (buckets->count > 1) free(buckets->elements.many);
	    buckets->count = 0;
	    buckets->elements.one = NULL;
	    };
	buckets++;
	};
}
    
void NXFreeHashTable (NXHashTable *table) {
    freeBuckets (table, 1);
    free (table->buckets);
    free (table);
}
    
void *NXHashGet (NXHashTable *table, const void *data) {
    HashBucket	*bucket = BUCKETOF(table, data);
    unsigned	j = bucket->count;
    const void	**pairs;
    
    if (! j) return NULL;
    if (j == 1) {
    	return ISEQUAL(table, data, bucket->elements.one)
	    ? (void *) bucket->elements.one : NULL; 
	};
    pairs = bucket->elements.many;
    while (j--) {
	/* we don't cache isEqual because lists are short */
    	if (ISEQUAL(table, data, *pairs)) return (void *) *pairs; 
	pairs ++;
	};
    return NULL;
}

static void _NXHashRehash (NXHashTable *table) {
    /* Rehash: we create a pseudo table pointing really to the old guys,
    extend self, copy the old pairs, and free the pseudo table */
    NXHashTable	*old;
    NXHashState	state;
    void	*aux;
    old = ALLOCTABLE();
    old->count = table->count; 
    old->nbBuckets = table->nbBuckets; old->buckets = table->buckets;
    table->nbBuckets += table->nbBuckets + 1; /* 2 times + 1 */
    table->count = 0; table->buckets = ALLOCBUCKETS(table->nbBuckets);
    state = NXInitHashState (old);
    while (NXNextHashState (old, &state, &aux))
	(void) NXHashInsert (table, aux);
    freeBuckets (old, 0);
#ifdef UNUSED
    if (old->count != table->count)
	_NXLogError("*** hashtable: count differs after rehashing; probably indicates a broken invariant: there are x and y such as isEqual(x, y) is TRUE but hash(x) != hash (y)\n");
#endif
    free (old->buckets); 
    free (old);
}

void *NXHashInsert (NXHashTable *table, const void *data) {
    HashBucket	*bucket = BUCKETOF(table, data);
    unsigned	j = bucket->count;
    const void	**pairs;
    const void	**new;
    if (! j) {
	bucket->count++; bucket->elements.one = data; 
	table->count++; 
	return NULL;
	};
    if (j == 1) {
    	if (ISEQUAL(table, data, bucket->elements.one)) {
	    const void	*old = bucket->elements.one;
	    bucket->elements.one = data;
	    return (void *) old;
	    };
	new = ALLOCPAIRS(2);
	new[1] = bucket->elements.one;
	*new = data;
	bucket->count++; bucket->elements.many = new; 
	table->count++; 
	if (table->count > table->nbBuckets) _NXHashRehash (table);
	return NULL;
	};
    pairs = bucket->elements.many;
    while (j--) {
	/* we don't cache isEqual because lists are short */
    	if (ISEQUAL(table, data, *pairs)) {
	    const void	*old = *pairs;
	    *pairs = data;
	    return (void *) old;
	    };
	pairs ++;
	};
    /* we enlarge this bucket; and put new data in front */
    new = ALLOCPAIRS(bucket->count+1);
    if (bucket->count) bcopy ((const char*)bucket->elements.many, (char*)(new+1), bucket->count * PTRSIZE);
    *new = data;
    free (bucket->elements.many);
    bucket->count++; bucket->elements.many = new; 
    table->count++; 
    if (table->count > table->nbBuckets) _NXHashRehash (table);
    return NULL;
}

void *NXHashRemove (NXHashTable *table, const void *data) {
    HashBucket	*bucket = BUCKETOF(table, data);
    unsigned	j = bucket->count;
    const void	**pairs;
    const void	**new;
    if (! j) return NULL;
    if (j == 1) {
	if (! ISEQUAL(table, data, bucket->elements.one)) return NULL;
	data = bucket->elements.one;
	table->count--; bucket->count--; bucket->elements.one = NULL;
	return (void *) data;
	};
    pairs = bucket->elements.many;
    if (j == 2) {
    	if (ISEQUAL(table, data, pairs[0])) {
	    bucket->elements.one = pairs[1]; data = pairs[0];
	    }
	else if (ISEQUAL(table, data, pairs[1])) {
	    bucket->elements.one = pairs[0]; data = pairs[1];
	    }
	else return NULL;
	free (pairs);
	table->count--; bucket->count--;
	return (void *) data;
	};
    while (j--) {
    	if (ISEQUAL(table, data, *pairs)) {
	    data = *pairs;
	    /* we shrink this bucket */
	    new = (bucket->count-1) 
		? ALLOCPAIRS(bucket->count-1) : NULL;
	    if (bucket->count-1 != j)
		    bcopy ((const char*)bucket->elements.many, (char*)new, PTRSIZE*(bucket->count-j-1));
	    if (j)
		    bcopy ((const char*)(bucket->elements.many + bucket->count-j), (char*)(new+bucket->count-j-1), PTRSIZE*j);
	    free (bucket->elements.many);
	    table->count--; bucket->count--; bucket->elements.many = new;
	    return (void *) data;
	    };
	pairs ++;
	};
    return NULL;
}

NXHashState NXInitHashState (NXHashTable *table) {
    NXHashState	state;
    
    state.i = table->nbBuckets;
    state.j = 0;
    return state;
}
    
int NXNextHashState (NXHashTable *table, NXHashState *state, void **data) {
    HashBucket		*buckets = (HashBucket *) table->buckets;
    
    while (state->j == 0) {
	if (state->i == 0) return 0;
	state->i--; state->j = buckets[state->i].count;
	}
    state->j--;
    buckets += state->i;
    *data = (void *) ((buckets->count == 1) 
    		? buckets->elements.one : buckets->elements.many[state->j]);
    return 1;
}

/**************************** fastcp itself *******************************/

static void check(int ret, const char *format, ...) {
    va_list	args;
    if (!ret) return;
    printf("%s: ", PROGRAM_NAME);
    va_start(args, format);
    vprintf(format, args);
#ifdef DEBUG
    printf(" : %s\n", sys_errlist[errno]);
#else
    printf("\n");
#endif
    exit(ret);
}

static void catdirfile(char *dirfile, const char *dir, const char *file) {
    strcpy(dirfile, dir); strcat(dirfile, "/"); strcat(dirfile, file);
}

static void checkdest(const char *dest) {
    struct stat	dstat;
    check(stat(dest, &dstat), "Destination '%s' does not exist.", dest);
    if (!IS_DIRECTORY(dstat.st_mode)) {
	printf("%s: Destination '%s' is not a directory\n", PROGRAM_NAME, dest);
	exit(-3);
    }
    check(access(dest, W_OK), "Destination '%s' is not writable directory.", dest);
}

static void settimes(const char *path, time_t mtime) {
#ifdef hpux
   struct utimbuf  times;
   times.actime = mtime;
   times.modtime = mtime;
   check(utime(path, &times), "Can't set time on '%s'.", path);
#else
    struct timeval  tv[2];
    tv[0].tv_sec = time(NULL);
    tv[1].tv_sec = mtime;
    tv[0].tv_usec = tv[1].tv_usec = 0;
    check(utimes(path, tv), "Can't set time on '%s'.", path);
#endif    
}

static void copy_dir_destmissing(const char *source, struct stat *sstat, const char *dest) {
    /* we know source is a directory */
    /* we know dest does not exist, but its parent is a directory */
    DIRENT	*dp;
    DIR		*dirp;
    check(mkdir(dest, 0777), "Can't create '%s'.", dest);
    dirp = opendir(source);
    check(!dirp, "Can't open directory '%s'.", source);
    dp = readdir(dirp); /* Skip . */
    dp = readdir(dirp); /* Skip .. */
    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	char	subsource[MAXPATHLEN+1];
	char	subdest[MAXPATHLEN+1];
	struct stat	cstat;
	catdirfile(subsource, source, dp->d_name);
	catdirfile(subdest, dest, dp->d_name);
	check(stat(subsource, &cstat), "'%s' does not exist", subsource);
	check(access(subsource, R_OK), "Cannot read '%s'", subsource);
	copy_destmissing(subsource, &cstat, subdest);
    }
    closedir(dirp);
    settimes(dest, sstat->st_mtime);
    check(chmod(dest, sstat->st_mode), "Cannot protect '%s'.", dest);
}

static void copy_file_destmissing(const char *source, struct stat *sstat, const char *dest) {
    int		sfd;
    int		dfd;
    
#ifdef TRY_TO_LINK
    if (!link(source, dest)) return;
#endif
    (void)unlink(dest);
    sfd = open(source, O_RDONLY, 0666);
    if (sfd < 0) {
	printf("%s: cannot open '%s' : %s\n", 
	        PROGRAM_NAME, source, sys_errlist[errno]);
	exit(-4);
    }
    dfd = open(dest, O_TRUNC | O_CREAT | O_WRONLY, sstat->st_mode & 0777);
    if (dfd < 0) {
	printf("%s: cannot create '%s' : %s\n", 
		PROGRAM_NAME, dest, sys_errlist[errno]);
	exit(-5);
    }
    if (sstat->st_size) {
	void *addr = mmap(NULL, (size_t)sstat->st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, sfd, 0);
	check(addr == MAP_FAILED, "cannot map file '%s'.", source);
	if (write(dfd, (char *)addr, (size_t)sstat->st_size) != sstat->st_size) {
	    printf("%s: Error writing %s\n", PROGRAM_NAME, dest);
	    exit(-6);
	}
	munmap(addr, (size_t)sstat->st_size);
    }
    check(close(sfd), "Error closing %s", source);
    check(close(dfd), "Error closing %s", dest);
    settimes(dest, sstat->st_mtime);
}

static void copy_destmissing(const char *source, struct stat *sstat, const char *dest) {
    /* we know dest does not exist, but its parent is a directory */
    if (IS_LINK(sstat->st_mode)) {
	check(stat(source, sstat), "'%s' does not exist.", source);
    }
    if (IS_DIRECTORY(sstat->st_mode)) {
	return copy_dir_destmissing(source, sstat, dest);
    }
    return copy_file_destmissing(source, sstat, dest);
}

static void blast_stated(const char *dest, struct stat *dstat) {
    if (IS_DIRECTORY(dstat->st_mode)) {
       DIRENT	*dp;
	DIR	*dirp;
	chmod(dest, 0777); /* We chmod dest in order to blast it */
	dirp = opendir(dest);
	check(!dirp, "Can't open directory '%s'.", dest);
	dp = readdir(dirp); /* Skip . */
	dp = readdir(dirp); /* Skip .. */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    char	subdest[MAXPATHLEN+1];
	    catdirfile(subdest, dest, dp->d_name);
	    blast(subdest);
	}
	closedir(dirp);
	check(rmdir(dest), "Cannot remove directory '%s'.", dest);
    } else {
	check(unlink(dest), "Cannot unlink '%s'.", dest);
    }
}

static void blast(const char *dest) {
    struct stat	dstat;
    /* we save the time of a stat by just going for it */
    if (!unlink(dest)) return;
    check(stat(dest, &dstat), "'%s' does not exist.", dest);
    blast_stated(dest, &dstat);
}

static void merge_dir_dir(const char *source, struct stat *sstat, const char *dest, struct stat *dstat) {
    /* we first put all names into a hashtable */
    DIRENT	*dp;
    DIR		*dirp;
    char	*names = malloc((size_t)sstat->st_size); /* this will be big enough for all names */
    char	*name = names;
    NXHashTable		*table = NXCreateHashTable();
    NXHashState	state;
    struct stat	cstat;

    dirp = opendir(source);
    check(!dirp, "Can't open directory '%s'.", source);
    dp = readdir(dirp); /* Skip . */
    dp = readdir(dirp); /* Skip .. */
    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	strcpy(name, dp->d_name);
	NXHashInsert(table, name);
#ifdef __svr4__
        name += strlen(dp->d_name) + 1;
#else        
        name += dp->d_namlen + 1;
#endif        
    }
    closedir(dirp);
    chmod(dest, 0777); /* We chmod dest in order to fill it */
    dirp = opendir(dest);
    check(!dirp, "Can't open directory '%s'.", dest);
    dp = readdir(dirp); /* Skip . */
    dp = readdir(dirp); /* Skip .. */
    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	char	subsource[MAXPATHLEN+1];
	char	subdest[MAXPATHLEN+1];
	catdirfile(subdest, dest, dp->d_name);
	if (NXHashGet(table, dp->d_name)) {
	    /* recurse! */
	    catdirfile(subsource, source, dp->d_name);
	    fastcp1(subsource, subdest);

/*	    check(stat(subsource, &cstat), "Cannot access '%s'", subsource);
	    check(access(subsource, R_OK), "Cannot access '%s'", subsource);
	    copy_destmissing(subsource, &cstat, subdest);
*/
	    NXHashRemove(table, dp->d_name); /* done ! */
	} else {
	    /* blast */
	    blast(subdest);
	}
    }
    closedir(dirp);
    /* in table only the ones that are new now */
    state = NXInitHashState(table);
    while (NXNextHashState(table, &state, (void **)&name)) {
	char	subsource[MAXPATHLEN+1];
	char	subdest[MAXPATHLEN+1];
	catdirfile(subdest, dest, name);
	catdirfile(subsource, source, name);
	check(stat(subsource, &cstat), "'%s' does not exist.", subsource);
	check(access(subsource, R_OK), "'%s' does not exist.", subsource);
	copy_destmissing(subsource, &cstat, subdest);
    }
    NXFreeHashTable(table);
    free(names);
    settimes(dest, sstat->st_mtime);
    check(chmod(dest, sstat->st_mode), "Cannot protect '%s'.", dest);
}

static void fastcp1(const char *source, const char *dest) {
    struct stat	sstat;
    struct stat	dstat;
    check(stat(source, &sstat), "'%s' does not exist.", source);
    check(access(source, R_OK), "'%s' does not exist.", source);
    if (lstat(dest, &dstat)) {  /* Note that we want to copy over a link */
	/* it's ok if dest does not exist */
    	return copy_destmissing(source, &sstat, dest);
    }
    if (IS_DIRECTORY(sstat.st_mode) && IS_DIRECTORY(dstat.st_mode)) {
    	return merge_dir_dir(source, &sstat, dest, &dstat);
    }
    if (IS_REGULAR(sstat.st_mode) && IS_REGULAR(dstat.st_mode) && (sstat.st_mtime == dstat.st_mtime) && (sstat.st_size == dstat.st_size)) {
	/* same date, same size, regular files, just trust */
	if (sstat.st_mode != dstat.st_mode) {
	    check(chmod(dest, sstat.st_mode), "Cannot protect '%s'.", dest);
	}
	return;
    }

    /* links or special files */
    blast_stated(dest, &dstat);
    copy_destmissing(source, &sstat, dest);
}

int main(int argc, char *argv[]) {
    unsigned	index = 1;
    char	wd[MAXPATHLEN+1];
    char	dest[MAXPATHLEN+1];

    if (argc < 2) {
	printf("usage: %s <source>1 ... <source>n <dest>\n", PROGRAM_NAME);
	printf("(recursively copies source files (or directories) to dest directory,\n");
#ifdef TRY_TO_LINK
	printf("when source and dest are on the same device, hard links are created\n");
#endif
	printf("but does nothing when type, time, and size of a file match.)\n");
	exit(-1);
    }
#if defined(sun) || defined(hpux)
    if (!getcwd(wd, MAXPATHLEN+1)) {
#else
    if (!getwd(wd)) {
#endif       
	printf("%s: Can't find wd.\n", PROGRAM_NAME);
	exit(-2);
    }
    if (argv[argc-1][0] == '/') {
	strcpy(dest, argv[argc-1]);
    } else {
	catdirfile(dest, wd, argv[argc-1]);
    }
    checkdest(dest);
    while (index < argc - 1) {
	char	source[MAXPATHLEN+1];
	char	dd[MAXPATHLEN+1];
	char	*file;
	if (argv[index][0] == '/') {
	    strcpy(source, argv[index]);
	} else {
	    catdirfile(source, wd, argv[index]);
	}
	file = strrchr(source, '/');
	catdirfile(dd, dest, file);
	fastcp1(source, dd);
	index++;
    }
    exit(0);
}
