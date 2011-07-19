/*
 * Contains:   Routines to support Apple Push Notification service.
 * Written by: Michael Dasenbrock (for addtl writers check SVN comments).
 * Copyright:  © 2008-2011 Apple, Inc., all rights reserved.
 * Note:       When editing this file set Xcode to "Editor uses tabs/width=4".
 *             Any other editor or tab setting may not show this file nicely!
 */

#include <sysexits.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syslog.h>

// Mac OS X Headers
#import <XMPP/XMPP.h>
#import <XMPP/Pubsub.h>

#import <Foundation/Foundation.h>
#import <OpenDirectory/NSOpenDirectory.h>
#import <OpenDirectory/OpenDirectoryPriv.h>
#import <AOSNotification/AOSNotification.h>
#import <DirectoryService/DirServicesTypes.h>
#import <DirectoryService/DirServicesConst.h>

#include "push_notify.h"

static int g_pid_fd	 = -1;

static volatile int sigusr1 = 0;
static volatile int sigusr2 = 0;
static volatile int got_sighup = 0;
static volatile int got_sigterm = 0;

NSAutoreleasePool  *gPool			= nil;
ODNode			   *g_od_search_node= nil;
XMPPSessionRef		g_xmpp_sess_ref	= NULL;
server_info_t		g_server_info;
char				g_hostname[256];
int					g_connected;


// -----------------------------------------------------------------
//	exit_with_error ()

void exit_with_error ( const char *msg, int code )
{
	syslog( LOG_CRIT, "%s", msg );
	syslog( LOG_NOTICE, "exiting" );

	[gPool release];

	exit( code );
} // exit_with_error


// -----------------------------------------------------------------
//	set_rlimits ()

int set_rlimits ( void )
{
	struct rlimit limit[1] = {{ 0, 0 }};

	if (getrlimit(RLIMIT_CORE, limit) == -1)
		return -1;

	limit->rlim_cur = 0;

	return(setrlimit(RLIMIT_CORE, limit));
} // set_rlimits


// -----------------------------------------------------------------
//	sighup_handler ()

void sighup_handler( int sig __attribute__((unused)) )
{
	got_sighup = 1;
} // sighup_handler


// -----------------------------------------------------------------
//	sigterm_handler ()

void sigterm_handler ( int sig __attribute__((unused)) )
{
	got_sigterm = 1;
} // sigterm_handler


// -----------------------------------------------------------------
//	sigalrm_handler ()

void sigalrm_handler(int sig __attribute__((unused)))
{
	return;
} // sigalrm_handler


// -----------------------------------------------------------------
//	sigusr1_handler ()

void sigusr1_handler(int sig __attribute__((unused)))
{
	if ( sigusr1 ) {
		sigusr1 = 0;
		syslog(LOG_INFO, "--: debug logging disabled" );
	} else {
		sigusr1 = 1;
		syslog(LOG_INFO, "--: debug logging enabled" );
	}
} //sigusr1_handler


// -----------------------------------------------------------------
//	sigusr2_handler ()

void sigusr2_handler(int sig __attribute__((unused)))
{
	if ( sigusr2 )
		sigusr2 = 0;
	else
		sigusr2 = 1;
} //sigusr2_handler


// -----------------------------------------------------------------
//	sighandler_setup ()

void sighandler_setup ( void )
{
	struct sigaction action;

	sigemptyset( &action.sa_mask );
	action.sa_flags |= SA_RESTART;
	action.sa_handler = sighup_handler;

	if ( sigaction( SIGHUP, &action, NULL ) < 0 )
		exit_with_error("unable to install signal handler for SIGHUP: %m", 1);

	action.sa_handler = sigalrm_handler;
	if ( sigaction( SIGALRM, &action, NULL ) < 0 )
		exit_with_error("unable to install signal handler for SIGALRM: %m", 1);

	action.sa_handler = sigusr1_handler;
	if ( sigaction( SIGUSR1, &action, NULL ) < 0 )
		exit_with_error("unable to install signal handler for SIGUSR1: %m", 1);

	action.sa_handler = sigusr2_handler;
	if ( sigaction( SIGUSR2, &action, NULL ) < 0 )
		exit_with_error("unable to install signal handler for SIGUSR2: %m", 1);

	action.sa_handler = sigterm_handler;
	if ( sigaction( SIGTERM, &action, NULL ) < 0 )
		exit_with_error("unable to install signal handler for SIGTERM: %m", 1);

	if ( sigaction( SIGINT, &action, NULL) < 0 )
		exit_with_error("unable to install signal handler for SIGINT: %m", 1);

} // sighandler_setup

