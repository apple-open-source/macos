/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <TargetConditionals.h>

extern "C" {
#import <stdio.h>
#import <string.h>
#import <sys/fcntl.h>
#import <sys/stat.h>
#import <syslog.h>
#import <unistd.h>
#import <LDAP/ldap.h>
#import "AuthDBFile.h"
#import "PSUtilitiesDefs.h"
#import "SASLCode.h"
#import "SMBAuth.h"
#import "ReplicaFileDefs.h"
#import "KerberosInterface.h"

#if COMPILE_WITH_RSA_LOAD
    #import "bufaux.h"
    #import "buffer.h"
    #import "cipher.h"
    #import "xmalloc.h"
    #import "ssh.h"
#endif
};

@implementation AuthDBFile

-(id)init
{
	self = [super init];
	mPWFileNO = -1;
	
	const char *altPathPrefix = getenv("PWSAltPathPrefix");
	if ( altPathPrefix != NULL )
	{
		char path[PATH_MAX];
		snprintf( path, sizeof(path), "%s/%s", altPathPrefix, kPWDirPath );
		mDirPathStr = strdup( path );
		snprintf( path, sizeof(path), "%s/%s", altPathPrefix, kPWFilePath );
		mFilePathStr = strdup( path );
	}
	else
	{
		mDirPathStr = strdup( kPWDirPath );
		mFilePathStr = strdup( kPWFilePath );
	}
	[self createOverflowObject];
	
	return self;
}

-(id)initWithFile:(const char *)inFilePath
{
	char *slash;
	
	self = [super init];
	mPWFileNO = -1;
	
	mFilePathStr = strdup( inFilePath );
	mDirPathStr = strdup( inFilePath );
	if ( mDirPathStr != NULL ) {
		slash = rindex( mDirPathStr, '/' );
		if ( slash != NULL )
			*slash = '\0';
	}
	
	[self createOverflowObject];
	return self;
}

-(void)createOverflowObject
{
	mOverflow = [AuthOverflowFile new];
}


-free
{	
	[self freeRSAKey];
	[self closePasswordFile];
	[self closeFreeListFile];
	
	if ( mFilePathStr != NULL )
		free( mFilePathStr );
	if ( mDirPathStr != NULL )
		free( mDirPathStr );
	if ( mSearchBase != NULL )
		free( mSearchBase );
	
	[mOverflow free];
	
	return [super free];
}


