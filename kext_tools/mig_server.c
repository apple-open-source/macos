#include <CoreFoundation/CoreFoundation.h>

#include <mach/mach.h>
#include <mach/mach_port.h>
#include <servers/bootstrap.h>
#include <sysexits.h>

#include "logging.h"

/* MiG generated externals and functions */
extern struct rpc_subsystem _kextmanager_subsystem;
extern boolean_t kextmanager_server(mach_msg_header_t *, mach_msg_header_t *);

int gClientUID = -1;

boolean_t kextd_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    boolean_t processed = FALSE;

    mach_msg_format_0_trailer_t * trailer;

    /* Feed the request into the ("MiG" generated) server */
    if (!processed &&
        (request->msgh_id >= _kextmanager_subsystem.start &&
         request->msgh_id < _kextmanager_subsystem.end)) {

        /*
         * Get the caller's credentials (eUID/eGID) from the message trailer.
         */
        trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
            round_msg(request->msgh_size));

        if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
           (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {

            gClientUID = trailer->msgh_sender.val[0];
#if 0
            kextd_log("caller has eUID = %d, eGID = %d",
                trailer->msgh_sender.val[0],
                trailer->msgh_sender.val[1]);
#endif 0
        } else {
            kextd_error_log("caller's credentials not available");
            gClientUID = -1;

        }
 
        /*
         * Process kextd requests.
         */
        processed = kextmanager_server(request, reply);
    }

    if (!processed &&
        (request->msgh_id >= MACH_NOTIFY_FIRST &&
         request->msgh_id < MACH_NOTIFY_LAST)) {

        kextd_error_log("failed to process message");
    }

    if (!processed) {
        kextd_error_log("unknown message received");
    }

    return processed;
}


void kextd_mach_port_callback(
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info)
{
    mig_reply_error_t * bufRequest = msg;
    mig_reply_error_t * bufReply = CFAllocatorAllocate(
        NULL, _kextmanager_subsystem.maxsize, 0);
    mach_msg_return_t   mr;
    int                 options;

    /* we have a request message */
    (void) kextd_demux(&bufRequest->Head, &bufReply->Head);

    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        (bufReply->RetCode != KERN_SUCCESS)) {

        if (bufReply->RetCode == MIG_NO_REPLY) {
            /*
             * This return code is a little tricky -- it appears that the
             * demux routine found an error of some sort, but since that
             * error would not normally get returned either to the local
             * user or the remote one, we pretend it's ok.
             */
            CFAllocatorDeallocate(NULL, bufReply);
            return;
        }

        /*
         * destroy any out-of-line data in the request buffer but don't destroy
         * the reply port right (since we need that to send an error message).
         */
        bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
        mach_msg_destroy(&bufRequest->Head);
    }

    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
        /* no reply port, so destroy the reply */
        if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            mach_msg_destroy(&bufReply->Head);
        }
        CFAllocatorDeallocate(NULL, bufReply);
        return;
    }

    /*
     * send reply.
     *
     * We don't want to block indefinitely because the client
     * isn't receiving messages from the reply port.
     * If we have a send-once right for the reply port, then
     * this isn't a concern because the send won't block.
     * If we have a send right, we need to use MACH_SEND_TIMEOUT.
     * To avoid falling off the kernel's fast RPC path unnecessarily,
     * we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */

    options = MACH_SEND_MSG;
    if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) == MACH_MSG_TYPE_MOVE_SEND_ONCE) {
        options |= MACH_SEND_TIMEOUT;
    }
    mr = mach_msg(&bufReply->Head,        /* msg */
              options,            /* option */
              bufReply->Head.msgh_size,    /* send_size */
              0,            /* rcv_size */
              MACH_PORT_NULL,        /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,    /* timeout */
              MACH_PORT_NULL);        /* notify */


    /* Has a message error occurred? */
    switch (mr) {
        case MACH_SEND_INVALID_DEST:
        case MACH_SEND_TIMED_OUT:
            /* the reply can't be delivered, so destroy it */
            mach_msg_destroy(&bufReply->Head);
            break;

        default :
            /* Includes success case.  */
            break;
    }

    CFAllocatorDeallocate(NULL, bufReply);
}
