/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sascha Schumann <sascha@schumann.cx>                        |
   +----------------------------------------------------------------------+
 */

/* $Id: ircg_scanner.re,v 1.1.1.1 2001/07/19 00:19:19 zarzycki Exp $ */

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
    "pink",
    "gray",
    "lightgrey"
};


enum {
	STATE_PLAIN,
	STATE_URL,
	STATE_COLOR_FG,
	STATE_COLOR_COMMA,
	STATE_COLOR_BG
};

typedef struct {
	int bg_code;
	int fg_code;
	int font_tag_open;
	int bold_tag_open;
	int underline_tag_open;
	
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
coloresc = "";
bold = "";
underline = "\037";
*/

#define YYFILL(n) { }
#define YYCTYPE unsigned char
#define YYCURSOR xp
#define YYLIMIT end
#define YYMARKER q
#define STATE mode

#define STD_PARA ircg_msg_scanner *ctx, const char *start, const char *YYCURSOR
#define STD_ARGS ctx, start, YYCURSOR

static void handle_scheme(STD_PARA)
{
	ctx->scheme.len = 0;
	smart_str_appendl_ex(&ctx->scheme, start, YYCURSOR - start, 1);
	smart_str_0(&ctx->scheme);
}

static void handle_url(STD_PARA)
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

static void passthru(STD_PARA)
{
	smart_str_appendl_ex(ctx->result, start, YYCURSOR - start, 1);
}

static void add_entity(STD_PARA, const char *entity)
{
	smart_str_appends_ex(ctx->result, entity, 1);
}

void ircg_mirc_color(const char *msg, smart_str *result, size_t msg_len) {
	int mode = STATE_PLAIN;
	const char *end, *xp, *q, *start;
	ircg_msg_scanner mctx, *ctx = &mctx;

	mctx.result = result;
	mctx.scheme.c = NULL;
	mctx.font_tag_open = mctx.bold_tag_open = mctx.underline_tag_open = 0;
	
	if (msg_len == -1)
		msg_len = strlen(msg);
	end = msg + msg_len;
	xp = msg;
	
	while (1) {
		start = YYCURSOR;

		switch (STATE) {

		case STATE_PLAIN:
/*!re2c
	scheme "://"	{ handle_scheme(STD_ARGS); STATE = STATE_URL; continue; }
	coloresc 		{ mctx.fg_code = mctx.bg_code = -1; STATE = STATE_COLOR_FG; continue; }
	"<"				{ add_entity(STD_ARGS, "&lt;"); continue; }
	">"				{ add_entity(STD_ARGS, "&gt;"); continue; }
	"&"				{ add_entity(STD_ARGS, "&amp;"); continue; }
	bold			{ handle_bold(STD_ARGS, 0); continue; }
	underline		{ handle_underline(STD_ARGS, 0); continue; }
	anynoneof		{ passthru(STD_ARGS); continue; }
	eof				{ goto stop; }
*/

			break;

		case STATE_URL:	

			
/*!re2c
  	[-a-zA-Z0-9~_?=.@&+/#:;!*'()%,$]+		{ handle_url(STD_ARGS); STATE = STATE_PLAIN; continue; }
	any				{ passthru(STD_ARGS); STATE = STATE_PLAIN; continue; }
*/

			break;

		case STATE_COLOR_FG:
		
/*!re2c
  	digit digit?		{ handle_color_digit(STD_ARGS, 0); STATE = STATE_COLOR_COMMA; continue; }
	any					{ finish_color_stuff(STD_ARGS); passthru(STD_ARGS); STATE = STATE_PLAIN; continue; }
*/

			break;
		
		case STATE_COLOR_COMMA:
		
/*!re2c
  ","					{ STATE = STATE_COLOR_BG; continue; }
  any					{ YYCURSOR--; commit_color_stuff(STD_ARGS); STATE = STATE_PLAIN; continue; }
*/

			break;

		case STATE_COLOR_BG:

/*!re2c
  	digit digit?		{ handle_color_digit(STD_ARGS, 1); commit_color_stuff(STD_ARGS); STATE = STATE_PLAIN; continue; }
	any					{ commit_color_stuff(STD_ARGS); STATE = STATE_PLAIN; continue; }
*/
			break;
		}
	}
stop:
	smart_str_free_ex(&ctx->scheme, 1);

	finish_color_stuff(STD_ARGS);
	handle_bold(STD_ARGS, 1);
	handle_underline(STD_ARGS, 1);
}
