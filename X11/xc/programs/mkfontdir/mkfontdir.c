/* $XFree86: xc/programs/mkfontdir/mkfontdir.c,v 3.20 2002/09/24 21:01:06 tsi Exp $ */
/***********************************************************

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* $Xorg: mkfontdir.c,v 1.5 2000/08/17 19:53:59 cpqbld Exp $ */

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif
#ifndef PATH_MAX
#ifdef WIN32
#define PATH_MAX 512
#else
#include <sys/param.h>
#endif
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif

#ifdef WIN32
#define BOOL wBOOL
#include <windows.h>
#undef BOOL
#define FileName(file) file.cFileName
#else
#define FileName(file) file->d_name
#ifndef X_NOT_POSIX
#include <dirent.h>
#else
#ifdef SYSV
#include <dirent.h>
#else
#ifdef USG
#include <dirent.h>
#else
#include <sys/dir.h>
#ifndef dirent
#define dirent direct
#endif
#endif
#endif
#endif
#endif

#include <X11/X.h>
#include <X11/Xproto.h>
#include "fntfilst.h"
#include "fontenc.h"
#include "bitmap.h"

#include <errno.h>


#define  XK_LATIN1
#include <X11/keysymdef.h>

Bool processFonts = TRUE;

char *progName;
char *prefix = "";
Bool relative = FALSE;
char *excludesuf = NULL;

/* The possible extensions for encoding files, in decreasing priority */
#ifdef X_GZIP_FONT_COMPRESSION
#define NUMENCODINGEXTENSIONS 2
char *encodingExtensions[]={".gz", ".Z"};
#else
#define NUMENCODINGEXTENSIONS 1
char *encodingExtensions[]={".Z"};
#endif


typedef struct _nameBucket {
    struct _nameBucket	*next;
    char		*name;
    FontRendererPtr	renderer;
} NameBucketRec, *NameBucketPtr;

typedef struct _encodingBucket {
  struct _encodingBucket *next;
  char *name;
  char *fileName;
  int priority;
} EncodingBucketRec, *EncodingBucketPtr;
    
#define HASH_SIZE   1024
/* should be a divisor of HASH_SIZE */
#define ENCODING_HASH_SIZE 256


static Bool WriteFontTable ( char *dirName, FontTablePtr table);
static Bool WriteEncodingsTable(char *dirName, EncodingBucketPtr *encodings, 
                                int count);
static char * NameForAtomOrNone ( Atom a );
static Bool GetFontName ( char *file_name, char *font_name );
static char * FontNameExists ( FontTablePtr table, char *font_name );
int AddEntry ( FontTablePtr table, char *fontName, char *fileName );
static Bool ProcessFile ( char *dirName, char *fileName, FontTablePtr table );

static void Estrip ( char *ext, char *name );
char * MakeName ( char *name );
int Hash ( char *name );
Bool LoadEncodings(EncodingBucketPtr *encodings, char *dirName, int priority);
static Bool LoadDirectory ( char *dirName, FontTablePtr table );
int LoadScalable ( char *dirName, FontTablePtr table );
static Bool DoDirectory(char *dirName, 
                        EncodingBucketPtr *encodings, int count);
void ErrorF ( void );

static Bool
WriteFontTable(
    char	    *dirName,
    FontTablePtr    table)
{
    FILE	    *file;
    char	    full_name[PATH_MAX];
    FontEntryPtr    entry;
    int             i;

    sprintf (full_name, "%s/%s", dirName, FontDirFile);

    /* remove old fonts.dir, in case it is a link */

    if (unlink(full_name) < 0 && errno != ENOENT)
    {
	fprintf(stderr, "%s: cannot unlink %s\n", progName, full_name);
	return FALSE;
    }

    file = fopen (full_name, "w");
    if (!file)
    {
	fprintf (stderr, "%s: can't create directory %s\n", progName, full_name);
	return FALSE;
    }
    fprintf(file, "%d\n", table->used);
    for (i = 0; i < table->used; i++) {
	entry = &table->entries[i];
	fprintf (file, "%s %s\n", entry->u.bitmap.fileName, entry->name.name);
    }
    fclose (file);

    return TRUE;
}