-(int)validateFiles
{
    int err;
    
    err = [self validatePasswordFile];
    if (err == 0)
		err = [self validateFreeListFile];
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	validatePasswordFile
//
//	Returns: file errors
//
//	Do internal checks on the database file.
//	Checks: signature, version, and size.
//----------------------------------------------------------------------------------------------------

-(int)validatePasswordFile
{
    int err;
    struct stat sb;
    PWFileHeader dbHeader;
	
    // validate
    err = lstat( mFilePathStr, &sb );
    if ( err == 0 )
		err = [self getHeader:&dbHeader];
    if ( err == 0 )
	{
        if ( mPWFile != NULL )
        {
            if ( mPWFileHeader.signature != kPWFileSignature ||
                 mPWFileHeader.version != kPWFileVersion ||
                 sb.st_size != (int32_t)(sizeof(mPWFileHeader) + mPWFileHeader.numberOfSlotsCurrentlyInFile * sizeof(PWFileEntry)) )
            {
                err = -1;
            }
        }
        else
        {
            err = -1;
        }
    }
    
    if ( err == 0 )
        mPWFileValidated = YES;
    
    return err;
}


-(int)validateFreeListFile
{
	return 0;
}


-(int)createPasswordFile
{
    int err = -1;
    size_t writeCount;
    const char *dirPathStr = mDirPathStr;
	const char *filePathStr = mFilePathStr;
	
	// make sure the directory exists
	err = pwsf_mkdir_p( dirPathStr, S_IRWXU );
	
	// if it existed before, double-check the permissions
	if ( err != 0 && errno == EEXIST )
		err = chmod( dirPathStr, S_IRWXU );
	
    // create new file
    mPWFile = fopen( filePathStr, "w+" );
    if ( mPWFile != NULL )
    {
		mPWFileNO = fileno( mPWFile );
        err = chmod( filePathStr, S_IRUSR | S_IWUSR );
        if ( err == -1 )
            err = errno;
        // ignore
        err = 0;
        
        // set header initial state
        bzero( &mPWFileHeader, sizeof(PWFileHeader) );
        mPWFileHeader.signature = kPWFileSignature;
        mPWFileHeader.version = kPWFileVersion;
		mPWFileHeader.entrySize = sizeof(PWFileEntry);
        mPWFileHeader.sequenceNumber = 0;
        mPWFileHeader.numberOfSlotsCurrentlyInFile = kPWFileInitialSlots;
        mPWFileHeader.deepestSlotUsed = 0;
        mPWFileHeader.deepestSlotUsedByThisServer = 0;
		
        mPWFileHeader.access.usingHistory = 0;
        mPWFileHeader.access.usingExpirationDate = 0;
        mPWFileHeader.access.usingHardExpirationDate = 0;
        mPWFileHeader.access.requiresAlpha = 0;
        mPWFileHeader.access.requiresNumeric = 0;
        mPWFileHeader.access.passwordIsHash = 0;
		
        // do not need to set these if usingExpirationDate and usingHardExpirationDate are false
        //mPWFileHeader.access.expirationDateGMT
        //mPWFileHeader.access.hardExpireDateGMT
    
        mPWFileHeader.access.maxMinutesUntilChangePassword = 0;
        mPWFileHeader.access.maxMinutesUntilDisabled = 0;	
        mPWFileHeader.access.maxMinutesOfNonUse = 0;
        mPWFileHeader.access.maxFailedLoginAttempts = 0;
        mPWFileHeader.access.minChars = 0;
        mPWFileHeader.access.maxChars = 0;
		time( (time_t *)&mPWFileHeader.accessModDate );
		
        // write header
		err = [self setHeader:&mPWFileHeader];
		
        // write blank space
        if ( err == 0 )
        {
            PWFileEntry anEntry;
            int i;
            
            bzero( &anEntry, sizeof(PWFileEntry) );
            
            for ( i = kPWFileInitialSlots; i > 0; i-- )
            {
                writeCount = fwrite( &anEntry, sizeof(PWFileEntry), 1, mPWFile );
                if ( writeCount != 1 )
                {
                    err = -1;
                    break;
                }
            }
        }
        fflush( mPWFile );
        
        if ( err == 0 )
			[self validateFiles];
		else
            unlink( filePathStr );
    }
    else
    {
        if ( errno )
            err = errno;
    }
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	openPasswordFile
//
//	Returns: file errors
//	Does not Return: file mapping errors.
//
//	Utility function to open the database. When the database is opened with write-access,
//	the file is not mapped to keep it immediately up-to-date. For read-only, the file is
//	mapped by all except getPasswordRec. Access is switched to write-access for each AUTH,
//	so there is no advantage to mapping.
//----------------------------------------------------------------------------------------------------

-(int)openPasswordFile:(const char *)mode
{
    int err = 0;
     
    if ( mPWFile != NULL && strcmp( mode, mPWFilePermission ) == 0 )
    {
        return err;
    }
    else
    {
        [self closePasswordFile];
        
        mPWFile = fopen( mFilePathStr, mode );
		
		// handle read-only file system
		if ( mPWFile == NULL && errno == EROFS )
		{
			mReadOnlyFileSystem = YES;
			mPWFile = fopen( mFilePathStr, "r" );
		}
		
        if ( mPWFile != NULL )
        {
		mPWFileNO = fileno( mPWFile );
            strcpy( mPWFilePermission, mReadOnlyFileSystem ? "r" : mode );
        }
        else
        {
            err = errno;
            if ( err == 0 )
                err = -1;
        }
    }
    
    return err;
}


-(void)closePasswordFile
{
	[self pwWait];
	if ( mPWFile != NULL ) {
		if ( mDBFileLocked )
			[self pwUnlock];
		
		fclose( mPWFile );
		mPWFile = NULL;
		mPWFileNO = -1;
    }
	mGotHeader = NO;
	[self pwSignal];
}


-(void)closeFreeListFile
{
	[self pwWait];
    if ( mFreeListFile != NULL ) {
        fclose( mFreeListFile );
        mFreeListFile = NULL;
    }
	[self pwSignal];
}


-(void)freeRSAKey
{
	[self rsaWait];
	if ( rsaKey != NULL ) {
		RSA_free( rsaKey );
		rsaKey = NULL;
	}
	[self rsaSignal];
}


-(void)resetPasswordFileState
{
    if ( mPWFile )
        fflush( mPWFile );
    
    [self closePasswordFile];
	
	// force the rsa key to be reloaded
    [self freeRSAKey];
}


-(void)pwLock
{
	int tries = 3;
	int result;
	
	if ( mPWFile != NULL )
	{
		while ( (result = flock( mPWFileNO, LOCK_EX | LOCK_NB )) == -1 && tries-- > 0 )
			usleep( 25000 );
		
		if ( result == 0 )
			mDBFileLocked = YES;
	}
}


//----------------------------------------------------------------------------------------------------
//	pwLock
//
//	Returns: YES if the lock is obtained.
//----------------------------------------------------------------------------------------------------

-(BOOL)pwLock:(unsigned long)inMillisecondsToWait
{
	const long millisecondsPerTry = 25;
	long tries = inMillisecondsToWait / millisecondsPerTry;
	BOOL locked = NO;
	
	if ( mPWFile == NULL )
	{
		mDBFileLocked = NO;
		[self openPasswordFile:"r+"];
    }
	
	if ( mPWFile != NULL )
	{
		if ( mDBFileLocked )
			return YES;
		
		if ( tries <= 0 )
			tries = 1;
		
		while ( tries-- > 0 )
		{
			if ( flock( mPWFileNO, LOCK_EX | LOCK_NB ) == 0 )
			{
				locked = YES;
				break;
			}
			
			usleep( millisecondsPerTry * 1000 );
		}
	}
	
	mDBFileLocked = locked;
	return locked;
}


-(void)pwUnlock
{
	if ( mPWFile != NULL )
		flock( mPWFileNO, LOCK_UN );
	mDBFileLocked = NO;
}


-(void)pwWait
{
	// override in sub-class
}


-(void)pwSignal
{
	// override in sub-class
}


-(void)rsaWait
{
	// override in sub-class
}


-(void)rsaSignal
{
	// override in sub-class
}


//----------------------------------------------------------------------------------------------------
//	getHeader
//
//	Returns: 0=success, -1=fail, -2=recovery failed, -3 recovery used
//----------------------------------------------------------------------------------------------------

-(int)getHeader:(PWFileHeader *)outHeader
{
	return [self getHeader:outHeader cachedCopyOK:NO];
}


-(int)getHeader:(PWFileHeader *)outHeader cachedCopyOK:(BOOL)inCanUseCachedCopy
{
    int err = -1;
    ssize_t readCount;
    BOOL saveAfterReleasingSemaphore = NO;
	
    if ( outHeader == NULL )
        return -1;
    
	if ( inCanUseCachedCopy && mGotHeader )
	{
		memcpy( outHeader, &mPWFileHeader, sizeof(PWFileHeader) );
		return 0;
	}
	
    [self pwWait];
	err = [self openPasswordFile:mReadOnlyFileSystem ? "r" : "r+"];
    if ( err == 0 && mPWFile )
    {
		readCount = pread( mPWFileNO, outHeader, sizeof(PWFileHeader), 0 );
        pwsf_EndianAdjustPWFileHeader( outHeader, 1 );
		
		if ( outHeader->signature == kPWFileSignature )
		{
			// adopt the new header data
			memcpy( &mPWFileHeader, outHeader, sizeof(PWFileHeader) );
		}
		else
		{
			err = -2;
			
			// bad news, try to recover
			if ( mGotHeader && mPWFileHeader.signature == kPWFileSignature )
			{
				err = -3;
				
				memcpy( outHeader, &mPWFileHeader, sizeof(PWFileHeader) );
				saveAfterReleasingSemaphore = YES;
			}
		}
		
		mGotHeader = YES;
    }
    [self pwSignal];
    
	if ( saveAfterReleasingSemaphore )
		[self setHeader:&mPWFileHeader];
	
    return err;
}


//----------------------------------------------------------------------------------------------------
//	setHeader
//
//	Returns: 0=success, -1=fail
//----------------------------------------------------------------------------------------------------

-(int)setHeader:(const PWFileHeader *)inHeader
{
    int err = -1;
    long writeCount;
    
    if ( inHeader == NULL )
        return -1;
	if ( inHeader->signature != kPWFileSignature )
		return -1;
	if ( mReadOnlyFileSystem )
		return -1;
	
    [self pwWait];
    err = [self openPasswordFile:"r+"];
    if ( err == 0 && mPWFile )
    {
        err = fseek( mPWFile, 0, SEEK_SET );
        if ( err == 0 )
        {
            // adopt the new header data
			if ( inHeader != &mPWFileHeader )
				memcpy( &mPWFileHeader, inHeader, sizeof(PWFileHeader) );
            
            // write to disk
#if TARGET_RT_LITTLE_ENDIAN
			PWFileHeader diskHeader = mPWFileHeader;
			pwsf_EndianAdjustPWFileHeader( &diskHeader, 0 );
            writeCount = fwrite( &diskHeader, sizeof(PWFileHeader), 1, mPWFile );
			bzero( &diskHeader, sizeof(PWFileHeader) );
#else
            writeCount = fwrite( &mPWFileHeader, sizeof(PWFileHeader), 1, mPWFile );
#endif
            if ( writeCount != 1 )
            {
                err = -1;
            }
            fflush( mPWFile );
        }
    }
	[self pwSignal];
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	getRSAPublicKey
//
//	Returns a base64 encoded rsa key
//----------------------------------------------------------------------------------------------------

-(int)getRSAPublicKey:(char *)outRSAKeyStr
{
    PWFileHeader dbHeader;
    int result = 0;
    long len;
	
    if ( outRSAKeyStr == NULL )
        return -1;
    *outRSAKeyStr = '\0';
    
	result = [self getHeader:&dbHeader cachedCopyOK:YES];
    if ( result == 0 || result == -3 )
	{
        strncpy(outRSAKeyStr, (char *)dbHeader.publicKey, kPWFileMaxPublicKeyBytes);
		
		// strip linefeed from the end
		len = strlen(outRSAKeyStr);
		if ( len > 0 && outRSAKeyStr[len-1] == '\n' )
			outRSAKeyStr[len-1] = '\0';
	}
	
    bzero(&dbHeader, sizeof(dbHeader));
    
    return result;
}


//----------------------------------------------------------------------------------------------------
//	loadRSAKeys
//
//	Returns: -1=no code, 0=no key, 1=success
//
//	loads the key blob from the database header into a struct that can be used with
//	BSD RSA functions.
//----------------------------------------------------------------------------------------------------

-(int)loadRSAKeys
{
    int result = 0;
	unsigned int idx;
	
#if COMPILE_WITH_RSA_LOAD

    PWFileHeader dbHeader;
    char passphrase[1] = "";
    
    // check if we already loaded the key
	[self rsaWait];
   	if ( rsaKey != NULL )
	{
		[self rsaSignal];
		return 1;
    }
	
	result = [self getHeader:&dbHeader cachedCopyOK:YES];
    if ( result == 0 || result == -3 )
    {
        int check1, check2, cipher_type;
        off_t len;
        Buffer buffer, decrypted;
        char *cp;
        CipherContext cipher;
        BN_CTX *ctx;
        BIGNUM *aux;
        time_t now;
		
        len = dbHeader.privateKeyLen;
        
        buffer_init(&buffer);
        buffer_append_space(&buffer, &cp, len);
        
        memcpy(cp, dbHeader.privateKey, len);
        
        /* Check that it is at least big enought to contain the ID string. */
        if ( len < (off_t)sizeof(AUTHFILE_ID_STRING) ) {
            syslog(LOG_INFO, "Bad key.");
            buffer_free(&buffer);
			
			[self rsaSignal];
            return 0;
        }
        /*
        * Make sure it begins with the id string.  Consume the id string
        * from the buffer.
        */
        for (idx = 0; idx < (unsigned int) sizeof(AUTHFILE_ID_STRING); idx++)
            if (buffer_get_char(&buffer) != (unsigned char) AUTHFILE_ID_STRING[idx]) {
                syslog(LOG_ALERT, "Bad key.");
				buffer_free(&buffer);
				
				[self rsaSignal];
                return 0;
            }
        /* Read cipher type. */
        cipher_type = buffer_get_char(&buffer);
        (void) buffer_get_int(&buffer);	/* Reserved data. */
    
        /* Read the public key from the buffer. */
        buffer_get_int(&buffer);
        rsaKey = RSA_new();
		if ( rsaKey == NULL ) {
			[self rsaSignal];
			return 0;
		}
        rsaKey->n = BN_new();
        buffer_get_bignum(&buffer, rsaKey->n);
        rsaKey->e = BN_new();
        buffer_get_bignum(&buffer, rsaKey->e);
        //if (comment_return)
        //    *comment_return = buffer_get_string(&buffer, NULL);
        //else
            xfree(buffer_get_string(&buffer, NULL));
    
        /* Check that it is a supported cipher. */
        if (((cipher_mask1() | SSH_CIPHER_NONE | SSH_AUTHFILE_CIPHER) & (1 << cipher_type)) == 0) {
            syslog(LOG_INFO, "Unsupported cipher %.100s used in key.", cipher_name(cipher_type));
            buffer_free(&buffer);
            goto fail;
        }
        /* Initialize space for decrypted data. */
        buffer_init(&decrypted);
        buffer_append_space(&decrypted, &cp, buffer_len(&buffer));
    
        /* Rest of the buffer is encrypted.  Decrypt it using the passphrase. */
        cipher_set_key_string(&cipher, cipher_type, passphrase);
        cipher_decrypt(&cipher, (unsigned char *) cp,
                (unsigned char *) buffer_ptr(&buffer),
                buffer_len(&buffer));
    
        buffer_free(&buffer);
    
        check1 = buffer_get_char(&decrypted);
        check2 = buffer_get_char(&decrypted);
        if (check1 != buffer_get_char(&decrypted) ||
            check2 != buffer_get_char(&decrypted)) {
            if (strcmp(passphrase, "") != 0)
                syslog(LOG_INFO, "Bad passphrase supplied for key.");
            /* Bad passphrase. */
            buffer_free(&decrypted);
    fail:
            RSA_free(rsaKey);
			rsaKey = NULL;
			[self rsaSignal];
            return 0;
        }
        /* Read the rest of the private key. */
        rsaKey->d = BN_new();
		if (rsaKey->d == NULL) {
			RSA_free(rsaKey);
			rsaKey = NULL;
			[self rsaSignal];
            return 0;
		}
        buffer_get_bignum(&decrypted, rsaKey->d);
        rsaKey->iqmp = BN_new();
        buffer_get_bignum(&decrypted, rsaKey->iqmp);	/* u */
        /* in SSL and SSH p and q are exchanged */
        rsaKey->q = BN_new();
        buffer_get_bignum(&decrypted, rsaKey->q);		/* p */
        rsaKey->p = BN_new();
        buffer_get_bignum(&decrypted, rsaKey->p);		/* q */
    
        ctx = BN_CTX_new();
        aux = BN_new();
    
        BN_sub(aux, rsaKey->q, BN_value_one());
        rsaKey->dmq1 = BN_new();
        BN_mod(rsaKey->dmq1, rsaKey->d, aux, ctx);
    
        BN_sub(aux, rsaKey->p, BN_value_one());
        rsaKey->dmp1 = BN_new();
        BN_mod(rsaKey->dmp1, rsaKey->d, aux, ctx);
    
        BN_clear_free(aux);
        BN_CTX_free(ctx);
    
        buffer_free(&decrypted);
    
		time(&now);
		srand((int)now);
		if ( RSA_blinding_on( rsaKey, NULL ) != 1 )
			syslog( LOG_INFO, "could not enable RSA_blinding" );
		
		bzero(&dbHeader, sizeof(dbHeader));
		
		[self rsaSignal];
        return 1;
	}
    
    bzero(&dbHeader, sizeof(dbHeader));
    [self rsaSignal];
	
#else
    syslog(LOG_ALERT, "RSA key loading not compiled\n");
    result = -1;
#endif

    return result;
}


//----------------------------------------------------------------------------------------------------
//	decryptRSA
//
//	Returns: -1=fail, 0=success
//----------------------------------------------------------------------------------------------------

-(int)decryptRSA:(unsigned char *)inBlob length:(int)inBlobLen result:(unsigned char *)outBlob
{
    int len;
    int result = 0;
	
	if ( [self loadRSAKeys] != 1 )
        return -1;
    
	[self rsaWait];
	len = RSA_private_decrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
	[self rsaSignal];
	
	if (len <= 0)
	{
		// print the error for debugging only. The error code may apply to Klima-Pokomy-Rosa attack.
		//syslog( LOG_INFO, "rsa_private_decrypt() failed, err = %lu", ERR_get_error() );
        syslog( LOG_ALERT, "rsa_private_decrypt() failed" );
        result = -1;
		
		// let's try reloading the key
		[self freeRSAKey];
		
		if ( [self loadRSAKeys] == 1 )
		{
			[self rsaWait];
			len = RSA_private_decrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
			[self rsaSignal];
			
			if ( len > 0 )
				result = 0;
		}
    }
	
    return result;
}


//----------------------------------------------------------------------------------------------------
//	encryptRSA
//
//	Returns: -1=fail, or length of encrypted part of <outBlob>
//----------------------------------------------------------------------------------------------------

-(int)encryptRSA:(unsigned char *)inBlob length:(int)inBlobLen result:(unsigned char *)outBlob
{
    int len;
    int maxRSASize;
	
    if ( [self loadRSAKeys] != 1 )
        return -1;
    
	// the maximum length of a block when using RSA_PKCS1_PADDING
	// is RSA_size( rsaKey ) - 11 (see the man page for RSA_public_encrypt)
	maxRSASize = RSA_size( rsaKey );
	if ( inBlobLen > maxRSASize - 11 )
		inBlobLen = maxRSASize - 11;
	
	[self rsaWait];
	len = RSA_public_encrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
	[self rsaSignal];
	
    if ( len <= 0 ) {
		//pwsf_fatal("RSA_public_encrypt() failed");
        return -1;
    }
    
	outBlob[len] = '\0';
	
    return len;
}


//----------------------------------------------------------------------------------------------------
//	isWeakAuthMethod
//
//	Returns: Boolean (0 == NO, 1 == YES)
//
//	A "weak" authentication method is one that is not secure enough to allow administration.
//	Generally, methods like CRYPT and PLAIN are not trusted because they are replayable.
//	CRAM and similar methods are trusted because a brute-force attack would take some time.
//----------------------------------------------------------------------------------------------------

-(int)isWeakAuthMethod:(const char *)inMethod
{
    int index;
    int result;
	PWFileHeader dbHeader;
    
	result = [self getHeader:&dbHeader cachedCopyOK:YES];
	if ( result != 0 && result != -3 )
        return 1;
    
    for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
        if ( strcmp( inMethod, dbHeader.weakAuthMethods[index].method ) == 0 )
            return 1;
    
    return 0;
}


-(int)addWeakAuthMethod:(const char *)inMethod
{
	int index;
    PWFileHeader ourHeader;
    int result = 0;
   
	if ( [self isWeakAuthMethod:inMethod] )
		return 0;
	 
	[self pwLock:kOneSecondLockInterval];
	
    result = [self getHeader:&ourHeader];
    if ( result == 0 || result == -3 )
    {
		for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
		{
			if ( ourHeader.weakAuthMethods[index].method[0] == 0 )
			{
				strcpy( ourHeader.weakAuthMethods[index].method, inMethod );
				result = [self setHeader:&ourHeader];
				break;
			}
		}
	}
	
	[self pwUnlock];
	
    return result;
}


-(int)removeWeakAuthMethod:(const char *)inMethod
{
	int index;
    PWFileHeader ourHeader;
    int result = 0;
    
    [self pwLock:kOneSecondLockInterval];
	
    result = [self getHeader:&ourHeader];
    if ( result == 0 || result == -3 )
    {
		for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
		{
			if ( strcmp( inMethod, mPWFileHeader.weakAuthMethods[index].method ) == 0 )
			{
				bzero( mPWFileHeader.weakAuthMethods[index].method, SASL_MECHNAMEMAX+1 );
				result = [self setHeader:&ourHeader];
				break;
			}
		}
	}
	
	[self pwUnlock];
	
    return result;
}


//----------------------------------------------------------------------------------------------------
//	expandDatabase
//
//	Returns: file errors
//
//	Expands the database file to allocate room for new slots.
//	If outSlot is NULL, no slots are assigned; otherwise, the next available slot is returned.
//----------------------------------------------------------------------------------------------------

-(int)expandDatabase:(unsigned long)inNumSlots nextAvailableSlot:(long *)outSlot
{
	int err;
	int writeCount;
	
	if ( mReadOnlyFileSystem )
		return -1;
	
	[self pwWait];
	
	err = [self openPasswordFile:"r+"];
	if ( err == 0 && mPWFile != NULL )
	{
		// write blank space
		err = fseek( mPWFile, 0, SEEK_END );
		if ( err == 0 )
		{
			PWFileEntry anEntry;
			int i;
			
			bzero( &anEntry, sizeof(PWFileEntry) );
			
			for ( i = inNumSlots; i > 0; i-- )
			{
				writeCount = fwrite( &anEntry, sizeof(PWFileEntry), 1, mPWFile );
				if ( writeCount != 1 )
				{
					err = -1;
					break;
				}
			}
		}
		
		// update header
		mPWFileHeader.numberOfSlotsCurrentlyInFile += inNumSlots;
		if ( outSlot != NULL )
		{
			mPWFileHeader.deepestSlotUsed++;
			mPWFileHeader.deepestSlotUsedByThisServer = mPWFileHeader.deepestSlotUsed;
			*outSlot = mPWFileHeader.deepestSlotUsed;
		}
		
		err = [self setHeader:&mPWFileHeader];
	}
	
	[self pwSignal];
	
	return err;
}


// ---------------------------------------------------------------------------
//	getBigNumber
// ---------------------------------------------------------------------------

-(BOOL)getBigNumber:(char **)outBigNumStr
{
    BIGNUM *nonce;
	char *bnStr = NULL;
	
    if ( outBigNumStr == NULL )
		return false;
	*outBigNumStr = NULL;
	
	// make nonce
	nonce = BN_new();
	if ( nonce == NULL )
		return false;
	
	// Generate a random challenge (256-bits)
	BN_rand(nonce, 256, 0, 0);
	bnStr = BN_bn2dec(nonce);
	
	BN_clear_free(nonce);
	
	if ( bnStr == NULL )
		return false;
    
	*outBigNumStr = bnStr;
    
    return true;
}


//----------------------------------------------------------------------------------------------------
//	nextSlot
//
//	Returns: 0 for invalid/error, or the next slot number in the pw file for writing the next entry.
//----------------------------------------------------------------------------------------------------

-(long)nextSlot
{
    long slot = 0;
    int err = -1;
    off_t curpos = 0;
    long readCount;
    PWFileEntry dbEntry;
	
    if ( mPWFileValidated )
    {
		#if DEBUG
		if ( mTestSpillBucket )
			return 2;
		#endif
		
		if ( mPWFileHeader.deepestSlotUsedByThisServer < mPWFileHeader.numberOfSlotsCurrentlyInFile - 1 )
		{
			err = [self getPasswordRec:mPWFileHeader.deepestSlotUsedByThisServer + 1 putItHere:&dbEntry];
			if ( err == 0 &&
				 dbEntry.time == 0 &&
				 dbEntry.rnd == 0 &&
				 dbEntry.sequenceNumber == 0 &&
				 dbEntry.slot == 0 )
			{
				mPWFileHeader.deepestSlotUsedByThisServer++;
				slot = mPWFileHeader.deepestSlotUsedByThisServer;
				if ( mPWFileHeader.deepestSlotUsedByThisServer > mPWFileHeader.deepestSlotUsed )
					mPWFileHeader.deepestSlotUsed = mPWFileHeader.deepestSlotUsedByThisServer;
				
				return slot;
			}
		}
		
        if ( mPWFileHeader.deepestSlotUsed < mPWFileHeader.numberOfSlotsCurrentlyInFile - 1 )
        {
            mPWFileHeader.deepestSlotUsed++;
			mPWFileHeader.deepestSlotUsedByThisServer = mPWFileHeader.deepestSlotUsed;
            slot = mPWFileHeader.deepestSlotUsed;
        }
        else
        {
            // go look in the freelist
            mFreeListFile = fopen( kFreeListFilePath, "r+" );
            if ( mFreeListFile != NULL )
            {
				[self pwWait];
                do
				{
					err = fseek( mFreeListFile, -sizeof(long), SEEK_END );
					if ( err == 0 )
					{
						curpos = ftell( mFreeListFile );
						readCount = fread( &slot, sizeof(long), 1, mFreeListFile );
						if ( readCount == 1 )
						{
							// snip the one we used
							err = ftruncate( fileno(mFreeListFile), curpos );
							if ( err == 0 )
							{
								// double-check that the slot is really free
								err = [self getPasswordRec:slot putItHere:&dbEntry unObfuscate:NO];
								if ( err == 0 && !PWRecIsZero(dbEntry) )
									err = -1;
							}
						}
						else
						{
							err = -1;
							break;
						}
					}
				}
				while ( err == -1 && curpos > 0 );
				[self closeFreeListFile];
				[self pwSignal];
            }
			
            // if freelist is empty, expand the file
            if ( err != 0 || slot == 0 )
                err = [self expandDatabase:kPWFileInitialSlots nextAvailableSlot:&slot];
        }
    }
    
    return slot;
}


//----------------------------------------------------------------------------------------------------
//	addRSAKeys 
//
//	Returns: 0 or -1
//	Adds RSA version 2 keys to the password database header using the ssh-keygen tool
//----------------------------------------------------------------------------------------------------

-(int)addRSAKeys
{
	return [self addRSAKeys:1024];
}


//----------------------------------------------------------------------------------------------------
//	addRSAKeys 
//
//	Returns: 0 or -1
//	Adds RSA version 2 keys to the password database header using the ssh-keygen tool
//----------------------------------------------------------------------------------------------------

-(int)addRSAKeys:(unsigned int)inBitCount
{
	FILE *aFile = NULL;
    int result = -1;
	unsigned char *publicKey = NULL;
	unsigned long publicKeyLen = 0;
	unsigned char *privateKey = NULL;
	unsigned long privateKeyLen = 0;
	char bitCountStr[256] = {0,};
    char tempFileStr[256] = {0,};
    char publicKeyFileStr[256] = {0,};
    struct stat sb = {0};
	char *argv[] = {	"/usr/bin/ssh-keygen",
						"-t", "rsa1",
						"-b", bitCountStr,
						"-f", tempFileStr,
						"-P", "",
						NULL };
	
	// setup command parameters
	sprintf( bitCountStr, "%u", inBitCount );
	strcpy( tempFileStr, kTempKeyTemplate );
	if ( mktemp(tempFileStr) == NULL )
		return -1;
	
	do
	{
		// make the keys
		if ( pwsf_LaunchTask("/usr/bin/ssh-keygen", argv) != EX_OK )
			break;
		
		// stat the key file, get the length
		if ( lstat(tempFileStr, &sb) != 0 )
			break;
		
		if ( !S_ISREG(sb.st_mode) || sb.st_nlink != 1 )
			break;
		
		// add the private key
		aFile = fopen( tempFileStr, "r" );
		if ( aFile != NULL )
		{
			privateKeyLen = (unsigned long)sb.st_size;
			privateKey = (unsigned char *) malloc( privateKeyLen + 1 );
			fread( (char*)privateKey, (unsigned long)sb.st_size, 1, aFile );
			fclose( aFile );
			
			// stat the public key file, get the length
			sprintf( publicKeyFileStr, "%s.pub", tempFileStr );
			if ( lstat(publicKeyFileStr, &sb) != 0 )
				break;
			
			// add the public key
			aFile = fopen( publicKeyFileStr, "r" );
			if ( aFile != NULL )
			{
				publicKeyLen = (unsigned long)sb.st_size;
				publicKey = (unsigned char *) malloc( publicKeyLen + 1 );
				fread( publicKey, (unsigned long)sb.st_size, 1, aFile );
				fclose( aFile );
				
				result = [self addRSAKeys:publicKey publicKeyLen:publicKeyLen privateKey:privateKey privateKeyLen:privateKeyLen];
			}
		}
	}
	while ( 0 );
	
	// we are done with these
	if ( publicKeyFileStr[0] != '\0' )
		unlink( publicKeyFileStr );
	if ( tempFileStr[0] != '\0' )
		unlink( tempFileStr );
	if ( privateKey != NULL ) {
		bzero( privateKey, privateKeyLen );
		free( privateKey );
	}
	if ( publicKey != NULL )
		free( publicKey );
	
    return result;
}


//----------------------------------------------------------------------------------------------------
//	addRSAKeys 
//
//	Returns: 0 or -1
//	Adds RSA version 2 keys to the password database header using the ssh-keygen tool
//----------------------------------------------------------------------------------------------------

-(int)addRSAKeys:(unsigned char *)publicKey
	publicKeyLen:(unsigned long)publicKeyLen
	privateKey:(unsigned char *)privateKey
	privateKeyLen:(unsigned long)privateKeyLen
{
    PWFileHeader ourHeader;
    int result;
	
    if ( privateKeyLen > kPWFileMaxPrivateKeyBytes )
        return -1;
    
    if ( publicKeyLen > kPWFileMaxPublicKeyBytes )
        return -1;
	
    // retrieve the pw database header
    result = [self getHeader:&ourHeader];
    if ( result != 0 && result != -3 )
        return result;
    
    ourHeader.privateKeyLen = privateKeyLen;
	memcpy( ourHeader.privateKey, privateKey, privateKeyLen );
    
    ourHeader.publicKeyLen = publicKeyLen;
	memcpy( ourHeader.publicKey, publicKey, publicKeyLen );
    
    // write it back to the pw database file
	result = [self setHeader:&ourHeader];
    
    // do not leave the private key sitting around in the stack
    bzero(&ourHeader, sizeof(ourHeader));
    
    return result;
}


//----------------------------------------------------------------------------------------------------
//	addGenesisUser 
//
//	Returns: errno
//	Creates an initial Admin user in slot 1 so that the database can be edited.
//	This operation should not be done by the password server. If an existing
//	password file were moved or damaged, it could give a hacker free reign.
//	This method should only be called by a tool on the local CPU that is only run by root.
//	(Setup Assistant, for example).
//----------------------------------------------------------------------------------------------------

-(int)addGenesisUser:(const char *)username password:(const char *)password pwsRec:(PWFileEntry *)outPWRec
{
	PWFileHeader dbHeader;
    PWFileEntry passwordRec;
    int err;
	int err2 = 0;
	
    bzero(&passwordRec, sizeof(passwordRec));
    
    passwordRec.time = 0;
    passwordRec.rnd = 0;
    passwordRec.sequenceNumber = 0;
    passwordRec.slot = 1;
    
    passwordRec.access.isDisabled = false;
    passwordRec.access.isAdminUser = true;
    passwordRec.access.newPasswordRequired = false;
    passwordRec.access.usingHistory = false;
    passwordRec.access.canModifyPasswordforSelf = true;
    passwordRec.access.usingExpirationDate = false;
    passwordRec.access.usingHardExpirationDate = false;
    passwordRec.access.requiresAlpha = false;
    passwordRec.access.requiresNumeric = false;
    passwordRec.access.passwordIsHash = false;
	
    passwordRec.access.maxMinutesOfNonUse = 0;
    passwordRec.access.maxFailedLoginAttempts = 0;
    passwordRec.access.minChars = 0;
    passwordRec.access.maxChars = 0;
    
    strcpy( passwordRec.usernameStr, (username) ? username : "admin" );
    strcpy( passwordRec.passwordStr, (password) ? password : "admin" );
    
	[self pwLock:kOneSecondLockInterval];
	
	err = [self getHeader:&dbHeader];
	if ( err == 0 || err == -3 )
	{
		// mark the slot used if the database is new
		// for established databases, we're just replacing the system administrator
		if ( dbHeader.sequenceNumber == 0 && dbHeader.deepestSlotUsed == 0 )
		{
			dbHeader.sequenceNumber++;
			dbHeader.deepestSlotUsed++;
			dbHeader.deepestSlotUsedByThisServer++;
		}
		
		err = [self setPassword:&passwordRec atSlot:passwordRec.slot];
		if ( err == 0 && outPWRec != NULL )
		{
			memcpy( outPWRec, &passwordRec, sizeof(PWFileEntry) );
		}
		
		err2 = [self setHeader:&dbHeader];
	}
	
	[self pwUnlock];
	
	if ( err == 0 && err2 != 0 )
		err = err2;
	
	return err;
}


//----------------------------------------------------------------------------------------------------
//	addPassword 
//
//	Returns: errno
//	Used to add new passwords
//----------------------------------------------------------------------------------------------------

-(int)addPassword:(PWFileEntry *)passwordRec obfuscate:(BOOL)obfuscate
{
    PWFileHeader ignoreHeader;
    int err, err2;
	
	[self pwLock:kOneSecondLockInterval];
	
    // refresh the header
	// the retrieved header is ignored because the nextSlot() method uses
	// the object's copy of the header in mPWFileHeader.
    err = [self getHeader:&ignoreHeader];
    if ( err != 0 && err != -3 )
		return err;
	
    passwordRec->time = pwsf_getTimeForRef();
    passwordRec->rnd = pwsf_getRandom();
    passwordRec->sequenceNumber = ++mPWFileHeader.sequenceNumber;
    passwordRec->slot = [self nextSlot];
    
    pwsf_getGMTime( (struct tm *)&passwordRec->creationDate );
    memcpy( &passwordRec->lastLogin, &passwordRec->creationDate, sizeof(struct tm) );
    memcpy( &passwordRec->modDateOfPassword, &passwordRec->creationDate, sizeof(struct tm) );
    
    err = [self setPassword:passwordRec atSlot:passwordRec->slot obfuscate:obfuscate setModDate:YES];
	
	// re-write the header to mark the slot used.
	err2 = [self setHeader:&mPWFileHeader];
	
	[self pwUnlock];
	
	if ( err == 0 && err2 != 0 )
		err = err2;
	
	return err;
}


//----------------------------------------------------------------------------------------------------
//	addPassword:atSlot
//
//	Returns: errno
//	Used to add password records from replicas. Fills in the slot if free; otherwise redirects the
//	record to the spill-bucket.
//----------------------------------------------------------------------------------------------------

-(int)addPassword:(PWFileEntry *)passwordRec atSlot:(long)slot obfuscate:(BOOL)obfuscate
{
	return [self addPassword:passwordRec atSlot:slot obfuscate:obfuscate setModDate:YES];
}

-(int)addPassword:(PWFileEntry *)passwordRec atSlot:(long)slot obfuscate:(BOOL)obfuscate setModDate:(BOOL)setModDate
{
	PWFileEntry dbEntry;
	int err;
	bool bGoesInMainDB = false;
	
	// verifying the slot id, do not need to un-obfuscate
	err = [self getPasswordRec:passwordRec->slot putItHere:&dbEntry unObfuscate:NO];
	if ( err != 0 )
		return err;
	
	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		// same user
		bGoesInMainDB = YES;
	}
	else
	if ( dbEntry.time == 0 && dbEntry.rnd == 0 &&
			dbEntry.sequenceNumber == 0 && dbEntry.slot == 0 )
	{
		// slot free
		bGoesInMainDB = YES;
	}
	
	if ( bGoesInMainDB )
	{
		err = [self setPassword:passwordRec atSlot:passwordRec->slot obfuscate:obfuscate setModDate:setModDate];
	}
	else
	{
		[mOverflow saveOverflowRecord:passwordRec obfuscate:obfuscate setModDate:setModDate];
	}
	
	return err;
}


//----------------------------------------------------------------------------------------------------
//	addPasswordAtSlotFast
//
//	Returns: errno
//
//	WARNING: passwordRec is invalid on exit.
//	Used to add password records from replicas. Fills in the slot if free; otherwise redirects the
//	record to the spill-bucket. Similar to the original addPasswordAtSlot() method, but:
//	obfuscate is YES, but the password is not un-obfuscated.
//	setModDate is YES
//----------------------------------------------------------------------------------------------------

-(int)addPasswordFast:(PWFileEntry *)passwordRec atSlot:(unsigned long)slot
{
	PWFileEntry dbEntry;
	int err;
	bool bGoesInMainDB = false;

	// verifying the slot id, do not need to un-obfuscate
	err = [self getPasswordRec:passwordRec->slot putItHere:&dbEntry unObfuscate:NO];
	if ( err != 0 )
		return err;

	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		// same user
		bGoesInMainDB = YES;
	}
	else
	if ( dbEntry.time == 0 && dbEntry.rnd == 0 &&
			dbEntry.sequenceNumber == 0 && dbEntry.slot == 0 )
	{
		// slot free
		bGoesInMainDB = YES;
	}

	if ( bGoesInMainDB )
	{
		err = [self setPasswordFast:passwordRec atSlot:passwordRec->slot];
	}
	else
	{
		[mOverflow saveOverflowRecord:passwordRec obfuscate:YES setModDate:YES];
	}

	return err;
}


//----------------------------------------------------------------------------------------------------
//	initPasswordRecord
//
//	Returns: errno
//	Used to add new passwords
//	Almost identical to addPassword() but doesn't write out the password record.
//	By default, use the addPassword() method. This method is used for optimizations.
//----------------------------------------------------------------------------------------------------

-(int)initPasswordRecord:(PWFileEntry *)passwordRec obfuscate:(BOOL)obfuscate
{
	PWFileHeader ignoreHeader;
	int err, err2;

	[self pwLock:kOneSecondLockInterval];
	
	// refresh the header
	// the retrieved header is ignored because the nextSlot() method uses
	// the object's copy of the header in mPWFileHeader.
	err = [self getHeader:&ignoreHeader];
	if ( err != 0 && err != -3 )
		return err;

	passwordRec->time = pwsf_getTimeForRef();
	passwordRec->rnd = pwsf_getRandom();
	passwordRec->sequenceNumber = ++mPWFileHeader.sequenceNumber;
	passwordRec->slot = [self nextSlot];

	pwsf_getGMTime( (struct tm *)&passwordRec->creationDate );
	memcpy( &passwordRec->lastLogin, &passwordRec->creationDate, sizeof(struct tm) );
	memcpy( &passwordRec->modDateOfPassword, &passwordRec->creationDate, sizeof(struct tm) );

	// re-write the header to mark the slot used.
	err2 = [self setHeader:&mPWFileHeader];

	[self pwUnlock];
	
	if ( err == 0 && err2 != 0 )
		err = err2;

	return err;
}


//------------------------------------------------------------------------------------------------
//	newPasswordForUser
//
//	Returns: errno
//	Similar to AddPassword() but faster for batch operations. 
//	Calls initPasswordRecord() which does not save the password record.
//	By default, use the AddPassword() method.
//------------------------------------------------------------------------------------------------

-(int)newPasswordForUser:(const char *)inUser
		password:(const char *)inPassword
		slotStr:(char *)outPasswordRef
		slotRec:(PWFileEntry *)inOutUserRec
{
	int result;

	if ( inUser == NULL || inPassword == NULL || outPasswordRef == NULL || inOutUserRec == NULL )
		return -1;

	if ( strlen(inPassword) > 511 )
		return kAuthPasswordTooLong;

	bzero( inOutUserRec, sizeof(PWFileEntry) );
	inOutUserRec->access.canModifyPasswordforSelf = true;

	strcpy( inOutUserRec->usernameStr, inUser );
	strcpy( inOutUserRec->passwordStr, inPassword );

	result = [self initPasswordRecord:inOutUserRec obfuscate:YES];

	pwsf_passwordRecRefToString( inOutUserRec, outPasswordRef );

	return result;
}


-(int)getPasswordRec:(long)slot putItHere:(PWFileEntry *)passRec
{
	return [self getPasswordRec:slot putItHere:passRec unObfuscate:YES];
}

-(int)getPasswordRec:(long)slot putItHere:(PWFileEntry *)passRec unObfuscate:(BOOL)unObfuscate
{
    long offset;
    int err = -1;
    ssize_t readCount;
    
    if ( slot > 0 )
    {
        [self pwWait];
        err = [self openPasswordFile:mReadOnlyFileSystem ? "r" : "r+"];
        if ( err == 0 && mPWFile )
        {
            offset = pwsf_slotToOffset( slot );
            
			readCount = pread( mPWFileNO, passRec, sizeof(PWFileEntry), offset );
			if ( readCount != sizeof(PWFileEntry) )
			{
				// failure could indicate a problem with the file descriptor
				// get a new one next time
				[self closePasswordFile];
				
				err = -2;
			}
            else
			{
				pwsf_EndianAdjustPWFileEntry( passRec, 1 );
			}
			
            // recover the password
			if ( unObfuscate && !PWRecIsZero(*passRec) )
				pwsf_DESAutoDecode( passRec->passwordStr );
        }
        [self pwSignal];
    }
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	getValidPasswordRec
//
//	Returns: errno
//	same as getPasswordRec but validates the record's ref numbers.
//----------------------------------------------------------------------------------------------------

-(int)getValidPasswordRec:(PWFileEntry *)passwordRec
{
	return [self getValidPasswordRec:passwordRec fromSpillBucket:NULL unObfuscate:YES];
}

-(int)getValidPasswordRec:(PWFileEntry *)passwordRec fromSpillBucket:(BOOL *)outFromSpillBucket		// default NULL
	unObfuscate:(BOOL)unObfuscate
{
    int err;
    PWFileEntry dbEntry;
    
	if ( outFromSpillBucket != NULL )
		*outFromSpillBucket = false;
	
    err = [self getPasswordRec:passwordRec->slot putItHere:&dbEntry unObfuscate:unObfuscate];
    if ( err != 0 )
		return err;
	
	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		memcpy( passwordRec, &dbEntry, sizeof(PWFileEntry) );
	}
	else
	{
		err = [mOverflow getPasswordRecFromSpillBucket:passwordRec unObfuscate:unObfuscate];
		if ( err == 0 )
		{
			if ( outFromSpillBucket != NULL )
				*outFromSpillBucket = YES;
		}
	}
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	getValidOrZeroPasswordRec
//
//	Returns: errno
//	same as getValidPasswordRec but returns the empty slot and err=0 if the main DB slot is empty.
//----------------------------------------------------------------------------------------------------

-(int)getValidOrZeroPasswordRec:(PWFileEntry *)passwordRec fromSpillBucket:(BOOL *)outFromSpillBucket		// default NULL
	unObfuscate:(BOOL)unObfuscate
{
    int err;
    PWFileEntry dbEntry;
    
	if ( outFromSpillBucket != NULL )
		*outFromSpillBucket = false;
	
    err = [self getPasswordRec:passwordRec->slot putItHere:&dbEntry unObfuscate:unObfuscate];
    if ( err != 0 )
		return err;
	
	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		memcpy( passwordRec, &dbEntry, sizeof(PWFileEntry) );
	}
	else
	{
		err = [mOverflow getPasswordRecFromSpillBucket:passwordRec unObfuscate:unObfuscate];
		if ( err == 0 )
		{
			if ( outFromSpillBucket != NULL )
				*outFromSpillBucket = YES;
		}
		else
		{
			if ( PWRecIsZero(dbEntry) )
			{
				memcpy( passwordRec, &dbEntry, sizeof(PWFileEntry) );
				err = 0;
			}
		}
	}
    
    return err;
}


//------------------------------------------------------------------------------------------------
//	purgeDeadSlots
//
//	RETURNS: an array of purged slot IDs
//
//	deleteWaitSeconds
//	The amount of time to wait after the deletion has been sent to all replicas
//
//	purgeWaitSeconds
//	The amount of time beyond which records are purged no matter what
//------------------------------------------------------------------------------------------------

-(CFArrayRef)purgeDeadSlots:(CFDateRef)beforeDate deleteWait:(long)deleteWaitSeconds purgeWait:(long)purgeWaitSeconds
{
	int err = 0;
	CFMutableArrayRef purgedSlotArray = NULL;
	CFStringRef purgedSlotString = NULL;
	UInt32 index = 0;
	struct tm beforeTime;
	time_t beforeSecs;
	time_t beforePurgeSecs;
	time_t deleteSecs;
	PWFileHeader dbHeader;
	PWFileEntry passRec;
	char idStr[35];
	
	if ( beforeDate == NULL )
		return NULL;
	
	purgedSlotArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( purgedSlotArray == NULL )
		return NULL;
	
	err = [self getHeader:&dbHeader];
	if ( err != 0 && err != -3 ) {
		CFRelease( purgedSlotArray );
		return NULL;
	}
	
	CFRetain(beforeDate);
	pwsf_ConvertCFDateToBSDTime((CFDateRef)beforeDate, &beforeTime);
	beforeSecs = timegm( &beforeTime );
	beforePurgeSecs = beforeSecs; 
	
	// subtract off enough time for post-processing of the sync files
	beforeSecs -= deleteWaitSeconds;
	beforePurgeSecs -= purgeWaitSeconds;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Get obfuscated record.
		err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
		if ( err == 0 && !PWRecIsZero(passRec) && passRec.extraAccess.recordIsDead )
		{
			deleteSecs = timegm( (struct tm *)&passRec.modDateOfPassword );
			if ( difftime(deleteSecs, beforeSecs) < 0 || difftime(deleteSecs, beforePurgeSecs) < 0 )
			{
				if ( [self freeSlot:&passRec deathCertificate:NO] == 0 )
				{
					pwsf_passwordRecRefToString( &passRec, idStr );
					purgedSlotString = CFStringCreateWithCString( kCFAllocatorDefault, idStr, kCFStringEncodingUTF8 );
					if ( purgedSlotString != NULL ) {
						CFArrayAppendValue( purgedSlotArray, purgedSlotString );
						CFRelease( purgedSlotString );
						purgedSlotString = NULL;
					}
				}
			}
		}
	}
	
	[mOverflow doActionForAllOverflowFiles:kOverflowActionPurgeDeadSlots
		principal:NULL userRec:NULL purgeBefore:beforeSecs];
	
	CFRelease(beforeDate);
	
	return purgedSlotArray;
}


