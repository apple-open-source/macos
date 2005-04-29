
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/dirent.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/tree.h>
#include <libxml/parserInternals.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

typedef struct _xrefnode {
    char *filename;
    char *xref;
    char *title;
    struct _xrefnode *left, *right;
} *xrefnode_t;

struct nodelistitem
{
    xmlNode *node;
    struct nodelistitem *next;
    struct nodelistitem *prev;
};

typedef struct fileref
{
	char name[MAXNAMLEN];
	struct fileref *next;
	struct fileref *threadnext;
} *fileref_t;

struct nodelistitem *nodelist(char *name, xmlNode *root);

xrefnode_t nodehead = NULL;

int debugging = 0;
int filedebug = 0;
int writedebug = 0;
int debug_relpath = 0;

int debug_reparent = 0;

/* Set quick_test to 1 to gather and parse and write without actually
   resolving, or 2 to not do much of anything. */
int quick_test = 0;

int stderrfd = -1;
int nullfd = -1;

char *xmlNodeGetRawString(htmlDocPtr dp, xmlNode *node, int whatever);
char *resolve(char *xref, char *filename, int retarget);
static void *resolve_main(void *ref);
void setup_redirection(void);

int resolved = 0, unresolved = 0, nfiles = 0, broken = 0, plain = 0;

// #define NTHREADS 8
#define MAXTHREADS 16
#define ERRS stderr

int thread_exit[MAXTHREADS];
int thread_processed_files[MAXTHREADS];
int nthreads = 2;

char *striplines(char *line);

#define MAX(a, b) ((a<b) ? b : a)

void redirect_stderr_to_null(void);
void restore_stderr(void);
void writeXRefFile(void);
void writeFile(xmlNode *node, htmlDocPtr dp, char *filename);
void gatherXRefs(xmlNode *node, htmlDocPtr dp, char *filename);
void resolveLinks(xmlNode *node, htmlDocPtr dp, char *filename);
char *proptext(char *name, struct _xmlAttr *prop);
void parseUsage(xmlNode *node);
char *getLogicalPath(char *commentString);
char *getTargetAttFromString(char *commentString);
void addAttribute(xmlNode *node, char *attname, char *attstring);
char *propString(xmlNode *node);
fileref_t getFiles(char *curPath);
void print_statistics(void);
int resolve_mainsub(int pos);
int countfiles(fileref_t rethead);
int onSameLevel(xmlNode *a, xmlNode *b);
int tailcompare(char *string, char *tail);
void writeFile_sub(xmlNode *node, htmlDocPtr dp, FILE *fp, int this_node_and_children_only);
void addXRefSub(xrefnode_t newnode, xrefnode_t tree);
char *relpath(char *origPath, char *filename);
char *fixpath(char *name);
int has_target(xmlNode *node);
char *ts_basename(char *path);
char *ts_dirname(char *path);

int exists(char *filename);

//void *malloc_long(size_t length) { return malloc(length * 30); }
//#define malloc malloc_long
void *db_malloc(size_t length);
void db_free(void *ptr);
// #define malloc db_malloc
// #define free db_free

/*!
	This function returns a malloc-allocated copy of a string.
	This is used for data to be inserted into the tree so that
	it can later be freed with <code>free()</code>.
 */
char *malloccopy(char *string) {
	char *ret = malloc((strlen(string)+1)*sizeof(char));
	strcpy(ret, string);
	return ret;
}

fileref_t threadfiles[MAXTHREADS];

/*! This tool processes links (both in anchor form and in a commented-out
    form) and named anchors, rewriting link destinations to point to those
    anchors.

    Note: if nthreads is negative, the files will be copied and whitespace
    reformatting will occur automatically, but resolution will be skipped.
 */
int main(int argc, char *argv[])
{
    htmlDocPtr dp;
    xmlNode *root;
    int cumulative_exit = 0;
    char *filename;
    char *cwd;
    fileref_t files, curfile;

    setup_redirection();

    if (argc < 1) {
	fprintf(ERRS, "resolveLinks: No arguments given.\n");
	exit(-1);
    }

    /* Test code for db_malloc (debugging malloc) */
    if (0) {
	char *test;
	printf("MALLOCTEST\n");
	test = malloc(37 * sizeof(char));
	free(test);
	printf("MALLOCTEST DONE\n");
	sleep(5);
    }

    LIBXML_TEST_VERSION;

    if (argc < 2) {
	fprintf(ERRS, "Usage: resolveLinks <directory>\n");
	exit(-1);
    }

    cwd = getcwd(NULL, 0);
    if (chdir(argv[1])) {
	if (errno == ENOTDIR) {
		perror(argv[1]);
		fprintf(ERRS, "Usage: resolveLinks <directory> [nthreads]\n");
	} else {
		perror(argv[1]);
		fprintf(ERRS, "Usage: resolveLinks <directory> [nthreads]\n");
	}
	exit(-1);
    }
    chdir(cwd);

    if (argc == 3) { nthreads = atoi(argv[2]); }

    {
      chdir(argv[1]);
      char *newdir = getcwd(NULL, 0);
      char *allyourbase = basename(newdir);
      free(newdir);
      printf("Finding files.\n");
      if (!((files = getFiles(fixpath(allyourbase))))) {
	fprintf(ERRS, "No HTML files found.\n");
	exit(-1);
      }
      chdir("..");
    }

    nfiles = countfiles(files);

    if (debugging || filedebug) {
	for (curfile = files; curfile; curfile = curfile->next) {
		printf("\nWill check %s", curfile->name);
	}
    }

    printf("\nChecking for cross-references\n");
    if (nthreads >= 0) {
      for (curfile = files; curfile; curfile = curfile->next) {
	filename = curfile->name;

	printf("."); fflush(stdout);
	if (debugging) {printf("FILE: %s\n", filename);fflush(stdout);}

	redirect_stderr_to_null();
	if (!(dp = htmlParseFile(filename, "")))
	{
	    restore_stderr();
	    fprintf(ERRS, "resolveLinks: could not parse XML file %s\n", filename);
	    fprintf(ERRS, "CWD is %s\n", getcwd(NULL, 0));
	    exit(-1);
	}
	root = xmlDocGetRootElement(dp);
	restore_stderr();

	if (quick_test < 2) gatherXRefs(root, dp, filename);
	xmlFreeDoc(dp);
      }

      printf("\nWriting xref file\n");
      writeXRefFile();
    } else {
	quick_test = 1;
    }

#ifdef OLD_CODE
    {
      fileref_t next = files;
      int count=0;
      for (count = 0 ; count < nthreads; count++) {
	threadfiles[count % nthreads] = NULL;
      }
      for (curfile = files; next; curfile = next, count++) {
	next = curfile->next;
	curfile->next = threadfiles[count % nthreads];
	threadfiles[count % nthreads] = curfile;
      }

	for (count=0; count < nthreads; count++) {
		if (debugging) printf("FILES[%d] = %d\n", count, countfiles(threadfiles[count]));
	}
    }
#endif


    if (nthreads > 0) {
	printf("\nResolving links (multithreaded)\n");
	/* Spawn off multiple processing threads. */
	pthread_t threads[MAXTHREADS];
	int thread_exists[MAXTHREADS];
	int i;
	for (i=0; i<nthreads; i++) {
		pthread_attr_t *attr = NULL;
		thread_exists[i] = 1;
		if (pthread_create(&threads[i], attr, resolve_main, (void *)i)) {
			printf("Thread %d failed.  Running in main thread.\n", i);
			resolve_mainsub(i);
			thread_exists[i] = 0;
		}
	}
	for (i=0; i<nthreads; i++) {
		int joinret;
		if (debugging) printf("JOINING %d\n", i);
		if (thread_exists[i]) {
			if ((joinret = pthread_join(threads[i], NULL))) {
				if (debugging) printf("JOIN RETURNED %d\n", joinret);
			}
		}
		cumulative_exit = thread_exit[i] ? thread_exit[i] : cumulative_exit;
		if (debugging) printf("thread_exit[%d] = %d\n", i, thread_exit[i]);
	}
    } else {
	/* Resolve links in the main thread */
	if (!nthreads) printf("\nResolving links (multithreaded)\n");
	cumulative_exit = resolve_mainsub(0);
    }
    printf("\nDone\n");

    print_statistics();

    /* Clean up just to be polite. */
    xmlCleanupParser();

    if (cumulative_exit) {
	printf("Exiting with status %d\n", cumulative_exit);
    }
    exit(cumulative_exit);
}

