/*
 * Utility class for manipulating possibly invalid file specs.
 */

#include "KClientFile.h"
#include <Kerberos/FSpUtils.h>

KClientFilePriv::KClientFilePriv ()
{
	mValid = false;
}

KClientFilePriv::KClientFilePriv (
	const FSSpec&			inFileSpec)
{
	*this = inFileSpec;
}

KClientFilePriv::KClientFilePriv (
	const char*				inFilePath)
{
	*this = inFilePath;
}

#pragma mark -

KClientFilePriv&
KClientFilePriv::operator = (
	const KClientFilePriv&	inOriginal)
{
	inOriginal.CheckValid ();
	*this = inOriginal.mFileSpec;
	return *this;
}

KClientFilePriv&
KClientFilePriv::operator = (
	const FSSpec&			inFileSpec)
{
	mFileSpec = inFileSpec;
	mValid = true;
	return *this;
}

KClientFilePriv&
KClientFilePriv::operator = (
	const char*				inFilePath)
{
    OSStatus err = POSIXPathToFSSpec (inFilePath, &mFileSpec);
	if (err != noErr) {
		DebugThrow_ (KClientRuntimeError (kcErrFileNotFound));
	}

	return *this;
}
						
#pragma mark -

Boolean
KClientFilePriv::IsValid () const
{
	return mValid;
}
	
void
KClientFilePriv::CheckValid () const
{
	if (!IsValid ())
		DebugThrow_ (std::logic_error ("Invalid KClient file"));
}
	
#pragma mark -

KClientFilePriv::operator const FSSpec& () const
{
    return mFileSpec;
}

KClientFilePriv::operator const char * () const
{
    OSStatus err = FSSpecToPOSIXPath (&mFileSpec, (char *) mFilePath, PATH_MAX);
    if (err != noErr) {
        DebugThrow_ (KClientRuntimeError (kcErrFileNotFound));
    }
    
    return mFilePath;
}
