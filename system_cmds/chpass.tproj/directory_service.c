#ifdef DIRECTORY_SERVICE

#include "directory_service.h"
#include "chpass.h"
#include <err.h>
#include <sys/time.h>

#include <sys/errno.h>
extern int errno;

#define	CONFIGNAMELEN	14
#define	GLOBALCONFIGLEN	20
#define	LOOKUPORDERLEN	13
#define	LINESIZE	128
#define	NETINFOROOTLEN	13
#define NLIST		13
#define	REMOTEINFOLEN	9
#define	USERCONFIGLEN	18

/*---------------------------------------------------------------------------
 * Global variables
 *---------------------------------------------------------------------------*/
char *DSPath = NULL;
const char MasterPasswd[] = "/etc/master.passwd";

/*---------------------------------------------------------------------------
 * Local variables
 *---------------------------------------------------------------------------*/
static char Agent[] = "Agent";
static char ConfigName[] = "_config_name: ";
static char DSFiles[] = "/BSD/local";
static char FFPatFmt[] = "^%s:";
static char GlobalConfig[] = "Global Configuration";
static char LocalNI[] = "/NetInfo/DefaultLocalNode";
static char LookupOrder[] = "LookupOrder: ";
static char LookupOrderSep[] = " ";
static char NetinfoRoot[] = "/NetInfo/root";
static char NiclPathFmt[] = "/users/%s";
static char NISPatFmt[] = "/usr/bin/ypcat passwd.byname | /usr/bin/grep -q '^%s:'";
static char RemoteNI[] = "/NetInfo/";
static char UserConfig[] = "User Configuration";
static unsigned char RestrictedFFRoot[] = {
	0, /*E_LOGIN */
	0, /*E_PASSWD */
	0, /*E_UID */
	0, /*E_GID */
	0, /*E_CHANGE */
	0, /*E_EXPIRE */
	0, /*E_CLASS */
	0, /*E_HOME */
	0, /*E_SHELL */
	0, /*E_NAME */
	1, /*E_LOCATE */
	1, /*E_BPHONE */
	1, /*E_HPHONE */
};
static unsigned char RestrictedFFUser[] = {
	1, /*E_LOGIN */
	1, /*E_PASSWD */
	1, /*E_UID */
	1, /*E_GID */
	1, /*E_CHANGE */
	1, /*E_EXPIRE */
	1, /*E_CLASS */
	1, /*E_HOME */
	0, /*E_SHELL */
	0, /*E_NAME */
	1, /*E_LOCATE */
	1, /*E_BPHONE */
	1, /*E_HPHONE */
};
static unsigned char RestrictedLocalNIRoot[] = {
	0, /*E_LOGIN */
	1, /*E_PASSWD */
	0, /*E_UID */
	0, /*E_GID */
	0, /*E_CHANGE */
	0, /*E_EXPIRE */
	0, /*E_CLASS */
	0, /*E_HOME */
	0, /*E_SHELL */
	0, /*E_NAME */
	1, /*E_LOCATE */
	1, /*E_BPHONE */
	1, /*E_HPHONE */
};
static unsigned char RestrictedLocalNIUser[] = {
	1, /*E_LOGIN */
	1, /*E_PASSWD */
	1, /*E_UID */
	1, /*E_GID */
	1, /*E_CHANGE */
	1, /*E_EXPIRE */
	1, /*E_CLASS */
	1, /*E_HOME */
	0, /*E_SHELL */
	0, /*E_NAME */
	1, /*E_LOCATE */
	1, /*E_BPHONE */
	1, /*E_HPHONE */
};

#define	NWHERE		4

typedef int (*wherefunc)(const char *);

static int compar(const void *, const void *);
static int runnicl(char *name, char *key, char *val);
static int whereCache(const char *);
static int whereDS(const char *);
static int whereFF(const char *);
static int whereNI(const char *);
static int whereNIL(const char *);
static int whereNIS(const char *);

/*---------------------------------------------------------------------------
 * WhereList determines what functions to call when the LookupOrder is followed
 *---------------------------------------------------------------------------*/
struct where {
    char *agent;
    int len;
    wherefunc func;
} WhereList[] = {
    {"Cache", 5, whereCache},
    {"DS", 2, whereDS},
    {"FF", 2, whereFF},
    {"NI", 2, whereNI},
    {"NIL", 3, whereNIL},
    {"NIS", 3, whereNIS},
};

#define	PATINDEX	2
static char *Grep[] = {
    "/usr/bin/grep",
    "-q",
    NULL, /* pattern goes here */
    (char *)MasterPasswd,
    NULL
};

