/* 
 * FSpUtils.h
 *
 * $Header: /cvs/kfm/KerberosFramework/Common/Headers/Kerberos/FSpUtils.h,v 1.3 2004/10/22 20:54:16 lxs Exp $
 */

#ifndef __FSPUTILS__
#define __FSPUTILS__

#include <CoreServices/CoreServices.h>

#ifdef __cplusplus
extern "C" {
#endif

OSStatus FSSpecToPOSIXPath (const FSSpec *inSpec, char *ioPath, unsigned long inPathLength);
OSStatus POSIXPathToFSSpec (const char *inPath, FSSpec *outSpec);

#ifdef __cplusplus
}
#endif

#endif __FSPUTILS__
