/*
 * Copyright (c) 2002-2010 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header
        Converts HeaderDoc-generated XML output into a
	form suitable for use with <code>xml2man</code>.
    @indexgroup HeaderDoc Tools
 */

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef USE_STRCOMPAT
#include "strcompat.h"
#endif

typedef struct usage {
    char *flag;
    char *longflag;
    char *text;
    char *arg;
    xmlNode *descnode;
    char *functype;
    char *funcname;
    struct usage *funcargs;
    struct usage *optionlist;
    int optional;
    struct usage *next;
    int nextwithsamename;
    int emitted;
} *usage_t;

#define MAXCOMMANDS 100

usage_t usage_head[MAXCOMMANDS], usage_tail[MAXCOMMANDS];
char commandnames[MAXCOMMANDS][MAXNAMLEN];
int seen_name = 0;
int seen_usage = 0;
int multi_command_syntax = 0;
int funccount = 0;

char *striplines(char *line);
int safe_asprintf(char **ret, const char *format, ...) __attribute__((format (printf, 2, 3)));

#define MAX(a, b) ((a<b) ? b : a)
#define MIN(a, b) ((a>b) ? b : a)

void xml2man(xmlNode *root, char *output_filename, int append_section_number);
void parseUsage(xmlNode *node, int pos);
char *xs(int count);
int force_write = 0;
void writeOptionsSub(FILE *fp, xmlNode *description, int lwc);

char *formattext(char *text, int textcontainer);

/*!
    @abstract
        Strips the trailing <code>.mxml</code> from a filename.
 */
void strip_dotmxml(char *filename)
{
    char *last = &filename[strlen(filename)-5];
    if (!strcmp(last, ".mxml")) *last = '\0';
}

/*!
    @abstract
        Main.
    @apiuid //apple_ref/c/func/xml2man_main
 */
int main(int argc, char *argv[])
{
    xmlDocPtr dp;
    xmlNode *root;
    char output_filename[MAXNAMLEN];
    int append_section_number;
    int realargs = argc;
    int argoffset = 0;

    bzero(usage_head, (sizeof(usage_t) * MAXCOMMANDS));
    bzero(usage_tail, (sizeof(usage_t) * MAXCOMMANDS));

    if (argc <= 1) {
	fprintf(stderr, "xml2man: No arguments given.\n");
	exit(-1);
    }

#ifdef LIBXML_TEST_VERSION
    LIBXML_TEST_VERSION;
#endif

    if (!strcmp(argv[1], "-f")) {
	force_write = 1;
	realargs--;
	argoffset++;
    }

    if (realargs >= 2) {
	if (!(dp = xmlParseFile(argv[1+argoffset]))) {
	    perror(argv[0+argoffset]);
	    fprintf(stderr, "xml2man: could not parse XML file\n");
	    exit(-1);
	}
    } else {
	char *buf = malloc(1024 * sizeof(char));
	int bufpos = 0;
	int bufsize = 1024;
	while (1) {
	    char line[1026]; int len;

	    if (fgets(line, 1024, stdin) == NULL) break;
	    len = strlen(line);
	    while ((bufpos + len + 2) >= bufsize) {
		bufsize *= 2;
		buf = realloc(buf, bufsize);
	    }
	    strlcat(&buf[bufpos], line, bufsize);
	    bufpos += len;
	}
	dp = xmlParseMemory(buf, bufpos+1);
    }
    root = xmlDocGetRootElement(dp);

    /* Walk the tree and convert to mdoc */
    if (realargs >= 3) {
	int len = MAX(strlen(argv[2+argoffset]), MAXNAMLEN-1);
	strncpy(output_filename, argv[2+argoffset], len);
	output_filename[len] = '\0';
	append_section_number = 0;
    } else if (realargs >= 2) {
	int len = MAX(strlen(argv[1+argoffset]), MAXNAMLEN-4);
	strncpy(output_filename, argv[1+argoffset], len);
	output_filename[len] = '\0';
	/* We'll append ".1" or whatever later. */
	strip_dotmxml(output_filename);
	append_section_number = 1;
    } else {
	/* We'll dump to stdout at the right time */
	output_filename[0] = 0;
	append_section_number = 0;
    }

    xml2man(root, output_filename, append_section_number);

    /* Clean up just to be polite. */
    xmlFreeDoc(dp);
    xmlCleanupParser();
    return 0;
}

/*!
    @abstract
        A <code>strcat</code> variant that returns an allocated string.
 */
char *malloccat(const char *string1, const char *string2)
{
    // char *ret = malloc((strlen(string1) + strlen(string2) + 1) * sizeof(char));
    // strcpy(ret, string1);
    // strcat(ret, string2);
    char *ret = NULL;
    safe_asprintf(&ret, "%s%s", string1, string2);
    if (!ret) { fprintf(stderr, "Out of memory.\n"); exit(1); }
    return ret;
}


char *textmatching(char *name, xmlNode *cur, int missing_ok, char *debugstring);
xmlNode *nodematching(char *name, xmlNode *cur);
xmlNode **nodesmatching(char *name, xmlNode *cur);
void writeData(FILE *fp, xmlNode *node);
void writeUsage(FILE *fp, xmlNode *description);
int writeUsageSub(FILE *fp, int showname, usage_t myusagehead, char *name_or_empty, char *optional_separator);
void printUsageOptionList(usage_t cur, FILE *fp, char *starting, char *separator);

/*!
    @abstract
        The main body of the translator code.
    @discussion
        Takes an XML tree, an output filename, and a flag to
        indicate whether the section number should be appended
        to that filename, then translatest that into the -mdoc
        macro set.
 */
