/*
 * makepsres.c
 *
 * (c) Copyright 1991, 1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/programs/makepsres/makepsres.c,v 1.9 2003/10/24 20:38:13 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#ifdef XENVIRONMENT
#include <X11/Xos.h>
#else
#include <string.h>
#include <sys/types.h>
#endif

#include <sys/stat.h>

#include <dirent.h>

#ifndef S_ISDIR
#ifdef S_IFDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#else
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif /* S_IFDIR */
#endif /* S_ISDIR */

#define	true	 1
#define	false	 0

/* The max line length is really 256, but why make things that are hard
   to read??? */
#define MAXLINELEN 79

#ifndef DEBUG
#define DEBUG false
#endif

#define	QUOTE 042

#define HASHSIZE 2048

/* Command line information */

char **directories;
int *directoryLen;
int directoryCount;
int recursive;
int discard;
int keep;
char *outputFilename;
char **inputFiles;
int inputCount;
int makeExclusive;
int interactive;
int strict;
int noPrefix;
int issueWarnings;
int noBackup;
int noSuffix;

typedef struct _t_Resource {
  char *name;
  char *file;
  int noPrefix;
  struct _t_Resource *next;
} Resource;

typedef struct _t_Duplicate {
  char *name;
  char *file1;
  char *file2;
  struct _t_Duplicate *next;
} Duplicate;

typedef struct _t_Category {
  char *name;
  Resource *list;
  Resource **hash;	/* Currently used only for mkpsresPrivate */
  Duplicate *duplicates;
  struct _t_Category *next;
} Category;

char *program;

#if 0
extern char *malloc(), *realloc();
extern char *sys_errlist[];
extern int errno;
#endif

#define BUFFER_SIZE 1024
static char lineBuffer[BUFFER_SIZE];

Category *categories;

static char *ckmalloc(int size, char *whynot)
{
    char *result;

    if (size == 0) size = 1;
    result = malloc(size);
    if (result == NULL) {
	fprintf(stderr, "%s:  %s\n", program, whynot);
	exit(1);
    }
    return result;
}

static char *ckrealloc(char *ptr, int size, char *whynot)
{
    char *result;

    if (size == 0) size = 1;
    result = realloc(ptr, size);
    if (result == NULL) {
	fprintf(stderr, "%sf : %s\n", program, whynot);
	exit(1);
    }
    return result;
}

static char *ckcalloc(int count, int size, char *whynot)
{
    char *result;

    if (size == 0) size = 1;
    if (count == 0) count = 1;
    result = (char *) calloc(count, size);
    if (result == NULL) {
	fprintf(stderr, "%s:  %s\n", program, whynot);
	exit(1);
    }
    return result;
}


static Category *AddCategory (name)
  char *name;
{
  Category *newCategory = (Category *) ckcalloc (1, sizeof (Category),
		"Failed to allocate Category record.");

  newCategory->name = (char *) ckmalloc (strlen (name) + 1,
					 "Failed to allocate Category name.");

  strcpy (newCategory->name, name);

  if (categories == NULL) {
    categories = newCategory;
  } else {
    /* Insert into alphabetical position */
    Category *current,
           *previous;

    current = previous = categories;

    while (current != NULL) {
      if (strcmp (current->name, name) > 0) {
	break;
      } else {
	previous = current;
	current = current->next;
      }
    }

    if (current == NULL) {
      newCategory->next = NULL;
      previous->next = newCategory;
    } else {
      newCategory->next = current;

      if (current == categories) {
	categories = newCategory;
      } else {
	previous->next = newCategory;
      }
    }
  }

  return (newCategory);
}


static Category *FindCategory (name)
  char *name;
{
  Category *category = categories;

  while (category != NULL) {
    if (strcmp (category->name, name) == 0)
      break;
    else
	category = category->next;
  }

   if (category == NULL)
     category = AddCategory (name);

  return (category);
}


int Hash(string)
    char *string;
{
    int hash = 0;
    unsigned char *ch = (unsigned char *) string;

    while (1) {
	if (*ch == '\0') return hash % HASHSIZE;
	if (*(ch+1) == '\0') {
	    hash += *ch;
	    return hash % HASHSIZE;
	}
	hash += *ch++;
	hash += (*ch++ << 8);
    }
}

static void AddHashedResource(resource, category)
    Resource *resource;
    Category *category;
{
    Resource *current, *previous;
    int     comparison, hash;

    if (category->hash == NULL) {
	category->hash = (Resource **) ckcalloc(HASHSIZE, sizeof(Resource *),
					"Failed to allocate hash table.");
    }

    hash = Hash(resource->file);
    current = previous = category->hash[hash];

    while (current != NULL) {
	comparison = strcmp (current->file, resource->file);
	if (comparison > 0) break;

	if (comparison == 0 &&
	    strcmp(current->name, resource->name) != 0) break;

	previous = current;
	current = current->next;
    }

    if (category->hash[hash] == NULL) {
	category->hash[hash] = resource;
	resource->next = NULL;

    } else if (current == NULL) {
	resource->next = NULL;
	previous->next = resource;

    } else {
	resource->next = current;

	if (current == category->hash[hash]) {
	    category->hash[hash] = resource;
	} else {
	    previous->next = resource;
	}
    }
}

void EnterDuplicateWarning(category, res1, res2)
    Category *category;
    Resource *res1, *res2;
{
    Duplicate *dup, *previous, *current;

    if (!issueWarnings) return;

    dup = (Duplicate *) ckcalloc(1, sizeof(Duplicate),
				 "Failed to allocate Duplicate record.");

    dup->name = res1->name;
    dup->file1 = res1->file;
    dup->file2 = res2->file;

    current = previous = category->duplicates;

    while (current != NULL) {
	if (strcmp (current->name, res1->name) >= 0) break;
	previous = current;
	current = current->next;
    }

    if (category->duplicates == NULL) category->duplicates = dup;
    else if (current == NULL) {
	dup->next = NULL;
	previous->next = dup;
    } else {
	dup->next = current;

	if (current == category->duplicates) category->duplicates = dup;
	else previous->next = dup;
    }
}

static void AddResource (categoryName, resourceName, fileName, noPrefix)
  char *resourceName;
  char *categoryName;
  char *fileName;
  int noPrefix;
{
  Category *category = FindCategory (categoryName);
  Resource *resource,
         *current,
         *previous;
  int     comparison;

  resource = (Resource *) ckcalloc (1, sizeof (Resource),
				    "Failed to allocate Resource record.");

  resource->name = ckmalloc (strlen (resourceName) + 1,
			     "Failed to allocate Resource name.");

  strcpy (resource->name, resourceName);

  if (fileName[0] == '.' && fileName[1] == '/') fileName+=2;

  resource->file = ckmalloc (strlen (fileName) + 1,
			     "Failed to allocate Resource filename.");

  strcpy (resource->file, fileName);

  resource->noPrefix = noPrefix;

  if (strcmp(categoryName, "mkpsresPrivate") == 0) {
      AddHashedResource(resource, category);
      return;
  }

  current = previous = category->list;

  while (current != NULL) {
      comparison = strcmp (current->name, resourceName);
      if (comparison > 0) break;
      else if (comparison == 0) {
	  comparison = strcmp (current->file, fileName);
	  if (comparison > 0) {
	      if (strcmp(categoryName, "FontBDFSizes") != 0 &&
		  strcmp(categoryName, "FontFamily") != 0 &&
		  strcmp(categoryName, "mkpsresPrivate") != 0) {
		  EnterDuplicateWarning(category, current, resource);
	      }
	      break;
	  } else if (comparison == 0) {		/* Same file */
	      free (resource->name);
	      free (resource->file);
	      free (resource);
	      return;
	  }
      }
      previous = current;
      current = current->next;
  }

  if (category->list == NULL) {
    category->list = resource;
  } else if (current == NULL) {
    resource->next = NULL;
    previous->next = resource;
  } else {
    resource->next = current;

    if (current == category->list) {
      category->list = resource;
    } else {
      previous->next = resource;
    }
  }
}

