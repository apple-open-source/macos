#ifndef	_make_make
#define	_make_make

/* Module make */

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/message.h>

#ifndef	mig_external
#define mig_external extern
#endif

#include <mach/std_types.h>
#include "apple/make-defs.h"

/* SimpleRoutine make_alert_old */
mig_external kern_return_t make_alert_old (
	port_t makePort,
	int eventType,
	make_string_t functionName,
	unsigned int functionNameCnt,
	make_string_t fileName,
	unsigned int fileNameCnt,
	int line,
	make_string_t message,
	unsigned int messageCnt);

/* SimpleRoutine make_alert */
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
	unsigned int messageCnt);

#endif	_make_make
