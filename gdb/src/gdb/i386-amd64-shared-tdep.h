#ifndef I386_AMD64_SHARED_TDEP_H
#define I386_AMD64_SHARED_TDEP_H

#include "i386-tdep.h"

int i386_length_of_this_instruction (CORE_ADDR memaddr);

int i386_mov_esp_ebp_pattern_p (CORE_ADDR memaddr);
int i386_push_ebp_pattern_p (CORE_ADDR memaddr);
int i386_ret_pattern_p (CORE_ADDR memaddr);
int i386_picbase_setup_pattern_p (CORE_ADDR memaddr, enum i386_regnum *regnum);
struct type * build_builtin_type_vec128i_big (void);


#endif /* I386_AMD64_SHARED_TDEP_H */
