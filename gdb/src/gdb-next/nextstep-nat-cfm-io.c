#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <string.h>

#if 0
pef_load_library (const struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  bfd *pbfd = NULL;
  bfd *sbfd = NULL;
  char *symname = NULL;
  asection *csection = NULL;
  asection *dsection = NULL;
  struct section_addr_info addrs;
  unsigned int i = 0;

  if (strcmp (bfd_get_target (pbfd), "pef-xlib") == 0) {
    return pbfd;
  }

  if (strcmp (bfd_get_target (pbfd), "pef") != 0) {
    warning ("Unable to read symbols from %s: invalid file format \"%s\".",
	     bfd_get_filename (pbfd), bfd_get_target (pbfd));
    return pbfd;
  }
  
  csection = bfd_get_section_by_name (pbfd, "code");
  if (csection == NULL) {
    warning ("Unable to find 'code' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return pbfd;
  }

  dsection = bfd_get_section_by_name (pbfd, "packed-data");
  if (dsection == NULL) {
    warning ("Unable to find 'packed-data' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return pbfd;
  }

  symname = xmalloc (strlen (name) + strlen (".xSYM") + 1);
  sprintf (symname, "%s%s", name, ".xSYM");
  sbfd = bfd_openr (symname, "sym");
#if 0
  if (sbfd == NULL) {
    warning ("unable to open \"%s\": %s", symname, bfd_errmsg (bfd_get_error ()));
  }
#endif

  for (i = 0; i < MAX_SECTIONS; i++) {
    addrs.other[i].name = NULL;
    addrs.other[i].addr = address;
    addrs.other[i].sectindex = 0;
  }
  addrs.addrs_are_offsets = 1;

  if (pbfd != NULL) {
    if (! bfd_check_format (pbfd, bfd_object)) {
      warning ("file \"%s\" is not a valid symbol file", pbfd->filename);
    } else {
      symbol_file_add_bfd_safe (pbfd, 0, &addrs, 0, 0, 0, 0);
    }
  }

  addrs.other[0].name = "code";
  addrs.other[0].addr = address + csection->vma;
  addrs.other[0].sectindex = 0;

  addrs.other[1].name = "packed-data";
  addrs.other[1].addr = address + dsection->vma;
  addrs.other[1].sectindex = 1;
  
  for (i = 2; i < MAX_SECTIONS; i++) {
    addrs.other[i].name = NULL;
    addrs.other[i].addr = address + csection->vma;
    addrs.other[i].sectindex = 0;
  }

  addrs.addrs_are_offsets = 1;

  if (sbfd != NULL) {
    if (! bfd_check_format (sbfd, bfd_object)) {
      warning ("file \"%s\" is not a valid symbol file", sbfd->filename);
    } else {
      symbol_file_add_bfd_safe (sbfd, 0, &addrs, 0, 0, 0, 0);
    }
  }

  return pbfd;
}
#endif
