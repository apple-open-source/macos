/* ex: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 * echo_client  : demo DCE RPC application
 *
 * Jim Doyle, jrd@bu.edu, 09-05-1998
 *
 *
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <compat/dcerpc.h>
#include "echo.h"
#include "misc.h"

#ifndef HAVE_GETOPT_H
#include "getopt.h"
#endif

#define MAX_USER_INPUT 128
#define MAX_LINE 100 * 1024

#ifdef _WIN32
#define EOF_STRING "^Z"
#else
#define EOF_STRING "^D"
#endif

/*
 * Forward declarations
 */

static int
get_client_rpc_binding(
    rpc_binding_handle_t * binding_handle,
    rpc_if_handle_t interface_spec,
    const char * hostname,
    const char * protocol,
    const char * endpoint
    );

/*
 * usage()
 */

static void usage(void)
{
    printf("usage: echo_client [-h hostname] [-e endpoint] [-n] [-u] [-t]\n");
    printf("         -h:  specify host of RPC server (default is localhost)\n");
    printf("         -e:  specify endpoint for protocol\n");
    printf("         -n:  use named pipe protocol\n");
    printf("         -u:  use UDP protocol\n");
    printf("         -t:  use TCP protocol (default)\n");
    printf("         -g:  instead of prompting, generate a data string of the specified length\n");
    printf("         -d:  turn on debugging\n");
    printf("\n");
    exit(1);
}

/* XXX this needs a home in the public headers ... */
extern void rpc__dbg_set_switches(
        const char      * /*s*/,
        unsigned32      * /*st*/
    );

int
main(
    int argc,
    char *argv[]
    )
{

    /*
     * command line processing and options stuff
     */

    extern char *optarg;
    extern int optind, opterr, optopt;
    int c;

    const char * rpc_host = "127.0.0.1";
    const char * protocol = PROTOCOL_TCP;
    const char * endpoint = NULL;

    char buf[MAX_LINE+1];

    /*
     * stuff needed to make RPC calls
     */

    unsigned32 status;
    rpc_binding_handle_t echo_server;
    args * inargs;
    args * outargs;
    int ok;
    unsigned32 i;
    int generate_length = -1;

    char * nl;

    /*
     * Process the cmd line args
     */

    while ((c = getopt(argc, argv, "h:e:nutdg:")) != EOF)
    {
        switch (c)
        {
        case 'h':
            rpc_host = optarg;
            break;
        case 'e':
            endpoint = optarg;
            break;
        case 'n':
            protocol = PROTOCOL_NP;
            break;
        case 'u':
            protocol = PROTOCOL_UDP;
            break;
        case 't':
            protocol = PROTOCOL_TCP;
            break;
        case 'd':
#ifdef _WIN32
	    printf("This option is only supported on Linux.\n");
#else
            rpc__dbg_set_switches("0-19.10", &status);
            //Skip 20, which is memory allocs and frees
            rpc__dbg_set_switches("21-43.10", &status);
#endif
            break;
        case 'g':
            generate_length = strtol(optarg, NULL, 10);
            break;
        default:
            usage();
        }
    }

    /*
     * Get a binding handle to the server using the following params:
     *
     *  1. the hostname where the server lives
     *  2. the interface description structure of the IDL interface
     *  3. the desired transport protocol (UDP or TCP)
     */

    if (get_client_rpc_binding(&echo_server,
                               echo_v1_0_c_ifspec,
                               rpc_host,
                               protocol,
                               endpoint) == 0)
    {
        printf ("Couldn't obtain RPC server binding. exiting.\n");
        exit(1);
    }


    /*
     * Allocate an "args" struct with enough room to accomodate
     * the max number of lines of text we can can from stdin.
     */

    inargs = (args *)malloc(sizeof(args) + MAX_USER_INPUT * sizeof(string_t));
    if (inargs == NULL) printf("FAULT. Didn't allocate inargs.\n");

    if (generate_length < 0)
    {
        /*
         * Get text from the user and pack into args.
         */

        printf ("enter stuff (%s on an empty line when done):\n\n\n", EOF_STRING);
        i = 0;
        while (!feof(stdin) && i < MAX_USER_INPUT )
        {
            if (NULL==fgets(buf, MAX_LINE, stdin))
                break;
            if ((nl=strchr(buf, '\n')))                   /* strip the newline */
                *nl=0;
            inargs->argv[i] = (string_t)strdup(buf);      /* copy from buf */
            i++;
        }
        inargs->argc = i;
    }
    else
    {
        inargs->argv[0] = malloc(generate_length + 1);
        inargs->argv[0][0] = 's';

        for(i = 1; i < (unsigned long)generate_length; i++)
        {
            inargs->argv[0][i] = i%10 + '0';
        }

        if(generate_length > 0)
            inargs->argv[0][generate_length - 1] = 'e';
        inargs->argv[0][generate_length] = '\0';
        inargs->argc = 1;
    }

    /*
     * Do the RPC call
     */

    printf ("calling server\n");
    ok = ReverseIt(echo_server, inargs, &outargs, &status);

    /*
     * Print the results
     */

    if (ok && status == error_status_ok)
    {
        printf ("got response from server. results: \n");
        for (i=0; i<outargs->argc; i++)
            printf("\t[%d]: %s\n", i, outargs->argv[i]);
        printf("\n===================================\n");

    }

    if (status != error_status_ok)
        chk_dce_err(status, "ReverseIt()", "main()", 1);

    /*
     * Done. Now gracefully teardown the RPC binding to the server
     */

    rpc_binding_free(&echo_server, &status);
    exit(0);

}