// -----------------------------------------------------------------
//	setup_socket_path ()

void setup_socket_path ( const char *in_path )
{
	NSString		*ns_str_path	= [[NSString stringWithUTF8String: in_path]autorelease];
	NSDictionary	*ns_dir_attr	= [[NSDictionary dictionaryWithObjectsAndKeys:
									   @"_dovecot", NSFileOwnerAccountName,
									   @"mail", NSFileGroupOwnerAccountName,
									   [NSNumber numberWithUnsignedLong: 0750], NSFilePosixPermissions,
									   nil]autorelease];

	if ( ![[NSFileManager defaultManager] fileExistsAtPath: ns_str_path isDirectory: nil] )
		[[NSFileManager defaultManager] createDirectoryAtPath: ns_str_path withIntermediateDirectories: YES attributes: ns_dir_attr error: nil];
	else
		[[NSFileManager defaultManager] setAttributes: ns_dir_attr ofItemAtPath: ns_str_path error: nil];

} // setup_socket_path


// -----------------------------------------------------------------
//	get_socket ()

int get_socket ( int in_buff_size )
{
	const char		   *socket_path	= "/var/dovecot/push_notify";
	struct sockaddr_un	sock_addr;

	if ( sigusr1 )
		syslog(LOG_INFO, "--: opening socket: %s", socket_path);

	// setup socket directories
	setup_socket_path("/var/dovecot");

	int out_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (out_socket < 0) {
		syslog(LOG_ERR, "open socket: \"%s\" failed", socket_path);
		return( -1 );
	}

	if ( in_buff_size ) {
		int rc = 0;
		int optval = 0;
		socklen_t optlen;
		if ( sigusr1 ) {
			rc = getsockopt(out_socket, SOL_SOCKET, SO_RCVBUF, (char *)&optval, &optlen);
			if ( !rc )
				syslog(LOG_INFO, "--: socket get size: %d", optval );
		}

		optlen = sizeof(in_buff_size);
		rc = setsockopt(out_socket, SOL_SOCKET, SO_RCVBUF, &in_buff_size, optlen);
		if ( !rc )
			syslog(LOG_INFO, "socket receive buffer size: %d", in_buff_size);
		else
			syslog(LOG_ERR, "setsockopt(SO_RCVBUF) failed: %d", rc);
	}

	// bind it to a local file
	sock_addr.sun_family = AF_UNIX;
	strlcpy(sock_addr.sun_path, socket_path, sizeof(sock_addr.sun_path));
	unlink(sock_addr.sun_path);

	int len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path) + 1;
	int result = bind(out_socket, (struct sockaddr *)&sock_addr, len);
	if (result < 0) {
		syslog(LOG_ERR, "bind() to socket: \"%s\" failed: %m", socket_path );
		return( -1 );
	}

	// setup socket permissions
	chmod( sock_addr.sun_path, 0666 );

	if ( sigusr1 )
		syslog(LOG_INFO, "--: socket opened: %d: %s", sock_addr.sun_family, sock_addr.sun_path );

	return( out_socket );
} // get_socket


// -----------------------------------------------------------------
//	od_init ()

