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

#include <signal.h> 
#include <sys/types.h>
#include <sys/stat.h>     
#include <sys/syslog.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>
#include "KerberosServiceSetup.h"
#include "FTPAccessFile.h"
#include <smb_server_prefs.h>

#include <SystemConfiguration/SystemConfiguration.h>

#define	kAFPConfigPath 		"/Library/Preferences/com.apple.AppleFileServer.plist"
#define kAFPPrincipalKey	"kerberosPrincipal"
#define	kAFPPidFilePath		"/var/run/AppleFileServer.pid"

#define	kFTPConfigPath		"/Library/FTPServer/Configuration/ftpaccess"
#define kFTPPrincipalKey	"ktb5_principal"
#define	kFTPPidFilePath		"/var/run/"

#define	kMailConfigPath		"/etc/MailServicesOther.plist"
#define	kSMTPConfigKey		"postfix"
#define kSMTPPrincipalKey	"smtp_principal"
#define	kIMAPConfigKey		"cyrus"
#define	kIMAPPrincipalKey	"imap_principal"
#define	kPOPConfigKey		"cyrus"
#define	kPOPPrincipalKey	"pop_principal"

#define kSMBConfigTool		"/usr/share/servermgrd/cgi-bin/servermgr_smb"

#define	kVPNConfigPath		"/Library/Preferences/SystemConfiguration/com.apple.RemoteAccessServers.plist"
#define	kVPNPrincipalKey	"KerberosServicePrincipalName"
#define	kVPNServerKey		"Servers"
#define	kVPNServiceKey		"com.apple.ppp.l2tp"
#define	kVPNEAPKey			"EAP"

#define	kXGridConfigPath	"/etc/xgrid/controller/service-principal"

#define	kApacheConfigTool	"/usr/sbin/apache-kerberos"

static CFDictionaryRef			CreateMyPropertyListFromFile( CFURLRef fileURL, Boolean isMutable);
static CFMutableDictionaryRef	CreateVPNDefaults(void);
static CFErrorRef				OpenXGridPrincipalFile(int openFlags, int *configFileRef);
static CFMutableDictionaryRef	CreateDictionaryFromFD(int inFd, Boolean inMakeMutable); 
static CFErrorRef				MyCFErrorCreate(CFStringRef domain, CFIndex code, CFStringRef description);

#pragma mark -
#pragma mark File Services
#pragma mark -

CFErrorRef SetAFPPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef				theError = NULL;
    CFMutableDictionaryRef	theConfig = NULL;
    CFURLRef				thePathURL = NULL;
    CFDataRef				theData = NULL;
	CFStringRef				logString = NULL;
    const void				*keys[1];
    const void				*values[1];
    CFIndex					dataLength;
    int						fd;
    pid_t					serverPid = 0;
    size_t					len;
    UInt8					theBuffer[16];
    mode_t					mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 -rw-r--r--
    
    thePathURL = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)kAFPConfigPath, strlen(kAFPConfigPath), false);
    
    // if the config file exists open it & parse it
    theConfig = (CFMutableDictionaryRef) CreateMyPropertyListFromFile(thePathURL, true);
    if (theConfig != NULL)
    {
        // and add the principal name or overwrite
        CFDictionarySetValue(theConfig, CFSTR(kAFPPrincipalKey), inPrincipal);
    } else {
        // else create minimal config dictionary
        keys[0] = CFSTR(kAFPPrincipalKey);
        values[0] = inPrincipal;
        theConfig = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    theData = CFPropertyListCreateXMLData(NULL, theConfig);
    if (theData != NULL)
    {
		dataLength = CFDataGetLength(theData);
        // open - truncate 
        
        if ((fd = open(kAFPConfigPath, O_RDWR | O_CREAT | O_TRUNC, mode)) == -1) 
        {
			logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("Error writing AFP config file at %s error = %d\n"),
							kAFPConfigPath, errno);
			
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
			
			if ( logString != NULL )
				CFRelease( logString );
        }
		else
		{
            write(fd, CFDataGetBytePtr(theData), dataLength);
            close(fd);
        }
        CFRelease(theData);
    }
    CFRelease(theConfig);
    // cause the afp server to reread its prefs: just need to HUP it.
    // get the pid of the afp server process
    if (theError == NULL)
    {
        bzero(theBuffer, sizeof(theBuffer));
		fd = open(kAFPPidFilePath, O_RDONLY, 0);	// open the file read only
        if (fd != -1)	// open failed, file server is not running
        {
            // read the pid from the file
            len = read(fd, theBuffer, sizeof(theBuffer));
            if (len != 0)	
                serverPid = atoi((char *)theBuffer);
			
            close(fd);

            if (serverPid > 0)
				kill(serverPid,SIGHUP);
        }
    }
    
    return theError;
}


