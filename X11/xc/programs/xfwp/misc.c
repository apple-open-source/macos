/* $Xorg: misc.c,v 1.6 2001/02/09 02:05:45 xorgcvs Exp $ */

/*
Copyright "1986-1997, 1998 The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and the following permission notice
shall be included in all copies of the Software:

THE SOFTWARE IS PROVIDED "AS IS ", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NON-INFRINGEMENT. IN NO EVENT SHALL THE OPEN GROUP BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER SIABILITIY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN
CONNNECTION WITH THE SOFTWARE OR THE USE OF OTHER DEALINGS IN
THE SOFTWARE.

Except as contained in this notice, the name of The Open Group
shall not be used in advertising or otherwise to promote the use
or other dealings in this Software without prior written
authorization from The Open Group.

X Window System is a trademark of The Open Group.
*/
/* $XFree86: xc/programs/xfwp/misc.c,v 1.7 2001/12/14 20:01:43 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xos.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <X11/Xfuncs.h>			/* Need for bcopy() */
#include <X11/ICE/ICElib.h>
#include <X11/PM/PM.h>

#include "xfwp.h"
#include "misc.h"
#include "pm.h"

static Bool printConfigVerify = FALSE;
static Bool HaveSitePolicy = 0;


/*ARGSUSED*/
static void 
BadSyntax(
    char *msg, 
    int line)
{
#ifdef DEBUG
    (void) fprintf(stderr, "Config error: %s at line %d\n", msg, line);
#endif
}

static void 
Usage(void)
{
    (void) fprintf (stderr, "Usage:  xfwp [-pdt <#secs>] [-clt <#secs>] \\\n"); 
    (void) fprintf (stderr, 
		    "\t[-cdt <#secs>] [-pmport <port#>] [-config <path>]\\\n");
    (void) fprintf (stderr, 
		    "\t[-logfile <path>] [-loglevel <0|1>] [-verify]\n");
    exit (0);
}

static void 
MallocFailed(void)
{
    (void) fprintf(stderr, "Memory allocation failed, exiting\n");
    exit(1);
}

static char* 
Realloc(
    char *p, 
    int s)
{
    if (!p)
	p = malloc(s);
    else
	p = realloc(p, s);

    if (!p)
	MallocFailed();

    return p;
}

static void 
BadMalloc(
    int line)
{
    (void) fprintf(stderr, "Error: memory exhaused at line %d\n", line);
}

static void 
doPrintEval(
    struct config * config_info, 
    int line_counter)
{
    struct config_line *ruleP = config_info->config_file_data[line_counter];

    if (!printConfigVerify)
        return;

    (void) fprintf(stderr,"matched: %s %s %s %s %s %s %s\n",
	    	   (ruleP->permit_deny)     ? ruleP->permit_deny     : "",
	    	   (ruleP->source_hostname) ? ruleP->source_hostname : "", 
	    	   (ruleP->source_netmask)  ? ruleP->source_netmask  : "",
	    	   (ruleP->dest_hostname)   ? ruleP->dest_hostname   : "", 
	    	   (ruleP->dest_netmask)    ? ruleP->dest_netmask    : "",
	    	   (ruleP->operator)        ? ruleP->operator        : "",
	    	   (ruleP->service)         ? ruleP->service         : "");
}

static Bool
doConfigRequireDisallow(
    int line, 
    char* result)
{
  Bool	permit = (strcmp("require", result) == 0);

  if (((result = strtok(NULL, SEPARATOR1)) == NULL) ||
      (strcmp(result, "sitepolicy") != 0))
  {
      BadSyntax("require/disallow must specify \"sitepolicy\"", line);
      return 1;
  }

  if (HaveSitePolicy && (SitePolicyPermit != permit))
  {
      BadSyntax("can't mix require and disallow policies", line);
      return 1;
  }

  HaveSitePolicy = True;
  SitePolicyPermit = permit;

  if ((result = strtok(NULL, " \n")) == NULL)
  {
      BadSyntax("missing policy string after \"sitepolicy\"", line);
      return 1;
  }

  if (SitePolicies)
    SitePolicies = (char**)realloc((char**)SitePolicies,
				   (SitePolicyCount+1) * sizeof(char*));
  else
    SitePolicies = (char**)malloc(sizeof(char*));

  if (!SitePolicies)
  {
      BadMalloc(line);
      return 1;
  }

  SitePolicies[SitePolicyCount] = malloc(strlen(result) + 1);

  if (!SitePolicies[SitePolicyCount])
  {
      BadMalloc(line);
      return 1;
  }

  strcpy(SitePolicies[SitePolicyCount++], result);

#ifdef DEBUG
  (void) fprintf(stderr, "%s %s", permit ? "requiring" : "disallowing", result);
#endif

  return False;
}

static int 
doVerifyHostMaskToken(
    char token[])
{
  char * result;
  int	 delimiter_count = 0;

  /*
   * verify there are 3 "." delimiters in the token
   */
  while (token)
  {
    if ((result = strchr(token, SEPARATOR2)) != NULL)
    {
      token = result;
      delimiter_count++;
      token ++;
    } else
      token = result;
  }
  if ((delimiter_count < 3) || (delimiter_count > 3))
    return 0; 
  else
    return 1;
}

