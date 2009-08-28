/*
 * Copyright © 2007 Paulo César Pereira de Andrade
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Paulo César Pereira de Andrade
 */

/*
 *   Certain tag files may require quite some time and memory to load.
 * Linux kernel 2.6x is an example, the tags file itself is almost 80Mb
 * and xedit will use over 100Mb to store the data, and take quite some
 * time to load it (and can grow drastically with every loaded files
 * due to the memory used by the file contents and internal structures,
 * like the syntax highlight ones).
 *   Possible workarounds could be to load the tags file in a separate
 * process or thread. The memory problem would be hard to circunvent,
 * as the tags file metadata would need to be stored in some very fast
 * database, or at least some special format that would not require
 * a linear search in a huge tags file.
 */

#include "xedit.h"
#include "util.h"
#include "re.h"
#include <unistd.h>

/*
 * Types
 */
typedef struct _TagsEntry	TagsEntry;
typedef struct _RegexEntry	RegexEntry;

struct _TagsEntry {
    hash_key	*symbol;
    TagsEntry	*next;

    int		nentries;
    hash_entry	**filenames;
    char	**patterns;
};

struct _RegexEntry {
    hash_key	*pattern;
    RegexEntry	*next;

    re_cod	regex;
};

struct _XeditTagsInfo {
    hash_key		*pathname;
    XeditTagsInfo	*next;

    hash_table		*entries;
    hash_table		*filenames;
    hash_table		*patterns;

    /* Used when searching for alternate tags and failing descending to
     * root directory */
    Boolean		visited;

    /* Flag to know if tags file is in xedit cwd and allow using relative
     * pathnames when loading a file with some tag definition, so that
     * other code will not fail to write file (or even worse, write to
     * wrong file) if file is edited and tags is not in the current dir */
    Boolean		incwd;

    /* Cache information for circulating over multiple definitions */
    XeditTagsInfo	*tags;		/* If trying another TagsInfo */
    TagsEntry		*entry;		/* Entry in tags->tags */
    int			offset;
    Widget		textwindow;
    XawTextPosition	position;
};

/*
 * Prototypes
 */
static XeditTagsInfo *LoadTagsFile(char *tagsfile);
static XeditTagsInfo *DoLoadTagsFile(char *tagsfile, int length);
static void FindTagFirst(XeditTagsInfo *tags, char *symbol, int length);
static void FindTagNext(XeditTagsInfo *tags,
			Widget window, XawTextPosition position);
static void FindTag(XeditTagsInfo *tags);

/*
 * Initialization
 */
extern Widget texts[3];
static hash_table *ht_tags;

/*
 * Implementation
 */
void
TagsAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    xedit_flist_item	*item;
    char		buffer[1024];
    XawTextPosition	position, left, right;
    XawTextBlock	block;
    int			length;
    Widget		source;

    source = XawTextGetSource(w);
    item = FindTextSource(source, NULL);
    if (item->tags == NULL)
	SearchTagsFile(item);

    if (item->tags) {
	position = XawTextGetInsertionPoint(w);
	XawTextGetSelectionPos(w, &left, &right);
	if (right > left) {
	    XawTextSourceRead(source, left, &block, right - left);
	    length = block.length + 1;
	    if (length >= sizeof(buffer))
		length = sizeof(buffer);
	    XmuSnprintf(buffer, length, "%s", block.ptr);
	    item->tags->textwindow = w;
	    item->tags->position = position;
	    FindTagFirst(item->tags, buffer, length - 1);
	}
	else
	    FindTagNext(item->tags, w, position);
    }
    else
	Feep();
}

void
SearchTagsFile(xedit_flist_item *item)
{
    if (app_resources.loadTags) {
	char		buffer[BUFSIZ];
	char		*ptr, *tagsfile;
	int		length;
	Boolean		exists;
	FileAccess	file_access;

	tagsfile = NULL;

	/* If path fully specified in resource */
	if (app_resources.tagsName[0] == '/')
	    tagsfile = ResolveName(app_resources.tagsName);
	/* Descend up to root directory searching for a tags file */
	else {
	    /* *scratch* buffer */
	    if (item->filename[0] != '/') {
		ptr = ResolveName(app_resources.tagsName);
		strncpy(buffer, ptr ? ptr : "", sizeof(buffer));
	    }
	    else
		strncpy(buffer, item->filename, sizeof(buffer));

	    /* Make sure buffer is nul terminated */
	    buffer[sizeof(buffer) - 1] = '\0';
	    ptr = buffer + strlen(buffer);

	    for (;;) {
		while (ptr > buffer && ptr[-1] != '/')
		    --ptr;
		if (ptr <= buffer)
		    break;
		length = ptr - buffer;
		if (length >= sizeof(buffer))
		    length = sizeof(buffer);
		strncpy(ptr, app_resources.tagsName,
			sizeof(buffer) - length);
		buffer[sizeof(buffer) - 1] = '\0';

		/* Check if tags filename exists */
		tagsfile = ResolveName(buffer);
		if (tagsfile != NULL) {
		    file_access = CheckFilePermissions(tagsfile, &exists);
		    /* Check if can read tagsfile */
		    if (exists &&
			(file_access == READ_OK || file_access == WRITE_OK))
			break;
		    else
			tagsfile = NULL;
		}
		*--ptr = '\0';
	    }
	}

	if (tagsfile)
	    item->tags = LoadTagsFile(tagsfile);
	else {
	    XeditPrintf("No tags file found."
			" Run \"ctags -R\" to build a tags file.\n");
	    item->tags = NULL;
	}
    }
}