static Bool
WriteEncodingsTable(char *dirName, EncodingBucketPtr *encodings, int count)
{
    char full_name[PATH_MAX];
    FILE *file;
    int i;
    EncodingBucketPtr encoding;

    sprintf (full_name, "%s/%s", dirName, "encodings.dir");
    if (unlink(full_name) < 0 && errno != ENOENT)
    {
      fprintf(stderr, "%s: warning: cannot unlink %s\n", progName, full_name);
      return FALSE;
    }
    if(!count) return TRUE;
    file = fopen (full_name, "w");
    if (!file)
    {
      fprintf (stderr, "%s: can't create directory %s\n", progName, full_name);
      return FALSE;
    }
    fprintf(file, "%d\n", count);
    for(i=0; i<ENCODING_HASH_SIZE; i++)
      for(encoding=encodings[i]; encoding; encoding=encoding->next)
        fprintf(file, "%s %s%s\n", 
                encoding->name, prefix, encoding->fileName);
    fclose(file);

    return TRUE;
}

static char *
NameForAtomOrNone (Atom a)
{
    char    *name;

    name = NameForAtom (a);
    if (!name)
	return "";
    return name;
}

static Bool
GetFontName(char *file_name, char *font_name)
{
    FontInfoRec	info;
    int		i;
    char	*atom_name;
    char	*atom_value;

    if (BitmapGetInfoBitmap ((FontPathElementPtr) 0, &info, (FontEntryPtr) 0, file_name) != Successful)
	return FALSE;

    for (i = 0; i < info.nprops; i++) 
    {
	atom_name = (char *) NameForAtomOrNone (info.props[i].name);
	if (atom_name && strcmp (atom_name, "FONT") == 0 && info.isStringProp[i])
	{
	    atom_value = NameForAtomOrNone (info.props[i].value);
	    if (strlen (atom_value) == 0)
		return FALSE;
	    strcpy (font_name, atom_value);
	    return TRUE;
	}
    }
    return FALSE;
}

static char *
FontNameExists (FontTablePtr table, char *font_name)
{
    FontNameRec	    name;
    FontEntryPtr    entry;

    name.name = font_name;
    name.length = strlen (font_name);
    name.ndashes = FontFileCountDashes (name.name, name.length);
    entry = FontFileFindNameInDir (table, &name);
    if (entry)
	return entry->u.bitmap.fileName;
    return 0;
}

int
AddEntry (
    FontTablePtr    table,
    char	    *fontName,
    char	    *fileName)
{
    FontEntryRec    prototype;

    prototype.name.name = fontName;
    prototype.name.length = strlen (fontName);
    prototype.name.ndashes = FontFileCountDashes (fontName, prototype.name.length);
    prototype.type = FONT_ENTRY_BITMAP;
    prototype.u.bitmap.fileName = FontFileSaveString (fileName);
    return FontFileAddEntry (table, &prototype) != 0;
}

static Bool
ProcessFile (
    char		*dirName,
    char		*fileName,
    FontTablePtr	table)
{
    char	    font_name[PATH_MAX];
    char	    full_name[PATH_MAX];
    char	    *existing;

    strcpy (full_name, dirName);
    if (dirName[strlen(dirName) - 1] != '/')
	strcat (full_name, "/");
    strcat (full_name, fileName);

    if (!GetFontName (full_name, font_name))
	return FALSE;

    CopyISOLatin1Lowered (font_name, font_name, strlen(font_name));

    if ((existing = FontNameExists (table, font_name)))
    {
	fprintf (stderr, "%s: Duplicate font names %s\n", progName, font_name);
	fprintf (stderr, "\t%s %s\n", existing, fileName);
	return FALSE;
    }
    return AddEntry (table, font_name, fileName);
}

static void
Estrip(
    char	*ext,
    char	*name)
{
    name[strlen(name) - strlen(ext)] = '\0';
}

/***====================================================================***/

#define New(type,count)	((type *) malloc (count * sizeof (type)))

char *
MakeName(char *name)
{
    char    *new;

    new = New(char, strlen(name) + 1);
    strcpy (new,name);
    return new;
}

int
Hash(char *name)
{
    int	    i;
    char    c;

    i = 0;
    while ((c = *name++))
	i = (i << 1) ^ c;
    return i & (HASH_SIZE - 1);
}