-(int)freeSlot:(PWFileEntry *)passwordRec
{
	[self setShouldSyncNow:YES];
	return [self freeSlot:passwordRec deathCertificate:YES];
}


-(int)freeSlot:(PWFileEntry *)passwordRec deathCertificate:(BOOL)useDeathCertificate
{
    int err;
    long slot = passwordRec->slot;
    long writeCount;
    PWFileEntry deleteRec;
	BOOL fromSpillBucket;
	
	// start with a zero record
	bzero( &deleteRec, sizeof(PWFileEntry) );
	
	// keep slot ID for routing in the DB
	deleteRec.time = passwordRec->time;
	deleteRec.rnd = passwordRec->rnd;
	deleteRec.sequenceNumber = passwordRec->sequenceNumber;
	deleteRec.slot = passwordRec->slot;
	
	if ( useDeathCertificate )
	{
		// keep the ID, mark the time of deletion in modDateOfPassword,
		// and mark dead.
		pwsf_getGMTime( (struct tm *)&deleteRec.modDateOfPassword );
		deleteRec.extraAccess.recordIsDead = true;
		deleteRec.changeTransactionID = passwordRec->changeTransactionID;
		
		// we need the kerberos <principal@@realm> information
		// to replicate the deletion
		memcpy( &deleteRec.digest[kPWHashSlotKERBEROS],
				&passwordRec->digest[kPWHashSlotKERBEROS],
				sizeof(PasswordDigest) );
		
		memcpy( &deleteRec.digest[kPWHashSlotKERBEROS_NAME],
				&passwordRec->digest[kPWHashSlotKERBEROS_NAME],
				sizeof(PasswordDigest) );
	}
	
    // original rec must be valid to have permission to clear a slot
    err = [self getValidPasswordRec:passwordRec fromSpillBucket:&fromSpillBucket unObfuscate:NO];
    if ( err == 0 )
    {		
		if ( fromSpillBucket )
		{
			if ( useDeathCertificate )
				err = [mOverflow saveOverflowRecord:&deleteRec obfuscate:YES setModDate:YES];
			else
				err = [mOverflow deleteSlot:&deleteRec];
		}
		else
		{
			err = [self setPassword:&deleteRec atSlot:slot];
			
			if ( ! useDeathCertificate )
			{
				// add the slot number to free list
				[self pwWait];
				mFreeListFile = fopen( kFreeListFilePath, "a+" );
				if ( mFreeListFile != NULL )
				{
					writeCount = fwrite( &slot, sizeof(long), 1, mFreeListFile );
					if ( writeCount != 1 )
					{
						// may have a forgotten slot
						err = -1;
					}
					
					[self closeFreeListFile];
				}
				[self pwSignal];
			}
		}
    }
	else
	{
		// no record, but invalidate the ID just to be sure
		if ( useDeathCertificate )
			[self addPassword:&deleteRec atSlot:slot obfuscate:NO];
	}
	
    return err;
}