/*! The main pthread body for link resolution */
static void *resolve_main(void *ref)
{
    int ret;

    if (debugging) printf("Thread %d spawned.\n", (int)ref);

// sleep(5*((int)ref));

    ret = resolve_mainsub((int)ref);
    thread_exit[(int)ref] = ret;
    pthread_exit(NULL);
}

#define OLD_LIBXML
/*! This function takes a single argument (the thread number) and
    processes all of the files in the file list corresponding with
    that thread number.
 */
int resolve_mainsub(int pos)
{
    fileref_t files, curfile;
    char *filename;
    htmlDocPtr dp;
    xmlNode *root;
    char tempname[MAXNAMLEN];
#ifndef OLD_LIBXML
    htmlParserCtxtPtr ctxt;
    int options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

#endif

    // if (nthreads > 0) {
	// sprintf(tempname, "/tmp/resolveLinks.%d.%d", getpid(), (int)pthread_self());
    // } else {
	snprintf(tempname, MAXNAMLEN, "%s-temp%d", filename, getpid());
    // }

    files = threadfiles[pos];
    thread_processed_files[pos] = 0;
#ifndef OLD_LIBXML
    int ks = 1;
#endif

    for (curfile = files; curfile; curfile = curfile->threadnext) {
	thread_processed_files[pos]++;

	filename = curfile->name;
	redirect_stderr_to_null();
#ifdef OLD_LIBXML
	if (!(dp = htmlParseFile(filename, "")))
#else
	ctxt = htmlCreateFileParserCtxt(filename, "");

	if (!ctxt) { fprintf(ERRS, "Could not create context\n"); exit(-1); }

	// if (!(dp = htmlCtxtReadFile(ctxt, filename, "", options)))
	ctxt->options = options;
	ctxt->space = &ks;
	ctxt->sax->ignorableWhitespace = NULL;
	ctxt->keepBlanks = 1;
	printf("PRE:\n");
	printf("SAX: %d IWS: %d\n", ctxt->sax, ctxt->sax ? ctxt->sax->ignorableWhitespace : 0);
	printf("KB: %d\n", ctxt->keepBlanks);
	htmlParseDocument(ctxt);
	dp = ctxt->myDoc;
	printf("POST: \n");
	printf("SAX: %d IWS: %d\n", ctxt->sax, ctxt->sax ? ctxt->sax->ignorableWhitespace : 0);
	printf("KB: %d\n", ctxt->keepBlanks);
	htmlFreeParserCtxt(ctxt);
	if (!dp)
#endif
	{
	    restore_stderr();
	    fprintf(ERRS, "resolveLinks: could not parse XML file\n");
	    return -1;
	}

	root = xmlDocGetRootElement(dp);
	restore_stderr();

	if (!quick_test) {
		resolveLinks(root, dp, filename);
		printf("."); fflush(stdout);
	} else {
		printf("X"); fflush(stdout);
	}

	if (debugging || writedebug) printf("WRITING FILE: %s\n", filename);
	writeFile(root, dp, tempname);
	// printf("TREE:\n");
	// writeFile_sub(root, dp, stdout, 1);
	// printf("TREEDONE.\n");
	// exit(-1);

	xmlFreeDoc(dp);
	if (rename(tempname, filename)) {
	    fprintf(ERRS, "error renaming temp file over original.\n");
	    perror("resolveLinks");
	    return -1;
	}
    }

    return 0;
}


char *textmatching(char *name, xmlNode *cur, int missing_ok, int recurse);
xmlNode *nodematching(char *name, xmlNode *cur, int recurse);

/*! This function takes a line from an xref cache file, splits it into
    its components, and adds the xref into the xref tree.

 */
void addXRefFromLine(char *line)
{
    char *iter = line;
    char *xref = NULL;
    char *title = NULL;
    xrefnode_t newnode;

    while (*iter && *iter != 1) {
	iter++;
    }
    if (*iter) {
	*iter = '\0';
	iter++;
	xref = iter;
    }
    while (*iter && *iter != 1) {
	iter++;
    }
    if (*iter) {
	*iter = '\0';
	iter++;
	title = iter;
    }
    if (!xref || !title) {
	fprintf(ERRS, "Corrupted line in xref file.\n");
	return;
    }

    if (!((newnode = malloc(sizeof(*newnode))))) {
	fprintf(ERRS, "Out of memory reading xref file.\n");
	exit(-1);
    }

    newnode->filename = malloccopy(line);
    newnode->xref = malloccopy(xref);
    newnode->title = malloccopy(title);
    newnode->left = NULL;
    newnode->right = NULL;

    if (nodehead) {
	addXRefSub(newnode, nodehead);
    } else {
	nodehead = newnode;
    }

}

/*! This function reads a cross-reference cache file.  This is intended
    to allow eventual incorporation of cross-references that do not live
    in the same directory (or even on the same machine.  It is currently
    unused.
 */
int readXRefFileSub(char *filename)
{
    FILE *fp;
    char line[4098];

    if (!((fp = fopen(filename, "r")))) {
	return 0;
    }

    while (1) {
	if (fgets(line, 4096, fp) == NULL) break;
	if (line[strlen(line)-1] != '\n') {
		fprintf(ERRS, "Warning: ridiculously long line in xref file.\n");
	} else {
		line[strlen(line)-1] = '\0';
	}
	addXRefFromLine(line);
    }

    fclose(fp);
    return 1;
}

/*! This function is the recursive tree walk subroutine used by
    writeXRefFile to write cross-references to a cache file.
 */
