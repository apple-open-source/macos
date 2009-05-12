/*
 * Argument handling and main.
 * Copyright (c) 1995-2003 Markku Rossi.
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

#include "gsint.h"
#include "getopt.h"

/*
 * Prototypes for static functions.
 */

/*
 * Open output file according to user options.  Void if output file
 * has already been opened.
 */
static void open_output_file ();

/* Close output file. */
static void close_output_file ();

/* Handle options from environment variable <var> */
static void handle_env_options ___P ((char *var));

/* Handle options from <argv> array. */
static void handle_options ___P ((int argc, char *argv[]));

/* Print usage info. */
static void usage ();

/* Print version info. */
static void version ();


/*
 * Global variables.
 */

char *program;			/* Program's name, used for messages. */
FILE *ofp = NULL;		/* Output file. */
void *printer_context;		/* Context for the printer. */
char *version_string = NULL;	/* Enscript's version string. */
char *ps_version_string = NULL;	/* Version string for PS procsets. */
char *date_string = NULL;	/* Preformatted time string. */
struct tm run_tm;		/* Time when program is run. */
struct tm mod_tm;		/* Last modification time for current file. */
struct passwd *passwd;		/* Passwd entry for the user running this
				   program. */

/* Path to our library. */
char *enscript_library = LIBRARY;

/* Library lookup path. */
char *libpath = NULL;

/* AFM library lookup path. */
char *afm_path = NULL;

MediaEntry *media_names = NULL;	/* List of known media. */
MediaEntry *media = NULL;	/* Entry for used media. */
int bs = 8;			/* The backspace character. */

/* Statistics. */
int total_pages = 0;		/* Total number of pages printed. */
int num_truncated_lines = 0;	/* Number of lines truncated. */
int num_missing_chars = 0;	/* Number of unknown characters. */
int missing_chars[256] = {0};	/* Table of unknown characters. */
int num_non_printable_chars = 0; /* Number of non-printable characters. */
int non_printable_chars[256] = {0}; /* Table of non-printable characters. */

/* Output media dimensions that are used during PostScript emission. */
int d_page_w = 0;		/* page's width */
int d_page_h = 0;		/* page's height */
int d_header_w = 0;		/* fancy header's width */
int d_header_h = 0;		/* fancy header's height */
int d_footer_h = 0;		/* fancy footer's height */
int d_output_w = 0;		/* output area's width */
int d_output_h = 0;		/* output area's height  */
int d_output_x_margin = 5;	/* output area's x marginal */
int d_output_y_margin = 2;	/* output area's y marginal */

/* Document needed resources. */
StringHashPtr res_fonts;	/* fonts */

/* Fonts to download. */
StringHashPtr download_fonts;

/* Additional key-value pairs, passed to the generated PostScript code. */
StringHashPtr pagedevice;	/* for setpagedevice */
StringHashPtr statusdict;	/* for statusdict */

/* User defined strings. */
StringHashPtr user_strings;

/* Cache for AFM files. */
StringHashPtr afm_cache = NULL;
StringHashPtr afm_info_cache = NULL;

/* AFM library handle. */
AFMHandle afm = NULL;


/* Options. */

/*
 * Free single-letter options are: Q, x, y, Y
 */

/*
 * -#
 *
 * An alias for -n, --copies.
 */

/*
 * -1, -2, -3, -4, -5, -6, -7, -8, -9, --columns=NUM
 *
 * Number of columns per page.  The default is 1 column.
 */
int num_columns = 1;

/*
 * -a PAGES, --pages=PAGES
 *
 * Specify which pages are printed.
 */
PageRange *page_ranges = NULL;

/*
 * -A ALIGN, --file-align=ALIGN
 *
 * Align input files to start from ALIGN page count.  This is handy
 * for two-side printings.
 */
unsigned int file_align = 1;

/*
 * -b STRING, --header=STRING
 *
 * Set the string that is used as the page header.  As a default, page
 * header is constructed from filename, date and page number.
 */
char *page_header = NULL;

/*
 * -B, --no-header
 *
 * Do not print page headers.
 */

/*
 * -c, --truncate-lines
 *
 * Truncate lines that are longer than the page width.  Default is character
 * wrap.
 */
LineEndType line_end = LE_CHAR_WRAP;

/*
 * -C [START], --line-numbers[=START]
 *
 * Precede each line with its line number.  As a default, do not mark
 * line numbers.  If the optional argument START is given, it
 * specifies the number from which the line numbers are assumed to
 * start in the file.  This is useful if the file contains a region
 * of a bigger file.
 */
int line_numbers = 0;
unsigned int start_line_number = 1;

/*
 * -d, -P, --printer
 *
 * Name of the printer to which output is send.  Defaults to system's
 * default printer.
 */
char *printer = NULL;

/*
 * -e [CHAR], --escapes[=CHAR]
 *
 * Enable special escape ('\000') interpretation.  If option CHAR is given
 * it is assumed to specify the escape character.
 */
int special_escapes = 0;
int escape_char = '\0';
int default_escape_char;

/*
 * -E [LANG], --highlight=[LANG] (deprecated --pretty-print[=LANG])
 *
 * Highlight program source code.  Highlighting is handled by creating
 * an input filter with the states-program.  States makes an educated
 * guess about the start state but sometimes it fails, so the start
 * state can also be specified to be LANG.  This option overwrites
 * input filter and enables special escapes.
 */

int highlight = 0;
char *hl_start_state = NULL;

/*
 * -f, --font
 *
 * Select body font.
 */
char *Fname = "Courier";
FontPoint Fpt = {10.0, 10.0};
FontPoint default_Fpt;		/* Point size of the original font. */
char *default_Fname;		/* Name of the original font. */
InputEncoding default_Fencoding; /* The encoding of the original font. */
int user_body_font_defined = 0;	/* Has user defined new body font? */

double font_widths[256];	/* Width array for body font. */
char font_ctype[256];		/* Font character types. */
int font_is_fixed;		/* Is body font a fixed pitch font? */
double font_bbox_lly;		/* Font's bounding box's lly-coordinate. */

/*
 * -F, --header-font
 *
 * Select font to be used to print the standard simple header.
 */
char *HFname = "Courier-Bold";
FontPoint HFpt = {10.0, 10.0};

/*
 * -g, --print-anyway
 *
 * Print document even it contains binary data.  This does nothing
 * since enscript prints files anyway.
 */

/*
 * -G, --fancy-header
 *
 * Add a fancy header to top of every page.  There are several header styles
 * but the default is 'no fancy header'.
 */
HeaderType header = HDR_SIMPLE;
char *fancy_header_name = NULL;
char *fancy_header_default = NULL;

/*
 * -h, --no-job-header
 *
 * Supress the job header page.
 */
static int no_job_header = 0;

/*
 * -H num, --highlight-bars=num
 *
 * Print highlight bars under text.  Bars will be <num> lines high.
 * As a default, do not print bars.
 */
unsigned int highlight_bars = 0;

/*
 * -i, --indent
 *
 * Indent every line this many characters.
 */
double line_indent = 0.0;
char *line_indent_spec = "0";

/*
 * -I CMD, --filter=CMD
 *
 * Read input files through input filter CMD.
 */
char *input_filter = NULL;

/*
 * -j, --borders
 *
 * Print borders around columns.
 */
int borders = 0;

/*
 * -J
 *
 * An alias for -t, --title.
 */

/*
 * -k, --page-prefeed
 * -K, --no-page-prefeed
 *
 * Control page prefeed.
 */
int page_prefeed = 0;

/*
 * -l, --lineprinter
 *
 * Emulate lineprinter -  make pages 66 lines long and omit headers.
 */

/*
 * -L, --lines-per-page
 *
 * Specify how many lines should be printed on a single page.  Normally
 * enscript counts it from font point sizes.
 */
unsigned int lines_per_page = (unsigned int) -1;

/*
 * -m, --mail
 *
 * Send mail notification to user after print job has been completed.
 */
int mail = 0;

/*
 * -M, --media
 *
 * Name of the output media.  Default is A4.
 */
char *media_name = NULL;

/*
 * -n, --copies
 *
 * Number of copies to print.
 */
int num_copies = 1;

/*
 * -N, --newline
 *
 * Set the newline character: '\n' or '\r'.  As a default, the newline
 * character is specified by the input encoding.
 */
int nl = -1;

/*
 * -o, -p, --output
 *
 * Leave output to the specified file.  As a default result is spooled to
 * printer.
 */
char *output_file = OUTPUT_FILE_NONE;

/*
 * -O, --missing-characters
 *
 * List all missing characters.  Default is no listing.
 */
int list_missing_characters = 0;

/*
 * -q, --quiet
 *
 * Do not tell what we are doing.  Default is to tell something but
 * not --verbose.
 */
int quiet = 0;

/*
 * -r, --landscape
 * -R, --portrait
 *
 * Print with page rotated 90 degrees (landscape mode).  Default is
 * portrait.
 */
int landscape = 0;

/*
 * -s, --baselineskip
 *
 * Specify baselineskip value that is used when enscript moves to
 * a new line.  Current point movement is font_point_size + baselineskip.
 */
double baselineskip = 1.0;

/*
 * -t, --title
 *
 * Title which is printed to the banner page.  If this option is given
 * from the command line, this sets also the name of the stdin which
 * is by the default "".
 */
char *title = "Enscript Output";
int title_given = 0;

/*
 * -T, --tabsize
 *
 * Specify tabulator size.
 */
