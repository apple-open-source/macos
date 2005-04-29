/*
 * PSres.c
 *
 * (c) Copyright 1991-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/psres/PSres.c,v 1.5 2003/05/27 22:26:51 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>

#ifdef XENVIRONMENT
#include <X11/Xos.h>
#else
#include <string.h>
#include <sys/types.h>
#endif

#define USE_POSIX

#ifdef NeXT
#undef USE_POSIX
#endif /* NeXT */

#ifdef USE_POSIX
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

#include <sys/stat.h>
#include <DPS/PSres.h>

#ifndef PSRES_NAME
#define PSRES_NAME "PSres.upr"
#endif /* PSRES_NAME */

#ifndef PSRES_EXT
#define PSRES_EXT ".upr"
#endif /* PSRES_EXT */

#ifndef SEEK_SET
#define SEEK_SET 0
#endif /* SEEK_SET */

/* MT is the right pointer type to pass to memcpy, MST the size type */
#define MT void *
#define MST size_t

#define MAXLEN 256
typedef int (*ReadContentsFunction)();

char *PSResFontOutline = "FontOutline",
     *PSResFontPrebuilt = "FontPrebuilt",
     *PSResFontAFM = "FontAFM",
     *PSResFontBDF = "FontBDF",
     *PSResFontFamily = "FontFamily",
     *PSResFontBDFSizes = "FontBDFSizes",
     *PSResForm = "Form",
     *PSResPattern = "Pattern",
     *PSResEncoding = "Encoding",
     *PSResProcSet = "ProcSet";

typedef struct _ResourceNameStruct {
    char *name;
    char *file;
} ResourceNameStruct, *ResourceName;

typedef struct _ResourceTypeStruct {
    char *type;
    long fileOffset;	/* -1 means not really in the file */
    ResourceName names;
    int nameCount;
    int filled;		/* Whether has all names or just already found names */
    char *nameBuffer;
    char **oldNameBuffers;
    int oldNameCount;
} ResourceTypeStruct, *ResourceType;

typedef struct _ResourceDirectoryStruct {
    char *directory;
    ResourceType types;
    int typeCount;
    char *typeNameBuffer;
    char *filePrefix;
    long endOfHeader;
    int exclusive;
    struct _ResourceDirectoryStruct *next;
} ResourceDirectoryStruct, *ResourceDirectory;

typedef struct {
    char *type;
    char *name;
} TypeName;

typedef struct {
    PSResourceEnumerator func;
    char *type;
    char *name;
    char *private;
    int done;
} EnumeratorStruct;

static ResourceDirectory resDir = NULL, lastResDir;
static char *savedPathOverride = NULL, *savedDefaultPath = NULL;
static PSResourceSavePolicy currentPolicy = PSSaveByType;
static char **currentResourceTypes = NULL;
static char *resourceTypeBuffer = NULL;
static time_t lastModifiedTime;
static char nullStr = '\0';

/* Wrappers */

static char *mymalloc(size)
    int size;
{
    char *ret;

#ifdef XENVIRONMENT
#ifdef MALLOC_0_RETURNS_NULL
    if (size < 1) size = 1;
#endif
#else
    if (size < 1) size = 1;			/* Better safe than sorry */
#endif
    
    ret = (char *) malloc((unsigned) size);
    if (ret != NULL) return ret;
    (void) fprintf(stderr, "malloc failed\n");
    exit(1);
    return NULL;
}

PSResMallocProc PSResMalloc = mymalloc;
#define MALLOC (*PSResMalloc)

static char *myrealloc(ptr, size)
    char *ptr;
    int size;
{
    char *ret;

#ifdef XENVIRONMENT
#ifdef MALLOC_0_RETURNS_NULL
    if (size < 1) size = 1;
#endif
#else
    if (size < 1) size = 1;			/* Better safe than sorry */
#endif

    if (ptr == NULL) return MALLOC(size);
#ifdef NO_VOID_STAR
    ret = (char *) realloc(ptr, (unsigned) size);
#else
    ret = (char *) realloc((void *) ptr, (unsigned) size);
#endif
    if (ret != NULL) return ret;
    (void) fprintf(stderr, "realloc failed\n");
    exit(1);
    return NULL;
}

PSResReallocProc PSResRealloc = myrealloc;
#define REALLOC (*PSResRealloc)

static void myfree(ptr)
    char *ptr;
{
#ifdef NO_VOID_STAR
    if (ptr != NULL) free(ptr);
#else
    if (ptr != NULL) free((void *) ptr);
#endif
}

PSResFreeProc PSResFree = myfree;
#define FREE (*PSResFree)

#define NewString(str) ((char *) strcpy(MALLOC((int) (strlen(str)+1)),(str)))

static void FileWarningHandler(file, extraInfo)
    char *file;
    char *extraInfo;
{
    fprintf(stderr,
	    "Syntax error found in PostScript resource file %s:\n  %s\n",
	    file, extraInfo);
}

PSResFileWarningHandlerProc PSResFileWarningHandler = FileWarningHandler;

/* Just like fgets, but strips trailing newline, eliminates trailing comments,
   skips trailing white space, skips blank lines, and chops lines longer
   than size */

