/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>

#include "FTPAccessFile.h"

//#warning work around for radar 3006576, remove when possible
//#include "strcasestr.h"
#include "ftpmunge.h"


using namespace std;


#pragma mark *** File Constants

static const size_t kMaxInt32Digits = 11;	// max digits for a 32 bit
						// decimal number +4294967295
static const size_t kMaxLogKindLen = 10;	// arbitrary for printing logs
static const int32_t kDefaultLoginFails = 5;	// taken from ftp source

static const char *kDefaultVirtualRoot = "/Library/FTPServer/FTPRoot";
static const char *kDefaultShutdownFilename	= "/Library/FTPServer/Messages/shutdown.txt";


#pragma mark *** File Macros

#define NEWMEMBER( name, word )		  	     	\
aclmember *name = new aclmember;			\
memset( name, 0, sizeof( aclmember ) );			\
strncpy( name->keyword, word, MAXKWLEN );		\
name->keyword[MAXKWLEN-1] = 0


#pragma mark *** FTPAccess Member functions

FTPAccessFile::FTPAccessFile( const char *filename )
	throw( std::bad_alloc, std::runtime_error )
	: ConfigurationFile( filename), 
	  aclmembers( NULL )
{
	parseacl( readacl( GetFileForReading() ) );
}

FTPAccessFile::~FTPAccessFile() throw()
{
	FlushFile();

	// walk the acl list, deallocating each one
	for( aclmember *member = aclmembers; member != NULL; ) {
		aclmember *next = member->next;
		RemoveAndDeallocMember( member );
		member = next;
	}
}

void FTPAccessFile::FlushFile()
{
	FILE *file = StartWrite();

	for( aclmember *member = aclmembers; member; member = member->next )
        	fprintf( file, "%s\n", member->rawText );
       
	StopWrite();
}

uint32_t FTPAccessFile::GetAnonymousStatus() throw()
{
	char *str = GetSingleton( "anonFTP" );

	if( str != NULL && 0 == strncasecmp( str, "YES", strlen( "YES" ) ) )
		return 1;

	return 0;
}

void FTPAccessFile::SetAnonymousStatus( uint32_t status )
	throw( std::bad_alloc )
{
	SetSingleton( "anonFTP", status == 0 ? "no" : "yes" );  
}

uint32_t FTPAccessFile::GetMacBinAndDmgAutoConversion() throw()
{
	char *str = GetSingleton( "influence_listings" );

	if( str != NULL && 0 == strncasecmp( str, "NO", strlen( "NO" ) ) )
		return 0;

	return 1; //default is yes
}

void FTPAccessFile::SetMacBinAndDmgAutoConversion( uint32_t status )
	throw( std::bad_alloc )
{
	SetSingleton( "influence_listings", status == 0 ? "no" : "yes" );  
}

int32_t FTPAccessFile::GetAnonymousLimit() throw()
{
	return GetLimitForClass( "anonusers" );
}

int32_t FTPAccessFile::GetRealLimit() throw()
{
	return GetLimitForClass( "realusers" );
}

char *FTPAccessFile::GetShutdownFilename() throw()
{
	const char *filename = GetSingleton( "shutdown" );
	if( !filename )
		SetShutdownFilename( kDefaultShutdownFilename );

	return ftp2rawstr( GetSingleton( "shutdown" ) );
}

void FTPAccessFile::SetShutdownFilename( const char *filename )
	throw( std::bad_alloc )
{
	char *ftpFilename = raw2ftpstr( filename );
	SetSingleton( "shutdown", ftpFilename );
	delete[] ftpFilename;
}

char *FTPAccessFile::GetBannerFilename() throw()
{
	return ftp2rawstr( GetSingleton( "banner" ) );
}

void FTPAccessFile::SetBannerFilename( const char *filename )
	throw( std::bad_alloc )
{
	char *ftpFilename = raw2ftpstr( filename );
	SetSingleton( "banner", ftpFilename );
	delete [] ftpFilename;
}