void writeXRefFileSub(xrefnode_t node, FILE *fp)
{
    if (!node) return;

    writeXRefFileSub(node->left, fp);

    //fprintf(fp, "filename=\"%s\" id=\"%s\" title=\"%s\"\n",
		// node->filename, node->xref, node->title);
    fprintf(fp, "%s%c%s%c%s\n",
		node->filename, 1, node->xref, 1, node->title);

    writeXRefFileSub(node->right, fp);
}

/*! This function writes a cross-reference cache file.  This is intended
    to allow eventual incorporation of cross-references that do not live
    in the same directory (or even on the same machine.  It is currently
    called, but the resulting file is not yet used.
 */
void writeXRefFile(void)
{
    FILE *fp;

    if (!((fp = fopen("/tmp/xref_out", "w")))) {
	return;
    }
    fprintf(fp, "Cross-references seen (for debugging only)\n\n");

    writeXRefFileSub(nodehead, fp);

    fclose(fp);
}

/*! This function is the recursive tree walk subroutine used by
    addXRef when adding cross-references to the xref tree.
 */
void addXRefSub(xrefnode_t newnode, xrefnode_t tree)
{
    int pos = strcmp(newnode->xref, tree->xref);
    if (pos < 0) {
	/* We go left */
	if (tree->left) addXRefSub(newnode, tree->left);
	else {
		tree->left = newnode;
	}
    } else {
	/* We go right */
	if (tree->right) addXRefSub(newnode, tree->right);
	else {
		tree->right = newnode;
	}
    }
}

/*! This function inserts a new cross-reference into the
    xref tree for later use in link resolution.
 */
void addXRef(xmlNode *node, char *filename)
{
    xrefnode_t newnode;
    char *tempstring;
    char *bufptr;
    char *nextstring;
    char *pt;

    if (!node) {
	printf("WARNING: addXRef called on null node\n");
    }

    pt = proptext("name", node->properties);
    if (!pt) {
	printf("WARNING: addXRef called on anchor with no name property\n");
    }

    if (debugging) {printf("STRL %ld\n",  strlen(pt)); fflush(stdout);}
    tempstring = (bufptr = malloc((strlen(pt)+1) * sizeof(char)));
    strcpy(tempstring, pt);

    while (tempstring && *tempstring) {
	for (nextstring = tempstring; *nextstring && (*nextstring != ' '); nextstring++);
	if (*nextstring) {
		*nextstring = '\0';
		nextstring++;
	} else {
		nextstring = NULL;
	}
	newnode = malloc(sizeof(*newnode));
	if (strlen(tempstring)) {
		newnode->filename = filename;
		newnode->xref = tempstring;
		// newnode->xref = pt;
		newnode->title = proptext("title", node->properties);
		newnode->left = NULL;
		newnode->right = NULL;

		if (nodehead) {
			addXRefSub(newnode, nodehead);
		} else {
			nodehead = newnode;
		}

		tempstring = nextstring;
	}
    }
}

/*! This function walks the parse tree of an HTML file, gathers all
    apple_ref anchors, and adds then into the xref tree with addXRef.
 */

void gatherXRefs(xmlNode *node, htmlDocPtr dp, char *filename)
{
    if (!node) return;
    if (node->name && !strcmp(node->name, "a")) {
	char *pt = proptext("name", node->properties);
	char *pos = pt;

	while (pos && *pos && *pos == ' ') pos++;

	if (pos) {
	    if (debugging) printf("MAYBE: %s\n", pos);
	    if ((pos[0] == '/') && (pos[1] == '/')) {
		/* It has a name property that starts with two
		   slashes.  It's an apple_ref. */
		if (debugging) printf("YO: %s\n", pos);
		addXRef(node, filename);
	    } else {
		if (debugging) printf("NOT A REF\n");
	    }
	}
    }

    gatherXRefs(node->children, dp, filename);
    gatherXRefs(node->next, dp, filename);
}

/*! Returns true if the text (from an HTML comment) represents the
    start of a link request
 */
int isStartOfLinkRequest(char *text)
{
    if (getLogicalPath(text)) return 1;
    return 0;
}

/*! Returns true if the text (from an HTML comment) represents the
    end of a link request
 */
int isEndOfLinkRequest(char *text)
{
    char *ptr = text;

    if (!ptr) return 0;

    while (ptr && *ptr == ' ') ptr++;
    if (ptr[0] != '/' || ptr[1] != 'a' || ptr[2] != ' ') {
	return 0;
    }
    if (debugging) printf("ENDLINK\n");
    return 1;
}


/*! Walks the parse tree of an HTML document and does the actual link
    resolution, inserting href attributes where applicable, converting
    anchors that fail to resolve into comments, and converting resolvable
    commented links back into anchors.
 */