/*==========================================================================
 *
 * get_client_rpc_binding()
 *
 *==========================================================================
 *
 * Gets a binding handle to an RPC interface.
 *
 * parameters:
 *
 *    [out]     binding_handle
 *    [in]      interface_spec <- DCE Interface handle for service
 *    [in]      hostname       <- Internet hostname where server lives
 *    [in]      protocol       <- "ncacn_ip_tcp", etc.
 *    [in]      endpoint       <- optional
 *
 *==========================================================================*/

static int
get_client_rpc_binding(
    rpc_binding_handle_t * binding_handle,
    rpc_if_handle_t interface_spec,
    const char * hostname,
    const char * protocol,
    const char * endpoint
    )
{
    unsigned_char_p_t string_binding = NULL;
    error_status_t status;

    /*
     * create a string binding given the command line parameters and
     * resolve it into a full binding handle using the endpoint mapper.
     *  The binding handle resolution is handled by the runtime library
     */

    rpc_string_binding_compose(NULL,
			       (unsigned_char_p_t)protocol,
			       (unsigned_char_p_t)hostname,
			       (unsigned_char_p_t)endpoint,
			       NULL,
			       &string_binding,
			       &status);
    chk_dce_err(status, "rpc_string_binding_compose()", "get_client_rpc_binding", 1);


    rpc_binding_from_string_binding(string_binding,
                                    binding_handle,
                                    &status);
    chk_dce_err(status, "rpc_binding_from_string_binding()", "get_client_rpc_binding", 1);

    if (!endpoint)
    {
        /*
         * Resolve the partial binding handle using the endpoint mapper
         */

        rpc_ep_resolve_binding(*binding_handle,
                               interface_spec,
                               &status);
        chk_dce_err(status, "rpc_ep_resolve_binding()", "get_client_rpc_binding", 1);
    }

    rpc_string_free(&string_binding, &status);
    chk_dce_err(status, "rpc_string_free()", "get_client_rpc_binding", 1);

    /*
     * Get a printable rendition of the binding handle and echo to
     * the user.
     */

    rpc_binding_to_string_binding(*binding_handle,
                                  (unsigned char **)&string_binding,
                                  &status);
    chk_dce_err(status, "rpc_binding_to_string_binding()", "get_client_rpc_binding", 1);

    printf("fully resolving binding for server is: %s\n", string_binding);

    rpc_string_free(&string_binding, &status);
    chk_dce_err(status, "rpc_string_free()", "get_client_rpc_binding", 1);

    return 1;
}