int tabsize = 8;

/*
 * -u, --underlay
 *
 * Place text under every page.  Default is no underlay.
 */
double ul_gray = .8;
FontPoint ul_ptsize = {200.0, 200.0};
char *ul_font = "Times-Roman";
char *underlay = NULL;
char *ul_position = NULL;	/* Position info as a string. */
double ul_x;			/* Position x-coordinate. */
double ul_y;			/* Position y-coordinate. */
double ul_angle;
unsigned int ul_style = UL_STYLE_OUTLINE;
char *ul_style_str = NULL;
int ul_position_p = 0;		/* Is ul-position given? */
int ul_angle_p = 0;		/* Is ul-angle given? */

/*
 * -U NUM, --nup=NUM
 *
 * Print NUM PostScript pages on each output page (n-up printing).
 */
unsigned int nup = 1;
unsigned int nup_exp = 0;
unsigned int nup_rows = 1;
unsigned int nup_columns = 1;
int nup_landscape = 0;
unsigned int nup_width;
unsigned int nup_height;
double nup_scale;

/*
 * -v, --verbose
 *
 * Tell what we are doing.  Default is no verbose outputs.
 */
int verbose = 0;

/*
 * -V, --version
 *
 * Print version information.
 */

/*
 * -w LANGUAGE, --language=LANGUAGE
 *
 * Generate output for language LANGUAGE.  The default is PostScript.
 */
char *output_language = "PostScript";
int output_language_pass_through = 0;

/*
 * -W APP,option, --options=APP,OPTION
 *
 * Pass additional option to enscript's helper applications.  The
 * first part of the option's argument (APP) specifies the
 * helper application to which the options are added.  Currently the
 * following helper application are defined:
 *
 *   s	states
 */
Buffer *helper_options[256] = {0};

/*
 * -X, --encoding
 *
 * Specifies input encoding.  Default is ISO-8859.1.
 */
InputEncoding encoding = ENC_ISO_8859_1;
char *encoding_name = NULL;

/*
 * -z, --no-formfeed
 *
 * Do not interpret form feed characters.  As a default, form feed
 * characters are interpreted.
 */
int interpret_formfeed = 1;

/*
 * -Z, --pass-through
 *
 * Pass through all PostScript and PCL files without any modifications.
 * As a default, don't.
 */
int pass_through = 0;

/*
 * --color[=bool]
 *
 * Create color output with states?
 */

/*
 * --continuous-page-numbers
 *
 * Count page numbers across input files.  Don't restart numbering
 * at beginning of each file.
 */
int continuous_page_numbers = 0;

/*
 * --download-font=FONT
 *
 * Download font FONT to printer.
 */

/*
 * --extended-return-values
 *
 * Enable extended return values.
 */
int extended_return_values = 0;

/*
 * --filter-stdin=STR
 *
 * How stdin is shown to the filter command.  The default is "" but
 * some utilities might want it as "-".
 */
char *input_filter_stdin = "";

/*
 * --footer=STRING
 *
 * Set the string that is used as the page footer.  As a default, the
 * page has no footer.  Setting this option does not necessary show
 * any footer strings in the output.  It depends on the selected
 * header (`.hdr' file) whether it supports footer strings or not.
 */
char *page_footer = NULL;

/*
 * --h-column-height=HEIGHT
 *
 * Set the horizontal column (channel) height to be HEIGHT.  This option
 * also sets the FormFeedType to `hcolumn'.  The default value is set to be
 * big enough to cause a jump to the next vertical column (100m).
 */
double horizontal_column_height = 283465.0;

/*
 * --help-highlight (deprecated --help-pretty-print)
 *
 * Descript all supported -E, --highlight languages and file formats.
 */
int help_highlight = 0;

/*
 * --highlight-bar-gray=val
 *
 * Specify the gray level for highlight bars.
 */
double highlight_bar_gray = .97;

/*
 * --list-media
 *
 * List all known media.  As a default do not list media names.
 */
int list_media = 0;

/*
 * --margins=LEFT:RIGHT:TOP:BOTTOM
 *
 * Adjust page marginals.
 */
char *margins_spec = NULL;

/*
 * --mark-wrapped-lines[=STYLE]
 *
 * Mark wrapped lines so that they can be easily detected from the printout.
 * Optional parameter STYLE specifies the marking style, the system default
 * is black box.
 */
char *mark_wrapped_lines_style_name = NULL;
MarkWrappedLinesStyle mark_wrapped_lines_style = MWLS_NONE;

/*
 * --non-printable-format=FORMAT
 *
 * Format in which non-printable characters are printed.
 */
char *npf_name = NULL;
NonPrintableFormat non_printable_format = NPF_OCTAL;

/*
 * --nup-columnwise
 *
 * Layout N-up pages colunwise instead of row-wise.
 */
int nup_columnwise = 0;

/*
 * --nup-xpad=NUM
 *
 * The x-padding between N-up subpages.
 */
unsigned int nup_xpad = 10;

/*
 * --nup-ypad=NUM
 *
 * The y-padding between N-up subpages.
 */
unsigned int nup_ypad = 10;

/*
 * --page-label-format=FORMAT
 *
 * Format in which page labels are printed; the default is "short".
 */
char *page_label_format = NULL;
PageLabelFormat page_label;

/*
 * --ps-level=LEVEL
 *
 * The PostScript language level that enscript should use; the default is 2.
 */
unsigned int pslevel = 2;

/*
 * --printer-options=OPTIONS
 *
 * Pass extra options OPTIONS to the printer spooler.
 */
char *printer_options = NULL;

/*
 * --rotate-even-pages
 *
 * Rotate each even-numbered page 180 degrees.  This might be handy in
 * two-side printing when the resulting pages are bind from some side.
 * Greetings to Jussi-Pekka Sairanen.
 */
int rotate_even_pages = 0;

/*
 * --slice=NUM
 *
 * Horizontal input slicing.  Print only NUMth wrapped input pages.
 */
int slicing = 0;
unsigned int slice = 1;

/*
 * --swap-even-page-margins
 *
 * Swap left and right side margins for each even numbered page.  This
 * might be handy in two-side printing.
 */
int swap_even_page_margins = 0;

/*
 * --toc
 *
 * Print Table of Contents page.
 */
int toc = 0;
FILE *toc_fp;
char *toc_fmt_string;

/*
 * --word-wrap
 *
 * Wrap long lines from word boundaries.  The default is character wrap.
 */

/*
 * AcceptCompositeCharacters: bool
 *
 * Specify whatever we accept composite characters or should them be
 * considered as non-existent.  As a default, do not accept them.
 */
int accept_composites = 0;

/*
 * AppendCtrlD: bool
 *
 * Append ^D character to the end of the output.  Some printers require this
 * but the default is false.
 */
int append_ctrl_D = 0;

/*
 * Clean7Bit: bool
 *
 * Specify how characters greater than 127 are printed.
 */
int clean_7bit = 1;

/*
 * FormFeedType: type
 *
 * Specify what to do when a formfeed character is encountered from the
 * input stream.  The default action is to jump to the beginning of the
 * next column.
 */
FormFeedType formfeed_type = FORMFEED_COLUMN;

/*
 * GeneratePageSize: bool
 *
 * Specify whether the `PageSize' pagedevice definitions should be
 * generated to the output.
 */
int generate_PageSize = 1;

/*
 * NoJobHeaderSwitch: switch
 *
 * Spooler switch to suppress the job header (-h).
 */
char *no_job_header_switch = NULL;

/*
 * OutputFirstLine: line
 *
 * Set the PostScript output's first line to something your system can handle.
 * The default is "%!PS-Adobe-3.0"
 */
char *output_first_line = NULL;

/*
 * QueueParam: param
 *
 * The spooler command switch to select the printer queue (-P).
 */
char *queue_param = NULL;

/*
 * Spooler: command
 *
 * The spooler command name (lpr).
 */
char *spooler_command = NULL;

/*
 * StatesBinary: path
 *
 * An absolute path to the `states' binary.
 */

char *states_binary = NULL;

/*
 * StatesColor: bool
 *
 * Should the States program generate color outputs.
 */
int states_color = 0;

/*
 * StatesConfigFile: file
 *
 * The name of the states' configuration file.
 */
char *states_config_file = NULL;

/*
 * StatesHighlightStyle: style
 *
 * The highlight style.
 */
char *states_highlight_style = NULL;

/*
 * StatesPath: path
 *
 * Define the path for the states program.  The states program will
 * lookup its state definition files from this path.
 */
char *states_path = NULL;

/* ^@shade{GRAY}, set the line highlight gray. */
double line_highlight_gray = 1.0;

/* ^@bggray{GRAY}, set the text background gray. */
double bggray = 1.0;

