#include <libc.h>
#include <string.h>
#include "paths.h"

typedef struct {
    unsigned int capacity;
    unsigned int length;
    char ** elements;
} patharray_t;

patharray_t * patharray_create(void);
void patharray_free(patharray_t * patharray);
int patharray_append(patharray_t * patharray, char * string,
    int copy_string);
int patharray_insert(patharray_t * patharray, unsigned int index,
    char * string, int copy_string);
void patharray_drop_one(patharray_t * patharray);
int patharray_is_absolute(patharray_t * patharray);
int patharray_is_root(patharray_t * patharray);

patharray_t * patharray_create(void)
{
    int error = 0;
    patharray_t * patharray = NULL; // returned
    unsigned int cap = 10;

    patharray = (patharray_t *)malloc(sizeof(patharray_t));
    if (!patharray) {
        goto finish;
    }

    patharray->capacity = cap;
    patharray->length = 0;
    patharray->elements = (char **)malloc(patharray->capacity *
        (sizeof(char *)));
    if (!patharray->elements) {
        error =1;
        goto finish;
    }

finish:
    if (error) {
        if (patharray) patharray_free(patharray);
    }
    return patharray;
}

void patharray_free(patharray_t * patharray)
{
    unsigned int i;

    for (i = 0; i < patharray->length; i++) {
        free(patharray->elements[i]);
    }
    free(patharray->elements);
    free(patharray);
    return;
}

int patharray_append(patharray_t * patharray, char * string,
    int copy_string)
{
    return patharray_insert(patharray, patharray->length,
        string, copy_string);
}

int patharray_insert(patharray_t * patharray, unsigned int index,
    char * string, int copy_string)
{
    int result = 1;
    char * copy = NULL;  // don't free

    if (patharray->length >= patharray->capacity) {
        if (!patharray->capacity)
            patharray->capacity = 10;
        else {
            patharray->capacity *= 2;
        }
        patharray->elements = (char **)realloc(patharray->elements,
            (patharray->capacity * sizeof(char *)));
        if (!patharray->elements) {
            result = 0;
        }
    }

    if (index > patharray->length) {
        result = 0;
        goto finish;
    } else if (index < patharray->length) {
        memmove(&patharray->elements[index+1], &patharray->elements[index],
            (sizeof(char *) * patharray->length - index));
    }
    patharray->length++;

    if (copy_string) {
        copy = strdup(string);
    } else {
        copy = string;
    }
    patharray->elements[index] = copy;

finish:
    return result;
}

void patharray_drop_one(patharray_t * patharray)
{
    if (patharray->length == 0) {
        return;
    }

    if (patharray->elements[patharray->length - 1]) {
        free(patharray->elements[patharray->length - 1]);
        patharray->length--;
    }
    return;

}

int patharray_is_absolute(patharray_t * patharray)
{
    if (patharray->length > 0 && patharray->elements[0][0] == '\0') {
        return 1;
    } else {
        return 0;
    }
}

int patharray_is_root(patharray_t * patharray)
{
    if (patharray->length == 0 ||
        (patharray->length == 1 && patharray_is_absolute(patharray))) {

        return 1;
    } else {
        return 0;
    }
}


static patharray_t * split_path(const char * path);
static patharray_t * canonicalize_patharray(patharray_t * patharray);
static char * assemble_path(patharray_t * patharray);


CFURLRef PATH_CopyCanonicalizedURL(CFURLRef anURL)
{
    return PATH_CopyCanonicalizedURLAndSetDirectory(
        anURL, CFURLHasDirectoryPath(anURL));
}

CFURLRef PATH_CopyCanonicalizedURLAndSetDirectory(
    CFURLRef anURL, Boolean isDirectory)
{
    CFURLRef    newURL = NULL;          // don't free
    CFStringRef absolutePath = NULL;    // must free
    char *      absolute_path = NULL;   // must free


    absolute_path = PATH_CanonicalizedCStringForURL(anURL);
    if (!absolute_path) {
        goto finish;
    }
    absolutePath = CFStringCreateWithCString(kCFAllocatorDefault,
        absolute_path, kCFStringEncodingMacRoman);
    if (!absolutePath) {
        goto finish;
    }

    newURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        absolutePath, kCFURLPOSIXPathStyle, isDirectory);
    if (!newURL) {
        goto finish;
    }


finish:
    if (absolutePath)    CFRelease(absolutePath);
    if (absolute_path)   free(absolute_path);

    return newURL;
}

