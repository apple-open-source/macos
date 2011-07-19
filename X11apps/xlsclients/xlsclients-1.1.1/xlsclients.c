/*
Copyright 1989, 1998  The Open Group
Copyright 2009  Open Text Corporation

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 * *
 * Author:  Jim Fulton, MIT X Consortium
 * Author:  Peter Harris, Open Text Corporation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>
#ifndef HAVE_STRNLEN
#include "strnlen.h"
#endif

#ifndef PRIx32
#define PRIx32 "x"
#endif
#ifndef PRIu32
#define PRIu32 "u"
#endif

static char *ProgramName;

static xcb_atom_t WM_STATE;

static void lookat (xcb_connection_t *dpy, xcb_window_t root, int verbose, int maxcmdlen);
static void print_client_properties (xcb_connection_t *dpy, xcb_window_t w,
				     int verbose, int maxcmdlen );
static void print_text_field (xcb_connection_t *dpy, char *s, xcb_get_property_reply_t *tp );
static int print_quoted_word (char *s, int maxlen);
static void unknown (xcb_connection_t *dpy, xcb_atom_t actual_type, int actual_format );

/* For convenience: */
typedef int Bool;
#define False (0)
#define True (!False)

static void 
usage(void)
{
    fprintf (stderr,
	     "usage:  %s  [-display dpy] [-m len] [-[a][l]]\n", ProgramName);
    exit (1);
}

typedef void (*queue_func)(void *closure);
typedef struct queue_blob {
    queue_func func;
    void *closure;
    struct queue_blob *next;
} queue_blob;

static queue_blob *head = NULL;
static queue_blob **tail = &head;

static void enqueue(queue_func func, void *closure)
{
    queue_blob *blob = malloc(sizeof(*blob));
    if (!blob)
	return; /* TODO: print OOM error */

    blob->func = func;
    blob->closure = closure;
    blob->next = NULL;
    *tail = blob;
    tail = &blob->next;
}

static void run_queue(void)
{
    while (head) {
	queue_blob *blob = head;
	blob->func(blob->closure);
	head = blob->next;
	free(blob);
    }
    tail = &head;
}

typedef struct {
    xcb_connection_t *c;
    xcb_intern_atom_cookie_t cookie;
    xcb_atom_t *atom;
} atom_state;

static void atom_done(void *closure)
{
    xcb_intern_atom_reply_t *reply;
    atom_state *as = closure;

    reply = xcb_intern_atom_reply(as->c, as->cookie, NULL);
    if (!reply)
	goto done; /* TODO: print Error message */

    *(as->atom) = reply->atom;
    free(reply);

done:
    free(as);
}

static void init_atoms(xcb_connection_t *c)
{
    atom_state *as;

    as = malloc(sizeof(*as));
    as->c = c;
    as->atom = &WM_STATE;
    as->cookie = xcb_intern_atom(c, 0, strlen("WM_STATE"), "WM_STATE");
    enqueue(atom_done, as);
}

int
main(int argc, char *argv[])
{
    int i;
    char *displayname = NULL;
    Bool all_screens = False;
    Bool verbose = False;
    xcb_connection_t *dpy;
    const xcb_setup_t *setup;
    int screen_number = 0;
    int maxcmdlen = 10000;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    char *cp;

	    switch (arg[1]) {
	      case 'd':			/* -display dpyname */
		if (++i >= argc) usage ();
		displayname = argv[i];
		continue;
	      case 'm':			/* -max maxcmdlen */
		if (++i >= argc) usage ();
		maxcmdlen = atoi (argv[i]);
		continue;
	    }

	    for (cp = &arg[1]; *cp; cp++) {
		switch (*cp) {
		  case 'a':		/* -all */
		    all_screens = True;
		    continue;
		  case 'l':		/* -long */
		    verbose = True;
		    continue;
		  default:
		    usage ();
		}
	    }
	} else {
	    usage ();
	}
    }

    dpy = xcb_connect(displayname, &screen_number);
    if (xcb_connection_has_error(dpy)) {
	char *name = displayname;
	if (!name)
	    name = getenv("DISPLAY");
	if (!name)
	    name = "";
	fprintf (stderr, "%s:  unable to open display \"%s\"\r\n",
		 ProgramName, name);
	exit (1);
    }

    init_atoms(dpy);

    setup = xcb_get_setup(dpy);
    if (all_screens) {
	xcb_screen_iterator_t screen;

	screen = xcb_setup_roots_iterator(setup);
	do {
	    lookat(dpy, screen.data->root, verbose, maxcmdlen);
	    xcb_screen_next(&screen);
	} while (screen.rem);
    } else {
	xcb_screen_iterator_t screen;

	screen = xcb_setup_roots_iterator(setup);
	for (i = 0; i < screen_number; i++)
	    xcb_screen_next(&screen);

	lookat (dpy, screen.data->root, verbose, maxcmdlen);
    }

    run_queue();

    xcb_disconnect(dpy);
    exit (0);
}