EncodingRegistry encodings[] =
{
  {{"88591", "latin1", NULL},		ENC_ISO_8859_1,		'\n', 8},
  {{"88592", "latin2", NULL},		ENC_ISO_8859_2,		'\n', 8},
  {{"88593", "latin3", NULL},		ENC_ISO_8859_3,		'\n', 8},
  {{"88594", "latin4", NULL},		ENC_ISO_8859_4,		'\n', 8},
  {{"88595", "cyrillic", NULL},		ENC_ISO_8859_5,		'\n', 8},
  {{"88597", "greek", NULL},		ENC_ISO_8859_7,		'\n', 8},
  {{"88599", "latin5", NULL},		ENC_ISO_8859_9,		'\n', 8},
  {{"885910", "latin6", NULL},		ENC_ISO_8859_10,	'\n', 8},
  {{"ascii", NULL, NULL},		ENC_ASCII, 		'\n', 8},
  {{"asciifise", "asciifi", "asciise"},	ENC_ASCII_FISE,		'\n', 8},
  {{"asciidkno", "asciidk", "asciino"},	ENC_ASCII_DKNO,		'\n', 8},
  {{"ibmpc", "pc", "dos"},		ENC_IBMPC, 		'\n', 8},
  {{"mac", NULL, NULL},			ENC_MAC, 		'\r', 8},
  {{"vms", NULL, NULL},			ENC_VMS, 		'\n', 8},
  {{"hp8", NULL, NULL},			ENC_HP8,		'\n', 8},
  {{"koi8", NULL, NULL},		ENC_KOI8,		'\n', 8},
  {{"ps", "PS", NULL},			ENC_PS, 		'\n', 8},
  {{"pslatin1", "ISOLatin1Encoding", NULL},	ENC_ISO_8859_1,	'\n', 8},

  {{NULL, NULL, NULL}, 0, 0, 0},
};


/*
 * Static variables.
 */

static struct option long_options[] =
{
  {"columns",			required_argument,	0, 0},
  {"pages",			required_argument,	0, 'a'},
  {"file-align",		required_argument,	0, 'A'},
  {"header",			required_argument,	0, 'b'},
  {"no-header",			no_argument,		0, 'B'},
  {"truncate-lines",		no_argument,		0, 'c'},
  {"line-numbers",		optional_argument,	0, 'C'},
  {"printer",			required_argument,	0, 'd'},
  {"setpagedevice",		required_argument,	0, 'D'},
  {"escapes",			optional_argument,	0, 'e'},
  {"highlight",			optional_argument,	0, 'E'},
  {"font",			required_argument,	0, 'f'},
  {"header-font",		required_argument,	0, 'F'},
  {"print-anyway",		no_argument,		0, 'g'},
  {"fancy-header",		optional_argument,	0, 'G'},
  {"no-job-header",		no_argument, 		0, 'h'},
  {"highlight-bars",		optional_argument,	0, 'H'},
  {"indent",			required_argument,	0, 'i'},
  {"filter",			required_argument,	0, 'I'},
  {"borders",			no_argument,		0, 'j'},
  {"page-prefeed",		no_argument,		0, 'k'},
  {"no-page-prefeed",		no_argument,		0, 'K'},
  {"lineprinter",		no_argument,		0, 'l'},
  {"lines-per-page",		required_argument,	0, 'L'},
  {"mail",			no_argument,		0, 'm'},
  {"media",			required_argument,	0, 'M'},
  {"copies",			required_argument,	0, 'n'},
  {"newline",			required_argument,	0, 'N'},
  {"output",			required_argument,	0, 'p'},
  {"missing-characters",	no_argument,		0, 'O'},
  {"quiet",			no_argument,		0, 'q'},
  {"silent",			no_argument,		0, 'q'},
  {"landscape",			no_argument,		0, 'r'},
  {"portrait",			no_argument,		0, 'R'},
  {"baselineskip",		required_argument,	0, 's'},
  {"statusdict",		required_argument,	0, 'S'},
  {"title",			required_argument,	0, 't'},
  {"tabsize",			required_argument,	0, 'T'},
  {"underlay",			optional_argument,	0, 'u'},
  {"nup",			required_argument,	0, 'U'},
  {"verbose",			optional_argument,	0, 'v'},
  {"version",			no_argument,		0, 'V'},
  {"language",			required_argument,	0, 'w'},
  {"option",			required_argument,	0, 'W'},
  {"encoding",			required_argument,	0, 'X'},
  {"no-formfeed",		no_argument,		0, 'z'},
  {"pass-through",		no_argument,		0, 'Z'},

  /* Long options without short counterparts.  Next free is 157. */
  {"color",			optional_argument,	0, 142},
  {"continuous-page-numbers",	no_argument,		0, 156},
  {"download-font",		required_argument,	0, 131},
  {"extended-return-values",	no_argument,		0, 154},
  {"filter-stdin",		required_argument,	0, 138},
  {"footer",			required_argument,	0, 155},
  {"h-column-height", 		required_argument,	0, 148},
  {"help", 			no_argument, 		0, 135},
  {"help-highlight",	 	no_argument, 		0, 141},
  {"highlight-bar-gray",	required_argument, 	0, 136},
  {"list-media",		no_argument,		&list_media, 1},
  {"margins",			required_argument,	0, 144},
  {"mark-wrapped-lines",	optional_argument,	0, 143},
  {"non-printable-format",	required_argument,	0, 134},
  {"nup-columnwise",		no_argument,		0, 152},
  {"nup-xpad",			required_argument,	0, 145},
  {"nup-ypad",			required_argument,	0, 146},
  {"page-label-format",		required_argument,	0, 130},
  {"ps-level",			required_argument,	0, 149},
  {"printer-options",		required_argument,	0, 139},
  {"rotate-even-pages",		no_argument,		0, 150},
  {"slice",			required_argument,	0, 140},
  {"style",			required_argument,	0, 151},
  {"swap-even-page-margins",	no_argument,		0, 153},
  {"toc",			no_argument,		&toc, 1},
  {"word-wrap",			no_argument,		0, 147},
  {"ul-angle",			required_argument,	0, 132},
  {"ul-font",			required_argument,	0, 128},
  {"ul-gray",			required_argument,	0, 129},
  {"ul-position",		required_argument,	0, 133},
  {"ul-style",			required_argument,	0, 137},

  /* Backwards compatiblity options. */
  {"pretty-print",		optional_argument,	0, 'E'},
  {"help-pretty-print", 	no_argument, 		0, 141},

  {NULL, 0, 0, 0},
};


/*
 * Global functions.
 */