int FindResource(categoryName, resourceName)
  char *categoryName;
  char *resourceName;
{
    Category *category = FindCategory (categoryName);
    Resource *resource;
    int i;

    for (resource = category->list;
	 resource != NULL && strcmp(resource->name, resourceName) != 0;
	 resource = resource->next) {}

    if (resource != NULL) return true;

    if (category->hash == NULL) return false;

    for (i = 0; i < HASHSIZE; i++) {
	for (resource = category->hash[i];
	     resource != NULL && strcmp(resource->name, resourceName) != 0;
	     resource = resource->next) {}
	if (resource != NULL) return true;
    }

    return false;
}


typedef struct _t_UPRResource {
  char *name;
  char *file;
  char *category;
  int found;
  int noPrefix;
  struct _t_UPRResource *next;
} UPRResource;

UPRResource *UPRresources[HASHSIZE];

#if DEBUG
int bucketCount[HASHSIZE];
int totalHashed = 0;
#endif

static void AddUPRResource (categoryName, resourceName, fileName, prefix,
			    noPrefix)
  char *resourceName;
  char *categoryName;
  char *fileName;
  char *prefix;
  int noPrefix;
{
  UPRResource *resource, *current, *previous;
  int     comparison, hash;

  if (noPrefix || prefix == NULL) {
      prefix = "";
      if (fileName[0] == '.' && fileName[1] == '/') fileName+=2;
  } else {
      prefix++; /* Skip over leading / */
      if (prefix[0] == '.' && prefix[1] == '/') prefix += 2;
  }

  resource = (UPRResource *) ckcalloc (1, sizeof (UPRResource),
				    "Failed to allocate Resource record.");

  resource->name = ckmalloc (strlen (resourceName) + 1,
			     "Failed to allocate Resource name.");

  strcpy (resource->name, resourceName);

  resource->file = ckmalloc (strlen (fileName) + strlen(prefix) + 2,
			     "Failed to allocate Resource filename.");

  if (prefix != NULL && prefix[0] != '\0') {
      strcpy (resource->file, prefix);
      strcat (resource->file, "/");
      strcat (resource->file, fileName);
  } else strcpy (resource->file, fileName);

  resource->category = ckmalloc (strlen (categoryName) + 1,
				 "Failed to allocate Resource name.");

  strcpy(resource->category, categoryName);

  resource->noPrefix = noPrefix;
  resource->found = false;

  hash = Hash(resource->file);
  current = previous = UPRresources[hash];

  while (current != NULL) {
      comparison = strcmp (current->file, resource->file);
      if (comparison > 0) break;

      if (comparison == 0) {
	  if (noPrefix) break;

	  if (strcmp(current->name, resource->name) != 0 ||
	      strcmp(current->category, resource->category) != 0) { /* Same */
	      if (strcmp(current->category, "mkpsresPrivate") == 0 &&
		  strcmp(current->name, "NONRESOURCE") == 0) {

		  /* Replace "NONRESOURCE" entry with resource one */
		  free(current->name);
		  current->name = resource->name;
		  free(current->category);
		  current->category = resource->category;
		  free(resource->file);
		  free (resource);
		  return;
	      }
	      fprintf(stderr,
		  "%s:  Warning:  file %s identified as different resources\n",
		  program, resource->file);
	      fprintf(stderr, "            Using %s\n", current->category);
	  }
	  free (resource->name);
	  free (resource->file);
	  free (resource->category);
	  free (resource);
	  return;
      }
      previous = current;
      current = current->next;
  }

  if (UPRresources[hash] == NULL) {
      UPRresources[hash] = resource;
      resource->next = NULL;

  } else if (current == NULL) {
      resource->next = NULL;
      previous->next = resource;
  } else {
      resource->next = current;

      if (current == UPRresources[hash]) {
	  UPRresources[hash] = resource;
      } else {
	  previous->next = resource;
      }
  }
#if DEBUG
  totalHashed++;
  bucketCount[hash]++;
#endif
}


void AddUPRResourceBDF(font, sizes)
    char *font;
    char *sizes;
{
    char *ch = sizes;
    char *buf;

    while (*ch != '\0') {
	while (*ch != '\0' && *ch != ',') ch++;
	if (*ch == ',') {
	    *ch = '\0';
	    if (*sizes != '\0') {
		/* Stick in the font size to spread out the hash table */

		buf = ckmalloc(strlen(font) + strlen(sizes) + 2,
			       "Failed to allocate BDF string");
		sprintf(buf, "%s,%s", font, sizes);
		AddUPRResource("FontBDFSizes", font, buf, NULL, true);
		free(buf);
	    }
	    sizes = ++ch;
	}
    }
}

void AddUPRResourceFontFamily(family, faces)
    char *family;
    char *faces;
{
    char *ch = faces, *chunk = faces;
    char old;

    while (true) {
	while (true) {
	    while (*ch != '\0' && *ch != ',') ch++;
	    if (*ch == '\0') return;
	    if (ch > faces && *(ch-1) == '\\') ch++;
	    else break;
	}
	/* Found the first , look for the second */
	ch++;
	while (true) {
	    while (*ch != '\0' && *ch != ',') ch++;
	    if (*ch == '\0') break;
	    if (*(ch-1) == '\\') ch++;
	    else break;
	}

	old = *ch;
	*ch = '\0';
	AddUPRResource("FontFamily", family, chunk, NULL, true);
	if (old == '\0') return;
	chunk = ++ch;
    }
}

static int SkipWhiteSpace(file)
    FILE *file;
{
    int c;

    while (1) {
	c = fgetc(file);
	if (c == ' ' || c == '\t') continue;
	if (c == EOF) return false;
	ungetc(c, file);
	return true;
    }
}


static int ReadItem(file, buf, size)
    FILE *file;
    char *buf;
    int size;
{
    int c;
    char closechar, openchar;
    int count = 0, nesting = 0;;

    openchar = '\0';

    c = fgetc(file);
    if (c == EOF) return false;
    if (c == '(') {closechar = ')'; openchar = c;}
    else if (c == '[') {closechar = ']'; openchar = c;}
    else if (c == QUOTE) closechar = QUOTE;
    else if (c == '/') closechar = '\0';
    else {
	closechar = '\0';
	ungetc(c, file);
    }

    while (count < size) {
	c = fgetc(file);
	if (openchar != '\0' && c == openchar) nesting++;
	if (c == EOF) break;
	if (c == closechar) {
	    if (nesting == 0) break;
	    else nesting--;
	}
	if (closechar == '\0' && strchr(" \t\n\r", c) != NULL) break;
	buf[count++] = c;
    }

    buf[count] = '\0';
    return true;
}

static char *FindKeyValue (file, key)
  FILE *file;
  char *key;
{
  char    lineKey[64];
  char   *result = NULL;

  while (true) {
    if (fgets (lineBuffer, BUFFER_SIZE, file) == NULL)
      break;

    sscanf (lineBuffer, "%63[%a-zA-Z]", lineKey);
    if (strcmp (key, lineKey) == 0) {
      result = strchr (lineBuffer, ' ');
      if (result != NULL) {
	result++;
	break;
      }
    }
  }

  return (result);
}

