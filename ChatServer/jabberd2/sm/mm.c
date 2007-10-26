/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "sm.h"

/** @file sm/mm.c
  * @brief module manager
  * @author Robert Norris
  * $Date: 2006/03/14 23:27:27 $
  * $Revision: 1.1 $
  */

/* these functions implement a multiplexor to get calls to the correct module
 * for the given type */


/* if you add a module, you'll need to update these arrays */

extern int active_init(mod_instance_t);
extern int announce_init(mod_instance_t);
extern int deliver_init(mod_instance_t);
extern int disco_init(mod_instance_t);
extern int disco_publish_init(mod_instance_t);
extern int echo_init(mod_instance_t);
extern int help_init(mod_instance_t);
extern int offline_init(mod_instance_t);
extern int presence_init(mod_instance_t);
extern int privacy_init(mod_instance_t);
extern int roster_init(mod_instance_t);
extern int session_init(mod_instance_t);
extern int vacation_init(mod_instance_t);
extern int validate_init(mod_instance_t);
extern int iq_last_init(mod_instance_t);
extern int iq_private_init(mod_instance_t);
extern int iq_time_init(mod_instance_t);
extern int iq_vcard_init(mod_instance_t);
extern int iq_version_init(mod_instance_t);
extern int template_roster_init(mod_instance_t);

char *module_names[] = {
    "active",
    "announce",
    "deliver",
    "disco",
    "disco-publish",
    "echo",
    "help",
    "offline",
    "presence",
    "privacy",
    "roster",
    "session",
    "vacation",
    "validate",
    "iq-last",
    "iq-private",
    "iq-time",
    "iq-vcard",
    "iq-version",
    "template-roster",
    NULL
};

module_init_fn module_inits[] = {
    active_init,
    announce_init,
    deliver_init,
    disco_init,
    disco_publish_init,
    echo_init,
    help_init,
    offline_init,
    presence_init,
    privacy_init,
    roster_init,
    session_init,
    vacation_init,
    validate_init,
    iq_last_init,
    iq_private_init,
    iq_time_init,
    iq_vcard_init,
    iq_version_init,
    template_roster_init,
    NULL
};

