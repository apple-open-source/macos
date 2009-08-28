/*

Copyright (c) 2003-2009 Apple Inc. All rights reserved.

License for apache_mod_bonjour module:
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this list of 
conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list 
of conditions and the following disclaimer in the documentation and/or other materials 
provided with the distribution.
3. The end-user documentation included with the redistribution, if any, must include 
the following acknowledgment:
	"This product includes software developed by Apple Computer, Inc."
Alternately, this acknowledgment may appear in the software itself, if and 
wherever such third-party acknowledgments normally appear.
4. The names "Apache", "Apache Software Foundation", "Apple" and "Apple Computer, Inc." 
must not be used to endorse or promote products derived from this software without 
prior written permission. For written permission regarding the "Apache" and 
"Apache Software Foundation" names, please contact apache@apache.org.
5. Products derived from this software may not be called "Apache" or "Apple", 
nor may "Apache" or "Apple" appear in their name, without prior written 
permission of the Apache Software Foundation, or Apple Computer, Inc., respectively.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE COMPUTER, INC., 
THE APACHE SOFTWARE FOUNDATION OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGE.

Except as expressly set forth above, nothing in this License shall be construed 
as granting licensee, expressly or by implication, estoppel or otherwise, any 
rights or license under any trade secrets, know-how, patents, registrations, 
copyrights or other intellectual property rights of Apple, including without 
limitation, application programming interfaces ("APIs") referenced by the code, 
the functionality implemented by the APIs and the functionality invoked by calling 
the APIs.

*/
/*345678911234567892123456789312345678941234567895123456789612345678971234567898
*/
/* 
 * mod_bonjour Apache module
 *
 
 * This does not process requests; it just processes the config file to 
 * determine what if any names need to be registered with Bonjour, and registers them.
 * 
 * An array of pointers to registrationRec structures is kept, one for each registered service, so that
 * the old registrations can be cleaned up on a graceful restart.
 *
 * Because Apache has a bootstrap phase where the config file gets processed, it's necessary to
 * put userdata in the global pool so that the code can tell whether it's in the 
 * bootstrap phase or the real processing phase. 
 *
 * To do : make this work without mod_userdir, by recognizing its config directives
 */

#define CORE_PRIVATE                    1
#define MSG_PREFIX                      "mod_bonjour:"

#include <SystemConfiguration/SystemConfiguration.h>
#include <dns_sd.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include "httpd.h" 
#include "http_config.h" 
#include "http_core.h" 
#include "http_log.h" 
#include "http_main.h" 
#include "http_protocol.h" 
#include "http_request.h" 
#include "util_script.h" 
#include "http_connection.h" 
#include "apr_strings.h"
#include "apr_lib.h"
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <inttypes.h>

#define TITLE_MAX 127
#define TXT_MAX 511
#define REGNAME_MAX 255
#define PORT_MAX 65535
#define MAX_NAME_FORMAT 64
#define ALL_USERS "all-users"
#define CUSTOMIZED_USERS "customized-users"
#define DEFAULT_NAME_FORMAT "%l"

typedef struct registrationRec {	/* Needs to store info required to clean up upon deregistration */
    char        name[REGNAME_MAX+1];
    DNSServiceRef serviceRef;
    server_rec* serverData;
} registrationRec;

typedef struct module_cfg_rec {
    apr_pool_t *pPool;
	char*	sig;
	char* regName;
	char* regNameFormat;
	char* regText;
	uint16_t port;
    apr_array_header_t *registrationRecPtrArray;
	Boolean regUserSiteCmd;
	Boolean regResourceCmd;
	Boolean regDefaultSiteCmd;
} module_cfg_rec;
// Revise to use array or hashes for mult-valued config items like regUserSite. See how mod_mime does it.

typedef struct server_cfg_rec {
	char* sig;
    module_cfg_rec *module_cfg;
} server_cfg_rec;

module AP_MODULE_DECLARE_DATA bonjour_module;

static void*	templateIndexMM = NULL;
static int		templateIndexFD = 0;
static struct stat 	templateIndexFinfo;

static apr_status_t unregisterRefs( void* arg );
static void 	registerService( const char* inName, uint16_t *inPort, char* inTxt, 
	server_rec* serverData );
static tDirStatus getDefaultLocalNode ( tDirReference inDirRef, tDirNodeReference *outLocalNodeRef );
static tDirStatus processUsers( tDirReference dirRef, tDirNodeReference defaultLocalNodeRef,
                    const char* whichUsers, const char* regNameFormat, uint16_t *port, cmd_parms *cmd );
static int 		getUserSitePath( const char *inUserName, char *outSitePath, cmd_parms* cmd );
static char* 	extractHTMLTitle( char* fileName, cmd_parms* cmd );
static Boolean	userHasValidCustomizedSite( char* inUserName, cmd_parms* cmd );
static void     registerUser( const char* inUserName, const char* inRegNameFormat,
	uint16_t *inPort, cmd_parms *cmd );
static void 	getTitle( char* inSiteFolder, char* outTitle, cmd_parms* cmd );
static void     registerUsers( const char* whichUsers, const char* regNameFormat, 
	uint16_t *port, cmd_parms *cmd );
static const char *processRegDefaultSite( cmd_parms *cmd, void *dummy, const char *arg );
static const char *processRegUserSite( cmd_parms *cmd, void *dummy, const char *inName,	
	const char *inPort, const char *inHost );
static const char *processRegResource( cmd_parms *cmd, void *dummy, const char *inName, 
	const char *inPath, const char* inPort );
static void 	*bonjourModuleCreateServerConfig( apr_pool_t *p, server_rec *serverData );
static apr_uint32_t cksum(const unsigned char *mem, size_t size);
static const apr_uint32_t crctab[];


