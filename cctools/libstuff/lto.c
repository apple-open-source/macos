#ifdef LTO_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <libc.h>
#include <sys/file.h>
#include <dlfcn.h>
#include <llvm-c/lto.h>
#include "stuff/ofile.h"
#include "stuff/lto.h"
#include "stuff/allocate.h"
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>

static int set_lto_cputype(
    struct ofile *ofile,
    char *target_triple);

static int tried_to_load_lto = 0;
static void *lto_handle = NULL;
static lto_module_t (*lto_create)(const void* mem, size_t length) = NULL;
static void (*lto_dispose)(void *mod) = NULL;
static char * (*lto_get_target)(void *mod) = NULL;
static uint32_t (*lto_get_num_symbols)(void *mod) = NULL;
static lto_symbol_attributes (*lto_get_sym_attr)(void *mod, uint32_t n) = NULL;
static char * (*lto_get_sym_name)(void *mod, uint32_t n) = NULL;

/*
 * is_llvm_bitcode() is passed an ofile struct pointer and a pointer and size
 * of some part of the ofile.  If it is an llvm bit code it returns 1 and
 * stores the lto module in the ofile lto field, and also sets the lto_cputype
 * and lto_cpusubtype fields.  If not it returns 0 and sets those fields to 0.
 */
__private_extern__
int
is_llvm_bitcode(
struct ofile *ofile,
char *addr,
size_t size)
{
   size_t bufsize;
   char *p, *prefix, *lto_path, buf[MAXPATHLEN], resolved_name[PATH_MAX];
   int i;

	/*
	 * If this is an llvm bitcode file these will be filled in.
	 */
	ofile->lto = NULL;
	ofile->lto_cputype = 0;
	ofile->lto_cpusubtype = 0;
	/*
	 * The caller needs to be the one to set ofile->file_type to
	 * OFILE_LLVM_BITCODE or not.  As the addr and size of of this
	 *"llvm bitcode file" could be in an archive or fat file.
	 */

	if(tried_to_load_lto == 0){
	    tried_to_load_lto = 1;
	    /*
	     * The design is rather lame and inelegant: "llvm support is only
	     * for stuff in /Developer and not the tools installed in /".
	     * Which would mean tools like libtool(1) run from /usr/bin would
	     * not work with lto, and work differently if the same binary was
	     * installed in /Developer/usr/bin . And if the tools were
	     * installed in some other location besides /Developer, like
	     * /Developer/Platforms/...  that would also not work.
	     *
	     * So instead construct the prefix to this executable assuming it
	     * is in a bin directory relative to a lib directory of the matching
	     * lto library and first try to load that.  If not then fall back to
	     * trying "/Developer/usr/lib/libLTO.dylib".
	     */
	    bufsize = MAXPATHLEN;
	    p = buf;
	    i = _NSGetExecutablePath(p, &bufsize);
	    if(i == -1){
		p = allocate(bufsize);
		_NSGetExecutablePath(p, &bufsize);
	    }
	    prefix = realpath(p, resolved_name);
	    p = rindex(prefix, '/');
	    if(p != NULL)
		p[1] = '\0';
	    lto_path = makestr(prefix, "../lib/libLTO.dylib", NULL);

	    lto_handle = dlopen(lto_path, RTLD_NOW);
	    if(lto_handle == NULL){
		free(lto_path);
		lto_path = NULL;
		lto_handle = dlopen("/Developer/usr/lib/libLTO.dylib",
				    RTLD_NOW);
	    }
	    if(lto_handle == NULL)
		return(0);

	    lto_create = dlsym(lto_handle, "lto_module_create_from_memory");
	    lto_dispose = dlsym(lto_handle, "lto_module_dispose");
	    lto_get_target = dlsym(lto_handle, "lto_module_get_target_triple");
	    lto_get_num_symbols = dlsym(lto_handle,
					"lto_module_get_num_symbols");
	    lto_get_sym_attr = dlsym(lto_handle,
				     "lto_module_get_symbol_attribute");
	    lto_get_sym_name = dlsym(lto_handle, "lto_module_get_symbol_name");

	    if(lto_create == NULL ||
	       lto_dispose == NULL ||
	       lto_get_target == NULL ||
	       lto_get_num_symbols == NULL ||
	       lto_get_sym_attr == NULL ||
	       lto_get_sym_name == NULL){
		dlclose(lto_handle);
		if(lto_path != NULL)
		    free(lto_path);
		return(0);
	    }
	}
	if(lto_handle == NULL)
	    return(0);
	    
	ofile->lto = lto_create(addr, size);
	if(ofile->lto == NULL)
	    return(0);

	/*
	 * It is possible for new targets to be added to lto that are not yet
	 * known to this code.  So we will try to get lucky and let them pass
	 * through with the lto_cputype set to 0. This should work for things
	 * like libtool(1) as long as we don't get two different unknown
	 * targets.  But we'll hope that just doesn't happen.
	 */
	set_lto_cputype(ofile, lto_get_target(ofile->lto));

	return(1);
}

/*
 * set_lto_cputype() takes an ofile pointer and the target_triple string
 * returned from lto_module_get_target_triple() and sets the fields lto_cputype
 * and lto_cpusubtype in the ofile pointer.  If it can parse and knows the
 * strings values it returns 1 and the fields are set.  Otherwise it returns 0
 * and the fields are not set.
 */