static Bool
LoadDirectory (char *dirName, FontTablePtr table)
{
#ifdef WIN32
    HANDLE		dirh;
    WIN32_FIND_DATA	file;
#else
    DIR			*dirp;
    struct dirent	*file;
#endif
    FontRendererPtr	renderer;
    char		fileName[PATH_MAX];
    int			hash;
    char		*extension;
    NameBucketPtr	*hashTable, bucket, *prev, next;
    
#ifdef WIN32
    if ((dirh = FindFirstFile("*.*", &file)) == INVALID_HANDLE_VALUE)
	return FALSE;
#else
    if ((dirp = opendir (dirName)) == NULL)
	return FALSE;
#endif
    hashTable = New (NameBucketPtr, HASH_SIZE);
    bzero((char *)hashTable, HASH_SIZE * sizeof(NameBucketPtr));
#ifdef WIN32
    do
#else
    while ((file = readdir (dirp)) != NULL)
#endif
    {
	renderer = FontFileMatchRenderer (FileName(file));
	if (renderer)
	{
	    extension = renderer->fileSuffix;
	    if (excludesuf &&
		strncmp(excludesuf, extension + 1, strlen(excludesuf)) == 0) {
		continue;
	    }
	    Estrip (extension, FileName(file));
	    hash = Hash (FileName(file));
	    prev = &hashTable[hash];
	    bucket = *prev;
	    while (bucket && strcmp (bucket->name, FileName(file)))
	    {
		prev = &bucket->next;
		bucket = *prev;
	    }
	    if (bucket)
	    {
		if (bucket->renderer->number > renderer->number)
		    bucket->renderer = renderer;
	    }
	    else
	    {
		bucket = New (NameBucketRec, 1);
		if (!bucket)
		    return FALSE;
		if (!(bucket->name = MakeName (FileName(file))))
		    return FALSE;
		bucket->next = 0;
		bucket->renderer = renderer;
		*prev = bucket;
	    }
	}
    }
#ifdef WIN32
    while (FindNextFile(dirh, &file));
#endif
    for (hash = 0; hash < HASH_SIZE; hash++)
    {
	for (bucket = hashTable[hash]; bucket; bucket = next)
	{
	    next = bucket->next;
	    strcpy (fileName, bucket->name);
	    strcat (fileName, bucket->renderer->fileSuffix);
	    if (!ProcessFile (dirName, fileName, table))
	    {
		fprintf(stderr, "%s: unable to process font %s/%s, skipping\n",
			progName, dirName, fileName);
	    }
	    free (bucket->name);
	    free (bucket);
	}
    }
    free (hashTable);
    return TRUE;
}

int
LoadScalable (char *dirName, FontTablePtr table)
{
    char    file_name[MAXFONTFILENAMELEN];
    char    font_name[MAXFONTNAMELEN];
    char    dir_file[MAXFONTFILENAMELEN];
    /* "+2" is for the space and the final null */
    char    dir_line[sizeof(file_name)+sizeof(font_name)+2];
    char    dir_format[20];
    FILE    *file;
    int	    count;
    int	    i;

    strcpy(dir_file, dirName);
    if (dirName[strlen(dirName) - 1] != '/')
	strcat(dir_file, "/");
    strcat(dir_file, FontScalableFile);
    file = fopen(dir_file, "r");
    if (file) {
	count = fscanf(file, "%d\n", &i);
	if ((count == EOF) || (count != 1)) {
	    fclose(file);
	    return BadFontPath;
	}
	(void) sprintf(dir_format, "%%%lds %%%ld[^\n]\n",
		       (unsigned long)sizeof(file_name) - 1,
		       (unsigned long)sizeof(font_name) - 1);
	while (fgets(dir_line, sizeof(dir_line), file) != NULL) {
	    count = sscanf(dir_line, dir_format, file_name, font_name);
	    if (count != 2) {
		fclose(file);
		fprintf (stderr, "%s: bad format for %s file\n",
			 progName, dir_file);
		return FALSE;
	    }
	    if (!AddEntry (table, font_name, file_name))
	    {
		fclose (file);
		fprintf (stderr, "%s: out of memory\n", progName);
		return FALSE;
	    }
	}
	fclose(file);
    } else if (errno != ENOENT) {
	perror (dir_file);
	return FALSE;
    }
    return TRUE;
}