static int 
doInitNewRule(
    struct config 	*config_info)
{
  int 			rule_number = config_info->rule_count;
  struct config_line 	*config_lineP;

  if (rule_number == config_info->lines_allocated)
  {
      if ((config_info->config_file_data = (struct config_line**)
	       Realloc((char*)config_info->config_file_data,
	    	       (config_info->lines_allocated += ADD_LINES) *
		       sizeof(struct config_line *))) == NULL) 
      {
        (void) fprintf (stderr, "realloc - config_file_data\n");
        return -1;
      }
  }
	
  if ((config_lineP = (struct config_line *) 
	  Malloc (sizeof(struct config_line))) == NULL)
  {
    (void) fprintf (stderr, "malloc - config_lineP\n");
    return -1;
  }

  config_lineP->permit_deny = NULL;
  config_lineP->source_hostname = NULL;
  config_lineP->source_host = 0;
  config_lineP->source_netmask = NULL;
  config_lineP->source_net = 0;
  config_lineP->dest_hostname = NULL;
  config_lineP->dest_host = 0;
  config_lineP->dest_netmask = NULL;
  config_lineP->dest_net = 0;
  config_lineP->operator = NULL;
  config_lineP->service = NULL; 

  config_info->config_file_data[rule_number] = config_lineP;

  return rule_number;
}

static int
doConfigPermitDeny(
    struct config *config_info, 
    char *result)
{
  struct config_line ** config_file_data;
  int		line_number;
  int		bad_token = 0;

  /* 
   * caution; config_info->config_file_data can move in doInitNewRule 
   */
  if ((line_number = doInitNewRule(config_info)) == -1)
    return 1;

  config_file_data = config_info->config_file_data;

  if ((config_file_data[line_number]->permit_deny = 
       (char *) malloc (strlen(result) + 1)) == NULL)
  {
    (void) fprintf(stderr, "malloc - config rule (permit/deny keyword)\n");
    return 0;
  }
  strcpy(config_file_data[line_number]->permit_deny, result);

#ifdef DEBUG
  (void) fprintf(stderr, 
		 "first token = %s\n", 
		 config_file_data[line_number]->permit_deny);
#endif

  /*
   * do the source hostname field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    char   	token[64];
    strcpy(token, result);

    if (doVerifyHostMaskToken(token))
    {
      if ((config_file_data[line_number]->source_hostname = 
	      (char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (source host)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->source_hostname, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	   	     "second token = %s\n", 
	    	     config_file_data[line_number]->source_hostname);
#endif
      /*
       * generate network address format
       */
      config_file_data[line_number]->source_host = 
	      inet_addr(config_file_data[line_number]->source_hostname);
    } else
      bad_token = 1;
  }

  /*
   * now the source netmask field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    char   	token[64];
    strcpy(token, result);

    if (doVerifyHostMaskToken(token))
    {
      if ((config_file_data[line_number]->source_netmask = 
		(char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (source netmask)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->source_netmask, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	   	     "third token = %s\n", 
	    	     config_file_data[line_number]->source_netmask);
#endif
      config_file_data[line_number]->source_net = 
	      inet_addr(config_file_data[line_number]->source_netmask);
    } else
      bad_token = 1;
  }

  /*
   * now the destination hostname field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    char   	token[64];
    strcpy(token, result);

    if (doVerifyHostMaskToken(token))
    {
      if ((config_file_data[line_number]->dest_hostname = 
		(char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (destination host)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->dest_hostname, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	   	     "fourth token = %s\n", 
	    	     config_file_data[line_number]->dest_hostname);
#endif
      config_file_data[line_number]->dest_host = 
	      inet_addr(config_file_data[line_number]->dest_hostname);
    } else
      bad_token = 1;
  }

  /*
   * now the destination netmask field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    char   	token[64];
    strcpy(token, result);

    if (doVerifyHostMaskToken(token))
    {
      if ((config_file_data[line_number]->dest_netmask = 
		(char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (destination mask)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->dest_netmask, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	   	     "fifth token = %s\n", 
	    	     config_file_data[line_number]->dest_netmask);
#endif
      config_file_data[line_number]->dest_net = 
	      inet_addr(config_file_data[line_number]->dest_netmask);
    } else
      bad_token = 1;
  }

  /*
   * now the operator field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    if (!strcmp("eq", result))
    {
      if ((config_file_data[line_number]->operator = 
		(char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (op)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->operator, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	      	     "sixth token = %s\n", 
	      	     config_file_data[line_number]->operator);
#endif
    } else
      bad_token = 1;
  }

  /*
   * and finally the service field
   */
  if ((result = strtok(NULL, SEPARATOR1)) != NULL)
  {
    if (!(strncmp("pm", result, 2)) || 
        (!strncmp("fp", result, 2)) ||
        (!strncmp("cd", result, 2)))	
    {
      if ((config_file_data[line_number]->service = 
		(char *) malloc (strlen(result) + 1)) == NULL)
      {
	(void) fprintf(stderr, "malloc - config rule (service)\n");
	return 0;
      }
      strcpy(config_file_data[line_number]->service, result);
#ifdef DEBUG
      (void) fprintf(stderr, 
	      	     "seventh token = %s\n", 
	      	     config_file_data[line_number]->service);
#endif
      /*
       * load the appropriate service id
       */
      if (!strncmp(config_file_data[line_number]->service, "pm", 2))
	config_file_data[line_number]->service_id = PMGR;
      else if (!strncmp(config_file_data[line_number]->service, "fp", 2))
	config_file_data[line_number]->service_id = FINDPROXY;
      else 
	if (!strncmp(config_file_data[line_number]->service, "cd", 2))
	  config_file_data[line_number]->service_id = CLIENT;
    } else
      bad_token = 1;
  }

  /*
   * rules for bad parse
   */
  if (bad_token ||
      (config_file_data[line_number]->permit_deny == NULL) ||
      ((config_file_data[line_number]->permit_deny != NULL) && 
       (config_file_data[line_number]->source_hostname == NULL)) ||
      ((config_file_data[line_number]->source_hostname != NULL) && 
       (config_file_data[line_number]->source_netmask == NULL)) || 
      ((config_file_data[line_number]->dest_hostname != NULL) && 
       (config_file_data[line_number]->dest_netmask == NULL)) || 
      ((config_file_data[line_number]->operator != NULL) && 
       (config_file_data[line_number]->service == NULL)))
      return 1;

  config_info->rule_count++;
  return 0;
}