int
main (int argc, char *argv[])
{
  InputStream is;
  time_t tim;
  struct tm *tm;
  int i, j, found;
  unsigned int ui;
  MediaEntry *mentry;
  AFMError afm_error;
  char *cp, *cp2;
  int retval = 0;
  Buffer buffer;

  /* Init our dynamic memory buffer. */
  buffer_init (&buffer);

  /* Get program's name. */
  program = strrchr (argv[0], '/');
  if (program == NULL)
    program = argv[0];
  else
    program++;

  /* Make getopt_long() to use our modified programname. */
  argv[0] = program;

  /* Create version strings. */

  buffer_clear (&buffer);
  buffer_append (&buffer, "GNU ");
  buffer_append (&buffer, PACKAGE);
  buffer_append (&buffer, " ");
  buffer_append (&buffer, VERSION);
  version_string = buffer_copy (&buffer);

  ps_version_string = xstrdup (VERSION);
  cp = strrchr (ps_version_string, '.');
  *cp = ' ';

  /* Create the default TOC format string.  Wow, this is cool! */
  /* xgettext:no-c-format */
  toc_fmt_string = _("$3v $-40N $3% pages $4L lines  $E $C");

  /* Internationalization. */
#if HAVE_SETLOCALE
  /*
   * We want to change only messages (gs do not like decimals in 0,1
   * format ;)
   */
#if HAVE_LC_MESSAGES
  setlocale (LC_MESSAGES, "");
#endif
#endif
#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  /* Create date string. */

  tim = time (NULL);
  tm = localtime (&tim);
  memcpy (&run_tm, tm, sizeof (*tm));

  date_string = xstrdup (asctime (&run_tm));
  i = strlen (date_string);
  date_string[i - 1] = '\0';

  /* Get user's passwd entry. */
  passwd = getpwuid (getuid ());
  if (passwd == NULL)
    FATAL ((stderr, _("couldn't get passwd entry for uid=%d: %s"), getuid (),
	    strerror (errno)));

  /* Defaults for some options. */
  media_name 		= xstrdup ("A4");
  encoding_name		= xstrdup ("88591");
  npf_name		= xstrdup ("octal");
  page_label_format	= xstrdup ("short");
  ul_style_str		= xstrdup ("outline");
  ul_position		= xstrdup ("+0-0");
  spooler_command 	= xstrdup ("lpr");
  queue_param 		= xstrdup ("-P");
  no_job_header_switch 	= xstrdup ("-h");
  fancy_header_default 	= xstrdup ("enscript");
  output_first_line 	= xstrdup ("%!PS-Adobe-3.0");

  /* Check ENSCRIPT_LIBRARY for custom library location. */
  cp = getenv ("ENSCRIPT_LIBRARY");
  if (cp)
    enscript_library = cp;

  /* Fill up build-in libpath. */

  cp = getenv ("HOME");
  if (cp == NULL)
    cp = passwd->pw_dir;

  buffer_clear (&buffer);
  buffer_append (&buffer, enscript_library);
  buffer_append (&buffer, PATH_SEPARATOR_STR);
  buffer_append (&buffer, cp);
  buffer_append (&buffer, "/.enscript");
  libpath = buffer_copy (&buffer);

  /* Defaults for the states filter. */

  states_binary = xstrdup ("states"); /* Take it from the user path. */

  buffer_clear (&buffer);
  buffer_append (&buffer, enscript_library);
  buffer_append (&buffer, "/hl/enscript.st");
  states_config_file = buffer_copy (&buffer);

  states_highlight_style = xstrdup ("emacs");

  /* The <cp> holds the user's home directory. */
  buffer_clear (&buffer);
  buffer_append (&buffer, cp);
  buffer_append (&buffer, "/.enscript");
  buffer_append (&buffer, PATH_SEPARATOR_STR);
  buffer_append (&buffer, enscript_library);
  buffer_append (&buffer, "/hl");
  states_path = buffer_copy (&buffer);

  /* Initialize resource sets. */
  res_fonts = strhash_init ();
  download_fonts = strhash_init ();
  pagedevice = strhash_init ();
  statusdict = strhash_init ();
  user_strings = strhash_init ();


  /*
   * Read configuration files.
   */

  /* Global config. */
#define CFG_FILE_NAME "enscript.cfg"
  if (!read_config (SYSCONFDIR, CFG_FILE_NAME))
    {
      int saved_errno = errno;

      /* Try to read it from our library directory.  This is mostly
	 the case for the micro ports.  */
      if (!read_config (enscript_library, CFG_FILE_NAME))
	{
	  /* Try `enscript_library/../../etc/'.  This is the case for
             installations which set the prefix after the compilation
             and our SYSCONFDIR points to wrong directory. */

	  buffer_clear (&buffer);
	  buffer_append (&buffer, enscript_library);
	  buffer_append (&buffer, "/../../etc");

	  if (!read_config (buffer_ptr (&buffer), CFG_FILE_NAME))
	    {
	      /* Maybe we are not installed yet, let's try `../lib'
                 and `../../lib'. */
	      if (!read_config ("../lib", CFG_FILE_NAME)
		  && !read_config ("../../lib", CFG_FILE_NAME))
		{
		  /* No luck, report error from the original config file. */
		  ERROR ((stderr, _("couldn't read config file \"%s/%s\": %s"),
			  enscript_library, CFG_FILE_NAME,
			  strerror (saved_errno)));
		  ERROR ((stderr,
			  _("I did also try the following directories:")));
		  ERROR ((stderr, _("\t%s"), SYSCONFDIR));
		  ERROR ((stderr, _("\t%s"), enscript_library));
		  ERROR ((stderr, _("\t%s"), buffer_ptr (&buffer)));
		  ERROR ((stderr, _("\t../lib")));
		  ERROR ((stderr, _("\t../../lib")));
		  ERROR ((stderr,
_("This is probably an installation error.  Please, try to rebuild:")));
		  ERROR ((stderr, _("\tmake distclean")));
		  ERROR ((stderr, _("\t./configure --prefix=PREFIX")));
		  ERROR ((stderr, _("\tmake")));
		  ERROR ((stderr, _("\tmake check")));
		  ERROR ((stderr, _("\tmake install")));
		  ERROR ((stderr,
_("or set the environment variable `ENSCRIPT_LIBRARY' to point to your")));
		  ERROR ((stderr,
_("library directory.")));
		  exit (1);
		}

	      /* Ok, we are not installed yet.  Here is a small kludge
		 to conform the GNU coding standards: we must be able
		 to run without being installed, so we must append the
		 `../lib' and `../../lib' directories to the libpath.
		 The later allows us to be run form the `src/tests'
		 directory.  */
	      buffer_clear (&buffer);
	      buffer_append (&buffer, libpath);
	      buffer_append (&buffer, PATH_SEPARATOR_STR);
	      buffer_append (&buffer, "../lib");
	      buffer_append (&buffer, PATH_SEPARATOR_STR);
	      buffer_append (&buffer, "../../lib");

	      xfree (libpath);
	      libpath = buffer_copy (&buffer);
	    }
	}
    }

  /* Site config. */
  (void) read_config (SYSCONFDIR, "enscriptsite.cfg");

  /* Personal config. */
  (void) read_config (passwd->pw_dir, ".enscriptrc");

  /*
   * Options.
   */

  /* Environment variables. */
  handle_env_options ("ENSCRIPT");
  handle_env_options ("GENSCRIPT");

  /* Command line arguments. */
  handle_options (argc, argv);

  /*
   * Check options which have some validity conditions.
   */

  /*
   * Save the user-specified escape char so ^@escape{default} knows
   * what to set.
   */
  default_escape_char = escape_char;

  /* Input encoding. */

  found = 0;
  for (i = 0; !found && encodings[i].names[0]; i++)
    for (j = 0; j < 3; j++)
      if (encodings[i].names[j] != NULL && MATCH (encodings[i].names[j],
						  encoding_name))
	{
	  /* Found a match for this encoding.  Use the first
             "official" name. */

	  encoding = encodings[i].encoding;
	  xfree (encoding_name);
	  encoding_name = xstrdup (encodings[i].names[0]);

	  if (nl < 0)
	    nl = encodings[i].nl;
	  bs = encodings[i].bs;
	  found = 1;
	  break;
	}
  if (!found)
    FATAL ((stderr, _("unknown encoding: %s"), encoding_name));

  /* Fonts. */

  /* Default font for landscape, 2 column printing is Courier 7. */
  if (!user_body_font_defined && landscape && num_columns > 1)
    Fpt.w = Fpt.h = 7.0;

  /* Cache for font AFM information. */
  afm_cache = strhash_init ();
  afm_info_cache = strhash_init ();

  /* Open AFM library. */
  afm_error = afm_create (afm_path, verbose, &afm);
  if (afm_error != AFM_SUCCESS)
    {
      char buf[256];

      afm_error_to_string (afm_error, buf);
      FATAL ((stderr, _("couldn't open AFM library: %s"), buf));
    }

  /*
   * Save default Fpt and Fname since special escape 'font' can change
   * it and later we might want to switch back to the "default" font.
   */
  default_Fpt.w = Fpt.w;
  default_Fpt.h = Fpt.h;
  default_Fname = Fname;
  default_Fencoding = encoding;

  /* Register that document uses at least these fonts. */
  strhash_put (res_fonts, Fname, strlen (Fname) + 1, NULL, NULL);
  strhash_put (res_fonts, HFname, strlen (HFname) + 1, NULL, NULL);

  /* As a default, download both named fonts. */
  strhash_put (download_fonts, Fname, strlen (Fname) + 1, NULL, NULL);
  strhash_put (download_fonts, HFname, strlen (HFname) + 1, NULL, NULL);

  /* Read font's character widths and character types. */
  read_font_info ();

  /* Count the line indentation. */
  line_indent = parse_float (line_indent_spec, 1, 1);

  /* List media names. */
  if (list_media)
    {
      printf (_("known media:\n\
name             width\theight\tllx\tlly\turx\tury\n\
------------------------------------------------------------\n"));
      for (mentry = media_names; mentry; mentry = mentry->next)
	printf ("%-16s %d\t%d\t%d\t%d\t%d\t%d\n",
		mentry->name, mentry->w, mentry->h,
		mentry->llx, mentry->lly, mentry->urx, mentry->ury);
      /* Exit after listing. */
      exit (0);
    }

  /* Output media. */
  for (mentry = media_names; mentry; mentry = mentry->next)
    if (strcmp (media_name, mentry->name) == 0)
      {
	media = mentry;
	break;
      }
  if (media == NULL)
    FATAL ((stderr, _("do not know anything about media \"%s\""), media_name));

  if (margins_spec)
    {
      /* Adjust marginals. */
      for (i = 0; i < 4; i++)
	{
	  if (*margins_spec == '\0')
	    /* All done. */
	    break;

	  if (*margins_spec == ':')
	    {
	      margins_spec++;
	      continue;
	    }

	  j = atoi (margins_spec);
	  for (; *margins_spec != ':' && *margins_spec != '\0'; margins_spec++)
	    ;
	  if (*margins_spec == ':')
	    margins_spec++;

	  switch (i)
	    {
	    case 0:		/* left */
	      media->llx = j;
	      break;

	    case 1:		/* right */
	      media->urx = media->w - j;
	      break;

	    case 2:		/* top */
	      media->ury = media->h - j;
	      break;

	    case 3:		/* bottom */
	      media->lly = j;
	      break;
	    }
	}
      MESSAGE (1,
	       (stderr,
		_("set new marginals for media `%s' (%dx%d): llx=%d, lly=%d, urx=%d, ury=%d\n"),
		media->name, media->w, media->h, media->llx, media->lly,
		media->urx, media->ury));
    }

  /* Page label format. */
  if (MATCH (page_label_format, "short"))
    page_label = LABEL_SHORT;
  else if (MATCH (page_label_format, "long"))
    page_label = LABEL_LONG;
  else
    FATAL ((stderr, _("illegal page label format \"%s\""), page_label_format));

  /* Non-printable format. */
  if (MATCH (npf_name, "space"))
    non_printable_format = NPF_SPACE;
  else if (MATCH (npf_name, "questionmark"))
    non_printable_format = NPF_QUESTIONMARK;
  else if (MATCH (npf_name, "caret"))
    non_printable_format = NPF_CARET;
  else if (MATCH (npf_name, "octal"))
    non_printable_format = NPF_OCTAL;
  else
    FATAL ((stderr, _("illegal non-printable format \"%s\""), npf_name));

  /* Mark wrapped lines style. */
  if (mark_wrapped_lines_style_name)
    {
      if (MATCH (mark_wrapped_lines_style_name, "none"))
	mark_wrapped_lines_style = MWLS_NONE;
      else if (MATCH (mark_wrapped_lines_style_name, "plus"))
	mark_wrapped_lines_style = MWLS_PLUS;
      else if (MATCH (mark_wrapped_lines_style_name, "box"))
	mark_wrapped_lines_style = MWLS_BOX;
      else if (MATCH (mark_wrapped_lines_style_name, "arrow"))
	mark_wrapped_lines_style = MWLS_ARROW;
      else
	FATAL ((stderr, _("illegal style for wrapped line marker: \"%s\""),
		mark_wrapped_lines_style_name));
    }

  /* Count N-up stuffs. */
  for (i = 0; ; i++)
    {
      ui = nup >> i;

      if (ui == 0)
	FATAL ((stderr, _("illegal N-up argument: %d"), nup));

      if (ui & 0x1)
	{
	  if (ui != 1)
	    FATAL ((stderr, _("N-up argument must be power of 2: %d"), nup));

	  nup_exp = i;
	  break;
	}
    }

  nup_rows = nup_exp / 2 * 2;
  if (nup_rows == 0)
    nup_rows = 1;
  nup_columns = (nup_exp + 1) / 2 * 2;
  if (nup_columns == 0)
    nup_columns = 1;

  nup_landscape = nup_exp & 0x1;


  /*
   * Count output media dimensions.
   */

  if (landscape)
    {
      d_page_w = media->ury - media->lly;
      d_page_h = media->urx - media->llx;
    }
  else
    {
      d_page_w = media->urx - media->llx;
      d_page_h = media->ury - media->lly;
    }

  /*
   * Count N-up page width, height and scale.
   */

  if (nup_landscape)
    {
      nup_width = media->ury - media->lly;
      nup_height = media->urx - media->llx;
    }
  else
    {
      nup_width = media->urx - media->llx;
      nup_height = media->ury - media->lly;
    }

  {
    double w, h;

    w = ((double) nup_width - (nup_columns - 1) * nup_xpad) / nup_columns;
    h = ((double) nup_height - (nup_rows - 1) * nup_ypad) / nup_rows;

    nup_width = w;
    nup_height = h;

    w = w / (media->urx - media->llx);
    h = h / (media->ury - media->lly);

    nup_scale = w < h ? w : h;
  }

  /*
   * Underlay (this must come after output media dimensions, because
   * `underlay position' needs them).
   */
  if (underlay != NULL)
    {
      strhash_put (res_fonts, ul_font, strlen (ul_font) + 1, NULL, NULL);
      underlay = escape_string (underlay);
    }

  /* Underlay X-coordinate. */
  ul_x = strtod (ul_position, &cp);
  if (cp == ul_position)
    {
    malformed_position:
      FATAL ((stderr, _("malformed underlay position: %s"), ul_position));
    }
  if (ul_position[0] == '-')
    ul_x += d_page_w;

  /* Underlay Y-coordinate. */
  ul_y = strtod (cp, &cp2);
  if (cp2 == cp)
    goto malformed_position;
  if (cp[0] == '-')
    ul_y += d_page_h;

  /* Underlay Angle. */
  if (!ul_angle_p)
    /* No angle given, count the default. */
    ul_angle = (atan2 (-d_page_h, d_page_w) / 3.14159265 * 180);

  /* Underlay style. */
  if (strcmp (ul_style_str, "outline") == 0)
    ul_style = UL_STYLE_OUTLINE;
  else if (strcmp (ul_style_str, "filled") == 0)
    ul_style = UL_STYLE_FILLED;
  else
    FATAL ((stderr, _("illegal underlay style: %s"), ul_style_str));

  /*
   * Header.  Note! The header attributes can be changed from
   * the `.hdr' files, these are only the defaults.
   */

  d_header_w = d_page_w;
  switch (header)
    {
    case HDR_NONE:
      d_header_h = 0;
      break;

    case HDR_SIMPLE:
      d_header_h = HFpt.h * 1.5;
      break;

    case HDR_FANCY:
      d_header_h = 36;
      break;
    }

  /* Help highlight. */
  if (help_highlight)
    {
      /* Create description with states. */
      printf (_("Highlighting is supported for the following languages and file formats:\n\n"));
      fflush (stdout);

      buffer_clear (&buffer);
      buffer_append (&buffer, states_binary);
      buffer_append (&buffer, " -f \"");
      buffer_append (&buffer, states_config_file);
      buffer_append (&buffer, "\" -p \"");
      buffer_append (&buffer, states_path);
      buffer_append (&buffer, "\" -s describe_languages ");
      buffer_append (&buffer, enscript_library);
      buffer_append (&buffer, "/hl/*.st");

      system (buffer_ptr (&buffer));
      exit (0);
    }

  /*
   * And now to the main business.  The actual input file processing
   * is divided to two parts: PostScript generation and everything else.
   * The PostScript generation is handled in the conventional way, we
   * process the input and generate PostScript.  However all other input
   * languages will be handled with States, we only pass enscript's
   * options to the states pre-filter and dump output.
   */
  if (output_language_pass_through)
    {
      char *start_state;
      Buffer cmd;
      char intbuf[256];

      /* The States output generation. */

      /* Resolve the start state. */
      if (hl_start_state)
	start_state = hl_start_state;
      else if (highlight)
	start_state = NULL;
      else
	start_state = "passthrough";

      /* Create the states command. */

      buffer_init (&cmd);

      buffer_append (&cmd, states_binary);
      buffer_append (&cmd, " -f \"");
      buffer_append (&cmd, states_config_file);
      buffer_append (&cmd, "\" -p \"");
      buffer_append (&cmd, states_path);
      buffer_append (&cmd, "\" ");

      if (verbose > 0)
	buffer_append (&cmd, "-v ");

      if (start_state)
	{
	  buffer_append (&cmd, "-s");
	  buffer_append (&cmd, start_state);
	  buffer_append (&cmd, " ");
	}

      buffer_append (&cmd, "-Dcolor=");
      buffer_append (&cmd, states_color ? "1" : "0");
      buffer_append (&cmd, " ");

      buffer_append (&cmd, "-Dstyle=");
      buffer_append (&cmd, states_highlight_style);
      buffer_append (&cmd, " ");

      buffer_append (&cmd, "-Dlanguage=");
      buffer_append (&cmd, output_language);
      buffer_append (&cmd, " ");

      buffer_append (&cmd, "-Dnum_input_files=");
      sprintf (intbuf, "%d", optind == argc ? 1 : argc - optind);
      buffer_append (&cmd, intbuf);
      buffer_append (&cmd, " ");

      buffer_append (&cmd, "-Ddocument_title=\'");
      if ((cp = shell_escape (title)) != NULL)
	{
	  buffer_append (&cmd, cp);
	  free (cp);
	}
      buffer_append (&cmd, "\' ");

      buffer_append (&cmd, "-Dtoc=");
      buffer_append (&cmd, toc ? "1" : "0");

      /* Additional options for states? */
      if (helper_options['s'])
	{
	  Buffer *opts = helper_options['s'];

	  buffer_append (&cmd, " ");
	  buffer_append_len (&cmd, buffer_ptr (opts), buffer_len (opts));
	}

      /* Append input files. */
      for (i = optind; i < argc; i++)
	{
	  char *cp;
	  if ((cp = shell_escape (argv[i])) != NULL)
	    {
	      buffer_append (&cmd, " \'");
	      buffer_append (&cmd, cp);
	      buffer_append (&cmd, "\'");
	      free (cp);
	    }
	}

      /* And do the job. */
      if (is_open (&is, stdin, NULL, buffer_ptr (&cmd)))
	{
	  open_output_file ();
	  process_file ("unused", &is, 0);
	  is_close (&is);
	}

      buffer_uninit (&cmd);
    }
  else
    {
      /* The conventional way. */

      /* Highlighting. */
      if (highlight)
	{
	  char fbuf[256];

	  /* Create a highlight input filter. */
	  buffer_clear (&buffer);
	  buffer_append (&buffer, states_binary);
	  buffer_append (&buffer, " -f \"");
	  buffer_append (&buffer, states_config_file);
	  buffer_append (&buffer, "\" -p \"");
	  buffer_append (&buffer, states_path);
	  buffer_append (&buffer, "\"");

	  if (verbose > 0)
	    buffer_append (&buffer, " -v");

	  if (hl_start_state)
	    {
	      buffer_append (&buffer, " -s ");
	      buffer_append (&buffer, hl_start_state);
	    }

	  buffer_append (&buffer, " -Dcolor=");
	  buffer_append (&buffer, states_color ? "1" : "0");

	  buffer_append (&buffer, " -Dstyle=");
	  buffer_append (&buffer, states_highlight_style);

	  buffer_append (&buffer, " -Dfont_spec=");
	  buffer_append (&buffer, Fname);
	  sprintf (fbuf, "@%g/%g", Fpt.w, Fpt.h);
	  buffer_append (&buffer, fbuf);

	  /* Additional options for states? */
	  if (helper_options['s'])
	    {
	      Buffer *opts = helper_options['s'];

	      buffer_append (&buffer, " ");
	      buffer_append_len (&buffer,
				 buffer_ptr (opts), buffer_len (opts));
	    }

	  buffer_append (&buffer, " \'%s\'");

	  input_filter = buffer_copy (&buffer);
	  input_filter_stdin = "-";
	}

      /* Table of Contents. */
      if (toc)
	{
	  toc_fp = tmpfile ();
	  if (toc_fp == NULL)
	    FATAL ((stderr, _("couldn't create temporary toc file: %s"),
		    strerror (errno)));
	}


      /*
       * Process files.
       */

      if (optind == argc)
	{
	  /* stdin's modification time is the current time. */
	  memcpy (&mod_tm, &run_tm, sizeof (run_tm));

	  if (is_open (&is, stdin, NULL, input_filter))
	    {
	      /* Open output file. */
	      open_output_file ();
	      process_file (title_given ? title : "", &is, 0);
	      is_close (&is);
	    }
	}
      else
	{
	  for (; optind < argc; optind++)
	    {
	      if (is_open (&is, NULL, argv[optind], input_filter))
		{
		  struct stat stat_st;

		  /* Get modification time. */
		  if (stat (argv[optind], &stat_st) == 0)
		    {
		      tim = stat_st.st_mtime;
		      tm = localtime (&tim);
		      memcpy (&mod_tm, tm, sizeof (*tm));

		      /*
		       * Open output file.  Output file opening is delayed to
		       * this point so we can optimize the case when a
		       * non-existing input file is printed => we do nothing.
		       */
		      open_output_file ();

		      process_file (argv[optind], &is, 0);
		    }
		  else
		    ERROR ((stderr, _("couldn't stat input file \"%s\": %s"),
			    argv[optind],
			    strerror (errno)));

		  is_close (&is);
		}
	    }
	}

      /* Table of Contents. */
      if (toc)
	{
	  /* This is really cool... */

	  /* Set the printing options for toc. */
	  toc = 0;
	  special_escapes = 1;
	  line_numbers = 0;

	  if (fseek (toc_fp, 0, SEEK_SET) != 0)
	    FATAL ((stderr, _("couldn't rewind toc file: %s"),
		    strerror (errno)));

	  memcpy (&mod_tm, &run_tm, sizeof (run_tm));
	  if (is_open (&is, toc_fp, NULL, NULL))
	    {
	      process_file (_("Table of Contents"), &is, 1);
	      is_close (&is);
	    }

	  /* Clean up toc file. */
	  fclose (toc_fp);
	}

      /* Give trailer a chance to dump itself. */
      dump_ps_trailer ();

      /*
       * Append ^D to the end of the output?  Note! It must be ^D followed
       * by a newline.
       */
      if (ofp != NULL && append_ctrl_D)
	fprintf (ofp, "\004\n");
    }

  /* Close output file. */
  close_output_file ();

  /* Tell how things went. */
  if (ofp == NULL)
    {
      /*
       * The value of <ofp> is not reset in close_output_file(),
       * this is ugly but it saves one flag.
       */
      MESSAGE (0, (stderr, _("no output generated\n")));
    }
  else if (output_language_pass_through)
    {
      if (output_file == OUTPUT_FILE_NONE)
	MESSAGE (0, (stderr, _("output sent to %s\n"),
		     printer ? printer : _("printer")));
      else
	MESSAGE (0, (stderr, _("output left in %s\n"),
		     output_file == OUTPUT_FILE_STDOUT ? "-" : output_file));
    }
  else
    {
      unsigned int real_total_pages;

      if (nup > 1)
	{
	  if (total_pages > 0)
	    real_total_pages = (total_pages - 1) / nup + 1;
	  else
	    real_total_pages = 0;
	}
      else
	real_total_pages = total_pages;

      /* We did something, tell what.  */
      MESSAGE (0, (stderr, _("[ %d pages * %d copy ]"), real_total_pages,
		   num_copies));
      if (output_file == OUTPUT_FILE_NONE)
	MESSAGE (0, (stderr, _(" sent to %s\n"),
		     printer ? printer : _("printer")));
      else
	MESSAGE (0, (stderr, _(" left in %s\n"),
		     output_file == OUTPUT_FILE_STDOUT ? "-" : output_file));
      if (num_truncated_lines)
	{
	  retval |= 2;
	  MESSAGE (0, (stderr, _("%d lines were %s\n"), num_truncated_lines,
		       line_end == LE_TRUNCATE
		       ? _("truncated") : _("wrapped")));
	}

      if (num_missing_chars)
	{
	  retval |= 4;
	  MESSAGE (0, (stderr, _("%d characters were missing\n"),
		       num_missing_chars));
	  if (list_missing_characters)
	    {
	      MESSAGE (0, (stderr, _("missing character codes (decimal):\n")));
	      do_list_missing_characters (missing_chars);
	    }
	}

      if (num_non_printable_chars)
	{
	  retval |= 8;
	  MESSAGE (0, (stderr, _("%d non-printable characters\n"),
		       num_non_printable_chars));
	  if (list_missing_characters)
	    {
	      MESSAGE (0, (stderr,
			   _("non-printable character codes (decimal):\n")));
	      do_list_missing_characters (non_printable_chars);
	    }
	}
    }

  /* Uninit our dynamic memory buffer. */
  buffer_uninit (&buffer);

  /* Return the extended return values only if requested. */
  if (!extended_return_values)
    retval = 0;

  /* This is the end. */
  return retval;
}


