/*
 * Copyright (c) 1999 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XdotOrg: xc/programs/xedit/ispell.c,v 1.6 2004/12/04 00:43:13 kuhn Exp $ */
/* $XFree86: xc/programs/xedit/ispell.c,v 1.19 2002/10/19 20:04:20 herrb Exp $ */

#include "xedit.h"
#include "util.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xos.h>

#define RECEIVE		1
#define SEND		2

#define CHECK		0
#define	ADD		1
#define REMOVE		2

#define	ASIS		1
#define UNCAP		2

/*
 * Types
 */
#define UNDO_DEPTH	16
typedef struct _ispell_undo {
    char *undo_str;
    int undo_count;
    XawTextPosition undo_pos;
    Boolean repeat;	/* two (misspelled?) words together */
    Boolean terse;
    int format;		/* remember text formatting style */
    struct _ispell_undo *next, *prev;
} ispell_undo;

typedef struct _ispell_dict {
    Widget sme;
    char *wchars;
    struct _ispell_dict *next;
} ispell_dict;

#define	TEXT	0
#define HTML	1
struct _ispell_format {
    char *name;
    int value;
    Widget sme;
};

static struct _ispell_format ispell_format[] = {
    {"text",	TEXT},
    {"html",	HTML},
};

struct _ispell {
    Widget shell, form, mispelled, repeated, word, replacement, text,
	   suggestions, viewport, list, commands, replace, status,
	   replaceAll, undo, ignore, ignoreAll, add, addUncap, suspend,
	   cancel, check, look, terse, options, dict, dictMenu,
	   format, formatMenu;

    Widget ascii, source;
    XtInputId id;
    int pid, ifd[2], ofd[2];
    XawTextPosition left, right;
    char *item;
    Bool lock;
    Bool repeat;
    Bool checkit;
    int stat;
    char *buf;
    int bufsiz;
    int buflen;
    char sendbuf[1024];
    char sentbuf[1024];

    int undo_depth;
    ispell_undo *undo_head, *undo_base;
    char *undo_for;

    char *wchars;
    char *cmd;
    char *skip;
    char *command;
    Boolean terse_mode, undo_terse_mode;
    char *guess_label, *miss_label, *root_label, *none_label, *eof_label,
	 *compound_label, *ok_label, *repeat_label, *working_label, *look_label;
    char *look_cmd;
    char *words_file;

    char *dictionary;
    char *dict_list;
    ispell_dict *dict_info;

    int format_mode;	/* to undo correctly */
    char *formatting;
    struct _ispell_format *format_info;
};

typedef struct _ReplaceEntry ReplaceEntry;
struct _ReplaceEntry {
    hash_key	*word;
    ReplaceEntry*next;
    char	*replace;
};

typedef struct _IgnoreEntry IgnoreEntry;
struct _IgnoreEntry {
    hash_key	*word;
    IgnoreEntry	*next;
    int		add;
};

/*
 * Prototypes
 */
static void AddIspell(Widget, XtPointer, XtPointer);
static void ChangeDictionaryIspell(Widget, XtPointer, XtPointer);
static void ChangeFormatIspell(Widget, XtPointer, XtPointer);
static void CheckIspell(Widget, XtPointer, XtPointer);
static void IgnoreIspell(Widget, XtPointer, XtPointer);
static Bool InitIspell(void);
static void IspellCheckUndo(void);
static int IspellConvertHtmlAmp(char*);
static Bool IspellDoIgnoredWord(char*, int, int);
static Bool IspellIgnoredWord(char*, int, int);
static void IspellInputCallback(XtPointer, int*, XtInputId*);
static void IspellKillUndoBuffer(void);
static Bool IspellReceive(void);
static char *IspellReplacedWord(char*, char*);
static int IspellSend(void);
static void IspellSetSelection(XawTextPosition, XawTextPosition);
static void IspellSetRepeated(Bool);
static void IspellSetSensitive(Bool);
static void IspellSetStatus(char*);
static void IspellSetTerseMode(Bool);
static Bool IspellStartProcess(void);
static Bool IspellCheckProcess(void);
static Bool IspellEndProcess(Bool, Bool);
static void LookIspell(Widget, XtPointer, XtPointer);
static void PopdownIspell(Widget, XtPointer, XtPointer);
static void ReplaceIspell(Widget, XtPointer, XtPointer);
static void RevertIspell(Widget, XtPointer, XtPointer);
static void SelectIspell(Widget, XtPointer, XtPointer);
static void ToggleTerseIspell(Widget, XtPointer, XtPointer);
#ifndef SIGNALRETURNSINT
static void timeout_signal(int);
static void (*old_timeout)(int);
#else
static int timeout_signal(int);
static int (*old_timeout)(int);
#endif
static void UndoIspell(Widget, XtPointer, XtPointer);

Bool _XawTextSrcUndo(TextSrcObject, XawTextPosition*);

/*
 * Initialization
 */
static struct _ispell ispell;

#define RSTRTBLSZ	23
#define ISTRTBLSZ	71
static hash_table *replace_hash;
static hash_table *ignore_hash;

#ifndef XtCStatus
#define XtCStatus	"Status"
#endif

#define Offset(field) XtOffsetOf(struct _ispell, field)
static XtResource resources[] = {
    {"wordChars", "Chars", XtRString, sizeof(char*),
	Offset(wchars), XtRString, ""},
    {"ispellCommand", "CommandLine", XtRString, sizeof(char*),
	Offset(cmd), XtRString, "/usr/local/bin/ispell"},
    {"terseMode", "Terse", XtRBoolean, sizeof(Boolean),
	Offset(terse_mode), XtRImmediate, (XtPointer)False},
    {"guessLabel", XtCStatus, XtRString, sizeof(String),
	Offset(guess_label), XtRString, "Guess"},
    {"missLabel", XtCStatus, XtRString, sizeof(String),
	Offset(miss_label), XtRString, "Miss"},
    {"rootLabel", XtCStatus, XtRString, sizeof(String),
	Offset(root_label), XtRString, "Root:"},
    {"noneLabel", XtCStatus, XtRString, sizeof(String),
	Offset(none_label), XtRString, "None"},
    {"compoundLabel", XtCStatus, XtRString, sizeof(String),
	Offset(compound_label), XtRString, "Compound"},
    {"okLabel", XtCStatus, XtRString, sizeof(String),
	Offset(ok_label), XtRString, "Ok"},
    {"eofLabel", XtCStatus, XtRString, sizeof(String),
	Offset(eof_label), XtRString, "End Of File"},
    {"repeatLabel", XtCStatus, XtRString, sizeof(String),
	Offset(repeat_label), XtRString, "Repeat"},
    {"workingLabel", XtCStatus, XtRString, sizeof(String),
	Offset(working_label), XtRString, "..."},
    {"lookLabel", XtCStatus, XtRString, sizeof(String),
	Offset(look_label), XtRString, "Look"},
    {"lookCommand", "CommandLine", XtRString, sizeof(char*),
	Offset(look_cmd), XtRString, "/usr/bin/egrep -i"},
    {"wordsFile", "Words", XtRString, sizeof(char*),
	Offset(words_file), XtRString, "/usr/share/dict/words"},
    {"dictionary", "Dictionary", XtRString, sizeof(char*),
	Offset(dictionary), XtRString, "american"},
    {"dictionaries", "Dictionary", XtRString, sizeof(char*),
	Offset(dict_list), XtRString, "american americanmed+ english"},
    {"formatting", "TextFormat", XtRString, sizeof(char*),
	Offset(formatting), XtRString, "text"},
};
#undef Offset

#ifdef NO_LIBC_I18N
static int
ToLower(int ch)
{
    char buf[2];

    *buf = ch;
    XmuNCopyISOLatin1Lowered(buf, buf, sizeof(buf));

    return (*buf);
}

static int
ToUpper(int ch)
{
    char buf[2];

    *buf = ch;
    XmuNCopyISOLatin1Uppered(buf, buf, sizeof(buf));

    return (*buf);
}

static int
IsLower(int ch)
{
    char upbuf[2];
    char lobuf[2];

    *upbuf = *lobuf = ch;
    XmuNCopyISOLatin1Lowered(lobuf, lobuf, sizeof(lobuf));
    XmuNCopyISOLatin1Uppered(upbuf, upbuf, sizeof(upbuf));

    return (*lobuf != *upbuf && ch == *lobuf);
}

static int
IsUpper(int ch)
{
    char upbuf[2];
    char lobuf[2];

    *upbuf = *lobuf = ch;
    XmuNCopyISOLatin1Lowered(lobuf, lobuf, sizeof(lobuf));
    XmuNCopyISOLatin1Uppered(upbuf, upbuf, sizeof(upbuf));

    return (*lobuf != *upbuf && ch == *upbuf);
}
#else
#define	ToLower	tolower
#define ToUpper	toupper
#define IsLower islower
#define IsUpper isupper
#endif

/*
 * Implementation
 */