static int 
doProcessLine(
    char *line,
    struct config *config_info,
    int config_line)
{
  char * 	result;
  int		bad_parse = 0;

  if (line[0] == '#' || line[0] == '\n')
      return 1;

  if ((result = strtok(line, SEPARATOR1)) != NULL)
  {
    if (!(strcmp("permit", result)) || (!strcmp("deny", result)))
    {
	bad_parse = doConfigPermitDeny(config_info, result);
    }
    else
    if (!strcmp("require", result) || !strcmp("disallow", result))
	bad_parse = doConfigRequireDisallow(config_line, result);

    else
	bad_parse = 1;
  }

  if (bad_parse)
    return 0;
  else
    return 1;
}

/*
 * Public functions
 */
char* 
Malloc(
    int s)
{
    char *p = malloc(s);

    if (!p)
	MallocFailed();

    return p;	
}

int 
doConfigCheck(
    struct sockaddr_in 	* source_sockaddr_in,
    struct sockaddr_in 	* dest_sockaddr_in,
    struct config 	* config_info,
    int 		context,
    int			* rule_number)
{
  int			line_counter;

  /*
   * look through the config file parse tree for a source IP address
   * that matches this request
   */ 
  for (line_counter = 0; line_counter < config_info->rule_count; line_counter++)
  {
    if (config_info->config_file_data[line_counter] != NULL)
    {
      if ((source_sockaddr_in->sin_addr.s_addr & 
	  (~(config_info->config_file_data[line_counter]->source_net))) ==
	  config_info->config_file_data[line_counter]->source_host)
      {
	/*
	 * okay, the source host and netmask fields pass, see if the
         * config file specifies "permit" or "deny" for this host
         */
        if (!strcmp(config_info->config_file_data[line_counter]->permit_deny,
	     "permit"))
  	{
	  /*
           * check for presence of destination info
           */
	  if ((config_info->config_file_data[line_counter]->dest_hostname) &&
	      (context != PMGR))
          {
	    /*
             * compute destination info restrictions
             */
            if ((dest_sockaddr_in->sin_addr.s_addr & 
	        (~(config_info->config_file_data[line_counter]->dest_net))) ==
	        config_info->config_file_data[line_counter]->dest_host)
	    {
	      /*
	       * you got a match on the destination, so look at 
               * the operator and service fields to see if the "permit"
               * might be specific to one particular connection-type only
               */
    	      if (config_info->config_file_data[line_counter]->operator != NULL)
	      {
		/*
                 * there *is* a service id; see if it matches our current
                 * config check request
                 */
		if (config_info->config_file_data[line_counter]->service_id == 
			context)
		{
		  doPrintEval(config_info, line_counter);  
                  /*
                   * if you are permitting, there's no rule match to log 
                   */
                  *rule_number = line_counter + 1;
		  return 1;
		} else
		  /*
	           * we didn't get a match on context; this "permit" doesn't 
		   * apply to the current request; so keep trying 
		   */
		  continue;
	      } else	
		/*
		 * there's no service qualifier; permit the connection 
		 */
		doPrintEval(config_info, line_counter);
                *rule_number = line_counter + 1;
		return 1;
            } else
              /*
	       * the destination field doesn't match; keep trying
               */
	      continue;
	  } else if ((config_info->
			config_file_data[line_counter]->dest_hostname) &&
	      		(context == PMGR))
            { 
	      /*
               * skip the destination address check and test for
               * the operator and service_id
               */
    	      if (config_info->config_file_data[line_counter]->operator != NULL)
	      {
		/*
                 * there *is* a service id; see if it matches our current
                 * config check context 
                 */
		if (config_info->config_file_data[line_counter]->service_id 
								== context)
		{
		  doPrintEval(config_info, line_counter);
                  /*
                   * not logging PM events so don't save rule match
                   */
		  return 1;
		} else
		  /*
	           * we didn't get a match on context; this "permit" doesn't
		   * apply to the current client request; so keep trying 
		   */
		  continue;
	      } else	
              {		
		/*
		 * there's no service qualifier; permit the connection 
		 */
		doPrintEval(config_info, line_counter);
                *rule_number = line_counter + 1;
		return 1;
	      }
	    } else
	    {
	      /*
               * there's no destination specified; permit the connection
               */
	      doPrintEval(config_info, line_counter);
              *rule_number = line_counter + 1;
	      return 1;
	    }
        }
        else
  	{
	  /*
           * we still have to check the destination and service fields
	   * to know exactly what we are denying
	   */
	  if ((config_info->config_file_data[line_counter]->dest_hostname) &&
	      (context != PMGR))
          {
	    /*
             * compute destination info restrictions
             */
            if ((dest_sockaddr_in->sin_addr.s_addr & 
	        (~(config_info->config_file_data[line_counter]->dest_net))) ==
	        config_info->config_file_data[line_counter]->dest_host)
	    {
	      /*
	       * you got a match on the destination, so look at
               * the operator and service fields to see if the "deny"
               * might be specific to one particular connection-type only
               */
    	      if (config_info->config_file_data[line_counter]->operator != NULL)
	      {
		/*
                 * there *is* a service id; see if it matches our current
                 * config check request
                 */
		if (config_info->config_file_data[line_counter]->service_id == 
			context)
		{
		  /*
                   * the match signifies an explicit denial of permission
	           */
		  doPrintEval(config_info, line_counter);  
                  /*
                   * save the rule match number before returning 
                   */
                  *rule_number = line_counter + 1;
		  return 0;
	        } else
		  /*
		   * we didn't get a match on the service id; the "deny"
	           * operation doesn't apply to this connection, so keep
                   * trying 
		   */
		  continue;
	      } else	
	      {
		/*
		 * there's no service qualifier; deny the connection 
		 */
		doPrintEval(config_info, line_counter);  
                /*
                 * save the rule match number before returning 
                 */
                *rule_number = line_counter + 1;
                return 0;
	      }	
            } else
              /*
               * the destination field doesn't match; keep trying 
               */
	      continue;
	  } else if ((config_info->
			config_file_data[line_counter]->dest_hostname) &&
	      		(context == PMGR))
            { 
	      /*
               * skip the destination address check and test for
               * the operator and service_id
               */
    	      if (config_info->config_file_data[line_counter]->operator != NULL)
	      {
		/*
                 * there *is* a service id; see if it matches our current
                 * config check context 
                 */
		if (config_info->config_file_data[line_counter]->service_id == 
			context)
		{
		  /*
                   * this is a request to explicitly deny service, so do it
		   */
		  doPrintEval(config_info, line_counter);  
                  /*
                   * not logging PM events, but if we were, save rule match here
                   */
		  return 0;
		} else
		  /*
	           * we didn't get a match on context; this "deny" doesn't
		   * apply to the current client request; so keep trying 
		   */
		  continue;
	      } else	
	      {
		/*
		 * there's no service qualifier; deny the connection 
		 */
		doPrintEval(config_info, line_counter);  
                /*
                 * if we were logging PM events ...
                 */
                return 0;
	      }
	    } else
	    {
	      /*
	       * there's no destination specified; deny the connection
	       */
	      doPrintEval(config_info, line_counter);  
              /*
               * save rule match
               */
              *rule_number = line_counter + 1;
              return 0;	
	    }
	  } /* end else deny */
      } /* end if match on source */
    } /* end if valid config line */
  } /* end all config lines for loop */

  /*
   * whatever you did not explicitly permit you must deny -- *unless* --
   * no config file was specified, in which case permit all
   */
  if (config_info->config_file_path == NULL)
  {
    if (printConfigVerify)
	(void) fprintf(stderr, 
		       "matched default permit 0.0.0.0 255.255.255.255\n");
    /*
     * there's no rule match to save
     */
    *rule_number = -1;
    return 1;
  }

  if (!config_info->config_file_data)
  {
     /*
      * A config file was specified but it had no permit/deny
      * entries.  This can happen if: the file was empty; the
      * file only contained comment entries; the file only has
      * require/disallow entries.  In any event, this case
      * should be treated as if no config file was used - that
      * is allow the connection.
      */
    *rule_number = -1;
    return 1;
  }

  if (printConfigVerify)
      (void) fprintf(stderr, "matched default deny 0.0.0.0 255.255.255.255\n");

  /*
   * not in this case either 
   */
  *rule_number = -1;
  return 0;
}