// NOTE: it would be better if we had a more general way to handle message
// directives
char *FTPAccessFile::GetLoginMessageFilename() throw()
{
	for(	aclmember *member = GetNextKeywordEntry( "message", NULL );
		member != NULL;
		member = GetNextKeywordEntry( "message", member ) ) {

		// message /path/to/msg login
		// [ make sure there is no class directive ]
		if(	0 == strncasecmp( "login", member->arg[1], strlen( "login" ) )
			&& !member->arg[2] )
			return ftp2rawstr( member->arg[0] );
	}

	return NULL;
}

void FTPAccessFile::SetLoginMessageFilename( const char *filename )
	throw( std::bad_alloc )
{
	for( aclmember *member = aclmembers; member; ) {
		aclmember *next = member->next;

		if(	0 == strncasecmp( "message", member->keyword, MAXKWLEN )
			&& 0 == strncasecmp( "login", member->arg[1], strlen( "login" ) ) ) {
			RemoveAndDeallocMember( member );
		}

		member = next;
	}

	if( filename == NULL )
		return;

	NEWMEMBER( member, "message" );
	member->arg[0] = raw2ftpstr( filename );
	member->arg[1] = NewStr( "login" );
	RedoRawText( member );
	AddMember( member );
}


int32_t FTPAccessFile::GetLoginFails() throw()
{
	aclmember *member = GetNextKeywordEntry( "loginfails", NULL );
	if( member == NULL )
		return kDefaultLoginFails;

	return atoi( member->arg[0] );
}

const char *FTPAccessFile::GetEmail() throw()
{
	return ftp2rawstr( GetSingleton( "email" ) );
}

void FTPAccessFile::SetEmail( const char *email ) throw( std::bad_alloc )
{
	const char *ftpEmail = raw2ftpstr( email );
	SetSingleton( "email", ftpEmail );
	delete[] ftpEmail;
}

const char *FTPAccessFile::GetKerberosPrincipal() throw()
{
	return ftp2rawstr( GetSingleton( "krb5_principal" ) );
}

void FTPAccessFile::SetKerberosPrincipal( const char *principal ) throw( std::bad_alloc )
{
	const char *ftpPrincipal = raw2ftpstr( principal );
	SetSingleton( "krb5_principal", ftpPrincipal );
	delete[] ftpPrincipal;
}

const char *FTPAccessFile::GetAnonRoot() throw()
{
        const char *root = GetSingleton( "anonymous-root" );
        if( !root )
                SetAnonRoot( kDefaultVirtualRoot );

	return ftp2rawstr( GetSingleton( "anonymous-root" ) );
}

void FTPAccessFile::SetAnonRoot( const char *anonRootPath ) throw( std::bad_alloc )
{
	const char *ftpAnonRoot = raw2ftpstr( anonRootPath );
	SetSingleton( "anonymous-root", ftpAnonRoot );
	delete[] ftpAnonRoot;
}

const char *FTPAccessFile::GetVirtualRoot() throw()
{
	const char *root = GetSingleton( "defrootdir" );
	if( !root )
		SetVirtualRoot( kDefaultVirtualRoot );

	return ftp2rawstr( GetSingleton( "defrootdir" ) );
}

void FTPAccessFile::SetVirtualRoot( const char *root ) throw( std::bad_alloc )
{
	char *ftpRoot = raw2ftpstr( root ? root : kDefaultVirtualRoot );
	SetSingleton( "defrootdir", ftpRoot );

	SetUploadDirectives( ftpRoot );

	delete[] ftpRoot;
}

void FTPAccessFile::SetUploadDirectives( const char *path ) throw(std::bad_alloc)
{
	DeleteKeywordEntries( "upload" );
	if( !path )
		return;

	// upload  /Library/FTPServer/FTPRoot /uploads yes ftp daemon 0666 nodirs
	//upload  /Library/FTPServer/FTPRoot /uploads/mkdirs yes ftp daemon 0666 dirs 0777
	// Create the required upload directives
	NEWMEMBER( first, "upload" );
	first->arg[0] = NewStr( path );
	first->arg[1] = raw2ftpstr( "/uploads" );
	first->arg[2] = NewStr( "yes" );
	first->arg[3] = NewStr( "ftp" );
	first->arg[4] = NewStr( "daemon" );
	first->arg[5] = NewStr( "0666" );
	first->arg[6] = NewStr( "nodirs" );
	RedoRawText( first );
	AddMember( first );

	NEWMEMBER( second, "upload" );
	second->arg[0] = NewStr( path );
	second->arg[1] = raw2ftpstr( "/uploads/mkdirs" );
	second->arg[2] = NewStr( "yes" );
	second->arg[3] = NewStr( "ftp" );
	second->arg[4] = NewStr( "daemon" );
	second->arg[5] = NewStr( "0666" );
	second->arg[6] = NewStr( "dirs" );
	second->arg[7] = NewStr( "0777" );
	RedoRawText( second );
	AddMember( second );
}

