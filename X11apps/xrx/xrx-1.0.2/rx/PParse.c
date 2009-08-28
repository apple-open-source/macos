/* $Xorg: PParse.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#include "RxI.h"

#define RX_DEFAULT_VERSION 1
#define RX_DEFAULT_REVISION 0


typedef struct {
    char *string;
    int length;
    int index;
} Token;

/* macros to make token tables */
#define TOKEN(s, i) { s, sizeof(s) - 1, i }
#define NULLTOKEN { NULL, 0, 0 }

/* tables of tokens,
   each token is made of a string and its associated enum value.
 */
static Token RxServices[] = {
    TOKEN(RX_UI, UI),
    TOKEN(RX_PRINT, Print),
    NULLTOKEN
};

static Token RxUIs[] = {
    TOKEN("X", XUI),
    NULLTOKEN
};

static Token RxPrints[] = {
    TOKEN("XPrint",  XPrint),
    NULLTOKEN
};


static Token RxXAuthentications[] = {
    TOKEN("MIT-MAGIC-COOKIE-1", MitMagicCookie1),
    NULLTOKEN
};

#ifdef NEED_STRCASECMP
/*
 * in case str[n]casecmp are not provided by the system here are some
 * which do the trick
 */
int
_RxStrcasecmp(register const char *s1, register const char *s2)
{
    register int c1, c2;

    while (*s1 && *s2) {
	c1 = tolower(*s1);
	c2 = tolower(*s2);
	if (c1 != c2)
	    return (c1 - c2);
	s1++;
	s2++;
    }
    return (int) (*s1 - *s2);
}

int
_RxStrncasecmp(register const char *s1, register const char *s2, size_t n)
{
    register int c1, c2;
    const char *l1 = s1 + n;
    const char *l2 = s2 + n;

    while (*s1 && *s2 && s1 < l1 && s2 < l2) {
	c1 = tolower(*s1);
	c2 = tolower(*s2);
	if (c1 != c2)
	    return (c1 - c2);
	s1++;
	s2++;
    }
    if (s1 < l1 && s2 < l2)
	return (int) (*s1 - *s2);
    else if (s1 < l1)
	return (int) *s1;
    else if (s2 < l2)
	return (int) *s2;
    return 0;
}
#endif

/* string copy functions */
static char *
strcopy(char *src)
{
    char *cpy = (char *)Malloc(strlen(src) + 1);
    if (cpy)
	strcpy(cpy, src);
    return cpy;
}

static char *
strncopy(char *src, int n)
{
    char *cpy = (char *)Malloc(n + 1);
    if (cpy) {
	strncpy(cpy, src, n);
	cpy[n] = '\0';
    }
    return cpy;
}


/* print out warning message */
#define WARNING(m, p) if (debug != 0) Warning(m, p)
static void
Warning(char *message, char *param)
{
    fprintf(stderr, "Warning: %s%s\n", message, param);
}

#define WARNINGN(m, p, n) if (debug != 0) WarningN(m, p, n)
static void
WarningN(char *message, char *param, int param_len)
{
    fprintf(stderr, "Warning: %s", message);
    fwrite((void *)param, sizeof(char), param_len, stderr);
    putc('\n', stderr);
}

/* print out error message */
static void
Error(char *message, char *param)
{
    fprintf(stderr, "Error: %s%s\n", message, param);
}

/* look for a known token and return its index or 0 */
static int
LookForToken(char *string, Token *token)
{
    for (; token->string != NULL; token++)
	if (Strncasecmp(string, token->string, token->length) == 0)
	    return token->index;
    return 0;
}

/* parse a comma separated list string: a,b,c
   for each element look for a known token,
   when found store the associated index in the given index table,
   when not found print out a WARNING and skip,
   terminate the index table with 0,
   return number of recognized tokens.
   Note that no array bound verification is done!
 */
static int
ParseList(char *value, Token *tokens, int *indices, int debug)
{
    char *next;
    int token;
    int n = 0;

    do {
	token = LookForToken(value, tokens);
	next = strchr(value, ',');

	/* if unknown token print out WARNING */
	if (token == 0) {
	    if (next != NULL) {
		/* not a NULL terminated string, so make one */
		char buf[BUFSIZ];
		int len = (next - value < BUFSIZ) ? next - value : BUFSIZ;

		strncpy(buf, value, len);
		buf[len] = '\0';
		WARNING("unknown parameter value: ", buf);
	    } else
		WARNING("unknown parameter value: ", value);
	} else
	    indices[n++] = token;

	if (next != NULL)
	    value = next + 1;
    } while (next);

    indices[n] = 0;

    return n;
}