int od_init ( void )
{
	int	rc = 0;

	if ( sigusr1 )
		syslog(LOG_INFO, "--: initializing open directory session" );

	NSError *nsError = nil;
	g_od_search_node = [ODNode nodeWithSession: [ODSession defaultSession] type: kODNodeTypeAuthentication error: &nsError];
	if ( g_od_search_node == nil ) {
		rc = 1;
		if (nsError != nil)
			syslog(LOG_ERR, "Error: unable to open search node: %s", [[nsError localizedDescription] UTF8String]);
		else
			syslog(LOG_ERR, "Error: unable to open search node");
	}

	if ( sigusr1 )
		syslog(LOG_INFO, "--: open directory session initialized successfully" );

	return( rc );
} // od_init


// -----------------------------------------------------------------
//	get_user_guid ()

int get_user_guid ( const char *in_user, char *out_guid )
{
	int			result				= 0;
	NSAutoreleasePool  *my_pool		= [[NSAutoreleasePool alloc] init];

	if ( sigusr1 )
		syslog(LOG_INFO, "--: getting GUID for user: %s, from directory", in_user );

	ODQuery *od_query = [ODQuery queryWithNode: g_od_search_node
								forRecordTypes: [NSArray arrayWithObject: @kDSStdRecordTypeUsers]
									 attribute: @kDSNAttrRecordName
									 matchType: kODMatchEqualTo
								   queryValues: [NSString stringWithUTF8String: in_user]
							  returnAttributes: [NSArray arrayWithObject: @kDS1AttrGeneratedUID]
								maximumResults: 1
										 error: nil];
	if ( od_query != nil ) {
		NSArray *nsArray_records = [od_query resultsAllowingPartial: NO error: nil];
		if ( (nsArray_records != nil) && [nsArray_records count] ) {
			ODRecord *od_record = [nsArray_records objectAtIndex: 0];
			if ( od_record != nil ) {
				// get the real name
				NSArray *nsArray_values = [od_record valuesForAttribute: @kDS1AttrGeneratedUID error: nil];
				if ( nsArray_values != nil ) {
					NSString *nsStr_name = [nsArray_values objectAtIndex: 0];

					if ( (nsStr_name != nil) && [nsStr_name length] ) {
						strlcpy( out_guid, [nsStr_name UTF8String], GUID_BUFF_SIZE );
						result = 1;
					}
				}
			}
		}
	}

	if ( sigusr1 )
		if ( result )
			syslog(LOG_INFO, "--: GUID: %s, for user: %s, found", out_guid, in_user );
		else
			syslog(LOG_INFO, "--: No GUID: for user: %s, found", in_user );


	[my_pool release];

	return( result );
} // get_user_guid


// -----------------------------------------------------------------
//	get_server_info ()