/*
 * Static functions.
 */

static void
open_output_file ()
{
  if (ofp)
    /* Output file has already been opened, do nothing. */
    return;

  if (output_file == OUTPUT_FILE_NONE)
    {
      char spooler_options[512];

      /* Format spooler options. */
      spooler_options[0] = '\0';
      if (mail)
	strcat (spooler_options, "-m ");
      if (no_job_header)
	{
	  strcat (spooler_options, no_job_header_switch);
	  strcat (spooler_options, " ");
	}
      if (printer_options)
	strcat (spooler_options, printer_options);

      /* Open printer. */
      ofp = printer_open (spooler_command, spooler_options, queue_param,
			  printer, &printer_context);
      if (ofp == NULL)
	FATAL ((stderr, _("couldn't open printer `%s': %s"), printer,
		strerror (errno)));
    }
  else if (output_file == OUTPUT_FILE_STDOUT)
    ofp = stdout;
  else
    {
      ofp = fopen (output_file, "w");
      if (ofp == NULL)
	FATAL ((stderr, _("couldn't create output file \"%s\": %s"),
		output_file, strerror (errno)));
    }
}


static void
close_output_file ()
{
  if (ofp == NULL)
    /* Output file hasn't been opened, we are done. */
    return;

  if (output_file == OUTPUT_FILE_NONE)
    printer_close (printer_context);
  else if (output_file != OUTPUT_FILE_STDOUT)
    if (fclose (ofp))
      FATAL ((stderr, _("couldn't close output file \"%s\": %s"),
	      output_file, strerror (errno)));

  /* We do not reset <ofp> since its value is needed in diagnostigs. */
}