static char *myfgets(buf, size, f)
    char *buf;
    register int size;
    FILE *f;
{
    register int ch;
    register int count = 0;

#define STRIP_BLANKS \
	while (--count >= 0 && (buf[count] == ' ' || buf[count] == '\t')) \
	       buf[count] = '\0';
#define RETURN_BUF \
        if (buf[0] != '\0') return buf; \
	else return myfgets(buf, size, f);

    if (size == 0) return buf;
    size--;

    while (count < size) {
	ch = getc(f);
	if (ch == EOF) {
	    buf[count] = '\0';
	    STRIP_BLANKS;
	    return NULL;
	}
	if (ch == '\n') {
	    buf[count] = '\0';
	    STRIP_BLANKS;
	    RETURN_BUF;
	}
	if (ch == '%') {	/* Comment */
	    if (count == 0 || buf[count-1] != '\\') {
		buf[count] = '\0';
		do {
		    ch = getc(f);
		} while (ch != EOF && ch != '\n');
		STRIP_BLANKS;
		if (ch == EOF) return NULL;
		else RETURN_BUF;
	    }
	}
		
	buf[count] = ch;
	count++;
    }

    /* Overflow, flush */
    buf[count] = '\0';
    do {
	ch = getc(f);
    } while (ch != EOF && ch != '\n');
    STRIP_BLANKS;
    if (ch == EOF) return NULL;
    else RETURN_BUF;
#undef RETURN_BUF
#undef STRIP_BLANKS
}

static int Dequote(buf, dontDequote)
    char *buf;
    char dontDequote;
{
    char *dst = buf, *src = buf;

    while (*src != '\0') {
	if (*src == '\\') {
	    src++;
	    if (*src == '\0') {
		*dst = '\0';
		return 1;
	    } else if (*src == dontDequote) *dst++ = '\\';
	}
	*dst++ = *src++;
    }
    *dst = '\0';
    return 0;
}

/* Sep returns the first unquoted position of the break character.  The
   dontDequote character only applies after the break character.  If the
   separator character is doubled, doubleSep is set to 1 and only one
   of the separators is installed */

static int DequoteAndBreak(buf, sep, bchar, dontDequote, doubleSep)
    char *buf;
    char **sep;
    char bchar;
    char dontDequote;
    int *doubleSep;
{
    char *dst = buf, *src = buf;

    *doubleSep = 0;
    *sep = NULL;

    while (*src != '\0') {
	if (*src == '\\') {
	    src++;
	    if (*src == '\0') {
		*dst = '\0';
		return 1;
	    } else if (*src == dontDequote && *sep != NULL) *dst++ = '\\';
	} else if (*sep == NULL && *src == bchar) {
	    *sep = dst;
	    if (*(src+1) == bchar) {
		src++;
		*doubleSep = 1;
	    }
	}
	*dst++ = *src++;
    }
    *dst = '\0';
    return 0;
}

static float ParseVersion(f, excl)
    FILE *f;
    int *excl;
{
    char inBuf[MAXLEN];
    float version = 0.0;
    int count;

    if (myfgets(inBuf, MAXLEN, f) == NULL) return 0.0;
    
    /* Compare against magic value */

    count = sscanf(inBuf, "PS-Resources-Exclusive-%f", &version);
    if (count == 1) {
	*excl = 1;
	return version;
    }
    count = sscanf(inBuf, "PS-Resources-%f", &version);
    if (count == 1) {
	*excl = 0;
	return version;
    }
    return 0.0;
}

static int ParseResourceTypes(f, dir)
    FILE *f;
    ResourceDirectory dir;
{
    char inBuf[MAXLEN];
    char typebuf[MAXLEN];
    char *types = typebuf;
    int buflen = MAXLEN, typelen = 0, len;
    int count, i;
    int continued = 0, precontinued = 0;

    typebuf[0] = '\0';
    count = 0;

    while (1) {
	if (myfgets(inBuf, MAXLEN, f) == NULL) {
	    if (types != typebuf) FREE((char *) types);
	    return 1;
	}
	if (strcmp(inBuf, ".") == 0) break;
	precontinued = continued;
	continued = Dequote(inBuf, '\0');

	len = strlen(inBuf);
	if (typelen + len + 1 > buflen) {
	    if (types == typebuf) {
		types = (char *) MALLOC(2*MAXLEN);
		(void) memcpy((MT) types, (MT) typebuf, (MST) typelen);
	    } else types = REALLOC(types, buflen+MAXLEN);
	    buflen += MAXLEN;
	}

	if (precontinued) typelen--;	/* Write over previous \0 */
	else count++;
	(void) strncpy(&types[typelen], inBuf, len+1);
	typelen += len+1;
    }

    dir->typeCount = count;
    if (count == 0) dir->types = NULL;
    else {
	dir->types = (ResourceType)
		MALLOC((int) (count * sizeof(ResourceTypeStruct)));
	dir->typeNameBuffer = (char *) MALLOC(typelen);
	(void) memcpy((MT) dir->typeNameBuffer, (MT) types, (MST) typelen);
    }

    len = 0;
    for (i = 0; i < count; i++) {
	dir->types[i].type = &(dir->typeNameBuffer[len]);
	dir->types[i].fileOffset = 0;
	dir->types[i].names = NULL;
	dir->types[i].nameBuffer = NULL;
	dir->types[i].filled = 0;
	dir->types[i].oldNameBuffers = NULL;
	dir->types[i].oldNameCount = 0;
	dir->types[i].nameCount = 0;
	len += strlen(dir->types[i].type) + 1;
    }

    if (types != typebuf) FREE((char *) types);
    return 0;
}

