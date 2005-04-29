/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**  NAME
**
**      comp.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of storage internal to Common Communications Service component.
**
**
*/

#include <commonp.h>    /* Common internals for RPC Runtime system      */
#include <com.h>        /* Externals for Common Services component      */
#include <comprot.h>    /* Externals for common Protocol Services       */
#include <comnaf.h>     /* Externals for common NAF Services            */
#include <comp.h>       /* Internals for Common Services component      */
#include <comfwd.h>     /* Externals for Common Services Fwd component  */
#include <npnaf.h>

/***********************************************************************/
/*
 * R P C _ G _ I N I T I A L I Z E D
 *
 * The value that indicates whether or not the RPC runtime has previously
 * been initialized (via a call to rpc__init). Its declaration is
 * in com.h since the Naming Service may need to invoke the
 * RPC_VERIFY_INIT macro.
 */
GLOBAL boolean              rpc_g_initialized = false;


/***********************************************************************/
/*
 * R P C _ G _ T H R E A D _ C O N T E X T _ K E Y
 *
 * The key visible to all threads that contains a pointer to the
 * per-thread context block.
 */
GLOBAL pthread_key_t        rpc_g_thread_context_key;

     
/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ M U T E X
 *
 * The global mutex used for the entire Communications Service. Note
 * that this may be temporary since a per-data structure mutex may
 * be employed. Its declaration is in com.h since RPC Protocol
 * Services will need to reference it through the mutex macros.
 */
GLOBAL rpc_mutex_t          rpc_g_global_mutex;


/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ B I N D I N G _ C O N D
 *
 * The global binding handle condition variable used for call
 * serialization.  This condition variable is protected by the global
 * mutex.  Note that this may be temporary since a per-binding handle
 * cond var / mutex may be employed.
 */
GLOBAL rpc_cond_t           rpc_g_global_binding_cond;


/***********************************************************************/
/*
 * R P C _ G _ F O R K _ C O U N T
 *
 * The global fork count used to detect when a process using
 * RPC has forked.  After a fork, the parent is allowed to proceed
 * unaffected--all RPC helper threads are restarted, and
 * all RPC state maintained.  In the child, however, all state
 * is dropped, and the RPC initialization process will have
 * to be repeated if the child tries to use RPC.  
 * 
 * The counter here is necessitated by the fact that it is possible
 * for the application to hold the only reference to certain
 * data structures allocated by the runtime;  the example of concern
 * here is binding handles.  Since we have no way at present
 * of tracking dow these binding handles during the normal fork
 * handling, we need some way to recognize them if the application
 * tries to use one *after* the fork.  By storing the current
 * global fork count in each handle, and then incrementing the
 * global count in the child of a fork, we can recognize such
 * handles the next time they are used.  Protocol specific routine
 * are then called to perform whatever actions are necessary to
 * drop any state associated with the handle.
 *
 * Note that we are currently stranding memory across forks; eg.
 * call handles, mutexes, etc.  This is considered a necessary and
 * acceptable evil based on the following considerations:
 *
 *     1) The reset/release/etc. routines that exist today are
 *        designed to carry out any pending operations associated
 *        with the data structure.  For example, before freeing
 *        a call handle, the DG code will try to send out any
 *        pending acknowledgements.  For this reason, most of
 *        these routines are unusable.
 *     2) It is too late to redesign all of these routines to 
 *        incorporate the correct fork-aware behavior.
 *     3) Vaporizing all state in the child of a fork creates a 
 *        scenario that is easier to understand, and implement
 *        correctly.
 *     4) The amount of memory stranded does not seem to be enough
 *        to worry about.  We are assuming that the child of
 *        a fork will not itself fork a child that will itself
 *        fork a child, etc., eventually filling the VA space
 *        with stranded memory.
 */
GLOBAL unsigned32 rpc_g_fork_count;


/***********************************************************************/
/*
 * R P C _ G _ F W D _ F N
 *
 * The global forwarding map function variable.  Its value indicates
 * whether or not the RPC runtime should be performing forwarding services
 * and if so, the forwarding map function to use.
 */
GLOBAL rpc_fwd_map_fn_t     rpc_g_fwd_fn = NULL;

/***********************************************************************/
/*
 * R P C _ G _ S E R V E R _ P T H R E A D _ A T T R
 *
 * A pthread attribute for server thread creation.
 * Initialized by rpc_init().
 */
