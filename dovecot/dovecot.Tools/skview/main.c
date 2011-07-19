#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#define SK_INDEX_NAME	"fts-sk"

void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [skindex-path ...]\n",
	    prog);
    exit(1);
}

void nonnull(const void *p, const char *tag)
{
    if (p == NULL) {
	fprintf(stderr, "Out of memory:  Can't allocate for %s\n", tag);
	exit(1);
    }
}

void indent(int level)
{
    printf("%*s", level * 4, "");
}

const char *skstr(const char *path, CFStringRef str)
{
    static char buf[1024];
    const char *ret = NULL;
    
    if (CFStringGetCString(str, buf, sizeof buf, kCFStringEncodingUTF8))
	ret = buf;
    else
	fprintf(stderr, "%s: CFStringGetCString failed\n", path);
    return ret;
}

Boolean skshow(const char *path, CFTypeRef object)
{
    Boolean ok = TRUE;

    if (CFGetTypeID(object) == CFStringGetTypeID()) {
	const char *str = skstr(path, object);
	if (str)
	    printf("%s", str);
	else
	    ok = FALSE;
    } else if (CFGetTypeID(object) == CFNumberGetTypeID()) {
	CFIndex ind;
	int val;
	if (CFNumberGetValue(object, kCFNumberCFIndexType, &ind))
	    printf("%ld", ind);
	else if (CFNumberGetValue(object, kCFNumberSInt32Type, &val))
	    printf("%d", val);
	else {
	    fprintf(stderr, "%s: CFNumberGetValue failed\n", path);
	    ok = FALSE;
	}
    } else if (CFGetTypeID(object) == CFArrayGetTypeID()) {
	CFIndex ac, ai;
	putchar('[');
	ac = CFArrayGetCount(object);
	for (ai = 0; ai < ac; ai++) {
	    if (ai > 0)
		printf(", ");
	    if (!skshow(path, CFArrayGetValueAtIndex(object, ai)))
		ok = FALSE;
	}

	putchar(']');
    } else {
	fprintf(stderr, "%s: property dictionary value not a string, number, or array\n", path);
	ok = FALSE;
    }

    return ok;
}

Boolean skprop(const char *path, CFDictionaryRef properties, int level)
{
    Boolean ok = FALSE;
    
    CFIndex count = CFDictionaryGetCount(properties);
    if (count > 0) {
	CFTypeRef *keys = malloc(count * sizeof *keys);
	CFTypeRef *values = malloc(count * sizeof *values);
	nonnull(keys, "dictionary keys");
	nonnull(values, "dictionary values");
	CFDictionaryGetKeysAndValues(properties, keys, values);
	ok = TRUE;
	for (CFIndex i = 0; i < count; i++) {
	    if (CFGetTypeID(keys[i]) == CFStringGetTypeID()) {
		const char *str = skstr(path, keys[i]);
		if (str) {
		    indent(level);
		    printf("%s = ", str);
		} else {
		    ok = FALSE;
		    continue;
		}
	    } else {
		fprintf(stderr, "%s: property dictionary key not a string\n", path);
		ok = FALSE;
		continue;
	    }

	    if (!skshow(path, values[i]))
		ok = FALSE;
	    putchar('\n');
	}
	free(values);
	free(keys);
    } else {
	indent(level);
	printf("[empty properties]\n");
    }
    
    return ok;
}

Boolean skterms(const char *path, SKIndexRef skiref, SKDocumentID docid, int level)
{
    Boolean ok = TRUE;
    
    CFArrayRef termids = SKIndexCopyTermIDArrayForDocumentID(skiref, docid);
    if (termids != NULL) {
	CFIndex count = CFArrayGetCount(termids);
	for (CFIndex i = 0; i < count; i++) {
	    CFIndex termid;
	    if (CFNumberGetValue(CFArrayGetValueAtIndex(termids, i), kCFNumberCFIndexType, &termid)) {
		indent(level);
		printf("term %ld", termid);
		CFStringRef term = SKIndexCopyTermStringForTermID(skiref, termid);
		if (term != NULL) {
		    printf(" %s\n", skstr(path, term));
		    CFRelease(term);
		} else {
		    printf(" [no term available]\n");
		    ok = FALSE;
		}
	    } else {
		fprintf(stderr, "%s: term %ld for document %ld not a number", path, i, docid);
		ok = FALSE;
	    }
	}
	CFRelease(termids);
    }
    
    return ok;
}
    