static void
handle_env_options (char *var)
{
  int argc;
  char **argv;
  char *string;
  char *str;
  int i;

  string = getenv (var);
  if (string == NULL)
    return;

  MESSAGE (2, (stderr, "handle_env_options(): %s=\"%s\"\n", var, string));

  /* Copy string so we can modify it in place. */
  str = xstrdup (string);

  /*
   * We can count this, each option takes at least 1 character and one
   * space.  We also need one for program's name and one for the
   * trailing NULL.
   */
  argc = (strlen (str) + 1) / 2 + 2;
  argv = xcalloc (argc, sizeof (char *));

  /* Set program name. */
  argc = 0;
  argv[argc++] = program;

  /* Split string and set arguments to argv array. */
  i = 0;
  while (str[i])
    {
      /* Skip leading whitespace. */
      for (; str[i] && isspace (str[i]); i++)
	;
      if (!str[i])
	break;

      /* Check for quoted arguments. */
      if (str[i] == '"' || str[i] == '\'')
	{
	  int endch = str[i++];

	  argv[argc++] = str + i;

	  /* Skip until we found the end of the quotation. */
	  for (; str[i] && str[i] != endch; i++)
	    ;
	  if (!str[i])
	    FATAL ((stderr, _("syntax error in option string %s=\"%s\":\n\
missing end of quotation: %c"), var, string, endch));

	  str[i++] = '\0';
	}
      else
	{
	  argv[argc++] = str + i;

	  /* Skip until whitespace if found. */
	  for (; str[i] && !isspace (str[i]); i++)
	    ;
	  if (str[i])
	    str[i++] = '\0';
	}
    }

  /* argv[argc] must be NULL. */
  argv[argc] = NULL;

  MESSAGE (2, (stderr, "found following options (argc=%d):\n", argc));
  for (i = 0; i < argc; i++)
    MESSAGE (2, (stderr, "%3d = \"%s\"\n", i, argv[i]));

  /* Process options. */
  handle_options (argc, argv);

  /* Check that all got processed. */
  if (optind != argc)
    {
      MESSAGE (0,
	       (stderr,
		_("warning: didn't process following options from \
environment variable %s:\n"),
		var));
      for (; optind < argc; optind++)
	MESSAGE (0, (stderr, _("  option %d = \"%s\"\n"), optind,
		     argv[optind]));
    }

  /* Cleanup. */
  xfree (argv);

  /*
   * <str> must not be freed, since some global variables can point to
   * its elements
   */
}