char *   PATH_CanonicalizedCStringForURL(CFURLRef anURL)
{
    CFURLRef    absURL = NULL;         // must free
    char *      absolute_path = NULL;  // returned
    CFStringRef absolutePath = NULL;   // must free
    char        path[MAXPATHLEN];

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        goto finish;
    }

    absolutePath = CFURLCopyFileSystemPath(absURL,
        kCFURLPOSIXPathStyle);
    if (!absolutePath) {
        goto finish;
    }
    if (!CFStringGetCString(absolutePath,
        path, sizeof(path) - 1, kCFStringEncodingMacRoman)) {

        goto finish;
    }

    CFRelease(absolutePath);
    absolutePath = NULL;

    absolute_path = PATH_canonicalizeCStringPath(path);
    if (!absolute_path) {
        goto finish;
    }

finish:
    if (absURL)        CFRelease(absURL);
    if (absolutePath)  CFRelease(absolutePath);

    return absolute_path;
}


char * PATH_canonicalizeCStringPath(const char * path)
{
    char * newpath = NULL;              // returned
    patharray_t * patharray = NULL;     // must free
    patharray_t * newpatharray = NULL;  // must free

    patharray = split_path(path);
    if (!patharray) {
        goto finish;
    }
    newpatharray = canonicalize_patharray(patharray);
    if (!newpatharray) {
        goto finish;
    }
    newpath = assemble_path(newpatharray);
    if (!newpath) {
        goto finish;
    }

finish:

    if (patharray)    patharray_free(patharray);
    if (newpatharray) patharray_free(newpatharray);
    return newpath;
}


patharray_t * split_path(const char * path)
{
    char error = 0;
    patharray_t * patharray = NULL;  // returned
    const char * scanner = NULL;
    const char * lookahead = NULL;
    unsigned int numslashes = 0;
    unsigned int length = 0;
    unsigned int i, j;

    for (i = 0; path[i]; i++) {
        if (path[i] == '/') {
            numslashes++;
        }
    }

    patharray = patharray_create();
    if (!patharray) {
        error = 1;
        goto finish;
    }

    scanner = path;

    length = strlen(path);

    for (j = 0; j <= length; j++) {
        lookahead = &path[j];
        if (*lookahead == '/' || *lookahead == '\0') {
            unsigned int component_length = lookahead - scanner;
            char * component = (char *)malloc(component_length + 1);
            if (!component) {
                error = 1;
                goto finish;
            }
            memcpy(component, scanner, component_length);
            component[component_length] = '\0';
            patharray_append(patharray, component, 0);

           /* Move the scanner ahead.
            */
            scanner = lookahead + 1;
        }
    }

finish:

    if (error) {
        patharray_free(patharray);
        patharray = NULL;
    }
    return patharray;
}

patharray_t * canonicalize_patharray(patharray_t * patharray)
{
    int error = 0;
    patharray_t * canonical_patharray = NULL;  // returned
    char * string = NULL;                      // part of new patharray
    unsigned int path_index;


   /* Get the beginning of the canonical path.
    */
    if (!patharray_is_absolute(patharray)) {
        string = getcwd(NULL, 0);
        if (!string) {
            error = 1;
            goto finish;
        }
        canonical_patharray = split_path(string);
        free(string);
        string = NULL;
        if (!canonical_patharray) {
            error = 1;
            goto finish;
        }
    } else {
        canonical_patharray = patharray_create();
        if (!canonical_patharray) {
            error = 1;
            goto finish;
        }
    }

    for (path_index = 0; path_index < patharray->length; path_index++) {
        char * component = (char *)patharray->elements[path_index];

        if (component[0] == '\0' || !strcmp(component, ".")) {

            // do nothing

        } else if (!strcmp(component, "..")) {

            patharray_drop_one(canonical_patharray);

        } else {
            patharray_append(canonical_patharray, component, 1);
        }
    }

    if (canonical_patharray->length == 0) {
        patharray_append(canonical_patharray, "", 1);
    } else if (!patharray_is_absolute(canonical_patharray)) {
        patharray_insert(canonical_patharray, 0, "", 1);
    }

finish:

    if (error) {
        if (canonical_patharray) patharray_free(canonical_patharray);
    }

    return canonical_patharray;
}


char * assemble_path(patharray_t * patharray)
{
    int error = 0;
    char * newpath = NULL;
    unsigned int i;
    size_t length = 0;

    for (i = 0; i < patharray->length; i++) {
        size_t clength = strlen(patharray->elements[i]);
        length += clength + 1; // add one for each slash
    }
    if (length < 2) {
        length = 2;
    }

    newpath = (char *)malloc(sizeof(char) * (length+1));
    if (!newpath) {
        error = 1;
        goto finish;
    }

    newpath[0] = '\0';
    if (patharray_is_root(patharray)) {
        strcat(newpath, "/");
    } else {
        for (i = 0; i < patharray->length; i++) {
            if (i > 0) {
                strcat(newpath, "/");
            }
            strcat(newpath, patharray->elements[i]);
        }
    }

finish:
    if (error) {
        if (newpath) {
            free(newpath);
            newpath = NULL;
        }
    }
    return newpath;
}
