/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef USE_STRCOMPAT
#include "strcompat.h"
#endif

typedef struct usage {
    char *flag;
    char *arg;
    char *desc;
    char *functype;
    char *funcname;
    struct usage *funcargs;
    int optional;
    struct usage *next;
} *usage_t;

usage_t usage_head = NULL, usage_tail = NULL;
int seen_name = 0;

char *man_section = NULL;

#define MAX(a, b) ((a<b) ? b : a)

/* @@@ REMINDER: Use xmlNodeListGetRawString instead of reading content */

int hdxml2man(xmlDocPtr dp, xmlNode *root, char *output_filename);
char *textmatching(xmlDocPtr dp, char *name, xmlNode *node, int missing_ok, int recurse);
xmlNode *nodematching(char *name, xmlNode *cur, int recurse);

struct nodelistitem
{
    xmlNode *node;
    struct nodelistitem *next;
};

struct nodelistitem *nodelist(char *name, xmlNode *root);
void processfile(char *filename, xmlDocPtr dp, int counter);

int main(int argc, char *argv[])
{
    xmlDocPtr dp;
    int counter = 0, i;
    int first_arg = 1;

    if (argc < 2) {
	fprintf(stderr, "hdxml2manxml: No arguments given.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\thdxml [-M section] filename.xml ...\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This tool outputs a series of .mxml files (suitable for use with\n");
	fprintf(stderr, "xml2man) in the current directory based on the contents of a\n");
	fprintf(stderr, "HeaderDoc XML output file (pass -X to headerdoc2html).  If no\n");
	fprintf(stderr, "section number is given, the pages generated are assumed to be\n");
	fprintf(stderr, "in man section 3.\n");
	fprintf(stderr, "\n");
	exit(-1);
    }

    if (!strcmp(argv[1], "-M")) {
	if (argc < 4) {
		fprintf(stderr, "hdxml2manxml: You must specify a man page section for -M.\n");
		exit(-1);
	}
	man_section = argv[2];
	first_arg += 2;
    }

#ifdef LIBXML_TEST_VERSION
    LIBXML_TEST_VERSION;
#endif

    xmlSubstituteEntitiesDefault(0);
// printf("%d %d\n", argc, first_arg);

    if (argc > (first_arg)) {
	for (i=first_arg; i < argc; i++) {
	    void *addr;
	    struct stat sb;
	    int fd = open(argv[i], O_RDONLY, 0);
	    size_t len;

	    if (fd == -1) continue;

	    if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "Could not stat file \"%s\".  Skipping.\n", argv[i]);
		continue;
	    }
	    len = sb.st_size;
	    addr = mmap(NULL, len, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	    // fprintf(stderr, "Mapped length 0x%lx at %p\n", len, addr);
	    if ((uintptr_t)addr == (uintptr_t)-1) {
		fprintf(stderr, "Could not memory map file.  Aborting.\n");
		perror("hdxml2manxml");
		exit(-1);
	    }
	    // fprintf(stderr, "DUMPING\n");
	    // { char *pos = addr; size_t count = len; while (count) { printf("%c", *pos++); count--; }}
	    // fprintf(stderr, "END DUMP\n");
	    if (!(dp = xmlParseMemory(addr, len))) {
		perror(argv[0]);
		fprintf(stderr, "hdxml2manxml: could not parse XML file\n");
		exit(-1);
	    } else {
		processfile(argv[i], dp, 0);
		xmlFreeDoc(dp);
		xmlCleanupParser();
	    }
	    munmap(addr, len);
	    close(fd);
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
		if (!(buf = realloc(buf, bufsize))) {
			perror(argv[0]);
			fprintf(stderr, "hdxml2manxml: Could not allocate memory.\n");
			exit(-1);
		}
	    }
	    bcopy(line, &buf[bufpos], strlen(line));
	    bufpos += len;
	}
	buf[bufpos] = '\0';
	dp = xmlParseMemory(buf, bufpos);
	processfile("stdin", dp, counter);
	xmlFreeDoc(dp);
	xmlCleanupParser();
    }

    return 0;
}

void processfile(char *filename, xmlDocPtr dp, int counter)
{
    xmlNode *root;
    struct nodelistitem *nodelistp;
    xmlNode *node;
    char *name;
    char output_filename[MAXNAMLEN];

    root = xmlDocGetRootElement(dp);

    /* Walk the tree and convert to mdoc */

    nodelistp = nodelist("function", root);

    // node = nodematching("function", root, 1);
    while (nodelistp) {
	node = nodelistp->node;
	if (!strcmp((char *)node->name, "function")) {
		name = textmatching(dp, "name", node->children, 0, 0);
		if (name) {
			strlcpy(output_filename, name, MAXNAMLEN);
			strlcat(output_filename, ".mxml", MAXNAMLEN);
		} else {
			snprintf(output_filename, MAXNAMLEN, "unknown-%06d.mxml", counter++);
		}
		hdxml2man(dp, node, output_filename);
	} else if (strcmp((char *)node->name, "text")) {
		fprintf(stderr, "Unexpected node %s in functions\n", node->name);
	}
	nodelistp = nodelistp->next;
    }

}


