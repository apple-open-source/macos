/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2003 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sascha Schumann <sascha@schumann.cx>                         |
   +----------------------------------------------------------------------+
 */

/* $Id: ircg_scanner.re,v 1.18.8.3 2003/06/04 07:01:27 sas Exp $ */

#include <ext/standard/php_smart_str.h>
#include <stdio.h>
#include <string.h>

static const char *color_list[] = {
    "white",
    "black",
    "blue",
    "green",
    "red",
    "brown",
    "purple",
    "orange",
    "yellow",
    "lightgreen",
    "teal",
    "lightcyan",
    "lightblue",
    "#ff00ff",
    "#bebebe",
    "lightgrey"
};


typedef struct {
	int bg_code;
	int fg_code;
	int font_tag_open;
	int bold_tag_open;
	int underline_tag_open;
	int italic_tag_open;
	char fg_color[6];
	char bg_color[6];
	
	smart_str scheme;
	smart_str *result;
} ircg_msg_scanner;

/*!re2c
any = [\000-\377];
anynoneof = [\001-\377];
eof = [\000];
alpha = [a-zA-Z];
alnum = [a-zA-Z0-9];
digit = [0-9];
scheme = alpha alnum*;
coloresc = "\003";
colorhex = "\004";
bold = "\002";
underline = "\037";
italic = "\35";
ircnl = "\036";
winquotes = [\204\223\224];
hex = [a-fA-F0-9];
*/

#define YYFILL(n) do { } while (0)
#define YYCTYPE unsigned char
#define YYCURSOR xp
#define YYLIMIT end
#define YYMARKER q

#define STD_PARA ircg_msg_scanner *ctx, const char *start, const char *YYCURSOR
#define STD_ARGS ctx, start, YYCURSOR

#define passthru() do {									\
	size_t __len = xp - start;							\
	smart_str_appendl_ex(mctx.result, start, __len, 1); \
} while (0)

static inline void handle_scheme(STD_PARA)
{
	ctx->scheme.len = 0;
	smart_str_appendl_ex(&ctx->scheme, start, YYCURSOR - start, 1);
	smart_str_0(&ctx->scheme);
}

static inline void handle_url(STD_PARA)
{
	smart_str_appends_ex(ctx->result, "<a target=blank href=\"", 1);
	smart_str_append_ex(ctx->result, &ctx->scheme, 1);
	smart_str_appendl_ex(ctx->result, start, YYCURSOR - start, 1);
	smart_str_appends_ex(ctx->result, "\">", 1);
	smart_str_append_ex(ctx->result, &ctx->scheme, 1);
	smart_str_appendl_ex(ctx->result, start, YYCURSOR - start, 1);
	smart_str_appends_ex(ctx->result, "</a>", 1);
}

static void handle_color_digit(STD_PARA, int mode)
{
	int len;
	int nr;

	len = YYCURSOR - start;
	switch (len) {
		case 2:
			nr = (start[0] - '0') * 10 + (start[1] - '0');
			break;
		case 1:
			nr = start[0] - '0';
			break;
	}
	
	switch (mode) {
		case 0: ctx->fg_code = nr; break;
		case 1: ctx->bg_code = nr; break;
	}
}

static void handle_hex(STD_PARA, int mode)
{
	memcpy(mode == 0 ? ctx->fg_color : ctx->bg_color, start, 6);
}

#define IS_VALID_CODE(n) (n >= 0 && n <= 15)

static void finish_color_stuff(STD_PARA)
{
	if (ctx->font_tag_open) {
		smart_str_appends_ex(ctx->result, "</font>", 1);
		ctx->font_tag_open = 0;
	}
}

static void handle_bold(STD_PARA, int final)
{
	switch (ctx->bold_tag_open) {
	case 0:
		if (!final) smart_str_appends_ex(ctx->result, "<b>", 1);
		break;
	case 1:
		smart_str_appends_ex(ctx->result, "</b>", 1);
		break;
	}

	ctx->bold_tag_open = 1 - ctx->bold_tag_open;
}

static void handle_underline(STD_PARA, int final)
{
	switch (ctx->underline_tag_open) {
	case 0:
		if (!final) smart_str_appends_ex(ctx->result, "<u>", 1);
		break;
	case 1:
		smart_str_appends_ex(ctx->result, "</u>", 1);
		break;
	}

	ctx->underline_tag_open = 1 - ctx->underline_tag_open;
}

static void handle_italic(STD_PARA, int final)
{
	switch (ctx->italic_tag_open) {
	case 0:
		if (!final) smart_str_appends_ex(ctx->result, "<i>", 1);
		break;
	case 1:
		smart_str_appends_ex(ctx->result, "</i>", 1);
		break;
	}

	ctx->italic_tag_open = 1 - ctx->italic_tag_open;
}

