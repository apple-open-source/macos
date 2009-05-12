/*
 * Internal header file.
 * Copyright (c) 1995-2000 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GSINT_H
#define GSINT_H

/*
 * Config stuffs.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef ___P
#if PROTOTYPES
#define ___P(protos) protos
#else /* no PROTOTYPES */
#define ___P(protos) ()
#endif /* no PROTOTYPES */
#endif

#if STDC_HEADERS

#include <stdlib.h>
#include <string.h>

#else /* no STDC_HEADERS */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#ifndef HAVE_STRCHR
#define strchr index
#define strrchr rindex
#endif
char *strchr ();
char *strrchr ();

#ifndef HAVE_STRERROR
extern char *strerror ___P ((int));
#endif

#ifndef HAVE_MEMMOVE
extern void *memmove ___P ((void *, void *, size_t));
#endif

#ifndef HAVE_MEMCPY
extern void *memcpy ___P ((void *, void *, size_t));
#endif

#endif /* no STDC_HEADERS */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_MATH_H
#include <math.h>
#else
extern double atan2 ___P ((double, double));
#endif

#include <errno.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

#if HAVE_PWD_H
#include <pwd.h>
#else
#include "dummypwd.h"
#endif

#if ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) String
#endif

#if HAVE_LC_MESSAGES
#include <locale.h>
#endif

#ifndef HAVE_GETCWD
#if HAVE_GETWD
#define getcwd(buf, len) getwd(buf)
#endif /* HAVE_GETWD */
#endif /* not HAVE_GETCWD */

#include "afm.h"
#include "strhash.h"
#include "xalloc.h"

/*
 * Types and definitions.
 */

#define MATCH(a, b) (strcmp (a, b) == 0)

#define ISNUMBERDIGIT(ch) \
  (('0' <= (ch) && (ch) <= '9') || (ch) == '.' || (ch) == '-' || (ch) == '+')

/* Return the width of the character <ch> */
#define CHAR_WIDTH(ch) (font_widths[(unsigned char) (ch)])

/* Current point y movement from line to line. */
#define LINESKIP (Fpt.h + baselineskip)


/* Constants for output files. */
#define OUTPUT_FILE_NONE   NULL
#define OUTPUT_FILE_STDOUT ((char *) 1)

/* Underlay styles. */
#define UL_STYLE_OUTLINE 	0
#define UL_STYLE_FILLED		1

struct media_entry_st
{
  struct media_entry_st *next;
  char *name;
  int w;
  int h;
  int llx;
  int lly;
  int urx;
  int ury;
};

typedef struct media_entry_st MediaEntry;

typedef enum
{
  HDR_NONE,
  HDR_SIMPLE,
  HDR_FANCY
} HeaderType;


typedef enum
{
  ENC_ISO_8859_1,
  ENC_ISO_8859_2,
  ENC_ISO_8859_3,
  ENC_ISO_8859_4,
  ENC_ISO_8859_5,
  ENC_ISO_8859_7,
  ENC_ISO_8859_9,
  ENC_ISO_8859_10,
  ENC_ASCII,
  ENC_ASCII_FISE,
  ENC_ASCII_DKNO,
  ENC_IBMPC,
  ENC_MAC,
  ENC_VMS,
  ENC_HP8,
  ENC_KOI8,
  ENC_PS
} InputEncoding;

struct encoding_registry_st
{
  char *names[3];
  InputEncoding encoding;
  int nl;
  int bs;
};

typedef struct encoding_registry_st EncodingRegistry;

typedef enum
{
  LABEL_SHORT,
  LABEL_LONG
} PageLabelFormat;

typedef enum
{
  MWLS_NONE	= 0,
  MWLS_PLUS	= 1,
  MWLS_BOX	= 2,
  MWLS_ARROW	= 3
} MarkWrappedLinesStyle;

typedef enum
{
  NPF_SPACE,
  NPF_QUESTIONMARK,
  NPF_CARET,
  NPF_OCTAL
} NonPrintableFormat;

typedef enum
{
  FORMFEED_COLUMN,
  FORMFEED_PAGE,
  FORMFEED_HCOLUMN
} FormFeedType;