static void
FindTagFirst(XeditTagsInfo *tags, char *symbol, int length)
{
    char	*ptr;
    TagsEntry	*entry;
    char	buffer[BUFSIZ];

    /* Check for malformed parameters */
    ptr = symbol;
    while (*ptr) {
	if (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r' ||
	    *ptr == '(' || *ptr == ')') {
	    Feep();
	    return;
	}
	ptr++;
    }

    /* First try in buffer tags */
    tags->tags = tags;
    entry = (TagsEntry *)hash_check(tags->entries, symbol, length);
    if (entry == NULL) {
	/* Try to find in alternate tags */
	strncpy(buffer, tags->pathname->value, tags->pathname->length);
	buffer[tags->pathname->length] = '\0';
	ptr = buffer + tags->pathname->length - 1;

	for (tags->tags = (XeditTagsInfo *)hash_iter_first(ht_tags);
	     tags->tags;
	     tags->tags = (XeditTagsInfo *)hash_iter_next(ht_tags))
	    tags->tags->visited = False;

	tags->visited = True;

	while (ptr > buffer && entry == NULL) {
	    --ptr;
	    while (ptr > buffer && ptr[-1] != '/')
		--ptr;
	    if (ptr <= buffer)
		break;
	    *ptr = '\0';

	    /* Try an upper directory tags */
	    tags->tags = (XeditTagsInfo *)
		hash_check(ht_tags, buffer, ptr - buffer);
	    if (tags->tags) {
		tags->tags->visited = True;
		entry = (TagsEntry *)
		    hash_check(tags->tags->entries, symbol, length);
	    }
	}

	/* If still failed, check other available tags
	 * for possible different projects */
	if (entry == NULL) {
	    for (tags->tags = (XeditTagsInfo *)hash_iter_first(ht_tags);
		 tags->tags;
		 tags->tags = (XeditTagsInfo *)hash_iter_next(ht_tags)) {
		if (tags->tags->visited == False) {
		    entry = (TagsEntry *)
			hash_check(tags->tags->entries, symbol, length);
		    /* Stop on first match */
		    if (entry != NULL)
			break;
		}
	    }
	}

	if (entry == NULL) {
	    XeditPrintf("Symbol %s not in tags\n", symbol);
	    Feep();
	    return;
	}
    }

    tags->entry = entry;
    tags->offset = 0;

    FindTag(tags);
}

static void
FindTagNext(XeditTagsInfo *tags, Widget window, XawTextPosition position)
{
    if (window != tags->textwindow || position != tags->position)
	Feep();
    else {
	if (tags->entry->nentries > 1) {
	    if (++tags->offset >= tags->entry->nentries)
		tags->offset = 0;
	    FindTag(tags);
	}
	else
	    Feep();
    }
}

static XeditTagsInfo *
LoadTagsFile(char *tagsfile)
{
    XeditTagsInfo	*tags;
    int			length;

    if (ht_tags == NULL)
	ht_tags = hash_new(11, NULL);

    /* tags key is only the directory name with ending '/' */
    length = strlen(tagsfile) - strlen(app_resources.tagsName);
    tags = (XeditTagsInfo *)hash_check(ht_tags, tagsfile, length);

    return (tags ? tags : DoLoadTagsFile(tagsfile, length));
}