/* Unregister all the saved refs, clean up, and clear the array
*/
static apr_status_t unregisterRefs( void* arg ) {
    int i;
	server_rec* serverData = (server_rec*)arg;

	server_cfg_rec *server_cfg = ap_get_module_config(serverData->module_config, &bonjour_module);
    module_cfg_rec *module_cfg = server_cfg->module_cfg;
		
    registrationRec** registrationRecPtrs = NULL;
    registrationRecPtrs = (registrationRec**)module_cfg->registrationRecPtrArray->elts;

    for (i = 0; i < module_cfg->registrationRecPtrArray->nelts; i++) {
        if (!registrationRecPtrs[i])
            continue;
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, registrationRecPtrs[i]->serverData,
            "%s DeRegistering '%s' from pid %d",
            MSG_PREFIX, registrationRecPtrs[i]->name, getpid() );

        if (registrationRecPtrs[i]->serviceRef) {
            DNSServiceRefDeallocate( registrationRecPtrs[i]->serviceRef );
			registrationRecPtrs[i]->serviceRef = 0;
        }
    }
    for (i = 0; i < module_cfg->registrationRecPtrArray->nelts; i++) {
        if (!registrationRecPtrs[i])
            continue;
        free( registrationRecPtrs[i] );
    }
    module_cfg->registrationRecPtrArray->nelts = 0;
    if (templateIndexMM) {
        close( templateIndexFD );
        munmap( templateIndexMM, (size_t)templateIndexFinfo.st_size );
        templateIndexMM = NULL;
    }
	return OK;
}
/*
* Register the service.
* The port is assumed to NOT already be in network byte order.
*/
static void registerService( const char* inName, uint16_t *inPort, char* inTxt, 
	server_rec* serverData ) {
    
	char regName[MAX_NAME_FORMAT];
    registrationRec** 	saveRegistrationRec;
	server_cfg_rec *server_cfg = ap_get_module_config(serverData->module_config, &bonjour_module);
    module_cfg_rec *module_cfg = server_cfg->module_cfg;

    // Allocate a registrationRec structure; this will be used for deregistration.
    registrationRec* registrationRecPtr = (registrationRec*) malloc( sizeof(registrationRec) );
    if (!registrationRecPtr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, serverData,
            "%s Error allocating memory, cannot register '%s'.",
            MSG_PREFIX, inName );
        return;
    }
	
    if (strlen(inName) >= MAX_NAME_FORMAT) {
	        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, serverData,
            "%s Note that service name '%s' will be truncated to %d bytes.",
            MSG_PREFIX, inName, MAX_NAME_FORMAT);
	}
	strlcpy(regName, inName, MAX_NAME_FORMAT);
		
    if (*regName)
        strncpy( registrationRecPtr->name, regName, sizeof(registrationRecPtr->name) );
		
    registrationRecPtr->serverData = serverData;
        
	uint8_t txtLen = (uint8_t)strlen(inTxt);
	TXTRecordRef txtRecord;
	TXTRecordCreate(&txtRecord, 0, NULL);
	TXTRecordSetValue(&txtRecord, "path", txtLen, inTxt);

	DNSServiceErrorType regErr = DNSServiceRegister(&(registrationRecPtr->serviceRef), 0, 0, regName, "_http._tcp", NULL, NULL, htons( *inPort ), TXTRecordGetLength(&txtRecord), TXTRecordGetBytesPtr(&txtRecord), NULL, NULL);

	TXTRecordDeallocate(&txtRecord);
    if (regErr != kDNSServiceErr_NoError || !registrationRecPtr->serviceRef) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, serverData,
            "%s Error %d trying to register '%s' from pid=%d.",
            MSG_PREFIX, regErr, regName, getpid() );
        free( registrationRecPtr );
        return;
    }
    
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, serverData,
        "%s Registered name='%s' txt='%s' port=%" PRIu16 " from pid=%d.",
        MSG_PREFIX, regName, inTxt, *inPort, getpid() );
		
	saveRegistrationRec = (registrationRec **)apr_array_push( module_cfg->registrationRecPtrArray );
	*saveRegistrationRec = registrationRecPtr;
}


/* Find the default local directory node 
*/
static tDirStatus getDefaultLocalNode( tDirReference inDirRef, tDirNodeReference *outLocalNodeRef )
{
    tDirStatus dsStatus 	= eMemoryAllocError;
    apr_uint32_t nodeCount 	= 0;
    tDataBuffer *pTDataBuff     = NULL;
    tDataList *pDataList        = NULL;

    pTDataBuff = dsDataBufferAllocate( inDirRef, 8*1024 );
    if ( pTDataBuff != NULL ) {
        dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, eDSLocalNodeNames, &nodeCount, NULL );
        if ( dsStatus == eDSNoErr ) {
            dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
            if ( dsStatus == eDSNoErr )
                dsStatus = dsOpenDirNode( inDirRef, pDataList, outLocalNodeRef );

            if ( pDataList != NULL ) {
                (void)dsDataListDeallocate( inDirRef, pDataList );
                free( pDataList );
                pDataList = NULL;
            }
        }
        (void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
        pTDataBuff = NULL;
    }
    return dsStatus;
}

