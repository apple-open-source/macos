/* 
 * FSpUtils.h
 *
 * $Header: /cvs/kfm/KerberosFramework/Common/Headers/Kerberos/FSpUtils.h,v 1.2 2003/03/20 04:59:37 lxs Exp $
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