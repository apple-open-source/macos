#ifndef _NEXTSTEP_TDEP_H_
#define _NEXTSTEP_TDEP_H_

struct internal_nlist;
struct external_nlist;

void next_internalize_symbol 
PARAMS ((struct internal_nlist *in, struct external_nlist *ext, bfd *abfd));

const char *dyld_symbol_stub_function_name (CORE_ADDR pc);
CORE_ADDR dyld_symbol_stub_function_address (CORE_ADDR pc, const char **name);

#endif /* _NEXTSTEP_TDEP_H_ */