static Bool
CompareEncodingFiles(char *name1, char *name2)
{
  int len, len1, len2, p1, p2, i;
  char *extension;

  len1=strlen(name1);
  len2=strlen(name2);
  p1=p2=-1;

  for(extension=encodingExtensions[0], i=0;
      i<NUMENCODINGEXTENSIONS;
      extension++, i++) {
    len=strlen(extension);
    if(p1<0 && len1>=len && !strcmp(name1+len1-len, extension))
      p1=i;
    if(p2<0 && len2>=len && !strcmp(name2+len2-len, extension))
      p2=i;
  }

  if(p1<0)
    return FALSE;
  else if(p2<0)
    return TRUE;
  else
    return(p1<p2);
}

static Bool
InsertEncoding(EncodingBucketPtr *encodings,
               char *name, char *fileName, int priority)
{
  int bucket;
  EncodingBucketPtr encoding;

  bucket=Hash(name)%ENCODING_HASH_SIZE;

  for(encoding=encodings[bucket]; encoding; encoding=encoding->next) {
    if(!strcmp(name, encoding->name)) {
      if(encoding->priority<priority)
        return TRUE;
      else if(encoding->priority>priority)
        break;
      else if(CompareEncodingFiles(fileName, encoding->fileName))
        break;
      else
        return TRUE;
    }
  }
  
  if(!encoding) {
    /* Need to insert new bucket */
    if((encoding=New(EncodingBucketRec, 1))==NULL)
      return FALSE;
    encoding->next=encodings[bucket];
    encodings[bucket]=encoding;
  }

  /* Now encoding points to a bucket to fill in */
  encoding->name=name;
  encoding->fileName=fileName;
  encoding->priority=priority;
  return TRUE;
}

