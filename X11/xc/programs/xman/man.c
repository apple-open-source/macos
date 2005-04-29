/* $XConsortium: man.c,v 1.30 94/04/17 20:43:56 rws Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xman/man.c,v 1.8 2003/04/09 20:31:31 herrb Exp $ */


#include "globals.h"
#include "vendor.h"		/* vendor-specific defines and data */

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

#ifdef DEBUG
static char error_buf[BUFSIZ];		/* The buffer for error messages. */
#endif /* DEBUG */

static void AddToCurrentSection(Manual * local_manual, char * path);
static void InitManual(Manual * l_manual, char * label);
static void ReadCurrentSection(Manual * local_manual, char * path);
static void ReadMandescFile(SectionList ** section_list, char * path);
static void SortAndRemove(Manual *man, int number);
static void SortList(SectionList ** list);

#define SECT_ERROR -1

#ifndef       Byte
#define       Byte    unsigned char
#endif
 
#ifndef       reg
#define       reg     register
#endif
 
static void sortstrs (Byte *data[], int size, Byte *otherdata[]);
static void sortstrs_block (Byte **, Byte **, int, Byte, Byte **, Byte **);
static void sortstrs_block_oo (Byte **, Byte **, int, Byte, int *, int *, Byte **, Byte **);
 
/*	Function Name: Man
 *	Description: Builds a list of all manual directories and files.
 *	Arguments: none. 
 *	Returns: the number of manual sections.
 */

int
Man(void)
{
  SectionList *list = NULL;
  char *ptr, *lang = 0, manpath[BUFSIZ], buf[BUFSIZ], *path, *current_label;
  int sect, num_alloced;

/* 
 * Get the environment variable MANPATH, and if it doesn't exist then use
 * SYSMANPATH and LOCALMANPATH.
 */

  /* if MANPATH variable ends in ':'. So, should extend it's value to the
   * default search path.
   */

  *manpath = '\0';
  if ((ptr = getenv("MANPATH")) != NULL)
    strcpy(manpath, ptr);
  if (ptr == NULL || streq(ptr , "") || ptr[strlen(ptr) - 1] == ':') {
    lang = getenv("LANG");
#ifdef MANCONF
    if (!ReadManConfig(manpath + strlen(manpath)))
#endif
    {
#ifdef MANCONF
      if (manpath[strlen(manpath) - 1] != ':')
	strcat(manpath, ":");
#endif
      strcat(manpath, SYSMANPATH);
#ifdef LOCALMANPATH
      strcat(manpath, ":");
      strcat(manpath, LOCALMANPATH);
#endif
    }
  }

/*
 * Get the list of manual directories in the users MANPATH that we should
 * open to look for manual pages.  The ``mandesc'' file is read here.
 */

  for ( path = manpath ; (ptr = index(path , ':')) != NULL ; path = ++ptr) { 
    *ptr = '\0';
    if (lang != 0) {
      strcpy(buf, path);
      strcat(buf, "/");
      strncat(buf, lang, sizeof(buf) - strlen(path) + 1);
      buf[sizeof(buf) - strlen(path) + 1] = '\0';
      ReadMandescFile(&list, buf);
    }
    ReadMandescFile(&list, path);
  }
  if (lang != 0) {
    strcpy(buf, path);
    strcat(buf, "/");
    strncat(buf, lang, sizeof(buf) - strlen(path) + 1);
    buf[sizeof(buf) - strlen(path) + 1] = '\0';
    ReadMandescFile(&list, buf);
  }
  ReadMandescFile(&list, path);

  SortList(&list);
  
  sect = 0;
  num_alloced = SECTALLOC;
  manual = (Manual *) XtMalloc( sizeof(Manual) * num_alloced );
  InitManual( manual, list->label );
  manual[sect].flags = list->flags;
  current_label = NULL;

  while ( list != NULL ) {
    SectionList * old_list;

    if ( current_label == NULL || streq(list->label, current_label) )
      AddToCurrentSection( manual + sect, list->directory);
    else {
      if (manual[sect].nentries == 0) {	/* empty section, re-use it. */
	XtFree(manual[sect].blabel);
	manual[sect].blabel = list->label;
	manual[sect].flags = list->flags;
      }
      else {
	if ( ++sect >= num_alloced ) {
	  num_alloced += SECTALLOC;
	  manual = (Manual *) XtRealloc ( (char *) manual,
				        (sizeof(Manual) * num_alloced));
	  if (manual == NULL) 
	    PrintError("Could not allocate memory for manual sections.");
	}
	InitManual( manual + sect, list->label );
	manual[sect].flags = list->flags;
      }
      AddToCurrentSection( manual + sect, list->directory);
    }
    /* Save label to see if it matches next entry. */
    current_label = list->label; 
    old_list = list;
    list = list->next;
    XtFree((char *) old_list);		/* free what you allocate. */
  }
  if (manual[sect].nentries != 0)
    sect++;			/* don't forget that last section. */
  
  SortAndRemove(manual, sect);

#ifdef notdef			/* dump info. */
  DumpManual(sect);
#endif
  
/*
 * realloc manual to be minimum space necessary.
 */

  if (sect == 0)
    PrintError("No manual pages found.");
  manual = (Manual *) XtRealloc( (char *) manual, (sizeof(Manual) * sect));
  if (manual == NULL) 
    PrintError("Could not allocate memory for manual sections.");

  return(sect);		/* return the number of man sections. */
}    

