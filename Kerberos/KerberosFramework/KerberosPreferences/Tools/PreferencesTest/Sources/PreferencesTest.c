/*
 *  Hello World for the CodeWarrior
 *  © 1997-1998 Metrowerks Corp.
 *
 *  Questions and comments to:
 *       <mailto:support@metrowerks.com>
 *       <http://www.metrowerks.com/>
 */
 
#include <Kerberos/Kerberos.h>
#include <KerberosPreferences/KerberosPreferences.h>

#if TARGET_API_MAC_OSX
    #include <Carbon/Carbon.h>
    #include "FullPOSIXPath.h"
#elif TARGET_API_MAC_OS8 || TARGET_API_MAC_CARBON
    #include <Memory.h>
    #include <MacTypes.h>
    #include <MacErrors.h>
    #include "FullPath.h"
#else
    #error "Unknown OS"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void PrintFiles (FSSpecPtr inFiles, UInt32 numFiles, char* inPrefix);

int main(void)
{
	FSSpecPtr	files;
	UInt32		numFiles;
	OSErr err = KPGetListOfPreferencesFiles (kpUserPreferences, &files, &numFiles);
	PrintFiles (files, numFiles, "User file");
	
	err = KPGetListOfPreferencesFiles (kpSystemPreferences, &files, &numFiles);
	PrintFiles (files, numFiles, "System file");

	err = KPGetListOfPreferencesFiles (kpUserPreferences | kpSystemPreferences, &files, &numFiles);
	PrintFiles (files, numFiles, "User/system file");

	return 0;
}

void PrintFiles (FSSpecPtr inFiles, UInt32 numFiles, char* inPrefix)
{
	UInt32 i;
	for (i = 0; i < numFiles; i++) {
		Handle	path;
		SInt16	pathLength;
		OSErr err;

		printf ("%s %d: ", inPrefix, i + 1);
		err = FSpGetFullPath (&inFiles [i], &pathLength, &path);
		if ((err == noErr) || (err == fnfErr)) {
			HLock (path);
			printf ("%.*s ", pathLength, *path);
			DisposeHandle (path);
		}
		
		if (KPPreferencesFileIsReadable (&inFiles[i]) == noErr)
			printf ("[Readable]");
		
		if (KPPreferencesFileIsWritable (&inFiles[i]) == noErr)
			printf ("[Writable]");
			
		printf ("\n");
	}
}
			