CFErrorRef SetFTPPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef	theError = NULL;
    char		buffer[1024];

    if ( CFStringGetCString(inPrincipal, buffer, sizeof(buffer), kCFStringEncodingASCII) == false )
        return MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)-2, NULL);
    
    try
	{
        FTPAccessFile	*configFile = (FTPAccessFile *) new FTPAccessFile();
        
        // set the principal name
        configFile->SetKerberosPrincipal(buffer);

        delete configFile;
    }
    catch(...)
	{
        theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)-1, NULL);
    }
	
    // do not need to HUP the FTP server, every instance is a single connection
    
    return theError;
}


/*		SetNFSPrincipal()	
*/

CFErrorRef SetNFSPrincipal(CFStringRef inPrincipal)
{
	return NULL;
}


/*
	from bug 3575788
	to enable the active directory domaim member configuration role send the following xml command to 
 /usr/share/servermgrd/cgi-bin/servermgr_smb

<?xml version="1.0" encoding="UTF-8"?>
<plist version="0.9">
<dict>
	<key>command</key>
	<string>writeSettings</string>
	<key>configuration</key>
	<dict>
		<key>adminCommands</key>
		<dict>
		      <key>serverRole</key>
		      <string>domainmember</string>
		      <key>changeRole</key>
		      <integer>1</integer>
		</dict>
		<key>domain master</key>
		<false/>
		<key>local master</key>
		<false/>
		<key>realm</key>
		<string>MY.KERBEROS.REALM</string>
		</dict>
	</dict>
</plist>

this will kerberize the smb service and should only be called for MS Kerberos realms.

Please note the 'netbios name' and 'workgroup' are required  and must be retrieved from the AD Plugin.
(not currently available or needed, there were changes in the other tool)

*/

CFErrorRef SetSMBPrincipal(CFStringRef inPrincipal, CFStringRef inAdminName, const char *inPassword)
{

    CFErrorRef				theError = NULL;
	CFArrayRef				tmpArray = NULL;
	SInt32					tmpInt;
	SCPreferencesRef prefs = NULL;
	
	prefs = SCPreferencesCreate(NULL, CFSTR("Password Server Plugin"), CFSTR(kSMBPreferencesAppID));	
	if (NULL == prefs) {
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)-1, CFSTR("SetSMBPrincipal: Cannot create SMB preferences\n"));
	} else {
		// get the realm
		tmpArray = CFStringCreateArrayBySeparatingStrings(NULL, inPrincipal, CFSTR("@"));
		tmpInt = CFArrayGetCount(tmpArray);
		if (tmpInt != 2)
		{
			CFRelease(tmpArray);
			tmpArray = NULL;
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)-1, CFSTR("SetSMBPrincipal: Cannot find the realm name\n"));
		}
		else 
		{
			CFStringRef newValueStrRef = (CFStringRef)CFArrayGetValueAtIndex(tmpArray, tmpInt-1);
			CFPropertyListRef cfpRealmRef = NULL;
			cfpRealmRef = newValueStrRef;
					
			if (SCPreferencesSetValue(prefs, CFSTR(kSMBPrefKerberosRealm), cfpRealmRef)) {
				if (!SCPreferencesCommitChanges(prefs)) {
					syslog(LOG_ERR,"Error in adding the SMB principal");
					theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)-1, CFSTR("SetSMBPrincipal: Cannot set the SMB Principal name\n"));
				}
				else
					SCPreferencesApplyChanges(prefs);
			}
		}
		CFRelease(prefs);
		CFRelease(tmpArray);
	}	

    return theError;
}