static XeditTagsInfo *
DoLoadTagsFile(char *tagsfile, int length)
{
    char		*ptr;
    FILE		*file;
    XeditTagsInfo	*tags;
    TagsEntry		*entry;
    hash_entry		*file_entry;
    char		buffer[BUFSIZ];
    char		*symbol, *filename, *pattern;

    file = fopen(tagsfile, "r");
    if (file) {
	char *cwd;

	tags = XtNew(XeditTagsInfo);

	cwd = getcwd(buffer, sizeof(buffer));
	tags->incwd = cwd &&
	    (strlen(cwd) == length - 1 &&
	     memcmp(cwd, tagsfile, length - 1) == 0);

	/* Build pathname as a nul terminated directory specification string */
	tags->pathname = XtNew(hash_key);
	tags->pathname->value = XtMalloc(length + 1);
	tags->pathname->length = length;
	memcpy(tags->pathname->value, tagsfile, length);
	tags->pathname->value[length] = '\0';
	tags->next = NULL;

	tags->entries = hash_new(809, NULL);
	tags->filenames = hash_new(31, NULL);
	tags->patterns = hash_new(47, NULL);

	/* Cache information */
	tags->tags = tags;	/* :-) */
	tags->entry = NULL;
	tags->offset = 0;
	tags->textwindow = NULL;
	tags->position = 0;

	while (fgets(buffer, sizeof(buffer) - 1, file)) {
	    /* XXX Ignore malformed lines and tags file format information */
	    if (isspace(buffer[0]) || buffer[0] == '!')
		continue;

	    /* Symbol name */
	    symbol = ptr = buffer;
	    while (*ptr && !isspace(*ptr))
		ptr++;
	    *ptr++ = '\0';
	    while (isspace(*ptr))
		ptr++;

	    /* Filename with basename of tagsfile for symbol definition */
	    filename = ptr;
	    while (*ptr && !isspace(*ptr))
		ptr++;
	    *ptr++ = '\0';
	    while (isspace(*ptr))
		ptr++;

	    pattern = ptr;
	    /* Check for regex */
	    if (*pattern == '/' || *pattern == '?') {
		ptr++;
		while (*ptr && *ptr != *pattern) {
		    if (*ptr == '\\') {
			if (ptr[1] == *pattern || ptr[1] == '\\') {
			    /* XXX tags will escape pattern end, and backslash
			     * not sure about other special characters */
			    memmove(ptr, ptr + 1, strlen(ptr));
			}
			else {
			    ++ptr;
			    if (!*ptr)
				break;
			}
		    }
		    ptr++;
		}

		if (*ptr != *pattern)
		    continue;
		++pattern;
		/*   Will do a RE_NOSPEC search, that means ^ and $
                 * would be literally search (do this to avoid escaping
		 * other regex characters and building a fast/simple literal
		 * string search pattern.
		 *   Expect patterns to be full line */
		if (*pattern == '^' && ptr[-1] == '$') {
		    ++pattern;
		    --ptr;
		}
	    }
	    /* Check for line number */
	    else if (isdigit(*ptr)) {
		while (isdigit(*ptr))
		    ptr++;
	    }
	    /* Format not understood */
	    else
		continue;

	    *ptr = '\0';

	    length = strlen(symbol);
	    entry = (TagsEntry *)hash_check(tags->entries,
					    symbol, length);
	    if (entry == NULL) {
		entry = XtNew(TagsEntry);
		entry->symbol = XtNew(hash_key);
		entry->symbol->value = XtNewString(symbol);
		entry->symbol->length = length;
		entry->next = NULL;
		entry->nentries = 0;
		entry->filenames = NULL;
		entry->patterns = NULL;
		hash_put(tags->entries, (hash_entry *)entry);
	    }

	    length = strlen(filename);
	    file_entry = hash_check(tags->filenames, filename, length);
	    if (file_entry == NULL) {
		file_entry = XtNew(hash_entry);
		file_entry->key = XtNew(hash_key);
		file_entry->key->value = XtNewString(filename);
		file_entry->key->length = length;
		file_entry->next = NULL;
		hash_put(tags->filenames, file_entry);
	    }

	    if ((entry->nentries % 4) == 0) {
		entry->filenames = (hash_entry **)
			XtRealloc((char *)entry->filenames,
				  sizeof(hash_entry *) *
				  (entry->nentries + 4));
		entry->patterns = (char **)
			XtRealloc((char *)entry->patterns,
				  sizeof(char *) *
				  (entry->nentries + 4));
	    }
	    entry->filenames[entry->nentries] = file_entry;
	    entry->patterns[entry->nentries] = XtNewString(pattern);
	    ++entry->nentries;
	}
	fclose(file);

	/* Add tags information to global hash table */
	hash_put(ht_tags, (hash_entry *)tags);
	XeditPrintf("Tags file %s loaded\n", tagsfile);
    }
    else {
	XeditPrintf("Failed to load tags file %s\n", tagsfile);
	tags = NULL;
    }

    return (tags);
}