-(int)setPassword:(PWFileEntry *)passwordRec atSlot:(unsigned long)slot
{
	return [self setPassword:passwordRec atSlot:slot obfuscate:YES setModDate:YES];
}

-(int)setPassword:(PWFileEntry *)passwordRec
	atSlot:(unsigned long)slot
	obfuscate:(BOOL)obfuscate
	setModDate:(BOOL)setModDate
{
    long offset;
    int err = -1;
    int writeCount;
    unsigned int encodeLen;
	
	if ( mReadOnlyFileSystem )
		return -1;
	
    if ( slot > 0 )
    {
		if ( (unsigned long)slot > mPWFileHeader.numberOfSlotsCurrentlyInFile )
			return -1;
		
		if ( setModDate )
			pwsf_getGMTime( (struct tm *)&passwordRec->modificationDate );
        
        [self pwWait];
        err = [self openPasswordFile:"r+"];
        if ( err == 0 && mPWFile != NULL )
        {
            offset = pwsf_slotToOffset( slot );
            
            err = fseek( mPWFile, offset, SEEK_SET );
            if ( err == 0 )
            {
                //passwordRec->slot = slot;
                encodeLen = strlen(passwordRec->passwordStr);
                encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
                if ( encodeLen > sizeof(passwordRec->passwordStr) )
                    encodeLen = sizeof(passwordRec->passwordStr);
                
				if ( obfuscate )
					pwsf_DESEncode(passwordRec->passwordStr, encodeLen);
				
				pwsf_EndianAdjustPWFileEntry( passwordRec, 0 );
                writeCount = fwrite( passwordRec, sizeof(PWFileEntry), 1, mPWFile );
				pwsf_EndianAdjustPWFileEntry( passwordRec, 1 );
				
                if ( obfuscate )
					pwsf_DESDecode(passwordRec->passwordStr, encodeLen);
				
				if ( writeCount == 1 )
					fflush( mPWFile );
                else
                    err = -1;
            }
		}
        [self pwSignal];
    }

    return err;
}


//----------------------------------------------------------------------------------------------------
//	setPasswordAtSlotFast
//
//	Returns: errno
//
//	WARNING: passwordRec is invalid on exit.
//	Used to write to a specific slot. Similar to the original setPasswordAtSlot() method, but:
//	setModeDate is TRUE
//	obfuscate is TRUE, but the password is not un-obfuscated.
//----------------------------------------------------------------------------------------------------

-(int)setPasswordFast:(PWFileEntry *)passwordRec atSlot:(unsigned long)slot
{
	long offset;
	int err = -1;
	int writeCount;
	unsigned int encodeLen;

	if ( mReadOnlyFileSystem )
		return -1;

	if ( slot > 0 )
	{
		if ( (unsigned long)slot > mPWFileHeader.numberOfSlotsCurrentlyInFile )
			return -1;
		
		pwsf_getGMTime( (struct tm *)&passwordRec->modificationDate );
		
		[self pwWait];
		err = [self openPasswordFile:"r+"];
		if ( err == 0 && mPWFile != NULL )
		{
			offset = pwsf_slotToOffset( slot );
			
			err = fseek( mPWFile, offset, SEEK_SET );
			if ( err == 0 )
			{
				//passwordRec->slot = slot;
				encodeLen = strlen(passwordRec->passwordStr);
				encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
				if ( encodeLen > sizeof(passwordRec->passwordStr) )
					encodeLen = sizeof(passwordRec->passwordStr);
								
				pwsf_DESEncode(passwordRec->passwordStr, encodeLen);
				
				pwsf_EndianAdjustPWFileEntry( passwordRec, 0 );
                writeCount = fwrite( passwordRec, sizeof(PWFileEntry), 1, mPWFile );
				pwsf_EndianAdjustPWFileEntry( passwordRec, 1 );
				
				if ( writeCount == 1 )
					fflush( mPWFile );
				else
					err = -1;
			}
		}
		[self pwSignal];
	}

	return err;
}


//------------------------------------------------------------------------------------------------
//	addHashes
//
//	inRealm				 ->		the realm to use for the DIGEST-MD5 hash
//	inOutPasswordRec	<->		in clear-text, out hash values
//	Takes the clear-text password and adds the hashes for auth methods
//------------------------------------------------------------------------------------------------

-(void)addHashes:(const char *)inRealm addNT:(BOOL)inAddNT addLM:(BOOL)inAddLM pwsRec:(PWFileEntry *)inOutPasswordRec
{
	pwsf_AddHashesToPWRecord( inRealm, (bool)inAddNT, (bool)inAddLM, inOutPasswordRec );
}


#pragma mark -
#pragma mark ADDITIONAL DATA FILE
#pragma mark -

//------------------------------------------------------------------------------------------------
//	addGroup:toAdmin:
//
//	Returns: YES if the group is added or present, NO if there is an error or the state is unknown.
//
//	The plist file of groups is saved to disk by this method; the slot record is not. It is the
//	caller's responsibility to write the slot.
//------------------------------------------------------------------------------------------------

-(BOOL)addGroup:(uuid_t)groupUUID toAdmin:(PWFileEntry *)inOutAdminRec
{
	BOOL result = NO;
	CFMutableDictionaryRef groupDict = NULL;
	CFMutableArrayRef groupArray = NULL;
	CFStringRef uuidString = NULL;
	char filePath[PATH_MAX] = {0};
	int err;
	uuid_t groupList[2];
	
	if (inOutAdminRec == NULL || inOutAdminRec->access.isAdminUser == 0)
		return NO;
	
	switch( inOutAdminRec->admingroup.list_type )
	{
		case kPWGroupNotSet:
			inOutAdminRec->admingroup.list_type = kPWGroupInSlot;
			memcpy( inOutAdminRec->admingroup.group_uuid, groupUUID, sizeof(uuid_t) );
			result = YES;
			break;
		
		case kPWGroupInSlot:
			if ( memcmp(&(inOutAdminRec->admingroup.group_uuid), groupUUID, sizeof(uuid_t)) != 0 )
			{
				memcpy( &groupList[0], &(inOutAdminRec->admingroup.group_uuid), sizeof(uuid_t) );
				memcpy( &groupList[1], (uuid_t *)groupUUID, sizeof(uuid_t) );
				groupDict = pwsf_CreateAdditionalDataDictionaryWithUUIDList( 2, (uuid_t *)groupList );
				if ( groupDict == NULL )
					break;
				
				[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutAdminRec];
				if ( pwsf_savexml(filePath, groupDict) == 0 )
				{
					// set record data
					inOutAdminRec->admingroup.list_type = kPWGroupInFile;
					bzero( &inOutAdminRec->admingroup.group_uuid, sizeof(uuid_t) );
					result = YES;
				}
			}
			break;
		
		case kPWGroupInFile:
			[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutAdminRec];
			err = pwsf_loadxml( filePath, &groupDict );
			if ( err == 0 )
			{
				groupArray = (CFMutableArrayRef) CFDictionaryGetValue( groupDict, CFSTR(kPWKey_ScopeOfAuthority) );
				if ( groupArray == NULL )
					break;
	
				uuidString = pwsf_UUIDToString( groupUUID );
				if ( uuidString == NULL )
					break;
				
				if ( !CFArrayContainsValue(groupArray, CFRangeMake(0, CFArrayGetCount(groupArray)), uuidString) )
					CFArrayAppendValue( groupArray, uuidString );
				
				// It's important to set groupArray to NULL.
				// The memory belongs to groupDict which is also released.
				groupArray = NULL;
				
				pwsf_savexml( filePath, groupDict );
			}
			else
			{
				groupDict = pwsf_CreateAdditionalDataDictionaryWithUUIDList( 1, (uuid_t *)&groupUUID );
			}
			break;
	}
	
	// clean up
	if ( groupDict != NULL )
		CFRelease( groupDict );
	if ( groupArray != NULL )
		CFRelease( groupArray );
	if ( uuidString != NULL )
		CFRelease( uuidString );
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	removeGroup:fromAdmin:
//
//	Returns: YES if the group is removed or not present, NO if there was an error and the state
//			 is unknown.
//
//	The plist file of groups is saved to disk by this method; the slot record is not. It is the
//	caller's responsibility to write the slot.
//------------------------------------------------------------------------------------------------

-(BOOL)removeGroup:(uuid_t)groupUUID fromAdmin:(PWFileEntry *)inOutAdminRec
{
	BOOL result = NO;
	CFMutableDictionaryRef groupDict = NULL;
	CFMutableArrayRef groupArray = NULL;
	CFStringRef targetUUIDString = NULL;
	CFStringRef curUUIDString = NULL;
	char filePath[PATH_MAX] = {0};
	CFIndex idx = 0;
	CFIndex groupCount = 0;
	int err = 0;
	
	if (inOutAdminRec == NULL)
		return NO;
	
	switch( inOutAdminRec->admingroup.list_type )
	{
		case kPWGroupNotSet:
			result = YES;
			break;
		
		case kPWGroupInSlot:
			targetUUIDString = pwsf_UUIDToString( groupUUID );
			if ( targetUUIDString == NULL )
				break;
			
			curUUIDString = pwsf_UUIDToString( inOutAdminRec->admingroup.group_uuid );
			if ( curUUIDString == NULL )
				break;
			
			if ( CFStringCompare(targetUUIDString, curUUIDString, 0) == kCFCompareEqualTo )
			{
				inOutAdminRec->admingroup.list_type = kPWGroupNotSet;
				bzero( &inOutAdminRec->admingroup.group_uuid, sizeof(uuid_t) );
				result = YES;
			}
			break;
		
		case kPWGroupInFile:
			[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutAdminRec];
			err = pwsf_loadxml( filePath, &groupDict );
			if ( err == 0 )
			{
				groupArray = (CFMutableArrayRef) CFDictionaryGetValue( groupDict, CFSTR(kPWKey_ScopeOfAuthority) );
				if ( groupArray == NULL )
					break;
				
				targetUUIDString = pwsf_UUIDToString( groupUUID );
				if ( targetUUIDString == NULL )
					break;
				
				groupCount = CFArrayGetCount( groupArray );
				for ( idx = 0; idx < groupCount; idx++ )
				{
					curUUIDString = (CFStringRef) CFArrayGetValueAtIndex( groupArray, idx );
					if ( curUUIDString != NULL && CFStringCompare(targetUUIDString, curUUIDString, 0) == kCFCompareEqualTo )
					{
						CFArrayRemoveValueAtIndex( groupArray, idx );
						if ( pwsf_savexml(filePath, groupDict) == 0 )
							result = YES;
						break;
					}
				}
			}
			break;
	}
	
	// clean up
	if ( groupDict != NULL )
		CFRelease( groupDict );
	if ( targetUUIDString != NULL )
		CFRelease( targetUUIDString );
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	removeAllGroupsFromAdmin
//
//	Returns: YES if all groups are removed, NO if there was an error and the state is unknown.
//
//	The plist file of groups is saved to disk by this method; the slot record is not. It is the
//	caller's responsibility to write the slot.
//------------------------------------------------------------------------------------------------

-(BOOL)removeAllGroupsFromAdmin:(PWFileEntry *)inOutAdminRec
{
	BOOL result = NO;
	char filePath[PATH_MAX] = {0};
	
	if (inOutAdminRec == NULL)
		return NO;
	
	switch( inOutAdminRec->admingroup.list_type )
	{
		case kPWGroupNotSet:
			result = YES;
			break;
		
		case kPWGroupInSlot:
			inOutAdminRec->admingroup.list_type = kPWGroupNotSet;
			bzero( &inOutAdminRec->admingroup.group_uuid, sizeof(uuid_t) );
			result = YES;
			break;
		
		case kPWGroupInFile:
			[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutAdminRec];
			[self removeKey:CFSTR(kPWKey_ScopeOfAuthority) fromAdditionalDataFile:filePath];
			inOutAdminRec->admingroup.list_type = kPWGroupNotSet;
			bzero( &inOutAdminRec->admingroup.group_uuid, sizeof(uuid_t) );
			result = YES;
			break;
	}
	
	return result;	
}


//------------------------------------------------------------------------------------------------
//	addOwners
//
//	Returns: YES if successful
//------------------------------------------------------------------------------------------------

-(BOOL)addOwners:(const char *)ownerList toEntry:(PWFileEntry *)inOutRec
{
	CFMutableDictionaryRef additionalDataDict = NULL;
	CFMutableArrayRef ownerArray = NULL;
	CFStringRef slotListString = NULL;
	CFArrayRef slotListArray = NULL;
	CFStringRef slotString = NULL;
	CFIndex slotListArrayIndex = 0;
	CFIndex slotListArrayCount = 0;
	CFRange ownerRange;
	int err = 0;
	char filePath[PATH_MAX] = {0};
	
	[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutRec];
	err = pwsf_loadxml( filePath, &additionalDataDict );
	if ( err == 0 && additionalDataDict != NULL )
	{
		slotListString = CFStringCreateWithCString( kCFAllocatorDefault, ownerList, kCFStringEncodingUTF8 );
		slotListArray = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, slotListString, CFSTR(",") );
		CFRelease( slotListString );
		
		if ( CFDictionaryGetValueIfPresent(additionalDataDict, CFSTR(kPWKey_ComputerAccountOwnerList), (const void **)&ownerArray) )
		{
			ownerRange = CFRangeMake( 0, CFArrayGetCount(ownerArray) );
			slotListArrayCount = CFArrayGetCount( slotListArray );
			for ( slotListArrayIndex = 0; slotListArrayIndex < slotListArrayCount; slotListArrayIndex++ )
			{
				slotString = (CFStringRef) CFArrayGetValueAtIndex( slotListArray, slotListArrayIndex );
				if ( !CFArrayContainsValue(ownerArray, ownerRange, slotString) )
					CFArrayAppendValue( ownerArray, slotString );
			}
		}
		else
		{
			CFDictionarySetValue( additionalDataDict, CFSTR(kPWKey_ComputerAccountOwnerList), slotListArray );
		}
		
		CFRelease( slotListArray );
	}
	else
	{
		additionalDataDict = pwsf_CreateAdditionalDataDictionaryWithOwners( ownerList );
	}
	if ( additionalDataDict != NULL ) {
		err = pwsf_savexml( filePath, additionalDataDict );
		CFRelease( additionalDataDict );
	}
	
	return (err == 0);
}


//------------------------------------------------------------------------------------------------
//	removeAllOwnersFromEntry
//
//	Returns: YES if successful
//------------------------------------------------------------------------------------------------

-(BOOL)removeAllOwnersFromEntry:(PWFileEntry *)inOutRec
{
	char filePath[PATH_MAX] = {0};
	
	[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inOutRec];
	return [self removeKey:CFSTR(kPWKey_ComputerAccountOwnerList) fromAdditionalDataFile:filePath];
}


//------------------------------------------------------------------------------------------------
//	isOwner
//
//	Returns: YES if successful
//------------------------------------------------------------------------------------------------

-(BOOL)isOwner:(const char *)user forEntry:(PWFileEntry *)inRec
{
	BOOL owner = NO;
	int err = 0;
	CFMutableDictionaryRef additionalDataDict = NULL;
	CFArrayRef ownerArray = NULL;
	CFStringRef userString = NULL;
	char filePath[PATH_MAX] = {0};
	
	[self setFilePath:filePath maxPathSize:sizeof(filePath) forEntry:inRec];
	err = pwsf_loadxml( filePath, &additionalDataDict );
	if ( err == 0 &&
		 additionalDataDict != NULL &&
		 CFDictionaryGetValueIfPresent(additionalDataDict, CFSTR(kPWKey_ComputerAccountOwnerList), (const void **)&ownerArray) )
	{
		userString = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, user, kCFStringEncodingUTF8, kCFAllocatorNull );
		if ( userString != NULL ) {
			owner = CFArrayContainsValue( ownerArray, CFRangeMake(0, CFArrayGetCount(ownerArray)), userString );
			CFRelease( userString );
		}
	}
	if ( additionalDataDict != NULL )
		CFRelease( additionalDataDict );
	
	return owner;
}