#pragma mark -
#pragma mark Mail Services
#pragma mark -

/* 
   We don't neeed to HUP the IMAP service   
*/

CFErrorRef SetIMAPPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef				theError = NULL;
    CFMutableDictionaryRef	theConfig = NULL;
    CFMutableDictionaryRef	theService = NULL;
    CFURLRef				thePathURL = NULL;
    CFDataRef				theData = NULL;
	CFStringRef				logString = NULL;
    const void				*keys[1];
    const void				*values[1];
    CFIndex					dataLength;
    int						fd;
    mode_t					mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 -rw-r--r--
    
    thePathURL = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)kMailConfigPath, strlen(kMailConfigPath), false);
    
    // if the config file exists open it & parse it
    theConfig = (CFMutableDictionaryRef)CreateMyPropertyListFromFile(thePathURL, true);
	CFRelease( thePathURL );
	thePathURL = NULL;
	
    if (theConfig != NULL)
    {
        theService = (CFMutableDictionaryRef)CFDictionaryGetValue(theConfig, CFSTR(kIMAPConfigKey));	// look for the imap config
        if (theService == NULL)
        {
            keys[0] = CFSTR(kIMAPPrincipalKey);
            values[0] = inPrincipal;
            theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionarySetValue(theConfig, CFSTR(kIMAPConfigKey), theService);
        }
		else
		{
            // and add the principal name or overwrite
            CFDictionarySetValue(theService, CFSTR(kIMAPPrincipalKey), inPrincipal);
        }
    }
	else
	{
        // else create minimal config dictionary
        keys[0] = CFSTR(kIMAPPrincipalKey);
        values[0] = inPrincipal;
        theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        keys[0] = CFSTR(kIMAPConfigKey);
        values[0] = theService;
        theConfig = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    theData = CFPropertyListCreateXMLData(NULL, theConfig);
    if (theData != NULL)
    {
		dataLength = CFDataGetLength(theData);
        // open - truncate 
        
        if ((fd = open(kMailConfigPath, O_RDWR | O_CREAT | O_TRUNC, mode)) == -1) 
        {
			logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("Error writing Mail config file at %s error = %d\n"),
							kMailConfigPath, errno);
			
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
			
			if ( logString != NULL )
				CFRelease( logString );
        }
		else
		{
            write(fd, CFDataGetBytePtr(theData), dataLength);
            close(fd);
        }
        CFRelease(theData);
    }
	
    CFRelease(theConfig);
	
    return theError;
}


/* 
   We don't neeed to HUP the POP service   
*/
CFErrorRef SetPOPPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef				theError = NULL;
    CFMutableDictionaryRef	theConfig = NULL;
    CFMutableDictionaryRef	theService = NULL;
    CFURLRef				thePathURL = NULL;
    CFDataRef				theData = NULL;
	CFStringRef				logString = NULL;
    const void				*keys[1];
    const void				*values[1];
    CFIndex					dataLength;
    int						fd;
    mode_t					mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 -rw-r--r--
    
    thePathURL = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)kMailConfigPath, strlen(kMailConfigPath), false);
    
    // if the config file exists open it & parse it
    theConfig = (CFMutableDictionaryRef)CreateMyPropertyListFromFile(thePathURL, true);
	CFRelease( thePathURL );
	thePathURL = NULL;

    if (theConfig != NULL)
    {
        theService = (CFMutableDictionaryRef)CFDictionaryGetValue(theConfig, CFSTR(kPOPConfigKey));	// look for the pop config
        if (theService == NULL)
        {
            keys[0] = CFSTR(kPOPPrincipalKey);
            values[0] = inPrincipal;
            theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionarySetValue(theConfig, CFSTR(kPOPConfigKey), theService);
        } else {
        
            // and add the principal name or overwrite
            CFDictionarySetValue(theService, CFSTR(kPOPPrincipalKey), inPrincipal);
        }
        
        
    } else {
        // else create minimal config dictionary
        keys[0] = CFSTR(kPOPPrincipalKey);
        values[0] = inPrincipal;
        theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        keys[0] = CFSTR(kPOPConfigKey);
        values[0] = theService;
        theConfig = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
	    
    theData = CFPropertyListCreateXMLData(NULL, theConfig);
    if (theData != NULL)
    {
		dataLength = CFDataGetLength(theData);
        // open - truncate 
        
        if ((fd = open(kMailConfigPath, O_RDWR | O_CREAT | O_TRUNC, mode)) == -1) 
        {
			logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("Error writing Mail config file at %s error = %d\n"),
							kMailConfigPath, errno);
			
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
			
			if ( logString != NULL )
				CFRelease( logString );
        }
		else
		{
            write(fd, CFDataGetBytePtr(theData), dataLength);
            close(fd);
        }
        CFRelease(theData);
    }
	
    CFRelease(theConfig);
	
    return theError;
}

