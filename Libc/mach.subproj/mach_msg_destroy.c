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

#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_init.h>

static void mach_msg_destroy_port(mach_port_t, mach_msg_type_name_t);
static void mach_msg_destroy_memory(vm_offset_t, vm_size_t);

/*
 *	Routine:	mach_msg_destroy
 *	Purpose:
 *		mach_msg_destroy is useful in two contexts.
 *
 *		First, it can deallocate all port rights and
 *		out-of-line memory in a received message.
 *		When a server receives a request it doesn't want,
 *		it needs this functionality.
 *
 *		Second, it can mimic the side-effects of a msg-send
 *		operation.  The effect is as if the message were sent
 *		and then destroyed inside the kernel.  When a server
 *		can't send a reply (because the client died),
 *		it needs this functionality.
 */
void
mach_msg_destroy(mach_msg_header_t *msg)
{
    mach_msg_bits_t mbits = msg->msgh_bits;

    /*
     *	The msgh_local_port field doesn't hold a port right.
     *	The receive operation consumes the destination port right.
     */

    mach_msg_destroy_port(msg->msgh_remote_port, MACH_MSGH_BITS_REMOTE(mbits));

    if (mbits & MACH_MSGH_BITS_COMPLEX) {
	mach_msg_body_t		*body;
	mach_msg_descriptor_t	*saddr, *eaddr;
	
    	body = (mach_msg_body_t *) (msg + 1);
    	saddr = (mach_msg_descriptor_t *) 
			((mach_msg_base_t *) msg + 1);
    	eaddr =  saddr + body->msgh_descriptor_count;

	for  ( ; saddr < eaddr; saddr++) {
	    switch (saddr->type.type) {
	    
	        case MACH_MSG_PORT_DESCRIPTOR: {
		    mach_msg_port_descriptor_t *dsc;

		    /* 
		     * Destroy port rights carried in the message 
		     */
		    dsc = &saddr->port;
		    mach_msg_destroy_port(dsc->name, dsc->disposition);		
		    break;
	        }

	        case MACH_MSG_OOL_DESCRIPTOR : {
		    mach_msg_ool_descriptor_t *dsc;

		    /* 
		     * Destroy memory carried in the message 
		     */
		    dsc = &saddr->out_of_line;
		    if (dsc->deallocate) {
		        mach_msg_destroy_memory((vm_offset_t)dsc->address,
						dsc->size);
		    }
		    break;
	        }

	        case MACH_MSG_OOL_PORTS_DESCRIPTOR : {
		    mach_port_t             		*ports;
		    mach_msg_ool_ports_descriptor_t	*dsc;
		    mach_msg_type_number_t   		j;

		    /*
		     * Destroy port rights carried in the message 
		     */
		    dsc = &saddr->ool_ports;
		    ports = (mach_port_t *) dsc->address;
		    for (j = 0; j < dsc->count; j++, ports++)  {
		        mach_msg_destroy_port(*ports, dsc->disposition);
		    }

		    /* 
		     * Destroy memory carried in the message 
		     */
		    if (dsc->deallocate) {
		        mach_msg_destroy_memory((vm_offset_t)dsc->address, 
					dsc->count * sizeof(mach_port_t));
		    }
		    break;
	        }
	    }
	}
    }
}

static void
mach_msg_destroy_port(mach_port_t port, mach_msg_type_name_t type)
{
    if (MACH_PORT_VALID(port)) switch (type) {
      case MACH_MSG_TYPE_MOVE_SEND:
      case MACH_MSG_TYPE_MOVE_SEND_ONCE:
	/* destroy the send/send-once right */
	(void) mach_port_deallocate(mach_task_self(), port);
	break;

      case MACH_MSG_TYPE_MOVE_RECEIVE:
	/* destroy the receive right */
	(void) mach_port_mod_refs(mach_task_self(), port,
				  MACH_PORT_RIGHT_RECEIVE, -1);
	break;

      case MACH_MSG_TYPE_MAKE_SEND:
	/* create a send right and then destroy it */
	(void) mach_port_insert_right(mach_task_self(), port,
				      port, MACH_MSG_TYPE_MAKE_SEND);
	(void) mach_port_deallocate(mach_task_self(), port);
	break;

      case MACH_MSG_TYPE_MAKE_SEND_ONCE:
	/* create a send-once right and then destroy it */
	(void) mach_port_extract_right(mach_task_self(), port,
				       MACH_MSG_TYPE_MAKE_SEND_ONCE,
				       &port, &type);
	(void) mach_port_deallocate(mach_task_self(), port);
	break;
    }
}

static void
mach_msg_destroy_memory(vm_offset_t addr, vm_size_t size)
{
    if (size != 0)
	(void) vm_deallocate(mach_task_self(), addr, size);
}