static void
FindTag(XeditTagsInfo *tags)
{
    static String	params[] = { "vertical", NULL };

    char		buffer[BUFSIZ];
    char		*pattern;
    int			length;
    char		*line;
    char		*text;
    RegexEntry		*regex;
    re_mat		match;
    XawTextPosition	position, left, right, last;
    Widget		source;
    XawTextBlock	block;
    int			size;
    int			lineno;
    Boolean		found;
    xedit_flist_item	*item;
    Widget		otherwindow;

    XmuSnprintf(buffer, sizeof(buffer), "%s%s", tags->tags->pathname->value,
		tags->entry->filenames[tags->offset]->key->value);

    pattern = tags->entry->patterns[tags->offset];
    if (isdigit(*pattern)) {
	lineno = atoi(pattern);
	regex = NULL;
    }
    else {
	lineno = 0;
	length = strlen(pattern);
	regex = (RegexEntry *)hash_check(tags->patterns, pattern, length);
	if (regex == NULL) {
	    regex = XtNew(RegexEntry);
	    regex->pattern = XtNew(hash_key);
	    regex->pattern->value = XtNewString(pattern);
	    regex->pattern->length = length;
	    regex->next = NULL;
	    if (recomp(&regex->regex, pattern, RE_NOSUB | RE_NOSPEC)) {
		XeditPrintf("Failed to compile regex %s\n", pattern);
		Feep();
		return;
	    }
	    hash_put(tags->patterns, (hash_entry *)regex);
	}
    }

    /* Short circuit to know if split horizontally */
    if (!XtIsManaged(texts[1]))
	XtCallActionProc(textwindow, "split-window", NULL, params, 1);

    /* Switch to "other" buffer */
    XtCallActionProc(textwindow, "other-window", NULL, NULL, 0);

    /* This should print an error message if tags file cannot be read */
    if (!LoadFileInTextwindow(tags->incwd ?
			      tags->entry->filenames[tags->offset]->key->value :
			      buffer, buffer))
	return;

    otherwindow = textwindow;

    item = FindTextSource(XawTextGetSource(textwindow), NULL);
    source = item->source;
    left = XawTextSourceScan(source, 0, XawstAll, XawsdLeft, 1, True);

    found = False;

    if (lineno) {
	right = RSCAN(left, lineno, False);
	left = LSCAN(right, 1, False);
	found = True;
    }
    else {
	right = RSCAN(left, 1, True);
	last = XawTextSourceScan(source, 0, XawstAll, XawsdRight, 1, True);
	text = buffer;

	size = sizeof(buffer);
	for (;;) {
	    length = right - left;
	    match.rm_so = 0;
	    match.rm_eo = length;
	    XawTextSourceRead(source, left, &block, right - left);
	    if (block.length >= length)
		line = block.ptr;
	    else {
		if (length > size) {
		    if (text == buffer)
			text = XtMalloc(length);
		    else
			text = XtRealloc(text, length);
		    size = length;
		}
		line = text;
		memcpy(line, block.ptr, block.length);
		length = block.length;
		for (position = left + length;
		     position < right;
		     position += block.length) {
		    XawTextSourceRead(source, position, &block, right - position);
		    memcpy(line + length, block.ptr, block.length);
		    length += block.length;
		}
	    }

	    /* If not last line or if it ends in a newline */
	    if (right < last ||
		(right > left && line[match.rm_eo - 1] == '\n')) {
		--match.rm_eo;
		length = match.rm_eo;
	    }

	    /* Accept as a match when matching the entire line, as the regex
	     * search pattern is optmized to not need to start with ^ and not
	     * need to end with $*/
	    if (reexec(&regex->regex, line, 1, &match, RE_STARTEND) == 0 &&
		match.rm_eo > match.rm_so &&
		match.rm_so == 0 && match.rm_eo == length) {
		right = left + match.rm_so + (match.rm_eo - match.rm_so);
		found = True;
		break;
	    }
	    else if (right >= last) {
		XeditPrintf("Failed to match regex %s\n", pattern);
		Feep();
		break;
	    }
	    else {
		left = LSCAN(right + 1, 1, False);
		right = RSCAN(left, 1, True);
	    }
	}

	if (text != buffer)
	    XtFree(text);
    }

    /* Switch back to editing buffer */
    XtCallActionProc(otherwindow, "other-window", NULL, NULL, 0);

    if (found) {
	if (source != XawTextGetSource(tags->textwindow) ||
	    right < tags->position || left > tags->position) {
	    XawTextSetInsertionPoint(otherwindow, left);
	    XawTextSetSelection(otherwindow, left, right);
	}
    }
}