/* Call registerUser for each user in node
*/
static tDirStatus processUsers( tDirReference inDirRef, tDirNodeReference inNodeRef, 
	const char* whichUsers, const char* regNameFormat, uint16_t *inPort, cmd_parms *cmd ) {

    tDataBufferPtr      nameDataBufferPtr;
    tDataList           recTypes;
    tDataList           recNames;
    tDataList           attributeList;
    tRecordEntryPtr     recordEntryPtr;
    tAttributeListRef   attrListRef = 0;
    tDirStatus          dsStatus;
    apr_uint32_t		recordCount = 0;
    unsigned long       i;
    tContextData        contextDataRecList = 0;
    tAttributeValueListRef  attrValueListRef;
    tAttributeEntryPtr	attrEntryPtr;
    tAttributeValueEntryPtr attrValueEntryPtr;
    char*               recordName;

    if( !(nameDataBufferPtr = dsDataBufferAllocate( inDirRef, 8*1024 )) ) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
            "%s Failure of dsDataBufferAllocate",
            MSG_PREFIX );
        return -1;
    }
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &recTypes, kDSStdRecordTypeUsers, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
            "%s Failure of dsDataBufferDeAllocate: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
        
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &recNames, kDSRecordsAll, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
        dsDataListDeallocate( inDirRef, &recTypes );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
            "%s Failure of dsBuildListFromStringsAlloc: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
 
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &attributeList, kDSNAttrRecordName,
        kDS1AttrUniqueID, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
        dsDataListDeallocate( inDirRef, &recTypes );
        dsDataListDeallocate( inDirRef, &recNames );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
            "%s Failure of dsBuildListFromStringsAlloc: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
    do { 
        /* 
            While there are more buffers 
        */

        dsStatus = dsGetRecordList( inNodeRef, nameDataBufferPtr, &recNames, eDSiExact, &recTypes, 
            &attributeList, 0, &recordCount, &contextDataRecList );
        if (dsStatus != eDSNoErr) {
            dsDataListDeallocate( inDirRef, &attributeList );
            dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
            dsDataListDeallocate( inDirRef, &recTypes );
            dsDataListDeallocate( inDirRef, &recNames );
            ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                "%s Failure of dsGetRecordList: %d",
                MSG_PREFIX, dsStatus );
            return dsStatus;
        }

        for (i = 1; i <= recordCount; i++) {
            /* 
                For one buffer load of records
            */
            
            dsStatus = dsGetRecordEntry( inNodeRef, nameDataBufferPtr, i, &attrListRef, 
                &recordEntryPtr );
            if (dsStatus != eDSNoErr) { 
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                    "%s Failure of dsGetRecordEntry: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
	// failing with -14061 i=38
                return dsStatus;
            }
    
            // Turn the AttrListRef into an AttrValueListRef
            dsStatus = dsGetAttributeEntry( inNodeRef, nameDataBufferPtr, attrListRef, 1, &attrValueListRef,
                &attrEntryPtr );
            if (dsStatus != eDSNoErr) {
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                dsCloseAttributeList( attrListRef );
                dsDeallocRecordEntry( inDirRef, recordEntryPtr );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                    "%s Failure of dsGetAttributeEntry: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
                return dsStatus;
            }
    
            // Finally get the name attribute
            dsStatus = dsGetAttributeValue( inNodeRef, nameDataBufferPtr, 1, attrValueListRef,
                &attrValueEntryPtr);
            if (dsStatus  != eDSNoErr) {
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                dsDeallocAttributeEntry( inDirRef, attrEntryPtr );
                dsCloseAttributeValueList( attrValueListRef );
                dsCloseAttributeList( attrListRef );
                dsDeallocRecordEntry( inDirRef, recordEntryPtr );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                    "%s Failure of dsGetAttributeValue: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
                return dsStatus;
            }
            
            recordName = attrValueEntryPtr->fAttributeValueData.fBufferData;
            if (!strcmp( whichUsers, ALL_USERS ))
                registerUser( recordName, regNameFormat, inPort, cmd );
            else if (!strcmp( whichUsers, CUSTOMIZED_USERS )) {
                if (userHasValidCustomizedSite( recordName, cmd ))
                    registerUser( recordName, regNameFormat, inPort, cmd );
                else
                    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
                        "%s Skipping non-customized user %s",
                        MSG_PREFIX, recordName );
            }
            else
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                        "%s Unexpected 'RegisterUserSite %s', not registering.",
                        MSG_PREFIX, whichUsers );

            dsDeallocAttributeValueEntry( inDirRef, attrValueEntryPtr );
            dsDeallocAttributeEntry( inDirRef, attrEntryPtr );
            dsCloseAttributeValueList( attrValueListRef );
            dsCloseAttributeList( attrListRef );
            dsDeallocRecordEntry( inDirRef, recordEntryPtr );
        }
    } while (contextDataRecList != 0);
    
    dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
    dsDataListDeallocate( inDirRef, &attributeList );
    dsDataListDeallocate( inDirRef, &recTypes );
    dsDataListDeallocate( inDirRef, &recNames );
    return eDSNoErr;
}

