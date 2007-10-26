/*
	File:		Main.h

	Product:	NeST (NetInfo Setup Tool)

	Version:	1.1

	Copyright:	© 2000-2001 by Apple Computer, Inc., all rights reserved.
*/


#ifndef __Main_h__
#define __Main_h__	1

#include <PasswordServer/ReplicaFile.h>
#include "Common.h"
#include "MainDefs.h"

const char *kNetInfoPath	=  "%s/Network/Global/NetInfo";
const char *kBindingPath	=  "%s/Network/Global/NetInfo/BindingMethods";
const char *kSrvrAddrPath	=  "%s/Network/Global/NetInfo/ServerAddresses";
const char *kSrvrTagsPath	=  "%s/Network/Global/NetInfo/ServerTags";
const char *kInactivePath	=  "%s/Network/Global/NetInfo/__INACTIVE__";
const char *kHostName		= "/machines/%s";
const char *kLocalHost		= "/machines/localhost";
const char *kHostNetwork	= "%s/network";
const char *kNetworkHost	= "localhost/network";
const char *kServersHost	= "%s/local";
const char *kServersLocal	= "localhost/local";

const char * kDHCPBinding =
"<array>\
	<string>DHCP</string>\
</array>";

const char * kNoBinding =
"<array>\
</array>";

const char * kBroadcastBinding =
"<array>\
	<string>Broadcast</string>\
</array>";

const char *kBroadcastPlusDHCPBinding = 
"<array>\
	<string>DHCP</string>\
	<string>Broadcast</string>\
</array>";

const char *kAllBindings = 
"<array>\
    <string>Manual</string>\
	<string>DHCP</string>\
	<string>Broadcast</string>\
</array>";

const char * kStaticIPBinding =
"<array>\
	<string>Manual</string>\
</array>";

const char * kIPAddress =
"<array>\
	<string>%s</string>\
</array>";

const char * kInactive =
"<dict>\
	<key>__INACTIVE__</key>\
	<integer>1</integer>\
</dict>";

const char * kServerTag =
"<array>\
	<string>network</string>\
</array>";

const char * kServerTagPrefix = "<array> <string>";
const char * kServerTagPostfix = "</string> </array>";


static void		_usage				( FILE *inFile, const char *inArgv0 );
static void		_checkUser			( FILE *inFile, const char *inArgv0, bool inQuiet, bool needAuthorization );
static FILE		*log_popen			( const char *inCmd, const char *inMode );
static int		GetCommandID		( int argc, char * const *argv, bool & needAuthorization, int *outCommandID );
static int		HostPasswordServer	( int argc, char * const *argv, bool quiet = false );
static int		HostPasswordServerInParent		( int argc, char * const *argv, bool quiet = false );
static void		SetRealmToHostnameIfPresent		( void );
static int		GetHostFromSystemConfiguration	( char *inOutHostStr, size_t maxHostStrLen );
static int		SetupPasswordServerConfigRecord	( ReplicaFile *inReplicaFile,
													const char *inDirAdmin,
													const char *inDirPassword,
													char *inProvidedIPBuff,
													bool inIPProvided );

static int		SetupReplica		( int argc, char * const *argv );
static int		StandAlone			( int argc, char * const *argv );
static int		Rekey				( const char *inBitCountStr );
static int		MigrateIP			( int argc, char * const *argv );
static int		RevokeReplica		( int argc, char * const *argv );
static bool		ArgsAreIPAddresses	( char * const *argv, int firstArg, int lastArg );
static int		PromoteToMaster		( void );
static int		LocalToShadowHash	( void );
static int		LocalToLDAP			( const char *inAdminUser, bool quiet );
static unsigned long MigrateLocalToLDAP( const char *inAdminUser, const char *inAdminPass );
static void		_version			( FILE *inFile );
static void		_appleVersion		( FILE *inFile );
static long		_localonly			( void );
static long		_nonetinfo			( void );
static void		_destroyParent		( const char *inTag );
static void		_destroyOrphanedParent	( const char *inTag );
static void		_removeBindings 	( SCPreferencesRef scpRef );
static void		_setInactiveFlag	( SCPreferencesRef scpRef );
PWServerStyle	GetPWServerStyle	( void );
static void		DeletePWServerRecords( bool removeDefaultIPRec = true );
static bool		DatabaseExists		( void );
static bool 	MakeReplica			( const char *inUserName, const char *inPubicKey, const char *inPrivateKey, long inPrivateKeyLen, unsigned long inFirstSlot, unsigned long inNumSlots, const char *inReplicaName );
static bool		GetPasswordServerKey	( char *key, long maxlen );

