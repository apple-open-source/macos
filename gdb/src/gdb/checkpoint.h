/* APPLE LOCAL file checkpoints */
/* Checkpoints for GDB.
   Copyright 2005
   Free Software Foundation, Inc.

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

/* The memory cache is a block of memory that is part of a checkpoint's
   state.  */

struct memcache
{
  struct memcache *next;
  ULONGEST startaddr;
  int len;
  gdb_byte *cache;
};

enum cp_type {
  unset = '?',
  manual = 'M',
  autogen = 'A',
  reexec = 'r'
};

/* A checkpoint is a saved state of the inferior at a particular point
   in time.  Ideally it includes all state, but may be incomplete in
   some way, and software should cope with that.  It is basically
   registers + memory, but there are optimizations, such as the use of
   process forks to save old state lazily.  */

struct checkpoint
{
  enum cp_type type;

  /* A unique identifier for each checkpoint. These are assigned
     consecutively throughout a debugging session.  */
  int number;

  /* Double linkage for the complete list of checkpoints.  */
  struct checkpoint *next;
  struct checkpoint *prev;

  /* The lnext/lprev linkage connects the line of checkpoints that
     correspond to states prior to the latest (current) state of
     the program. Checkpoints not in this line represent alternate
     lines of execution that may be randomly accessed by number, but
     that did not lead to the current state.  */
  struct checkpoint *lnext;
  struct checkpoint *lprev;

  struct checkpoint *immediate_prev;
  int sequence_number;

  struct regcache *regs;
  struct memcache *mem;

  int pid;

  /* Flag used to decide which ones to keep.  */
  int keep;
};

extern void memcache_get (struct checkpoint *cp, ULONGEST addr, int len);

extern void clear_checkpoints (void);
extern void clear_all_checkpoints (void);
extern struct checkpoint *create_checkpoint (void);
extern struct checkpoint *collect_checkpoint (void);
extern struct checkpoint *finish_checkpoint (struct checkpoint *cp);
extern struct checkpoint *find_checkpoint (int num);
extern void rollback_to_checkpoint (struct checkpoint *cp);
extern void print_checkpoint_info (struct checkpoint *cp);
extern int checkpoint_compare (struct checkpoint *cp1, struct checkpoint *cp2);
extern void checkpoint_clear_inferior ();

extern int auto_checkpointing;