static int ParseFilePrefix(f, dir, dirName)
    FILE *f;
    ResourceDirectory dir;
    char *dirName;
{
    char inBuf[MAXLEN];
    int continued;
    int len;
    long savePos;
    char *prefix;

    dir->filePrefix = NULL;

    savePos = ftell(f);
    if (myfgets(inBuf, MAXLEN, f) == NULL) return 1;

    if (inBuf[0] != '/') {
	if (fseek(f, savePos, SEEK_SET) == -1) return 1;
	len = strlen(dirName);
	dir->filePrefix = strcpy((char *) MALLOC(len+2), dirName);
	dir->filePrefix[len] = '/';
	dir->filePrefix[len+1] = '\0';
	return 0;
    }

    continued = Dequote(inBuf, '\0');
    prefix = inBuf+1;
    len = strlen(prefix);
    dir->filePrefix = (char *) strcpy((char *) MALLOC(len+2), prefix);
    if (!continued) {
	dir->filePrefix[len] = '/';
	dir->filePrefix[len+1] = '\0';
	return 0;
    }

    while (1) {
	if (myfgets(inBuf, MAXLEN, f) == NULL) return 1;
	continued = Dequote(inBuf, '\0');
	len += strlen(inBuf);
	dir->filePrefix = (char *) REALLOC(dir->filePrefix, len+2);
	(void) strcat(dir->filePrefix, inBuf);
	if (!continued) {
	    dir->filePrefix[len] = '/';
	    dir->filePrefix[len+1] = '\0';
	    return 0;
	}
    }
}

static ResourceDirectory ParseHeader(f, dirName, fileName)
    FILE *f;
    char *dirName, *fileName;
{
    ResourceDirectory dir;
    float version;
    int exclusive;

    rewind(f);

    version = ParseVersion(f, &exclusive);
    if (version < 1.0) return NULL;

    dir = (ResourceDirectory) MALLOC(sizeof(ResourceDirectoryStruct));
    dir->directory = (char *) NewString(fileName);
    dir->exclusive = exclusive;
    dir->next = NULL;

    if (ParseResourceTypes(f, dir)) {
	FREE((char *) dir->directory);
	FREE((char *) dir);
        return NULL;
    }

    if (ParseFilePrefix(f, dir, dirName)) dir->endOfHeader = 0;
    else dir->endOfHeader = ftell(f);

    return dir;
}

/* Store away old name buffer so pointers to it do not become invalid */

static void CacheOldNames(type)
    ResourceType type;
{
    type->oldNameCount++;
    type->oldNameBuffers = (char **) REALLOC((char *) type->oldNameBuffers,
					       type->oldNameCount);
    type->oldNameBuffers[type->oldNameCount-1] = type->nameBuffer;
    type->nameBuffer = NULL;
}

/* Verify that the name matches the name in the file */

static int VerifyName(f, name)
    FILE *f;
    char *name;
{
    char inBuf[MAXLEN];
    int continued = 0;
    int len;
    int start = 0;

    while (1) {
	if (myfgets(inBuf, MAXLEN, f) == NULL) return 1;
	continued = Dequote(inBuf, '\0');
	if (continued) {
	    len = strlen(inBuf);
	    if (strncmp(inBuf, &name[start], len) != 0) return 1;
	    start += len;
	} else {
	    if (strcmp(inBuf, &name[start]) != 0) return 1;
	    else break;
	}
    }
    return 0;
}

static int LookupResourceInList(type, name)
    ResourceType type;
    char *name;
{
    int i;

    for (i = 0; i < type->nameCount; i++) {
	if (strcmp(name, type->names[i].name) == 0) return 1;
    }
    return 0;
}

static int CheckInsertPrefix(type)
    char *type;
{
   /* Insert the prefix unless one of these special values */

   if (strcmp(type, PSResFontFamily) == 0 ||
       strcmp(type, PSResFontBDFSizes) == 0 ||
       strcmp(type, "FontAxes") == 0 ||
       strcmp(type, "FontBlendMap") == 0 ||
       strcmp(type, "FontBlendPositions") == 0 ||
       strcmp(type, "mkpsresPrivate") == 0) return 0;
   return 1;
}

/* Returns a line, including continuations.  Memory must be copied before
   calling this again. */

static int linebuflen = 0;
static char *inputline = NULL;

static char *ReadFullLine(f)
    FILE *f;
{
    char readbuf[MAXLEN];
    int start = 0;
    int len;

    while (1) {
	if (myfgets(readbuf, MAXLEN, f) == NULL) return NULL;
	len = strlen(readbuf);
	if (start + len + 1 > linebuflen) {
	    linebuflen += MAXLEN + 1;
	    inputline = REALLOC(inputline, linebuflen);
	}
	strncpy(inputline+start, readbuf, len+1);

	if (inputline[start+len-1] != '\\') break;
	
	start = start+len-1;
    }
    return inputline;
}

static void FreeLineBuf()
{
    if (inputline != NULL) FREE(inputline);
    inputline = NULL;
    linebuflen = 0;
}

/* Assumes being correctly positioned in the file */