mm_t mm_new(sm_t sm) {
    mm_t mm;
    int celem, melem, attr, i, *nlist = NULL;
    char id[13], name[32], arg[1024];
    mod_chain_t chain = (mod_chain_t) NULL;
    mod_instance_t **list = NULL, mi;
    module_t mod;

    mm = (mm_t) malloc(sizeof(struct mm_st));
    memset(mm, 0, sizeof(struct mm_st));

    mm->sm = sm;
    mm->modules = xhash_new(101);

    if((celem = nad_find_elem(sm->config->nad, 0, -1, "modules", 1)) < 0)
        return mm;

    celem = nad_find_elem(sm->config->nad, celem, -1, "chain", 1);
    while(celem >= 0) {
        if((attr = nad_find_attr(sm->config->nad, celem, -1, "id", NULL)) < 0) {
            celem = nad_find_elem(sm->config->nad, celem, -1, "chain", 0);
            continue;
        }

        snprintf(id, 13, "%.*s", NAD_AVAL_L(sm->config->nad, attr), NAD_AVAL(sm->config->nad, attr));
        id[12] = '\0';

        log_debug(ZONE, "processing config for chain '%s'", id);

        list = NULL;
        if(strcmp(id, "sess-start") == 0) {
            chain = chain_SESS_START;
            list = &mm->sess_start;
            nlist = &mm->nsess_start;
        }
        else if(strcmp(id, "sess-end") == 0) {
            chain = chain_SESS_END;
            list = &mm->sess_end;
            nlist = &mm->nsess_end;
        }
        else if(strcmp(id, "in-sess") == 0) {
            chain = chain_IN_SESS;
            list = &mm->in_sess;
            nlist = &mm->nin_sess;
        }
        else if(strcmp(id, "in-router") == 0) {
            chain = chain_IN_ROUTER;
            list = &mm->in_router;
            nlist = &mm->nin_router;
        }
        else if(strcmp(id, "out-sess") == 0) {
            chain = chain_OUT_SESS;
            list = &mm->out_sess;
            nlist = &mm->nout_sess;
        }
        else if(strcmp(id, "out-router") == 0) {
            chain = chain_OUT_ROUTER;
            list = &mm->out_router;
            nlist = &mm->nout_router;
        }
        else if(strcmp(id, "pkt-sm") == 0) {
            chain = chain_PKT_SM;
            list = &mm->pkt_sm;
            nlist = &mm->npkt_sm;
        }
        else if(strcmp(id, "pkt-user") == 0) {
            chain = chain_PKT_USER;
            list = &mm->pkt_user;
            nlist = &mm->npkt_user;
        }
        else if(strcmp(id, "pkt-router") == 0) {
            chain = chain_PKT_ROUTER;
            list = &mm->pkt_router;
            nlist = &mm->npkt_router;
        }
        else if(strcmp(id, "user-load") == 0) {
            chain = chain_USER_LOAD;
            list = &mm->user_load;
            nlist = &mm->nuser_load;
        }
        else if(strcmp(id, "user-create") == 0) {
            chain = chain_USER_CREATE;
            list = &mm->user_create;
            nlist = &mm->nuser_create;
        }
        else if(strcmp(id, "user-delete") == 0) {
            chain = chain_USER_DELETE;
            list = &mm->user_delete;
            nlist = &mm->nuser_delete;
        }

        if(list == NULL) {
            log_write(sm->log, LOG_ERR, "unknwon chain type '%s'", id);

            celem = nad_find_elem(sm->config->nad, celem, -1, "chain", 0);
            continue;
        }

        melem = nad_find_elem(sm->config->nad, celem, -1, "module", 1);
        while(melem >= 0) {
            if(NAD_CDATA_L(sm->config->nad, melem) <= 0) {
                melem = nad_find_elem(sm->config->nad, melem, -1, "module", 0);
                continue;
            }

            snprintf(name, 32, "%.*s", NAD_CDATA_L(sm->config->nad, melem), NAD_CDATA(sm->config->nad, melem));

            for(i = 0; module_names[i] != NULL; i++) {
                if(strcmp(name, module_names[i]) == 0) {
                    log_debug(ZONE, "adding module '%s' to chain '%s'", name, id);

                    arg[0] = '\0';
                    attr = nad_find_attr(sm->config->nad, melem, -1, "arg", NULL);
                    if(attr >= 0) {
                        snprintf(arg, 1024, "%.*s", NAD_AVAL_L(sm->config->nad, attr), NAD_AVAL(sm->config->nad, attr));
                        log_debug(ZONE, "module arg: %s", arg);
                    }

                    mod = xhash_get(mm->modules, name);
                    if(mod == NULL) {
                        mod = (module_t) malloc(sizeof(struct module_st));
                        memset(mod, 0, sizeof(struct module_st));

                        mod->mm = mm;
                        mod->index = mm->nindex;
                        mod->name = strdup(name);

                        xhash_put(mm->modules, mod->name, (void *) mod);

                        mm->nindex++;
                    }

                    mi = (mod_instance_t) malloc(sizeof(struct mod_instance_st));
                    memset(mi, 0, sizeof(struct mod_instance_st));

                    mi->sm = sm;
                    mi->mod = mod;
                    mi->chain = chain;
                    mi->arg = (arg[0] == '\0') ? NULL : strdup(arg);
                    mi->seq = mod->init;

                    if((module_inits[i])(mi) != 0) {
                        log_debug(ZONE, "init for module '%s' (seq %d) failed", name, mi->seq);

                        free(mi);

                        if(mod->init == 0) {
                            xhash_zap(mm->modules, mod->name);
                            free(mod->name);
                            free(mod);

                            mm->nindex--;

                            melem = nad_find_elem(sm->config->nad, melem, -1, "module", 0);
                            continue;
                        }
                    }

                    mod->init++;

                    *list = (mod_instance_t *) realloc(*list, sizeof(mod_instance_t) * (*nlist + 1));
                    (*list)[*nlist] = mi;

                    log_debug(ZONE, "module '%s' added to chain '%s' (order %d index %d seq %d)", mod->name, id, *nlist, mod->index, mi->seq);

                    (*nlist)++;
                }
            }

            melem = nad_find_elem(sm->config->nad, melem, -1, "module", 0);
        }

        celem = nad_find_elem(sm->config->nad, celem, -1, "chain", 0);
    }

    return mm;
}