static void
handle_options (int argc, char *argv[])
{
  int c;
  PageRange *prange;

  /* Reset optind. */
  optind = 0;

  while (1)
    {
      int option_index = 0;
      const char *cp;
      int i;

      c = getopt_long (argc, argv,
		       "#:123456789a:A:b:BcC::d:D:e::E::f:F:gGhH::i:I:jJ:kKlL:mM:n:N:o:Op:P:qrRs:S:t:T:u::U:vVW:X:zZ",
		       long_options, &option_index);

      if (c == -1)
	break;

      switch (c)
	{
	case 0:			/* Long option found. */
	  cp = long_options[option_index].name;

	  if (strcmp (cp, "columns") == 0)
	    {
	      num_columns = atoi (optarg);
	      if (num_columns < 1)
		FATAL ((stderr,
			_("number of columns must be larger than zero")));
	    }
	  break;

	  /* Short options. */

	case '1':		/* 1 column */
	case '2':		/* 2 columns */
	case '3':		/* 3 columns */
	case '4':		/* 4 columns */
	case '5':		/* 5 columns */
	case '6':		/* 6 columns */
	case '7':		/* 7 columns */
	case '8':		/* 8 columns */
	case '9':		/* 9 columns */
	  num_columns = c - '0';
	  break;

	case 'a':		/* pages */
	  prange = (PageRange *) xcalloc (1, sizeof (PageRange));

	  if (strcmp (optarg, "odd") == 0)
	    prange->odd = 1;
	  else if (strcmp (optarg, "even") == 0)
	    prange->even = 1;
	  else
	    {
	      cp = strchr (optarg, '-');
	      if (cp)
		{
		  if (optarg[0] == '-')
		    /* -end */
		    prange->end = atoi (optarg + 1);
		  else if (cp[1] == '\0')
		    {
		      /* begin- */
		      prange->start = atoi (optarg);
		      prange->end = (unsigned int) -1;
		    }
		  else
		    {
		      /* begin-end */
		      prange->start = atoi (optarg);
		      prange->end = atoi (cp + 1);
		    }
		}
	      else
		/* pagenumber */
		prange->start = prange->end = atoi (optarg);
	    }

	  prange->next = page_ranges;
	  page_ranges = prange;
	  break;

	case 'A':		/* file alignment */
	  file_align = atoi (optarg);
	  if (file_align == 0)
	    FATAL ((stderr, _("file alignment must be larger than zero")));
	  break;

	case 'b':		/* page header */
	  page_header = optarg;
	  break;

	case 'B':		/* no page headers */
	  header = HDR_NONE;
	  break;

	case 'c':		/* truncate (cut) long lines */
	  line_end = LE_TRUNCATE;
	  break;

	case 'C':		/* line numbers */
	  line_numbers = 1;
	  if (optarg)
	    start_line_number = atoi (optarg);
	  break;

	case 'd':		/* specify printer */
	case 'P':
	  xfree (printer);
	  printer = xstrdup (optarg);
	  output_file = OUTPUT_FILE_NONE;
	  break;

	case 'D':		/* setpagedevice */
	  parse_key_value_pair (pagedevice, optarg);
	  break;

	case 'e':		/* special escapes */
	  special_escapes = 1;
	  if (optarg)
	    {
	      /* Specify the escape character. */
	      if (isdigit (optarg[0]))
		/* As decimal, octal, or hexadicimal number. */
		escape_char = (int) strtoul (optarg, NULL, 0);
	      else
		/* As character directly. */
		escape_char = ((unsigned char *) optarg)[0];
	    }
	  break;

	case 'E':		/* highlight */
	  highlight = 1;
	  special_escapes = 1;
	  escape_char = '\0';
	  hl_start_state = optarg;
	  break;

	case 'f':		/* font */
	  if (!parse_font_spec (optarg, &Fname, &Fpt, NULL))
	    FATAL ((stderr, _("malformed font spec: %s"), optarg));
	  user_body_font_defined = 1;
	  break;

	case 'F':		/* header font */
	  if (!parse_font_spec (optarg, &HFname, &HFpt, NULL))
	    FATAL ((stderr, _("malformed font spec: %s"), optarg));
	  break;

	case 'g':		/* print anyway */
	  /* nothing. */
	  break;

	case 'G':		/* fancy header */
	  header = HDR_FANCY;
	  if (optarg)
	    fancy_header_name = optarg;
	  else
	    fancy_header_name = fancy_header_default;

	  if (!file_existsp (fancy_header_name, ".hdr"))
	    FATAL ((stderr,
		    _("couldn't find header definition file \"%s.hdr\""),
		    fancy_header_name));
	  break;

	case 'h':		/* no job header */
	  no_job_header = 1;
	  break;

	case 'H':		/* highlight bars */
	  if (optarg)
	    highlight_bars = atoi (optarg);
	  else
	    highlight_bars = 2;
	  break;

	case 'i':		/* line indent */
	  line_indent_spec = optarg;
	  break;

	case 'I':		/* input filter */
	  input_filter = optarg;
	  break;

	case 'j':		/* borders */
	  borders = 1;
	  break;

	case 'k':		/* enable page prefeed */
	  page_prefeed = 1;
	  break;

	case 'K':		/* disable page prefeed */
	  page_prefeed = 0;
	  break;

	case 'l':		/* emulate lineprinter */
	  lines_per_page = 66;
	  header = HDR_NONE;
	  break;

	case 'L':		/* lines per page */
	  lines_per_page = atoi (optarg);
	  if (lines_per_page <= 0)
	    FATAL ((stderr,
		    _("must print at least one line per each page: %s"),
		    argv[optind]));
	  break;

	case 'm':		/* send mail upon completion */
	  mail = 1;
	  break;

	case 'M':		/* select output media */
	  media_name = xstrdup (optarg);
	  break;

	case 'n':		/* num copies */
	case '#':
	  num_copies = atoi (optarg);
	  break;

	case 'N':		/* newline character */
	  if (!(optarg[0] == 'n' || optarg[0] == 'r') || optarg[1] != '\0')
	    {
	      fprintf (stderr, _("%s: illegal newline character specifier: \
'%s': expected 'n' or 'r'\n"),
		       program, optarg);
	      goto option_error;
	    }
	  if (optarg[0] == 'n')
	    nl = '\n';
	  else
	    nl = '\r';
	  break;

	case 'o':
	case 'p':		/* output file */
	  /* Check output file "-". */
	  if (strcmp (optarg, "-") == 0)
	    output_file = OUTPUT_FILE_STDOUT;
	  else
	    output_file = optarg;
	  break;

	case 'O':		/* list missing characters */
	  list_missing_characters = 1;
	  break;

	case 'q':		/* quiet */
	  quiet = 1;
	  verbose = 0;
	  break;

	case 'r':		/* landscape */
	  landscape = 1;
	  break;

	case 'R':		/* portrait */
	  landscape = 0;
	  break;

	case 's':		/* baselineskip */
	  baselineskip = atof (optarg);
	  break;

	case 'S':		/* statusdict */
	  parse_key_value_pair (statusdict, optarg);
	  break;

	case 't':		/* title */
	case 'J':
	  title = optarg;
	  title_given = 1;
	  break;

	case 'T':		/* tabulator size */
	  tabsize = atoi (optarg);
	  if (tabsize <= 0)
	    tabsize = 1;
	  break;

	case 'u':		/* underlay */
	  underlay = optarg;
	  break;

	case 'U':		/* nup */
	  nup = atoi (optarg);
	  break;

	case 'v':		/* verbose */
	  if (optarg)
	    verbose = atoi (optarg);
	  else
	    verbose++;
	  quiet = 0;
	  break;

	case 'V':		/* version */
	  version ();
	  exit (0);
	  break;

	case 'w':		/* output language */
	  output_language = optarg;
	  if (strcmp (output_language, "PostScript") != 0)
	    /* Other output languages are handled with states. */
	    output_language_pass_through = 1;
	  break;

	case 'W':		/* a helper application option */
	  cp = strchr (optarg, ',');
	  if (cp == NULL)
	    FATAL ((stderr,
		    _("malformed argument `%s' for option -W, --option: \
no comma found"),
		      optarg));

	  if (cp - optarg != 1)
	    FATAL ((stderr, _("helper application specification must be \
single character: %s"),
			      optarg));

	  /* Take the index of the helper application and update `cp'
             to point to the beginning of the option. */
	  i = *optarg;
	  cp++;

	  if (helper_options[i] == NULL)
	    helper_options[i] = buffer_alloc ();
	  else
	    {
	      /* We already had some options for this helper
                 application.  Let's separate these arguments. */
	      buffer_append (helper_options[i], " ");
	    }

	  /* Add this new option. */
	  buffer_append (helper_options[i], cp);
	  break;

	case 'X':		/* input encoding */
	  xfree (encoding_name);
	  encoding_name = xstrdup (optarg);
	  break;

	case 'z':		/* no form feeds */
	  interpret_formfeed = 0;
	  break;

	case 'Z':		/* pass through */
	  pass_through = 1;
	  break;

	case 128:		/* underlay font */
	  if (!parse_font_spec (optarg, &ul_font, &ul_ptsize, NULL))
	    FATAL ((stderr, _("malformed font spec: %s"), optarg));
	  break;

	case 129:		/* underlay gray */
	  ul_gray = atof (optarg);
	  break;

	case 130:		/* page label format */
	  xfree (page_label_format);
	  page_label_format = xstrdup (optarg);
	  break;

	case 131:		/* download font */
	  strhash_put (download_fonts, optarg, strlen (optarg) + 1, NULL,
		       NULL);
	  break;

	case 132:		/* underlay angle */
	  ul_angle = atof (optarg);
	  ul_angle_p = 1;
	  break;

	case 133:		/* underlay position */
	  xfree (ul_position);
	  ul_position = xstrdup (optarg);
	  ul_position_p = 1;
	  break;

	case 134:		/* non-printable format */
	  xfree (npf_name);
	  npf_name = xstrdup (optarg);
	  break;

	case 135:		/* help */
	  usage ();
	  exit (0);
	  break;

	case 136:		/* highlight bar gray */
	  highlight_bar_gray = atof (optarg);
	  break;

	case 137:		/* underlay style */
	  xfree (ul_style_str);
	  ul_style_str = xstrdup (optarg);
	  break;

	case 138:		/* filter stdin */
	  input_filter_stdin = optarg;
	  break;

	case 139:		/* extra options for the printer spooler */
	  printer_options = optarg;
	  break;

	case 140:		/* slicing */
	  slicing = 1;
	  slice = atoi (optarg);
	  if (slice <= 0)
	    FATAL ((stderr, _("slice must be greater than zero")));
	  break;

	case 141:		/* help-highlight */
	  help_highlight = 1;
	  break;

	case 142:		/* States color? */
	  if (optarg == NULL)
	    states_color = 1;
	  else
	    states_color = atoi (optarg);
	  break;

	case 143:		/* mark-wrapped-lines */
	  if (optarg)
	    {
	      xfree (mark_wrapped_lines_style_name);
	      mark_wrapped_lines_style_name = xstrdup (optarg);
	    }
	  else
	    /* Set the system default. */
	    mark_wrapped_lines_style = MWLS_BOX;
	  break;

	case 144:		/* adjust margins */
	  margins_spec = optarg;
	  break;

	case 145:		/* N-up x-pad */
	  nup_xpad = atoi (optarg);
	  break;

	case 146:		/* N-up y-pad */
	  nup_ypad = atoi (optarg);
	  break;

	case 147:		/* word wrap */
	  line_end = LE_WORD_WRAP;
	  break;

	case 148:		/* horizontal column height */
	  horizontal_column_height = atof (optarg);
	  formfeed_type = FORMFEED_HCOLUMN;
	  break;

	case 149:		/* PostScript language level */
	  pslevel = atoi (optarg);
	  break;

	case 150:		/* rotate even-numbered pages */
	  rotate_even_pages = 1;
	  break;

	case 151:		/* highlight style */
	  xfree (states_highlight_style);
	  states_highlight_style = xstrdup (optarg);
	  break;

	case 152:		/* N-up colunwise */
	  nup_columnwise = 1;
	  break;

	case 153:		/* swap even page margins */
	  swap_even_page_margins = 1;
	  break;

	case 154:		/* extended return values */
	  extended_return_values = 1;
	  break;

	case 155:		/* footer */
	  page_footer = optarg;
	  break;

	case 156:		/* continuous page numbers */
	  continuous_page_numbers = 1;
	  break;

	case '?':		/* Errors found during getopt_long(). */
	option_error:
	  fprintf (stderr, _("Try `%s --help' for more information.\n"),
		   program);
	  exit (1);
	  break;

	default:
	  printf ("Hey!  main() didn't handle option \"%c\" (%d)", c, c);
	  if (optarg)
	    printf (" with arg %s", optarg);
	  printf ("\n");
	  FATAL ((stderr, "This is a bug!"));
	  break;
	}
    }
}