static int ReadResourceSection(f, dir, type, name)
    FILE *f;
    ResourceDirectory dir;
    ResourceType type;
    char *name;		/* If NULL, enumerate */
{
#define GROW 1000
    char *linein;
    int start;
    int len;
    char namebuf[GROW];
    char *names = namebuf;
    int buflen = GROW, namelen = 0;
    int i, count = 0;
    char *sep;
    int prefixLen;
    int insertPrefix;
    char dontDequote;
    int doubleEquals;
    int addingPrefix;
    int newsize;

    if (type->nameBuffer != NULL) CacheOldNames(type);

    insertPrefix = CheckInsertPrefix(type->type);
    if (insertPrefix) {
	prefixLen = strlen(dir->filePrefix);
	dontDequote = '\0';
    } else {
	prefixLen = 0;
	dontDequote = ',';
    }

    while (1) {
	start = namelen;

	linein = ReadFullLine(f);
	if (linein == NULL) {
	    if (names != namebuf) FREE((char *) names);
	    FreeLineBuf();
	    return 1;
	}
	if (strcmp(linein, ".") == 0) break;

	sep = NULL;
	(void) DequoteAndBreak(linein, &sep, '=', dontDequote, &doubleEquals);

	/* If no separator, a bogus line */
	if (sep == NULL) continue;
	    
	/* Next line is UNIX specific! */
	addingPrefix = *(sep+1) != '/' && insertPrefix && !doubleEquals;

	len = strlen(linein) + 1 + (addingPrefix ? prefixLen : 0);

	if (namelen + len >= buflen) {
	    newsize = buflen + GROW;
	    if (namelen + len > newsize) newsize = namelen + len;

	    if (names == namebuf) {
		names = (char *) MALLOC(newsize);
		(void) memcpy((MT) names, (MT) namebuf, (MST) namelen);
	    } else names = REALLOC(names, newsize);
	    buflen = newsize;
	}

	*sep = '\0';
	len = strlen(linein);
	(void) strncpy(&names[namelen], linein, len+1);
	namelen += len+1;

	if (addingPrefix) {
	    (void) strncpy(&names[namelen], dir->filePrefix, prefixLen);
	    namelen += prefixLen;
	}
	
	len = strlen(sep+1);
	(void) strncpy(&names[namelen], sep+1, len+1);
	namelen += len+1;

	if (name != NULL && strcmp(names, name) != 0) namelen = start;
	else count++;
    }

    type->nameCount = count;
    if (count == 0) type->names = NULL;
    else {
	type->names = (ResourceName)
		MALLOC((int) (count * sizeof(ResourceNameStruct)));
	type->nameBuffer = (char *) MALLOC(namelen);
	(void) memcpy((MT) type->nameBuffer, (MT) names, (MST) namelen);
    }

    len = 0;
    for (i = 0; i < count; i++) {
 	type->names[i].name = &(type->nameBuffer[len]);
	len += strlen(type->names[i].name) + 1;
	type->names[i].file = &(type->nameBuffer[len]);
	len += strlen(type->names[i].file) + 1;
    }

    if (names != namebuf) FREE((char *) names);
    if (name == NULL) type->filled = 1;
    FreeLineBuf();
    return 0;

#undef GROW
}

/* Assumes being correctly positioned in the file */

static int SkipResourceSection(f, dir, type, checkName)
    FILE *f;
    ResourceDirectory dir;
    ResourceType type;
    int checkName;
{
    char inBuf[MAXLEN];
    int i;

    /* If next type has offset, just go there */

    for (i = 0; i < dir->typeCount && dir->types + i != type; i++) {}

    if (dir->types + i == type) {
	for (i++; i < dir->typeCount; i++) {
	    if (dir->types[i].fileOffset == -1) continue;
	    if (dir->types[i].fileOffset > 0) {
		if (fseek(f, dir->types[i].fileOffset, SEEK_SET) != -1) {
		    return 0;
		} else break;
	    }
	}
    }

    if (checkName && VerifyName(f, type->type) != 0) return 1;

    while (1) {
	if (myfgets(inBuf, MAXLEN, f) == NULL) return 1;
	if (strcmp(inBuf, ".") == 0) return 0;
    }
}

/* Assumes being correctly positioned in the file */

static int ParseResourceSection(f, dir, type, name, checkName)
    FILE *f;
    ResourceDirectory dir;
    ResourceType type;
    char *name;		/* If NULL, enumerate */
    int checkName;
{
    if (checkName && VerifyName(f, type->type) != 0) return 1;

    if (type->filled || (name != NULL && LookupResourceInList(type, name))) { 
	return SkipResourceSection(f, dir, type, 0);
    }

    return ReadResourceSection(f, dir, type, name);
}

void FreePSResourceStorage(everything)
    int everything;
{
    ResourceDirectory d;
    ResourceType t;
    int i, j;

    if (resDir == NULL) return;

    for (d = resDir; d != NULL; d = d->next) {
	for (i = 0; i < d->typeCount; i++) {
	    t = d->types + i;
	    FREE(t->nameBuffer);
	    FREE((char *) t->names);
	    t->nameCount = 0;
	    for (j = 0; j < t->oldNameCount; j++) FREE(t->oldNameBuffers[j]);
	    if (t->oldNameBuffers != NULL) FREE((char *) t->oldNameBuffers);
	    t->oldNameCount = 0;
	    t->nameCount = 0;
	    t->filled = 0;
	}
    }

    if (!everything) return;

    while (resDir != NULL) {
	d = resDir->next;
	FREE(resDir->directory);
	FREE((char *) resDir->types);
	FREE(resDir->typeNameBuffer);
	FREE(resDir->filePrefix);
	FREE((char *) resDir);
	resDir = d;
    }
    lastResDir = NULL;
    FREE(savedPathOverride);
    savedPathOverride = NULL;
    FREE(savedDefaultPath);
    savedDefaultPath = NULL;
}

static ResourceDirectory ReadAndStoreFile(dir, name, len, readFunc, data)
    char *dir, *name;
    int len;
    ReadContentsFunction readFunc;
    char *data;
{
    ResourceDirectory rd = NULL;
    FILE *f;
    char nameBuf[MAXLEN];
    char *fullName = nameBuf;
    int fullLen;

    fullLen = len + strlen(name) + 1;
    if (fullLen >= MAXLEN) fullName = MALLOC(fullLen+1);
    (void) strcpy(fullName, dir);
    fullName[len] = '/';
    (void) strcpy(fullName+len+1, name);

    f = fopen(fullName, "r");
    if (f != NULL) {
	rd = ParseHeader(f, dir, fullName);

	if (rd != NULL) {
	    if (resDir == NULL) resDir = rd;
	    else lastResDir->next = rd;
	    lastResDir = rd;
	    if (readFunc != NULL) (*readFunc) (f, rd, data);
	} else (*PSResFileWarningHandler)(fullName, "Malformed header");
	(void) fclose(f);
    }
    if (fullName != nameBuf) FREE(fullName);
    return rd;
}