#define	NICLPATHINDEX	3
#define	NICLKEYINDEX	4
#define	NICLVALUEINDEX	5
static char *Nicl[] = {
    "/usr/bin/nicl",
    ".",
    "-create",
    NULL, /* path goes here */
    NULL, /* key goes here */
    NULL, /* value goes here */
    NULL
};

#define	YPCATINDEX	2
static char *Ypcat[] = {
    "/bin/sh",
    "-c",
    NULL, /* ypcat cmd goes here */
    NULL
};

/*---------------------------------------------------------------------------
 * compar - called by bsearch() to search WhereList for an agent
 *---------------------------------------------------------------------------*/
#define	A	((const struct where *)a)
#define	KEY	((const char *)key)
static int
compar(const void *key, const void *a)
{
    int result = strncmp(KEY, A->agent, A->len);
    if(result)
	return result;
    if(KEY[A->len] == 0)
	return 0;
    return strcmp(KEY + A->len, Agent);
}
#undef KEY
#undef A

/*---------------------------------------------------------------------------
 * runnicl - run the nicl command to update local netinfo fields
 *---------------------------------------------------------------------------*/
static int
runnicl(char *name, char *key, char *val)
{
    char path[128];
    pid_t pid;
    int estat;
    int status;

    IF((pid = fork()) >= 0) {
	if(pid == 0) {
	    sprintf(path, NiclPathFmt, name);
	    Nicl[NICLPATHINDEX] = path;
	    Nicl[NICLKEYINDEX] = key;
	    Nicl[NICLVALUEINDEX] = val;
	    /*---------------------------------------------------------------
	     * Become fully root to call nicl
	     *---------------------------------------------------------------*/
	    setuid(geteuid());
	    execv(Nicl[0], Nicl);
	    _exit(1);
	}
	if(waitpid(pid, &estat, 0) < 0) {
	    status = errno;
	    break;
	}
	if(!WIFEXITED(estat)) {
	    status = E_NICLFAILED;
	    break;
	}
	status = (WEXITSTATUS(estat) == 0 ? 0 : E_NICLFAILED);
    } CLEANUP {
    } ELSE {
	status = errno;
    } ENDIF
    return status;
}
/*---------------------------------------------------------------------------
 * PUBLIC setrestricted - sets the restricted flag
 *---------------------------------------------------------------------------*/
void
setrestricted(int where, struct passwd *pw)
{
    unsigned char *restricted;
    int i;
    ENTRY *ep;

    switch(where)
	{
		case WHERE_FILES:
			restricted = uid ? RestrictedFFUser : RestrictedFFRoot;
			break;
		case WHERE_LOCALNI:
			restricted = uid ? RestrictedLocalNIUser : RestrictedLocalNIRoot;
			break;
		default:
			return;
    }
	
    for (ep = list, i = NLIST; i > 0; i--)
		(ep++)->restricted = *restricted++;
	
    if (uid && !ok_shell(pw->pw_shell))
		list[E_SHELL].restricted = 1;
}

/*---------------------------------------------------------------------------
 * PUBLIC update_local_ni - update local netinfo
 *---------------------------------------------------------------------------*/
void
update_local_ni(struct passwd *pworig, struct passwd *pw)
{
    char buf[64];
    char *np, *op, *bp;

    if(pworig->pw_uid != pw->pw_uid) {
	sprintf(buf, "%d", pw->pw_uid);
	runnicl(pworig->pw_name, "uid", buf);
    }
    if(pworig->pw_gid != pw->pw_gid) {
	sprintf(buf, "%d", pw->pw_gid);
	runnicl(pworig->pw_name, "gid", buf);
    }
    if(pworig->pw_change != pw->pw_change) {
	sprintf(buf, "%lu", pw->pw_change);
	runnicl(pworig->pw_name, "change", buf);
    }
    if(pworig->pw_expire != pw->pw_expire) {
	sprintf(buf, "%lu", pw->pw_expire);
	runnicl(pworig->pw_name, "expire", buf);
    }
    if(strcmp(pworig->pw_dir, pw->pw_dir) != 0)
	runnicl(pworig->pw_name, "home", pw->pw_dir);
    if(strcmp(pworig->pw_shell, pw->pw_shell) != 0)
	runnicl(pworig->pw_name, "shell", pw->pw_shell);
    if(strcmp(pworig->pw_class, pw->pw_class) != 0)
	runnicl(pworig->pw_name, "class", pw->pw_class);

    bp = pworig->pw_gecos;
    op = strsep(&bp, ",");
    if(!op)
	op = "";
    bp = pw->pw_gecos;
    np = strsep(&bp, ",");
    if(!np)
	np = "";
    if(strcmp(op, np) != 0)
	runnicl(pworig->pw_name, "realname", np);

    if(strcmp(pworig->pw_name, pw->pw_name) != 0)
	runnicl(pworig->pw_name, "name", pw->pw_name);

    warnx("netinfo domain \"%s\" updated", DSPath);
}

