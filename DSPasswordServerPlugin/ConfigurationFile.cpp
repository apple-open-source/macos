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

#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <cassert>
#include <new>

#include "syslog.h"

#include "ConfigurationFile.h"

using namespace std;

static const mode_t kFilePermissions =  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

const char *ConfigurationFile::kBackupSuffix	= "~";
const char *ConfigurationFile::kDefaultSuffix	= ".default";
const char *ConfigurationFile::kTmpSuffix	= ".tmp";

ConfigurationFile::ConfigurationFile( const char *fileName ) 
	throw( std::bad_alloc, std::runtime_error )
	: fd( -1 ), file( NULL ), tmpFile( NULL ), kFileName( NULL ), 
	backupFileName(NULL), defaultFileName(NULL), tmpFileName(NULL),
	permissions( kFilePermissions )
{
	if( !fileName )
		throw std::runtime_error( "NULL filename for ConfigurationFile" );

	kFileName = strdup( fileName );
	if( !this->kFileName ) {
		throw std::bad_alloc();
	}

	// NOTE: we might want to change this to something that asks the user
	// if they want to override the permisssions, if yes keep the change,
	// if not fail.

	// Save the old permissions, chmod to the new permissions, open the
	// file, then change back to the old permissions
	struct stat stats;
	int error = stat( kFileName, &stats );
	if( error != -1 ) {
		permissions = stats.st_mode;      
		chmod( kFileName, kFilePermissions );
	}

	try {
		// names of other files
		backupFileName = new char[strlen(kFileName) + strlen(kBackupSuffix) + 1] ;
		defaultFileName = new char[strlen(kFileName) + strlen(kDefaultSuffix) + 1];
		tmpFileName = new char[ strlen(kFileName) + strlen(kTmpSuffix) + 1 ];
		strcpy( backupFileName, kFileName );
		strcpy( defaultFileName, kFileName );
		strcpy( tmpFileName, kFileName );
		strcat( backupFileName, kBackupSuffix );
		strcat( defaultFileName, kDefaultSuffix );
		strcat( tmpFileName, kTmpSuffix );
		
		// try to open an already existing file
		fd = open(	kFileName,
				O_RDWR | O_EXLOCK | O_NONBLOCK,
				kFilePermissions );
		
		if( fd != -1 ) {
			file = fdopen( fd, "r+" );
		}
		else {
			// file does not exist or is locked by someone else,
			// so try to restore it
			file = AttemptRestore( kFileName, backupFileName );

			if( !file )
				file=AttemptRestore(kFileName,defaultFileName);

			if( !file ) {
				// try again after restore attempts, in the
				// worst case we create a blank file
				fd = open( kFileName, O_RDWR | O_EXLOCK | O_CREAT | O_NONBLOCK, kFilePermissions );
				file = fdopen( fd, "r+" );
			}

			// NOTE: we may want to add another failure case that attempts
			// to open the file disregarding the file locking semantics.
		}

		// reset the permissions
		chmod( kFileName, permissions );

		if( !file )
			throw runtime_error( UNABLE_TO_OPEN_FILE_ERR );

	}
	catch( ... ) {
		// reset the permissions
		chmod( kFileName, permissions );
		CleanupMemory();
		throw;
	}
}

void ConfigurationFile::CleanupMemory() throw()
{
	// file names
	delete[] backupFileName;
	delete[] defaultFileName;
	delete[] tmpFileName;

	if( kFileName )
		free( (char *)kFileName );

	kFileName = backupFileName = defaultFileName = tmpFileName = NULL;
}