static void		ConvertUser				( const char *currentPasswordServerKey,
											const char *userName,
											const char *password,
											bool isHashData,
											bool isAdminUser,
											const char *dirAdmin,
											const char *dirAdminPass );
											
static void		AddUserToNewDatabase	( const char *currentPasswordServerKey,
											const char *userName,
											PWFileEntry *pwRec,
											const char *dirAdmin = NULL,
											const char *password = NULL,
											bool keepSlotNumber = false );
											
static unsigned long ConvertLocalUsers	( ConvertTarget inConvertToWhat = kConvertToNewDatabase, const char *inParam1 = NULL, const char *inNewIP = NULL, const char *inAdminUser = NULL, const char *inAdminPass = NULL );

static long		ConvertLDAPUsers		( const char *inLDAPAdmin, const char *inAdminPass, ConvertTarget inConvertToWhat = kConvertToNewDatabase, const char *inOldIP = NULL, const char *inNewIP = NULL );
	
static long		GetDataFromDataBuff		( tDirReference dsRef,
											tDirNodeReference inNodeRef,
											tDataBuffer *tDataBuff,
											tRecordEntry *pRecEntry,
											tAttributeListRef attrListRef,
											char **outAuthAuthority,
											char **outRecordName );
										
static bool		GetPasswordDataForID( FILE *fp, const char *authAuthorityStr, PWFileEntry *outUserRecord );
static bool		GetObfuscatedPasswordForID( FILE *fp, const char *inUserID, PWFileEntry *outUserRecord );

static bool		NewPWServerAdminUser( const char *inUserName, const char *inPassword );
static bool		NewPWServerAddUser	( const char *inUserName, const char *inPassword, bool admin );
static long		NewPWServerAdminUserRemote( const char *inServerAddress,
                                            const char *inAdminID,
                                            const char *inAdminPassword,
                                            const char *inNewAdminName,
                                            const char *inNewAdminPassword,
                                            char *outID );

 Boolean	GetAuthAuthority	( const char *inServerAddress,
                                        const char *inUsername,
                                        const char *inPassword,
                                        char *inOutUserID );

static void		SetAuthAuthority	( const char *inServerAddress,
                                        const char *inUsername,
                                        const char *inPassword,
                                        char *inOutUserID,
										const char *dirAdmin = NULL,
										const char *dirAdminPass = NULL );

static void		SetAuthAuthorityToBasic( const char *inUsername, const char *inPassword );

static void		HandleAuthAuthority	( const char *inServerAddress,
                                        const char *inUsername,
                                        const char *inPassword,
                                        char *inOutUserID,
                                        Boolean inVerifyOnly,
										Boolean inSetToBasic = false,
										const char *dirAdmin = NULL,
										const char *dirAdminPass = NULL );
                                        
static void		GetSASLMechs		( char *outSASLStr, Boolean filterMechs );
static void		SetSASLMechs		( int argc, char * const *argv );

static long		VerifyAdmin			( const char *inServerAddress,
                                        const char *inUserName,
                                        const char *inPassword,
										char *outID );

static long		VerifyUser			( const char *inServerAddress,
                                        const char *inUserName,
                                        const char *inPassword,
										bool inCheckAllUsers,
                                        char *outID );

static long		VerifyUserID		( const char *inServerAddress,
										const char *inUserID,
										const char *inPassword,
										bool inVerifyPassword,
										bool *outIsAdmin );

static long		GetReplicaSetup		( const char *inServerAddress,
										const char *inUserName,
										const char *inPassword,
										char *outRSAPublicKey,
										char *outRSAPrivateKey,
										long *outPrivateKeyLen,
										char **outReplicaList );

static bool		LaunchdRunning			( unsigned long *outPID );
static bool		ResumePasswordServer	( void );
static void		PausePasswordServer		( void );
static bool		StopPasswordServer		( void );
static Boolean	PasswordServerRunning	( unsigned long *outPID );
static Boolean	PasswordServerListening ( int secondsToWait );
static int 		GetServerBaseInfo		( const char *inArg, char *outArg );
static int		GetCmdLinePass			( const char *inArg, char **outPass, long *outPassLen );

#endif



