/* Mac OS X support for GDB, the GNU debugger.
   Copyright 2007.
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file implements the three-function API documented here:
   http://www.gnu.org/software/libc/manual/html_node/Backtraces.html#Backtraces
   on MacOS X.  */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stddef.h>
#include "macosx-self-backtrace.h"

#define MACHO_SLIDE_TYPE int32_t
#define NLIST_RECORD_SIZE 12

static int look_up_sigtramp_addr_range (void **sigtramp_start, void **sigtramp_end);
static int slower_better_dladdr (void *addr, Dl_info *dlinfo);
static void fill_in_stack_frame_string_buffer (void *addr, char *buf, int buflen);
static int grope_around_incore_macho_file (void *incoreptr_macho_header, MACHO_SLIDE_TYPE *slide, void **incoreptr_linkedit_strings, void **incoreptr_linkedit_local_nlists, int *number_of_local_nlists, void **incoreptr_linkedit_external_nlists, int *number_of_external_nlists);


#define INVALID_ADDRESS ((intptr_t)-1)

/* Hardcoding the allowable range of the stack is definitely a hack but
   I don't know a better way to discover what address range may be valid.
   And wandering off into an unallocated page will cause a crash..  */

#define SAVED_FP_FAILS_SANITY_CHECKS \
          (saved_fp < (void *) fp) || \
          (saved_fp == (void *) fp) || \
          (saved_fp > (void *) 0xc0000000) ||  /* stack starts here */ \
          (saved_fp < (void *) 0xb0000000)     /* ends somewhere around here*/

int 
gdb_self_backtrace (void **buffer, int bufsize)
{
  void **fp;
  void *saved_pc, *saved_fp;
  int count = 0;
  static int sigtramp_addr_range_looked_up = 0;
  static void *sigtramp_start;
  static void *sigtramp_end;

  if (sigtramp_addr_range_looked_up == 0)
    if (look_up_sigtramp_addr_range (&sigtramp_start, &sigtramp_end))
      sigtramp_addr_range_looked_up = 1;

  fp = (void **) __builtin_frame_address (0);
  saved_pc = (void *) __builtin_return_address (0);
  saved_fp = (void *) __builtin_frame_address (1);

  if (SAVED_FP_FAILS_SANITY_CHECKS)
    return 0;

  buffer[count++] = saved_pc;
  fp = saved_fp;
  while (count < bufsize && fp != 0)
    {

#if defined (__ppc__) || defined (__ppc64__)
      saved_fp = *fp;       // saved FP at frame-pointer + 0
      if (SAVED_FP_FAILS_SANITY_CHECKS)
        break;
      fp = saved_fp;
      if (fp == 0)
        break;
      saved_pc = *(fp + 2); // saved PC at saved FP + sizeof(void*) * 2
#endif

#if defined (__i386__)
      saved_fp = *fp++;     // saved FP at frame-pointer + 0
      saved_pc = *fp;       // saved PC at frame-pointer + sizeof(void*) * 1
      if (SAVED_FP_FAILS_SANITY_CHECKS)
        break;
      fp = saved_fp;
#endif

      buffer[count++] = saved_pc;
    }

  if (fp == 0 || saved_fp == 0)
    count--;

  return count;
}

/* Find the address range of the sigtramp() function so we can backtrace
   through asynchronous signal handlers.  */

