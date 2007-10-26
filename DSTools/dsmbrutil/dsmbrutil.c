#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <membership.h>
#include <membershipPriv.h>
#include <servers/bootstrap.h>
#include <DSlibinfoMIG_types.h>
#include <DSmemberdMIG.h>


/* Return codes:
	0	Success
	1	Unknown error
	2	memberd not reachable
	3	Usage error (and nothing else)
	4	Incorrect id (uid, and gid)
	5	UUID error
	6	Incorrect option specified
	7	The user or group does not exist
	8	The user or group does not have an sid
*/

#define SUCCESS			0
#define UNKNOWN			1
#define NOTREACHABLE	2
#define USAGE			3
#define INCORRECTID		4
#define UUIDERROR		5
#define INCORRECTOPT	6
#define EXISTENCE		7
#define NOSID			8
#define NULLARG			9

static int isVerbose = 0;
static char myname[256];

typedef enum tReqType
{
	kUnknown	= 0,
	kUserID,
	kUserName,
	kUserSID,
	kUserUUID,
	kGroupID,
	kGroupName,
	kGroupSID,
	kGroupUUID
} tReqType;

#pragma mark *** Helper Functions ***

void printfullusage()
{
	fprintf(stderr, "Usage: %s [flags] <command> [options]\n", myname);
	fprintf(stderr, "flags:\n");
	fprintf(stderr, "    -v               verbose mode\n");
	fprintf(stderr, "    -h               this usage output\n");
	fprintf(stderr, "commands:\n");
	fprintf(stderr, "    getuuid          [-ugUGsS] value\n");
	fprintf(stderr, "    getid            [-UGsSX] value\n");
	fprintf(stderr, "    getsid           [-ugUGX] value\n");
	fprintf(stderr, "    checkmembership  [-uUxs] value [-gGXS] value\n");
	fprintf(stderr, "    flushcache\n");
	fprintf(stderr, "    dumpstate\n");
	fprintf(stderr, "    statistics       [-f]\n");
}

int notreachable()
{
	fprintf(stderr, "memberd is not reachable\n");
	return NOTREACHABLE;
}

int unknownerror()
{
	fprintf(stderr, "There was an unknown error.\n");
	return UNKNOWN;
}

int usernamecheck(char *username, uuid_t uuid)
{
	if(isVerbose)
		printf("Attempting to convert username to UUID\n");
	int err = mbr_user_name_to_uuid(username, uuid);
	if(err == EIO)
	{
		return notreachable();
	}
	else if(err == ENOENT)
	{
		fprintf(stderr, "The user %s cannot be found\n", username);
		return EXISTENCE;
	}
	if(isVerbose)
		printf("Conversion successful\n");
	return SUCCESS;
}

int groupnamecheck(char *groupname, uuid_t uuid)
{
	if(isVerbose)
		printf("Attempting to convert the groupname to a UUID\n");
	int err = mbr_group_name_to_uuid(groupname, uuid);
	if(err == EIO)
	{
		return notreachable();
	}
	else if(err == ENOENT)
	{
		fprintf(stderr, "The group %s cannot be found\n", groupname);
		return EXISTENCE;
	}
	return SUCCESS;
}

int getuuidforreqtype( tReqType reqType, uint32_t idValue, char *strValue, uuid_t theUUID )
{
	int	err	= SUCCESS;
	
	if ( reqType == kUserID )
	{
		err = mbr_uid_to_uuid( idValue, theUUID );
	}
	else if ( reqType == kGroupID )
	{
		err = mbr_gid_to_uuid( idValue, theUUID );
	}
	else if ( reqType == kUserName )
	{
		err = usernamecheck( strValue, theUUID );
	}
	else if ( reqType == kUserSID || reqType == kGroupSID )
	{
		nt_sid_t sid;
		err = mbr_string_to_sid( strValue, &sid );
		if ( err == 0 )
		{
			if( isVerbose )
				printf("Attempting to find the sid for the UUID\n");
			
			err = mbr_sid_to_uuid( &sid, theUUID );
		}
	}
	else if ( reqType == kGroupName )
	{
		err = groupnamecheck( strValue, theUUID );
	}
	else if ( reqType == kGroupUUID || reqType == kUserUUID ) // we use group or user
	{
		if ( uuid_parse( strValue, theUUID ) != 0 )
		{
			printf( "The UUID entered is not valid.\n" );
			return UUIDERROR;
		}
	}
	
	if ( err == ENOENT )
		printf( "Entry not found\n" );
	if ( err == EIO )
		notreachable();
	else if ( err != 0 )
		unknownerror();
	
	return err;
}

