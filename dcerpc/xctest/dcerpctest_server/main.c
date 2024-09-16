//
//  main.c
//  dcerpctest_server
//
//  Created by William Conway on 12/1/23.
//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <compat/dcerpc.h>
#include "xctest_iface.h"
#include "utils.h"

// The following define will set DCERPC debug switches,
// causing the framework to emit verbose debug logging.
//
// #define Enable_DCERPC_DebugLogging 1

int sendReadyIndication(int sockfd);
static void wait_for_signals(void);

const char protocol[] = "ncacn_ip_tcp";
const char readyStr[] = "ready";

// *************************
// *** Server Procedures ***
// *************************

idl_boolean EchoUint8(
    /* [in] */ handle_t h,
    /* [in] */ unsigned8 in_num,
    /* [out] */ unsigned8 *out_num,
    /* [out] */ error_status_t *status
);

idl_boolean EchoUint16(
    /* [in] */ handle_t h,
    /* [in] */ unsigned16 in_num,
    /* [out] */ unsigned16 *out_num,
    /* [out] */ error_status_t *status
);

idl_boolean EchoUint32(
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 in_num,
    /* [out] */ unsigned32 *out_num,
    /* [out] */ error_status_t *status
);

idl_boolean EchoUint64(
    /* [in] */ handle_t h,
    /* [in] */ idl_uhyper_int in_num,
    /* [out] */ idl_uhyper_int *out_num,
    /* [out] */ error_status_t *status
);

idl_boolean ReverseStrArr(
    /* [in] */ handle_t h,
    /* [in] */ rev_str_arr_args *in_str,
    /* [out] */ rev_str_arr_args **out_str,
    /* [out] */ error_status_t *status
);

#ifdef Enable_DCERPC_DebugLogging
/* XXX this needs a home in the public headers ... */
extern void rpc__dbg_set_switches(
        const char      * /*s*/,
        unsigned32      * /*st*/);
#endif


int main(int argc, const char * argv[]) {
    unsigned32              status;
    rpc_binding_vector_p_t  server_binding;
    char                    *string_binding;
    int                     ch, err;
    int                     sockfd;
    struct sockaddr_un      clientAddr;
    socklen_t               clientAddrLen;
    char                    *usockPath = NULL;
    unsigned32              i;
    
    // Check options
    ch = getopt(argc, (char ** const)argv, "s:");
    while (ch != -1) {
        switch (ch) {
            case 's':
                printf("dcerpctest_server: Got socket option: '-%c' %s\n", optopt, optarg);
                usockPath = optarg;
            break;
                
            case '?':
                printf("dcerpctest_server: Unknown option: '%c'\n", optopt);
                break;
        }
        
        ch = getopt(argc, (char ** const)argv, "s:");
    }
    
    if (usockPath == NULL) {
        // Fatal error
        printf("dcerpctest_server: missing '-s sockPath' argument\n");
        return 1;
    }
    
    printf("dcerpctest_server: usockpath is %s\n", usockPath);
    
#ifdef Enable_DCERPC_DebugLogging
    // Turn on rpc_es_dbg_general = 1  (so we can catch the sock bind error)
    rpc__dbg_set_switches("1.1", &status);
    
    // Turn on rpc_es_dbg_auth = 17
    rpc__dbg_set_switches("17.1", &status);
#endif

    // Register the Interface with the RPC runtime library.
    printf ("dcerpctest_server: Registering the interface with the runtime library....\n");
    rpc_server_register_if(xctest_iface_v1_0_s_ifspec,
               NULL,
               NULL,
               &status);
        chk_dce_err(status, "rpc_server_register_if()", "dcerpctest_server main", 1);

        printf("dcerpctest_server: registered, preparing binding handle...\n");
    
        // Select the protocol sequence with the RPC runtime library.
        rpc_server_use_protseq_ep((unsigned_char_p_t)protocol,
                rpc_c_protseq_max_calls_default, (unsigned_char_p_t)xctest_endpoint, &status);
    
        chk_dce_err(status, "rpc_server_use_protseq_ep()", "dcerpctest_server main", 1);
        rpc_server_inq_bindings(&server_binding, &status);
        chk_dce_err(status, "rpc_server_inq_bindings()", "dcerpctest_server main", 1);

    // List out the servers enpoints (TCP and UDP port numbers)
    printf ("dcerpctest_server: Server's communications endpoints are:\n");
    for (i=0; i<RPC_FIELD_COUNT(server_binding); i++)
    {
        rpc_binding_to_string_binding(RPC_FIELD_BINDING_H(server_binding)[i],
                      (unsigned char **)&string_binding,
                      &status);
        if (string_binding) {
            printf("\t%s\n",string_binding);
        }
    }

    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sun_family = PF_UNIX;
    strcpy(clientAddr.sun_path, usockPath);
    clientAddrLen = sizeof(struct sockaddr_un);
    
    // We're ready to receive RPC calls.
    // Open the UNIX domain socket so we can send a Ready indication.
    sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("dcerpctest_server: socket error: %d\n", errno);
        return 1;
    }
    
    err = connect(sockfd, (struct sockaddr *)&clientAddr, clientAddrLen);
    if (err < 0) {
        printf("dcerpctest_server: connect error: %d\n", errno);
        close (sockfd);
        return 1;
    }
    
    err = sendReadyIndication(sockfd);
    if (err) {
        printf("dcerpctest_server: failed to send Ready indication on domain socket\n");
        close(sockfd);
        return 1;
    }
    
    // Close the socket and unlink the sock path
    close(sockfd);
    unlink(usockPath);

    // Start listening for calls
    printf("dcerpctest_server: listening for calls.... \n");
    
    // Start the signal waiting thread in background. This thread will
    // Catch SIGINT and gracefully shutdown the server.
    wait_for_signals();

    DCETHREAD_TRY
      {
        rpc_server_listen(rpc_c_listen_max_calls_default, &status);
      }
    DCETHREAD_CATCH_ALL(THIS_CATCH)
      {
        printf ("dcerpctest_server: Server stopped listening\n");
      }
    DCETHREAD_ENDTRY

    // If we reached this point, then the server was stopped, most likely
    // by the signal handler thread called rpc_mgmt_stop_server().
    // gracefully cleanup and unregister the interface

    printf("dcerpctest_server: Cleaning up communications endpoints... \n");
    rpc_server_unregister_if(xctest_iface_v1_0_s_ifspec,
                 NULL,
                 &status);
    chk_dce_err(status, "rpc_server_unregister_if()", "", 0);

    exit(0);
  }