static time_t ReadFilesInDirectory(dirName, readFunc, data)
    char *dirName;
    ReadContentsFunction readFunc;
    char *data;
{
    DIR *dir;
#ifdef USE_POSIX
    struct dirent *d;
#else
    struct direct *d;
#endif
    ResourceDirectory rd;
    int len = strlen(dirName);
    static int extensionLen = 0;
    struct stat buf;
    int namelen;

    if (extensionLen == 0) extensionLen = strlen(PSRES_EXT);
    if (stat(dirName, &buf) != 0) buf.st_mtime = 0;

    rd = ReadAndStoreFile(dirName, PSRES_NAME, len, readFunc, data);

    if (rd != 0 && rd->exclusive) return buf.st_mtime;

    dir = opendir(dirName);
    if (dir == NULL) return buf.st_mtime;

    while ((d = readdir(dir)) != NULL) {
	namelen = strlen(d->d_name);
	if (namelen < extensionLen) continue;

	if (strcmp(d->d_name + (namelen - extensionLen), PSRES_EXT) == 0
		&& strcmp(d->d_name, PSRES_NAME) != 0) {
	    (void) ReadAndStoreFile(dirName, d->d_name, len, readFunc, data);
	}
    }
    (void) closedir(dir);
    return buf.st_mtime;
}

/* Returns nonzero if current paths different from saved ones */

static int SetUpSavedPaths(pathOverride, defaultPath)
    char *pathOverride;
    char *defaultPath;
{
    if (pathOverride == NULL) pathOverride = &nullStr;
    if (defaultPath == NULL) defaultPath = &nullStr;

    if (savedPathOverride == NULL ||
	    strcmp(pathOverride, savedPathOverride) != 0 ||
	    strcmp(defaultPath, savedDefaultPath) != 0) {

	FreePSResourceStorage(1);

	savedPathOverride = NewString(pathOverride);
	savedDefaultPath = NewString(defaultPath);

	return 1;
    }
    return 0;
}

/* Like SetUpSavedPaths, but never affects saved state */

static int CheckSavedPaths(pathOverride, defaultPath)
    char *pathOverride;
    char *defaultPath;
{
    if (pathOverride == NULL) pathOverride = &nullStr;
    if (defaultPath == NULL) defaultPath = &nullStr;

    if (savedPathOverride == NULL ||
	    strcmp(pathOverride, savedPathOverride) != 0 ||
	    strcmp(defaultPath, savedDefaultPath) != 0) return 1;
    else return 0;
}

static time_t ReadFilesInPath(string, readFunc, data)
    char *string;
    ReadContentsFunction readFunc;
    char *data;
{
    char *pathChar;
    char pathBuf[MAXLEN];
    char *path;
    register char *dir;
    int len;
    register char ch;
    time_t newTime, latestTime = 0;

    pathChar = string;

    if (*pathChar == ':') pathChar++;

    len = strlen(pathChar);
    if (len < MAXLEN) path = pathBuf;
    else path = MALLOC(len+1);

    do {
	dir = path;
	do {
	    while (*pathChar == '\\') {
		pathChar++;
		if (*pathChar != '\0') *dir++ = *pathChar++;
            }		  

	    *dir++ = ch = *pathChar++;
	} while (ch != '\0' && ch != ':');
	if (ch == ':') *(dir-1) = '\0';

	if (*path == '\0') {
	    if (ch == ':' && string != savedDefaultPath) {
		newTime = ReadFilesInPath(savedDefaultPath, readFunc, data);
		if (newTime > latestTime) latestTime = newTime;
	    }
	} else {
	    newTime = ReadFilesInDirectory(path, readFunc, data);
	    if (newTime > latestTime) latestTime = newTime;
	}
    } while (ch == ':');
    if (path != pathBuf) FREE(path);
    return latestTime;
}	

static time_t MaxTimeInPath(string)
    char *string;
{
    char *pathChar;
    char pathBuf[MAXLEN];
    char *path;
    register char *dir;
    int len;
    register char ch;
    time_t latestTime = 0;
    struct stat buf;

    pathChar = string;

    if (*pathChar == ':') pathChar++;

    len = strlen(pathChar);
    if (len < MAXLEN) path = pathBuf;
    else path = MALLOC(len+1);

    do {
	dir = path;
	do {
	    while (*pathChar == '\\') {
		pathChar++;
		if (*pathChar != '\0') *dir++ = *pathChar++;
            }		  

	    *dir++ = ch = *pathChar++;
	} while (ch != '\0' && ch != ':');
	if (ch == ':') *(dir-1) = '\0';

	if (*path == '\0') {
	    if (ch == ':' && string != savedDefaultPath) {
		buf.st_mtime = MaxTimeInPath(savedDefaultPath);
		if (buf.st_mtime > latestTime) latestTime = buf.st_mtime;
	    }
	} else {
	    if (stat(path, &buf) != 0) buf.st_mtime = 0;
	    if (buf.st_mtime > latestTime) latestTime = buf.st_mtime;
	}
    } while (ch == ':');
    if (path != pathBuf) FREE(path);
    return latestTime;
}	

static char *GetPath()
{
    static char defaultEnvironmentPath[] = "::";
    static char *environmentPath = NULL;

    if (savedPathOverride != NULL && *savedPathOverride != '\0') {
	return savedPathOverride;
    }

    if (environmentPath == NULL) {
	environmentPath = getenv("PSRESOURCEPATH");
	if (environmentPath == NULL) environmentPath = defaultEnvironmentPath;
    }

    return environmentPath;
}

