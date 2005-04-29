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

#include "jabberd.h"
#include "single.h"
#define MAX_INCLUDE_NESTING 20
extern HASHTABLE cmd__line;
extern pool      jabberd__runtime;
HASHTABLE instance__ids=NULL;

typedef struct shutdown_list
{
    pool p;
    shutdown_func f;
    void *arg;
    struct shutdown_list *next;
} _sd_list, *sd_list;
sd_list shutdown__list=NULL;

xmlnode greymatter__ = NULL;

void do_include(int nesting_level,xmlnode x)
{
    xmlnode cur;

    cur=xmlnode_get_firstchild(x);
    for(;cur!=NULL;)
    {
        if(cur->type!=NTYPE_TAG) 
        {
            cur=xmlnode_get_nextsibling(cur);
            continue;
        }
        if(j_strcmp(xmlnode_get_name(cur),"jabberd:include")==0)
        {
            xmlnode include;
            char *include_file=xmlnode_get_data(cur);
            xmlnode include_x=xmlnode_file(include_file);
            /* check for bad nesting */
            if(nesting_level>MAX_INCLUDE_NESTING)
            {
                fprintf(stderr, "ERROR: Included files nested %d levels deep.  Possible Recursion\n",nesting_level);
                exit(1);
            }
            include=cur;
            xmlnode_hide(include);
            /* check to see what to insert...
             * if root tag matches parent tag of the <include/> -- firstchild
             * otherwise, insert the whole file
             */
             if(j_strcmp(xmlnode_get_name(xmlnode_get_parent(cur)),xmlnode_get_name(include_x))==0)
                xmlnode_insert_node(x,xmlnode_get_firstchild(include_x));
             else
                xmlnode_insert_node(x,include_x);
             do_include(nesting_level+1,include_x);
             cur=xmlnode_get_nextsibling(cur);
             continue;
        }
        else 
        {
            do_include(nesting_level,cur);
        }
        cur=xmlnode_get_nextsibling(cur);
    }
}

void cmdline_replace(xmlnode x)
{
    char *flag;
    char *replace_text;
    xmlnode cur=xmlnode_get_firstchild(x);

    for(;cur!=NULL;cur=xmlnode_get_nextsibling(cur))
    {
        if(cur->type!=NTYPE_TAG)continue;
        if(j_strcmp(xmlnode_get_name(cur),"jabberd:cmdline")!=0)
        {
            cmdline_replace(cur);
            continue;
        }
        flag=xmlnode_get_attrib(cur,"flag");
        replace_text=ghash_get(cmd__line,flag);
        if(replace_text==NULL) replace_text=xmlnode_get_data(cur);

        xmlnode_hide(xmlnode_get_firstchild(x));
        xmlnode_insert_cdata(x,replace_text,-1);
        break;
    }
}

/* 
 * <pidfile>/path/to/pid.file</pidfile>
 *
 * Ability to store the PID of the process in a file somewhere.
 *
 */
void show_pid(xmlnode x)
{
    xmlnode pidfile;
    char *path;
    char pidstr[16];
    int fd;
    pid_t pid;

    /* HACKAGE: if we're reloading, ignore this check */
    if(jabberd__signalflag == SIGHUP) return;

    pidfile = xmlnode_get_tag(x, "pidfile");
    if(pidfile == NULL)
        return;

    path = xmlnode_get_data(pidfile);
    if(path == NULL)
    {
        return;
    }

    unlink(path);
    fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if(fd < 0)
    {
        fprintf(stderr, "Error opening pidfile at [%s].  %s.", path, strerror(errno) );
        exit(1);
    }
    pid = getpid();
    snprintf(pidstr, 16, "%d", pid);
    write(fd, &pidstr, strlen(pidstr));
    close(fd);

    return;
}

