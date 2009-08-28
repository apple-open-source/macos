
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/param.h>
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

#ifdef USE_STRCOMPAT
#include "strcompat.h"
#endif

#ifndef C99
	#define FMT_SIZE_T "%zu"
#else
	#define FMT_SIZE_T "%lu"
#endif

typedef struct _xrefnode {
    char *filename;
    char *xref;
    char *title;
    int fromseed;
    struct _xrefnode *left, *right, *dup;
} *xrefnode_t;

struct nodelistitem
{
    xmlNode *node;
    struct nodelistitem *next;
    struct nodelistitem *prev;
};

typedef struct fileref
{
	char name[MAXPATHLEN];
	struct fileref *next;
	struct fileref *threadnext;
} *fileref_t;

struct nodelistitem *nodelist(char *name, xmlNode *root);

xrefnode_t nodehead = NULL;

int debugging = 0;
int filedebug = 0;
int writedebug = 0;
int debug_relpath = 0;
int warn_each = 0;

int debug_reparent = 0;

/* Set quick_test to 1 to gather and parse and write without actually
   resolving, or 2 to not do much of anything. */
int quick_test = 0;

/* Set nowrite to 1 to disable writes, 2 to write to a temp file but not rename over anything. */
int nowrite = 0;

int stderrfd = -1;
int nullfd = -1;

char *xmlNodeGetRawString(htmlDocPtr dp, xmlNode *node, int whatever);
char *resolve(char *xref, char *filename, int *retarget, char **frametgt);
static void *resolve_main(void *ref);
void setup_redirection(void);

int resolved = 0, unresolved = 0, nfiles = 0, broken = 0, plain = 0;

// #define NTHREADS 8
#define MAXTHREADS 16
#define ERRS stderr

int thread_exit[MAXTHREADS];
int thread_processed_files[MAXTHREADS];
int nthreads = 2;
int duplicates = 0;
int seeding_in_progress = 0;

// #define MAX(a, b) ((a<b) ? b : a)

void redirect_stderr_to_null(void);
void restore_stderr(void);
void writeXRefFile(char *filename);
int readXRefFile(char *filename);
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
void printusage();
int printNodeRange(xmlNode *start, xmlNode *end);

#define MAXSEEDFILES 1024
#define MAXEXTREFS 1024