void 
doCheckTimeouts(
    struct config * config_info,
    int * nfds_ready,
    fd_set * rinit,
    fd_set * winit,
    fd_set * readable,
    fd_set * writable)
{
  int			client_data_counter;
  int			client_listen_counter;
  int			pm_conn_counter;
  struct timeval 	current_time;	
  struct timezone 	current_zone;

  /*
   * get current time
   */
  gettimeofday(&current_time, &current_zone); 

  /*
   * start with the clients; we have to do them all, because a 
   * timeout may occur even if the object's fd is not currently
   * readable or writable
   */
  for (client_data_counter = 0; 
       client_data_counter < config_info->num_client_conns;
       client_data_counter++)
  {
    if (client_conn_array[client_data_counter] != NULL)
    {
      /*
       * do the shutdown time computation
       */
      if ((current_time.tv_sec 
	  - client_conn_array[client_data_counter]->creation_time) 
	  > client_conn_array[client_data_counter]->time_to_close)
      {
	/*
         * time to shut this client conn down; we're not going to be graceful
         * about it, either; we're just going to clear the select() masks for 
         * the relevant file descriptors, close these fd's and deallocate
         * the connection objects (for both client and server), and finally 
         * adjust the select() return params as necessary
         */
	FD_CLR(client_conn_array[client_data_counter]->fd, rinit);
	FD_CLR(client_conn_array[client_data_counter]->fd, winit);
        FD_CLR(client_conn_array[client_data_counter]->conn_to, rinit);
        FD_CLR(client_conn_array[client_data_counter]->conn_to, winit); 
 	close(client_conn_array[client_data_counter]->fd);
 	close(client_conn_array[client_data_counter]->conn_to);
        free(client_conn_array[client_conn_array[client_data_counter]->conn_to]);
        if (client_conn_array[client_data_counter]->source)
	    free(client_conn_array[client_data_counter]->source);
        if (client_conn_array[client_data_counter]->destination)
	    free(client_conn_array[client_data_counter]->destination);
        free(client_conn_array[client_data_counter]);
        client_conn_array[client_conn_array[client_data_counter]->conn_to] = 
		NULL;
        client_conn_array[client_data_counter] = NULL; 
	/*
	 * the nfds_ready value is tricky, because we're not sure if we got
         * a readable or writable on the associated connection for this 
         * iteration through select(); we'll decrement it one instead of two,
         * but it really doesn't matter either way given the logic of the
         * process readables and writables code
 	 */
	(*nfds_ready)--;   /* XXX was *nfds_ready-- found spurious by gcc */
	/*
	 * if you just shut this connection object down, you don't want
         * to reset its creation date to now, so go to the next one
         */
 	continue;	
      }
      /*
       * recompute select() timeout to maximize blocking time without
       * preventing timeout checking
       */
      config_info->select_timeout.tv_sec = 
			min(config_info->select_timeout.tv_sec,
			client_conn_array[client_data_counter]->time_to_close -
			(current_time.tv_sec  - 
			client_conn_array[client_data_counter]->creation_time));
      /*
       * this wasn't a shutdown case, so check to see if there's activity
       * on the fd; if so, then reset the creation time field to now 
       */
      if (FD_ISSET(client_conn_array[client_data_counter]->fd, readable) ||
	  FD_ISSET(client_conn_array[client_data_counter]->fd, writable)) 
	client_conn_array[client_data_counter]->creation_time = 
		current_time.tv_sec;
      /*
       * do the same thing with the conn_to connections, but only
       * if they haven't already been marked for closing
       */
      if ((client_conn_array[client_data_counter]->conn_to) > 0)
      { 
      
        if ((FD_ISSET(client_conn_array[client_data_counter]->conn_to, 
								readable)) || 
	    (FD_ISSET(client_conn_array[client_data_counter]->conn_to, 
								writable)))
	  client_conn_array[client_data_counter]->creation_time = 
							current_time.tv_sec;
      }
    }
  } 

  /*
   * now do the client listen fds; as with the client data objects, 
   * we have to do them all, because a timeout may occur even if the
   * object's fd is not currently readable or writable
   */
  for (client_listen_counter = 0; 
       client_listen_counter < config_info->num_servers;
       client_listen_counter++)
  {
    if (server_array[client_listen_counter] != NULL)
    {
      /*
       * do the shutdown time computation
       */
      if ((current_time.tv_sec 
	  - server_array[client_listen_counter]->creation_time) 
	  > server_array[client_listen_counter]->time_to_close)
      {
	/*
         * time to shut this listener down just like we did above;
         * we close the server connection here as well but we don't
         * need to worry about the select() mask because we're not
         * using the server fd in this object for reading or writing --
         * only to initialize the client data object server connections
         */
	FD_CLR(server_array[client_listen_counter]->client_listen_fd, rinit);
	FD_CLR(server_array[client_listen_counter]->client_listen_fd, winit);
 	close(server_array[client_listen_counter]->client_listen_fd);
        free(server_array[client_listen_counter]);
        server_array[client_listen_counter] = NULL; 
	(*nfds_ready)--;	/* XXX */
	/*
	 * if you just shut this connection object down, you don't want
         * to reset its creation date to now, so go to the next one
         */
 	continue;	
      }
      /*
       * recompute select() timeout to maximize blocking time without
       * preventing timeout checking
       */
      config_info->select_timeout.tv_sec = 
			min(config_info->select_timeout.tv_sec,
			server_array[client_listen_counter]->time_to_close -
			(current_time.tv_sec  - 
			server_array[client_listen_counter]->creation_time));
      /*
       * this wasn't a shutdown case, so check to see if there's activity
       * on the fd; if so, then reset the creation time field to now 
       */
      if (FD_ISSET(server_array[client_listen_counter]->client_listen_fd, 
	           readable) ||
	  FD_ISSET(server_array[client_listen_counter]->client_listen_fd, 
		   writable))
	server_array[client_listen_counter]->creation_time = 
		current_time.tv_sec;
    }
  } 

  /*
   * last of all the pm connection fds
   */
  for (pm_conn_counter = 0; 
       pm_conn_counter < config_info->num_pm_conns;
       pm_conn_counter++)
  {
    if (pm_conn_array[pm_conn_counter] != NULL)
    {
      /*
       * do the shutdown time computation
       */
      if ((current_time.tv_sec 
	  - pm_conn_array[pm_conn_counter]->creation_time) 
	  > pm_conn_array[pm_conn_counter]->time_to_close)
      {
	/*
         * shut this connection down just like the others 
         */
	FD_CLR(pm_conn_array[pm_conn_counter]->fd, rinit);
	FD_CLR(pm_conn_array[pm_conn_counter]->fd, winit);
 	close(pm_conn_array[pm_conn_counter]->fd);
        free(pm_conn_array[pm_conn_counter]);
        pm_conn_array[pm_conn_counter] = NULL;
	(*nfds_ready)--;	/* XXX */
	/*
	 * if you just shut this connection object down, you don't want
         * to reset its creation date to now, so go to the next one
         */
 	continue;	
      }
      /*
       * recompute select() timeout to maximize blocking time without
       * preventing timeout checking
       */
      config_info->select_timeout.tv_sec = 
			min(config_info->select_timeout.tv_sec,
			pm_conn_array[pm_conn_counter]->time_to_close -
			(current_time.tv_sec  - 
			pm_conn_array[pm_conn_counter]->creation_time));
      /*
       * this wasn't a shutdown case, so check to see if there's activity
       * on the fd; if so, then reset the creation time field to now 
       */
      if (FD_ISSET(pm_conn_array[pm_conn_counter]->fd, readable) || 
	  FD_ISSET(pm_conn_array[pm_conn_counter]->fd, writable)) 
	pm_conn_array[pm_conn_counter]->creation_time = current_time.tv_sec;
    }
  }
}


