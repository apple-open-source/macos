#ifndef _STUFF_LTO_H_
#define _STUFF_LTO_H_

#ifdef LTO_SUPPORT

#include "stuff/ofile.h"

__private_extern__ int is_llvm_bitcode(
    struct ofile *ofile,
    char *addr,
    size_t size);

__private_extern__ uint32_t lto_get_nsyms(
    void *mod);

__private_extern__ int lto_toc_symbol(
    void *mod,
    uint32_t symbol_index,
    int commons_in_toc);

__private_extern__ void lto_get_nlist_64(
    struct nlist_64 *nl,
    void *mod,
    uint32_t symbol_index);

__private_extern__ char * lto_symbol_name(
    void *mod,
    uint32_t symbol_index);

__private_extern__ void lto_free(
    void *mod);

#endif /* LTO_SUPPORT */

#endif /* _STUFF_LTO_H_ */