void xml2man(xmlNode *root, char *output_filename, int append_section_number)
{
    int section;
    xmlNode **othertopics;
    xmlNode *names, *usage, *retvals, *env, *files, *examples, *diags, *errs;
    xmlNode *seeAlso, *conformingTo, *history, *description, *bugs;
    char *docdate = "January 1, 9999";
    char *doctitle = "UNKNOWN MANPAGE";
    char *os = "";
    char *osversion = "";
    char *temp;
    FILE *fp;

    temp = textmatching("section", root->children, 0, NULL);

    if (temp) section = atoi(temp);
    else { fprintf(stderr, "Assuming section 1.\n"); section = 1; }

    temp = textmatching("docdate", root->children, 0, NULL);
    if (temp) docdate = temp;

    temp = textmatching("doctitle", root->children, 0, NULL);
    if (temp) doctitle = temp;

    temp = textmatching("os", root->children, 1, NULL);
    if (temp) os = temp;
    temp = textmatching("osversion", root->children, 1, NULL);
    if (temp) osversion = temp;

    names = nodematching("names", root->children);
    usage = nodematching("usage", root->children);
    if (usage) seen_usage = 1;
    retvals = nodematching("returnvalues", root->children);
    env = nodematching("environment", root->children);
    files = nodematching("files", root->children);
    examples = nodematching("examples", root->children);
    diags = nodematching("diagnostics", root->children);
    errs = nodematching("errors", root->children);
    seeAlso = nodematching("seealso", root->children);
    conformingTo = nodematching("conformingto", root->children);
    history = nodematching("history", root->children);
    description = nodematching("description", root->children);
    bugs = nodematching("bugs", root->children);
    othertopics = nodesmatching("topic", root->children);

    if (usage) { parseUsage(usage->children, 0); }

    // printf("section %d\n", section);
    // printf("nodes: names = 0x%x, usage = 0x%x, retvals = 0x%x, env = 0x%x,\nfiles = 0x%x, examples = 0x%x, diags = 0x%x, errs = 0x%x,\nseeAlso = 0x%x, conformingTo = 0x%x, history = 0x%x, bugs = 0x%x\n", names, usage, retvals, env, files, examples, diags, errs, seeAlso, conformingTo, history, bugs);

    /* Write everything to stdout for now */
    if (!strlen(output_filename)) {
	fp = stdout;
    } else {
	if (append_section_number) {
	    snprintf(output_filename, MAXNAMLEN, "%s.%d", output_filename, section);
	}
	if (!force_write && ((fp = fopen(output_filename, "r")))) {
	    fprintf(stderr, "error: file %s exists.\n", output_filename);
	    fclose(fp);
	    exit(-1);
	} else {
	    if (!(fp = fopen(output_filename, "w"))) {
		fprintf(stderr, "error: could not create file %s\n", output_filename);
		exit(-1);
	    }
	}
    }

    /* write preamble */
    fprintf(fp, ".\\\" Automatically generated from mdocxml\n");
    fprintf(fp, ".Dd %s\n", formattext(docdate, 1));
    fprintf(fp, ".Dt \"%s\" %d\n", formattext(doctitle, 1), section);
    fprintf(fp, ".Os \"%s\" ", formattext(os, 1));
    fprintf(fp, "\"%s\"\n", formattext(osversion, 1));

    /* write rest of contents */
    writeData(fp, names);
    writeUsage(fp, description);
    writeData(fp, retvals);
    writeData(fp, env);
    writeData(fp, files);
    writeData(fp, examples);
    writeData(fp, diags);
    writeData(fp, errs);

    {
	xmlNode **pos = othertopics;
	while (pos && *pos) {
		writeData(fp, *pos);
		pos++;
	}
    }
    free(othertopics);

    writeData(fp, seeAlso);
    writeData(fp, conformingTo);
    writeData(fp, history);
    writeData(fp, bugs);

    if (strlen(output_filename)) {
	fclose(fp);
    }
}

/*!
    @abstract
        Returns the first node at the current level that matches
        the specified tag name.
    @apiuid //apple_ref/c/func/xml2man_nodematching
 */
xmlNode *nodematching(char *name, xmlNode *cur)
{
    while (cur) {
	if (!cur->name) break;
	if (!strcmp((char *)cur->name, name)) break;
	cur = cur->next;
    }

    return cur;
}

/*!
    @abstract
        Returns an array of nodes at the current level that match
        the specified tag name.
 */
xmlNode **nodesmatching(char *name, xmlNode *cur)
{
    xmlNode **buf = malloc(sizeof(xmlNode) * 10);
    int size = 10, count=0;
    while (cur) {
	if (!cur->name) continue;
	if (!strcmp((char *)cur->name, name)) {
		if (count == (size - 1)) {
			xmlNode **buf2;
			buf2 = realloc(buf, sizeof(xmlNode) * size * 2);
			if (!buf2) {
				/* Houston, we have a problem. */
				buf[count] = NULL;
				return buf;
			}
			size *= 2;
		}
		buf[count++] = cur;
	}
	cur = cur->next;
    }

    buf[count] = NULL;
    return buf;
}

/*!
    @abstract
        Returns the first text content of the first node at
        the current level that matches the specified tag name.
    @apiuid //apple_ref/c/func/xml2man_textmatching
 */
char *textmatching(char *name, xmlNode *node, int missing_ok, char *debugstr)
{
    xmlNode *cur = nodematching(name, node);
    char *ret = NULL;

    if (!cur) {
	if (!missing_ok) {
		fprintf(stderr, "Invalid or missing contents for %s.\n", debugstr ? debugstr : name);
	}
    } else if (cur && cur->children && cur->children->content) {
		ret = (char *)cur->children->content;
    } else if (!strcmp(name, "text")) {
		ret = (char *)cur->content;
    } else {
	if (!missing_ok) {
		fprintf(stderr, "Missing/invalid contents for %s.\n", debugstr ? debugstr : name);
	} else {
		return NULL;
	}
    }

    return ret;
}

/*!
    @abstract
        State values that represent where we are in the
        document.
    @constant kGeneral
        The normal state.
    @constant kNames
        Inside the NAME section.
    @constant kRetval
        Inside the RETURN VALUES section.
    @constant kMan
        Inside a .Xr cross reference.
 */
enum states
{
    kGeneral = 0,
    kNames   = 1,
    kRetval  = 2,
    kMan     = 3,
};

void writeData_sub(FILE *fp, xmlNode *node, int state, int textcontainer, int next, int seendt);

/*!
    @abstract
        Writes a normal section to the output file.
    @discussion
        Calls {@link writeData_sub} to do all the real work.
 */
void writeData(FILE *fp, xmlNode *node)
{
    writeData_sub(fp, node, 0, 0, 0, 0);
}


/*!
    @abstract
        Writes the guts of a term-and-definition, flag, or argument.
    @discussion
        In this context, we can't change lines to send a new command,
        so only certain things are allowed, and all commands are
        handled inline.
 */
void dodtguts(FILE *fp, xmlNode *parent, char *initial_add)
{
  xmlNode *node = parent->children;
  char *add = initial_add;
  int skip_one = 0;
  if (strlen(initial_add)) skip_one = 1;

// printf("ADD: %s\n", add);

  while (node) {
    if (!strcmp((char *)node->name, "tt")) {
	fprintf(fp, " Dl "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "code")) {
	dodtguts(fp, node, "");
    } else if (!strcmp((char *)node->name, "arg")) {
	fprintf(fp, " Ar "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "path")) {
	fprintf(fp, " Pa "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "var")) {
	fprintf(fp, " Va "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "function")) {
	fprintf(fp, " Fn "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "symbol")) {
	fprintf(fp, " Sy "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "command")) {
	fprintf(fp, " Nm "); dodtguts(fp, node, ""); add=" Li ";
    } else if (!strcmp((char *)node->name, "text")) {
	char *ptr;
	char *x = formattext(striplines((char *)node->content), 1);
	int found = 0;
	for (ptr = x; *ptr; ptr++) {
		if (*ptr != ' ' && *ptr != '\t') {
			found=1;
			break;
		}
	}
	if (found) {
		if (skip_one) skip_one=0;
		else {
			fprintf(fp, "%s", add);
			add = "";
		}
	}
	fprintf(fp, "%s", x);
    }
    node = node->next;
  }
  // fprintf(fp, "%s", add); add = "";
}

/*!
    @abstract
        Returns the last sibling of the current node, but
        only if it is a text node.
 */
xmlNode *lasttextnode(xmlNode *node)
{
	xmlNode *nextnode = NULL;
	if (!node) return NULL;
	nextnode = lasttextnode(node->next);
	if (nextnode) return nextnode;
	if (!strcmp((char *)node->name, "text")) return node;
	return NULL;
}