int sendReadyIndication(int sockfd)
{
    int ret = 1;
    ssize_t nBytes, readyStrLen;
    
    // Include terminating NULL
    readyStrLen = strlen(readyStr) + 1;
    
    // Connected to the client. Send Ready indication
    nBytes = write(sockfd, readyStr, readyStrLen);
    if (nBytes != readyStrLen) {
        printf("serv:sendReadyIndication: write(2) problem, writeLenDesired: %lu, wrote: %lu, errno: %d\n",
               readyStrLen, nBytes, errno);
    } else {
        printf("serv:sendReadyInd: sent ready indication successfully\n");
        ret = 0;
    }
    
    return (ret);
}

// ********************************************
// *** Server implementation of EchoUint8() ***
// ********************************************
idl_boolean EchoUint8(
    /* [in] */ handle_t h,
    /* [in] */ unsigned8 in_num,
    /* [out] */ unsigned8 *out_num,
    /* [out] */ error_status_t *status)
{
    char * binding_info;
    error_status_t e;

    // Get some info about the client binding

    rpc_binding_to_string_binding(h, (unsigned char **)&binding_info, &e);
    if (e == rpc_s_ok) {
        printf ("dcerpctest_server: EchoUint8() called by client: %s\n", binding_info);
    }

    printf("\ndcerpctest_server: Function EchoUint8() -- input argments\n");

    printf("\tin_num = %u\n", in_num);

    printf ("\n=========================================\n");

    *out_num = in_num;
    *status = error_status_ok;

    return 1;
}

// *********************************************
// *** Server implementation of EchoUint16() ***
// *********************************************
idl_boolean EchoUint16(
    /* [in] */ handle_t h,
    /* [in] */ unsigned16 in_num,
    /* [out] */ unsigned16 *out_num,
    /* [out] */ error_status_t *status)
{
    char * binding_info;
    error_status_t e;

    // Get some info about the client binding

    rpc_binding_to_string_binding(h, (unsigned char **)&binding_info, &e);
    if (e == rpc_s_ok) {
        printf ("dcerpctest_server: EchoUint16() called by client: %s\n", binding_info);
    }

    printf("\ndcerpctest_server: Function EchoUint16() -- input argments\n");

    printf("\tin_num = %u\n", in_num);

    printf ("\n=========================================\n");

    *out_num = in_num;
    *status = error_status_ok;

    return 1;
    
}