/* Parse a comma separated list of pairs name[:value] such as: A:a,B,C:c
   for each element look for a known token,
   when found store the associated index in the given index table and copy
   the associated value,
   when not found print out a WARNING and skip,
   terminate the index table with 0,
   return number of recognized tokens.
   Note that no array bound verification is done!
 */
static int
ParseAuthList(char *value, Token *tokens, int *indices, char **values,
	      int debug)
{
    char *next, *ptr;
    int token;
    int n = 0;

    do {
	token = LookForToken(value, tokens);
	ptr = strchr(value, ':');
	next = strchr(((ptr != NULL) ? ptr : value), ',');

	/* if unknown token print out WARNING */
	if (token == 0) {
	    if (next != NULL) {
		/* not a NULL terminated string, so make one */
		char buf[BUFSIZ];
		int len = (next - value < BUFSIZ) ? next - value : BUFSIZ;

		strncpy(buf, value, len);
		buf[len] = '\0';
		WARNING("unknown parameter value: ", buf);
	    } else
		WARNING("unknown parameter value: ", value);
	} else {
	    indices[n] = token;
	    if (ptr != NULL) {	/* there is an associated data */
		if (next != NULL)
		    values[n++] = strncopy(ptr + 1, next - ptr);
		else
		    values[n++] = strcopy(ptr + 1);
	    } else
		values[n++] = NULL;
	}

	if (next != NULL)
	    value = next + 1;
    } while (next);

    indices[n] = 0;

    return n;
}

/* parse a boolean string value returning 0 on success, 1 otherwise */
static int
ParseBoolean(char *strvalue, RxBool *value_ret)
{
    if (Strcasecmp(strvalue, RX_YES) == 0) {
	*value_ret = RxTrue;
	return 0;
    } else if (Strcasecmp(strvalue, RX_NO) == 0) {
	*value_ret = RxFalse;
	return 0;
    }
    return 1;
}

/* parse an X-UI-INPUT-METHOD parameter value */
static int
ParseXInputMethod(char *value,
		  RxBool *input_method_ret, char **input_method_url_ret,
		  int debug)
{
    char *url;

    url = strchr(value, ';');
    if (url != NULL) {
	if (strncmp(value, RX_YES, sizeof(RX_YES) - 1) == 0) {
	    *input_method_ret = RxTrue;
	    *input_method_url_ret = strcopy(url + 1);
	} else if (strncmp(value, RX_YES, sizeof(RX_YES) - 1) == 0)
	    *input_method_ret = RxFalse;
	else
	    WARNINGN("not a boolean value: ", value, url - value);
    } else {
	if (ParseBoolean(value, input_method_ret) != 0)
	    WARNING("not a boolean value: ", value);
    }
    return 0;
}