void write_arguments(xmlDocPtr dp, FILE *fp, xmlNode *root);
void write_examples(xmlDocPtr dp, FILE *fp, xmlNode *root);
void write_crossrefs(xmlDocPtr dp, FILE *fp, xmlNode *root);

int hdxml2man(xmlDocPtr dp,xmlNode *root, char *output_filename)
{
    // int section;
    char datebuf[22];
    char *docdate = NULL;
    char *doctitle = "UNKNOWN MANPAGE";
    char *temp;
    char *sectionstring = NULL;
    FILE *fp;

    temp = textmatching(dp, "section", root->children, 1, 0);

    if (temp) {
	sectionstring = temp;
    } else if (man_section && strlen(man_section)) {
	sectionstring = man_section;
    } else {
	fprintf(stderr, "Assuming section 3.\n"); sectionstring = "3";
    }

    temp = textmatching(dp, "updated", root->children, 1, 0);
    if (temp) docdate = temp;
    else {
	struct tm *tmval;
	time_t timet;
	time(&timet);
	tmval = localtime(&timet);
	strftime(datebuf, (20 * sizeof(char)), "%B %d, %Y", tmval);
	docdate = datebuf;
    }

    temp = textmatching(dp, "name", root->children, 0, 0);
    if (temp) doctitle = temp;

    // temp = textmatching(dp, "os", root->children, 1, 0);

    // printf("section %d\n", section);
    // printf("nodes: names = 0x%x, usage = 0x%x, retvals = 0x%x, env = 0x%x,\nfiles = 0x%x, examples = 0x%x, diags = 0x%x, errs = 0x%x,\nseeAlso = 0x%x, conformingTo = 0x%x, history = 0x%x, bugs = 0x%x\n", names, usage, retvals, env, files, examples, diags, errs, seeAlso, conformingTo, history, bugs);

    /* Write everything to stdout for now */
    if (!strlen(output_filename)) {
	fp = stdout;
    } else {
	if ((fp = fopen(output_filename, "r"))) {
	    fprintf(stderr, "error: file %s exists.\n", output_filename);
	    fclose(fp);
	    return(-1);
	} else {
	    if (!(fp = fopen(output_filename, "w"))) {
		fprintf(stderr, "error: could not create file %s\n", output_filename);
		return(-1);
	    }
	}
    }

    /* write preamble */
    fprintf(fp, "<manpage>\n\n");
    if (docdate) fprintf(fp, "<docdate>%s</docdate>\n", docdate);
    if (doctitle) fprintf(fp, "<doctitle>%s</doctitle>\n", doctitle);
    fprintf(fp, "<section>%s</section>\n", sectionstring);

    temp = textmatching(dp, "abstract", root->children, 0, 0);
    if (!temp) temp = "";

    /* write rest of contents */
    fprintf(fp, "<names><name>%s<desc>%s</desc></name></names>\n",
	doctitle, temp);

    write_arguments(dp, fp, root);
    write_examples(dp, fp, root);
    write_crossrefs(dp, fp, root);

    fprintf(fp, "</manpage>\n");

    if (strlen(output_filename)) {
	fclose(fp);
    }
    return 0;
}


struct param
{
    char *name;
    char *type;
    char *desc;
    struct param *next;
};


void write_arguments(xmlDocPtr dp, FILE *fp, xmlNode *root)
{
    char *name, *type;
    xmlNode *node;
    struct param *p_head = NULL, *p_tail = NULL, *p;

    // For each argument, coalesce the parsed and tagged parameters, then
    // output the XML representation thereof.

    type = textmatching(dp, "returntype", root->children, 0, 0);
    name = textmatching(dp, "name", root->children, 0, 0);

    fprintf(fp, "<usage><func><type>%s</type><name>%s</name>\n", type ? type : "", name ? name : "");

    node = nodematching("parsedparameter", root->children, 1);
    for ( ; node; node=node->next) {
	struct param *p;

	if (strcmp((char *)node->name, "parsedparameter")) continue;

	p = malloc(sizeof(struct param));
	if (!p) continue;
	p->name = textmatching(dp, "name", node->children, 1, 0);
	p->type = textmatching(dp, "type", node->children, 0, 0);
	p->desc = NULL;
	p->next = NULL;
	if (!p_head) {
		p_head = p;
		p_tail = p;
	} else {
		p_tail->next = p;
		p_tail = p;
	}
    }
    node = nodematching("parameter", root->children, 1);
    for ( ; node; node = node->next) {
	char *name, *desc;

	if (strcmp((char *)node->name, "parameter")) continue;
	name = textmatching(dp, "name", node->children, 0, 0);
	desc = textmatching(dp, "desc", node->children, 1, 0);
	if (!name) continue;
	for (p = p_head; p; p=p->next) {
		if (p->name && strlen(p->name)) {
			if (!strcmp(p->name, name)) break;
		} else if (p->type && strlen(p->type)){
			if (!strcmp(p->type, name)) break;
		}
	}
	if (p) {
		p->desc = desc;
	} else {
		p = malloc(sizeof(struct param));
		if (!p) continue;
		p->name = name;
		p->type = "";
		p->desc = desc;
		p->next = NULL;
		if (!p_head) {
			p_head = p;
			p_tail = p;
		} else {
			p_tail->next = p;
			p_tail = p;
		}
	}
    }

    for (p=p_head; p; p=p->next) {
	int space = 0;
	if (p->type && p->type[strlen(p->type)-1] == ' ') space = 1;
	if (p->type && p->type[strlen(p->type)-1] == '*') space = 1;
	fprintf(fp, "<arg>%s%s%s<desc>%s</desc></arg>\n",
		p->type ? p->type : "", space ? "" : " ",
		p->name ? p->name : "", p->desc ? p->desc : "");
    }

    fprintf(fp, "</func></usage>\n");
}


