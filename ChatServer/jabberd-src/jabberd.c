/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "JOSL").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the JOSL. You
 * may obtain a copy of the JOSL at http://www.jabber.org/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the JOSL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the JOSL
 * for the specific language governing rights and limitations under the
 * JOSL.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2002 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * Portions (c) Copyright 2005 Apple Computer, Inc.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the JOSL,
 * indicate your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.  If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the JOSL or the GPL. 
 * 
 * 
 * --------------------------------------------------------------------------*/
 
/*
 * Doesn't he wish!
 *
 * <ChatBot> jer: Do you sometimes wish you were written in perl?
 *
 */


// This overrides the system definition of FD_SETSIZE
#include "jabberd.h"

#include <pwd.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "single.h"
#include "revision.h"

HASHTABLE cmd__line, debug__zones;
extern HASHTABLE instance__ids;
extern int deliver__flag;
extern xmlnode greymatter__;
pool jabberd__runtime = NULL;
static char *cfgfile = NULL;
int jabberd__signalflag = 0;

/*** internal functions ***/
int configurate(char *file);
void static_init(void);
void dynamic_init(void);
void deliver_init(void);
void heartbeat_birth(void);
void heartbeat_death(void);
int configo(int exec);
void shutdown_callbacks(void);
int config_reload(char *file);
int  instance_startup(xmlnode x, int exec);
void instance_shutdown(instance i);
void _jabberd_signal(int sig);
void RemovePIDfile(void);
void Exit(int status);

void MaxOpenFiles()
{

    //grow our pool of file descriptors to the max!
    struct rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    

    getrlimit(RLIMIT_NOFILE,  &rl);
    log_debug(ZONE,"current open file limit: %lu", (long unsigned) rl.rlim_cur);
    log_debug(ZONE,"max open file limit: %lu", (long unsigned) rl.rlim_max);

    // set it to the maximum we have defined in jabber.h, FD_SETSIZE
    // This avoids the buffer overflow problem defined in bugtraq 12346; see
    // http://www.securityfocus.com/bid/12346/info/
    rl.rlim_cur = FD_SETSIZE;
    rl.rlim_max = FD_SETSIZE;
    setrlimit (RLIMIT_NOFILE, &rl);
    
    getrlimit(RLIMIT_NOFILE,  &rl);
    log_debug(ZONE,"new open file limit: %lu", (long unsigned) rl.rlim_cur);
    log_debug(ZONE,"new open file max:   %lu", (long unsigned) rl.rlim_max);

}

void Exit(int status)
{
	RemovePIDfile();
	exit(status);
}

