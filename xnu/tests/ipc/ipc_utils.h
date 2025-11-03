#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/code_signing.h>

/*
 * Common IPC utilities for XNU tests
 *
 * This header provides standardized message structures and utilities
 * for common IPC patterns used across multiple test files.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Message structure for sending a single port */
typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t port;
} ipc_single_port_msg_t;

/* Message structure for sending multiple ports in an array */
typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_ool_ports_descriptor_t ports_descriptor;
} ipc_port_array_msg_t;

/* Generic message structure with trailer for receiving */
typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	union {
		/* Single port descriptor */
		struct {
			mach_msg_port_descriptor_t port;
		} single;

		/* Port array descriptor */
		struct {
			mach_msg_ool_ports_descriptor_t ports_descriptor;
		} array;
	} data;
	mach_msg_max_trailer_t trailer;
} ipc_receive_msg_t;

/*
 * Port Management Functions
 */

/* Create a new receive port with send right */
mach_port_t ipc_create_receive_port(void);

/* Create a receive port with specific options */
mach_port_t ipc_create_receive_port_with_options(uint32_t mpo_flags);

/* Safely deallocate a port */
void ipc_deallocate_port(mach_port_t port);

/* Insert a send right to a receive right */
kern_return_t ipc_insert_send_right(mach_port_t receive_port);

/*
 * Single Port Messaging Functions
 */

/* Send a single port to destination */
kern_return_t ipc_send_port(mach_port_t destination, mach_port_t port,
    mach_msg_type_name_t disposition);

/* Receive a single port from destination */
kern_return_t ipc_receive_port(mach_port_t destination, mach_port_t *port);

/*
 * Port Array Messaging Functions
 */

/* Send an array of ports to destination */
kern_return_t ipc_send_port_array(mach_port_t destination,
    mach_port_t *ports, mach_msg_type_number_t count,
    mach_msg_type_name_t disposition);

/* Receive an array of ports from destination */
kern_return_t ipc_receive_port_array(mach_port_t destination,
    mach_port_t **ports, mach_msg_type_number_t *count);

/*
 * Generic Messaging Functions
 */

/* Send a pre-constructed message */
kern_return_t ipc_send_message(mach_msg_header_t *msg);

/* Receive a message into a pre-allocated buffer */
kern_return_t ipc_receive_message(mach_port_t destination, ipc_receive_msg_t *msg,
    mach_msg_size_t max_size);

/*
 * Security and Code Signing Utilities
 */

/* Check if IPC hardening is disabled (CS_CONFIG_GET_OUT_OF_MY_WAY) */
bool ipc_hardening_disabled(void);

#ifdef __cplusplus
}
#endif

#endif /* IPC_UTILS_H */