#ifdef STDERR_FILENO
# define WRITES(s) write(STDERR_FILENO, s, strlen(s))
#else
# define WRITES(s) write(fileno(stderr), s, strlen(s))
#endif

/*ARGSUSED*/
#ifndef SIGNALRETURNSINT
static void
timeout_signal(int unused)
{
    int olderrno = errno;

    WRITES("Warning: Timeout waiting ispell process to die.\n");
    kill(ispell.pid, SIGTERM);
    errno = olderrno;
}
#else
static int
timeout_signal(int unused)
{
    int olderrno = errno;

    WRITES("Warning: Timeout waiting ispell process to die.\n");
    kill(ispell.pid, SIGTERM);
    
    errno = olderrno;
    return (0);
}
#endif

static void
IspellSetSelection(XawTextPosition left, XawTextPosition right)
{
    /* Try to make sure the selected word is completely visible */
    XawTextSetInsertionPoint(ispell.ascii, right);
    XawTextSetInsertionPoint(ispell.ascii, left);
    XawTextSetSelection(ispell.ascii, left, right);
}

static void
IspellSetStatus(char *label)
{
    Arg args[1];

    XtSetArg(args[0], XtNlabel, label);
    XtSetValues(ispell.status, args, 1);
}

static void
IspellSetRepeated(Bool state)
{
    static char *mispelled, *repeated;
    Arg args[1];

    if (mispelled == NULL) {
	XtSetArg(args[0], XtNlabel, &mispelled);
	XtGetValues(ispell.mispelled, args, 1);
	mispelled = XtNewString(mispelled);
    }
    if (repeated == NULL) {
	XtSetArg(args[0], XtNlabel, &repeated);
	XtGetValues(ispell.repeated, args, 1);
	repeated = XtNewString(repeated);
    }
    XtSetSensitive(ispell.replaceAll, !state);
    XtSetSensitive(ispell.ignoreAll, !state);
    XtSetSensitive(ispell.add, !state);
    XtSetSensitive(ispell.addUncap, !state);
    if (!state) {
	XtSetArg(args[0], XtNlabel, mispelled);
	XtSetValues(ispell.mispelled, args, 1);
    }
    else {
	XtSetArg(args[0], XtNlabel, repeated);
	XtSetValues(ispell.mispelled, args, 1);
    }
}

static void
IspellSetSensitive(Bool state)
{
    XtSetSensitive(ispell.replace, state);
    XtSetSensitive(ispell.replaceAll, state);
    XtSetSensitive(ispell.ignore, state);
    XtSetSensitive(ispell.ignoreAll, state);
    XtSetSensitive(ispell.add, state);
    XtSetSensitive(ispell.addUncap, state);
}

static void
IspellSetTerseMode(Bool mode)
{
    Arg args[1];

    XtSetArg(args[0], XtNstate, ispell.terse_mode = mode);
    XtSetValues(ispell.terse, args, 1);
    write(ispell.ofd[1], mode ? "!\n" : "%\n", 2);
}

static void
IspellCheckUndo(void)
{
    ispell_undo *undo = XtNew(ispell_undo);

    if (ispell.undo_for && strcmp(ispell.undo_for, ispell.dictionary)) {
	XeditPrintf("Undo: Dictionary changed. Previous undo information lost.\n");
	IspellKillUndoBuffer();
	Feep();
    }

    undo->next = NULL;
    undo->repeat = False;
    undo->terse = ispell.undo_terse_mode;
    undo->format = ispell.format_mode;
    if ((undo->prev = ispell.undo_head) != NULL)
	undo->prev->next = undo;
    else
	undo->prev = NULL;
    ++ispell.undo_depth;
    if (!ispell.undo_base) {
	ispell.undo_base = undo;
	XtSetSensitive(ispell.undo, True);
    }
    else if (ispell.undo_depth > UNDO_DEPTH) {
	ispell_undo *tmp;

	if (ispell.undo_base->undo_str)
	    XtFree(ispell.undo_base->undo_str);
	tmp = ispell.undo_base->next;
	XtFree((char*)ispell.undo_base);
	tmp->prev = NULL;
	ispell.undo_base = tmp;
	ispell.undo_depth = UNDO_DEPTH;
    }
    ispell.undo_head = undo;
}

static char *
IspellReplacedWord(char *word, char *replace)
{
    int			word_len;
    hash_key		*word_key;
    ReplaceEntry	*entry;

    word_len = strlen(word);
    entry = (ReplaceEntry *)hash_check(replace_hash, word, word_len);
    if (entry == NULL) {
	word_key = XtNew(hash_key);
	word_key->value = XtNewString(word);
	word_key->length = word_len;
	entry = XtNew(ReplaceEntry);
	entry->word = word_key;
	entry->replace = NULL;
	entry->next = NULL;
	hash_put(replace_hash, (hash_entry *)entry);
    }

    if (replace) {
	XtFree(entry->replace);
	entry->replace = XtNewString(replace);
    }

    return (entry->replace);
}

static Bool
IspellDoIgnoredWord(char *word, int cmd, int add)
{
    int		word_len;
    hash_key	*word_key;
    IgnoreEntry	*entry;

    word_len = strlen(word);
    entry = (IgnoreEntry *)hash_check(ignore_hash, word, word_len);
    if (entry == NULL) {
	if (cmd != ADD)
	    return (False);

	word_key = XtNew(hash_key);
	word_key->value = XtNewString(word);
	word_key->length = word_len;
	entry = XtNew(IgnoreEntry);
	entry->word = word_key;
	entry->add = add;
	entry->next = NULL;
	hash_put(ignore_hash, (hash_entry *)entry);

	return (True);
    }
    else if (cmd == REMOVE)
	hash_rem(ignore_hash, (hash_entry *)entry);

    return (cmd == CHECK);
}

static Bool
IspellIgnoredWord(char *word, int cmd, int add)
{
    if (add != UNCAP && IspellDoIgnoredWord(word, cmd, add))
	return (True);

    /* add/remove uncapped word to/of list,
     * or cheks for correct capitalization */
    if (add == UNCAP || cmd == CHECK) {
	unsigned char *str = (unsigned char*)word;
	unsigned char string[1024];
	Bool upper, status;
	int i;

	status = True;
	upper = IsUpper(*str);
	*string = upper ? ToLower(*str) : *str;
	if (*str)
	    str++;
	if (IsLower(*str))
	    upper = False;
	for (i = 1; *str && i < sizeof(string) - 1; i++, str++) {
	    if (upper && IsLower(*str))
		status = False;
	    else if (!upper && IsUpper(*str))
		status = False;
	    string[i] = ToLower(*str);
	}
	string[i] = '\0';

	if ((cmd != CHECK || status) &&
	    IspellDoIgnoredWord((char*)string, cmd, add))
	    return (True);
    }

    return (False);
}