typedef struct {
    xcb_connection_t *c;
    xcb_get_property_cookie_t *prop_cookie;
    xcb_query_tree_cookie_t *tree_cookie;
    xcb_window_t *win;
    xcb_window_t orig_win;
    int list_length;
    int verbose;
    int maxcmdlen;
} child_wm_state;

static void child_info(void *closure)
{
    child_wm_state *cs = closure;
    xcb_window_t orig = cs->orig_win;
    xcb_connection_t *c = cs->c;
    int verbose = cs->verbose;
    int maxcmdlen = cs->maxcmdlen;
    int i, j;

    int child_count, num_rep;
    xcb_query_tree_reply_t **reply;

    for (i = 0; i < cs->list_length; i++) {
	xcb_get_property_reply_t *reply;
	reply = xcb_get_property_reply(c, cs->prop_cookie[i], NULL);
	if (reply) {
	    if (reply->type) {
		/* Show information for this window */
		print_client_properties(c, cs->win[i], cs->verbose, cs->maxcmdlen);

		free(reply);

		/* drain stale replies */
		for (j = i+1; j < cs->list_length; j++) {
		    reply = xcb_get_property_reply(c, cs->prop_cookie[j], NULL);
		    if (reply)
			free(reply);
		}
		for (j = 0; j < cs->list_length; j++) {
		    xcb_query_tree_reply_t *rep;
		    rep = xcb_query_tree_reply(c, cs->tree_cookie[j], NULL);
		    if (rep)
			free(rep);
		}
		goto done;
	    }
	    free(reply);
	}
    }

    /* WM_STATE not found. Recurse into children: */
    num_rep = 0;
    reply = malloc(sizeof(*reply) * cs->list_length);
    if (!reply)
	goto done; /* TODO: print OOM message, drain reply queue */

    for (i = 0; i < cs->list_length; i++) {
	reply[num_rep] = xcb_query_tree_reply(c, cs->tree_cookie[i], NULL);
	if (reply[num_rep])
	    num_rep++;
    }

    child_count = 0;
    for (i = 0; i < num_rep; i++)
	child_count += reply[i]->children_len;

    if (!child_count) {
	/* No children have CS_STATE; try the parent window */
	print_client_properties(c, cs->orig_win, cs->verbose, cs->maxcmdlen);
	goto reply_done;
    }

    cs = malloc(sizeof(*cs) + child_count * (sizeof(*cs->prop_cookie) + sizeof(*cs->tree_cookie) + sizeof(*cs->win)));
    if (!cs)
	goto reply_done; /* TODO: print OOM message */

    cs->c = c;
    cs->verbose = verbose;
    cs->maxcmdlen = maxcmdlen;
    cs->orig_win = orig;
    cs->prop_cookie = (void *)&cs[1];
    cs->tree_cookie = (void *)&cs->prop_cookie[child_count];
    cs->win = (void *)&cs->tree_cookie[child_count];
    cs->list_length = child_count;

    child_count = 0;
    for (i = 0; i < num_rep; i++) {
	xcb_window_t *child = xcb_query_tree_children(reply[i]);
	for (j = 0; j < reply[i]->children_len; j++) {
	    cs->win[child_count] = child[j];
	    cs->prop_cookie[child_count] = xcb_get_property(c, 0, child[j],
			    WM_STATE, XCB_GET_PROPERTY_TYPE_ANY,
			    0, 0);
	    /* Just in case the property isn't there, get the tree too */
	    cs->tree_cookie[child_count++] = xcb_query_tree(c, child[j]);
	}
    }

    enqueue(child_info, cs);

reply_done:
    for (i = 0; i < num_rep; i++)
	free(reply[i]);
    free(reply);

done:
    free(closure);
}

typedef struct {
    xcb_connection_t *c;
    xcb_query_tree_cookie_t cookie;
    int verbose;
    int maxcmdlen;
} root_list_state;

