#include "nextstep-nat-dyld-process.h"

#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include <string.h>

#include "mach-o.h"

extern bfd *exec_bfd;

void dyld_update_section_tables (struct dyld_objfile_info *result, struct target_ops *target)
{
  update_section_tables (result, target);
}

void dyld_merge_section_tables (struct dyld_objfile_info *result)
{
}