int getidopt( int argc, char *argv[], char *options, tReqType *outType, uint32_t *outid, char **outvalue )
{
	int		ch;
	char	*lastChar	= NULL;
	
	(*outType)	= kUnknown;
	
	while ( (*outType) == kUnknown && (ch = getopt(argc, argv, options)) != -1 )
    {
		switch (ch)
		{
			case 'u':
				(*outid) = strtol( optarg, &lastChar, 10 );
				if ( lastChar == NULL || lastChar[0] == '\0' )
					(*outType) = kUserID;
				else
					fprintf( stderr, "ID supplied was not a number\n" );
				break;
			
			case 'U':
				(*outType) = kUserName;
				(*outvalue) = optarg;
				break;
				
			case 's':
				(*outType) = kUserSID;
				(*outvalue) = optarg;
				break;

			case 'x':
				(*outType) = kUserUUID;
				(*outvalue) = optarg;
				break;
			
			case 'g':
				(*outid) = strtol( optarg, &lastChar, 10 );
				if ( lastChar == NULL || lastChar[0] == '\0' )
					(*outType) = kGroupID;
				else
					fprintf( stderr, "ID supplied was not a number\n" );
				break;
			
			case 'G':
				(*outType) = kGroupName;
				(*outvalue) = optarg;
				break;
			
			case 'S':
				(*outType) = kGroupSID;
				(*outvalue) = optarg;
				break;
			
			case 'X':
				(*outType) = kGroupUUID;
				(*outvalue) = optarg;
				break;
			
			case '?':
				exit( EINVAL );
				break;
			
			default:
				return EINVAL;
		}
	}
	
	return ((*outType) == kUnknown ? EINVAL : 0);
}

int printuuid(const uuid_t uuid)
{
	char string[37];
	if(isVerbose)
		printf("Attempting to convert the UUID to a string\n");
	int err = mbr_uuid_to_string(uuid, string);
	if(err == ENOENT)
	{
		return unknownerror();
	}
	else if(err == EIO)
	{
		return notreachable();
	}
	printf("%s\n", string);
	return SUCCESS;
}

// stringprint and idprint make the above code neater
// since there is a lot of repeat code.  user should
// be set to 1 if being used to output something related
// to a user, and 0 if related to a group.

// stringprint will print the uuid corresponding to string
int stringprint(char *string, int user)
{
	if(strlen(string) > 255)
	{
		fprintf(stderr, "Names cannot be longer than 255 characters\n");
		return 1;
	}
	uuid_t uuid;
	if(user)
	{
		if(isVerbose)
			printf("Attempting to convert username to UUID\n");
		int err = mbr_user_name_to_uuid(string, uuid);
		if(err == EIO)
		{
			return notreachable();
		}
		else if(err != ENOENT)
		{
			// We were able to find a uuid for this username.  Print it out
			if(isVerbose)
				printf("Conversion successful\n");
			return printuuid(uuid);
		}
		else
		{
			// We were not able to find a uuid, display an error.
			printf("There is no uuid for user %s\n", string);
			return UUIDERROR;
		}
	}
	else
	{
		if(isVerbose)
			printf("Attempting to convert groupname to UUID\n");
		int err = mbr_group_name_to_uuid(string, uuid);
		if(err == EIO)
		{
			return notreachable();
		}
		else if(err != ENOENT)
		{
			// We were able to find a uuid for this groupname.  Print it out
			if(isVerbose)
				printf("Conversion successful\n");
			return printuuid(uuid);
		}
		else
		{
			// We were not able to find a uuid, display an error.
			printf("There is no uuid for group %s\n", string);
			return UUIDERROR;
		}
	}
}