typedef enum
{
  LE_TRUNCATE,
  LE_CHAR_WRAP,
  LE_WORD_WRAP
} LineEndType;

struct buffer_st
{
  char *data;
  size_t allocated;
  size_t len;
};

typedef struct buffer_st Buffer;

struct file_lookup_ctx_st
{
  /* The name of the file to lookup. */
  char *name;

  /* The suffix of the file.  This string is appended to <name>. */
  char *suffix;

  /* Buffer to which the name of the file is constructed.  If the
     file_lookup() returns 1, the name of the file is here.  The
     caller of the file_lookup() must allocate this buffer. */
  Buffer *fullname;
};

typedef struct file_lookup_ctx_st FileLookupCtx;

typedef int (*PathWalkProc) ___P ((char *path, void *context));


struct input_stream_st
{
  int is_pipe;			/* Is <fp> opened to pipe? */
  FILE *fp;
  unsigned char buf[4096];
  unsigned int data_in_buf;
  unsigned int bufpos;
  unsigned int nreads;
  unsigned char *unget_ch;
  unsigned int unget_pos;
  unsigned int unget_alloc;
};

typedef struct input_stream_st InputStream;


struct page_range_st
{
  struct page_range_st *next;
  int odd;
  int even;
  unsigned int start;
  unsigned int end;
};

typedef struct page_range_st PageRange;

struct font_point_st
{
  double w;			/* width */
  double h;			/* height */
};

typedef struct font_point_st FontPoint;

struct color_st
{
  float r;
  float g;
  float b;
};

typedef struct color_st Color;

struct cached_font_info_st
{
  double font_widths[256];
  char font_ctype[256];
  AFMBoolean font_is_fixed;
  AFMNumber font_bbox_lly;
};

typedef struct cached_font_info_st CachedFontInfo;


/*
 * Global variables.
 */

extern char *program;
extern FILE *ofp;
extern char *version_string;
extern char *ps_version_string;
extern char *date_string;
extern struct tm run_tm;
extern struct tm mod_tm;
extern struct passwd *passwd;
extern char *libpath;
extern char *afm_path;
extern MediaEntry *media_names;
extern MediaEntry *media;
extern char *no_job_header_switch;
extern char *output_first_line;
extern char *queue_param;
extern char *spooler_command;
extern int nl;
extern int bs;
extern unsigned int current_pagenum;
extern unsigned int input_filenum;
extern unsigned int current_file_linenum;
extern char *fname;

/* Statistics. */
extern int total_pages;
extern int num_truncated_lines;
extern int num_missing_chars;
extern int missing_chars[];
extern int num_non_printable_chars;
extern int non_printable_chars[];

/* Dimensions that are used during PostScript generation. */
extern int d_page_w;
extern int d_page_h;
extern int d_header_w;
extern int d_header_h;
extern int d_footer_h;
extern int d_output_w;
extern int d_output_h;
extern int d_output_x_margin;
extern int d_output_y_margin;
extern unsigned int nup_xpad;
extern unsigned int nup_ypad;

/* Document needed resources. */
extern StringHashPtr res_fonts;

/* Fonts to download. */
extern StringHashPtr download_fonts;

/* Additional key-value pairs, passed to the generated PostScript code. */
extern StringHashPtr pagedevice;
extern StringHashPtr statusdict;

/* User defined strings. */
extern StringHashPtr user_strings;

/* Cache for AFM files. */
extern StringHashPtr afm_cache;
extern StringHashPtr afm_info_cache;

/* AFM library handle. */
extern AFMHandle afm;

/* Fonts. */
extern char *HFname;
extern FontPoint HFpt;
extern char *Fname;
extern FontPoint Fpt;
extern FontPoint default_Fpt;
extern char *default_Fname;
extern InputEncoding default_Fencoding;

extern double font_widths[];
extern char font_ctype[];
extern int font_is_fixed;
extern double font_bbox_lly;

/* Known input encodings. */
extern EncodingRegistry encodings[];

/* Options. */