/*!
    @abstract
        Writes a normal section to the output file.
 */
void writeData_sub(FILE *fp, xmlNode *node, int state, int textcontainer, int next, int seendt)
{
    int oldtextcontainer = textcontainer;
    int oldstate = state;
    int drop_children = 0;
    int localdebug = 0;
    char *xreftail = NULL;

    char *tail = NULL;

    if (!node) return;

    if (localdebug && node->content) {
	printf("NODE CONTENT: %s\n", node->content);
    }

    /* The reason for this block is that it is not possible (outside of
       argument lists) to have non-underlined/bold commas or periods after
       a .Em or .Sy tag due to limitations in the mdoc markup language.
       Thus, we had a choice: put a space between the word and the comma
       that follows it or allow the punctuation to be underlined or bold.
       in the interest of less confusing content, we chose to format the
       punctuation.

       We do not do this for content immediately after a <url> tag because
       that does not add any man-page-specific formatting.  The format of
       a URL is simply presented as <URL> in the output.

       We do something special for cross references because otherwise
       you would get "see foo.(3)" instead of "see foo(3)."
     */
    if (node->next && !strcmp((char *)node->next->name, "text") && strcmp((char *)node->name, "url")) {
	char *tmp = node->next->content ? (char *)node->next->content : "";
	char *end;
	char *nexttext;
	xmlNode *searchnode;

	if (tmp) {
	    searchnode = lasttextnode(node->children);
	    if (searchnode) {
		char *sntext;
		size_t alloc_len;

		/* Skip leading newlines, then capture any leading
		   commas or periods. */
		while (tmp[0] == '\n' || tmp[0] == '\r') tmp++;
		end = tmp;
		while (end[0] == '.' || end[0] == ',') end++;
		nexttext = malloc(end - tmp + sizeof(char));
		memcpy(nexttext, tmp, end-tmp);
		nexttext[(end-tmp)/sizeof(char)] = '\0';

		if (localdebug) printf("Appending \"%s\" to \"%s\"\n", nexttext, searchnode->content);
		if (!strcmp((char *)node->name, "manpage")) {
			safe_asprintf(&xreftail, "%s", nexttext);
			if (!xreftail) { fprintf(stderr, "Out of memory.\n"); exit(1); }
		} else {
			alloc_len = ((strlen((char *)searchnode->content) + strlen(nexttext) + 2) * sizeof(char));
			sntext = malloc(alloc_len);
			strlcpy(sntext, (char *)searchnode->content, alloc_len);
			// strcat(sntext, " ");
			strlcat(sntext, nexttext, alloc_len);
			if (localdebug) printf("new text \"%s\"\n", sntext);

			free(searchnode->content);
			searchnode->content = (unsigned char *)sntext;

		}

		tmp = strdup(end);
		free(node->next->content);
		node->next->content = (unsigned char *)tmp;
	    }
	}
    }

    if (!strcmp((char *)node->name, "docdate")) {
	/* silently ignore */
	if (localdebug) printf("docdate\n");
	writeData_sub(fp, node->next, state, 0, 1, seendt);
	return;
    } else if (!strcmp((char *)node->name, "doctitle")) {
	/* silently ignore */
	if (localdebug) printf("doctitle\n");
	writeData_sub(fp, node->next, state, 0, 1, seendt);
	return;
    } else if (!strcmp((char *)node->name, "section")) {
	if (localdebug) printf("section\n");
	if (state == kMan) {
		char *childtext = textmatching("text", node->children, 1, "man section element");
		char *pos = childtext;
    		char *tailcontents = NULL;

		while (pos && *pos && *pos != ',' && *pos != '.') pos++;
		if (pos && *pos) {
			tailcontents = strdup(pos);
			*pos = '\0';
		}

		if (localdebug) fprintf(stderr, "TAILCONTENTS: %s\n", tailcontents);

		fprintf(fp, " %s", formattext(childtext, textcontainer));
		if (tailcontents) fprintf(fp, " %s", tailcontents);
		tail = " ";

		/* Restore it just in case we need to read this node
		   later. */
		if (tailcontents) {
			*pos = *tailcontents;
			free(tailcontents);
		}
		
		drop_children = 1;
	} else {
		/* silently ignore */
		writeData_sub(fp, node->next, state, 0, 1, seendt);
		return;
	}
    } else if (!strcmp((char *)node->name, "desc")) {
	if (localdebug) printf("desc\n");
	if (state == kNames && node->children) {
		fprintf(fp, ".Nd ");
	}
	// if (!node->children) tail = "\n";
    } else if (!strcmp((char *)node->name, "names")) {
	if (localdebug) printf("names\n");
	state = kNames;
	fprintf(fp, ".Sh NAME\n");
    } else if (!strcmp((char *)node->name, "name")) {
	if (localdebug) printf("name\n");
	if (state == kNames) {
		if (seen_name) {
			fprintf(fp, ".Pp\n");
		}
		fprintf(fp, ".Nm ");
		textcontainer = 1;
		seen_name = 1;
	} else {
		char *tmp = textmatching("text", node->children, 0, "name tag");

		fprintf(fp, ".Nm%s%s\n", (tmp ? " " : ""), formattext(tmp ? tmp : "", textcontainer));
		if (tmp) { textcontainer = 0; }
	}
    } else if (!strcmp((char *)node->name, "usage")) {
	if (localdebug) printf("usage\n");
	textcontainer = 0;
	drop_children = 1;
    } else if (!strcmp((char *)node->name, "flag")) {
	if (textcontainer) {
		fprintf(fp, ".Fl "); dodtguts(fp, node, "\n"); drop_children = 1;
		tail = "\n";
	} else {
		if (localdebug) printf("flag\n");
		fprintf(fp, ".Pa "); dodtguts(fp, node, "\n"); drop_children = 1;
	}
    } else if (!strcmp((char *)node->name, "arg")) {
	if (localdebug) printf("arg\n");
	fprintf(fp, ".Pa "); dodtguts(fp, node, "\n"); drop_children = 1;
    } else if (!strcmp((char *)node->name, "returnvalues")) {
	if (localdebug) printf("returnvalues\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh RETURN VALUES\n");
    } else if (!strcmp((char *)node->name, "environment")) {
	if (localdebug) printf("environment\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh ENVIRONMENT\n");
    } else if (!strcmp((char *)node->name, "files")) {
	if (localdebug) printf("files\n");
	textcontainer = 0;
	fprintf(fp, ".Sh FILES\n");
	fprintf(fp, ".Bl -tag -width indent\n");
	tail = ".El\n";
    } else if (!strcmp((char *)node->name, "file")) {
	if (localdebug) printf("file\n");
	textcontainer = 1;
	fprintf(fp, ".It Pa ");
    } else if (!strcmp((char *)node->name, "examples")) {
	if (localdebug) printf("example\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh EXAMPLES\n");
    } else if (!strcmp((char *)node->name, "diagnostics")) {
	if (localdebug) printf("diagnostics\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh DIAGNOSTICS\n");
    } else if (!strcmp((char *)node->name, "errors")) {
	if (localdebug) printf("errors\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh ERRORS\n");
    } else if (!strcmp((char *)node->name, "seealso")) {
	if (localdebug) printf("seealso\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh SEE ALSO\n");
    } else if (!strcmp((char *)node->name, "conformingto")) {
	if (localdebug) printf("conformingto\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh CONFORMING TO\n");
    } else if (!strcmp((char *)node->name, "description")) {
	if (localdebug) printf("description\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh DESCRIPTION\n");
    } else if (!strcmp((char *)node->name, "history")) {
	if (localdebug) printf("history\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh HISTORY\n");
    } else if (!strcmp((char *)node->name, "bugs")) {
	if (localdebug) printf("bugs\n");
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh BUGS\n");
    } else if (!strcmp((char *)node->name, "topic")) {
	if (localdebug) printf("topic\n");
	xmlNode *topicnode = nodematching("name", node->children);
	char *topicname = "unknown";
	if (topicnode) {
		xmlUnlinkNode(topicnode);
		topicname = textmatching("text", topicnode->children, 1, "topic name");
	} else {
		fprintf(stderr, "Missing name tag for topic.  Usage is <topic><name>Topic Name</name>...\n");
	}
	state = kRetval;
	textcontainer = 1;
	fprintf(fp, ".Sh %s\n", formattext(topicname, textcontainer));
	if (topicnode) xmlFreeNode(topicnode);
    } else if (!strcmp((char *)node->name, "indent")) {
	if (localdebug) printf("indent\n");
	if (textcontainer) {
		fprintf(fp, "    ");
	}
    } else if (!strcmp((char *)node->name, "p")) {
	if (localdebug) printf("p (paragraph)\n");
	tail = ".Pp\n";
    } else if (!strcmp((char *)node->name, "blockquote")) {
	if (localdebug) printf("blockquote\n");
	fprintf(fp, ".Bd -ragged -offset indent\n");
	tail = ".Ed\n.Bd -ragged -offset indent\n.Ed\n";
    } else if (!strcmp((char *)node->name, "dl")) {
	if (localdebug) printf("dl\n");
	int minwidth = 6;
	xmlNode *ddnode = node->children;

	if (localdebug) printf("DL\n");
	textcontainer = 0;

	for ( ; ddnode ; ddnode = ddnode->next) {
		char *childtext;
		if (strcmp((char *)ddnode->name, "dd")) continue;

		childtext = textmatching("text", ddnode->children, 1, "dd element");
		if (!childtext) {
			if (ddnode->children) {
				childtext = textmatching("text", ddnode->children->children, 1, "dd element");
			}
		}
		if (childtext) {
			minwidth = MIN(minwidth, strlen(childtext));
		}
	}

	fprintf(fp, ".Bl -tag -width %s\n", xs(minwidth-1));
	tail = ".El\n.Pp\n";
    } else if (!strcmp((char *)node->name, "dt")) {
	if (localdebug) printf("dt\n");

	textcontainer = 0;
	drop_children = 1;
	seendt = 1;

	fprintf(fp, ".It ");
	dodtguts(fp, node, ""); //textmatching("text", node->children, 0, "dt element");
	// printf("DTGUTS: \"%s\"\n", guts ? guts : "");
	
	// while (guts[strlen(guts-1)] == '\n' || guts[strlen(guts-1)] == '\r') {
		// guts[strlen(guts-1)] = '\0';
	// }
	tail = "\n";
    } else if (!strcmp((char *)node->name, "dd")) {
	if (localdebug) printf("dd (definition)\n");
	if (localdebug) printf("DD\n");
	if (!seendt) {
		if (localdebug) printf("  NO DT\n");
		fprintf(fp, ".It \"\"\n");
	}
	textcontainer = 1;
	// tail = "\n";
	tail = "";
    } else if (!strcmp((char *)node->name, "tt")) {
	if (localdebug) printf("tt (typewriter)\n");
	fprintf(fp, ".Dl ");
    } else if (!strcmp((char *)node->name, "ul")) {
	if (localdebug) printf("ul (list)\n");
	fprintf(fp, ".Bl -bullet\n");
	tail = ".El\n.Pp\n";
    } else if (!strcmp((char *)node->name, "ol")) {
	if (localdebug) printf("ol (list)\n");
	fprintf(fp, ".Bl -enum\n");
	tail = ".El\n.Pp\n";
    } else if (!strcmp((char *)node->name, "li")) {
	if (localdebug) printf("li (list)\n");
	fprintf(fp, ".It\n");
    } else if (!strcmp((char *)node->name, "literal")) {
	if (localdebug) printf("literal\n");
	fprintf(fp, ".Li ");
    } else if (!strcmp((char *)node->name, "code")) {
	if (localdebug) printf("code\n");
	fprintf(fp, ".Li ");
    } else if (!strcmp((char *)node->name, "path")) {
	if (localdebug) printf("path\n");
	fprintf(fp, ".Pa ");
    } else if (!strcmp((char *)node->name, "u") || !strcmp((char *)node->name, "u")) {
	if (localdebug) printf("u (underline)/em (emphasis)\n");
	fprintf(fp, ".Em ");
	// tail = ".Li ";
    } else if (!strcmp((char *)node->name, "var")) {
	if (localdebug) printf("var\n");
	fprintf(fp, ".Va ");
    } else if (!strcmp((char *)node->name, "function")) {
	if (localdebug) printf("function\n");
	fprintf(fp, ".Fn ");
    } else if (!strcmp((char *)node->name, "symbol")) {
	if (localdebug) printf("symbol\n");
	fprintf(fp, ".Sy ");
    } else if (!strcmp((char *)node->name, "url")) {
	char *childtext = formattext(node->children ? (node->children->content ? (char *)node->children->content : "") : "", textcontainer);
	char *nexttext = node->next ? (char *)node->next->content : "";
	fprintf(fp, "<%s>", childtext);
	while (nexttext && *nexttext && (*nexttext == '.' || *nexttext == ',')) {
		fprintf(fp, "%c", *nexttext);
		nexttext++;
	}
	drop_children = 1;
    } else if (!strcmp((char *)node->name, "b")) {
	if (localdebug) printf("b (bold)\n");
	fprintf(fp, ".Sy ");
    } else if (!strcmp((char *)node->name, "subcommand")) {
	if (localdebug) printf("subcommand\n");
	/* @@@ Is this the best choice? @@@ */
	fprintf(fp, ".Nm ");
    } else if (!strcmp((char *)node->name, "command")) {
	if (localdebug) printf("command\n");
	/* @@@ Is this the best choice? @@@ */
	fprintf(fp, ".Nm ");
    } else if (!strcasecmp((char *)node->name, "br")) {
	if (localdebug) printf("br (break)\n");
	// fprintf(fp, ".Bd -ragged -offset indent\n.Ed\n");
	fprintf(fp, ".br\n");
    } else if (!strcmp((char *)node->name, "manpage")) {
	if (localdebug) printf("manpage\n");
	/* Cross-reference */
	fprintf(fp, ".Xr ");
	state = kMan;
	tail = "\n";
	textcontainer = 2;
    } else if (!strcmp((char *)node->name, "text")) {
	if (localdebug) printf("text node\n");
	if (textcontainer) {
		if (localdebug) printf("WILL PRINT %s\n", node->content);
		char *stripped_text = striplines((char *)node->content);
		if (strlen(stripped_text)) {
			/* These have already been glued onto the previous
			   node. */
			while (stripped_text[0] == '.' || stripped_text[0] == ',') stripped_text++;
			fprintf(fp, "%s%s", formattext(stripped_text, textcontainer), (state == kMan ? "" : "\n"));
		}
	} else {
		if (localdebug) printf("NOT PRINTING %s\n", node->content);
	}
    } else {
	fprintf(stderr, "unknown field %s\n", node->name);
    }

    if (!drop_children) {
    	writeData_sub(fp, node->children, state, textcontainer, 1, seendt);
    }
    textcontainer = oldtextcontainer;
    state = oldstate;
    if (xreftail) {
	fprintf(fp, " %s", xreftail);
    }
    if (tail) {
	fprintf(fp, "%s", tail);
    }
    if (next) {
	writeData_sub(fp, node->next, state, textcontainer, 1, seendt);
    }
}