int sidprint( char *strValue, int user )
{
	nt_sid_t	sid;
	int			err	= EXISTENCE;
	
	if(isVerbose)
		printf("Attempting to convert the string to a SID\n");

	if ( mbr_string_to_sid( strValue, &sid) == 0 )
	{
		uuid_t	theUUID;
		
		int err = mbr_sid_to_uuid( &sid, theUUID );
		if ( err == 0 )
			printuuid( theUUID );
		else
			printf( "SID not found\n" );
	}
	else
	{
		printf( "Invalid SID value\n" );
	}

	return err;
}

int idprint(int id, int user)
{
	uuid_t uuid;
	if(user)
	{
		if(isVerbose)
			printf("Attempting to convert the uid to a UUID\n");
		int err = mbr_uid_to_uuid(id, uuid);
		if(err == EIO)
		{
			return notreachable();
		}
		else if(err != ENOENT)
		{
			// We were able to find a uuid for this, output it here.
			if(isVerbose)
				printf("Conversion successful\n");
			return printuuid(uuid);
		}
		else
		{
			// We were not able to find a uuid for this, output an error.
			printf("There is no uuid for uid %i\n", id);
			return UUIDERROR;
		}
	}
	else
	{
		if(isVerbose)
			printf("Attempting to convert the gid to a UUID\n");
		int err = mbr_gid_to_uuid(id, uuid);
		if(err == EIO)
		{
			return notreachable();
		}
		else if(err != ENOENT)
		{
			// We were able to find a uuid for this, output it here.
			if(isVerbose)
				printf("Conversion successful\n");
			return printuuid(uuid);
		}
		else
		{
			// We were not able to find a uuid for this, output an error.
			printf("There is no uuid for gid %i\n", id);
			return UUIDERROR;
		}
	}
}

void PrintAverageTime(uint32_t numMicroSeconds)
{
	if (numMicroSeconds < 500)
		printf(" average time %d uSecs\n", numMicroSeconds);
	else if (numMicroSeconds < 10 * 1000)
	{
		int numMilliseconds = numMicroSeconds / 1000;
		int fraction = (numMicroSeconds % 1000) / 100;
		printf(" average time %d.%d mSecs\n", numMilliseconds, fraction);
	}
	else if (numMicroSeconds < 1000 * 1000)
	{
		int numMilliseconds = numMicroSeconds / 1000;
		printf(" average time %d mSecs\n", numMilliseconds);
	}
	else
	{
		int numSeconds = numMicroSeconds / 1000000;
		int fraction = (numMicroSeconds % 1000000) / 100000;
		printf(" average time %d.%d seconds\n", numSeconds, fraction);
	}
}


#pragma mark *** Memberd abstracted API calls ***

int getUUID( int argc, char *argv[] )
{
	tReqType	reqType;
	uint32_t	idValue		= 0;
	char		*strValue	= NULL;
	
	optind = 0;

	if ( getidopt( argc, argv, "u:U:g:G:s:S:", &reqType, &idValue, &strValue ) == EINVAL || optind != argc )
	{
		fprintf( stderr, "Usage: %s getuuid must have one flag and parameter\n", myname );
		return EINVAL;
	}

	switch (reqType)
	{
		case kUserID:
			return idprint( idValue, 1 );
		case kUserName:
			return stringprint( strValue, 1 );
		case kUserSID:
			return sidprint( strValue, 1 );
		case kGroupID:
			return idprint( idValue, 0 );
		case kGroupName:
			return stringprint( strValue, 0 );
		case kGroupSID:
			return sidprint( strValue, 0 );
		default:
			fprintf( stderr, "Unknown lookup type %d\n", reqType );
			return EINVAL;
	}

	return SUCCESS;
}