static int
look_up_sigtramp_addr_range (void **sigtramp_start, void **sigtramp_end)
{
  Dl_info sigtramp_dlinfo;
  void *incoreptr_linkedit_strings;
  void *incoreptr_linkedit_local_nlists;
  void *incoreptr_linkedit_external_nlists;
  char *p, *endpoint;
  int number_of_local_nlists, number_of_external_nlists;
  MACHO_SLIDE_TYPE slide;

  if (sigtramp_start == NULL || sigtramp_end == NULL)
    return 0;
  *sigtramp_start = dlsym (RTLD_DEFAULT, "_sigtramp");
  if (*sigtramp_start == NULL)
    return 0;

  if (dladdr (*sigtramp_start, &sigtramp_dlinfo) == 0)
    return 0;

  if (grope_around_incore_macho_file (sigtramp_dlinfo.dli_fbase, 
                                      &slide,
                                      &incoreptr_linkedit_strings, 
                                      &incoreptr_linkedit_local_nlists, 
                                      &number_of_local_nlists,
                                      &incoreptr_linkedit_external_nlists, 
                                      &number_of_external_nlists) == 0)
    {
      return 0;
    }
         
  p = incoreptr_linkedit_external_nlists;
  endpoint = p + (number_of_external_nlists * NLIST_RECORD_SIZE);
  *sigtramp_end = *sigtramp_start + 8192;
  
  while (p < endpoint)
    {
      struct nlist *nlist = (struct nlist *) p;
      void *possibly_better_address;
      possibly_better_address = (void *) nlist->n_value + slide;
      if ((nlist->n_type & N_STAB) == 0
          && (nlist->n_type & N_TYPE) == N_SECT
          && nlist->n_sect != 0)
        {
          /* Is the address POSSIBLY_BETTER_ADDRESS a closer match than
             the best we've seen so far?  */

          if (possibly_better_address > sigtramp_dlinfo.dli_saddr
              && possibly_better_address < *sigtramp_end)
            {
              *sigtramp_end = possibly_better_address;
            }
        }
      p += NLIST_RECORD_SIZE;
    }

  if (*sigtramp_end == *sigtramp_start + 8192)
    return 0;

  return 1;
}


#define SYMBOL_MAXCHARS 128  /* a random guess, the max length of a symbol 
                                not as obvious as you'd think - ObjC symnames
                                can be quite long.  */

char **
gdb_self_backtrace_symbols (void **addrbuf, int num_of_addrs)
{
  int count = 0;
  char *buf;
  char **charstar_array;
  char *strings_ptr;

  int bufsize;
  int address_digits;

  address_digits = 8;
    
  /* malloc an initial buffer large enough to hold all the
     pointers-to-strings, followed by 512 bytes for the strings. */

  bufsize = (num_of_addrs * sizeof (char *)) + 512;
  buf = (char *) malloc (bufsize);
  if (buf == NULL)
    return NULL;

  /* the array of char *'s */
  charstar_array = (char **) buf;

  /* the strings for each stack frame */
  strings_ptr = &buf[num_of_addrs * sizeof (char *)];

  while (count < num_of_addrs)
    {
      /* "pathname (funcname+0xoffset) [0xaddr]" */
      char tmpbuf[PATH_MAX + SYMBOL_MAXCHARS + (address_digits * 2) + 12];

      fill_in_stack_frame_string_buffer (addrbuf[count], tmpbuf,
                                         sizeof (tmpbuf));

      if (strings_ptr - buf + strlen (tmpbuf) >= bufsize)
        {
          size_t tmp;
          bufsize += 512;
          buf = (char *) realloc (buf, bufsize);
          if (buf == NULL)
            return NULL;
          /* buf may have moved; update the two pointers we keep into it */
          tmp =  strings_ptr - (char *) charstar_array;
          charstar_array = (char **) buf;
          strings_ptr = buf + tmp;
        }
      *strings_ptr = '\0';
      strcpy (strings_ptr, tmpbuf);
      charstar_array[count] = strings_ptr;
      strings_ptr = strings_ptr + strlen (tmpbuf) + 1;

      count++;
    }

  return (char **) buf;
}

