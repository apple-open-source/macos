/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
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

#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "symfile.h"
#include "symtab.h"
#include "target.h"

#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-cfm.h"
#include "macosx-nat-cfm-io.h"
#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-io.h"

#define CFM_MAX_UNIVERSE_LENGTH 1024
#define CFM_MAX_CONNECTION_LENGTH 1024
#define CFM_MAX_CONTAINER_LENGTH 1024
#define CFM_MAX_SECTION_LENGTH 1024
#define CFM_MAX_INSTANCE_LENGTH 1024

#define CFM_NO_ERROR 0
#define CFM_INTERNAL_ERROR -1
#define CFM_NO_SECTION_ERROR -2

#define CFContHashedStringLength(hash) ((hash) >> 16)

extern macosx_inferior_status *macosx_status;

void
cfm_init (void)
{
  struct minimal_symbol *hooksym, *system, *context;
  struct cfm_parser *parser = &macosx_status->cfm_status.parser;
  CORE_ADDR offset = 0;

  hooksym = lookup_minimal_symbol ("gPCFMInfoHooks", NULL, NULL);
  if (hooksym == NULL)
    return;

  system = lookup_minimal_symbol ("gPCFMSystemUniverse", NULL, NULL);
  if (system == NULL)
    return;

  context = lookup_minimal_symbol ("gPCFMContextUniverse", NULL, NULL);
  if (context == NULL)
    return;

  offset = SYMBOL_VALUE_ADDRESS (context) - SYMBOL_VALUE_ADDRESS (system);

  if (offset == 88)
    {
      parser->version = 3;
      parser->universe_length = 88;
      parser->universe_container_offset = 48;
      parser->universe_connection_offset = 60;
      parser->universe_closure_offset = 72;
      parser->connection_length = 68;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 28;
      parser->container_length = 176;
      parser->container_address_offset = 24;
      parser->container_length_offset = 36;
      parser->container_fragment_name_offset = 44;
      parser->container_section_count_offset = 100;
      parser->container_sections_offset = 104;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
    }
  else if (offset == 104)
    {
      parser->version = 2;
      parser->universe_length = 104;
      parser->universe_container_offset = 52;
      parser->universe_connection_offset = 68;
      parser->universe_closure_offset = 84;
      parser->connection_length = 72;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 32;
      parser->container_length = 176;
      parser->container_address_offset = 28;
      parser->container_length_offset = 36;
      parser->container_fragment_name_offset = 44;
      parser->container_section_count_offset = 100;
      parser->container_sections_offset = 104;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
    }
  else if (offset == 120)
    {
      parser->version = 1;
      parser->universe_length = 120;
      parser->universe_container_offset = 68;
      parser->universe_connection_offset = 84;
      parser->universe_closure_offset = 100;
      parser->connection_length = 84;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 36;
      parser->container_length = 172;
      parser->container_address_offset = 28;
      parser->container_length_offset = 32;
      parser->container_fragment_name_offset = 40;
      parser->container_section_count_offset = 96;
      parser->container_sections_offset = 100;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
    }
  else
    {
      warning ("unable to determine CFM version; disabling CFM support");
      parser->version = 0;
      return;
    }

  macosx_status->cfm_status.info_api_cookie = SYMBOL_VALUE_ADDRESS (hooksym);
  dyld_debug ("Found gPCFMInfoHooks in CarbonCore: 0x%s with version %d\n",
              paddr_nz (SYMBOL_VALUE_ADDRESS (hooksym)), parser->version);
}

