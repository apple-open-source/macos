#ifndef __GDB_MACOSX_NAT_DYLD_INFO_H__
#define __GDB_MACOSX_NAT_DYLD_INFO_H__

#include "defs.h"

struct _bfd;

typedef enum dyld_objfile_reason
{

  dyld_reason_deallocated = 0x0000,

  dyld_reason_user = 0x0001,
  dyld_reason_init = 0x0002,
  dyld_reason_executable = 0x0004,
  dyld_reason_dyld = 0x0008,
  dyld_reason_cfm = 0x0010,

  dyld_reason_executable_mask = 0x0004,

  dyld_reason_type_mask = 0x00ff,
  dyld_reason_flags_mask = 0xff00,

  dyld_reason_image_mask = 0x0006,

  dyld_reason_cached_mask = 0x0100,
  dyld_reason_weak_mask = 0x0200,
  dyld_reason_cached_weak_mask = 0x0300,

  dyld_reason_all_mask = 0xfffe
} dyld_objfile_reason;


/* There is one struct dyld_objfile_entry for each file image that
   the program has loaded, or will load.  */

struct dyld_objfile_entry
{
  const char *prefix;  /* "__dyld_" for the dyld image file by default */

  CORE_ADDR dyld_addr;
  CORE_ADDR dyld_slide;
  CORE_ADDR dyld_length;
  struct section_offsets *dyld_section_offsets;

  /* This boolean seems to indicate that dyld has told us about this particular
      dyld_objfile_entry.  I guess that's distinguished from load
      command-discovered images that aren't actually loaded yet?  */

  int dyld_valid;

#if WITH_CFM
  unsigned long cfm_container;  /* it really is 32 bits - CFM won't go 64bit */
#endif

  /* Names names names.
     Why oh why lord are there all these names? 
     What use can they possibly have? */

  /* USER_NAME is a name coming from the user.  This can happen with
     a DYLD_INSERT_LIBRARY name, or it can happen if we've got an
     objfile w/o a corresponding dyld_objfile_entry - we'll use the
     objfile->name and put it in the d_o_e's user_name field.  */

  char *user_name;

  char *dyld_name;
  int dyld_name_valid;

  char *image_name;
  int image_name_valid;

  CORE_ADDR image_addr;
  int image_addr_valid;

  /* We attempt to slide all the dylibs etc we see before executing the
     program so they don't overlap.  This is the arbitrarily chosen address
     for this dylib, if any.  */
  CORE_ADDR pre_run_slide_addr;
  int pre_run_slide_addr_valid;

  /* We got this name from the executable's mach_header load commands,
     or we copied it from the bfd of the specified executable file.  */
  char *text_name;
  int text_name_valid;

  const char *loaded_name;


  struct bfd *abfd;
  struct objfile *objfile;

  struct bfd *commpage_bfd;           /* The BFD for the commpage symbol info */
  struct objfile *commpage_objfile;   /* The corresponding objfile */

  CORE_ADDR loaded_memaddr;
  CORE_ADDR loaded_addr;
  CORE_ADDR loaded_offset;

  /* God as my witness, I can't figure out what this one is for.  It seems
     to indicate that the LOADED_ADDR field is the slide (aka offset) instead
     of an absolute address.  Huh?  Like it gets set for the main executable
     which is at 0x0, and if the DYLD_VALID isn't set and
     IMAGE_ADDR_VALID isn't set, dyld_load_symfile will set it to
     the slide, which I guess somehow got set without DYLD_VALID getting set...
     uh.... 
     I don't think it actually does anything.  jsm/2004-12-15*/

  int loaded_addrisoffset;

  int loaded_from_memory;
  int loaded_error;

  int load_flag;

  enum dyld_objfile_reason reason;

  /* As of Leopard, dyld makes up a "shared cache" of commonly used dylibs.
     All the dylibs in this shared cache are "prebound" into the cache, so 
     the copy in memory looks like it's loaded at it's intended address.
     But that address is NOT the same as the load address of the disk file.  
     This tells us whether the library is in the cache  */
  int in_shared_cache;

  /* The array of dyld_objfile_entry's for the inferior process is a little
     sparse -- some of the entries will be used, others will be allocated but
     unused.  This boolean tells us whether this dyld_objfile_entry is used
     or not.  */

  int allocated;
};


/* There is one struct dyld_objfile_info per inferior process.  */