void
gdb_self_backtrace_symbols_fd (void **addrbuf, int num_of_addrs, int fd,
                      int skip, int maxdepth)
{
  /* "pathname (funcname+0xoffsetaddr) [0xaddr]" */
  char tmpbuf[PATH_MAX + SYMBOL_MAXCHARS + (16 * 2) + 12];
  int count = skip;
    
  if (count > skip || skip >= maxdepth)
    return;

  while (count < num_of_addrs && count <= maxdepth + skip)
    {
      sprintf (tmpbuf, "[ %d ] ", count - skip);
      fill_in_stack_frame_string_buffer (addrbuf[count], 
                                         tmpbuf + strlen (tmpbuf),
                                         sizeof (tmpbuf));
      strlcat (tmpbuf, "\n", sizeof (tmpbuf));

      write (fd, tmpbuf, strlen (tmpbuf));
      count++;
    }
}

static void
fill_in_stack_frame_string_buffer (void *addr, char *buf, int buflen)
{
  char addressbuf[16 + 1];
  buf[0] = '\0';
  addressbuf[0] = '\0';
  Dl_info dlinfo;

  if (slower_better_dladdr (addr, &dlinfo))
    {
      if (dlinfo.dli_fname != NULL)
        {
          strlcat (buf, dlinfo.dli_fname, buflen);
          strlcat (buf, " ", buflen);
        }
      if (dlinfo.dli_sname != NULL)
        {
          strlcat (buf, "(", buflen);
          strlcat (buf, dlinfo.dli_sname, buflen);
          strlcat (buf, "+0x", buflen);
          snprintf (addressbuf, 16, "%tx", addr - dlinfo.dli_saddr);
          strlcat (buf, addressbuf, buflen);
          strlcat (buf, ") ", buflen);
          addressbuf[0] = '\0';
        }
    }
  strlcat (buf, "[", buflen);
  snprintf (addressbuf, 16, "%p", addr);
  strlcat (buf, addressbuf, buflen);
  strlcat (buf, "]", buflen);
}

/* Returns 0 if there were any problem parsing the MachO file in memory.
   Sets incoreptr_linkedit_strings to the address of the strings section.
   Sets incoreptr_linkedit_local_nlists to the start of the local nlists.
   Sets number_of_local_nlists to the number of local nlist entries.
   Sets incoreptr_linkedit_external_nlists to the start of the external nlists.
   Sets number_of_external_nlists to the number of external nlist entries.  */

static int
grope_around_incore_macho_file (void *incoreptr_macho_header, 
                                MACHO_SLIDE_TYPE *slide,
                                void **incoreptr_linkedit_strings,
                                void **incoreptr_linkedit_local_nlists,
                                int *number_of_local_nlists,
                                void **incoreptr_linkedit_external_nlists,
                                int *number_of_external_nlists)
{
  char *incoreptr_linkedit_start;
  char *incoreptr_linkedit_nlists;

  char *p;
  uint32_t tmp4bytes;
  int wordsize;
  size_t macho_linkedit_fileoff;
  int i;

  if (slide == NULL
      || incoreptr_linkedit_strings == NULL 
      || incoreptr_linkedit_local_nlists == NULL 
      || number_of_local_nlists == NULL
      || incoreptr_linkedit_external_nlists == NULL
      || number_of_external_nlists == NULL)
    return 0;

  *slide = 0;
  *incoreptr_linkedit_strings = 0;
  *incoreptr_linkedit_local_nlists = 0;
  *number_of_local_nlists = 0;
  *incoreptr_linkedit_external_nlists = 0;
  *number_of_external_nlists = 0;

  tmp4bytes = *((uint32_t *) incoreptr_macho_header);
  if (tmp4bytes != MH_MAGIC) 
    return 0;
  wordsize = 4;

  struct mach_header *mach_header = incoreptr_macho_header;
  p = incoreptr_macho_header + sizeof (struct mach_header);
  i = 0;
  while (i < mach_header->ncmds)
    {
      uint32_t cmd = ((struct load_command *) p)->cmd;
      uint32_t size = ((struct load_command *) p)->cmdsize; 
      struct segment_command *segment_command = (struct segment_command *) p;
      struct symtab_command *symtab_command = (struct symtab_command *) p;
      struct dysymtab_command *dysymtab_command = (struct dysymtab_command *) p;

      if ((cmd == LC_SEGMENT || cmd == LC_SEGMENT_64)
          && strcmp ((const char *) segment_command->segname, SEG_TEXT) == 0)
        {
          *slide = incoreptr_macho_header - (void *) segment_command->vmaddr;
        }
      if ((cmd == LC_SEGMENT  || cmd == LC_SEGMENT_64)
          && strcmp (segment_command->segname, SEG_LINKEDIT) == 0)
        {
          incoreptr_linkedit_start = (void *) segment_command->vmaddr + *slide;
          macho_linkedit_fileoff = segment_command->fileoff; 
        }
      if (cmd == LC_SYMTAB)
        {
          *incoreptr_linkedit_strings = symtab_command->stroff - 
                                        macho_linkedit_fileoff + 
                                        incoreptr_linkedit_start;
          incoreptr_linkedit_nlists = symtab_command->symoff - 
                                      macho_linkedit_fileoff + 
                                      incoreptr_linkedit_start;
        }
      if (cmd == LC_DYSYMTAB)
        {
          *incoreptr_linkedit_local_nlists = incoreptr_linkedit_nlists + 
                            (dysymtab_command->ilocalsym * NLIST_RECORD_SIZE);
          *number_of_local_nlists = dysymtab_command->nlocalsym;
          *incoreptr_linkedit_external_nlists = incoreptr_linkedit_nlists + 
                            (dysymtab_command->iextdefsym * NLIST_RECORD_SIZE);
          *number_of_external_nlists = dysymtab_command->nextdefsym;
          break;
        }
      p += size;
      i++;
    }

  return 1;
}

