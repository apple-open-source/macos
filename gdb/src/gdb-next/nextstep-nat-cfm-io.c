#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <string.h>

struct pef_inferior_info {
  bfd_vma addr;
  bfd_vma len;
};

bfd_size_type pef_inferior_read
(PTR iodata, PTR data, bfd_size_type size, bfd_size_type nitems, bfd *abfd, bfd_vma where)
{
  struct pef_inferior_info *iptr = (struct pef_inferior_info *) iodata;
  int ret;

  CHECK_FATAL (iptr != NULL);
  
  if (strcmp (current_target.to_shortname, "macos-child") != 0) {
    bfd_set_error (bfd_error_no_contents);
    return 0;
  }

  if (where > iptr->len) {
    bfd_set_error (bfd_error_no_contents);
    return 0;
  }

  ret = current_target.to_xfer_memory (iptr->addr + where, data, (size * nitems), 0, &current_target);
  if (ret <= 0) {
    bfd_set_error (bfd_error_system_call);
    return 0;
  }

  return ret;
}

bfd_size_type pef_inferior_write
(PTR iodata, const PTR data, 
 bfd_size_type size, bfd_size_type nitems,
 bfd *abfd, bfd_vma where)
{
  error ("unable to write to in-memory images");
}

int pef_inferior_flush (PTR iodata, bfd *abfd)
{
  return 0;
}

boolean pef_inferior_close (PTR iodata, bfd *abfd)
{
  free (iodata);
  return 1;
}

bfd *pef_inferior_bfd (const char *name, CORE_ADDR addr, CORE_ADDR len)
{
  struct pef_inferior_info *iptr = NULL;
  struct bfd_io_functions fdata;
  char *filename = NULL;
  bfd *ret = NULL;
  int iret = 0;

  iptr = (struct pef_inferior_info *) xmalloc (sizeof (struct pef_inferior_info));
  iptr->addr = addr;
  iptr->len = len;

  fdata.iodata = iptr;
  fdata.read_func = &pef_inferior_read;
  fdata.write_func = &pef_inferior_write;
  fdata.flush_func = &pef_inferior_flush;
  fdata.close_func = &pef_inferior_close;

  iret = asprintf (&filename, "[pef container \"%s\" at 0x%lx for 0x%lx]",
		   name, (unsigned long) addr, (unsigned long) len);
  if (iret == 0) {
    warning ("unable to allocate for filename for pef container \"%s\"", name);
    return NULL;
  }

  ret = bfd_funopenr (filename, NULL, &fdata);
  if (ret == NULL) { 
    warning ("Unable to open memory image for pef container \"%s\" at 0x%lx for 0x%lx; skipping",
	     name, (unsigned long) addr, (unsigned long) len);
    return NULL;
  }
  
  /* FIXME: should check for errors from bfd_close (for one thing, on
     error it does not free all the storage associated with the bfd).  */

  if (! bfd_check_format (ret, bfd_object)) {
    warning ("Unable to read symbols from %s: %s.", bfd_get_filename (ret), bfd_errmsg (bfd_get_error ()));
    bfd_close (ret);
    return NULL;
  }

  return ret;
}

void pef_read (const char *name, CORE_ADDR address, CORE_ADDR length)
{
  bfd *pbfd = NULL;
  bfd *sbfd = NULL;
  char *symname = NULL;
  asection *csection = NULL;
  asection *dsection = NULL;
  struct section_addr_info addrs;
  unsigned int i = 0;

  pbfd = pef_inferior_bfd (name, address, length);
  if (pbfd == NULL) {
    warning ("Unable to map BFD for pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return;
  }

  if (strcmp (bfd_get_target (pbfd), "pef-xlib") == 0) {
    return;
  }

  if (strcmp (bfd_get_target (pbfd), "pef") != 0) {
    warning ("Unable to read symbols from %s: invalid file format \"%s\".",
	     bfd_get_filename (pbfd), bfd_get_target (pbfd));
    return;
  }
  
  csection = bfd_get_section_by_name (pbfd, "code");
  if (csection == NULL) {
    warning ("Unable to find 'code' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return;
  }

  dsection = bfd_get_section_by_name (pbfd, "packed-data");
  if (dsection == NULL) {
    warning ("Unable to find 'packed-data' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return;
  }

  symname = xmalloc (strlen (name) + strlen (".xSYM") + 1);
  sprintf (symname, "%s%s", name, ".xSYM");
  sbfd = bfd_openr (symname, "sym");
  if (sbfd == NULL) {
    /* warning ("unable to open \"%s\": %s", symname, bfd_errmsg (bfd_get_error ())); */
  }

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
}

static void pef_read_command (char *args, int from_tty)
{
  char **argv;
  char *val;
  struct cleanup *cleanups;
  CORE_ADDR address, length;

  argv = buildargv (args);
  if (argv == NULL)
    nomem (0);
  cleanups = make_cleanup_freeargv (argv);

  if ((argv[0] == NULL) || (argv[1] == NULL) || (argv[2] != NULL))
    error ("usage: pef-read <address> <length>");

  val = argv[0];
  if (val[0] == '0' && val[1] == 'x')
    address = strtoul (val + 2, NULL, 16);
  else
    address = strtoul (val, NULL, 10);

  val = argv[1];
  if (val[0] == '0' && val[1] == 'x')
    length = strtoul (val + 2, NULL, 16);
  else
    length = strtoul (val, NULL, 10);

  pef_read ("user-specified", address, length);
  
  do_cleanups (cleanups);
}  

void
_initialize_nextstep_nat_cfm_io ()
{
  add_com ("pef-read", class_run, pef_read_command,
	   "Read symbols from a pef container at address <addr>.");
}