uint32_t FTPAccessFile::GetAuthLevel() throw( std::bad_alloc )
{
	char *str = GetSingleton( "auth_level" );

	if( str == NULL || 0 == strcasecmp( str, "standard" ) )
		return kAuthLevelStandard;
	else if( 0 == strcasecmp( str, "gssapi" ) )
		return kAuthLevelGSSAPI;
	else if( 0 == strcasecmp( str, "both" ) )
		return kAuthLevelBoth;
	else {
		// unknown auth_level, defaulting to standard, repair ftpaccess
		SetSingleton( "auth_level", "standard" );
	}

	return kAuthLevelStandard;
}

void FTPAccessFile::SetAuthLevel( uint32_t level ) throw( std::bad_alloc )
{
	const char *str = "standard";

	if( level == kAuthLevelGSSAPI )
		str = "gssapi";
	else if( level == kAuthLevelBoth )
		str = "both";
	//else if( level != kAuthLevelStandard )
	// unknonw auth_level, defaulting to standard.

	SetSingleton( "auth_level", str );
}

uint32_t FTPAccessFile::GetChrootType() throw( std::bad_alloc )
{
	const char *str = GetSingleton( "chroot_type" );

	if( str == NULL || 0 == strcasecmp( str, "standard" ) )
		return kChrootTypeStandard;
	else if( 0 == strcasecmp( str, "homedir" ) )
		return kChrootTypeHomedir;
	else if( 0 == strcasecmp( str, "restricted" ) )
		return kChrootTypeRestricted;
	else {
		// unkown chroot_type, defauting to standard, repairing file
		SetSingleton( "chroot_type", "standard" );
	}

	return kChrootTypeStandard;
}

void FTPAccessFile::SetChrootType( uint32_t type ) throw( std::bad_alloc )
{
	const char *str = "standard";

	if( type == kChrootTypeHomedir )
		str = "homedir";
	else if( type == kChrootTypeRestricted )
		str = "restricted";
	//	else if( type != kChrootTypeStandard )
	//	unknown chroot_type, defaulting to standard

	SetSingleton( "chroot_type", str );
}

#pragma mark ** Singleton functions

char *FTPAccessFile::GetSingleton( const char *kind ) throw()
{
	assert( kind != NULL );
	aclmember *member = GetNextKeywordEntry( kind, NULL );
	if( member == NULL )
		return NULL;

	return member->arg[0];
}

void FTPAccessFile::SetSingleton( const char *kind, const char *arg0 )
	throw( std::bad_alloc )
{
	assert( kind != NULL );
	DeleteKeywordEntries( kind );

	if( arg0 == NULL )
		return;

	NEWMEMBER( member, kind );
	member->arg[0] = NewStr( arg0 );
	RedoRawText( member );
	AddMember( member );
}

#pragma mark ** Limit helper functions

int32_t FTPAccessFile::GetLimitForClass( const char *limitClass ) throw()
{
	for(	aclmember *member = GetNextKeywordEntry( "limit", NULL );
		member;
		member = GetNextKeywordEntry( "limit", member ) ) {
		if( 0 == strncasecmp( limitClass, member->arg[0], strlen(limitClass) ) )
			return atoi( member->arg[1] );
	}

	return -1;
}

#pragma mark ** Functions for dealing with log directives

LogCommands FTPAccessFile::GetLogCommands() throw()
{
	LogCommands commands = { 0, 0, 0 };

	for( aclmember *member = GetNextKeywordEntry( "log", NULL );
	     member; member = GetNextKeywordEntry( "log", member ) ) {
    
		if( 0 == strncasecmp("commands",member->arg[0],kMaxLogKindLen))
			ExtractTypeList( member->arg[1], commands );
	}

	return commands;
}