/*---------------------------------------------------------------------------
 * whereCache - we skip the cache
 *---------------------------------------------------------------------------*/
static int
whereCache(const char *name)
{
    return E_NOTFOUND;
}

/*---------------------------------------------------------------------------
 * whereDS - call DirectoryService.  This does both netinfo and other directory
 * services, so we cache the value so we only process once.
 *---------------------------------------------------------------------------*/
static int
whereDS(const char *name)
{
    tDirReference dsRef;
    static tDirStatus status;
    static int dsCached = 0;

    if(dsCached)
	return status;
    dsCached = 1;
    IF((status = dsOpenDirService(&dsRef)) == eDSNoErr) {
	tDataBuffer *dataBuff;

	IF((dataBuff = dsDataBufferAllocate(dsRef, 4096)) != NULL) {
	    tContextData context = NULL;
	    unsigned long nodeCount;

	    /*---------------------------------------------------------------
	     * Find and open the search node.
	     *---------------------------------------------------------------*/
	    IF((status = dsFindDirNodes(dsRef, dataBuff, NULL,
	     eDSAuthenticationSearchNodeName, &nodeCount, &context))
	     == eDSNoErr)
		{
			tDataListPtr nodeName;
			if(nodeCount < 1) {
				status = eDSNodeNotFound;
				break;
			}
			nodeName = NULL;
			IF((status = dsGetDirNodeName(dsRef, dataBuff, 1, &nodeName)) == eDSNoErr)
			{
				tDirNodeReference    nodeRef;

				IF((status = dsOpenDirNode(dsRef, nodeName, &nodeRef)) == eDSNoErr) {
					tDataListPtr pRecType;
					tDataListPtr pAttrType;
					tDataListPtr pPattern;
					unsigned long recCount;
					tContextData context2 = NULL;

					/*---------------------------------------------------
					 * Now search the search node for the given user name.
					 *---------------------------------------------------*/
					pRecType = dsBuildListFromStrings(dsRef,
					 kDSStdRecordTypeUsers, NULL);
					pAttrType = dsBuildListFromStrings(dsRef,
					 kDSNAttrMetaNodeLocation, NULL);
					pPattern = dsBuildListFromStrings(dsRef, name, NULL);
					IF((status = dsGetRecordList(nodeRef, dataBuff,
					 pPattern, eDSExact, pRecType, pAttrType, 0, &recCount,
					 &context2)) == eDSNoErr) {
						tAttributeListRef attrListRef;
						tRecordEntry *pRecEntry;

						if(recCount < 1) {
						status = E_NOTFOUND;
						break;
						}
						/*-----------------------------------------------
						 * Get the attributes for the first entry we find
						 *-----------------------------------------------*/
						IF((status = dsGetRecordEntry(nodeRef,
						 dataBuff, 1, &attrListRef, &pRecEntry)) ==
						 eDSNoErr) {
						tAttributeValueListRef valueRef;
						tAttributeEntry *pAttrEntry;

						/*-------------------------------------------
						 * Get the first (only) attribute
						 *-------------------------------------------*/
						IF((status = dsGetAttributeEntry( nodeRef, dataBuff, attrListRef, 1, &valueRef,
							&pAttrEntry)) == eDSNoErr)
						{
							tAttributeValueEntry *pValueEntry;
							
							/*---------------------------------------
							 * Put the attribute values into a data
							 * list.
							 *---------------------------------------*/
							
							status = dsGetAttributeValue(nodeRef, dataBuff, 1, valueRef, &pValueEntry);
							if ( status == eDSNoErr )
							{
								DSPath = (char *) malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
								if ( DSPath != NULL )
									strlcpy( DSPath, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength + 1 );
								
								dsDeallocAttributeValueEntry(dsRef, pValueEntry);
							}
							
							if(status != eDSNoErr)
								break;
							
							if(strcmp(DSPath, LocalNI) == 0)
							{
								status = WHERE_LOCALNI;
								/*---------------------------
								 * Translate to netinfo path
								 *---------------------------*/
								free((void *)DSPath);
								DSPath = strdup(".");
							}
							else if(strcmp(DSPath, DSFiles) == 0)
							{
								status = WHERE_FILES;
								/*---------------------------
								 * Translate to master.passwd
								 * path
								 *---------------------------*/
								free((void *)DSPath);
								DSPath = strdup(MasterPasswd);
							}
							else if(strncmp(DSPath, RemoteNI, REMOTEINFOLEN) == 0)
							{
								status = WHERE_REMOTENI;
								/*---------------------------
								 * Translate to netinfo path
								 *---------------------------*/
								if(strncmp(DSPath, NetinfoRoot,
								 NETINFOROOTLEN) == 0) {
									if(DSPath[NETINFOROOTLEN]
									 == 0) {
									free((void *)DSPath);
									DSPath = strdup("/");
									} else {
									char *tmp =
									 strdup(DSPath +
									 NETINFOROOTLEN);
									free((void *)DSPath);
									DSPath = tmp;
									}
								}
							}
							else
							{
								status = WHERE_DS;
							}
						} CLEANUP {
							dsCloseAttributeValueList(valueRef);
							dsDeallocAttributeEntry(dsRef, pAttrEntry);
						} ELSE {
						} ENDIF
						} CLEANUP {
						dsCloseAttributeList(attrListRef); 
						dsDeallocRecordEntry(dsRef, pRecEntry);
						} ENDIF
					} CLEANUP {
						if(context2)
						dsReleaseContinueData(dsRef, context2);
					} ENDIF
					dsDataListDeallocate(dsRef, pRecType);
					free(pRecType);
					dsDataListDeallocate(dsRef, pAttrType);
					free(pAttrType);
					dsDataListDeallocate(dsRef, pPattern);
					free(pPattern);
				} CLEANUP {
				dsCloseDirNode(nodeRef);
				} ENDIF
			} CLEANUP {
		    dsDataListDeallocate(dsRef, nodeName);
		} ENDIF
	    } CLEANUP {
		if(context)
		    dsReleaseContinueData(dsRef, context);
	    } ENDIF
	} CLEANUP {
	    dsDataBufferDeAllocate(dsRef, dataBuff);
	} ELSE {
	    status = eMemoryAllocError;
	} ENDIF
    } CLEANUP {
	dsCloseDirService(dsRef);
    } ENDIF
    return status;
}

