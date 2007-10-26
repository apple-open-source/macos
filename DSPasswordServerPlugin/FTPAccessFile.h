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

#ifndef FTPACCESSFILE_H
#define FTPACCESSFILE_H

#include <stdint.h>

using namespace std;

#include "ConfigurationFile.h"

/* helper structures */
// Supported authentication levels
enum {
	kAuthLevelStandard	= 1,
	kAuthLevelGSSAPI	= 2,
	kAuthLevelBoth		= 3
};

// Supported chroot types
enum {
	kChrootTypeStandard	= 1,
	kChrootTypeHomedir	= 2,
	kChrootTypeRestricted	= 4
};

typedef struct FTPConfigData {
	int32_t		maxConcurrentAnonUsers;
	int32_t		maxConcurrentRealUsers;
	uint32_t	allowAnonAccess;
} FTPConfigData, *FTPConfigDataPtr;

typedef struct {
	uint32_t	real;
	uint32_t	guest;
	uint32_t	anonymous;
} TypeList;

typedef TypeList LogTypeList;
typedef LogTypeList LogSecurity;
typedef LogTypeList LogCommands;

typedef struct {
	uint32_t	inbound;
	uint32_t	outbound;
} LogDirections;

typedef struct {
	LogDirections	real;
	LogDirections	guest;
	LogDirections	anonymous;
} LogTransfers;


/* relevent defines from wu-ftpd's extensions.h */
#define MAXARGS		50
#define MAXKWLEN	20

/* Modified structure to add prev for faster list operations and rawText
   field to help preserve existing configuration files */
typedef struct aclmember {
	aclmember	*next;
	aclmember	*prev;
	char		*rawText;
	char		keyword[MAXKWLEN];
	char		*arg[MAXARGS];
} aclmember;

class FTPAccessFile : public ConfigurationFile
{
public:
	FTPAccessFile( const char *filename = 
		       "/Library/FTPServer/Configuration/ftpaccess" )
		throw( std::bad_alloc, std::runtime_error );
	virtual  ~FTPAccessFile() throw();

	/* Flush the contents of the config file to disk */
	virtual void FlushFile();

	/* Set the number of allowed login failures. < 0 for default values */
	virtual int32_t GetLoginFails() throw();
  
	/* Set the administrator's email address. NULL means no address.
	   If the result is not NULL, then it has been alloced with new and
	   the caller is responsible for deallocating it. */
	virtual const char *GetEmail() throw();
	virtual void SetEmail( const char *email ) throw( std::bad_alloc );

	/* Set the server's Kerberos prinicpal. NULL means use the default
	   If the result is not NULL, then it has been alloced with new and
	   the caller is responsible for deallocating it. */
	virtual const char *GetKerberosPrincipal() throw();
	virtual void SetKerberosPrincipal( const char *principal ) throw( std::bad_alloc );


	/* Set the anonymous ftp root dir. NULL means no address.
		If the result is not NULL, then it has been alloced with new and
		the caller is responsible for deallocating it. */
	virtual const char *GetAnonRoot() throw();
	virtual void SetAnonRoot( const char *anonRootPath ) throw( std::bad_alloc );
 
	/* Set the status of anonymous users. 1 = YES, 0 = NO */
	virtual uint32_t GetAnonymousStatus() throw();
	virtual void SetAnonymousStatus( uint32_t status )
		throw( std::bad_alloc );
 
	/* Set the status of macbinary & disk Image auto-conversion (influence_listings). 1 = YES, 0 = NO */
	virtual uint32_t GetMacBinAndDmgAutoConversion() throw();
	virtual void SetMacBinAndDmgAutoConversion( uint32_t status )
		throw( std::bad_alloc );

	/* Set the limit of anonymous users. limit < 0 = no limit */
	virtual int32_t GetAnonymousLimit() throw();

	/* Set the limit of real users. limit < 0 = no limit */
	virtual int32_t GetRealLimit() throw();

	/* Set the name for the shutdown file the server monitors. NULL = no
	   file. GetShutdownFilename returns NULL or a new'd string that must
	   be deleted. */
	virtual char *GetShutdownFilename() throw();
	virtual void SetShutdownFilename( const char *filename )
		throw( std::bad_alloc );

	/* Set the name of the banner file used for banners. NULL = no file.
	  GetBannerFilename returns NULL or a new'd string that must be
	  deleted.*/
	virtual char *GetBannerFilename() throw();
	virtual void SetBannerFilename( const char *filename )
		throw(std::bad_alloc);

	/* Set the name of the login welcome message file. NULL = no file.
	   GetLoginMessageFilename returns NULL or a new'd string that must
	   deleted. */
	virtual char *GetLoginMessageFilename() throw();
	virtual void SetLoginMessageFilename( const char *filename )
		throw( std::bad_alloc );