void FTPAccessFile::SetLogCommands( LogCommands commands )
	throw( std::bad_alloc )
{
	DeleteLogKind( "commands" );

	if( !commands.real && !commands.guest && !commands.anonymous )
		return;

	NEWMEMBER( member, "log" );

	member->arg[0] = NewStr( "commands" );
	member->arg[1] = TypeListToStr( commands );
	RedoRawText( member );
	AddMember( member );
}

LogSecurity FTPAccessFile::GetLogSecurity() throw()
{
	LogSecurity security = { 0, 0, 0 };

	for( aclmember *member = GetNextKeywordEntry( "log", NULL );
	     member; member = GetNextKeywordEntry( "log", member ) ) {

		if( 0 == strncasecmp( "security", member->arg[0], MAXKWLEN ) )
			ExtractTypeList( member->arg[1], security );
	}

	return security;
}

void FTPAccessFile::SetLogSecurity( LogSecurity security )
	throw( std::bad_alloc )
{
	DeleteLogKind( "security" );

	if( !security.real && !security.guest && !security.anonymous )
		return;
    
	NEWMEMBER( member, "log" );

	member->arg[0] = NewStr( "security" );
	member->arg[1] = TypeListToStr( security );
	RedoRawText( member );
	AddMember( member );
}

uint32_t FTPAccessFile::GetLogSyslog() throw()
{
	for( aclmember *member = GetNextKeywordEntry( "log", NULL );
	     member; member = GetNextKeywordEntry( "log", member ) ) {

		if( 0 == strncasecmp( "syslog", member->arg[0], MAXKWLEN ) )
			return 1;
	}

	return 0;
}

void FTPAccessFile::SetLogSyslog( uint32_t syslog ) throw( std::bad_alloc )
{
	DeleteLogKind( "syslog" );

	if( !syslog )
		return;
    
	NEWMEMBER( member, "log" );
	member->arg[0] = NewStr( "syslog" );
	RedoRawText( member );
	AddMember( member );
}

LogTransfers FTPAccessFile::GetLogTransfers() throw()
{
	TypeList in = { 0, 0, 0 };
	TypeList out = { 0, 0, 0 };

	for( aclmember *member = GetNextKeywordEntry( "log", NULL );
	     member; member = GetNextKeywordEntry( "log", member ) ) {
     
		if( 0 == strncasecmp( "transfers", member->arg[0],
				      strlen( "transfers" ) )
		    && NULL != strcasestr( member->arg[2], "inbound" ) )
			ExtractTypeList( member->arg[1], in );
        
		if( 0 == strncasecmp( "transfers", member->arg[0],
				      strlen( "transfers" ) )
		    && NULL != strcasestr( member->arg[2], "outbound" ) )
			ExtractTypeList( member->arg[1], out );
	}

	LogTransfers transfers;
	transfers.real.inbound		= in.real;
	transfers.real.outbound		= out.real;
	transfers.guest.inbound		= in.guest;
	transfers.guest.outbound	= out.guest;
	transfers.anonymous.inbound	= in.anonymous;
	transfers.anonymous.outbound	= out.anonymous; 

	return transfers;
}

void FTPAccessFile::SetLogTransfers( LogTransfers transfers )
	throw( std::bad_alloc )
{
	DeleteLogKind( "transfers" );

	LogTransfers c = transfers;

	if( transfers.real.inbound || transfers.anonymous.inbound
	    || transfers.guest.inbound ) {
		NEWMEMBER( member, "log" );
		member->arg[0] = NewStr( "transfers" );

		TypeList inbound;
		inbound.real		= transfers.real.inbound;
		inbound.anonymous	= transfers.anonymous.inbound;
		inbound.guest		= transfers.guest.inbound;

		member->arg[1] = TypeListToStr( inbound );
		member->arg[2] = NewStr( "inbound" );
		RedoRawText( member );
		AddMember( member );
	}

	if( transfers.real.outbound || transfers.anonymous.outbound
	    || transfers.guest.outbound ) {
		NEWMEMBER( member, "log" );
		member->arg[0] = NewStr( "transfers" );

		TypeList outbound;
		outbound.real		= transfers.real.outbound;
		outbound.anonymous	= transfers.anonymous.outbound;
		outbound.guest		= transfers.guest.outbound;

		member->arg[1] = TypeListToStr( outbound );
		member->arg[2] = NewStr( "outbound" );
		RedoRawText( member );
		AddMember( member );
	}
}

