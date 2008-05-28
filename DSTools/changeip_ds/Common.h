#ifndef __COMMON_H__
#define __COMMON_H__


#include <stdio.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

#include <PasswordServer/AuthFile.h>
#include "DSUtilsDeleteRecords.h"
#include "DSUtilsAuthAuthority.h"

#define kSASLPrefixStr			"(SASL "
#define kSASLKnownMechCount		10	
#define kSASLFilterMechCount	5
#define kIsAdminPolicyStr		"isAdminUser="

#define kPWConfigDefaultRecordName		"passwordserver"
#define kPWConfigRecordPrefix			"passwordserver_"
#define kPWReplicaFileSavedName			"/var/db/authserver/authserverreplicas.saved"
#define kPWFileSavedName				"/var/db/authserver/authserversaved"

#define MYDEBUG 1
#if MYDEBUG
#define debug(A, args...)					if (gLogFileDesc!=NULL) {nest_log_time( gLogFileDesc ); fprintf(gLogFileDesc, (A), ##args);}
#define debugcat(A, args...)				if (gLogFileDesc!=NULL) fprintf(gLogFileDesc, (A), ##args)
#define debugerr(ERR, A, args...)			if ((ERR!=0) & (gLogFileDesc!=NULL)) {nest_log_time( gLogFileDesc ); fprintf(gLogFileDesc, (A), ##args);}
#else
#define debug(A, ...)
#define debugerr(ERR, A, args...)
#endif

#define DEBUGSPECIAL	0
#if (DEBUGSPECIAL && MYDEBUG)
#define debugspecial(A, args...)			if (gLogFileDesc!=NULL) {nest_log_time( gLogFileDesc ); fprintf(gLogFileDesc, (A), ##args);}
#else
#define debugspecial(A, ...)
#endif


__BEGIN_DECLS
    extern FILE *gLogFileDesc;
__END_DECLS

typedef enum SetupActionType {
	kSetupActionGeneral = 0,
	kSetupActionSetupReplica = 1,
	kSetupActionRevokeReplica = 2
};


__BEGIN_DECLS
	extern void		get_myaddress		( struct sockaddr_in* server_addr );
	pid_t			ProcessRunning		( const char *inProcName );
	char *			ProcessName			( pid_t inPID );
__END_DECLS

void		nest_log_time( FILE *inFile );
void		UpdateReplicaList(const char *inOldIP, const char* inNewIP);
void		GetPWServerAddresses( char *outAddressStr );
void		SetPWServerAddress	( const char *inUsername, const char *inPassword, char *inOutAddressStr, bool inHost, const char *inReplicaList, const char *inReplicaRecordName, SetupActionType action = kSetupActionGeneral );
long		RemovePWSListAttributeAndPWSReplicaRecord( const char *inUsername, const char *inPassword );
void		GetMyAddressAsString	( char *outAddressStr );
long		SetPWConfigAttribute( const char *inRecordName, const char *inAttributeName, const char *inValue, const char *inUser = NULL, const char *inPassword = NULL, DSUtils *ldapNode = NULL );
long		SetPWConfigAttributeLocal( const char *inRecordName, const char *inAttributeName, const char *inValue, bool inCreateRecord = true, bool inParentOnly = false );
long		SetPWConfigAttributeLDAP( const char *inRecordName, const char *inAttributeName, const char *inValue, const char *inUser = NULL, const char *inPassword = NULL, DSUtils *ldapNode = NULL );


#endif