/*!
    @abstract
        Writes function arguments to the output file.
 */
void write_funcargs(FILE *fp, usage_t cur)
{
    for (; cur; cur = cur->next) {
	fprintf(fp, ".It Ar \"%s\"", formattext(cur->arg ? cur->arg : "", 1));
	fprintf(fp, "\n");
	// @@@
	if (cur->descnode && cur->descnode->children) {
		writeData_sub(fp, cur->descnode->children, 0, 1, 1, 0);
	} // %s%s", formattext(cur->desc ? cur->desc : "", 1), (cur->desc ? "\n" : ""));
    }
}


/*!
    @abstract
        Returns a static buffer (not thread safe) containing
        the specified number of "X" characters.
 */
char *xs(int count)
{
    static char *buffer = NULL;
    if (buffer) free(buffer);
    buffer = malloc((count+1) * sizeof(char));
    if (buffer) {
	int i;
	for (i=0; i<count; i++) buffer[i] = 'X';
	buffer[count] = '\0';
    }

    return buffer;
}

/*!
    @abstract
        Writes the USAGE section of a manual page.
 */
void writeUsage(FILE *fp, xmlNode *description)
{
    int lwc, pos;
    char *name_or_empty = NULL;
    usage_t cur;

    /* Write SYNOPSIS section */

    lwc = 6;
    if (seen_usage) {
	fprintf(fp, ".Sh SYNOPSIS\n");

	// fprintf(stderr, "MCS: %d\n", multi_command_syntax);
	for (pos = 0; pos < (multi_command_syntax ? multi_command_syntax : 1); pos++) {

		if (multi_command_syntax) {
			name_or_empty = commandnames[pos];
		}
		// fprintf(stderr, "WRITING POS=%d (%s)\n", pos, name_or_empty);

		for (cur = usage_head[pos]; cur; cur = cur->next) {
			int len;
			len = 0;
			if (cur->flag) len += strlen(cur->flag) + 2;
			if (cur->longflag) len += strlen(cur->longflag) + 3;
			if (cur->flag && cur->longflag) len += 4;
			if (cur->arg) len += strlen(cur->arg) + 1;
			if (len > lwc) lwc = len;
		}
		if (lwc < 4) lwc = 4;
	
		writeUsageSub(fp, 1, usage_head[pos], name_or_empty, "");
	}
    }

    /* Write DESCRIPTION section */
    writeData(fp, description);

    /* Write OPTIONS section */
    writeOptionsSub(fp, description, lwc);
}

