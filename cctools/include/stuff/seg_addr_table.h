#include "stuff/bool.h"

/*
 * These are the tokens for the "install name" for the next addresses to use
 * when updating the table.  And also the token for fixed regions.
 */
#define NEXT_FLAT_ADDRESS_TO_ASSIGN  "<<< Next flat address to assign >>>"
#define NEXT_SPLIT_ADDRESS_TO_ASSIGN "<<< Next split address to assign >>>"
#define FIXED_ADDRESS_AND_SIZE "<<< Fixed address and size not to assign >>>"

/*
 * The table of dynamic library install names and their addresses they are
 * linked at.  This is used with the -seg_addr_table option from the static
 * link editor, ld(1), and the seg_addr_table(1) program.
 */
struct seg_addr_table {
    char *install_name;
    enum bool split;
    unsigned long seg1addr;
    unsigned long segs_read_only_addr;
    unsigned long segs_read_write_addr;
    unsigned long line;
};

extern struct seg_addr_table *parse_default_seg_addr_table(
    char **seg_addr_table_name,
    unsigned long *table_size);

extern struct seg_addr_table * parse_seg_addr_table(
    char *file_name,
    char *flag,
    char *argument,
    unsigned long *table_size);

extern struct seg_addr_table * search_seg_addr_table(
    struct seg_addr_table *seg_addr_table,
    char *install_name);

extern void process_seg_addr_table(
    char *file_name,
    FILE *out_fp,
    char *comment_prefix,
    void (*processor)(struct seg_addr_table *entry, FILE *out_fp, void *cookie),
    void *cookie);