/*
* Read mod_userdir's config structure to determine userdir site path.
* This function includes code from translate_userdir() in mod_userdir from
* apache 1.3.27.
* Returns 0 on success and sets outSitePath to the translated path to the site
* (i.e., it sets outSitePath to the local file path that 
* "http://www.example.com/~username/" translates to).
* Returns 1 if there is a problem or if mod_userdir config does not allow access to 
* user's dir.
*/
static int getUserSitePath( const char *inUserName, char *outSitePath, cmd_parms* cmd ) {

    typedef struct userdir_config {
        int globally_disabled;
        char *userdir;
        apr_table_t *enabled_users;
        apr_table_t *disabled_users;
    } userdir_config;

    userdir_config *s_cfg;
    
    module* userdir_mod = ap_find_linked_module( "mod_userdir.c" );
    if (!userdir_mod)
        userdir_mod = ap_find_linked_module( "mod_userdir_apple.c" );
    if (!userdir_mod) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Mod_userdir not loaded", MSG_PREFIX);
        return 1;
    }

    s_cfg = (userdir_config *) ap_get_module_config( cmd->server->module_config, userdir_mod );
	
    if (!s_cfg) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Mod_userdir config not present", MSG_PREFIX );
        return 1;
    }
    
    if (!s_cfg->userdir) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Null userdir", MSG_PREFIX );
        return 1;
    }

    const char *userdirs = s_cfg->userdir;
    struct stat statbuf;

    /*
     * Skip username if it's in the disabled list.
     */
	if (apr_table_get( s_cfg->disabled_users, inUserName ) != NULL) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
        "%s User '%s' is in disabled users list; not registering", MSG_PREFIX, inUserName );
        return 1;
    }
    /*
     * If there's a global interdiction on UserDirs, check to see if this
     * name is one of the Blessed.
     */
    if (s_cfg->globally_disabled
        && (apr_table_get( s_cfg->enabled_users, inUserName ) == NULL)) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Userdir globally disabled, and user '%s' is not an exception; not registering",
            MSG_PREFIX, inUserName );
        return 1;
    }

    /*
     * Special cases all checked, onward to normal substitution processing.
     */

    while (*userdirs) {
        const char *userdir = ap_getword_conf( cmd->pool, &userdirs );
        char *filename = NULL;
        int is_absolute = ap_os_is_path_absolute( cmd->pool, userdir );

        if (strchr( userdir, '*' )) {
            /* token '*' embedded:
             */
            char *x = ap_getword( cmd->pool, &userdir, '*' );
            if (is_absolute) {
                /* token '*' within absolute path
                 * serves [UserDir arg-pre*][user][UserDir arg-post*]
                 * /somepath/ * /somedir + /~smith -> /somepath/smith/somedir
                 */
                filename = apr_pstrcat( cmd->pool, x, inUserName, userdir, NULL );
            }
            else if (strchr( x, ':' )) {
                /* token '*' within a redirect path
                 * serves [UserDir arg-pre*][user][UserDir arg-post*]
                 * http://server/user/ * + /~smith/foo ->
                 *   http://server/user/smith/foo
                 */
                break;
            }
            else {
                /* Not a redirect, not an absolute path, '*' token:
                 * serves [homedir]/[UserDir arg]
                 * something/ * /public_html
                 * Shouldn't happen, we trap for this in set_user_dir
                 */
                break;
            }
        }
        else if (is_absolute) {
            /* An absolute path, no * token:
             * serves [UserDir arg]/[user]
             * /home + /~smith -> /home/smith
             */
            if (userdir[strlen( userdir ) - 1] == '/')
                filename = apr_pstrcat( cmd->pool, userdir, inUserName, NULL );
            else
                filename = apr_pstrcat( cmd->pool, userdir, "/", inUserName, NULL );
        }
        else if (strchr( userdir, ':' )) {
            /* A redirect, not an absolute path, no * token:
             * serves [UserDir arg]/[user][dname]
             * http://server/ + /~smith/foo -> http://server/smith/foo
             */
            break;
        }
        else {
            /* Not a redirect, not an absolute path, no * token:
             * serves [homedir]/[UserDir arg]
             * e.g. /~smith -> /home/smith/public_html
             */
            struct passwd *pw;
            if ((pw = getpwnam( inUserName ))) {
                filename = apr_pstrcat( cmd->pool, pw->pw_dir, "/", userdir, NULL );
            }
        }

        /*
         * Now see if it exists.
         */
        if (filename && (stat( filename, &statbuf ) != -1)) {
            strncpy( outSitePath, filename, PATH_MAX );
            return 0;
        }
		else
			ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
				"%s Userdir %s does not exist",
				MSG_PREFIX, filename );

		
    }

    return 1;
}

/*
* Most of this code comes from the find_title() function in mod_autoindex in Apache 2.2
*/
static char *extractHTMLTitle( char* fileName, cmd_parms* cmd )
{
    char titlebuf[MAX_STRING_LEN], *find = "<title>";
    apr_file_t *thefile = NULL;
    int x, y, p;
    apr_size_t n;

	if (apr_file_open(&thefile, fileName, APR_READ,
		APR_OS_DEFAULT, cmd->pool) != APR_SUCCESS) {
		return NULL;
	}
	
	n = sizeof(char) * (MAX_STRING_LEN - 1);
	apr_file_read(thefile, titlebuf, &n);
	if (n <= 0) {
		apr_file_close(thefile);
		return NULL;
	}

	titlebuf[n] = '\0';
	for (x = 0, p = 0; titlebuf[x]; x++) {
		if (apr_tolower(titlebuf[x]) == find[p]) {
			if (!find[++p]) {
				if ((p = ap_ind(&titlebuf[++x], '<')) != -1) {
					titlebuf[x + p] = '\0';
				}
				/* Scan for line breaks */
				for (y = x; titlebuf[y]; y++) {
					if ((titlebuf[y] == CR) || (titlebuf[y] == LF)) {
						if (y == x) {
							x++;
						}
						else {
							titlebuf[y] = ' ';
						}
					}
				}
				apr_file_close(thefile);
				return apr_pstrdup(cmd->pool, &titlebuf[x]);
			}
		}
		else {
			p = 0;
		}
	}
	apr_file_close(thefile);
	return NULL;
}

/*
* Figure out the index file at the given Site Folder path, as determined by mod_dir,
* and call a function to extract the HTML title.
*/
static void getTitle( char* inSiteFolder, char* outTitle, cmd_parms* cmd ) {

    typedef struct dir_config_struct {
        apr_array_header_t *index_names;
    } dir_config_rec;

    dir_config_rec *dir_cfg;
    char *dummy_ptr[1];
    char **dirIndexNames;
    int dirIndexNameCount;
    
    module* dir_mod = ap_find_linked_module( "mod_dir.c" );
    if (!dir_mod) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Mod_dir not loaded", MSG_PREFIX);
        outTitle = NULL;
        return;
    }

    dir_cfg = (dir_config_rec *) ap_get_module_config( cmd->server->lookup_defaults, dir_mod );
    if (!dir_cfg) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Mod_dir config not present", MSG_PREFIX );
        outTitle = NULL;
        return;
    }

    if (dir_cfg->index_names) {
		dirIndexNames = (char **)dir_cfg->index_names->elts;
		dirIndexNameCount = dir_cfg->index_names->nelts;
    }
    else {
		dummy_ptr[0] = AP_DEFAULT_INDEX;
		dirIndexNames = dummy_ptr;
		dirIndexNameCount = 1;
    }

    for (; dirIndexNameCount; ++dirIndexNames, --dirIndexNameCount) {
        char *dirIndexName = *dirIndexNames;
        char fullSitePath[PATH_MAX+1];

        strlcpy( fullSitePath, inSiteFolder, sizeof(fullSitePath) );
        strlcat( fullSitePath, "/", sizeof(fullSitePath) );
        strlcat( fullSitePath, dirIndexName, sizeof(fullSitePath) );
        // Use the first index file of type text/html. Should fix this to use mime type
        if ((strlen( dirIndexName ) > 5 && !strcmp( &(dirIndexName[strlen( dirIndexName ) - 5]), ".html" ))
            || (strlen( dirIndexName ) > 4 && !strcmp( &(dirIndexName[strlen( dirIndexName ) - 4]), ".htm" ))) {
            char* titleStr = extractHTMLTitle( fullSitePath, cmd );
            if (!titleStr || !strcmp( titleStr, "" )) {
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, cmd->server,
                    "%s Index file %s has no title.",
                    MSG_PREFIX, fullSitePath );
                outTitle = NULL;
                return;
            }
            // Title found by extractHTMLTitle could be way longer than what we want to use for Bonjour; truncate.
            strncpy( outTitle, titleStr, TITLE_MAX );
            return;
        }
    }

    outTitle = NULL;
}