extern char *printer;
extern int verbose;
extern int num_copies;
extern char *title;
extern int num_columns;
extern LineEndType line_end;
extern int quiet;
extern int landscape;
extern HeaderType header;
extern char *fancy_header_name;
extern char *fancy_header_default;
extern double line_indent;
extern char *page_header;
extern char *page_footer;
extern char *output_file;
extern unsigned int lines_per_page;
extern InputEncoding encoding;
extern char *media_name;
extern char *encoding_name;
extern int special_escapes;
extern int escape_char;
extern int default_escape_char;
extern int tabsize;
extern double baselineskip;
extern FontPoint ul_ptsize;
extern double ul_gray;
extern char *ul_font;
extern char *underlay;
extern char *ul_position;
extern double ul_x;
extern double ul_y;
extern double ul_angle;
extern unsigned int ul_style;
extern char *ul_style_str;
extern int ul_position_p;
extern int ul_angle_p;
extern PageLabelFormat page_label;
extern char *page_label_format;
extern int pass_through;
extern int line_numbers;
extern unsigned int start_line_number;
extern int interpret_formfeed;
extern NonPrintableFormat non_printable_format;
extern MarkWrappedLinesStyle mark_wrapped_lines_style;
extern char *mark_wrapped_lines_style_name;
extern char *npf_name;
extern int clean_7bit;
extern int append_ctrl_D;
extern unsigned int highlight_bars;
extern double highlight_bar_gray;
extern int page_prefeed;
extern PageRange *page_ranges;
extern int borders;
extern double line_highlight_gray;
extern double bggray;
extern int accept_composites;
extern FormFeedType formfeed_type;
extern char *input_filter_stdin;
extern int toc;
extern FILE *toc_fp;
extern char *toc_fmt_string;
extern unsigned int file_align;
extern int slicing;
extern unsigned int slice;

extern char *states_binary;
extern int states_color;
extern char *states_config_file;
extern char *states_highlight_style;
extern char *states_path;

extern unsigned int nup;
extern unsigned int nup_rows;
extern unsigned int nup_columns;
extern int nup_landscape;
extern unsigned int nup_width;
extern unsigned int nup_height;
extern double nup_scale;
extern int nup_columnwise;
extern char *output_language;
extern int output_language_pass_through;
extern int generate_PageSize;
extern double horizontal_column_height;
extern unsigned int pslevel;
extern int rotate_even_pages;
extern int swap_even_page_margins;
extern int continuous_page_numbers;


/*
 * Prototypes for global functions.
 */

/* Print message if <verbose> is >= <verbose_level>. */
#define MESSAGE(verbose_level, body)		\
  do {						\
    if (!quiet && verbose >= (verbose_level))	\
      fprintf body;				\
  } while (0)

/* Report continuable error. */
#define ERROR(body)			\
  do {					\
    fprintf (stderr, "%s: ", program);	\
    fprintf body;			\
    fprintf (stderr, "\n");		\
    fflush (stderr);			\
  } while (0)

/* Report fatal error and exit with status 1.  Function never returns. */
#define FATAL(body)			\
  do {					\
    fprintf (stderr, "%s: ", program);	\
    fprintf body;			\
    fprintf (stderr, "\n");		\
    fflush (stderr);			\
    exit (1);				\
  } while (0)

/*
 * Read config file <path, name>.  Returns bool.  If function fails, error
 * is found from errno.
 */
int read_config ___P ((char *path, char *name));

/* Print PostScript header to our output stream. */
void dump_ps_header ___P ((void));

/* Print PostScript trailer to our output stream. */
void dump_ps_trailer ___P ((void));

/*
 * Open InputStream to <fp> or <fname>.  If <input_filter> is given
 * it is used to pre-filter the incoming data stream.  Function returns
 * 1 if stream could be opened or 0 otherwise.
 */
int is_open ___P ((InputStream *is, FILE *fp, char *fname,
		   char *input_filter));

/* Close InputStream <is>. */
void is_close ___P ((InputStream *is));

/*
 * Read next character from the InputStream <is>.  Returns EOF if
 * EOF was reached.
 */
int is_getc ___P ((InputStream *is));

/*
 * Put character <ch> back to the InputStream <is>.  Function returns EOF
 * if character couldn't be unget.
 */
int is_ungetc ___P ((int ch, InputStream *is));


void buffer_init ___P ((Buffer *buffer));

void buffer_uninit ___P ((Buffer *buffer));

