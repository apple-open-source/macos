/* APPLE LOCAL file indexing */
/* stub-genindex.c Stubs for symbols provided by genindex.c and referenced by
   front-ends (e.g., Fortran) that do not support indexing. 
*/

#include "config.h"
#include "system.h"

#include "genindex.h"

int flag_debug_gen_index = 0;    
int flag_gen_index = 0;          
int flag_gen_index_original = 0;
int c_language = -1;
int flag_gen_index_header = 0;

void
init_gen_indexing ()
{
}

void
finish_gen_indexing ()
{
}

void
push_cur_index_filename (name)
     char *name ATTRIBUTE_UNUSED;
{
}

void
pop_cur_index_filename ()
{
}

void
flush_index_buffer ()
{
}

void
gen_indexing_info (info_tag, name, number)
    int info_tag ATTRIBUTE_UNUSED;
    const char *name ATTRIBUTE_UNUSED;
    int  number ATTRIBUTE_UNUSED;
{
}

void gen_indexing_header (name)
     char *name ATTRIBUTE_UNUSED;
{
}

void gen_indexing_footer ()
{
}

/* APPLE LOCAL begin constant strings */
int flag_next_runtime = 0;
const char *constant_string_class_name = 0;
/* APPLE LOCAL end constant strings */

/* APPLE LOCAL begin constant cfstrings */
int 
build_cfstring_ascii (str)
     int str ATTRIBUTE_UNUSED;
{
  return 0;
}
/* APPLE LOCAL end constant cfstrings */

