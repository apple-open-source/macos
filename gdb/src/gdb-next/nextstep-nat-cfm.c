#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"
#include "gdbcore.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-cfm.h"
#include "nextstep-nat-cfm-io.h"
#include "nextstep-nat-cfm-process.h"

#define CFM_MAX_UNIVERSE_LENGTH 1024
#define CFM_MAX_CONNECTION_LENGTH 1024
#define CFM_MAX_CONTAINER_LENGTH 1024
#define CFM_MAX_SECTION_LENGTH 1024
#define CFM_MAX_INSTANCE_LENGTH 1024

enum cfm_errtype { 
  noErr = 0,
  paramErr = -1,
  cfragCFMInternalErr = -2,
  cfragConnectionIDErr = -3,
  cfragContainerIDErr = -4,
  cfragFragmentCorruptErr = -5,
  cfragNoSectionErr = -6
};

#define CFContHashedStringLength(hash) ((hash) >> 16)

extern next_inferior_status *next_status;

long
cfm_update (task_t task, struct dyld_objfile_info *info)
{
  long ret;

  unsigned long n_connection_ids;
  unsigned long nread_connection_ids;
  unsigned long *connection_ids;

  unsigned long connection_index;

  CORE_ADDR cfm_cookie;
  CORE_ADDR cfm_context;
  struct cfm_parser *cfm_parser;

  cfm_cookie = next_status->cfm_status.info_api_cookie;
  cfm_parser = &next_status->cfm_status.parser;

  if (cfm_cookie == NULL)
    return -1;

  cfm_context = read_memory_unsigned_integer (cfm_cookie, 4);

  ret = cfm_fetch_context_connections (cfm_parser, cfm_context, 0, 0, &n_connection_ids, NULL);
  if (ret != noErr)
      return ret;

  connection_ids = (unsigned long *) xmalloc (n_connection_ids * sizeof (unsigned long));

  ret = cfm_fetch_context_connections (cfm_parser, cfm_context, n_connection_ids, 0, &nread_connection_ids, connection_ids);
  if (ret != noErr)
      return ret;

  CHECK (n_connection_ids == nread_connection_ids);

  for (connection_index = 0; connection_index < n_connection_ids; connection_index++)
    {
      NCFragConnectionInfo connection_info;
      NCFragContainerInfo container_info;
      NCFragSectionInfo section_info;
      NCFragInstanceInfo instance_info;

      ret = cfm_fetch_connection_info (cfm_parser, connection_ids[connection_index], &connection_info);
      if (ret != noErr)
	continue;
      
      ret = cfm_fetch_container_info (cfm_parser, connection_info.container, &container_info);
      if (ret != noErr)
	continue;
      
      if (container_info.sectionCount > 0) {
	ret = cfm_fetch_connection_section_info (cfm_parser, connection_ids[connection_index], 0, &section_info, &instance_info);
	if (ret != noErr)
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
	entry->dyld_index = 0;
	entry->dyld_valid = 1;

	entry->cfm_connection = connection_ids[connection_index];

	entry->reason = dyld_reason_cfm;
      }
    }

  return noErr;
}

long
cfm_fetch_context_connections
(struct cfm_parser *parser,
 CORE_ADDR contextAddr,
 unsigned long requestedCount, unsigned long skipCount,
 unsigned long *totalCount_o, unsigned long* connectionIDs_o)
{
  int ret;

  unsigned long localTotal = 0;
  unsigned long currIDSlot;

  NCFragUniverseInfo universe;
  NCFragConnectionInfo connection;

  CORE_ADDR curConnection = 0;

  *totalCount_o = 0;

  ret = cfm_fetch_universe_info (parser, contextAddr, &universe);

  localTotal = universe.connections.length;

  if (skipCount >= localTotal)
    {
      *totalCount_o = localTotal;
      return noErr;
    }

  if (requestedCount > (localTotal - skipCount))
    requestedCount = localTotal - skipCount;

  curConnection = universe.connections.head;

  while (skipCount > 0)
    {
      if (curConnection == 0)
	return cfragCFMInternalErr;

      ret = cfm_fetch_connection_info (parser, curConnection, &connection);

      curConnection = connection.next;
      skipCount -= 1;
    }

  for (currIDSlot = 0; currIDSlot < requestedCount; currIDSlot += 1)
    {
      if (curConnection == 0)
	return cfragCFMInternalErr;

      ret = cfm_fetch_connection_info (parser, curConnection, &connection);

      connectionIDs_o[currIDSlot] = curConnection;
      curConnection = connection.next;
    }

  *totalCount_o = localTotal;
  return noErr;
}

long
cfm_parse_universe_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragUniverseInfo *info)
{
  if (parser->universe_container_offset + 12 > len) { return -1; }
  if (parser->universe_connection_offset + 12 > len) { return -1; }
  if (parser->universe_closure_offset + 12 > len) { return -1; }

  info->containers.head = bfd_getb32 (buf + parser->universe_container_offset);
  info->containers.tail = bfd_getb32 (buf + parser->universe_container_offset + 4);
  info->containers.length = bfd_getb32 (buf + parser->universe_container_offset + 8);
  info->connections.head = bfd_getb32 (buf + parser->universe_connection_offset);
  info->connections.tail = bfd_getb32 (buf + parser->universe_connection_offset + 4);
  info->connections.length = bfd_getb32 (buf + parser->universe_connection_offset + 8);
  info->closures.head = bfd_getb32 (buf + parser->universe_closure_offset);
  info->closures.tail = bfd_getb32 (buf + parser->universe_closure_offset + 4);
  info->closures.length = bfd_getb32 (buf + parser->universe_closure_offset + 8);

  return 0;
}

