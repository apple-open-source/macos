/*
	File:		Main.cpp

	Product:	NeST (NetInfo Setup Tool)

	Version:	1.1

	Copyright:	� 2000-2001 by Apple Computer, Inc., all rights reserved.
*/


#include <sys/types.h>
#include <sys/uio.h>
#include <pwd.h>
#include <signal.h>				// for signal handling
#include <string.h>				// for memset
#include <stdlib.h>				// for exit
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <sysexits.h>

#include <stdio.h>
#include <stdint.h>
#include <paths.h>

#include "Main.h"
#include "NetworkUtilities.h"
#include "WatchdogSupport.h"
#include <Foundation/Foundation.h>
#include <PasswordServer/AuthDBFile.h>
#include <PasswordServer/CPSUtilities.h>
#include <PasswordServer/PasswordServerPrefsDefs.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <time.h>
#include <unistd.h>
#include <regex.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>					// for struct kinfo_proc and sysctl()
#include <sys/types.h>
#include <Security/Authorization.h>

#include "bridge.h"
#include "KerberosInterface.h"

#define kMaxPasswordServerWait		30

extern int errno;

// GLOBALS
RC4_KEY gRC4Key;

AuthorizationRef authorization = NULL;

char *gKnownMechList[] =
{
	"APOP",
	"CRAM-MD5",
	"CRYPT",
	"DHX",
	"DIGEST-MD5",
	"GSSAPI",
	"KERBEROS_V4",
	"MS-CHAPv2",
	"NTLM",
	"OTP",
	"SMB-LAN-MANAGER",
	"SMB-NT",
	"SMB-NTLMv2",
	"TWOWAYRANDOM",
	"WEBDAV-DIGEST",
	NULL
};

// by default, list only optional mechanisms.
// filter out mechanisms that are required or unsupported.
char *gFilterMechList[] =
{
	"OTP",
	"DIGEST-MD5",
	"DHX",
	"CRYPT",
	"TWOWAYRANDOM",
	"CRAM-MD5",
	"KERBEROS_V4",
	"NTLM",
	NULL
};

extern "C" {
FILE *gLogFileDesc = NULL;
bool gNeededToManuallyLaunchPasswordService = false;
};

#define debug_and_stderr(A)	{debug(A); fprintf(stderr, (A));}

//--------------------------------------------------------------------------------------------------
// * main()
//
//--------------------------------------------------------------------------------------------------

int main ( int argc, char * const *argv )
{
	sInt32 exitStatus = EX_OK;
    int commandID;
	int argLogLimit = 3;
	bool quiet = false;
	bool needAuthorization = true;
	
	[NSAutoreleasePool new];
		
#if MYDEBUG
	gLogFileDesc = fopen( "/Library/Logs/NeST.log", "a+" );
#endif

    if ( argc < 2 )
	{
		debug( "NeST called with no command.\n" );
        _usage( stderr, argv[0] );
        exit( 0 );
    }
    
	char ppidStr[256];
	
	debug_and_stderr( "WARNING: NeST is a deprecated tool. Use slapconfig instead.\n\n" );
	char *parentProcName = ProcessName(getppid());
	if ( parentProcName == NULL )
		parentProcName = strdup( "<unknown>" );
	sprintf(ppidStr, "Parent PID: %d\nParent name: %s\n", getppid(), parentProcName);
	free( parentProcName );
	debug_and_stderr( ppidStr );
	debug_and_stderr( "Initiating an 8 second delay to call attention to the need to switch tools...\n" );
	sleep(4);
	debug_and_stderr( "4 more seconds to go...\n" );
	sleep(4);

	exitStatus = GetCommandID(argc, argv, needAuthorization, &commandID);
	
	switch( commandID )
	{
		// commands not to log
		case kCmd_getconfig:
		case kCmd_getstyle:
		case kCmd_getchildconfig:
		case kCmd_parentconfig:
		case kCmd_getpasswordserverstyle:
		case kCmd_getpasswordserveraddress:
		case kCmd_getprotocols:
			break;
		
		// commands that can log all args (no passwords)
		case kCmd_setldapstatic:
		case kCmd_static:
		case kCmd_allbindings:
		case kCmd_setnetinfo:
		case kCmd_addchild:
		case kCmd_setprotocols:
		case kCmd_revokereplica:
			argLogLimit = argc;
			// fall through
			
		default:
			debugcat( "\n" );
			debug( "command: " );
			// only printing the first 3 args to avoid logging passwords
			for ( int idx = 0; idx < argc && idx < argLogLimit; idx++ )
				debugcat( "%s ", argv[idx] );
			if (argc > argLogLimit) {
				debugcat( "...\n" );
			}
			else {
				debugcat( "\n" );
			}
	}
    
	if ( exitStatus == -1 )
	{
		debug( "bad command.\n" );
		_usage(stderr, argv[0]);
        exit(0);
    }
	
	// Anyone can check the version, get help, or get the configuration

    switch( commandID )
    {
        case kCmd_ver:
            _version( stderr );
			exit( 0 );
            break;
            
        case kCmd_appleversion:
            _appleVersion( stderr );
			exit( 0 );
            break;
            
        case kCmd_help:
            _usage( stderr, argv[0] );
			exit( 0 );
            break;
            
        case kCmd_getconfig:
            {
                char configStr[256];
                
                getConfigStr(configStr);
                printf("%s\n", configStr);
                exit( 0 );
            }
            break;

		case kCmd_getstyle:
			printStyleStr();
			exit( 0 );
			break;
			
        case kCmd_getchildconfig:
			printChildConfig();
			exit( 0 );
            break;

        case kCmd_parentconfig:
			printParentConfig();
			exit( 0 );
            break;
        
        case kCmd_getpasswordserverstyle:
            switch(GetPWServerStyle())
            {
                case kPasswordServerNone:
                    printf("0 - none\n");
                    break;
                    
                case kPasswordServerUse:
                    printf("1 - use\n");
                    break;
                    
                case kPasswordServerHost:
                    printf("2 - host\n");
                    break;
            }
            exit( 0 );
            break;
        
        case kCmd_getpasswordserveraddress:
            {
                char result[1024] = {0};
                
                GetPWServerAddresses(result);
                printf("%s\n", result);
            }
            exit( 0 );
            break;
        
        case kCmd_getprotocols:
            {
                char result[1024] = {0};
                
                GetSASLMechs( result, (argc==2 || strcmp(argv[2], "-a") != 0) );
                printf("%s", result);
            }
            exit( 0 );
            break;
    }

    // Only the root user or an admin user can run this app to do anything else
	if (strcasecmp(argv[argc-1],"-q") == 0)
	{
		quiet = true;
		argc--;
	}
    _checkUser( stderr, argv[0], quiet, needAuthorization );
    
    switch( commandID )
    {
        case kCmd_disableldapserver:
			setLDAPServer(false);
			break;
			
		case kCmd_destroyparent:
            if ( argc == 3 )
                _destroyParent( argv[ 2 ] );
            else
                _destroyParent( "network" );
            break;

		case kCmd_destroyorphanedparent:
            if ( argc == 3 )
                _destroyOrphanedParent( argv[ 2 ] );
            else
                _destroyOrphanedParent( "network" );
            break;
        
        case kCmd_localonly:
            _localonly();
            break;
            
        case kCmd_authserver:
            setAuthServer();
            break;

        case kCmd_setldapdhcp:
			_nonetinfo();
			setLDAPDHCP(true);
			break;			

        case kCmd_setldapstatic:
			_nonetinfo();
			setLDAPStatic( argv[ 2 ], argv[ 3 ], argv[ 4 ], argv[ 5 ] );
			break;			
			
        case kCmd_verifypasswordserveradmin:
            {
                long result;
                char idStr[1024];
                char *pwPass = NULL;
				long len;
				
				exitStatus = GetCmdLinePass( argv[4], &pwPass, &len );
				if ( exitStatus == EX_OK )
				{
					// verify the password 
					result = VerifyAdmin( argv[2], argv[3], pwPass, idStr );
					printf( "%s %s\n", (result == eDSNoErr) ? "success" : "failure", idStr );
					
					if ( pwPass != NULL ) {
						bzero( pwPass, len );
						free( pwPass );
					}
				}
            }
            break;
        
        case kCmd_nopasswordserver:
			DeletePWServerRecords();
			autoLaunchPasswordServer(false, true);
            //SetServiceState( "pwd", kActionOff, kPasswordServerCmdStr );
			
			if ( argc == 4 )
				ConvertLocalUsers( kConvertToBasic, NULL, NULL, argv[2], argv[3] );
			
#if MYDEBUG
			if ( argc == 3 && strcmp( argv[2], "clean" ) == 0 )
			{
				FILE *fp;
				
				fp = log_popen( "/bin/rm -r /var/db/authserver", "r" );
				if ( fp != NULL )
					pclose( fp );
					
				ConvertLocalUsers( kConvertToBasic );
			}
#else
			if ( argc == 3 )
				printf( "user <%s> not converted to basic because a password is required.\n", argv[2] );
#endif
            break;
            
        case kCmd_usepasswordserver:
            {
                // format of command: 1			2			3		4		5			6
                // NeST -usepasswordserver server-address niuser nipw pwserver-user pwserver-pw
                
                long result;
                char *serverAddress = argv[2];
                char *niUserName = argv[3];
                char currentAdminIDStr[1024];
                char newAdminIDStr[1024];
                char *niPass = NULL;
				char *pwPass = NULL;
				long niPassLen;
				long pwPassLen;
				
				// before doing anything, we want to erase the passwords from a ps listing
				// copy and blank arg 4
				exitStatus = GetCmdLinePass( argv[4], &niPass, &niPassLen );
				if ( exitStatus != EX_OK )
					exit( exitStatus );
				
				exitStatus = GetCmdLinePass( argv[6], &pwPass, &pwPassLen );
				if ( exitStatus == EX_OK )
				{
					result = VerifyAdmin( serverAddress, argv[5], pwPass, currentAdminIDStr );
					if (result == eDSNoErr)
					{
						result = NewPWServerAdminUserRemote( serverAddress,
															currentAdminIDStr,
															pwPass,
															niUserName,
															niPass,
															newAdminIDStr );
					}
					
					debugerr( result, "NewPWServerAdminUserRemote=%ld\n", result);
					if (result == eDSNoErr)
					{
						SetPWServerAddress( niUserName, niPass, serverAddress, false, NULL, NULL );
						SetAuthAuthority( serverAddress, niUserName, niPass, newAdminIDStr );
					}
					
					if ( niPass != NULL ) {
						bzero( niPass, niPassLen );
						free( niPass );
					}
					if ( pwPass != NULL ) {
						bzero( pwPass, pwPassLen );
						free( pwPass );
					}
				}
            }
            break;
            
        case kCmd_hostpasswordserver:
            exitStatus = HostPasswordServer( argc, argv, quiet );
            break;
            
		case kCmd_setprotocols:
            SetSASLMechs(argc, argv);
            break;
			
		case kCmd_rebootnow:
			{
				FILE *pf = NULL;
				pf = log_popen("/sbin/reboot now", "r");
				pclose(pf);
			}
			break;
        
		case kCmd_setupreplica:
			exitStatus = SetupReplica( argc, argv );
			break;
			
		case kCmd_convertuser:
			{
                char *passwd = NULL;
				char dbKey[kPWFileMaxPublicKeyBytes] = {0};
				char userPass[130] = {0};
				char adminPass[130] = {0};
				
				// format of command:  2	    3			4				5
                // NeST -convertuser niuser dir-admin niuser-password dir-admin-password
                
				if ( argc >= 6 )
				{
					// copy and blank the arg ASAP
					strlcpy( adminPass, argv[5], sizeof(adminPass) );
					bzero( argv[5], strlen(adminPass) );
				}
				
				if ( argc >= 5 )
				{
					// copy and blank the arg ASAP
					strlcpy( userPass, argv[4], sizeof(userPass) );
					bzero( argv[4], strlen(userPass) );
				}
				
				if ( argc < 5 )
				{
					// getpass() returns a pointer to a static object so it needs to be copied
					passwd = getpass( "User's new password:");
					if ( passwd != NULL )
						strlcpy( userPass, passwd, sizeof(userPass) );
				}
				
				if ( argc < 6 && argc != 3 )
				{
					passwd = getpass( "Administrator password for the directory node:");
					if ( passwd != NULL )
						strlcpy( adminPass, passwd, sizeof(adminPass) );
				}
				
				// do not allow blank passwords
				if ( *userPass == '\0' ) {
					debug( "blank passwords are not allowed.\n");
					fprintf( stderr, "blank passwords are not allowed.\n" );
					exit(-1);
				}
				
				if ( ! GetPasswordServerKey( dbKey, sizeof(dbKey) ) ) {
					debug("cannot get the password server's public key.\n");
					fprintf(stderr, "cannot get the password server's public key.\n");
					exit(-1);
				}
				
				ConvertUser( dbKey, argv[2], userPass, false, false, (argc>=4) ? argv[3] : NULL, adminPass );
				
                bzero( userPass, sizeof(userPass) );
				bzero( adminPass, sizeof(adminPass) );
				if ( passwd != NULL )
					bzero( passwd, strlen(passwd) );
            }
			break;
		
		case kCmd_replicapolicy:
			break;
		
		case kCmd_stripsyncdates:
			{
				ReplicaFile *replicaFile = [ReplicaFile new];
				[replicaFile stripSyncDates];
				[replicaFile saveXMLData];
			}
			break;
		
		case kCmd_pwsstandalone:
			/*exitStatus = */StandAlone( argc, argv );
			break;
		
		case kCmd_migrateip:
			// format is:
			// NeST -migrateip [from|any] to user pass
			exitStatus = MigrateIP( argc, argv );
			break;
		
		case kCmd_revokereplica:
			exitStatus = RevokeReplica( argc, argv );
			break;
		
		case kCmd_startpasswordserver:
			{
				ReplicaFile *replicaFile = [ReplicaFile new];
				char ipBuff[256];
				
				if ( ResumePasswordServer() )
				{
					if ( ! PasswordServerListening(kMaxPasswordServerWait) )
					{
						debug( "ERROR: Could not confirm that password server is listening.\n");
						fprintf( stderr, "ERROR: Could not confirm that password server is listening.\n" );
						exitStatus = EX_UNAVAILABLE;
					}
				}
				
				exitStatus = SetupPasswordServerConfigRecord( replicaFile, NULL, NULL, ipBuff, false );
				[replicaFile free];
			}
			break;
		
		case kCmd_stoppasswordserver:
			autoLaunchPasswordServer(false, true);
			//SetServiceState( "pwd", kActionOff, kPasswordServerCmdStr );
			DeletePWServerRecords();
			break;
		
		case kCmd_pwsrekey:
			exitStatus = Rekey( (argc==3) ? argv[2] : NULL );
			break;
		
		case kCmd_hostpasswordserverinparent:
			exitStatus = HostPasswordServerInParent( argc, argv, quiet );
			break;
		
		case kCmd_promote:
			exitStatus = PromoteToMaster();
			break;
		
		case kCmd_local_to_shadowhash:
			exitStatus = LocalToShadowHash();
			break;
		
		case kCmd_shadowhash_to_ldap:
			exitStatus = LocalToLDAP(argv[2], quiet);
			break;
		
		default:
            _usage( stderr, argv[0] );
			exit(EX_USAGE);
	}
	
	if ( gNeededToManuallyLaunchPasswordService )
	{
		unsigned long pid = 0;
		unsigned long watchDogPID = 0;
		
		if ( LaunchdRunning( &watchDogPID ) )
		{
			// stop our force-launched process
			if ( PasswordServerRunning( &pid ) )
			{
				kill( pid, SIGTERM );
				sleep(1);
				if ( PasswordServerRunning( &pid ) )
					sleep(1);
				if ( PasswordServerRunning( &pid ) )
					sleep(1);
				if ( PasswordServerRunning( &pid ) )
					kill( pid, SIGKILL );
			}
		}
		
		// resume launchd
		debug( "setting password server back to autolaunch with launchd.\n" );
		autoLaunchPasswordServer( true, true );
	}
	
	if ( exitStatus == EX_USAGE )
		_usage( stderr, argv[0] );
	
	exit( exitStatus );

} // main


//-----------------------------------------------------------------------------
//	_usage ()
//
//-----------------------------------------------------------------------------

void _usage ( FILE *inFile, const char *inArgv0 )
{
	static const char * const	_szpUsage =
		"Usage:\t%s\nDo not use this tool. It is deprecated and will be "
		"removed from 10.5. Use the slapconfig tool instead.\n"
		"  -ver                         Displays version information.\n"
		"";

	fprintf( inFile, _szpUsage, inArgv0 );
} // _usage


//-----------------------------------------------------------------------------
//	_checkUser ()
//
//-----------------------------------------------------------------------------

