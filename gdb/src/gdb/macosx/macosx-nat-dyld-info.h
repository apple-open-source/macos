#ifndef __GDB_MACOSX_NAT_DYLD_INFO_H__
#define __GDB_MACOSX_NAT_DYLD_INFO_H__

#include "defs.h"

struct _bfd;

typedef enum dyld_objfile_reason { 

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

struct dyld_objfile_entry {

  const char *prefix;

  char *dyld_name;
  int dyld_name_valid;

  CORE_ADDR dyld_addr;
  CORE_ADDR dyld_slide;
  CORE_ADDR dyld_length;
  unsigned long dyld_index;
  int dyld_valid;

  unsigned long cfm_container;

  char *user_name;

  char *image_name;
  int image_name_valid;

  CORE_ADDR image_addr;
  int image_addr_valid;

  char *text_name;
  int text_name_valid;

  struct _bfd *abfd;
  struct objfile *objfile;

  struct _bfd *commpage_bfd; /* The BFD corresponding to the commpage symbol information */
  struct objfile *commpage_objfile; /* The corresponding objfile */

  const char *loaded_name;
  CORE_ADDR loaded_memaddr;
  CORE_ADDR loaded_addr;
  CORE_ADDR loaded_offset;
  int loaded_addrisoffset;
  int loaded_from_memory;
  int loaded_error;

  int load_flag;

  enum dyld_objfile_reason reason;

  int allocated;
};

struct dyld_objfile_info {
  struct dyld_objfile_entry *entries;
  unsigned int nents;
  unsigned int maxents;
  struct obj_section *sections;
  struct obj_section *sections_end;
};

struct dyld_path_info;

enum { DYLD_ENTRY_FILENAME_LOADED = 1 } dyld_entry_filename_type;

const char *dyld_entry_filename
PARAMS ((const struct dyld_objfile_entry *e, const struct dyld_path_info *d, int type));

char *dyld_offset_string
PARAMS ((unsigned long offset));

char *dyld_entry_string
PARAMS ((struct dyld_objfile_entry *e, int use_shortnames));

const char *dyld_reason_string
PARAMS ((dyld_objfile_reason r));

void dyld_objfile_entry_clear
PARAMS ((struct dyld_objfile_entry *e));

void dyld_objfile_info_init
PARAMS ((struct dyld_objfile_info *i));

void dyld_objfile_info_pack
PARAMS ((struct dyld_objfile_info *i));

void dyld_objfile_info_free
PARAMS ((struct dyld_objfile_info *i));

void dyld_objfile_info_copy
PARAMS ((struct dyld_objfile_info *d, struct dyld_objfile_info *s));

void dyld_objfile_info_copy_entries
PARAMS ((struct dyld_objfile_info *d, struct dyld_objfile_info *s, unsigned int mask));

struct dyld_objfile_entry *dyld_objfile_entry_alloc
PARAMS ((struct dyld_objfile_info *i));

void dyld_print_shlib_info
PARAMS ((struct dyld_objfile_info *s, unsigned int reason_mask, int header, const char *args));

int dyld_resolve_shlib_num
PARAMS ((struct dyld_objfile_info *s, unsigned int num,
	 struct dyld_objfile_entry **eptr, struct objfile **optr));

int dyld_objfile_info_compare
PARAMS ((struct dyld_objfile_info *a, struct dyld_objfile_info *b));

void dyld_convert_entry PARAMS ((struct objfile *o, struct dyld_objfile_entry *e));

void 
dyld_entry_info PARAMS ((struct dyld_objfile_entry *e, int print_basenames, 
			 char **in_name, char **in_objname, char **in_symname,
			 char **in_commobjname, char **in_commsymname,
			 char **addr, char **slide, char **prefix));

void dyld_print_entry_info
PARAMS ((struct dyld_objfile_entry *j, unsigned int shlibnum,
	 unsigned int baselen));

int dyld_entry_shlib_num
PARAMS ((struct dyld_objfile_info *s, struct dyld_objfile_entry *eptr,
	 unsigned int *numptr));

int dyld_entry_shlib_num_matches
PARAMS ((int shlibnum, const char *args, int verbose));

unsigned int dyld_next_allocated_shlib
PARAMS ((struct dyld_objfile_info *info, unsigned int n));

#define DYLD_ALL_OBJFILE_INFO_ENTRIES(info, o, n)          	   \
  for ((n) = dyld_next_allocated_shlib ((info), 0);		   \
       ((n) < (info)->nents) ? (o = &(info)->entries[(n)], 1) : 0; \
       (n) = dyld_next_allocated_shlib (info, (n) + 1))

#endif /* __GDB_MACOSX_NAT_DYLD_INFO_H__ */