/* Compute checksum with same result as cksum(1)
*/
static apr_uint32_t cksum(const unsigned char *mem, size_t size)
{
	size_t  i;
	apr_uint32_t   c, s = 0;
    for (i = size; i > 0; --i) {
        c = (unsigned)(*mem++);
        s = (s << 8) ^ crctab[(s >> 24) ^ c];
    }

    while (size != 0) {
        c = size & 0377;
        size >>= 8;
        s = (s << 8) ^ crctab[(s >> 24) ^ c];
    }
    return ~s;
}

/* 
* Compare user's index.html file with the one in the installed customer template (in case the
* template itself was customized by an administrator), and the compare the user's 
* index.html cksum with that of old templates.
* Return true only if able to confirm that site fails to match any known templates.
*/
static Boolean	userHasValidCustomizedSite( char* inUserName, cmd_parms* cmd ) {
    char	site_path[PATH_MAX+1];
    Boolean		filesDiffer;
    int 	userIndexFD;
    struct stat userIndexFinfo;
    void*	userIndexMM;
#define TEMPLATE_PATH "/System/Library/User Template/English.lproj/Sites/index.html"
	apr_uint32_t knownTemplateSums[] = {
	/* Jaguar6C115 English  */ 3903344769U, 
	/* Jaguar6C115 Japanese */ 553143326U, 
	/* Jaguar6C115 French   */ 230947327U, 
	/* Jaguar6C115 German   */ 1336227689U, 
	/* Jaguar6C115 Italian  */ 1554190806U, 
	/* Jaguar6C115 Spanish  */ 2132327898U, 
	/* Jaguar6C115 Dutch    */ 2599945973U, 

	/* Panther7B85 English  */ 1127019111U, 
	/* Panther7B85 Japanese */ 3264273210U,
	/* Panther7B85 French   */ 1533594624U, 
	/* Panther7B85 German   */ 696810959U,
	/* Panther7B85 Italian  */ 1738661094U, 
	/* Panther7B85 Spanish  */ 251789223U, 
	/* Panther7B85 Dutch    */ 901476833U, 
	
	/* Tiger8A428 French    */ 308734950U,
	/* Other Tiger sums appear same as Panther */

	/* Leopard9A556 English  */ 3807920194U,
	/* Leopard9A556 Japanese */ 1095971935U, 
	/* Leopard9A556 French   */ 706365046U, 
	/* Leopard9A556 German   */ 472546109U, 
	/* Leopard9A556 Italian  */ 1624979716U, 
	/* Leopard9A556 Spanish  */ 2075108216U, 
	/* Leopard9A556 Dutch    */ 3080089696U, 
	
	/* SnowLeopard10A354 English */ 3347385980U,
	/* SnowLeopard10A354 Japanese */ 560382875U, 
	/* SnowLeopard10A354 French   */ 1071842387U, 
	/* SnowLeopard10A354 German   */ 2756617632U, 
	/* SnowLeopard10A354 Italian same as Leopard */
	/* SnowLeopard10A354 Spanish  */ 1593960486U, 
	/* SnowLeopard10A354 Dutch    */ 3594138231U, 

	0U};
	apr_uint32_t checkSum;
	Boolean validTemplate = TRUE;
	if (templateIndexMM == NULL || templateIndexMM == (void*) -1 ) {
        if (stat( TEMPLATE_PATH, &templateIndexFinfo ) == -1) {
            ap_log_error( APLOG_MARK, APLOG_WARNING, 0, cmd->server,
                "%s Cannot stat template index file '%s'.",
                MSG_PREFIX, TEMPLATE_PATH );
            validTemplate = FALSE;
        }
		else {
			templateIndexFD = open( TEMPLATE_PATH, O_RDONLY, 0 );
			templateIndexMM = mmap( NULL, (size_t)templateIndexFinfo.st_size, PROT_READ, MAP_SHARED, templateIndexFD, (off_t)0 );
			if ( templateIndexMM == (void*) -1) {
				ap_log_error( APLOG_MARK, APLOG_WARNING, 0, cmd->server,
					"%s Cannot read template index file '%s'.",
					MSG_PREFIX, TEMPLATE_PATH );
				close( userIndexFD );
				validTemplate = FALSE;
			}
		}
    }

    if (getUserSitePath( inUserName, site_path, cmd )) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Skipping user '%s' - unable to confirm userdir site.",
            MSG_PREFIX, inUserName );
        return FALSE;
    }
    strlcat( site_path, "/index.html", sizeof(site_path) );
	if (stat( site_path, &userIndexFinfo ) == -1) {
        ap_log_error( APLOG_MARK,  - APLOG_NOERRNO|APLOG_WARNING, 0, cmd->server,
            "%s Skipping user '%s' - cannot read index file '%s'.",
            MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }
    if ((userIndexFinfo.st_mode & S_IFMT) != S_IFREG) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, cmd->server,
            "%s Skipping user '%s' - index file %s isn't a regular file.",
            MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }
	
	if (userIndexFinfo.st_size == 0) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, cmd->server,
			"%s Skipping user '%s' - index file %s has zero length.",
			MSG_PREFIX, inUserName, site_path );
		return FALSE;
	}
	
	if (userIndexFinfo.st_size != templateIndexFinfo.st_size && userIndexFinfo.st_size > 10000) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, cmd->server,
			"%s Concluding user '%s' has customized site - size %qd differs from template size %qd "
			"and user site is bigger than known templates.",
			MSG_PREFIX, inUserName, userIndexFinfo.st_size, templateIndexFinfo.st_size );
		return TRUE;
	}

    userIndexFD = open( site_path, O_RDONLY, 0 );
	if (userIndexFD == -1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, cmd->server,
            "%s Skipping user '%s' - cannot open index file '%s'.",
             MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }

    userIndexMM = mmap( NULL, (size_t)userIndexFinfo.st_size, PROT_READ, MAP_SHARED, userIndexFD, (off_t)0);
	if ( userIndexMM == (void*) -1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, cmd->server,
            "%s Skipping user '%s' - cannot read index file '%s'.",
             MSG_PREFIX, inUserName, site_path );
        close( userIndexFD );
        return FALSE;
    }
	
	filesDiffer = validTemplate ? (memcmp( templateIndexMM, userIndexMM, (size_t)userIndexFinfo.st_size ) != 0) : TRUE;
	checkSum = cksum(userIndexMM, userIndexFinfo.st_size);
    munmap( userIndexMM, (size_t)userIndexFinfo.st_size );
    close( userIndexFD );
    if (filesDiffer) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, cmd->server,
			"%s User '%s' does not match current template; checking old ones.",
			MSG_PREFIX, inUserName );
		int i = 0;
		while (knownTemplateSums[i] != 0) {
			if (knownTemplateSums[i] == checkSum) {
				ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, cmd->server,
					"%s User '%s' matched an oldtemplate %" PRIu32 ".",
					MSG_PREFIX, inUserName, checkSum );
				return FALSE;
			}
			i++;
		}

        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, cmd->server,
            "%s Concluding user '%s' has customized site - contents differ.",
            MSG_PREFIX, inUserName );
        return TRUE;
    }
    return FALSE;
}