Bool
LoadEncodings(EncodingBucketPtr *encodings, char *dirName, int priority)
{
  char *filename;
  char **names;
  char **name;
  char fullname[MAXFONTFILENAMELEN];
  int len;
#ifdef WIN32
  HANDLE		dirh;
  WIN32_FIND_DATA	file;
#else
  DIR			*dirp;
  struct dirent	        *file;
#endif

  if (strcmp(dirName, ".") == 0) {
    len=0;
  } else {
    len=strlen(dirName);
    strcpy(fullname, dirName);
    if(fullname[len-1]!='/')
      fullname[len++]='/';
  }
    

#ifdef WIN32
  if ((dirh = FindFirstFile("*.*", &file)) == INVALID_HANDLE_VALUE)
    return FALSE;
#else
  if ((dirp = opendir (dirName)) == NULL)
    return FALSE;
#endif
#ifdef WIN32
  do {
#else
  while ((file = readdir (dirp)) != NULL) {
#endif
    if(len+strlen(FileName(file))>=MAXFONTFILENAMELEN) {
      fprintf(stderr, "%s: warning: filename `%s/%s' too long, ignored\n",
              progName, dirName, FileName(file));
      continue;
    }
    strcpy(fullname+len, FileName(file));
    names=FontEncIdentify(fullname);
    if(names) {
      if((filename=New(char, strlen(fullname)+1))==NULL) {
        fprintf(stderr, "%s: warning: out of memory.\n", progName);
        break;
      }
      strcpy(filename, fullname);
      for(name=names; *name; name++)
        if(!InsertEncoding(encodings, *name, filename, priority))
	  fprintf(stderr, "%s: warning: failed to insert encoding %s\n", 
		  progName, *name);
      /* Only free the spine -- the names themselves may be used */
      free(names);
    }
  }
#ifdef WIN32
    while (FindNextFile(dirh, &file));
#endif
  return TRUE;
}

static Bool
DoDirectory(char *dirName, EncodingBucketPtr *encodings, int count)
{
    FontTableRec	table;
    Bool		status;

    status = TRUE;

    if(processFonts) {
        if (!FontFileInitTable (&table, 100))
            return FALSE;
        if (!LoadDirectory (dirName, &table))
            {
                FontFileFreeTable (&table);
                return FALSE;
            }
        if (!LoadScalable (dirName, &table))
            {
                FontFileFreeTable (&table);
                return FALSE;
            }
        if (table.used >= 0)
            status = WriteFontTable (dirName, &table);
        FontFileFreeTable (&table);
    }
    if (status)
	WriteEncodingsTable(dirName, encodings, count);
    return status;
}

int
GetDefaultPointSize (void)
{
    return 120;
}

FontResolutionPtr GetClientResolutions (int *num)
{
    return 0;
}

void
ErrorF (void)
{
}

/***====================================================================***/

int
main (int argc, char **argv)
{
    int argn, i, count;
    char *dirname, fulldirname[MAXFONTFILENAMELEN];
    EncodingBucketPtr *encodings;
    EncodingBucketPtr encoding;

    BitmapRegisterFontFileFunctions ();
    progName = argv[0];
    if((encodings=New(EncodingBucketPtr, ENCODING_HASH_SIZE))==NULL) {
      fprintf(stderr, "%s: out of memory\n", progName);
      exit(2);
    }

    for(i=0; i<ENCODING_HASH_SIZE; i++)
      encodings[i]=NULL;

    for(argn=1; argn<argc; argn++) {
      if(argv[argn][0]=='\0' || argv[argn][0]!='-')
        break;
      if(argv[argn][1]=='-') {
        argn++;
        break;
      } else if(argv[argn][1]=='e') {
        if(argv[argn][2]=='\0') {
          argn++;
	  if (argn < argc)
            dirname=argv[argn];
	  else {
	    fprintf(stderr, "%s: -e requires an argument\n", progName);
	    break;
	  }
        } else
          dirname=argv[argn]+2;
        if(dirname[0]=='/' || relative)
          LoadEncodings(encodings, dirname, argn);
        else {
          if(getcwd(fulldirname, MAXFONTFILENAMELEN)==NULL) {
            fprintf(stderr, "%s: failed to get cwd\n", progName);
            break;
          }
          i=strlen(fulldirname);
          if(i+1+strlen(dirname)>=MAXFONTFILENAMELEN-1) {
          fprintf(stderr, "%s: directory name `%s' too long\n", progName,
		  dirname);
          break;
          }
          fulldirname[i++]='/';
          strcpy(fulldirname+i, dirname);
          LoadEncodings(encodings, fulldirname, argn);
        }
      } else if(argv[argn][1]=='p') {
        if(argv[argn][2]=='\0') {
          argn++;
          prefix=argv[argn];
        } else
          prefix=argv[argn]+2;
      } else if(argv[argn][1]=='n') {
        if(argv[argn][2] == '\0') {
          processFonts = FALSE;
        } else {
          fprintf(stderr, "%s: unknown option `%s'\n", progName, argv[argn]);
          continue;
        }
      } else if(argv[argn][1]=='r') {
        if(argv[argn][2]=='\0')
          relative=TRUE;
        else {
          fprintf(stderr, "%s: unknown option `%s'\n", progName, argv[argn]);
          continue;
        }
      } else if(argv[argn][1]=='x') {
        if(argv[argn][2]=='\0') {
          argn++;
	  if (argn < argc)
            excludesuf=argv[argn];
	  else {
	    fprintf(stderr, "%s: -x requires an argument\n", progName);
	    break;
	  }
        } else
          excludesuf=argv[argn]+2;
      } else
        fprintf(stderr, "%s: unknown option `%s'\n", progName, argv[argn]);
    }

    count=0;
    for(i=0; i<ENCODING_HASH_SIZE; i++)
      for(encoding=encodings[i]; encoding; encoding=encoding->next) 
        count++;
        
    if (argn==argc)
    {
	if (!DoDirectory(".", encodings, count))
	{
	    fprintf (stderr, "%s: failed to create directory in %s\n",
		     progName, ".");
	    exit (1);
	}
    }
    else
	for (; argn < argc; argn++) {
	    if (!DoDirectory(argv[argn], encodings, count))
	    {
		fprintf (stderr, "%s: failed to create directory in %s\n",
			 progName, argv[argn]);
		exit (1);
	    }
 	}
    exit(0);	
}