/*ARGSUSED*/
static Bool
IspellReceive(void)
{
    int i, len, old_len;
    Arg args[2];
    char *str, *end, **list, **old_list;
    char *tmp, word[1024];
    int j;

    if (ispell.lock || ispell.stat != RECEIVE)
	return (False);

    while (1) {		/* read the entire line */
	if (ispell.buflen >= ispell.bufsiz - 1)
	    ispell.buf = XtRealloc(ispell.buf, ispell.bufsiz += BUFSIZ);
	if ((len = read(ispell.ifd[0], &ispell.buf[ispell.buflen],
			ispell.bufsiz - ispell.buflen - 1)) <= 0)
	    break;
	ispell.buflen += len;
    }
    if (ispell.buflen <= 0)
	return (False);
    len = 0;
    i = ispell.buflen - 1;
    while (i >= 0 && ispell.buf[i] == '\n') {
	++len;
	--i;
    }
    if (len < 2 - ((ispell.terse_mode && i == -1) || ispell.buf[0] == '@'))
	return (False);
    ispell.buf[ispell.buflen - len] = '\0';
    ispell.buflen = 0;

    if ((tmp = strchr(ispell.sendbuf, '\n')) != NULL)
	*tmp = '\0';

    switch (ispell.buf[0]) {
	case '&':	/* MISS */
	case '?':	/* GUESS */
	    str = strchr(&ispell.buf[2], ' ');
	    if (!ispell.checkit) {
		*str = '\0';
		XtSetArg(args[0], XtNlabel, &ispell.buf[2]);
		XtSetValues(ispell.word, args, 1);
	    }
	    ++str;
	    list = NULL;
	    str = strchr(str, ':') + 1;
	    for (i = 0; ; i++) {
		end = strchr(str, ',');
		if (end)	*end = '\0';
		if ((i % 16) == 0)
		    list = (char**)XtRealloc((char*)list, (i + 16) * sizeof(char*));
		tmp = word;
		for (j = 1; j < sizeof(word) && str[j]; j++) {
		    if (str[j] == '+')
			continue;
		    else if (str[j] == '-' && str[j+1] != '-' && str[j-1] != '-') {
			char *p, string[256];
			int k, l;

			for (l = 0, k = j + 1; str[k] != '+' && str[k] != '-'
			     && str[k] && l < sizeof(string) - 1; k++, l++)
			    string[l] = str[k];
			string[l] = '\0';
			*tmp = '\0';
			if (l && (p = strstr(word, string)) != NULL) {
			    char *sav = p;

			    while ((p = strstr(p + l, string)) != NULL)
				sav = p;
			    p = sav;
			    if (strcmp(p, string) == 0) {
				tmp = p;
				j = k - 1;
			    }
			    else
				*tmp++ = '-';
			}
			else
			    *tmp++ = '-';
		    }
		    else
			*tmp++ = str[j];
		}
		*tmp = '\0';
		list[i] = XtNewString(word);

		if (end)	str = end + 1;
		else		break;
	    }
	    len = i + 1;

	    XtSetArg(args[0], XtNlist, &old_list);
	    XtSetArg(args[1], XtNnumberStrings, &old_len);
	    XtGetValues(ispell.list, args, 2);

	    ispell.item = NULL;
	    if ((str = IspellReplacedWord(&ispell.buf[2], NULL)) != NULL)
		for (i = 0; i < len; i++) {
		    if (strcmp(list[i], str) == 0) {
			ispell.item = list[i];
			break;
		    }
		}
	    else
		ispell.item = list[i = 0];
	    if (!ispell.item) {
		list = (char**)XtRealloc((char*)list, (len + 1) * sizeof(char*));
		ispell.item = list[i] = XtNewString(str);
		++len;
	    }

	    XtSetArg(args[0], XtNlist, list);
	    XtSetArg(args[1], XtNnumberStrings, len);
	    XtSetValues(ispell.list, args, 2);

	    XtSetSensitive(ispell.list, True);
	    if (!ispell.checkit)
		XawListHighlight(ispell.list, i);

	    if (old_len > 1 || (XtName(ispell.list) != old_list[0])) {
		while (--old_len > -1)
		    XtFree(old_list[old_len]);
		XtFree((char*)old_list);
	    }

	    if (!ispell.checkit) {
		XtSetArg(args[0], XtNstring, ispell.item);
		XtSetValues(ispell.text, args, 1);
		IspellSetSelection(ispell.left, ispell.right);
		if (ispell.repeat)
		    IspellSetRepeated(ispell.repeat = False);
	    }

	    IspellSetStatus(ispell.buf[0] == '?' ?
			    ispell.guess_label : ispell.miss_label);
	    ispell.undo_terse_mode = ispell.terse_mode;
	    ispell.format_mode = ispell.format_info->value;
	    ispell.lock = True;
	    break;
	case '#':	/* NONE */
	case '-':	/* COMPOUND */
	case '+':	/* ROOT */
	check_label:
	    str = &ispell.sendbuf[1];
	    if (!ispell.checkit) {
		XtSetArg(args[0], XtNlabel, str);
		XtSetValues(ispell.word, args, 1);
	    }

	    XtSetArg(args[0], XtNlist, &old_list);
	    XtSetArg(args[1], XtNnumberStrings, &old_len);
	    XtGetValues(ispell.list, args, 2);
	    ispell.item = NULL;

	    list = (char**)XtMalloc(sizeof(char**));
	    if ((tmp = IspellReplacedWord(str, NULL)) != NULL)
		str = tmp;
	    if (tmp == NULL && ispell.buf[0] == '#')
		list[0] = XtNewString("");
	    else
		list[0] = XtNewString(str);

	    XtSetArg(args[0], XtNlist, list);
	    XtSetArg(args[1], XtNnumberStrings, 1);
	    XtSetValues(ispell.list, args, 2);

	    if (tmp == NULL && ispell.buf[0] == '#') {
		XawListUnhighlight(ispell.list);
		XtSetSensitive(ispell.list, False);
	    }
	    else {
		XtSetSensitive(ispell.list, True);
		if (!ispell.checkit)
		    XawListHighlight(ispell.list, 0);
	    }
	    if (old_len > 1 || (XtName(ispell.list) != old_list[0])) {
		while (--old_len > -1)
		    XtFree(old_list[old_len]);
		XtFree((char*)old_list);
	    }

	    if (!ispell.checkit) {
		XtSetArg(args[0], XtNstring, str);
		XtSetValues(ispell.text, args, 1);
		IspellSetSelection(ispell.left, ispell.right);
		if (ispell.repeat)
		    IspellSetRepeated(ispell.repeat = False);
	    }

	    ispell.undo_terse_mode = ispell.terse_mode;
	    ispell.format_mode = ispell.format_info->value;
	    ispell.lock = True;
	    if (ispell.buf[0] == '+') {
		if ((tmp = strchr(&ispell.buf[2], '\n')) != NULL)
		    *tmp = '\0';
		XmuSnprintf(word, sizeof(word), "%s %s",
			    ispell.root_label, &ispell.buf[2]);
		IspellSetStatus(word);
	    }
	    else
		IspellSetStatus(ispell.buf[0] == '#' ? ispell.none_label :
				ispell.buf[0] == '-' ? ispell.compound_label :
				ispell.ok_label);
	    break;
	case '*':	/* OK */
	case '\0':	/* when running in terse mode */
	    if (!ispell.checkit)
		(void)IspellIgnoredWord(&ispell.sendbuf[1], ADD, 0);
	    else
		goto check_label;
	    ispell.lock = False;
	    break;
	case '@':	/* Ispell banner */
	    /* it only happens when the dictionary is changed */
	    if (!ispell.repeat) {
		XawTextPosition left, right;

		ispell.stat = SEND;
		while (IspellSend() == 0)
		    ;
		/* word chars may have changed */
		XawTextGetSelectionPos(ispell.ascii, &left, &right);
		if (left != ispell.left || right != ispell.right) {
		    XtSetArg(args[0], XtNstring, &ispell.sendbuf[1]);
		    XtSetValues(ispell.text, args, 1);
		    IspellSetSelection(ispell.left, ispell.right);
		}
		ispell.checkit = True;
	    }
	    else {
		IspellSetStatus(ispell.repeat_label);
		ispell.undo_terse_mode = ispell.terse_mode;
		ispell.format_mode = ispell.format_info->value;
		ispell.lock = True;
		return (True);
	    }
	    break;
	default:
	    fprintf(stderr, "Unknown ispell command '%c'\n", ispell.buf[0]);
	    return (False);
    }

    if (!ispell.lock && !ispell.checkit) {
	ispell.stat = SEND;
	while (IspellSend() == 0)
	    ;
    }

    return (True);
}

static int
IspellConvertHtmlAmp(char *buf)
{
    int len, ch = '?';

    /* this function is static, so I can do it */
    *strchr(++buf, ';') = '\0';

    len = strlen(buf);
    if (len == 0)
	return ('&');
    if (len > 1) {
	if (strcasecmp(&buf[1], "lt") == 0)
	    ch = '<';
	else if (strcasecmp(&buf[1], "gt") == 0)
	    ch = '>';
	else if (strcasecmp(&buf[1], "nbsp") == 0)
	    ch = ' ';
	else if (strcasecmp(&buf[1], "amp") == 0)
	    ch = '&';
	else if (strcasecmp(&buf[1], "quot") == 0)
	    ch = '"';
	else if (*buf == '#') {
	    char *tmp;

	    if (len == 1)
		return ('?');
	    ch = strtol(&buf[1], &tmp, 10);
	    if (*tmp)
		fprintf(stderr, "Warning: bad html interpreting '&#' mark.\n");
	}
	else if (strcmp(&buf[1], "acute") == 0) {
	    switch (*buf) {
		case 'a': ch = 0xe1; break;
		case 'e': ch = 0xe9; break;
		case 'i': ch = 0xed; break;
		case 'o': ch = 0xf3; break;
		case 'u': ch = 0xfa; break;
		case 'A': ch = 0xc1; break;
		case 'E': ch = 0xc9; break;
		case 'I': ch = 0xcd; break;
		case 'O': ch = 0xd3; break;
		case 'U': ch = 0xda; break;
	    }
	}
	else if (strcmp(&buf[1], "grave") == 0) {
	    switch (*buf) {
		case 'a': ch = 0xe0; break;
		case 'e': ch = 0xe8; break;
		case 'i': ch = 0xec; break;
		case 'o': ch = 0xf2; break;
		case 'u': ch = 0xf9; break;
		case 'A': ch = 0xc0; break;
		case 'E': ch = 0xc8; break;
		case 'I': ch = 0xcc; break;
		case 'O': ch = 0xd2; break;
		case 'U': ch = 0xd9; break;
	    }
	}
	else if (strcmp(&buf[1], "tilde") == 0) {
	    switch (*buf) {
		case 'a': ch = 0xe3; break;
		case 'o': ch = 0xf5; break;
		case 'n': ch = 0xf1; break;
		case 'A': ch = 0xe3; break;
		case 'O': ch = 0xd5; break;
		case 'N': ch = 0xd1; break;
	    }
	}
	else if (strcmp(&buf[1], "circ") == 0) {
	    switch (*buf) {
		case 'a': ch = 0xe2; break;
		case 'e': ch = 0xea; break;
		case 'i': ch = 0xee; break;
		case 'o': ch = 0xf4; break;
		case 'u': ch = 0xfb; break;
		case 'A': ch = 0xc2; break;
		case 'E': ch = 0xca; break;
		case 'I': ch = 0xce; break;
		case 'O': ch = 0xd4; break;
		case 'U': ch = 0xdb; break;
	    }
	}
	else if (strcmp(&buf[1], "cedil") == 0) {
	    switch (*buf) {
		case 'c': ch = 0xe7; break;
		case 'C': ch = 0xc7; break;
	    }
	}
	/* add more cases here */
    }

    return (ch);
}

