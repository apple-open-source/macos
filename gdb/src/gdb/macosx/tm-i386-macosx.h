#ifndef __GDB_TM_i386_MACOSX_H__
#define __GDB_TM_i386_MACOSX_H__


int
i386_fast_show_stack (unsigned int count_limit, unsigned int print_limit,
                     unsigned int *count,
                     void (print_fun) (struct ui_out * uiout, int frame_num,
                                       CORE_ADDR pc, CORE_ADDR fp));
#define FAST_COUNT_STACK_DEPTH(count_limit, print_limit, count, print_fun) \
  (i386_fast_show_stack (count_limit, print_limit, count, print_fun))

char *i386_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                                     int exception_type);
#define THROW_CATCH_FIND_TYPEINFO(curr_frame, exception_type) \
  (i386_throw_catch_find_typeinfo (curr_frame, exception_type))


#endif /* __GDB_TM_i386_MACOSX_H__ */
