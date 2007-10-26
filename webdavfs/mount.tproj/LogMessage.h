/*
	File:		LogMessage.h

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 2001-2006 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(bms)	Brad Suinn

	Change History is in CVS:
*/

#ifndef __LOGMESSAGE__
#define __LOGMESSAGE__
#ifdef __cplusplus
extern "C" {
#endif

#define LOGMESSAGEON 1

extern void LogMessage(u_int32_t level, char *format, ...);
extern u_int32_t gPrintLevel;

enum {
    kNone		= 	0x1,
    kError		= 	0x2,
    kWarning 	= 	0x4,
    kTrace		= 	0x8,
    kInfo		=	0x10,
    kTrace2		=	0x20,
    kSysLog		=	0x40,
    kAll		=	-1
};

#ifdef  __cplusplus
}
#endif

#endif /*__LOGMESSAGE__*/