//------------------------------------------------------------------------------------------------
//	filePathForEntry
//------------------------------------------------------------------------------------------------

-(void)setFilePath:(char *)inOutPath maxPathSize:(size_t)inPathMax forEntry:(PWFileEntry *)inRec
{
	char adminID[35] = {0};
	
	pwsf_passwordRecRefToString( inRec, adminID );
	snprintf( inOutPath, inPathMax, "%s/%s/%s.plist", mDirPathStr, kPWAuxDirName, adminID );
}


//------------------------------------------------------------------------------------------------
//	removeKeyFromAdditionalDataFile
//
//	Returns: YES if successful
//------------------------------------------------------------------------------------------------

-(BOOL)removeKey:(CFStringRef)inKey fromAdditionalDataFile:(const char *)inFilePath
{
	CFMutableDictionaryRef additionalDataDict = NULL;
	int err = 0;
	
	err = pwsf_loadxml( inFilePath, &additionalDataDict );
	if ( err != 0 )
		return YES;		// no file, no key
	
	CFDictionaryRemoveValue( additionalDataDict, inKey );
	
	err = (CFDictionaryGetCount(additionalDataDict) > 0) ? pwsf_savexml( inFilePath, additionalDataDict ) : unlink( inFilePath );
	
	if ( additionalDataDict != NULL )
		CFRelease( additionalDataDict );
	
	return (err == 0);
}


#pragma mark -
#pragma mark REVERSE LOOKUPS
#pragma mark -

//------------------------------------------------------------------------------------------------
//	getUserIDFromName
//
//	Returns: Boolean (1==found, 0=not found)
//------------------------------------------------------------------------------------------------

-(int)getUserIDFromName:(const char *)inName anyUser:(BOOL)inAnyUser maxBuffSize:(long)inMaxBuffSize pwsID:(char *)outID
{
	PWFileHeader dbHeader;
	int result = 0;
	int err = 0;
	UInt32 index;
	PWFileEntry passRec;
	char theAdminID[256];
    long buffRemaining = inMaxBuffSize;
    long len;
    BOOL ignore;
	
    if ( outID == NULL || buffRemaining < 1 )
        return 0;
    
    *outID = '\0';
    buffRemaining--;
    
	if ( [self shouldTryLDAP] && [self getAccountIDFromLDAP:inName slotID:outID] )
	{
		if ( inAnyUser )
		{
			return 1;
		}
		else
		{
			// verify we got an admin
			pwsf_stringToPasswordRecRef( outID, &passRec );
			result = [self getValidPasswordRec:&passRec fromSpillBucket:&ignore unObfuscate:NO];
			if ( result == 0 && passRec.access.isAdminUser )
				return 1;
		}
	}
	
	err = [self getHeader:&dbHeader cachedCopyOK:NO];
	if ( err != 0 && err != -3 )
		return result;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
		if ( err != 0 )
			break;
		
		if ( (inAnyUser || passRec.access.isAdminUser) && !passRec.access.isDisabled && strcmp( inName, passRec.usernameStr ) == 0 )
		{
            if ( result == 1 )
            {
                if ( buffRemaining < 1 )
                    break;
                
                strcat( outID, ";" );
                buffRemaining--;
            }
            
			pwsf_passwordRecRefToString( &passRec, theAdminID );
            len = strlen( theAdminID );
            
            if ( buffRemaining <= len )
                break;
            
            strcat( outID, theAdminID );
            buffRemaining -= len;
            
            result = 1;
		}
	}
		
	return result;
}


//------------------------------------------------------------------------------------------------
//	getUserIDFromPrincipal
//
//	Returns: Boolean (1==found, 0=not found)
//
//  If found, inOutUserRec contains the database record with the password field obfuscated.
//------------------------------------------------------------------------------------------------

-(int)getUserRecordFromPrincipal:(const char *)inPrincipal record:(PWFileEntry *)inOutUserRec
{
	PWFileHeader dbHeader;
	int err = 0;
	unsigned long index;
	PWFileEntry passRec;
	char *thePrincDomain = NULL;
    long len;
	BOOL ignore;
	char thePrincName[256];
	char slotID[35];
        
	// break principal into name and domain
	thePrincDomain = strchr( inPrincipal, '@' );
	if ( thePrincDomain == NULL )
		return 0;
	
	// must have a principal name
	len = thePrincDomain - inPrincipal;
	if ( len == 0 )
		return 0;
	
	// advance past the '@'
	thePrincDomain++;
	
	// save the name as a c-str
	strlcpy( thePrincName, inPrincipal, len + 1 );
	
	// Question: What about subdomains (dot in the principal name)?
	
	if ( [self shouldTryLDAP] )
	{
		err = [self getAccountIDFromLDAP:thePrincName slotID:slotID];
		if ( err == 1 )
		{
			pwsf_stringToPasswordRecRef( slotID, inOutUserRec );
			err = [self getValidPasswordRec:inOutUserRec fromSpillBucket:&ignore unObfuscate:NO];
			if ( err == 0 )
				return 1;
		}
	}
	
	err = [self getHeader:&dbHeader cachedCopyOK:YES];
	if ( err != 0 && err != -3 )
		return 0;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
		if ( err != 0 )
			break;
		
		if ( strcmp( thePrincName, pwsf_GetPrincName(&passRec) ) == 0 )
		{
            memcpy( inOutUserRec, &passRec, sizeof(PWFileEntry) );
			return 1;
		}
	}
	
	return 0;
}


//------------------------------------------------------------------------------------------------
//	getLDAPSearchBase
//------------------------------------------------------------------------------------------------

-(char *)getLDAPSearchBase
{
	if ( mSearchBase == NULL )
	{
		char buffer[1024];
		char *argv[] = {"/usr/bin/ldapsearch", "-LLL", "-x", "-z", "1", "-b", "", "-s", "base", "namingContexts", NULL};
		int result = pwsf_LaunchTaskWithIO("/usr/bin/ldapsearch", argv, NULL, buffer, sizeof(buffer), NULL);
		if ( result == 0 )
		{
			char *tptr = strstr( buffer, "namingContexts: " );
			if ( tptr != NULL )
			{
				tptr += sizeof("namingContexts: ") - 1;
				mSearchBase = strdup( tptr );
				tptr = strchr( mSearchBase, '\n' );
				if ( tptr != NULL )
					*tptr = '\0';
			}
		}
	}
	
	return mSearchBase;
}


//------------------------------------------------------------------------------------------------
//	getAccountIDFromLDAP
//
//	RETURNS: 1=found, 0=not found
//------------------------------------------------------------------------------------------------

-(int)getAccountIDFromLDAP:(const char *)inUID slotID:(char *)outID
{
	char *searchBase = NULL;
	char *tptr = NULL;
	char filter[256];
	int found = 0;
	
	if ( inUID == NULL || outID == NULL )
		return 0;
	*outID = '\0';
	
	searchBase = [self getLDAPSearchBase];
	if ( searchBase != NULL )
	{
		int result = 0;
		char buffer[1024];
		char *argv[] = {"/usr/bin/ldapsearch", "-LLL", "-x", "-z", "1", "-b", searchBase, filter, "authAuthority", NULL};
		snprintf( filter, sizeof(filter), "(|(uid=%s)(cn=%s))", inUID, inUID );		
		result = pwsf_LaunchTaskWithIO("/usr/bin/ldapsearch", argv, NULL, buffer, sizeof(buffer), NULL);
		if ( result == 0 )
		{
			tptr = strstr( buffer, "authAuthority: ;ApplePasswordServer;" );
			if ( tptr != NULL )
			{
				tptr += sizeof("authAuthority: ;ApplePasswordServer;") - 1;
				*(tptr + 34) = '\0';
				strcpy( outID, tptr );
				return 1;
			}
		}
	}
	
	return found;
}


// --------------------------------------------------------------------------------
//	shouldTryLDAP
// --------------------------------------------------------------------------------

-(int)shouldTryLDAP
{
	PWFileHeader dbHeader;
	
	return ( [self getHeader:&dbHeader cachedCopyOK:YES] == 0 && dbHeader.deepestSlotUsed > 100 );
}


#pragma mark -
#pragma mark PASSWORD UTILS
#pragma mark -

//------------------------------------------------------------------------------------------------
//	requireNewPasswordForAllAccounts
//------------------------------------------------------------------------------------------------

-(void)requireNewPasswordForAllAccounts:(BOOL)inRequireNew
{
	PWFileHeader dbHeader;
	int err = 0;
	unsigned long index;
	PWFileEntry passRec;
        
	err = [self getHeader:&dbHeader cachedCopyOK:YES];
	if ( err != 0 && err != -3 )
		return;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
		if ( err != 0 )
			continue;
		
		passRec.access.newPasswordRequired = inRequireNew;
		
		[self setPassword:&passRec atSlot:index obfuscate:NO setModDate:YES];
	}
}


//------------------------------------------------------------------------------------------------
//	kerberizeOrRequireNewPasswordForAllAccounts
//------------------------------------------------------------------------------------------------

-(void)kerberizeOrRequireNewPasswordForAllAccounts
{
	PWFileHeader dbHeader;
	int err = 0;
	UInt32 index;
	PWFileEntry passRec;
	
	err = [self getHeader:&dbHeader cachedCopyOK:YES];
	if ( err != 0 && err != -3 )
		return;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Get obfuscated record. Performance is better for the passwordIsHash case
		err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
		if ( err != 0 || PWRecIsZero(passRec) || passRec.extraAccess.recordIsDead )
			continue;
		
		if ( passRec.access.passwordIsHash )
		{
			// require new
			passRec.access.newPasswordRequired = 1;
			[self setPassword:&passRec atSlot:index obfuscate:NO setModDate:YES];
		}
		else
		{
			// kerberize
			
			// un-obfuscate the password
			pwsf_DESAutoDecode( passRec.passwordStr );
			
			if ( pwsf_AddPrincipal(pwsf_GetPrincName(&passRec), passRec.passwordStr, passRec.digest[kPWHashSlotKERBEROS].digest,
				 sizeof(passRec.digest[kPWHashSlotKERBEROS].digest)) == 0 )
			{
				if ( passRec.digest[kPWHashSlotKERBEROS].digest[0] != '\0' )
				{
					strcpy( passRec.digest[kPWHashSlotKERBEROS].method, "KerberosRealmName" );
					// if the method field isn't assigned, then the pwsf_GetPrincName() function returns username
					strlcpy( passRec.digest[kPWHashSlotKERBEROS_NAME].digest, pwsf_GetPrincName(&passRec),
								sizeof(passRec.digest[kPWHashSlotKERBEROS_NAME].digest) );
					strcpy( passRec.digest[kPWHashSlotKERBEROS_NAME].method, "KerberosPrincName" );
				}
				else
				{
					passRec.digest[kPWHashSlotKERBEROS].method[0] = 0;
				}
				[self setPassword:&passRec atSlot:index];
			}
		}
		bzero( &passRec, sizeof(passRec) );
	}
	
	[mOverflow kerberizeOrNewPassword];
}


//------------------------------------------------------------------------------------------------
//	addPasswordForUser
//------------------------------------------------------------------------------------------------

-(int)addPasswordForUser:(const char *)inUser password:(const char *)inPassword pwsID:(char *)outPasswordRef
{
    int result;
    PWFileEntry anEntry;
    char refStr[256];
    
	if ( strlen(inPassword) > sizeof(anEntry.passwordStr) - 1 )
		return kAuthPasswordTooLong;
	
	bzero( &anEntry, sizeof(anEntry) );
	
    anEntry.access.isDisabled = NO;
    anEntry.access.isAdminUser = NO;
    anEntry.access.newPasswordRequired = NO;
    anEntry.access.usingHistory = NO;
    anEntry.access.canModifyPasswordforSelf = YES;
    anEntry.access.usingExpirationDate = NO;
    anEntry.access.usingHardExpirationDate = NO;
    anEntry.access.requiresAlpha = NO;
    anEntry.access.requiresNumeric = NO;
    anEntry.access.passwordIsHash = NO;
	
    anEntry.access.maxMinutesOfNonUse = 0;
    anEntry.access.maxFailedLoginAttempts = 0;
    anEntry.access.minChars = 0;
    anEntry.access.maxChars = 0;
    
    strcpy( anEntry.usernameStr, inUser );
    strlcpy( anEntry.passwordStr, inPassword, sizeof(anEntry.passwordStr) );
 
	result = [self addPassword:&anEntry obfuscate:YES];
	
    pwsf_passwordRecRefToString( &anEntry, refStr );
    strcpy( outPasswordRef, refStr );
    
    return result;
}


//------------------------------------------------------------------------------------------------
//	NewPasswordSlot
//
//	Returns: errno
//	Similar to AddPassword() but faster for batch operations. 
//	Calls initPasswordRecord() which does not save the password record.
//	By default, use the AddPassword() method.
//------------------------------------------------------------------------------------------------