static int
slower_better_dladdr (void *address_to_find, Dl_info *dlinfo)
{
  void *incoreptr_linkedit_strings;
  void *incoreptr_linkedit_local_nlists;
  void *incoreptr_linkedit_external_nlists;
  char *p, *endpoint;
  int number_of_local_nlists, number_of_external_nlists;
  MACHO_SLIDE_TYPE slide;

  if (dladdr (address_to_find, dlinfo) == 0)
    return 0;

  if (grope_around_incore_macho_file (dlinfo->dli_fbase, 
                                      &slide,
                                      &incoreptr_linkedit_strings, 
                                      &incoreptr_linkedit_local_nlists, 
                                      &number_of_local_nlists,
                                      &incoreptr_linkedit_external_nlists, 
                                      &number_of_external_nlists) == 0)
    {
      return 0;
    }

  p = incoreptr_linkedit_local_nlists;
  endpoint = p + (number_of_local_nlists * NLIST_RECORD_SIZE);

  while (p < endpoint)
    {
      struct nlist *nlist = (struct nlist *) p;
      void *possibly_better_address;
      possibly_better_address = (void *) nlist->n_value + slide;
      if ((nlist->n_type & N_STAB) == 0 
          && (nlist->n_type & N_TYPE) == N_SECT 
          && nlist->n_sect != 0)
        {
          /* Is the address POSSIBLY_BETTER_ADDRESS a closer match than 
             the best we've seen so far?  (best we've seen so far is 
             dlinfo->dli_saddr; the target address we're aiming for 
             is in ADDRESS_TO_FIND)  */
          if (possibly_better_address > dlinfo->dli_saddr 
              && possibly_better_address <= address_to_find)
            {
              dlinfo->dli_saddr = possibly_better_address;
              dlinfo->dli_sname = incoreptr_linkedit_strings +
                                   nlist->n_un.n_strx;
              if (*dlinfo->dli_sname == '_')
                dlinfo->dli_sname++;
            }
        }
      p += NLIST_RECORD_SIZE;
    }

  if (dlinfo->dli_saddr == dlinfo->dli_fbase)
    return 0;

  return 1;
}