Boolean skwalk(const char *path, SKIndexRef skiref, SKDocumentRef parent, int level, Boolean terms)
{
    Boolean ok = FALSE;

    SKIndexDocumentIteratorRef iter = SKIndexDocumentIteratorCreate(skiref, parent);
    if (iter != NULL) {
	SKDocumentRef doc;
	ok = TRUE;
	while ((doc = SKIndexDocumentIteratorCopyNext(iter)) != NULL) {
	    SKDocumentID docid = SKIndexGetDocumentID(skiref, doc);
	    CFStringRef docname = SKDocumentGetName(doc);
	    if (docname != NULL) {
		const char *str = skstr(path, docname);
		if (str != NULL) {
		    indent(level);
		    printf("(%ld) \"%s\"", docid, str);

		    str = NULL;
		    CFURLRef url = SKDocumentCopyURL(doc);
		    if (url != NULL) {
			str = skstr(path, CFURLGetString(url));
			CFRelease(url);
		    }
		    if (str != NULL)
			printf(" <%s>\n", str);
		    else
			printf(" [no URL available]\n");

		    CFDictionaryRef props = SKIndexCopyDocumentProperties(skiref, doc);
		    if (props) {
			if (!skprop(path, props, level + 1))
			    ok = FALSE;
			CFRelease(props);
		    } /* else not an error */
		    
		    if (terms) {
			if (!skterms(path, skiref, docid, level + 1))
			    ok = FALSE;
		    }
		    
		    if (!skwalk(path, skiref, doc, level + 1, terms))
			ok = FALSE;
		} else
		    ok = FALSE;
	    } else {
		fprintf(stderr, "%s: SKDocumentGetName failed for document %ld\n", path, docid);
		ok = FALSE;
	    }

	    CFRelease(doc);
	}
	
	CFRelease(iter);
    } else
	fprintf(stderr, "%s: SKIndexDocumentIteratorCreate on the root node failed\n", path);

    return ok;
}

Boolean skread(const char *path, int level, Boolean terms)
{
    Boolean ok = FALSE;
    
    CFURLRef index_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *) path, strlen(path), FALSE);
    if (index_url != NULL) {
	/* SK can't iterate through index names so just use the one we know */
	SKIndexRef skiref = SKIndexOpenWithURL(index_url, CFSTR(SK_INDEX_NAME), FALSE);
	if (skiref != NULL) {
	    CFIndex doc_count = SKIndexGetDocumentCount(skiref);
	    indent(level);
	    printf("document count = %ld\n", doc_count);

	    CFIndex max_docid = SKIndexGetMaximumDocumentID(skiref);
	    indent(level);
	    printf("max document id = %ld\n", max_docid);

	    CFIndex max_termid = SKIndexGetMaximumTermID(skiref);
	    indent(level);
	    printf("max term id = %ld\n", max_termid);

	    CFDictionaryRef properties = SKIndexGetAnalysisProperties(skiref);
	    if (properties) {
		indent(level);
		printf("index properties:\n");
		ok = skprop(path, properties, level + 1);
	    } else
		fprintf(stderr, "%s: SKIndexGetAnalysisProperties failed\n", path);

	    indent(level);
	    printf("documents:\n");
	    if (!skwalk(path, skiref, NULL, level + 1, terms))
		ok = FALSE;
	    SKIndexClose(skiref);
	} else
	    fprintf(stderr, "%s: SKIndexOpenWithURL failed\n", path);

	CFRelease(index_url);
    }
    
    return ok;
}

Boolean skadm(const char *path, Boolean terms)
{
    Boolean ok = FALSE;
    unsigned int frag = 1;

    struct stat stbuf;
    if (stat(path, &stbuf) == 0) {
	if (S_ISDIR(stbuf.st_mode)) {
	    char newpath[PATH_MAX];
	    snprintf(newpath, sizeof newpath, "%s/dovecot.skindex", path);
	    ok = skadm(newpath, terms);

	    for (;;) {
		snprintf(newpath, sizeof newpath, "%s/dovecot.skindex-%u", path, frag);
		if (stat(newpath, &stbuf) < 0) {
		    if (errno != ENOENT) {
			perror(newpath);
			ok = FALSE;
		    }
		    break;
		}
		if (!skadm(newpath, terms))
		    ok = FALSE;
		++frag;
	    }
	} else {
	    printf("%s:\n", path);
	    indent(1);
	    printf("size = %llu\n", (unsigned long long) stbuf.st_size);
	    ok = skread(path, 1, terms);
	}
    } else
	perror(path);

    return ok;
}

int main (int argc, const char *argv[])
{
    Boolean terms = FALSE;
    Boolean ok = TRUE;
    int arg = 1;
    
    if (argc > 1 && strcmp(argv[1], "-t") == 0) {
	terms = TRUE;
	arg = 2;
    }
    
    while (arg < argc) {
	if (!skadm(argv[arg], terms))
	    ok = FALSE;
	putchar('\n');
	++arg;
    }

    return !ok;
}