/*!
    @abstract
        Writes an individual command usage inside the USAGE
        section of a manual page.
 */
int writeUsageSub(FILE *fp, int showname, usage_t myusagehead, char *name_or_empty, char *optional_separator)
{
    usage_t cur;
    int first;
    char dot = '.';
	int nonl = 0;
	int lastnonl = 0;
	int prevwasliteral = 0;

	first = 1;
	for (cur = myusagehead; cur; cur = cur->next) {
		int isliteral = 0;
		char *optiontag=".";
		// int oldfirst = first;
		if (!showname || lastnonl) {
			dot = ' ';
			optiontag = " ";
		} else {
			dot = '.';
		}

		if (cur->flag || cur->longflag) {
			uint64_t flaglen = cur->flag ? strlen(cur->flag) : 0;
			uint64_t longflaglen = cur->longflag ? strlen(cur->longflag) : 0;
			if (first && showname) fprintf(fp, ".Nm%s%s\n", (name_or_empty ? " " : ""), formattext(name_or_empty ? name_or_empty : "", 1));
			else if (!first) fprintf(fp, "%s", optional_separator);
			if (flaglen) {
				fprintf(fp, "%c%s%sFl %s", dot, (cur->optional?"Op ":""), longflaglen ? "{ " : "", formattext(cur->flag, 1));
				dot = ' ';
			}
			if (cur->longflag && strlen(cur->longflag)) {
				fprintf(fp, "%c%s%sFl -%s%s", dot, flaglen ? " | " : "", flaglen ? "" : (cur->optional?"Op ":""), formattext(cur->longflag, 1), flaglen ? " Li }" : "");
				dot = ' ';
			}
			first = 0;
			optiontag="";
		}
		if (cur->arg) {
			if (first && showname) {
				fprintf(fp, "%cNm%s%s\n", dot, (name_or_empty ? " " : ""), formattext(name_or_empty ? name_or_empty : "", 1));
				dot = ' ';
			} else if (!first) fprintf(fp, "%s", optional_separator);
			fprintf(fp, "%c%sAr %s", dot, (cur->optional?"Op ":""), formattext(cur->arg, 1));

			first = 0;
			optiontag="";
		}
		if (cur->text) {
			int notsymbol = 0;

			if (cur->text[0] >= 'a' && cur->text[0] <= 'z') notsymbol = 1;
			if (cur->text[0] >= 'A' && cur->text[0] <= 'Z') notsymbol = 1;
			if (strlen(optional_separator)) notsymbol = 0;

			if (!first && !showname) fprintf(fp, " Li ");
			else if (prevwasliteral) fprintf(fp, " Li ");
			else fprintf(fp, ".Li ");
			if (!first) fprintf(fp, "%s", optional_separator);
			fprintf(fp, "%s", formattext(cur->text, 1));
			isliteral = 1;
			if (notsymbol) {
				optiontag="\n";
			} else {
				nonl = 1;
			}
		}
		if (cur->optionlist) {
			// usage_t temppos;

			if (first) { fprintf(fp, ".Nm%s%s\n", (name_or_empty ? " " : ""), formattext(name_or_empty ? name_or_empty : "", 1)); first = 0; }
			else {
				if (!first) {
					if (!(cur->flag || cur->longflag || cur->arg || cur->text)) {
						fprintf(fp, "%s", optional_separator);
					} else fprintf(fp, " ");
				}
			}
			printUsageOptionList(cur, fp, optiontag, (cur->flag || cur->longflag || cur->arg) ? " " : " | ");
			isliteral = 0;
		}
		if (cur->functype) {
			usage_t arg;
			fprintf(fp, ".Ft %s\n", formattext(cur->functype, 1));
			fprintf(fp, ".Fn \"%s\" ", formattext(cur->funcname, 1));
			for (arg = cur->funcargs; arg; arg = arg->next) {
				fprintf(fp, "\"%s\" ", formattext(arg->arg, 1));
			}
			isliteral = 0;
		} else if (cur->funcargs) {
			usage_t arg;
			for (arg = cur->funcargs; arg; arg = arg->next) {
				fprintf(fp, " %sAr %s", (arg->optional?"Op ":""), formattext(arg->arg, 1));
			}
			isliteral = 0;
		}
		if (showname && !nonl) {
			/* We're not nested in anything */
			fprintf(fp, "\n");
			isliteral = 0;
		}
		lastnonl = nonl;
		prevwasliteral = isliteral;
		nonl = 0;
    }
    if (lastnonl && showname) {
	fprintf(fp, "\n");
    }

    return 0;
}

