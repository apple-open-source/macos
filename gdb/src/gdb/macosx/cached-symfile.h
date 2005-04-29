#ifndef __GDB_MACOSX_CACHED_SYMFILE_H__
#define __GDB_MACOSX_CACHED_SYMFILE_H__

#include "defs.h"
#include "objfiles.h"

struct relocation_context
{
  struct objfile *objfile;

  size_t mapped_start;
  size_t from_start;
  size_t to_start;

  size_t len;
};

struct symbol;
struct type;

#ifndef FSF_OBJFILES
struct objfile *allocate_objfile (bfd *abfd, int flags, int symflags, CORE_ADDR mapaddr, const char *prefix);
#endif

extern void mmalloc_protect (PTR md, int flags);

extern struct objfile *cache_bfd (bfd *abfd, const char *prefix, int symflags,
                                  size_t addr, size_t mapaddr,
                                  const char *dest);

struct objfile *create_objfile (bfd *abfd);

extern int build_objfile_section_table (struct objfile *);

extern struct objfile *open_objfile_from_mmalloc_pool (PTR md, bfd *abfd,
                                                       int fd, time_t mtime,
                                                       char *filename,
                                                       const char *prefix);

extern struct objfile *create_objfile_from_mmalloc_pool (bfd *abfd, PTR md,
                                                         int fd,
                                                         CORE_ADDR mapaddr);

extern struct objfile *open_mapped_objfile (const char *filename, bfd *abfd,
                                            time_t mtime, CORE_ADDR mapaddr,
                                            const char *prefix);

struct objfile *objfile_reposition (struct objfile *o,
                                    struct relocation_context *r);

void objfile_verify (struct objfile *o);
void symbol_verify (struct objfile *o, struct symbol *s);
void type_verify (struct objfile *o, struct type *s);

#endif /* __GDB_MACOSX_CACHED_SYMFILE_H__ */