/*ARGSUSED*/
static int
IspellSend(void)
{
    XawTextPosition position, old_left, pos;
    XawTextBlock block;
    int i, len, spaces, nls;
    Bool nl, html, inside_html;
    char ampbuf[32];
    int amplen;

    if (ispell.lock || ispell.stat != SEND)
	return (-1);

    len = 1;
    ispell.sendbuf[0] = '^';	/* don't evaluate following characters as commands */

    spaces = nls = 0;

    html = ispell.format_info->value == HTML;
    inside_html = False;
    amplen = 0;

    /* skip non word characters */
    pos = position = ispell.right;
    nl = False;
    while (1) {
	Bool done = False;
	char mb[sizeof(wchar_t)];

	retry_html_space:
	position = XawTextSourceRead(ispell.source, position,
				     &block, BUFSIZ);
	if (block.length == 0) {	/* end of file */
	    ispell.stat = 0;
	    ispell.lock = True;
	    XawTextSetInsertionPoint(ispell.ascii, ispell.right);
	    XawTextUnsetSelection(ispell.ascii);
	    IspellSetSensitive(False);
	    IspellSetStatus(ispell.eof_label);
	    return (-1);
	}
	for (i = 0; i < block.length; i++) {
	    if (international)
		wctomb(mb, ((wchar_t*)block.ptr)[i]);
	    else
		mb[0] = block.ptr[i];
	    if (amplen) {
		if (amplen + 2 >= sizeof(ampbuf)) {
		    if (!ispell.terse_mode)
			fprintf(stderr, "Warning: error interpreting '&' mark.\n");
		    amplen = 0;
		    position = pos + 1;
		    goto retry_html_space;
		}
		else if ((ampbuf[amplen++] = *mb) == ';') {
		    int ch;

		    ampbuf[amplen] = '\0';
		    ch = IspellConvertHtmlAmp(ampbuf);
		    amplen = 0;
		    if (isalpha(ch) ||
			(ch && strchr(ispell.wchars, ch))) {
			/* interpret it again */
			ispell.right = pos;
			i = 0;
			done = True;
			break;
		    }
		    else if ((ch == '\n' || isspace(ch)) && spaces >= 0)
			++spaces;
		    else
			spaces = -1;
		}
	    }
	    else if (html && *mb == '&') {
		ampbuf[amplen++] = *mb;
		pos = block.firstPos + i;
		continue;
	    }
	    else if ((!html || !inside_html) && (isalpha(*mb) ||
		(*mb && strchr(ispell.wchars, *mb)))) {
		done = True;
		break;
	    }
	    else if (!html && *mb == '\n') {
		nl = True;
		if (++nls > 1 && (!html || !inside_html))
		    spaces = -1;
		else if (spaces >= 0)
		    ++spaces;
	    }
	    else if (nl) {
		nl = False;
		if (*mb && strchr(ispell.skip, *mb)) {
		    position = ispell.right =
			XawTextSourceScan(ispell.source, ispell.right + i,
					  XawstEOL, XawsdRight, 1, False);
		    i = 0;
		    break;
		}
		else if (spaces >= 0 && isspace(*mb))
		    ++spaces;
		else
		    spaces = -1;
	    }
	    else if (html && inside_html) {
		if (*mb == '>')
		    inside_html = False;
	    }
	    else if (html && *mb == '<')
		inside_html = True;
	    else if (spaces >= 0 && (isspace(*mb) || (html && *mb == '\n')))
		++spaces;
	    else
		spaces = -1;
	}

	ispell.right += i;
	if (done)
	    break;
    }

    old_left = ispell.left;

    /* read a word */
    position = ispell.left = ispell.right;
    while (1) {
	Bool done = False;
	char mb[sizeof(wchar_t)];

	retry_html_word:
	position = XawTextSourceRead(ispell.source, position,
				     &block, BUFSIZ);
	if (block.length == 0 && len == 1) {	/* end of file */
	    ispell.stat = 0;
	    ispell.lock = True;
	    XawTextSetInsertionPoint(ispell.ascii, ispell.right);
	    XawTextUnsetSelection(ispell.ascii);
	    IspellSetSensitive(False);
	    IspellSetStatus(ispell.eof_label);
	    return (-1);
	}
	for (i = 0; i < block.length; i++) {
	    if (international)
		wctomb(mb, ((wchar_t*)block.ptr)[i]);
	    else
		mb[0] = block.ptr[i];
	    if (amplen) {
		if (amplen + 2 >= sizeof(ampbuf)) {
		    if (!ispell.terse_mode)
			fprintf(stderr, "Warning: error interpreting '&' mark.\n");
		    amplen = 0;
		    position = pos + 1;
		    if (strchr(ispell.wchars, '&')) {
			if (len + 1 >= sizeof(ispell.sendbuf) - 1) {
			    done = True;
			    fprintf(stderr, "Warning: word is too large!\n");
			    break;
			}
			ispell.sendbuf[len++] = '&';
			goto retry_html_word;
		    }
		    else {
			ispell.right = position;
			i = 0;
			done = True;
			break;
		    }
		}
		else if ((ampbuf[amplen++] = *mb) == ';') {
		    int ch;

		    ampbuf[amplen] = '\0';
		    ch = IspellConvertHtmlAmp(ampbuf);
		    amplen = 0;
		    if (!isalpha(ch) &&
			(!ch || !strchr(ispell.wchars, ch))) {
			ispell.right = pos;
			i = 0;
			done = True;
			break;
		    }
		    *mb = ch;
		}
		else
		    continue;
	    }
	    else if (html && *mb == '&') {
		ampbuf[amplen++] = *mb;
		pos = block.firstPos + i;
		continue;
	    }
	    else if (!isalpha(*mb) && (!*mb || !strchr(ispell.wchars, *mb))) {
		done = True;
		break;
	    }
	    ispell.sendbuf[len] = *mb;
	    if (++len >= sizeof(ispell.sendbuf) - 1) {
		done = True;
		fprintf(stderr, "Warning: word is too large!\n");
		break;
	    }
	}
	ispell.right += i;
	if (done || block.length == 0)
	    break;
    }

    ispell.sendbuf[len] = '\0';

    if (spaces > 0 && spaces <= 32 && strcmp(ispell.sendbuf, ispell.sentbuf) == 0) {
	Arg args[2];
	int old_len;	
	char **list, **old_list;
	char label[sizeof(ispell.sendbuf) + sizeof(ispell.sentbuf) + 32];

	strcpy(label, &ispell.sendbuf[1]);
	for (i = 0; i < spaces; i++)
	    label[len + i - 1] = ' ';
	strcpy(&label[len + i - 1], &ispell.sendbuf[1]);
	XtSetArg(args[0], XtNlabel, label);
	XtSetValues(ispell.word, args, 1);

	XtSetArg(args[0], XtNstring, &ispell.sendbuf[1]);
	XtSetValues(ispell.text, args, 1);

	XtSetArg(args[0], XtNlist, &old_list);
	XtSetArg(args[1], XtNnumberStrings, &old_len);
	XtGetValues(ispell.list, args, 2);
	list = (char**)XtMalloc(sizeof(char**));
	list[0] = XtNewString(&ispell.sendbuf[1]);
	XtSetArg(args[0], XtNlist, list);
	XtSetArg(args[1], XtNnumberStrings, 1);
	XtSetValues(ispell.list, args, 2);
	XtSetSensitive(ispell.list, True);
	XawListHighlight(ispell.list, 0);
	if (old_len > 1 || (XtName(ispell.list) != old_list[0])) {
	    while (--old_len > -1)
		XtFree(old_list[old_len]);
	    XtFree((char*)old_list);
	}

	IspellSetRepeated(True);
	IspellSetSelection(old_left, ispell.right);
	IspellSetStatus(ispell.repeat_label);
	ispell.repeat = ispell.lock = True;

	return (1);
    }
    strcpy(ispell.sentbuf, ispell.sendbuf);

    if (len <= 2 || IspellIgnoredWord(&ispell.sendbuf[1], CHECK, 0))
	return (0);

    ispell.sendbuf[len++] = '\n';

    write(ispell.ofd[1], ispell.sendbuf, len);

    ispell.stat = RECEIVE;

    return (1);
}