static void StripName (name)
  char *name;
{
  char    closeCharacter = '\0';
  char   *pointer;

  if (name[0] == '/') strcpy(name, name+1);

  while (true) {
    if (name[0] == '(') closeCharacter = ')';
    else if (name[0] == QUOTE) closeCharacter = QUOTE;
    else break;

    pointer = strrchr (name, closeCharacter);

    if (pointer != NULL) {
      *pointer = '\0';
      strcpy (name, name + 1);
    } else break;			/* No close character */
  }

  pointer = strrchr (name, '\r');

  if (pointer != NULL) *pointer = '\0';
  else {
    pointer = strrchr (name, '\n');
    if (pointer != NULL) *pointer = '\0';
  }
}


static char *bugFamilies[] = {
    "Berkeley", "CaslonFiveForty", "CaslonThree", "GaramondThree",
    "Music", "TimesTen", NULL
	};

static char *fixedFamilies[] = {
    "ITC Berkeley Oldstyle", "Caslon 540", "Caslon 3", "Garamond 3",
    "Sonata", "Times 10", NULL
};

static char *missingFoundries[] = {
    "Berthold ", "ITC ", "Linotype ", NULL
};

static int missingFoundryLen[] = {
    9, 4, 9, 0
};

static void MungeFontNames(name, family, fullname, weight,
			   familyReturn, fullnameReturn, faceReturn)
    char *name, *family, *fullname, *weight;
    char *familyReturn, *fullnameReturn, *faceReturn;
{
    register char *src, *dst, prev;
    char buf[256];
    int digits = 0;
    int i, diff;

    /* Copy the fullname into buf, enforcing one space between words.
       Eliminate leading digits and spaces, ignore asterisks, if the
       full name ends with 5 digits strip them, and replace periods that
       aren't followed by a space with a space.  If leading digits are
       followed by " pt " skip that too. */

    dst = buf;
    prev = ' ';
    src = fullname; 
    while (isdigit(*src)) src++;
    while (*src == ' ' || *src == '\t') src++;
    if (strncmp(src, "pt ", 3) == 0) src += 3;
    else if (strncmp(src, "pt. ", 4) == 0) src += 4;

    while (*src != '\0') {
	if (*src == '*') {
	    src++;
	    continue;
	}

	if (*src == '.') {
	    if (*(src+1) != ' ') {
		prev = *dst++ = ' ';
	    } else prev = *dst++ = '.';
	    src++;
	    continue;
	}

	if (isdigit(*src)) digits++;
	else digits = 0;

	if (isupper(*src)) {
	    if (prev != ' ' && (islower(*(src+1)) || islower(prev))) {
		*dst++ = ' ';
		prev = *dst++ = *src++;
	    } else prev = *dst++ = *src++;

	} else if (*src == ' ' || *src == '\t') {
	    if (prev == ' ') {
		src++;
		continue;
	    }
	    prev = *dst++ = ' ';
	    src++;

	} else prev = *dst++ = *src++;
    }

    if (digits == 5) {
	dst -= 5;
    }
    if (dst > buf && *(dst-1) == ' ') dst--;

    *dst = '\0';

    if (strcmp(name, "FetteFraktur-Dfr") == 0) strcat(buf, " Black Dfr");
    else if (strcmp(name, "Linotext-Dfr") == 0) strcat(buf, " Dfr");

    if (strncmp(fullname, "pt ", 3) == 0) {
	src = buf + 2;
	while (*++src != '\0') *(src-3) = *src;
	*(src-3) = '\0';
    }

    strcpy(fullnameReturn, buf);

    /* From here on fullname should not be used */

    /* Done with the full name; now onto the family */

    for (i = 0; bugFamilies[i] != NULL; i++) {
	diff = strcmp(family, bugFamilies[i]);
	if (diff < 0) break;
	if (diff == 0) {
	    strcpy(familyReturn, fixedFamilies[i]);
	    goto FAMILY_DONE;
	}
    }

    /* Copy the family into buf, enforcing one space between words */

    dst = buf;
    prev = ' ';
    src = family; 

    while (*src != '\0') {
	if (isupper(*src)) {
	    if (prev != ' ' && (islower(*(src+1)) || islower(prev))) {
		*dst++ = ' ';
		prev = *dst++ = *src++;
	    } else prev = *dst++ = *src++;

	} else if (*src == ' ' || *src == '\t') {
	    if (prev == ' ') {
		src++;
		continue;
	    }
	    prev = *dst++ = ' ';
	    src++;

	} else prev = *dst++ = *src++;
    }

    if (dst > buf && *(dst-1) == ' ') dst--;
    *dst = '\0';

    /* Compensate for fonts with foundries in the full name but not the
       family name by adding to the family name */
 
    for (i = 0; missingFoundries[i] != NULL; i++) {
	diff = strncmp(fullnameReturn, missingFoundries[i],
		       missingFoundryLen[i]);
	if (diff > 0) continue;
	if (diff == 0 && strncmp(buf, missingFoundries[i],
		       missingFoundryLen[i] != 0)) {
	    while (dst >= buf) {
		*(dst+missingFoundryLen[i]) = *dst;
		dst--;
	    }
	    strncpy(buf, missingFoundries[i], missingFoundryLen[i]);
	}
	break;
    }

    /* From here on dst no longer points to the end of the buffer */

    if (strncmp(fullnameReturn, "Helvetica Rounded ", 18) == 0) {
	strcat(buf, " Rounded");
    }

    strcpy(familyReturn, buf);

FAMILY_DONE:

    /* From here on family should not be used */

    /* Now to find the face in all this */

    src = fullnameReturn;
    dst = familyReturn;
    while (*dst == *src && *dst != '\0') {
        src++;
        dst++;
    }
    if (*src == ' ') src++;

    if (*src == '\0') {
	if (*weight != '\0') {
	    /* Handle Multiple Master fonts */
	    if (strcmp(weight, "All") == 0) src = "Roman";
	    else {
		if (islower(weight[0])) weight[0] = toupper(weight[0]);
		src = weight;
	    }
	} else src = "Medium";
    }

    strcpy(faceReturn, src);
}


void StripComments(buf)
    char *buf;
{
    register char *ch = buf;

    while (true) {
	while (*ch != '%' && *ch != '\0') ch++;
	if (*ch == '\0') break;
	if (ch == buf || *(ch-1) != '\\') {
	    *ch = '\0';
	    break;
	}
	ch++;
    }

    /* ch points to '\0' right now */

    if (ch == buf) return;
    ch--;

    while (ch > buf && (*ch == ' ' || *ch == '\t' || *ch == '\n')) {
	*ch = '\0';
	ch--;
    }

    if (ch == buf && (*ch == ' ' || *ch == '\t' || *ch == '\n')) *ch = '\0';
}

/* Caller must free returned line */

char *GetWholeLine(file)
    FILE *file;
{
    char *line;
    int len, oldlen;

    if (fgets (lineBuffer, BUFFER_SIZE, file) == NULL) return NULL;

    StripComments(lineBuffer);
    
    len = strlen(lineBuffer); 
    line = ckmalloc(len+1, "Failed to allocate input line.");
    strcpy(line, lineBuffer);

    if (line[len-1] == '\\') {	/* Continued... */
	line[len-1] = '\0';
	oldlen = len-1;
	while (true) {
	    if (fgets(lineBuffer, BUFFER_SIZE, file) == NULL) {
		return line;
	    }

	    StripComments(lineBuffer);
	    if (lineBuffer[0] == '\0') return line;

	    len = strlen(lineBuffer);
	    line = ckrealloc(line, oldlen+len+1,
			     "Failed to reallocate input line.");
	    strcat(line, lineBuffer);

	    oldlen += len;
	    if (line[oldlen-1] != '\\') break;
	    line[oldlen-1] = '\0';
	    oldlen--;
	}
    }
    return line;
}