/*
* Register the sites folder for specified user.
*/
static void registerUser( const char* inUserName, const char* inRegNameFormat, 
	uint16_t *inPort, cmd_parms* cmd ) {
	
    char		txt[TXT_MAX+1];
    char		site_path[PATH_MAX+1];
    char		title[TITLE_MAX+1];
    char		regName[REGNAME_MAX+1];
	char		regNameFormat[REGNAME_MAX+1];
    struct passwd*	pw = getpwnam( inUserName );

    if (!pw) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
        "%s Skipping user '%s' - no pw entry.",
        MSG_PREFIX, inUserName );
        return;
    }
        
    if ((int)pw->pw_uid < 100) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Skipping user '%s' - uid '%d' indicates system user.",
            MSG_PREFIX, pw->pw_name, pw->pw_uid );
        return;
    }
    
    if (getUserSitePath( inUserName, site_path, cmd )) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, cmd->server,
            "%s Skipping user '%s' - unable to confirm userdir site.",
            MSG_PREFIX, pw->pw_name );
        return;
    }
     
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, cmd->server,
	"%s Confirmed user '%s' has existing userdir site directory '%s'.",
	MSG_PREFIX, pw->pw_name, site_path );
    
	strncpy(regNameFormat, inRegNameFormat ? inRegNameFormat : DEFAULT_NAME_FORMAT, sizeof(regNameFormat));
	snprintf( txt, sizeof(txt), "~%s/", inUserName );

	if (!strcasecmp( regNameFormat, "longname" ))
        // On X, pw_gecos always contains the long name.
        strncpy( regName, pw->pw_gecos, sizeof(regName) );
	else if (!strcasecmp( regNameFormat, "title" )) {
        getTitle( site_path, (char*)&title, cmd );
        if (*title && strcmp( title, ""  ))
            strncpy( regName, title, sizeof(regName) );
        else
            strncpy( regName, pw->pw_gecos, sizeof(regName) );
    }
	else if (!strcasecmp( regNameFormat, "longname-title" )) {
        getTitle( site_path, (char*)&title, cmd );
        if (*title && strcmp( title, ""  ))
            snprintf( regName, sizeof(regName), "%s - %s", pw->pw_gecos, title );
        else
            strncpy( regName, pw->pw_gecos, sizeof(regName) );
    }
	else {
        // Format string
        int len = strlen(regNameFormat);
        int i;
        int j = 0;
        char uidStr[8];
        CFStringRef hostRef;
        char hostStr[65];

        bzero( regName, sizeof(regName) );
        for (i = 0; i < len; i++) {
            if (regNameFormat[i] == '%') {
                switch (regNameFormat[i + 1]) {
                    case 't':	// %t - HTML title
                        getTitle( site_path, (char*)&title, cmd );
                        if (*title && strcmp( title, "" )) {
                            strncat( regName, title, sizeof(regName) );
                            j = j + strlen( title );
                        }
                        i++;
                        break;
                    case 'l':	// %l - long name
                        strncat( regName, pw->pw_gecos, sizeof(regName) );
                        j = j + strlen( pw->pw_gecos );
                        i++;
                        break;
                    case 'n':	// %n - short name
                        strncat( regName, pw->pw_name, sizeof(regName) );
                        j = j + strlen( pw->pw_name );
                        i++;
                        break;
                    case 'u':	// %u - uid
                        snprintf( uidStr, sizeof(uidStr), "%d", pw->pw_uid );
                        strncat( regName, uidStr, sizeof(regName) );
                        j = j + strlen( uidStr );
                        i++;
                        break;
                    case 'c':	// %c - computer name
                        hostRef = SCDynamicStoreCopyComputerName( NULL, NULL );
                        CFStringGetCString( hostRef, hostStr, sizeof(hostStr), kCFStringEncodingMacRoman ); 
                        strncat( regName, hostStr, sizeof(regName) );
                        j = j + strlen( hostStr );
                        i++;
                        break;
                    default:
                        regName[j++] = regNameFormat[i];
                }
            }
            else
                regName[j++] = regNameFormat[i];
        }
    }
	/* max length of regname is MAX_NAME_FORMAT */
    registerService( regName, inPort, txt, cmd->server );
    return;
}

