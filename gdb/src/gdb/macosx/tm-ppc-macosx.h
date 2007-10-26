#ifndef __GDB_TM_PPC_MACOSX_H__
#define __GDB_TM_PPC_MACOSX_H__

#ifndef GDB_MULTI_ARCH
#define GDB_MULTI_ARCH 1
#endif

int
ppc_fast_show_stack (unsigned int count_limit, unsigned int print_limit,
                     unsigned int *count,
                     void (print_fun) (struct ui_out * uiout, int frame_num,
                                       CORE_ADDR pc, CORE_ADDR fp));
#define FAST_COUNT_STACK_DEPTH(count_limit, print_limit, count, print_fun) \
  (ppc_fast_show_stack (count_limit, print_limit, count, print_fun))


char *ppc_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                                     int exception_type);
#define THROW_CATCH_FIND_TYPEINFO(curr_frame, exception_type) \
  (ppc_throw_catch_find_typeinfo (curr_frame, exception_type))

#endif /* __GDB_TM_PPC_MACOSX_H__ */