long
cfm_update (task_t task, struct dyld_objfile_info *info)
{
  long ret;

  unsigned long n_container_ids;
  unsigned long nread_container_ids;
  unsigned long *container_ids;
  ULONGEST tmpbuf;
  struct minimal_symbol *doublecheck;

  unsigned long container_index;

  CORE_ADDR cfm_cookie;
  CORE_ADDR cfm_context;
  struct cfm_parser *cfm_parser;

  if (macosx_status->cfm_status.info_api_cookie == 0)
    cfm_init ();

  if (macosx_status->cfm_status.info_api_cookie == 0)
    return -1;

  cfm_cookie = macosx_status->cfm_status.info_api_cookie;
  cfm_parser = &macosx_status->cfm_status.parser;

  /* The cfm_status was initialized with a set of symbol addresses but
     if CarbonCore slid since then, or the slide wasn't yet seen when
     the cfm_status was initialized, then we will be reading CFM information
     from an incorrect location.  
     So this is a double-check that the address seen when we initialized the
     cfm_status remain the same.  This should be true, of course... but we're
     still seeing cases (unreproducible by us) where gdb is getting a bogus CFM 
     runtime address and crashing on non-CFM apps.  */

  doublecheck = lookup_minimal_symbol_by_pc (cfm_cookie);
  if (strcmp (SYMBOL_LINKAGE_NAME (doublecheck), "gPCFMInfoHooks") != 0)
    {
      warning ("CFM runtime slid since cfm_status initialized; disregarding.");
      return -1;
    }

  if (!safe_read_memory_unsigned_integer (cfm_cookie, 4, &tmpbuf))
    return -1;

  cfm_context = tmpbuf;

  /* No valid context - don't give the following code a chance to do 
     something wrong.  */
  if (cfm_context == 0)
    return -1;

  ret =
    cfm_fetch_context_containers (cfm_parser, cfm_context, 0, 0,
                                  &n_container_ids, NULL);
  if (ret != CFM_NO_ERROR)
    return ret;

  /* More than 10,000 containers?  I distrust you.  We're getting
     cases where gdb, when attaching to a process, reads
     invalid-but-almost-plausible CFM information from the executing
     process and we die trying to allocate an insane amount of
     memory.  Instead of figuring out what the heck is happening
     with the CFM runtime layout, let's just add this sanity check
     this before we try to allocate a huge chunk of memory.  */
  if (n_container_ids > 10000)
    {
      warning ("gdb tried to read %d CFM container IDs; disregarding", 
               (int) n_container_ids);
      return -1;
    }

  container_ids =
    (unsigned long *) xmalloc (n_container_ids * sizeof (unsigned long));

  ret = cfm_fetch_context_containers
    (cfm_parser, cfm_context,
     n_container_ids, 0, &nread_container_ids, container_ids);
  if (ret != CFM_NO_ERROR)
    return ret;

  /* This used to be an assertion but we started getting spurrious cases
     for non-CFM debugging where this condition would somehow be triggered.
     Given that CFM debugging is a rare thing under gdb at this point I'm
     changing this to warn the user about the nature of the problem and
     bail without trying to read the fragment containers.  For real CFM
     debugging users I don't expect this code section to be hit - and I'd
     really like to know why it's getting here at all for non-CFM
     debugging... */
  if (n_container_ids != nread_container_ids)
    {
      warning (
              "gdb expected to read %d CFM container IDs, but actually read %d",
               (int) n_container_ids, (int) nread_container_ids);
      return -1;
    }

  for (container_index = 0; container_index < n_container_ids;
       container_index++)
    {
      NCFragContainerInfo container_info;
      NCFragSectionInfo section_info;

      ret =
        cfm_fetch_container_info (cfm_parser, container_ids[container_index],
                                  &container_info);
      if (ret != CFM_NO_ERROR)
        continue;

      if (container_info.sectionCount > 0)
        {
          ret =
            cfm_fetch_container_section_info (cfm_parser,
                                              container_ids[container_index],
                                              0, &section_info);
          if (ret != CFM_NO_ERROR)
            continue;
        }

      {
        struct dyld_objfile_entry *entry;

        entry = dyld_objfile_entry_alloc (info);

        entry->dyld_name = xstrdup (container_info.name + 1);
        entry->dyld_name_valid = 1;

        entry->dyld_addr = container_info.address;
        entry->dyld_slide = container_info.address;
        entry->dyld_length = container_info.length;
        entry->dyld_valid = 1;

        entry->cfm_container = container_ids[container_index];

        entry->reason = dyld_reason_cfm;
      }
    }

  return CFM_NO_ERROR;
}

