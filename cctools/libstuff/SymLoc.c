#import <libc.h>
#import <ctype.h>
#import <sys/types.h>
#ifdef __OPENSTEP__
#define _POSIX_SOURCE
#endif
#import <dirent.h>
#import <pwd.h>
#import "stuff/bool.h"
#import "stuff/errors.h"
#import "stuff/allocate.h"
#import "stuff/SymLoc.h"

const char *
symLocForDylib(const char *installName, const char *releaseName,
enum bool *found_project,
enum bool disablewarnings)
{
	return(LocForDylib(installName, releaseName, "Symbols", found_project,
			   disablewarnings));
}

const char *
dstLocForDylib(const char *installName, const char *releaseName,
enum bool *found_project,
enum bool disablewarnings)
{
	return(LocForDylib(installName, releaseName, "Roots", found_project,
			   disablewarnings));
}

// caller is responsible for freeing the returned string (using free(3)) 
const char *
LocForDylib(const char *installName, const char *releaseName,
const char *dirname,
enum bool *found_project,
enum bool disablewarnings)
{
    struct passwd	*passwd		= NULL;
    struct dirent	*dp		= NULL;
    FILE		*file 		= NULL;
    DIR			*dirp		= NULL;
    char		*line		= NULL;
    char		*c		= NULL;
    char		*v		= NULL;
    int			 releaseLen	= strlen(releaseName);
    char		 buf[MAXPATHLEN+MAXNAMLEN+64];
    char		 readbuf[MAXPATHLEN+64];
    char		 viewPath[MAXPATHLEN];
    char		 dylibList[MAXPATHLEN];

    *found_project = FALSE;

    // check parameters
    if (!installName || !*installName || !releaseName || !*releaseName) {
        fatal("internal error symLocForDylib(): Null or empty parameter");
        return NULL;
    }

    // find ~rc's home directory
    if (!(passwd = getpwnam("rc"))) {
        system_error("symLocForDylib(): getpwnam(\"rc\") returns NULL");
        return NULL;
    }
    strcpy(buf, passwd->pw_dir);

    // open release-to-view file
    strcat(buf, "/Data/release_to_view.map");
    if (!(file = fopen(buf, "r"))) {
        system_error("symLocForDylib(): Can't fopen %s", buf);
        return NULL;
    }

    // parse release-to-view file
    *viewPath = '\0';
    while ((line = fgets(buf, sizeof(buf), file))) {
        if (!strncmp(line, releaseName, releaseLen) && isspace(line[releaseLen])) {
            c = &line[releaseLen] + 1;
            while (isspace(*c)) c++;
            for (v = &viewPath[0]; !isspace(*c); c++, v++) *v = *c;
            *v = '\0';
            break;
        }
    }
    if(fclose(file) != 0)
	system_error("fclose() failed");
    if (!*viewPath) {
        error("symLocForDylib(): Can't locate view path for release %s",
	      releaseName);
        return NULL;
    }

    // open DylibProjects directory
    strcpy(dylibList, viewPath);
    c = &dylibList[strlen(dylibList)];
    strcpy(c, "/.BuildData/DylibProjects");
    if (!(dirp = opendir(dylibList))) {
        system_error("symLocForDylib(): Can't opendir %s", buf);
        return NULL;
    }

    // read DylibProjects entries
    *buf = '\0';
    v = &dylibList[strlen(dylibList)];
    *v = '/';
    v++;
    while ((dp = readdir(dirp))) {

        // skip "." and ".."
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;

        // open file
        strcpy(v, dp->d_name);
        if (!(file = fopen(dylibList, "r"))) {
            system_error("symLocForDylib(): Can't fopen %s", dylibList);
	    if(closedir(dirp) != 0)
		system_error("closedir() failed");
            return NULL;
        }

        // parse file
        while ((line = fgets(readbuf, sizeof(readbuf), file))) {
            if (!*line || *line == '(' || *line == ')') continue;
            while (*line == ' ') line++;
            if (*line != '"') {
                warning("symLocForDylib(): %s contains malformed line",
			dp->d_name);
                continue;
            }
            line++;
            for (c = &buf[0]; *line && *line != '"'; *c++ = *line++);
            if (*line != '"') {
                warning("symLocForDylib(): %s contains malformed line",
		        dp->d_name);
                continue;
            }
            *c = '\0';
            if (!strcmp(buf, installName)) {
                c = allocate(strlen(viewPath) + strlen(releaseName) +
			     strlen(dirname) + strlen(dp->d_name) + 32);
                sprintf(c, "%s/Updates/Built%s/%s/%s", viewPath, releaseName,
			dirname, dp->d_name);
                break;
            } else {
                c = NULL;
            }
        }
        if(fclose(file) != 0)
	    system_error("fclose() failed");
        if (c) break;
    }
    if(closedir(dirp) != 0)
	system_error("closedir() failed");

    // process return value
    if (!c) {
        error("Can't find project that builds %s", installName);
        return NULL;
    } else {
	*found_project = TRUE;
        return c;
    }
}