int configurate(char *file)
{
    char def[] = "jabber.xml";
    char *realfile = (char *)def;
    xmlnode incl;
    char *c;

    /* if no file name is specified, fall back to the default file */
    if(file != NULL)
        realfile = file;

    /* read and parse file */
    greymatter__ = xmlnode_file(realfile);

#ifdef SINGLE
    if(greymatter__ == NULL)
        greymatter__ = xmlnode_str(SINGLE_CONFIG, strlen(SINGLE_CONFIG));
#endif

    /* was the there a read/parse error? */
    if(greymatter__ == NULL)
    {
        fprintf(stderr, "Configuration parsing using %s failed: %s\n",realfile,xmlnode_file_borked(realfile));
        return 1;
    }

    /* parse -i foo.xml,bar.xml */
    if((realfile = ghash_get(cmd__line,"i")) != NULL)
        while(realfile != NULL)
        {
            c = strchr(realfile,',');
            if(c != NULL)
            {
                *c = '\0';
                c++;
            }
            if((incl = xmlnode_file(realfile)) == NULL)
            {
                fprintf(stderr, "Configuration parsing included file %s failed: %s\n",realfile,xmlnode_file_borked(realfile));
                return 1;
            }else{
                xmlnode_insert_tag_node(greymatter__,incl);
                xmlnode_free(incl);
            }
            realfile = c;
        }


    /* check greymatter for additional includes */
    do_include(0,greymatter__);
    cmdline_replace(greymatter__);

    show_pid(greymatter__);

    return 0;
}

int config_reload(char *file)
{
    xmlnode old_config=greymatter__;
    int retval=configurate(file);

    if(retval) /* failed to load config */
    {
        greymatter__=old_config; /* restore old config */
        return 1;
    }
    else
    {
        xmlnode_free(old_config); /* free the old config */
        return 0;
    }
}
/* private config handler list */
typedef struct cfg_struct
{
    char *node;
    cfhandler f;
    void *arg;
    struct cfg_struct *next;
} *cfg, _cfg;

cfg cfhandlers__ = NULL;
pool cfhandlers__p = NULL;

/* register a function to handle that node in the config file */
void register_config(char *node, cfhandler f, void *arg)
{
    cfg newg;

    cfhandlers__p = jabberd__runtime;

    /* create and setup */
    newg = pmalloc_x(cfhandlers__p, sizeof(_cfg), 0);
    newg->node = pstrdup(cfhandlers__p,node);
    newg->f = f;
    newg->arg = arg;

    /* hook into global */
    newg->next = cfhandlers__;
    cfhandlers__ = newg;
}

/* util to scan through registered config callbacks */
cfg cfget(char *node)
{
    cfg next = NULL;

    for(next = cfhandlers__; next != NULL && strcmp(node,next->node) != 0; next = next->next);

    return next;
}

/* 
 * walk through the instance HASH, and cleanup the instances
 */
int _instance_cleanup(void *arg,const void *key,void *data)
{
    instance i=(instance)data;
    unregister_instance(i,i->id);
    ghash_remove(instance__ids, i->id);
    while(i->hds)
    {
        handel h=i->hds->next;
        pool_free(i->hds->p);
        i->hds=h;
    }
    pool_free(i->p);
    return 1;
}