/*ARGSUSED*/
static void
IspellInputCallback(XtPointer closure, int *source, XtInputId *id)
{
    if (ispell.right < 0) {
	int len;
	char buf[1024];

	ispell.right = XawTextGetInsertionPoint(ispell.ascii);
	ispell.right = XawTextSourceScan(ispell.source, ispell.right,
					      XawstEOL, XawsdLeft, 1, True);
	len = read(ispell.ifd[0], buf, sizeof(buf));
	if (strncmp(buf, "@(#)", 4) == 0) {
	    Arg args[1];

	    buf[len - 1] = '\0';
	    XtSetArg(args[0], XtNtitle, &buf[5]);
	    XtSetValues(ispell.shell, args, 1);
	}
	else
	    fprintf(stderr, "Error: is ispell talking with me?\n");
	IspellSetTerseMode(ispell.terse_mode);
	while (IspellSend() == 0)
	    ;
    }
    else if (ispell.source)
	IspellReceive();
}

/*ARGSUSED*/
void
IspellCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    Cardinal zero = 0;

    IspellAction(textwindow, NULL, NULL, &zero);
}

/*ARGSUSED*/
void
IspellAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg args[3];
    Cardinal num_args;
    char **strs, **list;
    int n_strs;
    Bool first_time = InitIspell();

    if (*num_params == 1 && (params[0][0] == 'e' || params[0][0] == 'E')) {
	PopdownIspell(w, (XtPointer)True, NULL);
	return;
    }

    if (!XtIsSubclass(w, textWidgetClass) || ispell.source) {
	Feep();
	return;
    }

    ispell.source = XawTextGetSource(ispell.ascii = w);

    if (first_time) {
	/* let the user choose the better position for the ispell window */
	Dimension width, height, b_width;
	Position x, y, max_x, max_y;

	x = y = -1;
	if (event) {
	    switch (event->type) {
		case ButtonPress:
		case ButtonRelease:
		    x = event->xbutton.x_root;
		    y = event->xbutton.y_root;
		    break;
		case KeyPress:
		case KeyRelease:
		    x = event->xkey.x_root;
		    y = event->xkey.y_root;
		    break;
	    }
	}
	if (x < 0 || y < 0) {
	    Window r, c;
	    int rx, ry, wx, wy;
	    unsigned mask;

	    XQueryPointer(XtDisplay(ispell.shell), XtWindow(ispell.shell),
			  &r, &c, &rx, &ry, &wx, &wy, &mask);
	    x = rx;
	    y = ry;
	}

	num_args = 0;
	XtSetArg(args[num_args], XtNwidth, &width);		num_args++;
	XtSetArg(args[num_args], XtNheight, &height);		num_args++;
	XtSetArg(args[num_args], XtNborderWidth, &b_width);	num_args++;
	XtGetValues(ispell.shell, args, num_args);

	width += b_width << 1;
	height += b_width << 1;

	x -= (Position)(width >> 1);
	if (x < 0)
	    x = 0;
	if (x > (max_x = (Position)(XtScreen(w)->width - width)))
	    x = max_x;

	y -= (Position)(height >> 1);
	if (y < 0)
	    y = 0;
	if (y > (max_y = (Position)(XtScreen(w)->height - height)))
	    y = max_y;

	num_args = 0;
	XtSetArg(args[num_args], XtNx, x);	num_args++;
	XtSetArg(args[num_args], XtNy, y);	num_args++;
	XtSetValues(ispell.shell, args, num_args);
    }

    if (ispell.repeat)
	IspellSetRepeated(False);
    ispell.lock = ispell.repeat = ispell.checkit = False;
    ispell.stat = SEND;

    IspellSetSensitive(True);
    XtSetSensitive(ispell.undo, False);

    XtSetArg(args[0], XtNlabel, "");
    XtSetValues(ispell.word, args, 1);

    XtSetArg(args[0], XtNstring, "");
    XtSetValues(ispell.text, args, 1);

    XtSetArg(args[0], XtNlist, &strs);
    XtSetArg(args[1], XtNnumberStrings, &n_strs);
    XtGetValues(ispell.list, args, 2);

    list = (char**)XtMalloc(sizeof(char**));
    list[0] = XtNewString("");
    XtSetArg(args[0], XtNlist, list);
    XtSetArg(args[1], XtNnumberStrings, 1);
    XtSetValues(ispell.list, args, 2);

    if (n_strs > 1 || (XtName(ispell.list) != strs[0])) {
	while (--n_strs > -1)
	    XtFree(strs[n_strs]);
	XtFree((char*)strs);
    }

    IspellSetStatus(ispell.working_label);

    if (!ispell.pid)
	(void)IspellStartProcess();
    else {
	ispell.right = XawTextGetInsertionPoint(ispell.ascii);
	ispell.right = XawTextSourceScan(ispell.source, ispell.right,
					      XawstEOL, XawsdLeft, 1, True);
	while (IspellSend() == 0)
	    ;
    }

    XtPopup(ispell.shell, XtGrabExclusive);
    XtSetKeyboardFocus(ispell.shell, ispell.text);
}

static Bool
IspellStartProcess(void)
{
    if (!ispell.pid) {
	int len;
	char format[32];
	static char *command;

	ispell.source = XawTextGetSource(ispell.ascii);

	if (command)
	    XtFree(command);

	strcpy(format, "%s -a");
	len = strlen(ispell.cmd) + 4;
	if (ispell.dictionary && *ispell.dictionary) {
	    len += strlen(ispell.dictionary) + 6;
	    strcat(format, " -d '%s'");
	    if (ispell.wchars && *ispell.wchars) {
		len += strlen(ispell.wchars + 6);
		strcat(format, " -w '%s'");
	    }
	}
	command = XtMalloc(len);
	XmuSnprintf(command, len, format,
		    ispell.cmd, ispell.dictionary, ispell.wchars);

	pipe(ispell.ifd);
	pipe(ispell.ofd);
	if ((ispell.pid = fork()) == 0) {
	    close(0);
	    close(1);
	    dup2(ispell.ofd[0], 0);
	    dup2(ispell.ifd[1], 1);
	    close(ispell.ofd[0]);
	    close(ispell.ofd[1]);
	    close(ispell.ifd[0]);
	    close(ispell.ifd[1]);
	    if (!international)
		setlocale(LC_ALL, "ISO-8859-1");
	    execl("/bin/sh", "sh", "-c", command, NULL);
	    exit(-127);
	}
	else if (ispell.pid < 0) {
	    fprintf(stderr, "Cannot fork\n");
	    exit(1);
	}
	ispell.buf = XtMalloc(ispell.bufsiz = BUFSIZ);
	ispell.right = -1;
	ispell.id = XtAppAddInput(XtWidgetToApplicationContext(ispell.shell),
				  ispell.ifd[0], (XtPointer)XtInputReadMask,
				  IspellInputCallback, NULL);
	fcntl(ispell.ifd[0], F_SETFL, O_NONBLOCK);
    }
    else
	return (False);

    return (True);
}

/*ARGSUSED*/
static void
PopdownIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)IspellEndProcess((Bool)(long)client_data, True);
    XtPopdown(ispell.shell);
    *ispell.sentbuf = '\0';
}

static Bool
IspellCheckProcess(void)
{
    int status;

    if (ispell.pid) {
	waitpid(ispell.pid, &status, WNOHANG);
	if (WIFEXITED(status)) {
	    ispell.pid = 0;
	}
	else
	    return (True);
    }

    return (False);
}

static Bool
IspellEndProcess(Bool killit, Bool killundo)
{
    ispell.source = NULL;

    if (ispell.pid) {
	IgnoreEntry	*ientry;
	ReplaceEntry	*rentry;

	/* insert added words in private dictionary */
	for (ientry = (IgnoreEntry *)hash_iter_first(ignore_hash);
	     ientry;
	     ientry = (IgnoreEntry *)hash_iter_next(ignore_hash)) {
	    if (ientry->add) {
		if (ientry->add == UNCAP)
		    write(ispell.ofd[1], "&", 1);
		else
		    write(ispell.ofd[1], "*", 1);
		write(ispell.ofd[1], ientry->word->value, ientry->word->length);
		write(ispell.ofd[1], "\n", 1);
	    }
	}
	write(ispell.ofd[1], "#\n", 2);		/* save dictionary */
	hash_clr(ignore_hash);

	if (killit) {
	    XtRemoveInput(ispell.id);

	    close(ispell.ofd[0]);
	    close(ispell.ofd[1]);
	    close(ispell.ifd[0]);
	    close(ispell.ifd[1]);

	    /* if something goes wrong, we don't want to block here forever */
	    old_timeout = signal(SIGALRM, timeout_signal);
	    alarm(10);
	    waitpid(ispell.pid, NULL, 0);
	    alarm(0);
	    signal(SIGALRM, old_timeout);

	    ispell.pid = 0;
	    if (ispell.buf)
		XtFree(ispell.buf);
	    ispell.buf = NULL;

	    /* forget about replace matches */
	    for (rentry = (ReplaceEntry *)hash_iter_first(replace_hash);
		 rentry;
		 rentry = (ReplaceEntry *)hash_iter_next(replace_hash)) {
		XtFree(rentry->replace);
	    }
	    hash_clr(replace_hash);
	}

	if (killundo)
	    IspellKillUndoBuffer();
    }
    else
	return (False);

    return (True);
}

