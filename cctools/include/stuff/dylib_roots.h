extern char * get_symfile_for_dylib(
    char *install_name,
    char *release_name,
    enum bool *found_project,
    enum bool disablewarnings,
    enum bool no_error_if_missing);

extern char * get_dstfile_for_dylib(
    char *install_name,
    char *release_name,
    enum bool *found_project,
    enum bool disablewarnings,
    enum bool no_error_if_missing);

extern char * find_dylib_in_root(
    char *install_name,
    const char *root);