int writeOptionsSubWithObject(FILE *fp, usage_t obj, int pos, int topfirst, int lwc, int noheading);

/*!
    @abstract
        Writes the OPTIONS section of a manual page.
 */
void writeOptionsSub(FILE *fp, xmlNode *description, int lwc)
{
    int topfirst;
    int pos;

    if (multi_command_syntax) {
	int outerpos, innerpos;

	for (outerpos = 0; outerpos < multi_command_syntax; outerpos++) {
		usage_head[outerpos]->emitted = 0;
		usage_head[outerpos]->nextwithsamename = 0;
		for (innerpos = outerpos + 1; innerpos < (multi_command_syntax ? multi_command_syntax : 1); innerpos++) {
			if (!strcmp(commandnames[outerpos], commandnames[innerpos])) {
				usage_head[outerpos]->nextwithsamename = innerpos;
				break;
			}
		}
	}
    }

    topfirst = 1;
    for (pos = 0; pos < (multi_command_syntax ? multi_command_syntax : 1); pos++) {
	if (!usage_head[pos]) continue;
	if (usage_head[pos]->emitted) continue;

	topfirst = writeOptionsSubWithObject(fp, usage_head[pos], pos, topfirst, lwc, usage_head[pos]->nextwithsamename ? 2 : 0);

	int temppos = usage_head[pos]->nextwithsamename;
	while (temppos) {
		topfirst = writeOptionsSubWithObject(fp, usage_head[temppos], temppos, topfirst, lwc, usage_head[temppos]->nextwithsamename ? 3 : 1);
		usage_head[temppos]->emitted = 1;
		temppos = usage_head[temppos]->nextwithsamename;
	}
    }
	
}


/*!
    @abstract
        Writes an individual option entry in the OPTIONS section of a manual page.
 */
int writeOptionsSubWithObject(FILE *fp, usage_t obj, int pos, int topfirst, int lwc, int noheading)
{
	int first;
	char *name_or_empty = NULL;
	usage_t cur;

	if (multi_command_syntax) {
		name_or_empty = commandnames[pos];
	}

	/* noheading:
		0 - normal output.
		1 - no heading, but closes the container.
		2 - normal output, but leaves the container open.
		3 - no heading, but leaves the container open.
	 */
	if (noheading == 1 || noheading == 3) first = 0;
	else first = 1;

	for (cur = usage_head[pos]; cur; cur = cur->next) {
		if (cur->funcargs && !cur->flag && !cur->longflag) {
			if (first) {
				if (topfirst) { fprintf(fp, ".Sh PARAMETERS\n"); }
				first = 0; topfirst = 0;
			} else {
				fprintf(fp, ".El\n.Pp\n");
			}
			fprintf(fp, "The parameters %s%s%sare as follows:\n",  (name_or_empty ? "for\n.Sy " : ""), formattext(name_or_empty ? name_or_empty : "", 1), (name_or_empty ? " Li " : ""));
			fprintf(fp, ".Bl -tag -width %s\n", xs(lwc));

			write_funcargs(fp, cur->funcargs);
			continue;
		}
		// if (!cur->flag) continue;
		if (!cur->descnode) continue;
		if (first) {
			if (topfirst) { fprintf(fp, ".Sh OPTIONS\n"); }
			fprintf(fp, "The available options %s%s%sare as follows:\n",  (name_or_empty ? "for\n.Sy " : ""), formattext(name_or_empty ? name_or_empty : "", 1), (name_or_empty ? " Li " : ""));
			fprintf(fp, ".Bl -tag -width %s\n", xs(lwc));
			first = 0; topfirst = 0;
		}
		fprintf(fp, ".It");
		if (cur->flag) { fprintf(fp, " Fl %s", formattext(cur->flag, 1)); }
		if (cur->longflag) { fprintf(fp, "%s Fl -%s", cur->flag ? " Li or" : "", formattext(cur->longflag, 1)); }
		if (cur->arg) {
			fprintf(fp, " Ar \"%s\"", formattext(cur->arg, 1));
		}
		// fprintf(fp, "\n%s\n", formattext(cur->desc ? cur->desc : "", 1));
		fprintf(fp, "\n");
		// @@@
		if (cur->descnode && cur->descnode->children) {
			writeData_sub(fp, cur->descnode->children, 0, 1, 1, 0);
		} // %s%s", formattext(cur->desc ? cur->desc : "", 1), (cur->desc ? "\n" : ""));
	}
	if (noheading < 2) {
		if (!first) { fprintf(fp, ".El\n"); }
	}

	return topfirst;
}

/*!
    @abstract
        Prints a list of options (A | B | C) for the USAGE
        section of a manual page.
 */
void printUsageOptionList(usage_t cur, FILE *fp, char *starting, char *separator)
{
	// usage_t temppos;
	int optional = cur->optional;
	// int first = 1;


	// fprintf(fp, "[\n");
	// if (cur->arg) {
		// fprintf(fp, "%s \n", cur->arg);
	// }

	// printf("printUsageOptionList (OPTIONTAG: '%s')\n", starting);

	fprintf(fp, "%s", formattext(starting, 1));
	if (starting[strlen(starting)-1] == '\n') fprintf(fp, ".");
	if (optional) fprintf(fp, "Op ");
	// for (temppos = cur->optionlist; temppos; temppos = temppos->next) {
		// if (temppos->optionlist) {
			// // if (!first) fprintf(fp, "\n");
			// printf("pUOL RECURSE\n");
			// printUsageOptionList(temppos, fp);
			// printf("pUOL RECURSEOUT\n");
			// // if (!first) fprintf(fp, ".Op ");
		// } else {
			// printf("pUOL WRITEUSAGE\n");
			writeUsageSub(fp, 0, cur->optionlist, "", separator);
			// printf("pUOL WRITEUSAGEOUT\n");
		// }
		// // first = 0;
	// }
	// fprintf(fp, "]\n");
}

/*!
    @abstract
        Searches a list of XML properties and returns a string
        containing the text of the first property that matches
        the specified name.
 */
char *propstring(char *name, struct _xmlAttr *prop)
{
    for (; prop; prop=prop->next) {
	if (!strcmp((char *)prop->name, name)) {
		if (prop->children && prop->children->content) {
			return (char *)prop->children->content;
		}
	}
    }
    return NULL;
}

/*!
    @abstract
        Returns the numerical value (integer) of an XML property.
    @apiuid //apple_ref/c/func/xml2man_propval
 */