int
doHandleConfigFile (
    struct config * config_info)
{
  FILE *			stream;
  char				line[128];
  int				num_chars = 120;
  int				line_number = 0;

  if (!config_info->config_file_path)
      return 1;

  if ((stream = fopen(config_info->config_file_path, "r")) == NULL)
  {
    perror("Could not open config file");
    return 0;
  }

  while (1) 
  { 
    if ((fgets(line, num_chars, stream)) == NULL) 
    {
#ifdef DEBUG
      (void) fprintf(stderr, "Reading config file - got 0 bytes\n");
#endif
      break;
    }

#ifdef DEBUG
      (void) fprintf(stderr, line);
#endif

    line_number++;

    if (!doProcessLine(line, config_info, line_number))
    {
      (void) fprintf(stderr,"Config file format error. Parse failed.\n");
      (void) fprintf(stderr,"\tline: %s\n", line);
      (void) fclose(stream);
      return 0;
    }
  } 

  if (!feof(stream))
  {
      (void) fprintf(stderr, "Error parsing config file; not at eof\n");
      (void) fclose(stream);
      return 0;
  }

  if (printConfigVerify)
      (void) fprintf(stderr, "%d rules read\n", config_info->rule_count);

  (void) fclose(stream);
  return 1;
}

void 
doWriteLogEntry(
    char 		* source,
    char		* destination,
    int			event,
    int			rule_number,
    struct config 	* config_info)
{
  FILE 			* stream;
  struct timezone 	current_zone;
  struct timeval 	current_time;	
  char			* time_stamp;
  int			time_length;

