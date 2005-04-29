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

/* ---------------------------------------------------------
   base_exec - Starts a specified coprocess and exchanges 
               xmlnodes with it via piped IO
   ---------------------------------------------------------*/

int exec_and_capture(char* const args[], int* in, int* out)
{
     int left_fds[2], right_fds[2];
     int pid;
     char *filename;

     /* Create left and right pipes */
     if (pipe(left_fds) < 0 || pipe(right_fds) < 0)
	  return r_ERR;

     pid = fork();
     if (pid < 0)
	  return r_ERR;
     else if (pid > 0)		/* Parent */
     {
	  /* Close unneeded file handles */
	  close(left_fds[0]);
	  close(right_fds[1]);
	  /* Return the in and out file descriptors */
	  *in = right_fds[0];
	  *out = left_fds[1];
	  return pid;
     }
     else			/* Child */
     {
      char *last,*cur;
	  /* Close unneeded file handles */
	  close(left_fds[1]);
	  close(right_fds[0]);
	  /* Map left's STDIN to the child's STDIN */
	  if (left_fds[0] != 0)
	  {
	       dup2(left_fds[0], 0);
	       close(left_fds[0]);
	  }
	  /* Map right's STDOUT to the child's STDOUT */
	  if (right_fds[1] != 1)
	  {
	       dup2(right_fds[1], 1);
	       close(right_fds[1]);
	  }
	  /* Execute another process */
      for(last=NULL,cur=strchr(args[0],'/');cur!=NULL;last=cur+1,cur=strchr(last,'/'));
      filename=(char*)args[0];
      if(last!=NULL)
      {
        last--;
        last[0]='\0';
        chdir(args[0]);
        filename=last+1;
      }
	  if( execv(filename, args) < 0)
	       exit(1);
     }
     return 0;
}

char** tokenize_args(pool p, const char* cmdstr)
{
     char** result      = NULL;
     char* result_array[100];
     char* result_data = NULL;
     char* token       = NULL;
     int   tokencnt    = 0;
     int   i           = 0;

     /* Simplicity check */
     if (cmdstr == NULL)
	  return NULL;

     /* Create a copy of the command str */
     result_data = pstrdup(p, cmdstr);

     /* Tokenize the string, storing the individual token
	pointers in the result_array */
     while ( (token = strchr(result_data,' ')) != NULL && (tokencnt < 100) )
     {
         /* Insert this token into the result array, and increment our tokencnt */
         *token++ = '\0';
	  result_array[tokencnt++] = token;
     }

     /* Allocate the result -- this was overrunning by 4 bytes, added the +1 */
     result = pmalloco(p, (tokencnt + 1) * sizeof(char*));
     
     /* Iterate across the tokens and store in the result */
     for (i = 0; i < tokencnt; i++)
     {
	  result[i] = result_array[i];
     }

     /* Be sure that the result in NULL terminated */
     result[tokencnt] = NULL;

     return result;
}


/* base_exec -------------------------------------------------------------------------------*/

/* process states */
typedef enum { p_OPEN, p_CLOSED, p_RESTART } pstate;

/* process_info - stores thread data for a coprocess */
typedef struct
{
     char**        args;	   /* Process arguments (ala argv[]) */
     int           pid;		   /* Process ID */
     pstate        state;	   /* Process state flag */
     pool          mempool;	   /* Memory pool for this structt */
     instance      inst;	   /* Instance this coprocess is assoc. with */
     int           in;	   /* Coprocess stdin filehandle */
     int           out;	   /* "     "   stdout "      " */
     pth_msgport_t write_queue;	   /* Queue of write_buf packets which need to be written */
     pth_event_t   e_write;	   /* Event set when data is available to be written */
     pth_event_t   e_read;	   /* Event set when data is available to be read */
     pth_event_t   events;	   /* Event ring for e_write & e_read */
} *process_info, _process_info;


/* process_write_buf - stores a dpacket that needs to be written to the coprocess */
typedef struct
{
     pth_message_t head;
     dpacket       packet;
} *process_write_buf, _process_write_buf;


/* Deliver packets to the coprocess*/
result base_exec_deliver(instance i, dpacket p, void* args)
{
     process_info pi = (process_info)args;
     process_write_buf wb = NULL;

     /* Allocate a new write buffer */
     wb         = pmalloco(p->p, sizeof(_process_write_buf));
     wb->packet = p;
     
     /* Send the buffer to the processing thread */
     pth_msgport_put(pi->write_queue, (pth_message_t*)wb);
     
     return r_DONE;   
}

void base_exec_handle_xstream_event(int type, xmlnode x, void* arg)
{
     process_info pi = (process_info)arg;
     char*   header;
     xmlnode headernode;

     switch(type)
     {
     case XSTREAM_ROOT:
      /* Check incoming root node and verify the namespace */
      if ( j_strcmp(xmlnode_get_attrib(x, "xmlns"), "jabber:component:exec") != 0)
      {
            /* Log that this component sent an invalid namespace... */
            log_warn(pi->inst->id, "Recv'd invalid namespace. Stopping component.");
            /* Notify component with stream:error */
            MIO_WRITE_FUNC(pi->out, SERROR_NAMESPACE, strlen(SERROR_NAMESPACE)); 
            pi->state = p_CLOSED;
            xmlnode_free(x);
            return;
      }
      /* Send a corresponding root node */
	  headernode = xstream_header("jabber:component:exec",NULL, pi->inst->id);
      header     = xstream_header_char(headernode);
	  /* Return a fake root tag */
	  MIO_WRITE_FUNC(pi->out, header, strlen(header));
      xmlnode_free(headernode);
	  /* Hook the event for delivering messages to the coprocess */
	  pi->e_write = pth_event(PTH_EVENT_MSG, pi->write_queue);  
	  pi->events  = pth_event_concat(pi->e_read, pi->e_write, NULL);  
	  /* Validate namespace */
	  xmlnode_free(x);
	  break;
     case XSTREAM_NODE:
	  /* Deliver the packet */
	  deliver(dpacket_new(x), pi->inst);
	  break;
     case XSTREAM_CLOSE:
     case XSTREAM_ERR:
	  xmlnode_free(x);
      pi->state=p_RESTART;
     }

}