	/* Set the command logging options */
	virtual LogCommands GetLogCommands() throw();
	virtual void SetLogCommands( LogCommands commands )
		throw( std::bad_alloc );

	/* Set the security / access violation options */
	virtual LogSecurity GetLogSecurity() throw();
	virtual void SetLogSecurity( LogSecurity security )
		throw( std::bad_alloc );

	/* Set if the ftp server uses syslog. 1 = syslog, 0 = no syslog */
	virtual uint32_t GetLogSyslog() throw();
	virtual void SetLogSyslog( uint32_t syslog ) throw( std::bad_alloc );

	/* Set the transfer logging options */
	virtual LogTransfers GetLogTransfers() throw();
	virtual void SetLogTransfers( LogTransfers transfers )
		throw( std::bad_alloc );

	/* Set the path to the virtual root, if FILENAME is NULL, then set
	   the root to the default root. GetVirtualRoot() returns NULL
	   on error only. If there is no virtual root, GetVirtualRoot() will
	   set the virtual root to the default and return that. The string
	   returned has been allocated with new and must be deleted. */
	virtual const char *GetVirtualRoot() throw();
	virtual void SetVirtualRoot( const char *filename )
		throw(std::bad_alloc);

	/* Set the password authentication method with kAuthLevel*.
	   Unknown levels default to setting the level to kAuthLevelStandard.*/
	virtual uint32_t GetAuthLevel() throw( std::bad_alloc );
	virtual void SetAuthLevel( uint32_t level ) throw( std::bad_alloc );

	/* Set the virtual chroot methodology. Uses kChrootType*. Unknown
	   types default to setting the tyep to kChrootTypeStandard. */
	virtual uint32_t GetChrootType() throw( std::bad_alloc );
	virtual void SetChrootType( uint32_t type ) throw( std::bad_alloc );

 protected:
	/* Create a new set of upload directives based on the ftp
	   cannocicalized path PATH. If PATH is NULL remove all
	   upload directives. */
	virtual void SetUploadDirectives( const char *path )
		throw( std::bad_alloc );
 
	/* return the string of the first element of the first aclmember of
	   kind in the config file, NULL if no such member exists */
	virtual char *GetSingleton( const char *kind ) throw();

	/* Make sure that there is only one element of kind in the config
	   file and that its first element is arg0. If arg0 = NULL then
	   remove all elements of kind from the config file. */
	virtual void SetSingleton( const char *kind, const char *arg0 )
		throw( std::bad_alloc );

	/* return the limit for a limit directive with class */
	virtual int32_t GetLimitForClass( const char *limitClass ) throw();

	/* set the limit for class to limit and optionaly specify a file
	   explaining why the user cannot login. If limit < 0 removes all
	   limit directives for class */

	/* Delete all log entries of the form log kind * */
	virtual void DeleteLogKind( const char *kind ) throw();

	/* Get a string representation of a typelist. Note that the typelist
	   must have at least one enabled member. The resulting string must
	   be deallocated. */
	virtual char *TypeListToStr( TypeList typelist ) throw(std::bad_alloc);

	/* Set the bits of typelist information in string in typelist. This
	   can be used to incrementally add info to typelist */
	virtual void ExtractTypeList( const char *string, TypeList &typelist )
		throw();

	/* Return a new string which is a copy of str */
	virtual char *NewStr( const char *str ) throw( std::bad_alloc );

	/* Return a new string which is the result of STR formatted with the
	   additional arguements. The result of the formatted string will
	   not exceed  STR. */

	/* recompute the rawText line for a member. Will deallocate an existing
	   rawText. Seperates the arguements by tabs */
	virtual void RedoRawText( aclmember *member ) throw( std::bad_alloc );

	/* return the next aclmember for the keyword. Not thread safe. Pass in
	   NULL to get the first entry. Returns NULL if no such entry */
	virtual aclmember *GetNextKeywordEntry( const char *keyword,
						aclmember *previous ) throw();

	/* debugging function  */
	void Description();
	
	/* Add a new aclmember to the end of the members list */
	void AddMember( aclmember *member ) throw();

	/* delete all the aclentries with a given keyword */
	virtual void DeleteKeywordEntries( const char *keyword ) throw();

	/* Remove a member from the acl list and deallocate its storage */
	virtual void RemoveAndDeallocMember( aclmember *member ) throw();

	/* wu-ftpd derived parsing function. Parses the acl buffer into a
	   list of acl components, takes in buffer to parse and free's the
	   buffer before returning. */
	virtual void parseacl( char *aclbuf ) throw( std::bad_alloc );
  
	/* reads the acl file into a buffer. Does not close the file. Rewinds
	   the file when it's done. Returns a pointer to the acl buffer */
	virtual char *readacl( std::FILE *file )
		throw( std::bad_alloc, std::runtime_error );

protected:
	aclmember	*aclmembers;	// acl list
};

#endif // FTPACCESSFILE_H