char *translation(const char *tag)
{
    if (!strcmp(tag, "b")) return "function";
    if (!strcmp(tag, "i")) return "path";
    if (!strcmp(tag, "p")) return "p";
    if (!strcmp(tag, "tt")) return "tt";
    if (!strcmp(tag, "blockquote")) return "blockquotep";
    if (!strcmp(tag, "ul")) return "ul";
    if (!strcmp(tag, "ol")) return "ol";
    if (!strcmp(tag, "li")) return "li";
    if (!strcmp(tag, "code")) return "code";
    return NULL;
}


void write_example_sub(xmlDocPtr dp, FILE *fp, xmlNode *root);
void write_examples(xmlDocPtr dp, FILE *fp, xmlNode *root)
{
    xmlNode *node;

    node = nodematching("description", root->children, 0);
    if (node) {
	fprintf(fp, "<examples>\n");
	write_example_sub(dp, fp, node->children);
	fprintf(fp, "</examples>\n");
    }
}

void write_example_sub(xmlDocPtr dp, FILE *fp, xmlNode *root)
{
    char *tag;

    if (!root) return;

    tag = translation((char *)root->name);

    if (tag) { fprintf(fp, "<%s>", tag); }
    if (root->content) { fprintf(fp, "%s", xmlNodeListGetRawString(dp, root, 0)); } // root->content
    write_example_sub(dp, fp, root->children);
    if (tag) { fprintf(fp, "</%s>", tag); }
    write_example_sub(dp, fp, root->next);
}

xmlNode *nodematching(char *name, xmlNode *cur, int recurse)
{
    xmlNode *temp = NULL;
    while (cur) {
        if (!cur->name) break;
        if (!strcmp((char *)cur->name, name)) break;
        if (recurse) {
                if ((temp=nodematching(name, cur->children, recurse))) {
                        return temp;
                }
        }
        cur = cur->next;
    }

    return cur;
}

void nodelist_rec(char *name, xmlNode *cur, struct nodelistitem **nl);
struct nodelistitem *nodelist(char *name, xmlNode *root)
{
    struct nodelistitem *nl = NULL;
    nodelist_rec(name, root, &nl);
    return nl;
}

void nodelist_rec(char *name, xmlNode *cur, struct nodelistitem **nl)
{
    struct nodelistitem *nli = NULL;

    if (!cur) return;

    if (cur->name && !strcmp((char *)cur->name, name)) {
	nli = malloc(sizeof(*nli));
	if (nli) {
	    nli->node = cur;
	    nli->next = *nl;
	    *nl = nli;
	}
    }
    nodelist_rec(name, cur->children, nl);
    nodelist_rec(name, cur->next, nl);
}

char *textmatching(xmlDocPtr dp, char *name, xmlNode *node, int missing_ok, int recurse)
{
    xmlNode *cur = nodematching(name, node, recurse);
    char *ret = NULL;

    if (!cur) {
	if (!missing_ok) {
		fprintf(stderr, "Invalid or missing contents for %s.\n", name);
	}
    } else if (cur && cur->children && cur->children->content) {
		ret = (char *)xmlNodeListGetRawString(dp, cur->children, 0);
		// ret = cur->children->content;
    } else if (!strcmp(name, "text")) {
		ret = (char *)xmlNodeListGetRawString(dp, cur, 0);
		// ret = cur->content;
    } else {
	if (!missing_ok) {
		fprintf(stderr, "Missing/invalid contents for %s.\n", name);
	} else {
		return NULL;
	}
    }

    return ret;
}

enum states
{
    kGeneral = 0,
    kNames   = 1,
    kRetval  = 2,
    kMan     = 3,
    kLast    = 256
};


void write_funcargs(FILE *fp, usage_t cur)
{
    for (; cur; cur = cur->next) {
	fprintf(fp, ".It Ar \"%s\"", cur->arg);
	fprintf(fp, "\n%s\n", cur->desc);
    }
}


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


int propval(char *name, struct _xmlAttr *prop)
{
    for (; prop; prop=prop->next) {
	if (!strcmp((char *)prop->name, name)) {
		if (prop->children && prop->children->content) {
			return atoi((char *)prop->children->content);
		}
	}
    }
    /* Assume 0 */
    return 0;
}


void write_crossrefs(xmlDocPtr dp, FILE *fp, xmlNode *root)
{
    // xmlNode *node;

    /* For now, there's nothing to do here. */

}