static void
IspellKillUndoBuffer(void)
{
    ispell_undo *undo, *pundo;

    undo = pundo = ispell.undo_base;
    while (undo) {
	undo = undo->next;
	if (pundo->undo_str)
	    XtFree(pundo->undo_str);
	XtFree((char*)pundo);
	pundo = undo;
    }
    ispell.undo_base = ispell.undo_head = NULL;
    ispell.undo_for = NULL;
    ispell.undo_depth = 0;
    XtSetSensitive(ispell.undo, False);
}

/*ARGSUSED*/
static void
RevertIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[1];
    char *string, *repstr = NULL;

    XtSetArg(args[0], XtNlabel, &string);
    XtGetValues(ispell.word, args, 1);
    if ((repstr = strchr(string, ' ')) != NULL) {
	string = repstr = XtNewString(string);
	*strchr(repstr, ' ') = '\0';
    }
    XtSetArg(args[0], XtNstring, string);
    XtSetValues(ispell.text, args, 1);
    if (repstr)
	XtFree(repstr);
}

/*ARGSUSED*/
static void
SelectIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    XawListReturnStruct *info = (XawListReturnStruct *)call_data;
    Arg args[1];

    XtSetArg(args[0], XtNstring, ispell.item = info->string);
    XtSetValues(ispell.text, args, 1);
}

/*ARGSUSED*/
void
ReplaceIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    XawTextPosition pos = XawTextGetInsertionPoint(ispell.ascii);
    XawTextBlock check, search, replace;
    Arg args[1];
    char *text;

    if (!ispell.lock)
	return;

    XtSetArg(args[0], XtNlabel, &text);
    XtGetValues(ispell.word, args, 1);
    search.ptr = text;
    search.format = XawFmt8Bit;
    search.firstPos = 0;
    search.length = ispell.right - pos;

    XtSetArg(args[0], XtNstring, &text);
    XtGetValues(ispell.text, args, 1);
    replace.ptr = text;
    replace.format = XawFmt8Bit;
    replace.firstPos = 0;
    replace.length = strlen(text);

    if (strcmp(search.ptr, replace.ptr) != 0 &&
	XawTextReplace(ispell.ascii, pos, pos + search.length,
		       &replace) == XawEditDone) {
	ispell.right += replace.length - search.length;
	IspellCheckUndo();
	ispell.undo_head->undo_str = NULL;
	ispell.undo_head->undo_pos = pos;
	ispell.undo_head->undo_count = 1;

	if (ispell.repeat) {
	    ispell.undo_head->repeat = 2; /* To recognize later it was replaced */
	    ispell.undo_head->undo_count = ispell.right;
	    ispell.undo_head->undo_str = XtNewString(search.ptr);
	}
	if (client_data && !ispell.repeat) {
	    XawTextDisableRedisplay(ispell.ascii);
	    pos = ispell.right;
	    while ((pos = XawTextSourceSearch(ispell.source, pos, XawsdRight, &search))
		!= XawTextSearchError) {
		Bool do_replace = True;
		char mb[sizeof(wchar_t)];

		if (XawTextSourceRead(ispell.source, pos - 1, &check, 1) > 0) {
		    if (international)
			wctomb(mb, *(wchar_t*)check.ptr);
		    else
			mb[0] = check.ptr[0];
		    do_replace = !isalpha(*mb) && *mb && !strchr(ispell.wchars, *mb);
		}
		if (do_replace &&
		    XawTextSourceRead(ispell.source, pos + search.length, &check, 1) > 0) {
		    if (international)
			wctomb(mb, *(wchar_t*)check.ptr);
		    else
			mb[0] = check.ptr[0];
		    do_replace = !isalpha(*mb) && *mb && !strchr(ispell.wchars, *mb);
		}
		if (do_replace) {
		    XawTextReplace(ispell.ascii, pos, pos + search.length, &replace);
		    ++ispell.undo_head->undo_count;
		}
		pos += search.length;
	    }
	    XawTextEnableRedisplay(ispell.ascii);
	}
	(void)IspellReplacedWord(search.ptr, replace.ptr);

	strncpy(&ispell.sentbuf[1], replace.ptr, sizeof(ispell.sentbuf) - 2);
	ispell.sentbuf[sizeof(ispell.sentbuf) - 1] = '\0';
    }
    else
	Feep();

    if (ispell.repeat)
	ispell.right = ispell.left = XawTextGetInsertionPoint(ispell.ascii);
    else if (!ispell.terse_mode || !ispell.item ||
	     strcmp(ispell.item, replace.ptr))
	ispell.right = ispell.left;	/* check it again! */

    ispell.lock = ispell.checkit = False;

    ispell.stat = SEND;
    IspellSetStatus(ispell.working_label);
    while (IspellSend() == 0)
	;
}

/*ARGSUSED*/
void
IgnoreIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[1];
    char *text;

    if (!ispell.lock)
	return;

    XtSetArg(args[0], XtNlabel, &text);
    XtGetValues(ispell.word, args, 1);

    IspellCheckUndo();

    if ((ispell.undo_head->repeat = ispell.repeat) != False) {
	ispell.undo_head->undo_count = ispell.right;
	ispell.undo_head->undo_str = XtNewString(text);
    }
    else
	ispell.undo_head->undo_count = 0;

    ispell.undo_head->undo_pos = XawTextGetInsertionPoint(ispell.ascii);

    if (!ispell.repeat) {
	if (client_data) {
	    IspellIgnoredWord(text, ADD, 0);
	    ispell.undo_head->undo_str = XtNewString(text);
	}
	else 
	    ispell.undo_head->undo_str = NULL;
    }

    ispell.lock = ispell.checkit = False;

    ispell.stat = SEND;
    IspellSetStatus(ispell.working_label);
    while (IspellSend() == 0)
	;
}

/*ARGSUSED*/
void
AddIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[1];
    char *text;
    int cmd = (long)client_data;

    if (!ispell.lock || ispell.repeat)
	return;

    XtSetArg(args[0], XtNlabel, &text);
    XtGetValues(ispell.word, args, 1);

    IspellCheckUndo();
    ispell.undo_head->undo_str = XtNewString(text);
    ispell.undo_head->undo_pos = XawTextGetInsertionPoint(ispell.ascii);
    ispell.undo_head->undo_count = -cmd;

    (void)IspellIgnoredWord(text, ADD, cmd);

    ispell.lock = ispell.checkit = False;
    ispell.stat = SEND;
    IspellSetStatus(ispell.working_label);
    while (IspellSend() == 0)
	;
}