int get_server_info ( server_info_t *in_out_info )
{
	int					fd				= 0;
	char			   *p				= NULL;
	char			   *buf				= NULL;
	struct stat			stat_struct;

	if ( stat( AUTH_INFO_PATH, &stat_struct ) < 0 ) {
		syslog(LOG_ERR, "open(%s) failed: %m", AUTH_INFO_PATH );
		return( 1 );
	}

	fd = open( AUTH_INFO_PATH, O_RDONLY, 0644 );
	if ( fd < 0 ) {
		syslog(LOG_ERR, "open(%s) failed: %m", AUTH_INFO_PATH );
		return( 1 );
	}

	in_out_info->port		= 5218;
	in_out_info->address	= NULL;
	in_out_info->passwd		= NULL;
	in_out_info->username	= NULL;
	in_out_info->cf_str_addr= NULL;
	in_out_info->cf_str_user= NULL;
	in_out_info->cf_str_pwd	= NULL;

	buf = malloc( stat_struct.st_size + 1 );
	if ( buf == NULL ) {
		syslog(LOG_ERR, "malloc failed: %m" );
		return( 1 );
	}

	ssize_t bytes = read( fd, buf, stat_struct.st_size + 1 );
	if ( bytes < 0 ) {
		close( fd );
		free( buf );
		syslog(LOG_ERR, "read(%s) failed: %m", AUTH_INFO_PATH );
		return( 1 );
	}

	CFDataRef cf_data_ref = CFDataCreate( NULL, (const UInt8 *)buf, stat_struct.st_size );
	if ( cf_data_ref != NULL ) {
		CFPropertyListRef cf_plist_ref = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data_ref, kCFPropertyListImmutable, NULL );
		if ( cf_plist_ref != NULL ) {
			if ( CFDictionaryGetTypeID() == CFGetTypeID( cf_plist_ref ) ) {
				CFDictionaryRef cf_dict_ref = (CFDictionaryRef)cf_plist_ref;
				if ( CFDictionaryContainsKey( cf_dict_ref, CFSTR( "notification_server_address" ) ) ) {
					CFStringRef cf_string_ref = (CFStringRef)CFDictionaryGetValue( cf_dict_ref, CFSTR( "notification_server_address" ) );
					if ( (cf_string_ref != NULL) && (CFGetTypeID( cf_string_ref ) == CFStringGetTypeID()) ) {
						in_out_info->cf_str_addr =  CFStringCreateCopy(kCFAllocatorDefault, cf_string_ref);
						CFRetain( in_out_info->cf_str_addr );
						in_out_info->cf_str_pubsub =  CFStringCreateWithFormat(NULL, 0, CFSTR("pubsub.%@"), cf_string_ref);
						in_out_info->cf_str_apn =  CFStringCreateWithFormat(NULL, 0, CFSTR("apn.%@"), cf_string_ref);
						p = (char *)CFStringGetCStringPtr( cf_string_ref, kCFStringEncodingMacRoman );
						if ( (p != NULL) && strlen( p ) ) {
							in_out_info->address = malloc( strlen( p ) + 1 );
							strlcpy( in_out_info->address, p, strlen( p ) + 1 );
							syslog( LOG_INFO, "notification service address: %s", in_out_info->address );
						}
					}
				}

				if ( CFDictionaryContainsKey( cf_dict_ref, CFSTR( "notification_server_username" ) ) ) {
					CFStringRef cf_string_ref = (CFStringRef)CFDictionaryGetValue( cf_dict_ref, CFSTR( "notification_server_username" ) );
					if ( (cf_string_ref != NULL) && (CFGetTypeID( cf_string_ref ) == CFStringGetTypeID()) ) {
						in_out_info->cf_str_user =  CFStringCreateWithFormat(NULL, 0, CFSTR("%@@%@/push_notify"), cf_string_ref, in_out_info->cf_str_addr);
						p = (char *)CFStringGetCStringPtr( cf_string_ref, kCFStringEncodingMacRoman );
						if ( (p != NULL) && strlen( p ) ) {
							in_out_info->username = malloc( strlen( p ) + 1 );
							strlcpy( in_out_info->username, p, strlen( p ) + 1 );
							syslog( LOG_INFO, "notification service user: %s", in_out_info->username );
						}
					}
				}

				if ( CFDictionaryContainsKey( cf_dict_ref, CFSTR( "notification_server_password" ) ) ) {
					CFStringRef cf_string_ref = (CFStringRef)CFDictionaryGetValue( cf_dict_ref, CFSTR( "notification_server_password" ) );
					if ( (cf_string_ref != NULL) && (CFGetTypeID( cf_string_ref ) == CFStringGetTypeID()) ) {
						in_out_info->cf_str_pwd =  CFStringCreateCopy(kCFAllocatorDefault, cf_string_ref);
						CFRetain( in_out_info->cf_str_pwd );
						p = (char *)CFStringGetCStringPtr( cf_string_ref, kCFStringEncodingMacRoman );
						if ( (p != NULL) && strlen( p ) ) {
							in_out_info->passwd = malloc( strlen( p ) + 1 );
							strlcpy( in_out_info->passwd, p, strlen( p ) + 1 );
						}
					}
				}

				if ( CFDictionaryContainsKey( cf_dict_ref, CFSTR( "notification_server_port" ) ) ) {
					CFNumberRef cf_number_ref = (CFNumberRef)CFDictionaryGetValue( cf_dict_ref, CFSTR( "notification_server_port" ) );
					if ( (cf_number_ref != NULL) && (CFGetTypeID( cf_number_ref ) == CFNumberGetTypeID()) ) {
						if ( !CFNumberGetValue( cf_number_ref, kCFNumberIntType, &in_out_info->port ) )
							in_out_info->port = 5218;
					}
					syslog( LOG_INFO, "notification service port: %d", in_out_info->port );
				}
			}
			CFRelease(cf_plist_ref);
		}
		CFRelease(cf_data_ref);
	}

	close( fd );
	free( buf );

	return( 0 );
} // get_server_info