int nextrefs = 0;
char *extrefs[MAXEXTREFS];
fileref_t threadfiles[MAXTHREADS];
char *progname;

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
    char *directory;
    char *xref_output_file = NULL;
    char *seedfiles[MAXSEEDFILES];
    int nseedfiles = 0;

    setup_redirection();

    if (argc < 1) {
	fprintf(ERRS, "resolveLinks: No arguments given.\n");
	printusage();
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

#ifdef LIBXML_TEST_VERSION
    LIBXML_TEST_VERSION;
#endif

    if (argc < 2) {
	// fprintf(ERRS, "Usage: resolveLinks <directory>\n");
	printusage();
	exit(-1);
    }
    progname = argv[0];

    {
	int temp, debug_flags;
	while ((temp = getopt(argc, argv, "d:s:t:r:"))) {
		if (temp == -1) break;
		switch(temp) {
			case 'r':
				if (nextrefs > MAXEXTREFS) {
					fprintf(ERRS, "Maximum number of external reference anchor types (%d) exceeded.  Extra files ignored.\n", MAXSEEDFILES);
				} else {
					extrefs[nextrefs++] = optarg;
					// printf("EXT REF: %s\n", extrefs[nextrefs-1]);
				}
				break;
			case 's':
				if (nseedfiles > MAXSEEDFILES) {
					fprintf(ERRS, "Maximum number of seed files (%d) exceeded.  Extra files ignored.\n", MAXSEEDFILES);
				} else {
					seedfiles[nseedfiles++] = optarg;
				}
				break;
			case 'x':
				xref_output_file = optarg;
				break;
			case 't':
				nthreads = atoi(optarg);
				break;
			case 'd':
				debug_flags = atoi(optarg);
				debugging =      ((debug_flags &  1) != 0);
				filedebug =      ((debug_flags &  2) != 0);
				writedebug =     ((debug_flags &  4) != 0);
				debug_relpath =  ((debug_flags &  8) != 0);
				debug_reparent = ((debug_flags & 16) != 0);
				warn_each =      ((debug_flags & 32) != 0);

				nthreads = 0; // Disable multithreaded processing for debugging.

				break;
			case 'n':
				nowrite = 1;
				break;
			case ':':
			case '?':
			default:
				printusage(); exit(-1);
		}
	}
	directory = argv[optind];
    // *argc = *argc - optind;
    // *argv = *argv + optind;
    }
    if (debug_reparent) nthreads = 0; // Disable multithreaded processing for debugging.


    // printf("Number of seed files: %d\nNumber of threads: %d\nDirectory: %s\n", nseedfiles, nthreads, directory);
    // { int i; for (i=0; i<nseedfiles; i++) { printf("Seed file %d: %s\n", i, seedfiles[i]); }}

    cwd = getcwd(NULL, 0);
    if (chdir(directory)) {
	// if (errno == ENOTDIR) {
		perror(directory);
		// fprintf(ERRS, "Usage: resolveLinks <directory> [nthreads]\n");
		printusage();
	// } else {
		// perror(directory);
		// // fprintf(ERRS, "Usage: resolveLinks <directory> [nthreads]\n");
		// printusage();
	// }
	exit(-1);
    }
    chdir(cwd);

    // if (argc == 3) { nthreads = atoi(argv[2]); }

    if (nseedfiles) {
	int i;

	seeding_in_progress = 1;
	printf("Loading seed files.\n");
	for (i=0; i<nseedfiles; i++) {
		// printf("Seed file %d: %s\n", i, seedfiles[i]);
		int ret = readXRefFile(seedfiles[i]);
		if (!ret) {
			fprintf(ERRS, "%s: xref seed file %s missing or malformed.\n", progname, seedfiles[i]);
		}
	}

	seeding_in_progress = 0;
    }
    duplicates = 0;

    {
      chdir(directory);
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
	    fprintf(ERRS, "error: resolveLinks: could not parse XML file %s\n", filename);
	    fprintf(ERRS, "CWD is %s\n", getcwd(NULL, 0));
	    exit(-1);
	}
	root = xmlDocGetRootElement(dp);
	restore_stderr();

	if (quick_test < 2) gatherXRefs(root, dp, filename);
	xmlFreeDoc(dp);
      }


      printf("\nWriting xref file\n");
      writeXRefFile(xref_output_file);
    } else {
	quick_test = 1;
    }

    if (nowrite == 1) exit(0); // We're done.

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
		uintptr_t temp_i = i;
		pthread_attr_t *attr = NULL;
		thread_exists[i] = 1;
		if (pthread_create(&threads[i], attr, resolve_main, (void *)temp_i)) {
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
	if (!nthreads) printf("\nResolving links (single threaded)\n");
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

    if (debugging) printf("Thread %d spawned.\n", (int)(uintptr_t)ref); // Intentional truncation to integer.

// sleep(5*((int)ref));

    ret = resolve_mainsub((int)(uintptr_t)ref);
    thread_exit[(int)(uintptr_t)ref] = ret;
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


    files = threadfiles[pos];
    thread_processed_files[pos] = 0;
#ifndef OLD_LIBXML
    int ks = 1;
#endif

    for (curfile = files; curfile; curfile = curfile->threadnext) {
	thread_processed_files[pos]++;

	filename = curfile->name;

	if (debugging || writedebug || debug_reparent) printf("READING FILE: %s\n", filename);

    // if (nthreads > 0) {
	// sprintf(tempname, "/tmp/resolveLinks.%d.%d", getpid(), (int)pthread_self());
    // } else {
	snprintf(tempname, MAXNAMLEN, "%s-temp%d-%d", filename, getpid(), pos);
    // }

	redirect_stderr_to_null();
#ifdef OLD_LIBXML
	if (!(dp = htmlParseFile(filename, NULL)))
#else
	ctxt = htmlCreateFileParserCtxt(filename, NULL);

	if (!ctxt) { fprintf(ERRS, "error: could not create context\n"); exit(-1); }

	// if (!(dp = htmlCtxtReadFile(ctxt, filename, "", options)))
	ctxt->options = options;
	ctxt->space = &ks;
	ctxt->sax->ignorableWhitespace = NULL;
	ctxt->keepBlanks = 1;
	printf("PRE:\n");
	printf("SAX: %p IWS: %p\n", ctxt->sax, ctxt->sax ? ctxt->sax->ignorableWhitespace : 0);
	printf("KB: %d\n", ctxt->keepBlanks);
	htmlParseDocument(ctxt);
	dp = ctxt->myDoc;
	printf("POST: \n");
	printf("SAX: %p IWS: %p\n", ctxt->sax, ctxt->sax ? ctxt->sax->ignorableWhitespace : 0);
	printf("KB: %d\n", ctxt->keepBlanks);
	if (!dp)
#endif
	{
	    restore_stderr();
	    fprintf(ERRS, "error: resolveLinks: could not parse XML file\n");
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

	if (debugging || writedebug || debug_reparent) printf("WRITING FILE: %s\n", filename);
	
	writeFile(root, dp, tempname);
	// printf("TREE:\n");
	// writeFile_sub(root, dp, stdout, 1);
	// printf("TREEDONE.\n");
	// exit(-1);

	xmlFreeDoc(dp);

	if (nowrite == 2) {
		fprintf(stderr, "TEMPNAME: %s\n", tempname);
	} else if (rename(tempname, filename)) {
	    fprintf(ERRS, "error: error renaming temp file over original.\n");
	    perror("resolveLinks");
	    return -1;
	}
#ifndef OLD_LIBXML
	htmlFreeParserCtxt(ctxt);
#endif
    }

    return 0;
}


char *textmatching(char *name, xmlNode *cur, int missing_ok, int recurse);
xmlNode *nodematching(char *name, xmlNode *cur, int recurse);

/*! This function takes a line from an xref cache file, splits it into
    its components, and adds the xref into the xref tree.

 */
int addXRefFromLine(char *line)
{
    char *iter = line;
    char *xref = NULL;
    char *title = NULL;
    xrefnode_t newnode;

    if (!strcmp(line, "Cross-references seen (for debugging only)")) return -1;
    if (!strlen(line)) return -1;

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
	fprintf(ERRS, "warning: Corrupted line in xref file.\n");
	// fprintf(ERRS, "%x %x\n", xref, title);
	return 0;
    }

    if (!((newnode = malloc(sizeof(*newnode))))) {
	fprintf(ERRS, "error: Out of memory reading xref file.\n");
	exit(-1);
    }

    newnode->filename = strdup(line);
    newnode->xref = strdup(xref);
    newnode->title = strdup(title);
    newnode->left = NULL;
    newnode->right = NULL;
    newnode->dup = NULL;
    newnode->fromseed = seeding_in_progress;

    // printf("From File: Title was %s\n", title);

    if (nodehead) {
	addXRefSub(newnode, nodehead);
    } else {
	nodehead = newnode;
    }

    return 1;
}