void FTPAccessFile::DeleteLogKind( const char *kind ) throw()
{
	assert( kind );

	for( aclmember *member = aclmembers; member; ) {
		aclmember *next = member->next;

		if( 0 == strncasecmp( "log", member->keyword, MAXKWLEN )
		    && 0 == strncasecmp(kind, member->arg[0],kMaxLogKindLen)) {
			RemoveAndDeallocMember( member );
		}

		member = next;
	}
}

#pragma mark ** General utility functions

char *FTPAccessFile::TypeListToStr( TypeList typelist ) throw( std::bad_alloc )
{
	// NOTE: this code is ugly, but gets the job done. If we ever need to
	// support more "types" in the future the code should be changed to
	// incrementally add the string representation of each type.
	if( typelist.real && typelist.guest && typelist.anonymous )
		return NewStr( "real,guest,anonymous" );
	else if( typelist.real && typelist.guest )
		return NewStr( "real,guest" );
	else if( typelist.real && typelist.anonymous )
		return NewStr( "real,anonymous" );
	else if( typelist.guest && typelist.anonymous )
		return NewStr( "guest,anonymous" );
	else if( typelist.real )
		return NewStr( "real" );
	else if( typelist.guest )
		return NewStr( "guest" );
	else if( typelist.anonymous )
		return NewStr( "anonymous" );

	assert( "not reached" == NULL );
	return NULL;
}

void FTPAccessFile::ExtractTypeList( const char *string, TypeList &typelist )
	throw()
{
	assert( string );

	if( strcasestr( string, "real" ) != NULL )
		typelist.real = true;
	if( strcasestr( string, "anonymous" ) != NULL )
		typelist.anonymous = true;
	if( strcasestr( string, "guest" ) != NULL )
		typelist.guest = true;
}

char *FTPAccessFile::NewStr( const char *str)
	throw( bad_alloc )
{
	if( str == NULL )
		return NULL;

	char *tmp = new char[ strlen( str ) + 1 ];
	strcpy( tmp, str );

	return tmp;
}

void FTPAccessFile::RedoRawText( aclmember *member ) throw( std::bad_alloc )
{
	assert( member );

	size_t len = strlen( member->keyword );

	for( int i = 0; i < MAXKWLEN && member->arg[i]; i++ ) {
		len += strlen( "\t" ) + strlen( member->arg[i] ) ;
	}

	len += 1;

	delete [] member->rawText;

	member->rawText = new char[ len ];
	strcpy( member->rawText, member->keyword );

	for( int i = 0; i < MAXKWLEN && member->arg[i]; i++ ) {
		strcat( member->rawText, "\t" );
		strcat( member->rawText, member->arg[i] );
	}
}

#pragma mark ** ACL member manipulation functions

aclmember *FTPAccessFile::GetNextKeywordEntry( const char *keyword,
					       aclmember *previous ) throw()
{
	assert( keyword );

	if( previous == NULL )
		previous = aclmembers;
	else
		previous = previous->next;

	for( ; previous && 0!= strncasecmp(keyword,previous->keyword,MAXKWLEN);
		previous = previous->next );

	return previous;
}

void FTPAccessFile::Description()
{
	aclmember* member = aclmembers;

	while (member != NULL)
	{
		int n = 0;
		
		printf("rawText=%s\n	keyword=%s\n", member->rawText, member->keyword);
		while ( member->arg[n] )
		{
			printf("	arg[%d]=%s\n", n, member->arg[n] );
			n++;
		}
		member = member->next;
	}
}


void FTPAccessFile::DeleteKeywordEntries( const char *keyword ) throw()
{
	for( aclmember *member = aclmembers; member != NULL; ) {
		aclmember *next = member->next;

		if( 0 == strncasecmp( keyword, member->keyword, MAXKWLEN ) )
			RemoveAndDeallocMember( member );

		member = next;
	}
}