void _checkUser ( FILE *inFile, const char *inArgv0, bool inQuiet, bool needAuthorization )
{
	uid_t		userID		= 99;
    Boolean		userGood	= false;
	 
	userID = getuid();
	if ( userID == 0 || !needAuthorization )
    {
        userGood = true;
    }
#if RUNNING_SETUID
    else
	{
		OSStatus	status		= noErr;
		AuthorizationItem rights[] = { { "system.preferences", 0, 0, 0 } };
		AuthorizationRights rightSet = { sizeof(rights)/ sizeof(*rights), rights };
		
		// prompt for a password
		struct passwd * user = getpwuid(getuid());
		char* prompt = "";
		Buffer* password;
		if ( !inQuiet )
		{
			prompt = "Password:";
		}
		password = getSecretBuffer(prompt);
		if ( user != NULL && user->pw_name != NULL && password != NULL )
		{
			AuthorizationItem params[] = { {"username", strlen(user->pw_name), (void*)user->pw_name, 0}, {"password", password->length, (void*)password->data, 0} };
			AuthorizationEnvironment environment = { sizeof(params)/ sizeof(*params), params };

			status = AuthorizationCreate( &rightSet, &environment, kAuthorizationFlagExtendRights, &authorization);
		}
		if ( password != NULL )
		{
			bufferRelease( password );
		}
		if ( status == noErr )
		{
			status = AuthorizationCopyRights(authorization, &rightSet, NULL, 0, NULL);
			if ( status == errAuthorizationSuccess ) {
				setuid(0);
				userGood = true;
			}
		}
	}
#endif
    
    if ( !userGood )
    {
		fprintf( inFile, "Invalid user.  \"%s\"  must be run by root.\n", inArgv0 );
		exit( EX_NOPERM );
	}
} // _checkUser


// --------------------------------------------------------------------------------
//	log_popen
// --------------------------------------------------------------------------------

FILE *log_popen( const char *inCmd, const char *inMode )
{
	if ( gLogFileDesc != NULL )
	{
		nest_log_time( gLogFileDesc );
		fprintf( gLogFileDesc, "  popen: %s, \"%s\"\n", inCmd, inMode );
	}
	
	return popen( inCmd, inMode );
}


// --------------------------------------------------------------------------------
//	GetCommandID
// --------------------------------------------------------------------------------

