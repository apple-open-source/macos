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


#ifndef __CAuthFileBase__
#define __CAuthFileBase__

//#include <Carbon/Carbon.h>
#include "AuthFile.h"

extern "C" {
#include "DES.h"
#include "key.h"
//#include "CWeakDict.h"
};
#include "CAuthFileUtils.h"

// Reposonse Codes (used numerically)
enum {
    kAuthOK = 0,
    kAuthFail = -1,
    kAuthUserDisabled = -2,
    kAuthNeedAdminPrivs = -3,
    kAuthUserNotSet = -4,
    kAuthUserNotAuthenticated = -5,
    kAuthPasswordExpired = -6,
    kAuthPasswordNeedsChange = -7,
    kAuthPasswordNotChangeable = -8,
    kAuthPasswordTooShort = -9,
    kAuthPasswordTooLong = -10,
    kAuthPasswordNeedsAlpha = -11,
    kAuthPasswordNeedsDecimal = -12,
    kAuthMethodTooWeak = -13
};


class CAuthFileBase
{
    public:
    
                                        CAuthFileBase();
                                        CAuthFileBase( const char *inDBFilePath );
        virtual                         ~CAuthFileBase();
    	
		virtual	void					Init(void);
        virtual int						validateFiles(void);
        virtual int						validatePasswordFile(void);
        virtual int						validateFreeListFile(void);
        virtual int						createPasswordFile(void);
        virtual int						openPasswordFile(const char *mode, Boolean map);
        virtual int						mapPasswordFile(void);
        virtual void					closePasswordFile(void);
        virtual void					closeFreeListFile(void);
		
        virtual void					resetPasswordFileState(void);
        virtual void					carryOn(void);
		virtual void					pwLock(void);
		virtual bool					pwLock( unsigned long inMillisecondsToWait );
        virtual void					pwUnlock(void);
        virtual void					pwWait(void);
        virtual void					pwSignal(void);
        virtual void					rsaWait(void);
        virtual void					rsaSignal(void);
        
        virtual int						getHeader( PWFileHeader *outHeader, bool inCanUseCachedCopy = false );
        virtual int						setHeader( const PWFileHeader *inHeader );
        
        virtual int						getRSAPublicKey( char *outRSAKeyStr );
        virtual int						loadRSAKeys( void );
        virtual int						decryptRSA( unsigned char *inBlob, int inBlobLen, unsigned char *outBlob );
        virtual int						encryptRSA( unsigned char *inBlob, int inBlobLen, unsigned char *outBlob );
        
        virtual int						isWeakAuthMethod( const char *inMethod );
        virtual int						addWeakAuthMethod( const char *inMethod );
        virtual int						removeWeakAuthMethod( const char *inMethod );
        
		virtual int						expandDatabase( unsigned long inNumSlots, long *outSlot );
        virtual long					nextSlot(void);
        virtual void					getGMTime(struct tm *inOutGMT);
        virtual UInt32					getTimeForRef(void);
        virtual UInt32					getRandom(void);
        
        virtual int						addRSAKeys(void);
		virtual int						addRSAKeys(unsigned char *publicKey, unsigned long publicKeyLen, unsigned char *privateKey, unsigned long privateKeyLen );
        virtual int						addGenesisPassword(const char *username, const char *password, PWFileEntry *outPWRec = NULL);
        
        virtual int						addPassword(PWFileEntry *passwordRec, bool obfuscate = true);
        virtual int						addPasswordAtSlot(PWFileEntry *passwordRec, long slot, bool obfuscate = true, bool setModDate = true);
        virtual int						setPasswordAtSlot(PWFileEntry *passwordRec, long slot, bool obfuscate = true, bool setModDate = true);
		//virtual void					addHashes( const char *inRealm, PWFileEntry *inOutPasswordRec );
		virtual void					addHashDigestMD5( const char *inRealm, PWFileEntry *inOutPasswordRec );
		virtual void					addHashCramMD5( PWFileEntry *inOutPasswordRec );
		static void						getHashCramMD5( const unsigned char *inPassword, long inPasswordLen, unsigned char *outHash, unsigned long *outHashLen );
		virtual bool					ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr );
        virtual int						getPasswordRec(long slot, PWFileEntry *passRec, bool unObfuscate = true);
        virtual int						getValidPasswordRec(PWFileEntry *passwordRec, bool *outFromSpillBucket = NULL, bool unObfuscate = true);
        virtual int						freeSlot(PWFileEntry *passwordRec);
        virtual void					passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr);
        virtual int						stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec);
        virtual int						getUserIDFromName(const char *inName, bool inAllUsers, long inMaxBuffSize, char *outID);				
        virtual int						AddPassword( const char *inUser, const char *inPassword, char *outPasswordRef );
		//virtual int						MakeSyncFile( const char *inFileName, time_t inAfterDate, long inTimeSkew, long *outNumRecordsUpdated );
		virtual int						GetSyncTimeFromSyncFile( const char *inSyncFile, time_t *outSyncTime );
		//virtual int						ProcessSyncFile( const char *inSyncFile, long inTimeSkew, long *outNumAccepted, long *outNumTrumped );
		
        // policy testers
        virtual int						DisableStatus(PWFileEntry *inOutPasswordRec, Boolean *outChanged);
        virtual int						ChangePasswordStatus(PWFileEntry *inPasswordRec);
        virtual int						RequiredCharacterStatus(PWFileEntry *inPasswordRec, const char *inPassword);
        
		//virtual CAuthFileOverflow*		GetOverflowObject( void ) { return &fOverflow; };
		virtual CAuthFileUtils*			GetUtilsObject( void ) { return &fUtils; };
		
		int								getPasswordRecFromSpillBucket(PWFileEntry *inRec, PWFileEntry *passRec);
		int								SaveOverflowRecord( PWFileEntry *inPasswordRec, bool obfuscate = true, bool setModDate = true );
		int								OpenOverflowFile( PWFileEntry *inPasswordRec, bool create, FILE **outFP );
		void							PWRecToOverflowFileName( PWFileEntry *inPasswordRec, char *outFileName );
		
    protected:
        FILE *pwFile;
        FILE *freeListFile;
        caddr_t pwFileBasePtr;
        size_t pwFileLen;
        PWFileHeader pwFileHeader;
        Boolean pwFileValidated;
        PWFileEntry lastRetrievedEntry;
        char pwFilePermission[10];
        RSA *rsaKey;
        bool fWriteSuspended;
		bool fGotHeader;
        time_t fSuspendTime;
		CAuthFileUtils fUtils;
		//CAuthFileOverflow fOverflow;
		char fFilePath[256];
};

#endif