// -----------------------------------------------------------------
//	get_node_name_from_guid ()

CFStringRef get_node_name_from_guid ( const char *in_guid )
{
	char node_name[1024];

	// creat node name
	snprintf(node_name, sizeof(node_name), "/Public/IMAP/%s/portnum/Mailbox/%s", g_hostname, in_guid);

	return( CFStringCreateWithCString(NULL, node_name, kCFStringEncodingMacRoman) );
} // get_node_name_from_guid


// -----------------------------------------------------------------
//	get_node_name_from_user ()

CFStringRef get_node_name_from_user ( const char *in_user )
{
	char guid[GUID_BUFF_SIZE];

	// get user GUID
	if ( !get_user_guid(in_user, guid) )
		return(NULL);

	return( get_node_name_from_guid(guid) );
} // get_node_name_from_user


// -----------------------------------------------------------------
//	create_node ()

void create_node ( const char *in_user )
{
	if ( sigusr1 )
		syslog(LOG_INFO, "--: creating node for user: %s", in_user);

	CFStringRef	cf_str_node	= get_node_name_from_user( in_user );
	if ( cf_str_node != NULL ) {
		// create the node
		PSCreateNode( g_xmpp_sess_ref, g_server_info.cf_str_pubsub, cf_str_node, NULL, error_callback, NULL );

		if ( sigusr1 ) {
			char buffer[PATH_MAX];
			const char *ptr = CFStringGetCStringPtr(cf_str_node, kCFStringEncodingUTF8);
				if (ptr == NULL)
					if (CFStringGetCString(cf_str_node, buffer, PATH_MAX, kCFStringEncodingUTF8))
						ptr = buffer;

			syslog(LOG_INFO, "--: node created: %s ", ptr);
		}
		CFRelease( cf_str_node );
	}
	else
		syslog(LOG_WARNING, "Warning: unable to create node for: %s", in_user);
} // create_node


// -----------------------------------------------------------------
//	publish_callback ()

int publish_callback ( void *in_user_info )
{
	msg_data_t *msg_data = (msg_data_t *)in_user_info;

	if ( sigusr1 )
		syslog(LOG_INFO, "--: publish_callback: success: %s", msg_data->d1);

	return( 0 );
} // publish_callback


// -----------------------------------------------------------------
//	publish ()
//
//		- message data: d1: to

void publish ( msg_data_t *in_msg )
{
	CFStringRef cf_str_node	= get_node_name_from_user( in_msg->d1 );
	if ( cf_str_node != NULL ) {

		if ( sigusr1 )
			syslog(LOG_INFO, "--: publish node for usr: %s", in_msg->d1);

		CFDictionaryKeyCallBacks dict_callbacks;
		CFDictionaryValueCallBacks value_callbacks;
		CFPropertyListRef cf_dict_ref = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &dict_callbacks, &value_callbacks);
		XMLNodeRef item = createItem( cf_dict_ref );
		PSPublish( g_xmpp_sess_ref, g_server_info.cf_str_pubsub, cf_str_node, &item, publish_callback, error_callback, in_msg );

		CFRelease( cf_dict_ref );
		CFRelease( cf_str_node );
		XMLNodeRelease( item );
	}
} // publish


// -----------------------------------------------------------------
//	error_callback ()

