/*
 * @OSF_COPYRIGHT@
 */

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/vm_statistics.h>

/*
 *	Routine:	mach_msg_server_once
 *	Purpose:
 *		A simple generic server function.  It allows more flexibility
 *		than mach_msg_server by processing only one message request
 *		and then returning to the user.  Note that more in the way
 * 		of error codes are returned to the user; specifically, any
 * 		failing error from mach_msg_overwrite_trap will be returned
 *		(though errors from the demux routine or the routine it calls
 *		will not be).
 */
mach_msg_return_t
mach_msg_server_once(
    boolean_t (*demux)(mach_msg_header_t *, mach_msg_header_t *),
    mach_msg_size_t max_size,
    mach_port_t rcv_name,
    mach_msg_options_t options)
{
    mig_reply_error_t *bufRequest = 0, *bufReply = 0, *bufTemp;
    register mach_msg_return_t mr;
    register kern_return_t kr;

    if ((kr = vm_allocate(mach_task_self(),
		     (vm_address_t *)&bufRequest,
		     max_size + MAX_TRAILER_SIZE,
		     VM_MAKE_TAG(VM_MEMORY_MACH_MSG)|TRUE)) != KERN_SUCCESS)
      return kr;    
    if ((kr = vm_allocate(mach_task_self(),
		     (vm_address_t *)&bufReply,
		     max_size + MAX_TRAILER_SIZE,
		     VM_MAKE_TAG(VM_MEMORY_MACH_MSG)|TRUE)) != KERN_SUCCESS)
      return kr;    

    mr = mach_msg_overwrite_trap(&bufRequest->Head, MACH_RCV_MSG|options,
				 0, max_size, rcv_name,
				 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
				 (mach_msg_header_t *) 0, 0);
    if (mr == MACH_MSG_SUCCESS) {
	/* we have a request message */

	(void) (*demux)(&bufRequest->Head, &bufReply->Head);

	if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
	    bufReply->RetCode != KERN_SUCCESS) {
	    if (bufReply->RetCode == MIG_NO_REPLY)
		/*
		 * This return code is a little tricky--
		 * it appears that the demux routine found an
		 * error of some sort, but since that error
		 * would not normally get returned either to
		 * the local user or the remote one, we pretend it's
		 * ok.
		 */
		return KERN_SUCCESS;

	    /* don't destroy the reply port right,
	       so we can send an error message */
	    bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
	    mach_msg_destroy(&bufRequest->Head);
	}

	if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
	    /* no reply port, so destroy the reply */
	    if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
		mach_msg_destroy(&bufReply->Head);

	    return KERN_SUCCESS;
	}

	/* send reply.  */

	bufTemp = bufRequest;
	bufRequest = bufReply;
	bufReply = bufTemp;

	/*
	 *	We don't want to block indefinitely because the client
	 *	isn't receiving messages from the reply port.
	 *	If we have a send-once right for the reply port, then
	 *	this isn't a concern because the send won't block.
	 *	If we have a send right, we need to use MACH_SEND_TIMEOUT.
	 *	To avoid falling off the kernel's fast RPC path unnecessarily,
	 *	we only supply MACH_SEND_TIMEOUT when absolutely necessary.
	 */

	mr = mach_msg_overwrite_trap(&bufRequest->Head,
			 (MACH_MSGH_BITS_REMOTE(bufRequest->Head.msgh_bits) ==
			  MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
			 MACH_SEND_MSG|options :
			 MACH_SEND_MSG|MACH_SEND_TIMEOUT|options,
			 bufRequest->Head.msgh_size, 0, MACH_PORT_NULL,
			 0, MACH_PORT_NULL, (mach_msg_header_t *) 0, 0);
    }
    /* Has a message error occurred? */

    switch (mr) {
      case MACH_SEND_INVALID_DEST:
      case MACH_SEND_TIMED_OUT:
	/* the reply can't be delivered, so destroy it */
	mach_msg_destroy(&bufRequest->Head);
	return KERN_SUCCESS;	/* Matches error hiding behavior in
				   mach_msg_server.  */

      case MACH_RCV_TOO_LARGE:
	return KERN_SUCCESS;	/* Matches error hiding behavior in
				   mach_msg_server.  */

      default:
	/* Includes success case.  */
	(void)vm_deallocate(mach_task_self(),
			    (vm_address_t) bufRequest,
			    max_size + MAX_TRAILER_SIZE);
	(void)vm_deallocate(mach_task_self(),
			    (vm_address_t) bufReply,
			    max_size + MAX_TRAILER_SIZE);
	return mr;
    }
}

