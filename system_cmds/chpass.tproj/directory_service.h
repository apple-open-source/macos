#ifndef _DIRECTORY_SERVICE_H_
#define _DIRECTORY_SERVICE_H_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <DirectoryService/DirectoryService.h>

/*---------------------------------------------------------------------------
 * Convenience macros: to be used to provide a cleanup section (eg, to
 * deallocate memory, etc).  IF, CLEANUP and ENDIF must always be used, and
 * must be used with braces; ELSE is optional, but must also be used with
 * braces.
 *
 * IF(expression) {
 *   ...
 * } CLEANUP {
 *   ...
 * } ELSE {
 *   ...
 * } ENDIF
 *
 * "break" may be used in the IF section to exit the block prematurely; the
 * CLEANUP section will still be performed.  "break" in the CLEANUP and ELSE
 * sections apply to higher level blocks.
 *---------------------------------------------------------------------------*/
#define	IF(x)		if(x) { do
#define	CLEANUP		while(0);
#define	ELSE		} else {
#define	ENDIF		}

/*---------------------------------------------------------------------------
 * Error codes (not including DirectoryService error codes and standard error
 * codes)
 *---------------------------------------------------------------------------*/
#define	E_NOTFOUND		-1000
#define	E_NOGLOBALCONFIG	-1001
#define	E_NOLOOKUPORDER		-1002
#define	E_POPENFAILED		-1003
#define	E_CHILDFAILED		-1004
#define	E_DATALISTOUTOFMEM	-1005
#define	E_PATHOUTOFMEM		-1006
#define	E_NICLFAILED		-1007

/*---------------------------------------------------------------------------
 * Success return values from wherepwent()
 *---------------------------------------------------------------------------*/
enum {
    WHERE_FILES = 0,
    WHERE_LOCALNI,
    WHERE_REMOTENI,
    WHERE_DS,
    WHERE_NIS,
};

/*---------------------------------------------------------------------------
 * Global variables
 *---------------------------------------------------------------------------*/
extern const char *DSPath;
extern const char MasterPasswd[];

/*---------------------------------------------------------------------------
 * Function prototypes
 *---------------------------------------------------------------------------*/
extern void setrestricted(int where, struct passwd *pw);
extern void update_local_ni(struct passwd *pworig, struct passwd *pw);
extern int wherepwent(const char *name);

#endif /* _DIRECTORY_SERVICE_H_ */