/* Process incoming data from the coprocess */
void* base_exec_process_io(void* threadarg)
{
     process_info pi = (process_info)threadarg;
     int      retcode = 0;	   /* Process return code */
     char     readbuf[1024];	   /* Raw buffer to read into */
     int      readlen = 0;	   /* Amount of data read into readbuf */

     xstream  xs;		   /* XMLStream */

     process_write_buf pwb;	   /* Process write buffer */
     char*             writebuf;   /* Raw buffer to write */
     
     /* Setup event ring for this coprocess */
     pi->e_read  = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE, pi->in);
     pi->events  = pth_event_concat(pi->e_read, NULL);

     /* Allocate an xstream for this coprocess */
     xs = xstream_new(pi->mempool, base_exec_handle_xstream_event, threadarg);

     /* Loop on events */
     while (pth_wait(pi->events) > 0)
     {
	  /* Data is available from coprocess */
	  if (pth_event_occurred(pi->e_read))
	  {
	       readlen = MIO_READ_FUNC(pi->in, readbuf, sizeof(readbuf));
	       if (readlen <= 0)
	       {
		    log_debug(ZONE,"base_exec_process_io Read error on process!\n");
		    break;
	       }

	       if (xstream_eat(xs, readbuf, readlen) > XSTREAM_NODE)
		    break;

           /* Check state of the process..if it is now p_CLOSED, go ahead and kick out
            * of the while loop */
           if (pi->state == p_CLOSED || pi->state == p_RESTART)
                   break;
	  }
	  /* Data is available to be written to the coprocess, and the coprocess is ready */
	  if (pth_event_occurred(pi->e_write))
	  {
	       /* Get the packet.. */
	       pwb = (process_write_buf)pth_msgport_get(pi->write_queue);

           /* sanity check */
           if(pwb == NULL) break;
	       
	       /* Serialize the packet.. */
	       writebuf = xmlnode2tstr(pwb->packet->x);

	       /* Write the raw buffer */
	       if (MIO_WRITE_FUNC(pi->out, writebuf, strlen(writebuf)) < 0)
	       {
		    /* FIXME: it would be cool to make this completely safe by reinserting
		       the message back in the queue until the the process is restarted */
		    log_debug(ZONE,"base_exec_process_io Write error.\n");
		    pool_free(pwb->packet->p);
		    break;
	       }
	       
	       /* Data is sent, release the packet */
	       pool_free(pwb->packet->p);
	  }
     }

     /* Cleanup... */
     close(pi->out);
     close(pi->in);
     pth_event_free(pi->e_read, PTH_FREE_THIS);
     pth_event_free(pi->e_write, PTH_FREE_THIS);

     /* Get return code from our coprocess */
     pth_waitpid(pi->pid, &retcode, 0); 

     /* If the state is set to close, an error must have occurred and we won't
      * want to restart the the thread. Otherwise (as shown below) we want to
      * keep the ball rolling and restart the thread */
     if (pi->state != p_CLOSED)
     {
        /* Exec and capture the STDIN/STDOUT */
        pi->pid = exec_and_capture(pi->args, &(pi->in), &(pi->out));

        /* Recreate the thread */
        pth_spawn(PTH_ATTR_DEFAULT, base_exec_process_io, (void*) pi);
     }
     else
     {
        pth_msgport_destroy(pi->write_queue);
     }
     return NULL;
}

result base_exec_config(instance id, xmlnode x, void *arg)
{
     process_info pi = NULL;
	  
     if(id == NULL)
     {	 
	  if (xmlnode_get_data(x) == NULL)
	  {
	       log_debug(ZONE,"base_exec_config error: no script provided\n");
           xmlnode_put_attrib(x,"error","'exec' tag must contain a command line to run");
	       return r_ERR;
	  }
	  log_debug(ZONE,"base_exec_config validating configuration\n");
	  return r_PASS;
     }

     /* Allocate an info structure, and associate with the
	instance pool */
     pi = pmalloco(id->p, sizeof(_process_info));
     pi->inst        = id;
     pi->mempool     = id->p;
     pi->write_queue = pth_msgport_create(id->id);   
     pi->state       = p_OPEN;

     /* Parse out command and arguments */
     pi->args = tokenize_args(pi->mempool, xmlnode_get_data(x));

     /* Exec and capture the STDIN/STDOUT of the child process */
     pi->pid = exec_and_capture(pi->args, &(pi->in), &(pi->out));

     /* Spawn a new thread to handle IO for this coprocess */
     pth_spawn(PTH_ATTR_DEFAULT, base_exec_process_io, (void*) pi);

     /* Register a handler to recieve inbound data */
     register_phandler(id, o_DELIVER, base_exec_deliver, (void*) pi);

     log_debug(ZONE,"base_exec_config performing configuration %s\n",xmlnode2str(x));
     return r_DONE;
}

void base_exec(void)
{
     log_debug(ZONE,"base_exec loading...\n");
     register_config("exec",base_exec_config,NULL);
}