int getID( int argc, char *argv[] )
{
	tReqType	reqType;
	uint32_t	idValue		= 0;
	char		*strValue	= NULL;
	uuid_t		theUUID;
	
	optind = 0;
	
	if ( getidopt( argc, argv, "U:G:s:S:x:X:", &reqType, &idValue, &strValue ) == EINVAL || optind != argc )
	{
		fprintf( stderr, "Usage: %s getid must have one flag and parameter\n", myname );
		return EINVAL;
	}
	
	uuid_clear( theUUID );

	if ( getuuidforreqtype( reqType, idValue, strValue, theUUID) == 0 && uuid_is_null(theUUID) == 0 )
	{
		id_t id;
		int type;
		if(isVerbose)
			printf("Attempting to find the id for the UUID\n");
		
		int err = mbr_uuid_to_id( theUUID, &id, &type );
		if ( err == 0 )
		{
			if ( isVerbose )
				printf( "id was found\n" );
			
			if ( type == ID_TYPE_UID )
				printf( "uid: %i\n", id );
			else if ( type == ID_TYPE_GID )
				printf( "gid: %i\n", id );
		}
		else if( err == EIO )
			return notreachable();
		else
			return unknownerror();
	}
	else
	{
		printf( "id not found\n" );
	}
	
	return SUCCESS;
}

int getSID(int argc, char *argv[])
{
	tReqType	reqType;
	uint32_t	idValue		= 0;
	char		*strValue	= NULL;
	uuid_t		theUUID;
	int			err			= SUCCESS;
	
	optind = 0;
	
	if ( getidopt( argc, argv, "u:g:U:G:x:X:", &reqType, &idValue, &strValue ) == EINVAL || optind != argc )
	{
		fprintf( stderr, "Usage: %s getsid must have one flag and parameter\n", myname );
		return EINVAL;
	}
	
	uuid_clear( theUUID );
	
	if ( getuuidforreqtype( reqType, idValue, strValue, theUUID) == 0 && uuid_is_null(theUUID) == 0 )
	{
		nt_sid_t sid;
		
		if(isVerbose)
			printf("Attempting to find the SID for the UUID\n");
		
		err = mbr_uuid_to_sid( theUUID, &sid );
		if ( err == 0 )
		{
			char string[MBR_MAX_SID_STRING_SIZE];
			
			if(isVerbose)
				printf("Attempting to convert the SID to a string\n");
			err = mbr_sid_to_string( &sid, string );
			if ( err == 0 )
				printf( "%s\n", string );
			else if(err == EIO)
				return notreachable();
			else
				return unknownerror();
		}
		
		if ( err == EIO )
			return notreachable();
		else if ( err == ENOENT )
			printf( "no SID found\n" );
		else if ( err != 0 )
			return unknownerror();
	}	
	else
	{
		printf( "no SID found\n" );
	}
	
	return err;
}

int checkmembership(int argc, char *argv[])
{
	tReqType	reqType;
	uint32_t	idValue		= 0;
	char		*strValue	= NULL;
	uuid_t		userUUID;
	uuid_t		groupUUID;
	int			err			= SUCCESS;
	
	optind = 0;
	
	uuid_clear( userUUID );
	uuid_clear( groupUUID );
	
	err = getidopt( argc, argv, "u:U:x:s:", &reqType, &idValue, &strValue );
	if ( err == 0 )
	{
		err = getuuidforreqtype( reqType, idValue, strValue, userUUID );
		if ( err == 0 )
		{
			err = getidopt( argc, argv, "g:G:X:S:", &reqType, &idValue, &strValue );
			if ( err == 0 )
			{
				err = getuuidforreqtype( reqType, idValue, strValue, groupUUID );
			}
		}
	}
	
	if ( err == EINVAL )
	{
		fprintf( stderr, "Usage: %s checkmembership missing appropriate options\n", myname );
		fprintf( stderr, "    checkmembership  [-uUx] value [-GX] value\n" );
		return EINVAL;
	}
	else if ( uuid_is_null(userUUID) )
	{
		printf( "User not found\n" );
		err = EXISTENCE;
	}
	else if ( uuid_is_null(groupUUID) )
	{
		printf( "Group not found\n" );
		err = EXISTENCE;
	}
	else
	{
		// we made it here we are good
		int isMember;
		
		err = mbr_check_membership( userUUID, groupUUID, &isMember );
		if ( err == 0 )
		{
			if ( isMember == 1 )
				printf( "user is a member of the group\n" );
			else
				printf( "user is not a member of the group\n");
		}
		else if ( err == EIO )
			return notreachable();
	}
	
	return err;
}

