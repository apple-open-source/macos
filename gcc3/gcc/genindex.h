/* APPLE LOCAL file indexing */
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
extern int flag_debug_gen_index;
extern int flag_gen_index_original;
extern int flag_gen_index_header;
extern int flag_suppress_builtin_indexing;
extern int flag_check_indexed_header_list;
extern char *index_header_list_filename;
extern int index_buffer_count;

int connect_to_socket                    PARAMS ((char *, unsigned));
int read_indexed_header_list             PARAMS ((void));
void write_indexed_header_list           PARAMS ((void));
struct indexed_header * add_index_header_name PARAMS ((char *));
struct indexed_header * add_index_header PARAMS ((char *, time_t));
void free_indexed_header_list            PARAMS ((void));
int process_header_indexing              PARAMS ((char *, int));
/* Update header status.  */
void  update_header_status PARAMS ((struct indexed_header *, int, int));
void flush_index_buffer                  PARAMS  ((void));
void print_indexed_header_list           PARAMS ((void));
void gen_indexing_info                   PARAMS ((int, const char *, int));
void gen_indexing_header                 PARAMS ((char *));
void gen_indexing_footer                 PARAMS ((void));
void init_gen_indexing                   PARAMS ((void));
void finish_gen_indexing                 PARAMS ((void));
void set_index_lang                      PARAMS ((int));

/* Enum to represet the language of indexing symbol.  */
typedef enum index_language_kind {
 PB_INDEX_LANG_C,               /* C */
 PB_INDEX_LANG_CP,              /* C++ */
 PB_INDEX_LANG_OBJC,            /* Objective C */
 PB_INDEX_LANG_OBJCP,           /* Objective C++ */
 PB_INDEX_LANGUAGE_INVALID = 99 /* Invalid lang */
} index_language_kind;

/* Enum to represent the status of the header. Only used when generating
   indexing information.  */
enum {
  PB_INDEX_UNKNOWN,     /* Initial status */
  PB_INDEX_SEEN,        /* Header processing is started but not finished yet */
  PB_INDEX_RECURSIVE,   /* Header is seen more than once */
  PB_INDEX_DONE         /* Header is processed */
};

/* Enum to represent the begin or end of header processing.  */
enum {
  PB_INDEX_BEGIN,
  PB_INDEX_END
};

enum index_info {
  INDEX_ERROR,
  INDEX_ENUM,                   /* +vm */
  INDEX_FILE_BEGIN,             /* +Fm */
  INDEX_FILE_END,               /* -Fm */
  INDEX_FILE_INCLUDE,		/* +Fi */
  INDEX_FUNCTION_BEGIN,         /* +fm */
  INDEX_FUNCTION_END,           /* -fm */
  INDEX_FUNCTION_DECL,          /* +fh */
  INDEX_CONST_DECL,             /* +nh */
  INDEX_VAR_DECL,               /* +vm */
  INDEX_TYPE_DECL,              /* +th  typedefs */
  INDEX_PROTOCOL_BEGIN,         /* +Pm */
  INDEX_PROTOCOL_END,           /* -Pm */
  INDEX_PROTOCOL_INHERITANCE,   /* +Pi */
  INDEX_CATEGORY_BEGIN,         /* +Cm */
  INDEX_CATEGORY_END,           /* -Cm */
  INDEX_CATEGORY_DECL,          /* +Ch */
  INDEX_CATEGORY_DECL_END,      /* -Ch */
  INDEX_CLASS_METHOD_BEGIN,     /* ++m */
  INDEX_CLASS_OPERATOR_BEGIN,   /* ++m  C++ Operator */
  INDEX_CLASS_METHOD_END,       /* -+m */
  INDEX_CLASS_METHOD_DECL,      /* ++h */
  INDEX_CLASS_OPERATOR_DECL,    /* ++h  C++ Operator */
  INDEX_INSTANCE_METHOD_BEGIN,  /* +-m */
  INDEX_INSTANCE_OPERATOR_BEGIN,/* +-m  C++ Operator */
  INDEX_INSTANCE_METHOD_END,    /* --m */
  INDEX_INSTANCE_METHOD_DECL,   /* +-h */
  INDEX_INSTANCE_OPERATOR_DECL, /* +-h  C++ Operator */
  INDEX_MACRO,                  /* +Mh */
  INDEX_DATA_DECL,              /* +dm  Data Member */
  INDEX_DATA_INHERITANCE,       /* +di  Data Member Inheritance */
  INDEX_NAMESPACE_DECL,         /* +Nh  Namespace */
  INDEX_CONSTRUCTOR_BEGIN,      /* +-m */
  INDEX_CONSTRUCTOR_DECL,       /* +-h */
  INDEX_DESTRUCTOR_BEGIN,       /* +-m ~	C++ Destructor  */
  INDEX_DESTRUCTOR_DECL,        /* +-h ~	C++ Destructor  */
  INDEX_CLASS_DECL,             /* +ch */
  INDEX_CLASS_DECL_END,         /* -ch */
  INDEX_CLASS_BEGIN,            /* +cm */
  INDEX_CLASS_END,              /* -cm */
  INDEX_CLASS_INHERITANCE,      /* +ci */
  INDEX_UNION_BEGIN,            /* +um  union */
  INDEX_UNION_END,              /* -um */
  INDEX_RECORD_BEGIN,           /* +sm  struct */
  INDEX_RECORD_END              /* -sm */
};

#endif