static void HandleUnopenableUPRFile(filename, err)
    char *filename;
    int err;
{
    if (issueWarnings) {
	fprintf (stderr, "%s:  Could not open file %s (%s).\n",
		 program, filename, strerror(err));
    }

    if (strict) exit(1);
}


void PreprocessResourceDirectory(fullname)
    char *fullname;
{
    char *category;
    FILE *file;
    char *line;
    char *directoryPrefix = NULL;
    int noPrefix;

    file = fopen (fullname, "r");

    if (file == NULL) {
	HandleUnopenableUPRFile(fullname, errno);
	return;
    }

    /* Skip over list of categories */

    while (true) {
	if (fgets (lineBuffer, BUFFER_SIZE, file) == NULL) return;

	if (lineBuffer[0] == '.') break;
    }

    while (true) {
	/* Process category */
     
	line = GetWholeLine(file);
	if (line == NULL) {
	    if (directoryPrefix != NULL) free(directoryPrefix);
	    return;
	}
      
	if (line[0] == '/') { /* Handle optional directory prefix */
	    directoryPrefix = line;
	    continue;
	}

	category = line;

	while (true) {
	    char *resourceFile;

	    line = GetWholeLine(file);
	    if (line == NULL) {
		if (directoryPrefix != NULL) free(directoryPrefix);
		free(category);
	    }

	    if (line[0] == '.') {
		free(category);
		free(line);
		break;
	    }

	    resourceFile = line;
	    while (true) {
		if ((resourceFile = strchr(resourceFile, '=')) != NULL) {
		    if (resourceFile != line && *(resourceFile-1) != '\\') {
			*resourceFile++ = '\0';
			noPrefix = (*resourceFile == '=');
			if (noPrefix) resourceFile++;
			if (strcmp(category, "FontBDFSizes") == 0) {
			    AddUPRResourceBDF(line, resourceFile);
			} else if (strcmp(category, "FontFamily") == 0) {
			    AddUPRResourceFontFamily(line, resourceFile);
			} else AddUPRResource (category, line,
					       resourceFile,
					       (noPrefix ? NULL :
						           directoryPrefix),
					       noPrefix);
			break;
		    }
		    resourceFile++;
		} else break;	/* Bogus line */
	    }
	    free(line);
	}
    }
}

static int SkipToCharacter (file, character)
  FILE *file;
  char character;
{
  int c;

  while ((c = fgetc (file)) != EOF) {
    if (c == character)
      return (true);
  }

  return (false);
}

static int SkipToEitherCharacter (file, character1, character2, outchar)
  FILE *file;
  char character1, character2;
  char *outchar;
{
  register int c;

  while ((c = fgetc (file)) != EOF) {
    if (c == character1 || c == character2) {
	*outchar = c;
	return (true);
    }
  }

  return (false);
}


static void ProcessFont (file, fileName)
    FILE *file;
    char *fileName;
{
    char fontName[256], fontFamily[256], fontFullName[256], fontWeight[256];
    char key[256], buf[513];
    char familyReturn[256], fullnameReturn[256], faceReturn[256];
    char blendDesignPositions[256], blendDesignMap[256], blendAxisTypes[256];  
    char out;
    int found = 0;

    fontName[0] = fontFamily[0] = fontFullName[0] = fontWeight[0] = '\0';
    blendDesignPositions[0] = blendDesignMap[0] = blendAxisTypes[0] = '\0';  

    while (found != 0x7F && SkipToEitherCharacter (file, '/', 'e', &out)) {
	/* If we encounter an eexec, skip the rest of the file */
	if (out == 'e') {
	    if (fscanf (file, "%255s", key) != 1) continue;
	    if (strcmp(key, "exec") == 0) break;
	    continue;
	}

	if (fscanf (file, "%255s", key) != 1) continue;
	if (!SkipWhiteSpace(file)) break;
	if (!ReadItem(file, buf, 256)) break;

	if ((found & 0x1) == 0 && strcmp(key, "FullName") == 0) {
	    strcpy(fontFullName, buf);
	    found |= 0x1;
	    continue;
	}
	if ((found & 0x2) == 0 && strcmp(key, "FamilyName") == 0) {
	    strcpy(fontFamily, buf);
	    found |= 0x2;
	    continue;
	}
	if ((found & 0x4) == 0 && strcmp(key, "Weight") == 0) {
	    strcpy(fontWeight, buf);
	    found |= 0x4;
	    continue;
	}
	if ((found & 0x8) == 0 && strcmp(key, "FontName") == 0) {
	    strcpy(fontName, buf);
	    found |= 0x8;
	    continue;
	}
	if ((found & 0x10) == 0 && strcmp(key, "BlendDesignPositions") == 0) {
	    strcpy(blendDesignPositions, buf);
	    found |= 0x10;
	    continue;
	}
	if ((found & 0x20) == 0 && strcmp(key, "BlendDesignMap") == 0) {
	    strcpy(blendDesignMap, buf);
	    found |= 0x20;
	    continue;
	}
	if ((found & 0x40) == 0 && strcmp(key, "BlendAxisTypes") == 0) {
	    strcpy(blendAxisTypes, buf);
	    found |= 0x40;
	    continue;
	}
    }

    if (fontName[0] != '\0') {
	if (fontFullName[0] == '\0') {
	    if (fontFamily[0] != '\0') strcpy(fontFullName, fontFamily);
	    else strcpy(fontFullName, fontName);
	}
	if (fontFamily[0] == '\0') strcpy(fontFamily, fontFullName);

	MungeFontNames(fontName, fontFamily, fontFullName, fontWeight,
		       familyReturn, fullnameReturn, faceReturn);
#if DEBUG
	printf("Found font %s\n", fontName);
	printf("Munged to (%s) (%s)\n", familyReturn, faceReturn);
#endif
	sprintf(buf, "%s,%s", faceReturn, fontName);
	AddResource ("FontOutline", fontName, fileName, false);
	AddResource("FontFamily", familyReturn, buf, true);
	if (blendDesignPositions[0] != '\0' && blendDesignMap[0] != '\0' &&
	    blendAxisTypes[0] != '\0') {
#if DEBUG
	    printf("Font %s is multiple master\n", fontName);
#endif
	    AddResource("FontBlendPositions", fontName, blendDesignPositions, true);
	    AddResource("FontBlendMap", fontName, blendDesignMap, true);
	    AddResource("FontAxes", fontName, blendAxisTypes, true);
	}
    }
}


static void ProcessResource (file, fileName)
    FILE *file;
    char *fileName;
{
    char    resourceType[256];
    char    resourceName[256];
    char   *pointer;

    sscanf (lineBuffer, "%%!PS-Adobe-%*[0123456789.] Resource-%128s",
	    resourceType);

    StripName (resourceType);

    if (strcmp(resourceType, "Font") == 0) {
	ProcessFont(file, fileName);
	return;
    }

    pointer = FindKeyValue (file, "%%BeginResource");

    if (pointer == NULL) return;

    sscanf (pointer, "%*256s%255s", resourceName);
    StripName (resourceName);

    AddResource (resourceType, resourceName, fileName, false);
}


