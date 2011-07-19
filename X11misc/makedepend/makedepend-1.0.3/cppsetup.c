/*

Copyright (c) 1993, 1994, 1998  The Open Group

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

*/

#include "def.h"

#include "ifparser.h"

struct _parse_data {
    struct filepointer *filep;
    struct inclist *inc;
    const char *filename;
    const char *line;
};

static const char *
my_if_errors (IfParser *ip, const char *cp, const char *expecting)
{
    struct _parse_data *pd = (struct _parse_data *) ip->data;
    int lineno = pd->filep->f_line;
    const char *filename = pd->filename;
    char prefix[300];
    int prefixlen;
    int i;

    sprintf (prefix, "\"%s\":%d", filename, lineno);
    prefixlen = strlen(prefix);
    fprintf (stderr, "%s:  %s", prefix, pd->line);
    i = cp - pd->line;
    if (i > 0 && pd->line[i-1] != '\n') {
	putc ('\n', stderr);
    }
    for (i += prefixlen + 3; i > 0; i--) {
	putc (' ', stderr);
    }
    fprintf (stderr, "^--- expecting %s\n", expecting);
    return NULL;
}


#define MAXNAMELEN 256

static struct symtab **
lookup_variable (IfParser *ip, const char *var, int len)
{
    char tmpbuf[MAXNAMELEN + 1];
    struct _parse_data *pd = (struct _parse_data *) ip->data;

    if (len > MAXNAMELEN)
	return NULL;

    strncpy (tmpbuf, var, len);
    tmpbuf[len] = '\0';
    return isdefined (tmpbuf, pd->inc, NULL);
}


static int
my_eval_defined (IfParser *ip, const char *var, int len)
{
    if (lookup_variable (ip, var, len))
	return 1;
    else
	return 0;
}

#define isvarfirstletter(ccc) (isalpha(ccc) || (ccc) == '_')

static long
my_eval_variable (IfParser *ip, const char *var, int len)
{
    long val;
    struct symtab **s;

    s = lookup_variable (ip, var, len);
    if (!s)
	return 0;
    do {
	var = (*s)->s_value;
	if (!isvarfirstletter(*var) || !strcmp((*s)->s_name, var))
	    break;
	s = lookup_variable (ip, var, strlen(var));
    } while (s);

    var = ParseIfExpression(ip, var, &val);
    if (var && *var) debug(4, ("extraneous: '%s'\n", var));
    return val;
}

int
cppsetup(const char *filename,
	 const char *line,
	 struct filepointer *filep,
	 struct inclist *inc)
{
    IfParser ip;
    struct _parse_data pd;
    long val = 0;

    pd.filep = filep;
    pd.inc = inc;
    pd.line = line;
    pd.filename = filename;
    ip.funcs.handle_error = my_if_errors;
    ip.funcs.eval_defined = my_eval_defined;
    ip.funcs.eval_variable = my_eval_variable;
    ip.data = (char *) &pd;

    (void) ParseIfExpression (&ip, line, &val);
    if (val)
	return IF;
    else
	return IFFALSE;
}