long
cfm_parse_universe_info (struct cfm_parser *parser,
                         unsigned char *buf,
                         size_t len, NCFragUniverseInfo *info)
{
  if (parser->universe_container_offset + 12 > len)
    {
      return -1;
    }
  if (parser->universe_connection_offset + 12 > len)
    {
      return -1;
    }
  if (parser->universe_closure_offset + 12 > len)
    {
      return -1;
    }

  info->containers.head =
    bfd_getb32 (buf + parser->universe_container_offset);
  info->containers.tail =
    bfd_getb32 (buf + parser->universe_container_offset + 4);
  info->containers.length =
    bfd_getb32 (buf + parser->universe_container_offset + 8);
  info->connections.head =
    bfd_getb32 (buf + parser->universe_connection_offset);
  info->connections.tail =
    bfd_getb32 (buf + parser->universe_connection_offset + 4);
  info->connections.length =
    bfd_getb32 (buf + parser->universe_connection_offset + 8);
  info->closures.head = bfd_getb32 (buf + parser->universe_closure_offset);
  info->closures.tail =
    bfd_getb32 (buf + parser->universe_closure_offset + 4);
  info->closures.length =
    bfd_getb32 (buf + parser->universe_closure_offset + 8);

  return 0;
}

long
cfm_fetch_universe_info (struct cfm_parser *parser,
                         CORE_ADDR addr, NCFragUniverseInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_UNIVERSE_LENGTH];
  if (parser->universe_length > CFM_MAX_UNIVERSE_LENGTH)
    {
      return -1;
    }

  ret = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL, buf, 
                     addr, parser->universe_length); 
  if (ret < 0)
    {
      return -1;
    }

  return cfm_parse_universe_info (parser, buf, parser->universe_length, info);
}

long
cfm_parse_container_info (struct cfm_parser *parser,
                          unsigned char *buf,
                          size_t len, NCFragContainerInfo *info)
{
  info->next = bfd_getb32 (buf + 0);
  info->address = bfd_getb32 (buf + parser->container_address_offset);
  info->length = bfd_getb32 (buf + parser->container_length_offset);
  info->sectionCount =
    bfd_getb32 (buf + parser->container_section_count_offset);

  return 0;
}

long
cfm_fetch_container_info (struct cfm_parser *parser,
                          CORE_ADDR addr, NCFragContainerInfo *info)
{
  int ret, err;
  unsigned long name_length, name_addr;

  unsigned char buf[CFM_MAX_CONTAINER_LENGTH];
  if (parser->container_length > CFM_MAX_CONTAINER_LENGTH)
    {
      return -1;
    }

  ret = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
                     buf, addr, parser->container_length);
  if (ret < 0)
    {
      return -1;
    }

  ret =
    cfm_parse_container_info (parser, buf, parser->container_length, info);
  if (ret < 0)
    {
      return -1;
    }

  name_length =
    CFContHashedStringLength (bfd_getb32
                              (buf + parser->container_fragment_name_offset));
  if (name_length > 63)
    return CFM_INTERNAL_ERROR;
  name_addr = bfd_getb32 (buf + parser->container_fragment_name_offset + 4);

  info->name[0] = name_length;

  ret = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
                     &info->name[1], name_addr, name_length);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;

  info->name[name_length + 1] = '\0';

  return 0;
}