static void commit_color_stuff(STD_PARA)
{
	finish_color_stuff(STD_ARGS);

	if (IS_VALID_CODE(ctx->fg_code)) {
		smart_str_appends_ex(ctx->result, "<font color=\"", 1);
		smart_str_appends_ex(ctx->result, color_list[ctx->fg_code], 1);
		smart_str_appends_ex(ctx->result, "\">", 1);
		ctx->font_tag_open = 1;
	}
}

static void commit_color_hex(STD_PARA)
{
	finish_color_stuff(STD_ARGS);

	if (ctx->fg_color[0] != 0) {
		smart_str_appends_ex(ctx->result, "<font color=\"", 1);
		smart_str_appendl_ex(ctx->result, ctx->fg_color, 6, 1);
		smart_str_appends_ex(ctx->result, "\">", 1);
		ctx->font_tag_open = 1;
	}
}

static void add_entity(STD_PARA, const char *entity)
{
	smart_str_appends_ex(ctx->result, entity, 1);
}

void ircg_mirc_color(const char *msg, smart_str *result, size_t msg_len, int auto_links, int gen_br) 
{
	const char *end, *xp, *q, *start;
	ircg_msg_scanner mctx, *ctx = &mctx;

	mctx.result = result;
	mctx.scheme.c = NULL;
	mctx.italic_tag_open = mctx.font_tag_open = mctx.bold_tag_open = mctx.underline_tag_open = 0;
	
	if (msg_len == -1)
		msg_len = strlen(msg);
	end = msg + msg_len;
	xp = msg;
	

state_plain:
	if (xp >= end) goto stop;
	start = YYCURSOR;
/*!re2c
	scheme "://"	{ if (auto_links) { handle_scheme(STD_ARGS); goto state_url; } else { passthru(); goto state_plain; } }
	coloresc 		{ mctx.fg_code = mctx.bg_code = -1; goto state_color_fg; }
	colorhex		{ mctx.fg_color[0] = mctx.bg_color[0] = 0; goto state_color_hex; }
	"<"				{ add_entity(STD_ARGS, "&lt;"); goto state_plain; }
	">"				{ add_entity(STD_ARGS, "&gt;"); goto state_plain; }
	"&"				{ add_entity(STD_ARGS, "&amp;"); goto state_plain; }
	winquotes		{ add_entity(STD_ARGS, "&quot;"); goto state_plain; }
	ircnl			{ if (gen_br) smart_str_appendl_ex(ctx->result, "<br>", 4, 1); goto state_plain; }
	bold			{ handle_bold(STD_ARGS, 0); goto state_plain; }
	underline		{ handle_underline(STD_ARGS, 0); goto state_plain; }
	italic			{ handle_italic(STD_ARGS, 0); goto state_plain; }
	anynoneof		{ passthru(); goto state_plain; }
*/

state_color_hex:
	start = YYCURSOR;
/*!re2c
  hex hex hex hex hex hex { handle_hex(STD_ARGS, 0); goto state_color_hex_bg; }
  ","					{ goto state_color_hex_bg; }
  any					{ finish_color_stuff(STD_ARGS); passthru(); goto state_plain; }
*/

	
state_color_hex_comma:	
	start = YYCURSOR;
/*!re2c
  ","					{ goto state_color_hex_bg; }
  any					{ YYCURSOR--; commit_color_hex(STD_ARGS); goto state_plain; }
*/


state_color_hex_bg:
	start = YYCURSOR;
/*!re2c
  hex hex hex hex hex hex	{ handle_hex(STD_ARGS, 1); commit_color_hex(STD_ARGS); goto state_plain; }
  any						{ commit_color_hex(STD_ARGS); passthru(); goto state_plain; }
*/

state_url:
	start = YYCURSOR;
/*!re2c
  	[-a-zA-Z0-9~_?=.@&+/#:;!*'()%,$]+		{ handle_url(STD_ARGS); goto state_plain; }
	any				{ passthru(); goto state_plain; }
*/


state_color_fg:		
	start = YYCURSOR;
/*!re2c
  	digit digit?		{ handle_color_digit(STD_ARGS, 0); goto state_color_comma; }
	","					{ goto state_color_bg; }
	any					{ finish_color_stuff(STD_ARGS); passthru(); goto state_plain; }
*/

	
state_color_comma:	
	start = YYCURSOR;
/*!re2c
  ","					{ goto state_color_bg; }
  any					{ YYCURSOR--; commit_color_stuff(STD_ARGS); goto state_plain; }
*/
	

state_color_bg:
	start = YYCURSOR;
/*!re2c
  	digit digit?		{ handle_color_digit(STD_ARGS, 1); commit_color_stuff(STD_ARGS); goto state_plain; }
	any					{ commit_color_stuff(STD_ARGS); passthru(); goto state_plain; }
*/

stop:
	smart_str_free_ex(&ctx->scheme, 1);

	finish_color_stuff(STD_ARGS);
	handle_bold(STD_ARGS, 1);
	handle_underline(STD_ARGS, 1);
	handle_italic(STD_ARGS, 1);
}
