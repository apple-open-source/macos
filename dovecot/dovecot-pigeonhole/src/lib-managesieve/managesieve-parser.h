/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_PARSER_H
#define __MANAGESIEVE_PARSER_H

/*
 * QUOTED-SPECIALS    = <"> / "\"
 */
#define IS_QUOTED_SPECIAL(c) \
	((c) == '"' || (c) == '\\')

/* 
 * ATOM-SPECIALS      = "(" / ")" / "{" / SP / CTL / QUOTED-SPECIALS
 */
#define IS_ATOM_SPECIAL(c) \
	((c) == '(' || (c) == ')' || (c) == '{' || \
	 (c) <= 32 || (c) == 0x7f || \
	 IS_QUOTED_SPECIAL(c)) 

/* 
 * CHAR               = %x01-7F
 */
#define IS_CHAR(c) \
	(((c) & 0x80) == 0)

/* 
 * TEXT-CHAR          = %x01-09 / %x0B-0C / %x0E-7F
 *                       ;; any CHAR except CR and LF
 */
#define IS_TEXT_CHAR(c) \
	(IS_CHAR(c) && (c) != '\r' && (c) != '\n')

/*
 * SAFE-CHAR          = %x01-09 / %x0B-0C / %x0E-21 /
 *                      %x23-5B / %x5D-7F
 *                      ;; any TEXT-CHAR except QUOTED-SPECIALS
 */
#define IS_SAFE_CHAR(c) \
	(IS_TEXT_CHAR(c) && !IS_QUOTED_SPECIAL(c))

enum managesieve_parser_flags {
	/* Set this flag if you wish to read only size of literal argument
	   and not convert literal into string. Useful when you need to deal
	   with large literal sizes. The literal must be the last read
	   parameter. */
	MANAGESIEVE_PARSE_FLAG_LITERAL_SIZE	= 0x01,
	/* Don't remove '\' chars from string arguments */
	MANAGESIEVE_PARSE_FLAG_NO_UNESCAPE	= 0x02,
	/* Return literals as MANAGESIEVE_ARG_LITERAL instead of MANAGESIEVE_ARG_STRING */
	MANAGESIEVE_PARSE_FLAG_LITERAL_TYPE	= 0x04
};

enum managesieve_arg_type {
	MANAGESIEVE_ARG_ATOM = 0,
	MANAGESIEVE_ARG_STRING,

	/* literals are returned as MANAGESIEVE_ARG_STRING by default */
	MANAGESIEVE_ARG_LITERAL,
	MANAGESIEVE_ARG_LITERAL_SIZE,

	MANAGESIEVE_ARG_EOL /* end of argument list */
};

struct managesieve_parser;

struct managesieve_arg {
	enum managesieve_arg_type type;

	union {
		char *str;
		uoff_t literal_size;
	} _data;
};

#define MANAGESIEVE_ARG_STR(arg) \
	((arg)->type == MANAGESIEVE_ARG_STRING || \
   (arg)->type == MANAGESIEVE_ARG_ATOM || \
	 (arg)->type == MANAGESIEVE_ARG_LITERAL ? \
	 (arg)->_data.str : _managesieve_arg_str_error(arg))

#define MANAGESIEVE_ARG_LITERAL_SIZE(arg) \
	(((arg)->type == MANAGESIEVE_ARG_LITERAL_SIZE) ? \
	 (arg)->_data.literal_size : _managesieve_arg_literal_size_error(arg))

struct managesieve_arg_list {
	size_t size, alloc;
	struct managesieve_arg args[1]; /* variable size */
};


/* Create new MANAGESIEVE argument parser. output is used for sending command
   continuation requests for literals.

   max_line_size can be used to approximately limit the maximum amount of
   memory that gets allocated when parsing a line. Input buffer size limits
   the maximum size of each parsed token.

   Usually the largest lines are large only because they have a one huge
   message set token, so you'll probably want to keep input buffer size the
   same as max_line_size. That means the maximum memory usage is around
   2 * max_line_size. */
struct managesieve_parser *
managesieve_parser_create(struct istream *input, struct ostream *output,
		   size_t max_line_size);
void managesieve_parser_destroy(struct managesieve_parser **parser);

/* Reset the parser to initial state. */
void managesieve_parser_reset(struct managesieve_parser *parser);

/* Return the last error in parser. fatal is set to TRUE if there's no way to
   continue parsing, currently only if too large non-sync literal size was
   given. */
const char *managesieve_parser_get_error(struct managesieve_parser *parser, bool *fatal);

/* Read a number of arguments. This function doesn't call i_stream_read(), you
   need to do that. Returns number of arguments read (may be less than count
   in case of EOL), -2 if more data is needed or -1 if error occurred.

   count-sized array of arguments are stored into args when return value is
   0 or larger. If all arguments weren't read, they're set to NIL. count
   can be set to 0 to read all arguments in the line. Last element in
   args is always of type MANAGESIEVE_ARG_EOL. */
int managesieve_parser_read_args(struct managesieve_parser *parser, unsigned int count,
			  enum managesieve_parser_flags flags, struct managesieve_arg **args);

/* just like managesieve_parser_read_args(), but assume \n at end of data in
   input stream. */
int managesieve_parser_finish_line(struct managesieve_parser *parser, unsigned int count,
			    enum managesieve_parser_flags flags,
			    struct managesieve_arg **args);

/* Read one word - used for reading tag and command name.
   Returns NULL if more data is needed. */
const char *managesieve_parser_read_word(struct managesieve_parser *parser);

/* Returns the managesieve argument as string. If it is no string this returns NULL */
const char *managesieve_arg_string(struct managesieve_arg *arg);

/* Returns 1 if the argument is a number. If it is no number this returns -1.
 * The number itself is stored in *number.
 */
int managesieve_arg_number
  (struct managesieve_arg *arg, uoff_t *number);

/* Error functions */
char *_managesieve_arg_str_error(const struct managesieve_arg *arg);
uoff_t _managesieve_arg_literal_size_error(const struct managesieve_arg *arg);
struct managesieve_arg_list *_managesieve_arg_list_error(const struct managesieve_arg *arg);

#endif