/* Find the local Directory node and call processUsers
*/
static void registerUsers( const char* whichUsers, const char* regNameFormat, uint16_t *port, cmd_parms *cmd ) {

    tDirStatus dsStatus;
    tDirReference dirRef;
    tDirNodeReference defaultLocalNodeRef;

    dsStatus = dsOpenDirService( &dirRef );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
            "%s Unable to open Directory Services (error = %d).", MSG_PREFIX, dsStatus );
        return;
    }
    
    dsStatus = getDefaultLocalNode( dirRef, &defaultLocalNodeRef );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                "%s Unable to open Directory Services local node (error = %d).", MSG_PREFIX, dsStatus );
        (void)dsCloseDirService( dirRef );
        return;
    }
    
    dsStatus = processUsers( dirRef, defaultLocalNodeRef, whichUsers, regNameFormat, port, cmd );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server,
                "%s Unable to process all users (error = %d).", MSG_PREFIX, dsStatus );
        (void)dsCloseDirService( dirRef );
        return;
    }

    (void)dsCloseDirNode( defaultLocalNodeRef );
    (void)dsCloseDirService( dirRef );
}

/*
 * Process the RegisterDefaultSite directive.
 * RegisterDefaultSite [port | main]
 */
static const char *processRegDefaultSite( cmd_parms *cmd, __attribute__((unused)) void *dummy, 
	const char *arg ) {


	/* Ensure that we only init once. */
    void *data;
    const char *userdata_key = "mod_bonjour_register_default_site";
	apr_pool_t *pPool = cmd->server->process->pool;

    apr_pool_userdata_get(&data, userdata_key, pPool);
    if (!data) {
        apr_pool_userdata_set((const void *) 1, userdata_key,
                              apr_pool_cleanup_null, pPool);
        return NULL;
    }


	server_cfg_rec *server_cfg = ap_get_module_config(cmd->server->module_config, &bonjour_module);
    module_cfg_rec *module_cfg = server_cfg->module_cfg;
    
    uint16_t port = DEFAULT_HTTP_PORT;
    int	err = 0;

    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
    
    char* portArg = ap_getword_conf( cmd->pool, &arg );

    if (portArg && strcmp( portArg, "" )) {
        if (strlen( portArg ) > 5)
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );

        if (!strcasecmp( portArg, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( portArg, "%" PRIu16, &port );
            if (!err)
                return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
        }
    }
	
	module_cfg->regDefaultSiteCmd = TRUE;
	module_cfg->port = port;
	

    return NULL;
}
/*
 * Process the RegisterUserSite directive. Just save config
 * RegisterUserSite username | all-users 
 *	| customized-users [ longname | title | longname-title [port | main ]]
 */
static const char *processRegUserSite( cmd_parms *cmd, __attribute__((unused)) void *dummy, 
	const char *inName, const char *inRegNameFormat, const char *inPort ) {
    uint16_t port = DEFAULT_HTTP_PORT;
    int err = 0;
	server_cfg_rec *server_cfg = ap_get_module_config(cmd->server->module_config, &bonjour_module);
	if (!server_cfg) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server, 
            "%s Internal error - Could not retrieve server configuration", MSG_PREFIX );
		return "no server_cfg";
	}
		
    module_cfg_rec *module_cfg = server_cfg->module_cfg;
	if (!module_cfg) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, cmd->server, 
            "%s Internal error - Could not retrieve module configuration", MSG_PREFIX );
		return "no module_cfg";
	}
    
    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
   
    if (!inName || !strcmp( inName, "" )) {
	return apr_pstrcat( cmd->pool, MSG_PREFIX, "Name argument missing", NULL );
    }
    if (strlen( inName ) > MAXLOGNAME) {
	return apr_pstrcat( cmd->pool, MSG_PREFIX, "Name argument too long", NULL );
    }
    if (inRegNameFormat && strlen( inRegNameFormat ) > MAX_NAME_FORMAT) {
	return apr_pstrcat( cmd->pool, MSG_PREFIX, "Name format argument too long", NULL );
    }
    if (inPort && strcmp( inPort, "" )) {
        if (strlen( inPort ) > 5)
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );
        
        if (!strcasecmp( inPort, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( inPort, "%" PRIu16, &port );
            if (!err)
                return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
        }
    }

	/* To do: maintain array of users */

	module_cfg->regUserSiteCmd = TRUE;
	module_cfg->regName = apr_pstrdup( cmd->pool, inName );
	module_cfg->regNameFormat = apr_pstrdup( cmd->pool, inRegNameFormat  );
	module_cfg->port = port;
        
    return NULL;
}

/*
 * Process the RegisterResource directive.
 * RegisterResource name path [port | main]
 */
static const char *processRegResource( cmd_parms *cmd, __attribute__((unused)) void *dummy, 
	const char *inName, const char *inPath, const char* inPort ) {

    uint16_t port = DEFAULT_HTTP_PORT;
    int err = 0;

	server_cfg_rec *server_cfg = ap_get_module_config(cmd->server->module_config, &bonjour_module);
    module_cfg_rec *module_cfg = server_cfg->module_cfg;

    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
    
    if (inName && strcmp(inName, "" ))
        if (strlen( inName ) > REGNAME_MAX)
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Name argument too long", NULL );
            
    if (inPath && strcmp(inPath, "" )) {
        if (strlen( inPath ) > PATH_MAX)
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Path argument too long to be a path", NULL );
        if (strlen( inPath ) > TXT_MAX - 6) // allow space for "path="
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Path argument too long to use with Bonjour", NULL );
    }
    
    if (inPort && strcmp( inPort, "" )) {
        if (strlen( inPort ) > 5)
            return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );

        if (!strcasecmp( inPort, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( inPort, "%" PRIu16, &port );
            if (!err)
                return apr_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
        }
    }
	
	/* To do: maintain array of resources */
	
	module_cfg->regResourceCmd = TRUE;
	module_cfg->regName = apr_pstrdup( cmd->pool, inName );
	module_cfg->regText = apr_pstrdup( cmd->pool, inPath );
	module_cfg->port = port;

    return NULL;
}