/*	Function Name: SortList
 *	Description: Sorts the list of sections to search.
 *	Arguments: list - a pointer to the list to sort.
 *	Returns: a sorted list.
 *
 * This is the most complicated part of the entire operation.
 * all sections with the same label must by right next to each other,
 * but the sections that are in the standard list have to come first.
 */

static void
SortList(SectionList ** list)
{
  SectionList * local;
  SectionList *head, *last, *inner, *old;
  
  if (*list == NULL)
    PrintError("No manual sections to read, exiting.");

/* 
 * First step 
 * 
 * Look for standard list items, and more them to the top of the list.
 */

  last = NULL;			/* keep Saber happy. */
  for ( local = *list ; local->next != NULL ; local = local->next) {
    if ( local->flags ) {
      if ( local == *list )	/* top element is already standard. */
	break;
      head = local;

      /* Find end of standard block */
      for (old = 0 ; (local->next != NULL) && (local->flags) 
	   ; old = local, local = local->next); 

      if (old != 0) {
          last->next = old->next; /* Move the block. */
          old->next = *list;
          *list = head;
      }

      break;			/* First step accomplished. */
    }
    last = local;
  }

/*
 *  Second step
 *
 *  Move items with duplicate labels right next to each other.
 *
 *  Changed to keep the order of the list entries unchanged.
 */

  for (local = *list; local->next != NULL; local = local->next) {
    head = local;
    old = inner = local->next;
    while (inner != NULL) {
      if (streq(inner->label, local->label)) {
	if (old != inner) {
	  old->next = inner->next;
	  last = inner->next;
	  inner->next = head->next;
	  head->next = inner;
	  head = inner;
	  old = inner = last;
	  continue;
	}
	else
	  head = inner;
      }
      old = inner;
      inner = inner->next;
    }
  }
}	

/*	Function Name: ReadMandescFile
 *	Description: Reads the mandesc file, and adds more sections as 
 *                   nescessary.
 *	Arguments: path - path name if the current search directory.
 *                 section_list - pointer to the list of sections.
 *	Returns: TRUE in we should use default sections
 */
  
static void
ReadMandescFile(SectionList ** section_list, char * path)
{
  char mandesc_file[BUFSIZ];	/* full path to the mandesc file. */
  FILE * descfile;
  char string[BUFSIZ], local_file[BUFSIZ];
  Boolean use_defaults = TRUE;
  char *cp;

  sprintf(mandesc_file, "%s/%s", path, MANDESC);
  if ( (descfile = fopen(mandesc_file, "r")) != NULL) {
    while ( fgets(string, BUFSIZ, descfile) != NULL) {
      string[strlen(string)-1] = '\0';        /* Strip off the CR. */

      if ( streq(string, NO_SECTION_DEFAULTS) ) {
	use_defaults = FALSE;
	continue;
      }

      if ((cp = index(string,'\t')) != NULL) {
	char *s;
	*cp++ = '\0';
	strcpy(local_file, MAN);
	strcat(local_file, string);
	if ((s = index(cp,'\t')) != NULL) {
	  *s++ = '\0';
	  if (streq(s, SUFFIX))
	    AddNewSection(section_list, path, local_file, cp, MSUFFIX);
	  else if (streq(s, FOLD))
	    AddNewSection(section_list, path, local_file, cp, MFOLD);
	  else if (streq(s, FOLDSUFFIX))
	    AddNewSection(section_list, path, local_file, cp, MFOLDSUFFIX);
	  else
	    AddNewSection(section_list, path, local_file, cp, MNULL);
        } else
	    AddNewSection(section_list, path, local_file, cp, MNULL);
      } else {
	sprintf(local_file, "%s%c", MAN, string[0]);
	AddNewSection(section_list, path, local_file, (string + 1), FALSE );
#ifdef SEARCHOTHER
	sprintf(local_file, "%s%c", SEARCHOTHER, string[0]);
	AddNewSection(section_list, path, local_file, (string + 1), FALSE);
#endif
      }
    }

    fclose(descfile);
  }
  if (use_defaults)
    AddStandardSections(section_list, path);
}