int propval(char *name, struct _xmlAttr *prop)
{
    char *ps = propstring(name, prop);

    if (!ps) {
	/* Assume 0 if property not found */
	return 0;
    }

    return atoi(ps);
}

/*!
    @abstract
        Reads the argument list for a flag (from the XML tree) and
        creates data structures accordingly.
 */
usage_t getflagargs(xmlNode *node)
{
    usage_t head = NULL, tail = NULL;
    usage_t newnode;

    while (node) {
	if (strcmp((char *)node->name, "arg")) { node = node->next; continue; }

	if (!(newnode = malloc(sizeof(struct usage)))) return NULL;

	newnode->flag = NULL;
	newnode->longflag = NULL;
	newnode->arg  = textmatching("text", node->children, 0, "flag argument");
	newnode->descnode = NULL;
	newnode->optional = propval("optional", node->properties);
	newnode->functype = NULL;
	newnode->funcname = NULL;
	newnode->funcargs = NULL;
	newnode->next = NULL;

	if (!head) {
		head = newnode;
		tail = newnode;
	} else {
		tail->next = newnode;
		tail = newnode;
	}
	node = node->next;
    }
    return head;
}


void parseUsageSub(xmlNode *node, int pos, int drop_first_text);
/*!
    @abstract
        Calls {@link parseUsageSub}.
 */
void parseUsage(xmlNode *node, int pos) {
    parseUsageSub(node, pos, 1);
}

/*!
    @abstract
        Extracts the usage section from the XML tree and
        builds up data structures.
 */
void parseUsageSub(xmlNode *node, int pos, int drop_first_text)
{
    usage_t flag_or_arg = NULL;

    if (!node) return;

    if (!strcmp((char *)node->name, "text") || !strcmp((char *)node->name, "type") ||
	!strcmp((char *)node->name, "name")) {
		char *ptr;
		char *x = striplines((char *)node->content);
		int found = 0;
		for (ptr = x; *ptr; ptr++) {
			if (*ptr != ' ' && *ptr != '\t') {
				found=1;
				break;
			}
		}
		if (!found || drop_first_text) {
			parseUsageSub(node->next, pos, 0);
			return;
    		}
    }

    if (!strcmp((char *)node->name, "command")) {
	int i = 0;

	while (node) {
		char *name;

		// printf("MCS\n");
		if (strcmp((char *)node->name, "command")) {
			node = node->next;
			continue;
		}
		name = propstring("name", node->properties);
		if (name) {
			strlcpy(commandnames[i], name, MAXNAMLEN);
		} else {
			fprintf(stderr, "WARNING: command has no name\n");
		}
		// fprintf(stderr, "CMDNAMES[%d] = %s\n", i, commandnames[i]);

		parseUsageSub(node->children, i, 0);
		multi_command_syntax = i+1;

		node = node->next;
		if ((++i >= MAXCOMMANDS) && node) {
			fprintf(stderr, "MAXCOMMANDS reached.\n");
			break;
		}
	}
	return;
    }

    if (strcmp((char *)node->name, "desc")) {
	flag_or_arg = (usage_t)malloc(sizeof(struct usage));
	if (!flag_or_arg) return;
	if (!usage_head[pos]) {
		usage_head[pos] = flag_or_arg;
		usage_tail[pos] = flag_or_arg;
	} else {
		usage_tail[pos]->next = flag_or_arg;
		usage_tail[pos] = flag_or_arg;
	}
    }

    if (!strcmp((char *)node->name, "optionlist")) {
	char *tempstring;
	flag_or_arg->flag = NULL;
	flag_or_arg->longflag = NULL;
	flag_or_arg->optionlist = NULL;
	flag_or_arg->text  = NULL;
	flag_or_arg->arg = NULL;
	flag_or_arg->descnode = NULL;
	flag_or_arg->optional = 1;
	tempstring = propstring("optional", node->properties);
	if (tempstring) {
		flag_or_arg->optional = atoi(tempstring);
		// printf("OPTIONAL: %d\n", flag_or_arg->optional);
	}
	flag_or_arg->functype = NULL;
	flag_or_arg->funcname = NULL;
	flag_or_arg->funcargs = NULL;
	flag_or_arg->next = NULL;
	// printf("RECURSE\n");
	parseUsageSub(node->children, pos, 0);
	// printf("RECURSEOUT\n");
	flag_or_arg->optionlist = flag_or_arg->next;
	usage_tail[pos] = flag_or_arg;
	flag_or_arg->next = NULL;
    } else if (!strcmp((char *)node->name, "subcommand") || !strcmp((char *)node->name, "literal") || !strcmp((char *)node->name, "text")) {
	char *dbstr = malloccat((char *)node->name, " tag");
	int istext = 0;
	if (!strcmp((char *)node->name, "text")) {
		istext = 1;
	}
	flag_or_arg->flag = NULL;
	flag_or_arg->longflag = NULL;
	flag_or_arg->arg = NULL;
	flag_or_arg->optionlist = NULL;
	if (istext) {
		flag_or_arg->text  = strdup(striplines((char *)node->content));
	} else {
		flag_or_arg->text  = strdup(striplines(textmatching("text", node->children, 0, dbstr)));
	}
	free(dbstr);
	flag_or_arg->descnode = NULL;
	flag_or_arg->optional = propval("optional", node->properties);
	flag_or_arg->functype = NULL;
	flag_or_arg->funcname = NULL;
	flag_or_arg->funcargs = NULL;
	flag_or_arg->next = NULL;
	// printf("RECURSE\n");
	if (!istext) {
		parseUsage(node->children, pos);
		// printf("RECURSEOUT\n");
		flag_or_arg->optionlist = flag_or_arg->next;
		usage_tail[pos] = flag_or_arg;
	}
	flag_or_arg->next = NULL;
    } else if (!strcmp((char *)node->name, "arg")) {
	xmlNode *tempnode = nodematching("arg", node->children);

	flag_or_arg->flag = NULL;
	flag_or_arg->longflag = NULL;
	flag_or_arg->optionlist = NULL;
	flag_or_arg->text  = NULL;
	flag_or_arg->arg  = strdup(striplines(textmatching("text", node->children, 0, "arg tag")));
	flag_or_arg->descnode = nodematching("desc", node->children);
	flag_or_arg->optional = propval("optional", node->properties);
	flag_or_arg->functype = NULL;
	flag_or_arg->funcname = NULL;
	flag_or_arg->funcargs = NULL;
	flag_or_arg->next = NULL;
	if (tempnode) {
		// printf("ARGRECURSE\n");
		parseUsage(tempnode, pos);
		// printf("ARGRECURSEOUT\n");
		flag_or_arg->optionlist = flag_or_arg->next;
		usage_tail[pos] = flag_or_arg;
		flag_or_arg->next = NULL;
	}
    } else if (!strcmp((char *)node->name, "desc")) {
    } else if (!strcmp((char *)node->name, "flag")) {
	flag_or_arg->flag = textmatching("text", node->children, 1, "flag tag");
	flag_or_arg->longflag = textmatching("long", node->children, 1, "long flag tag");
	if (!flag_or_arg->flag && !flag_or_arg->longflag) {
		fprintf(stderr, "Invalid or missing contents for flag tag.\n");
	}
	flag_or_arg->optionlist = NULL;
	flag_or_arg->text  = NULL;
	flag_or_arg->arg = NULL;
	flag_or_arg->descnode = nodematching("desc", node->children);
	flag_or_arg->optional = propval("optional", node->properties);
	flag_or_arg->functype = NULL;
	flag_or_arg->funcname = NULL;
	flag_or_arg->funcargs  = getflagargs(node->children);
	flag_or_arg->next = NULL;
    } else if (!strcmp((char *)node->name, "func")) {
	/* "func" */
	flag_or_arg->flag = NULL;
	flag_or_arg->longflag = NULL;
	flag_or_arg->optionlist = NULL;
	flag_or_arg->text  = NULL;
	flag_or_arg->arg  = NULL;
	flag_or_arg->descnode = NULL;
	flag_or_arg->optional = 0;
	flag_or_arg->functype = strdup(striplines(textmatching("type", node->children, 0, "func type")));
	flag_or_arg->funcname = strdup(striplines(textmatching("name", node->children, 0, "func name")));
	flag_or_arg->next = NULL;
	// printf("RECURSE\n");
	parseUsage(node->children, pos);
	if (++funccount > 1) {
		strncpy(commandnames[multi_command_syntax], flag_or_arg->funcname, MAXNAMLEN-1);
		commandnames[multi_command_syntax][MAXNAMLEN-1] = '\0';
		multi_command_syntax++;
	}
	// printf("RECURSEOUT\n");
	flag_or_arg->funcargs = flag_or_arg->next;
	usage_tail[pos] = flag_or_arg;
	flag_or_arg->next = NULL;
    } else {
	fprintf(stderr, "UNKNOWN NODE NAME: %s\n", node->name);
	flag_or_arg->flag = NULL;
	flag_or_arg->longflag = NULL;
	flag_or_arg->optionlist = NULL;
	flag_or_arg->text  = NULL;
	flag_or_arg->arg = NULL;
	flag_or_arg->descnode = NULL;
	flag_or_arg->optional = 1;
	flag_or_arg->functype = NULL;
	flag_or_arg->funcname = NULL;
	flag_or_arg->funcargs = NULL;
	flag_or_arg->next = NULL;
	flag_or_arg->optionlist = NULL;
	flag_or_arg->next = NULL;
    }

    parseUsageSub(node->next, pos, 0);
}