/*! This function reads a cross-reference cache file.  This is intended
    to allow eventual incorporation of cross-references that do not live
    in the same directory (or even on the same machine.  It is currently
    unused.
 */
int readXRefFile(char *filename)
{
    FILE *fp;
    char line[4098];
    int ret = 1;

    if (!((fp = fopen(filename, "r")))) {
	return 0;
    }

    while (1) {
	if (fgets(line, 4096, fp) == NULL) break;
	if (line[strlen(line)-1] != '\n') {
		fprintf(ERRS, "warning: ridiculously long line in xref file.\n");
		ret = 0;
	} else {
		line[strlen(line)-1] = '\0';
	}
	if (!addXRefFromLine(line)) ret = 0;
    }

    fclose(fp);
    return ret;
}

/*! This function is the recursive tree walk subroutine used by
    writeXRefFile to write cross-references to a cache file.
 */
void writeXRefFileSub(xrefnode_t node, FILE *fp)
{
    if (!node) return;

    writeXRefFileSub(node->left, fp);

    //fprintf(fp, "filename=\"%s\" id=\"%s\" title=\"%s\"\n",
		// node->filename, node->xref, node->title ? node->title : "");
    fprintf(fp, "%s%c%s%c%s\n",
		node->filename, 1, node->xref, 1, node->title ? node->title : "");

    writeXRefFileSub(node->dup, fp);
    writeXRefFileSub(node->right, fp);
}

/*! This function writes a cross-reference cache file.  This is intended
    to allow eventual incorporation of cross-references that do not live
    in the same directory (or even on the same machine.  It is currently
    called, but the resulting file is not yet used.
 */