  /*
   * if no logfile, then return without action
   */
  if (!config_info->log_file_path)
    return;

  /*
   * The xfwp audit/logging spec says to always log CLIENT_REJECT_CONFIG
   * events but other events should only be logged if log_level is
   * > 0
   */
  if (event != CLIENT_REJECT_CONFIG && !config_info->log_level)
    return;

  if ((stream = fopen(config_info->log_file_path, "a")) == NULL)
  {
    (void) fprintf(stderr, 
		   "Failed to open log file '%s'\n",
	    	   config_info->log_file_path);
    return;
  }

  /*
   * generate time stamp for this event
   */ 
  gettimeofday(&current_time, &current_zone); 
  time_stamp = ctime((time_t *) &current_time.tv_sec); 
  time_length = strlen(time_stamp);

  /*
   * eliminate newline character in time stamp
   */
  *(&time_stamp[time_length - 1]) = (char) 0;

  (void) fprintf (stream, "%s %2d  %s  %s  %2d\n",
		  time_stamp,
		  event,
		  (source)      ? source      : "",
		  (destination) ? destination : "",
		  rule_number);
 
  (void) fclose(stream); 
}


void 
doCopyFromTo(
    int fd_from, 
    int fd_to, 
    fd_set * rinit,
    fd_set * winit)
{
  int ncopy;
	
  if (client_conn_array[fd_from]->wbytes < RWBUFFER_SIZE)
  {
    /*
     * choose to write either how much you have (from->rbytes),
     * or how much you can hold (to->wbytes), whichever is
     * smaller 
     */
    ncopy = min(client_conn_array[fd_from]->rbytes,
	        RWBUFFER_SIZE - client_conn_array[fd_to]->wbytes);
    /*
     * index into existing number bytes into the write buffer
     * to get the start point for copying 
     */
    bcopy(client_conn_array[fd_from]->readbuf,
	  client_conn_array[fd_to]->writebuf + 
	  client_conn_array[fd_to]->wbytes, ncopy);
    /*
     * Then up the to->wbytes counter
     */
    client_conn_array[fd_to]->wbytes += ncopy;
    /*
     * something has to be done here with the select mask!!
     */
    FD_SET(fd_to, winit); 
    if (ncopy == client_conn_array[fd_from]->rbytes)
      client_conn_array[fd_from]->rbytes = 0;
    else
    {
      bcopy(client_conn_array[fd_from]->readbuf + ncopy,
	    client_conn_array[fd_from]->readbuf,
	    client_conn_array[fd_from]->rbytes - ncopy);
      client_conn_array[fd_from]->rbytes -= ncopy;
    }
    /*
     * and here
     */
    FD_SET(fd_to, rinit);
  }
  /*
   * If there's no room in the fd_to write buffer, do nothing 
   * this iteration (keep iterating on select() until something
   * gets written from this fd)
   */
  return;
}