static void _mm_reaper(xht modules, const char *module, void *val, void *arg) {
    module_t mod = (module_t) val;

    if(mod->free != NULL)
        (mod->free)(mod);

    free(mod->name);
    free(mod);
}

void mm_free(mm_t mm) {
    int i, j, *nlist = NULL;
    mod_instance_t **list = NULL, mi;

    /* close down modules */
    xhash_walk(mm->modules, _mm_reaper, NULL);

    /* free instances */
    for(i = 0; i < 12; i++) {
        switch(i) {
            case 0:
                list = &mm->sess_start;
                nlist = &mm->nsess_start;
                break;
            case 1:
                list = &mm->sess_end;
                nlist = &mm->nsess_end;
                break;
            case 2:
                list = &mm->in_sess;
                nlist = &mm->nin_sess;
                break;
            case 3:
                list = &mm->in_router;
                nlist = &mm->nin_router;
                break;
            case 4:
                list = &mm->out_sess;
                nlist = &mm->nout_sess;
                break;
            case 5:
                list = &mm->out_router;
                nlist = &mm->nout_router;
                break;
            case 6:
                list = &mm->pkt_sm;
                nlist = &mm->npkt_sm;
                break;
            case 7:
                list = &mm->pkt_user;
                nlist = &mm->npkt_user;
                break;
            case 8:
                list = &mm->pkt_router;
                nlist = &mm->npkt_router;
                break;
            case 9:
                list = &mm->user_load;
                nlist = &mm->nuser_load;
                break;
            case 10:
                list = &mm->user_create;
                nlist = &mm->nuser_create;
                break;
            case 11:
                list = &mm->user_delete;
                nlist = &mm->nuser_delete;
                break;
        }

        for(j = 0; j < *nlist; j++) {
            mi = (*list)[j];
            if(mi->arg != NULL)
                free(mi->arg);
            free(mi);
        }
    }

    /* free lists */
    free(mm->sess_start);
    free(mm->sess_end);
    free(mm->in_sess);
    free(mm->in_router);
    free(mm->out_sess);
    free(mm->out_router);
    free(mm->pkt_sm);
    free(mm->pkt_user);
    free(mm->pkt_router);
    free(mm->user_load);
    free(mm->user_create);
    free(mm->user_delete);

    xhash_free(mm->modules);

    free(mm);
}

/** session starting */
int mm_sess_start(mm_t mm, sess_t sess) {
    int n, ret = 0;
    mod_instance_t mi;

    log_debug(ZONE, "dispatching sess-start chain");

    ret = 0;
    for(n = 0; n < mm->nsess_start; n++) {
        mi = mm->sess_start[n];
        if(mi == NULL || mi->mod->sess_start == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->sess_start)(mi, sess);
        if(ret != 0)
            break;
    }

    log_debug(ZONE, "sess-start chain returning %d", ret);

    return ret;
}