void resolveLinks(xmlNode *node, htmlDocPtr dp, char *filename)
{
    if (!node) return;

    if (node->name && !strcmp(node->name, "comment")) {
	if (debugging) { printf("comment: \"%s\"\n", node->content); }
	if (isStartOfLinkRequest(node->content)) {
		xmlNode *close = NULL;
		struct nodelistitem *nodelisthead = NULL;
		struct nodelistitem *nodelistiterator = NULL;

		if (debugging || debug_reparent) printf("SOLR\n");
		if (node->next) {
			/* The node list is in reverse order of match. Skip to the end and
			   work our way back to the beginning.
			 */
			nodelisthead = nodelist("comment", node->next);
			while (nodelisthead && nodelisthead->next) nodelisthead = nodelisthead->next;
		}
		nodelistiterator = nodelisthead;

		while (nodelistiterator && !close) {

			if (debugging || debug_reparent) printf("NODE: %s\n", nodelistiterator->node->name);

			if (nodelistiterator->node->name && !strcmp(nodelistiterator->node->name, "comment") &&
					isEndOfLinkRequest(nodelistiterator->node->content)) {
				if (debugging || debug_reparent) printf("Is EOLR\n");
				close = nodelistiterator->node;
			} else {
				if (debugging || debug_reparent) {
					printf("No match.  Content was \"%s\"\n", nodelistiterator->node->content);
				}
			}
			
			nodelistiterator = nodelistiterator->prev;
		}
		if (close) {
			/* Link Request. */
			char *lp = getLogicalPath(node->content);
			char *frametgt = getTargetAttFromString(node->content);
			int retarget = (!frametgt || !strlen(frametgt));
			char *target = resolve(lp, filename, retarget);
			if (debugging) printf("RETARGET SHOULD HAVE BEEN %d (frametgt is 0x%p)\n", retarget, frametgt);
			if (debugging) printf("EOLR\n");

			if (debugging) {printf("LP: \"%s\"\n", lp);}

			if (target) {
				xmlNode *lastnode;
				int samelevel = 0;

				resolved++;

				/* Make link live */
				if (debugging) printf("FOUND!\n");

				samelevel = onSameLevel(node, close);
				if (samelevel) {
					/* If close is on the same level as open,
					   the last node inside the anchor is
					   the one before the close.  */
					lastnode = close->prev;
					lastnode->next = close->next;
					if (close->next) close->next->prev = close->prev;
				} else {
#if 0
					if (close->prev) {
						/* Make element after close tag
						   be after elemenet prior to cloes tag */
						close->prev->next = close->next;
						if (close->next) {
							close->next->prev = close->prev;
						}
					} else {
						/* Make element after close tag
						   a child of close tag's parent. */
						close->parent->children = close->next;
						if (close->next) {
							close->next->prev = NULL;
							close->next->parent = close->parent;
						}
					}
#else
					/* Just change the end tag to an empty text container.  It's safer. */
					close->name=malloccopy("text");
					close->content=malloccopy("");
					close->type=XML_TEXT_NODE;
#endif
					lastnode = close->parent;
					while (lastnode && !onSameLevel(lastnode, node)) {
						lastnode = lastnode->parent;
					}
				}
				if (lastnode) {
					xmlNode *iter;
					if (debugging) printf("LAST NODE FOUND.\n");
					/* We successfully resolved this 
					   commented-out link and successfully
					   found the commend marker indicating
					   the end of the anchor.

					   Because the close tag could be
					   in some illegal place (splitting
					   an element), we work our way up
					   the tree until we are at the same
					   level as the start node and make
					   whatever node we find be the
					   last child.

					   The effect of this is that if the
					   close tag splits some element,
					   that entire element will end up
					   as part of the link.  It isn't
					   worth trying to split an element
					   in half to make up for an
					   incorrectly-generated link request.

					   In an ideal world, that link-end
					   comment should always be on the
					   same level, but in practice, this
					   has often failed to be the case.

					   In any case, once we have the
					   tailnode (the rightmost node
					   to move), we reparent everything
					   from node through tailnode as a
					   child of node.  The tailnode
					   (link-end comment) gets turned
					   into an empty text element (which
					   effectively deletes it without
					   requiring as much effort).
					 */

					node->name = malloccopy("a");
					node->type = XML_ELEMENT_NODE;
					addAttribute(node, "href", target);
					addAttribute(node, "logicalPath", lp);
					if (frametgt) {
						if (debugging) printf("FRAMETGT FOUND\n");
						addAttribute(node, "target", frametgt);
					} else {
						if (debugging) printf("NO FRAMETGT FOUND\n");
						char *pos;
						int index = 1;

						pos = target;
						while (*pos && *pos != '?' && *pos != '#') pos++;
						if (pos < target + 10) {
							index = 0;
						} else {
							pos -= 10;
							if (strncmp(pos, "index.html", 10)) index = 0;
						}
						if (index) {
							addAttribute(node, "target", "_top");
						}
					}
					node->content = NULL; /* @@@ LEAK? @@@ */
					/* Reparent everything from the open comment to the close comment under an anchor tag. */
					node->children = node->next;
					node->children->prev = NULL;
					for (iter = node->children; iter && iter != lastnode; iter = iter->next) {
						
						iter->parent = node;
						if (debug_reparent) {
							printf("REPARENTING: \n");
							writeFile_sub(node, dp, stdout, 1);
							printf("DONE REPARENTING\n");
						}
					}
					if (iter != lastnode) {
						fprintf(stderr, "Warning: reparenting failed.\n");
					}
					lastnode->parent = node;
					if (lastnode->next) {
						lastnode->next->prev = node;
					}
					node->next = lastnode->next;
					lastnode->next = NULL;
				} else {
					/* We ran off the top of the tree! */
					fprintf(ERRS, "NESTING PROBLEM: close link request marker nested higher than open marker.\n");
					fprintf(ERRS, "Giving up.\n");
				}
			} else {
				unresolved++;
			}
		} else {
			broken++;
		}
	}
    } else if (node->name && !strcmp(node->name, "a")) {
		/* Handle the already-live link */
		int retarget = (!has_target(node));
		char *lp = proptext("logicalPath", node->properties);
		// char *name = proptext("name", node->properties);
		char *href = proptext("href", node->properties);

		if (lp && href) {
			char *target = resolve(lp, filename, retarget);

			if (target) {
				if (debugging) printf("FOUND!\n");
				addAttribute(node, "href", target);
				resolved++;
			} else {
				xmlNode *iter = node->children, *tailnode;

				/* We couldn't resolve this live link.
				   turn it back into a comment, inserting
				   a close comment tag after its last child
				   and reparenting its children as children
				   of its parent (as its "next" node). */

				unresolved++;

				if (debugging) printf("Disabling link\n");
				tailnode = malloc(sizeof(*tailnode));
				bzero(tailnode, sizeof(*tailnode));

				while (iter && iter->next) {
					iter->parent = node->parent;
					iter = iter->next;
					if (debug_reparent) {
						printf("REPARENTING: \n");
						writeFile_sub(node, dp, stdout, 1);
						printf("DONE REPARENTING\n");
					}
				}
				tailnode->next = node->next;
				if (iter) {
					iter->parent = node->parent;
					iter->next = tailnode;
					tailnode->prev = iter;
					node->children->prev = node;
					node->next = node->children;
				} else {
					tailnode->prev = node;
					node->next = tailnode;
				}
				node->children = NULL;

				node->name = malloccopy("comment");
				node->type = XML_COMMENT_NODE;
				tailnode->name = malloccopy("comment");
				tailnode->type = XML_COMMENT_NODE;
				tailnode->content = malloccopy(" /a ");
				node->content = propString(node);
				if (debugging) printf("PS: \"%s\"\n", node->content);
			}
		} else {
			if (debugging) printf("Not a logicalPath link.  Skipping.\n");
			plain++;
		}
    }

    resolveLinks(node->children, dp, filename);
    resolveLinks(node->next, dp, filename);
}

/*! Returns true if the two nodes are at the same level in the XML
    parse tree, else false.
 */
int onSameLevel(xmlNode *a, xmlNode *b)
{
    xmlNode *iter;

    if (a == b) return -1;

    for (iter = a->prev; iter; iter=iter->prev) {
	if (iter == b) {
		return 1;
	}
    }
    for (iter = a->next; iter; iter=iter->next) {
	if (iter == b) {
		return 1;
	}
    }

    return 0;
}

/*! Writes an HTML parse tree to a file on disk. */
void writeFile(xmlNode *node, htmlDocPtr dp, char *filename)
{
#ifdef OLD_CODE
    FILE *fp;
    if (debugging) writeFile_sub(node, dp, stdout, 0);
    if (!((fp = fopen(filename, "w")))) {
	fprintf(ERRS, "Could not open file %s for writing\n", filename);
	exit(-1);
    }

    writeFile_sub(node, dp, fp, 0);

    fclose(fp);
#else
    int ret = htmlSaveFile(filename, dp);

    if (ret <= 0) {
	fprintf(ERRS, "Failed to save file \"%s\"\n", filename);
	perror("xmlSaveFile");

	fprintf(stderr, "PRINTING TREE DUMP\n");

	writeFile_sub(xmlDocGetRootElement(dp), dp, stdout, 1);

	exit(-1);
    }
#endif
}