int main (int argc, char** argv)
{
    int help, i;           /* temporary variables */
    char *c, *cmd, *home = NULL;   /* strings used to load the server config */
    pool cfg_pool=pool_new();
    float avload;
    int do_debug = 0;           /* Debug output option, default no */
    int do_background = 0;      /* Daemonize option, default no */
    
    jabberd__runtime = pool_new();

    /* start by assuming the parameters were entered correctly */
    help = 0;
    cmd__line = ghash_create_pool(jabberd__runtime, 11,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);

    /* process the parameterss one at a time */
    for(i = 1; i < argc; i++)
    {
        if(argv[i][0]!='-')
        { /* make sure it's a valid command */
            help=1;
            break;
        }
        for(c=argv[i]+1;c[0]!='\0';c++)
        {
            /* loop through the characters, like -Dc */
            if(*c == 'D')
            {
                do_debug = 1;
                continue;
            }
            else if(*c == 'V' || *c == 'v')
            {
                printf("Jabberd Version %s Build %s\n", VERSION, BUILD);
                exit(0);
            }
            else if(*c == 'B')
            {
                if (do_debug)
                {
                    printf("Debug output is enabled, can not background.\n");
                }
                else
                {
                    do_background = 1;
                }
                continue;
            }

            cmd = pmalloco(cfg_pool,2);
            cmd[0]=*c;
            if(i+1<argc)
            {
               ghash_put(cmd__line,cmd,argv[++i]);
            }else{
                help=1;
                break;
            }
        }
    }

    /* the special -Z flag provides a list of zones to filter debug output for, flagged w/ a simple hash */
    if((cmd = ghash_get(cmd__line,"Z")) != NULL)
    {
        set_debug_flag(1);
        debug__zones = ghash_create_pool(jabberd__runtime, 11,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);
        while(cmd != NULL)
        {
            c = strchr(cmd,',');
            if(c != NULL)
            {
                *c = '\0';
                c++;
            }
            ghash_put(debug__zones,cmd,cmd);
            cmd = c;
        }
    }else{
        debug__zones = NULL;
    }

    /* were there any bad parameters? */
    if(help)
    {
        fprintf(stderr,"Usage:\n%s [params]\n Optional Parameters:\n -c\t\tconfiguration file\n -D\t\tenable debug output (disables background)\n -H\t\tlocation of home folder\n -B\t\tbackground the server process\n -Z\t\tdebug zones\n -v\t\tserver version\n -V\t\tserver version\n", argv[0]);
        exit(0);
    }

    /* set to debug mode if we have it */
    set_debug_flag(do_debug);
    

    /* set maximum open files */
    MaxOpenFiles();

#ifdef SINGLE
    SINGLE_STARTUP
#else
    if((home = ghash_get(cmd__line,"H")) == NULL)
        home = pstrdup(jabberd__runtime,HOME);
#endif
	cmd = ghash_get(cmd__line, "U");
	if (cmd == NULL)
    	cmd = pstrdup(jabberd__runtime,"jabber");

    /* Switch to the specified user */
    if (cmd != NULL)
    {
        struct passwd* user = NULL;

        user = getpwnam(cmd);
        if (user == NULL)
        {
            fprintf(stderr, "Unable to lookup user %s.\n", cmd);
            exit(1);
        }
        
		if (initgroups(cmd, user->pw_gid) < 0)
        {
            fprintf(stderr, "Unable to initialize group memberships.\n");
            exit(1);
        }
        if (setgid(user->pw_gid) < 0)
        {
            fprintf(stderr, "Unable to set group permissions.\n");
            exit(1);
        }
        if (setuid(user->pw_uid) < 0)
        {
            fprintf(stderr, "Unable to set user permissions.\n");
            exit(1);
        }
    }

    /* change the current working directory so everything is "local" */
    if(home != NULL && chdir(home))
        fprintf(stderr,"Unable to access home folder %s: %s\n",home,strerror(errno));

    /* background ourselves if we have been flagged to do so */
    if(do_background != 0)
    {
        if (fork() != 0)
        {
            exit(0);
        }
    }

    /* load the config passing the file if it was manually set */
    if((cfgfile = ghash_get(cmd__line,"c")) == NULL)
        cfgfile = pstrdup(jabberd__runtime,CONFIGXML);

    if(configurate(cfgfile))
        Exit(1);

    /* EPIPE is easier to handle than a signal */
    signal(SIGPIPE, SIG_IGN);

    /* handle signals */
    signal(SIGHUP,_jabberd_signal);
    signal(SIGINT,_jabberd_signal);
    signal(SIGTERM,_jabberd_signal);

    openlog("iChatServer-jabberd", LOG_PID, LOG_DAEMON);

    /* init pth */
    pth_init();

    /* fire em up baby! */
    heartbeat_birth();

    /* init MIO */
    mio_init();

    static_init();
    dynamic_init();
    deliver_init();

    /* everything should be registered for the config pass, validate */
    deliver__flag = 0; /* pause deliver() while starting up */
    if(configo(0))
        Exit(1);

    log_notice(NULL,"initializing server");

    /* karma granted, rock on */
    if(configo(1))
        Exit(1);
    
    log_notice(NULL,"server started");

    /* begin delivery of queued msgs */
    deliver__flag=1;
    deliver(NULL,NULL);

    while(1)
    {
        pth_ctrl(PTH_CTRL_GETAVLOAD, &avload);
        log_debug(ZONE,"main load check of %.2f with %ld total threads", avload, pth_ctrl(PTH_CTRL_GETTHREADS));
        pth_sleep(60);
    };

    /* we never get here */
    return 0;
}

void _jabberd_signal(int sig)
{
    log_debug(ZONE,"received signal %d",sig);
    jabberd__signalflag = sig;
}

void _jabberd_restart(void)
{
    xmlnode temp_greymatter;

    log_notice(NULL, "reloading configuration");

    /* keep greymatter around till we are sure the reload is OK */
    temp_greymatter = greymatter__;

    log_debug(ZONE, "Loading new config file");

    /* try to load the config file */
    if(configurate(cfgfile))
    { /* failed to load.. restore the greymatter */
        log_debug(ZONE, "Failed to load new config, resetting greymatter");
        log_alert(ZONE, "Failed to reload config!  Resetting internal config -- please check your configuration!");
        greymatter__ = temp_greymatter;
        return;
    }

    /* free old greymatter (NOTE: shea, right! many internal tables/callbacs/etc have old config pointers :)
    if(temp_greymatter != NULL)
        xmlnode_free(temp_greymatter); */

    /* XXX do more smarts on new config */

    log_debug(ZONE, "reload process complete");

}

void RemovePIDfile(void)
{
    xmlnode pidfile;
    char *pidpath;
    
    pidfile = xmlnode_get_tag(greymatter__, "pidfile");
    if(pidfile != NULL)
    {
        pidpath = xmlnode_get_data(pidfile);
        if(pidpath != NULL)
            unlink(pidpath);
    }

}

void _jabberd_shutdown(void)
{

    log_notice(NULL,"shutting down server");

    /* pause deliver() this sucks, cuase we lose shutdown messages */
    deliver__flag = 0;
    shutdown_callbacks();

    /* one last chance for threads to finish shutting down */
    pth_sleep(1);

    /* stop MIO and heartbeats */
    mio_stop();
    heartbeat_death();

    /* kill any leftover threads */
    pth_kill();

    /* Get rid of our pid file */
    RemovePIDfile();
    
    xmlnode_free(greymatter__);

    /* base modules use jabberd__runtime to know when to shutdown */
    pool_free(jabberd__runtime);
    
    closelog();

    exit(0);
}

/* process the signal */
void jabberd_signal(void)
{
    if(jabberd__signalflag == SIGHUP)
    {
        _jabberd_restart();
        jabberd__signalflag = 0;
        return;
    }
    _jabberd_shutdown();
}