GLOBAL pthread_attr_t   rpc_g_server_pthread_attr;

/***********************************************************************/
/*
 * R P C _ G _ R U N T I M E _ P T H R E A D _ A T T R
 *
 * A pthread attribute for internal thread creation.  This parameter
 * is of particular interest to those threads internal to the runtime
 * that can call security routines.  These threads include: the
 * network listener and the receiver threads.
 * Initialized by rpc_init().
 */
GLOBAL pthread_attr_t   rpc_g_default_pthread_attr;

/***********************************************************************/
/*
 * RPCMEM package statistics.  Extern'd in "rpcmem.h>.
 *
 * ### !!! seems like this should be in the (non-existent) "rpcmem.c" file.
 */
GLOBAL rpc_mem_stats_elt_t  rpc_g_mem_stats[RPC_C_MEM_MAX_TYPES];

/***********************************************************************/
/*
 * R P C _ G _ N S _ S P E C I F I C _ F R E E _ F N
 *
 * The global NS binding->ns_specific free function.  The NS init routine
 * inits this.  It's purpose is to prevent the runtime from always pulling
 * in the name service modules when they're not necessary.
 */
GLOBAL rpc_g_ns_specific_free_fn_t  rpc_g_ns_specific_free_fn = NULL;


/***********************************************************************/
/*
 * R P C _ G _ P R O T S E Q _ I D
 *
 * The RPC Protocol Sequence ID table.  This table is indexed by an RPC
 * Protocol Sequence ID.
 *
 * An RPC Protocol Sequence represents a specific RPC Protocol/Network
 * Address Family combination which is by definition a valid combination
 * of protocols. An RPC Protocol Sequence also represents a specific
 * NAF interface type, since there may be multiple within a NAF. Each
 * RPC Protocol Sequence has an entry in this table.
 *
 * Note that the ".rpc_protseq_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 *
 * The fields are:
 *
 *      supported       A boolean flag initialized to zero and filled
 *                      in by rpc__init if it determines that this Protocol
 *                      Sequence is actually supported by the system.
 *
 *      rpc_protseq_id  A constant identifier for the Protocol Sequence.
 *      
 *      rpc_protocol_id A constant identifier for the RPC Protocol used
 *                      in this Protocol Sequence.
 *      
 *      naf_id          A constant identifier for the Network Address
 *                      Family used in this Protocol Sequence.
 *      
 *      net_protocol_id A constant identifier for the network protocol
 *                      used in this Protocol Sequence.
 *      
 *      net_if_id       A constant identifier for the network interface
 *                      type used in this Protocol Sequence.
 *      
 *      rpc_protseq     A string constant defining this Protocol Sequence.
 *
 *      port_restriction_list_p
 *                      An optionally pointer to a port_restriction_list 
 *                      object.
 */

GLOBAL 
#ifdef VMS
/* Provide psect name if VMS */
{"rpc_sym9_g_protseq_id"}
#endif
rpc_protseq_id_elt_t     rpc_g_protseq_id[RPC_C_PROTSEQ_ID_MAX] = 
{
    {                                   /* Connection-RPC / NetBIOS / SMB */
        0,                              
        RPC_C_PROTSEQ_ID_NCACN_NP,
        RPC_C_PROTOCOL_ID_NCACN,
        RPC_C_NAF_ID_NP,
        RPC_C_NETWORK_PROTOCOL_ID_NP,
	RPC_C_NETWORK_IF_ID_STREAM,
        RPC_PROTSEQ_NCACN_NP,
        (rpc_port_restriction_list_p_t) NULL
    },
};


/***********************************************************************/
/*
 * R P C _ G _ P R O T O C O L _ I D
 *
 * The RPC Protocol ID table.  Each RPC Protocol has an entry in this
 * table.  This table is index by RPC Protocol ID.
 *
 * Note that the ".rpc_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 *
 * The fields are:
 *
 *      prot_init       The address of an initialization routine in the
 *                      Protocol Service that will be called by rpc__init.
 *      
 *      prot_fork_handler  The address of a routine to call to handle 
 *                      protocol specific, fork-related processing.
 *
 *      rpc_protocol_id A constant identifier for this RPC Protocol.
 *      
 *      call_epv        An entry point vector for the Call Services in
 *                      the Protocol Service.
 *      
 *      mgmt_epv        An entry point vector for the Management Services in 
 *                      the Protocol Service.
 *      
 *      binding_epv     An entry point vector for the Binding Services
 *                      in the Protocol Service.
 *      
 *      network_epv     An entry point vector for the Network Services
 *                      in the Protocol Service.
 */

