/*
 * p12.h - catch-all header for declarations of various test modules
 */
 
#ifndef	__P12_P12_H__
#define __P12_P12_H__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

/* in p12Parse.cpp */
extern int p12ParseTop(
	CSSM_DATA		&rawBlob,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef		pwd,
	bool 			verbose);

/* in p12Decode.cpp */
extern OSStatus p12Decode(
	const CSSM_DATA &pfx, 
	CSSM_CSP_HANDLE cspHand,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	bool verbose,
	unsigned loops);

/* in p12Encode.cpp */
extern int p12Reencode(
	const CSSM_DATA &pfx, 
	CSSM_CSP_HANDLE cspHand,
	CFStringRef pwd,			// explicit passphrase
	bool verbose,
	unsigned loops);

/* in p12ImportExport.cpp */
extern int p12Import(
	const char *pfxFile,
	const char *kcName,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	const char *kcPwd);			// optional
	
extern int p12Export(
	const char *pfxFile,
	const char *kcName,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	const char *kcPwd,			// optional
	bool 		noPrompt);		// true --> export all 
	

#endif	/* __P12_P12_H__ */
