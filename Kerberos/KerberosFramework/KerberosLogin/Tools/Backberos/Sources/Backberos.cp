#include <KerberosSupport/KerberosConditionalMacros.h>

#if TARGET_API_MAC_OSX && TARGET_API_MAC_CARBON
    #include <CoreServices/CoreServices.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <string>
#elif TARGET_API_MAC_OS8 || TARGET_API_MAC_CARBON
    #include <CodeFragments.h>
    #include <MacTypes.h>
    #include <Memory.h>
    #include <Errors.h>
    #include <CFM.InitTerm.h>
#else
	#error "Unknown OS"
#endif
#include <string.h>


#include <KerberosSupport/ErrorLib.h>
#include <KerberosSupport/KerberosDebug.h>

#include <KerberosLogin/KLPrincipalTranslation.h>

extern "C" {

#define kBackberosErrorTable 128

#if TARGET_RT_MAC_MACHO
void InitializeBackberos (void)
{
    dprintf ("Backberos: Initializing error tables\n");
    RegisterErrorTableForBundle (CFSTR("edu.mit.Kerberos.Backberos"), 
                                kBackberosErrorTable);
}
#else
pascal OSErr
InitializeBackberos (
	CFragInitBlockPtr	inInitBlock)
{
	FSSpec	libraryFile;
	OSErr err = __initialize (inInitBlock);
	
	if (err != noErr)
		return err;

	if (inInitBlock -> fragLocator.where == kDataForkCFragLocator) {
		libraryFile = *(inInitBlock -> fragLocator.u.onDisk.fileSpec);
	} else if (inInitBlock -> fragLocator.where == kResourceCFragLocator) {
		libraryFile = *(inInitBlock -> fragLocator.u.inSegs.fileSpec);
	}

	err = ::RegisterErrorTable (&libraryFile, kBackberosErrorTable);
	
	return err;
}
#endif

#pragma export on

KLStatus KerberosLoginPrincipalTranslation_InitializePlugin (
	KLPT_APIVersion		inAPIVersion)
{
    dprintf ("Backberos: entering KerberosLoginPrincipalTranslation_InitializePlugin\n");
 	if (inAPIVersion == kKLPT_APIVersion_1) {
		return noErr;
	} else {
		return paramErr;
	}
}

KLStatus KerberosLoginPrincipalTranslation_TranslatePrincipal (
	const char*		inName,
	const char*		inInstance,
	const char*		inRealm,
	const char**	outName,
	const char**	outInstance,
	const char**	outRealm,
	Boolean*		outChanged)
{
	*outChanged = false;
	
	OSErr err = noErr;

	UInt32	nameLength = strlen (inName);
	UInt32	instanceLength = strlen (inInstance);
	UInt32	realmLength = strlen (inRealm);

	char*	newName = NewPtr (nameLength + 1);
	char*	newInstance = NewPtr (instanceLength + 1);
	char*	newRealm = NewPtr (realmLength + 1);

    dprintf ("Backberos: entering KerberosLoginPrincipalTranslation_TranslatePrincipal\n");
    dprintf ("Backberos: got inName = '%s'; inInstance = '%s'; inRealm = '%s'\n",
            inName, inInstance, inRealm);

	if ((newName == nil) || (newInstance == nil) || (newRealm == nil)) {
		err = memFullErr;
	}
	
	if (err == noErr) {
#if TARGET_API_MAC_OSX
        // Switch from OS long to short name (try several combinations)
        strcpy (newName, inName);
        strcpy (newInstance, inInstance);

        std::string inName5 = (std::string)inName + '/' + inInstance;
        std::string inName4 = (std::string)inName + '.' + inInstance;
        
        struct passwd *pw;
        while ((pw = getpwent ()) != NULL) {
            if (strcmp (pw->pw_gecos, inName) == 0) {
                // The long name matches inName, copy in the short name:
                DisposePtr (newName);
                newName = NewPtr (strlen (pw->pw_name) + 1);
                strcpy (newName, pw->pw_name);
                strcpy (newInstance, inInstance);
                break;
            }
            if (strcmp (pw->pw_gecos, inName5.c_str ()) == 0 ||
                strcmp (pw->pw_gecos, inName4.c_str ()) == 0) {
                // The long name matches inName and inInstance, copy in the short name:
                DisposePtr (newName);
                newName = NewPtr (strlen (pw->pw_name) + 1);
                strcpy (newName, pw->pw_name);
                newInstance [0] = 0;	// no instance if we used both
                continue;				// keep trying and only fall through with this if we have to
            }
        }
        endpwent ();
#else
		// Flip the name
		/*for (UInt32 i = 0; i < nameLength; i++) {
            newName [i] = inName [nameLength - i - 1];
		}
		newName [nameLength] = '\0';*/
		
        // Swap first two characters
		strcpy (newName, inName);
		char c = newName [0];
		newName [0] = newName [1];
		newName [1] = c;
		strcpy (newInstance, inInstance);
#endif        
		strcpy (newRealm, inRealm);

		*outName = newName;
		*outInstance = newInstance;
		*outRealm = newRealm;

		*outChanged = true;	
	} else {
		if (newName != nil) {
			DisposePtr (newName);
		}

		if (newInstance != nil) {
			DisposePtr (newInstance);
		}

		if (newRealm != nil) {
			DisposePtr (newRealm);
		}
	}
	
    if (err == noErr) {
        dprintf ("Backberos: returning outName = '%s'; outInstance = '%s'; outRealm = '%s'\n",
            *outName, *outInstance, *outRealm);
    }
	return err;
}

void KerberosLoginPrincipalTranslation_ReleasePrincipal (
	char*	inName,
	char*	inInstance,
	char*	inRealm)
{
    dprintf ("Backberos: entering KerberosLoginPrincipalTranslation_ReleasePrincipal\n");
	DisposePtr (inName);
	DisposePtr (inInstance);
	DisposePtr (inRealm);
}

}