int 
doCheckServerList(
    char 		* server_address, 
    char 		** listen_port_string,
    int 		num_servers)
{
  /*
   * this routine checks the server_address (provided by XFindProxy
   * and forwarded through the PM to the FWP) against the list of
   * servers to which connections have already been established;
   * it does no format type checking or conversions! (i.e., network-id 
   * vs. hostname representations); if the string received is not an 
   * exact match to one in the list, FWP will open a new connection
   * to the specified server, even though one may already exist under
   * a different name-format; all this is in a separate routine in
   * case we want to check the various formats in the future
   */
  int list_counter;

  for (list_counter = 0; list_counter < num_servers; list_counter++)
  {
    if (server_array[list_counter] != NULL)
    { 
      if (!strcmp(server_array[list_counter]->x_server_hostport, 
	          server_address))
      {
	/*
	 * allocate and return the listen_port_string 
	 */
	if ((*listen_port_string = (char *) malloc
	    (strlen(server_array[list_counter]->listen_port_string) + 1)) 
		== NULL)
	{
    	  (void) fprintf(stderr, "malloc - listen_port_string\n");
	  return FAILURE;
	}
        strcpy(*listen_port_string, 
	       server_array[list_counter]->listen_port_string);
        return SUCCESS; 
      }
    }
  }
  return FAILURE;
}