/*ARGSUSED*/
static void
UndoIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    Bool enable_redisplay = False;
    ispell_undo *undo = ispell.undo_head;

    if ((!ispell.lock && ispell.stat) || !undo)
	return;

    if (ispell.undo_for && strcmp(ispell.undo_for, ispell.dictionary)) {
	XeditPrintf("Undo: Dictionary changed. Undo information was lost.\n");
	IspellKillUndoBuffer();
	Feep();
	return;
    }

    if (undo->terse != ispell.terse_mode)
	IspellSetTerseMode(undo->terse);

    if (undo->format != ispell.format_info->value) {
	struct _ispell_format *fmt = &ispell_format[undo->format];
	ChangeFormatIspell(fmt->sme, (XtPointer)fmt, NULL);
    }

    if (undo->undo_count > 0 && !undo->repeat) {
	XawTextPosition tmp;

	enable_redisplay = undo->undo_count > 1;
	if (enable_redisplay)
	    XawTextDisableRedisplay(ispell.ascii);
	while (undo->undo_count--)
	    if (!_XawTextSrcUndo((TextSrcObject)ispell.source, &tmp)) {
		Feep();
		break;
	    }
    }
    else if (undo->undo_count < 0) {
	if (undo->undo_str)
	    (void)IspellIgnoredWord(undo->undo_str, REMOVE, -undo->undo_count);
    }
    else if (undo->undo_str) {
	if (!undo->repeat)
	    IspellIgnoredWord(undo->undo_str, REMOVE, 0);
    }

    XawTextSetInsertionPoint(ispell.ascii,
			     ispell.right = ispell.left = undo->undo_pos);
    if (enable_redisplay)
	XawTextEnableRedisplay(ispell.ascii);

    /* need to do it because may be two misspelled words together */
    if (undo->repeat) {
	char **list, **old_list;
	int old_len;
	Arg args[2];

	if (undo->repeat > 1) {
	    XawTextDisableRedisplay(ispell.ascii);
	    if (!_XawTextSrcUndo((TextSrcObject)ispell.source, &ispell.right))
		Feep();
	    XawTextEnableRedisplay(ispell.ascii);
	}
	else
	    ispell.right = (XawTextPosition)undo->undo_count;
	IspellSetRepeated(ispell.repeat = True);
	XtSetArg(args[0], XtNlabel, undo->undo_str);
	XtSetValues(ispell.word, args, 1);
	XmuSnprintf(ispell.sentbuf, sizeof(ispell.sentbuf), "^%s",
		    strrchr(undo->undo_str, ' ') + 1);
	strcpy(ispell.sendbuf, ispell.sentbuf);
	XtSetArg(args[0], XtNstring, &ispell.sentbuf[1]);
	XtSetValues(ispell.text, args, 1);

	XtSetArg(args[0], XtNlist, &old_list);
	XtSetArg(args[1], XtNnumberStrings, &old_len);
	XtGetValues(ispell.list, args, 2);

	list = (char **)XtMalloc(sizeof(char*));
	list[0] = XtNewString(&ispell.sentbuf[1]);
	XtSetArg(args[0], XtNlist, list);
	XtSetArg(args[1], XtNnumberStrings, 1);
	XtSetValues(ispell.list, args, 2);
	XtSetSensitive(ispell.list, True);
	XawListHighlight(ispell.list, 0);

	if (old_len > 1 || (XtName(ispell.list) != old_list[0])) {
	    while (--old_len > -1)
		XtFree(old_list[old_len]);
	    XtFree((char*)old_list);
	}

	IspellSetSelection(ispell.left, ispell.right);
	IspellSetStatus(ispell.repeat_label);
	ispell.lock = True;
	ispell.checkit = False;
    }
    else if (ispell.repeat) {
	*ispell.sentbuf = '\0';
	IspellSetRepeated(ispell.repeat = False);
    }

    if (undo->prev)
	undo->prev->next = NULL;
    ispell.undo_head = undo->prev;
    if (undo == ispell.undo_base) {
	ispell.undo_base = NULL;
	ispell.undo_for = NULL;
	XtSetSensitive(ispell.undo, False);
    }
    if (undo->undo_str)
	XtFree(undo->undo_str);
    XtFree((char*)undo);
    --ispell.undo_depth;

    if (!ispell.stat || ispell.checkit)
	IspellSetSensitive(True);

    if (!ispell.repeat) {
	ispell.lock = ispell.checkit = False;
	ispell.stat = SEND;
	IspellSetStatus(ispell.working_label);
	while (IspellSend() == 0)
	    ;
    }
}

/*ARGSUSED*/
static void
CheckIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[1];
    char *text, *str, string[1024];
    int i, len;

    if (!ispell.lock)
	return;

    XtSetArg(args[0], XtNstring, &text);
    XtGetValues(ispell.text, args, 1);

    /* Check only a word at a time */
    len = 0;
    str = text;
    while (*str) {
	if (isalpha(*str) || strchr(ispell.wchars, *str))
	    break;
	++str;
	++len;
    }
    i = 0;
    while (*str) {
	if (isalpha(*str) || strchr(ispell.wchars, *str))
	    string[i++] = *str++;
	else
	    break;
    }
    string[i] = '\0';

    if (strcmp(text, string)) {
	XawTextPosition pos = XawTextGetInsertionPoint(ispell.text) - len;

	XtSetArg(args[0], XtNstring, string);
	XtSetValues(ispell.text, args, 1);
	XawTextSetInsertionPoint(ispell.text, pos);
	Feep();
    }

    if (i == 0) {
	Feep();
	return;
    }

    len = XmuSnprintf(ispell.sendbuf, sizeof(ispell.sendbuf), "^%s\n", string);

    ispell.sendbuf[sizeof(ispell.sendbuf) - 1] = '\n';

    write(ispell.ofd[1], ispell.sendbuf, len);

    ispell.lock = False;
    ispell.checkit = True;
    ispell.stat = RECEIVE;
}

/*ARGSUSED*/
static void
LookIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    int len, old_len;
    FILE *fd;
    Arg args[2];
    char *text, *str, **list, **old_list, command[1024], buffer[1024];
    Bool sensitive = True;

    if (!ispell.lock)
	return;

    XtSetArg(args[0], XtNstring, &text);
    XtGetValues(ispell.text, args, 1);

    if (!*text) {
	Feep();
	return;
    }

    if (strlen(ispell.look_cmd) + strlen(text) + strlen(ispell.words_file) + 8
	> sizeof(command) - 1) {
	fprintf(stderr, "Command line too large\n");
	return;
    }

    XmuSnprintf(command, sizeof(command), "%s '^%s.*$' %s",
		ispell.look_cmd, text, ispell.words_file);

    if ((fd = popen(command, "r")) == NULL) {
	fprintf(stderr, "Cannot popen '%s'\n", ispell.look_cmd);
	return;
    }

    list = NULL;
    len = 0;

#define	MAX_LOOK_RESULTS	256
    while (fgets(buffer, sizeof(buffer), fd) != NULL) {
	if ((str = strchr(buffer, '\n')) == NULL) {
	    fprintf(stderr, "String is too large\n");
	    break;
	}
	*str = '\0';
	if ((len % 16) == 0)
	    list = (char**)XtRealloc((char*)list, sizeof(char*) * (len + 16));
	list[len] = XtNewString(buffer);
	if (++len >= MAX_LOOK_RESULTS) {
	    Feep();
	    break;
	}
    }
#undef MAX_LOOK_RESULTS

    XtSetArg(args[0], XtNlist, &old_list);
    XtSetArg(args[1], XtNnumberStrings, &old_len);
    XtGetValues(ispell.list, args, 2);

    if (len == 0) {
	list = (char**)XtMalloc(sizeof(char*));
	list[0] = XtNewString("");
	len = 1;
	sensitive = False;
    }

    XtSetArg(args[0], XtNlist, list);
    XtSetArg(args[1], XtNnumberStrings, len);
    XtSetValues(ispell.list, args, 2);

    XtSetSensitive(ispell.list, sensitive);
    IspellSetStatus(sensitive ? ispell.look_label : ispell.none_label);

    if (old_len > 1 || (XtName(ispell.list) != old_list[0])) {
	while (--old_len > -1)
	    XtFree(old_list[old_len]);
	XtFree((char*)old_list);
    }

    pclose(fd);
}

/*ARGSUSED*/
static void
ToggleTerseIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (!ispell.lock)
	return;

    ispell.terse_mode = !ispell.terse_mode;
    write(ispell.ofd[1], ispell.terse_mode ? "!\n" : "%\n", 2);
}

/*ARGSUSED*/
static void
ChangeDictionaryIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    ispell_dict *tmp, *dic = (ispell_dict*)client_data;
    XawTextPosition pos = XawTextGetInsertionPoint(ispell.ascii);
    XawTextPosition right = ispell.right;
    Arg args[1];

    if (strcmp(XtName(dic->sme), ispell.dictionary) == 0)
	return;

    if (!ispell.lock) {
	if (IspellCheckProcess()) {
	    Feep();
	    return;
	}
    }

    for (tmp = ispell.dict_info; tmp; tmp = tmp->next)
	if (strcmp(XtName(tmp->sme), ispell.dictionary) == 0) {
	    XtSetArg(args[0], XtNleftBitmap, None);
	    XtSetValues(tmp->sme, args, 1);
	}

    if (ispell.undo_base && !ispell.undo_for)
	ispell.undo_for = ispell.dictionary;

    XtSetArg(args[0], XtNleftBitmap, flist.pixmap);
    XtSetValues(dic->sme, args, 1);
    ispell.dictionary = XtName(dic->sme);
    ispell.wchars = dic->wchars;
    XtSetArg(args[0], XtNlabel, XtName(dic->sme));
    XtSetValues(ispell.dict, args, 1);

    IspellSetStatus(ispell.working_label);

    (void)IspellEndProcess(True, False);
    ispell.lock = ispell.checkit = False;
    (void)IspellStartProcess();

    ispell.stat = RECEIVE;

    /* restart at the same selected word */
    if (ispell.repeat == False)
	ispell.left = ispell.right = pos;
    else
	ispell.right = right;
}

/*ARGSUSED*/
static void
ChangeFormatIspell(Widget w, XtPointer client_data, XtPointer call_data)
{
    struct _ispell_format *fmt = (struct _ispell_format*)client_data;
    Arg args[1];

    if (strcmp(fmt->name, ispell.formatting) == 0)
	return;

    if (!ispell.lock) {
	Feep();
	return;
    }

    XtSetArg(args[0], XtNleftBitmap, None);
    XtSetValues(ispell.format_info->sme, args, 1);

    XtSetArg(args[0], XtNleftBitmap, flist.pixmap);
    XtSetValues(fmt->sme, args, 1);
    ispell.formatting = fmt->name;
    ispell.format_info = fmt;
    XtSetArg(args[0], XtNlabel, fmt->name);
    XtSetValues(ispell.format, args, 1);
}