/* parse an RX parameter */
static int
ParseParam(char *name, char *value, RxParams *params, int debug)
{
    if (Strcasecmp(name, RX_ACTION) == 0) {
	Free(params->action);
	params->action = strcopy(value);
    } else if (Strcasecmp(name, RX_EMBEDDED) == 0) {
	if (ParseBoolean(value, &params->embedded) != 0)
	    WARNING("not a boolean value: ", value);
    } else if (Strcasecmp(name, RX_AUTO_START) == 0) {
	if (ParseBoolean(value, &params->auto_start) != 0)
	    WARNING("not a boolean value: ", value);
    } else if (Strcasecmp(name, RX_WIDTH) == 0)
	params->width = atoi(value);
    else if (Strcasecmp(name, RX_HEIGHT) == 0)
	params->height = atoi(value);
    else if (Strcasecmp(name, RX_APP_GROUP) == 0) {
	Free(params->app_group);
	params->app_group = strcopy(value);
    } else if (Strcasecmp(name, RX_REQUIRED_SERVICES) == 0)
	ParseList(value, RxServices, (int*)params->required_services, debug);
    else if (Strcasecmp(name, RX_UI) == 0)
	ParseList(value, RxUIs, (int*)params->ui, debug);
    else if (Strcasecmp(name, RX_PRINT) == 0)
	ParseList(value, RxPrints, (int*)params->print, debug);
    else if (Strcasecmp(name, RX_X_UI_INPUT_METHOD) == 0)
	ParseXInputMethod(value, &params->x_ui_input_method,
			  &params->x_ui_input_method_url, debug);
    else if (Strcasecmp(name, RX_X_AUTH) == 0)
	ParseAuthList(value, RxXAuthentications,
		      (int*)params->x_auth, params->x_auth_data, debug);
    else if (Strcasecmp(name, RX_X_UI_AUTH) == 0)
	ParseAuthList(value, RxXAuthentications,
		      (int*)params->x_ui_auth, params->x_ui_auth_data, debug);
    else if (Strcasecmp(name, RX_X_PRINT_AUTH) == 0)
	ParseAuthList(value, RxXAuthentications,
		      (int*)params->x_print_auth, params->x_print_auth_data,
		      debug);
    else if (Strcasecmp(name, RX_X_UI_LBX_AUTH) == 0)
	ParseAuthList(value, RxXAuthentications,
		      (int*)params->x_ui_lbx_auth,
		      params->x_ui_lbx_auth_data, debug);
    else if (Strcasecmp(name, RX_X_PRINT_LBX_AUTH) == 0)
	ParseAuthList(value, RxXAuthentications,
		      (int*)params->x_print_lbx_auth,
		      params->x_print_lbx_auth_data,
		      debug);
    else if (Strcasecmp(name, RX_X_UI_LBX) == 0) {
	if (ParseBoolean(value, &params->x_ui_lbx) != 0)
	    WARNING("not a boolean value: ", value);
    } else if (Strcasecmp(name, RX_X_PRINT_LBX) == 0) {
	if (ParseBoolean(value, &params->x_print_lbx) != 0)
	    WARNING("not a boolean value: ", value);
    } else /* unknown parameter */
	WARNING("unknown parameter name: ", name);
    return 0;
}

/* parse a list of RX arguments storing result in the given RxParams struct,
 * since this function might be called several times using the same structure,
 * it is assumed that the structure is correctly initialized.
 */
int
RxParseParams(char *argn[], char *argv[], int argc, RxParams *params,
	      int debug)
{
    int i;
    int version, revision;

    if (argc == 0)
	return 0;

    /* the first param should be the Version number */
    if (Strcasecmp(argn[0], RX_VERSION) == 0) {
	if (sscanf(argv[0], "%d.%d", &version, &revision) == 2) {
	    params->version = version;
	    params->revision = revision;
	} else {
	    Error("invalid version identifier: ", argv[0]);
	    return 1;
	}
	argn++;
	argv++;
	i = 1;
    } else {
	/* no version identifier assume 1.0 */
	params->version = RX_DEFAULT_VERSION;
	params->revision = RX_DEFAULT_REVISION;
	i = 0;
    }

    /* parse given params */
    for (; i < argc; i++, argn++, argv++)
	if (ParseParam(*argn, *argv, params, debug))
	    return 1;

    return 0;
}

/* Initialize RxParams structure */
void
RxInitializeParams(RxParams *params)
{
    memset(params, 0, sizeof(RxParams));
    params->embedded = RxUndef;
    params->auto_start = RxUndef;
    params->width = RxUndef;
    params->height = RxUndef;
    /* init X params */
    params->x_ui_input_method = RxUndef;
    params->x_ui_input_method_url = NULL;
    params->x_ui_lbx = RxUndef;
    params->x_print_lbx = RxUndef;
}

void
FreeAuthListData(char **list)
{
    while (*list != NULL)
	Free(*list++);
}

/* Free data stored in RxParams structure */
int
RxFreeParams(RxParams *params)
{
    Free(params->action);
    Free(params->app_group);

    Free(params->x_ui_input_method_url);
    FreeAuthListData(params->x_auth_data);
    FreeAuthListData(params->x_ui_auth_data);
    FreeAuthListData(params->x_print_auth_data);
    FreeAuthListData(params->x_ui_lbx_auth_data);
    FreeAuthListData(params->x_print_lbx_auth_data);

    return 0;
}

/* Free data stored in RxReturnParams structure */
int
RxFreeReturnParams(RxReturnParams *params)
{
    /* action is only a reference - do not free */
    Free(params->ui);
    Free(params->print);

    Free(params->x_ui_lbx_auth);
    Free(params->x_print_lbx_auth);
    return 0;
}