// *********************************************
// *** Server implementation of EchoUint32() ***
// *********************************************
idl_boolean EchoUint32(
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 in_num,
    /* [out] */ unsigned32 *out_num,
    /* [out] */ error_status_t *status)
{
    char * binding_info;
    error_status_t e;

    
    // Get some info about the client binding
    rpc_binding_to_string_binding(h, (unsigned char **)&binding_info, &e);
    if (e == rpc_s_ok)
    {
        printf ("dcerpctest_server: EchoUint32() called by client: %s\n", binding_info);
    }

    printf("\ndcerpctest_server: Function EchoUint32() -- input argments\n");

    printf("\tin_num = %u\n", in_num);

    printf ("\n=========================================\n");

    *out_num = in_num;
    *status = error_status_ok;

    return 1;

  }

// *********************************************
// *** Server implementation of EchoUint64() ***
// *********************************************
idl_boolean EchoUint64(
    /* [in] */ handle_t h,
    /* [in] */ idl_uhyper_int in_num,
    /* [out] */ idl_uhyper_int *out_num,
    /* [out] */ error_status_t *status)
{
    char * binding_info;
    error_status_t e;

    // Get some info about the client binding
    rpc_binding_to_string_binding(h, (unsigned char **)&binding_info, &e);
    if (e == rpc_s_ok)
    {
        printf ("dcerpctest_server: EchoUint64() called by client: %s\n", binding_info);
    }

    printf("\ndcerpctest_server: Function EchoUint64() -- input argments\n");

    printf("\tin_num = 0x%llx\n", in_num);

    printf ("\n=========================================\n");

    *out_num = in_num;
    *status = error_status_ok;

    return 1;
  }

idl_boolean ReverseStrArr(
    /* [in] */ handle_t h,
    /* [in] */ rev_str_arr_args *in_str,
    /* [out] */ rev_str_arr_args **out_str,
    /* [out] */ error_status_t *status)
{
    char * binding_info;
    uint32_t i, j, outputSizeNeeded;
    rev_str_arr_args *returnArr;
    size_t slen;
    error_status_t e;
    
    // Get some info about the client binding
    rpc_binding_to_string_binding(h, (unsigned char **)&binding_info, &e);
    if (e == rpc_s_ok)
    {
        printf ("dcerpctest_server: EchoUint64() called by client: %s\n", binding_info);
    }
    
    printf("\n\nFunction ReverseStrArr() -- input argments\n");

    for (i = 0; i < in_str->arr_len; i++) {
        printf("\t[str_arr[%d]: ---> %s\n", i, in_str->str_arr[i]);
    }

    printf ("\n=========================================\n");
    
    // Allocate the output string array as dynamic storage bound
    // to this RPC instance.  The output string array is the same size
    // as the input string array.
    outputSizeNeeded = sizeof(rev_str_arr_args) + in_str->arr_len * sizeof(string_t *);
    returnArr = rpc_ss_allocate(outputSizeNeeded);
    if (!returnArr) {
        printf("dcerpctest_server: ReverseStrArr: rpc_ss_allocate failed us\n");
        *status = rpc_s_no_memory;
        return 0;
    }
    
    returnArr->arr_len = in_str->arr_len;
    
    // Now allocate storage for each array element
    for (i = 0; i < in_str->arr_len; i++) {
        slen = strlen((const char *)in_str->str_arr[i]) + 1;
        returnArr->str_arr[i] = rpc_ss_allocate((uint32_t)slen);
        
        if (returnArr->str_arr[i] == NULL) {
            // This is not good.
            printf("Alloc failure for str element %u\n", i);
            *status = rpc_s_no_memory;
            return 0;
        }
    }
    
    // Now copy input strings to output array in reverse fashion
    for (i = 0; i < in_str->arr_len; i++) {
        slen = strlen((const char *)in_str->str_arr[i]);
        for (j = 0; j < slen; j++) {
            returnArr->str_arr[i][j] = in_str->str_arr[i][slen - 1 - j];
        }
        returnArr->str_arr[i][j] = 0;
    }
    
    // Set our output str array
    *out_str = returnArr;
    *status = rpc_s_ok;
    
    return 1;
}

  /*=========================================================================
   *
   * wait_for_signals()
   *
   *
   * Set up the process environment to properly deal with signals.
   * By default, we isolate all threads from receiving asynchronous
   * signals. We create a thread that handles all async signals.
   * The signal handling actions are handled in the handler thread.
   *
   * For AIX, we cant use a thread that sigwaits() on a specific signal,
   * we use a plain old, lame old Unix signal handler.
   *
   *=========================================================================*/
  void
  wait_for_signals(void)
  {
      sigset_t signals;

      sigemptyset(&signals);
      sigaddset(&signals, SIGINT);

      dcethread_signal_to_interrupt(&signals, dcethread_self());
  }