/*	Function Name: AddNewSection
 *	Description: Adds the new section onto the current section list.
 *	Arguments: list - pointer to the section list.
 *                 path - the path to the current manual section.
 *                 file - the file to save.
 *                 label - the current section label.
 *                 flags = 1 - add a suffix
 *			 = 2 - fold to lower case
 *	Returns: none.
 */

void
AddNewSection(
SectionList **list,
char * path, char * file, char * label, 
int flags)
{
  SectionList * local_list, * end;
  char full_path[BUFSIZ];

/* Allocate a new list element */

  local_list = (SectionList *) XtMalloc(sizeof(SectionList));

  if (*list != NULL) {
    for ( end = *list ; end->next != NULL ; end = end->next );
    end->next = local_list;
  }
  else 
    *list = local_list;

  local_list->next = NULL;
  local_list->label = StrAlloc(label);
  sprintf(full_path, "%s/%s", path, file);
  local_list->directory = StrAlloc(full_path);
  local_list->flags = flags;
}  

/*	Function Name: AddToCurrentSection
 *	Description: This function gets the names of the manual page
 *                   directories, then closes the directory.
 *	Arguments:  local_manual - a pointer to a manual pages structure.
 *                  path - the path to this directory.
 *	Returns: none.
 */

static void
AddToCurrentSection(Manual * local_manual, char * path)
{
  char temp_path[BUFSIZ];

#if defined(__OpenBSD__) || defined(__NetBSD__)
  sprintf(temp_path, "%s/%s", path, MACHINE);
  ReadCurrentSection(local_manual, temp_path);
#endif
  ReadCurrentSection(local_manual, path);
  sprintf(temp_path, "%s.%s", path, COMPRESSION_EXTENSION);
  ReadCurrentSection(local_manual, temp_path);
}

/*	Function Name: ReadCurrentSection
 *	Description: Actually does the work of adding entries to the 
 *                   new section
 *	Arguments:  local_manual - a pointer to a manual pages structure.
 *                  path - the path to this directory.
 *                  compressed - Is this a compressed directory?
 *	Returns: TRUE if any entries are found.
 */

static void
ReadCurrentSection(Manual * local_manual, char * path)
{
  DIR * dir;

  register struct dirent *dp;

  register int nentries;
  register int nalloc;
  char full_name[BUFSIZ], *ptr;

  if((dir = opendir(path)) == NULL) {
#ifdef DEBUG
    sprintf(error_buf,"Can't open directory %s", path);
    PopupWarning(NULL, error_buf);
#endif /* DEBUG */
    return;
  }

/*
 * Remove the compression extension from the path name.
 */

  if ( (ptr = rindex(path, '.')) != NULL) {
#if !defined(SCO) && !defined(ISC)
    if (streq(ptr + 1, COMPRESSION_EXTENSION)) 
#else
    if (strpbrk(ptr + 1, COMPRESSION_EXTENSIONS) != NULL)
#endif
      *ptr = '\0';
#ifdef GZIP_EXTENSION
    else if (streq(ptr + 1, GZIP_EXTENSION))
      *ptr = '\0';
#endif
  }
  
  nentries = local_manual->nentries;
  nalloc = local_manual->nalloc;

  while( (dp = readdir(dir)) != NULL ) {
    char * name = dp->d_name;
    if (name[0] == '.')
      continue;
#ifndef CRAY
    if (index(name, '.') == NULL)
      continue;
#endif
    if( nentries >= nalloc ) {
      nalloc += ENTRYALLOC;
      local_manual->entries =(char **) XtRealloc((char *)local_manual->entries,
						 nalloc * sizeof(char *));
      local_manual->entries_less_paths =
	(char **) XtRealloc((char *)local_manual->entries_less_paths,
			    nalloc * sizeof(char *));
    }

    sprintf(full_name, "%s/%s", path, name);
/*
 * Remove the compression extension from the entry name.
 */

    if ( (ptr = rindex(full_name, '.')) != NULL) {
#if !defined(SCO) && !defined(ISC)
      if (streq(ptr + 1, COMPRESSION_EXTENSION)) 
#else
      if (strpbrk(ptr + 1, COMPRESSION_EXTENSIONS) != NULL)
#endif
	*ptr = '\0';
#ifdef GZIP_EXTENSION
      else if (streq(ptr + 1, GZIP_EXTENSION))
	*ptr = '\0';
#endif
    }
    local_manual->entries[nentries] = StrAlloc(full_name);
    local_manual->entries_less_paths[nentries] = 
      rindex(local_manual->entries[nentries], '/');
    if ( local_manual->entries_less_paths[nentries] == NULL )
      PrintError("Internal error while cataloging manual pages.");
    ++ nentries;
  }
  
  local_manual->nentries = nentries;
  local_manual->nalloc = nalloc;

  closedir(dir);
}