struct dyld_objfile_info
{
  /* ENTRIES is an array of dyld_objfile_entry structs; MAXENTS of them
     are allocated, NENTS of them are currently being used.

     The offsets in the ENTRIES array correspond with the "shlib numbers"
     that gdb exposes to the users, but they are off by one.  e.g. the
     first shlib number that a user will see is 1, but that corresponds to
     entries[0] in this structure.  Be careful to maintain this off-by-one
     relationship or you'll have conflicting input/output.

     This "offset is the key" appraoch works, but it would be just as easy
     to disassociate the two and have dyld_objfile_entry contain a KEY 
     field... */

  struct dyld_objfile_entry *entries;
  int nents;
  int maxents;
  struct obj_section *sections;
  struct obj_section *sections_end;
};

struct dyld_path_info;

enum dyld_entry_filename_type
{
  DYLD_ENTRY_FILENAME_BASE = 0,
  DYLD_ENTRY_FILENAME_LOADED = 1,
  DYLD_ENTRY_FILENAME_USER = 2
};

const char *dyld_entry_filename (const struct dyld_objfile_entry *e,
                                 const struct dyld_path_info *d, 
                                 enum dyld_entry_filename_type type);

char *dyld_offset_string (CORE_ADDR offset);

char *dyld_entry_string (struct dyld_objfile_entry *e, int use_shortnames);

const char *dyld_reason_string (dyld_objfile_reason r);

void dyld_objfile_entry_clear (struct dyld_objfile_entry *e);

void dyld_objfile_info_init (struct dyld_objfile_info *i);

void dyld_objfile_info_clear_objfiles (struct dyld_objfile_info *i);

void dyld_objfile_info_pack (struct dyld_objfile_info *i);

void dyld_objfile_info_free (struct dyld_objfile_info *i);

void dyld_objfile_info_copy (struct dyld_objfile_info *d,
                             struct dyld_objfile_info *s);

void dyld_objfile_info_copy_entries (struct dyld_objfile_info *d,
                                     struct dyld_objfile_info *s,
                                     unsigned int mask);

struct dyld_objfile_entry *dyld_objfile_entry_alloc (struct dyld_objfile_info *i);

void dyld_print_shlib_info (struct dyld_objfile_info *s,
                            unsigned int reason_mask, int header, char *args);

int dyld_resolve_shlib_num (struct dyld_objfile_info *s, int num,
                            struct dyld_objfile_entry **eptr,
                            struct objfile ** optr);

int dyld_objfile_info_compare (struct dyld_objfile_info *a,
                               struct dyld_objfile_info *b);

void dyld_convert_entry (struct objfile *o, struct dyld_objfile_entry *e);

void dyld_entry_info (struct dyld_objfile_entry *e, int print_basenames,
                      char **in_name, char **in_objname, char **in_symname,
                      char **in_auxobjname, char **in_auxsymname, char **in_dsymobjname,
		      char **addr, char **slide, char **prefix);

void dyld_print_entry_info (struct dyld_objfile_entry *j, int shlibnum, int baselen);

int dyld_shlib_info_basename_length (struct dyld_objfile_info *, unsigned int);

int dyld_entry_shlib_num (struct dyld_objfile_info *s,
                          struct dyld_objfile_entry *eptr,
                          int *numptr);

int dyld_entry_shlib_num_matches (int shlibnum, char *args, int verbose);

int dyld_next_allocated_shlib (struct dyld_objfile_info *info, int n);


/* The one-per-inferior INFO structure has an array of dyld_objfile_entry
   structures, one-per-executable/dylib/bundle/etc.  This macro iterates
   over all the dyld_objfile_entry's in the given INFO, setting O to
   each in turn, and using N to keep track of where it is in the array.
   INFO is type struct dyld_objfile_info
   O is type struct dyld_objfile_entry
   N is type int

   The middle expression in the for loop is equivalent to
     if (n < info->nents)
        o = info->entries[n]
     else
        break;
*/
#define DYLD_ALL_OBJFILE_INFO_ENTRIES(info, o, n)                  \
  for ((n) = dyld_next_allocated_shlib ((info), 0);                \
       ((n) < (info)->nents) ? (o = &(info)->entries[(n)], 1) : 0; \
       (n) = dyld_next_allocated_shlib (info, (n) + 1))

#endif /* __GDB_MACOSX_NAT_DYLD_INFO_H__ */