/** session ending */
void mm_sess_end(mm_t mm, sess_t sess) {
    int n;
    mod_instance_t mi;

    log_debug(ZONE, "dispatching sess-end chain");

    for(n = 0; n < mm->nsess_end; n++) {
        mi = mm->sess_end[n];
        if(mi == NULL || mi->mod->sess_end == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        (mi->mod->sess_end)(mi, sess);
    }

    log_debug(ZONE, "sess-end chain returning");
}

/** packets from active session */
mod_ret_t mm_in_sess(mm_t mm, sess_t sess, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching in-sess chain");

    ret = mod_PASS;
    for(n = 0; n < mm->nin_sess; n++) {
        mi = mm->in_sess[n];
        if(mi == NULL || mi->mod->in_sess == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->in_sess)(mi, sess, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "in-sess chain returning %d", ret);

    return ret;
}

/** packets from router */
mod_ret_t mm_in_router(mm_t mm, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching in-router chain");

    for(n = 0; n < mm->nin_router; n++) {
        mi = mm->in_router[n];
        if(mi == NULL || mi->mod->in_router == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->in_router)(mi, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "in-router chain returning %d", ret);

    return ret;
}

/** packets to active session */
mod_ret_t mm_out_sess(mm_t mm, sess_t sess, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching out-sess chain");

    for(n = 0; n < mm->nout_sess; n++) {
        mi = mm->out_sess[n];
        if(mi == NULL || mi->mod->out_sess == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->out_sess)(mi, sess, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "out-sess chain returning %d", ret);

    return ret;
}

/** packets to router */
mod_ret_t mm_out_router(mm_t mm, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching out-router chain");

    for(n = 0; n < mm->nout_router; n++) {
        mi = mm->out_router[n];
        if(mi == NULL || mi->mod->out_router == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->out_router)(mi, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "out-router chain returning %d", ret);

    return ret;
}

/** packets for sm */
mod_ret_t mm_pkt_sm(mm_t mm, pkt_t pkt) {
    int n, ret = 0;
    mod_instance_t mi;

    log_debug(ZONE, "dispatching pkt-sm chain");

    for(n = 0; n < mm->npkt_sm; n++) {
        mi = mm->pkt_sm[n];
        if(mi == NULL || mi->mod->pkt_sm == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->pkt_sm)(mi, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "pkt-sm chain returning %d", ret);

    return ret;
}

/** packets for user */
mod_ret_t mm_pkt_user(mm_t mm, user_t user, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching pkt-user chain");

    for(n = 0; n < mm->npkt_user; n++) {
        mi = mm->pkt_user[n];
        if(mi == NULL || mi->mod->pkt_user == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->pkt_user)(mi, user, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "pkt-user chain returning %d", ret);

    return ret;
}

/** packets from the router */
mod_ret_t mm_pkt_router(mm_t mm, pkt_t pkt) {
    int n;
    mod_instance_t mi;
    mod_ret_t ret = mod_PASS;

    log_debug(ZONE, "dispatching pkt-router chain");

    for(n = 0; n < mm->npkt_router; n++) {
        mi = mm->pkt_router[n];
        if(mi == NULL || mi->mod->pkt_router == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->pkt_router)(mi, pkt);
        if(ret != mod_PASS)
            break;
    }

    log_debug(ZONE, "pkt-router chain returning %d", ret);

    return ret;
}

/** load user data */
int mm_user_load(mm_t mm, user_t user) {
    int n;
    mod_instance_t mi;
    int ret = 0;

    log_debug(ZONE, "dispatching user-load chain");

    for(n = 0; n < mm->nuser_load; n++) {
        mi = mm->user_load[n];
        if(mi == NULL || mi->mod->user_load == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->user_load)(mi, user);
        if(ret != 0)
            break;
    }

    log_debug(ZONE, "user-load chain returning %d", ret);

    return ret;
}

/** create user */
int mm_user_create(mm_t mm, jid_t jid) {
    int n;
    mod_instance_t mi;
    int ret = 0;

    log_debug(ZONE, "dispatching user-create chain");

    for(n = 0; n < mm->nuser_create; n++) {
        mi = mm->user_create[n];
        if(mi == NULL || mi->mod->user_create == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        ret = (mi->mod->user_create)(mi, jid);
        if(ret != 0)
            break;
    }

    log_debug(ZONE, "user-create chain returning %d", ret);

    return ret;
}

/** delete user */
void mm_user_delete(mm_t mm, jid_t jid) {
    int n;
    mod_instance_t mi;

    log_debug(ZONE, "dispatching user-delete chain");

    for(n = 0; n < mm->nuser_delete; n++) {
        mi = mm->user_delete[n];
        if(mi == NULL || mi->mod->user_delete == NULL) {
            log_debug(ZONE, "module %s has no handler for this chain", mi->mod->name);
            continue;
        }

        log_debug(ZONE, "calling module %s", mi->mod->name);

        (mi->mod->user_delete)(mi, jid);
    }

    log_debug(ZONE, "user-delete chain returning");
}
