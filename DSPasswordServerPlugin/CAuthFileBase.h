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

#include <PasswordServer/AuthFile.h>
#include <PasswordServer/AuthDBFileDefs.h>

extern "C" {
#include "DES.h"
#include "key.h"
//#include "CWeakDict.h"
};
#include "CAuthFileUtils.h"

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

		virtual int						expandDatabase( uint32_t inNumSlots, uint32_t *outSlot );
		virtual uint32_t				nextSlot(void);
		virtual void					getGMTime(struct tm *outGMT);
        virtual void					getGMTime(BSDTimeStructCopy *outGMT);
		virtual UInt32					getTimeForRef(void);
		virtual UInt32					getRandom(void);

		virtual int						addRSAKeys(unsigned int inBitCount = 1024);
		virtual int						addRSAKeys(unsigned char *publicKey, uint32_t publicKeyLen, unsigned char *privateKey, uint32_t privateKeyLen );
		virtual int						addGenesisPassword(const char *username, const char *password, PWFileEntry *outPWRec = NULL);

		virtual int						addPassword(PWFileEntry *passwordRec, bool obfuscate = true);
		virtual int						initPasswordRecord(PWFileEntry *passwordRec, bool obfuscate = true);
		virtual int						addPasswordAtSlot(PWFileEntry *passwordRec, uint32_t slot, bool obfuscate = true, bool setModDate = true);
		virtual int						addPasswordAtSlotFast(PWFileEntry *passwordRec, uint32_t slot);
		virtual int						setPasswordAtSlot(PWFileEntry *passwordRec, uint32_t slot, bool obfuscate = true, bool setModDate = true);
		virtual int						setPasswordAtSlotFast(PWFileEntry *passwordRec, uint32_t slot);
		//virtual void					addHashes( const char *inRealm, PWFileEntry *inOutPasswordRec );
		virtual void					addHashDigestMD5(const char *inRealm, PWFileEntry *inOutPasswordRec);
		virtual void					addHashCramMD5(PWFileEntry *inOutPasswordRec);
		static void						getHashCramMD5(const unsigned char *inPassword, size_t inPasswordLen, unsigned char *outHash, size_t *outHashLen );
		virtual bool					ConvertBinaryToHex(const unsigned char *inData, long len, char *outHexStr);
		virtual int						getPasswordRec(uint32_t slot, PWFileEntry *passRec, bool unObfuscate = true);
		virtual int						getValidPasswordRec(PWFileEntry *passwordRec, bool *outFromSpillBucket = NULL, bool unObfuscate = true);
		virtual int						freeSlot(PWFileEntry *passwordRec);
		virtual void					passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr);
		virtual int						stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec);
		virtual int						getUserIDFromName(const char *inName, bool inAllUsers, long inMaxBuffSize, char *outID);
		virtual int						getUserRecordFromPrincipal(const char *inPrincipal, PWFileEntry *inOutUserRec);
		virtual void					requireNewPasswordForAllAccounts(bool inRequireNew);
										
		virtual int						AddPassword( const char *inUser, const char *inPassword, char *outPasswordRef );
		virtual int						NewPasswordSlot( const char *inUser, const char *inPassword, char *outPasswordRef, PWFileEntry *inOutUserRec );

		/*
		virtual int						MakeSyncFile( const char *inFileName,
														time_t inAfterDate,
														long inTimeSkew,
														long *outNumRecordsUpdated,
														unsigned int inKerberosRecordLimit = 0,
														bool *outKerberosOmitted = NULL );
		*/

		virtual int						GetSyncTimeFromSyncFile( const char *inSyncFile, time_t *outSyncTime );

		/*
		virtual int						ProcessSyncFile( const char *inSyncFile,
															long inTimeSkew,
															long *outNumAccepted,
															long *outNumTrumped );
		*/

		// policy testers
		virtual int						DisableStatus(PWFileEntry *inOutPasswordRec, Boolean *outChanged, PWDisableReasonCode *outReasonCode = NULL);
		virtual int						ChangePasswordStatus(PWFileEntry *inPasswordRec);
		virtual int						RequiredCharacterStatus(PWFileEntry *inPasswordRec, const char *inPassword);
		virtual int						ReenableStatus(PWFileEntry *inPasswordRec, unsigned long inGlobalReenableMinutes);

		virtual CAuthFileUtils*			GetUtilsObject( void );
		
		int								getPasswordRecFromSpillBucket(PWFileEntry *inRec, PWFileEntry *passRec, bool unObfuscate = true);
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
		char fFilePath[256];
		bool fDBFileLocked;
		bool fReadOnlyFileSystem;
};

#endif

