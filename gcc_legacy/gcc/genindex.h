
/* genindex keeps all the functions related to the indexing information
   generation. Indexing information is used by the IDEs like Project Builder.

   This file is used by both - preprocessor (cpp) and the compiler (cc1*)
*/

#ifndef __GCC_GENINDEX_H
#define __GCC_GENINDEX_H

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

/* Flag to indicate, if indexing information needs to be generated */
extern int flag_dump_symbols;
extern int flag_gen_index;
extern int flag_gen_index_original;
extern char *index_host_name;
extern char *index_port_string;
extern unsigned index_port_number;
extern int flag_check_indexed_header_list;
extern char *index_header_list_filename;
extern int c_language;
extern int index_socket_fd;
extern int index_buffer_count;


int connect_to_socket PROTO((char *, unsigned));
int read_indexed_header_list PROTO((void));
void write_indexed_header_list PROTO((void));
struct indexed_header * add_index_header_name PROTO((char *));
struct indexed_header * add_index_header PROTO((char *, time_t));
void free_indexed_header_list PROTO((void));
int process_header_indexing PROTO((char *, int));
/* Update header status.  */
void  update_header_status PROTO((struct indexed_header *, int, int));

void dump_symbol_info PROTO((char *, char *, int));
void flush_index_buffer PROTO ((void));

/* Enum to represent the status of the header. Only used when generating
   indexing information.  */
enum {
  PB_INDEX_UNKNOWN,     // Initial status
  PB_INDEX_SEEN,        // Header processing is started but not finished yet
  PB_INDEX_RECURSIVE,   // Header is seen more than once
  PB_INDEX_DONE         // Header is processed
};

/* Enum to represent the begin or end of header processing.  */
enum {
  PB_INDEX_BEGIN,
  PB_INDEX_END
};

#endif