GLOBAL rpc_protocol_id_elt_t     rpc_g_protocol_id[RPC_C_PROTOCOL_ID_MAX] = 
{
#ifdef PROT_NCACN
    {
        rpc__ncacn_init,                /* Connection-RPC */
        NULL,
        RPC_C_PROTOCOL_ID_NCACN,
        NULL, NULL, NULL, NULL 
    },
#else
    {NULL},
#endif

#ifdef PROT_NCATP
    ,{
        rpc__ncatp_init,                /* Test-RPC */
        NULL,
        RPC_C_PROTOCOL_ID_NCATP,
        NULL, NULL, NULL, NULL 
    }
#endif
};

/***********************************************************************/
/*
 * R P C _ G _ N A F _ I D
 *
 * The Network Address Family ID table.  This table is indexed by a NAF
 * ID.
 *
 * Each Network Address Family Extension has an entry in this table.
 * Note that this is a sparse table because it uses the Unix Address
 * Family ID's as NAF ID's.
 *
 * Note that the ".naf_id" field of i'th element in the table is always
 * "i".  While redundant, this is useful so that you can pass pointers
 * to individual table elements.
 * 
 * The fields are:
 *
 *      naf_init        The address of an initialization routine in the
 *                      NAF Service that will be called by rpc__init
 *
 *      naf_id          A constant identifier for this NAF.
 *
 *      net_if_id       A constant identifier for the network interface
 *                      type used in the NAF initialization routine (when
 *                      determining if this NAF is supported).
 *      
 *      naf_epv         An entry point vector for the NAF Service.
 */

GLOBAL rpc_naf_id_elt_t     rpc_g_naf_id[RPC_C_NAF_ID_MAX] = 
{
    {
        rpc__np_init,
        RPC_C_NAF_ID_NP,
        RPC_C_NETWORK_IF_ID_STREAM,
        NULL 
    }
};

/***********************************************************************/
/*
 * R P C _ G _ A U T H N _ P R O T O C O L _ I D
 *
 * The RPC Authentication Protocol ID table.
 *
 * Each RPC Authentication protocol has an entry in this table.  These
 * entries include the following fields:
 *
 *      auth_init           The address of an initialization routine in the
 *                          Authentication Service that will be called by 
 *                          rpc__init.
 *
 *      authn_protocol_id   A constant identifier for this Authentication Service.
 *
 *      dce_rpc_authn_protocol_id_t
 *                          The value that goes into RPC protocol messages to
 *                          identify which authentication protocol is in use.
 *
 *      epv                 An entry point vector for the Authentication Service
 *                          functions.
 *
 * Note that the ".authn_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 *
 * Note that the ".auth_protocol_id" contains API values (see
 * "rpc_c_authn_..." constants in "rpc.idl").
 * "dce_rpc_authn_protocol_id_t" contains architectural values that appear
 * in network messages (see "dce_c_rpc_authn_protocol_..." constants in
 * "nbase.idl").
 */

GLOBAL rpc_authn_protocol_id_elt_t rpc_g_authn_protocol_id[RPC_C_AUTHN_PROTOCOL_ID_MAX] =
{

    {                               /* 0 */
        NULL, 
        rpc_c_authn_none, 
        dce_c_rpc_authn_protocol_none, 
        NULL,
        0
    },
    {                               /* 1 */
#ifdef AUTH_KRB 
        rpc__krb_init,
#else
        NULL,
#endif
        rpc_c_authn_dce_private,
        dce_c_rpc_authn_protocol_krb5,
        NULL,
        0
    },
    {                               /* 2 (reserved for dce_public) */
        NULL, 
        rpc_c_authn_dce_public,
        /* dce_c_rpc_authn_protocol_... */ 0,
        NULL,
        0
    },
    {                               /* 3 */
#ifdef AUTH_DUMMY
        rpc__noauth_init,
#else
        NULL,
#endif
        rpc_c_authn_dce_dummy,
        dce_c_rpc_authn_protocol_dummy,
        NULL,
        0
    },
    {                               /* 4 (reserved for dssa_public) */
        NULL,
        rpc_c_authn_dssa_public, 
        0,
        NULL,
        0
    }
};