/*! Returns a string containing the text representation of a node's properties.
 */
char *propString(xmlNode *node)
{
    xmlAttr *prop = node->properties;
    char *propstring;
    int len=1;

    while (prop) {
	// fprintf(fp, " %s=\"%s\"", prop->name, prop->children->content);
	if (prop->children) {
		len += strlen(prop->name) + strlen(prop->children->content) + 4;
	}
	prop = prop->next;
    }
    propstring = malloc((len + 5) * sizeof(char));

    strcpy(propstring, "a");

    prop = node->properties;
    while (prop) {
	if (prop->children) {
		strcat(propstring, " ");
		strcat(propstring, prop->name);
		strcat(propstring, "=\"");
		strcat(propstring, prop->children->content);
		strcat(propstring, "\"");
	}

	prop = prop->next;
    }

    // printf("propstring was %s\n", propstring);

    return propstring;
}

/*! Writes a node's properties to a file. */
void writeProps(xmlNode *node, FILE *fp)
{
    xmlAttr *prop = node->properties;

    while (prop) {
	if (prop->children) {
		fprintf(fp, " %s=\"%s\"", prop->name, prop->children->content);
	}
	prop = prop->next;
    }

}

/*! This function is the recursive tree walk subroutine used by
    writeFile to write an HTML parse tree to disk.
 */
void writeFile_sub(xmlNode *node, htmlDocPtr dp, FILE *fp, int this_node_and_children_only)
{
    if (!node) return;

    // if (debugging) printf("DP: 0x%x NODE: 0x%x\n", (long)dp, (long)node);
    // fprintf(fp, "RS: %s\n", xmlNodeListGetRawString(dp, node, 0));

    if (node->name) {
      if (!strcmp(node->name, "text")) {
	fprintf(fp, "%s", xmlNodeGetRawString(dp, node, 0));
      } else if (!strcmp(node->name, "comment")) {
	fprintf(fp, "<!--%s-->", node->content);
      } else {
	fprintf(fp, "<%s", node->name);
	writeProps(node, fp);
	fprintf(fp, ">");
      }
    }

    writeFile_sub(node->children, dp, fp, 0);

    if (strcmp(node->name, "text") && strcmp(node->name, "comment")) {
	fprintf(fp, "</%s>", node->name);
    }

    if (!this_node_and_children_only) writeFile_sub(node->next, dp, fp, 0);
    return;
}

/*! Returns the first node whose element name matches a given node.

    If recurse is false, it begins at the current node and moves
    sideways across the parse tree to the next node until it runs
    out of nodes.

    If recurse is true, it processes all of the children of each node
    before moving across to the next one.

    This function is part of the xml2man library code and is not used in
    this tool.  @@@ remove me? @@@
 */
xmlNode *nodematching(char *name, xmlNode *cur, int recurse)
{
    xmlNode *temp = NULL;
    while (cur) {
	if (!cur->name) break;
	if (!strcasecmp(cur->name, name)) break;
	if (recurse) {
		if ((temp=nodematching(name, cur->children, recurse))) {
			return temp;
		}
	}
	cur = cur->next;
    }

    return cur;
}

/*! This function does the same thing as nodematching, only it returns
    the first text contents of the node.

    If the node name is "text" or "comment", it returns the text of the
    node itself.  Otherwise, it returns the text of the node's first
    child.

    This function is part of the xml2man library code and is not used in
    this tool.  @@@ remove me? @@@
 */
char *textmatching(char *name, xmlNode *node, int missing_ok, int recurse)
{
    xmlNode *cur = nodematching(name, node, recurse);
    char *ret = NULL;

    if (!cur) {
	if (!missing_ok) {
		fprintf(ERRS, "Invalid or missing contents for %s.\n", name);
	}
    } else if (cur && cur->children && cur->children->content) {
		ret = cur->children->content;
    } else if (!strcasecmp(name, "text")) {
		ret = cur->content;
    } else {
	fprintf(ERRS, "Missing/invalid contents for %s.\n", name);
    }

    return ret;
}

/*! This function returns the text contents of a named attribute
    in a list of attributes. */
char *proptext(char *name, struct _xmlAttr *prop)
{
    for (; prop; prop=prop->next) {
	if (!strcasecmp(prop->name, name)) {
		if (prop->children && prop->children->content) {
			return prop->children->content;
		}
	}
    }
    /* Assume 0 */
    return 0;
}

/*! This returns the value of a numeric property.

    This function is part of the xml2man library code and is not used in
    this tool.  @@@ remove me? @@@
 */
int propval(char *name, struct _xmlAttr *prop)
{
    for (; prop; prop=prop->next) {
	if (!strcasecmp(prop->name, name)) {
		if (prop->children && prop->children->content) {
			return atoi(prop->children->content);
		}
	}
    }
    /* Assume 0 */
    return 0;
}

/*! This function pulls the logicalPath attribute out of a string of
    attributes.  The logicalPath attribtue must currently be first.

    @@@ Do we want to support logicalPath appearing elsewhere? @@@
 */
char *getLogicalPath(char *commentString)
{
    char *retptr = NULL;
    char *ptr = commentString;

    if (!commentString) return NULL;

// printf("commentString: %s\n", commentString);

    while (*ptr == ' ') ptr++;
    if (*ptr != 'a') return NULL;
    ptr++;
    if (*ptr != ' ') return NULL;
    while (*ptr == ' ') ptr++;
    if (!strncasecmp(ptr, "logicalPath", 11)) {
	char *startptr; int count=0;

	if (debugging) printf("STARTLINK\n");

	ptr += 11;
	while (*ptr == ' ') ptr++;
	if (*ptr != '=') return NULL;
	ptr++;
	while (*ptr == ' ') ptr++;
	if (*ptr != '"') return NULL;
	ptr++;
	startptr = ptr;
	// printf("STARTPTR: %s\n", startptr);
	while (*ptr && *ptr != '"') { ptr++; count++; }
	retptr = malloc((count + 1024) * sizeof(char));
	strncpy(retptr, startptr, count);
	retptr[count] = '\0';
	return retptr;
    }

    return NULL;
}

/*! getTargetAttFromString gets a target attribute from a commented-out
    link request. */
