/*
 * The table of known dynamic library names and addresses they are linked at.
 * This is loaded from a -dylib_table option or from the default file:
 * ~rc/Data/DylibTable .
 */
struct dylib_table {
    unsigned long seg1addr;
    char *name;
};

extern struct dylib_table * parse_dylib_table(
    char *file_name,
    char *flag,
    char *argument);

extern struct dylib_table * parse_default_dylib_table(
    char **file_name);

extern struct dylib_table *search_dylib_table(
    struct dylib_table *dylib_table,
    char *name);

extern char * guess_dylib_install_name(
    char *name);
