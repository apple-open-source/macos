#include <stdio.h>
#include <stdlib.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach/mach.h>

#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/dylib_table.h"
/* This may change to "/MacOSX/System" someday */
#define SYSTEM_LIBRARY "/System/Library"

/*
 * parse_dylib_table() opens the file_name passed to it and parses it as a
 * dylib table.  The flag and argument parameters are used for error messages
 * if there is a problem parsing the file.  The file should
 * contains lines of the form:
 *	<hex address> <short name>
 * The fields are to be separated by spaces or tabs.  Lines with just spaces
 * and tabs are ignored.  This routine returns the parsed table as an
 * array of dylib_table structs and returns a pointer to the table.  The last
 * entry in the table has a NULL name.
 */
struct dylib_table *
parse_dylib_table(
char *file_name,/* file name of dylib table file */
char *flag,	/* "-dylib_file" or "default" */
char *argument) /* -dylib_file argument or "dylib table" */
{
    int fd;
    kern_return_t r;
    struct stat stat_buf;
    unsigned long j, k, file_size, new_dylib_table_size;
    char *file_addr, *endp;
    struct dylib_table *new_dylib_table;

	if((fd = open(file_name, O_RDONLY, 0)) == -1)
	    system_fatal("Can't open: %s for %s %s",
		    file_name, flag, argument);
	if(fstat(fd, &stat_buf) == -1)
	    system_fatal("Can't stat file: %s for %s %s",
		    file_name, flag, argument);
	/*
	 * For some reason mapping files with zero size fails
	 * so it has to be handled specially.
	 */
	if(stat_buf.st_size != 0){
	    if((r = map_fd((int)fd, (vm_offset_t)0,
		(vm_offset_t *)&file_addr, (boolean_t)TRUE,
		(vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't map file: %s for %s %s",
		    file_name, flag, argument);
	}
	else
	    fatal("Empty file: %s for %s %s", file_name, flag, argument);
	close(fd);
	file_size = stat_buf.st_size;

	/*
	 * Got the file mapped now parse it.
	 */
	if(file_addr[file_size - 1] != '\n')
	    fatal("file: %s for %s %s does not end in new line",
		  file_name, flag, argument);
	new_dylib_table_size = 0;
	for(j = 1; j < file_size; j++){
	    if(file_addr[j] == '\n'){
		new_dylib_table_size++;
	    }
	}
	new_dylib_table_size++;
	new_dylib_table = allocate(sizeof(struct dylib_table) *
				   new_dylib_table_size);
	k = 0;
	for(j = 0; j < file_size; /* no increment expression */ ){
	    /* Skip blank lines */
	    while(file_addr[j] == ' ' || file_addr[j] == '\t')
		j++;
	    if(file_addr[j] == '\n'){
		j++;
		continue;
	    }
	    new_dylib_table[k].seg1addr =
		strtoul(file_addr + j, &endp, 16);
	    if(endp == NULL)
		fatal("improper hexadecimal number on line %lu in "
		      "file: %s for %s %s", j, file_name, flag, argument);
	    j = endp - file_addr;
	    if(j == file_size)
		fatal("missing library name on line %lu in file: "
		      "%s for %s %s", j, file_name, flag, argument);
	    /*
	     * Since we checked to see the file ends in a '\n' we can
	     * be assured this won't run off the end of the file.
	     */
	    while(file_addr[j] == ' ' || file_addr[j] == '\t')
		j++;
	    if(file_addr[j] == '\n')
		fatal("missing library name on line %lu in file: "
		      "%s for %s %s", j, file_name, flag, argument);
	    new_dylib_table[k].name = file_addr + j;
	    k++;
	    while(file_addr[j] != '\n')
		j++;
	    file_addr[j] = '\0';
	    j++;
	}
	new_dylib_table[k].seg1addr = 0;
	new_dylib_table[k].name = NULL;
	return(new_dylib_table);
}

/*
 * parse_default_dylib_table() opens and parses ~rc/Data/DylibTable .
 */
struct dylib_table *
parse_default_dylib_table(
char **file_name)
{
    unsigned long i;
    FILE *fp;

	*file_name = allocate(MAXPATHLEN+1);
	fp = popen("/bin/echo ~rc/Data/DylibTable", "r");
	if(fp == NULL)
	    fatal("must use -dylib_table (popen failed on \"/bin/echo "
		  "~rc/Data/DylibTable\"");
	if(fgets(*file_name, MAXPATHLEN, fp) == NULL)
	    fatal("must use -dylib_table (fgets failed from popen of "
		  "\"/bin/echo ~rc/Data/DylibTable\"");
	i = strlen(*file_name);
	if(i == 0 || (*file_name)[i-1] != '\n')
	    fatal("must use -dylib_table (file name from popen of "
		  "\"/bin/echo ~rc/Data/DylibTable\" greater than "
		  "MAXPATHLEN");
	(*file_name)[i-1] = '\0';
	pclose(fp);
	return(parse_dylib_table(*file_name, "default", "dylib table"));
}

/*
 * search_dylib_table() searches the specified dylib table for the specified
 * name and returns the entry to it if it is found.  If it is not found NULL
 * is returned.
 */
struct dylib_table *
search_dylib_table(
struct dylib_table *dylib_table,
char *name)
{
    struct dylib_table *p;

	if(dylib_table == NULL)
	    return(NULL);

	for(p = dylib_table; p->name != NULL; p++){
	    if(strcmp(p->name, name) == 0)
		return(p);
	}
	return(NULL);
}

static char *versions[] = { "A", "B", "C", "0", NULL };

/*
 * guess_dylib_install_name() is passed a name from the dylib file and then
 * the install name is guessed based on the files on the system the program is
 * running on.  This routines makes up conventional install names base on the 
 * name passed in and then sees if there is a file on the system with that
 * install_name.  If it finds one it returns the install_name else it returns
 * NULL.
 */
char *
guess_dylib_install_name(
char *name)
{
    unsigned long i;
    char *guess;
    struct stat stat_buf;

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(
		    SYSTEM_LIBRARY,"/Frameworks/JavaVM.framework/Libraries/lib",
		    name, ".", versions[i], ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/", name,
			".framework/Versions/", versions[i], "/", name, NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/PrivateFrameworks/", name,
			".framework/Versions/", versions[i], "/", name, NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"Printers/", name, ".",
			    versions[i],".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
#ifdef __GONZO_BUNSEN_BEAKER__
	    guess = makestr("/Local/Library/Frameworks/", name,
			".framework/Versions/", versions[i], "/", name, NULL);
#else
	    guess = makestr("/MacOSX/Library/Frameworks/", name,
			".framework/Versions/", versions[i], "/", name, NULL);
#endif
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

#ifndef __GONZO_BUNSEN_BEAKER__
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/AppleInternal/Library/Frameworks/", name,
			".framework/Versions/", versions[i], "/", name, NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
#endif

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/lib/", name, ".", versions[i], ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/usr/lib/", name, ".", versions[i], ".dylib",
			NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/usr/lib/java/", name, ".", versions[i],
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/usr/local/lib/", name, ".", versions[i],
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr("/usr/canna/dylib/", name, ".", versions[i],
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
/* From ALLOW_MACOSX_PR1_PATHS */
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/CarbonCore.framework/"
			    "Versions/", versions[i], "/Support/", name,
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"Frameworks/Carbon.framework/"
			    "Versions/", versions[i], "/Libraries/", name,
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/CoreGraphics.framework/"
			    "Versions/", versions[i], "/Libraries/", name, ".",
			    versions[i], ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
/* From ALLOW_MACOSX_DP3_PATHS */
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Components/", name, ".qtx", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/Carbon.framework/"
			    "Versions/", versions[i], "/Resources//", name,
			    ".qtx", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/CoreGraphics.framework/"
			    "Versions/", versions[i], "/Resources/", name, ".",
			    versions[i], ".qtx", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/JavaVM.framework/"
			    "Versions/1.2/Libraries/", name, ".", versions[i],
			    ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/PrintingCore.framework/"
			    "Versions/", versions[i], "/Libraries/", name, ".",
			    versions[i], ".dylib", NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}
	for(i = 0; versions[i] != NULL; i++){
	    guess = makestr(SYSTEM_LIBRARY,"/Frameworks/QuickTime.framework/"
			    "Versions/", versions[i], "/", name, NULL);
	    if(stat(guess, &stat_buf) != -1)
		return(guess);
	    free(guess);
	}

	return(NULL);
}