char *getTargetAttFromString(char *commentString)
{
    char *retptr = NULL;
    char *ptr = commentString;

    if (!commentString) return NULL;

// printf("commentString: %s\n", commentString);

    while (*ptr == ' ') ptr++;
    if (*ptr != 'a') return NULL;
    ptr++;
    if (*ptr != ' ') return NULL;
    while (*ptr == ' ') ptr++;
    if (strncasecmp(ptr, "logicalPath", 11)) {
	return NULL;
    }
    ptr += strlen("logicalPath");
    while (*ptr && (*ptr != '"')) ptr++;
    ptr++;
    while (*ptr && (*ptr != '"')) ptr++;
    ptr++;
    while (*ptr && (*ptr != ' ')) ptr++;
    while (*ptr && (*ptr == ' ')) ptr++;

    if (!strncasecmp(ptr, "target", 11)) {
	char *startptr; int count=0;

	if (debugging) printf("STARTTARGET\n");

	ptr += 11;
	while (*ptr == ' ') ptr++;
	if (*ptr != '=') return NULL;
	ptr++;
	while (*ptr == ' ') ptr++;
	if (*ptr != '"') return NULL;
	ptr++;
	startptr = ptr;
	// printf("STARTPTR: %s\n", startptr);
	while (*ptr && *ptr != '"') { ptr++; count++; }
	retptr = malloc((count + 1024) * sizeof(char));
	strncpy(retptr, startptr, count);
	retptr[count] = '\0';
	return retptr;
    }

    if (debugging) printf("TARGETFIND RETURNING NULL\n");
    return NULL;
}



/*! This function gets the raw text (with entities intact) for a
    single node.  This is similar to xmlNodeListGetRawString (part of
    libxml), only it does not traverse the node tree.
 */
char *xmlNodeGetRawString(htmlDocPtr dp, xmlNode *node, int whatever)
{
    xmlNode copynode;

    bcopy(node, &copynode, sizeof(copynode));
    copynode.next = NULL;

    return xmlNodeListGetRawString(dp, &copynode, whatever);
}


typedef struct refparts {
	char *refpart;
	char *langpart;
	char *rest;
} *refparts_t;


/*! This function divides an apple_ref reference into its constituent
    parts and returns then in a refparts_t structure.
 */
refparts_t getrefparts(char *origref)
{
    char *refpart = origref;
    char *langpart;
    char *rest;
    char *refpartcopy = NULL, *langpartcopy = NULL;
    static struct refparts retval;
    int refpartlen = 0, langpartlen = 0;

    if (origref[0] != '/' || origref[0] != '/') return NULL;

    refpart += 2;
    langpart = refpart;
    while (*langpart && *langpart != '/') {
	langpart++; refpartlen++;
    }
    langpart++;
    rest = langpart;
    while (*rest && *rest != '/') {
	rest++; langpartlen++;
    }
    rest++;

    if (!strlen(refpart) || !strlen(langpart) ||!strlen(rest)) {
	fprintf(ERRS, "WARNING: malformed apple_ref has less than three parts.\n");
	return NULL;
    }

    refpartcopy = malloc((refpartlen + 1) * sizeof(char));
    strncpy(refpartcopy, refpart, refpartlen);
    refpartcopy[refpartlen] = '\0';

    langpartcopy = malloc((langpartlen + 1) * sizeof(char));
    strncpy(langpartcopy, langpart, langpartlen);
    langpartcopy[langpartlen] = '\0';

    // printf("LPC: \"%s\"\n", langpartcopy);

    retval.refpart = refpartcopy;
    retval.langpart = langpartcopy;
    retval.rest = rest;

    return &retval;
}

/*! This function takes an apple_ref and rewrites it, changing the
    language to the language specified by <code>lang</code>.
 */
char *refLangChange(char *ref, char *lang)
{
    refparts_t rp = getrefparts(ref);
    char *refpart, *rest, *retval;
    int length;

    if (!rp) return NULL;
    refpart = rp->refpart;
    rest = rp->rest;

    // printf("LANGPART: \"%s\"\n", rp->langpart);

    /* length = "//" + refpart + "/" + "C" + "/" + rest + "\0" */
    length = 2 + strlen(refpart) + 1 + 1 + 1 + strlen(rest) + 1;

    retval = malloc((length + 1) * sizeof(char));
    sprintf(retval, "//%s/c/%s", refpart, rest);

    return retval;
}

/*! This function takes a filename and an anchor within that file
    and concatenates them into a full URL.
 */
char *makeurl(char *filename, char *offset, int retarget)
{
#if 1
    char *buf = NULL;
    char *dir = ts_dirname(filename);
    char *base = ts_basename(filename);
    char *updir = ts_dirname(dir);
    char *upbase = ts_basename(dir);
    char *indexpath = malloc(strlen(updir) + 1 /*/*/ + strlen("index.html") + 1);

    int len = (strlen(filename)+strlen(offset)+2) * sizeof(char);

if (debugging) printf("RETARGET (INITIAL): %d\n", retarget);

    sprintf(indexpath, "%s/index.html", updir);
    if (!strcmp(base, "index.html") || !strcmp(base, "CompositePage.html")) {
	if (debugging) printf("Going to an index.html file.  Not retargetting.\n");
	if (debugging) printf("FILENAME: %s\nOFFSET: %s\n", filename, offset);
	retarget = 0;
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
    }

    if (retarget && !exists(indexpath)) {
	fprintf(stderr, "No index found at %s.\n", indexpath);
	fprintf(stderr, "DIR: %s\nBASE: %s\nUPDIR: %s\nUPBASE: %s\nORIG_FILENAME: %s\n", dir, base, updir, upbase, filename);
	retarget = 0;
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
    }

    if (retarget) {
	len = strlen(indexpath) + 1 /*?*/ + strlen(upbase) + 1 /*/*/ + strlen(base) + 1 /*#*/ + strlen(offset) + 1 /*NULL*/;
    }

    buf = malloc(len);

    if (!buf) return "BROKEN";

if (debugging) printf("RETARGET: %d\n", retarget);

    if (retarget) {
	sprintf(buf, "%s?%s/%s#%s", indexpath, upbase, base, offset);
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
    } else {
	strcpy(buf, filename);
	strcat(buf, "#");
	strcat(buf, offset);
    }

    return buf;
#else
    return malloccopy(filename);
#endif
}

/*! This function recursively searches for an xref in the xref tree,
    returning the filename and anchor within that file, concatenated with
    makeurl.
 */
char *searchref(char *xref, xrefnode_t tree, int retarget)
{
    int pos;

    if (!tree) return NULL;

    pos = strcmp(xref, tree->xref);

    if (!pos) {
	/* We found it. */
	return makeurl(tree->filename, tree->xref, retarget);
    } else if (pos < 0) {
        /* We go left */
        return searchref(xref, tree->left, retarget);
    } else {
        /* We go right */
        return searchref(xref, tree->right, retarget);
    }
}

/*! This function attempts to resolve a space-delimited list of
    cross-references, allowing language fallback (c++ to C, etc.),
    and returns the URL associated with it.
 */