int error_callback ( XMLNodeRef in_err_node, void *in_user_info )
{
	CFStringRef err_str_ref = XMLNodeCreateTextRepresentation( in_err_node );
	char *err_str = (char *)CFStringGetCStringPtr( err_str_ref, kCFStringEncodingMacRoman );

	if ( sigusr1 )
		syslog(LOG_INFO, "--: error_callback: %s", err_str );
	else if (strstr(err_str, "<conflict ") == NULL) // skip duplicate node creation messages
		if (strstr(err_str, "code='404'><item-not-found ") == NULL) // skip node not created yet messages
			syslog(LOG_ERR, "error: %s", err_str );

	CFRelease( err_str_ref );
	return( 0 );
} // error_callback


// -----------------------------------------------------------------
//	register_callback ()

int register_callback ( XMPPSessionRef session, XMLNodeRef node, void *info )
{
	// should check for error and log?
	return 0;
} // register_callback


// -----------------------------------------------------------------
//	register_client ()

int register_client ( msg_data_t *in_msg )
{
	if ( sigusr1 )
		syslog(LOG_INFO, "--: register_client: \"%s\" \"%s\" \"%s\" \"%s\" ", in_msg->d1, in_msg->d2, in_msg->d3, in_msg->d4 );

	// setup
	XMLNodeRef registerNode = XMLNodeCreate(CFSTR("apnregister"));
	XMLNodeRef subscribeNode = XMLNodeCreate(CFSTR("subscribe"));
	XMLNodeAddChild(registerNode, subscribeNode);
	XMLNodeRelease(subscribeNode);

	// subtopic
	XMLNodeRef tempNode = XMLNodeCreate(CFSTR("subtopic"));
	CFStringRef text_str = CFStringCreateWithCString(NULL, in_msg->d4, kCFStringEncodingMacRoman);
	XMLNodeSetText(tempNode, text_str);
	XMLNodeAddChild(subscribeNode, tempNode);
	CFRelease(text_str);
	XMLNodeRelease(tempNode);

	// aps-device-token
	tempNode = XMLNodeCreate(CFSTR("device"));
	text_str = CFStringCreateWithCString(NULL, in_msg->d3, kCFStringEncodingMacRoman);
	XMLNodeSetText(tempNode, text_str);
	XMLNodeAddChild(subscribeNode, tempNode);
	CFRelease(text_str);
	XMLNodeRelease(tempNode);

	// aps-account-id
	tempNode = XMLNodeCreate(CFSTR("account"));
	text_str = CFStringCreateWithCString(NULL, in_msg->d2, kCFStringEncodingMacRoman);
	XMLNodeSetText(tempNode, text_str);
	XMLNodeAddChild(subscribeNode, tempNode);
	CFRelease(text_str);
	XMLNodeRelease(tempNode);

	// node name
	tempNode = XMLNodeCreate(CFSTR("node"));
	text_str = get_node_name_from_user(in_msg->d1);
	XMLNodeSetText(tempNode, text_str);
	XMLNodeAddChild(subscribeNode, tempNode);
	CFRelease(text_str);
	XMLNodeRelease(tempNode);

	// register
	XMLNodeRef iqNode = XMLNodeCreateIQNode(IQTypeSet, registerNode);
	XMLNodeSetAttribute(iqNode, kAttributeNameTo, g_server_info.cf_str_apn);
	XMPPSessionSendStanzaWithReplyCallback(g_xmpp_sess_ref, iqNode, register_callback, in_msg);

	XMLNodeRelease(registerNode);
	XMLNodeRelease(iqNode);

	return( 0 );
}

// -----------------------------------------------------------------
//	timer_callback ()

void timer_callback ( CFRunLoopTimerRef timer, void *info )
{
	start_xmpp_session();
}

// -----------------------------------------------------------------
//	reschedule_xmpp_connect ()

void reschedule_xmpp_connect ()
{
	if ( sigusr1 )
		syslog(LOG_INFO, "--: reschedule xmpp connect" );

	if (g_connected == 1)
		g_connected = -1;
	else if (g_connected != -6)
		g_connected--;

	const int connectIntervals[] = {15, 15, 30, 60, 120, 300};
	int reconnectInterval = connectIntervals[-1 - g_connected];
	syslog(LOG_ERR, "notification server connect failed, will retry in %d seconds", reconnectInterval );

	XMPPSessionRelease(g_xmpp_sess_ref);
	g_xmpp_sess_ref = NULL;


	CFRunLoopRef runLoop = CFRunLoopGetCurrent();
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent()+reconnectInterval, 0, 0, 0,
												   &timer_callback, NULL);

	if ( timer ) {
		CFRunLoopAddTimer(runLoop, timer, kCFRunLoopCommonModes);
		CFRelease(timer);
	} else
		syslog(LOG_ERR, "Error: unable to create run loop timer" );
}

