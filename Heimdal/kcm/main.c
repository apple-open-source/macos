/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kcm_locl.h"

#ifdef __APPLE__
#include <sandbox.h>
#endif

sig_atomic_t exit_flag = 0;

krb5_context kcm_context = NULL;

const char *service_name = "org.h5l.kcm";

static void
terminated(void *ctx)
{
    exit(0);
}

static void
sigusr1(void *ctx)
{
    kcm_debug_ccache(kcm_context);
}

static void
sigusr2(void *ctx)
{
    kcm_debug_events(kcm_context);
}

static void
timeout_handler(void)
{
    kcm_write_dump(kcm_context);
    exit(0);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    setprogname(argv[0]);

    ret = krb5_init_context(&kcm_context);
    if (ret) {
	errx (1, "krb5_init_context failed: %d", ret);
	return ret;
    }

    kcm_configure(argc, argv);

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGPIPE, &sa, NULL);
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    heim_sipc_signal_handler(SIGINT, terminated, "SIGINT");
    heim_sipc_signal_handler(SIGTERM, terminated, "SIGTERM");
    heim_sipc_signal_handler(SIGUSR1, sigusr1, NULL);
    heim_sipc_signal_handler(SIGUSR2, sigusr2, NULL);
#ifdef SIGXCPU
    heim_sipc_signal_handler(SIGXCPU, terminated, "CPU time limit exceeded");
#endif


#ifdef SUPPORT_DETACH
    if (detach_from_console)
	daemon(0, 0);
#endif
    kcm_session_setup_handler();

    kcm_read_dump(kcm_context);
    
    if (launchd_flag) {
	heim_sipc mach;
	heim_sipc_launchd_mach_init(service_name, kcm_service, NULL, &mach);
    } else {
	heim_sipc un;
	heim_sipc_service_unix(service_name, kcm_service, NULL, &un);
    }

#ifdef __APPLE__
    {
	char *errorstring;
	ret = sandbox_init("kcm", SANDBOX_NAMED, &errorstring);
	if (ret)
	    errx(1, "sandbox_init failed: %d: %s", ret, errorstring);
    }
#endif /* __APPLE__ */

    heim_sipc_set_timeout_handler(timeout_handler);

    /*
     * If we have didn't get a time or, and it was non zero,
     * lets set the timeout
     */
    if (kcm_timeout < 0)
	kcm_timeout = 15;
    if (kcm_timeout != 0)
	heim_sipc_timeout(kcm_timeout);

    heim_ipc_main();

    krb5_free_context(kcm_context);
    return 0;
}