/* 
   We don't neeed to HUP the SMTP service   
*/

CFErrorRef SetSMTPPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef				theError = NULL;
    CFMutableDictionaryRef	theConfig = NULL;
    CFMutableDictionaryRef	theService = NULL;
    CFURLRef				thePathURL = NULL;
    CFDataRef				theData = NULL;
	CFStringRef				logString = NULL;
    const void				*keys[1];
    const void				*values[1];
    CFIndex					dataLength;
    int						fd;
    mode_t					mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 -rw-r--r--
    
    thePathURL = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)kMailConfigPath, strlen(kMailConfigPath), false);
    
    // if the config file exists open it & parse it
    theConfig = (CFMutableDictionaryRef)CreateMyPropertyListFromFile(thePathURL, true);
 	CFRelease( thePathURL );
	thePathURL = NULL;

	if (theConfig != NULL)
    {
        theService = (CFMutableDictionaryRef)CFDictionaryGetValue(theConfig, CFSTR(kSMTPConfigKey));	// look for the imap config
        if (theService == NULL)
        {
            keys[0] = CFSTR(kIMAPPrincipalKey);
            values[0] = inPrincipal;
            theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionarySetValue(theConfig, CFSTR(kSMTPConfigKey), theService);
        } else {
        
            // and add the principal name or overwrite
            CFDictionarySetValue(theService, CFSTR(kSMTPPrincipalKey), inPrincipal);
        }
        
        
    } else {
        // else create minimal config dictionary
        keys[0] = CFSTR(kSMTPPrincipalKey);
        values[0] = inPrincipal;
        theService = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        keys[0] = CFSTR(kSMTPConfigKey);
        values[0] = theService;
        theConfig = (CFMutableDictionaryRef)CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
	    
    theData = CFPropertyListCreateXMLData(NULL, theConfig);
    if (theData != NULL)
    {
		dataLength = CFDataGetLength(theData);
        // open - truncate 
        
        if ((fd = open(kMailConfigPath, O_RDWR | O_CREAT | O_TRUNC, mode)) == -1) 
        {
			logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("Error writing Mail config file at %s error = %d\n"),
							kMailConfigPath, errno);
			
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
			
			if ( logString != NULL )
				CFRelease( logString );
        }
		else
		{
            write(fd, CFDataGetBytePtr(theData), dataLength);
            close(fd);
        }
		
        CFRelease(theData);
    }
	
    CFRelease(theConfig);
	
    return theError;
}


#pragma mark -
#pragma mark More Services
#pragma mark -

// placehloders, there isn't anything to do in order to Kerberize these services

CFErrorRef SetSSHPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}

CFErrorRef SetLDAPPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}


CFErrorRef SetHTTPPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}

CFErrorRef SetIPPPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}

CFErrorRef SetJABBERPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}

CFErrorRef SetVNCPrincipal(CFStringRef inPrincipal)
{
    return NULL;
}

/*
	SetVPNPrincipal()
	
	fd = open the file create (mode 644) if it does not exist
	if file length is 0
		workingConfig = CreateVPNDefaults()
	else
		workingConfig = Create Dictionary from fd (mutable)
		
	Navigate within the file to /Servers/com.apple.ppp.l2tp/
		create EAP dictionary if needed
		add KerberosServicePrincipalName key
		
	write out the new workingConfig
	close file
*/