static command_rec bonjourModuleCmds[]=
    {
    AP_INIT_RAW_ARGS("RegisterDefaultSite", 
		processRegDefaultSite, 
		NULL, 
		RSRC_CONF,
		"Optionally, specify a port or keyword main; defaults to 80"
	),
	AP_INIT_TAKE123("RegisterUserSite", 
		processRegUserSite, 
		NULL, 
		RSRC_CONF,
		"Specify a user name or the keyword all-users or customized-users, " 
		"optionally followed by keyword longname, title, or longname-title, "
		"optionally followed by a port or keyword main; default is 80"
	),
    AP_INIT_TAKE23("RegisterResource",
		processRegResource, 
		NULL, 
		RSRC_CONF,
		"Specify a name under which to register, and a path, " 
		"optionally followed by a port or keyword main; default is 80"
	),
    {NULL, {NULL}, NULL, 0, NO_ARGS, NULL}
    };


static int bonjourPostConfig( apr_pool_t *p, __attribute__((unused)) apr_pool_t *plog, 
	__attribute__((unused)) apr_pool_t *ptemp, server_rec *serverData ) {
/* 
	Called during bootstrap and again for real a second time.
	Walk through the configuration and perform the appropriate registrations.
*/
	server_cfg_rec *server_cfg = ap_get_module_config(serverData->module_config, &bonjour_module);
	if (!server_cfg)
		return OK;
    module_cfg_rec *module_cfg = server_cfg->module_cfg;
	
    if (!module_cfg) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, serverData,
            "%s module config not set pid=%d.", MSG_PREFIX, getpid());
		return !OK;
	}
	
	apr_pool_t *pPool = serverData->process->pool;
		/* Ensure that we only init once. */
    void *data;
    const char *userdata_key = "mod_bonjour_post_config";

    apr_pool_userdata_get(&data, userdata_key, pPool);
    if (!data) {
        apr_pool_userdata_set((const void *) 1, userdata_key,
                              apr_pool_cleanup_null, pPool);
        return OK;
    }

	cmd_parms fake_cmd;
	fake_cmd.server = serverData;
	fake_cmd.pool = p;

	/* To do: Handle array of users or resources */
	if (module_cfg->regUserSiteCmd) {
		//registerUser( module_cfg->regName, module_cfg->regNameFormat, module_cfg->port, &fake_cmd );
		
		if (!strcasecmp( module_cfg->regName, ALL_USERS ) || !strcasecmp( module_cfg->regName, CUSTOMIZED_USERS ) ) {
			registerUsers( module_cfg->regName,
				module_cfg->regNameFormat ? module_cfg->regNameFormat : DEFAULT_NAME_FORMAT, 
				&module_cfg->port, &fake_cmd );
		}
		else // it's an individual user
            registerUser( module_cfg->regName, module_cfg->regNameFormat, &module_cfg->port, &fake_cmd );
	}
	
	if (module_cfg->regResourceCmd)
        registerService( module_cfg->regName, &module_cfg->port, module_cfg->regText, serverData );

	if (module_cfg->regDefaultSiteCmd)
        registerService( "", &module_cfg->port, "", serverData );

	return OK;
}

/*
	Create server_cfg and module_cfg
*/
static void *bonjourModuleCreateServerConfig( apr_pool_t *p, server_rec *serverData ) {
#define BONJOUR_KEY "mod_bonjour"
    module_cfg_rec *module_cfg;
	apr_pool_t *pPool = serverData->process->pool;
	void *void_module_cfg;
	
	/* Ensure that we only init once. */
    void *data;
    const char *userdata_key = "mod_bonjour_config";

    apr_pool_userdata_get(&data, userdata_key, pPool);
    if (!data) {
        apr_pool_userdata_set((const void *) 1, userdata_key,
                              apr_pool_cleanup_null, pPool);
        return OK;
    }
	
	server_cfg_rec *server_cfg = apr_palloc(p, sizeof(*server_cfg));
	apr_pool_cleanup_register( p, (void *)serverData, unregisterRefs, apr_pool_cleanup_null );

    apr_pool_userdata_get(&void_module_cfg, BONJOUR_KEY, pPool);
    if (void_module_cfg) {
		server_cfg->module_cfg = (module_cfg_rec*)void_module_cfg;
        return server_cfg; /* reused for lifetime of the server */
    }

    /*
     * allocate an own subpool which survives server restarts
     */
    module_cfg = (module_cfg_rec *)apr_palloc(pPool, sizeof(*module_cfg));
	/*
	 * Initialize per-module configuration
	 */
	module_cfg->pPool = pPool;
	module_cfg->registrationRecPtrArray = apr_array_make( pPool, 0, sizeof(registrationRec*) );
	module_cfg->regUserSiteCmd = FALSE;
	module_cfg->regResourceCmd = FALSE;
	module_cfg->regDefaultSiteCmd = FALSE;

	apr_pool_userdata_set(module_cfg, BONJOUR_KEY, apr_pool_cleanup_null, pPool);
    server_cfg->module_cfg = module_cfg;

    return (void *) server_cfg;
}

static void RegisterHooks(__attribute__((unused)) apr_pool_t *pPool) {

    ap_hook_post_config(bonjourPostConfig, NULL, NULL, APR_HOOK_MIDDLE);
}

/*
 * Module dispatch table.
 */
module AP_MODULE_DECLARE_DATA bonjour_module = {
        STANDARD20_MODULE_STUFF,
        NULL,									/* dir config creater */
        NULL,                                   /* dir merger --- default is to override */
        bonjourModuleCreateServerConfig,         /* server config */
        NULL,                                   /* merge server config */
        bonjourModuleCmds,                        /* command table */
        RegisterHooks                          /* hook registration */
};

static const apr_uint32_t crctab[] = {
        0x0,
        0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
        0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
        0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
        0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
        0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
        0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
        0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
        0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
        0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
        0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
        0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
        0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
        0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
        0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
        0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
        0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
        0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
        0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
        0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
        0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
        0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
        0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
        0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
        0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
        0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
        0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
        0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
        0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
        0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
        0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
        0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
        0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
        0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
        0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
        0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
        0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
        0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
        0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
        0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
        0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
        0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
        0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
        0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

