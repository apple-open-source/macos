/* GenericNBPURL.h created by root on Mon 15-Nov-1999 */

#ifndef __GENERICNBPURL__
#define __GENERICNBPURL__

#include <CoreServices/CoreServices.h>

#define kNBPDivider				":/at/"
#define kEntityZoneDelimiter	":"
#define kAFPServerNBPType		"AFPServer"
#define kAFPServerURLType		"afp"

void MakeGenericNBPURL(const char *entityType, const char *zoneName, const char *name, char* returnBuffer, UInt16* returnBufferLen );
//OSStatus ParseGenericNBPURL(char* url, StringPtr entityType, StringPtr zoneName, StringPtr name);

#endif