CFErrorRef SetVPNPrincipal(CFStringRef inPrincipal)
{
	CFMutableDictionaryRef	workingDict = NULL;
	CFMutableDictionaryRef	tmpDict1 = NULL;
	CFMutableDictionaryRef	tmpDict2 = NULL;
	CFMutableDictionaryRef	tmpDict3 = NULL;
	CFDataRef				theData = NULL;
    CFErrorRef				theError = NULL;
	CFStringRef				logString = NULL;
	int						theConfigFile;
	CFIndex					dataLength = 0;
	mode_t					mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 -rw-r--r--
	struct stat				fileInfo;
	bool					releaseTmpDict3 = false;
	
	theConfigFile = open(kVPNConfigPath, O_CREAT | O_EXLOCK | O_RDWR, mode);
	if (theConfigFile == -1)
	{
		logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: could not open/create %s errno = %d\n"),
							kVPNConfigPath, errno);
		
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
		
		if ( logString != NULL )
			CFRelease( logString );

		return theError;
	}
	if (fstat(theConfigFile, &fileInfo) == -1) 
	{
		logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: could not stat %s errno = %d\n"),
							kVPNConfigPath, errno);
		
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
		
		if ( logString != NULL )
			CFRelease( logString );

		close(theConfigFile);	// would reset errno
		return theError;
	}
	
	// could complain here if the file was not a regular file (check fileInfo.st_mode)
	if (fileInfo.st_size == 0)	// we just created the file or it is empty and we can stomp on it
	{
		workingDict = CreateVPNDefaults();
	}
	else
	{
		workingDict = CreateDictionaryFromFD(theConfigFile, true);
	}
	
	if (workingDict == NULL)
	{
		logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: File is not a recognizable config file %s\n"),
							kVPNConfigPath, EINVAL);
		
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EINVAL, logString);
		
		if ( logString != NULL )
			CFRelease( logString );
		
		close(theConfigFile);
		return theError;
	}
	
	// add the key
	tmpDict1 = (CFMutableDictionaryRef)CFDictionaryGetValue(workingDict, CFSTR(kVPNServerKey));
	if (tmpDict1 == NULL)
	{
		logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: File is not a recognizable config file %s\n"),
							kVPNConfigPath, EINVAL);
		
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EINVAL, logString);
		
		if ( logString != NULL )
			CFRelease( logString );
			
		close(theConfigFile);
		CFRelease(workingDict);
		return theError;
	}

	tmpDict2 = (CFMutableDictionaryRef)CFDictionaryGetValue(tmpDict1, CFSTR(kVPNServiceKey));
	if (tmpDict2 == NULL)
	{
		logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: File is not a recognizable config file %s\n"),
							kVPNConfigPath, EINVAL);
		
		theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EINVAL, logString);
		
		if ( logString != NULL )
			CFRelease( logString );
			
		close(theConfigFile);
		CFRelease(workingDict);
		return theError;
	}
	
	tmpDict3 = (CFMutableDictionaryRef)CFDictionaryGetValue(tmpDict2, CFSTR(kVPNEAPKey)); 
	if (tmpDict3 == NULL)	// the EAP directory does not exist, create it
	{
		tmpDict3 = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(tmpDict2, CFSTR(kVPNEAPKey), tmpDict3);
		releaseTmpDict3 = true;
	}
	CFDictionarySetValue(tmpDict3, CFSTR(kVPNPrincipalKey), inPrincipal);

	// only release tmpDict3 if it was created here.
	if (releaseTmpDict3)
	{
		CFRelease(tmpDict3); // retained in tmpDict2
	}
		
	// write out the new plist
	theData = CFPropertyListCreateXMLData(NULL, workingDict);
    if (theData != NULL)
    {
		dataLength = CFDataGetLength(theData);
		lseek(theConfigFile, 0, SEEK_SET);	// start at the beginning
		if (write(theConfigFile, CFDataGetBytePtr(theData), dataLength) != dataLength)	// didn't write the whole file
		{
			logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetVPNPrincipal: failed to write %s, errno = %d\n"),
								kVPNConfigPath, theError);
			
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
			
			if ( logString != NULL )
				CFRelease( logString );
			
			close(theConfigFile);	// would reset errno
			CFRelease(theData);
			CFRelease(workingDict);
			return theError;
		}
		
		CFRelease(theData);
	}
	else
	{
		close(theConfigFile);
		CFRelease(workingDict);
		return MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EIO, NULL);
	}
	
	close(theConfigFile);
	CFRelease(workingDict);
    
	return NULL;
}