/*	Function Name: SortAndRemove
 *	Description: This function sorts all the entry names and
 *                   then removes all the duplicate entries.
 *	Arguments: man - a pointer to the manual structure.
 *                 number - the number of manual sections.
 *	Returns: an improved manual stucure
 */

static void
SortAndRemove(Manual *man, int number)
{
  int i;
  char *l1, *l2, **s1;
  
  for ( i = 0; i < number; man++, i++) { /* sort each section */
    register int i2 = 0;      
    
#ifdef DEBUG
    printf("sorting section %d - %s\n", i, man->blabel);
#endif /* DEBUG */

    s1 = (char **)malloc(man->nentries * sizeof(char *));
    
    /* temporarily remove suffixes of entries, preventing them from */
    /* being used in alpabetic comparison ie sccs-delta.1 vs sccs.1 */
    for (i2=0; i2<man->nentries; i2++)
      if ((s1[i2] = rindex(man->entries_less_paths[i2], '.')) != NULL)
	*s1[i2] = '\0';  

    sortstrs ( (Byte **)man->entries_less_paths, man->nentries, (Byte **)man->entries );

    /* put back suffixes */
    for (i2=0; i2<man->nentries; i2++) 
      if (s1[i2] != NULL) *s1[i2] = '.';      

    free(s1); 
    
#ifdef DEBUG
    printf("removing from section %d.\n", i);
#endif /* DEBUG */
    
    {
      register int   j, k, nent, nentm1;
      int     j2;
      nent   = man -> nentries;
      nentm1 = nent - 1;
      j = 0;
      l2 = man->entries_less_paths[j++];
      if ( l2 == NULL )
        PrintError("Internal error while removing duplicate manual pages.");
      while ( j < nentm1 )
	{
	  l1 = l2;
	  l2 = man->entries_less_paths[j++];
	  if ( l2 == NULL )
	    PrintError("Internal error while removing duplicate manual pages."
		       );
	  if ( streq(l1,l2) )
	    {
	      j2 = j-1;
	      k  = j2;
	      while ( j < nent )
                {
		  man -> entries_less_paths[k] = man -> entries_less_paths[j];
                man -> entries[k++] = man -> entries[j++];
                }
	      j = j2;
	      -- man -> nentries;
	      -- nent;
	      -- nentm1;
	    }
	}
    }
  }
}

 /*
       *******  Replacement for qsort to keep
       *******  identical entries in order
 
       A somewhat ugly hack of something that was once simpler...
 */
 /*
       Sort an array of pointers to strings, keeping it
       in ascending order by (1) string comparison and
       (2) original entry order in the pointer array.
 
       This is a modified radix exchange algorithm.
 
       In case there's insufficient memory for a temporary copy
       of the pointer array, the original order of identical strings
       isn't preserved.
 */
 
