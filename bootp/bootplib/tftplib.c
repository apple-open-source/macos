
/*
 * tftp.c
 * - fork/exec tftp to retrieve a remote file
 */
/*
 * Modification History
 *
 * December 14, 1999	Dieter Siegmund (dieter@apple)
 * - created
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <mach/boolean.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <string.h>

#define TFTP	"/usr/bin/tftp > /dev/null 2>&1"
 
/*
 * Function: tftp_get
 *
 * Purpose:
 *   Reads the file from the given server and places it in a temporary
 *   file in /tmp. 
 * Returns:
 *   Malloc'd pathname to temporary file if successful,
 *   NULL otherwise.
 */
char *
tftp_get(char * host, char * remote_file, off_t * len_p, int t)
{
    struct stat sb;
    char 	filename[128] = "/tmp/tftp_getXXXXXX";
    FILE * 	f;

    *len_p = 0;

    if (mktemp(filename) == NULL) {
#ifdef DEBUG
	perror("mktemp");
#endif DEBUG
	return (NULL);
    }
    f = popen(TFTP, "w");
    if (f == NULL)
	return (NULL);

    fprintf(f, "binary\n");
    fprintf(f, "timeout %d\n", t);
    fprintf(f, "connect %s\n", host);
    fprintf(f, "get %s %s\n", remote_file, filename);
    pclose(f);
    if (stat(filename, &sb) < 0) {
#ifdef DEBUG
	fprintf(stderr, TFTP " %s:%s -> %s: download unsuccessful", 
		host, remote_file, filename);
#endif DEBUG
	return (NULL);
    }
    *len_p = sb.st_size;
    return (strdup(filename));
}

#ifdef TESTING
int 
main(int argc, char * argv[])
{
    char * 	filename = NULL;
    off_t 	len = 0;

    if (argc < 3) {
	fprintf(stderr, "usage: %s <host> <remote>\n", argv[0]);
	exit(2);
    }
    filename = tftp_get(argv[1], argv[2], &len);
    if (filename) {
	printf("read %s:%s -> %s %qd\n", argv[1], argv[2], filename, len);
	free(filename);
	exit(0);
    }
    else {
	fprintf(stderr, "tftp %s:%s failed\n", argv[1], argv[2]);
	exit(1);
    }
    return (0);
}
#endif TESTING