void SetPSResourcePolicy(policy, willList, resourceTypes)
    PSResourceSavePolicy policy;
    int willList;
    char **resourceTypes;
{
    currentPolicy = policy;

    if (currentResourceTypes != NULL) FREE((char *) currentResourceTypes);
    if (resourceTypeBuffer != NULL) FREE((char *) resourceTypeBuffer);

    if (resourceTypes == NULL) {
	currentResourceTypes = NULL;
	resourceTypeBuffer = NULL;
    } else {
	int i = 0, len = 0;
	char **cp = resourceTypes;

	while (*cp != NULL) {
	    i++;
	    len += strlen(*cp) + 1;
	    cp++;
	}
	
	currentResourceTypes =
		(char **) MALLOC((int) ((i+1) * sizeof(char *)));
	resourceTypeBuffer = MALLOC(len);
	
	len = 0;
	i = 0;
	cp = resourceTypes;

	while (*cp != NULL) {
	    (void) strcpy(resourceTypeBuffer+len, *cp);
	    currentResourceTypes[i++] = resourceTypeBuffer + len;
	    len += strlen(*cp) + 1;
	    cp++;
	}
	currentResourceTypes[i] = NULL;
    }
}

int InSavedList(type)
    char *type;
{
    char **cp = currentResourceTypes;;

    if (cp == NULL) return 0;

    while (*cp != NULL) {
	if (strcmp(*cp++, type) == 0) return 1;
    }
    return 0;
}