static void ProcessBDF (file, fileName)
    FILE *file;
    char *fileName;
{
    char    fontName[256];
    char    psFontName[256];
    char    key[256];
    unsigned int found = 0;
    char	  nameSize[300];
    int     resx, resy, size;
    char    sizebuf[50];

    fontName[0] = psFontName[0] = '\0';
    resx = resy = size = 0;

    while (SkipToCharacter(file, '\n')) {
	if (!SkipWhiteSpace(file)) break;
	if (fscanf (file, "%255s", key) != 1) continue;
	if (!SkipWhiteSpace(file)) break;

	if ((found & 1) == 0 && strcmp(key, "FONT") == 0) {
	    if (!ReadItem(file, fontName, 256)) break;
	    found |= 1;
	    continue;
	}
	if ((found & 2) == 0 && strcmp(key, "RESOLUTION_X") == 0) {
	    if (fscanf (file, "%d", &resx) != 1) break;
	    found |= 2;
	    continue;
	}
	if ((found & 4) == 0 && strcmp(key, "RESOLUTION_Y") == 0) {
	    if (fscanf (file, "%d", &resy) != 1) break;
	    found |= 4;
	    continue;
	}
	if ((found & 8) == 0 && strcmp(key, "_ADOBE_PSFONT") == 0) {
	    if (!ReadItem(file, psFontName, 256)) break;
	    found |= 8;
	    continue;
	}
	if ((found & 16) == 0 && strcmp(key, "SIZE") == 0) {
	    if (fscanf(file, "%d %d %d", &size, &resx, &resy) != 3) break;
	    found |= 16;
	    continue;
	}
	if ((found & 32) == 0 && strcmp(key, "POINT_SIZE") == 0) {
	    if (fscanf(file, "%d", &size) != 1) break;
	    size /= 10;
	    found |= 32;
	}
	if (strcmp(key, "ENDPROPERTIES") == 0) break;
	if (strcmp(key, "STARTCHAR") == 0) break;
    }
	
    if (psFontName[0] != '\0') strcpy(fontName, psFontName);

    if (size == 0 || fontName[0] == '\0') return;
    if (resx == 0 || resy == 0) sprintf(sizebuf, "%d", size);
    else sprintf(sizebuf, "%d-%d-%d", size, resx, resy);
	
    sprintf(nameSize, "%s%s", fontName, sizebuf);

    AddResource ("FontBDF", nameSize, fileName, false);
    AddResource ("FontBDFSizes", fontName, sizebuf, true);
}

static void ProcessAFM (file, fileName)
    FILE *file;
    char *fileName;
{
    char    fontName[256];
    char   *pointer;
    char   *extraCr;

    pointer = FindKeyValue (file, "FontName");

    if (pointer == NULL)
	    return;

    sscanf (pointer, "%255s", fontName);

    extraCr = strchr (fontName, '\r'); /* Handle DOS newlines */

    if (extraCr != NULL) *extraCr = '\0';

    AddResource ("FontAFM", fontName, fileName, false);
}

/* ARGSUSED */
static void ProcessResourceDirectory (file, fileName)
    FILE *file;
    char *fileName;
{
}

void MarkAsNonResource(filename)
    char *filename;
{
    AddResource("mkpsresPrivate", "NONRESOURCE", filename, false);
}

char *userCategories[] = {
    "Encoding",
    "File",
    "FontAFM",
    "FontBDF",
    "FontOutline",
    "FontPrebuilt",
    "Form",
    "Pattern",
    "ProcSet",
    NULL
};