std::FILE *ConfigurationFile::AttemptRestore( const char *newFileName, 
					      const char *backupFileName )
{
	assert( newFileName );
	assert( backupFileName );
	
	// make sure we don't block waiting for a lock on the backup file
	// only have a shared lock on the backup file because we only need
	// to read it
	int backupFD = open( backupFileName, O_RDONLY | O_SHLOCK | O_NONBLOCK,
			     kFilePermissions );
  
	// if we couldn't open the file there might be a permissions problem,
	// temporarily change them to include S_IRUSR and try again. Then set
	// them back
	if( backupFD == -1 ) {
		struct stat backupStats;
		int error = stat( backupFileName, &backupStats );
  
		if( error == -1 )
			return NULL;

		// by default make sure we can read this file
		mode_t backupPermissions = backupStats.st_mode | S_IRUSR;
		chmod( backupFileName, backupPermissions );
		backupFD = open( backupFileName,
				 O_RDONLY | O_SHLOCK | O_NONBLOCK,
				 kFilePermissions );
		chmod( backupFileName, backupStats.st_mode );
   
		if( backupFD == -1 )
			return NULL;
	}

	FILE *backup = fdopen( backupFD, "r" );  
	if( backup == NULL )
		return NULL;
  
	int newFD = open( newFileName, O_RDWR | O_CREAT | O_EXCL | O_EXLOCK,
			  kFilePermissions );
	if( newFD == -1 ) {
		flock( backupFD, LOCK_UN );
		fclose( backup );
		return NULL;
	}
	
	FILE *newFile = fdopen( newFD, "r+" );
	if( newFile == NULL ) {
		flock( backupFD, LOCK_UN );
		fclose( backup );
		return NULL;
	}

#define CLEANUP_FILES()					\
do {							\
	flock( backupFD, LOCK_UN );			\
	flock( newFD, LOCK_UN );			\
	fclose( backup );				\
	fclose( newFile );				\
} while(0)		

	// make sure there were no file errors
	if( ferror( newFile ) || ferror( backup ) ) {
		CLEANUP_FILES();	
    
		// remove file because it could be only partially restored
		unlink( newFileName );
		return NULL;
	}
  
	// read data from backup and copy it to new
	int pageSize = getpagesize();
	assert( pageSize > 0 );
	char *buffer = (char *) alloca( pageSize );
	assert( buffer != NULL );

	while( !feof( backup ) && !ferror( newFile ) && !ferror( backup ) ) {
		size_t readSize = fread( buffer, sizeof( buffer[0] ), 
					 sizeof( buffer ), backup );
		size_t writeSize = fwrite( buffer, sizeof( buffer[0] ), 
					   readSize, newFile );

		// only happens when there is an error
		if( readSize != writeSize ) {
			CLEANUP_FILES();
      
			// remove file because it could be only partially
			// restored
			unlink( newFileName );
			return NULL;
		}
	}

	flock( backupFD, LOCK_UN );
	fclose( backup );
	file = newFile;
	fd = newFD;
	rewind( file );
	fflush( file );
	clearerr( file );

	return file;

#undef CLEANUP_FILES
}

ConfigurationFile::~ConfigurationFile() throw()
{
	// unlock the file, taking proper precautions
	if( fd != -1 )
		flock( fd, LOCK_UN );
	
	// close the file, taking proper precautions
	if( file != NULL )
		fclose( file );
  
	CleanupMemory();
}

FILE *ConfigurationFile::GetFileForReading() throw()
{
	return file;
}

FILE *ConfigurationFile::StartWrite()
{	
	// clean up a crufty tmp file if it still exists for some reason
	unlink( tmpFileName );
  
	assert( fd != -1 );
	assert( file != NULL );
	assert( tmpFile == NULL );

	// create a new file with tmpFileName
	int tmpFD = open( tmpFileName, 
			O_RDWR | O_TRUNC | O_EXLOCK | O_CREAT | O_NONBLOCK,
			kFilePermissions );
  
	if( tmpFD == -1 ) {
		return NULL;
	}

	tmpFile = fdopen( tmpFD, "r+" );
	if( tmpFile == NULL ) {
		flock( tmpFD, LOCK_UN );
		close( tmpFD );
		tmpFD = -1 ;
		unlink( tmpFileName );

		return NULL;
	}
  
	return tmpFile;
}
 

bool ConfigurationFile::StopWrite()
{
	assert( tmpFile != NULL );
	fflush( tmpFile ) ;

	// save the old backup permissions
	struct stat backupStats;
	int error = stat( backupFileName, &backupStats );
	mode_t backupPermissions = 
		error != -1 ? backupStats.st_mode : kFilePermissions;

	// rename realFile -> backupFile
	error = rename( kFileName, backupFileName );
	if( error != 0 ) {
		flock( fileno( tmpFile ), LOCK_UN );
		fclose( tmpFile );
		unlink( tmpFileName );
		tmpFile = NULL;
		return false;
	}

	// restore the old backup permissions
	chmod( backupFileName, backupPermissions );

	// rename tmpFile -> realfile
	error = rename( tmpFileName, kFileName );
	if( error != 0 ) {
		flock( fileno( tmpFile ), LOCK_UN );
		fclose( tmpFile );
		unlink( tmpFileName );
		tmpFile = NULL;
		return false;
	}

	// set the realfile to have the same perms as the old realfile
	chmod( kFileName, permissions );

	// close the old file descriptors and release the locks for file and fd
	// which now point to backupFileName
	// NOTE: we should be able to communicate non-fatal errors here, perhaps
	// through a callback.
	error = flock( fd, LOCK_UN );
	error = fclose( file );

	// set file and fd to the tmpFile
	file = tmpFile;
	fd = fileno( tmpFile );
	rewind( file );
	tmpFile = NULL;

	// reset the file's error status because it is in a known good state
	clearerr(file);

	return true;
}
