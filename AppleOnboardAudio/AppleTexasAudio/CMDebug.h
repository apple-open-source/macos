/*
 *  O3Debug.h
 *  ClickMonitor
 *
 *  Created by jester on Mon Aug 21 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#include <Carbon/Carbon.h>
#include "CMLog.h"

//****************************************************************************************
// PRMSG
//
//----------------------------------------------------------------------------------------
#if DEBUG
//	#define PRMSG( text, file, line )					printf( text, file, line );
	#define PRMSG( text, file, line )					CMLogArg2( text, file, line );
	#define PRMSG_INT( text, integer, file, line )		CMLogArg3( text, integer, file, line );
#else
	#define PRMSG( text, file, line )
	#define PRMSG_INT( text, integer, file, line )
#endif

//****************************************************************************************
// ERROR_ACTION( type, test, label, action )
//
//	This macro will perform the given action if the given test is false. This behavior
//	will not change regardless of the DEBUG setting. 
//
//	Depending on the setting of DEBUG, this macro may or may not deliver a message
//	to the user about the failure.
//----------------------------------------------------------------------------------------
#define ERROR_ACTION( type, test, testText, action )									\
	do {																				\
		if ( !(test) ) {																\
			PRMSG( "\nCM" #type " FAILED: " #testText "\n   - file: %s\n   - line: %d\n\n", __FILE__, __LINE__ );		\
			{ action; }																	\
		}																				\
	} while( false )

//****************************************************************************************
// ERROR_ACTION_INT( type, test, label, action, integer )
//
//	This macro will perform the given action if the given test is false. This behavior
//	will not change regardless of the DEBUG setting. 
//
//	Depending on the setting of DEBUG, this macro may or may not deliver a message
//	to the user about the failure.
//----------------------------------------------------------------------------------------
#define ERROR_ACTION_INT( type, test, testText, action, integer )						\
	do {																				\
		if ( !(test) ) {																\
			PRMSG_INT( "\nCM" #type " FAILED: " #testText " [0x%lx]\n   - file: %s\n   - line: %d\n\n", integer, __FILE__, __LINE__ );		\
			{ action; }																	\
		}																				\
	} while( false )

//****************************************************************************************
// QUIET_ACTION( type, test, label, action )
//
//	This macro will perform the given action if the given test is false. This behavior
//	will not change regardless of the DEBUG setting. 
//
//	Regardless of the setting of DEBUG, this macro will not deliver any message to the
//	user about the failure.
//----------------------------------------------------------------------------------------
#define QUIET_ACTION( test, action )			\
	do {										\
		if ( !(test) ) { action; }				\
	} while( false )


//****************************************************************************************
// Exception Stuff
//
//----------------------------------------------------------------------------------------
#define CMRequire_action( assertion, label, action )			ERROR_ACTION( REQUIRE, assertion, assertion, action; goto label )
#define CMRequire_action_msg( assertion, msg, label, action )	ERROR_ACTION( REQUIRE, assertion, msg, action; goto label )

#define CMRequire_noErr( resultVal, label )						ERROR_ACTION( REQUIRE, resultVal == noErr, resultVal == noErr, goto label )
#define CMRequire_noErr_msg( resultVal, msg, label )			ERROR_ACTION( REQUIRE, resultVal == noErr, msg, goto label )

#define CMRequire( assertion, label )							ERROR_ACTION( REQUIRE, assertion, assertion, goto label )
#define CMRequire_int( assertion, integer, label )				ERROR_ACTION_INT( REQUIRE, assertion, assertion, goto label, integer )
#define CMRequire_msg( assertion, msg, label )					ERROR_ACTION( REQUIRE, assertion, msg, goto label )
#define CMRequire_msg_int( assertion, msg, integer, label )		ERROR_ACTION_INT( REQUIRE, assertion, msg, goto label, integer )

#define CMRequire_quiet( assertion, label )						QUIET_ACTION( assertion, goto label )
#define CMRequire_action_quiet( assertion, label, action )		QUIET_ACTION( assertion, action; goto label )

//****************************************************************************************
// Diagnostic Check Stuff
//
//	These macros do not alter the flow of code, and compile out when debug is not on.
//----------------------------------------------------------------------------------------
#if DEBUG
	#define CMCheck( assertion )						ERROR_ACTION( CHECK, assertion, assertion, while(false){} )
	#define CMCheck_int( assertion, integer )			ERROR_ACTION_INT( CHECK, assertion, assertion, while(false){}, integer )
	#define CMCheck_msg( assertion, msg )				ERROR_ACTION( CHECK_MSG, assertion, msg, while(false){} )
	#define CMCheck_msg_int( assertion, msg, integer )	ERROR_ACTION_INT( CHECK_MSG, assertion, msg, while(false){}, integer )
#else
	#define CMCheck( assertion )
	#define CMCheck_int( assertion, integer )
	#define CMCheck_msg( assertion, msg )
	#define CMCheck_msg_int( assertion, msg, integer )
#endif

//****************************************************************************************
// Error Message Stuff
//
//	These macros do not alter the flow of code, and compile out when debug is not on.
//----------------------------------------------------------------------------------------
#if DEBUG
	#define error_msg( msg )				PRMSG( "\nCMERROR: " #msg "\n   - file: %s\n   - line: %d\n\n", __FILE__, __LINE__ )
#else
	#define error_msg( msg )
#endif
