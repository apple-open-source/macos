/*
 * For all APIs in this file the parameters program_name and error_message
 * are used the same.  For unrecoverable resource errors like being unable to
 * allocate memory each API prints a message to stderr precede with program_name
 * then calls exit(2) with the value EXIT_FAILURE.  If an API is unsuccessful
 * and if error_message pass to it is not NULL it is set to a malloc(3)'ed
 * buffer with a NULL terminated string with the error message.  For all APIs 
 * when they return they release all resources (memory, open file descriptors,
 * etc). 
 * 
 * The file_name parameter for these APIs may be of the form "foo(bar)" which is
 * NOT interpreted as an archive name and a member name in that archive.  As
 * these API deal with prebinding and prebound binaries ready for execution
 * can't be in archives.
 * 
 * If the executable_path parameter for these APIs is not NULL it is used for
 * any dependent library has a path that starts with "@executable_path". Then
 * "@executable_path" is replaced with executable_path. 
 * 
 * If the root_dir parameter is not NULL it is prepended to all the rooted
 * dependent library paths. 
 */

/*
 * dependent_libs() takes a file_name of a binary and returns a malloc(3)'ed
 * array of pointers (NULL terminated) to names (also malloc(3)'ed and '\0'
 * terminated names) of all the dependent libraries for that binary (not
 * recursive) for all of the architectures of that binary.  If successful
 * dependent_libs() returns a non NULL value (at minimum a pointer to one NULL
 * pointer). If unsuccessful dependent_libs() returns NULL.
 */ 
extern
char **
dependent_libs(
const char *file_name,
const char *program_name,
char **error_message);

/*
 * redo_prebinding() takes a file_name of a binary and redoes the prebinding on
 * it.  If output_file is not NULL the update file is written to output_file,
 * if not it is written to file_name.  If redo_prebinding() is successful it
 * returns 0 otherwise it returns 1.  If not all architectures can be updated
 * it is not successful and nothing is done.
 *
 * The not yet supported slide_to_address parameter should be passed a value of
 * zero. When supported a non-zero value will be the address a dynamic library
 * is to be relocated to as its preferred address.
 */
extern 
int
redo_prebinding(
const char *file_name,
const char *executable_path,
const char *root_dir,
const char *output_file,
const char *program_name,
char **error_message,
unsigned long slide_to_address /* not yet supported parameter */ );


/* return values for needs_redo_prebinding() */
enum needs_redo_prebinding_retval {
    PREBINDING_UPTODATE,  /* a binary who's prebinding is up todate */
    PREBINDING_OUTOFDATE, /* a binary who's prebinding is out of date */
    NOT_PREBOUND,	  /* a binary, but not built prebound */
    NOT_PREBINDABLE,	  /* not a binary, prebinding does not apply */
    PREBINDING_UNKNOWN	  /* a binary who's prebinding can't be determined
			     because it is malformed, a library it depends
			     on is missing, etc. */
};

/*
 * needs_redo_prebinding() takes a file_name and determines if it is a binary
 * and if its prebinding is uptodate.  It returns one of the return values
 * above depending on the state of the binary and libraries.  The value returned
 * is based on the first architecture for fat files.
 */
extern
enum needs_redo_prebinding_retval
needs_redo_prebinding(
const char *file_name,
const char *executable_path,
const char *root_dir,
const char *program_name,
char **error_message);
