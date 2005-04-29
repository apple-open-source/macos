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


#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <openssl/err.h>

#include "DSMutexSemaphore.h"
#include "CAuthFileBase.h"
#include "SASLCode.h"

extern "C" {
#include "saslutil.h"

#if COMPILE_WITH_RSA_LOAD
    #include "bufaux.h"
    #include "buffer.h"
    #include "cipher.h"
    #include "xmalloc.h"
    #include "ssh.h"
#endif
};

#define kFixedDESKey			"1POTATO2potato3PotatoFOUR"
//#define kFixedDESKey			"M&2y(V40"
#define kFixedDESChunk			8
#define kMaxWriteSuspendTime	2			// seconds
#define kPWUserIDSize			4*sizeof(long)

/* Version identification string for identity files. */
#define AUTHFILE_ID_STRING "SSH PRIVATE KEY FILE FORMAT 1.1\n"

#define Max(A,B)			(((A) > (B)) ? (A):(B))
#define PWRecIsZero(A)		(((A).time == 0) && ((A).rnd == 0) && ((A).sequenceNumber == 0) && ((A).slot == 0))

extern int errno;

CAuthFileBase::CAuthFileBase()
{
	this->Init();
}


CAuthFileBase::CAuthFileBase( const char *inDBFilePath )
{
	this->Init();
	if ( inDBFilePath != NULL && strlen(inDBFilePath) < sizeof(fFilePath) )
		strcpy( fFilePath, inDBFilePath );
}


CAuthFileBase::~CAuthFileBase()
{
    this->resetPasswordFileState();
}


void
CAuthFileBase::Init(void)
{
	time_t now;

    pwFile = NULL;
    freeListFile = NULL;
    pwFileHeader.signature = 'null';
    pwFileValidated = false;
    pwFilePermission[0] = '\0';
    pwFileBasePtr = NULL;
    pwFileLen = 0;
    rsaKey = NULL;
    fWriteSuspended = false;
	fGotHeader = false;
	strcpy( fFilePath, kPWFilePath );
	fDBFileLocked = false;
	fReadOnlyFileSystem = false;
	
	// seed random # generator
	time(&now);
	srandom((UInt32)now);
}


