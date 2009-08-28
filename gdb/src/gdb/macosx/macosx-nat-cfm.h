#ifndef __GDB_MACOSX_NAT_CFM_H__
#define __GDB_MACOSX_NAT_CFM_H__
#if WITH_CFM

#include <mach/mach.h>

struct dyld_objfile_info;

struct NCFragListInfo
{
  unsigned long head;
  unsigned long tail;
  unsigned long length;
};
typedef struct NCFragListInfo NCFragListInfo;

struct NCFragUniverseInfo
{
  NCFragListInfo containers;
  NCFragListInfo connections;
  NCFragListInfo closures;
};
typedef struct NCFragUniverseInfo NCFragUniverseInfo;

struct NCFragConnectionInfo
{
  unsigned long container;
  unsigned long next;
};
typedef struct NCFragConnectionInfo NCFragConnectionInfo;

struct NCFragContainerInfo
{
  unsigned long address;
  unsigned long length;
  unsigned long next;
  unsigned long sectionCount;
  char name[66];
};
typedef struct NCFragContainerInfo NCFragContainerInfo;

struct NCFragSectionInfo
{
  unsigned long length;
};
typedef struct NCFragSectionInfo NCFragSectionInfo;

struct NCFragInstanceInfo
{
  unsigned long address;
};
typedef struct NCFragInstanceInfo NCFragInstanceInfo;

struct cfm_parser
{
  unsigned int version;
  size_t universe_length;
  size_t universe_container_offset;
  size_t universe_connection_offset;
  size_t universe_closure_offset;
  size_t connection_length;
  size_t connection_next_offset;
  size_t connection_container_offset;
  size_t container_length;
  size_t container_address_offset;
  size_t container_length_offset;
  size_t container_fragment_name_offset;
  size_t container_section_count_offset;
  size_t container_sections_offset;
  size_t section_length;
  size_t section_total_length_offset;
  size_t instance_length;
  size_t instance_address_offset;
};

extern long
  cfm_parse_universe_info
  (struct cfm_parser *parser, unsigned char *buf, size_t len,
   NCFragUniverseInfo *info);

extern long
  cfm_fetch_universe_info
  (struct cfm_parser *parser, CORE_ADDR addr, NCFragUniverseInfo *info);

extern long
  cfm_parse_connection_info
  (struct cfm_parser *parser, unsigned char *buf, size_t len,
   NCFragConnectionInfo *info);

extern long
  cfm_fetch_connection_info
  (struct cfm_parser *parser, CORE_ADDR addr, NCFragConnectionInfo *info);

extern long
  cfm_parse_container_info
  (struct cfm_parser *parser, unsigned char *buf, size_t len,
   NCFragContainerInfo *info);

extern long
  cfm_fetch_container_info
  (struct cfm_parser *parser, CORE_ADDR addr, NCFragContainerInfo *info);

extern long
  cfm_fetch_context_containers
  (struct cfm_parser *parser, CORE_ADDR addr,
   unsigned long requestedCount, unsigned long skipCount,
   unsigned long *totalCount_o, unsigned long *containerIDs_o);

extern long
  cfm_fetch_container_section_info
  (struct cfm_parser *parser, CORE_ADDR addr, unsigned long sectionIndex,
   NCFragSectionInfo *section);

long cfm_update (task_t task, struct dyld_objfile_info *info);
void cfm_init (void);


#endif /* WITH_CFM */
#endif /* __GDB_MACOSX_NAT_CFM_H__ */