-(int)newPasswordSlot:(const char *)inUser
	password:(const char *)inPassword
	pwsID:(char *)outPasswordRef
	pwsRec:(PWFileEntry *)inOutUserRec
{
	int result;

	if ( inUser == NULL || inPassword == NULL || outPasswordRef == NULL || inOutUserRec == NULL )
		return -1;

	if ( strlen(inPassword) > 511 )
		return kAuthPasswordTooLong;
	
	bzero( inOutUserRec, sizeof(PWFileEntry) );
	inOutUserRec->access.canModifyPasswordforSelf = YES;
	
	strcpy( inOutUserRec->usernameStr, inUser );
	strcpy( inOutUserRec->passwordStr, inPassword );
	
	result = [self initPasswordRecord:inOutUserRec obfuscate:YES];
	
	pwsf_passwordRecRefToString( inOutUserRec, outPasswordRef );

	return result;
}

#pragma mark -
#pragma mark REPLICATION
#pragma mark -

//------------------------------------------------------------------------------------------------
//	getSyncTime:fromSyncFile
//
//	Returns: 0 == success, -1 == fail
//
//	This method gets the official sync time (watermark) for a data file
//------------------------------------------------------------------------------------------------

-(int)getSyncTime:(time_t *)outSyncTime fromSyncFile:(const char *)inSyncFile 
{
	int err;
	FILE *syncFile;
	int readCount;
	unsigned long remoteFileLen;
	struct stat sb;
	time_t syncTime = 0;
	
	// sanity
    if ( inSyncFile == NULL || outSyncTime == NULL )
		return -1;
	
	*outSyncTime = 0;
	
	// get file len
    remoteFileLen = 0;
    err = lstat( inSyncFile, &sb );
    if ( err == 0 )
        remoteFileLen = sb.st_size;
    if ( remoteFileLen < sizeof(PWFileHeader) )
		return -1;
	
	// open the sync file
	syncFile = fopen( inSyncFile, "r" );
    if ( syncFile == NULL )
		return -1;
	
	// read opposing sync time
	readCount = fread( &syncTime, sizeof(syncTime), 1, syncFile );
	if ( readCount == 1 )
		*outSyncTime = syncTime;
	
	fclose( syncFile );
	
	return 0;
}


//------------------------------------------------------------------------------------------------
//	makeSyncFile
//
//  Returns: 0 = success, -1 fail
//
//  <inKerberosRecordLimit>		0=no limit; otherwise, database size beyond which kerberos sync
//								is queued
//------------------------------------------------------------------------------------------------