static void 
sortstrs (Byte *data[], int size, Byte *otherdata[])
{
       Byte   **sp, **ep;
       Byte   **othersp, **otherep;
       int     *origorder;
 
 origorder = (int *) calloc (size, sizeof(int));
 if ( origorder )
    {
    reg int     i;
 
    for ( i=0; i < size; ++i )
       origorder[i] = i;
    }
 
 sp = data;
 ep = &data[size-1];
 othersp = otherdata;
 otherep = &otherdata[size-1];
 if ( origorder )
    {
    sortstrs_block_oo ( sp, ep, 0, 0x80, origorder, &origorder[size-1],
       othersp, otherep );
    free (origorder);
    }
 else
    sortstrs_block ( sp, ep, 0, 0x80, othersp, otherep );
}
 

 
 /*---------------------------------*/
 /*  Sort 1 block of data on 1 bit  */
 /*---------------------------------*/
 
static void
sortstrs_block (  
       Byte   **start,
       Byte   **end,
       int      offset,
       Byte     mask,
       Byte   **otherstart,
       Byte   **otherend)
 
{
 reg   Byte   **sp, **ep;
 reg   Byte     m;
 reg   int      off;
 reg   Byte    *t;
 reg   int      curstrlen;
       int      maxstrlen;
       Byte   **othersp, **otherep;
 
 
#define       newstring(ptr) \
 { \
 t = *ptr; \
 curstrlen = 0; \
 while ( *t++ ) ++ curstrlen; \
 if ( curstrlen > maxstrlen ) maxstrlen = curstrlen; \
 t = *ptr; \
 }
 
 
 maxstrlen = 0;
 sp  = start;
 ep  = end;
 off = offset;
 m   = mask;
 othersp = otherstart;
 otherep = otherend;
 
 while (1)
     {
     newstring(sp)
     while (((sp != ep) && ((curstrlen < off) || ((t[off] & m) == 0))))
       {
       ++ sp;
       ++ othersp;
       newstring(sp)
       }
     if ( sp == ep )
       break;
 
     newstring(ep);
     while (((sp != ep) && (curstrlen >= off) && ((t[off] & m) != 0)))
       {
       -- ep;
       -- otherep;
       newstring(ep)
       }
     if ( sp == ep )
       break;
 
     t = *sp;
     *sp = *ep;
     *ep = t;
 
     t      = *othersp;
     *othersp = *otherep;
     *otherep = t;
     }
 
 t = *sp;
 if ((curstrlen < off) || ((t[off] & m) == 0))
    {
    if ( ep != end )
       {
       ++ ep;
       ++ otherep;
       }
    }
 else
    {
    if ( sp != start )
       {
       -- sp;
       -- othersp;
       }
    }
 
 m >>= 1;
 if ( m == 0 )
    {
    m = 0x80;
    if ( ++off >= maxstrlen )
       return;
    }
 
 
 if ( sp != start )
    sortstrs_block ( start, sp, off, m, otherstart, othersp );
 if ( ep != end )
    sortstrs_block ( ep, end, off, m, otherep, otherend );
}
 

 
 /*-----------------------------------------------------------------*/
 /*  Sort 1 block of data on 1 bit; check for out-of-order entries  */
 /*-----------------------------------------------------------------*/
 
static void
 sortstrs_block_oo (
       Byte   **start,
       Byte   **end,
       int      offset,
       Byte     mask,
       int     *ostart,
       int     *oend,
       Byte   **otherstart,
       Byte   **otherend)
 