static void IdentifyFromUser(filename, file)
    char *filename;
    FILE *file;
{
    int i, numCats, choice, size;
    char buf[256], name[256];
    static int stdinEOF = false;

    if (stdinEOF) return;

    if (file != NULL) rewind(file);

    while (1) {
	printf("Please indentify the file\n");
	printf("  0 - Not a resource file\n");
	i = 0;
	while (userCategories[i] != NULL) {
	    printf("  %d - %s\n", i+1, userCategories[i]);
	    i++;
	}
	numCats = i;
	printf("  %d - Other\n", numCats+1);
	if (file != NULL) printf("  %d - Show some of file\n", numCats+2);
	printf("> ");
	fflush(stdout);
	if (scanf("%d", &choice) != 1) {
	    stdinEOF = true;
	    return;
	}
	/* Skip the last of the number input line */
	if (fgets(buf, 256, stdin) == NULL) {
	    stdinEOF = true;
	    return;
	}
	if (choice < 0 || (file != NULL && choice > numCats+2) ||
	    (file == NULL && choice > numCats+1)) {
	    printf("Invalid choice\n\n");
	    continue;
	}
	if (choice == numCats+2) {
	    printf("\n");
	    for (i = 0; i < 10; i++) {
		if (fgets(buf, 256, file) != NULL) fputs(buf, stdout);
	    }
	    printf("\n");
	    continue;
	}
	if (choice == 0) {
	    MarkAsNonResource(filename);
	    return;
	}
	if (choice == numCats+1) {
	    printf("Please enter resource category:  ");
	    fflush(stdout);
	    if (fgets(buf, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (buf[0] == '\0') continue;
	    printf("Please enter resource name:  ");
	    fflush(stdout);
	    if (fgets(name, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (name[0] == '\0') continue;
	    StripName(buf);
	    StripName(name);
	    AddResource(buf, name, filename, false);
	    return;
	}
	if (strcmp(userCategories[choice-1], "FontBDF") == 0) {
	    printf("Please enter font name:  ");
	    fflush(stdout);
	    if (fgets(name, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (name[0] == '\0') continue;
	    StripName(name);
	    printf("Please enter font size:  ");
	    fflush(stdout);
	    if (scanf("%d", &size) != 1) {
		stdinEOF = true;
		return;
	    }
	    /* Skip the last of the number input line */
	    if (fgets(buf, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (size <= 0) continue;
	    sprintf(buf, "%d", size);
	    AddResource("FontBDFSizes", name, buf, false);
	    sprintf(buf, "%s%d", name, size);
	    AddResource("FontBDF", buf, filename, true);
	    return;
	}
	if (strcmp(userCategories[choice-1], "FontOutline") == 0) {
	    char family[256], face[256];
	    printf("Please enter font name:  ");
	    fflush(stdout);
	    if (fgets(name, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (name[0] == '\0') continue;
	    StripName(name);
	    printf("Please enter font family:  ");
	    fflush(stdout);
	    if (fgets(family, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (name[0] == '\0') continue;
	    StripName(family);
	    printf("Please enter font face:  ");
	    fflush(stdout);
	    if (fgets(face, 256, stdin) == NULL) {
		stdinEOF = true;
		return;
	    }
	    if (name[0] == '\0') continue;
	    StripName(face);
	    AddResource("FontOutline", name, filename, true);
	    sprintf(buf, "%s,%s", face, name);
	    AddResource("FontFamily", family, buf, false);
	    return;
	}

	printf("Please enter %s name:  ", userCategories[choice-1]);
	fflush(stdout);
	if (fgets(name, 256, stdin) == NULL) {
	    stdinEOF = true;
	    return;
	}
	if (name[0] == '\0') continue;
	StripName(name);
	AddResource(userCategories[choice-1], name, filename, true);
	return;
    }
}

int IdentifyFromUPRList(filename)
    char *filename;
{
    UPRResource *resource = UPRresources[Hash(filename)];

    while (resource != NULL && strcmp(filename, resource->file) != 0) {
	resource = resource->next;
    }
    if (resource == NULL) return false;
    AddResource(resource->category, resource->name, resource->file,
		resource->noPrefix);
    return true;
}

int IdentifyFromFileSuffix(fileName)
    char *fileName;
{
    int len, len1;
    char fontName[256];
    register char *ch;

    /* The only files we can get anything useful from without looking inside
       are AFM files and prebuilt files */

    len = strlen(fileName);
    if (len < 5) return false;
    if (strcmp(".afm", fileName + len - 4) == 0) {
	len1 = 0;
	for (ch = fileName+len-5; ch > fileName && *ch != '/'; ch--) len1++;
	if (*ch == '/') ch++;
	else len1++;
	strcpy(fontName, ch);
	fontName[len1] = '\0';
	AddResource("FontAFM", fontName, fileName, false);
	return true;
    }
    if (len < 6) return false;
    if ((strcmp(".bepf", fileName + len - 5) == 0) ||
	(strcmp(".lepf", fileName + len - 5) == 0)) {
	len1 = 0;
	for (ch = fileName+len-6; ch > fileName && *ch != '/'; ch--) len1++;
	if (*ch == '/') ch++;
	else len1++;
	strcpy(fontName, ch);
	fontName[len1] = '\0';
	AddResource("FontPrebuilt", fontName, fileName, false);
	return true;
    }
    return false;
}

static void HandleUnopenableFile(filename, err)
    char *filename;
    int err;
{
    if (IdentifyFromUPRList(filename)) return;

    if (!noSuffix && IdentifyFromFileSuffix(filename)) return;

    if (issueWarnings) {
	fprintf (stderr, "%s:  Could not open file %s (%s).\n",
		 program, filename, strerror(err));
    }

    if (strict) exit(1);

    if (interactive) IdentifyFromUser(filename, (FILE *) NULL);
    else MarkAsNonResource(filename);
}


static void HandleUnidentifiableFile(filename, file)
    char *filename;
    FILE *file;
{
    if (IdentifyFromUPRList(filename)) return;

    if (!noSuffix && IdentifyFromFileSuffix(filename)) return;

    if (issueWarnings) {
	fprintf (stderr, "%s:  Could not identify file %s.\n",
		 program, filename);
    }

    if (strict) exit(1);

    if (interactive) IdentifyFromUser(filename, file);
    else MarkAsNonResource(filename);
}


typedef struct {
    char *key;
    int   keyLength;
    char *key2;
    int	  key2Length;
    void (*proc)(/* FILE *file, char *fileName */);
} ResourceKey;

static ResourceKey resourceTypes[] = {
  {"%!PS-AdobeFont-",	15, NULL, 		 0, ProcessFont},
  {"%!PS-Adobe-",	11, " Resource-",	10, ProcessResource},
  {"STARTFONT",		 9, NULL,		 0, ProcessBDF},
  {"StartFontMetrics",	16, NULL,		 0, ProcessAFM},
  {"PS-Resources-",	13, NULL,		 0, ProcessResourceDirectory},
  {NULL,		 0, NULL}
};

static void ProcessFile (fileName, filePath)
  char *fileName;
  char  *filePath;
{
  FILE   *file;
  ResourceKey *resourceType;
  char	version[10];

  if (fileName[0] == '.')
    return;

  file = fopen (filePath, "r");

  if (file == NULL) {
      HandleUnopenableFile(filePath, errno);
      return;
  }

  fgets (lineBuffer, BUFFER_SIZE, file);

  for (resourceType = resourceTypes; resourceType->key != NULL;
       resourceType++) {
      if (strncmp (resourceType->key, lineBuffer,
		   resourceType->keyLength) == 0) {
	  if (resourceType->key2 == NULL) {
	      (*resourceType->proc) (file, filePath);
	      break;

	  } else {
	      if (sscanf(lineBuffer+resourceType->keyLength,
			 "%10[0123456789.]", version) != 1) continue;

	      if (strncmp(resourceType->key2,
			  lineBuffer + resourceType->keyLength +
				  strlen(version),
			  resourceType->key2Length) == 0) {
		  (*resourceType->proc) (file, filePath);
		  break;
	      }
	  }
      }
  }

  if (resourceType->key == NULL) HandleUnidentifiableFile(filePath, file);
  
  fclose (file);
}

static void ProcessUPRFile (fileName, filePath)
  char *fileName;
  char  *filePath;
{
  int len;

  if (fileName[0] == '.')
    return;

  len = strlen(fileName);
  if (len < 4) return;
  if (strcmp(".upr", fileName + len - 4) != 0) return;

  PreprocessResourceDirectory(filePath);
}


static void ProcessDirectory (directoryName, top, fileFunction)
  char *directoryName;
  int top;
  void (*fileFunction)();
{
  DIR    *directory;
  struct dirent *directoryEntry;
  struct stat status;
  char *filePath, *fileName;

#if DEBUG
  fprintf (stderr, "Directory: %s\n", directoryName);
#endif

  directory = opendir (directoryName);

  if (directory == NULL) {
      /* Treat top level failures to open differently from subdirectories */
      if (top || issueWarnings) {
	  fprintf (stderr, "%s:  Could not open directory %s (%s).\n",
		   program, directoryName, strerror(errno));
      }
      if (strict) exit(1);
      return;
  }
  while ((directoryEntry = readdir (directory)) != NULL) {
      fileName = directoryEntry->d_name;
      if (fileName[0] == '.') continue;
      filePath = ckmalloc (strlen (directoryName) + strlen (fileName) + 2,
		       "Failed to allocate file name string.");
      sprintf (filePath, "%s/%s", directoryName, fileName);
      if (stat(filePath, &status) == -1) {
	  if (issueWarnings) {
	      fprintf(stderr, "Couldn't get status of file %s (%s)\n",
		      filePath, strerror(errno));
	  }
	  if (strict) exit(1);
	  continue;
      }
      if (S_ISDIR(status.st_mode)) {
	  if (recursive) ProcessDirectory(filePath, false, fileFunction);
      } else (*fileFunction) (fileName, filePath);
      free (filePath);
  }

  closedir (directory);
}


void GenerateEntriesFromUPRList()
{
  Category *category;
  Resource *resource;
  UPRResource *upr;
  int i, bucket;

  for (category = categories; category != NULL; category = category->next) {
      if (strcmp(category->name, "FontBDFSizes") == 0 ||
	  strcmp(category->name, "FontFamily") == 0) {
	  continue;
      }

      for (resource = category->list; resource != NULL;
	   resource = resource->next) {
	  for (upr = UPRresources[Hash(resource->file)];
	       upr != NULL && strcmp(upr->file, resource->file) < 0;
	       upr = upr->next) {}
	  if (upr != NULL) upr->found = true;
      }

      if (category->hash != NULL) {
	  for (bucket = 0; bucket < HASHSIZE; bucket++) {
	      for (resource = category->hash[bucket]; resource != NULL;
		   resource = resource->next) {
		  for (upr = UPRresources[Hash(resource->file)];
		       upr != NULL && strcmp(upr->file, resource->file) < 0;
		       upr = upr->next) {}
		  if (upr != NULL) upr->found = true;
	      }
	  }
      }
  }

  for (bucket = 0; bucket < HASHSIZE; bucket++) {
      for (upr = UPRresources[bucket]; upr != NULL; upr = upr->next) {
	  if (upr->found) continue;

	  if (strcmp(upr->category, "FontBDFSizes") != 0 &&
	      strcmp(upr->category, "FontFamily") != 0) {

	      if (!keep) {
		  /* If this file is in one of the input dirs, but wasn't
		     found, it must have been deleted since the previous run */

		  for (i = 0; i < directoryCount; i++) {
		      if (strncmp(upr->file, directories[i],
				  directoryLen[i]) == 0 &&
			  upr->file[directoryLen[i]+1] == '/') goto NEXT_UPR;
		  }
	      }
	      AddResource(upr->category, upr->name, upr->file, upr->noPrefix);
	  }
NEXT_UPR:	;
      }
  }

  /* Now do BDFSizes and Families */

  for (bucket = 0; bucket < HASHSIZE; bucket++) {
      for (upr = UPRresources[bucket]; upr != NULL; upr = upr->next) {
	  if (upr->found) continue;

	  if (strcmp(upr->category, "FontBDFSizes") == 0) {
	      char *buf, *ch = upr->file;

	      /* We know there's a comma since we put one in.  Anything
		 before the comma is just there to make hashing work better */

	      while (*ch != '\0') ch++;
	      while (ch >= upr->file && *ch != ',') ch--;
	      if (*ch == ',') ch++;

	      buf = ckmalloc(strlen(upr->name) + strlen(ch) + 1,
			     "Failed to allocate BDF name\n");
	      strcpy(buf, upr->name);
	      strcat(buf, ch);

	      if (FindResource("FontBDF", buf)) {
		  AddResource(upr->category, upr->name, ch, false);
	      }
	      free(buf);

	  } else if (strcmp(upr->category, "FontFamily") == 0) {
	      char *ch = upr->file;

	      while (true) {
		  while (*ch != '\0' && *ch != ',') ch++;
		  if (*ch == '\0') break;
		  if (ch > upr->file && *(ch-1) == '\\') ch++;
		  else break;
	      }
	      if (*ch == ',') {
		  ch++;
		  if (FindResource("FontOutline", ch)) {
		      AddResource(upr->category, upr->name, upr->file, false);
		  }
	      }
	  }
      }
  }
}


static char *ExtractDirectoryPrefix()
{
  Category *category;
  Resource *resource;
  char *prefix = NULL;
  char *ch1, *ch2;
  int bucket;

  if (noPrefix) return NULL;

  category = categories;

  while (category != NULL) {
      if (strcmp(category->name, "FontBDFSizes") == 0 ||
	  strcmp(category->name, "FontFamily") == 0 ||
	  strcmp(category->name, "FontBlendPositions") == 0 ||
	  strcmp(category->name, "FontBlendMap") == 0 ||
	  strcmp(category->name, "FontAxes") == 0) {
	  category = category->next;
	  continue;
      }
      resource = category->list;
      
      while (resource != NULL) {
	  if (resource->noPrefix) {
	      resource = resource->next;
	      continue;
	  }
	  if (prefix == NULL) {
	      prefix = ckmalloc(strlen(resource->file) + 1,
				"Failed to allocate directory prefix");
	      strcpy(prefix, resource->file);
#if DEBUG
	      printf("New directory prefix: %s\n", prefix);
#endif
	  } else {
	      ch1 = prefix;
	      ch2 = resource->file;
	      while (*ch1 != '\0' && *ch2 != '\0' && *ch1 == *ch2) {
		  ch1++; ch2++;
	      }
#if DEBUG
	      if (*ch1 != '\0') {
		  *ch1 = '\0';
		  printf("New directory prefix: %s\n", prefix);
	      }
#endif
	      *ch1 = '\0';
	  }
	  resource = resource->next;
      }

      if (category->hash != NULL) {
	  for (bucket = 0; bucket < HASHSIZE; bucket++) {
	      for (resource = category->hash[bucket]; resource != NULL;
		   resource = resource->next) {

		  if (resource->noPrefix) continue;

		  if (prefix == NULL) {
		      prefix = ckmalloc(strlen(resource->file) + 1,
					"Failed to allocate directory prefix");
		      strcpy(prefix, resource->file);
#if DEBUG
		      printf("New directory prefix: %s\n", prefix);
#endif
		  } else {
		      ch1 = prefix;
		      ch2 = resource->file;
		      while (*ch1 != '\0' && *ch2 != '\0' && *ch1 == *ch2) {
			  ch1++; ch2++;
		      }
#if DEBUG
		      if (*ch1 != '\0') {
			  *ch1 = '\0';
			  printf("New directory prefix: %s\n", prefix);
		      }
#endif
		      *ch1 = '\0';
		  }
	      }
	  }
      }
      category = category->next;
  }
  if (prefix != NULL) {
      ch1 = ch2 = prefix;
      while (*ch1 != '\0') {
	  if (*ch1 == '/') ch2 = ch1;
	  ch1++;
      }
      *ch2 = '\0';
  }

  /* Prefixes must be absolute path names */
  if (prefix != NULL && prefix[0] != '/') prefix[0] = '\0';
  return prefix;
}


static void OutputChar(file, c)
    FILE *file;
    char c;
{
    static int len;	/* Rely upon being 0 initially */

    if (c == '\n') len = 0;
    else {
	len++;
	if (len == MAXLINELEN) {
	    fputs("\\\n", file);
	    len = 1;;
	}
    }
	
    putc(c, file);
}

static void OutputString(file, s)
    FILE *file;
    char *s;
{
    while (*s != '\0') OutputChar(file, *s++);
}


static void PrintResourceDirectory (directoryName, file)
  char *directoryName;
  FILE *file;
{
  Category *category;
  Resource *resource;
  char *pname;
  int prefixlen;
  int bucket;
#define outs(s) OutputString(file, s)
#define outc(c) OutputChar(file, c)

  if (directoryName == NULL || *directoryName == '\0') prefixlen = 0;
  else prefixlen = strlen(directoryName) + 1;

  if (makeExclusive) outs("PS-Resources-Exclusive-1.0\n");
  else outs("PS-Resources-1.0\n");

  category = categories;

  while (category != NULL) {
    outs(category->name);
    outc('\n');
    category = category->next;
  }

  outs(".\n");
  if (prefixlen > 1) {
      outc('/');
      outs(directoryName);
      outc('\n');
  }

  category = categories;

  while (category != NULL) {
    resource = category->list;
    outs(category->name);
    outc('\n');
    if (strcmp(category->name, "FontBDFSizes") != 0 &&
	strcmp(category->name, "FontFamily") != 0) {
	while (resource != NULL) {
	    outs(resource->name);
	    outc('=');
	    if (resource->noPrefix) {
		outc('=');
		outs(resource->file);
	    } else outs(resource->file+prefixlen);
	    outc('\n');
	    resource = resource->next;
	}
    } else {
	while (resource != NULL) {
	    outs(resource->name);
	    outc('=');
	    outs(resource->file);
	    pname = resource->name;
	    resource = resource->next;
	    while (resource != NULL && strcmp(resource->name, pname) == 0) {
		outc(',');
		outs(resource->file);
		resource = resource->next;
	    }
	    outc('\n');
	}
    }
    if (category->hash != NULL) {
	for (bucket = 0; bucket < HASHSIZE; bucket++) {
	    for (resource = category->hash[bucket]; resource != NULL;
		 resource = resource->next) {
		outs(resource->name);
		outc('=');
		if (resource->noPrefix) {
		    outc('=');
		    outs(resource->file);
		} else outs(resource->file+prefixlen);
		outc('\n');
	    }
	}
    }
    outs(".\n");
    category = category->next;
  }
#undef outs
#undef outc
}


void Usage()
{
    fprintf(stderr,
	   "Usage: %s [-o outputfile] [-f inputfile]... [-dir directory]...\n",
	    program);
    fprintf(stderr,
	   "        [-e] [-i] [-nr] [-s] [-p] [-d] [-k] [-q] directory...\n");
    exit(1);
}

int stdinDirectories = false;

void ReadStdinDirectories()
{
    char buf[256];

    if (stdinDirectories) {
	fprintf(stderr, "%s:  Can only read stdin as directory list once.\n",
		program);
	Usage();
    }

    stdinDirectories = true;

    while (scanf("%255s", buf) == 1) {
	directoryCount++;
	directories = (char **) ckrealloc((char *) directories,
				  directoryCount * sizeof(char *),
				  "Failed to reallocate directory list.");
	directories[directoryCount-1] = ckmalloc(strlen(buf)+1,
				  "Failed to allocate directory name.");
	strcpy(directories[directoryCount-1], buf);
    }
}

void ProcessArglist(argc, argv)
    int argc;
    char *argv[];
{
    int i = 1, j;

    if (argc > 0) {
	program = strrchr(argv[0], '/');
	if (program != NULL) program++;
	else program = argv[0];
    } else program = "makepsres";

    directories = (char **) ckmalloc(argc * sizeof(char *),
				     "Failed to allocate directory list.");
    while (i < argc) {
	if (strcmp(argv[i], "-help") == 0) Usage();

	else if (strcmp(argv[i], "-q") == 0) issueWarnings = false;

	else if (strcmp(argv[i], "-k") == 0) keep = true;

	else if (strcmp(argv[i], "-d") == 0) discard = true;

	else if (strcmp(argv[i], "-p") == 0) noPrefix = true;

	else if (strcmp(argv[i], "-s") == 0) strict = true;

	else if (strcmp(argv[i], "-nr") == 0) recursive = false;

	else if (strcmp(argv[i], "-nb") == 0) noBackup = true;

	else if (strcmp(argv[i], "-ns") == 0) noSuffix = true;

	else if (strcmp(argv[i], "-i") == 0) interactive = true;

	else if (strcmp(argv[i], "-e") == 0) makeExclusive = true;

	else if (strcmp(argv[i], "-f") == 0) {
	    i++;
	    if (i >= argc) Usage();
	    if (inputFiles == NULL) {
		inputFiles = (char **) ckmalloc(argc * sizeof(char *),
					"Failed to allocat input file list.");
	    }
	    inputFiles[inputCount++] = argv[i];
	}

	else if (strcmp(argv[i], "-o") == 0) {
	    i++;
	    if (i >= argc) Usage();
	    outputFilename = argv[i];
	}

	else if (strcmp(argv[i], "-dir") == 0) {
	    i++;
	    if (i >= argc) Usage();
	    directories[directoryCount++] = argv[i];
	}

	else directories[directoryCount++] = argv[i];

	i++;
    }

    if (directoryCount == 0) {
	directoryCount = 1;
	directories[0] = ".";
    } else {
	for (i = 0; i < directoryCount; i++) {
	    if (strcmp(directories[i], "-") == 0) ReadStdinDirectories();
	}
    }

    if (stdinDirectories) {
	j = 0;
	for (i = 0; i < directoryCount; i++) {
	    if (strcmp(directories[i], "-") != 0) {
		directories[j] = directories[i];
		j++;
	    }
	}
	directoryCount--;
    }

    for (i = 0; i < inputCount; i++) {
	if (strcmp(inputFiles[i], "-") == 0 && stdinDirectories) {
	    fprintf(stderr,
	    "%s:  Cannot read stdin as both directory list and input file\n",
		    program);
	    Usage();
	}
    }

#if DEBUG
    printf("Input directory list:\n");
    for (i = 0; i < directoryCount; i++) printf("  %s\n", directories[i]);
#endif

    directoryLen = (int *) ckmalloc(directoryCount * sizeof(int),
			    "Failed to allocate directory length list.");

    for (i = 0; i < directoryCount; i++) {
	directoryLen[i] = strlen(directories[i]);
    }
}


void CheckBackup(filename)
    char *filename;
{
    char *backupname;

    if (noBackup || strcmp(filename, "/dev/null") == 0) return;

    backupname = ckmalloc(strlen(filename)+2,
			  "Failed to allocate backup file name.'");
    strcpy(backupname, filename);
    strcat(backupname, "~");

    /* Effect "rename (filename, backupname)" in BSD/Sys-V independent way */

    (void) unlink (backupname); /* Ignore error */

#ifndef __UNIXOS2__
    if (link (filename, backupname) != 0) {
	if (errno != ENOENT) {
	    fprintf(stderr, "%s:  Could not back up output file %s\n",
		    program, filename);
	    fprintf(stderr, "            and will not write over it (%s)\n",
		    strerror(errno));
	    exit(1);
	}
    } else (void) unlink(filename);
#else
    if (rename(filename,backupname) != 0) {
	fprintf(stderr, "%s:  Could not back up output file %s\n",
		program, filename);
	fprintf(stderr, "            and will not write over it (%s)\n",
		strerror(errno));
	exit(1);
    }
#endif
    free(backupname);
}


void IssueDuplicateWarnings()
{
    int headerIssued = false;
    Category *category;
    Duplicate *dup;

    for (category = categories; category != NULL; category = category->next) {
	if (category->duplicates == NULL) continue;
	if (!headerIssued) {
	    headerIssued = true;
	    fprintf(stderr, 
		    "%s:  Warning:  The output resource file contains the following\n",
		    program);
	    fprintf(stderr,
		    "            duplicates.  Edit them out if you wish.\n");
	}
	fprintf(stderr, "Category %s:\n", category->name);
	for (dup = category->duplicates; dup != NULL; dup = dup->next) {
	    fprintf(stderr, "  Resource:  %s\n", dup->name);
	    fprintf(stderr, "    First file:  %s\n", dup->file1);
	    fprintf(stderr, "    Second file:  %s\n", dup->file2);
	}
    }
}

int
main (argc, argv)
  int argc;
  char *argv[];
{
  FILE   *outputFile;
  int     i, len;
  char *prefix;

  directories = NULL;
  directoryCount = 0;
  recursive = true;
  discard = false;
  keep = false;
  outputFilename = NULL;
  inputFiles = NULL;
  inputCount = 0;
  makeExclusive = false;
  interactive = false;
  strict = false;
  noPrefix = false;
  issueWarnings = true;
  noBackup = false;
  noSuffix = false;

  ProcessArglist(argc, argv);

  for (i = 0; i < inputCount; i++) {
      PreprocessResourceDirectory(inputFiles[i]);
  }

  /* Make two passes; the first time we just look for .upr files, and the
     second time we look for everything.  This gives us a better chance at
     identifying files. */

  for (i = 0; i < directoryCount; i++) {
      len = strlen (directories[i]);
      if (directories[i][len - 1] == '/') directories[i][len - 1] = '\0';

      ProcessDirectory (directories[i], true, ProcessUPRFile);
  }

#if DEBUG
  {
      int i, biggestBucket, emptyCount;
      static int count[11];
      emptyCount = biggestBucket = 0;
      for (i = 0; i < HASHSIZE; i++) {
	  if (bucketCount[i] == 0) emptyCount++;
	  if (bucketCount[i] > biggestBucket) biggestBucket = bucketCount[i];
      }
      if (biggestBucket != 0) {
	  for (i = 0; i < HASHSIZE; i++) {
	      count[bucketCount[i] * 10 / biggestBucket]++;
	  }
      }
      printf("Total UPR entries: %d\n", totalHashed);
      printf("Buckets: %d\n", HASHSIZE);
      printf("Longest bucket: %d\n", biggestBucket);
      printf("Number of empty buckets: %d\n", emptyCount);
      if (biggestBucket != 0) {
	  for (i = 0; i < 10; i++) {
	      printf("Number of buckets <= %d entries: %d\n",
		     biggestBucket*(i+1) / 10, count[i]);
	  }
      }
  }
#endif

  for (i = 0; i < directoryCount; i++) {
      len = strlen (directories[i]);
      if (directories[i][len - 1] == '/') directories[i][len - 1] = '\0';

      ProcessDirectory (directories[i], true, ProcessFile);
  }

  if (outputFilename == NULL) outputFilename = "PSres.upr";

  if (strcmp(outputFilename, "-") == 0) outputFile = stdout;
  else {
      CheckBackup(outputFilename);
      outputFile = fopen (outputFilename, "w");
  }

  if (outputFile == NULL) {
      fprintf (stderr,
	       "%s: Failed to open %s for writing: %s\n",
	       program, outputFilename, strerror(errno));
      exit (1);
  }

  if (!discard) GenerateEntriesFromUPRList();

  prefix = ExtractDirectoryPrefix();
  PrintResourceDirectory (prefix, outputFile);
  IssueDuplicateWarnings();
  exit (0);
}