-(int)makeSyncFile:(const char *)inFileName
	afterDate:(time_t)inAfterDate
	timeSkew:(long)inTimeSkew
	numRecordsUpdated:(long *)outNumRecordsUpdated
	kerberosRecordLimit:(unsigned int)inKerberosRecordLimit
	kerberosOmitted:(BOOL *)outKerberosOmitted
	passwordServerOmitted:(BOOL *)outPasswordServerOmitted
{
	PWFileHeader dbHeader;
	int result = 0;
	int err;
	UInt32 index;
	PWFileEntry passRec;
	PWSFKerberosPrincipal* kerberosRec;
    time_t theTime;
	FILE *syncFile;
	int writeCount;
	int zeroLen = 0;
	PWSFKerberosPrincipalList kerbList;
	bool addKerberosRecords = true;
	
	// sanity
    if ( inFileName == NULL )
		return -1;

	if ( outNumRecordsUpdated != NULL )
		*outNumRecordsUpdated = 0;
	if ( outKerberosOmitted != NULL )
		*outKerberosOmitted = false;
	if ( outPasswordServerOmitted != NULL )
		*outPasswordServerOmitted = false;
		
	// create sync file
	syncFile = fopen( inFileName, "w+" );
    if ( syncFile == NULL )
		return -1;
	
	err = chmod( inFileName, S_IRUSR | S_IWUSR );
	
	// load/copy header
	do
	{
		writeCount = fwrite( &inAfterDate, sizeof(inAfterDate), 1, syncFile );
		if ( writeCount != 1 ) {
			result = -1;
			break;
		}
		
		err = [self getHeader:&dbHeader];
		if ( err != 0 && err != -3 ) {
			result = err;
			break;
		}
		
		writeCount = fwrite( &dbHeader, sizeof(dbHeader), 1, syncFile );
		if ( writeCount != 1 ) {
			result = -1;
			break;
		}
				
		if ( inKerberosRecordLimit > 0 && dbHeader.deepestSlotUsed > inKerberosRecordLimit )
			addKerberosRecords = false;
		
		if ( outPasswordServerOmitted != NULL && dbHeader.deepestSlotUsed > 4176 )
		{
			*outPasswordServerOmitted = true;
		}
		else
		{
			if ( addKerberosRecords )
				kerbList.ReadAllPrincipalsFromDB( inAfterDate );
			
			// copy records after the sync date
			for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
			{				
				err = [self getPasswordRec:index putItHere:&passRec unObfuscate:NO];
				if ( err != 0 ) {
					result = err;
					break;
				}
				if ( PWRecIsZero(passRec) )
					continue;
				
				// adjust time skew for comparison purposes. The record itself is
				// adjusted on the processing side.
				theTime = timegm( (struct tm *)&passRec.modificationDate ) + inTimeSkew;
				if ( theTime >= inAfterDate )
				{
					// replicate the identity to avoid collisions, but blank out any useful data
					if ( passRec.extraAccess.doNotReplicate )
					{
						bzero( &passRec.passwordStr, sizeof(passRec.passwordStr) );
						bzero( &passRec.digest[0], sizeof(PasswordDigest) * kPWFileMaxDigests );
						bzero( &passRec.userdata, sizeof(passRec.userdata) );
						bzero( &passRec.extraAccess.userkey, sizeof(passRec.extraAccess.userkey) );
						
						// set passwordIsHash to guarantee that the plain text field is useless
						passRec.access.passwordIsHash = 1;
					}
					
					writeCount = fwrite( &passRec, sizeof(passRec), 1, syncFile );
					if ( writeCount != 1 ) {
						result = -1;
						break;
					}
										
					kerberosRec = NULL;
					if ( addKerberosRecords && passRec.digest[kPWHashSlotKERBEROS].digest[0] != '\0' )
					{
						char principalName[600] = {0,};
						
						// kerberos records only need to be sent if the password changed
						theTime = timegm( (struct tm *)&passRec.modDateOfPassword ) + inTimeSkew;
						if ( theTime >= inAfterDate )
						{
							strcpy(principalName, pwsf_GetPrincName(&passRec));
							strcat(principalName, "@");
							strcat(principalName, passRec.digest[kPWHashSlotKERBEROS].digest);
							kerberosRec = kerbList.GetPrincipalByName(principalName);
						}
					}
					if (kerberosRec != NULL) {
						writeCount = kerberosRec->WritePrincipalToFile(syncFile);
						delete kerberosRec;
					}
					else {
						writeCount = fwrite( &zeroLen, sizeof(zeroLen), 1, syncFile );
					}
					if ( writeCount != 1 ) {
						result = -1;
						break;
					}
					
					if ( outNumRecordsUpdated != NULL )
						(*outNumRecordsUpdated)++;
				}
			}
			
			// look in the spill-bucket
			[mOverflow addOverflowToSyncFile:syncFile afterDate:inAfterDate timeSkew:inTimeSkew numUpdated:outNumRecordsUpdated];
			
			if ( addKerberosRecords )
			{
				// Now add remaining Kerberos records since modDate, since these records don't have
				// a corresponding password server record, just put an invalid slot number there
				bzero(&passRec, sizeof(passRec));
				passRec.slot = (UInt32)-1;
				
				index = 0;
				kerberosRec = kerbList.GetPrincipalByIndex(index++);
				while (kerberosRec != NULL)
				{
					writeCount = fwrite( &passRec, sizeof(passRec), 1, syncFile );
					if ( writeCount != 1 ) {
						result = -1;
						break;
					}
					writeCount = kerberosRec->WritePrincipalToFile(syncFile);
					if ( writeCount != 1 ) {
						result = -1;
						break;
					}
					kerberosRec = kerbList.GetPrincipalByIndex(index++);
				}
			}
			
			// TODO:add additional-data files
			
		}
	}
	while (0);
	
	fclose( syncFile );
	if ( result != 0 )
		unlink( inFileName );
	
	bzero( &dbHeader, sizeof(dbHeader) );
	bzero( &passRec, sizeof(passRec) );
	
	if ( outKerberosOmitted != NULL )
		*outKerberosOmitted = (!addKerberosRecords);
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	processSyncFile
//
//	Returns: 0 == success, -1 == fail, -2 == incompatible databases, -3 == db file busy
//
//	This method processes the records from a remote parent or replica. Any local records
//	believed to be newer than the remote ones are preserved.
//------------------------------------------------------------------------------------------------

-(int)processSyncFile:(const char *)inSyncFile
	timeSkew:(long)inTimeSkew
	numAccepted:(long *)outNumAccepted
	numTrumped:(long *)outNumTrumped
{
	int result = 0;
	int err;
	FILE *syncFile;
	time_t localTime, remoteTime;
	PWFileHeader localHeader, remoteHeader;
	PWFileEntry localRec, remoteRec;
    int readCount;
	unsigned long remoteFileLen = 0;
	struct stat sb;
	time_t syncTime = 0;
	BOOL bFromSpillBucket;
	bool bNeedsUpdate;
	bool haveLock;
	bool kerberosAliveAndBarking = true;
	int kerbRecordLen;
	char *kerbNamePtr = NULL;
	PWSFKerberosPrincipal* remoteKerbRec;
	PWSFKerberosPrincipal* localKerbRec;
	PWSFKerberosPrincipalList kerbList;
	PWSFKerberosPrincipalList localKerbList;
	char *kerbCmdBuffer = NULL;
	int kerbCmdBufferSize = 0;
	
	[self setForceSync:NO];
	[self setShouldSyncNow:NO];
	
	// sanity
    if ( inSyncFile == NULL )
		return -1;
	
	if ( outNumAccepted != NULL )
		*outNumAccepted = 0;
	if ( outNumTrumped != NULL )
		*outNumTrumped = 0;
	
	// get file len
    remoteFileLen = 0;
    err = lstat( inSyncFile, &sb );
    if ( err == 0 )
        remoteFileLen = sb.st_size;
    if ( remoteFileLen < sizeof(PWFileHeader) )
		return -1;
	
	// open the sync file
	syncFile = fopen( inSyncFile, "r" );
    if ( syncFile == NULL )
		return -1;
	
	[self pwWait];
	haveLock = [self pwLock:kLongerLockInterval];
	[self pwSignal];
	
	do
	{
		// Giving up is not fatal
		// The data can be retrieved at the next sync
		if ( ! haveLock ) {
			result = -3;
			break;
		}
		
		// copy our header
		err = [self getHeader:&localHeader];
		if ( err != 0 && err != -3 ) {
			result = err;
			break;
		}
		
		// read opposing sync time
		readCount = fread( &syncTime, sizeof(syncTime), 1, syncFile );
		if ( readCount != 1 ) {
			result = -1;
			break;
		}
		
		// read opposing header
		readCount = fread( &remoteHeader, sizeof(remoteHeader), 1, syncFile );
		if ( readCount != 1 ) {
			result = -1;
			break;
		}
				
		// check compatibility
		if ( localHeader.signature != remoteHeader.signature ||
			 localHeader.version != remoteHeader.version ||
			 localHeader.entrySize != remoteHeader.entrySize )
		{
			result = -2;
			break;
		}
		
		// sync the header
		localHeader.sequenceNumber = MAX(localHeader.sequenceNumber, remoteHeader.sequenceNumber);
		localHeader.deepestSlotUsed = MAX(localHeader.deepestSlotUsed, remoteHeader.deepestSlotUsed);
		
		if ( remoteHeader.accessModDate > 0 && remoteHeader.accessModDate - inTimeSkew > localHeader.accessModDate ) {
			localHeader.access = remoteHeader.access;
			localHeader.extraAccess = remoteHeader.extraAccess;
			localHeader.accessModDate = remoteHeader.accessModDate - inTimeSkew;
		}
		
		err = [self setHeader:&localHeader];
		
		if ( remoteHeader.numberOfSlotsCurrentlyInFile > localHeader.numberOfSlotsCurrentlyInFile )
			[self expandDatabase:remoteHeader.numberOfSlotsCurrentlyInFile - localHeader.numberOfSlotsCurrentlyInFile nextAvailableSlot:NULL];
		
		[self pwUnlock];
		
		localKerbList.ReadAllPrincipalsFromDB( 0, mKerberosCacheLimit );
		
		// either update the record or trump it
		while (true)
		{
			bNeedsUpdate = false;
			
			readCount = fread( &remoteRec, sizeof(remoteRec), 1, syncFile );
			if ( readCount != 1 )
				break;
			
			readCount = fread( &kerbRecordLen, sizeof(kerbRecordLen), 1, syncFile );
			if ( readCount != 1 )
				break;
				
			localKerbRec = NULL;
			if (kerbRecordLen > 0)
			{
				// always read to advance the file position, then toss if kerberos is OFF.
				remoteKerbRec = kerbList.ReadPrincipalFromFile(syncFile, kerbRecordLen);
				if ( ! kerberosAliveAndBarking ) {
					delete remoteKerbRec;
					remoteKerbRec = NULL;
				}
			}
			else
				remoteKerbRec = NULL;
			
			if (remoteKerbRec != NULL)
			{
				localKerbRec = localKerbList.GetPrincipalByName( remoteKerbRec->GetName() );
				if ( localKerbRec == NULL )
				{
					err = PWSFKerberosPrincipal::ReadPrincipalFromDB(remoteKerbRec->GetName(), &localKerbRec);
					if ( err == -4 )
						kerberosAliveAndBarking = false;
				}
			}
			
			localRec = remoteRec;
			
			if (remoteRec.slot != (UInt32)-1)
			{	
				// this is a normal user record
				err = [self getValidPasswordRec:&localRec fromSpillBucket:&bFromSpillBucket unObfuscate:NO];
				if ( err == 0 && bFromSpillBucket )
				{
					PWFileEntry mainRec;
					
					// if the main slot has a death certificate, replace it.
					err = [self getPasswordRec:localRec.slot putItHere:&mainRec unObfuscate:NO];
					if ( err == 0 )
					{
						if ( PWRecIsZero(mainRec) )
						{
							// if the main slot is empty, delete the slot in the overflow
							// (it will get written into the main db)
							[mOverflow deleteSlot:&localRec];
						}
						else if ( mainRec.extraAccess.recordIsDead && !localRec.extraAccess.recordIsDead )
						{
							// replace death certificates with the new record
							if ( [self setPassword:&localRec atSlot:localRec.slot obfuscate:NO setModDate:NO] == 0 )
							{
								[mOverflow deleteSlot:&localRec];
								
								// keep the death certificate in the overflow. It will
								// get cleaned up eventually.
								[mOverflow saveOverflowRecord:&mainRec obfuscate:NO setModDate:NO];
							}
						}
					}
				}
				
				if ( err == 0 && localRec.extraAccess.doNotReplicate )
					continue;
				
				if ( err != 0 )
				{
					if ( remoteRec.extraAccess.recordIsDead )
					{
						bzero( &localRec, sizeof(localRec) );
					}
					else
					{
						// record not in the database yet
						localRec = remoteRec;
						bNeedsUpdate = true;
					}
				}
				else if ( localKerbRec == NULL )
				{
					// if we couldn't load the kerberos record earlier,
					// let's see if the principal exists locally because we
					// may need to delete it.
					char princ[256] = {0,};
					
					if ( localRec.digest[4].digest[0] != 0 &&
						strcmp(localRec.digest[4].method, "KerberosRealmName") == 0 )
					{
						snprintf( princ, sizeof(princ), "%s@%s", pwsf_GetPrincName(&localRec), localRec.digest[4].digest );
						localKerbRec = localKerbList.GetPrincipalByName( princ );
						if ( localKerbRec == NULL )
						{
							err = PWSFKerberosPrincipal::ReadPrincipalFromDB(princ, &localKerbRec);
							if ( err == -4 )
								kerberosAliveAndBarking = false;
						}
					}
				}
				
				// recordIsDead needs to be an OR, not last mod date or we get the undead
				if ( localRec.extraAccess.recordIsDead != remoteRec.extraAccess.recordIsDead )
				{
					localRec.extraAccess.recordIsDead |= remoteRec.extraAccess.recordIsDead;
					bNeedsUpdate = true;
				}
				
				// password fields
				localTime = timegm( (struct tm *)&localRec.modDateOfPassword );
				remoteTime = timegm( (struct tm *)&remoteRec.modDateOfPassword ) - inTimeSkew;
				if ( inTimeSkew != 0 )
					gmtime_r( &remoteTime, (struct tm *)&remoteRec.modDateOfPassword );
				
				if ( remoteTime > localTime )
				{
					memcpy( &localRec.modDateOfPassword, &remoteRec.modDateOfPassword, sizeof(BSDTimeStructCopy) );
					memcpy( localRec.passwordStr, remoteRec.passwordStr, sizeof(localRec.passwordStr) );
					for ( int idx = 0; idx < kPWFileMaxDigests; idx++ )
						localRec.digest[idx] = remoteRec.digest[idx];
						
					bNeedsUpdate = true;
				}
				else if (localKerbRec != NULL && remoteKerbRec != NULL)
					remoteKerbRec->CopyPassword(localKerbRec);
				
				// last login time
				localTime = timegm( (struct tm *)&localRec.lastLogin );
				remoteTime = timegm( (struct tm *)&remoteRec.lastLogin );
				if ( remoteTime > inTimeSkew )
					remoteTime -= inTimeSkew;
				if ( inTimeSkew != 0 )
					gmtime_r( &remoteTime, (struct tm *)&remoteRec.lastLogin );
				
				if ( remoteTime > localTime )
				{
					memcpy( &localRec.lastLogin, &remoteRec.lastLogin, sizeof(BSDTimeStructCopy) );
					bNeedsUpdate = true;
				}
				else if (localKerbRec != NULL && remoteKerbRec != NULL)
					remoteKerbRec->CopyLastLogin(localKerbRec);
				
				// all non-special fields
				localTime = timegm( (struct tm *)&localRec.modificationDate );
				remoteTime = timegm( (struct tm *)&remoteRec.modificationDate ) - inTimeSkew;
				if ( inTimeSkew != 0 )
					gmtime_r( &remoteTime, (struct tm *)&remoteRec.modificationDate );
				
				if ( remoteTime > localTime )
				{
					// policy updates for kerberos
					if ( localKerbRec != NULL && memcmp(&localRec.access, &remoteRec.access, sizeof(PWAccessFeatures)) != 0 )
					{
						pwsf_ModifyPrincipalWithBuffer( localKerbRec->GetName(), &remoteRec.access,
							localRec.access.maxMinutesUntilChangePassword, &kerbCmdBuffer, &kerbCmdBufferSize );
					}
					
					memcpy( &localRec.modificationDate, &remoteRec.modificationDate, sizeof(BSDTimeStructCopy) );
					memcpy( &localRec.access, &remoteRec.access, sizeof(PWAccessFeatures) );
					memcpy( &localRec.usernameStr, &remoteRec.usernameStr, sizeof(localRec.usernameStr) );
					memcpy( &localRec.userdata, &remoteRec.userdata, sizeof(localRec.userdata) );
					localRec.failedLoginAttempts = remoteRec.failedLoginAttempts;
					localRec.extraAccess.doNotReplicate = remoteRec.extraAccess.doNotReplicate;
					
					bNeedsUpdate = true;
				}
				
				// do not replicate deleted users back into Kerberos DB
				if ( localRec.extraAccess.recordIsDead )
				{
					if ( remoteKerbRec != NULL )
						delete remoteKerbRec;
					if ( localKerbRec != NULL && (kerbNamePtr=localKerbRec->GetName()) )
					{
						pwsf_DeletePrincipal( kerbNamePtr );
					}
				}
				
				if ( bNeedsUpdate )
				{
					if ( outNumAccepted != NULL )
						(*outNumAccepted)++;
					
					[self pwWait];
					haveLock = [self pwLock:kLongerLockInterval];
					[self pwSignal];
					
					if ( ! haveLock )
					{
						// Note: this only breaks out of the inner while loop.
						result = -3;
						break;
					}
					
					err = [self addPassword:&localRec atSlot:localRec.slot obfuscate:NO setModDate:NO];
				}
				else
				{
					if ( remoteKerbRec != NULL )
						delete remoteKerbRec;	// remove from list to update
					
					if ( outNumTrumped != NULL )
						(*outNumTrumped)++;
				}
			}
			else if (remoteKerbRec != NULL)
			{
				// this is a non-user kerberos record.  We just check the mod-date for a winner.
				// If the local kerb record exists and has a moddate at least as recent as the
				// remote record, then remove the record from the list to update.
				if ((localKerbRec != NULL) && (remoteKerbRec->GetRecordModDate() <= localKerbRec->GetRecordModDate()))
					delete remoteKerbRec;
			}
			
			// remaining remote kerberos records will be delete along with the kerbList
			if (localKerbRec != NULL)
				delete localKerbRec;
		}
			
		// now sync kerberos information
		if ( kerberosAliveAndBarking )
			kerbList.WriteAllPrincipalsToDB();
	}
	while (0);
	
	[self pwUnlock];
	
	fclose( syncFile );
	
	if ( kerbCmdBuffer != NULL ) {
		free( kerbCmdBuffer );
		kerbCmdBuffer = NULL;
		kerbCmdBufferSize = 0;
	}

	// clear sensitive info
	bzero( &localHeader, sizeof(localHeader) );
	bzero( &remoteHeader, sizeof(remoteHeader) );
	bzero( &localRec, sizeof(localRec) );
	bzero( &remoteRec, sizeof(remoteRec) );
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	mergeHeader
//
//	Returns: 0 == success, -1 == fail, -2 == incompatible databases, -3 == db file busy
//
//  Updates the database header
//------------------------------------------------------------------------------------------------

-(int)mergeHeader:(PWFileHeader *)inRemoteHeader timeSkew:(long)inTimeSkew
{
	int result = 0;
	int err;
	PWFileHeader localHeader;
	bool haveLock;
	
	// sanity
    if ( inRemoteHeader == NULL )
		return -1;
	
	// grab file lock
	[self pwWait];
	haveLock = [self pwLock:kLongerLockInterval];
	[self pwSignal];
	
	do
	{
		// Giving up is not fatal
		// The data can be retrieved at the next sync
		if ( ! haveLock )
		{
			syslog( LOG_ALERT, "[AuthDBFile mergeHeader] - can't get file lock");
			result = -3;
			break;
		}
		
		// copy our header
		err = [self getHeader:&localHeader];
		if ( err != 0 && err != -3 ) {
			result = err;
			break;
		}
		
		// check compatibility
		if ( localHeader.signature != inRemoteHeader->signature ||
			 localHeader.version != inRemoteHeader->version ||
			 localHeader.entrySize != inRemoteHeader->entrySize )
		{
			result = -2;
			break;
		}
		
		// sync the header
		localHeader.sequenceNumber = MAX(localHeader.sequenceNumber, inRemoteHeader->sequenceNumber);
		localHeader.deepestSlotUsed = MAX(localHeader.deepestSlotUsed, inRemoteHeader->deepestSlotUsed);
		
		// add time skew adjustment
		if ( inRemoteHeader->accessModDate > 0 && inRemoteHeader->accessModDate - inTimeSkew > localHeader.accessModDate ) {
			localHeader.access = inRemoteHeader->access;
			localHeader.extraAccess = inRemoteHeader->extraAccess;
			localHeader.accessModDate = inRemoteHeader->accessModDate - inTimeSkew;
		}
		
		err = [self setHeader:&localHeader];
		
		if ( inRemoteHeader->numberOfSlotsCurrentlyInFile > localHeader.numberOfSlotsCurrentlyInFile )
			[self expandDatabase:inRemoteHeader->numberOfSlotsCurrentlyInFile - localHeader.numberOfSlotsCurrentlyInFile
				nextAvailableSlot:NULL];
	}
	while (0);
	
	[self pwUnlock];
	
	// clear sensitive info
	bzero( &localHeader, sizeof(localHeader) );
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	mergeSlot
//
//	Returns: 0 == success, -1 == fail, -2 == incompatible databases, -3 == db file busy
//
//  Updates the database account records; assumes the header has been updated separately
//------------------------------------------------------------------------------------------------

-(SyncStatus)mergeSlot:(PWFileEntry *)inRemoteRecord
	timeSkew:(long)inTimeSkew
	accepted:(BOOL *)outAccepted
{
	SyncStatus result = kSyncStatusNoErr;
	int err = 0;
	time_t localTime = 0;
	time_t remoteTime = 0;
	PWFileEntry localRec = {0};
	BOOL bFromSpillBucket = NO;
	BOOL bNeedsUpdate = NO;
	BOOL haveLock = NO;
	
	if ( outAccepted != NULL )
		*outAccepted = NO;
	
	do
	{
		if ( inRemoteRecord->slot != (UInt32)-1 )
		{
			// this is a normal user record
			localRec = *inRemoteRecord;
			err = [self getValidPasswordRec:&localRec fromSpillBucket:&bFromSpillBucket unObfuscate:NO];
			if ( err == 0 && bFromSpillBucket )
			{
				PWFileEntry mainRec;
				
				// if the main slot has a death certificate, replace it.
				err = [self getPasswordRec:localRec.slot putItHere:&mainRec unObfuscate:NO];
				if ( err == 0 )
				{
					if ( PWRecIsZero(mainRec) )
					{
						// if the main slot is empty, delete the slot in the overflow
						// (it will get written into the main db)
						[mOverflow deleteSlot:&localRec];
					}
					else if ( mainRec.extraAccess.recordIsDead && !localRec.extraAccess.recordIsDead )
					{
						// replace death certificates with the new record
						if ( [self setPassword:&localRec atSlot:localRec.slot obfuscate:NO setModDate:NO] == 0 )
						{
							[mOverflow deleteSlot:&localRec];
							
							// keep the death certificate in the overflow. It will
							// get cleaned up eventually.
							[mOverflow saveOverflowRecord:&mainRec obfuscate:NO setModDate:NO];
						}
					}
				}
			}
			
			if ( err == 0 && localRec.extraAccess.doNotReplicate )
				continue;
			
			if ( err != 0 )
			{
				if ( inRemoteRecord->extraAccess.recordIsDead )
				{
					bzero( &localRec, sizeof(localRec) );
				}
				else
				{
					// record not in the database yet
					localRec = *inRemoteRecord;
					bNeedsUpdate = YES;
				}
			}
			
			// recordIsDead needs to be an OR, not last mod date or we get the undead
			if ( localRec.extraAccess.recordIsDead != inRemoteRecord->extraAccess.recordIsDead )
			{
				char princ[512];
				
				localRec.extraAccess.recordIsDead = 1;
				bNeedsUpdate = YES;
				
				// remove the kerberos principal
				if ( inRemoteRecord->extraAccess.recordIsDead &&
					 inRemoteRecord->digest[kPWHashSlotKERBEROS].digest[0] != '\0' )
				{
					snprintf( princ, sizeof(princ), "%s@%s",
								pwsf_GetPrincName(inRemoteRecord),
								inRemoteRecord->digest[kPWHashSlotKERBEROS].digest );
					pwsf_DeletePrincipal( princ );
				}
			}
			
			// password fields
			localTime = timegm( (struct tm *)&localRec.modDateOfPassword );
			remoteTime = timegm( (struct tm *)&inRemoteRecord->modDateOfPassword ) - inTimeSkew;
			if ( inTimeSkew != 0 )
				gmtime_r( &remoteTime, (struct tm *)&inRemoteRecord->modDateOfPassword );
				
			if ( remoteTime > localTime )
			{
				memcpy( &localRec.modDateOfPassword, &inRemoteRecord->modDateOfPassword, sizeof(BSDTimeStructCopy) );
				memcpy( localRec.passwordStr, inRemoteRecord->passwordStr, sizeof(localRec.passwordStr) );
				for ( int idx = 0; idx < kPWFileMaxDigests; idx++ )
					localRec.digest[idx] = inRemoteRecord->digest[idx];
						
				bNeedsUpdate = YES;
			}
				
			// last login time
			localTime = timegm( (struct tm *)&localRec.lastLogin );
			remoteTime = timegm( (struct tm *)&inRemoteRecord->lastLogin );
			if ( remoteTime > inTimeSkew )
				remoteTime -= inTimeSkew;
			if ( inTimeSkew != 0 )
				gmtime_r( &remoteTime, (struct tm *)&inRemoteRecord->lastLogin );
				
			if ( remoteTime > localTime )
			{
				memcpy( &localRec.lastLogin, &inRemoteRecord->lastLogin, sizeof(BSDTimeStructCopy) );
				bNeedsUpdate = YES;
			}

			// all non-special fields
			localTime = timegm( (struct tm *)&localRec.modificationDate );
			remoteTime = timegm( (struct tm *)&inRemoteRecord->modificationDate ) - inTimeSkew;
			if ( inTimeSkew != 0 )
				gmtime_r( &remoteTime, (struct tm *)&inRemoteRecord->modificationDate );
				
			if ( remoteTime > localTime )
			{
				memcpy( &localRec.modificationDate, &inRemoteRecord->modificationDate, sizeof(BSDTimeStructCopy) );
				memcpy( &localRec.access, &inRemoteRecord->access, sizeof(PWAccessFeatures) );
				memcpy( &localRec.extraAccess, &inRemoteRecord->extraAccess, sizeof(PWMoreAccessFeatures) );
				memcpy( &localRec.usernameStr, &inRemoteRecord->usernameStr, sizeof(localRec.usernameStr) );
				memcpy( &localRec.userGUID, &inRemoteRecord->userGUID, sizeof(localRec.userGUID) );
				memcpy( &localRec.admingroup, &inRemoteRecord->admingroup, sizeof(localRec.admingroup) );
				memcpy( &localRec.userdata, &inRemoteRecord->userdata, sizeof(localRec.userdata) );
				memcpy( &localRec.disableReason, &inRemoteRecord->disableReason, sizeof(localRec.disableReason) );
				localRec.failedLoginAttempts = inRemoteRecord->failedLoginAttempts;
				
				bNeedsUpdate = YES;
			}
			
			if ( bNeedsUpdate )
			{
				if ( outAccepted != NULL )
					*outAccepted = YES;
					
				[self pwWait];
				haveLock = [self pwLock:kLongerLockInterval];
				[self pwSignal];
				
				if ( ! haveLock )
				{
					result = kSyncStatusServerDatabaseBusy;
					break;
				}
				
				err = [self addPassword:&localRec atSlot:localRec.slot obfuscate:NO setModDate:NO];
			}
		}
	}
	while (0);
	
	[self pwUnlock];
	
	// clear sensitive info
	bzero( &localRec, sizeof(localRec) );
		
	return result;
}


//------------------------------------------------------------------------------------------------
//	mergeKerberosRecord
//
//	Returns: 0 == success, -1 == fail, -2 == incompatible databases, -3 == db file busy
//
//  Updates the database account records; assumes the header has been updated separately
//------------------------------------------------------------------------------------------------

-(SyncStatus)mergeKerberosRecord:(unsigned char *)inRemoteKerberosRecordData
	recordDataLen:(int)inRemoteKerberosRecordDataLen
	withList:(PWSFKerberosPrincipalList *)inKerbList
	modList:(PWSFKerberosPrincipalList *)inOutModKerbList
	timeSkew:(long)inTimeSkew
	accepted:(BOOL *)outAccepted
{
	PWSFKerberosPrincipal *localKerbRec = NULL;
	PWSFKerberosPrincipal *remoteKerbRec = NULL;
	
	if ( outAccepted != NULL )
		*outAccepted = NO;
	
	if ( inRemoteKerberosRecordData == NULL || inKerbList == NULL || inOutModKerbList == NULL )
		return kSyncStatusFail;
	
	remoteKerbRec = inOutModKerbList->ReadPrincipalFromData(inRemoteKerberosRecordData, inRemoteKerberosRecordDataLen);
	if ( remoteKerbRec == NULL )
		return kSyncStatusFail;
	
	localKerbRec = inKerbList->GetPrincipalByName( remoteKerbRec->GetName() );
	if ( localKerbRec == NULL )
	{
		// not in the database yet (or not in the cache)
		*outAccepted = YES;
		return kSyncStatusNoErr;
	}
	
	// Check the modate for a winner.
	// If the local kerb record exists and has a moddate at least as recent as the
	// remote record, then remove the record from the list to update.
	if ((localKerbRec != NULL) && (remoteKerbRec->GetRecordModDate() <= localKerbRec->GetRecordModDate()))
	{
		delete remoteKerbRec;
		remoteKerbRec = NULL;
	}
	else
	{
		*outAccepted = YES;
	}
		
	return kSyncStatusNoErr;
}


#pragma mark -
#pragma mark POLICY TESTING
#pragma mark -

//------------------------------------------------------------------------------------------------
//	disableStatus
//
//	Returns: kAuthOK, kAuthUserDisabled, kAuthPasswordExpired
//  <outReasonCode> is only valid if the return value is kAuthUserDisabled.
//------------------------------------------------------------------------------------------------

-(int)disableStatus:(PWFileEntry *)inOutPasswordRec changed:(BOOL *)outChanged reason:(PWDisableReasonCode *)outReasonCode
{
	PWAccessFeatures *access;
    int result = kAuthOK;
	PWDisableReasonCode reason;
	
    if ( inOutPasswordRec == NULL || outChanged == NULL )
        return kAuthFail;
        
    *outChanged = false;
	if ( outReasonCode != NULL )
		*outReasonCode = kPWDisabledNotSet;
	access = &inOutPasswordRec->access;
	
    // do not disable administrators
    if ( access->isAdminUser || inOutPasswordRec->extraAccess.isComputerAccount )
        return kAuthOK;
    
	if ( inOutPasswordRec->access.isDisabled )
		return kAuthUserDisabled;
	
	result = pwsf_TestDisabledStatusWithReasonCode( access, &mPWFileHeader.access, (struct tm *)&inOutPasswordRec->creationDate, (struct tm *)&inOutPasswordRec->lastLogin, &inOutPasswordRec->failedLoginAttempts, &reason );
	
	// update the user record
	if ( result == kAuthUserDisabled )
	{
		BOOL markDisabled = YES;
		
		// Note: maxMinutesOfNonUse is special
		// If a user logs in in the nick-of-time on a replica, then synchronizing should un-disable the
		// account. Therefore, we do not want to toggle the disabled bit.
		if ( access->maxMinutesOfNonUse > 0 )
		{
			if ( LoginTimeIsStale( &inOutPasswordRec->lastLogin, access->maxMinutesOfNonUse ) )
				markDisabled = NO;
		}
		else
		if ( mPWFileHeader.access.maxMinutesOfNonUse > 0 &&
			 LoginTimeIsStale( &inOutPasswordRec->lastLogin, mPWFileHeader.access.maxMinutesOfNonUse ) )
		{
			markDisabled = NO;
		}		
		
		if ( markDisabled )
		{
			inOutPasswordRec->access.isDisabled = true;
			*outChanged = true;
		}
		
		if ( outReasonCode != NULL )
			*outReasonCode = reason;
	}
	
    return result;
}


//------------------------------------------------------------------------------------------------
//	ChangePasswordStatus
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange, kAuthPasswordExpired
//------------------------------------------------------------------------------------------------

-(int)changePasswordStatus:(PWFileEntry *)inPasswordRec
{
	BOOL needsChange = NO;
	
	if ( inPasswordRec->access.isAdminUser || inPasswordRec->extraAccess.isComputerAccount )
		return kAuthOK;
	
	if ( inPasswordRec->access.newPasswordRequired )
	{
		needsChange = YES;
	}
	else
	{
		// usingExpirationDate
		if ( inPasswordRec->access.usingExpirationDate )
		{
			if ( TimeIsStale( &inPasswordRec->access.expirationDateGMT ) )
				needsChange = YES;
		}
		else
		if ( mPWFileHeader.access.usingExpirationDate && TimeIsStale( &mPWFileHeader.access.expirationDateGMT ) )
		{
			needsChange = YES;
		}
		
		// maxMinutesUntilChangePassword
		if ( inPasswordRec->access.maxMinutesUntilChangePassword > 0 )
		{
			if ( LoginTimeIsStale( &inPasswordRec->modDateOfPassword, inPasswordRec->access.maxMinutesUntilChangePassword ) )
				needsChange = YES;
		}
		else
		if ( mPWFileHeader.access.maxMinutesUntilChangePassword > 0 && LoginTimeIsStale( &inPasswordRec->modDateOfPassword, mPWFileHeader.access.maxMinutesUntilChangePassword ) )
		{
			needsChange = YES;
		}
	}
	
	if ( needsChange )
    {
        if ( inPasswordRec->access.canModifyPasswordforSelf || inPasswordRec->access.isAdminUser )
            return kAuthPasswordNeedsChange;
        else
        	return kAuthPasswordExpired;
    }
    
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	requiredCharacterStatus
//
//	Returns: enum of Reposonse Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

-(int)requiredCharacterStatus:(PWFileEntry *)inPasswordRec forPassword:(const char *)inPassword
{
	return pwsf_RequiredCharacterStatusExtra( &(inPasswordRec->access), &(mPWFileHeader.access), inPasswordRec->usernameStr, inPassword, &(inPasswordRec->extraAccess) );
}


//------------------------------------------------------------------------------------------------
//	passwordHistoryStatus
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange (recycled password)
//------------------------------------------------------------------------------------------------

-(int)passwordHistoryStatus:(PWFileEntry *)inPasswordRec password:(const char *)inPassword
{
	int historyCount = 0;
	int result;
	
    if ( inPasswordRec->access.usingHistory || mPWFileHeader.access.usingHistory )
	{
		// At a minimum, do not allow the same password again
		if ( strcmp( inPassword, inPasswordRec->passwordStr ) == 0 )
		{
			return kAuthPasswordNeedsChange;
		}
		
		if ( inPasswordRec->access.historyCount > 0 )
			historyCount = inPasswordRec->access.historyCount;
		else
		if ( GlobalHistoryCount(mPWFileHeader.access) > 0 )
		{
			historyCount = GlobalHistoryCount(mPWFileHeader.access);
		}
		
		if ( historyCount > 0 )
		{
			result = [self checkHistory:inPasswordRec count:historyCount password:inPassword];
			if ( result == -1 )
				return kAuthPasswordNeedsChange;
		}
	}
	
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	ReenableStatus
//
//	Returns: bool
//------------------------------------------------------------------------------------------------

-(int)reenableStatus:(PWFileEntry *)inPasswordRec enableMinutes:(unsigned long)inGlobalReenableMinutes
{
	unsigned long inMaxMinutes = inPasswordRec->extraAccess.minutesUntilFailedLoginReset;
	
	if ( inMaxMinutes == 0 )
		inMaxMinutes = inGlobalReenableMinutes;
	if ( inMaxMinutes == 0 )
		return 0;
	
	if ( inPasswordRec->access.isDisabled &&
		 inPasswordRec->disableReason == kPWDisabledTooManyFailedLogins &&
		 LoginTimeIsStale( &(inPasswordRec->modificationDate), inMaxMinutes ) )
	{
		return 1;
	}
	
	return 0;
}


#pragma mark -
#pragma mark HISTORY ACCESSORS
#pragma mark -

//------------------------------------------------------------------------------------------------
//	checkHistory
//
//	Returns: -1 for reused password, or 0 for good password
//	Checks to see if <inPassword> is in the password history
//------------------------------------------------------------------------------------------------

-(int)checkHistory:(PWFileEntry *)inPasswordRec count:(int)inMaxHistory password:(const char *)inPassword
{
	char historyData[16*sizeof(HMAC_MD5_STATE)];
	int err;
	char *tptr;
	int index;
	unsigned long hashLen;
	unsigned char passwordHash[sizeof(HMAC_MD5_STATE)];
	
	if ( inMaxHistory > 16 )
		inMaxHistory = 16;
	
	err = [self getHistory:inPasswordRec data:historyData];
	
	// if there is no history, then any password is good
	if ( err != 0 )
		return 0;
	
	// get the CRAM-MD5 hash of the password
	pwsf_getHashCramMD5( (unsigned char *)inPassword, strlen(inPassword), passwordHash, &hashLen );
	
	tptr = historyData;
	for ( index = 0; index < inMaxHistory; index++ )
	{
		if ( memcmp( passwordHash, tptr, sizeof(HMAC_MD5_STATE) ) == 0 )
			return -1;
		
		tptr += sizeof(HMAC_MD5_STATE);
	}
	
	return 0;
}


//------------------------------------------------------------------------------------------------
//	putInHistory
//
//	Returns: -1, errno, or 0 for success
//------------------------------------------------------------------------------------------------

-(int)putInHistory:(PWFileEntry *)inPasswordRec passwordHash:(const char *)inPasswordHash
{
	char historyData[16*sizeof(HMAC_MD5_STATE)];
	int err;
	int historyCount = 0;
	int idx;
	
	if ( inPasswordRec->access.usingHistory )
		historyCount = inPasswordRec->access.historyCount;
	else
	if ( mPWFileHeader.access.usingHistory )
		historyCount = GlobalHistoryCount(mPWFileHeader.access);
	
	if ( historyCount == 0 )
		return 0;
	
	// Does the user have a history already?
	err = [self getHistory:inPasswordRec data:historyData];
	
	// if there is no history, then add a record
	if ( err != 0 )
	{
		err = [self addHistory:inPasswordRec];
		
		// if we couldn't add a history record, we're out of luck
		if ( err != 0 )
			return -1;
		
		bzero( historyData, sizeof(historyData) );
	}
	
	// dump the oldest entry
	// memcpy does not support forward overlapping
	for ( idx = 15; idx >= 1; idx-- )
		memcpy( historyData + idx*sizeof(HMAC_MD5_STATE),
				historyData + (idx-1)*sizeof(HMAC_MD5_STATE),
				sizeof(HMAC_MD5_STATE) );
	
	// add the new one
	memcpy( historyData, inPasswordHash, sizeof(HMAC_MD5_STATE) );
	
	// only store the maximum history count
	// add +1 to the count to include the current password
	// (if usingHistory==true and historyCount==0, that's really a history of 1)
	if ( historyCount < 15 )
		bzero( historyData + (historyCount+1)*sizeof(HMAC_MD5_STATE),
				sizeof(historyData) - (historyCount+1)*sizeof(HMAC_MD5_STATE) );
	
	// write to file
	err = [self saveHistory:inPasswordRec data:historyData];
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	getHistory
//
//	Returns: -1, errno, or 0 for success
//	outHistoryData		<- 		The entire block of history data for the given user ID
//------------------------------------------------------------------------------------------------

-(int)getHistory:(PWFileEntry *)inPasswordRec data:(char *)outHistoryData
{
	char historyPath[1024] = {0,};
	char buff[kPWUserIDSize + 16*sizeof(HMAC_MD5_STATE)];
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	PWFileEntry *dbEntry;
	
	if ( inPasswordRec == NULL || outHistoryData == NULL )
		return -1;
	
	sprintf( historyPath, "%s/%s", mDirPathStr, kPWHistoryFileName );
	fp = fopen( historyPath, "r" );
	if ( fp == NULL ) {
		err = errno;
		if ( err == 0 )
			err = -1;
		return err;
	}
	
	offset = [self historySlotToOffset:inPasswordRec->slot];
	
	byteCount = pread( fileno(fp), buff, sizeof(buff), offset );
	
	dbEntry = (PWFileEntry *)buff;
	
	if ( inPasswordRec->time == dbEntry->time &&
		 inPasswordRec->rnd == dbEntry->rnd &&
		 inPasswordRec->sequenceNumber == dbEntry->sequenceNumber &&
		 inPasswordRec->slot == dbEntry->slot )
	{
		// copy it out
		memcpy( outHistoryData, buff + kPWUserIDSize, sizeof(buff) - kPWUserIDSize );
				
		// zero our copy
		bzero( buff, sizeof(buff) );
		
		err = 0;
	}
		
	fclose( fp );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	addHistory
//
//	Returns: -1, errno, or 0 for success
//	Appends a new record to the appropriate file for the given user ID
//------------------------------------------------------------------------------------------------

-(int)addHistory:(PWFileEntry *)inPasswordRec
{
	char historyPath[1024] = {0,};
	char buff[16*sizeof(HMAC_MD5_STATE)];
	int writeCount;
	FILE *fp;
	int err = -1;
	off_t offset;
	struct stat sb;
	
	if ( inPasswordRec == NULL )
		return -1;
	
	offset = [self historySlotToOffset:inPasswordRec->slot];
	
    sprintf( historyPath, "%s/%s", mDirPathStr, kPWHistoryFileName );	

	// Use open(2) so the file will get created if needed, but not truncated.
	int fd = open( historyPath, O_RDWR|O_CREAT );
	if (fd >= 0) {
		fp = fdopen( fd, "r+" );
	}
	if ( fp == NULL ) {
		err = errno;
		if ( err == 0 )
			err = -1;
		return err;
	}
	
	err = lstat( historyPath, &sb );
	if ( err == 0 )
	{
		// is the file big enough?
		if ( (int64_t)(offset + kPWUserIDSize + sizeof(buff)) > sb.st_size )
		{
			long amountToWrite = offset + kPWUserIDSize + sizeof(buff) - sb.st_size;
			char *zeroBuff = (char *) calloc( 1, amountToWrite );
			
			err = fseek( fp, 0, SEEK_END );
			if ( err == 0 )
			{
				writeCount = fwrite( zeroBuff, amountToWrite, 1, fp );
				if ( writeCount != 1 )
					err = -1;
			}
			
			free( zeroBuff );
		}
		
		if ( err == 0 )
		{
			err = fseek( fp, offset, SEEK_SET );
			if ( err == 0 )
			{
				writeCount = fwrite( inPasswordRec, kPWUserIDSize, 1, fp );
				if ( writeCount != 1 )
					err = -1;
			}
		}
	}
		
	fclose( fp );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	saveHistory
//
//	Returns: -1, errno, or 0 for success
//	inOutHistoryData	<->		The entire 8K block of history data for the given user ID
//								On exit, the data is encrypted
//------------------------------------------------------------------------------------------------

-(int)saveHistory:(PWFileEntry *)inPasswordRec data:(char *)inOutHistoryData
{
	char historyPath[1024] = {0,};
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	const long sizeOfHistory = 16*sizeof(HMAC_MD5_STATE);
	
	if ( inPasswordRec == NULL || inOutHistoryData == NULL )
		return -1;
	
	sprintf( historyPath, "%s/%s", mDirPathStr, kPWHistoryFileName );	
	fp = fopen( historyPath, "r+" );
	if ( fp == NULL ) {
		err = errno;
		if ( err == 0 )
			err = -1;
		return err;
	}
	
	offset = [self historySlotToOffset:inPasswordRec->slot];
	
	byteCount = pwrite( fileno(fp), inOutHistoryData, sizeOfHistory, offset + kPWUserIDSize );
	if ( byteCount == sizeOfHistory )
		err = 0;
	
	fclose( fp );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	historySlotToOffset
//
//	Returns: the position in the history database file for the slot
//------------------------------------------------------------------------------------------------

-(unsigned long)historySlotToOffset:(unsigned long)inSlotNumber
{
	// each slot is:
	// 16 byte userID
	// 512 bytes of CRAM-MD5 hashes (16 hashes * 32 bytes each)
	
	return ( (inSlotNumber-1) * ( kPWUserIDSize + 16*sizeof(HMAC_MD5_STATE)) );
}


#pragma mark -
#pragma mark MISC
#pragma mark -

//------------------------------------------------------------------------------------------------
-(SyncPriority)syncPriority
{
	return mSyncPriority;
}


//------------------------------------------------------------------------------------------------
-(void)setShouldSyncNow:(BOOL)onOff
{
	if ( onOff == YES )
	{
		if ( mSyncPriority == kSyncPriorityNormal )
			mSyncPriority = kSyncPriorityDirty;
	}
	else
	{
		mSyncPriority = kSyncPriorityNormal;
	}
}


//------------------------------------------------------------------------------------------------
-(void)setForceSync:(BOOL)onOff
{
	if ( onOff == YES ) 
	{
		mSyncPriority = kSyncPriorityForce;
	}
	else
	{
		if ( mSyncPriority == kSyncPriorityForce )
			mSyncPriority = kSyncPriorityDirty;
	}
}


//------------------------------------------------------------------------------------------------
-(void)setTestSpillBucket:(BOOL)onOff
{
	mTestSpillBucket = onOff;
}

//------------------------------------------------------------------------------------------------
-(uint32_t)kerberosCacheLimit
{
	return mKerberosCacheLimit;
}

//------------------------------------------------------------------------------------------------
-(void)setKerberosCacheLimit:(uint32_t)inLimit
{
	mKerberosCacheLimit = inLimit;
}


@end
