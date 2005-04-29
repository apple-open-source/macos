/*
 *  ipfwloggerd.c
 *  FirewallTool
 *
 *  Created by Mary Chan on Thu Jun 03 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/kern_event.h>
#include <net/if.h>
#include <net/dlil.h>

#define		IPFWLOGEVENT	0
#define		KEV_LOG_SUBCLASS 10
#define		LOGGERPIDPATH   "/var/run/ipfwlogger.pid"
#define		IPFWLOGPATH		"/var/log/ipfw.log"

int main (int argc, char * argv[]) {
	int						ch;
	pid_t					childpid;
	int						so, status;
    struct kev_request  	kev_req;
    char					buf[1000];
    struct kern_event_msg 	*ev_msg;
    char	*netdata;
	unsigned char			priv;
    FILE *fp;
	struct stat sb;
	int		fs;
	

	signal(SIGHUP, SIG_IGN);
	while ((ch = getopt(argc, argv, "v:")) != -1)
		switch (ch) {
		default:
			break;
		}
		
        switch (childpid = fork()) {
			case -1:
					return (-1);
			case 0:
					break;
			default:
					exit(1);
        }
		
		/* write PID to file */
        fp = fopen(LOGGERPIDPATH, "w");
        if (fp != NULL) {
                fprintf(fp, "%d\n", getpid());
                fclose(fp);
        }

        so = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
        if (so != -1) {
                /* establish filter to return all events */
                kev_req.vendor_code  = KEV_VENDOR_APPLE;
                kev_req.kev_class    = KEV_NETWORK_CLASS;       /* Not used if vendor_code is 0 */
                kev_req.kev_subclass = KEV_LOG_SUBCLASS;       /* Not used if either kev_class OR vendor_code are 0 */
                status = ioctl(so, SIOCSKEVFILT, &kev_req);
                if (status) {
                        printf("could not establish event filter, ioctl() failed: %d", errno);
                        (void) close(so);
                        exit(1);
                }
        } else {                
                printf("cannot open socket\n");
				exit(1);
	} 

	openlog( "ipfw", 0, LOG_LOCAL0);
	/* is there a ipfw.log in /var/log/? */
	/* create one if not */
	if ( stat(IPFWLOGPATH, &sb))
	{
		printf("cannot find /var/log/ipfw/log\n");
		if (errno == ENOENT){
			fs = (int)creat(IPFWLOGPATH, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			if (fs != -1) {
				close(fs);
			}
			else printf("create errorno = %d\n", errno);
			printf("after creating /var/log/ipfw.log fs = %d\n", fs);
		}
	}
	for ( ;;){
		status = recv(so, &buf, sizeof(buf), 0);
        if (status == -1) {
            printf("recv fails errno = %d\n", errno);
			exit(1);
        }
		ev_msg = (struct kern_event_msg *) &buf;
		netdata = (char*)&ev_msg->event_data[0];
		if (ev_msg->event_code == IPFWLOGEVENT) {
			/* first byte of data is priv */
			priv = (unsigned char)*netdata++;
			syslog( LOG_LOCAL0 | priv,(char*)netdata);
		}
		
	}

}