static
int
set_lto_cputype(
struct ofile *ofile,
char *target_triple)
{
    char *p;
    size_t n;

	if(target_triple == NULL)
	    return(0);
	p = index(target_triple, '-');
	if(p == NULL)
	    return(0);
	n = p - target_triple;
	if(strncmp(target_triple, "i686", n) == 0 ||
	   strncmp(target_triple, "i386", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_I386;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_I386_ALL;
	}
	else if(strncmp(target_triple, "x86_64", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_X86_64;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_X86_64_ALL;
	}
	else if(strncmp(target_triple, "powerpc", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_POWERPC;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_POWERPC_ALL;
	}
	else if(strncmp(target_triple, "powerpc64", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_POWERPC64;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_POWERPC_ALL;
	}
	else if(strncmp(target_triple, "arm", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_ARM;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_ARM_V4T;
	}
	else if(strncmp(target_triple, "armv5", n) == 0 ||
	        strncmp(target_triple, "armv5e", n) == 0 ||
		strncmp(target_triple, "thumbv5", n) == 0 ||
		strncmp(target_triple, "thumbv5e", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_ARM;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_ARM_V5TEJ;
	}
	else if(strncmp(target_triple, "armv6", n) == 0 ||
	        strncmp(target_triple, "thumbv6", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_ARM;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_ARM_V6;
	}
	else if(strncmp(target_triple, "armv7", n) == 0 ||
	        strncmp(target_triple, "thumbv7", n) == 0){
	    ofile->lto_cputype = CPU_TYPE_ARM;
	    ofile->lto_cpusubtype = CPU_SUBTYPE_ARM_V7;
	}
	else{
	    return(0);
	}
	return(1);
}

/*
 * lto_get_nsyms() returns the number of symbol in the lto module passed to it.
 */
__private_extern__
uint32_t
lto_get_nsyms(
void *mod)
{
	return(lto_get_num_symbols(mod));
}

/*
 * lto_get_nsyms() is passed an lto module and a symbol index in that module,
 * and returns 1 if the symbol should be part of the archive table of contents
 * or 0 if not.  The parameter commons_in_toc is non-zero if tentative
 * defintions are to be included in the table of contents and zero if not.
 */
__private_extern__
int
lto_toc_symbol(
void *mod,
uint32_t symbol_index,
int commons_in_toc)
{
    lto_symbol_attributes attr;

	attr = lto_get_sym_attr(mod, symbol_index);
	if((attr & LTO_SYMBOL_SCOPE_MASK) == LTO_SYMBOL_SCOPE_INTERNAL)
	   return(0);

	if((attr & LTO_SYMBOL_DEFINITION_MASK) ==
		LTO_SYMBOL_DEFINITION_REGULAR ||
	   (attr & LTO_SYMBOL_DEFINITION_MASK) ==
		LTO_SYMBOL_DEFINITION_WEAK)
	    return(1);
	if((attr & LTO_SYMBOL_DEFINITION_MASK) ==
		LTO_SYMBOL_DEFINITION_TENTATIVE &&
	   commons_in_toc)
	    return(1);
	return(0);
}

/*
 * lto_get_nlist_64() is used by nm(1) to fake up an nlist structure to used
 * for printing.  The "object" is assumed to have three sections, code: data,
 * and rodata.
 */
__private_extern__
void
lto_get_nlist_64(
struct nlist_64 *nl,
void *mod,
uint32_t symbol_index)
{
    lto_symbol_attributes attr;

	memset(nl, '\0', sizeof(struct nlist_64));
	attr = lto_get_sym_attr(mod, symbol_index);

	switch(attr & LTO_SYMBOL_SCOPE_MASK){
	case LTO_SYMBOL_SCOPE_INTERNAL:
	    break;
	case LTO_SYMBOL_SCOPE_HIDDEN:
	    nl->n_type |= N_EXT;
	    nl->n_type |= N_PEXT;
	    break;
	case LTO_SYMBOL_SCOPE_DEFAULT:
	    nl->n_type |= N_EXT;
	}

	if((attr & LTO_SYMBOL_DEFINITION_MASK) == LTO_SYMBOL_DEFINITION_WEAK)
	    nl->n_desc |= N_WEAK_DEF;

	if((attr & LTO_SYMBOL_DEFINITION_MASK) ==
	   LTO_SYMBOL_DEFINITION_TENTATIVE){
	    nl->n_type |= N_EXT;
	    nl->n_type |= N_UNDF;
	    nl->n_value = 1; /* no interface to get the size */
	}
	if((attr & LTO_SYMBOL_DEFINITION_MASK) ==
	   LTO_SYMBOL_DEFINITION_UNDEFINED){
	    nl->n_type |= N_EXT;
	    nl->n_type |= N_UNDF;
	}
	else
	    switch(attr & LTO_SYMBOL_PERMISSIONS_MASK){
	    case LTO_SYMBOL_PERMISSIONS_CODE:
		nl->n_sect = 1;
		nl->n_type |= N_SECT;
		break;
	    case LTO_SYMBOL_PERMISSIONS_DATA:
		nl->n_sect = 2;
		nl->n_type |= N_SECT;
		break;
	    case LTO_SYMBOL_PERMISSIONS_RODATA:
		nl->n_sect = 3;
		nl->n_type |= N_SECT;
		break;
	}
}

/*
 * lto_symbol_name() is passed an lto module and a symbol index in that module,
 * and returns the name of that symbol.
 */
__private_extern__
char *
lto_symbol_name(
void *mod,
uint32_t symbol_index)
{
	return(lto_get_sym_name(mod, symbol_index));
}

__private_extern__
void
lto_free(
void *mod)
{
	lto_dispose(mod);
}

#endif /* LTO_SUPPORT */
