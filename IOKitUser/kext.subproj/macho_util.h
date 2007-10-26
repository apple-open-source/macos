#ifndef __MACHO_UTIL_H__
#define __MACHO_UTIL_H__

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

/*******************************************************************************
********************************************************************************
**                   DO NOT USE THIS API. IT IS VOLATILE.                     **
********************************************************************************
*******************************************************************************/

/*! @header macho_util
Handy functions for working with Mach-O files. Very rudimentary, use at your
own risk.
*/

/*!
 * @define  MAGIC32
 * @abstract  Get the 32-bit magic number from a file data pointer.
 * @param ptr A pointer to a file whose magic number you want to get.
 * @result    Returns an unsigned 32-bit integer containing
 *            the first four bytes of the file data.
 */
#define MAGIC32(ptr)          (*((uint32_t *)(ptr)))

#define ISMACHO(magic)        (((magic) == MH_MAGIC) || \
                               ((magic) == MH_CIGAM) || \
                               ((magic) == MH_MAGIC_64) || \
                               ((magic) == MH_CIGAM_64))
#define ISSWAPPEDMACHO(magic)  (((magic) == MH_CIGAM) || \
                               ((magic) == MH_CIGAM_64))

/*******************************************************************************
*
*******************************************************************************/
#define CondSwapInt32(flag, value)  ((flag) ? OSSwapInt32((value)) : \
                                    (uint32_t)(value))
/*!
 * @enum macho_seek_result
 * @abstract Results of a lookup within a Mach-O file.
 * @discussion This enum lists the possible return values for a Mach-O lookup.
 * @constant macho_seek_result_error Invalid arguments or bad Mach-O file.
 * @constant macho_seek_result_found The requested item was found.
 * @constant macho_seek_result_not_found The requested item was not found.
 *           For functions called repeatedly (such as the macho_lc_callback),
 *           this means the item may be found on a subsequent invocation.
 * @constant macho_seek_result_stop The requested item will not be found.
 *           This is only returned by functions that may be called repeatedly,
 *           when they can determine conclusively that the requested item does not exist.
 */
typedef enum {
    macho_seek_result_error = -1,
    macho_seek_result_found = 0,
    macho_seek_result_found_no_value = 1,
    macho_seek_result_not_found = 2,
    macho_seek_result_stop = 3,
} macho_seek_result;

/*!
 * @function macho_find_symbol
 * @abstract Finds symbol data in a mapped Mach-O file.
 * @discussion
 *        The macho_find_symbol function searches a memory-mapped Mach-O file
 *        for a given symbol,
 *        indirectly returning the address within that mapped file
 *        containing the data for that symbol. Only symbols of type
 *        N_SECT, N_UNDF, and N_ABS are currently supported, and for N_ABS
 *        the result will be macho_seek_result_found_no_value.
 * @param mach_header A pointer to the beginning of the mapped Mach-O file.
 * @param file_end A pointer to the end of the mapped Mach-O file.
 * @param symbol_entry The nlist entry for the symbol. It is only valid while
 *        the macho_iterator exists.
 * @param symbol_name The name of the symbol to find.
 * @param symbol_address The address of a pointer that will be filled
 *        with the location of the symbol's data in the mapped file,
 *        if it is found.
 * @result Returns macho_seek_result_found if the symbol is found,
 * macho_seek_result_not_found if the symbol is not defined in the given file,
 * or macho_seek_result_error if an error occurs.
 */
macho_seek_result macho_find_symbol(
    const void * file_start,
    const void * file_end,
    const char * symbol_name,
    const struct nlist ** symbol_entry,
    const void ** mapped_file_address);