void 
doProcessInputArgs (
    struct config 	* config_info, 
    int 		argc, 
    char 		* argv[])
{
  int 			arg_counter;
  int			break_flag = 0;

  config_info->num_servers = 0;
  config_info->num_pm_conns = 0;
  config_info->pm_data_timeout = 0;
  config_info->client_listen_timeout = 0;
  config_info->client_data_timeout = 0;
  config_info->log_level = 0;
  config_info->rule_count = config_info->lines_allocated = 0;
  config_info->pm_listen_port = NULL;
  config_info->config_file_data = NULL;
  config_info->config_file_path = NULL;
  config_info->log_file_path = NULL;
  
  /*
   * initialize timeout for three port types; if a timeout for a 
   * particular port type (pmdata, clientlisten, clientdata) is 
   * not specified explicitly, then it assumes the hard-coded
   * default value; initialize other command line options here
   * as well
   */
  for (arg_counter = 1; arg_counter < argc; arg_counter++)
  {
    if (argv[arg_counter][0] == '-')
    {
      if (!strcmp("-pdt", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
	{
          break_flag = 1; 
	  break;
        }
	config_info->pm_data_timeout = atoi(argv[arg_counter + 1]);
      }
      else if (!strcmp("-clt", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
	  break_flag = 1;
          break; 
        }
	config_info->client_listen_timeout = atoi(argv[arg_counter + 1]);
      }
      else if (!strcmp("-cdt", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1;
          break; 
        }
	config_info->client_data_timeout = atoi(argv[arg_counter + 1]);
      }
      else if (!strcmp("-pmport", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1; 
          break;
        }
	if (atoi(argv[arg_counter + 1]) > 65536)
        {
          break_flag = 1;
	  break;     
        }
	config_info->pm_listen_port = Malloc(strlen(argv[arg_counter+1])+1);
	strcpy(config_info->pm_listen_port, argv[arg_counter + 1]);
      }
      else if (!strcmp("-max_pm_conns", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1; 
          break;
        }
	config_info->num_pm_conns = atoi(argv[arg_counter + 1]);
      }
      else if (!strcmp("-max_server_conns", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1; 
          break;
        }
	config_info->num_servers = atoi(argv[arg_counter + 1]);
      }
      else if (!strcmp("-config", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1;
          break; 
        }
        config_info->config_file_path = Malloc(strlen(argv[arg_counter+1])+1);
	strcpy(config_info->config_file_path, argv[arg_counter + 1]);
      }
      else if (!strcmp("-verify", argv[arg_counter]))
      {
	  printConfigVerify = TRUE;
      }
      else if (!strcmp("-logfile", argv[arg_counter]))
      {
        if (arg_counter + 1 == argc)
        {
          break_flag = 1;
          break;
        }
        config_info->log_file_path = Malloc(strlen(argv[arg_counter+1])+1);
        strcpy(config_info->log_file_path, argv[arg_counter + 1]);
      }
      else if (!strcmp("-loglevel", argv[arg_counter]))
      {
        if ((arg_counter + 1 == argc) || (atoi(argv[arg_counter + 1]) > 1))
        {
          break_flag = 1;
          break; 
        }
	config_info->log_level = atoi(argv[arg_counter + 1]);
      }
      else
      {
        (void) fprintf(stderr, "Unrecognized command line argument\n");
	Usage();
      }
    }
  } 
  if (break_flag)
      Usage();

  /*
   * Create space for the global connection arrays
   */
#ifdef XNO_SYSCONF      /* should only be on FreeBSD 1.x and NetBSD 0.x */
#undef _SC_OPEN_MAX
#endif
#ifdef _SC_OPEN_MAX
    config_info->num_client_conns = sysconf(_SC_OPEN_MAX) - 1;
#else
#ifdef hpux
    config_info->num_client_conns = _NFILE - 1;
#else
    config_info->num_client_conns = getdtablesize() - 1;
#endif
#endif

  client_conn_array = (struct client_conn_buf **) 
     malloc (config_info->num_client_conns * sizeof (struct client_conn_buf *));
  if (!client_conn_array)
  {
    (void) fprintf (stderr, "malloc - client connection array\n");
    exit (1);
  }

  if (!config_info->num_pm_conns)
    config_info->num_pm_conns = MAX_PM_CONNS;
  pm_conn_array = (struct pm_conn_buf **) 
     malloc (config_info->num_client_conns * sizeof (struct pm_conn_buf *));
  if (!pm_conn_array) 
  {
    (void) fprintf (stderr, "malloc - PM connection array\n");
    exit (1);
  }

  if (!config_info->num_servers)
    config_info->num_servers = MAX_SERVERS;
  server_array = (struct server_list **)
    malloc (config_info->num_servers * sizeof (struct server_list *));
  if (!server_array)
  {
    (void) fprintf (stderr, "malloc - server listen array\n");
    exit (1);
  }

  /*
   * check timeout values; if still zero then apply defaults
   */
  if (config_info->pm_data_timeout <= 0)
    config_info->pm_data_timeout = PM_DATA_TIMEOUT_DEFAULT;
  if (config_info->client_listen_timeout <= 0)
    config_info->client_listen_timeout = CLIENT_LISTEN_TIMEOUT_DEFAULT;
  if (config_info->client_data_timeout <= 0)
    config_info->client_data_timeout = CLIENT_DATA_TIMEOUT_DEFAULT;
  if (config_info->pm_listen_port == NULL)
  {
    config_info->pm_listen_port = Malloc(strlen(PM_LISTEN_PORT) + 1);
    strcpy(config_info->pm_listen_port, PM_LISTEN_PORT);
  }
}


int
doInitDataStructs(
    struct config * config_info, 
    struct ICE_setup_info * pm_conn_setup)
{
  int i;

  /*
   * Initialize select() timeout; start with a high value, which will
   * be overridden by the minimum timeout value of all fd's taken
   * together;this heuristic allows us to block inside select()
   * as much as possible (avoiding CPU spin cycles), as well as
   * to periodically check timeouts on open ports and thereby recover
   * them
   */
  config_info->select_timeout.tv_usec = 0;
  config_info->select_timeout.tv_sec = 180000; 

  /*
   * NULL the connection arrays
   */
  for (i = 0; i < config_info->num_client_conns; i++)
    client_conn_array[i] = NULL;
  for (i = 0; i < config_info->num_pm_conns; i++)
    pm_conn_array[i] = NULL;
  for (i = 0; i < config_info->num_servers; i++)
    server_array[i] = NULL;

  /*
   * init ICE connection setup data
   */
  pm_conn_setup->opcode = 0;
  pm_conn_setup->versionCount = 1;
  pm_conn_setup->PMVersions->major_version = 1;
  pm_conn_setup->PMVersions->minor_version = 0;
  pm_conn_setup->PMVersions->process_msg_proc = 
			(IcePaProcessMsgProc) FWPprocessMessages;
  /*
   * Register for protocol setup
   */
  if ((pm_conn_setup->opcode = IceRegisterForProtocolReply(
				PM_PROTOCOL_NAME,
				"XC",
				"1.0",
				pm_conn_setup->versionCount,
				pm_conn_setup->PMVersions,
				0, /* authcount */
				NULL, /* authname */
				NULL, /* authprocs */
				FWPHostBasedAuthProc, /* force non-auth */
				FWPprotocolSetupProc,
				NULL, /* protocol activate proc */
				NULL /* IceIOErrorProc */ )) < 0)
  {
    /*
     * Log this message?
     */
    (void) fprintf(stderr, "Could not register PROXY_MANAGEMENT ICE protocol");
    return FAILURE;
  }

  global_data.major_opcode = pm_conn_setup->opcode;

  return SUCCESS;
}