static void
usage ()
{
  printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
Mandatory arguments to long options are mandatory for short options too.\n\
  -#                         an alias for option -n, --copies\n\
  -1                         same as --columns=1\n\
  -2                         same as --columns=2\n\
      --columns=NUM          specify the number of columns per page\n\
  -a, --pages=PAGES          specify which pages are printed\n\
  -A, --file-align=ALIGN     align separate input files to ALIGN\n\
  -b, --header=HEADER        set page header\n\
  -B, --no-header            no page headers\n\
  -c, --truncate-lines       cut long lines (default is to wrap)\n\
  -C, --line-numbers[=START]\n\
                             precede each line with its line number\n\
  -d                         an alias for option --printer\n\
  -D, --setpagedevice=KEY[:VALUE]\n\
                             pass a page device definition to output\n\
  -e, --escapes[=CHAR]       enable special escape interpretation\n"),
          program);

  printf (_("\
  -E, --highlight[=LANG]     highlight source code\n"));

  printf (_("\
  -f, --font=NAME            use font NAME for body text\n\
  -F, --header-font=NAME     use font NAME for header texts\n\
  -g, --print-anyway         nothing (compatibility option)\n\
  -G                         same as --fancy-header\n\
      --fancy-header[=NAME]  select fancy page header\n\
  -h, --no-job-header        suppress the job header page\n\
  -H, --highlight-bars=NUM   specify how high highlight bars are\n\
  -i, --indent=NUM           set line indent to NUM characters\n\
  -I, --filter=CMD           read input files through input filter CMD\n\
  -j, --borders              print borders around columns\n\
  -J,                        an alias for option --title\n\
  -k, --page-prefeed         enable page prefeed\n\
  -K, --no-page-prefeed      disable page prefeed\n\
  -l, --lineprinter          simulate lineprinter, this is an alias for:\n\
                               --lines-per-page=66, --no-header, --portrait,\n\
                               --columns=1\n"));

  printf (_("\
  -L, --lines-per-page=NUM   specify how many lines are printed on each page\n\
  -m, --mail                 send mail upon completion\n\
  -M, --media=NAME           use output media NAME\n\
  -n, --copies=NUM           print NUM copies of each page\n\
  -N, --newline=NL           select the newline character.  Possible\n\
                             values for NL are: n (`\\n') and r (`\\r').\n\
  -o                         an alias for option --output\n\
  -O, --missing-characters   list missing characters\n\
  -p, --output=FILE          leave output to file FILE.  If FILE is `-',\n\
                             leave output to stdout.\n\
  -P, --printer=NAME         print output to printer NAME\n\
  -q, --quiet, --silent      be really quiet\n\
  -r, --landscape            print in landscape mode\n\
  -R, --portrait             print in portrait mode\n"));

  printf (_("\
  -s, --baselineskip=NUM     set baselineskip to NUM\n\
  -S, --statusdict=KEY[:VALUE]\n\
                             pass a statusdict definition to the output\n\
  -t, --title=TITLE          set banner page's job title to TITLE.  Option\n\
                             sets also the name of the input file stdin.\n\
  -T, --tabsize=NUM          set tabulator size to NUM\n\
  -u, --underlay[=TEXT]      print TEXT under every page\n\
  -U, --nup=NUM              print NUM logical pages on each output page\n\
  -v, --verbose              tell what we are doing\n\
  -V, --version              print version number\n\
  -w, --language=LANG        set output language to LANG\n\
  -W, --options=APP,OPTION   pass option OPTION to helper application APP\n\
  -X, --encoding=NAME        use input encoding NAME\n\
  -z, --no-formfeed          do not interpret form feed characters\n\
  -Z, --pass-through         pass through PostScript and PCL files\n\
                             without any modifications\n"));

  printf (_("Long-only options:\n\
  --color[=bool]             create color outputs with states\n\
  --continuous-page-numbers  count page numbers across input files.  Don't\n\
                             restart numbering at beginning of each file.\n\
  --download-font=NAME       download font NAME\n\
  --extended-return-values   enable extended return values\n\
  --filter-stdin=NAME        specify how stdin is shown to the input filter\n\
  --footer=FOOTER            set page footer\n\
  --h-column-height=HEIGHT   set the horizontal column height to HEIGHT\n\
  --help                     print this help and exit\n"));

  printf (_("\
  --help-highlight           describe all supported --highlight languages\n\
                             and file formats\n\
  --highlight-bar-gray=NUM   print highlight bars with gray NUM (0 - 1)\n\
  --list-media               list names of all known media\n\
  --margins=LEFT:RIGHT:TOP:BOTTOM\n\
                             adjust page marginals\n\
  --mark-wrapped-lines[STYLE]\n\
                             mark wrapped lines in the output with STYLE\n\
  --non-printable-format=FMT specify how non-printable chars are printed\n"));

  printf (_("\
  --nup-columnwise           layout pages in the N-up printing columnwise\n\
  --nup-xpad=NUM             set the page x-padding of N-up printing to NUM\n\
  --nup-ypad=NUM             set the page y-padding of N-up printing to NUM\n\
  --page-label-format=FMT    set page label format to FMT\n\
  --ps-level=LEVEL           set the PostScript language level that enscript\n\
                             should use\n\
  --printer-options=OPTIONS  pass extra options to the printer command\n\
  --rotate-even-pages        rotate even-numbered pages 180 degrees\n"));

  printf (_("\
  --slice=NUM                print vertical slice NUM\n\
  --style=STYLE              use highlight style STYLE\n\
  --swap-even-page-margins   swap left and right side margins for each even\n\
                             numbered page\n\
  --toc                      print table of contents\n\
  --ul-angle=ANGLE           set underlay text's angle to ANGLE\n\
  --ul-font=NAME             print underlays with font NAME\n\
  --ul-gray=NUM              print underlays with gray value NUM\n\
  --ul-position=POS          set underlay's starting position to POS\n\
  --ul-style=STYLE           print underlays with style STYLE\n\
  --word-wrap                wrap long lines from word boundaries\n\
"));

  printf (_("\nReport bugs to mtr@iki.fi.\n"));
}


static void
version ()
{
  printf ("%s\n\
Copyright (C) 2003 Markku Rossi.\n\
GNU enscript comes with NO WARRANTY, to the extent permitted by law.\n\
You may redistribute copies of GNU enscript under the terms of the GNU\n\
General Public License.  For more information about these matters, see\n\
the files named COPYING.\n",
	  version_string);
}