static void root_list(void *closure)
{
    int i;
    xcb_window_t *child;
    xcb_query_tree_reply_t *reply;
    root_list_state *rl = closure;

    reply = xcb_query_tree_reply(rl->c, rl->cookie, NULL);
    if (!reply)
	goto done;

    child = xcb_query_tree_children(reply);
    for (i = 0; i < reply->children_len; i++) {
	/* Get information about each child */
	child_wm_state *cs = malloc(sizeof(*cs) + sizeof(*cs->prop_cookie) + sizeof(*cs->tree_cookie) + sizeof(*cs->win));
	if (!cs)
	    goto done; /* TODO: print OOM message */
	cs->c = rl->c;
	cs->verbose = rl->verbose;
	cs->maxcmdlen = rl->maxcmdlen;
	cs->prop_cookie = (void *)&cs[1];
	cs->tree_cookie = (void *)&cs->prop_cookie[1];
	cs->win = (void *)&cs->tree_cookie[1];

	cs->orig_win = child[i];
	cs->win[0] = child[i];

	cs->prop_cookie[0] = xcb_get_property(rl->c, 0, child[i],
			WM_STATE, XCB_GET_PROPERTY_TYPE_ANY,
			0, 0);
	/* Just in case the property isn't there, get the tree too */
	cs->tree_cookie[0] = xcb_query_tree(rl->c, child[i]);

	cs->list_length = 1;
	enqueue(child_info, cs);
    }
    free(reply);

done:
    free(rl);
}

static void
lookat(xcb_connection_t *dpy, xcb_window_t root, int verbose, int maxcmdlen)
{
    root_list_state *rl = malloc(sizeof(*rl));

    if (!rl)
	return; /* TODO: OOM message */

    /*
     * get the list of windows
     */

    rl->c = dpy;
    rl->cookie = xcb_query_tree(dpy, root);
    rl->verbose = verbose;
    rl->maxcmdlen = maxcmdlen;
    enqueue(root_list, rl);
}

static char *Nil = "(nil)";

typedef struct {
    xcb_connection_t *c;
    xcb_get_property_cookie_t client_machine;
    xcb_get_property_cookie_t command;
    xcb_get_property_cookie_t name;
    xcb_get_property_cookie_t icon_name;
    xcb_get_property_cookie_t wm_class;
    xcb_window_t w;
    int verbose;
    int maxcmdlen;
} client_state;

static void
show_client_properties(void *closure)
{
    client_state *cs = closure;
    xcb_get_property_reply_t *client_machine;
    xcb_get_property_reply_t *command;
    xcb_get_property_reply_t *name;
    xcb_get_property_reply_t *icon_name;
    xcb_get_property_reply_t *wm_class;
    char *argv;
    int charsleft = cs->maxcmdlen;
    int i;

    /*
     * get the WM_MACHINE and WM_COMMAND list of strings
     */
    client_machine = xcb_get_property_reply(cs->c, cs->client_machine, NULL);
    command = xcb_get_property_reply(cs->c, cs->command, NULL);
    if (cs->verbose) {
	name = xcb_get_property_reply(cs->c, cs->name, NULL);
	icon_name = xcb_get_property_reply(cs->c, cs->icon_name, NULL);
	wm_class = xcb_get_property_reply(cs->c, cs->wm_class, NULL);
    }

    if (!command || !command->type)
	goto done;

    /*
     * do header information
     */
    if (cs->verbose) {
	printf ("Window 0x%" PRIx32 ":\n", cs->w);
	print_text_field (cs->c, "  Machine:  ", client_machine);
	if (name && name->type)
	    print_text_field (cs->c, "  Name:  ", name);
    } else {
	print_text_field (cs->c, NULL, client_machine);
	putchar (' ');
	putchar (' ');
    }


    if (cs->verbose)
	if (icon_name && icon_name->type)
	    print_text_field (cs->c, "  Icon Name:  ", icon_name);


    /*
     * do the command
     */
    if (cs->verbose)
	printf ("  Command:  ");
    argv = xcb_get_property_value(command);
    for (i = 0; i < command->value_len && charsleft > 0; ) {
	charsleft -= print_quoted_word (argv + i, charsleft);
	i += strnlen(argv + i, command->value_len - i) + 1;
	if (i < command->value_len && charsleft > 0) {
	    putchar (' ');
	    charsleft--;
	}
    }
    putchar ('\n');


    /*
     * do trailer information
     */
    if (cs->verbose) {
	if (wm_class && wm_class->type) {
	    char *res_name, *res_class;
	    int name_len, class_len;
	    res_name = xcb_get_property_value(wm_class);
	    name_len = strnlen(res_name, wm_class->value_len) + 1;
	    class_len = wm_class->value_len - name_len;
	    if (class_len > 0) {
		res_class = res_name + name_len;
	    } else {
		res_class = Nil;
		class_len = strlen(res_class);
	    }

	    printf ("  Instance/Class:  %.*s/%.*s",
		    name_len, res_name,
		    class_len, res_class);
	    putchar ('\n');
	}
    }

done:
    if (client_machine)
	free(client_machine);
    if (command)
	free(command);
    if (cs->verbose) {
	if (name)
	    free(name);
	if (icon_name)
	    free(icon_name);
	if (wm_class)
	    free(wm_class);
    }
    free(cs);
}