/*
 *	Routine:	mach_msg_server
 *	Purpose:
 *		A simple generic server function.  Note that changes here
 * 		should be considered for duplication above.
 */
mach_msg_return_t
mach_msg_server(
    boolean_t (*demux)(mach_msg_header_t *, mach_msg_header_t *),
    mach_msg_size_t max_size,
    mach_port_t rcv_name,
    mach_msg_options_t options)
{
    mig_reply_error_t *bufRequest = 0, *bufReply = 0, *bufTemp;
    register mach_msg_return_t mr;
    register kern_return_t kr;

    if ((kr = vm_allocate(mach_task_self(),
		     (vm_address_t *)&bufRequest,
		     max_size + MAX_TRAILER_SIZE,
		     VM_MAKE_TAG(VM_MEMORY_MACH_MSG)|TRUE)) != KERN_SUCCESS)
      return kr;    
    if ((kr = vm_allocate(mach_task_self(),
		     (vm_address_t *)&bufReply,
		     max_size + MAX_TRAILER_SIZE,
		     VM_MAKE_TAG(VM_MEMORY_MACH_MSG)|TRUE)) != KERN_SUCCESS)
      return kr;    

    for (;;) {
      get_request:
	mr = mach_msg_overwrite_trap(&bufRequest->Head, MACH_RCV_MSG|options,
		      0, max_size, rcv_name,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
		      (mach_msg_header_t *) 0, 0);
	while (mr == MACH_MSG_SUCCESS) {
	    /* we have a request message */

	    (void) (*demux)(&bufRequest->Head, &bufReply->Head);

	    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
		bufReply->RetCode != KERN_SUCCESS) {
		    if (bufReply->RetCode == MIG_NO_REPLY)
			goto get_request;

		    /* don't destroy the reply port right,
			so we can send an error message */
		    bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
		    mach_msg_destroy(&bufRequest->Head);
	    }

	    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
		/* no reply port, so destroy the reply */
		if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
		    mach_msg_destroy(&bufReply->Head);

		goto get_request;
	    }

	    /* send reply and get next request */

	    bufTemp = bufRequest;
	    bufRequest = bufReply;
	    bufReply = bufTemp;

	    /*
	     *	We don't want to block indefinitely because the client
	     *	isn't receiving messages from the reply port.
	     *	If we have a send-once right for the reply port, then
	     *	this isn't a concern because the send won't block.
	     *	If we have a send right, we need to use MACH_SEND_TIMEOUT.
	     *	To avoid falling off the kernel's fast RPC path unnecessarily,
	     *	we only supply MACH_SEND_TIMEOUT when absolutely necessary.
	     */

	    mr = mach_msg_overwrite_trap(&bufRequest->Head,
			  (MACH_MSGH_BITS_REMOTE(bufRequest->Head.msgh_bits) ==
						MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
			  MACH_SEND_MSG|MACH_RCV_MSG|options :
			  MACH_SEND_MSG|MACH_SEND_TIMEOUT|MACH_RCV_MSG|options,
			  bufRequest->Head.msgh_size, max_size, rcv_name,
			  0, MACH_PORT_NULL, (mach_msg_header_t *) 0, 0);
	}

	/* a message error occurred */

	switch (mr) {
	  case MACH_SEND_INVALID_DEST:
	  case MACH_SEND_TIMED_OUT:
	    /* the reply can't be delivered, so destroy it */
	    mach_msg_destroy(&bufRequest->Head);
	    break;

	  case MACH_RCV_TOO_LARGE:
	    /* the kernel destroyed the request */
	    break;

	  default:
	    /* should only happen if the server is buggy */
	    (void)vm_deallocate(mach_task_self(),
				(vm_address_t) bufRequest,
				max_size + MAX_TRAILER_SIZE);
	    (void)vm_deallocate(mach_task_self(),
				(vm_address_t) bufReply,
				max_size + MAX_TRAILER_SIZE);
	    return mr;
	}
    }
}
