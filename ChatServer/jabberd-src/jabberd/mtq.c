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

/*** mtq is Managed Thread Queues ***/
/* they queue calls to be run sequentially on a thread, that comes from a system pool of threads */

typedef struct mtqcall_struct
{
    pth_message_t head; /* the standard pth message header */
    mtq_callback f; /* function to run within the thread */
    void *arg; /* the data for this call */
    mtq q; /* if this is a queue to process */
} _mtqcall, *mtqcall;

typedef struct mtqmaster_struct
{
    mth all[MTQ_THREADS];
    int overflow;
    pth_msgport_t mp;
} *mtqmaster, _mtqmaster;

mtqmaster mtq__master = NULL;

/* cleanup a queue when it get's free'd */
void mtq_cleanup(void *arg)
{
    mtq q = (mtq)arg;
    mtqcall c;

    /* if there's a thread using us, make sure we disassociate ourselves with them */
    if(q->t != NULL)
        q->t->q = NULL;

    /* What?  not empty?!?!?! probably a programming/sequencing error! */
    while((c = (mtqcall)pth_msgport_get(q->mp)) != NULL)
    {
        log_debug("mtq","%X last call %X",q->mp,c->arg);
        (*(c->f))(c->arg);
    }
    pth_msgport_destroy(q->mp);
}

/* public queue creation function, queue lives as long as the pool */
mtq mtq_new(pool p)
{
    mtq q;

    if(p == NULL) return NULL;

    log_debug(ZONE,"MTQ(new)");

    /* create queue */
    q = pmalloco(p, sizeof(_mtq));

    /* create msgport */
    q->mp = pth_msgport_create("mtq");

    /* register cleanup handler */
    pool_cleanup(p, mtq_cleanup, (void *)q);

    return q;
}

/* main slave thread */
void *mtq_main(void *arg)
{
    mth t = (mth)arg;
    pth_event_t mpevt;
    mtqcall c;

    log_debug("mtq","%X starting",t->id);

    /* create an event ring for receiving messges */
    mpevt = pth_event(PTH_EVENT_MSG,t->mp);

    /* loop */
    while(1)
    {

        /* before checking our mp, see if the master one has overflow traffic in it */
        if(mtq__master->overflow)
        {
            /* get the call from the master */
            c = (mtqcall)pth_msgport_get(mtq__master->mp);
            if(c == NULL)
            { /* empty! */
                mtq__master->overflow = 0;
                continue;
            }
        }else{
            /* debug: note that we're waiting for a message */
            log_debug("mtq","%X leaving to pth",t->id);
            t->busy = 0;

            /* wait for a master message on the port */
            pth_wait(mpevt);

            /* debug: note that we're working */
            log_debug("mtq","%X entering from pth",t->id);
            t->busy = 1;

            /* get the message */
            c = (mtqcall)pth_msgport_get(t->mp);
            if(c == NULL) continue;
        }


        /* check for a simple "one-off" call */
        if(c->q == NULL)
        {
            log_debug("mtq","%X one call %X",t->id,c->arg);
            (*(c->f))(c->arg);
            continue;
        }

        /* we've got a queue call, associate ourselves and process all it's packets */
        t->q = c->q;
        t->q->t = t;
        while((c = (mtqcall)pth_msgport_get(t->q->mp)) != NULL)
        {
            log_debug("mtq","%X queue call %X",t->id,c->arg);
            (*(c->f))(c->arg);
            if(t->q == NULL) break;
        }

        /* disassociate the thread and queue since we processed all the packets */
        /* XXX future pthreads note: mtq_send() could have put another call on the queue since we exited the while, that would be bad */
        if(t->q != NULL)
        {
            t->q->t = NULL; /* make sure the queue doesn't point to us anymore */
            t->q->routed = 0; /* nobody is working on the queue anymore */
            t->q = NULL; /* we're not working on the queue */
        }

    }

    /* free all memory stuff associated with the thread */
    pth_event_free(mpevt,PTH_FREE_ALL);
    pth_msgport_destroy(t->mp);
    pool_free(t->p);
    return NULL;
}

void mtq_send(mtq q, pool p, mtq_callback f, void *arg)
{
    mtqcall c;
    mth t = NULL;
    int n; pool newp;
    pth_msgport_t mp = NULL; /* who to send the call too */
    pth_attr_t attr;

    /* initialization stuff */
    if(mtq__master == NULL)
    {
        mtq__master = malloc(sizeof(_mtqmaster)); /* happens once, global */
        mtq__master->mp = pth_msgport_create("mtq__master");
        for(n=0;n<MTQ_THREADS;n++)
        {
            newp = pool_new();
            t = pmalloco(newp, sizeof(_mth));
            t->p = newp;
            t->mp = pth_msgport_create("mth");
            attr = pth_attr_new();
            pth_attr_set(attr, PTH_ATTR_PRIO, PTH_PRIO_MAX);
            t->id = pth_spawn(attr, mtq_main, (void *)t);
            pth_attr_destroy(attr);
            mtq__master->all[n] = t; /* assign it as available */
        }
    }

    /* find a waiting thread */
    for(n = 0; n < MTQ_THREADS; n++)
        if(mtq__master->all[n]->busy == 0)
        {
            mp = mtq__master->all[n]->mp;
            break;
        }

    /* if there's no thread available, dump in the overflow msgport */
    if(mp == NULL)
    {
        log_debug("mtqoverflow","%d overflowing %X",mtq__master->overflow,arg);
        mp = mtq__master->mp;
        /* XXX this is a race condition in pthreads.. if the overflow
         * is not put on the mp, before a worker thread checks the mp
         * for messages, then it will set this variable to 0 and not
         * check the overflow mp until another item overflows it */
        mtq__master->overflow++;
    }

    /* track this call */
    c = pmalloco(p, sizeof(_mtqcall));
    c->f = f;
    c->arg = arg;

    /* if we don't have a queue, just send it */
    if(q == NULL)
    {
        pth_msgport_put(mp, (pth_message_t *)c);
        /* if we use a thread, mark it busy */
        if(mp != mtq__master->mp)
            mtq__master->all[n]->busy = 1;
        return;
    }

    /* if we have a queue, insert it there */
    pth_msgport_put(q->mp, (pth_message_t *)c);

    /*if(pth_msgport_pending(q->mp) > 10)
        log_debug("mtqoverflow","%d queue overflow on %X",pth_msgport_pending(q->mp),q->mp);*/

    /* if we haven't told anyone to take this queue yet */
    if(q->routed == 0)
    {
        c = pmalloco(p, sizeof(_mtqcall));
        c->q = q;
        pth_msgport_put(mp, (pth_message_t *)c);
        /* if we use a thread, mark it busy */
        if(mp != mtq__master->mp)
            mtq__master->all[n]->busy = 1;
        q->routed = 1;
    }
}