/*---------------------------------------------------------------------------
 * whereFF - check the flat file (/etc/master.passwd)
 *---------------------------------------------------------------------------*/
static int
whereFF(const char *name)
{
    pid_t pid;
    int estat;
    int status;

    IF((pid = fork()) >= 0) {
	if(pid == 0) {
	    char pat[64];

	    sprintf(pat, FFPatFmt, name);
	    Grep[PATINDEX] = pat;
	    /*---------------------------------------------------------------
	     * Become fully root to read /etc/master.passwd
	     *---------------------------------------------------------------*/
	    setuid(geteuid());
	    execv(Grep[0], Grep);
	    _exit(1);
	}
	if(waitpid(pid, &estat, 0) < 0) {
	    status = errno;
	    break;
	}
	if(!WIFEXITED(estat)) {
	    status = E_CHILDFAILED;
	    break;
	}
	status = (WEXITSTATUS(estat) == 0 ? WHERE_FILES : E_NOTFOUND);
    } CLEANUP {
    } ELSE {
	status = errno;
    } ENDIF
    return status;
}

/*---------------------------------------------------------------------------
 * whereNI - call whereDS to do the work, then the entry is found in directory
 * service (and not netinfo), mark as not found.
 *---------------------------------------------------------------------------*/
static int
whereNI(const char *name)
{
    int status = whereDS(name);

    if(status == WHERE_DS)
	status = E_NOTFOUND;
    return status;
}

/*---------------------------------------------------------------------------
 * whereNIL - we skip the NILAgent
 *---------------------------------------------------------------------------*/
static int
whereNIL(const char *name)
{
    return E_NOTFOUND;
}

/*---------------------------------------------------------------------------
 * whereNIS - check NIS passwd.byname
 *---------------------------------------------------------------------------*/
static int
whereNIS(const char *name)
{
    pid_t pid;
    int estat;
    int status;

    IF((pid = fork()) >= 0) {
	if(pid == 0) {
	    char cmd[256];

	    sprintf(cmd, NISPatFmt, name);
	    Ypcat[YPCATINDEX] = cmd;
	    execv(Ypcat[0], Ypcat);
	    _exit(1);
	}
	if(waitpid(pid, &estat, 0) < 0) {
	    status = errno;
	    break;
	}
	if(!WIFEXITED(estat)) {
	    status = E_CHILDFAILED;
	    break;
	}
	status = (WEXITSTATUS(estat) == 0 ? WHERE_NIS : E_NOTFOUND);
    } CLEANUP {
    } ELSE {
	status = errno;
    } ENDIF
    return status;
}

