#ifndef __GDB_TM_PPC_MACOSX_H__
#define __GDB_TM_PPC_MACOSX_H__

#ifndef GDB_MULTI_ARCH
#define GDB_MULTI_ARCH 1
#endif

struct frame_info;

extern CORE_ADDR ppc_macosx_skip_trampoline_code (CORE_ADDR pc);
#define	SKIP_TRAMPOLINE_CODE(pc) ppc_macosx_skip_trampoline_code (pc)

extern CORE_ADDR ppc_macosx_dynamic_trampoline_nextpc (CORE_ADDR pc);
#define DYNAMIC_TRAMPOLINE_NEXTPC(pc) ppc_macosx_dynamic_trampoline_nextpc (pc)

extern int ppc_macosx_in_solib_dynsym_resolve_code (CORE_ADDR pc);
#define IN_SOLIB_DYNSYM_RESOLVE_CODE(pc) ppc_macosx_in_solib_dynsym_resolve_code (pc)

extern int ppc_macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name);
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) ppc_macosx_in_solib_call_trampoline (pc, name)

extern int ppc_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name);
#define IN_SOLIB_RETURN_TRAMPOLINE(pc,name) ppc_macosx_in_solib_return_trampoline (pc, name)

void ppc_init_frame_pc_first PARAMS ((int fromleaf, struct frame_info *prev));
#define INIT_FRAME_PC_FIRST(FROMLEAF, PREV) ppc_init_frame_pc_first ((FROMLEAF), (PREV))

void ppc_init_frame_pc PARAMS ((int fromleaf, struct frame_info *prev));
#define INIT_FRAME_PC(FROMLEAF, PREV) ppc_init_frame_pc (FROMLEAF, PREV)

void ppc_stack_alloc PARAMS ((CORE_ADDR *sp, CORE_ADDR *start, size_t argsize, size_t len));
#define STACK_ALLOC(SP, NSP, LEN) ppc_stack_alloc (&(SP), &(NSP), 0, (LEN))

CORE_ADDR ppc_convert_from_func_ptr_addr PARAMS ((CORE_ADDR addr));
#define CONVERT_FROM_FUNC_PTR_ADDR(ADDR) ppc_convert_from_func_ptr_addr (ADDR)

int ppc_fast_show_stack_helper (int show_frames, int show_names, int limit, int *count);
#define FAST_COUNT_STACK_DEPTH(count) \
    (ppc_fast_show_stack_helper(0, 0, 0, count))

#endif /* __GDB_TM_PPC_MACOSX_H__ */
