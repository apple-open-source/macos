/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  provide helper function for ipsec operations.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/dlil.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/pfkeyv2.h>
#include <pthread.h>
#include <sys/kern_event.h>
#include <netinet/in_var.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <netinet6/ipsec.h>

#include "libpfkey.h"
#include "ipsec_utils.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */


#include "../../../Controller/ppp_msg.h"
#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppp.h"
#include "../../../Family/ppp_domain.h"
#include "../L2TP-extension/l2tpk.h"
#include "../../../Helpers/pppd/pppd.h"
#include "../../../Helpers/pppd/fsm.h"
#include "../../../Helpers/pppd/lcp.h"
#include "l2tp.h"

/* -----------------------------------------------------------------------------
    globals
----------------------------------------------------------------------------- */
 
/* -----------------------------------------------------------------------------
    Function Prototypes
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
check if racoon is started with the same configuration file
returns:
    0: racoon is correctly started
    1: racoon is already started with our configuration
    -1: racoon is already started but with a different configuration
    -2: failed to start racoon
----------------------------------------------------------------------------- */
int 
start_racoon(CFBundleRef bundle, char *filename)
{
    int   	pid, err, tries;
    CFURLRef	url;
    char 	name[MAXPATHLEN]; 

    name[0] = 0;
    
    /* if a bundle and a name are passed, start racoon with the specific file */
    if (bundle) {
        if (url = CFBundleCopyBundleURL(bundle)) {
            CFURLGetFileSystemRepresentation(url, 0, name, MAXPATHLEN - 1);
            CFRelease(url);
            strcat(name, "/");
            if (url = CFBundleCopyResourcesDirectoryURL(bundle)) {
                CFURLGetFileSystemRepresentation(url, 0, name + strlen(name), 
                        MAXPATHLEN - strlen(name) - strlen(filename) - 1);
                CFRelease(url);
                strcat(name, "/");
                strcat(name, filename);
            }
        }
    
        if (name[0] == 0)
            return -2;
    }
    
    /* check first is racoon is started */
    err = is_racoon_started(name);
    if (err != 0)
        return err;            

    pid = fork();
    if (pid < 0)
        return -2;

    if (pid == 0) {

        closeall();
        
        // need to exec a tool, with complete parameters list
        if (name[0])
            execl("/usr/sbin/racoon", "racoon", "-f", name, (char *)0);
        else
            execl("/usr/sbin/racoon", "racoon", (char *)0);
            
        // child exits
        exit(0);
        /* NOTREACHED */
    }

    // parent wait for child's completion, that occurs immediatly since racoon daemonize itself
    while (waitpid(pid, &err, 0) < 0) {
        if (errno == EINTR)
            continue;
        return -2;
    }
        
    // give some time to racoon
    sleep(3);
    
    // wait for racoon pid
    tries = 5; // give 5 seconds to racoon to write its pid.
    while (!is_racoon_started(name) && tries) {
        sleep(1);
        tries--;
    }

    if (tries == 0)
        return -1;
        
    //err = (err >> 8) & 0xFF;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
racoon_pid()
{
    int   	pid = 0;
    FILE 	*f;

    f = fopen("/var/run/racoon.pid", "r");
    if (f) {
        fscanf(f, "%d", &pid);
        fclose(f);
    }
    return pid;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
stop_racoon()
{
    int   	pid = racoon_pid();

    if (pid)
        kill(pid, SIGTERM);
    return 0;
}

/* -----------------------------------------------------------------------------
check if racoon is started with the same configuration file
returns:
    0: racoon is not started
    1: racoon is already started with our configuration
    -1: racoon is already started but with a different configuration
----------------------------------------------------------------------------- */
int 
is_racoon_started(char *filename)
{
    return (racoon_pid() != 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
configure_racoon(struct sockaddr_in *src, struct sockaddr_in *dst, struct sockaddr_in *dst2, char proto, char *secret, char *secret_type) 
{
    int 	pid, f;
    char 	text[MAXPATHLEN], src_address[32], dst_address[32];
    u_int32_t	a;
    int		isclient = 1;

    a = src->sin_addr.s_addr;;
    sprintf(src_address, "%d.%d.%d.%d", a >> 24, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF);

    if (dst && (dst->sin_addr.s_addr != INADDR_ANY)) {
        isclient = 1;
        a = dst->sin_addr.s_addr;;
        sprintf(dst_address, "%d.%d.%d.%d", a >> 24, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF);
    }
    else {
        isclient = 0;
        sprintf(dst_address, "anonymous");
    }

    // make the file only readable by root, so we can stick the shared secret inside

    sprintf(text, "/etc/racoon/remote/%s.conf", dst_address);

    f = open(text, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (f == -1) 
        return -1;

    /* beginning of the REMOTE record */
    sprintf(text, "remote %s \n{\n", dst_address);
    write(f, text, strlen(text));
    
    write(f, "exchange_mode main;\n", strlen("exchange_mode main;\n"));
    write(f, "doi ipsec_doi;\n", strlen("doi ipsec_doi;\n"));
    write(f, "situation identity_only;\n", strlen("situation identity_only;\n"));
    
    // we must use address identifier for main mode
    // let racoon handle the default identifier
    // we don't want to check the peer identifier
    
    /* write the secret */
    if (secret[0]) {
        sprintf(text, "shared_secret %s \"%s\";\n", secret_type, secret);
        write(f, text, strlen(text));
    }
    
    write(f, "nonce_size 16;\n", 	strlen("nonce_size 16;\n"));
    write(f, "lifetime time 3600 sec;\n", 	strlen("lifetime time 3600 sec;\n"));
    write(f, "initial_contact on;\n", 	strlen("initial_contact on;\n"));
    write(f, "support_mip6 on;\n", 	strlen("support_mip6 on;\n"));
    if (isclient)
      // we are the client, we obey to the server
      write(f, "proposal_check obey;\n",  strlen("proposal_check obey;\n"));
    else 
      // we are the server, we impose our settings
      write(f, "proposal_check claim;\n", strlen("proposal_check claim;\n"));

    write(f, "proposal {\n", 		strlen("proposal {\n"));
    write(f, "encryption_algorithm 3des;\n", strlen("encryption_algorithm 3des;\n"));
    write(f, "hash_algorithm sha1;\n", 	strlen("hash_algorithm sha1;\n"));
    write(f, "authentication_method pre_shared_key;\n", 	strlen("authentication_method pre_shared_key;\n"));
    write(f, "dh_group 2;\n", 		strlen("dh_group 2;\n"));
    write(f, "}\n", strlen("}\n"));

    /* end of the record */
    write(f, "}\n", strlen("}\n"));


    /* beginning of the SAINFO record */
    if (isclient) {
        sprintf(text, "sainfo address %s/32 [%d] %d address %s/32 [%d] %d\n{\n", src_address, src->sin_port, proto, dst_address, dst->sin_port, proto);
        write(f, text, strlen(text));
        write(f, "lifetime time 3600 sec;\n", strlen("lifetime time 3600 sec;\n"));
        write(f, "encryption_algorithm aes, 3des;\n", strlen("encryption_algorithm aes, 3des;\n"));
        write(f, "authentication_algorithm hmac_sha1, hmac_md5;\n", strlen("authentication_algorithm hmac_sha1, hmac_md5;\n"));
        write(f, "compression_algorithm deflate;\n", strlen("compression_algorithm deflate;\n"));

        /* end of the record */
        write(f, "}\n", strlen("}\n"));
        
        if (dst2) {
            a = dst2->sin_addr.s_addr;;
            sprintf(dst_address, "%d.%d.%d.%d", a >> 24, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF);
          
            sprintf(text, "sainfo address %s/32 [%d] %d address %s/32 [%d] %d\n{\n", src_address, src->sin_port, proto, dst_address, dst2->sin_port, proto);
            write(f, text, strlen(text));
            write(f, "encryption_algorithm aes, 3des;\n", strlen("encryption_algorithm aes, 3des;\n"));
            write(f, "authentication_algorithm hmac_sha1, hmac_md5;\n", strlen("authentication_algorithm hmac_sha1, hmac_md5;\n"));
            write(f, "compression_algorithm deflate;\n", strlen("compression_algorithm deflate;\n"));
    
            /* end of the record */
            write(f, "}\n", strlen("}\n"));
        }
    }
    else {
        sprintf(text, "sainfo %s \n{\n", dst_address);
        write(f, text, strlen(text));
        write(f, "lifetime time 3600 sec;\n", strlen("lifetime time 3600 sec;\n"));
        write(f, "encryption_algorithm aes, 3des;\n", strlen("encryption_algorithm aes, 3des;\n"));
        write(f, "authentication_algorithm hmac_sha1, hmac_md5;\n", strlen("authentication_algorithm hmac_sha1, hmac_md5;\n"));
        write(f, "compression_algorithm deflate;\n", strlen("compression_algorithm deflate;\n"));
    
        /* end of the record */
        write(f, "}\n", strlen("}\n"));
    }

    close(f);

    /* signal racoon... */
    pid = racoon_pid();
    if (pid) {
        kill(pid, SIGHUP);
        sleep(1);
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
cleanup_racoon(struct sockaddr_in *src, struct sockaddr_in *dst) 
{
    int 	pid;
    char 	text[MAXPATHLEN], dst_address[32];
    u_int32_t	a;

    if (dst && (dst->sin_addr.s_addr != INADDR_ANY)) {
        a = dst->sin_addr.s_addr;;
        sprintf(dst_address, "%d.%d.%d.%d", a >> 24, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF);
    }
    else {
        sprintf(dst_address, "anonymous");
        // don't remove anymous entry ?
        return 0;
    }

    sprintf(text, "/etc/racoon/remote/%s.conf", dst_address);
    remove(text);

    /* signal racoon... */
    pid = racoon_pid();
    if (pid) {
        kill(pid, SIGHUP);
        sleep(1);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
    way is "in" our "out"
----------------------------------------------------------------------------- */
int 
require_secure_transport(struct sockaddr *src, struct sockaddr *dst, u_int8_t proto, char *way) 
{
    int 	s, err, seq = 0;
    char	policystr[64];
    caddr_t	policy;
    u_int32_t 	policylen;
    int 	prefs = (((struct sockaddr_in *)src)->sin_addr.s_addr) ? 32 : 0;
    int 	prefd = (((struct sockaddr_in *)dst)->sin_addr.s_addr) ? 32 : 0;

    s = pfkey_open();
    if (s < 0) {
        return -1;
    }
    
    sprintf(policystr, "%s ipsec esp/transport//require", way);
    policy = ipsec_set_policy(policystr, strlen(policystr));
    if (policy == 0) {
        pfkey_close(s);
        return -1;
    }

    policylen = ((struct sadb_x_policy *)policy)->sadb_x_policy_len << 3;
    
    err = pfkey_send_spdadd(s, src, prefs, dst, prefd, proto, policy, policylen, seq++);
    if (err < 0) {
        free(policy);
        pfkey_close(s);
        return -1;
    }
    
    free(policy);
    pfkey_close(s);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
remove_security_associations(struct sockaddr *src, struct sockaddr *dst) 
{
    int 	s, err;

    s = pfkey_open();
    if (s < 0)
        return -1;
        
    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, src, dst);
    if (err < 0)
        goto end;

    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, dst, src);
    if (err < 0)
        goto end;
    
end:
    pfkey_close(s);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
remove_secure_transport(struct sockaddr *src, struct sockaddr *dst, u_int8_t proto, char *way) 
{
    int 	s, err, seq = 0;
    char	policystr[64];
    caddr_t	policy = 0;
    u_int32_t 	policylen;
    int 	prefs = (((struct sockaddr_in *)src)->sin_addr.s_addr) ? 32 : 0;
    int 	prefd = (((struct sockaddr_in *)dst)->sin_addr.s_addr) ? 32 : 0;

    s = pfkey_open();
    if (s < 0)
        return -1;
    
    sprintf(policystr, "%s", way);
    policy = ipsec_set_policy(policystr, strlen(policystr));
    if (policy == 0) {
        pfkey_close(s);
        return -1;
    }

    policylen = ((struct sadb_x_policy *)policy)->sadb_x_policy_len << 3;
    
    err = pfkey_send_spddelete(s, src, prefs, dst, prefd, proto, policy, policylen, seq++);
    if (err < 0)
       goto end;

end:
    free(policy);
    pfkey_close(s);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_long)) :\
						 sizeof(u_long)))

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
    int             i;

    for (i = 0; i < RTAX_MAX; i++) {
        if (addrs & (1 << i)) {
            rti_info[i] = sa;
            NEXT_SA(sa);
            addrs ^= (1 << i);
        } else
            rti_info[i] = NULL;
    }
}


#define BUFLEN (sizeof(struct rt_msghdr) + 512)	/* 8 * sizeof(struct sockaddr_in6) = 192 */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
sockaddr_to_string(const struct sockaddr *address, char *buf, size_t bufLen)
{
    bzero(buf, bufLen);
    switch (address->sa_family) {
        case AF_INET :
            (void)inet_ntop(((struct sockaddr_in *)address)->sin_family,
                            &((struct sockaddr_in *)address)->sin_addr,
                            buf,
                            bufLen);
            break;
        case AF_INET6 : {
            (void)inet_ntop(((struct sockaddr_in6 *)address)->sin6_family,
                            &((struct sockaddr_in6 *)address)->sin6_addr,
                            buf,
                            bufLen);
            if (((struct sockaddr_in6 *)address)->sin6_scope_id) {
                int	n;

                n = strlen(buf);
                if ((n+IF_NAMESIZE+1) <= bufLen) {
                    buf[n++] = '%';
                    if_indextoname(((struct sockaddr_in6 *)address)->sin6_scope_id, &buf[n]);
                }
            }
            break;
        }
        case AF_LINK :
            if (((struct sockaddr_dl *)address)->sdl_len < bufLen) {
                bufLen = ((struct sockaddr_dl *)address)->sdl_len;
            } else {
                bufLen = bufLen - 1;
            }

            bcopy(((struct sockaddr_dl *)address)->sdl_data, buf, bufLen);
            break;
        default :
            snprintf(buf, bufLen, "unexpected address family %d", address->sa_family);
            break;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
get_src_address(struct sockaddr *src, const struct sockaddr *dst)
{
    char		buf[BUFLEN];
    struct rt_msghdr	*rtm;
    pid_t		pid = getpid();
    int			rsock = -1, seq = 0, n;
    struct sockaddr	*rti_info[RTAX_MAX], *sa;
    struct sockaddr_dl	*sdl;

    rsock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (rsock == -1)
        return -1;

    bzero(&buf, sizeof(buf));

    rtm = (struct rt_msghdr *)&buf;
    rtm->rtm_msglen  = sizeof(struct rt_msghdr);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type    = RTM_GET;
    rtm->rtm_flags   = RTF_STATIC|RTF_UP|RTF_HOST|RTF_GATEWAY;
    rtm->rtm_addrs   = RTA_DST|RTA_IFP; /* Both destination and device */
    rtm->rtm_pid     = pid;
    rtm->rtm_seq     = ++seq;

    sa = (struct sockaddr *) (rtm + 1);
    bcopy(dst, sa, dst->sa_len);
    rtm->rtm_msglen += sa->sa_len;

    sdl = (struct sockaddr_dl *) ((void *)sa + sa->sa_len);
    sdl->sdl_family = AF_LINK;
    sdl->sdl_len = sizeof (struct sockaddr_dl);
    rtm->rtm_msglen += sdl->sdl_len;

    do {
        n = write(rsock, &buf, rtm->rtm_msglen);
        if (n == -1 && errno != EINTR) {
            close(rsock);
            return -1;
        }
    } while (n == -1); 

    /* Type, seq, pid identify our response.
        Routing sockets are broadcasters on input. */
    do {
        do {
            n = read(rsock, (void *)&buf, sizeof(buf));
            if (n == -1 && errno != EINTR) {
                close(rsock);
                return -1;
            }
        } while (n == -1); 
    } while (rtm->rtm_type != RTM_GET 
            || rtm->rtm_seq != seq
            || rtm->rtm_pid != pid);

    get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

#if 0
{ /* DEBUG */
    int 	i;
    char	buf[200];

    //SCLog(_sc_debug, LOG_DEBUG, CFSTR("rtm_flags = 0x%8.8x"), rtm->rtm_flags);

    for (i=0; i<RTAX_MAX; i++) {
        if (rti_info[i] != NULL) {
                sockaddr_to_string(rti_info[i], buf, sizeof(buf));
                printf("%d: %s\n", i, buf);
        }
    }
} /* DEBUG */
#endif
    
    bcopy(rti_info[5], src, rti_info[5]->sa_len);
    close(rsock);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
get_my_address(struct sockaddr *src)
{
    struct sockaddr_in *s = (struct sockaddr_in*)src;
    
    s->sin_len = sizeof(*s);
    s->sin_family = AF_INET;
    s->sin_port = 0;
    s->sin_addr.s_addr = INADDR_ANY;
    return 0;
}