{
 reg   Byte   **sp, **ep;
 reg   int     *osp, *oep;
 reg   Byte     m;
 reg   int      off;
 reg   Byte    *t;
 reg   int      u;
 reg   int      curstrlen;
       int      maxstrlen;
       Byte   **othersp, **otherep;
 
 
#define       newstring(ptr) \
 { \
 t = *ptr; \
 curstrlen = 0; \
 while ( *t++ ) ++ curstrlen; \
 if ( curstrlen > maxstrlen ) maxstrlen = curstrlen; \
 t = *ptr; \
 }
 
 
 maxstrlen = 0;
 sp  = start;
 ep  = end;
 osp = ostart;
 oep = oend;
 off = offset;
 m   = mask;
 othersp = otherstart;
 otherep = otherend;
 
 while (1)
     {
     newstring(sp)
     while (((sp != ep) && ((curstrlen < off) || ((t[off] & m) == 0))))
       {
       ++ sp;
       ++ osp;
       ++ othersp;
       newstring(sp)
       }
     if ( sp == ep )
       break;
 
     newstring(ep);
     while (((sp != ep) && (curstrlen >= off) && ((t[off] & m) != 0)))
       {
       -- ep;
       -- oep;
       -- otherep;
       newstring(ep)
       }
     if ( sp == ep )
       break;
 
     t   = *sp;
     *sp = *ep;
     *ep = t;
 
     t      = *othersp;
     *othersp = *otherep;
     *otherep = t;
 
     u    = *osp;
     *osp = *oep;
     *oep = u;
     }
 
 t = *sp;
 if ((curstrlen < off) || ((t[off] & m) == 0))
    {
    if ( ep != end )
       {
       ++ ep;
       ++ oep;
       ++ otherep;
       }
    }
 else
    {
    if ( sp != start )
       {
       -- sp;
       -- osp;
       -- othersp;
       }
    }
 
 m >>= 1;
 if ( m == 0 )
    {
    m = 0x80;
    if ( ++off >= maxstrlen )  /*  Finished sorting block of strings:    */
       {                               /*  Restore duplicates to
riginal order  */
       reg Byte **cp;
       reg int *ocp;
         Byte **othercp;
 
 
       if ( sp != start )
        {
        cp  = start;
        ocp = ostart;
        othercp = otherstart;
        while ( cp != sp )
           {
           if ( *ocp > *(ocp+1) )
               {
               t       = *(cp+1);
               *(cp+1) = *cp;
               *cp     = t;
 
               t               = *(othercp+1);
               *(othercp+1)    = *othercp;
               *othercp        = t;
 
               u        = *(ocp+1);
               *(ocp+1) = *ocp;
               *ocp     = u;
 
               if ( cp != start )
                  {
                  -- cp;
                  -- ocp;
                  -- othercp;
                  continue;
                  }
               }
           ++ cp;
           ++ ocp;
           ++ othercp;
           }
        }
       if ( ep != end )
        {
        cp  = ep;
        ocp = oep;
        othercp = otherep;
        while ( cp != end )
           {
           if ( *ocp > *(ocp+1) )
               {
               t       = *(cp+1);
               *(cp+1) = *cp;
               *cp     = t;
 
               t               = *(othercp+1);
               *(othercp+1)    = *othercp;
               *othercp        = t;
 
               u        = *(ocp+1);
               *(ocp+1) = *ocp;
               *ocp     = u;
 
               if ( cp != ep )
                  {
                  -- cp;
                  -- ocp;
                  -- othercp;
                  continue;
                  }
               }
           ++ cp;
           ++ ocp;
           ++ othercp;
           }
        }
       return;
       }
    }
 
 
 if ( sp != start )
    sortstrs_block_oo ( start, sp, off, m, ostart, osp, otherstart, othersp );
 if ( ep != end )
    sortstrs_block_oo ( ep, end, off, m, oep, oend, otherep, otherend );
}


/*	Function Name: InitManual
 *	Description: Initializes this manual section.
 *	Arguments: l_manual - local copy of the manual structure.
 *                 label - the button label for this section.
 *	Returns: none.
 */

static void
InitManual(Manual * l_manual, char * label)
{
  bzero( l_manual, sizeof(Manual) );	        /* clear it. */
  l_manual->blabel = label;	                /* set label. */
}
  
#if defined(DEBUG)

/*	Function Name: DumpManual
 *	Description: Debugging function that dumps the entire manual page
 *                   structure.
 *	Arguments: number - the number of sections.
 *	Returns: none.
 */

void
DumpManual(int number)
{
  register int i,j;
  
  for ( i = 0; i < number; i++) {
    printf("label: %s\n", manual[i].blabel);
    for (j = 0; j < manual[i].nentries; j++) 
      printf("%s\n", manual[i].entries[j]);
  }
}

#endif /* DEBUG */



#ifdef MANCONF

#if defined(MANCONFIGSTYLE_FreeBSD)

/*    Function Name: ReadManConfig
 *    Description: Reads man.conf file used by FreeBSD man
 *      Argument: manpath - char array to return path in.
 *    Returns: TRUE if read was successful.
 */

Bool
ReadManConfig(char manpath[])
{
  FILE        *fp;
  char        line[BUFSIZ];
  char        *path;
  Bool  firstpath = TRUE;

  if (!(fp = fopen(MANCONF, "r")))
    return(FALSE);

  while (fgets(line, sizeof(line), fp)) {
    path = strtok(line, " \t\n");
    if (!path || *path == '#')
      continue;
    if (strcmp(path, "MANPATH_MAP") == 0)
      path = strtok((char *)NULL, " \t\n");
    else if (strcmp(path, "MANDATORY_MANPATH") != 0 &&
	     strcmp(path, "OPTIONAL_MANPATH") != 0)
      return(FALSE);
    path = strtok((char *)NULL, " \t\n");
    if (!path || *path == '#')
      return FALSE;
    if (firstpath) {
      strcpy(manpath, path);
      firstpath = FALSE;
    }
    else if (!strstr(manpath,path)) {
      strcat(manpath, ":");
      strcat(manpath, path);
    }
  }
  fclose(fp);
  return(!firstpath);
}


