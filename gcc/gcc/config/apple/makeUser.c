#include "config.h"
#include "apple/make.h"
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/msg_type.h>
#if	!defined(KERNEL) && !defined(MIG_NO_STRINGS)
#if defined (NEXT_PDO) && !defined (_WIN32)
#include <string.h>
#else
#include <strings.h>
#endif
#endif
/* LINTLIBRARY */

extern port_t mig_get_reply_port();
extern void mig_dealloc_reply_port();

#ifndef	mig_internal
#define	mig_internal	static
#endif

#ifndef	TypeCheck
#define	TypeCheck 1
#endif

#ifndef	UseExternRCSId
#ifdef	hc
#define	UseExternRCSId		1
#endif
#endif

#ifndef	UseStaticMsgType
#if	!defined(hc) || defined(__STDC__)
#define	UseStaticMsgType	1
#endif
#endif

#define msg_request_port	msg_remote_port
#define msg_reply_port		msg_local_port


/* SimpleRoutine alert_old */
mig_external kern_return_t make_alert_old (
	port_t makePort,
	int eventType,
	make_string_t functionName,
	unsigned int functionNameCnt,
	make_string_t fileName,
	unsigned int fileNameCnt,
	int line,
	make_string_t message,
	unsigned int messageCnt)
{
	typedef struct {
		msg_header_t Head;
		msg_type_t eventTypeType;
		int eventType;
		msg_type_t functionNameType;
		char functionName[1024];
		msg_type_t fileNameType;
		char fileName[1024];
		msg_type_t lineType;
		int line;
		msg_type_t messageType;
		char message[1024];
	} Request;

	union {
		Request In;
	} Mess;

	register Request *InP = &Mess.In;

	unsigned int msg_size = 52;
	/* Maximum request size 3124 */
	unsigned int msg_size_delta;

#if	UseStaticMsgType
	static const msg_type_t eventTypeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t functionNameType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t fileNameType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t lineType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t messageType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->eventTypeType = eventTypeType;
#else	UseStaticMsgType
	InP->eventTypeType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->eventTypeType.msg_type_size = 32;
	InP->eventTypeType.msg_type_number = 1;
	InP->eventTypeType.msg_type_inline = TRUE;
	InP->eventTypeType.msg_type_longform = FALSE;
	InP->eventTypeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->eventType /* eventType */ = /* eventType */ eventType;

#if	UseStaticMsgType
	InP->functionNameType = functionNameType;
#else	UseStaticMsgType
	InP->functionNameType.msg_type_name = MSG_TYPE_CHAR;
	InP->functionNameType.msg_type_size = 8;
	InP->functionNameType.msg_type_inline = TRUE;
	InP->functionNameType.msg_type_longform = FALSE;
	InP->functionNameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (functionNameCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) functionName, (char *) InP->functionName, 1 * functionNameCnt);

	InP->functionNameType.msg_type_number /* functionNameCnt */ = /* functionNameType.msg_type_number */ functionNameCnt;

	msg_size_delta = (1 * functionNameCnt + 3) & ~3;
	msg_size += msg_size_delta;
	InP = (Request *) ((char *) InP + msg_size_delta - 1024);

#if	UseStaticMsgType
	InP->fileNameType = fileNameType;
#else	UseStaticMsgType
	InP->fileNameType.msg_type_name = MSG_TYPE_CHAR;
	InP->fileNameType.msg_type_size = 8;
	InP->fileNameType.msg_type_inline = TRUE;
	InP->fileNameType.msg_type_longform = FALSE;
	InP->fileNameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (fileNameCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) fileName, (char *) InP->fileName, 1 * fileNameCnt);

	InP->fileNameType.msg_type_number /* fileNameCnt */ = /* fileNameType.msg_type_number */ fileNameCnt;

	msg_size_delta = (1 * fileNameCnt + 3) & ~3;
	msg_size += msg_size_delta;
	InP = (Request *) ((char *) InP + msg_size_delta - 1024);

#if	UseStaticMsgType
	InP->lineType = lineType;
#else	UseStaticMsgType
	InP->lineType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->lineType.msg_type_size = 32;
	InP->lineType.msg_type_number = 1;
	InP->lineType.msg_type_inline = TRUE;
	InP->lineType.msg_type_longform = FALSE;
	InP->lineType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->line /* line */ = /* line */ line;

#if	UseStaticMsgType
	InP->messageType = messageType;
#else	UseStaticMsgType
	InP->messageType.msg_type_name = MSG_TYPE_CHAR;
	InP->messageType.msg_type_size = 8;
	InP->messageType.msg_type_inline = TRUE;
	InP->messageType.msg_type_longform = FALSE;
	InP->messageType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (messageCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) message, (char *) InP->message, 1 * messageCnt);

	InP->messageType.msg_type_number /* messageCnt */ = /* messageType.msg_type_number */ messageCnt;

	msg_size_delta = (1 * messageCnt + 3) & ~3;
	msg_size += msg_size_delta;

	InP = &Mess.In;
	InP->Head.msg_simple = TRUE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL;
	InP->Head.msg_request_port = makePort;
	InP->Head.msg_reply_port = PORT_NULL;
	InP->Head.msg_id = 100;

	return msg_send(&InP->Head, MSG_OPTION_NONE, 0);
}