// -----------------------------------------------------------------
//	start_xmpp_session ()

void start_xmpp_session ()
{
	if ( sigusr1 )
		syslog(LOG_INFO, "--: starting xmpp session" );

	// -----------------------------------------------
	// Setup the XMPP Session
	while ( (g_xmpp_sess_ref = XMPPSessionCreate(session_callback)) == NULL ) {
		syslog(LOG_ERR, "could not create XMPP session");
		sleep(60);
		if (got_sigterm)
			exit(0);
	}

	int r = XMPPSessionStart(g_xmpp_sess_ref, g_server_info.cf_str_addr, g_server_info.port, g_server_info.cf_str_user, g_server_info.cf_str_pwd, kSecurityTLS);
	if (r != kStatusOK)
		reschedule_xmpp_connect();
}

// -----------------------------------------------------------------
//	session_callback ()

void session_callback ( XMPPSessionRef in_sess_ref, XMPPSessionEvent in_event )
{
	if ( sigusr1 )
		syslog(LOG_INFO, "--: xmpp session callback: event: %d", in_event);

	switch ( in_event )
	{
		case kEventSessionStarted:
			g_connected = 1;
			syslog(LOG_NOTICE, "notification server session successfully established" );
			SendPresence(in_sess_ref, NULL, PresenceTypeNoType, PresenceShowNothing, NULL);
			break;

		case kEventSessionStopped:
		case kEventAuthenticationFailed:
		case kEventBindFailed:
		case kEventConnectionFailed:
			reschedule_xmpp_connect();
			break;

		case kEventOther:
		case kEventNetworkUnreachable:
			break;
	}
} // session_callback


// -----------------------------------------------------------------
//	received_data_callback ()

void received_data_callback (CFSocketRef s, CFSocketCallBackType callbackType, CFDataRef address, const void *in_data, void *info)
{
	int					a_socket			= -1;
	ssize_t				msg_size			= 0;
	msg_data_t			message_data;

	// read from unix socket
	a_socket = CFSocketGetNative(s);
	NSData *msg_data = (NSData *)in_data;
	msg_size = [msg_data length];

	if ( sigusr1 )
		syslog(LOG_INFO, "--: message received: size = %d", (int)msg_size);

	if ( g_connected != 1 ) {
		syslog(LOG_ERR, "discarding message; not connected to notification server");
		return;
	}

	if (msg_size == sizeof(msg_data_t))
		[msg_data getBytes: &message_data length: sizeof(msg_data_t)];

	if (msg_size > 0) {
		switch ( message_data.msg ) {
			case 1:
				// create node, user ID only
				syslog(LOG_DEBUG, "creating node: user: %s", message_data.d1);

				create_node(message_data.d1);
				break;
			case 2:
				// register from IMAP ID command
				syslog(LOG_DEBUG, "register node: user: \"%s\" \"%s\" \"%s\"", message_data.d1, message_data.d2, message_data.d3);

				if (!strlen(message_data.d1) || !strlen(message_data.d2) || !strlen(message_data.d3)) {
					syslog(LOG_ERR, "Error: missing registration data");
					return;
				}

				register_client( &message_data );
				break;
			case 3:
				// publish/notify to node
				syslog(LOG_DEBUG, "publish node: user: %s", message_data.d1);

				publish( &message_data );
				break;
			default:
				syslog(LOG_ERR, "Error: unknown message type: %lu", message_data.msg);
		}
	}
} // received_data_callback


// -----------------------------------------------------------------
//	main ()

