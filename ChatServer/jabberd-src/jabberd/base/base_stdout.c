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
#include "jabberd.h"

/* for cleanup signalling */
pth_t main__thread = NULL;

/*

<stdout/>

with this flag in any instance of any type, it causes all packets to be delivered to standard out (STDOUT) from the jabberd process
it also flags a thread to read on STDIN for incoming packets

*/

/* simple wrapper around the pth messages to pass packets */
typedef struct
{
    pth_message_t head; /* the standard pth message header */
    dpacket p;
} *drop, _drop;

result base_stdout_heartbeat(void *arg)
{
    static int parent = 0;

    if(parent == 0) parent = getppid();

    if(parent != getppid())
    {
        /* parent pid has changed, bail */
        log_alert("stdout","Parent PID has changed, Server Exiting");
        exit(1);
    }

    return r_DONE;
}

/* write packets to sink */
result base_stdout_phandler(instance i, dpacket p, void *arg)
{
    pth_msgport_t mp = (pth_msgport_t)arg;
    drop d;

    log_debug(ZONE,"stdout packet being queued");

    d = pmalloco(p->p, sizeof(_drop));
    d->p = p;

    pth_msgport_put(mp, (pth_message_t *)d);

    return r_DONE;
}

void base_stdin_packets(int type, xmlnode x, void *arg)
{
    switch(type)
    {
    case XSTREAM_ROOT:
        log_debug(ZONE,"stdin opened stream");
        xmlnode_free(x);
        break;
    case XSTREAM_NODE:
        /* deliver the packets coming on stdin... they aren't associated with an instance really */
        log_debug(ZONE,"stdin incoming packet");
        deliver(dpacket_new(x), NULL); 
        break;
    default:
        xmlnode_free(x);
    }

}

/* thread to handle io from socket */
void *base_stdoutin(void *arg)
{
    pth_msgport_t mp = (pth_msgport_t)arg;
    xstream xs;
    int len;
    char buff[1024], *block;
    dpacket p = NULL;
    drop d;
    xmlnode x;
    pth_event_t eread, emp, ering;
    pool xsp;
    /* for cleanup */
    int sig;
    sigset_t sigs;
    pth_event_t esig;

    /* init the signal junk */
    sigemptyset(&sigs);
    sigaddset(&sigs,SIGUSR2);

    log_debug(ZONE,"io thread starting");

    /* send the header to stdout */
    x = xstream_header("jabber:component:exec",NULL,NULL);
    block = xstream_header_char(x);
    MIO_WRITE_FUNC(STDOUT_FILENO,block,strlen(block));
    xmlnode_free(x);

    /* start xstream and event for reading packets from stdin */
    xsp = pool_new();
    xs = xstream_new(xsp, base_stdin_packets, NULL);
    eread = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE,STDIN_FILENO);

    /* event for packets going to stdout and ring em all together */
    emp = pth_event(PTH_EVENT_MSG,mp);
    esig = pth_event(PTH_EVENT_SIGS,&sigs,&sig);
    ering = pth_event_concat(esig,eread, emp, NULL);

    /* spin waiting on the mp(stdout) or read(stdin) events */
    while(pth_wait(ering) > 0)
    {

        /* we were notified to shutdown */
        if(pth_event_occurred(esig))
        {
            break;
        }

        /* handle reading the incoming stream */
        if(pth_event_occurred(eread))
        {
            log_debug(ZONE,"stdin read event");
            len = MIO_READ_FUNC(STDIN_FILENO, buff, 1024);
            if(len <= 0) break;

            if(xstream_eat(xs, buff, len) > XSTREAM_NODE) break;
        }

        /* handle the packets to be sent to the socket */
        if(pth_event_occurred(emp))
        {
            log_debug(ZONE,"io incoming message event for stdout");

            /* get packet */
            d = (drop)pth_msgport_get(mp);
            p = d->p;

            /* write packet phase */
            block = xmlnode2str(p->x);
            if(MIO_WRITE_FUNC(STDOUT_FILENO, block, strlen(block)) <= 0)
                break;

            /* all sent, yay */
            pool_free(p->p);
            p = NULL;
        }

    }

    log_debug(ZONE,"thread exiting");

    /* we shouldn't ever get here, I don't think */
    pth_event_free(ering, PTH_FREE_ALL);
    pth_msgport_destroy(mp);
    pool_free(xsp);

    return NULL;
}

void base_stdout_shutdown(void *arg)
{
    drop d;
    pth_msgport_t mp=(pth_msgport_t)arg;
    while((d = (drop)pth_msgport_get(mp)) != NULL)
    {
        pool_free(d->p->p);
    }
    if(main__thread!=NULL) pth_raise(main__thread,SIGUSR2);
}

result base_stdout_config(instance id, xmlnode x, void *arg)
{
    static pth_msgport_t mp = NULL;

    if(id == NULL) 
    {
        register_beat(2,base_stdout_heartbeat,NULL);
        return r_PASS;
    }

    log_debug(ZONE,"base_stdout_config performing configuration");

    /* create the mp and start the io thread only once */
    if(mp == NULL)
    {
        mp = pth_msgport_create("base_stdout");
        main__thread = pth_spawn(PTH_ATTR_DEFAULT, base_stdoutin, (void *)mp);
        pool_cleanup(id->p, base_stdout_shutdown, (void*)mp);
    }

    /* register phandler with the stdout mp */
    register_phandler(id, o_DELIVER, base_stdout_phandler, (void *)mp);

    return r_DONE;
}

void base_stdout(void)
{
    log_debug(ZONE,"base_stdout loading...\n");
    register_config("stdout",base_stdout_config,NULL);
}