int instance_startup(xmlnode x, int exec)
{

    ptype type;
    xmlnode cur;
    cfg c;
    instance newi = NULL;
    pool p;

    type = p_NONE;

    if(j_strcmp(xmlnode_get_name(x), "pidfile") == 0)
        return 0;
    if(j_strcmp(xmlnode_get_name(x), "io") == 0)
        return 0;

    if(j_strcmp(xmlnode_get_name(x), "log") == 0)
        type = p_LOG;
    if(j_strcmp(xmlnode_get_name(x), "xdb") == 0)
        type = p_XDB;
    if(j_strcmp(xmlnode_get_name(x), "service") == 0)
        type = p_NORM;

    if(type == p_NONE || xmlnode_get_attrib(x, "id") == NULL || xmlnode_get_firstchild(x) == NULL)
    {
        fprintf(stderr, "Configuration error in:\n%s\n", xmlnode2str(x));
        if(type == p_NONE) 
        {
            fprintf(stderr, "ERROR: Invalid Tag type: %s\n",xmlnode_get_name(x));
        }
        if(xmlnode_get_attrib(x, "id") == NULL)
        {
            fprintf(stderr, "ERROR: Section needs an 'id' attribute\n");
        }
        if(xmlnode_get_firstchild(x)==NULL)
        {
            fprintf(stderr, "ERROR: Section Has no data in it\n");
        }
        return -1;
    }

    if(exec == 1)
    {
        newi = ghash_get(instance__ids, xmlnode_get_attrib(x,"id"));
        if(newi != NULL)
        {
            fprintf(stderr, "ERROR: Multiple Instances with same id: %s\n",xmlnode_get_attrib(x,"id"));
            return -1;
        }
    }

    /* create the instance */
    if(exec)
    {
        jid temp;
        p = pool_new();
        newi = pmalloc_x(p, sizeof(_instance), 0);
        newi->id = pstrdup(p,xmlnode_get_attrib(x,"id"));
        newi->type = type;
        newi->p = p;
        newi->x = x;
        /* make sure the id is valid for a hostname */
        temp = jid_new(p, newi->id);
        if(temp == NULL || j_strcmp(temp->server, newi->id) != 0)
        {
            log_alert(NULL, "ERROR: Invalid id name: %s\n",newi->id);
            pool_free(p);
            return -1;
        }
        ghash_put(instance__ids,newi->id,newi);
        register_instance(newi,newi->id);
    }


    /* loop through all this sections children */
    for(cur = xmlnode_get_firstchild(x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        /* only handle elements */
        if(xmlnode_get_type(cur) != NTYPE_TAG)
            continue;

        /* find the registered function for this element */
        c = cfget(xmlnode_get_name(cur));

        /* if we don't have a handler, but we do have a namespace, we can just be ignored */
        if(c == NULL && xmlnode_get_attrib(cur, "xmlns") != NULL)
            continue;

        /* no handler or handler returning an error, die */
        if(c == NULL  || (c->f)(newi, cur, c->arg) == r_ERR)
        {
            char *error = pstrdup(xmlnode_pool(cur), xmlnode_get_attrib(cur,"error"));
            xmlnode_hide_attrib(cur, "error");
            fprintf(stderr, "Invalid Configuration in instance '%s':\n%s\n",xmlnode_get_attrib(x,"id"),xmlnode2str(cur));
            if(c == NULL) 
                fprintf(stderr, "ERROR: Unknown Base Tag: %s\n",xmlnode_get_name(cur));
            else if(error != NULL)
                fprintf(stderr, "ERROR: Base Handler Returned an Error:\n%s\n", error);
            return -1;
        }
    }

    return 0;
}

/* execute configuration file */
int configo(int exec)
{
    xmlnode cur;

    if(instance__ids==NULL)
        instance__ids = ghash_create_pool(jabberd__runtime, 19,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);

    for(cur = xmlnode_get_firstchild(greymatter__); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if(xmlnode_get_type(cur) != NTYPE_TAG || strcmp(xmlnode_get_name(cur),"base") == 0)
            continue;

        if(instance_startup(cur, exec))
        {
            return 1;
        }

    }

    return 0;
}

/* shuts down a single instance,
 * or all the instances, if i == NULL
 */
void instance_shutdown(instance i)
{
    if(i != NULL)
    {
        unregister_instance(i,i->id);
        ghash_remove(instance__ids, i->id);
        while(i->hds)
        {
            handel h=i->hds->next;
            pool_free(i->hds->p);
            i->hds=h;
        }
        pool_free(i->p);
    }
    else
    {
        ghash_walk(instance__ids, _instance_cleanup, NULL);
    }
}

void shutdown_callbacks(void)
{
    while(shutdown__list)
    {
        sd_list s=shutdown__list->next;
        (*shutdown__list->f)(shutdown__list->arg);
        pool_free(shutdown__list->p);
        shutdown__list=s;
    }
}

void register_shutdown(shutdown_func f,void *arg)
{
    pool p;
    sd_list new;
    if(f==NULL) return;
    
    p=pool_new();
    new=pmalloco(p,sizeof(_sd_list));
    new->p=p;
    new->f=f;
    new->arg=arg;
    new->next=shutdown__list;
    shutdown__list=new;
}