char *resolve(char *xref, char *filename, int retarget)
{
    char *curref = xref;
    char *writeptr = xref;
    char *target = NULL;

    if (!xref) return NULL;

    while (writeptr) {
	while (*writeptr && *writeptr != ' ') writeptr++;
	if (*writeptr == ' ') *writeptr = '\0';
	else writeptr = 0;

	if (strlen(curref)) {
		char *altLangRef = NULL;
		refparts_t rp;

		if (debugging) { printf("SEARCHING FOR \"%s\"\n", curref); }

		if ((rp = getrefparts(curref))) {
			if (!strcmp(rp->langpart, "cpp") ||
			    !strcmp(rp->langpart, "occ") ||
			    !strcmp(rp->langpart, "C")) {
				altLangRef = refLangChange(curref, "c");
			}
		}

		if (altLangRef && debugging) { printf("ALSO SEARCHING FOR \"%s\"\n", altLangRef); }

		target = searchref(curref, nodehead, retarget);
		if (!target && altLangRef) target = searchref(altLangRef, nodehead, retarget);
		if (target) {
			if (debugging) printf("Mapping %s to %s\n", xref, target);
			return relpath(target, filename);
		}
	}

	if (writeptr) {
		*writeptr = ' ';
		writeptr++;
		curref = writeptr;
	}
    }

    if (debugging) printf("Ref not found\n");
    return NULL;
}

void nodelist_rec(char *name, xmlNode *cur, struct nodelistitem **nl);

/*! This function recursively descends an HTML parse tree,
    returning a list of nodes matching a given name.
 */
struct nodelistitem *nodelist(char *name, xmlNode *root)
{
    struct nodelistitem *nl = NULL;
    nodelist_rec(name, root, &nl);
    return nl;
}

/*! This function is the recursive tree walk subroutine used by
    the nodelist function.
 */
void nodelist_rec(char *name, xmlNode *cur, struct nodelistitem **nl)
{
    struct nodelistitem *nli = NULL;

    if (!cur) return;

    if (cur->name && !strcmp(cur->name, name)) {
        nli = malloc(sizeof(*nli));
        if (nli) {
            nli->node = cur;
            nli->next = *nl;
	    nli->prev = NULL;
	    if (*nl) {
		(*nl)->prev = nli;
	    }
            *nl = nli;
        }
    }
    nodelist_rec(name, cur->children, nl);
    nodelist_rec(name, cur->next, nl);
}

/*! This function adds an attribute to an HTML node, deleting
    any preexisting attribute with the same name as it does so.
 */
void addAttribute(xmlNode *node, char *attname, char *attstring)
{
    xmlAttr *properties, *myprop, *lastprop;
    xmlNode *mypropnode;
    xmlAttr *delprop = NULL;

    myprop = malloc(sizeof(*myprop));
    bzero(myprop, sizeof(*myprop));

    mypropnode = malloc(sizeof(*mypropnode));
    bzero(mypropnode, sizeof(*mypropnode));

    myprop->type = XML_ATTRIBUTE_NODE;
    myprop->name = malloccopy(attname);
    myprop->children = mypropnode;
    myprop->parent = node;
    myprop->doc = node->doc;
    mypropnode->type = XML_TEXT_NODE;
    mypropnode->name = malloccopy("text");
    mypropnode->content = malloccopy(attstring);
    mypropnode->parent = (void *)&myprop;
    mypropnode->doc = node->doc;

    properties = node->properties;
    lastprop = properties;
    for (properties = node->properties; properties; properties = properties->next) {
	if (delprop) {
		// free(delprop); /* @@@ */
		delprop = NULL;
	}
	if (!strcasecmp(properties->name, attname)) {
		delprop = properties;
		if (delprop->prev) delprop->prev->next = delprop->next;
		else node->properties = delprop->next;
		if (delprop->next) delprop->next->prev = delprop->prev;

	}
	if (!delprop) lastprop = properties;
    }
    if (!lastprop) {
	node->properties = myprop;
    } else {
	lastprop->next = myprop;
	myprop->prev = lastprop;
    }
    // myprop->parent = node;
}

/*! This function returns a list of HTML files within a given directory. */
fileref_t getFiles(char *curPath)
{
    fileref_t rethead = NULL, rettail = NULL;
    DIR *dirp;
    struct dirent *entp;
    int localDebug = 0;

    if (!((dirp = opendir(".")))) return NULL;

    while ((entp = readdir(dirp))) {
	printf("."); fflush(stdout);
	if (entp->d_type == DT_DIR && strcmp(entp->d_name, ".") && strcmp(entp->d_name, "..")) {
		fileref_t recreturn;
		char *newpath;

		newpath = malloc(MAXNAMLEN * sizeof(char));

		if (!newpath) {
			perror("resolveLinks"); return NULL;
		}

		if (chdir(entp->d_name)) {
			perror("resolveLinks"); return NULL;
		}

		if (debugging || localDebug) printf("CURPATH: \"%s\" NP: 0x%lx\n", curPath, (long)newpath);

		strcpy(newpath, curPath);
		strcat(newpath, "/");
		strcat(newpath, entp->d_name);

		if (debugging || localDebug) printf("Recursing into %s.\n", newpath);

		recreturn = getFiles(newpath);
		free(newpath);

		if (debugging || localDebug) printf("Recursing out.\n");

		if (debugging || localDebug) printf("OLD COUNT: %d\n", countfiles(rethead));
		if (debugging || localDebug) printf("INS COUNT: %d\n", countfiles(recreturn));
		if (rettail) {
			rettail->next = recreturn;
			while (rettail && rettail->next) {
				rettail = rettail->next;
				// printf("NEXT\n");
			}
			if (debugging || localDebug) printf("CONCATENATING LISTS\n");
		} else {
			rethead = rettail = recreturn;
			while (rettail && rettail->next) {
				rettail = rettail->next;
				// printf("NEXT\n");
			}
			if (debugging || localDebug) printf("NEW LIST\n");
		}
		if (debugging || localDebug) printf("NEW COUNT: %d\n", countfiles(rethead));
		chdir("..");
	} else if (tailcompare(entp->d_name, ".htm") || tailcompare (entp->d_name, ".html") || tailcompare(entp->d_name, ".shtml") || tailcompare(entp->d_name, ".shtml")) {
		/* HTML FILE */
		if (debugging || localDebug) printf("HTML FILE %s\n", entp->d_name);
		fileref_t newent = malloc(sizeof(*newent));
		if (!newent) { perror("resolveLinks"); exit(-1); }
		newent->next = NULL;
		strcpy(newent->name, curPath);
		strcat(newent->name, "/");
		strcat(newent->name, entp->d_name);

		if (rettail) {
			rettail->next = newent;
			rettail = newent;
		} else rethead = rettail = newent;

		if (nthreads) {
			newent->threadnext = threadfiles[nfiles % nthreads];
			threadfiles[nfiles % nthreads] = newent;
		} else {
			newent->threadnext = threadfiles[0];
			threadfiles[0] = newent;
		}

		if (debugging || localDebug) printf("NEWCOUNT: %d\n", countfiles(rethead));
	}
    }
    // countfiles(rethead);

    closedir(dirp);
    return rethead;
}

/*! This function counts the number of files in a list of files.
    It is used for debugging purposes.
 */
int countfiles(fileref_t rethead)
    {
	fileref_t iter;
	int stagecount = 0;
	for (iter = rethead; iter; iter = iter->next) stagecount++;
	// printf("stage: %d\n", stagecount);
	return stagecount;
    }