#elif defined(MANCONFIGSTYLE_Linux) /* not FreeBSD */

/*    Function Name: ReadManConfig
 *    Description: Reads man.conf file used by Linux man
 *      Argument: manpath - char array to return path in.
 *    Returns: TRUE if read was successful.
 */


Bool
ReadManConfig(char manpath[])
{
  FILE        *fp;
  char        line[BUFSIZ];
  char        *path;
  Bool  firstpath = TRUE;

  if (!(fp = fopen(MANCONF, "r")))
    return(FALSE);

  while (fgets(line, sizeof(line), fp)) {
    path = strtok(line, " \t\n");
    if (!path || *path == '#' || (strcmp(path, "MANPATH") != 0))
      continue;
    path = strtok((char *)NULL, " \t\n");
    if (!path || *path == '#')
      return FALSE;
    if (firstpath) {
      strcpy(manpath, path);
      firstpath = FALSE;
    }
    else {
      strcat(manpath, ":");
      strcat(manpath, path);
    }
  }
  fclose(fp);
  return(!firstpath);
}

#elif defined(MANCONFIGSTYLE_OpenBSD) /* not FreeBSD or Linux */

/*    Function Name: ReadManConfig
 *    Description: Reads man.conf file used by Open/NetBSD
 *      Argument: manpath - char array to return path in.
 *    Returns: TRUE if read was successful.
 *
 *     This version expands the glob pattern that can be found 
 *     in man.conf
 */
#include <glob.h>

Bool
ReadManConfig(char manpath[])
{
    FILE        *fp;
    char        line[BUFSIZ];
    char        *path;
    Bool        firstpath = TRUE;
    glob_t      gs;
    int         i;
    
    if (!(fp = fopen(MANCONF, "r")))
	return(FALSE);
    
    while (fgets(line, sizeof(line), fp)) {
	path = strtok(line, " \t\n");
	if (!path || *path == '#')
	    continue;
	if (strcmp(path, "_default")) {
	    /* for now */
	    continue;
	}
	memset(&gs, 0, sizeof(glob_t));
	while ((path = strtok((char *)NULL, " \t\n"))) {
	    if (glob(path, GLOB_BRACE, NULL, &gs) < 0) {
		fclose(fp);
		return FALSE;
	    }
	} /* while */
	for (i = 0; i < gs.gl_pathc; i++) {
	    
	    if (firstpath) {
		strcpy(manpath, gs.gl_pathv[i]);
		firstpath = FALSE;
	    }
	    else {
		strcat(manpath, ":");
		strcat(manpath, gs.gl_pathv[i]);
	    }
	} /* for */
	globfree(&gs);
    }
    fclose(fp);
    return(!firstpath);
}

#elif defined(MANCONFIGSTYLE_BSD) /* not FreeBSD, Linux, or OpenBSD */

/*    Function Name: ReadManConfig
 *    Description: Reads man.conf file used by BSD 4.4
 *      Argument: manpath - char array to return path in.
 *    Returns: TRUE if read was successful.
 */

Bool
ReadManConfig(manpath)

char  manpath[];

{
  FILE        *fp;
  char        line[BUFSIZ];
  char        *path;
  Bool  firstpath = TRUE;

  if (!(fp = fopen(MANCONF, "r")))
    return(FALSE);

  while (fgets(line, sizeof(line), fp)) {
    path = strtok(line, " \t\n");
    if (!path || *path == '#' || strcmp(path, "_default"))
      continue;
    while ((path = strtok((char *)NULL, " \t\n"))) {
      if (firstpath) {
        strcpy(manpath, path);
        firstpath = FALSE;
      }
      else {
        strcat(manpath, ":");
        strcat(manpath, path);
      }
    }
  }
  fclose(fp);
  return(!firstpath);
}

#else /* not BSD */

#error "MANCONF defined (in vendor.h) for unknown operating system."

#endif /* MANCONFIGSTYLE == FreeBSD ... BSD */

#endif /* MANCONF */
