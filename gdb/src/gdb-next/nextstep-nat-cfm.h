#ifndef _NEXTSTEP_NAT_CFM_H_
#define _NEXTSTEP_NAT_CFM_H_

#pragma options align=mac68k
#include "CodeFragmentInfoPriv.h"
#pragma options align=reset

struct next_cfm_info;

extern void cfm_info_init (struct next_cfm_info *cfm_info);
extern void next_init_cfm_info_api (struct next_inferior_status *status);
extern void next_handle_cfm_event (struct next_inferior_status *status, unsigned char *buf);

typedef struct CFMSection CFMSection;
typedef struct CFMContainer CFMContainer;
typedef struct CFMConnection CFMConnection;
typedef struct CFMClosure CFMClosure;

struct CFMSection
{
  CFMSection *mNext;
  CFragSectionInfo mSection;
  CFMContainer *mContainer;
};

struct CFMContainer
{
  CFMContainer *mNext;
  CFMSection *mSections;
  CFragContainerInfo mContainer;
};

struct CFMConnection
{
  CFMConnection *mNext;
  CFMContainer *mContainer;
  CFragConnectionInfo mConnection;
};

struct CFMClosure
{
  CFMClosure *mNext;
  CFMConnection *mConnections;
  CFragClosureInfo mClosure;
};

extern CFMContainer *CFM_FindContainerByName (char *name, int length);
extern CFMSection *CFM_FindSection (CFMContainer *container, CFContMemoryAccess accessType);
extern CFMSection *CFM_FindContainingSection (CORE_ADDR address);

extern void enable_breakpoints_in_containers (void);

extern CFMClosure *gClosures;

#endif /* _NEXTSTEP_NAT_CFM_H_ */