static Bool
InitIspell(void)
{
    Atom delete_window;
    char *str, *list;
    XtResource dict_res;
    ispell_dict *dict, *prev_dict;
    int i;
    static XtResource text_res[] = {
	{"skipLines", "Skip", XtRString, sizeof(char*),
	 XtOffsetOf(struct _ispell, skip), XtRString, "#"},
    };

    if (ispell.shell)
	return (False);

    replace_hash = hash_new(RSTRTBLSZ, NULL);
    ignore_hash = hash_new(ISTRTBLSZ, NULL);

    ispell.shell	= XtCreatePopupShell("ispell", transientShellWidgetClass,
					     topwindow, NULL, 0);

    XtGetApplicationResources(ispell.shell, (XtPointer)&ispell, resources,
			      XtNumber(resources), NULL, 0);

    ispell.form		= XtCreateManagedWidget("form", formWidgetClass,
						ispell.shell, NULL, 0);
    ispell.mispelled	= XtCreateManagedWidget("mispelled", labelWidgetClass,
						ispell.form, NULL, 0);
    ispell.repeated	= XtCreateWidget("repeated", labelWidgetClass,
					 ispell.form, NULL, 0);
    ispell.word		= XtCreateManagedWidget("word", commandWidgetClass,
						ispell.form, NULL, 0);
    XtAddCallback(ispell.word, XtNcallback, RevertIspell, NULL);
    ispell.replacement	= XtCreateManagedWidget("replacement", labelWidgetClass,
						ispell.form, NULL, 0);
    ispell.text		= XtVaCreateManagedWidget("text", asciiTextWidgetClass,
						ispell.form,
						XtNeditType, XawtextEdit,
						NULL);
    ispell.suggestions	= XtCreateManagedWidget("suggestions", labelWidgetClass,
						ispell.form, NULL, 0);
    ispell.viewport	= XtCreateManagedWidget("viewport", viewportWidgetClass,
						ispell.form, NULL, 0);
    ispell.list		= XtCreateManagedWidget("list", listWidgetClass,
						ispell.viewport, NULL, 0);
    XtAddCallback(ispell.list, XtNcallback, SelectIspell, NULL);
    ispell.commands	= XtCreateManagedWidget("commands", formWidgetClass,
						ispell.form, NULL, 0);
    ispell.check	= XtCreateManagedWidget("check", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.check, XtNcallback, CheckIspell, NULL);
    ispell.look		= XtCreateManagedWidget("look", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.look, XtNcallback, LookIspell, NULL);
    ispell.undo		= XtCreateManagedWidget("undo", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.undo, XtNcallback, UndoIspell, NULL);
    ispell.replace	= XtCreateManagedWidget("replace", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.replace, XtNcallback, ReplaceIspell, (XtPointer)False);
    ispell.replaceAll	= XtCreateManagedWidget("replaceAll", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.replaceAll, XtNcallback, ReplaceIspell, (XtPointer)True);
    ispell.ignore	= XtCreateManagedWidget("ignore", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.ignore, XtNcallback, IgnoreIspell, (XtPointer)False);
    ispell.ignoreAll	= XtCreateManagedWidget("ignoreAll", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.ignoreAll, XtNcallback, IgnoreIspell, (XtPointer)True);
    ispell.add		= XtCreateManagedWidget("add", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.add, XtNcallback, AddIspell, (XtPointer)ASIS);
    ispell.addUncap	= XtCreateManagedWidget("addUncap", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.addUncap, XtNcallback, AddIspell, (XtPointer)UNCAP);
    ispell.suspend	= XtCreateManagedWidget("suspend", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.suspend, XtNcallback, PopdownIspell, (XtPointer)False);
    ispell.cancel	= XtCreateManagedWidget("cancel", commandWidgetClass,
						ispell.commands, NULL, 0);
    XtAddCallback(ispell.cancel, XtNcallback, PopdownIspell, (XtPointer)True);
    ispell.terse	= XtVaCreateManagedWidget("terse", toggleWidgetClass,
						  ispell.commands,
						  XtNstate, ispell.terse_mode,
						  NULL);
    XtAddCallback(ispell.terse, XtNcallback, ToggleTerseIspell, NULL);
    ispell.status	= XtCreateManagedWidget("status", labelWidgetClass,
						ispell.form, NULL, 0);
    ispell.options	= XtCreateManagedWidget("options", formWidgetClass,
						ispell.form, NULL, 0);
    ispell.dict		= XtVaCreateManagedWidget("dict", menuButtonWidgetClass,
						  ispell.options,
						  XtNmenuName, "dictionaries",
						  NULL);
    ispell.dictMenu	= XtCreatePopupShell("dictionaries", simpleMenuWidgetClass,
					     ispell.options, NULL, 0);
    XtRealizeWidget(ispell.dictMenu);

    ispell.format	= XtVaCreateManagedWidget("format", menuButtonWidgetClass,
						  ispell.options,
						  XtNmenuName, "formats",
						  NULL);
    ispell.formatMenu	= XtCreatePopupShell("formats", simpleMenuWidgetClass,
					     ispell.options, NULL, 0);
    XtRealizeWidget(ispell.formatMenu);

    XtRealizeWidget(ispell.shell);

    for (i = 0; i < sizeof(ispell_format) / sizeof(ispell_format[0]); i++) {
	struct _ispell_format *fmt = &ispell_format[i];

	fmt->sme = XtCreateManagedWidget(fmt->name, smeBSBObjectClass,
					 ispell.formatMenu, NULL, 0);
	XtAddCallback(fmt->sme, XtNcallback, ChangeFormatIspell, (XtPointer)fmt);

	if (strcmp(fmt->name, ispell.formatting) == 0) {
	    Arg args[1];

	    XtSetArg(args[0], XtNlabel, ispell.formatting);
	    XtSetValues(ispell.format, args, 1);
	    XtSetArg(args[0], XtNleftBitmap, flist.pixmap);
	    XtSetValues(fmt->sme, args, 1);
	    ispell.format_info = fmt;
	}
    }
    if (ispell.format_info == NULL) {
	Arg args[1];
	char msg[256];

	ispell.format_info = &ispell_format[TEXT];

	XmuSnprintf(msg, sizeof(msg),
		    "Unrecognized formatting type \"%s\", will use \"%s\"",
		    ispell.formatting, ispell.format_info->name);
	XtAppWarning(XtWidgetToApplicationContext(ispell.shell), msg);
	ispell.formatting = ispell.format_info->name;

	XtSetArg(args[0], XtNlabel, ispell.format_info->name);
	XtSetValues(ispell.format, args, 1);
	XtSetArg(args[0], XtNleftBitmap, flist.pixmap);
	XtSetValues(ispell.format_info->sme, args, 1);
    }
    XtGetApplicationResources(ispell_format[TEXT].sme, (XtPointer)&ispell,
			      text_res, XtNumber(text_res), NULL, 0);

    dict_res.resource_name = "wordChars";
    dict_res.resource_class = "Chars";
    dict_res.resource_type = XtRString;
    dict_res.resource_size = sizeof(char*);
    dict_res.resource_offset = XtOffsetOf(ispell_dict, wchars);
    dict_res.default_type = XtRString;
    dict_res.default_addr = "";

    list = XtNewString(ispell.dict_list);

    /* Create first empty entry */
    dict = XtNew(ispell_dict);
    dict->sme = XtCreateManagedWidget("", smeBSBObjectClass,
				      ispell.dictMenu, NULL, 0);
    dict->wchars = "";
    XtAddCallback(dict->sme, XtNcallback, ChangeDictionaryIspell,
		  (XtPointer)dict);
    ispell.dict_info = prev_dict = dict;

    for (str = strtok(list, " \t,"); str; str = strtok(NULL, " \t,")) {
	dict = XtNew(ispell_dict);
	dict->sme = XtCreateManagedWidget(str, smeBSBObjectClass,
					  ispell.dictMenu, NULL, 0);
	XtGetApplicationResources(dict->sme, (XtPointer)dict, &dict_res,
				  1, NULL, 0);
	XtAddCallback(dict->sme, XtNcallback, ChangeDictionaryIspell,
		      (XtPointer)dict);
	prev_dict->next = dict;
	prev_dict = dict;
	dict->next = NULL;
    }
    XtFree(list);

    for (dict = ispell.dict_info; dict; dict = dict->next) {
	if (strcmp(XtName(dict->sme), ispell.dictionary) == 0) {
	    Arg args[1];

	    XtSetArg(args[0], XtNleftBitmap, flist.pixmap);
	    XtSetValues(dict->sme, args, 1);
	    XtSetArg(args[0], XtNlabel, XtName(dict->sme));
	    XtSetValues(ispell.dict, args, 1);
	    ispell.wchars = dict->wchars;
	    break;
	}
    }


    delete_window = XInternAtom(XtDisplay(ispell.shell), "WM_DELETE_WINDOW", False);
    XSetWMProtocols(XtDisplay(ispell.shell), XtWindow(ispell.shell), &delete_window, 1);

    return (True);
}