void FTPAccessFile::RemoveAndDeallocMember( aclmember *member ) throw()
{
	assert( member );

	if( member->next )
		member->next->prev = member->prev;

	if( member->prev )
		member->prev->next = member->next;
	else 
		aclmembers = NULL;	// this was the only item

	for( uint8_t i = 0; i < MAXKWLEN; i++ ) {
		delete[] member->arg[i];
		member->arg[i] = NULL;
	}

	delete[] member->rawText;
	delete member;
}

void FTPAccessFile::AddMember( aclmember *member ) throw()
{
	assert( member != NULL );
	member->next = member->prev = NULL;

	// this is the first member added
	if( !aclmembers ) {
		aclmembers = member;
		return;
	}

	// NOTE: might want to add tail pointer if this becomes too slow
	// find the end and add the member there
	aclmember *end = aclmembers; 
	for( ; end->next; end = end->next )
		;

	assert( end->next == NULL );

	end->next = member;
	member->prev = end;
}

#pragma mark ** wu-ftpd derived configuration parsing functions

char *FTPAccessFile::readacl( std::FILE *aclfile ) 
	throw( std::bad_alloc, std::runtime_error )
{
	assert( aclfile != NULL );
	// get the size of the file
	if( fseek( aclfile, 0, SEEK_END ) == -1 ) {
		rewind( aclfile );
		throw std::runtime_error( "cannot seek to the end of ftpaccess file" );
	}

	long size = ftell( aclfile );
	rewind( aclfile );

	if( size < 0 )
		throw std::runtime_error( "size of ftpaccess < 0" );

	char *aclbuf = NULL;

	if( size == 0 ) {
		aclbuf = new char[1] ;
		memset( aclbuf, 0, 1 ) ;
	}
	else {
		aclbuf = new char[ size + 1 ];
		size_t bytes = fread( aclbuf, (size_t)size, 1, aclfile );

		if( bytes == 0 && ferror( aclfile ) ) {
			delete[] aclbuf;
			aclbuf = NULL;
			rewind( aclfile );
			throw std::runtime_error( "unable to read in ftpaccess file" );
		}
		*(aclbuf + size) = '\0';
	}

	rewind( aclfile );
	return aclbuf;
}

void FTPAccessFile::parseacl( char *aclbuf ) throw( std::bad_alloc )
try {
	assert( aclbuf != NULL );

	char *aclptr = aclbuf;


	while( *aclptr != '\0' ) {
		char *line = aclptr;

		while( *aclptr && *aclptr != '\n' )
			aclptr++;

		// now make the line a null terminated string
		*aclptr++ = (char) NULL;

		// save the raw line here, this grabs comments and blank lines
		aclmember *member = new aclmember;
		memset( member, 0, sizeof(aclmember) );
		member->rawText = new char[ strlen(line) + 1 ];
		strcpy( member->rawText, line );

		char *ptr = NULL;
		// deal with comments
		if( (ptr = strchr(line, '#')) != NULL ) {
			// allowed escaped '#' chars for path-filter (Dib)
			// added code to ensure comment at start of file works
			if( ((ptr > aclbuf) && (*(ptr-1) != '\\'))
			    || ptr == aclbuf )
				*ptr++ = '\0';
		}

		// get the keyword for the line if it exists. this is tricky
		// and is taken from the man page
		while( (ptr = strsep( &line, " \t" )) != NULL && *ptr == '\0')
			;

		if( ptr ) {
			strncpy( member->keyword, ptr, MAXKWLEN );
			member->keyword[ MAXKWLEN - 1 ] = '\0';
			int cnt = 0;

			while( (ptr = strsep( &line, " \t" )) != NULL ) {
				if( *ptr == '\0' )
					continue;

				// real error reporting here done by wu-ftpd
				if( cnt >= MAXARGS )
					break;

				member->arg[cnt] = NewStr( ptr );
				cnt++ ;
			}
		}

		AddMember( member );
	}

	delete[] aclbuf;
}       
catch(...) {
	delete[] aclbuf;
	throw;
}