void writeXRefFile(char *filename)
{
    FILE *fp;
    char *outfile = "/tmp/xref_out";
    if (filename) outfile = filename;

    if (!((fp = fopen(outfile, "w")))) {
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

    if (!newnode->title) newnode->title = strdup("");
    // tree->fromseed = seeding_in_progress;

    if (pos < 0) {
	/* We go left */
	if (tree->left) addXRefSub(newnode, tree->left);
	else {
		tree->left = newnode;
	}
    } else if (!pos) {
	xrefnode_t iter;
	int oops = 0, drop = 0;

	for (iter = tree; iter; iter = iter->dup) {
		if (!strcmp(newnode->filename, iter->filename)) {
		    if (!strcmp(newnode->title, iter->title)) {
				// printf("Exact dup.  Dropping.\n");
				drop = 1;
				if (!newnode->fromseed) iter->fromseed = 0;
		    }
		}
		// if (iter->title) printf("TITLE FOUND: %s\n", iter->title);
		if (debugging) printf("Dup: %s %s %s == %s %s %s\n", iter->title, iter->filename,
			iter->xref, newnode->title, newnode->filename, newnode->xref);
		if (!iter->fromseed) {
			oops = 1;
		};
		if (!iter->dup) {
			// end of the chain.
			if (!drop) {
				iter->dup = newnode;
			}
			if (oops) {
				duplicates++;
			}
			break;
		}
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
	printf("warning: addXRef called on null node\n");
    }

    pt = proptext("name", node->properties);
    if (!pt) {
	printf("warning: addXRef called on anchor with no name property\n");
    }

    if (debugging) {printf("STRL " FMT_SIZE_T "\n",  strlen(pt)); fflush(stdout);}
    tempstring = (bufptr = malloc((strlen(pt)+1) * sizeof(char)));
    strlcpy(tempstring, pt, (strlen(pt)+1));

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
		newnode->dup = NULL;
		newnode->fromseed = seeding_in_progress;

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
    if (node->name && !strcmp((char *)node->name, "a")) {
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
    char *retval = getLogicalPath(text);
    if (debug_reparent) {
	printf("isSOLR: %s: %d\n", text, retval ? 1 : 0);
    }
    return retval ? 1 : 0;
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
  while (node) {

    if (node->name && !strcmp((char *)node->name, "comment")) {
	if (debugging) { printf("comment: \"%s\"\n", node->content); }
	if (isStartOfLinkRequest((char *)node->content)) {
		xmlNode *close = NULL;
		struct nodelistitem *nodelisthead = NULL;
		struct nodelistitem *nodelistiterator = NULL;

		if (debugging || debug_reparent) printf("SOLR\n");
		if (node->next) {
			/* The node list is in reverse order of match. Skip to the last node.
			   Later, we'll work backwards so that we find the first link end
			   comment.
			 */
			nodelisthead = nodelist("comment", node->next);
			while (nodelisthead && nodelisthead->next) nodelisthead = nodelisthead->next;
		}
		nodelistiterator = nodelisthead;

		/* Iterate backwards. */
		while (nodelistiterator && !close) {

			if (debugging || debug_reparent) printf("NODE: %s\n", nodelistiterator->node->name);
			if (debugging || debug_reparent) printf("NODETEXT: %s\nEONODETEXT\n", nodelistiterator->node->content ? (char *)nodelistiterator->node->content : "(null)");

			if (nodelistiterator->node->name && !strcmp((char *)nodelistiterator->node->name, "comment") &&
					isEndOfLinkRequest((char *)nodelistiterator->node->content)) {
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
			if (debug_reparent) {
				printf("Printing nodes between start and end.\n");
				printNodeRange(node, close);
				printf("Done printing nodes between start and end.\n");
			}

			/* Link Request. */
			char *lp = getLogicalPath((char *)node->content);
			char *frametgt = getTargetAttFromString((char *)node->content);
			int retarget = (!frametgt || !strlen(frametgt));
			char *target = resolve(lp, filename, &retarget, &frametgt);
			if (debugging) printf("RETARGET SHOULD HAVE BEEN %d (frametgt is %p)\n", retarget, frametgt);
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
					/* Just change the end tag to an empty text container.  It's safer. */
					close->name=(unsigned char *)strdup("text");
					close->content=(unsigned char *)strdup("");
					close->type=XML_TEXT_NODE;

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

					node->name = (unsigned char *)strdup("a");
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
					if(node->children != NULL){
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
                            fprintf(stderr, "warning: reparenting failed.\n");
                        }
					}
					else{
					   // ERROR: could happen when the @link is missing the second (text) element after the apiref!
                        fprintf(stderr, "error: some @link is missing the second field; see resulting file %s to find out which one.\n", filename);
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
				free(target);
			} else {
				/*                char    *full_filename = malloc(PATH_MAX * sizeof(char));
				char    *printed_filename = full_filename;

				// Let's get full file name to display error messages containing full path to file
				if(!realpath(filename, full_filename))
					printed_filename = filename;
				if (warn_each)
					fprintf(ERRS, "\n%s:0: error: unable to resolve link %s.\n", printed_filename, lp);
				free(full_filename);*/
				unresolved++;
			}
		} else {
        		char *full_filename = malloc(PATH_MAX * sizeof(char));
        		char *printed_filename = full_filename;

			// Let's get full file name to display error messages containing full path to file
			if (!realpath(filename, full_filename))
				printed_filename = filename;
			fprintf(ERRS, "\n%s:0: error: broken link.  No closing link request comment found.\n", printed_filename);
			free(full_filename);
			broken++;
		}
	}
    } else if (node->name && !strcmp((char *)node->name, "a")) {
		/* Handle the already-live link */
		int retarget = (!has_target(node));
		char *lp = proptext("logicalPath", node->properties);
		// char *name = proptext("name", node->properties);
		char *href = proptext("href", node->properties);

		if (lp && href) {
			char *frametgt = getTargetAttFromString((char *)node->content);
			char *target = resolve(lp, filename, &retarget, &frametgt);

			if (target) {
				if (debugging) printf("FOUND!\n");
				addAttribute(node, "href", target);
				resolved++;
				free(target);
			} else {
				xmlNode *iter = node->children, *tailnode;

				/* We couldn't resolve this live link.
				   turn it back into a comment, inserting
				   a close comment tag after its last child
				   and reparenting its children as children
				   of its parent (as its "next" node). */
/*
                char    *full_filename = malloc(PATH_MAX * sizeof(char));
                char    *printed_filename = full_filename;

                // Let's get full file name to display error messages containing full path to file
                if(!realpath(filename, full_filename))
                    printed_filename = filename;
                    if (warn_each) fprintf(ERRS, "%s:0: error: unable to resolve link %s.\n", printed_filename, lp);
                free(full_filename);*/
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

				node->name = (unsigned char *)strdup("comment");
				node->type = XML_COMMENT_NODE;
				tailnode->name = (unsigned char *)strdup("comment");
				tailnode->type = XML_COMMENT_NODE;
				tailnode->content = (unsigned char *)strdup(" /a ");
				node->content = (unsigned char *)propString(node);
				if (debugging) printf("PS: \"%s\"\n", node->content);
			}
		} else {
			if (debugging) printf("Not a logicalPath link.  Skipping.\n");
			plain++;
		}
    }
    else{
	if (debugging) { printf("%s: \"%s\"\n", node->name, node->content); }
    }

    resolveLinks(node->children, dp, filename);
    // resolveLinks(node->next, dp, filename);
    node = node->next;
  }
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
	fprintf(ERRS, "error: could not open file %s for writing\n", filename);
	exit(-1);
    }

    writeFile_sub(node, dp, fp, 0);

    fclose(fp);
#else
    if (!htmlGetMetaEncoding(dp)) {
	htmlSetMetaEncoding(dp, (unsigned char *)"ascii");
    }
    // printf("META ENCODING PRE: %s\n", htmlGetMetaEncoding(dp));

    // BUGBUGBUG: libxml2 actually writes HTML into the encoding field in the
    // meta tag, resulting in an unreadable HTML file.
    // int ret = htmlSaveFileEnc(filename, dp, "HTML");

    int ret = htmlSaveFile(filename, dp);
    // printf("META ENCODING: %s\n", htmlGetMetaEncoding(dp));

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
		len += strlen((char *)prop->name) + strlen((char *)prop->children->content) + 4;
	}
	prop = prop->next;
    }
    len += 5;
    propstring = malloc(len * sizeof(char));

    strlcpy(propstring, "a", len);

    prop = node->properties;
    while (prop) {
	if (prop->children) {
		strlcat(propstring, " ", len);
		strlcat(propstring, (char *)prop->name, len);
		strlcat(propstring, "=\"", len);
		strlcat(propstring, (char *)prop->children->content, len);
		strlcat(propstring, "\"", len);
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
      if (!strcmp((char *)node->name, "text")) {
	fprintf(fp, "%s", xmlNodeGetRawString(dp, node, 0));
      } else if (!strcmp((char *)node->name, "comment")) {
	fprintf(fp, "<!--%s-->", node->content);
      } else {
	fprintf(fp, "<%s", node->name);
	writeProps(node, fp);
	fprintf(fp, ">");
      }
    }

    writeFile_sub(node->children, dp, fp, 0);

    if (strcmp((char *)node->name, "text") && strcmp((char *)node->name, "comment")) {
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
	if (!strcasecmp((char *)cur->name, name)) break;
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
		ret = (char *)cur->children->content;
    } else if (!strcasecmp(name, "text")) {
		ret = (char *)cur->content;
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
	if (!strcasecmp((char *)prop->name, name)) {
		if (prop->children && prop->children->content) {
			return (char *)prop->children->content;
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
	if (!strcasecmp((char *)prop->name, name)) {
		if (prop->children && prop->children->content) {
			return atoi((char *)prop->children->content);
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

    if (!strncasecmp(ptr, "target", 6)) {
	char *startptr; int count=0;

	if (debugging) printf("STARTTARGET\n");

	ptr += 6;
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

    return (char *)xmlNodeListGetRawString(dp, &copynode, whatever);
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
	fprintf(ERRS, "warning: malformed apple_ref has less than three parts.\n");
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
    apple_ref bit to the external ref specified by <code>extref</code>.
 */
char *refRefChange(char *ref, char *extref)
{
    refparts_t rp = getrefparts(ref);
    char *langpart, *rest, *retval;
    int length;

    if (!rp) return NULL;
    langpart = rp->langpart;
    rest = rp->rest;

    // printf("LANGPART: \"%s\"\n", rp->langpart);

    /* length = "//" + refpart + "/" + "C" + "/" + rest + "\0" */
    length = 2 + strlen(extref) + 1 + strlen(langpart) + 1 + strlen(rest) + 1;

    retval = malloc((length + 1) * sizeof(char));
    snprintf(retval, (length + 1), "//%s/%s/%s", extref, langpart, rest);

    return retval;
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
    length = 2 + strlen(refpart) + 1 + strlen(lang) + 1 + strlen(rest) + 1;

    retval = malloc((length + 1) * sizeof(char));
    snprintf(retval, (length + 1), "//%s/%s/%s", refpart, lang, rest);

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
    char *indexpath = NULL;

    // int len = (strlen(filename)+strlen(offset)+2) * sizeof(char);

if (debugging) printf("RETARGET (INITIAL): %d\n", retarget);

    asprintf(&indexpath, "%s/index.html", updir);
    if (retarget && (!strcmp(base, "index.html") || !strcmp(base, "CompositePage.html"))) {
	if (debugging) printf("Going to an index.html file.  Not retargetting.\n");
	if (debugging) printf("FILENAME: %s\nOFFSET: %s\n", filename, offset);
	retarget = 0;
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
	indexpath = NULL;
    }

    if (retarget && !exists(indexpath)) {
	fprintf(stderr, "\nNo index found at %s.\n", indexpath);
	if (debugging) fprintf(stderr, "DIR: %s\nBASE: %s\nUPDIR: %s\nUPBASE: %s\nORIG_FILENAME: %s\n", dir, base, updir, upbase, filename);
	retarget = 0;
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
	indexpath = NULL;
    }

    // if (retarget) {
	// len = strlen(indexpath) + 1 /*?*/ + strlen(upbase) + 1 /*/*/ + strlen(base) + 1 /*#*/ + strlen(offset) + 1 /*NULL*/;
    // }

    // buf = malloc(len);

    // if (!buf) return "BROKEN";

if (debugging) printf("RETARGET: %d\n", retarget);

    if (retarget) {
	asprintf(&buf, "%s?%s/%s#%s", indexpath, upbase, base, offset);
	free(dir);
	free(base);
	free(updir);
	free(upbase);
	free(indexpath);
    } else {
	asprintf(&buf, "%s#%s", filename, offset);
	// strcpy(buf, filename);
	// strcat(buf, "#");
	// strcat(buf, offset);
    }

    return buf;
#else
    return strdup(filename);
#endif
}

int matchingPathParts(char *a, char *b, int *isbelowme)
{
    char *aiter, *biter;
    int found;

    /* Remove leading ./ and any extra slashes after it. */
    found = 0;
    for (aiter=a; *aiter; ) {
	if (*aiter == '.' && *(aiter+1) == '/') {
		aiter+=2; found = 1;
	} else if (found && *aiter == '/') {
		aiter++;
	} else break;
    }
    found = 0;
    for (biter=b; *biter; ) {
	if (*biter == '.' && *(biter+1) == '/') {
		biter+=2; found = 1;
	} else if (found && *biter == '/') {
		biter++;
	} else break;
    }

    found = 0;
    for (; ; aiter++,biter++)
    {
	// printf("CMP: %c %c\n", *aiter, *biter);
	if (*aiter == '/' && !(*biter)) {
		char *aiterx; int moreslashes = 0;
		for (aiterx = aiter + 1; *aiterx; aiterx++) {
			if (*aiterx == '/') moreslashes = 1;
		}
		*isbelowme = !moreslashes;
		// printf("SlashEnd-1\n");
		found++;
	}
	if (!(*aiter) && *biter == '/' ) {
		char *biterx; int moreslashes = 0;
		for (biterx = biter + 1; *biterx; biterx++) {
			if (*biterx == '/') moreslashes = 1;
		}
		*isbelowme = !moreslashes;
		// printf("SlashEnd-2\n");
		found++;
	}
	if (*aiter != *biter) {
		// printf("NE\n");
		break;
	}
	if (*aiter == '/') {
		// printf("Slash\n");
		found++;
	}
	if (!(*aiter && *biter)) break;
    }

    // printf("Compare: %s to %s: %d\n", a, b, found);
    return found;
}


/*! This function recursively searches for an xref in the xref tree,
    returning the filename and anchor within that file, concatenated with
    makeurl.
 */
char *searchref(char *xref, xrefnode_t tree, int retarget, char *basepath)
{
    int pos;

    if (!tree) return NULL;

    pos = strcmp(xref, tree->xref);

    if (!pos) {
	/* We found one.  Find the closest match. */
	xrefnode_t iter, maxmatchnode = NULL, maxmatchnode_nocache = NULL;
	int maxmatch = -1;
	int maxmatch_nocache = -1;

	for (iter = tree; iter; iter = iter->dup) {
		int belowme;
		int match = matchingPathParts(iter->filename, basepath, &belowme);

		// printf("Checking %s: ", iter->filename);
		if (match > maxmatch || (match == maxmatch && belowme)) {
			// printf("%d is better than %d.  ", match, maxmatch);
			maxmatch = match;
			maxmatchnode = iter;
		}
		if (!iter->fromseed) {
			if (match > maxmatch_nocache || (match == maxmatch_nocache && belowme)) {
				// printf("Ooh.  a real one!  ");
				maxmatch_nocache = match;
				maxmatchnode_nocache = iter;
			}
		}
		// printf("\n");
	}
	if (maxmatchnode) tree = maxmatchnode;
	if (maxmatchnode_nocache) tree = maxmatchnode_nocache;

	return makeurl(tree->filename, tree->xref, retarget);
    } else if (pos < 0) {
        /* We go left */
        return searchref(xref, tree->left, retarget, basepath);
    } else {
        /* We go right */
        return searchref(xref, tree->right, retarget, basepath);
    }
}

/*! This function attempts to resolve a space-delimited list of
    cross-references, allowing language fallback (c++ to C, etc.),
    and returns the URL associated with it.
 */
char *resolve(char *xref, char *filename, int *retarget, char **frametgt)
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
		char *altLangRef1 = NULL;
		char *altLangRef2 = NULL;
		char *altLangRef3 = NULL;
		refparts_t rp;
		int i;

		if (debugging) { printf("SEARCHING FOR \"%s\"\n", curref); }

		for (i=-1; i<=nextrefs; i++) {
		    char *newcurref = NULL;
		    char *refpart;

		    if ((rp = getrefparts(curref))) {
			if (i == nextrefs) {
				refpart = "apple_ref";
			} else if (i == -1) {
				refpart = strdup(rp->refpart);
			} else {
				refpart = extrefs[i];
			}
			newcurref = refRefChange(curref, refpart);

			if (!strcmp(rp->langpart, "cpp") ||
			    !strcmp(rp->langpart, "occ") ||
			    !strcmp(rp->langpart, "C")) {
				altLangRef1 = refLangChange(newcurref, "c");
				altLangRef2 = refLangChange(newcurref, "cpp");
				altLangRef3 = refLangChange(newcurref, "occ");
			}
		    } else {
			newcurref = curref;
		    }

		    if (altLangRef1 && debugging) { printf("ALSO SEARCHING FOR \"%s\"\n", altLangRef1); }
		    if (altLangRef2 && debugging) { printf("ALSO SEARCHING FOR \"%s\"\n", altLangRef2); }
		    if (altLangRef3 && debugging) { printf("ALSO SEARCHING FOR \"%s\"\n", altLangRef3); }

		    target = searchref(newcurref, nodehead, *retarget, dirname(filename));
		    if (!target && altLangRef1) target = searchref(altLangRef1, nodehead, *retarget, dirname(filename));
		    if (!target && altLangRef2) target = searchref(altLangRef2, nodehead, *retarget, dirname(filename));
		    if (!target && altLangRef3) target = searchref(altLangRef3, nodehead, *retarget, dirname(filename));
		    if (target) break;
		}

		if (target) {
			if (debugging) printf("Mapping %s to %s\n", xref, target);
			if(strlen(target) > 7 && (strncmp(target, "file://", 7) == 0 || strncmp(target, "http://", 7) == 0)){
				*retarget = 1;
				*frametgt = "_top";
				return target;
			} else {
				return relpath(target, filename);
			}
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

    if (cur->name && !strcmp((char *)cur->name, name)) {
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
    myprop->name = (unsigned char *)strdup(attname);
    myprop->children = mypropnode;
    myprop->parent = node;
    myprop->doc = node->doc;
    mypropnode->type = XML_TEXT_NODE;
    mypropnode->name = (unsigned char *)strdup("text");
    mypropnode->content = (unsigned char *)strdup(attstring);
    mypropnode->parent = (void *)&myprop;
    mypropnode->doc = node->doc;

    properties = node->properties;
    lastprop = properties;
    for (properties = node->properties; properties; properties = properties->next) {
	if (delprop) {
		// free(delprop); /* @@@ */
		delprop = NULL;
	}
	if (!strcasecmp((char *)properties->name, attname)) {
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

		if (debugging || localDebug) printf("CURPATH: \"%s\" NP: %p\n", curPath, newpath);

		strlcpy(newpath, curPath, MAXNAMLEN);
		strlcat(newpath, "/", MAXNAMLEN);
		strlcat(newpath, entp->d_name, MAXNAMLEN);

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
		strlcpy(newent->name, curPath, MAXPATHLEN);
		strlcat(newent->name, "/", MAXPATHLEN);
		strlcat(newent->name, entp->d_name, MAXPATHLEN);

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

    if (debugging) printf("LENGTHS: " FMT_SIZE_T " " FMT_SIZE_T " " FMT_SIZE_T "\n", strlen(string), strlen(tail), strlen(string)-strlen(tail));

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
    printf("    duplicates: %3d\n", duplicates);
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
	printf("warning: freeing region not allocated by db_malloc\n");
	free(ptr);
    } else {
	intptr--;
	tail += (*intptr);
	intcheckptr = (int *)tail;
	// printf("Length was %d\n", *intptr);
	if (*intcheckptr != 0xdeadbeef) {
		printf("warning: region scribbled off end\n");
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
	fprintf(ERRS, "warning: Could not open /dev/null!\n");
	fail = 1;
    }
    if (stderrfd == -1) {
	fprintf(ERRS, "warning: Could not dup stderr!\n");
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

int *partsOfPath(char *path)
{
    /* The number of path parts in a path can't be more than
       half the number of characters because we don't include
       empty path parts. */
    int *list = malloc(((strlen(path) / 2) + 1) * sizeof(int));
    int listpos = 0;
    int pos = 0;

    while (path && path[pos] == '/') pos++;
    while (path && path[pos]) {
	if (path[pos] == '/') {
		while (path[pos] == '/') pos++;

		if (path[pos] == '\0') break;
		list[listpos++] = pos;
	}
	pos++;
    }

    list[listpos] = -1;
    return list;
}

char *malloccopypart(char *source, int start, int length)
{
	char *ret = malloc((length + 1) * sizeof(char));
	strncpy(ret, &source[start], length);
	ret[length] = '\0';
	return ret;
}

/*! relpath generates a relative path to the file specifief by target
    from the file specified by fromFile.  Both must be relative paths to
    begin with.
 */
char *relpath(char *target, char *fromFile)
{
    char *iter = fromFile;
    int pathparts = 0;
    size_t alloc_len;

    int *base_pathparts = partsOfPath(fromFile);
    int *target_pathparts = partsOfPath(target);
    int i;

    int startpos_in_target = 0;

    for (i=0; ((base_pathparts[i] != -1) && (target_pathparts[i] != -1)); i++) {
	int start_of_base = base_pathparts[i];
	int start_of_target = target_pathparts[i];
	int end_of_base = base_pathparts[i+1] == -1 ? strlen(fromFile) : base_pathparts[i+1];
	int end_of_target = base_pathparts[i+1] == -1 ? strlen(target) : base_pathparts[i+1];
	char *basepart, *targetpart;

	/* Sadly, asnprintf isn't widely available. */
	basepart = malloccopypart(fromFile, start_of_base, end_of_base - start_of_base);
	targetpart = malloccopypart(target, start_of_target, end_of_target - start_of_target);

	while (basepart[strlen(basepart)-1] == '/') basepart[strlen(basepart)-1] = '\0';
	while (targetpart[strlen(targetpart)-1] == '/') targetpart[strlen(targetpart)-1] = '\0';

	startpos_in_target = start_of_target;

	if (strcmp(basepart, targetpart)) {
		int j;
		for (j = i; base_pathparts[j] != -1; j++); // empty loop

		pathparts = j - i - 1; // Subtract 1 for filename.
		break;
	}

	free(basepart);
	free(targetpart);
    }
    free(base_pathparts);
    free(target_pathparts);

    // while (*iter) {
	// if (*iter == '/') pathparts++;
	// iter++;
    // }

    alloc_len = ((strlen(target) + (3 * pathparts) + 1) * sizeof(char));
    iter = malloc(alloc_len);
    iter[0] = '\0';
    while (pathparts) {
	strlcat(iter, "../", alloc_len);
	pathparts--;
    }
    strlcat(iter, &target[startpos_in_target], alloc_len);

if (debug_relpath) { printf("OP: %s\nFN: %s\nRP: %s\n", target, fromFile, iter); }

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
    char *orig = NULL, *copy = NULL, *junk = NULL;

    while (pthread_mutex_lock(&mylock));

    // junk = malloc((strlen(path) + 1) * sizeof(char));
    // strcpy(junk, path);
    asprintf(&junk, "%s", path);
    orig = dirname(junk);
    // copy = malloc((strlen(orig) + 1) * sizeof(char));
    // strcpy(copy, orig);
    asprintf(&copy, "%s", orig);
    free(junk);
    while (pthread_mutex_unlock(&mylock));

    return copy;
}

// Thread-safe basename function.  Yuck.
char *ts_basename(char *path)
{
    static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
    char *orig = NULL, *copy = NULL, *junk = NULL;

    while (pthread_mutex_lock(&mylock));

    // junk = malloc((strlen(path) + 1) * sizeof(char));
    // strcpy(junk, path);
    asprintf(&junk, "%s", path);
    orig = basename(junk);
    // copy = malloc((strlen(orig) + 1) * sizeof(char));
    // strcpy(copy, orig);
    asprintf(&copy, "%s", orig);
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


int printNodeRangeSub(xmlNode *start, xmlNode *end, int leading);

int printNodeRange(xmlNode *start, xmlNode *end) {
	return printNodeRangeSub(start, end, 0);
}

int printNodeRangeSub(xmlNode *start, xmlNode *end, int leading)
{
	xmlNode *iter = start;
	char *leadspace=malloc(leading + 1);
	memset(leadspace, ' ', leading);
	leadspace[leading] = '\0';

	while (iter && iter != end) {
		printf("%sNODE: %s CONTENT: %s\n", leadspace, iter->name ? (char *) iter->name : "(no name)", iter->content ? (char *)iter->content : "(null)");
		if (iter->children) {
			printf("%sCHILDREN:\n", leadspace);
 			if (printNodeRangeSub(iter->children, end, leading + 8)) { free(leadspace); return 1; }
		}
		iter = iter->next;
	}
	if (iter == end) printf("%sNODE: %s CONTENT: %s\n", leadspace, iter->name ? (char *) iter->name : "(no name)", iter->content ? (char *)iter->content : "(null)");
	free(leadspace);
	return (iter == end);
}

void printusage()
{
    fprintf(ERRS, "Usage: resolveLinks [-s xref_seed_file] [-t nthreads] [-d debug_level] [ -r ext_ref ] <directory>\n");
    // fprintf(ERRS, "Usage: resolveLinks [options] <directory>\n");
    fprintf(ERRS, "Options are:\n");
    fprintf(ERRS, "\t-d <debug flags>                (default 0)\n");
    fprintf(ERRS, "\t-r <alternative to apple_ref>   (can be used multiple times)\n");
    fprintf(ERRS, "\t-s <seed file>                  (can be used multiple times)\n");
    fprintf(ERRS, "\t-t <number of threads>          (default 2)\n");
}

// #if ((LIBXML_VERSION < 20609) || ((LIBXML_VERSION == 20609) && !defined(__APPLE__)))
// #include "workaround.c"
// #endif