/*!
 * @function macho_find_symtab
 * @abstract Finds a mapped Mach-O file's symbol table.
 * @discussion
 *        The macho_find_symtab function locates the symbol table of a Mach-O
 *        file. Only the LC_SYMTAB load command is located, not the LC_DSYMTAB.
 * @param mach_header A pointer to the beginning of the mapped Mach-O file.
 * @param file_end A pointer to the end of the mapped Mach-O file.
 * @param symtab A pointer to the address of a symtab_command struct;
 *        if provided, this is filled with the address of the file's symbol table.
 * @result Returns macho_seek_result_found if the symbol table is found,
 * macho_seek_result_not_found if a symbol table is not defined in the given file,
 * or macho_seek_result_error if an error occurs.
 */
macho_seek_result macho_find_symtab(
    const void * file_start,
    const void * file_end,
    struct symtab_command ** symtab);

/*!
 * @function macho_find_section_numbered
 * @abstract Finds an ordinal section in a Mach-O file.
 * @discussion
 *        The macho_find_section_numbered function locates an ordinally-numbered
 *        section within a Mach-O file, which is needed for calculating proper
 *        offsets and addresses of such things as nlist entries.
 *        Sections are numbered in a Mach-O file starting with 1, since zero
 *        is reserved to mean "no section".
 *        Since you will likely be passing a section number directly from other
 *        structure within the same Mach-O file, this should not be a problem.
 * @param mach_header A pointer to the beginning of the mapped Mach-O file.
 * @param file_end A pointer to the end of the mapped Mach-O file.
 * @param sect_num The ordinal number of the section to get, starting with 1.
 * @result Returns a pointer to the requested section struct if it exists;
 *         otherwise returns NULL.
 */
struct section * macho_find_section_numbered(
    const void * file_start,
    const void * file_end,
    uint8_t sect_num);

/*!
 * @typedef macho_lc_callback
 * @abstract The callback function used when scanning Mach-O load commands
 *           with macho_scan_load_commands.
 * @discussion macho_lc_callback defines the callback function
 * invoked by macho_scan_load_commands for each load command it encounters.
 * The scan function passes a pointer to a user data struct that you can use
 * for search parameters, running information, and search results.
 * @param load_command A pointer to the Mach-O load command being processed.
 * @param file_end A pointer to the end of the mapped Mach-O file,
 *        to be used for bounds checking.
 * @param swap A boolean flag indicating whether the Mach-O file's byte order
 *        is opposite the host's.
 *        If nonzero, the callback needs to swap all multibyte values
 *        read from the Mach-O file.
 * @param user_data A pointer to user-defined data that is passed unaltered
 *        across invocations of the callback function.
 * @result Returns macho_seek_result_found if the requested item is found,
 * macho_seek_result_not_found if is not found on this call,
 * macho_seek_result_stop if the function determined that it does not exist,
 * or macho_seek_result_error if an error occurs
 * (particularly if any data structure to be accessed
 * would run past the end of the file).
 */
typedef macho_seek_result (*macho_lc_callback)(
    struct load_command * load_command,
    const void * file_end,
    uint8_t swap,
    void * user_data
);

/*!
 * @function macho_scan_load_commands
 * @abstract Iterates over the load commands of a Mach-O file using a callback.
 * @discussion
 *        The macho_scan_load_commands iterates over the load commands within a
 *        Mach-O file, invoking a user-supplied callback until that callback
 *        returns a result indicating the scan should stop.
 * @param mach_header A pointer to the beginning of the mapped Mach-O file.
 * @param file_end A pointer to the end of the mapped Mach-O file.
 * @param lc_callback A function that is invoked on each load command
 *        of the Mach-O file until the callback function
 *        indicates the scan is finished.
 * @param user_data A pointer to user-defined data that is passed unaltered
 *        across invocations of the callback function.
 * @result Returns a pointer to the requested section struct if it exists;
 *         otherwise returns NULL.
 */
macho_seek_result macho_scan_load_commands(
    const void * file_start,
    const void * file_end,
    macho_lc_callback lc_callback,
    void * user_data);

#endif /* __MACHO_UTIL_H__ */