long
cfm_fetch_universe_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragUniverseInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_UNIVERSE_LENGTH];
  if (parser->universe_length > CFM_MAX_UNIVERSE_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->universe_length, &err);
  if (ret < 0) { return -1; }

  return cfm_parse_universe_info (parser, buf, parser->universe_length, info);
}

long
cfm_parse_container_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragContainerInfo *info)
{
  info->address = bfd_getb32 (buf + parser->container_address_offset);
  info->length = bfd_getb32 (buf + parser->container_length_offset);
  info->sectionCount = bfd_getb32 (buf + parser->container_section_count_offset);

  return 0;
}

long
cfm_fetch_container_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragContainerInfo *info)
{
  int ret, err;
  unsigned long name_length, name_addr;

  unsigned char buf[CFM_MAX_CONTAINER_LENGTH];
  if (parser->container_length > CFM_MAX_CONTAINER_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->container_length, &err);
  if (ret < 0) { return -1; }

  ret = cfm_parse_container_info (parser, buf, parser->container_length, info);
  if (ret < 0) { return -1; }

  name_length = CFContHashedStringLength (bfd_getb32 (buf + parser->container_fragment_name_offset));
  if (name_length > 63)
    return cfragFragmentCorruptErr;
  name_addr = bfd_getb32 (buf + parser->container_fragment_name_offset + 4);

  info->name[0] = name_length;
  
  ret = target_read_memory_partial (name_addr, &info->name[1], name_length, &err);
  if (ret < 0)
    return cfragFragmentCorruptErr;

  info->name[name_length + 1] = '\0';

  return 0;
}

long
cfm_parse_connection_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragConnectionInfo *info)
{
  if (parser->connection_next_offset + 4 > len) { return -1; }
  if (parser->connection_container_offset + 4 > len) { return -1; }

  info->next = bfd_getb32 (buf + parser->connection_next_offset);
  info->container = bfd_getb32 (buf + parser->connection_container_offset);

  return 0;
}

long
cfm_fetch_connection_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragConnectionInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_CONNECTION_LENGTH];
  if (parser->connection_length > CFM_MAX_CONNECTION_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->connection_length, &err);
  if (ret < 0) { return -1; }

  return cfm_parse_connection_info (parser, buf, parser->connection_length, info);
}

long
cfm_parse_section_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragSectionInfo *info)
{
  if (parser->section_total_length_offset + 4 > len) { return -1; }

  info->length = bfd_getb32 (buf + parser->section_total_length_offset);

  return 0;
}

long
cfm_parse_instance_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragInstanceInfo *info)
{
  if (parser->instance_address_offset + 4 > len) { return -1; }

  info->address = bfd_getb32 (buf + parser->instance_address_offset);

  return 0;
}

long
cfm_fetch_connection_section_info
(struct cfm_parser *parser, CORE_ADDR addr, unsigned long sectionIndex, NCFragSectionInfo *section, NCFragInstanceInfo *instance)
{
  int ret, err;
  unsigned long offset;

  NCFragConnectionInfo connection;
  NCFragContainerInfo container;
  unsigned char section_buf[CFM_MAX_SECTION_LENGTH];
  unsigned char instance_buf[CFM_MAX_INSTANCE_LENGTH];
  unsigned long instance_ptr;

  ret = cfm_fetch_connection_info (parser, addr, &connection);
  if (ret < 0)
    return cfragCFMInternalErr;

  ret = cfm_fetch_container_info (parser, connection.container, &container);
  if (ret < 0)
    return cfragCFMInternalErr;
  
  if (sectionIndex >= container.sectionCount)
    return cfragNoSectionErr;

  offset = (connection.container + parser->container_length - (2 * parser->section_length) + (sectionIndex * parser->section_length));
  
  ret = target_read_memory_partial (offset, section_buf, parser->section_length, &err);
  if (ret < 0)
    return cfragCFMInternalErr;
  
  offset = (addr + parser->connection_length - (2 * sizeof (unsigned long)) + (sectionIndex * sizeof (unsigned long)));
  
  ret = target_read_memory_partial (offset, (unsigned char *) &instance_ptr, sizeof (unsigned long), &err);
  if (ret < 0)
	return cfragCFMInternalErr;
  if (instance_ptr == 0)
    return cfragNoSectionErr;
  
  ret = target_read_memory_partial (instance_ptr, instance_buf, parser->instance_length, &err);
  if (ret < 0)
    return cfragCFMInternalErr;
  
  ret = cfm_parse_section_info (parser, section_buf, parser->section_length, section);
  if (ret < 0)
    return ret;

  ret = cfm_parse_instance_info (parser, instance_buf, parser->instance_length, instance);
  if (ret < 0)
    return ret;

  return noErr;
}
