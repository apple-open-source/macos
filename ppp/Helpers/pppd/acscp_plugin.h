/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __ACSCP_PLUGIN_H__
#define __ACSCP_PLUGIN_H__

// acsp header flags
#define ACSP_FLAG_END		0x0001	
#define ACSP_FLAG_START		0x0002	
#define ACSP_FLAG_ACK		0x0004	
#define ACSP_FLAG_REQUIRE_ACK	0x0008	

#define	ACSP_HDR_SIZE		8

typedef struct acsp_packet {
    u_int8_t	type;
    u_int8_t	seq;
    u_int16_t	len;
    u_int16_t	flags;
    u_int16_t	reserved;
    u_int8_t	data[1];
} acsp_packet;


#define ACSP_NOTIFICATION_NONE 			0
#define ACSP_NOTIFICATION_START			1
#define ACSP_NOTIFICATION_STOP			2
#define ACSP_NOTIFICATION_PACKET		3
#define ACSP_NOTIFICATION_TIMEOUT		4
#define ACSP_NOTIFICATION_DATA_FROM_UI		5
#define ACSP_NOTIFICATION_ERROR			6

typedef struct ACSP_Input {
    u_int16_t 	size; 		// size of the structure (for future extension)
    u_int16_t	mtu;		// mtu wll determine the maximum packet size to send
    u_int16_t	notification;	// notification ACSP sends to the plugin
    u_int16_t	data_len;	// len of the data
    void	*data;		// data to be consumed depending on the notification
    void 	(*log_debug) __P((char *, ...));	/* log a debug message */
    void 	(*log_error) __P((char *, ...));	/* log an error message */
} ACSP_Input;

#define ACSP_ACTION_NONE			0
#define ACSP_ACTION_SEND			1
#define ACSP_ACTION_INVOKE_UI			2
#define ACSP_ACTION_SEND_WITH_TIMEOUT		3
#define ACSP_ACTION_SET_TIMEOUT			4  /* data_len = 4;  data = timeout value in seconds */
#define ACSP_ACTION_CANCEL_TIMEOUT		5

typedef struct ACSP_Output {
    u_int16_t 	size; 		// size of the structure (for future extension)
    u_int16_t	action;		// action the ACSP engine needs to perform
    u_int16_t	data_len;	// len of the data
    void	*data;		// data to be consumed depending on the action
} ACSP_Output;

#define ACSP_TIMERSTATE_STOPPED	0
#define ACSP_TIMERSTATE_PACKET	1
#define ACSP_TIMERSTATE_GENERAL	2

// extension structure - one for each option
typedef struct acsp_ext {
    struct acsp_ext 	*next;			// next extension structure
       
    // option state data
    u_int8_t 		last_seq;		// seq number sent with timeout - to handle timer cancellation
    int			timer_state;		// timeout set    
    ACSP_Input		in;			// input structure
    ACSP_Output		out;			// output structure

    // the following are filled in by the plugin
    u_int8_t 		type;			// acsp type
    void		*context;		// context for the extension
    void (*dispose) __P((void*));
    void (*free) __P((void*, ACSP_Output*));
    void (*process) __P((void*, ACSP_Input*, ACSP_Output*));
    void (*interactive_ui) __P((void*, int, void**, int*));
    void (*print_packet) __P((void (*printer)(void *, char *, ...), void *arg, u_char code, char *inbuf, int insize));
 } acsp_ext;

// startup callbacks for acsp plugins
// not currently used - will be used when plugin code is separated out
typedef struct acsp_channel {
    struct acsp_channel		*next;

    // filled in by plugin start()
    int (*check_options) __P((void));
    int	(*get_type_count)  __P((void));
    int (*init_type) __P((acsp_ext *ext, int type_index));	// zero based index
} acsp_channel;


// int start(acsp_channel *plugin_channel)

#endif