int main ( int argc, char **argv )
{
	int	ch;
	int buff_size = BUFF_SIZE;
	int a_socket = -1;
	char *value = NULL;

	gPool = [[NSAutoreleasePool alloc] init];

	// turn on logging
	openlog("push_notify", LOG_PID, LOG_LOCAL6);

	while ( (ch = getopt(argc, argv, "b:")) != EOF ) {
		switch( ch ) {
			case 'b':
				// buffer size
				value = optarg;
				if ( value ) {
					NSString *value_str = [NSString stringWithCString: value encoding: NSUTF8StringEncoding];
					buff_size = [value_str intValue];
					if ( buff_size < BUFF_SIZE )
						buff_size = BUFF_SIZE;
					else if ( buff_size > MAX_BUF_SIZE )
						buff_size = MAX_BUF_SIZE;
				}
				break;
			default:
				break;
		}
	}

	// setup the pid file
	char *pidfile = "/var/run/push_notify.pid";
	g_pid_fd = open(pidfile, O_CREAT | O_RDWR | O_EXLOCK | O_NONBLOCK, 0644);
	if (g_pid_fd == -1) {
		fprintf(stderr, "ERROR: can't open pidfile: %m\n");
		exit(EX_OSERR);
	} else {
		char buf[100];
		snprintf(buf, sizeof(buf), "%lu\n", (unsigned long int)getpid());
		if (lseek(g_pid_fd, 0, SEEK_SET) == -1	||
			ftruncate(g_pid_fd, 0) == -1		||
			write(g_pid_fd, buf, strlen(buf)) == -1)
		{
			fprintf(stderr, "ERROR: unable to write to pid file\n");
			exit(EX_OSERR);
		}
		fsync(g_pid_fd);
	}

	// -----------------------------------------------
	// begin setup    
	syslog(LOG_INFO, "initializing mail notification services");

	// -----------------------------------------------
	// setrlimit
	set_rlimits();

	// -----------------------------------------------
	// Set signal handlers
	sighandler_setup();

	// -----------------------------------------------
	// create socket we are going to use for listening
	while ( (a_socket = get_socket(buff_size)) < 0 ) {
		sleep(60);
		if (got_sigterm)
			exit(0);
	}

	// -----------------------------------------------
	// Initialize open directory
	//	- no need to contiure if OD is failing
	while ( od_init() == 1 ) 	{
		sleep(60);
		if (got_sigterm)
			exit(0);
	}

	// -----------------------------------------------
	// Set global hostname
	if ( gethostname(g_hostname, sizeof(g_hostname)-1) == -1 ) {
		syslog(LOG_ERR, "error: gethostname() failed: %m, setting to unknown");
		strlcpy(g_hostname, "unknown", sizeof(g_hostname) );
	}


	// -----------------------------------------------
	// Get notification server
	while ((get_server_info( &g_server_info ) != 0) && (g_server_info.address == NULL)) {
		syslog(LOG_ERR, "error: no notification server defined");
		sleep(60);
		if (got_sigterm)
			exit(0);
	}

	// -----------------------------------------------
	// Start the XMPP Session
	g_connected = 0;
	start_xmpp_session();

	CFSocketRef listenSocket = CFSocketCreateWithNative(NULL, a_socket, kCFSocketDataCallBack, received_data_callback, NULL);
	CFRunLoopSourceRef rl_source = CFSocketCreateRunLoopSource(NULL, listenSocket, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rl_source, kCFRunLoopDefaultMode);
	CFRelease(rl_source);

	// -----------------------------------------------
	// Begin heavy lifting
	syslog(LOG_INFO, "starting mail notification services");

	// main loop
	for (;;) {
		if ( got_sigterm != 0 ) {
			// Say Goodnight Gracie
			syslog(LOG_INFO, "terminating mail notification services: SIGTERM");

			[gPool release];

			close(a_socket);
			exit(0);
		}

		if ( got_sighup ) {
			// right now this is a do-nothing check
			syslog(LOG_INFO, "SIGHUP signal received");
			got_sighup = 0;
		}

		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, 0);
	}

	[gPool release];
} // main