CFErrorRef SetXGridPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef		theError = NULL;
	CFStringRef		logString = NULL;
	UInt8			buffer[1024];
	size_t			length;
	int				theConfigFile = -1;
	
	theError = OpenXGridPrincipalFile(O_CREAT | O_EXLOCK | O_RDWR | O_TRUNC, &theConfigFile);
	if ( theError == NULL )
	{
		bzero(buffer, sizeof(buffer));
		if (CFStringGetCString(inPrincipal, (char *)buffer, sizeof(buffer), kCFStringEncodingUTF8) == false)
		{
			theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EINVAL,
							CFSTR("SetXGridPrincipal: could not convert the principal name\n"));
		}
		else
		{
			length = strlen((char *)buffer);
			if (write(theConfigFile, buffer, length) != (ssize_t)length)
			{
				logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetXGridPrincipal: failed to write %s, errno = %d\n"),
									kXGridConfigPath, errno);
				
				theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
				
				if ( logString != NULL )
					CFRelease( logString );
			}
		}
		
		close(theConfigFile);
	}
	
	return theError;
}


CFErrorRef AddXGridPrincipal(CFStringRef inPrincipal)
{
    CFErrorRef		theError = NULL;
	CFStringRef		logString = NULL;
	UInt8			buffer[1024];
	size_t			length;
	int				theConfigFile = -1;
	char			linefeed = '\n';
	struct stat		sb;
	int				err;
	
	err = stat(kXGridConfigPath, &sb);
	if (err != 0)
	{
		theError = SetXGridPrincipal(inPrincipal);
	}
	else
	{
		theError = OpenXGridPrincipalFile(O_CREAT | O_EXLOCK | O_RDWR | O_APPEND, &theConfigFile);
		if (theError == NULL)
		{
			bzero(buffer, sizeof(buffer));
			if (CFStringGetCString(inPrincipal, (char *)buffer, sizeof(buffer), kCFStringEncodingUTF8) == false)
			{
				theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)EINVAL,
								CFSTR("SetXGridPrincipal: could not convert the principal name\n"));
			}
			else
			{
				if (sb.st_size > 0)
					write(theConfigFile, &linefeed, 1);
				
				length = strlen((char *)buffer);
				if (write(theConfigFile, buffer, length) != (ssize_t)length)
				{
					logString = CFStringCreateWithFormat(NULL, NULL, CFSTR("SetXGridPrincipal: failed to write %s, errno = %d\n"),
										kXGridConfigPath, errno);
					
					theError = MyCFErrorCreate(kCFErrorDomainPOSIX, (CFIndex)errno, logString);
					
					if ( logString != NULL )
						CFRelease( logString );
				}
			}
			
			close(theConfigFile);
		}
	}
	
	return theError;
}


#pragma mark -
#pragma mark Support Functions
#pragma mark -

/* create a mutable dictionary of the form:
	{
		CFString		AutoGenerated	"krbservicesetup"
		CFDictionary	Servers
		{
			CFDictionary	com.apple.ppp.l2tp
			{
			}
		}
	}
*/

CFMutableDictionaryRef	CreateVPNDefaults(void)
{
	CFMutableDictionaryRef	tmpDict1 = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFMutableDictionaryRef	tmpDict2 = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFDictionaryAddValue(tmpDict1, CFSTR(kVPNServiceKey), tmpDict2);
	CFRelease(tmpDict2);
	tmpDict2 = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(tmpDict2, CFSTR(kVPNServerKey), tmpDict1);
	CFRelease(tmpDict1);
	CFDictionaryAddValue(tmpDict2, CFSTR("AutoGenerated"), CFSTR("krbservicesetup"));
	
	return tmpDict2;
}