/*---------------------------------------------------------------------------
 * PUBLIC wherepwent - Given a const char *, determine lookupd's LookupOrder
 * and then search for the corresponding record for each agent.
 *---------------------------------------------------------------------------*/
int
wherepwent(const char *name)
{
    char user[LINESIZE];
    char *cp, *str;
    struct where *w;
    FILE *fp = NULL;
    int status = 0;
	fd_set fdset;
	struct timeval selectTimeout = { 2, 0 };
	int result;
	char order[LINESIZE], line[LINESIZE];
	char *task_argv[3] = {NULL};
	int readPipe = -1;
	int writePipe = -1;
	
	/*-------------------------------------------------------------------
 	 * Save the first LookupOrder as the global setting.  We make sure
	 * that the first _config_name is Global Configuration.
 	 *-------------------------------------------------------------------*/
	
	do
	{
		task_argv[0] = "/usr/sbin/lookupd";
		task_argv[1] = "-configuration";
		task_argv[2] = NULL;
	
		if ( LaunchTaskWithPipes(task_argv[0], task_argv, &readPipe, &writePipe) != 0 )
		return E_NOTFOUND;
	
		// close this pipe now so the forked process quits on completion
		if ( writePipe != -1 )
			close( writePipe );
		
		// wait for data (and skip signals)
	FD_ZERO( &fdset );
		FD_SET( readPipe, &fdset );
		do {
	result = select( FD_SETSIZE, &fdset, NULL, NULL, &selectTimeout );
		}
		while ( result == -1 && errno == EINTR );
		if ( result == -1 || result == 0 ) {
			status = E_NOTFOUND;
			break;
		}
	
	// now that the descriptor is ready, parse the configuration
		fp = fdopen(readPipe, "r");
		if ( fp == NULL ) {
			status = E_NOTFOUND;
			break;
		}
	*user = 0;
	while(fgets(line, LINESIZE, fp))
	{
	    if(strncasecmp(line, LookupOrder, LOOKUPORDERLEN) == 0) {
			if((cp = strchr(line, '\n')) != NULL)
				*cp = 0;
			strcpy(user, line + LOOKUPORDERLEN);
			continue;
	    }
	    if(strncasecmp(line, ConfigName, CONFIGNAMELEN) == 0) {
			if(strncasecmp(line + CONFIGNAMELEN, GlobalConfig, GLOBALCONFIGLEN) != 0) {
				status = E_NOGLOBALCONFIG;
			}
			break;
	    }
	}
	if(status < 0)
	    break;
	/*-------------------------------------------------------------------
 	 * Save the each LookupOrder and look for _config_name of User
	 * Configuration.  If found, replace the global order with this one.
 	 *-------------------------------------------------------------------*/
	*order = 0;
		while(fgets(line, LINESIZE, fp))
		{
	    if(strncasecmp(line, LookupOrder, LOOKUPORDERLEN) == 0) {
		if((cp = strchr(line, '\n')) != NULL)
		    *cp = 0;
		strcpy(order, line + LOOKUPORDERLEN);
		continue;
	    }
	    if(strncasecmp(line, ConfigName, CONFIGNAMELEN) == 0) {
		if(strncasecmp(line + CONFIGNAMELEN, UserConfig, USERCONFIGLEN) == 0) {
		    if(*order)
				strcpy(user, order);
		    break;
		}
		*order = 0;
	    }
	}
	if(*user == 0) {
	    status = E_NOLOOKUPORDER;
	    break;
	}
	}
	while ( 0 );
	
	if ( fp != NULL )
		fclose( fp );
	else if ( readPipe != -1 )
		close( readPipe );
	
    if(status < 0)
	return status;
	
    /*-----------------------------------------------------------------------
     * Now for each agent, call the corresponding where function.  If the
     * return value is no E_NOTFOUND, then we either have found it or have
     * detected an error.
     *-----------------------------------------------------------------------*/
    str = user;
    while((cp = strtok(str, LookupOrderSep)) != NULL) {
	if((w = bsearch(cp, WhereList, NWHERE, sizeof(struct where),
	 compar)) != NULL) {
	    if((status = w->func(name)) != E_NOTFOUND)
		return status;
	} else
	    printf("%s not supported\n", cp);
	str = NULL;
    }
    return E_NOTFOUND;
}
#endif /* DIRECTORY_SERVICE */
