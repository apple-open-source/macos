#ifndef __DGRAPH_H__
#define __DGRAPH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct dgraph_entry_t {

    char is_kernel_component; // means that filename is a CFBundleIdentifier!!!

    // What we have to start from
    char * filename;

   /* If is_kernel_component is true then the do_load field is cleared and
    * the kmod_id field gets set.
    */

    // Immediate dependencies of this entry
    unsigned int dependencies_capacity;
    unsigned int num_dependencies;
    struct dgraph_entry_t ** dependencies;

    // These are filled in when the entry is created, and must match
    // what is found in the kmod binary itself.
    char * expected_kmod_name;
    UInt32 expected_kmod_vers;  // parsed, or course

    kmod_info_t * kmod_info;    // from disk image, *not* link/loaded data

    // For tracking already-loaded kmods or for doing symbol generation only
    int do_load;   // actually loading
    vm_address_t loaded_address;  // address loaded at or being faked at for symbol generation

    // for loading into kernel
    vm_address_t  kernel_alloc_address;
    unsigned long kernel_alloc_size;
    vm_address_t  kernel_load_address;
    unsigned long kernel_load_size;
    unsigned long kernel_hdr_size;
    unsigned long kernel_hdr_pad;
    int need_cleanup;  // true if load failed with kernel memory allocated
    kmod_t kmod_id;    // the id assigned by the kernel to a loaded kmod

} dgraph_entry_t;

typedef struct {
    unsigned int      capacity;
    unsigned int      length;
    dgraph_entry_t ** graph;
    dgraph_entry_t ** load_order;
    dgraph_entry_t  * root;
} dgraph_t;

typedef enum {
    dgraph_error = -1,
    dgraph_invalid = 0,
    dgraph_valid = 1
} dgraph_error_t;


dgraph_error_t dgraph_init(dgraph_t * dgraph);

/**********
 * Initialize a dependency graph passed in. Returns nonzero on success, zero
 * on failure.
 *
 *     dependency_graph: a pointer to the dgraph to initialize.
 *     argc: the number of arguments in argv
 *     argv: an array of strings defining the dependency graph. This is a
 *         series of dependency lists, delimited by "-d" (except before
 *         the first list, naturally). Each list has as its first entry
 *         the dependent, followed by any number of DIRECT dependencies.
 *         The lists may be given in any order, but the first item in each
 *         list must be the dependent. Also, there can only be one root
 *         item (an item with no dependents upon it), and it must not be
 *         a kernel component.
 */
dgraph_error_t dgraph_init_with_arglist(
    dgraph_t * dgraph,
    int expect_addresses,
    const char * dependency_delimiter,
    const char * kernel_dependency_delimiter,
    int argc,
    char * argv[]);

void dgraph_free(
    dgraph_t * dgraph,
    int free_graph);

dgraph_entry_t * dgraph_find_root(dgraph_t * dgraph);

int dgraph_establish_load_order(dgraph_t * dgraph);

void dgraph_verify(char * tag, dgraph_t * dgraph);
void dgraph_print(dgraph_t * dgraph);
void dgraph_log(dgraph_t * depgraph);


/*****
 * These functions are useful for hand-building a dgraph.
 */
dgraph_entry_t * dgraph_find_dependent(dgraph_t * dgraph, const char * filename);

dgraph_entry_t * dgraph_add_dependent(
    dgraph_t * dgraph,
    const char * filename,
    const char * expected_kmod_name,
    UInt32 expected_kmod_vers,
    vm_address_t load_address,
    char is_kernel_component);

int dgraph_add_dependency(
    dgraph_t * dgraph,
    dgraph_entry_t * current_dependent,
    const char * filename,
    const char * expected_kmod_name,
    UInt32 expected_kmod_vers,
    vm_address_t load_address,
    char is_kernel_component);

#ifdef __cplusplus
}
#endif

#endif __DGRAPH_H__