/*
	need to place the principal name into /etc/xgrid/controller/service-principal
	one principal per line, we will stomp on this file.

*/

static CFErrorRef OpenXGridPrincipalFile(int openFlags, int *configFileRef)
{
    CFErrorRef		theError = NULL;
	CFStringRef		logString = NULL;
	mode_t			mode = S_IRUSR | S_IWUSR | S_IRGRP; // 644 -rw-r-----
	
	*configFileRef = open( kXGridConfigPath, openFlags, mode );
	if ( *configFileRef == -1 )
	{
		logString = CFStringCreateWithFormat( NULL, NULL, CFSTR("SetXGridPrincipal: could not open/create %s errno = %d\n"),
								kXGridConfigPath, errno );
		
		theError = MyCFErrorCreate( kCFErrorDomainPOSIX, (CFIndex)errno, logString );
		
		if ( logString != NULL )
			CFRelease( logString );
	}
	
	return theError;
}


static CFMutableDictionaryRef CreateDictionaryFromFD(int inFd, Boolean inMakeMutable)
{
	struct stat				fileInfo;
	UInt8					*buffer = NULL;
	CFIndex					size;
	CFMutableDictionaryRef	theDictionary = NULL;
	CFDataRef				theData = NULL;
	off_t					savedPos;
	
	// find out the size
	if (fstat(inFd, &fileInfo) == -1) 
		return NULL;

	size = fileInfo.st_size;
	
	// malloc the buffer
	buffer = (UInt8 *)calloc(1,size);
	
	// read the file
	savedPos = lseek(inFd, 0, SEEK_SET);
	if (read(inFd, buffer, size) != size)
	{
		free((void *)buffer);
		lseek(inFd, savedPos, SEEK_SET);
		return NULL;
	}
	lseek(inFd, savedPos, SEEK_SET);
	
	// make a CFData
	theData = CFDataCreate (NULL, buffer, size);
	free((void *)buffer);
	if (theData == NULL)
		return NULL;
  	
	// make the dictionary
	if (inMakeMutable == true)
	{
		theDictionary = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData (NULL, theData, kCFPropertyListMutableContainersAndLeaves, NULL);
	}
	else
	{
		theDictionary = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData (NULL, theData, kCFPropertyListImmutable, NULL);
	}
	
	CFRelease(theData);
	
	return theDictionary;
}


static CFDictionaryRef CreateMyPropertyListFromFile( CFURLRef fileURL, Boolean isMutable ) 
{
   CFPropertyListRef propertyList = NULL;
   CFStringRef       errorString = NULL;
   CFDataRef         resourceData = NULL;
   Boolean           status;
   SInt32            errorCode;

   // Read the XML file.
   status = CFURLCreateDataAndPropertiesFromResource(
               kCFAllocatorDefault,
               fileURL,
               &resourceData,            // place to put file data
               NULL,      
               NULL,
               &errorCode);
    if(resourceData != NULL)
    {
		// Reconstitute the dictionary using the XML data.
        if (isMutable == true)
        {
            propertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                        resourceData,
                        kCFPropertyListMutableContainersAndLeaves,
                        &errorString);
        }
		else {
            propertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                        resourceData,
                        kCFPropertyListImmutable,
                        &errorString);
        }

        CFRelease( resourceData );
	}
	
	return (CFDictionaryRef)propertyList;
}


static CFErrorRef MyCFErrorCreate(CFStringRef domain, CFIndex code, CFStringRef description)
{
	CFErrorRef		theError	= NULL;
	CFDictionaryRef logDict		= NULL;

	if ( description != NULL )
	{
		logDict = CFDictionaryCreate(NULL, (CFTypeRef *)&kCFErrorLocalizedDescriptionKey, (CFTypeRef *)&description, 1,
						&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	
	theError = CFErrorCreate(NULL, domain, code, logDict);
	
	if ( logDict != NULL )
		CFRelease( logDict );
		
	return theError;
}