/*!
    @abstract
        The current state in the {@link striplines} function.
    @constant kSOL
	Short for "start of line", this is the initial state
	and the state after a newline but before any non-whitespace.
    @constant kText
	This is the state after text has appeared on a line.
 */
enum stripstate
{
    kSOL = 1,
    kText = 2
};

/*!
    @abstract
        Strips leading whitespace and replaces all line breaks with spaces.
 */
char *striplines(char *line)
{
    static char *ptr = NULL;
    char *pos;
    char *linepos;
    int state = 0;

    if (!line) return "";
    linepos = line;

    if (ptr) free(ptr);
    ptr = malloc((strlen(line) + 1) * sizeof(char));

    state = kSOL;
    for (pos = ptr; (*linepos); linepos++,pos++) {
	switch(state) {
		case kSOL:
			if (*linepos == ' ' || *linepos == '\n' || *linepos == '\r' ||
			    *linepos == '\t') { pos--; continue; }
		case kText:
			if (*linepos == '\n' || *linepos == '\r') {
				state = kSOL;
				*pos = ' ';
			} else {
				state = kText;
				*pos = *linepos;
			}
	}
    }
    *pos = '\0';

    // printf("LINE \"%s\" changed to \"%s\"\n", line, ptr);

    return ptr;
}

/*!
    @abstract
        Returns 1 if a token might potentially be misinterpreted
        as a macro.  Used to control quoting.
 */
int checkcurword(char *word, int textcontainer)
{
    if (textcontainer == 2) return 0;
    if (*word == '.') return 1;
    if (strlen(word) == 1 && (!(word[0] >= '0' && word[0] <= '9'))) return 1;
    if (strlen(word) == 2 && (!(word[0] >= '0' && word[0] <= '9')) && (!(word[1] >= '1' && word[1] <= '9'))) return 1;

    return 0;
}

/*!
    @abstract
        Emits a block of text, quoting tokens within it as needed.
 */
char *formattext(char *text, int textcontainer)
{
    static char *result = NULL;
    char *pos;
    char *curword;
    char *temp;
    int iskey;
    char *space = "";

    if (result) free(result);

// fprintf(stderr, "TC: %d STRING: %s\n", textcontainer, text);

    // safe_asprintf(&result, "");
    result = calloc(1,1);
    // safe_asprintf(&curword, "");
    curword = calloc(1,1);

    for (pos = text; *pos; pos++) {
	iskey = checkcurword(curword, textcontainer);
	switch (*pos) {
		case ' ':
			temp = result;
			result = NULL;
			safe_asprintf(&result, "%s%s%s%s", temp, space, iskey ? "\\&" : "", curword);
			if (!result) { fprintf(stderr, "Out of memory.\n"); exit(1); }
			free(temp);
			free(curword);
			// safe_asprintf(&curword, "");
			curword = calloc(1,1);
			space = " ";
			break;
		case '\\':
			temp = result;
			result = NULL;
			safe_asprintf(&result, "%s%s%s%s\\e", temp, space, iskey ? "\\&" : "", curword);
			if (!result) { fprintf(stderr, "Out of memory.\n"); exit(1); }
			free(temp);
			free(curword);
			// safe_asprintf(&curword, "");
			curword = calloc(1,1);
			space = " ";
			break;
		default:
			temp = curword;
			curword = NULL;
			safe_asprintf(&curword, "%s%c", temp, *pos);
			if (!curword) { fprintf(stderr, "Out of memory.\n"); exit(1); }
			free(temp);
	}
    }
    iskey = checkcurword(curword, textcontainer);
    temp = result;
    result = NULL;
    safe_asprintf(&result, "%s%s%s%s", temp, space, iskey ? "\\&" : "", curword);
    if (!result) { fprintf(stderr, "Out of memory.\n"); exit(1); }
    free(temp);
    free(curword);

    return result;
}

/*!
    @abstract
	Compatibility shim for Linux
    @discussion
	Unlike the BSD implementation of <code>asprintf</code>,
	the Linux implementation does not guarantee that the
	variable pointed to by <code>ret</code> is set to
	<code>NULL</code> in the event of an error.

	Because it is poor programming practice to accept a
	pointer from a system routine without checking to see
	if the routine returned NULL, this results in a rather
	messy pair of checks in order to get the desired
	behavior (one check for the return value, then another
	for the pointer).

	This function works around that flaw in the Linux
	implementation by simply checking the return value,
	then setting the variable pointed to by <code>ret</code>
	to NULL in the event of an error.
 */
int safe_asprintf(char **ret, const char *format, ...)
{
    va_list ap;
    int retval;

    va_start(ap, format);
    retval = vasprintf(ret, format, ap);
    if (ret && (retval < 0)) {
	*ret = NULL;
    }

    return retval;
}