/* ARGSUSED */
static int ReadEverything(f, rd, data)
    FILE *f;
    ResourceDirectory rd;
    char *data;
{
    int i;
    ResourceType t;
    long pos;

    /* We're at the start of the resource section; read all of them */

    for (i = 0; i < rd->typeCount; i++) {
	t = &rd->types[i];

	if (t->fileOffset == -1) continue;	/* Not really there */

	if (t->fileOffset != 0 && fseek(f, t->fileOffset, SEEK_SET) != -1) {
	    if (!t->filled) {
		if (ParseResourceSection(f, rd, t, (char *) NULL, 1)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    } else {
		if (SkipResourceSection(f, rd, t, 1)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    }
	    continue;
        }

	pos = ftell(f);
	if (VerifyName(f, t->type) == 0) {
	    t->fileOffset = pos;
	    if (ParseResourceSection(f, rd, t, (char *) NULL, 0)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	} else {
	    /* No resources of this type; try the next type */
	    t->fileOffset = -1;
	    if (fseek(f, pos, SEEK_SET)) {
		PSResFileWarningHandler(rd->directory,
					"File changed during execution");
		return 1;
	    }
	}
    }
    return 0;
}

static int ReadType(f, rd, type)
    FILE *f;
    ResourceDirectory rd;
    char *type;
{
    int i;
    ResourceType t;
    long pos;

    /* We're at the start of the resource section; read the sections that
       are in the cached type list or are the passed in type */

    for (i = 0; i < rd->typeCount; i++) {
	t = &rd->types[i];

	if (t->fileOffset == -1) continue;	/* Not really there */
	if (t->fileOffset != 0 && fseek(f, t->fileOffset, SEEK_SET) != -1) {
	    if (!t->filled &&
		    (strcmp(t->type, type) == 0 || InSavedList(t->type))) {
		if (ParseResourceSection(f, rd, t, (char *) NULL, 1)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    } else if (SkipResourceSection(f, rd, t, 1)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	    continue;
        }

	pos = ftell(f);
	if (VerifyName(f, t->type) == 0) {
	    t->fileOffset = pos;
	    if (strcmp(t->type, type) == 0 || InSavedList(t->type)) {
		if (ParseResourceSection(f, rd, t, (char *) NULL, 0)){
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    } else if (SkipResourceSection(f, rd, t, 0)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	} else {
	    /* No resources of this type; try the next type */
	    t->fileOffset = -1;
	    if (fseek(f, pos, SEEK_SET) == -1) {
		PSResFileWarningHandler(rd->directory,
					"File changed during execution");
		return 1;
	    }
	}
    }
    return 0;
}

static int ReadName(f, rd, data)
    FILE *f;
    ResourceDirectory rd;
    char *data;
{
    TypeName *tn = (TypeName *) data;
    int i;
    ResourceType t;
    long pos;

    /* We're at the start of the resource section; read the name in the
       section for the passed in type */

    for (i = 0; i < rd->typeCount; i++) {
	t = &rd->types[i];

	if (t->fileOffset == -1) continue;	/* Not really there */
	if (t->fileOffset != 0 && fseek(f, t->fileOffset, SEEK_SET)) {
	    if (strcmp(t->type, tn->type) == 0) {
		if (ParseResourceSection(f, rd, t, tn->name, 1)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    } else if (SkipResourceSection(f, rd, t, 1)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	    continue;
        }

	pos = ftell(f);
	if (VerifyName(f, t->type) == 0) {
	    t->fileOffset = pos;
	    if (fseek(f, pos, SEEK_SET) == -1) {
		PSResFileWarningHandler(rd->directory,
					"File changed during execution");
		return 1;
	    }
	    if (strcmp(t->type, tn->type) == 0) {
		if (ParseResourceSection(f, rd, t, tn->name, 0)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
	    } else if (SkipResourceSection(f, rd, t, 0)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	} else {
	    /* No resources of this type; try the next type */
	    t->fileOffset = -1;
	    if (fseek(f, pos, SEEK_SET) == -1) {
		PSResFileWarningHandler(rd->directory,
					"File changed during execution");
		return 1;
	    }
	}
    }
    return 0;
}

static void ReadHeadersAndData(resourceType, resourceName)
    char *resourceType;
    char *resourceName;
{
    TypeName t;

    switch (currentPolicy) {
	case PSSaveEverything:
	    lastModifiedTime =
		    ReadFilesInPath(GetPath(), ReadEverything, (char *) NULL);
	    break;
	case PSSaveByType:
	    lastModifiedTime =
		    ReadFilesInPath(GetPath(), ReadType, resourceType);
	    break;
	case PSSaveReturnValues:
	    t.type = resourceType;
	    t.name = resourceName;
	    lastModifiedTime =
		    ReadFilesInPath(GetPath(), ReadName, (char *) &t);
	    break;
    }
}

static void UpdateData(resourceType, resourceName)
    char *resourceType;
    char *resourceName;
{
    ResourceDirectory rd;
    ResourceType rt;
    int i;
    FILE *f;
    TypeName tn;

    /* Make two passes; the first figures out if we're going to have to read
       the file to service this request.  If we are, open the file and read
       in sections in the saved list (from SetPSResourcePolicy).  If not
       just saving return values, make sure we read in everything
       in the section. */

    for (rd = resDir; rd != NULL; rd = rd->next) {
	f = NULL;
	for (i = 0; i < rd->typeCount; i++) {
	    rt = rd->types + i;
	    if (rt->filled) continue;
	    if (strcmp(rt->type, resourceType) != 0) continue;

	    if (resourceName != NULL &&
		    LookupResourceInList(rt, resourceName)) continue;

	    f = fopen(rd->directory, "r");
	    break;
	}
	if (f == NULL) continue;

	/* Nuts, have to read the file */

	if (fseek(f, rd->endOfHeader, SEEK_SET) != -1) {
	    switch (currentPolicy) {
		case PSSaveEverything:
		    (void) ReadEverything(f, rd, (char *) NULL);
		    break;
		case PSSaveByType:
		    (void) ReadType(f, rd, resourceType);
		    break;
		case PSSaveReturnValues:
		    tn.type = resourceType;
		    tn.name = resourceName;
		    (void) ReadName(f, rd, (char *) &tn);
		    break;
	    }
	} else (*PSResFileWarningHandler)(rd->directory,
					  "File changed during execution");
	(void) fclose(f);
    }
}

static int FindData(resourceType, resourceName,
		    resourceNamesReturn, resourceFilesReturn)
    char *resourceType;
    char *resourceName;
    char ***resourceNamesReturn;
    char ***resourceFilesReturn;
{
    ResourceDirectory rd;
    ResourceType rt;
    int i, j, k;
    int nameCount = 0;
    char **names, **files;

    /* Make two passes; first count, then set and return */

    for (rd = resDir; rd != NULL; rd = rd->next) {
	for (i = 0; i < rd->typeCount; i++) {
	    rt = rd->types + i;
	    if (strcmp(rt->type, resourceType) != 0) continue;
	    if (resourceName == NULL) nameCount += rt->nameCount;
	    else {
		for (j = 0; j < rt->nameCount; j++) {
		    if (strcmp(rt->names[j].name, resourceName) == 0) {
			nameCount++;
		    }
		}
	    }
	}
    }

    if (nameCount == 0) return 0;

    names = (char **) MALLOC((int) (nameCount * sizeof(char *)));
    files = (char **) MALLOC((int) (nameCount * sizeof(char *)));
    k = 0;

    for (rd = resDir; rd != NULL; rd = rd->next) {
	for (i = 0; i < rd->typeCount; i++) {
	    rt = rd->types + i;
	    if (strcmp(rt->type, resourceType) != 0) continue;
	    for (j = 0; j < rt->nameCount; j++) {
		if (resourceName == NULL ||
			strcmp(rt->names[j].name, resourceName) == 0) {
		    names[k] = rt->names[j].name;
		    files[k++] = rt->names[j].file;
		}
	    }
	}
    }

    *resourceNamesReturn = names;
    *resourceFilesReturn = files;
    return nameCount;
}

extern int ListPSResourceFiles(psResourcePathOverride, defaultPath,
			       resourceType, resourceName,
			       resourceNamesReturn, resourceFilesReturn)
    char *psResourcePathOverride;
    char *defaultPath;
    char *resourceType;
    char *resourceName;
    char ***resourceNamesReturn;
    char ***resourceFilesReturn;
{
    if (SetUpSavedPaths(psResourcePathOverride, defaultPath)) {
	ReadHeadersAndData(resourceType, resourceName);
    } else UpdateData(resourceType, resourceName);
    return FindData(resourceType, resourceName,
		    resourceNamesReturn, resourceFilesReturn);
}

int ListPSResourceTypes(pathOverride, defaultPath, typesReturn)
    char *pathOverride;
    char *defaultPath;
    char ***typesReturn;
{
#define GROW 5
#define START 15
    int typeCount = 0, i, j, typeBufSize = 0;
    ResourceDirectory d;
    register char **types = NULL;
    int sig;
    int *sigs = NULL;
    char *ch;

    if (SetUpSavedPaths(pathOverride, defaultPath)) {
	if (currentPolicy != PSSaveEverything) {
	    lastModifiedTime =
		    ReadFilesInPath(GetPath(), (ReadContentsFunction) NULL,
			    (char *) NULL);
	} else lastModifiedTime =
		    ReadFilesInPath(GetPath(), ReadEverything, (char *) NULL);
    }

    for (d = resDir; d != NULL; d = d->next) {
	for (i = 0; i < d->typeCount; i++) {
	    for (sig = 0, ch = d->types[i].type; *ch != '\0'; sig += *ch++) {}
	    for (j = 0; j < typeCount; j++) {
		if (sig == sigs[j] &&
		    strcmp(types[j], d->types[i].type) == 0) break;
	    }
	    if (j >= typeCount) {	/* Have to add */
		if (typeCount >= typeBufSize) {
		    if (typeCount == 0) typeBufSize = START;
		    else typeBufSize += GROW;
		    types = (char **) REALLOC((char *) types,
					      typeBufSize * sizeof(char *));
		    sigs = (int *) REALLOC((char *) sigs,
					   typeBufSize * sizeof(int));
		}
		types[typeCount] = d->types[i].type;
		sigs[typeCount++] = sig;
	    }
	}
    }

    FREE((char *) sigs);
    *typesReturn = types;
    return typeCount;
#undef START
#undef GROW
}

/* Assumes being correctly positioned in the file */

static int EnumerateResourceSection(f, dir, type, s, checkName)
    FILE *f;
    ResourceDirectory dir;
    ResourceType type;
    EnumeratorStruct *s;
    int checkName;
{
#define GROW 1000
    char *linein;
    int len;
    char namebuf[GROW];
    char *names = namebuf;
    int buflen = GROW, namelen = 0;
    char *sep;
    int prefixLen;
    int insertPrefix;
    char *file;
    char dontDequote;
    int doubleEquals;
    int addingPrefix;

    if (checkName && VerifyName(f, type->type) != 0) return 1;

    insertPrefix = CheckInsertPrefix(type->type);
    if (insertPrefix) {
	prefixLen = strlen(dir->filePrefix);
	dontDequote = '\0';
    } else {
	prefixLen = 0;
	dontDequote = ',';
    }

    while (1) {
	linein = ReadFullLine(f);
	if (linein == NULL) {
	    if (names != namebuf) FREE((char *) names);
	    FreeLineBuf();
	    return 1;
	}

	if (strcmp(linein, ".") == 0) {
	    if (names != namebuf) FREE((char *) names);
	    FreeLineBuf();
	    return 0;
	}

	sep = NULL;
	(void) DequoteAndBreak(linein, &sep, '=', dontDequote, &doubleEquals);

	/* If no separator, a bogus line */
	if (sep == NULL) continue;

	/* Next line is UNIX specific! */
	addingPrefix = *(sep+1) != '/' && insertPrefix && !doubleEquals;

	len = strlen(linein) + (addingPrefix ? 0 : prefixLen) + 1;
	if (len > buflen) {
	    if (names != namebuf) FREE((char *) names);
	    names = (char *) MALLOC(len);
	}

	*sep = '\0';
	len = strlen(linein);
	(void) strncpy(names, linein, len+1);

	namelen = len+1;
	file = &names[namelen];
	
	if (addingPrefix) {
	    (void) strncpy(&names[namelen], dir->filePrefix, prefixLen);
	    namelen += prefixLen;
	}
	
	len = strlen(sep+1);
	(void) strncpy(&names[namelen], sep+1, len+1);

	if (s->name == NULL || strcmp(names, s->name) == 0) {
	    s->done = (*s->func) (s->type, names, file, s->private);
	    if (s->done) {
		if (names != namebuf) FREE((char *) names);
		FreeLineBuf();
		return 0;
	    }
	}
    }
#undef GROW
}

static int Enumerate(f, rd, data)
    FILE *f;
    ResourceDirectory rd;
    char *data;
{
    EnumeratorStruct *s = (EnumeratorStruct *) data;
    int i;
    ResourceType t;
    long pos;

    if (s->done) return 0;

    for (i = 0; i < rd->typeCount; i++) {
	t = &rd->types[i];

	if (t->fileOffset == -1) continue;	/* Not really there */
	if (t->fileOffset != 0 && fseek(f, t->fileOffset, SEEK_SET) != -1) {
	    if (strcmp(t->type, s->type) == 0) {
		if (EnumerateResourceSection(f, rd, t, s, 1)) {
		    char buf[256];
		    sprintf(buf, "Trouble parsing resource type %s", t->type);
		    PSResFileWarningHandler(rd->directory, buf);
		    return 1;
		}
		if (s->done) return 0;;
	    } else if (SkipResourceSection(f, rd, t, 1)) {
		char buf[256];
		sprintf(buf, "Trouble parsing resource type %s", t->type);
		PSResFileWarningHandler(rd->directory, buf);
		return 1;
	    }
	    continue;
        }

	pos = ftell(f);
	if (VerifyName(f, t->type) == 0) {
	    t->fileOffset = pos;
	    if (strcmp(t->type, s->type) == 0) {
		if (EnumerateResourceSection(f, rd, t, s, 0)) return 1;
		if (s->done) return 0;
	    } else if (SkipResourceSection(f, rd, t, 0)) return 1;
	} else {
	    /* No resources of this type; try the next type */
	    t->fileOffset = -1;
	    if (fseek(f, pos, SEEK_SET) == -1) return 1;
	}
    }
    return 0;
}

void EnumeratePSResourceFiles(pathOverride, defaultPath, resourceType,
			      resourceName, enumerator, private)
    char *pathOverride;
    char *defaultPath;
    char *resourceType;
    char *resourceName;
    PSResourceEnumerator enumerator;
    char *private;
{
    ResourceDirectory d;
    FILE *f;
    EnumeratorStruct s;

    s.func = enumerator;
    s.type = resourceType;
    s.name = resourceName;
    s.private = private;
    s.done = 0;

    if (SetUpSavedPaths(pathOverride, defaultPath)) {
	lastModifiedTime =
		    ReadFilesInPath(GetPath(), Enumerate, (char *) &s);
	return;
    }

    for (d = resDir; d != NULL && !s.done; d = d->next) {
	f = fopen(d->directory, "r");
	if (f == NULL) continue;
	if (fseek(f, d->endOfHeader, SEEK_SET) != -1) {
	    (void) Enumerate(f, d, (char *) &s);
	}
	(void) fclose(f);
    }
}

int CheckPSResourceTime(pathOverride, defaultPath)
    char *pathOverride;
    char *defaultPath;
{
    if (CheckSavedPaths(pathOverride, defaultPath)) return 1;
    return MaxTimeInPath(GetPath()) > lastModifiedTime;
}
