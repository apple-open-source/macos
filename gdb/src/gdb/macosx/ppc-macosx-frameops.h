#ifndef __GDB_PPC_MACOSX_FRAMEOPS_H__
#define __GDB_PPC_MACOSX_FRAMEOPS_H__

#include "defs.h"

struct frame_info;

void ppc_frame_cache_saved_regs (struct frame_info *frame);

void ppc_frame_saved_regs (struct frame_info *frame, CORE_ADDR *regs);

#endif /* __GDB_PPC_MACOSX_FRAMEOPS_H__ */