int GetCommandID( int argc, char * const *argv, bool & needAuthorization, int *outCommandID )
{
    int commandID = kCmd_unknown;
    int argMin = 2;
    int argMax = 2;
    const char *p;
    bool needsRC4Key = false;
	
    p = argv[1];
	needAuthorization = true;
    
    if ( strcmp( p, "-ver" ) == 0 ) 			// Version
    {
        commandID = kCmd_ver;
		needAuthorization = false;
    }
    else 
    if ( strcmp( p, "-appleversion" ) == 0 )	// Internal apple version
    {
        commandID = kCmd_appleversion;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-help" ) == 0 )			// Easter egg
    {
        commandID = kCmd_help;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-getconfig" ) == 0 )
    {
        commandID = kCmd_getconfig;
		needAuthorization = false;
    }
	else
	if ( strcmp( p, "-disableldapserver" ) == 0 )		// Disable LDAP on parent
	{
		commandID = kCmd_disableldapserver;
	}
    else
    if ( strcmp( p, "-destroyparent" ) == 0 )		// Destroy a parent domain
    {
        commandID = kCmd_destroyparent;
        argMax = 3;
    }
    else
    if ( strcmp( p, "-destroyorphanedparent" ) == 0 )	// Destroy, but don't change bindings
    {
        commandID = kCmd_destroyorphanedparent;
        argMax = 3;
    }
    else
    if ( strcmp( p, "-addchild" ) == 0 )				// Add a child for this server to bind to
    {
        commandID = kCmd_addchild;
        argMin = 4;
        argMax = 4;
    }
    else
    if ( strcmp( p, "-localonly" ) == 0 )				// set to no bindings
    {
        commandID = kCmd_localonly;
    }
    else
    if ( strcmp( p, "-authserver" ) == 0 )
    {
        commandID = kCmd_authserver;
    }
    else
    if ( strcmp( p, "-getstyle" ) == 0 )
    {
        commandID = kCmd_getstyle;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-getchildconfig" ) == 0 )
    {
        commandID = kCmd_getchildconfig;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-getparentconfig" ) == 0 )
    {
        commandID = kCmd_parentconfig;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-setnetinfo" ) == 0 )
    {
        commandID = kCmd_setnetinfo;
		argMin = 2;
		argMax = 7;
    }
    else
	if ( strcmp( p, "-setldapdhcp" ) == 0 )
	{
		commandID = kCmd_setldapdhcp;
	}
    else
	if ( strcmp( p, "-setldapstatic" ) == 0 )
	{
		commandID = kCmd_setldapstatic;
		argMin = 6;
		argMax = 6;
	}
    else
    if ( strcmp( p, "-getpasswordserverstyle" ) == 0 )
    {
        commandID = kCmd_getpasswordserverstyle;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-getpasswordserveraddress" ) == 0 )
    {
        commandID = kCmd_getpasswordserveraddress;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-verifypasswordserveradmin" ) == 0 )
    {
        commandID = kCmd_verifypasswordserveradmin;
        argMin = 5;
        argMax = 5;
		needsRC4Key = true;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-NOpasswordserver" ) == 0 )
    {
        commandID = kCmd_nopasswordserver;
		argMax = 4;
		needsRC4Key = true;
    }
    else
    if ( strcmp( p, "-usepasswordserver" ) == 0 )
    {
        commandID = kCmd_usepasswordserver;
        argMin = 7;
        argMax = 7;
		needsRC4Key = true;
    }
    else
    if ( strcmp( p, "-hostpasswordserver" ) == 0 )
    {
        commandID = kCmd_hostpasswordserver;
        argMin = 3;
        argMax = 256;
		needsRC4Key = true;
    }
	else
    if ( strcmp( p, "-getprotocols" ) == 0 )
    {
        commandID = kCmd_getprotocols;
        argMax = 3;
		needAuthorization = false;
    }
    else
    if ( strcmp( p, "-setprotocols" ) == 0 )
    {
        commandID = kCmd_setprotocols;
        argMin = 4;
        argMax = 255;
    }
    else
    if ( strcmp( p, "-rebootnow" ) == 0 )
    {
        commandID = kCmd_rebootnow;
        argMin = 2;
        argMax = 2;
    }
	else
	if ( strcmp( p, "-setupreplica" ) == 0 )
    {
		// NeST -setupreplica serverIP pw-admin pw-pass
    	commandID = kCmd_setupreplica;
		argMin = 5;
		argMax = 256;
		needsRC4Key = true;
	}
	else
	if ( strcmp( p, "-convertuser" ) == 0 )
    {
		// NeST -convertuser user dir-admin
    	commandID = kCmd_convertuser;
		argMin = 3;
		argMax = 6;
		needsRC4Key = true;
	}
	else
	if ( strcmp( p, "-replicapolicy" ) == 0 )
	{
		commandID = kCmd_replicapolicy;
		argMin = 3;
		argMax = 255;
	}
	else
	if ( strcmp( p, "-stripsyncdates" ) == 0 )
	{
		commandID = kCmd_stripsyncdates;
	}
	else
	if ( strcmp( p, "-pwsstandalone" ) == 0 )
	{
		commandID = kCmd_pwsstandalone;
		argMin = 2;
        argMax = 255;
		needsRC4Key = true;
	}
	else
	if ( strcmp( p, "-migrateip" ) == 0 )
	{
		commandID = kCmd_migrateip;
		argMin = 4;
        argMax = 6;
		needsRC4Key = true;
	}
	else
	if ( strcmp( p, "-revokereplica" ) == 0 )
	{
		commandID = kCmd_revokereplica;
		argMin = 3;
		argMax = 4;
	}
	else
	if ( strcmp( p, "-startpasswordserver" ) == 0 )
	{
		commandID = kCmd_startpasswordserver;
	}
	else
	if ( strcmp( p, "-stoppasswordserver" ) == 0 )
	{
		commandID = kCmd_stoppasswordserver;
	}
	else
	if ( strcmp( p, "-pwsrekey" ) == 0 )
	{
		commandID = kCmd_pwsrekey;
		argMax = 3;
	}
	else
	if ( strcmp( p, "-hostpasswordserverinparent" ) == 0 )
	{
		commandID = kCmd_hostpasswordserverinparent;
		argMin = 3;
		argMax = 4;
	}
	else
	if ( strcmp( p, "-promote" ) == 0 )
	{
		commandID = kCmd_promote;
	}
	else
	if ( strcmp( p, "--local-to-shadowhash" ) == 0 )
	{
		commandID = kCmd_local_to_shadowhash;
	}
	else
	if ( strcmp( p, "--shadowhash-to-ldap" ) == 0 )
	{
		commandID = kCmd_shadowhash_to_ldap;
		argMin = 3;
		argMax = 3;
	}
	
	
	if ( strcasecmp(argv[argc-1],"-q") == 0 )
	{
		argc--; // ignore -q for purposes of maximum argument enforcement
	}
	
	// do some manipulations to extract our fixed RC4 key
	if ( needsRC4Key )
	{
		unsigned char stuff[16];

		memset(stuff, 0, sizeof(stuff));
		strncpy((char *)stuff, _PATH_CONSOLE, 8);
		stuff[0] = (-22 * 4) & 0xff;
		stuff[1] = (((int)stuff[0] >> 3) - 3) & 0xff;
		stuff[2] = ((((int)stuff[0]) & 0240) | 03) & 0xff;
		stuff[3] = (((int)stuff[1] * 2) - 5) & 0xff;
		stuff[4] = (((int)stuff[1] << 6) | 023)  & 0xff;
		stuff[5] = (stuff[0] << 4) & 0xff;
		stuff[6] = (((int)stuff[3] << 6) | 0201) & 0xff;
		stuff[7] = (stuff[3] + 2) & 0xff;
		
		RC4_set_key(&gRC4Key, 8, stuff);
		memset(stuff, 0, sizeof(stuff));
	}
	
	*outCommandID = commandID;
	
	if ( commandID == kCmd_unknown || argc < argMin || argc > argMax )
		return -1;
	
	return 0;
}


//-----------------------------------------------------------------------------
//	HostPasswordServer
//
//-----------------------------------------------------------------------------

int HostPasswordServer( int argc, char * const *argv, bool quiet )
{
	long result = eDSNoErr;
	bool hasServerID = false;
	char *niPass = NULL;
	long len = 0;
	bool ipProvided = false;
	char curServerAddrStr[256];
	char permServerAddrStr[256];
	char idStr[1024];
	int returnValue = EX_OK;
	
	// format of command: 1		  2		3		4 (optional)
	// NeST -hostpasswordserver niuser nipw   ip-address
	
	if ( argc > 3 )
	{
		result = GetCmdLinePass( argv[3], &niPass, &len );
		if ( result != EX_OK )
			return result;
	}
	
	GetMyAddressAsString( curServerAddrStr );
	
	ipProvided = ( argc >= 5 );
	if ( ipProvided && !ArgsAreIPAddresses( argv, 4, argc-1 ) ) {
		debug( "An argument after #4 is not an IP address.\n");
		fprintf( stderr, "An argument after #4 is not an IP address.\n");
		exit( EX_USAGE );
	}
	
	strlcpy( permServerAddrStr, ipProvided ? argv[4] : curServerAddrStr, sizeof(permServerAddrStr) );
	
	debug( "active ip: %s, configuring ip(s): %s", curServerAddrStr, permServerAddrStr );
	if ( argc > 5 )
	{
		for ( int idx = 5; idx < argc; idx++ )
			debugcat( ", %s", argv[idx] );
	}
	debugcat( "\n" );
	
	hasServerID = GetAuthAuthority( curServerAddrStr, argv[2], niPass, idStr );
	if ( hasServerID )
	{
		bool isAdmin = false;
		
		// is the ID still good?					
		result = VerifyUserID( curServerAddrStr, idStr, "", false, &isAdmin );
		if ( result == eDSNoErr && isAdmin == false )
		{
			FILE *pf;
			char cmdStr[256];
			char uid[35];
			
			// we are converting a regular user to a password server admin
			// need to change the user's status
			strncpy( uid, idStr, 34 );
			uid[34] = '\0';
			snprintf( cmdStr, sizeof(cmdStr), "/usr/sbin/mkpassdb -setadmin %s", uid );
			pf = log_popen( cmdStr, "r" );
			result = pclose( pf );
			debugerr( result, "error upgrading account to an administrator, exitcode = %ld\n", result );
		}
		
		if ( result != eDSNoErr )
			hasServerID = false;
	}
	
	if ( ! hasServerID )
	{
		// need to add
		if ( argc == 3 )
		{
			char *ptrToStaticBuf = getpass(quiet ? "":"Password:");
			if ( ptrToStaticBuf != NULL )
			{
				len = strlen( ptrToStaticBuf );
				if ( len > 511 ) {
					return EX_USAGE;
				}
				niPass = (char *) malloc( len + 1 );
				if ( niPass == NULL ) {
					debug( "memory error\n" );
					return -1;
				}
				strcpy( niPass, ptrToStaticBuf );
			}
		}
		
		// do not allow blank passwords
		if ( niPass == NULL || *niPass == '\0' ) {
			debug( "blank passwords are not allowed.\n");
			fprintf( stderr, "blank passwords are not allowed.\n");
			exit( EX_USAGE );
		}
		
		PausePasswordServer();
		NewPWServerAdminUser( argv[2], niPass ); 
		if ( ResumePasswordServer() )
		{
			// make sure the server has time to start listening
			if ( ! PasswordServerListening(kMaxPasswordServerWait) ) {
				debug( "The administrator may not be bound to the password server because the server is not responding." );
				fprintf( stderr, "The administrator may not be bound to the password server because the server is not responding.\n" );
			}
		}
		
		result = VerifyAdmin( curServerAddrStr, argv[2], niPass, idStr );
		debugerr( result, "error creating password server admin, VerifyAdmin = %ld\n", result );
	}
	
	if ( result == eDSNoErr )
		SetAuthAuthority( permServerAddrStr, argv[2], niPass, idStr );
	
	// set the passwordserver config record
	// must be done after the creation of the database
	ReplicaFile *replicaFile = [ReplicaFile new];
					
	// make sure the provided IP is in the list
	if ( ipProvided )
	{
		CFMutableDictionaryRef parentDict = NULL;
		CFStringRef replicaNameString = NULL;
		
		for ( int idx = 4; idx < argc; idx++ )
		{
			replicaNameString = [replicaFile getNameFromIPAddress:argv[idx]];
			if ( replicaNameString != NULL && CFStringGetLength(replicaNameString) == 0 )
			{
				// we know we are the parent
				parentDict = (CFMutableDictionaryRef)[replicaFile getParent];
				if ( parentDict != nil )
					[replicaFile addIPAddress:argv[idx] toReplica:parentDict];
			}
		}
	}
	
	returnValue = SetupPasswordServerConfigRecord( replicaFile, argv[2], niPass, permServerAddrStr, ipProvided );
	
	SetRealmToHostnameIfPresent();
	
	// clean up
	if ( niPass != NULL ) {
		bzero( niPass, len );
		free( niPass );
	}
	
	return returnValue;
}


//-----------------------------------------------------------------------------
//	HostPasswordServerInParent
//
//	RETURNS: exitcode
//
//	Command format: NeST -hostpasswordserverinparent admin ip
//-----------------------------------------------------------------------------

int HostPasswordServerInParent( int argc, char * const *argv, bool quiet )
{
	long result = eDSNoErr;
	const char *adminName = argv[2];
	bool hasServerID = false;
	char *niPass = NULL;
	char curServerAddrStr[256];
	char idStr[1024] = {0,};
	int returnValue = EX_OK;
	
	if ( adminName == NULL || adminName[0] == '\0' )
		return EX_USAGE;
	
	if ( argc == 3 )
	{
		GetMyAddressAsString( curServerAddrStr );
	}
	else
	{
		if ( ! ArgsAreIPAddresses(argv, 3, 3) )
			return EX_USAGE;
		
		strlcpy( curServerAddrStr, argv[3], sizeof(curServerAddrStr) );
	}
	
	// get auth authority for user in parent node
	DSUtilsAuthAuthority dsUtils( adminName, curServerAddrStr, niPass, idStr, true, false );
	result = dsUtils.OpenNetInfoParentNode();
	if ( result != eDSNoErr )
		return EX_UNAVAILABLE;
	
	result = dsUtils.DoActionOnCurrentNode();
	if ( result != eDSNoErr )
		return EX_NOUSER;
	
	dsUtils.CopyUserID( idStr );
	hasServerID = (idStr[0] != '\0');
	if ( hasServerID )
	{
		bool isAdmin = false;
		
		// is the ID still good?					
		result = VerifyUserID( curServerAddrStr, idStr, "", false, &isAdmin );
		if ( result == eDSNoErr && isAdmin == false )
		{
			FILE *pf;
			char cmdStr[256];
			char uid[35];
			
			// we are converting a regular user to a password server admin
			// need to change the user's status
			strncpy( uid, idStr, 34 );
			uid[34] = '\0';
			snprintf( cmdStr, sizeof(cmdStr), "/usr/sbin/mkpassdb -setadmin %s", uid );
			pf = log_popen( cmdStr, "r" );
			result = pclose( pf );
			debugerr( result, "error upgrading account to an administrator, exitcode = %ld\n", result );
		}
		
		if ( result != eDSNoErr )
			hasServerID = false;
	}
	
	if ( ! hasServerID )
	{
		char *ptrToStaticBuf = getpass(quiet ? "":"Password:");
		if ( ptrToStaticBuf != NULL )
		{
			niPass = strdup( ptrToStaticBuf );
			if ( niPass == NULL ) {
				debug( "memory error\n" );
				return EX_OSERR;
			}
		}
		
		// do not allow blank passwords
		if ( niPass == NULL || *niPass == '\0' ) {
			debug( "blank passwords are not allowed.\n");
			fprintf( stderr, "blank passwords are not allowed.\n");
			exit( EX_USAGE );
		}
		
		PausePasswordServer();
		NewPWServerAdminUser( adminName, niPass ); 
		if ( ResumePasswordServer() )
		{
			// make sure the server has time to start listening
			if ( ! PasswordServerListening(kMaxPasswordServerWait) ) {
				debug( "The administrator may not be bound to the password server because the server is not responding." );
				fprintf( stderr, "The administrator may not be bound to the password server because the server is not responding.\n" );
			}
		}
		
		result = VerifyAdmin( curServerAddrStr, adminName, niPass, idStr );
		debugerr( result, "error creating password server admin, VerifyAdmin = %ld\n", result );

		if ( result == eDSNoErr )
		{
			dsUtils.SetVerifyOnly( false );
			dsUtils.SetUserID( idStr );
			dsUtils.SetPassword( niPass );
			result = dsUtils.DoActionOnCurrentNode();
			debugerr( result, "error setting authentication_authority, result = %ld\n", result );
		}
	}
	
	// set the passwordserver config record
	// must be done after the creation of the database
	ReplicaFile *replicaFile = [ReplicaFile new];
	
	// make sure the provided IP is in the list
	CFMutableDictionaryRef parentDict = NULL;
	NSString *replicaNameString = nil;
	
	replicaNameString = (NSString *)[replicaFile getNameFromIPAddress:curServerAddrStr];
	if ( replicaNameString != nil && [replicaNameString length] == 0 )
	{
		// we know we are the parent
		parentDict = (CFMutableDictionaryRef) [replicaFile getParent];
		if ( parentDict != NULL )
			[replicaFile addIPAddress:curServerAddrStr toReplica:parentDict];
	}
	
	returnValue = SetupPasswordServerConfigRecord( replicaFile, adminName, niPass, curServerAddrStr, true );
	[replicaFile free];
	
	SetRealmToHostnameIfPresent();
	
	// clean up
	if ( niPass != NULL ) {
		bzero( niPass, strlen(niPass) );
		free( niPass );
	}
	
	return returnValue;
}


//-----------------------------------------------------------------------------
//	SetRealmToHostnameIfPresent
//-----------------------------------------------------------------------------

void SetRealmToHostnameIfPresent( void )
{
	FILE *pf = NULL;
	char cmdStr[1024];
	char saslRealm[1024] = {0,};
	
	if ( GetHostFromSystemConfiguration(saslRealm, sizeof(saslRealm)) != 0 ||
		 saslRealm[0] == '\0' ||
		 strstr(saslRealm, ".local") != NULL )
	{
		// The official realm hasn't been set, so generate a unique realm
		strcpy( saslRealm, "OpenDirectory.XXXXXX" );
		mktemp( saslRealm );
	}
	
	debug( "Setting SASL realm to <%s>\n", saslRealm );
	snprintf( cmdStr, sizeof(cmdStr), "/usr/sbin/mkpassdb -setrealm %s", saslRealm );
	pf = log_popen( cmdStr, "w" );
	if ( pf != NULL )
		pclose( pf );
}


//-----------------------------------------------------------------------------
//	GetHostFromSystemConfiguration
//-----------------------------------------------------------------------------

int GetHostFromSystemConfiguration( char *inOutHostStr, size_t maxHostStrLen )
{
	int result = -1;
	SCPreferencesRef scpRef = NULL;
	
	try
	{		
		scpRef = SCPreferencesCreate( NULL, CFSTR("NeST"), 0 );
		if ( scpRef == NULL )
			throw(1);
		
		CFDictionaryRef sysDict = (CFDictionaryRef) SCPreferencesGetValue( scpRef, CFSTR("System") );
		if ( sysDict == NULL )
			throw(1);
		
		CFDictionaryRef sys2Dict = (CFDictionaryRef) CFDictionaryGetValue( sysDict, CFSTR("System") );
		if ( sys2Dict == NULL )
			throw(1);
		
		CFStringRef hostString = (CFStringRef) CFDictionaryGetValue( sys2Dict, CFSTR("HostName") );
		if ( hostString == NULL )
			throw(1);
		
		if ( CFStringGetCString(hostString, inOutHostStr, maxHostStrLen, kCFStringEncodingUTF8) )
			result = 0;
	}
	catch ( ... )
	{
		result = -1;
	}
	
	if ( scpRef != NULL )
		CFRelease( scpRef );
	
	return result;
}


//-----------------------------------------------------------------------------
//	SetupPasswordServerConfigRecord
//
//-----------------------------------------------------------------------------

int SetupPasswordServerConfigRecord(
	ReplicaFile *inReplicaFile,
	const char *inDirAdmin,
	const char *inDirPassword,
	char *inProvidedIPBuff,
	bool inIPProvided )
{
	// set the passwordserver config record
	// must be done after the creation of the database
	CFStringRef replicaListString = NULL;
	NSString *replicaIDString = nil;
		
	replicaListString = [inReplicaFile xmlString];
	debugerr( (replicaListString == NULL), "no replica list.\n" );
	
	replicaIDString = (NSString *)[inReplicaFile getUniqueID];
	SetPWServerAddress( inDirAdmin, inDirPassword, inProvidedIPBuff, !inIPProvided, [(NSString *)replicaListString UTF8String],
							replicaIDString ? [replicaIDString UTF8String] : NULL );
	
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	SetupReplica
//
//-----------------------------------------------------------------------------

int SetupReplica( int argc, char * const *argv )
{
	long len;
	long syncInterval;
	char *pwsPass = NULL;
	NSMutableDictionary *selfDict = nil;
	id valueRef = nil;
	AuthDBFile *authFile = nil;
	bool ipProvided = false;
	char idStr[1024];
	char pubKey[kPWFileMaxPublicKeyBytes];
	char privateKey[kPWFileMaxPrivateKeyBytes];
	long privateKeyLen = 0;
	long result;
	char *replicaList = NULL;
	struct stat sb;
	char serverAddrStr[256];
	NSString *replicaNameString = nil;
	char cmdStr[256];
	FILE *pf = NULL;

	// format of command:   2		  3		4    
	// NeST -setupreplica serverIP pwsuser pwspw 
	
	result = GetCmdLinePass( argv[4], &pwsPass, &len );
	if ( result != EX_OK )
		return result;
	
	ipProvided = ( argc >= 6 );
	if ( ipProvided && !ArgsAreIPAddresses( argv, 5, argc-1 ) ) {
		debug( "An argument after #5 is not an IP address.\n");
		fprintf( stderr, "An argument after #5 is not an IP address.\n");
		exit(-1);
	}
	
	result = VerifyAdmin( argv[2], argv[3], pwsPass, idStr );
	if ( result != eDSNoErr )
	{
		debug( "The administrator password was rejected by the master password server.\n");
		fprintf(stderr, "No admin.\n");
		exit(-1);
	}
	
	result = GetReplicaSetup( argv[2], idStr, pwsPass, pubKey, privateKey, &privateKeyLen, &replicaList );
	if ( result != eDSNoErr || privateKeyLen < 9 )
	{
		debug("GetReplicaSetup = %ld\n", result);
		if ( privateKeyLen < 9 )
			debug("no RSA private key\n");
		fprintf(stderr, "GetReplicaSetup = %ld\n", result);
		exit(-1);
	}
	
	// load replica list
	ReplicaFile *replicaFile = [[ReplicaFile alloc] initWithXMLStr:replicaList];
	UInt32 firstID, lastID;
	
	if ( [replicaFile replicaCount] == 0 ) {
		debug("FAIL: replication list retrieved from the master password server is defective.\n");
		debug("%s\n\n", [(NSString *)[replicaFile xmlString] UTF8String]);
		return EX_CONFIG;
	}
	
	// clear the replica list associated with the current key
	DeletePWServerRecords( false );
	
	[replicaFile stripSyncDates];
	
	GetMyAddressAsString( serverAddrStr );
	replicaNameString = (NSString *)[replicaFile getNameFromIPAddress:serverAddrStr];
	
	// If we're not in the replica table, something is wrong and it's
	// time to abort
	if ( replicaNameString == nil || [replicaNameString length] == 0 )
	{
		debug("FAIL: this server's IP address is not in the replication list retrieved from the master password server.\n");
		debug("%s\n\n", replicaList);
		return EX_CONFIG;
	}
	
	[replicaFile getIDRangeForReplica:(CFStringRef)replicaNameString start:&firstID end:&lastID];
	selfDict = (NSMutableDictionary *)[replicaFile getReplicaByName:(CFStringRef)replicaNameString];
	
	// make sure the provided IP is in the list
	if ( ipProvided && selfDict != nil )
	{
		NSString *tempReplicaNameString = nil;
		
		for ( int idx = 5; idx < argc; idx++ )
		{
			tempReplicaNameString = (NSString *)[replicaFile getNameFromIPAddress:argv[idx]];
			if ( tempReplicaNameString != nil )
				[replicaFile addIPAddress:argv[idx] toReplica:(CFMutableDictionaryRef)selfDict];
		}
	}
	
	// save a copy of the old replica list
	if ( lstat( kPWReplicaFile, &sb ) == 0 )
		rename( kPWReplicaFile, kPWReplicaFileSavedName );
	
	// Note: the replica list must be saved before changing the database to a replica. Otherwise
	// the password server may try to synchronize with itself.
	[replicaFile saveXMLData];
	
	// make our database
	PausePasswordServer();
	if ( MakeReplica(argv[2], pubKey, privateKey, privateKeyLen, firstID, lastID, [replicaNameString UTF8String]) == false )
	{
		// database creation failed
		debug( "database creation failed.\n");
		fprintf(stderr, "database creation failed.\n");
		exit(-1);
	}
	
	// construct a slot for the primary administrator
	PWFileEntry dbEntry = {0};
	strlcpy( dbEntry.usernameStr, argv[3], sizeof(dbEntry.usernameStr) );
    strlcpy( dbEntry.passwordStr, pwsPass, sizeof(dbEntry.passwordStr) );
	dbEntry.access.isAdminUser = 1;
	dbEntry.access.canModifyPasswordforSelf = 1;
	
	authFile = [[AuthDBFile alloc] init];
	if ( authFile != NULL ) {
		result = [authFile validateFiles];
		if ( result == 0 ) {
			idStr[34] = '\0';
			if ( pwsf_stringToPasswordRecRef(idStr, &dbEntry) )
				result = [authFile setPassword:&dbEntry atSlot:dbEntry.slot obfuscate:YES setModDate:NO];
		}
		[authFile free];
		authFile = nil;
	}
	
	// set up replication interval
	valueRef = [selfDict objectForKey:@"SyncInterval"];
	if ( valueRef != nil )
	{
		if ( [valueRef isKindOfClass:[NSNumber class]] )
		{
			syncInterval = [(NSNumber *)valueRef longValue];
			sprintf( cmdStr, "/usr/sbin/mkpassdb -setreplicationinterval %ld", syncInterval );
			pf = log_popen( cmdStr, "w" );
			if ( pf != NULL )
				pclose( pf );
		}
		[selfDict removeObjectForKey:@"SyncInterval"];
	}
	
	// set up SASL realm
	valueRef = [selfDict objectForKey:@"SASLRealm"];
	if ( valueRef != nil )
	{
		if ( [valueRef isKindOfClass:[NSString class]] )
		{
			sprintf( cmdStr, "/usr/sbin/mkpassdb -setrealm %s", [(NSString *)valueRef UTF8String] );
			pf = log_popen( cmdStr, "w" );
			if ( pf != NULL )
				pclose( pf );
			
		}
		[selfDict removeObjectForKey:@"SASLRealm"];
	}
	[replicaFile saveXMLData];
	
	if ( selfDict != nil )
	{
		[selfDict release];
		selfDict = nil;
	}
	
	replicaNameString = (NSString *)[replicaFile getUniqueID];
	
	if ( ipProvided )
		strlcpy( serverAddrStr, argv[5], sizeof(serverAddrStr) );
		
	// Note: pass false for <inHost> (4) because we have already retrieved the IP address to use
	SetPWServerAddress( argv[3], pwsPass, serverAddrStr, false, replicaList, replicaNameString ? [replicaNameString UTF8String] : NULL, kSetupActionSetupReplica );
	
	if ( pwsPass != NULL ) {
		bzero( pwsPass, len );
		free( pwsPass );
	}
	
	if ( replicaList != NULL )
		free( replicaList );
	
	// guarantee running
	if ( ResumePasswordServer() && !PasswordServerListening(1) )
	{
		debug("WARNING: password server did not start.\n");
	}
	else
	{
		ConvertLocalUsers();
		
		// send a HUP signal to start replication
		PausePasswordServer();
	}
	
	// remove the local replica cache so the client re-discovers the replica list
	if ( lstat( kPWReplicaLocalFile, &sb ) == 0 )
		unlink( kPWReplicaLocalFile );
	
	// verify that the replica configuration file got updated (paranoia check)
	{
		ReplicaFile *verifyRepFile = [ReplicaFile new];
		unsigned long pid = 0;
		
		if ( [verifyRepFile replicaCount] == 0 )
		{
			// DOH!!!
			debug("WARNING: replication list verification failed, attempting to resave.\n");
			[replicaFile saveXMLData];
			
			// brute-force restart
			if ( PasswordServerRunning( &pid ) )
			{
				kill( pid, SIGKILL );
				if ( gNeededToManuallyLaunchPasswordService )
					ResumePasswordServer();
			}
			
			// verify retry
			{
				ReplicaFile *verifyAgainRepFile = [ReplicaFile new];
				if ( [verifyAgainRepFile replicaCount] == 0 )
					return EX_CONFIG;
			}
		}
	}
	
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	StandAlone
//
//  Returns: exitcode
//-----------------------------------------------------------------------------

int StandAlone( int argc, char * const *argv )
{
	int exitcode = EX_OK;
	int err;
	bool passwordServerStopped = false;
	PWFileEntry userRec;
	AuthDBFile *authFile = nil;
	unsigned long modCount = 0;
	
	// clear the replica list associated with the current key
	DeletePWServerRecords( false );
	
	// clear replica list
	unlink( kPWReplicaFile );
	
	// shutdown our replica
	passwordServerStopped = StopPasswordServer();
	
	// save off the database temporarily to extract our local users
	if ( DatabaseExists() )
    {
		unlink( kPWFileSavedName );
		rename( kPWFilePath, kPWFileSavedName );
    }
	
	if ( argc == 2 )
	{
		PausePasswordServer();
		NewPWServerAdminUser( "disabled-slot-0x1", "temporary" ); 
			
		try
		{
			authFile = [[AuthDBFile alloc] init];
			if ( authFile == NULL )
				throw( (int)EX_SOFTWARE );
			
			err = [authFile validateFiles];
			if ( err != 0 )
				throw( (int)EX_NOINPUT );
				
			userRec.slot = 1;
			err = [authFile getPasswordRec:1 putItHere:&userRec unObfuscate:NO];
			if ( err != 0 )
				throw( (int)EX_NOINPUT );
			
			userRec.access.isDisabled = true;
			err = [authFile setPassword:&userRec atSlot:1 obfuscate:NO setModDate:YES];
			if ( err != 0 )
				throw( (int)EX_SOFTWARE );
		}
		catch( int catchErr )
		{
			unlink( kPWFilePath );
			
			exitcode = catchErr;
		}
		
		[authFile free];
		
		if ( exitcode == EX_OK )
		{
			if ( ResumePasswordServer() )
			{
				// make sure the server has time to start listening
				if ( ! PasswordServerListening(kMaxPasswordServerWait) ) {
					debug( "The administrator may not be bound to the password server because the server is not responding." );
					fprintf( stderr, "The administrator may not be bound to the password server because the server is not responding.\n" );
				}
			}
		}
	}
	else
	{
		// create a new database (-hostpasswordserver command)
		exitcode = HostPasswordServer( argc, argv );
	}
	
	if ( exitcode == EX_OK )
	{
		// transfer the password server records for local users
		modCount = ConvertLocalUsers( kConvertToNewEmptyDatabase );
		
		// revoke the database from the replicated system (the cheese stands alone)
		unlink( kPWFileSavedName );
	}
	else
	{
		rename( kPWFileSavedName, kPWFilePath );
	}
	
	if ( modCount == 0 )
	{
		// no accounts using password server, shut it off
		passwordServerStopped = StopPasswordServer();
		DeletePWServerRecords();
	}
	else
	{
		// if it didn't stop, ResumePasswordServer() will not start it so do it here
		if ( !passwordServerStopped )
			autoLaunchPasswordServer( true, true );
    }
	
	return exitcode;
}


//-----------------------------------------------------------------------------
//	Rekey
//
//  Returns: exitcode
//-----------------------------------------------------------------------------

int Rekey( const char *inBitCountStr )
{
	int exitcode = EX_OK;
	FILE *pf = NULL;
	unsigned int bitCount = 0;
	int idx = 0;
	char curChar;
	char commandBuf[256];
	char dbKey[kPWFileMaxPublicKeyBytes] = {0,};
		
	// get the bit count arg
	if ( inBitCountStr != NULL )
		sscanf( inBitCountStr, "%u", &bitCount );
	
	if ( bitCount != 0 && bitCount != 1024 && bitCount != 2048 && bitCount != 3072 )
		return EX_USAGE;
	
	// shutdown our password server
	if ( ! StopPasswordServer() )
		return EX_SOFTWARE;
	
	// clear the replica list associated with the current key
	DeletePWServerRecords( false );
	
	// rekey the database
	if ( bitCount == 0 )
		sprintf( commandBuf, "/usr/sbin/mkpassdb -rekeydb" );
	else
		sprintf( commandBuf, "/usr/sbin/mkpassdb -rekeydb %u", bitCount );
	
	pf = log_popen( commandBuf, "r" );
	if ( pf != NULL )
	{
		do
		{
			fscanf( pf, "%c", &curChar );
			if ( curChar != '\n' )
				dbKey[idx++] = curChar;
		}
		while ( curChar != '\n' && idx < (int)sizeof(dbKey) );
		dbKey[idx] = '\0';
		exitcode = pclose( pf );
	}
	
	// update the local NetInfo config record if present
	{
		ReplicaFile *replicaFile = [ReplicaFile new];
		NSString *xmlDataString = (NSString *)[replicaFile xmlString];
		long status; 
		
		if ( xmlDataString != nil )
		{
			status = SetPWConfigAttributeLocal( kPWConfigDefaultRecordName, kDS1AttrPasswordServerList, [xmlDataString UTF8String], false );
			if ( status != eDSRecordNotFound )
				debugerr( status, "WARNING: did not update the local NetInfo /config/passwordserver record (error = %ld).\n", status );
			[xmlDataString release];
		}
	}
	
	// rekey the users in the directory
	ConvertLocalUsers( kConvertToNewRSAKey, dbKey );
	
	// restart password server
	ResumePasswordServer();
	
	// output the new key
	printf( "%s\n", dbKey );
	
	return exitcode;
}


//-----------------------------------------------------------------------------
//	MigrateIP
//
//  Returns: exitcode
//
//  This command is used when promoting an LDAP replica to become the master.
//  It performs the migration of user records and the PasswordServerLocation
//  attribute for Jaguar. For other IP migration scenarios, use the changeip
//  tool instead.
//-----------------------------------------------------------------------------

int MigrateIP( int argc, char * const *argv )
{
	char *ptrToStaticBuf = NULL;
	long len;
	char *admin = NULL;
	char *pwsPass = NULL;
	long result;
	bool bRestrictToGivenIP;
	const char *oldIP = NULL;
	char newIP[256];
	
	// format of command:   2	3	4    	5
	// NeST -migrateip	  from to dir-user dir-pw 
	
	// decrypt password
	if ( argc == 6 )
	{
		result = GetCmdLinePass( argv[5], &pwsPass, &len );
		if ( result != EX_OK )
			return result;
		
		admin = argv[4];
	}
	else
	if ( argc == 5 )
	{
		// prompt for password
		char promptStr[256];
		
		admin = argv[4];
		snprintf( promptStr, sizeof(promptStr), "Password for %s:", admin );
		ptrToStaticBuf = getpass( promptStr );
		if ( ptrToStaticBuf == NULL )
			return EX_SOFTWARE;
			
		len = strlen( ptrToStaticBuf );
		pwsPass = (char *) malloc( len + 1 );
		if ( pwsPass == NULL )
			return EX_SOFTWARE;
	
		strcpy( pwsPass, ptrToStaticBuf );
		bzero( ptrToStaticBuf, len );
	}
	
	strlcpy( newIP, argv[3], sizeof(newIP) );
	
	bRestrictToGivenIP = ( strcasecmp( argv[2], "all" ) != 0 );
	oldIP = bRestrictToGivenIP ? argv[2] : NULL;
		
	ConvertLocalUsers( kConvertToNewIPAddress, oldIP, newIP );
	ConvertLDAPUsers( admin, pwsPass, kConvertToNewIPAddress, oldIP, newIP );
		
	SetPWServerAddress( admin, pwsPass, newIP, false, NULL, NULL );
	RemovePWSListAttributeAndPWSReplicaRecord( admin, pwsPass );
	
	if ( pwsPass != NULL )
	{
		bzero( pwsPass, strlen(pwsPass) );
		free( pwsPass );
	}
	
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	RevokeReplica
//
//	Returns: exitcode <sysexits.h>
//	Format: NeST -revokereplica [replica-name|replica-IP] [domain-admin]
//-----------------------------------------------------------------------------

int RevokeReplica( int argc, char * const *argv )
{
	char *ptrToStaticBuf = NULL;
	NSString *xmlDataString = nil;
	NSMutableDictionary *replicaDict = nil;
	ReplicaFile *replicaFile = [ReplicaFile new];
	NSString *replicaNameString = nil;
	NSString *replicaIDString = nil;
	
	if ( ArgsAreIPAddresses( argv, 2, 2 ) )
	{
		replicaNameString = (NSString *)[replicaFile getNameFromIPAddress:argv[2]];
		if ( replicaNameString == nil )
			return EX_CONFIG;
	}
	else
	{
		replicaNameString = [NSString stringWithUTF8String:argv[2]];
	}
	
	replicaDict = (NSMutableDictionary *)[replicaFile getReplicaByName:(CFStringRef)replicaNameString];
	if ( replicaDict == NULL )
		return EX_CONFIG;
	
	[replicaFile setReplicaStatus:kReplicaPermissionDenied forReplica:(CFMutableDictionaryRef)replicaDict];
	[replicaFile saveXMLData];
	
	xmlDataString = (NSString *)[replicaFile xmlString];
	if ( xmlDataString == nil )
		return EX_SOFTWARE;
	
	replicaIDString = (NSString *)[replicaFile getUniqueID];
	
	// if a domain admin name is provided, prompt for the password
	if ( argc == 4 )
	{
		char promptStr[256];
		
		snprintf( promptStr, sizeof(promptStr), "Password for %s:", argv[3] );
		ptrToStaticBuf = getpass( promptStr );
		if ( ptrToStaticBuf == NULL )
			return EX_SOFTWARE;
		
		SetPWServerAddress( argv[3], ptrToStaticBuf, NULL, false, [xmlDataString UTF8String], replicaIDString ? [replicaIDString UTF8String] : NULL, kSetupActionRevokeReplica );
	}
	else
	{
		SetPWServerAddress( NULL, NULL, NULL, false, [xmlDataString UTF8String], replicaIDString ? [replicaIDString UTF8String] : NULL, kSetupActionRevokeReplica );
	}
	
	// clean up
	[xmlDataString release];
	[replicaDict release];
	if ( ptrToStaticBuf != NULL )
		bzero( ptrToStaticBuf, strlen(ptrToStaticBuf) );
	
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	ArgsAreIPAddresses ()
//-----------------------------------------------------------------------------

bool ArgsAreIPAddresses	( char * const *argv, int firstArg, int lastArg )
{
	int index;
	char ptonResult[sizeof(struct in_addr) + 500];
	bool result = true;
	
	for ( index = firstArg; index <= lastArg; index++ )
	{
		if ( inet_pton(AF_INET, argv[index], &ptonResult) != 1 )
		{	
			result = false;
			break;
		}
	}
	
	return result;
}


//-----------------------------------------------------------------------------
//	PromoteToMaster ()
//-----------------------------------------------------------------------------

int PromoteToMaster( void )
{
	AuthDBFile *authFile = [[AuthDBFile alloc] init];
	ReplicaFile *replicaFile = [ReplicaFile new];
	NSMutableDictionary *curRepDict;
	NSMutableDictionary *parentDict;
	NSString *replicaNameString = nil;
	int err;
	PWFileHeader dbHeader;
	
	// get our replica name
	err = [authFile validateFiles];
	if ( err == 0 )
		err = [authFile getHeader:&dbHeader];
	if ( err != 0 ) {
		debug("Error: Could not validate the password server database.\n");
		fprintf(stderr, "Error: Could not validate the password server database.\n");
		return EX_OSFILE;
	}
	replicaNameString = [NSString stringWithUTF8String:dbHeader.replicationName];
	
	// retrieve the two items in the table to change
	curRepDict = (NSMutableDictionary *)[replicaFile getReplicaByName:(CFStringRef)replicaNameString];
	parentDict = (NSMutableDictionary *) [replicaFile getParent];
	if ( curRepDict == NULL || parentDict == NULL ) {
		debug("Error: Could not validate the replica table.\n");
		fprintf(stderr, "Error: Could not validate the replica table.\n");
		return EX_OSFILE;
	}
	
	// retrieve data to transfer
	NSObject *ipValue = [[curRepDict objectForKey:@kPWReplicaIPKey] retain];
	NSString *dnsValue = [[curRepDict objectForKey:@kPWReplicaDNSKey] retain];
	NSString *idBeginValue = [[curRepDict objectForKey:@kPWReplicaIDRangeBeginKey] retain];
	NSString *idEndValue = [[curRepDict objectForKey:@kPWReplicaIDRangeEndKey] retain];
	
	// Note: not checking the DNS key (optional key)
	if ( ipValue == NULL || idBeginValue == NULL || idEndValue == NULL ) {
		debug("Error: Could not extract data from the replica table.\n");
		fprintf(stderr, "Error: Could not extract data from the replica table.\n");
		return EX_CONFIG;
	}
		
	// we're ready, shut down the server for the transformation
	if ( ! StopPasswordServer() ) {
		debug("Error: Could not shut down the password server.\n");
		fprintf(stderr, "Error: Could not shut down the password server.\n");
		return EX_TEMPFAIL;
	}
	
	// update the table
	[replicaFile decommissionReplica:(CFStringRef)replicaNameString];
	[replicaFile stripSyncDates];
	
	// Note: The DNS key must be cleared because there may not be a replacement
	[parentDict removeObjectForKey:@kPWReplicaDNSKey];
		
	// some other keys to remove
	[parentDict removeObjectForKey:@kPWReplicaPolicyKey];
	[parentDict removeObjectForKey:@kPWReplicaSyncDateKey];
	[parentDict removeObjectForKey:@kPWReplicaSyncServerKey];
	[parentDict removeObjectForKey:@kPWReplicaStatusKey];
	[parentDict removeObjectForKey:@kPWReplicaSyncAttemptKey];
	[parentDict removeObjectForKey:@kPWReplicaIncompletePullKey];
	
	// replace values
	[parentDict setObject:ipValue forKey:@kPWReplicaIPKey];
	[parentDict setObject:idBeginValue forKey:@kPWReplicaIDRangeBeginKey];
	[parentDict setObject:idEndValue forKey:@kPWReplicaIDRangeEndKey];
	if ( dnsValue != NULL ) {
		[parentDict setObject:dnsValue forKey:@kPWReplicaDNSKey];
		[dnsValue release];
	}
	
	[ipValue release];
	[idBeginValue release];
	[idEndValue release];
	
	// update the parent entry's mod date
	[replicaFile setEntryModDateForReplica:(CFMutableDictionaryRef)parentDict];
	
	// save
	[replicaFile saveXMLData];
	
	// update identity
	bzero( dbHeader.replicationName, sizeof(dbHeader.replicationName) );
	err = [authFile setHeader:&dbHeader];
	if ( err != 0 ) {
		fprintf(stderr, "Error: Could not update the server's identity.\n");
		return EX_IOERR;
	}
	
	// restart the server as the parent
	if ( ! ResumePasswordServer() ) {
		fprintf(stderr, "Warning: Could not start the password server.\n");
	}
	
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	LocalToShadowHash
//-----------------------------------------------------------------------------

int LocalToShadowHash(void)
{
	unsigned long recordsModified;
	
	recordsModified = ConvertLocalUsers(kConvertToShadowHash);
	printf("%lu record%s modified.\n", recordsModified, (recordsModified==1) ? "" : "s");
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	LocalToLDAP
//-----------------------------------------------------------------------------

int LocalToLDAP(const char *inAdminUser, bool quiet)
{
	unsigned long recordsModified = 0;
	char *ldapPass = NULL;
	int len;
	
	char *ptrToStaticBuf = getpass(quiet ? "":"Password:");
	if ( ptrToStaticBuf != NULL )
	{
		len = strlen( ptrToStaticBuf );
		if ( len > 511 ) {
			return EX_USAGE;
		}
		ldapPass = (char *) malloc( len + 1 );
		if ( ldapPass == NULL ) {
			debug( "memory error\n" );
			return EX_OSERR;
		}
		strlcpy( ldapPass, ptrToStaticBuf, len + 1 );
	}
	
	recordsModified = MigrateLocalToLDAP(inAdminUser, ldapPass);
	printf("%lu record%s modified.\n", recordsModified, (recordsModified==1) ? "" : "s");
	return EX_OK;
}


//-----------------------------------------------------------------------------
//	MigrateLocalToLDAP
//
//	Returns: the number of records that were modified
//
//-----------------------------------------------------------------------------

unsigned long MigrateLocalToLDAP( const char *inAdminUser, const char *inAdminPass )
{
	tDirReference				dsRef							= 0;
    tDataBuffer				   *tDataBuff						= NULL;
    tDirNodeReference			nodeRef							= 0;
    long						status							= eDSNoErr;
    tContextData				context							= nil;
    unsigned long				index							= 0;
    unsigned long				nodeCount						= 0;
	tDataList					*recordNameList					= nil;
	tDataList					*recordTypeList					= nil;
	tDataList					*attributeList					= nil;
	unsigned long				recIndex						= 0;
	unsigned long				recCount						= 0;
	tRecordEntry		  		*recEntry						= nil;
	tAttributeListRef			attrListRef						= 0;
	unsigned long				modCount						= 0;
/*
	char						*userName						= nil;
	char						*tptr							= nil;
	char 						*version;
	char 						*tag;
	char 						*data;
*/
	DSUtils						dsUtils;
	//PWFileEntry					userRec;
	
	status = dsUtils.GetLocallyHostedNodeList();
	if ( status != eDSNoErr )
		return modCount;
	
	dsRef = dsUtils.GetDSRef();
	
    do
    {
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if ( tDataBuff == NULL )
			break;
				
		recordNameList = dsBuildListFromStrings( dsRef, kDSRecordsAll, nil );
		recordTypeList = dsBuildListFromStrings( dsRef, kDSStdRecordTypeUsers, nil );
		attributeList = dsBuildListFromStrings( dsRef, kDSAttributesAll, nil );
		
		nodeCount = dsUtils.GetLocallyHostedNodeCount();
        for ( index = 1; index <= nodeCount; index++ )
        {
			status = dsUtils.OpenLocallyHostedNode( index );
			if ( status != eDSNoErr )
				continue;
            
			nodeRef = dsUtils.GetCurrentNodeRef();
			
			do
			{
				status = dsGetRecordList( nodeRef, tDataBuff, recordNameList, eDSExact,
											recordTypeList, attributeList, false,
											&recCount, &context );
				if (status != eDSNoErr) break;
				
				for ( recIndex = 1; recIndex <= recCount; recIndex++ )
				{
					status = dsGetRecordEntry( nodeRef, tDataBuff, recIndex, &attrListRef, &recEntry );
					if ( status != eDSNoErr && recEntry == NULL )
						continue;
					
					
				}
			}
			while ( status == eDSNoErr && context != NULL );
        }
    }
    while(false);
	
    if ( tDataBuff != NULL ) {
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
	
	return modCount;
}


//-----------------------------------------------------------------------------
//	_version ()
//
//-----------------------------------------------------------------------------

void _version ( FILE *inFile )
{
	static const char * const	_ver =
		"NetInfo Setup Tool (NeST), Apple Computer, Inc.,  Version 2.2\n";

	fprintf( inFile, _ver );
} // _version


//-----------------------------------------------------------------------------
//	_appleVersion ()
//
//-----------------------------------------------------------------------------

void _appleVersion ( FILE *inFile )
{
	static const char * const	_ver =
		"NetInfo Setup Tool (NeST), Apple Computer, Inc.,  Version 255\n";
	
	fprintf( inFile, _ver );
} // _appleVersion


//-----------------------------------------------------------------------------
//	 _localonly ()
//
//-----------------------------------------------------------------------------

long _localonly( void )
{
	setDSSearchPolicy( 2 ); // local only
	return _nonetinfo();	
} //  _localonly


//-----------------------------------------------------------------------------
//	 _nonetinfo ()
//
//-----------------------------------------------------------------------------

long _nonetinfo( void )
{
	bool					scpStatus	= true;
	SCPreferencesRef		scpRef;
	long					status		= 0;

	scpRef = SCPreferencesCreate( NULL, CFSTR("NeST"), 0 );
	if ( scpRef == NULL )
	{
		return -1;
	}

	_removeBindings( scpRef );
	_setInactiveFlag( scpRef );

    scpStatus = SCPreferencesCommitChanges( scpRef );
	scpStatus = SCPreferencesApplyChanges( scpRef );
	CFRelease( scpRef );

	return( status );

} //  _nonetinfo


//-----------------------------------------------------------------------------
//	_destroyParent ()
//
//-----------------------------------------------------------------------------

void _destroyParent ( const char *inTag )
{
	bool				scpStatus	= true;
	SCPreferencesRef			scpRef;
	char					string[ 256 ];

	// kill LDAP server
    setLDAPServer( false );
	setDSSearchPolicy( 2 ); // local only
	scpRef = SCPreferencesCreate( NULL, CFSTR("NeST"), 0 );
	if ( scpRef != NULL )
	{
		_removeBindings( scpRef );
	
		_setInactiveFlag( scpRef );
	
		scpStatus = SCPreferencesCommitChanges( scpRef );
		scpStatus = SCPreferencesApplyChanges( scpRef );
		CFRelease( scpRef );
		sleep( 2 );
	}

	snprintf( string, sizeof(string), "/usr/sbin/nidomain -d %s", inTag );

	system( string );

	snprintf( string, sizeof(string), "/var/db/netinfo/.%s.tim", inTag );
    remove(string);
	
	setNIServer(false);
	
} // _destroyParent


//-----------------------------------------------------------------------------
//	_destroyOrphanedParent ()
//
//-----------------------------------------------------------------------------

void _destroyOrphanedParent ( const char *inTag )
{
	char					string[ 256 ];

	// kill LDAP server
	setLDAPServer( false );
	snprintf( string, sizeof(string), "/usr/sbin/nidomain -d %s", inTag );

	system( string );

	snprintf( string, sizeof(string), "/var/db/netinfo/.%s.tim", inTag );
    remove(string);
	
	setNIServer(false);
    
} // _destroyParent


//-----------------------------------------------------------------------------
//	 _removeBindings ()
//
//-----------------------------------------------------------------------------

void _removeBindings ( SCPreferencesRef scpRef )
{
	bool					scpStatus	= true;
	char				   *pCurrSet	= nil;
	CFStringRef				cfNetInfoKeyPath;
	CFTypeRef				cfTypeRef;
	char					cArray1[ 2048 ];
	
	// Modify the system config file
	sprintf( cArray1, kBindingPath, "/Sets/0" );
	
	//	Get the current set
	cfTypeRef = SCPreferencesGetValue( scpRef, CFSTR( "CurrentSet" ) );
	if ( cfTypeRef != NULL )
	{
		pCurrSet = (char *)CFStringGetCStringPtr( (CFStringRef)cfTypeRef, kCFStringEncodingMacRoman );
		if ( pCurrSet != nil )
		{
			sprintf( cArray1, kNetInfoPath, pCurrSet );
		}
	}
    
	cfNetInfoKeyPath = CFStringCreateWithCString( kCFAllocatorDefault, cArray1, kCFStringEncodingMacRoman );
	
	// Remove any old binding methods
	scpStatus = SCPreferencesPathRemoveValue( scpRef, cfNetInfoKeyPath );

	CFRelease( cfNetInfoKeyPath );
} // _removeBindings


//-----------------------------------------------------------------------------
//	 _setInactiveFlag ()
//
//-----------------------------------------------------------------------------

void _setInactiveFlag ( SCPreferencesRef scpRef )
{
	bool				scpStatus	= true;
	unsigned long			uiDataLen	= 0;
	char				   *pCurrSet	= nil;
	CFStringRef				cfInactive;
	CFTypeRef				cfTypeRef;
	char					cArray1[ 2048 ];
	CFDataRef				dataRef;
	CFDictionaryRef			dictRef;
	CFPropertyListRef		plistRef;

	_removeBindings( scpRef );

	// Set the new binding method to broadcast
	uiDataLen = strlen( kInactive );
	dataRef = CFDataCreate( nil, (const UInt8 *)kInactive, uiDataLen );
	if ( dataRef != nil )
	{
		plistRef = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nil );
		if ( plistRef != nil )
		{
			dictRef = (CFDictionaryRef)plistRef;

			sprintf( cArray1, kNetInfoPath, "/Sets/0" );
			cfInactive = CFStringCreateWithCString( kCFAllocatorDefault, cArray1, kCFStringEncodingMacRoman );
	
			//	Get the current set
			cfTypeRef = SCPreferencesGetValue( scpRef, CFSTR( "CurrentSet" ) );
			if ( cfTypeRef != NULL )
			{
				pCurrSet = (char *)CFStringGetCStringPtr( (CFStringRef)cfTypeRef, kCFStringEncodingMacRoman );
				if ( pCurrSet != nil )
				{
					sprintf( cArray1, kNetInfoPath, pCurrSet );
					cfInactive = CFStringCreateWithCString( kCFAllocatorDefault, cArray1, kCFStringEncodingMacRoman );
				}
			}
	
			scpStatus = SCPreferencesPathSetValue( scpRef, cfInactive, dictRef );
	
			CFRelease( cfInactive );

			CFRelease( dictRef );
		}
	}
} // _setInactiveFlag


//------------------------------------------------------------------------------------------------
//	GetPWServerStyle
//------------------------------------------------------------------------------------------------

PWServerStyle GetPWServerStyle( void )
{
    char addressStr[256];
    char configAddrStr[256] = {0};
    char *tptr;
    
    // get this machine's IP
	GetMyAddressAsString( addressStr );
    
    // get the IP from the config record
    GetPWServerAddresses(configAddrStr);
    tptr = strchr(configAddrStr, ',');
    if ( tptr ) *tptr = '\0';
    
    if ( *configAddrStr == '\0' )
        return kPasswordServerNone;
    else
    if ( strcmp( addressStr, configAddrStr ) == 0 )
        return kPasswordServerHost;

    return kPasswordServerUse;
}


void DeletePWServerRecords( bool removeDefaultIPRec )
{
	DSUtilsDeleteRecords dsUtils( removeDefaultIPRec );
	
	/*long status = (long)*/ dsUtils.DoActionForAllLocalNodes();
}


bool DatabaseExists( void )
{
	struct stat sb;
    int err = -1;
	
	err = lstat( kPWFilePath, &sb );

	return ( err == 0 );
}


//-----------------------------------------------------------------------------
//	MakeReplica
//
//	Returns: false if function exits and no database exists
//-----------------------------------------------------------------------------

bool MakeReplica( const char *inUserName, const char *inPubicKey, const char *inPrivateKey, long inPrivateKeyLen, unsigned long inFirstSlot, unsigned long inNumSlots, const char *inReplicaName )
{
    // mkpassdb -a -u %s -p -q 
    
    FILE *pf;
    bool result = true;
	bool hasDatabase;
	char *encodedPrivateKey;
	char cmdStr[256];
	
    if ( inUserName == nil || inPubicKey == nil || inPrivateKey == nil )
        return false;
    
	hasDatabase = DatabaseExists();
    if ( hasDatabase )
		rename( kPWFilePath, kPWFileSavedName );
    
	encodedPrivateKey = (char *)malloc( inPrivateKeyLen * 2 + 1 );
	ConvertBinaryToHex( (const unsigned char *)inPrivateKey, inPrivateKeyLen, encodedPrivateKey );
	
	sprintf( cmdStr, "/usr/sbin/mkpassdb -zoq -s %lu -e %lu", inFirstSlot, inNumSlots );
	if ( inReplicaName != NULL && *inReplicaName != '\0' )
	{
		strcat( cmdStr, " -n " );
		strcat( cmdStr, inReplicaName );
	}
	
	pf = log_popen(cmdStr, "w");
    if (pf != nil)
    {
        fprintf(pf, "%s\n", inPubicKey);
        fprintf(pf, "%s\n", encodedPrivateKey);
        pclose(pf);
    }
	
	free( encodedPrivateKey );
	
	if ( ! hasDatabase )
		result = DatabaseExists();
	
	return result;
}


//-----------------------------------------------------------------------------
//	GetPasswordServerKey
//
//	Returns: TRUE if the key was obtained.
//-----------------------------------------------------------------------------

bool GetPasswordServerKey( char *key, long maxlen )
{
	FILE *fp;
	char curChar;
	int idx = 0;
	bool loaded = false;
	
	fp = log_popen( "/usr/sbin/mkpassdb -key", "r" );
	if ( fp != NULL )
	{
		do
		{
			fscanf( fp, "%c", &curChar );
			if ( curChar != '\n' )
				key[idx++] = curChar;
		}
		while ( curChar != '\n' && idx < maxlen );
		key[idx] = '\0';
		
		loaded = true;
		pclose(fp);
	}
	
	return loaded;
}


//-----------------------------------------------------------------------------
//	ConvertUser
//
//	Returns: void
//-----------------------------------------------------------------------------

void ConvertUser(
	const char *currentPasswordServerKey,
	const char *userName,
	const char *password,
	bool isHashData,
	bool isAdminUser,
	const char *dirAdmin,
	const char *dirAdminPass )
{
	char *tptr;
	char addressStr[256];
	char idStr[2048];
	bool hasServerID = false;
	char *userNodeName = NULL;
	DSUtils dsUtils;
	
	// get the current password server address
	//GetPWServerAddresses( addressStr );
	tDirStatus status = dsUtils.GetServerAddressForUser( userName, addressStr, &userNodeName );
	if ( status != eDSNoErr || addressStr[0] == '\0' ) {
		printf("no password server\n");
		exit( EX_UNAVAILABLE );
	}
	
	tptr = strchr(addressStr, ',');
	if ( tptr != NULL )
		*tptr = '\0';
	
	// do not allow blank passwords
	if ( *password == '\0' ) {
		debug( "blank passwords are not allowed.\n");
		exit( EX_USAGE );
	}
	
	hasServerID = GetAuthAuthority( addressStr, userName, password, idStr );
	if ( hasServerID )
	{
		// does the key match our local database?
		char *userKey;
		
		userKey = strchr( idStr, ',' );
		if ( userKey != NULL && currentPasswordServerKey != NULL )
		{
			// if the keys match, the user is already a valid password server user
			if ( strcmp( userKey + 1, currentPasswordServerKey ) == 0 ) {
				debug( "user key matches password server key.\n");
				return;
			}
		}
	}
	
	// need to add
	PausePasswordServer();
	if ( NewPWServerAddUser( userName, password, isAdminUser ) == false )
	{
		// database creation failed
		debug( "could not add user, maybe no database.\n" );
		fprintf(stderr, "could not add user, maybe no database.\n");
		exit(-1);
	}
	// guarantee running
	ResumePasswordServer();
	
	status = (tDirStatus) VerifyUser( "127.0.0.1", userName, password, true, idStr );
	if ( status == eDSNoErr )
	{
		DSUtilsAuthAuthority dsUtils( userName, addressStr, password, idStr, false, false );
		status = dsUtils.OpenNodeByName( userNodeName, dirAdmin, dirAdminPass );
		if ( status == eDSNoErr )
		{
			char realmName[256];
			
			status = dsUtils.DoActionOnCurrentNode();
			
			if ( status == eDSNoErr )
			{
				if ( AddPrincipal( userName, password, realmName, sizeof(realmName) ) == 0 )
				{
					dsUtils.SetAuthTypeToKerberos( realmName );
					status = dsUtils.DoActionOnCurrentNode();
				}
			}
			else
			{
				debug( "Error: Could not modify the user's record (error = %d).\n", status );
			}
		}
		else
		{
			debug( "Error: Could not open directory node: %s (error = %d).\n", userNodeName, status );
		}
	}
	else
	{
		debug( "Error: Could not verify the user has a password server account (error = %d).", status );
	}
	
	if ( userNodeName != NULL )
		free( userNodeName );
}


void AddUserToNewDatabase( const char *currentPasswordServerKey, const char *userName, PWFileEntry *pwRec,
		const char *dirAdmin, const char *password, bool keepSlotNumber )
{
	char *tptr;
	bool hasServerID = false;
	char addressStr[256];
	char idStr[1024];
	int err;
	
	AuthDBFile *authFile = [[AuthDBFile alloc] init];
    if ( authFile == nil )
		return;
	
	err = [authFile validateFiles];
	if ( err != 0 )
		return;
	
	// get the current password server address
	GetPWServerAddresses( addressStr );
	if ( addressStr[0] == '\0' ) {
		GetMyAddressAsString( addressStr );
		if ( addressStr[0] == '\0' ) {
			printf("no password server\n");
			exit(0);
		}
	}
	
	tptr = strchr(addressStr, ',');
	if ( tptr != NULL )
		*tptr = '\0';
	
	hasServerID = GetAuthAuthority( addressStr, userName, "", idStr );
	if ( hasServerID )
	{
		// does the key match our local database?
		char *userKey;
		
		userKey = strchr( idStr, ',' );
		if ( userKey != NULL && currentPasswordServerKey != NULL )
		{
			// if the keys match, the user is already a valid password server user
			if ( strcmp( userKey + 1, currentPasswordServerKey ) == 0 ) {
				debug( "user key matches password server key.\n");
				return;
			}
		}
	}
	
	// need to add
	PausePasswordServer();
		
	if ( keepSlotNumber )
		err = [authFile addPassword:pwRec atSlot:pwRec->slot obfuscate:NO];
	else
		err = [authFile addPassword:pwRec obfuscate:NO];
	if ( authFile != nil ) {
		[authFile closePasswordFile];
		[authFile free];
	}
	
	// guarantee running
	ResumePasswordServer();
	
	if ( err == 0 )
	{
		pwsf_passwordRecRefToString( pwRec, idStr );
		idStr[34] = ',';
		strlcpy( idStr + 35, currentPasswordServerKey, sizeof(idStr) - 35 );
		
		SetAuthAuthority( addressStr, userName, "", idStr, dirAdmin, password );
	}
}


//-----------------------------------------------------------------------------
//	ConvertLocalUsers
//
//	Returns: the number of records that were modified
//
//	Gets all password server users from the directory and re-adds them to
//	the password server after it becomes a replica of a greater system.
//	It can also convert to basic or a new IP address.
//-----------------------------------------------------------------------------

unsigned long ConvertLocalUsers( ConvertTarget inConvertToWhat, const char *inParam1, const char *inNewIP, const char *inAdminUser, const char *inAdminPass )
{
	tDirReference				dsRef							= 0;
    tDataBuffer				   *tDataBuff						= NULL;
    tDirNodeReference			nodeRef							= 0;
    long						status							= eDSNoErr;
    tContextData				context							= nil;
	tAttributeValueEntry	   *pExistingAttrValue				= NULL;
    unsigned long				index							= 0;
    unsigned long				nodeCount						= 0;
    bool						hasServerID						= false;
    unsigned long				attrValueIDToReplace			= 0;
	tDataList					*recordNameList					= nil;
	tDataList					*recordTypeList					= nil;
	tDataList					*attributeList					= nil;
	unsigned long				recIndex						= 0;
	unsigned long				recCount						= 0;
	char						*authAuthorityStr				= nil;
	tRecordEntry		  		*recEntry						= nil;
	tAttributeListRef			attrListRef						= 0;
	FILE						*fp								= nil;
	char						*userName						= nil;
	char						dbKey[kPWFileMaxPublicKeyBytes]	= {0};
	char						*tptr							= nil;
	unsigned long				modCount						= 0;
	const char					*inOldIP						= inParam1;
	const char					*inNewRSAKey					= inParam1;
	char 						*version;
	char 						*tag;
	char 						*data;
	DSUtils						dsUtils;
	PWFileEntry					userRec;
	
	status = dsUtils.GetLocallyHostedNodeList();
	if ( status != eDSNoErr )
		return modCount;
	
	dsRef = dsUtils.GetDSRef();
	
    do
    {
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if ( tDataBuff == NULL )
			break;
		
		if ( inConvertToWhat == kConvertToNewDatabase || inConvertToWhat == kConvertToNewEmptyDatabase )
		{
			if ( ! GetPasswordServerKey( dbKey, sizeof(dbKey) ) )
				return modCount;
			
			fp = fopen( kPWFileSavedName, "r" );
			if ( fp == NULL )
				return modCount;
		}
		
		recordNameList = dsBuildListFromStrings( dsRef, (inAdminUser != NULL) ? inAdminUser : kDSRecordsAll, nil );
		recordTypeList = dsBuildListFromStrings( dsRef, kDSStdRecordTypeUsers, nil );
		attributeList = dsBuildListFromStrings( dsRef, kDSNAttrAuthenticationAuthority, kDSNAttrGroupMembership, kDSNAttrRecordName, nil );
		
		nodeCount = dsUtils.GetLocallyHostedNodeCount();
        for ( index = 1; index <= nodeCount; index++ )
        {
            // initialize state
            hasServerID = false;
            pExistingAttrValue = nil;
            attrValueIDToReplace = 0;
            
			status = dsUtils.OpenLocallyHostedNode( index );
			if ( status != eDSNoErr )
				continue;
            
			nodeRef = dsUtils.GetCurrentNodeRef();
			
			do
			{
				status = dsGetRecordList( nodeRef, tDataBuff, recordNameList, eDSExact,
											recordTypeList, attributeList, false,
											&recCount, &context );
				if (status != eDSNoErr) break;
				
				for ( recIndex = 1; recIndex <= recCount; recIndex++ )
				{
					status = dsGetRecordEntry( nodeRef, tDataBuff, recIndex, &attrListRef, &recEntry );
					if ( status != eDSNoErr && recEntry == NULL )
						continue;
					
					status = GetDataFromDataBuff( dsRef, nodeRef, tDataBuff, recEntry, attrListRef, &authAuthorityStr, &userName );
					if ( status == eDSNoErr && authAuthorityStr != NULL )
					{
						switch( inConvertToWhat )
						{
							case kConvertToBasic:
								SetAuthAuthorityToBasic( userName, inAdminPass ? inAdminPass : "admin" );
								modCount++;
								break;
							
							case kConvertToNewDatabase:
								if ( GetPasswordDataForID( fp, authAuthorityStr, &userRec ) )
								{
									AddUserToNewDatabase( dbKey, userName, &userRec, NULL, NULL, NO );
									modCount++;
								}
								break;
							
							case kConvertToNewEmptyDatabase:
								if ( GetPasswordDataForID( fp, authAuthorityStr, &userRec ) )
								{
									AddUserToNewDatabase( dbKey, userName, &userRec, NULL, NULL, (userRec.slot==1) );
									modCount++;
								}
								break;
							
							case kConvertToNewIPAddress:
								status = dsParseAuthAuthority( authAuthorityStr, &version, &tag, &data );
								if ( status == eDSNoErr )
								{
									// tag is already checked in GetDataFromDataBuff()
									
									tptr = strchr( data, ':' );
									if ( tptr != NULL )
									{
										if ( inOldIP == NULL || strcmp( inOldIP, tptr + 1 ) == 0 )
										{
											*tptr = '\0';
											SetAuthAuthority( inNewIP, userName, "", data );
											modCount++;
										}
									}
									
									if ( version != NULL ) free( version );
									if ( tag != NULL ) free( tag );
									if ( data != NULL ) free( data );
								}
								break;
								
							case kConvertToNewRSAKey:
								status = dsParseAuthAuthority( authAuthorityStr, &version, &tag, &data );
								if ( status == eDSNoErr )
								{
									// tag is already checked in GetDataFromDataBuff()
									
									tptr = strchr( data, ':' );
									if ( tptr != NULL )
									{
										char newAAData[kPWFileMaxPublicKeyBytes+36];
										
										strlcpy( newAAData, data, 36 );
										strlcat( newAAData, inNewRSAKey, sizeof(newAAData) );
										
										SetAuthAuthority( tptr + 1, userName, "", newAAData );
										modCount++;
									}
									
									if ( version != NULL ) free( version );
									if ( tag != NULL ) free( tag );
									if ( data != NULL ) free( data );
								}
								break;
							
							case kConvertToShadowHash:
								/*
								if ( GetPasswordDataForID( fp, authAuthorityStr, &userRec ) )
								{
									unsigned char hashes[kHashTotalLength] = {0};
									UInt32 hashesLen;
									
									// make shadowhash
									if ( userRec.access.passwordIsHash )
									{
										memcpy( outHashes, &userRec.digest[kPWHashSlotSMB_NT].digest[1], 32 );
										memcpy( outHashes + kHashShadowOneLength, &userRec.digest[kPWHashSlotSMB_LAN_MANAGER].digest[1], 32 );
										memcpy( outHashes + kHashOffsetToCramMD5, &userRec.digest[kPWHashSlotCRAM_MD5].digest[1], 64 );
									}
									else
									{
										pwsf_DESAutoDecode( "1POTATO2", userRec.passwordStr );
										GenerateShadowHashes(
											userRec.passwordStr, strlen(userRec.passwordStr),
											kNiPluginHashDefaultServerSet, NULL,
											hashes, &hashesLen );
									}
									
									modCount++;
								}
								*/
								break;
						}
					}
					
					if ( authAuthorityStr != NULL ) {
						free( authAuthorityStr );
						authAuthorityStr = NULL;
					}
					if ( userName != NULL ) {
						free( userName );
						userName = NULL;
					}
				}
			}
			while ( status == eDSNoErr && context != NULL );
        }
    }
    while(false);
    
	if ( fp != NULL )
		fclose( fp );
	
    if ( tDataBuff != NULL ) {
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
	
	return modCount;
}


//--------------------------------------------------------------------------------------------------
//	ConvertLDAPUsers
//
//  Returns: ds err or -1 for non-DS errors
//--------------------------------------------------------------------------------------------------

long ConvertLDAPUsers( const char *inLDAPAdmin, const char *inAdminPass,
	ConvertTarget inConvertToWhat, const char *inOldIP, const char *inNewIP )
{
	tDirReference				dsRef							= 0;
    tDataBuffer				   *tDataBuff						= NULL;
    tDirNodeReference			nodeRef							= 0;
    long						status							= eDSNoErr;
    tContextData				context							= nil;
	tAttributeValueEntry	   *pExistingAttrValue				= NULL;
    unsigned long				attrValueIDToReplace			= 0;
	tDataList					*recordNameList					= nil;
	tDataList					*recordTypeList					= nil;
	tDataList					*attributeList					= nil;
	unsigned long				recIndex						= 0;
	unsigned long				recCount						= 0;
	char						*authAuthorityStr				= nil;
	tRecordEntry		  		*recEntry						= nil;
	tAttributeListRef			attrListRef						= 0;
	FILE						*fp								= nil;
	char						*userName						= nil;
	char						dbKey[kPWFileMaxPublicKeyBytes]	= {0};
	char						*tptr							= nil;
	unsigned long				buffSize						= 4096;
	char 						*version;
	char 						*tag;
	char 						*data;
	DSUtils						dsUtils;
	PWFileEntry					userRec;
	
	if ( inConvertToWhat == kConvertToBasic || inConvertToWhat == kConvertToNewRSAKey )
		return -1;
	
	status = dsUtils.OpenLocalLDAPNode( inLDAPAdmin, inAdminPass );
	if ( status != eDSNoErr )
		return status;
	
	dsRef = dsUtils.GetDSRef();
	
	tDataBuff = dsDataBufferAllocate( dsRef, buffSize );
	if ( tDataBuff == NULL )
		return -1;
	
	if ( inConvertToWhat == kConvertToNewDatabase || inConvertToWhat == kConvertToNewEmptyDatabase )
	{
		if ( ! GetPasswordServerKey( dbKey, sizeof(dbKey) ) )
			return -1;
		
		fp = fopen( kPWFileSavedName, "r" );
		if ( fp == NULL )
			return -1;
	}
	
    try
    {
		recordNameList = dsBuildListFromStrings( dsRef, kDSRecordsAll, nil );
		recordTypeList = dsBuildListFromStrings( dsRef, kDSStdRecordTypeUsers, nil );
		attributeList = dsBuildListFromStrings( dsRef, kDSNAttrAuthenticationAuthority, kDSNAttrRecordName, nil );
		
		// initialize state
		pExistingAttrValue = nil;
		attrValueIDToReplace = 0;
            
		nodeRef = dsUtils.GetCurrentNodeRef();
			
		do
		{
			do
			{
				status = dsGetRecordList( nodeRef, tDataBuff, recordNameList, eDSExact, recordTypeList, attributeList, false,
												&recCount, &context );
				
				if ( status == eDSBufferTooSmall )
				{
					buffSize *= 2;
					
					// a safety for a runaway condition
					if ( buffSize > 1024 * 1024 )
						throw( (long)eDSBufferTooSmall );
					
					dsDataBufferDeAllocate( dsRef, tDataBuff );
					tDataBuff = dsDataBufferAllocate( dsRef, buffSize );
					if ( tDataBuff == NULL )
						throw( (long)eMemoryError );
				}
			}
			while ( status == eDSBufferTooSmall );
			
			if ( status != eDSNoErr )
				throw( status );
				
			for ( recIndex = 1; recIndex <= recCount; recIndex++ )
			{
				status = dsGetRecordEntry( nodeRef, tDataBuff, recIndex, &attrListRef, &recEntry );
				if ( status != eDSNoErr && recEntry == NULL )
					continue;
				
				status = GetDataFromDataBuff( dsRef, nodeRef, tDataBuff, recEntry, attrListRef, &authAuthorityStr, &userName );
				if ( status == eDSNoErr && authAuthorityStr != NULL )
				{
					switch( inConvertToWhat )
					{
						case kConvertToBasic:
						case kConvertToNewRSAKey:
							// not supported
							break;
						
						case kConvertToNewDatabase:
						case kConvertToNewEmptyDatabase:
							if ( GetPasswordDataForID( fp, authAuthorityStr, &userRec ) )
								AddUserToNewDatabase( dbKey, userName, &userRec, inLDAPAdmin, inAdminPass );
							break;
						
						case kConvertToNewIPAddress:
							status = dsParseAuthAuthority( authAuthorityStr, &version, &tag, &data );
							if ( status == eDSNoErr )
							{
								// tag is already checked in GetDataFromDataBuff()
								
								tptr = strchr( data, ':' );
								if ( tptr != NULL )
								{
									if ( inOldIP == NULL || strcmp( inOldIP, tptr + 1 ) == 0 )
									{
										*tptr = '\0';
										
										//SetAuthAuthority( inNewIP, userName, "", data, inLDAPAdmin, inAdminPass );
										{
											long status = eDSNoErr;
											
											try
											{
												if (inNewIP == nil)
													throw( -1 );
												
												DSUtilsAuthAuthority dsUtils( userName, inNewIP, "", data, false, false );
												dsUtils.CopyUserID( data );
												if ( inLDAPAdmin != NULL && inAdminPass != NULL )
												{
													status = dsUtils.OpenLocalLDAPNode( inLDAPAdmin, inAdminPass );
													if (status != eDSNoErr)
														throw( status );
													
													status = dsUtils.DoActionOnCurrentNode();
													dsUtils.CopyUserID( data );
													if (status != eDSNoErr)
														throw( status );
												}
											}
											catch(long catchErr)
											{
												status = catchErr;
												debugerr( catchErr, "HandleAuthAuthority error = %ld\n", catchErr );
											}
											catch(...)
											{
												debug( "HandleAuthAuthority: an unexpected error occurred.\n" );
											}
										}
									}
								}
								
								if ( version != NULL ) free( version );
								if ( tag != NULL ) free( tag );
								if ( data != NULL ) free( data );
							}
							break;
						
						case kConvertToShadowHash:
							// not valid for LDAP
							break;
					}
				}
				
				if ( authAuthorityStr != NULL ) {
					free( authAuthorityStr );
					authAuthorityStr = NULL;
				}
				if ( userName != NULL ) {
					free( userName );
					userName = NULL;
				}
			}
		}
		while ( status == eDSNoErr && context != NULL );
    }
    catch( long catchErr )
	{
		status = catchErr;
	}
    catch(...)
	{
		status = -1;
	}
	
	if ( fp != NULL )
		fclose( fp );
	
    if ( tDataBuff != NULL ) {
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
	
	return status;
}


//--------------------------------------------------------------------------------------------------
//	GetDataFromDataBuff
//--------------------------------------------------------------------------------------------------

long GetDataFromDataBuff(
	tDirReference dsRef,
	tDirNodeReference inNodeRef,
	tDataBuffer *tDataBuff,
	tRecordEntry *pRecEntry,
	tAttributeListRef attrListRef,
	char **outAuthAuthority,
	char **outRecordName )
{
	sInt32					error			= eDSNoErr;
	uInt32					j				= 0;
	uInt32					k				= 0;
	char				   *pRecNameStr		= nil;
	char				   *pRecTypeStr		= nil;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	
	// Do not initialize to NULL; this method may be called multiple times
	// and we do not want to stomp on a successful result
	//*outAuthAuthority = NULL;
	
	*outRecordName = NULL;
	
	if ( inNodeRef != 0 && pRecEntry != nil )
	{
		error = dsGetRecordNameFromEntry( pRecEntry, &pRecNameStr );
		if ( error == eDSNoErr )
			error = dsGetRecordTypeFromEntry( pRecEntry, &pRecTypeStr );
		if ( error == eDSNoErr )
		{
			// DEBUG
			if ( true )
			{
				debug( "\n" );
				debug( "    Record Name     = %s\n", pRecNameStr );
				debug( "    Record Type     = %s\n", pRecTypeStr );
				debug( "    Attribute count = %ld\n", pRecEntry->fRecordAttributeCount );
			}
			
			for ( j = 1; (j <= pRecEntry->fRecordAttributeCount) && (error == eDSNoErr); j++ )
			{
				error = dsGetAttributeEntry( inNodeRef, tDataBuff, attrListRef, j, &valueRef, &pAttrEntry );
				if ( error == eDSNoErr && pAttrEntry != NULL )
				{
					for ( k = 1; (k <= pAttrEntry->fAttributeValueCount) && (error == eDSNoErr); k++ )
					{
						error = dsGetAttributeValue( inNodeRef, tDataBuff, k, valueRef, &pValueEntry );
						if ( error == eDSNoErr && pValueEntry != NULL )
						{
							if ( strstr( pValueEntry->fAttributeValueData.fBufferData, "ApplePasswordServer" ) != NULL )
							{
								*outAuthAuthority = (char *) malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
								strcpy( *outAuthAuthority, pValueEntry->fAttributeValueData.fBufferData );
								break;
							}
							
							// DEBUG
							if ( true )
							{
								debug( "      %ld - %ld: (%s) %s\n", j, k,
												pAttrEntry->fAttributeSignature.fBufferData,
												pValueEntry->fAttributeValueData.fBufferData );
							}
							dsDeallocAttributeValueEntry( dsRef, pValueEntry );
							pValueEntry = NULL;
						}
						else
						{
							//PrintError( kErrGetAttributeEntry, error );
						}
					}
					dsDeallocAttributeEntry( dsRef, pAttrEntry );
					pAttrEntry = NULL;
					dsCloseAttributeValueList(valueRef);
					valueRef = 0;
				}
				else
				{
					//PrintError( kErrGetAttributeEntry, error );
				}
			}

			delete( pRecTypeStr );
			pRecTypeStr = nil;
		}
		else
		{
			//PrintError( kErrGetRecordTypeFromEntry, error );
		}
		
		*outRecordName = pRecNameStr;
		
		dsDeallocRecordEntry( dsRef, pRecEntry );
		pRecEntry = NULL;
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
	}
	
	return( error );

} // GetDataFromDataBuff


//-----------------------------------------------------------------------------
//	GetPasswordDataForID
//
//	Returns: TRUE if successful
//-----------------------------------------------------------------------------

bool GetPasswordDataForID( FILE *fp, const char *authAuthorityStr, PWFileEntry *outUserRecord )
{
	char *version;
	char *tag;
	char *data;
	long status;
	char userID[35];
	bool success = false;
	
	if ( fp == NULL || authAuthorityStr == NULL || outUserRecord == NULL )
		return false;
	
	status = dsParseAuthAuthority( authAuthorityStr, &version, &tag, &data );
	if ( status == eDSNoErr )
	{
		strncpy( userID, data, 34 );
		userID[34] = '\0';
		
		success = GetObfuscatedPasswordForID( fp, userID, outUserRecord );
	}
	
	if ( version != NULL )
		free( version );
	if ( tag != NULL )
		free( tag );
	if ( data != NULL )
		free( data );
	
	return success;
}


//-----------------------------------------------------------------------------
//	GetObfuscatedPasswordForID
//
//	Returns: TRUE if successful
//-----------------------------------------------------------------------------

bool GetObfuscatedPasswordForID( FILE *fp, const char *inUserID, PWFileEntry *outUserRecord )
{
	off_t pos;
	PWFileEntry slotRec;
	PWFileEntry pwRec;
	ssize_t byteCount;
	bool success = false;
	
	if ( fp == NULL || inUserID == NULL || outUserRecord == NULL )
		return false;
	
	if ( pwsf_stringToPasswordRecRef( inUserID, &slotRec ) )
	{
		pos = pwsf_slotToOffset( slotRec.slot );
		byteCount = pread( fileno(fp), &pwRec, sizeof(pwRec), pos );
		if ( byteCount == sizeof(pwRec) &&
				slotRec.time == pwRec.time &&
				slotRec.rnd == pwRec.rnd &&
				slotRec.sequenceNumber == pwRec.sequenceNumber &&
				slotRec.slot == pwRec.slot
			)
		{
			memcpy( outUserRecord, &pwRec, sizeof(pwRec) );
			success = true;
		}
	}
	
	return success;	
}


//-----------------------------------------------------------------------------
//	NewPWServerAdminUser
//
//	Returns: false if function exits and no database exists
//-----------------------------------------------------------------------------

bool NewPWServerAdminUser( const char *inUserName, const char *inPassword )
{
	return NewPWServerAddUser( inUserName, inPassword, true );
}

//-----------------------------------------------------------------------------
//	NewPWServerAddUser
//
//	Returns: false if function exits and no database exists
//-----------------------------------------------------------------------------

bool NewPWServerAddUser( const char *inUserName, const char *inPassword, bool admin )
{
    // mkpassdb -[a|b] -u %s -p -q 
    
    FILE *pf;
    char commandStr[512];
    bool result = true;
	bool hasDatabase;
	
    if ( inUserName == nil || inPassword == nil )
        return false;
    
	hasDatabase = DatabaseExists();
    if ( hasDatabase )
    {
        // database exists, add a new admin
        snprintf(commandStr, sizeof(commandStr), "/usr/sbin/mkpassdb -%c -u %s -p -q", admin ? 'a' : 'b', inUserName);
    }
    else
    {
		if ( ! admin )
			return false;
		
        snprintf(commandStr, sizeof(commandStr), "/usr/sbin/mkpassdb -u %s -p -q", inUserName);
    }
    
    pf = log_popen(commandStr, "w");
    if (pf != nil)
    {
        fprintf(pf, "%s\n", inPassword);
        pclose(pf);
    }
	
	if ( ! hasDatabase )
		result = DatabaseExists();
	
	return result;
}


long NewPWServerAdminUserRemote(
    const char *inServerAddress,
    const char *inAdminID,
    const char *inAdminPassword,
    const char *inNewAdminName,
    const char *inNewAdminPassword,
    char *outID )
{
    tDirReference dsRef = 0;
    tDataBuffer *tDataBuff = 0;
    unsigned long len;
    tDataNode *pAuthType = nil;
    tDataBuffer *pStepBuff = nil;
	long status = eDSNoErr;
    tDirNodeReference nodeRef = 0;
    char userID[1024] = {0};
    DSUtils dsUtils;
	
    do
    {
        if ( inServerAddress == nil || inAdminID == nil || inAdminPassword == nil ||
             inNewAdminName == nil || inNewAdminPassword == nil || outID == nil )
        {
            status = -1;
            break;
        }
        
		status = dsUtils.OpenSpecificPasswordServerNode( inServerAddress );
        if (status != eDSNoErr) break;
        
		dsRef = dsUtils.GetDSRef();
		nodeRef = dsUtils.GetCurrentNodeRef();
		
        tDataBuff = dsDataBufferAllocate( dsRef, 2048 );
		if (tDataBuff == 0) break;
        
        pStepBuff = dsDataBufferAllocate( dsRef, 2048 );
		if (pStepBuff == 0) break;
        
        pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthNewUser );
        if ( pAuthType == nil ) break;
        
		status = dsUtils.FillAuthBuff( tDataBuff, 4,
										strlen(inAdminID), inAdminID,
										strlen(inAdminPassword), inAdminPassword,
										strlen(inNewAdminName), inNewAdminName,
										strlen(inNewAdminPassword), inNewAdminPassword );
		if ( status != eDSNoErr ) break;
        
        // dsDoDirNodeAuth -- get new user
        status = dsDoDirNodeAuth( nodeRef, pAuthType, false, tDataBuff, pStepBuff, nil );
        if ( status != eDSNoErr ) break;
        
        // extract the ID
        memcpy(&len, pStepBuff->fBufferData, 4);
        pStepBuff->fBufferData[len+4] = '\0';
        if ( len > 4 && len < 1024 )
        {
            strncpy( userID, pStepBuff->fBufferData + 4, len );
            userID[len] = '\0';
        }
        
        // clean up
        if ( pAuthType ) {
            dsDataBufferDeAllocate( dsRef, pAuthType );
            pAuthType = nil;
        }
        
        tDataBuff->fBufferLength = 0;
        pStepBuff->fBufferLength = 0;
        
		status = dsUtils.FillAuthBuff( tDataBuff, 4,
										strlen(inAdminID), inAdminID,
										strlen(inAdminPassword), inAdminPassword,
										strlen(userID), userID,
										strlen(kSetAdminUserStr), kSetAdminUserStr );
		if ( status != eDSNoErr ) break;
                
        // set policy to admin user
        pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthSetPolicy );
        if ( pAuthType == nil ) break;
        
		
        // TEMP WORKAROUND
        /*
        if (nodeRef != 0) {
            dsCloseDirNode(nodeRef);
            nodeRef = 0;
        }
        pDataList = dsBuildFromPath( dsRef, pwServerNodeStr, "/" );
        status = dsOpenDirNode( dsRef, pDataList, &nodeRef );
        dsDataListDeallocate( dsRef, pDataList );
        free( pDataList );
        pDataList = nil;
        if (status != eDSNoErr) break;
        */
        
        
        status = dsDoDirNodeAuth( nodeRef, pAuthType, false, tDataBuff, pStepBuff, nil );
        if ( status != eDSNoErr ) break;
	}
    while(false);
    
    strcpy( outID, userID );
    
    if ( tDataBuff )
        dsDataBufferDeAllocate( dsRef, tDataBuff );
    if ( pStepBuff )
        dsDataBufferDeAllocate( dsRef, pStepBuff );
    if ( pAuthType )
        dsDataBufferDeAllocate( dsRef, pAuthType );
        
    return status;
}


//-----------------------------------------------------------------------------
//	GetAuthAuthority
//
//	Returns TRUE if the user is a password server user and returns the user ID
//	in <inOutUserID>
//-----------------------------------------------------------------------------

Boolean	GetAuthAuthority(
    const char *inServerAddress,
    const char *inUsername,
    const char *inPassword,
    char *inOutUserID )
{
    if ( inOutUserID == nil )
    {
        debug("GetAuthAuthority(): inOutUserID must be non-null\n");
        exit(-1);
    }
    
    *inOutUserID = '\0';
    HandleAuthAuthority( inServerAddress, inUsername, inPassword, inOutUserID, true );
    return( *inOutUserID != '\0' );
}


//-----------------------------------------------------------------------------
//	 SetAuthAuthority
//-----------------------------------------------------------------------------

void SetAuthAuthority(
    const char *inServerAddress,
    const char *inUsername,
    const char *inPassword,
	char *inOutUserID,
	const char *dirAdmin,
	const char *dirAdminPass )
{
	HandleAuthAuthority( inServerAddress, inUsername, inPassword, inOutUserID, false, false, dirAdmin, dirAdminPass );
}


//-----------------------------------------------------------------------------
//	 SetAuthAuthorityToBasic
//-----------------------------------------------------------------------------

void SetAuthAuthorityToBasic(
    const char *inUsername,
    const char *inPassword )
{
	char userID[1024];
	
	HandleAuthAuthority( "0.0.0.0", inUsername, inPassword, userID, false, true );
}


//-----------------------------------------------------------------------------
//	 HandleAuthAuthority
//-----------------------------------------------------------------------------

void HandleAuthAuthority(
    const char *inServerAddress,
    const char *inUsername,
    const char *inPassword,
    char *inOutUserID,
  	Boolean inVerifyOnly,
	Boolean inSetToBasic,
	const char *dirAdmin,
	const char *dirAdminPass )
{
    long status = eDSNoErr;
	
	try
    {
        if (inServerAddress == nil)
			throw( -1 );
        
		DSUtilsAuthAuthority dsUtils( inUsername, inServerAddress, inPassword, inOutUserID, inVerifyOnly, inSetToBasic );
		
		dsUtils.DoActionForAllLocalNodes();
		dsUtils.CopyUserID( inOutUserID );
		
		if ( dirAdmin != NULL && dirAdminPass != NULL )
		{
			status = dsUtils.OpenLocalLDAPNode( dirAdmin, dirAdminPass );
			if (status != eDSNoErr)
				throw( status );
			
			status = dsUtils.DoActionOnCurrentNode();
			dsUtils.CopyUserID( inOutUserID );
			if (status != eDSNoErr)
				throw( status );
		}
    }
    catch(long catchErr)
	{
		status = catchErr;
		debugerr( catchErr, "HandleAuthAuthority error = %ld\n", catchErr );
	}
	catch(...)
	{
		debug( "HandleAuthAuthority: an unexpected error occurred.\n" );
	}
}


void GetSASLMechs(char *outSASLStr, Boolean filterMechs)
{
    FILE *pf;
    char curChar;
    char saslStr[1024];
    char saslMechStr[256];
    char *tptr = saslStr;
    char *endPtr = nil;
    long index, index2;
    long len;
    long saslPrefixLen;
    bool needsPlain;
    Boolean skipMech;
    Boolean knownMechListAvail[] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
	
    if ( outSASLStr == nil )
        return;
    *outSASLStr = '\0';
    
	pf = popen("/usr/sbin/mkpassdb -list", "r");
    if ( pf != nil )
    {
        // get the raw string
        do
        {
            fscanf(pf, "%c", &curChar);
            *tptr++ = curChar;
        }
        while ( curChar != ')' &&  curChar != '\n' );
        
        *tptr = '\0';
        
        pclose(pf);
        
        // sanity checks
        saslPrefixLen = strlen(kSASLPrefixStr);
        len = strlen(saslStr);
        if ( len > saslPrefixLen && strncmp(saslStr, kSASLPrefixStr, saslPrefixLen) == 0 && saslStr[len-1] == ')' )
        {
            // strip off front and back
            saslStr[len-2] = '\0';
            tptr = saslStr + saslPrefixLen;
            
            // add info
            do
            {
                endPtr = strchr(tptr+1, '"');
                if (!endPtr)
                    return;
                
                // assume we need a plain text password unless proven otherwise
                needsPlain = true;
                
                // set the enabled flag
                strlcpy(saslMechStr, tptr + 1, endPtr - tptr);
                
                // filter out mechanisms our UI doesn't want
                skipMech = false;
                if ( filterMechs )
                {
                    for ( index = 0; gFilterMechList[index] != NULL; index++ )
                    {
                        if ( strcmp( saslMechStr, gFilterMechList[index] ) == 0 )
                        {
                            skipMech = true;
                            break;
                        }
                    }
                }
                
                if ( !skipMech )
                {
                    // find out if the mechanism is enabled and if it can store a hash on disk
					for ( index = 0; gKnownMechList[index] != NULL; index++ )
                    {
                        if ( !knownMechListAvail[index] && strcmp( saslMechStr, gKnownMechList[index] ) == 0 )
                        {
                            knownMechListAvail[index] = true;
                            break;
                        }
                    }
                    
                    // add to the return string
                    strncat(outSASLStr, tptr, endPtr - tptr + 1);
                    strcat(outSASLStr, " Enabled ");
					if ( pwsf_GetSASLMechInfo( saslMechStr, NULL, &needsPlain ) )
						strcat(outSASLStr, needsPlain ? "Plain\n" : "Hash\n");
					else
						strcat(outSASLStr, "Unknown\n");
                }
                
                tptr = endPtr + 1;
                if ( *tptr ) tptr++;
            }
            while (*tptr);
            
            // add the disabled mechs
            for ( index = 0; gKnownMechList[index] != NULL; index++ )
            {
                // filter out mechanisms our UI doesn't want
                skipMech = false;
                if ( filterMechs )
                {
                    for ( index2 = 0; gFilterMechList[index2] != NULL; index2++ )
                    {
                        if ( strcmp( gKnownMechList[index], gFilterMechList[index2] ) == 0 )
                        {
                            skipMech = true;
                            break;
                        }
                    }
                }
                
                if ( !skipMech && !knownMechListAvail[index] )
                {
                    strcat(outSASLStr, "\"");
                    strcat(outSASLStr, gKnownMechList[index]);
                    strcat(outSASLStr, "\" Disabled ");
					if ( pwsf_GetSASLMechInfo( gKnownMechList[index], NULL, &needsPlain ) )
						strcat(outSASLStr, needsPlain ? "Plain\n" : "Hash\n");
					else
						strcat(outSASLStr, "Unknown\n");
                }
            }
        }
    }
    
    if ( *outSASLStr == '\0' )
        strcat(outSASLStr, "\n");
}


void SetSASLMechs( int argc, char * const *argv )
{
    int argIndex;
	bool changeMade = false;
	bool enable;
	
    for ( argIndex = 2; argIndex < argc - 1; argIndex++ )
    {
		enable = (strcasecmp( argv[argIndex + 1], "on" ) == 0);
		if ( enable || strcasecmp( argv[argIndex + 1], "off" ) == 0 )
			if ( pwsf_SetSASLPluginState( argv[argIndex], enable ) )
				changeMade = true;
    }
	
	// send HUP signal
	if ( changeMade )
		PausePasswordServer();
}


//-----------------------------------------------------------------------------
//	VerifyAdmin
//
//	Returns: DS Error Code
//-----------------------------------------------------------------------------

long VerifyAdmin( const char *inServerAddress,
                    const char *inUserName,
                    const char *inPassword,
                    char *outID )
{
	return VerifyUser( inServerAddress, inUserName, inPassword, false, outID );
}


//-----------------------------------------------------------------------------
//	VerifyUser
//
//	Returns: DS Error Code
//-----------------------------------------------------------------------------

long VerifyUser( const char *inServerAddress,
                    const char *inUserName,
                    const char *inPassword,
					bool inCheckAllUsers,
                    char *outID )
{
    tDirReference dsRef = 0;
    tDataBuffer *tDataBuff = 0;
    unsigned long len;
    tDataNode *pAuthType = nil;
    tDataBuffer *pStep1Buff = nil;
	tDataBuffer *pStepBuff = nil;
	long status = eDSAuthFailed;
    tDirNodeReference nodeRef = 0;
	char userID[1280] = {0};
	char rsaKey[1024] = {0};
    char *startPtr, *endPtr;
    const long bufferSize = 2048;
	DSUtils dsUtils;
	
    do
    {
        if ( inServerAddress == nil || inUserName == nil || inPassword == nil || outID == nil ) {
            status = eParameterError;
            break;
        }
        
        *outID = '\0';
        
		status = dsUtils.OpenSpecificPasswordServerNode( inServerAddress );
		debugerr( status, "VerifyUser, OpenSpecificPasswordServerNode = %ld\n", status );
        if (status != eDSNoErr) break;
		
		dsRef = dsUtils.GetDSRef();
		nodeRef = dsUtils.GetCurrentNodeRef();
        
        tDataBuff = dsDataBufferAllocate( dsRef, bufferSize );
		if (tDataBuff == 0) {
            status = eMemoryError;
            break;
        }
        
        pStep1Buff = dsDataBufferAllocate( dsRef, bufferSize );
		if (pStep1Buff == 0) {
            status = eMemoryError;
            break;
        }
        
        pStepBuff = dsDataBufferAllocate( dsRef, bufferSize );
		if (pStepBuff == 0) {
            status = eMemoryError;
            break;
        }
        
        // first get the admin's user ID
        pAuthType = dsDataNodeAllocateString( dsRef, "dsAuthMethodNative:dsAuthGetIDByName" );
        if ( pAuthType == nil ) {
            status = eMemoryError;
            break;
        }
        
		status = dsUtils.FillAuthBuff( tDataBuff, 3,
								strlen(inUserName), inUserName,
								strlen(inPassword), inPassword,
								strlen(inUserName), inUserName );
        if (status != eDSNoErr) break;
		
		if ( inCheckAllUsers )
		{
			// ALL
			len = strlen("ALL");
			memcpy( tDataBuff->fBufferData + tDataBuff->fBufferLength, &len, 4 );
			tDataBuff->fBufferLength += 4;
			
			memcpy( tDataBuff->fBufferData + tDataBuff->fBufferLength, "ALL", len );
			tDataBuff->fBufferLength += len;
		}
		
        // dsDoDirNodeAuth
        status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStep1Buff, nil );
        debugerr( status, "VerifyUser, dsDoDirNodeAuth - dsAuthGetIDByName = %ld\n", status );
        if ( status != eDSNoErr ) break;
        
        // extract the ID and try to verify the password until we
        // find the right one (annoying)
        memcpy(&len, pStep1Buff->fBufferData, 4);
        if ( len < 4 ) {
            status = eDSAuthResponseBufTooSmall;
            break;
        }
        if ( len > bufferSize - 5 ) {
            status = eDSInvalidBuffFormat;
            break;
        }
        
        pStep1Buff->fBufferData[len+4] = '\0';
        
		// get the RSA Key
		startPtr = strchr( pStep1Buff->fBufferData + 4, ',' );
		if ( startPtr != NULL )
			strlcpy( rsaKey, startPtr, sizeof(rsaKey) );
		
        startPtr = pStep1Buff->fBufferData + 4;
        do
        {
            // get an ID
            endPtr = strchr( startPtr, ';' );
            if ( endPtr != NULL )
            {
                strncpy( userID, startPtr, endPtr - startPtr );
                userID[endPtr - startPtr] = '\0';
                startPtr = endPtr + 1;
				
				// append the RSA key
				strlcat( userID, rsaKey, sizeof(userID) );
            }
            else
            {
                strlcpy( userID, startPtr, sizeof(userID) );
                startPtr = NULL;
				
				// don't append the RSA key (already attached)
            }
			
            debug( "userID = %s\n", userID );
            
            // clean up
            if ( pAuthType ) {
                dsDataBufferDeAllocate( dsRef, pAuthType );
                pAuthType = nil;
            }
            
            tDataBuff->fBufferLength = 0;
            pStepBuff->fBufferLength = 0;
            
			if ( !inCheckAllUsers )
			{
				// getpolicy does not require an authentication, but the buffer format
				// takes one anyway in case the behavior changes.
				// admin name
				
				status = dsUtils.FillAuthBuff( tDataBuff, 3,
									strlen(userID), userID,
									strlen(inPassword), inPassword,
									strlen(userID), userID );
				if (status != eDSNoErr) break;
				
				// get policy
				pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthGetPolicy );
				if ( pAuthType == nil ) {
					status = eMemoryError;
					break;
				}
				
				status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStepBuff, nil );
				debugerr( status, "VerifyUser, dsDoDirNodeAuth - kDSStdAuthGetPolicy = %ld\n", status );
				if ( status != eDSNoErr ) break;
				
				memcpy(&len, pStepBuff->fBufferData, 4);
				pStepBuff->fBufferData[len+4] = '\0';
				if ( len > 4 )
				{
					char *adminStr = strstr(pStepBuff->fBufferData+4, kIsAdminPolicyStr);
					if ( adminStr == nil ) continue;
					
					adminStr += strlen(kIsAdminPolicyStr);
					if ( *adminStr == '0' )
						continue;
				}
				
				// clean up
				if ( pAuthType ) {
					dsDataBufferDeAllocate( dsRef, pAuthType );
					pAuthType = nil;
				}
				
				pStepBuff->fBufferLength = 0;
			}
			
            // verify password
			tDataBuff->fBufferLength = 0;
			status = dsUtils.FillAuthBuff( tDataBuff, 2,
									strlen(userID), userID,
									strlen(inPassword), inPassword );
			if (status != eDSNoErr) break;
			
            pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthNodeNativeNoClearText );
            if ( pAuthType == nil ) {
                status = eMemoryError;
                break;
            }
            
            status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStepBuff, nil );
            debugerr( status, "VerifyUser, dsDoDirNodeAuth - kDSStdAuthNodeNativeNoClearText = %ld\n", status );
			if ( status == eDSNoErr )
            {
                strcpy( outID, userID );
                break;
            }
        }
        while ( startPtr != NULL && endPtr != NULL );
    }
    while( false );
    
    if ( *outID == '\0' )
        status = eDSAuthUnknownUser;
	
    // clean up
    if ( tDataBuff )
        dsDataBufferDeAllocate( dsRef, tDataBuff );
    if ( pStep1Buff )
        dsDataBufferDeAllocate( dsRef, pStep1Buff );
    if ( pStepBuff )
        dsDataBufferDeAllocate( dsRef, pStepBuff );
    if ( pAuthType )
        dsDataBufferDeAllocate( dsRef, pAuthType );
	
	debugerr( status, "VerifyUser = %ld\n", status );
	
    return status;
}


//-----------------------------------------------------------------------------
//	VerifyUserID
//
//	Returns: DS Error Code
//
//  succeeds if the UserID is valid.
//-----------------------------------------------------------------------------

long VerifyUserID( const char *inServerAddress,
                    const char *inUserID,
                    const char *inPassword,
					bool inVerifyPassword,
					bool *outIsAdmin )
{
    tDirReference dsRef = 0;
    tDataBuffer *tDataBuff = 0;
    unsigned long len;
    tDataNode *pAuthType = nil;
    tDataBuffer *pStep1Buff = nil;
	tDataBuffer *pStepBuff = nil;
	long status = eDSAuthFailed;
    tDirNodeReference nodeRef = 0;
    const long bufferSize = 2048;
	DSUtils dsUtils;
	
    do
    {
        if ( inServerAddress == nil || inUserID == nil || inPassword == nil ) {
            status = eParameterError;
            break;
        }
        
		if ( outIsAdmin != NULL )
			*outIsAdmin = false;
		
		status = dsUtils.OpenSpecificPasswordServerNode( inServerAddress );
        if (status != eDSNoErr) break;
		
		dsRef = dsUtils.GetDSRef();
		nodeRef = dsUtils.GetCurrentNodeRef();
        
        tDataBuff = dsDataBufferAllocate( dsRef, bufferSize );
		if (tDataBuff == 0) {
            status = eMemoryError;
            break;
        }
        
        pStep1Buff = dsDataBufferAllocate( dsRef, bufferSize );
		if (pStep1Buff == 0) {
            status = eMemoryError;
            break;
        }
        
        pStepBuff = dsDataBufferAllocate( dsRef, bufferSize );
		if (pStepBuff == 0) {
            status = eMemoryError;
            break;
        }
        
		tDataBuff->fBufferLength = 0;
		pStepBuff->fBufferLength = 0;
		
		// getpolicy does not require an authentication, but the buffer format
		// takes one anyway in case the behavior changes.
		// admin name
		
		status = dsUtils.FillAuthBuff( tDataBuff, 3,
							strlen(inUserID), inUserID,
							strlen(inPassword), inPassword,
							strlen(inUserID), inUserID );
		if (status != eDSNoErr) break;
		
		// get policy
		pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthGetPolicy );
		if ( pAuthType == nil ) {
			status = eMemoryError;
			break;
		}
		
		status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStepBuff, nil );
		if ( status != eDSNoErr ) break;
		
		memcpy(&len, pStepBuff->fBufferData, 4);
		pStepBuff->fBufferData[len+4] = '\0';
		
		if ( outIsAdmin != NULL && len > 4 )
		{
			char *adminStr = strstr(pStepBuff->fBufferData+4, kIsAdminPolicyStr);
			if ( adminStr != nil )
			{
				adminStr += strlen(kIsAdminPolicyStr);
				if ( *adminStr == '1' )
					*outIsAdmin = true;
			}
		}
		
		// clean up
		if ( pAuthType ) {
			dsDataBufferDeAllocate( dsRef, pAuthType );
			pAuthType = nil;
		}
		
		if ( inVerifyPassword )
		{
			pStepBuff->fBufferLength = 0;
			
			// verify password
			pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthNodeNativeNoClearText );
			if ( pAuthType == nil ) {
				status = eMemoryError;
				break;
			}
			
			status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStepBuff, nil );
			if ( status == eDSNoErr )
				break;
		}
    }
    while(false);

    // clean up
    if ( tDataBuff )
        dsDataBufferDeAllocate( dsRef, tDataBuff );
    if ( pStep1Buff )
        dsDataBufferDeAllocate( dsRef, pStep1Buff );
    if ( pStepBuff )
        dsDataBufferDeAllocate( dsRef, pStepBuff );
    if ( pAuthType )
        dsDataBufferDeAllocate( dsRef, pAuthType );
	
	debugerr( status, "VerifyUserID = %ld\n", status );
	
    return status;
}


//-----------------------------------------------------------------------------
//	GetReplicaSetup
//
//	Returns: DS Error Code
//-----------------------------------------------------------------------------

long GetReplicaSetup(
	const char *inServerAddress,
	const char *inUserName,
	const char *inPassword,
	char *outRSAPublicKey,
	char *outRSAPrivateKey,
	long *outPrivateKeyLen,
	char **outReplicaList )
{
	char pwServerNodeStr[256];
    tDirReference dsRef = 0;
    tDataBuffer *tDataBuff = 0;
    unsigned long len;
    tDataNode *pAuthType = nil;
    tDataBuffer *pStep1Buff = nil;
	long status = eDSAuthFailed;
    tDirNodeReference nodeRef = 0;
	tContextData continueData = nil;
    const long bufferSize = 4096;
	long replicaListMax = 50 * 700;
	char *listPtr = nil;
	char *pos;
	size_t listSize = 0;
	DSUtils dsUtils;
	
    try
    {
        if ( inServerAddress == nil || inUserName == nil || outPrivateKeyLen == nil ||
			 inPassword == nil || outRSAPublicKey == nil || outRSAPrivateKey == nil ||
			 outReplicaList == nil )
		{
            throw((long)eParameterError);
        }
        
		*outRSAPublicKey = '\0';
        *outRSAPrivateKey = '\0';
        *outPrivateKeyLen = 0;
		*outReplicaList = NULL;
		
        status = dsUtils.OpenSpecificPasswordServerNode( inServerAddress );
        if (status != eDSNoErr)
            throw(status);
		
		dsRef = dsUtils.GetDSRef();
		nodeRef = dsUtils.GetCurrentNodeRef();
                
        tDataBuff = dsDataBufferAllocate( dsRef, bufferSize );
		if (tDataBuff == NULL)
            throw((long)eMemoryError);
        
        pStep1Buff = dsDataBufferAllocate( dsRef, bufferSize );
		if (pStep1Buff == NULL)
			throw((long)eMemoryError);
        
		// reuse <pwServerNodeStr>
		sprintf( pwServerNodeStr, "%sdsAuthSyncSetupReplica", kDSNativeAuthMethodPrefix );
        pAuthType = dsDataNodeAllocateString( dsRef, pwServerNodeStr );
        if ( pAuthType == NULL )
			throw((long)eMemoryError);
        
		status = dsUtils.FillAuthBuff( tDataBuff, 3,
								strlen(inUserName), inUserName,
								strlen(inPassword), inPassword,
								strlen("GET"), "GET" );
        if (status != eDSNoErr)
			throw(status);
		
        // dsDoDirNodeAuth
        status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStep1Buff, nil );
        if ( status != eDSNoErr )
			throw(status);
        
        memcpy(&len, pStep1Buff->fBufferData, 4);
        if ( len < 4 )
			throw((long)eDSAuthResponseBufTooSmall);
		
        if ( len > bufferSize - 5 )
            throw((long)eDSInvalidBuffFormat);
        
        memcpy( outRSAPrivateKey, pStep1Buff->fBufferData + 4, len );
		*outPrivateKeyLen = len;
		
		pos = pStep1Buff->fBufferData + 4 + len;
		memcpy(&len, pos, 4);
        if ( len < 4 )
            throw((long)eDSAuthResponseBufTooSmall);
		
        if ( len > bufferSize - 5 || len > kPWFileMaxPublicKeyBytes - 1 )
            throw((long)eDSInvalidBuffFormat);
		
        strlcpy( outRSAPublicKey, pos + 4, len + 1 );
		
		// get the replica list, can use the same auth buffers
		
		dsDataBufferDeAllocate( dsRef, pAuthType );
		sprintf( pwServerNodeStr, "%sdsAuthListReplicas", kDSNativeAuthMethodPrefix );
		pAuthType = dsDataNodeAllocateString( dsRef, pwServerNodeStr );
        if ( pAuthType == nil )
            throw((long)eMemoryError);
		
		// dsDoDirNodeAuth
		do
		{
			status = dsDoDirNodeAuth( nodeRef, pAuthType, true, tDataBuff, pStep1Buff, &continueData );
			if ( status != eDSNoErr )
				throw( status );
				
        	if ( pStep1Buff->fBufferLength < 4 )
				throw( (long)eDSInvalidBuffFormat );
			
			memcpy( &len, pStep1Buff->fBufferData, 4 );
			if ( len > bufferSize - 4 )
				throw( (long)eDSInvalidBuffFormat );
			
			// first time through?
			if ( *outReplicaList == NULL )
			{
				listSize = (continueData != NULL) ? replicaListMax : (len + 1);
				*outReplicaList = (char *) malloc( listSize );
				if ( *outReplicaList == NULL )
					throw( (long)eMemoryError );
				
				listPtr = *outReplicaList;
			}
			
			// time to realloc?
			if ( ((listPtr + len) - (*outReplicaList)) > replicaListMax )
			{
				char *tempResult;
				
				replicaListMax *= 3;
				
				// if the new allocation fails, the old pointer is still good and
				// needs to be freed below in the clean up section
				tempResult = (char *) realloc( *outReplicaList, replicaListMax );
				if ( tempResult == NULL )
					throw( (sInt32)eMemoryError );
				
				// restore listPtr to the current position in the buffer
				listPtr = tempResult + (listPtr - (*outReplicaList));
				
				// change to the new allocation
				*outReplicaList = tempResult;
			}
			
			strncpy( listPtr, pStep1Buff->fBufferData + 4, len );
			listPtr += len;
			*listPtr = '\0';
		}
		while ( status == eDSNoErr && continueData != NULL );
    }
	catch( long catchErr )
	{
		status = catchErr;
	}
    catch(...)
	{
		status = -1;
	}
    
    // clean up
	if ( status != eDSNoErr )
	{
		if ( *outReplicaList != NULL )
		{
			free( *outReplicaList );
			*outReplicaList = NULL;
		}
	}
	
    if ( tDataBuff )
        dsDataBufferDeAllocate( dsRef, tDataBuff );
    if ( pStep1Buff )
        dsDataBufferDeAllocate( dsRef, pStep1Buff );
    if ( pAuthType )
        dsDataBufferDeAllocate( dsRef, pAuthType );
        
    return status;
}


#pragma mark -
#pragma mark PASSWORD SERVER STATE
#pragma mark -

//-----------------------------------------------------------------------------
//	 launchdRunning
//-----------------------------------------------------------------------------

bool LaunchdRunning( unsigned long *outPID )
{
	pid_t pid = ProcessRunning( "launchd" );
	if ( outPID != NULL )
		*outPID = pid;
	
	return ( pid > 0 );
}	


//-----------------------------------------------------------------------------
//  ResumePasswordServer
//
//  Returns: TRUE if the password server had to be launched.
//-----------------------------------------------------------------------------

bool ResumePasswordServer( void )
{
    unsigned long pid = 0;
    unsigned long launchdPID = 0;
    int retries = 7;
    FILE *fp = NULL;
    int result = 0;
	bool returnCode = false;
	
	if ( PasswordServerRunning( &pid ) )
    {
		// (re)save the plist but don't relaunch
		autoLaunchPasswordServer( true, false );
		kill(pid, SIGINFO);
    }
    else
    {
		returnCode = true;
		
		autoLaunchPasswordServer( true, true );
		
		if ( LaunchdRunning( &launchdPID ) )
		{        
			// launchd will start the password server, just wait
			while ( retries-- > 0 && ! PasswordServerRunning( &pid ) )
				sleep(1);
        }
		
        if ( ! PasswordServerRunning( &pid ) )
        {
            // watchdog is asleep; fire up the server manually
			
			// we don't want watchdog to perpetually launch a second copy
			autoLaunchPasswordServer( false, true );
			debug( "temporarily setting password server to off in launchd for safe manual launch of password server.\n" );
			
			// launch
            fp = log_popen("/usr/sbin/PasswordService", "r");
            if ( fp != NULL )
				result = pclose( fp );
			debugerr( result, "error starting PasswordService = %d\n", result );
			
			// wait for start
			if ( result == 0 )
				while ( retries-- > 0 && ! PasswordServerRunning( &pid ) )
					sleep(1);
			
			gNeededToManuallyLaunchPasswordService = true;
        }
        
        // give the server a moment to initialize
        PasswordServerListening( kMaxPasswordServerWait );
    }
	
	return returnCode;
}


//-----------------------------------------------------------------------------
//	 PausePasswordServer
//-----------------------------------------------------------------------------

void PausePasswordServer( void )
{
    unsigned long pid = 0;
    
    if ( PasswordServerRunning( &pid ) )
        kill(pid, SIGHUP);
}


//-----------------------------------------------------------------------------
//  StopPasswordServer
//
//  Sets the password server to "off" in watchdog and waits for the server
//  to quit.
//-----------------------------------------------------------------------------

bool StopPasswordServer( void )
{
	//int result = 0;
	int tries = 30;
	unsigned long pid = 0;
	bool returnCode = false;
	
	autoLaunchPasswordServer( false, true );
	//result = SetServiceState( "pwd", kActionOff, kPasswordServerCmdStr );
	//if ( result == 0 )
	{
		while ( PasswordServerRunning( &pid ) && tries-- > 0 )
			sleep(1);
		
		if ( PasswordServerRunning( &pid ) )
		{
			kill( pid, SIGKILL );
			sleep(1);
			returnCode = ! PasswordServerRunning( &pid );
		}
		else
		{
			returnCode = true;
		}
	}
	
	return returnCode;
}


//-----------------------------------------------------------------------------
//	 PasswordServerRunning
//-----------------------------------------------------------------------------

Boolean	PasswordServerRunning( unsigned long *outPID )
{
	pid_t pid = ProcessRunning( "PasswordService" );
	if ( outPID != NULL )
		*outPID = pid;
	
	return ( pid > 0 );
}


//-----------------------------------------------------------------------------
//  PasswordServerListening
//
//  Returns: TRUE if the password server has completed at least one startup
//-----------------------------------------------------------------------------

Boolean	PasswordServerListening( int secondsToWait )
{
	bool listening = false;
	int sock = -1;
	socklen_t structlength;
	int byteCount;
	struct sockaddr_in cin;
	char packetData[64];
	fd_set fdset;
	struct timeval selectTimeout = { 1, 0 };
	
	for ( int ticker = secondsToWait - 1; ticker >= 0; ticker-- )
	{
		// send a ping
		if ( testconn_udp( "127.0.0.1", "3659", &sock ) == 0 )
		{
			bzero( &cin, sizeof(cin) );
			cin.sin_family = AF_INET;
			cin.sin_addr.s_addr = htonl( INADDR_ANY );
			cin.sin_port = htons( 0 );
				
			byteCount = 0;
			FD_ZERO( &fdset );
			FD_SET( sock, &fdset );
			if ( select( FD_SETSIZE, &fdset, NULL, NULL, &selectTimeout ) > 0 )
			{
				structlength = sizeof( cin );
				byteCount = recvfrom( sock, packetData, sizeof(packetData) - 1, MSG_DONTWAIT, (struct sockaddr *)&cin, &structlength );
			}
			close( sock );
			
			// don't care what the response is, just that we got one
			if ( byteCount > 0 )
			{
				listening = true;
				break;
			}
		}
		else
		{
			sleep( 1 );
		}
	}
	
	return listening;
}


#pragma mark -
#pragma mark RC4 UTILITIES
#pragma mark -

//------------------------------------------------------------------------------------------
//	GetServerBaseInfo
//
//	Returns: 0 == ok, -1 == fail
//
//	The name is for obfuscation. This function really does RC4 decryption on command-line
//	arguments.
//------------------------------------------------------------------------------------------

int GetServerBaseInfo(const char *inArg, char *outArg)
{
	unsigned long buffLen;
	unsigned char ibuf[1024];
	unsigned char obuf[1024];
	char out[8];
	unsigned char *p;
	long lval;
	unsigned long n;
	int cnt;
	RC4_KEY keycopy;
	
	if ( inArg == NULL || outArg == NULL )
		return -1;
	
	buffLen = strlen(inArg) + 1;
	if ( buffLen >= 1024 )
		return -1;
	
	snprintf((char *)ibuf, sizeof(ibuf), "%s", inArg);
	memset(out, 0, sizeof(out));
    memset(obuf, 0, sizeof(obuf));
    cnt=0;
    
    n=strlen((char *)ibuf);
    p=ibuf;
    while (p < (ibuf + n)) {
        strncpy(out, (char *)p, 2);
        out[2] = '\0';
        lval=strtol(out, NULL, 16);
        obuf[cnt++] = lval;
        p +=2;
    }
	
    memcpy(ibuf, obuf, buffLen);
    memset(obuf, 0, sizeof(obuf));
	memcpy(&keycopy, &gRC4Key, sizeof(RC4_KEY));
	RC4(&keycopy,cnt,ibuf,obuf);
	
	memcpy( outArg, obuf, cnt );
	outArg[cnt] = '\0';
	
	return 0;
}


//------------------------------------------------------------------------------------------
//	GetCmdLinePass
//
//	Returns: exitcode
//
//  utility function to get passwords from the command args
//------------------------------------------------------------------------------------------

int GetCmdLinePass( const char *inArg, char **outPass, long *outPassLen )
{
	long len;
	char *pwsPass;
	int result;
	
	if ( outPass == NULL )
		return EX_SOFTWARE;
	*outPass = NULL;
	
	len = strlen( inArg );
	if ( len > 511 ) {
		return EX_USAGE;
	}
	
	pwsPass = (char *) malloc( len + 1 );
	if ( pwsPass == NULL )
		return EX_SOFTWARE;
	
	if ( len == 128 || len == 256 )
	{
		result = GetServerBaseInfo( inArg, pwsPass );
		debugspecial( "result=%ld, inarg=%s, outArg=%s\n", result, inArg, pwsPass);
		if ( result != 0 ) {
			debug( kBadEncodingStr );
			return EX_DATAERR;
		}
		if ( outPassLen != NULL )
			*outPassLen = strlen( pwsPass );
	}
	else
	{
		strcpy( pwsPass, inArg );
		if ( outPassLen != NULL )
			*outPassLen = len;
	}
	bzero( (char *)inArg, len );
	
	*outPass = pwsPass;
	
	return EX_OK;
}


