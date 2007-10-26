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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H


#include <sys/types.h>

#include <cstdio>
#include <stdexcept>


// Strings to use when throwing configuration errors. These strings should
// be in servermgr_comman/constants.h, but we are not ready for that yet.
#define	UNABLE_TO_OPEN_FILE_ERR	"UNABLE_TO_OPEN_FILE_ERR"
#define	UNKNOWN_ERR		"UNKNOWN_ERR"


class ConfigurationFile
{
public:
	ConfigurationFile( const char *fileName )
		throw( std::bad_alloc, std::runtime_error );
	virtual ~ConfigurationFile() throw() ;

	/* Call this function to start writing a configuration file. Must be
	   matched by a call to StopWrite(). Returns a FILE* to write to,
	   NULL if there is an error. This call is not really thread safe,
	   that is each configuration file should have at most one thread
	   writing to it at a time. We could add locking to enforce this
	   constraint, but the work and overhead is not justified at the
	   present. */
	virtual std::FILE *StartWrite();
 
	/* Return true if the old configuration file was replaced by the new
	   file, otherwise return false. */
	virtual bool StopWrite();

	/* Returns a FILE* for the configuration file. Do not close this file.
	   Use this to read a file. */
	virtual std::FILE *GetFileForReading() throw();

protected:
	/* Attempts to copy the contents of backupFileName to a new,
	   nonexistant file in newFileName while ensuring it has exclusive
	   file locks on the  file. Returns true if the restore completed
	   sucessfully, false otherwise */
	virtual std::FILE *AttemptRestore(	const char *newFileName, 
						const char *backupFileName );

	/* Cleanup the memory. This function helps make cleaning up memory
	   in the constructor cleaner. */
	virtual void CleanupMemory() throw();

protected:
	int fd;			// file descriptor for the config file
	std::FILE *file;	// FILE* for the config file
	std::FILE *tmpFile;	// FILE* for temporary config file that
				// StartWrite() gives out
	const char *kFileName;	// name of the configuration file
	char *backupFileName;	// name of the backup configuration file
	char *defaultFileName;	// name of the default configuration file
	char *tmpFileName;	// name of the temporary file used when writing
				// out the configuration file
  	mode_t permissions;	// default file permissions for the config file
 
protected:
	static const char *kBackupSuffix;	// suffix for backup files
	static const char *kDefaultSuffix;	// suffix for default files
	static const char *kTmpSuffix;		// suffix for temporary files
};

#endif // CONFIGURATION_H
