#ifndef _NEXTSTEP_NAT_DYLD_INFO_H_
#define _NEXTSTEP_NAT_DYLD_INFO_H_

#include "defs.h"

struct _bfd;

typedef enum dyld_objfile_reason { 
  dyld_reason_cached, dyld_reason_init, dyld_reason_dyld, dyld_reason_executable
} dyld_objfile_reason;

struct dyld_objfile_entry {

  const char *prefix;

  char *dyld_name;
  int dyld_name_valid;

  CORE_ADDR dyld_addr;
  CORE_ADDR dyld_slide;
  unsigned long dyld_index;
  int dyld_valid;

  char *user_name;

  char *image_name;
  int image_name_valid;

  CORE_ADDR image_addr;
  int image_addr_valid;

  char *text_name;
  int text_name_valid;

  struct _bfd *abfd;
  struct objfile *objfile;

  const char *loaded_name;
  CORE_ADDR loaded_memaddr;
  CORE_ADDR loaded_addr;
  CORE_ADDR loaded_offset;
  int loaded_addrisoffset;
  int loaded_from_memory;
  int loaded_error;
  int loaded_flag;

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

const int dyld_entry_source_filename_is_absolute
PARAMS ((struct dyld_objfile_entry *e));

const char *dyld_entry_source_filename
PARAMS ((struct dyld_objfile_entry *e));

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

struct dyld_objfile_entry *dyld_objfile_entry_alloc
PARAMS ((struct dyld_objfile_info *i));

void dyld_print_shlib_info
PARAMS ((struct dyld_objfile_info *s) );

#endif /* _NEXTSTEP_NAT_DYLD_INFO_H_ */