long
cfm_parse_connection_info (struct cfm_parser *parser,
                           unsigned char *buf,
                           size_t len, NCFragConnectionInfo *info)
{
  if (parser->connection_next_offset + 4 > len)
    {
      return -1;
    }
  if (parser->connection_container_offset + 4 > len)
    {
      return -1;
    }

  info->next = bfd_getb32 (buf + parser->connection_next_offset);
  info->container = bfd_getb32 (buf + parser->connection_container_offset);

  return 0;
}

long
cfm_fetch_connection_info (struct cfm_parser *parser,
                           CORE_ADDR addr, NCFragConnectionInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_CONNECTION_LENGTH];
  if (parser->connection_length > CFM_MAX_CONNECTION_LENGTH)
    {
      return -1;
    }

  ret = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
                     buf, addr, parser->connection_length);
  if (ret < 0)
    {
      return -1;
    }

  return cfm_parse_connection_info (parser, buf, parser->connection_length,
                                    info);
}

long
cfm_parse_section_info (struct cfm_parser *parser,
                        unsigned char *buf,
                        size_t len, NCFragSectionInfo *info)
{
  if (parser->section_total_length_offset + 4 > len)
    {
      return -1;
    }

  info->length = bfd_getb32 (buf + parser->section_total_length_offset);

  return 0;
}

long
cfm_parse_instance_info (struct cfm_parser *parser,
                         unsigned char *buf,
                         size_t len, NCFragInstanceInfo *info)
{
  if (parser->instance_address_offset + 4 > len)
    {
      return -1;
    }

  info->address = bfd_getb32 (buf + parser->instance_address_offset);

  return 0;
}

long
cfm_fetch_context_containers (struct cfm_parser *parser,
                              CORE_ADDR contextAddr,
                              unsigned long requestedCount,
                              unsigned long skipCount,
                              unsigned long *totalCount_o,
                              unsigned long *containerIDs_o)
{
  int ret;

  unsigned long localTotal = 0;
  unsigned long currIDSlot;

  NCFragUniverseInfo universe;
  NCFragContainerInfo container;

  CORE_ADDR curContainer = 0;

  *totalCount_o = 0;

  ret = cfm_fetch_universe_info (parser, contextAddr, &universe);

  localTotal = universe.containers.length;

  if (skipCount >= localTotal)
    {
      *totalCount_o = localTotal;
      return CFM_NO_ERROR;
    }

  if (requestedCount > (localTotal - skipCount))
    requestedCount = localTotal - skipCount;

  curContainer = universe.containers.head;

  while (skipCount > 0)
    {
      if (curContainer == 0)
        return CFM_INTERNAL_ERROR;

      ret = cfm_fetch_container_info (parser, curContainer, &container);

      curContainer = container.next;
      skipCount -= 1;
    }

  for (currIDSlot = 0; currIDSlot < requestedCount; currIDSlot += 1)
    {
      if (curContainer == 0)
        return CFM_INTERNAL_ERROR;

      ret = cfm_fetch_container_info (parser, curContainer, &container);

      containerIDs_o[currIDSlot] = curContainer;
      curContainer = container.next;
    }

  *totalCount_o = localTotal;
  return CFM_NO_ERROR;
}

long
cfm_fetch_container_section_info (struct cfm_parser *parser,
                                  CORE_ADDR addr,
                                  unsigned long sectionIndex,
                                  NCFragSectionInfo *section)
{
  int ret, err;
  unsigned long offset;

  NCFragContainerInfo container;
  unsigned char section_buf[CFM_MAX_SECTION_LENGTH];

  ret = cfm_fetch_container_info (parser, addr, &container);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;

  if (sectionIndex >= container.sectionCount)
    return CFM_NO_SECTION_ERROR;

  offset =
    (addr + parser->container_length - (2 * parser->section_length) +
     (sectionIndex * parser->section_length));
  ret = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
                     section_buf, offset, parser->section_length);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;

  ret =
    cfm_parse_section_info (parser, section_buf, parser->section_length,
                            section);
  if (ret < 0)
    return ret;

  return CFM_NO_ERROR;
}