int flushcache( void )
{
	mbr_reset_cache();
	return 0;
}

int dumpstate( void )
{
	mach_port_t	serverPort;
	
	if ( bootstrap_look_up( bootstrap_port, kDSStdMachDSLookupPortName, &serverPort ) != KERN_SUCCESS)
	{
		printf( "Got an error looking up DirectoryService running?\n");
		exit(EIO);
	}
	
	memberdDSmig_DumpState( serverPort );
	
	return 0;
}

int statistics( int argc, char *argv[] )
{
	StatBlock stats;
	int temp, minutes, hours;
	mach_port_t	serverPort;
	int flushStats = 0;
	char ch;
	
	optind = 0;
	
	while ( (ch = getopt(argc, argv, "f")) != -1 )
	{
		switch (ch)
		{
			case 'f':
				flushStats = 1;
				break;
		}
	}

	if ( bootstrap_look_up( bootstrap_port, kDSStdMachDSLookupPortName, &serverPort ) != KERN_SUCCESS)
	{
		printf( "Got an error looking up DirectoryService running?\n");
		exit(EIO);
	}
	
	if ( flushStats == 1 )
	{
		memberdDSmig_ClearStats( serverPort );
	}
	else
	{
		memberdDSmig_GetStats( serverPort, &stats );
		
		temp = stats.fTotalUpTime / 60;
		minutes = temp % 60;
		temp /= 60;
		hours = temp % 24;
		temp /= 24;
		printf("DirectoryService running for %d days, %d hours and %d minutes\n", temp, hours, minutes);
		printf("%d requests,", stats.fTotalCallsHandled);
		PrintAverageTime(stats.fAverageuSecPerCall);
		printf("%d cache hits, %d cache misses\n", stats.fCacheHits, stats.fCacheMisses);
		printf("%d Directory record lookups (%d failed),", 
			   stats.fTotalRecordLookups, stats.fNumFailedRecordLookups);
		PrintAverageTime(stats.fAverageuSecPerRecordLookup);
		printf("%d membership searches,", stats.fTotalMembershipSearches);
		PrintAverageTime(stats.fAverageuSecPerMembershipSearch);
		printf("%d searches for legacy groups,", stats.fTotalLegacySearches);
		PrintAverageTime(stats.fAverageuSecPerLegacySearch);
		printf("%d searches for groups containing user,", stats.fTotalGUIDMemberSearches);
		PrintAverageTime(stats.fAverageuSecPerGUIDMemberSearch);
		printf("%d nested group membership searches,", stats.fTotalNestedMemberSearches);
		PrintAverageTime(stats.fAverageuSecPerNestedMemberSearch);
	}
	
	return 0;
}

#pragma mark *** Main ***

int main(int argc, char *argv[])
{
	int ch;
	
	strcpy( myname, argv[0] );
	if(argc < 2)
	{
		printfullusage();
		return SUCCESS;
	}
	
	while ( (ch = getopt(argc, argv, "vh")) != -1 )
	{
		switch (ch)
		{
			case 'v':
				isVerbose = 1;
				break;
				
			case 'h':
				printfullusage();
				return 0;
				
			case '?':
				printfullusage();
				return EINVAL;
				
			default:
				break;
		}
	}
	
	argc -= optind;
	argv += optind;
	
	char *command = argv[0];
	
	// now increment past command
	argc--;
	argv++;
	
	if ( strcasecmp(command, "getuuid") == 0 )
		return getUUID(argc, argv);
	else if ( strcasecmp(command, "getid") == 0 )
		return getID( argc, argv );
	else if (strcasecmp(command, "getsid") == 0 )
		return getSID( argc, argv );
	else if ( strcasecmp(command, "checkmembership") == 0 )
		return checkmembership(argc, argv);
	else if ( strcasecmp(command, "flushcache") == 0 )
		return flushcache();
	else if ( strcasecmp(command, "dumpstate") == 0 )
		return dumpstate();
	else if ( strcasecmp(command, "statistics") == 0 )
		return statistics( argc, argv );
	
	printfullusage();
	
	return USAGE;
}