Buffer *buffer_alloc ();

void buffer_free ___P ((Buffer *buffer));

void buffer_append ___P ((Buffer *buffer, const char *data));

void buffer_append_len ___P ((Buffer *buffer, const char *data, size_t len));

char *buffer_copy ___P ((Buffer *buffer));

void buffer_clear ___P ((Buffer *buffer));

char *buffer_ptr ___P ((Buffer *buffer));

size_t buffer_len ___P ((Buffer *buffer));


/*
 * Process single input file <fp>.  File's name is given in <fname> and
 * it is used to print headers.  The argument <is_toc> specifies whether
 * the file is a table of contents file or not.
 */
void process_file ___P ((char *fname, InputStream *fp, int is_toc));

/* Add a new media to the list of known media. */
void add_media ___P ((char *name, int w, int h, int llx, int lly, int urx,
		      int ury));

/* Print a listing of missing characters. */
void do_list_missing_characters ___P ((int *array));

/*
 * Check if file <name, suffix> exists.  Returns bool.  If function fails
 * error can be found from errno.
 */
int file_existsp ___P ((char *name, char *suffix));

/*
 * Paste file <name, suffix> to output stream.  Returns bool. If
 * function fails, error can be found from errno.
 */
int paste_file ___P ((char *name, char *suffix));

/*
 * Do tilde substitution for filename <fname>.  The function returns a
 * xmalloc()ated result that must be freed by the caller.
 */
char *tilde_subst ___P ((char *fname));

/*
 * Parse one float dimension from string <string>.  If <units> is true,
 * then number can be followed by an optional unit specifier.  If
 * <horizontal> is true, then dimension is horizontal, otherwise it
 * is vertical.
 */
double parse_float ___P ((char *string, int units, int horizontal));

/*
 * Parse font spec <spec> and return font's name, size, and encoding
 * in variables <name_return>, <size_return>, and <encoding_return>.
 * Returns 1 if <spec> was a valid font spec or 0 otherwise.  Returned
 * name <name_return> is allocated with xcalloc() and must be freed by
 * caller.
 */
int parse_font_spec ___P ((char *spec, char **name_return,
			   FontPoint *size_return,
			   InputEncoding *encoding_return));

/*
 * Read body font's character widths and character codes from AFM files.
 */
void read_font_info ___P ((void));

/*
 * Try to download font <name>.
 */
void download_font ___P ((char *name));

/*
 * Escape all PostScript string's special characters from string <string>.
 * Returns a xmalloc()ated result.
 */
char *escape_string ___P ((char *string));

/*
 * Expand user escapes from string <string>.  Returns a xmalloc()ated
 * result.
 */
char *format_user_string ___P ((char *context_name, char *string));

/*
 * Parses key-value pair <kv> and inserts/deletes key from <set>.
 */
void parse_key_value_pair ___P ((StringHashPtr set, char *kv));

/*
 * Count how many non-empty items there are in the key-value set <set>.
 */
int count_key_value_set ___P ((StringHashPtr set));

/*
 * Walk through path <path> and call <proc> once for each of its
 * components.  Function returns 0 if all components were accessed.
 * Callback <proc> can interrupt walking by returning a non zero
 * return value.  In that case value is returned as the return value
 * of the pathwalk().
 */
int pathwalk ___P ((char *path, PathWalkProc proc, void *context));

/* Lookup file from path.  <context> must point to FileLookupCtx. */
int file_lookup ___P ((char *path, void *context));


/*
 * Portable printer interface.
 */

/*
 * Open and initialize printer <cmd>, <options>, <queue_param> and
 * <printer_name>.  Function returns a FILE pointer to which enscript
 * can generate its PostScript output or NULL if printer
 * initialization failed.  Command can store its context information
 * to variable <context_return> wich is passed as an argument to the
 * printer_close() function.
 */
FILE *printer_open ___P ((char *cmd, char *options, char *queue_param,
			  char *printer_name, void **context_return));

/*
 * Flush all pending output to printer <context> and close it.
 */
void printer_close ___P ((void *context));

/*
 * Escape filenames for shell usage
 */
char *shell_escape ___P ((const char *fn));

#endif /* not GSINT_H */