static void
print_client_properties(xcb_connection_t *dpy, xcb_window_t w, int verbose, int maxcmdlen)
{
    client_state *cs = malloc(sizeof(*cs));
    if (!cs)
	return; /* TODO: print OOM message */

    cs->c = dpy;
    cs->w = w;
    cs->verbose = verbose;
    cs->maxcmdlen = maxcmdlen;

    /*
     * get the WM_CLIENT_MACHINE and WM_COMMAND list of strings
     */
    cs->client_machine = xcb_get_property(dpy, 0, w,
			    WM_CLIENT_MACHINE, XCB_GET_PROPERTY_TYPE_ANY,
			    0, 1000000L);
    cs->command = xcb_get_property(dpy, 0, w,
			    WM_COMMAND, XCB_GET_PROPERTY_TYPE_ANY,
			    0, 1000000L);

    if (verbose) {
	cs->name = xcb_get_property(dpy, 0, w,
			    WM_NAME, XCB_GET_PROPERTY_TYPE_ANY,
			    0, 1000000L);
	cs->icon_name = xcb_get_property(dpy, 0, w,
			    WM_ICON_NAME, XCB_GET_PROPERTY_TYPE_ANY,
			    0, 1000000L);
	cs->wm_class = xcb_get_property(dpy, 0, w,
			    WM_CLASS, STRING,
			    0, 1000000L);
    }

    enqueue(show_client_properties, cs);
}

static void
print_text_field(xcb_connection_t *dpy, char *s, xcb_get_property_reply_t *tp)
{
    if (tp->type == XCB_NONE || tp->format == 0) {  /* Or XCB_ATOM_NONE after libxcb 1.5 */
	printf ("''");
	return;
    }

    if (s) printf ("%s", s);
    if (tp->type == STRING && tp->format == 8) {
	printf ("%.*s", (int)tp->value_len, (char *)xcb_get_property_value(tp));
    } else {
	unknown (dpy, tp->type, tp->format);
    }
    if (s) putchar ('\n');
}

/* returns the number of characters printed */
static int
print_quoted_word(char *s, 
		  int maxlen)		/* max number of chars we can print */
{
    register char *cp;
    Bool need_quote = False, in_quote = False;
    char quote_char = '\'', other_quote = '"';
    int charsprinted = 0;

    /*
     * walk down seeing whether or not we need to quote
     */
    for (cp = s; *cp; cp++) {

	if (! ((isascii(*cp) && isalnum(*cp)) || 
	       (*cp == '-' || *cp == '_' || *cp == '.' || *cp == '+' ||
		*cp == '/' || *cp == '=' || *cp == ':' || *cp == ','))) {
	    need_quote = True;
	    break;
	}
    }

    /*
     * write out the string: if we hit a quote, then close any previous quote,
     * emit the other quote, swap quotes and continue on.
     */
    in_quote = need_quote;
    if (need_quote) {
	putchar (quote_char);
	charsprinted++; maxlen--;
    }
    for (cp = s; *cp && maxlen>0; cp++) {
	if (*cp == quote_char) {
	    if (in_quote) {
		putchar (quote_char);
		charsprinted++; maxlen--;
	    }
	    putchar (other_quote);
	    charsprinted++; maxlen--;
	    { 
		char tmp = other_quote; 
		other_quote = quote_char; quote_char = tmp;
	    }
	    in_quote = True;
	}
	putchar (*cp);
	charsprinted++; maxlen--;
    }
    /* close the quote if we opened one and if we printed the whole string */
    if (in_quote && maxlen>0) {
	putchar (quote_char);
	charsprinted++; maxlen--;
    }

    return charsprinted;
}

static void
unknown(xcb_connection_t *dpy, xcb_atom_t actual_type, int actual_format)
{
    printf ("<unknown type ");
    if (actual_type == XCB_NONE)
	printf ("None");
    else {
	/* This should happen so rarely as to make no odds. Eat a round-trip: */
	xcb_get_atom_name_reply_t *atom =
	    xcb_get_atom_name_reply(dpy,
		xcb_get_atom_name(dpy, actual_type), NULL);
	if (atom) {
	    printf("%.*s", xcb_get_atom_name_name_length(atom),
			  xcb_get_atom_name_name(atom));
	    free(atom);
	} else
	    fputs (Nil, stdout);
    }
    printf (" (%" PRIu32 ") or format %d>", actual_type, actual_format);
}