/* SimpleRoutine alert */
mig_external kern_return_t make_alert (
	port_t makePort,
	int eventType,
	make_string_t functionName,
	unsigned int functionNameCnt,
	make_string_t fileName,
	unsigned int fileNameCnt,
	make_string_t directory,
	unsigned int directoryCnt,
	int line,
	make_string_t message,
	unsigned int messageCnt)
{
	typedef struct {
		msg_header_t Head;
		msg_type_t eventTypeType;
		int eventType;
		msg_type_t functionNameType;
		char functionName[1024];
		msg_type_t fileNameType;
		char fileName[1024];
		msg_type_t directoryType;
		char directory[1024];
		msg_type_t lineType;
		int line;
		msg_type_t messageType;
		char message[1024];
	} Request;

	union {
		Request In;
	} Mess;

	register Request *InP = &Mess.In;

	unsigned int msg_size = 56;
	/* Maximum request size 4152 */
	unsigned int msg_size_delta;

#if	UseStaticMsgType
	static const msg_type_t eventTypeType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t functionNameType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t fileNameType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t directoryType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t lineType = {
		/* msg_type_name = */		MSG_TYPE_INTEGER_32,
		/* msg_type_size = */		32,
		/* msg_type_number = */		1,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	static const msg_type_t messageType = {
		/* msg_type_name = */		MSG_TYPE_CHAR,
		/* msg_type_size = */		8,
		/* msg_type_number = */		1024,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	FALSE,
		/* msg_type_deallocate = */	FALSE,
		/* msg_type_unused = */		0,
	};
#endif	UseStaticMsgType

#if	UseStaticMsgType
	InP->eventTypeType = eventTypeType;
#else	UseStaticMsgType
	InP->eventTypeType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->eventTypeType.msg_type_size = 32;
	InP->eventTypeType.msg_type_number = 1;
	InP->eventTypeType.msg_type_inline = TRUE;
	InP->eventTypeType.msg_type_longform = FALSE;
	InP->eventTypeType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->eventType /* eventType */ = /* eventType */ eventType;

#if	UseStaticMsgType
	InP->functionNameType = functionNameType;
#else	UseStaticMsgType
	InP->functionNameType.msg_type_name = MSG_TYPE_CHAR;
	InP->functionNameType.msg_type_size = 8;
	InP->functionNameType.msg_type_inline = TRUE;
	InP->functionNameType.msg_type_longform = FALSE;
	InP->functionNameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (functionNameCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) functionName, (char *) InP->functionName, 1 * functionNameCnt);

	InP->functionNameType.msg_type_number /* functionNameCnt */ = /* functionNameType.msg_type_number */ functionNameCnt;

	msg_size_delta = (1 * functionNameCnt + 3) & ~3;
	msg_size += msg_size_delta;
	InP = (Request *) ((char *) InP + msg_size_delta - 1024);

#if	UseStaticMsgType
	InP->fileNameType = fileNameType;
#else	UseStaticMsgType
	InP->fileNameType.msg_type_name = MSG_TYPE_CHAR;
	InP->fileNameType.msg_type_size = 8;
	InP->fileNameType.msg_type_inline = TRUE;
	InP->fileNameType.msg_type_longform = FALSE;
	InP->fileNameType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (fileNameCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) fileName, (char *) InP->fileName, 1 * fileNameCnt);

	InP->fileNameType.msg_type_number /* fileNameCnt */ = /* fileNameType.msg_type_number */ fileNameCnt;

	msg_size_delta = (1 * fileNameCnt + 3) & ~3;
	msg_size += msg_size_delta;
	InP = (Request *) ((char *) InP + msg_size_delta - 1024);

#if	UseStaticMsgType
	InP->directoryType = directoryType;
#else	UseStaticMsgType
	InP->directoryType.msg_type_name = MSG_TYPE_CHAR;
	InP->directoryType.msg_type_size = 8;
	InP->directoryType.msg_type_inline = TRUE;
	InP->directoryType.msg_type_longform = FALSE;
	InP->directoryType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (directoryCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) directory, (char *) InP->directory, 1 * directoryCnt);

	InP->directoryType.msg_type_number /* directoryCnt */ = /* directoryType.msg_type_number */ directoryCnt;

	msg_size_delta = (1 * directoryCnt + 3) & ~3;
	msg_size += msg_size_delta;
	InP = (Request *) ((char *) InP + msg_size_delta - 1024);

#if	UseStaticMsgType
	InP->lineType = lineType;
#else	UseStaticMsgType
	InP->lineType.msg_type_name = MSG_TYPE_INTEGER_32;
	InP->lineType.msg_type_size = 32;
	InP->lineType.msg_type_number = 1;
	InP->lineType.msg_type_inline = TRUE;
	InP->lineType.msg_type_longform = FALSE;
	InP->lineType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	InP->line /* line */ = /* line */ line;

#if	UseStaticMsgType
	InP->messageType = messageType;
#else	UseStaticMsgType
	InP->messageType.msg_type_name = MSG_TYPE_CHAR;
	InP->messageType.msg_type_size = 8;
	InP->messageType.msg_type_inline = TRUE;
	InP->messageType.msg_type_longform = FALSE;
	InP->messageType.msg_type_deallocate = FALSE;
#endif	UseStaticMsgType

	if (messageCnt > 1024)
		return MIG_ARRAY_TOO_LARGE;
	bcopy((char *) message, (char *) InP->message, 1 * messageCnt);

	InP->messageType.msg_type_number /* messageCnt */ = /* messageType.msg_type_number */ messageCnt;

	msg_size_delta = (1 * messageCnt + 3) & ~3;
	msg_size += msg_size_delta;

	InP = &Mess.In;
	InP->Head.msg_simple = TRUE;
	InP->Head.msg_size = msg_size;
	InP->Head.msg_type = MSG_TYPE_NORMAL;
	InP->Head.msg_request_port = makePort;
	InP->Head.msg_reply_port = PORT_NULL;
	InP->Head.msg_id = 101;

	return msg_send(&InP->Head, MSG_OPTION_NONE, 0);
}