int
CAuthFileBase::validateFiles(void)
{
    int err;
    
    err = this->validatePasswordFile();
    this->validateFreeListFile();
    
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
int
CAuthFileBase::validatePasswordFile(void)
{
    int err;
    struct stat sb;
    PWFileHeader dbHeader;

    // validate
    err = lstat( fFilePath, &sb );
    if ( err == 0 )
		err = this->getHeader( &dbHeader );
    if ( err == 0 )
	{
        if ( pwFile != NULL )
        {
            if ( pwFileHeader.signature != kPWFileSignature ||
                 pwFileHeader.version != kPWFileVersion ||
                 sb.st_size != sizeof(PWFileHeader) + pwFileHeader.numberOfSlotsCurrentlyInFile * sizeof(PWFileEntry) )
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
        pwFileValidated = true;
    
    return err;
}


int
CAuthFileBase::validateFreeListFile(void)
{
	return 0;
}


int
CAuthFileBase::createPasswordFile(void)
{
    int err = -1;
    size_t writeCount;
    
	// make sure the directory exists
	err = mkdir( kPWDirPath, S_IRWXU );
	
	// if it existed before, double-check the permissions
	if ( err != 0 && errno == EEXIST )
		err = chmod( kPWDirPath, S_IRWXU );
	
    // create new file
    pwFile = fopen( fFilePath, "w+" );
    if ( pwFile != NULL )
    {
        err = chmod( fFilePath, S_IRUSR | S_IWUSR );
        if ( err == -1 )
            err = errno;
        // ignore
        err = 0;
        
        // set header initial state
        bzero( &pwFileHeader, sizeof(PWFileHeader) );
        pwFileHeader.signature = kPWFileSignature;
        pwFileHeader.version = kPWFileVersion;
        pwFileHeader.sequenceNumber = 0;
        pwFileHeader.numberOfSlotsCurrentlyInFile = kPWFileInitialSlots;
        pwFileHeader.deepestSlotUsed = 0;
        pwFileHeader.deepestSlotUsedByThisServer = 0;
		
        pwFileHeader.access.usingHistory = false;
        pwFileHeader.access.usingExpirationDate = false;
        pwFileHeader.access.usingHardExpirationDate = false;
        pwFileHeader.access.requiresAlpha = false;
        pwFileHeader.access.requiresNumeric = false;
        pwFileHeader.access.passwordIsHash = false;
		
        // do not need to set these if usingExpirationDate and usingHardExpirationDate are false
        //pwFileHeader.access.expirationDateGMT
        //pwFileHeader.access.hardExpireDateGMT
    
        pwFileHeader.access.maxMinutesUntilChangePassword = 0;
        pwFileHeader.access.maxMinutesUntilDisabled = 0;	
        pwFileHeader.access.maxMinutesOfNonUse = 0;
        pwFileHeader.access.maxFailedLoginAttempts = 0;
        pwFileHeader.access.minChars = 0;
        pwFileHeader.access.maxChars = 0;

        // write header
        writeCount = fwrite( &pwFileHeader, sizeof(PWFileHeader), 1, pwFile );
        if ( writeCount != 1 )
        {
            err = -1;
        }
        
        // write blank space
        if ( err == 0 )
        {
            PWFileEntry anEntry;
            int i;
            
            bzero( &anEntry, sizeof(PWFileEntry) );
            
            for ( i = kPWFileInitialSlots; i > 0; i-- )
            {
                writeCount = fwrite( &anEntry, sizeof(PWFileEntry), 1, pwFile );
                if ( writeCount != 1 )
                {
                    err = -1;
                    break;
                }
            }
        }
        
        this->closePasswordFile();
        
        if ( err != 0 )
            remove( fFilePath );
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
int
CAuthFileBase::openPasswordFile(const char *mode, Boolean map)
{
    int err = 0;
     
    if ( pwFile && strcmp( mode, pwFilePermission ) == 0 )
    {
        return err;
    }
    else
    {
        this->closePasswordFile();
        
        pwFile = fopen( fFilePath, mode );
		
		// handle read-only file system
		if ( pwFile == NULL && errno == EROFS )
		{
			fReadOnlyFileSystem = true;
			pwFile = fopen( fFilePath, "r" );
		}
		
        if ( pwFile != NULL )
        {
            strcpy( pwFilePermission, fReadOnlyFileSystem ? "r" : mode );
            
            if ( map && strcmp( mode, "r" ) == 0 ) {
                err = this->mapPasswordFile();
                
                // do not report mapping errors. all methods can operate unmapped.
                err = 0;
            }
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


int
CAuthFileBase::mapPasswordFile(void)
{
    int err;
    struct stat sb;
    int fileNum;
    
    // can't map if not open
    if ( pwFile == NULL )
        return -1;
    
    // dump the old
    if ( pwFileBasePtr )
    {
        munmap( pwFileBasePtr, pwFileLen );
        pwFileBasePtr = nil;
    }
    
    // get file len
    pwFileLen = 0;
    err = lstat( fFilePath, &sb );
    if ( err == 0 )
        pwFileLen = sb.st_size;
    
    // map
    if ( pwFileLen > 0 )
    {
        fileNum = fileno( pwFile );
        pwFileBasePtr = (caddr_t) mmap( 0, pwFileLen, PROT_READ | PROT_WRITE, MAP_FILE, fileNum, 0 );
        if ( (long)pwFileBasePtr == -1 )
        {
            err = errno;
            
            // errno could get stomped by another thread
            if ( err == 0 )
                err = -1;
            
            // let everyone know there is no map
            pwFileBasePtr = NULL;
            
            // do not close the file; go about life unmapped
            //this->closePasswordFile();
        }
    }
    
    return err;
}


void
CAuthFileBase::closePasswordFile(void)
{
    if ( pwFile )
    {
		if ( fDBFileLocked )
			pwUnlock();

		if ( pwFileBasePtr )
		{
			munmap( pwFileBasePtr, pwFileLen );
			pwFileBasePtr = nil;
			pwFileLen = 0;
		}
		fclose( pwFile );
		pwFile = nil;
    }
	
	fGotHeader = false;
}


void
CAuthFileBase::closeFreeListFile(void)
{
    if ( freeListFile )
    {
        fclose( freeListFile );
        freeListFile = nil;
    }
}

		
void
CAuthFileBase::resetPasswordFileState(void)
{
    if ( pwFile )
        fflush( pwFile );
    
    pwWait();
    closePasswordFile();
    pwSignal();
	
	// force the rsa key to be reloaded
	rsaWait();
	if ( rsaKey != NULL ) {
		RSA_free( rsaKey );
		rsaKey = NULL;
	}
	rsaSignal();
}


void
CAuthFileBase::carryOn( void )
{
}


void
CAuthFileBase::pwLock(void)
{
	int tries = 3;
	
	if ( pwFile != NULL )
	{
		while ( flock( fileno(pwFile), LOCK_EX | LOCK_NB ) == -1 && tries-- > 0 )
			usleep( 25000 );
	}
}


//----------------------------------------------------------------------------------------------------
//	pwLock
//
//	Returns: TRUE if the lock is obtained.
//----------------------------------------------------------------------------------------------------
bool
CAuthFileBase::pwLock( unsigned long inMillisecondsToWait )
{
	const long millisecondsPerTry = 25;
	long tries = inMillisecondsToWait / millisecondsPerTry;
	bool locked = false;
	
	if ( pwFile == NULL )
	{
		fDBFileLocked = false;
		this->openPasswordFile( "r+", false );
    }
	
	if ( pwFile != NULL )
	{
		if ( fDBFileLocked )
			return true;
		
		if ( tries <= 0 )
			tries = 1;
		
		while ( tries-- > 0 )
		{
			if ( flock( fileno(pwFile), LOCK_EX | LOCK_NB ) == 0 )
			{
				locked = true;
				break;
			}
			
			usleep( millisecondsPerTry * 1000 );
		}
	}
	
	fDBFileLocked = locked;
	return locked;
}


void
CAuthFileBase::pwUnlock(void)
{
	if ( pwFile != NULL )
		flock( fileno(pwFile), LOCK_UN );
	fDBFileLocked = false;
}


void
CAuthFileBase::pwWait(void)
{
	// override in sub-class
}


void
CAuthFileBase::pwSignal(void)
{
	// override in sub-class
}


void
CAuthFileBase::rsaWait(void)
{
	// override in sub-class
}


void
CAuthFileBase::rsaSignal(void)
{
	// override in sub-class
}


//----------------------------------------------------------------------------------------------------
//	getHeader
//
//	Returns: 0=success, -1=fail, -2=recovery failed, -3 recovery used
//----------------------------------------------------------------------------------------------------
int
CAuthFileBase::getHeader( PWFileHeader *outHeader, bool inCanUseCachedCopy )
{
    int err = -1;
    ssize_t readCount;
    bool saveAfterReleasingSemaphore = false;
	
    if ( outHeader == NULL )
        return -1;
    
	if ( inCanUseCachedCopy && fGotHeader )
	{
		memcpy( outHeader, &pwFileHeader, sizeof(PWFileHeader) );
		return 0;
	}
	
    pwWait();
	err = this->openPasswordFile( fReadOnlyFileSystem ? "r" : "r+", false );
    if ( err == 0 && pwFile )
    {
        if ( pwFileBasePtr != NULL )
        {
            // get from the map
            memcpy( outHeader, pwFileBasePtr, sizeof(PWFileHeader) );
        }
        else
        {
			/*
            err = fseek( pwFile, 0, SEEK_SET );
            if ( err == 0 )
                readCount = fread( outHeader, sizeof(PWFileHeader), 1, pwFile );
			*/
			
			// This one is faster (Panther7A122)
			readCount = pread( fileno(pwFile), outHeader, sizeof(PWFileHeader), 0 );
        }
        
		if ( outHeader->signature == kPWFileSignature )
		{
			// adopt the new header data
			memcpy( &pwFileHeader, outHeader, sizeof(PWFileHeader) );
		}
		else
		{
			err = -2;
			
			// bad news, try to recover
			if ( fGotHeader && pwFileHeader.signature == kPWFileSignature )
			{
				err = -3;
				
				memcpy( outHeader, &pwFileHeader, sizeof(PWFileHeader) );
				saveAfterReleasingSemaphore = true;
			}
		}
		
		fGotHeader = true;
    }
    pwSignal();
    
	if ( saveAfterReleasingSemaphore )
		this->setHeader( &pwFileHeader );
	
    return err;
}


//----------------------------------------------------------------------------------------------------
//	setHeader
//
//	Returns: 0=success, -1=fail
//----------------------------------------------------------------------------------------------------
int
CAuthFileBase::setHeader( const PWFileHeader *inHeader )
{
    int err = -1;
    long writeCount;
    
    if ( inHeader == NULL )
        return -1;
	if ( inHeader->signature != kPWFileSignature )
		return -1;
	if ( fReadOnlyFileSystem )
		return -1;
	
    pwWait();
    err = this->openPasswordFile( "r+", false );
    if ( err == 0 && pwFile )
    {
        err = fseek( pwFile, 0, SEEK_SET );
        if ( err == 0 )
        {
            // adopt the new header data
			if ( inHeader != &pwFileHeader )
				memcpy( &pwFileHeader, inHeader, sizeof(PWFileHeader) );
            
            // write to disk
            writeCount = fwrite( &pwFileHeader, sizeof(PWFileHeader), 1, pwFile );
            if ( writeCount != 1 )
            {
                err = -1;
            }
            fflush( pwFile );
        }
    }
	pwSignal();
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	getRSAPublicKey
//
//	Returns a base64 encoded rsa key
//----------------------------------------------------------------------------------------------------
int
CAuthFileBase::getRSAPublicKey( char *outRSAKeyStr )
{
    PWFileHeader dbHeader;
    int result = 0;
    long len;
	
    if ( outRSAKeyStr == NULL )
        return -1;
    *outRSAKeyStr = '\0';
    
	result = this->getHeader( &dbHeader, true );
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
int
CAuthFileBase::loadRSAKeys( void )
{
    int result = 0;

#if COMPILE_WITH_RSA_LOAD

    PWFileHeader dbHeader;
    char passphrase[1] = "";
    
    // check if we already loaded the key
	rsaWait();
   	if ( rsaKey != NULL )
	{
		rsaSignal();
		return 1;
    }
	
	result = this->getHeader( &dbHeader, true );
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
        if (len < strlen(AUTHFILE_ID_STRING) + 1) {
            syslog(LOG_INFO, "Bad key.");
            buffer_free(&buffer);
			
			rsaSignal();
            return 0;
        }
        /*
        * Make sure it begins with the id string.  Consume the id string
        * from the buffer.
        */
        for (unsigned int i = 0; i < (unsigned int) sizeof(AUTHFILE_ID_STRING); i++)
            if (buffer_get_char(&buffer) != (unsigned char) AUTHFILE_ID_STRING[i]) {
                syslog(LOG_INFO, "Bad key.");
				buffer_free(&buffer);
				
				rsaSignal();
                return 0;
            }
        /* Read cipher type. */
        cipher_type = buffer_get_char(&buffer);
        (void) buffer_get_int(&buffer);	/* Reserved data. */
    
        /* Read the public key from the buffer. */
        buffer_get_int(&buffer);
        rsaKey = RSA_new();
		if ( rsaKey == NULL ) {
			rsaSignal();
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
			rsaSignal();
            return 0;
        }
        /* Read the rest of the private key. */
        rsaKey->d = BN_new();
		if (rsaKey->d == NULL) {
			RSA_free(rsaKey);
			rsaKey = NULL;
			rsaSignal();
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
		
		rsaSignal();
        return 1;
	}
    
    bzero(&dbHeader, sizeof(dbHeader));
    rsaSignal();
	
#else
    syslog(LOG_INFO, "RSA key loading not compiled\n");
    result = -1;
#endif

    return result;
}


//----------------------------------------------------------------------------------------------------
//	decryptRSA
//
//	Returns: -1=fail, 0=success
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::decryptRSA( unsigned char *inBlob, int inBlobLen, unsigned char *outBlob )
{
    int len;
    int result = 0;
	
	if ( this->loadRSAKeys() != 1 )
        return -1;
    
	rsaWait();
	len = RSA_private_decrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
	rsaSignal();
	
	if (len <= 0)
	{
		// print the error for debugging only. The error code may apply to Klima-Pokomy-Rosa attack.
		//syslog( LOG_INFO, "rsa_private_decrypt() failed, err = %lu", ERR_get_error() );
        syslog( LOG_INFO, "rsa_private_decrypt() failed" );
        result = -1;
		
		// let's try reloading the key
		rsaWait();
		RSA_free( rsaKey );
		rsaKey = NULL;
		rsaSignal();
		
		if ( this->loadRSAKeys() == 1 )
		{
			rsaWait();
			len = RSA_private_decrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
			rsaSignal();
			
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

int
CAuthFileBase::encryptRSA( unsigned char *inBlob, int inBlobLen, unsigned char *outBlob )
{
    int len;
    int maxRSASize;
	
    if ( this->loadRSAKeys() != 1 )
        return -1;
    
	// the maximum length of a block when using RSA_PKCS1_PADDING
	// is RSA_size( rsaKey ) - 11 (see the man page for RSA_public_encrypt)
	maxRSASize = RSA_size( rsaKey );
	if ( inBlobLen > maxRSASize - 11 )
		inBlobLen = maxRSASize - 11;
	
	rsaWait();
	len = RSA_public_encrypt( inBlobLen, inBlob, outBlob, rsaKey, RSA_PKCS1_PADDING );
	rsaSignal();
	
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
int
CAuthFileBase::isWeakAuthMethod( const char *inMethod )
{
    int index;
    int result;
	PWFileHeader dbHeader;
    
	result = this->getHeader( &dbHeader, true );
    if ( result != 0 && result != -3 )
        return 1;
    
    for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
        if ( strcmp( inMethod, dbHeader.weakAuthMethods[index].method ) == 0 )
            return 1;
    
    return 0;
}


int
CAuthFileBase::addWeakAuthMethod( const char *inMethod )
{
	int index;
    PWFileHeader ourHeader;
    int result = 0;
    
	pwLock( kOneSecondLockInterval );
	
    result = this->getHeader( &ourHeader );
    if ( result == 0 || result == -3 )
    {
		for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
		{
			if ( ourHeader.weakAuthMethods[index].method[0] == 0 )
			{
				strcpy( ourHeader.weakAuthMethods[index].method, inMethod );
				result = this->setHeader( &ourHeader );
				break;
			}
		}
	}
	
	pwUnlock();
	
    return result;
}


int
CAuthFileBase::removeWeakAuthMethod( const char *inMethod )
{
	int index;
    PWFileHeader ourHeader;
    int result = 0;
    
    pwLock( kOneSecondLockInterval );
	
    result = this->getHeader( &ourHeader );
    if ( result == 0 || result == -3 )
    {
		for ( index = 0; index < kPWFileMaxWeakMethods; index++ )
		{
			if ( strcmp( inMethod, pwFileHeader.weakAuthMethods[index].method ) == 0 )
			{
				bzero( pwFileHeader.weakAuthMethods[index].method, SASL_MECHNAMEMAX+1 );
				result = this->setHeader( &ourHeader );
				break;
			}
		}
	}
	
	pwUnlock();
	
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

int
CAuthFileBase::expandDatabase( unsigned long inNumSlots, long *outSlot )
{
	int err;
	int writeCount;
	
	if ( fReadOnlyFileSystem )
		return -1;
	
	pwWait();
	
	err = this->openPasswordFile( "r+", false );
	if ( err == 0 && pwFile != NULL )
	{
		// write blank space
		err = fseek( pwFile, 0, SEEK_END );
		if ( err == 0 )
		{
			PWFileEntry anEntry;
			int i;
			
			bzero( &anEntry, sizeof(PWFileEntry) );
			
			for ( i = inNumSlots; i > 0; i-- )
			{
				writeCount = fwrite( &anEntry, sizeof(PWFileEntry), 1, pwFile );
				if ( writeCount != 1 )
				{
					err = -1;
					break;
				}
			}
		}
		
		// update header
		if ( err == 0 )
			err = fseek( pwFile, 0, SEEK_SET );
		if ( err == 0 )
		{
			pwFileHeader.numberOfSlotsCurrentlyInFile += inNumSlots;
			if ( outSlot != NULL )
			{
				pwFileHeader.deepestSlotUsed++;
				pwFileHeader.deepestSlotUsedByThisServer = pwFileHeader.deepestSlotUsed;
				*outSlot = pwFileHeader.deepestSlotUsed;
			}
			
			writeCount = fwrite( &pwFileHeader, sizeof(PWFileHeader), 1, pwFile );
			if ( writeCount != 1 )
			{
				err = -1;
			}
		}
	}
	
	pwSignal();
	
	return err;
}


//----------------------------------------------------------------------------------------------------
//	nextSlot
//
//	Returns: 0 for invalid/error, or the next slot number in the pw file for writing the next entry.
//----------------------------------------------------------------------------------------------------

long
CAuthFileBase::nextSlot(void)
{
    long slot = 0;
    int err = -1;
    off_t curpos;
    long readCount;
    PWFileEntry dbEntry;
	
    if ( pwFileValidated )
    {
		if ( pwFileHeader.deepestSlotUsedByThisServer < pwFileHeader.numberOfSlotsCurrentlyInFile - 1 )
		{
			err = this->getPasswordRec( pwFileHeader.deepestSlotUsedByThisServer + 1, &dbEntry );
			if ( err == 0 &&
				 dbEntry.time == 0 &&
				 dbEntry.rnd == 0 &&
				 dbEntry.sequenceNumber == 0 &&
				 dbEntry.slot == 0 )
			{
				pwFileHeader.deepestSlotUsedByThisServer++;
				slot = pwFileHeader.deepestSlotUsedByThisServer;
				if ( pwFileHeader.deepestSlotUsedByThisServer > pwFileHeader.deepestSlotUsed )
					pwFileHeader.deepestSlotUsed = pwFileHeader.deepestSlotUsedByThisServer;
				
				return slot;
			}
		}
		
        if ( pwFileHeader.deepestSlotUsed < pwFileHeader.numberOfSlotsCurrentlyInFile - 1 )
        {
            pwFileHeader.deepestSlotUsed++;
			pwFileHeader.deepestSlotUsedByThisServer = pwFileHeader.deepestSlotUsed;
            slot = pwFileHeader.deepestSlotUsed;
        }
        else
        {
            // go look in the freelist
            freeListFile = fopen( kFreeListFilePath, "r+" );
            if ( freeListFile )
            {
                err = fseek( freeListFile, -sizeof(long), SEEK_END );
                if ( err == 0 )
                {
                    curpos = ftell( freeListFile );
                    readCount = fread( &slot, sizeof(long), 1, freeListFile );
                    this->closeFreeListFile();
                    if ( readCount == 1 )
                    {
                        // snip the one we used
                        err = truncate( kFreeListFilePath, curpos );
                    }
                    else
                    {
                        err = -1;
                    }
                }
            }
            
            // if freelist is empty, expand the file
            if ( err != 0 )
            {
                err = this->expandDatabase( kPWFileInitialSlots, &slot );
            }
        }
    }
    
    return slot;
}


//----------------------------------------------------------------------------------------------------
//	getGMTime
//
//	Returns: a time struct based on GMT
//----------------------------------------------------------------------------------------------------

void
CAuthFileBase::getGMTime(struct tm *inOutGMT)
{
    fUtils.getGMTime( inOutGMT );
}


//----------------------------------------------------------------------------------------------------
//	getTimeForRef
//
//	Returns: a timestamp based on GMT
//----------------------------------------------------------------------------------------------------

UInt32
CAuthFileBase::getTimeForRef(void)
{
    time_t theTime;
    
    time(&theTime);
    return (UInt32)theTime;
}


//----------------------------------------------------------------------------------------------------
//	getRandom
//
//	Returns: a random number for user IDs
//----------------------------------------------------------------------------------------------------

UInt32
CAuthFileBase::getRandom(void)
{
	UInt32 result;
	UInt32 uiNow;
	time_t now;
	
	result = (UInt32) random();
	
	time(&now);
	uiNow = (UInt32)now + result;
	srandom(uiNow);
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	addRSAKeys 
//
//	Returns: 0 or -1
//	Adds RSA version 2 keys to the password database header using the ssh-keygen tool
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::addRSAKeys(unsigned int inBitCount)
{
    char commandStr[256];
    FILE *aFile;
    struct stat sb;
    int result;
	unsigned char *publicKey;
	unsigned long publicKeyLen;
	unsigned char *privateKey;
	unsigned long privateKeyLen;
	
    // make the keys
    sprintf(commandStr, "/usr/bin/ssh-keygen -t rsa1 -b %u -f %s -P \"\"", inBitCount, kTempKeyFile);
    aFile = popen( commandStr, "r" );
	if ( !aFile )
        return -1;
    
    // read the data back (but there's nothing to read)
    
    pclose(aFile);
    
    // stat the key file to make sure we created it
    result = stat( kTempKeyFile, &sb );
    if ( result != 0 )
        return result;
        
    // add the private key
    aFile = fopen( kTempKeyFile, "r" );
    if ( !aFile )
        return -1;
    
    privateKeyLen = (unsigned long)sb.st_size;
	privateKey = (unsigned char *) malloc( privateKeyLen + 1 );
    fread((char*)privateKey, (unsigned long)sb.st_size, 1, aFile);
    fclose(aFile);
    
    // stat the public key file to make sure we created it
    sprintf(commandStr, "%s.pub", kTempKeyFile);
    result = stat( commandStr, &sb );
    if ( result != 0 )
        return result;
    
    // add the public key
    aFile = fopen( commandStr, "r" );
    if ( !aFile )
        return -1;
    
    publicKeyLen = (unsigned long)sb.st_size;
    publicKey = (unsigned char *) malloc( publicKeyLen + 1 );
    fread(publicKey, (unsigned long)sb.st_size, 1, aFile);
    fclose(aFile);
    
    result = this->addRSAKeys( publicKey, publicKeyLen, privateKey, privateKeyLen );
	
	// we are done with these
    remove(commandStr);
    remove(kTempKeyFile);
    free(privateKey);
	free(publicKey);
	
    return result;
}

        
//----------------------------------------------------------------------------------------------------
//	addRSAKeys 
//
//	Returns: 0 or -1
//	Adds RSA version 2 keys to the password database header using the ssh-keygen tool
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::addRSAKeys(
	unsigned char *publicKey,
	unsigned long publicKeyLen,
	unsigned char *privateKey,
	unsigned long privateKeyLen )
{
    PWFileHeader ourHeader;
    int result;
	
    if ( privateKeyLen > kPWFileMaxPrivateKeyBytes )
        return -1;
    
    if ( publicKeyLen > kPWFileMaxPublicKeyBytes )
        return -1;
	
    // retrieve the pw database header
    result = this->getHeader( &ourHeader );
    if ( result != 0 && result != -3 )
        return result;
    
    ourHeader.privateKeyLen = privateKeyLen;
	memcpy( ourHeader.privateKey, privateKey, privateKeyLen );
    
    ourHeader.publicKeyLen = publicKeyLen;
	memcpy( ourHeader.publicKey, publicKey, publicKeyLen );
    
    // write it back to the pw database file
	result = this->setHeader( &ourHeader );
    
    // do not leave the private key sitting around in the stack
    bzero(&ourHeader, sizeof(ourHeader));
    
    return result;
}

        
//----------------------------------------------------------------------------------------------------
//	addGenesisPassword 
//
//	Returns: errno
//	Creates an initial Admin user in slot 1 so that the database can be edited.
//	This operation should not be done by the password server. If an existing
//	password file were moved or damaged, it could give a hacker free reign.
//	This method should only be called by a tool on the local CPU that is only run by root.
//	(Setup Assistant, for example).
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::addGenesisPassword(const char *username, const char *password, PWFileEntry *outPWRec )
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
    passwordRec.access.newPasswordRequired = false;	// TEMP DISABLE
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
    
	pwLock( kOneSecondLockInterval );
	
	err = this->getHeader( &dbHeader );
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
		
		err = this->setPasswordAtSlot( &passwordRec, passwordRec.slot );
		if ( err == 0 && outPWRec != NULL )
		{
			memcpy( outPWRec, &passwordRec, sizeof(PWFileEntry) );
		}
		
		err2 = this->setHeader( &dbHeader );
	}
	
	pwUnlock();
	
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

int
CAuthFileBase::addPassword(PWFileEntry *passwordRec, bool obfuscate)
{
    PWFileHeader ignoreHeader;
    int err, err2;
	
	pwLock( kOneSecondLockInterval );
	
    // refresh the header
	// the retrieved header is ignored because the nextSlot() method uses
	// the object's copy of the header in pwFileHeader.
    err = this->getHeader( &ignoreHeader );
    if ( err != 0 && err != -3 )
		return err;
	
    passwordRec->time = this->getTimeForRef();
    passwordRec->rnd = this->getRandom();
    passwordRec->sequenceNumber = ++pwFileHeader.sequenceNumber;
    passwordRec->slot = this->nextSlot();
    
    fUtils.getGMTime( (struct tm *)&passwordRec->creationDate );
    memcpy( &passwordRec->lastLogin, &passwordRec->creationDate, sizeof(struct tm) );
    memcpy( &passwordRec->modDateOfPassword, &passwordRec->creationDate, sizeof(struct tm) );
    
    err = this->setPasswordAtSlot( passwordRec, passwordRec->slot, obfuscate );
	
	// re-write the header to mark the slot used.
	err2 = this->setHeader( &pwFileHeader );
	
	pwUnlock();
	
	if ( err == 0 && err2 != 0 )
		err = err2;
	
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

int
CAuthFileBase::initPasswordRecord(PWFileEntry *passwordRec, bool obfuscate)
{
	PWFileHeader ignoreHeader;
	int err, err2;

	pwLock( kOneSecondLockInterval );

	// refresh the header
	// the retrieved header is ignored because the nextSlot() method uses
	// the object's copy of the header in pwFileHeader.
	err = this->getHeader( &ignoreHeader );
	if ( err != 0 && err != -3 )
		return err;

	passwordRec->time = this->getTimeForRef();
	passwordRec->rnd = this->getRandom();
	passwordRec->sequenceNumber = ++pwFileHeader.sequenceNumber;
	passwordRec->slot = this->nextSlot();

	fUtils.getGMTime( (struct tm *)&passwordRec->creationDate );
	memcpy( &passwordRec->lastLogin, &passwordRec->creationDate, sizeof(struct tm) );
	memcpy( &passwordRec->modDateOfPassword, &passwordRec->creationDate, sizeof(struct tm) );

	// re-write the header to mark the slot used.
	err2 = this->setHeader( &pwFileHeader );

	pwUnlock();

	if ( err == 0 && err2 != 0 )
		err = err2;

	return err;
}


//----------------------------------------------------------------------------------------------------
//	addPasswordAtSlot
//
//	Returns: errno
//	Used to add password records from replicas. Fills in the slot if free; otherwise redirects the
//	record to the spill-bucket.
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::addPasswordAtSlot(PWFileEntry *passwordRec, long slot, bool obfuscate, bool setModDate)
{
	PWFileEntry dbEntry;
	int err;
	bool bGoesInMainDB = false;
	
	// verifying the slot id, do not need to un-obfuscate
	err = this->getPasswordRec( passwordRec->slot, &dbEntry, false );
	if ( err != 0 )
		return err;
	
	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		// same user
		bGoesInMainDB = true;
	}
	else
	if ( dbEntry.time == 0 && dbEntry.rnd == 0 &&
			dbEntry.sequenceNumber == 0 && dbEntry.slot == 0 )
	{
		// slot free
		bGoesInMainDB = true;
	}
	
	if ( bGoesInMainDB )
	{
		err = this->setPasswordAtSlot( passwordRec, passwordRec->slot, obfuscate, setModDate );
	}
	else
	{
		err = this->SaveOverflowRecord( passwordRec, obfuscate, setModDate );
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
//	obfuscate is TRUE, but the password is not un-obfuscated.
//	setModDate is TRUE
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::addPasswordAtSlotFast(PWFileEntry *passwordRec, long slot)
{
	PWFileEntry dbEntry;
	int err;
	bool bGoesInMainDB = false;

	// verifying the slot id, do not need to un-obfuscate
	err = this->getPasswordRec( passwordRec->slot, &dbEntry, false );
	if ( err != 0 )
		return err;

	if ( passwordRec->time == dbEntry.time &&
			passwordRec->rnd == dbEntry.rnd &&
			passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
			passwordRec->slot == dbEntry.slot )
	{
		// same user
		bGoesInMainDB = true;
	}
	else
	if ( dbEntry.time == 0 && dbEntry.rnd == 0 &&
			dbEntry.sequenceNumber == 0 && dbEntry.slot == 0 )
	{
		// slot free
		bGoesInMainDB = true;
	}

	if ( bGoesInMainDB )
	{
		err = this->setPasswordAtSlotFast( passwordRec, passwordRec->slot );
	}
	else
	{
		err = this->SaveOverflowRecord( passwordRec, true, true );
	}

	return err;
}


//----------------------------------------------------------------------------------------------------
//	setPasswordAtSlot
//
//	Returns: errno
//	Used to write to a specific slot.
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::setPasswordAtSlot(PWFileEntry *passwordRec, long slot, bool obfuscate, bool setModDate)
{
    long offset;
    int err = -1;
    int writeCount;
    unsigned int encodeLen;
	
	if ( fReadOnlyFileSystem )
		return -1;
	
    if ( slot > 0 )
    {
		if ( (unsigned long)slot > pwFileHeader.numberOfSlotsCurrentlyInFile )
			return -1;
		
		if ( setModDate )
			fUtils.getGMTime( (struct tm *)&passwordRec->modificationDate );
        
        pwWait();
        err = this->openPasswordFile( "r+", false );
        if ( err == 0 && pwFile )
        {
            offset = fUtils.slotToOffset( slot );
            
            err = fseek( pwFile, offset, SEEK_SET );
            if ( err == 0 )
            {
                //passwordRec->slot = slot;
                encodeLen = strlen(passwordRec->passwordStr);
                encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
                if ( encodeLen > sizeof(passwordRec->passwordStr) )
                    encodeLen = sizeof(passwordRec->passwordStr);
                
				if ( obfuscate )
					fUtils.DESEncode(kFixedDESKey, passwordRec->passwordStr, encodeLen);
					
                writeCount = fwrite( passwordRec, sizeof(PWFileEntry), 1, pwFile );
				
                if ( obfuscate )
					fUtils.DESDecode(kFixedDESKey, passwordRec->passwordStr, encodeLen);
				
				if ( writeCount == 1 )
					fflush( pwFile );
                else
                    err = -1;
            }
            
            /*
            memcpy( pwFileBasePtr + offset, passwordRec, sizeof(PWFileEntry) );
            memcpy( pwFileBasePtr, &pwFileHeader, sizeof(PWFileHeader) );
            */
        }
        pwSignal();
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

int
CAuthFileBase::setPasswordAtSlotFast(PWFileEntry *passwordRec, long slot)
{
	long offset;
	int err = -1;
	int writeCount;
	unsigned int encodeLen;

	if ( fReadOnlyFileSystem )
		return -1;

	if ( slot > 0 )
	{
		if ( (unsigned long)slot > pwFileHeader.numberOfSlotsCurrentlyInFile )
			return -1;
		
		fUtils.getGMTime( (struct tm *)&passwordRec->modificationDate );
		
		pwWait();
		err = this->openPasswordFile( "r+", false );
		if ( err == 0 && pwFile )
		{
			offset = fUtils.slotToOffset( slot );
			
			err = fseek( pwFile, offset, SEEK_SET );
			if ( err == 0 )
			{
				//passwordRec->slot = slot;
				encodeLen = strlen(passwordRec->passwordStr);
				encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
				if ( encodeLen > sizeof(passwordRec->passwordStr) )
					encodeLen = sizeof(passwordRec->passwordStr);
								
				fUtils.DESEncode(kFixedDESKey, passwordRec->passwordStr, encodeLen);
					
				writeCount = fwrite( passwordRec, sizeof(PWFileEntry), 1, pwFile );
				if ( writeCount == 1 )
					fflush( pwFile );
				else
					err = -1;
			}
		}
		pwSignal();
	}

	return err;
}


#if 0
// --------------------------------------------------------------------------------
//	addHashes
//
//	inRealm				 ->		the realm to use for the DIGEST-MD5 hash
//	inOutPasswordRec	<->		in clear-text, out hash values
//	Takes the clear-text password and adds the hashes for auth methods
// --------------------------------------------------------------------------------

void
CAuthFileBase::addHashes( const char *inRealm, PWFileEntry *inOutPasswordRec )
{
	unsigned char smbntHash[32];
	unsigned char smblmHash[16];
	long pwLen;
	
	// SMB-NT			[ 0 ]
	CalculateSMBNTHash(inOutPasswordRec->passwordStr, smbntHash);
	strcpy( inOutPasswordRec->digest[kPWHashSlotSMB_NT].method, kSMBNTStorageTag );
	inOutPasswordRec->digest[kPWHashSlotSMB_NT].digest[0] = 64;
	ConvertBinaryToHex( smbntHash, 32, &inOutPasswordRec->digest[kPWHashSlotSMB_NT].digest[1] );
	
	// SMB-LAN-MANAGER	[ 1 ]
	CalculateSMBLANManagerHash(inOutPasswordRec->passwordStr, smblmHash);
	strcpy( inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].method, "*cmusaslsecretSMBLM" );
	inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].digest[0] = 32;
	ConvertBinaryToHex( smblmHash, 16, &inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].digest[1] );
	
	// DIGEST-MD5		[ 2 ]
	this->addHashDigestMD5( inRealm, inOutPasswordRec );
	
	// CRAM-MD5			[ 3 ]
	this->addHashCramMD5( inOutPasswordRec );
	
	// KERBEROS			[ 4 ]
	// Kerberos doesn't currently store a hash here, we just store the realm name.
	// combined with the user name, we can call the KDC to get the kerberos hashes
	
	// KERBEROS			[ 5 ]
	// Kerberos doesn't currently store a hash here, we just store the principal name.
	// combined with the domain, we can call the KDC to get the kerberos hashes
}
#endif


void
CAuthFileBase::addHashDigestMD5( const char *inRealm, PWFileEntry *inOutPasswordRec )
{
	long pwLen;
	HASH HA1;
	char userID[35];
		
	// DIGEST-MD5		[ 2 ]
	pwLen = strlen(inOutPasswordRec->passwordStr);
	
	this->passwordRecRefToString( inOutPasswordRec, userID );
	DigestCalcSecret( (unsigned char *)userID,
						(unsigned char *)inRealm,
						(unsigned char *)inOutPasswordRec->passwordStr,
						pwLen,
						HA1 );
	
	/*
		* A1 = { H( { username-value, ":", realm-value, ":", passwd } ),
		* ":", nonce-value, ":", cnonce-value }
		*/
	
	// not enough room to store "*cmusaslsecretDIGEST-MD5" so truncate to 20 chars
	strncpy( inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].method, "*cmusaslsecretDIGEST-MD5", SASL_MECHNAMEMAX );
	inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].method[SASL_MECHNAMEMAX] = '\0';
	inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].digest[0] = HASHLEN;
	memcpy( &inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].digest[1], HA1, HASHLEN );
}


void
CAuthFileBase::addHashCramMD5( PWFileEntry *inOutPasswordRec )
{
	unsigned long pwLen;
	
	// CRAM-MD5			[ 3 ]
	strncpy( inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].method, "*cmusaslsecretCRAM-MD5", SASL_MECHNAMEMAX );
	inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].method[SASL_MECHNAMEMAX] = '\0';
	
	this->getHashCramMD5( (unsigned char *)inOutPasswordRec->passwordStr,
						  strlen(inOutPasswordRec->passwordStr),
						  (unsigned char *)&inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].digest[1],
						  &pwLen );
						  
	inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].digest[0] = (unsigned char)pwLen;
}


void
pwsf_getHashCramMD5(const unsigned char *inPassword, long inPasswordLen, unsigned char *outHash, unsigned long *outHashLen )
{
	HMAC_MD5_STATE state;
	
	if ( inPassword == NULL || outHash == NULL || outHashLen == NULL )
		return;
	
	pwsf_hmac_md5_precalc( &state, inPassword, inPasswordLen );
	*outHashLen = sizeof(HMAC_MD5_STATE);
	memcpy( outHash, &state, sizeof(HMAC_MD5_STATE) );
}

void
CAuthFileBase::getHashCramMD5( const unsigned char *inPassword, long inPasswordLen, unsigned char *outHash, unsigned long *outHashLen )
{
	pwsf_getHashCramMD5( inPassword, inPasswordLen, outHash, outHashLen );
}


//-----------------------------------------------------------------------------
//	ConvertBinaryToHex
//-----------------------------------------------------------------------------

bool
CAuthFileBase::ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr )
{
    bool result = true;
	char *tptr = outHexStr;
	char base16table[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	
    if ( inData == nil || outHexStr == nil )
        return false;
    
	for ( int idx = 0; idx < len; idx++ )
	{
		*tptr++ = base16table[(inData[idx] >> 4) & 0x0F];
		*tptr++ = base16table[(inData[idx] & 0x0F)];
	}
	*tptr = '\0';
		
	return result;
}


int
CAuthFileBase::getPasswordRec(long slot, PWFileEntry *passRec, bool unObfuscate)
{
    long offset;
    int err = -1;
    ssize_t readCount;
    
    if ( slot > 0 )
    {
        pwWait();
        err = this->openPasswordFile( fReadOnlyFileSystem ? "r" : "r+", false );
        if ( err == 0 && pwFile )
        {
            offset = fUtils.slotToOffset( slot );
            
            if ( pwFileBasePtr )
            {
                // file is memory-mapped
                memcpy( passRec, pwFileBasePtr + offset, sizeof(PWFileEntry) );
            }
            else
            {
                readCount = pread( fileno(pwFile), passRec, sizeof(PWFileEntry), offset );
				if ( readCount != sizeof(PWFileEntry) )
				{
					// failure could indicate a problem with the file descriptor
					// get a new one next time
					this->closePasswordFile();
					
					err = -2;
				}
            }
            
            // recover the password
			if ( unObfuscate && !PWRecIsZero(*passRec) )
				fUtils.DESAutoDecode( kFixedDESKey, passRec->passwordStr );
        }
        pwSignal();
    }
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	getValidPasswordRec
//
//	Returns: errno
//	same as getPasswordRec but validates the record's ref numbers.
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::getValidPasswordRec(PWFileEntry *passwordRec, bool *outFromSpillBucket, bool unObfuscate)
{
    int err;
    PWFileEntry dbEntry;
    
	if ( outFromSpillBucket != NULL )
		*outFromSpillBucket = false;
	
    err = this->getPasswordRec( passwordRec->slot, &dbEntry, unObfuscate );
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
		err = this->getPasswordRecFromSpillBucket( passwordRec, &dbEntry );
		if ( err == 0 )
		{
			if ( passwordRec->time == dbEntry.time &&
					passwordRec->rnd == dbEntry.rnd &&
					passwordRec->sequenceNumber == dbEntry.sequenceNumber &&
					passwordRec->slot == dbEntry.slot )
			{
				memcpy( passwordRec, &dbEntry, sizeof(PWFileEntry) );
				if ( outFromSpillBucket != NULL )
					*outFromSpillBucket = true;
			}
			else
			{
				err = -3;
			}
		}
		else
		{
			err = -3;
		}
	}
	
	// invalidate the data
	bzero( &dbEntry, sizeof(PWFileEntry) );
    
    return err;
}


int
CAuthFileBase::freeSlot(PWFileEntry *passwordRec)
{
    int err;
    long slot = passwordRec->slot;
    long writeCount;
    PWFileEntry deleteRec;
	bool fromSpillBucket;
	
    // original rec must be valid to have permission to clear a slot
    err = this->getValidPasswordRec( passwordRec, &fromSpillBucket );
    if ( err == 0 )
    {
		// start with a zero record
        bzero( &deleteRec, sizeof(PWFileEntry) );
		
		// keep the ID, mark the time of deletion in modDateOfPassword,
		// and mark dead.
		deleteRec.time = passwordRec->time;
		deleteRec.rnd = passwordRec->rnd;
		deleteRec.sequenceNumber = passwordRec->sequenceNumber;
		deleteRec.slot = passwordRec->slot;
		fUtils.getGMTime( (struct tm *)&deleteRec.modDateOfPassword );
		deleteRec.extraAccess.recordIsDead = true;
		
		if ( fromSpillBucket )
			err = this->SaveOverflowRecord( &deleteRec );
		else
			err = this->setPasswordAtSlot( &deleteRec, slot );
        
        // add the slot number to free list
        freeListFile = fopen( kFreeListFilePath, "a+" );
        if ( freeListFile )
        {
            writeCount = fwrite( &slot, sizeof(long), 1, freeListFile );
            if ( writeCount != 1 )
            {
                // may have a forgotten slot
                err = -1;
            }
            
            this->closeFreeListFile();
        }
    }
    
    return err;
}


//----------------------------------------------------------------------------------------------------
//	passwordRecRefToString
//----------------------------------------------------------------------------------------------------

void
CAuthFileBase::passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr)
{
	fUtils.passwordRecRefToString( inPasswordRec, outRefStr );
}
        

//----------------------------------------------------------------------------------------------------
//	stringToPasswordRecRef
//
//	Returns: Boolean (1==valid ref, 0==fail)
//----------------------------------------------------------------------------------------------------

int
CAuthFileBase::stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec)
{
    return fUtils.stringToPasswordRecRef( inRefStr, outPasswordRec );
}


//------------------------------------------------------------------------------------------------
//	getUserIDFromName
//
//	Returns: Boolean (1==found, 0=not found)
//------------------------------------------------------------------------------------------------
int
CAuthFileBase::getUserIDFromName(const char *inName, bool inAllUsers, long inMaxBuffSize, char *outID)
{
	PWFileHeader dbHeader;
	int result = 0;
	int err = 0;
	UInt32 index;
	PWFileEntry passRec;
	char theAdminID[256];
    long buffRemaining = inMaxBuffSize;
    long len;
    
    if ( outID == NULL || buffRemaining < 1 )
        return 0;
    
    *outID = '\0';
    buffRemaining--;
    
	err = this->getHeader( &dbHeader, false );
	if ( err != 0 && err != -3 )
		return result;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = this->getPasswordRec( index, &passRec, false );
		if ( err != 0 )
			break;
		
		if ( (inAllUsers || passRec.access.isAdminUser) && !passRec.access.isDisabled && strcmp( inName, passRec.usernameStr ) == 0 )
		{
            if ( result == 1 )
            {
                if ( buffRemaining < 1 )
                    break;
                
                strcat( outID, ";" );
                buffRemaining--;
            }
            
			fUtils.passwordRecRefToString( &passRec, theAdminID );
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
int
CAuthFileBase::getUserRecordFromPrincipal(const char *inPrincipal, PWFileEntry *inOutUserRec)
{
	PWFileHeader dbHeader;
	int err = 0;
	UInt32 index;
	PWFileEntry passRec;
	char *thePrincDomain = NULL;
	char thePrincName[256];
    long len;
        
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
	strncpy( thePrincName, inPrincipal, len );
	thePrincName[len] = '\0';
	
	err = this->getHeader( &dbHeader, true );
	if ( err != 0 && err != -3 )
		return 0;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = this->getPasswordRec( index, &passRec, false );
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
//	requireNewPasswordForAllAccounts
//------------------------------------------------------------------------------------------------

void
CAuthFileBase::requireNewPasswordForAllAccounts( bool inRequireNew )
{
	PWFileHeader dbHeader;
	int err = 0;
	UInt32 index;
	PWFileEntry passRec;
        
	err = this->getHeader( &dbHeader, true );
	if ( err != 0 && err != -3 )
		return;
	
	for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
	{
		// Not checking passwords, so leave them obfuscated for performance
		err = this->getPasswordRec( index, &passRec, false );
		if ( err != 0 )
			continue;
		
		passRec.access.newPasswordRequired = inRequireNew;
		
		this->setPasswordAtSlot( &passRec, index, false, true );
	}
}


#pragma mark -

//------------------------------------------------------------------------------------------------
//	AddPassword
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::AddPassword( const char *inUser, const char *inPassword, char *outPasswordRef )
{
    int result;
    PWFileEntry anEntry;
    char refStr[256];
    
	if ( strlen(inPassword) > sizeof(anEntry.passwordStr) - 1 )
		return kAuthPasswordTooLong;
	
	bzero( &anEntry, sizeof(anEntry) );
	
    anEntry.access.isDisabled = false;
    anEntry.access.isAdminUser = false;
    anEntry.access.newPasswordRequired = false;
    anEntry.access.usingHistory = false;
    anEntry.access.canModifyPasswordforSelf = true;
    anEntry.access.usingExpirationDate = false;
    anEntry.access.usingHardExpirationDate = false;
    anEntry.access.requiresAlpha = false;
    anEntry.access.requiresNumeric = false;
    anEntry.access.passwordIsHash = false;
	
    anEntry.access.maxMinutesOfNonUse = 0;
    anEntry.access.maxFailedLoginAttempts = 0;
    anEntry.access.minChars = 0;
    anEntry.access.maxChars = 0;
    
    strcpy( anEntry.usernameStr, inUser );
    strncpy( anEntry.passwordStr, inPassword, sizeof(anEntry.passwordStr) );
 
	result = this->addPassword( &anEntry );
	
    fUtils.passwordRecRefToString( &anEntry, refStr );
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

int
CAuthFileBase::NewPasswordSlot( const char *inUser, const char *inPassword, char *outPasswordRef, PWFileEntry *inOutUserRec )
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

	result = this->initPasswordRecord( inOutUserRec );

	fUtils.passwordRecRefToString( inOutUserRec, outPasswordRef );

	return result;
}


#if 0

//------------------------------------------------------------------------------------------------
//	MakeSyncFile
//
//  Returns: 0 = success, -1 fail
//
//  <inKerberosRecordLimit>		0=no limit; otherwise, database size beyond which kerberos sync
//								is queued
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::MakeSyncFile(
	const char *inFileName,
	time_t inAfterDate,
	long inTimeSkew,
	long *outNumRecordsUpdated,
	unsigned int inKerberosRecordLimit,
	bool *outKerberosOmitted )
{
	PWFileHeader dbHeader;
	int result = 0;
	int err;
	UInt32 index;
	PWFileEntry passRec;
	CKerberosPrincipal* kerberosRec;
    time_t theTime;
	FILE *syncFile;
	int writeCount;
	int zeroLen = 0;
	CKerberosPrincipalList kerbList;
	bool addKerberosRecords = true;
	
	// sanity
    if ( inFileName == NULL )
		return -1;

	if ( outNumRecordsUpdated != NULL )
		*outNumRecordsUpdated = 0;
	if ( outKerberosOmitted != NULL )
		*outKerberosOmitted = false;
	
	// create sync file
	syncFile = fopen( inFileName, "w+" );
    if ( syncFile == NULL )
		return -1;
	
	err = chmod( inFileName, S_IRUSR | S_IWUSR );
	
	// load/copy header
	try
	{
		writeCount = fwrite( &inAfterDate, sizeof(inAfterDate), 1, syncFile );
		if ( writeCount != 1 )
			throw( -1 );
		
		err = this->getHeader(&dbHeader);
		if ( err != 0 && err != -3 )
			throw( err );
		
		writeCount = fwrite( &dbHeader, sizeof(dbHeader), 1, syncFile );
		if ( writeCount != 1 )
			throw( -1 );
		
		if ( inKerberosRecordLimit > 0 && dbHeader.deepestSlotUsed > inKerberosRecordLimit )
			addKerberosRecords = false;
		
		if ( addKerberosRecords )
			kerbList.ReadAllPrincipalsFromDB();
		
		// copy records after the sync date
		for ( index = dbHeader.deepestSlotUsed; index > 0; index-- )
		{
			err = this->getPasswordRec( index, &passRec, false );
			if ( err != 0 )
				throw( err );
			
			// adjust time skew for comparison purposes. The record itself is
			// adjusted on the processing side.
			theTime = ::timegm( (struct tm *)&passRec.modificationDate ) + inTimeSkew;
			
			if ( theTime >= inAfterDate )
			{
				// replicate the identity to avoid collisions, but blank out any useful data
				if ( passRec.doNotReplicate )
				{
					bzero( &passRec.passwordStr, sizeof(passRec.passwordStr) );
					bzero( &passRec.digest[0], sizeof(PasswordDigest) * kPWFileMaxDigests );
					bzero( &passRec.userdata, sizeof(passRec.userdata) );
					bzero( &passRec.userkey, sizeof(passRec.userkey) );
					
					// set passwordIsHash to guarantee that the plain text field is useless
					passRec.access.passwordIsHash = 1;
				}
				
				writeCount = fwrite( &passRec, sizeof(passRec), 1, syncFile );
				if ( writeCount != 1 )
					throw( -1 );
				
				if ( addKerberosRecords && strlen(passRec.digest[4].digest) > 0 )
				{
					char principalName[600];
					strcpy(principalName, passRec.usernameStr);
					strcat(principalName, "@");
					strcat(principalName, passRec.digest[4].digest);
					kerberosRec = kerbList.GetPrincipalByName(principalName);
				}
				else
                	kerberosRec = NULL;
				
				if (kerberosRec != NULL)
				{
					writeCount = kerberosRec->WritePrincipalToFile(syncFile);
					delete kerberosRec;
				}
				else
					writeCount = fwrite( &zeroLen, sizeof(zeroLen), 1, syncFile );

				if ( writeCount != 1 )
					throw( -1 );
				
				if ( outNumRecordsUpdated != NULL )
					(*outNumRecordsUpdated)++;
			}
		}

		// look in the spill-bucket
		this->AddOverflowToSyncFile( syncFile, inAfterDate, inTimeSkew, outNumRecordsUpdated );
		
		if ( addKerberosRecords )
		{
			// Now add remaining Kerberos records since modDate, since these records don't have
			// a corresponding password server record, just put an invalid slot number there
			memset(&passRec, 0, sizeof(passRec));
			passRec.slot = (UInt32)-1;
			kerbList.DeleteOldPrincipals(inAfterDate);

			index = 0;
			kerberosRec = kerbList.GetPrincipalByIndex(index++);
			while (kerberosRec != NULL)
			{
				writeCount = fwrite( &passRec, sizeof(passRec), 1, syncFile );
				if ( writeCount != 1 )
					throw( -1 );
				writeCount = kerberosRec->WritePrincipalToFile(syncFile);
				if ( writeCount != 1 )
					throw( -1 );
				kerberosRec = kerbList.GetPrincipalByIndex(index++);
			}
		}
	}
	catch( int error )
	{
		result = error;
	}
	
	fclose( syncFile );
	
	bzero( &dbHeader, sizeof(dbHeader) );
	bzero( &passRec, sizeof(passRec) );
	
	if ( outKerberosOmitted != NULL )
		*outKerberosOmitted = (!addKerberosRecords);
	
	return result;
}

#endif

//------------------------------------------------------------------------------------------------
//	GetSyncTimeFromSyncFile
//
//	Returns: 0 == success, -1 == fail
//
//	This method gets the official sync time (watermark) for a data file
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::GetSyncTimeFromSyncFile( const char *inSyncFile, time_t *outSyncTime )
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

#if 0

//------------------------------------------------------------------------------------------------
//	ProcessSyncFile
//
//	Returns: 0 == success, -1 == fail, -2 == incompatible databases, -3 == db file busy
//
//	This method processes the records from a remote parent or replica. Any local records
//	believed to be newer than the remote ones are preserved.
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::ProcessSyncFile( const char *inSyncFile, long inTimeSkew, long *outNumAccepted, long *outNumTrumped )
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
	bool bFromSpillBucket;
	bool bNeedsUpdate;
	bool haveLock;
	int kerbRecordLen;
	CKerberosPrincipal* remoteKerbRec;
	CKerberosPrincipal* localKerbRec;
	CKerberosPrincipalList kerbList;
	
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
	
	pwWait();
	haveLock = pwLock( kLongerLockInterval );
	pwSignal();
	
	try
	{
		// Giving up is not fatal
		// The data can be retrieved at the next sync
		if ( ! haveLock )
		{
			syslog( LOG_ALERT, "ProcessSyncFile - can't get file lock");
			throw( -3 );
		}
		
		// copy our header
		err = this->getHeader( &localHeader );
		if ( err != 0 && err != -3 )
			throw( err );
		
		// read opposing sync time
		readCount = fread( &syncTime, sizeof(syncTime), 1, syncFile );
		if ( readCount != 1 )
			throw( -1 );
		
		// read opposing header
		readCount = fread( &remoteHeader, sizeof(remoteHeader), 1, syncFile );
		if ( readCount != 1 )
			throw( -1 );
		
		// check compatibility
		if ( localHeader.signature != remoteHeader.signature ||
			 localHeader.version != remoteHeader.version ||
			 localHeader.entrySize != remoteHeader.entrySize )
		{
			throw( -2 );
		}
		
		// sync the header
		localHeader.sequenceNumber = Max(localHeader.sequenceNumber, remoteHeader.sequenceNumber);
		localHeader.deepestSlotUsed = Max(localHeader.deepestSlotUsed, remoteHeader.deepestSlotUsed);
		
		if ( remoteHeader.accessModDate > localHeader.accessModDate )
			localHeader.access = remoteHeader.access;
		
		err = this->setHeader( &localHeader );
		
		if ( remoteHeader.numberOfSlotsCurrentlyInFile > localHeader.numberOfSlotsCurrentlyInFile )
			this->expandDatabase( remoteHeader.numberOfSlotsCurrentlyInFile - localHeader.numberOfSlotsCurrentlyInFile, NULL );
		
		pwUnlock();
		
		// either update the record or trump it
		while (true)
		{
			bNeedsUpdate = false;
			
			readCount = fread( &remoteRec, sizeof(remoteRec), 1, syncFile );
			if ( readCount != 1 )
				throw( -1 );
			
			readCount = fread( &kerbRecordLen, sizeof(kerbRecordLen), 1, syncFile );
			if ( readCount != 1 )
				throw( -1 );
				
			localKerbRec = NULL;
			if (kerbRecordLen > 0)
				remoteKerbRec = kerbList.ReadPrincipalFromFile(syncFile, kerbRecordLen);
			else
				remoteKerbRec = NULL;
			
			if (remoteKerbRec != NULL)
				CKerberosPrincipal::ReadPrincipalFromDB(remoteKerbRec->GetName(), &localKerbRec);
			
			localRec = remoteRec;
			
			if (remoteRec.slot != (UInt32)-1)
			{	
				// this is a normal user record
				err = this->getValidPasswordRec( &localRec, &bFromSpillBucket, false );
				
				if ( err == 0 && localRec.doNotReplicate )
					continue;
				
				if ( err != 0 )
				{
					// record not in the database yet
					localRec = remoteRec;
					bNeedsUpdate = true;
				}
				
				// password fields
				localTime = ::timegm( (struct tm *)&localRec.modDateOfPassword );
				remoteTime = ::timegm( (struct tm *)&remoteRec.modDateOfPassword ) - inTimeSkew;
				::gmtime_r( &remoteTime, (struct tm *)&remoteRec.modDateOfPassword );
				
				if ( remoteTime > localTime )
				{
					memcpy( &localRec.modDateOfPassword, &remoteRec.modDateOfPassword, sizeof(BSDTimeStructCopy) );
					memcpy( localRec.passwordStr, remoteRec.passwordStr, sizeof(localRec.passwordStr) );
					for ( int idx = 0; idx < kPWFileMaxDigests; idx++ )
						localRec.digest[idx] = remoteRec.digest[idx];
						
					bNeedsUpdate = true;
				}
				else if (localKerbRec != NULL)
					remoteKerbRec->CopyPassword(localKerbRec);
				
				// last login time
				localTime = ::timegm( (struct tm *)&localRec.lastLogin );
				remoteTime = ::timegm( (struct tm *)&remoteRec.lastLogin ) - inTimeSkew;
				::gmtime_r( &remoteTime, (struct tm *)&remoteRec.lastLogin );
				
				if ( remoteTime > localTime )
				{
					memcpy( &localRec.lastLogin, &remoteRec.lastLogin, sizeof(BSDTimeStructCopy) );
					bNeedsUpdate = true;
				}
				else if (localKerbRec != NULL)
					remoteKerbRec->CopyLastLogin(localKerbRec);
				
				// all non-special fields
				localTime = ::timegm( (struct tm *)&localRec.modificationDate );
				remoteTime = ::timegm( (struct tm *)&remoteRec.modificationDate ) - inTimeSkew;
				::gmtime_r( &remoteTime, (struct tm *)&remoteRec.modificationDate );
				
				if ( remoteTime > localTime )
				{
					memcpy( &localRec.modificationDate, &remoteRec.modificationDate, sizeof(BSDTimeStructCopy) );
					memcpy( &localRec.access, &remoteRec.access, sizeof(PWAccessFeatures) );
					memcpy( &localRec.usernameStr, &remoteRec.usernameStr, sizeof(localRec.usernameStr) );
					memcpy( &localRec.userdata, &remoteRec.userdata, sizeof(localRec.userdata) );
					localRec.failedLoginAttempts = remoteRec.failedLoginAttempts;
					localRec.recordIsDead = remoteRec.recordIsDead;
					localRec.doNotReplicate = remoteRec.doNotReplicate;
					localRec.unused511 = remoteRec.unused511;
		
					bNeedsUpdate = true;
				}
				
				// do not replicate deleted users back into Kerberos DB
				if ( localRec.recordIsDead && remoteKerbRec != NULL )
					delete remoteKerbRec;
				
				if ( bNeedsUpdate )
				{
					if ( outNumAccepted != NULL )
						(*outNumAccepted)++;
					
					pwWait();
					haveLock = pwLock( kLongerLockInterval );
					pwSignal();
					
					if ( ! haveLock )
					{
						syslog( LOG_ALERT, "ProcessSyncFile - can't get file lock 2");
						throw( (int) -3 );
					}
					
					err = this->addPasswordAtSlot( &localRec, localRec.slot, false, false );
				}
				else
				{
					if (remoteKerbRec != NULL)
						delete remoteKerbRec;	// remove from list to update
					
					if ( outNumTrumped != NULL )
						(*outNumTrumped)++;
				}
			}
			else if (remoteKerbRec != NULL)
			{
				// this is a non-user kerberos record.  We just check the modate for a winner.
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
		kerbList.WriteAllPrincipalsToDB();
	}
	catch( int error )
	{
		result = error;
	}
	
	pwUnlock();
	
	fclose( syncFile );
	
	// clear sensitive info
	bzero( &localHeader, sizeof(localHeader) );
	bzero( &remoteHeader, sizeof(remoteHeader) );
	bzero( &localRec, sizeof(localRec) );
	bzero( &remoteRec, sizeof(remoteRec) );
	
	return result;
}
#endif

#pragma mark -
#pragma mark ¥POLICY TESTING¥
#pragma mark -

//------------------------------------------------------------------------------------------------
//	DisableStatus
//
//	Returns: kAuthOK, kAuthUserDisabled, kAuthPasswordExpired
//  <outReasonCode> is only valid if the return value is kAuthUserDisabled.
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::DisableStatus(PWFileEntry *inOutPasswordRec, Boolean *outChanged, PWDisableReasonCode *outReasonCode)
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
    if ( access->isAdminUser )
        return kAuthOK;
    
	if ( inOutPasswordRec->access.isDisabled )
		return kAuthUserDisabled;
	
	result = pwsf_TestDisabledStatusWithReasonCode( access, &pwFileHeader.access, (struct tm *)&inOutPasswordRec->creationDate, (struct tm *)&inOutPasswordRec->lastLogin, &inOutPasswordRec->failedLoginAttempts, &reason );
	
	// update the user record
	if ( result == kAuthUserDisabled )
	{
		bool markDisabled = true;
		
		// Note: maxMinutesOfNonUse is special
		// If a user logs in in the nick-of-time on a replica, then synchronizing should un-disable the
		// account. Therefore, we do not want to toggle the disabled bit.
		if ( access->maxMinutesOfNonUse > 0 )
		{
			if ( LoginTimeIsStale( &inOutPasswordRec->lastLogin, access->maxMinutesOfNonUse ) )
				markDisabled = false;
		}
		else
		if ( pwFileHeader.access.maxMinutesOfNonUse > 0 &&
			 LoginTimeIsStale( &inOutPasswordRec->lastLogin, pwFileHeader.access.maxMinutesOfNonUse ) )
		{
			markDisabled = false;
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


//------------------------------------------------------------------------------------
//	pwsf_TestDisabledStatus
//
//	Returns: kAuthUserDisabled or kAuthOK
//
//  <inOutFailedLoginAttempts> is set to 0 if the failed login count is exceeded.
//------------------------------------------------------------------------------------

int pwsf_TestDisabledStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inCreationDate, struct tm *inLastLoginTime, UInt16 *inOutFailedLoginAttempts )
{
	PWDisableReasonCode ignored;
	
	return pwsf_TestDisabledStatusWithReasonCode( inAccess, inGAccess, inCreationDate, inLastLoginTime,
				inOutFailedLoginAttempts, &ignored );
}


//------------------------------------------------------------------------------------
//	pwsf_TestDisabledStatusWithReasonCode
//
//	Returns: kAuthUserDisabled or kAuthOK
//
//  <inOutFailedLoginAttempts> is set to 0 if the failed login count is exceeded.
//  <outReasonCode> is only valid if the return value is kAuthUserDisabled.
//------------------------------------------------------------------------------------

int pwsf_TestDisabledStatusWithReasonCode( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inCreationDate, struct tm *inLastLoginTime, UInt16 *inOutFailedLoginAttempts, PWDisableReasonCode *outReasonCode )
{
	bool setToDisabled = false;
	
	*outReasonCode = kPWDisabledNotSet;
	
    // test policies in the user record
	if ( inAccess->maxFailedLoginAttempts > 0 )
	{
		if ( *inOutFailedLoginAttempts >= inAccess->maxFailedLoginAttempts )
		{
			// for failed login attempts, if the maximum is exceeded, set the isDisabled flag on the record
			// and reset <maxFailedLoginAttempts> so that the account can be re-enabled later.
			*inOutFailedLoginAttempts = 0;
			*outReasonCode = kPWDisabledTooManyFailedLogins;
			setToDisabled = true;
		}
    }
	else
	// test policies in the global record
    if ( inGAccess->maxFailedLoginAttempts > 0 &&
         *inOutFailedLoginAttempts >= inGAccess->maxFailedLoginAttempts )
    {
        // for failed login attempts, if the maximum is exceeded, set the isDisabled flag on the record
        // and reset <maxFailedLoginAttempts> so that the account can be re-enabled later.
		*inOutFailedLoginAttempts = 0;
		*outReasonCode = kPWDisabledTooManyFailedLogins;
		setToDisabled = true;
    }
	
	// usingHardExpirationDate
	if ( inAccess->usingHardExpirationDate )
	{
		if ( TimeIsStale(&(inAccess->hardExpireDateGMT)) )
		{
			*outReasonCode = kPWDisabledExpired;
			setToDisabled = true;
		}
	}
	else
	if ( inGAccess->usingHardExpirationDate && TimeIsStale(&inGAccess->hardExpireDateGMT) )
	{
		*outReasonCode = kPWDisabledExpired;
		setToDisabled = true;
	}
	
	// maxMinutesUntilDisabled
	if ( inAccess->maxMinutesUntilDisabled > 0 )
	{
		if ( LoginTimeIsStale((BSDTimeStructCopy *)inCreationDate, inAccess->maxMinutesUntilDisabled) )
		{
			*outReasonCode = kPWDisabledExpired;
			setToDisabled = true;
		}
	}
	else
	if ( inGAccess->maxMinutesUntilDisabled > 0 && LoginTimeIsStale((BSDTimeStructCopy *)inCreationDate, inGAccess->maxMinutesUntilDisabled) )
	{
		*outReasonCode = kPWDisabledExpired;
		setToDisabled = true;
	}
	
	if ( inAccess->maxMinutesOfNonUse > 0 )
	{
		if ( LoginTimeIsStale( (BSDTimeStructCopy *)inLastLoginTime, inAccess->maxMinutesOfNonUse) )
		{
			*outReasonCode = kPWDisabledInactive;
			setToDisabled = true;
		}
	}
	else
    if ( inGAccess->maxMinutesOfNonUse > 0 &&
		 LoginTimeIsStale( (BSDTimeStructCopy *)inLastLoginTime, inGAccess->maxMinutesOfNonUse) )
    {
		*outReasonCode = kPWDisabledInactive;
		setToDisabled = true;
    }
	
	return ( setToDisabled ? kAuthUserDisabled : kAuthOK );
}


//------------------------------------------------------------------------------------------------
//	ChangePasswordStatus
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange, kAuthPasswordExpired
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::ChangePasswordStatus(PWFileEntry *inPasswordRec)
{
	int result;
	
	if ( inPasswordRec->access.isAdminUser )
		return kAuthOK;
	
	result = pwsf_ChangePasswordStatus( &inPasswordRec->access, &pwFileHeader.access, (struct tm *)&inPasswordRec->modDateOfPassword );
	
    return result;
}


//------------------------------------------------------------------------------------------------
//	pwsf_ChangePasswordStatus
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange, kAuthPasswordExpired
//------------------------------------------------------------------------------------------------

int pwsf_ChangePasswordStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inModDateOfPassword )
{
	bool needsChange = false;
	
	if ( inAccess->newPasswordRequired )
	{
		needsChange = true;
	}
	else
	{
		// usingExpirationDate
		if ( inAccess->usingExpirationDate )
		{
			if ( TimeIsStale( &inAccess->expirationDateGMT ) )
				needsChange = true;
		}
		else
		if ( inGAccess->usingExpirationDate && TimeIsStale( &inGAccess->expirationDateGMT ) )
		{
			needsChange = true;
		}
		
		// maxMinutesUntilChangePassword
		if ( inAccess->maxMinutesUntilChangePassword > 0 )
		{
			if ( LoginTimeIsStale( (BSDTimeStructCopy *)inModDateOfPassword, inAccess->maxMinutesUntilChangePassword ) )
				needsChange = true;
		}
		else
		if ( inGAccess->maxMinutesUntilChangePassword > 0 && LoginTimeIsStale( (BSDTimeStructCopy *)inModDateOfPassword, inGAccess->maxMinutesUntilChangePassword ) )
		{
			needsChange = true;
		}
	}
	
	if ( needsChange )
    {
        if ( inAccess->canModifyPasswordforSelf )
            return kAuthPasswordNeedsChange;
        else
        	return kAuthPasswordExpired;
    }
    
    // not implemented
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	RequiredCharacterStatus
//
//	Returns: enum of Reposonse Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::RequiredCharacterStatus(PWFileEntry *inPasswordRec, const char *inPassword)
{
	return pwsf_RequiredCharacterStatusExtra( &(inPasswordRec->access), &(pwFileHeader.access), inPasswordRec->usernameStr, inPassword, &(inPasswordRec->extraAccess) );
}


//------------------------------------------------------------------------------------------------
//	pwsf_RequiredCharacterStatus
//
//	Returns: enum of Reposonse Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

int pwsf_RequiredCharacterStatus(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword)
{
    Boolean requiresAlpha = (access->requiresAlpha || inGAccess->requiresAlpha );
    Boolean requiresNumeric = (access->requiresNumeric || inGAccess->requiresNumeric );
    UInt16 minChars = (access->minChars > 0) ? access->minChars : inGAccess->minChars;
    UInt16 maxChars = (access->maxChars > 0) ? access->maxChars : inGAccess->maxChars;
	Boolean passwordCannotBeName = (access->passwordCannotBeName || inGAccess->passwordCannotBeName );
    UInt16 len;
	
	if ( inPassword == NULL )
		return kAuthPasswordTooShort;
		
	len = strlen(inPassword);
    
	// The password server is not accepting blank passwords because some auth 
	// methods, such as DIGEST-MD5, will not authenticate them.
	if ( len == 0 )
		return kAuthPasswordTooShort;
	
    if ( len < minChars )
        return kAuthPasswordTooShort;
    
    if ( maxChars > 0 && len > maxChars )
        return kAuthPasswordTooLong;
    
    if ( requiresAlpha )
    {
        Boolean hasAlpha = false;
        
        for ( int index = 0; index < len; index++ )
        {
            if ( isalpha(inPassword[index]) )
            {
                hasAlpha = true;
                break;
            }
        }
        
        if ( !hasAlpha )
            return kAuthPasswordNeedsAlpha;
    }
	
    if ( requiresNumeric )
    {
        Boolean hasDecimal = false;
        
        for ( int index = 0; index < len; index++ )
        {
            if ( isdigit(inPassword[index]) )
            {
                hasDecimal = true;
                break;
            }
        }
        
        if ( !hasDecimal )
            return kAuthPasswordNeedsDecimal;
    }
	
	if ( passwordCannotBeName )
	{
		UInt16 unameLen = strlen( inUsername );
		UInt16 smallerLen = ((len < unameLen) ? len : unameLen);
		
		// disallow the smaller substring, case-insensitive
		if ( strncasecmp( inPassword, inUsername, smallerLen ) == 0 )
			return kAuthPasswordNeedsChange;
	}
	
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	pwsf_RequiredCharacterStatusExtra
//
//	Returns: enum of Response Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

int pwsf_RequiredCharacterStatusExtra(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword, PWMoreAccessFeatures *inExtraFeatures )
{
	int responseCode;
		
	responseCode = pwsf_RequiredCharacterStatus( access, inGAccess, inUsername, inPassword );
	if ( responseCode != kAuthOK )
		return responseCode;
	
	UInt16 len = strlen( inPassword );
	
	if ( inGAccess->requiresMixedCase || inExtraFeatures->requiresMixedCase )
	{
		Boolean hasUpper = false;
        Boolean hasLower = false;
        
        for ( int index = 0; index < len; index++ )
        {
            if ( inPassword[index] >= 'A' && inPassword[index] <= 'Z' )
                hasUpper = true;
			else
			if ( inPassword[index] >= 'a' && inPassword[index] <= 'z' )
				hasLower = true;
			
			if ( hasUpper && hasLower )
				break;
        }
        
        if ( !(hasUpper && hasLower) )
            return kAuthPasswordNeedsMixedCase;
	}
	
	/*if ( inGAccess->notGuessablePattern || inExtraFeatures->notGuessablePattern )
	{
	}*/
	
	return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	ReenableStatus
//
//	Returns: bool
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::ReenableStatus(PWFileEntry *inPasswordRec, unsigned long inGlobalReenableMinutes)
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


/*----------------------------------------------------------------------------------*/

#pragma mark -
#pragma mark ¥SPILL-BUCKET ACCESSORS¥
#pragma mark -

int
CAuthFileBase::getPasswordRecFromSpillBucket(PWFileEntry *inRec, PWFileEntry *passRec)
{
	PWFileEntry recBuff;
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	char uidStr[35];
	char buff[35];
	unsigned int encodeLen;
	
	if ( inRec == NULL || passRec == NULL )
		return -1;
	
	err = this->OpenOverflowFile( inRec, false, &fp );
	if ( err != 0 )
		return err;
	
	// use text-based matching to avoid endian problems
	fUtils.passwordRecRefToString( inRec, uidStr );
	
	do
	{
		byteCount = pread( fileno(fp), buff, sizeof(buff), offset );
		
		if ( strncmp( uidStr, buff, 34 ) == 0 )
		{
			// found it
			byteCount = pread( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
			
			// for any endian
			//fUtils.stringToPasswordRecRef( uidStr, recBuff );
			
			// recover the password
            encodeLen = strlen(recBuff.passwordStr);
            encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
            if ( encodeLen > sizeof(recBuff.passwordStr) )
                encodeLen = sizeof(recBuff.passwordStr);
            
            fUtils.DESDecode(kFixedDESKey, recBuff.passwordStr, encodeLen);
			
			// copy the record
			memcpy( passRec, &recBuff, sizeof(PWFileEntry) );
			
			// zero our copy
			memset( &recBuff, 0, sizeof(recBuff) );
			
			err = 0;
			break;
		}
		
		offset += sizeof(PWFileEntry) + 34;
	}
	while ( byteCount == sizeof(uidStr) );
	
	fclose( fp );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	SaveOverflowRecord
//
//	Returns: -1, errno, or 0 for success
//
//	Updates and existing record in the overflow bucket
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::SaveOverflowRecord( PWFileEntry *inPasswordRec, bool obfuscate, bool setModDate )
{
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	char uidStr[35];
	char buff[35];
	unsigned int encodeLen;
    int writeCount;
    
	if ( inPasswordRec == NULL )
		return -1;
	
	if ( inPasswordRec->slot <= 0 )
    	return -1;	
	
	err = this->OpenOverflowFile( inPasswordRec, true, &fp );
	if ( err != 0 )
		return err;
	
	// use text-based matching to avoid endian problems
	fUtils.passwordRecRefToString( inPasswordRec, uidStr );
	
	if ( setModDate )
		fUtils.getGMTime( (struct tm *)&inPasswordRec->modificationDate );
	
	encodeLen = strlen(inPasswordRec->passwordStr);
	encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
	if ( encodeLen > sizeof(inPasswordRec->passwordStr) )
		encodeLen = sizeof(inPasswordRec->passwordStr);
	
	if ( obfuscate )
		fUtils.DESEncode(kFixedDESKey, inPasswordRec->passwordStr, encodeLen);
	
	err = -1;
	do
	{
		byteCount = pread( fileno(fp), buff, sizeof(buff), offset );
		
		if ( strncmp( uidStr, buff, 34 ) == 0 )
		{
			// found it
			byteCount = pwrite( fileno(fp), inPasswordRec, sizeof(PWFileEntry), offset+34 );
			err = 0;
			break;
		}
		
		offset += 34+sizeof(PWFileEntry);
	}
	while ( byteCount == sizeof(uidStr) );

	// if not found, append the new record
	if ( err == -1 )
	{
		err = fseek( fp, 0, SEEK_END );
		if ( err == 0 )
		{
			writeCount = fwrite( uidStr, 34, 1, fp );
			if ( writeCount != 1 )
				err = -1;
		}
		
		if ( err == 0 )
		{
			writeCount = fwrite( inPasswordRec, sizeof(PWFileEntry), 1, fp );
			if ( writeCount != 1 )
				err = -1;
		}
	}
	
	if ( obfuscate )
		fUtils.DESDecode(kFixedDESKey, inPasswordRec->passwordStr, encodeLen);
	
	fclose( fp );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	OpenOverflowFile
//------------------------------------------------------------------------------------------------

int
CAuthFileBase::OpenOverflowFile( PWFileEntry *inPasswordRec, bool create, FILE **outFP )
{
	char overflowFileName[50];
	char overflowPath[1024];
	FILE *fp;
	int err = -1;
	
	if ( inPasswordRec == NULL || outFP == NULL )
		return -1;
	
	*outFP = NULL;
	
	this->PWRecToOverflowFileName( inPasswordRec, overflowFileName );
	sprintf( overflowPath, "%s/%s", kPWDirPath, overflowFileName );
	
	fp = fopen( overflowPath, create ? "a+" : "r+" );
	if ( fp == NULL )
	{
		err = errno;
		if ( err == 0 )
			err = -1;
		return err;
	}
	
	*outFP = fp;
	return 0;
}


//------------------------------------------------------------------------------------------------
//	PWRecToOverflowFileName
//
//	Returns: void
//	outFileName		<- 		The name of the file that contains the overflow for a given slot
//
//	The user ID overflows are put into multiple files as an optimization because we are doing
//	sequential searching.
//------------------------------------------------------------------------------------------------

void
CAuthFileBase::PWRecToOverflowFileName( PWFileEntry *inPasswordRec, char *outFileName )
{
	if ( outFileName == NULL )
		return;
	
	strcpy( outFileName, "authserveroverflow.exception" );
	
	if ( inPasswordRec == NULL )
		return;
	
	long simpleHash = ( (inPasswordRec->time ^ inPasswordRec->rnd) * (inPasswordRec->sequenceNumber + inPasswordRec->slot) ) % 100;
	
	sprintf( outFileName, "authserveroverflow.%lu", simpleHash );
}