/*! This function compares the end of a filename to a given substring.
    It returns 1 on a match or 0 on failure.
 */
int tailcompare(char *string, char *tail)
{
    char *pos = &string[strlen(string) - strlen(tail)];
    if (strlen(string) < strlen(tail)) return 0;

    if (debugging) printf("LENGTHS: %ld %ld %ld\n", strlen(string), strlen(tail), strlen(string)-strlen(tail));

    if (debugging) printf("Comparing: \"%s\" to \"%s\"\n", pos, tail);

    if (!strcasecmp(pos, tail)) {
	if (debugging) printf("MATCH\n");
	return 1;
    }
    return 0;
}

/*! This function prints cumulative statistics about this run of the tool. */
void print_statistics(void)
{
    int i, nprocessedfiles=0, ttlreqs = resolved + unresolved + broken;

    for (i=0; i<nthreads; i++) {
	nprocessedfiles += thread_processed_files[i];
    }

    printf("=====================================================================\n");
    printf("  Statistics:\n\n");
    printf("         files: %3d\n", nfiles);
    printf("     processed: %3d\n", nprocessedfiles);
    printf("    total reqs: %3d\n", ttlreqs);
    printf("      resolved: %3d\n", resolved);
    printf("    unresolved: %3d\n", unresolved);
    printf("        broken: %3d\n", broken);
    printf("         plain: %3d\n", plain);
    printf("         total: %3d\n", plain+broken+resolved+unresolved);

    if (ttlreqs) {
	float percent = (((float) resolved / (float)ttlreqs) * 100.0f);
	printf("    %% resolved: %f\n", percent);
    }

}

int round4(int k) { return ((k+3) & ~0x3); }

#undef malloc
#undef free

/*! This function is a debug version of malloc that uses guard bytes
    to detect buffer overflows.
 */
void *db_malloc(size_t length)
{
    int *ret = malloc(round4(length)+12);
    ret[0] = round4(length);
    ret[1] = 0xdeadbeef;
    ret[(round4(length)/4)+2] = 0xdeadbeef;

    ret++; ret++;

    return ret;
}

/*! This function is a debug version of free that uses guard bytes
    to detect buffer overflows.
 */
void db_free(void *ptr)
{
    int *intptr = ptr, *intcheckptr;
    char *tail = ptr;

    intptr--;
    if (*intptr != 0xdeadbeef) {
	printf("WARNING: freeing region not allocated by db_malloc\n");
	free(ptr);
    } else {
	intptr--;
	tail += (*intptr);
	intcheckptr = (int *)tail;
	// printf("Length was %d\n", *intptr);
	if (*intcheckptr != 0xdeadbeef) {
		printf("WARNING: region scribbled off end\n");
	}
	free(intptr);
    }
}

void setup_redirection(void)
{
    int fail = 0, localDebug = 0;

    nullfd = open("/dev/null", (O_RDWR|O_NONBLOCK), 0);
    stderrfd = dup(STDERR_FILENO);

    if (nullfd == -1) {
	fprintf(ERRS, "WARNING: Could not open /dev/null!\n");
	fail = 1;
    }
    if (stderrfd == -1) {
	fprintf(ERRS, "WARNING: Could not dup stderr!\n");
	fail = 1;
    }
    if (!fail && localDebug) {
	printf("Dup successful\n");
    }
}

void redirect_stderr_to_null(void)
{
    if ((nullfd != -1) && (stderrfd != -1)) {
	dup2(nullfd, STDERR_FILENO);
    }
}

void restore_stderr(void)
{
    if ((nullfd != -1) && (stderrfd != -1)) {
	dup2(stderrfd, STDERR_FILENO);
    }
}

/*! relpath generates a relative path to the file specifief by origPath
    from the file specified by filename.  Both must be relative paths to
    begin with.
 */
char *relpath(char *origPath, char *filename)
{
    char *iter = filename;
    int pathparts = 0;

    while (*iter) {
	if (*iter == '/') pathparts++;
	iter++;
    }

    iter = malloc((strlen(origPath) + (3 * pathparts) + 1) * sizeof(char));
    iter[0] = '\0';
    while (pathparts) {
	strcat(iter, "../");
	pathparts--;
    }
    strcat(iter, origPath);

if (debug_relpath) { printf("OP: %s\nFN: %s\nRP: %s\n", origPath, filename, iter); }

    return iter;
}

/*! Fixpath fixes a path by removing double slashes and trailing slashes
    from a path.

    NOTE: This function has a side-effect.  The string passed via the
    name argument is modified in place.  Since it can only shrink, not grow,
    it wasn't worth running the potential for memory leaks to avoid this
    side effect.
 */

char *fixpath(char *name)
{
    char *iter = name, *iterb;
    char *lastc = iter;
    int abspath = 0;

    if (name && (*name == '/')) {
	abspath = 1;
	// printf("ABSPATH\n");
    // } else {
	// printf("RELPATH\n");
    }

    /* Remove double slashes */
    while (iter && *iter) {
	if (*iter == '/' && *lastc == '/') {
	    for (iterb = iter; *iterb; iterb++) {
		*(iterb-1) = *iterb;
	    }
	}

	lastc = iter;
	iter++;
    }

    /* Remove trailing slash */
    if (*lastc == '/') *lastc = '\0';

    if (abspath) {
	bcopy(name, name+1, strlen(name));
	name[0] = '/';
    }

    return name;
}

int has_target(xmlNode *node)
{
    char *target = proptext("target", node->properties);
    char *retarget = proptext("retarget", node->properties);

    if (retarget && !strcasecmp(retarget, "yes")) return 0;
    if (target && strlen(target)) return 1;

    return 0;
}

// Thread-safe dirname function.  Yuck.
char *ts_dirname(char *path)
{
    static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
    char *orig, *copy, *junk;

    while (pthread_mutex_lock(&mylock));

    junk = malloc((strlen(path) + 1) * sizeof(char));
    strcpy(junk, path);
    orig = dirname(junk);
    copy = malloc((strlen(orig) + 1) * sizeof(char));
    strcpy(copy, orig);
    free(junk);
    while (pthread_mutex_unlock(&mylock));

    return copy;
}

// Thread-safe basename function.  Yuck.
char *ts_basename(char *path)
{
    static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
    char *orig, *copy, *junk;

    while (pthread_mutex_lock(&mylock));

    junk = malloc((strlen(path) + 1) * sizeof(char));
    strcpy(junk, path);
    orig = basename(junk);
    copy = malloc((strlen(orig) + 1) * sizeof(char));
    strcpy(copy, orig);
    free(junk);
    while (pthread_mutex_unlock(&mylock));

    return copy;
}

int exists(char *filename)
{
    FILE *fp;

    if (!(fp = fopen(filename, "r"))) return 0;
    fclose(fp);
    return 1;
}

// #if ((LIBXML_VERSION < 20609) || ((LIBXML_VERSION == 20609) && !defined(__APPLE__)))
// #include "workaround.c"
// #endif

