/*
 * "$Id: dirsvc.c,v 1.39.2.2 2005/07/27 21:58:45 jlovell Exp $"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ProcessBrowseData() - Process new browse data.
 *   SendBrowseList()    - Send new browsing information as necessary.
 *   SendCUPSBrowse()    - Send new browsing information using the CUPS protocol.
 *   StartBrowsing()     - Start sending and receiving broadcast information.
 *   StartPolling()      - Start polling servers as needed.
 *   StopBrowsing()      - Stop sending and receiving broadcast information.
 *   StopPolling()       - Stop polling servers as needed.
 *   UpdateCUPSBrowse()  - Update the browse lists using the CUPS protocol.
 *   UpdatePolling()     - Read status messages from the poll daemons.
 *   RegReportCallback() - Empty SLPRegReport.
 *   SendSLPBrowse()     - Register the specified printer with SLP.
 *   SLPDeregPrinter()   - SLPDereg() the specified printer
 *   GetSlpAttrVal()     - Get an attribute from an SLP registration.
 *   AttrCallback()      - SLP attribute callback 
 *   SrvUrlCallback()    - SLP service url callback
 *   UpdateSLPBrowse()   - Get browsing information via SLP.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>


#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#  include <nameser.h>
#  include <nameser.h>

#  include <CoreFoundation/CoreFoundation.h>
#  include <SystemConfiguration/SystemConfiguration.h>

#  ifdef HAVE_NOTIFY_H
#    include <notify.h>
#  endif /* HAVE_NOTIFY_H */
#endif /* HAVE_DNSSD */

#  include <CoreFoundation/CoreFoundation.h>

/*
 * Local functions...
 */

static printer_t* ProcessBrowseData(const char *uri, cups_ptype_t type,
		                  ipp_pstate_t state, const char *location,
				  const char *info, const char *make_model,
				  int protocol);
static void	SendCUPSBrowse(printer_t *p);

#ifdef HAVE_LIBSLP
static void	SendSLPBrowse(printer_t *p);
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD

/*
 * For IPP register using a subtype of 'cups' so that shared printer browsing
 * only finds other cups servers (not all IPP based printers).
 */
static char		  dnssdIPPRegType[] = "_ipp._tcp,_cups";

static int 		  dnssdBrowsing = 0;
static SCDynamicStoreRef  mSysConfigStore = NULL;

static void dnssdRegisterPrinter(printer_t *p);
static void dnssdDeregisterPrinter(printer_t *p);

static void dnssdRegisterCallback(DNSServiceRef sdRef, DNSServiceFlags flags, 
				 DNSServiceErrorType errorCode, const char *name,
				 const char *regtype, const char *domain, void *context);
static void dnssdBrowseCallback(DNSServiceRef sdRef, DNSServiceFlags flags, 
				uint32_t interfaceIndex, DNSServiceErrorType errorCode, 
				const char *service_name, const char *regtype, 
				const char *replyDomain, void *context);
static void dnssdResolveCallback(DNSServiceRef sdRef, DNSServiceFlags flags,
				uint32_t interfaceIndex, DNSServiceErrorType errorCode,
				const char *fullname, const char *host_target, uint16_t port,
				uint16_t txt_len, const char *txtRecord, void *context);
static void dnssdQueryRecordCallback(DNSServiceRef sdRef, DNSServiceFlags flags, 
				uint32_t interfaceIndex, DNSServiceErrorType errorCode,
				const char *fullname, uint16_t rrtype, uint16_t rrclass, 
				uint16_t rdlen, const void *rdata, uint32_t ttl, void *context);

static char *dnssdBuildTxtRecord(int *txt_len, printer_t *p);
static char *dnssdPackTxtRecord(int *txt_len, char *keyvalue[][2], int count);
static int  dnssdFindAttr(const unsigned char *txtRecord, int txt_len, const char *key, char **value);

#endif /* HAVE_DNSSD */


/*
 * 'ProcessBrowseData()' - Process new browse data.
 */

static printer_t *				/* O - Printer in list */
ProcessBrowseData(const char   *uri,		/* I - URI of printer/class */
                  cups_ptype_t type,		/* I - Printer type */
		  ipp_pstate_t state,		/* I - Printer state */
                  const char   *location,	/* I - Printer location */
		  const char   *info,		/* I - Printer information */
                  const char   *make_model,	/* I - Printer make and model */
                  int		protocol)	/* I - Browse protocol */
{
  int		i;			/* Looping var */
  int		update;			/* Update printer attributes? */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*hptr,			/* Pointer into hostname */
		*sptr;			/* Pointer into ServerName */
  char		local_make_model[IPP_MAX_NAME];
					/* Local make and model */
  printer_t	*p,			/* Printer information */
		*pclass,		/* Printer class */
		*first,			/* First printer in class */
		*next;			/* Next printer in list */
  int		offset,			/* Offset of name */
		len;			/* Length of name */


 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

 /*
  * Determine if the URI contains any illegal characters in it...
  */

  if (strncmp(uri, "ipp://", 6) != 0 ||
      !host[0] ||
      (strncmp(resource, "/printers/", 10) != 0 &&
       strncmp(resource, "/classes/", 9) != 0))
  {
    LogMessage(L_ERROR, "ProcessBrowseData: Bad printer URI in browse data: %s",
               uri);
    return NULL;
  }

  if (strchr(resource, '?') != NULL ||
      (strncmp(resource, "/printers/", 10) == 0 &&
       strchr(resource + 10, '/') != NULL) ||
      (strncmp(resource, "/classes/", 9) == 0 &&
       strchr(resource + 9, '/') != NULL))
  {
    LogMessage(L_ERROR, "ProcessBrowseData: Bad resource in browse data: %s",
               resource);
    return NULL;
  }
    
 /*
  * OK, this isn't a local printer; see if we already have it listed in
  * the Printers list, and add it if not...
  */

  type   |= CUPS_PRINTER_REMOTE;
  update = 0;
  hptr   = strchr(host, '.');
  sptr   = strchr(ServerName, '.');

  if (sptr != NULL && hptr != NULL)
  {
   /*
    * Strip the common domain name components...
    */

    while (hptr != NULL)
    {
      if (strcasecmp(hptr, sptr) == 0)
      {
        *hptr = '\0';
	break;
      }
      else
        hptr = strchr(hptr + 1, '.');
    }
  }

  if (type & CUPS_PRINTER_CLASS)
  {
   /*
    * Remote destination is a class...
    */

    if (strncmp(resource, "/classes/", 9) == 0)
      snprintf(name, sizeof(name), "%s@%s", resource + 9, host);
    else
      return NULL;

    if ((p = FindClass(name)) == NULL && BrowseShortNames)
    {
      if ((p = FindClass(resource + 9)) != NULL)
      {
        if (p->hostname && strcasecmp(p->hostname, host) != 0)
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other class and then find a class using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
            SetStringf(&p->name, "%s@%s", p->name, p->hostname);
	    SetPrinterAttrs(p);
	    SortPrinters();
	  }

          p = NULL;
	}
	else if (!p->hostname)
	{
          SetString(&p->hostname, host);
	  SetString(&p->uri, uri);
	  SetString(&p->device_uri, uri);
          update = 1;
        }
      }
      else
        strlcpy(name, resource + 9, sizeof(name));
    }
    else if (p != NULL && !p->hostname)
    {
      SetString(&p->hostname, host);
      SetString(&p->uri, uri);
      SetString(&p->device_uri, uri);
      update = 1;
    }

    if (p == NULL && !(type & CUPS_PRINTER_DELETE))
    {
     /*
      * Class doesn't exist; add it...
      */

      p = AddClass(name, 1);

      LogMessage(L_INFO, "Added remote class \"%s\"...", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type      = type & ~CUPS_PRINTER_REJECTING;
      p->accepting = 1;
      SetString(&p->uri, uri);
      SetString(&p->device_uri, uri);
      SetString(&p->hostname, host);

      update = 1;
    }
  }
  else
  {
   /*
    * Remote destination is a printer...
    */

    if (strncmp(resource, "/printers/", 10) == 0)
      snprintf(name, sizeof(name), "%s@%s", resource + 10, host);
    else
      return NULL;

    if ((p = FindPrinter(name)) == NULL && BrowseShortNames)
    {
      if ((p = FindPrinter(resource + 10)) != NULL)
      {
        if (p->hostname && strcasecmp(p->hostname, host) != 0)
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other printer and then find a printer using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
	    SetStringf(&p->name, "%s@%s", p->name, p->hostname);
	    SetPrinterAttrs(p);
	    SortPrinters();
	  }

          p = NULL;
	}
	else if (!p->hostname)
	{
          SetString(&p->hostname, host);
	  SetString(&p->uri, uri);
	  SetString(&p->device_uri, uri);
          update = 1;
        }
      }
      else
        strlcpy(name, resource + 10, sizeof(name));
    }
    else if (p != NULL && !p->hostname)
    {
      SetString(&p->hostname, host);
      SetString(&p->uri, uri);
      SetString(&p->device_uri, uri);
      update = 1;
    }

    if (p == NULL && !(type & CUPS_PRINTER_DELETE))
    {
     /*
      * Printer doesn't exist; add it...
      */

      p = AddPrinter(name, 1);

      LogMessage(L_INFO, "Added remote printer \"%s\"...", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type      = type & ~CUPS_PRINTER_REJECTING;
      p->accepting = 1;
      SetString(&p->hostname, host);
      SetString(&p->uri, uri);
      SetString(&p->device_uri, uri);

      update = 1;
    }
  }

 /*
  * If we didn't find or create a printer we're done...
  */

  if (p == NULL)
    return NULL;

 /*
  * Update the state...
  */

  if (p->state != state)
    update       = 1;

  p->state       = state;
  p->browse_time = time(NULL);
  p->browse_protocol |= protocol;

  if (type & CUPS_PRINTER_REJECTING)
  {
    type &= ~CUPS_PRINTER_REJECTING;

    if (p->accepting)
    {
      update       = 1;
      p->accepting = 0;
    }
  }
  else if (!p->accepting)
  {
    update       = 1;
    p->accepting = 1;
  }

  if (p->type != type)
  {
    p->type = type;
    update  = 1;
  }

  if (location && (!p->location || strcmp(p->location, location)))
  {
    SetString(&p->location, location);
    update = 1;
  }

  if (info && (!p->info || strcmp(p->info, info)))
  {
    SetString(&p->info, info);
    update = 1;
  }

  if (!make_model || !make_model[0])
  {
    if (type & CUPS_PRINTER_CLASS)
      snprintf(local_make_model, sizeof(local_make_model),
               "Remote Class on %s", host);
    else
      snprintf(local_make_model, sizeof(local_make_model),
               "Remote Printer on %s", host);
  }
  else
#ifdef __APPLE__
    /* Changing make_model makes it difficult to localize so we don't do it */
    strlcpy(local_make_model, make_model, sizeof(local_make_model));
#else
    snprintf(local_make_model, sizeof(local_make_model),
             "%s on %s", make_model, host);
#endif        /* __APPLE__ */

  if (!p->make_model || strcmp(p->make_model, local_make_model))
  {
    SetString(&p->make_model, local_make_model);
    update = 1;
  }

  if (type & CUPS_PRINTER_DELETE)
  {
    DeletePrinter(p, 1);
    UpdateImplicitClasses();
  }
  else if (update)
  {
    SetPrinterAttrs(p);
    UpdateImplicitClasses();
  }

 /*
  * See if we have a default printer...  If not, make the first printer the
  * default.
  */

  if (DefaultPrinter == NULL && Printers != NULL)
  {
#ifdef __APPLE__
   /*
    * Don't pick a remote printer as the default...
    */

    for (p = Printers; p != NULL; p = p->next)
      if (!(p->type & CUPS_PRINTER_REMOTE))
      {
	DefaultPrinter = p;
	WritePrintcap();
	break;
      }
#else
    DefaultPrinter = Printers;

    WritePrintcap();
#endif        /* __APPLE__ */
  }

 /*
  * Do auto-classing if needed...
  */

  if (ImplicitClasses)
  {
   /*
    * Loop through all available printers and create classes as needed...
    */

    for (p = Printers, len = 0, offset = 0, first = NULL;
         p != NULL;
	 p = next)
    {
     /*
      * Get next printer in list...
      */

      next = p->next;

     /*
      * Skip implicit classes...
      */

      if (p->type & CUPS_PRINTER_IMPLICIT)
      {
        len = 0;
        continue;
      }

     /*
      * If len == 0, get the length of this printer name up to the "@"
      * sign (if any).
      */

      if (len > 0 &&
	  strncasecmp(p->name, name + offset, len) == 0 &&
	  (p->name[len] == '\0' || p->name[len] == '@'))
      {
       /*
	* We have more than one printer with the same name; see if
	* we have a class, and if this printer is a member...
	*/

        if ((pclass = FindDest(name)) == NULL)
	{
	 /*
	  * Need to add the class...
	  */

	  pclass = AddPrinter(name, 1);
	  pclass->type      |= CUPS_PRINTER_IMPLICIT;
	  pclass->accepting = 1;
	  pclass->state     = IPP_PRINTER_IDLE;

          SetString(&pclass->location, p->location);
          SetString(&pclass->info, p->info);

          SetPrinterAttrs(pclass);

          LogMessage(L_INFO, "Added implicit class \"%s\"...", name);
	}

        if (first != NULL)
	{
          for (i = 0; i < pclass->num_printers; i ++)
	    if (pclass->printers[i] == first)
	      break;

          if (i >= pclass->num_printers)
	    AddPrinterToClass(pclass, first);

	  first = NULL;
	}

        for (i = 0; i < pclass->num_printers; i ++)
	  if (pclass->printers[i] == p)
	    break;

        if (i >= pclass->num_printers)
	  AddPrinterToClass(pclass, p);
      }
      else
      {
       /*
        * First time around; just get name length and mark it as first
	* in the list...
	*/

	if ((hptr = strchr(p->name, '@')) != NULL)
	  len = hptr - p->name;
	else
	  len = strlen(p->name);

        strncpy(name, p->name, len);
	name[len] = '\0';
	offset    = 0;

	if ((pclass = FindDest(name)) != NULL &&
	    !(pclass->type & CUPS_PRINTER_IMPLICIT))
	{
	 /*
	  * Can't use same name as a local printer; add "Any" to the
	  * front of the name, unless we have explicitly disabled
	  * the "ImplicitAnyClasses"...
	  */

          if (ImplicitAnyClasses && len < (sizeof(name) - 4))
	  {
	   /*
	    * Add "Any" to the class name...
	    */

            strcpy(name, "Any");
            strncpy(name + 3, p->name, len);
	    name[len + 3] = '\0';
	    offset        = 3;
	  }
	  else
	  {
	   /*
	    * Don't create an implicit class if we have a local printer
	    * with the same name...
	    */

	    len = 0;
	    continue;
	  }
	}

	first = p;
      }
    }
  }
  return p;
}


/*
 * 'SendBrowseList()' - Send new browsing information as necessary.
 */

void
SendBrowseList(void)
{
  int			count,		/* Number of dests to update */
			num_printers;	/* Number of printers */
  printer_t		*p,		/* Current printer */
			*next;		/* Next printer */
  time_t		ut,		/* Minimum update time */
			to;		/* Timeout time */


  if (!Browsing || !BrowseLocalProtocols)
    return;

 /*
  * Compute the update and timeout times...
  */

  ut = time(NULL) - BrowseInterval;
  to = time(NULL) - BrowseTimeout;

 /*
  * Figure out how many printers need an update...
  */

  if (BrowseInterval > 0)
  {
    int	max_count;			/* Maximum number to update */


   /*
    * Throttle the number of printers we'll be updating this time
    * around based on the number of queues that need updating and
    * the maximum number of queues to update each second...
    */

    max_count = 2 * NumPrinters / BrowseInterval + 1;

    for (count = 0, p = Printers; count < max_count && p != NULL; p = p->next)
      if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
          p->browse_time < ut && p->shared)
        count ++;

   /*
    * Loop through all of the printers and send local updates as needed...
    */

    for (p = BrowseNext; count > 0; p = p->next)
    {
     /*
      * Check for wraparound...
      */

      if (!p)
        p = Printers;

      if (!p)
        break;
      else if (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT))
        continue;
      else if (p->browse_time < ut && p->shared)
      {
       /*
	* Need to send an update...
	*/

	count --;

	p->browse_time = time(NULL);

	if (BrowseLocalProtocols & BROWSE_CUPS)
          SendCUPSBrowse(p);

#ifdef HAVE_LIBSLP
	if (BrowseLocalProtocols & BROWSE_SLP)
          SendSLPBrowse(p);
#endif /* HAVE_LIBSLP */
      }
    }

   /*
    * Save where we left off so that all printers get updated...
    */

    BrowseNext = p;
  }

 /*
  * Loop through all of the printers and send local updates as needed...
  */

  for (p = Printers; p != NULL; p = next)
  {
    next = p->next;

   /*
    * If this is a remote queue, see if it needs to be timed out...
    */

    if (p->type & CUPS_PRINTER_REMOTE)
    {
      if (p->browse_time < to && p->browse_protocol == BROWSE_CUPS)
      {
        LogMessage(L_INFO, "Remote destination \"%s\" has timed out; deleting it...",
	           p->name);

       /*
	* Deleting a printer that is part of an implicit class can also cause 
	* a following class to be deleted (which our 'next' may be pointing at). 
	* In this case reset 'next' to the head of the list...
	*/
  
	num_printers = NumPrinters;
  
        DeletePrinter(p, 1);

	if (NumPrinters != num_printers - 1)
	  next = Printers;
      }
    }
  }
}


/*
 * 'SendCUPSBrowse()' - Send new browsing information using the CUPS protocol.
 */

static void
SendCUPSBrowse(printer_t *p)		/* I - Printer to send */
{
  int			i;		/* Looping var */
  cups_ptype_t		type;		/* Printer type */
  dirsvc_addr_t		*b;		/* Browse address */
  int			bytes;		/* Length of packet */
  char			packet[1453];	/* Browse data packet */
  cups_netif_t		*iface;		/* Network interface */
  char			*make_model,	/* Make and model */
  			*location,	/* Location */
  			*info,		/* Printer information */
  			*ptr;		/* Temp pointer */


 /*
  * Figure out the printer type value...
  */

  type = p->type | CUPS_PRINTER_REMOTE;

  if (!p->accepting)
    type |= CUPS_PRINTER_REJECTING;

 /*
  * Remove any double quotes from the strings we quote in the packet...
  */

  make_model = p->make_model;
  if (make_model && strchr(make_model, '\"'))
    if ((make_model = strdup(make_model)))
      while ((ptr = strchr(make_model, '\"')))
        *ptr = '\'';

  location = p->location;
  if (location && strchr(location, '\"'))
    if ((location = strdup(location)))
      while ((ptr = strchr(location, '\"')))
        *ptr = '\'';

  info = p->info;
  if (info && strchr(info, '\"'))
    if ((info = strdup(info)))
      while ((ptr = strchr(info, '\"')))
        *ptr = '\'';

 /*
  * Send a packet to each browse address...
  */

  for (i = NumBrowsers, b = Browsers; i > 0; i --, b ++)
    if (b->iface[0])
    {
     /*
      * Send the browse packet to one or more interfaces...
      */

      if (strcmp(b->iface, "*") == 0)
      {
       /*
        * Send to all local interfaces...
	*/

        NetIFUpdate(FALSE);

        for (iface = NetIFList; iface != NULL; iface = iface->next)
	{
	 /*
	  * Only send to local interfaces...
	  */

	  if (!iface->is_local || !iface->port)
	    continue;

	  snprintf(packet, sizeof(packet), "%x %x ipp://%s:%d/%s/%s \"%s\" \"%s\" \"%s\"\n",
        	   type, p->state, iface->hostname, iface->port,
		   (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers",
		   p->name, location ? location : "",
		   info ? info : "",
		   make_model ? make_model : "Unknown");

	  bytes = strlen(packet);

	  LogMessage(L_DEBUG2, "SendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	     iface->name, packet);

          iface->broadcast.sin_port = htons(BrowsePort);

	  sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(iface->broadcast),
		 sizeof(struct sockaddr_in));
        }
      }
      else if ((iface = NetIFFind(b->iface)) != NULL)
      {
       /*
        * Send to the named interface...
	*/

	if (!iface->port)
	  continue;

	snprintf(packet, sizeof(packet), "%x %x ipp://%s:%d/%s/%s \"%s\" \"%s\" \"%s\"\n",
        	 type, p->state, iface->hostname, iface->port,
		 (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers",
		 p->name, location ? location : "",
		 info ? info : "",
		 make_model ? make_model : "Unknown");

	bytes = strlen(packet);

	LogMessage(L_DEBUG2, "SendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	   iface->name, packet);

        iface->broadcast.sin_port = htons(BrowsePort);

	sendto(BrowseSocket, packet, bytes, 0,
	       (struct sockaddr *)&(iface->broadcast),
	       sizeof(struct sockaddr_in));
      }
    }
    else
    {
     /*
      * Send the browse packet to the indicated address using
      * the default server name...
      */

      snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"\n",
       	       type, p->state, p->uri,
	       location ? location : "",
	       info ? info : "",
	       make_model ? make_model : "Unknown");

      bytes = strlen(packet);
      LogMessage(L_DEBUG2, "SendBrowseList: (%d bytes to %x) %s", bytes,
        	 (unsigned)ntohl(b->to.sin_addr.s_addr), packet);

      if (sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(b->to), sizeof(struct sockaddr_in)) <= 0)
      {
       /*
        * Unable to send browse packet, so remove this address from the
	* list...
	*/

	LogMessage(L_ERROR, "SendBrowseList: sendto failed for browser %d - %s.",
	           b - Browsers + 1, strerror(errno));

        if (i > 1)
	  memmove(b, b + 1, (i - 1) * sizeof(dirsvc_addr_t));

	b --;
	NumBrowsers --;
      }
    }

 /*
  * If we made copies of strings be sure to free them...
  */

  if (make_model != p->make_model)
    free(make_model);

  if (location != p->location)
    free(location);

  if (info != p->info)
    free(info);
}


/*
 * 'StartBrowsing()' - Start sending and receiving broadcast information.
 */

void
StartBrowsing(void)
{
  int			val;	/* Socket option value */
  struct sockaddr_in	addr;	/* Broadcast address */
  printer_t		*p;	/* Pointer to current printer/class */


  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS)
  {
   /*
    * Create the broadcast socket...
    */

    if ((BrowseSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to create broadcast socket - %s.",
        	 strerror(errno));
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols   &= ~BROWSE_CUPS;
      return;
    }

   /*
    * Set the "broadcast" flag...
    */

    val = 1;
    if (setsockopt(BrowseSocket, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to set broadcast mode - %s.",
        	 strerror(errno));

#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      BrowseSocket    = -1;
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols   &= ~BROWSE_CUPS;
      return;
    }

   /*
    * Bind the socket to browse port...
    */

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(BrowsePort);

    if (bind(BrowseSocket, (struct sockaddr *)&addr, sizeof(addr)))
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to bind broadcast socket - %s.",
        	 strerror(errno));

#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      BrowseSocket    = -1;
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols   &= ~BROWSE_CUPS;
      return;
    }

   /*
    * Close the socket on exec...
    */

    fcntl(BrowseSocket, F_SETFD, fcntl(BrowseSocket, F_GETFD) | FD_CLOEXEC);

   /*
    * Finally, add the socket to the input selection set...
    */

    LogMessage(L_DEBUG2, "StartBrowsing: Adding fd %d to InputSet...",
               BrowseSocket);

    FD_SET(BrowseSocket, InputSet);
  }
  else
    BrowseSocket = -1;

#ifdef HAVE_LIBSLP
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP)
  {
   /* 
    * Open SLP handle...
    */

    if (SLPOpen("en", SLP_FALSE, &BrowseSLPHandle) != SLP_OK)
    {
      LogMessage(L_ERROR, "Unable to open an SLP handle; disabling SLP browsing!");
      BrowseLocalProtocols &= ~BROWSE_SLP;
      BrowseRemoteProtocols   &= ~BROWSE_SLP;
    }

    BrowseSLPRefresh = 0;
  }
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
  if (BrowseRemoteProtocols & BROWSE_DNSSD)
  {
    DNSServiceErrorType	se;	/* dnssd errors */

    dnssdBrowsing = 1;

   /*
    * Start browsing for services
    */

    if ((se = DNSServiceBrowse(&BrowseDNSSDRef, 0, 0, dnssdIPPRegType,
				NULL, dnssdBrowseCallback, NULL)) == 0)
    {
      BrowseDNSSDfd = DNSServiceRefSockFD(BrowseDNSSDRef);

      LogMessage(L_DEBUG2, "StartBrowsing: Adding fd %d to InputSet...",
               BrowseDNSSDfd);

      FD_SET(BrowseDNSSDfd, InputSet);
    }
  }
#endif /* HAVE_DNSSD */

 /*
  * Register the individual printers
  */

  for (p = Printers; p != NULL; p = p->next)
    BrowseRegisterPrinter(p);
}


/*
 * 'StartPolling()' - Start polling servers as needed.
 */

void
StartPolling(void)
{
  int		i;		/* Looping var */
  dirsvc_poll_t	*poll;		/* Current polling server */
  int		pid;		/* New process ID */
  char		sport[10];	/* Server port */
  char		bport[10];	/* Browser port */
  char		interval[10];	/* Poll interval */
  int		statusfds[2];	/* Status pipe */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* POSIX signal handler */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Don't do anything if we aren't polling...
  */

  if (NumPolled == 0)
  {
    PollPipe = -1;
    return;
  }

 /*
  * Setup string arguments for port and interval options.
  */

  sprintf(bport, "%d", BrowsePort);

  if (BrowseInterval)
    sprintf(interval, "%d", BrowseInterval);
  else
    strcpy(interval, "30");

 /*
  * Create a pipe that receives the status messages from each
  * polling daemon...
  */

  if (cupsdOpenPipe(statusfds))
  {
    LogMessage(L_ERROR, "Unable to create polling status pipes - %s.",
	       strerror(errno));
    PollPipe = -1;
    return;
  }

  PollPipe = statusfds[0];

 /*
  * Run each polling daemon, redirecting stderr to the polling pipe...
  */

  for (i = 0, poll = Polled; i < NumPolled; i ++, poll ++)
  {
    sprintf(sport, "%d", poll->port);

   /*
    * Block signals before forking...
    */

    HoldSignals();

    if ((pid = fork()) == 0)
    {
     /*
      * Child...
      */

      if (getuid() == 0)
      {
       /*
	* Running as root, so change to non-priviledged user...
	*/

	if (setgid(Group))
          exit(errno);

	if (setgroups(1, &Group))
          exit(errno);

	if (setuid(User))
          exit(errno);
      }
      else
      {
       /*
	* Reset group membership to just the main one we belong to.
	*/

	setgroups(1, &Group);
      }

     /*
      * Redirect stdin and stdout to /dev/null, and stderr to the
      * status pipe.  Close all other files.
      */

      close(0);
      open("/dev/null", O_RDONLY);

      close(1);
      open("/dev/null", O_WRONLY);

      close(2);
      dup(statusfds[1]);

     /*
      * Unblock signals before doing the exec...
      */

#ifdef HAVE_SIGSET
      sigset(SIGTERM, SIG_DFL);
      sigset(SIGCHLD, SIG_DFL);
#elif defined(HAVE_SIGACTION)
      memset(&action, 0, sizeof(action));

      sigemptyset(&action.sa_mask);
      action.sa_handler = SIG_DFL;

      sigaction(SIGTERM, &action, NULL);
      sigaction(SIGCHLD, &action, NULL);
#else
      signal(SIGTERM, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_SIGSET */

      ReleaseSignals();

     /*
      * Execute the polling daemon...
      */

      execl(CUPS_SERVERBIN "/daemon/cups-polld", "cups-polld", poll->hostname,
            sport, interval, bport, NULL);
      exit(errno);
    }
    else if (pid < 0)
    {
      LogMessage(L_ERROR, "StartPolling: Unable to fork polling daemon - %s",
                 strerror(errno));
      poll->pid = 0;
      break;
    }
    else
    {
      poll->pid = pid;
      LogMessage(L_DEBUG, "StartPolling: Started polling daemon for %s:%d, pid = %d",
                 poll->hostname, poll->port, pid);
    }

    ReleaseSignals();
  }

  close(statusfds[1]);

 /*
  * Finally, add the pipe to the input selection set...
  */

  LogMessage(L_DEBUG2, "StartPolling: Adding fd %d to InputSet...",
             PollPipe);

  FD_SET(PollPipe, InputSet);
}


/*
 * 'StopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
StopBrowsing(void)
{
  printer_t		*p;	/* Pointer to current printer/class */

  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS)
  {
   /*
    * Close the socket and remove it from the input selection set.
    */

    if (BrowseSocket >= 0)
    {
#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      LogMessage(L_DEBUG2, "StopBrowsing: Removing fd %d from InputSet...",
        	 BrowseSocket);

      FD_CLR(BrowseSocket, InputFds);
      FD_CLR(BrowseSocket, InputSet);
      BrowseSocket = -1;
    }
  }

#ifdef HAVE_LIBSLP
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP)
  {
   /* 
    * Close SLP handle...
    */

    SLPClose(BrowseSLPHandle);
  }
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_DNSSD)
  {
    dnssdBrowsing = 0;

   /*
    * Close the socket to stop browsing
    */

    if (BrowseDNSSDRef)
    {
      LogMessage(L_DEBUG2, "StopBrowsing: Removing fd %d from InputSet...",
        	 BrowseDNSSDfd);

      FD_CLR(BrowseDNSSDfd, InputFds);
      FD_CLR(BrowseDNSSDfd, InputSet);
      DNSServiceRefDeallocate(BrowseDNSSDRef);
      BrowseDNSSDRef = NULL;
      BrowseDNSSDfd = -1;
    }
  }

 /*
  * De-register the individual printers
  */

  for (p = Printers; p != NULL; p = p->next)
    BrowseDeregisterPrinter(p, 1);
#endif /* HAVE_DNSSD */
}


/*
 * 'StopPolling()' - Stop polling servers as needed.
 */

void
StopPolling(void)
{
  int		i;		/* Looping var */
  dirsvc_poll_t	*poll;		/* Current polling server */


  if (PollPipe >= 0)
  {
    close(PollPipe);

    LogMessage(L_DEBUG2, "StopPolling: removing fd %d from InputSet.",
               PollPipe);
    FD_CLR(PollPipe, InputFds);
    FD_CLR(PollPipe, InputSet);

    PollPipe = -1;
  }

  for (i = 0, poll = Polled; i < NumPolled; i ++, poll ++)
    if (poll->pid)
      kill(poll->pid, SIGTERM);
}


/*
 * 'UpdateCUPSBrowse()' - Update the browse lists using the CUPS protocol.
 */

void
UpdateCUPSBrowse(void)
{
  int		i;			/* Looping var */
  int		auth;			/* Authorization status */
  int		len;			/* Length of name string */
  int		bytes;			/* Number of bytes left */
  char		packet[1541],		/* Broadcast packet */
		*pptr;			/* Pointer into packet */
  struct sockaddr_in srcaddr;		/* Source address */
  char		srcname[1024];		/* Source hostname */
  unsigned	address;		/* Source address (host order) */
  struct hostent *srchost;		/* Host entry for source address */
  unsigned	type;			/* Printer type */
  unsigned	state;			/* Printer state */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI],	/* Resource portion of URI */
		info[IPP_MAX_NAME],	/* Information string */
		location[IPP_MAX_NAME],	/* Location string */
		make_model[IPP_MAX_NAME];/* Make and model string */
  int		port;			/* Port portion of URI */
  cups_netif_t	*iface;			/* Network interface */


 /*
  * Read a packet from the browse socket...
  */

  len = sizeof(srcaddr);
  if ((bytes = recvfrom(BrowseSocket, packet, sizeof(packet) - 1, 0, 
                        (struct sockaddr *)&srcaddr, &len)) < 0)
  {
   /*
    * "Connection refused" is returned under Linux if the destination port
    * or address is unreachable from a previous sendto(); check for the
    * error here and ignore it for now...
    */

    if (errno != ECONNREFUSED && errno != EAGAIN)
    {
      LogMessage(L_ERROR, "Browse recv failed - %s.", strerror(errno));
      LogMessage(L_ERROR, "Browsing turned off.");

      StopBrowsing();
      Browsing = 0;
    }

    return;
  }

  packet[bytes] = '\0';


#ifdef __APPLE__
 /*
  * If we're about to sleep ignore incoming browse packets.
  */

  if (Sleeping)
    return;
#endif	/* __APPLE__ */


 /*
  * Figure out where it came from...
  */

  address = ntohl(srcaddr.sin_addr.s_addr);

  if (HostNameLookups)
#ifndef __sgi
    srchost = gethostbyaddr((char *)&(srcaddr.sin_addr), sizeof(struct in_addr),
                            AF_INET);
#else
    srchost = gethostbyaddr(&(srcaddr.sin_addr), sizeof(struct in_addr),
                            AF_INET);
#endif /* !__sgi */
  else
    srchost = NULL;

  if (srchost == NULL)
    sprintf(srcname, "%d.%d.%d.%d", address >> 24, (address >> 16) & 255,
            (address >> 8) & 255, address & 255);
  else
    strlcpy(srcname, srchost->h_name, sizeof(srcname));

  len = strlen(srcname);

 /*
  * Do ACL stuff...
  */

  if (BrowseACL && (BrowseACL->num_allow || BrowseACL->num_deny))
  {
    if (address == 0x7f000001 || strcasecmp(srcname, "localhost") == 0)
    {
     /*
      * Access from localhost (127.0.0.1) is always allowed...
      */

      auth = AUTH_ALLOW;
    }
    else
    {
     /*
      * Do authorization checks on the domain/address...
      */

      switch (BrowseACL->order_type)
      {
        default :
	    auth = AUTH_DENY;	/* anti-compiler-warning-code */
	    break;

	case AUTH_ALLOW : /* Order Deny,Allow */
            auth = AUTH_ALLOW;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = AUTH_DENY;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;
	    break;

	case AUTH_DENY : /* Order Allow,Deny */
            auth = AUTH_DENY;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = AUTH_DENY;
	    break;
      }
    }
  }
  else
    auth = AUTH_ALLOW;

  if (auth == AUTH_DENY)
  {
    LogMessage(L_DEBUG, "UpdateCUPSBrowse: Refused %d bytes from %s", bytes,
               srcname);
    return;
  }

  LogMessage(L_DEBUG2, "UpdateCUPSBrowse: (%d bytes from %s) %s", bytes, srcname,
             packet);

 /*
  * Parse packet...
  */

  if (sscanf(packet, "%x%x%1023s", &type, &state, uri) < 3)
  {
    LogMessage(L_WARN, "UpdateCUPSBrowse: Garbled browse packet - %s",
               packet);
    return;
  }

  strcpy(location, "Location Unknown");
  strcpy(info, "No Information Available");
  make_model[0] = '\0';

  if ((pptr = strchr(packet, '\"')) != NULL)
  {
   /*
    * Have extended information; can't use sscanf for it because not all
    * sscanf's allow empty strings with %[^\"]...
    */

    for (i = 0, pptr ++;
         i < (sizeof(location) - 1) && *pptr && *pptr != '\"';
         i ++, pptr ++)
      location[i] = *pptr;

    if (i)
      location[i] = '\0';

    if (*pptr == '\"')
      pptr ++;

    while (*pptr && isspace(*pptr & 255))
      pptr ++;

    if (*pptr == '\"')
    {
      for (i = 0, pptr ++;
           i < (sizeof(info) - 1) && *pptr && *pptr != '\"';
           i ++, pptr ++)
	info[i] = *pptr;

      if (i)
	info[i] = '\0';

      if (*pptr == '\"')
	pptr ++;

      while (*pptr && isspace(*pptr & 255))
	pptr ++;

      if (*pptr == '\"')
      {
	for (i = 0, pptr ++;
             i < (sizeof(make_model) - 1) && *pptr && *pptr != '\"';
             i ++, pptr ++)
	  make_model[i] = *pptr;

	if (i)
	  make_model[i] = '\0';
      }
    }
  }

  DEBUG_puts(packet);
  DEBUG_printf(("type=%x, state=%x, uri=\"%s\"\n"
                "location=\"%s\", info=\"%s\", make_model=\"%s\"\n",
	        type, state, uri, location, info, make_model));

 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

  DEBUG_printf(("host=\"%s\", ServerName=\"%s\"\n", host, ServerName));

 /*
  * Check for packets from the local server...
  */

  if (strcasecmp(host, ServerName) == 0)
    return;

  NetIFUpdate(FALSE);

  for (iface = NetIFList; iface != NULL; iface = iface->next)
    if (strcasecmp(host, iface->hostname) == 0)
      return;

 /*
  * Do relaying...
  */

  for (i = 0; i < NumRelays; i ++)
    if (CheckAuth(address, srcname, len, 1, &(Relays[i].from)))
      if (sendto(BrowseSocket, packet, bytes, 0,
                 (struct sockaddr *)&(Relays[i].to),
		 sizeof(struct sockaddr_in)) <= 0)
      {
	LogMessage(L_ERROR, "UpdateCUPSBrowse: sendto failed for relay %d - %s.",
	           i + 1, strerror(errno));
	return;
      }

 /*
  * Process the browse data...
  */

  ProcessBrowseData(uri, type, state, location, info, make_model, BROWSE_CUPS);
}


/*
 * 'UpdatePolling()' - Read status messages from the poll daemons.
 */

void
UpdatePolling(void)
{
  int		bytes;		/* Number of bytes read */
  char		*lineptr;	/* Pointer to end of line in buffer */
  static int	bufused = 0;	/* Number of bytes used in buffer */
  static char	buffer[1024];	/* Status buffer */


  if ((bytes = read(PollPipe, buffer + bufused,
                    sizeof(buffer) - bufused - 1)) > 0)
  {
    bufused += bytes;
    buffer[bufused] = '\0';
    lineptr = strchr(buffer, '\n');
  }
  else if (bytes < 0 && errno == EINTR)
    return;
  else
  {
    lineptr    = buffer + bufused;
    lineptr[1] = 0;
  }

  if (bytes == 0 && bufused == 0)
    lineptr = NULL;

  while (lineptr != NULL)
  {
   /*
    * Terminate each line and process it...
    */

    *lineptr++ = '\0';

    if (!strncmp(buffer, "ERROR: ", 7))
      LogMessage(L_ERROR, "%s", buffer + 7);
    else if (!strncmp(buffer, "DEBUG: ", 7))
      LogMessage(L_DEBUG, "%s", buffer + 7);
    else if (!strncmp(buffer, "DEBUG2: ", 8))
      LogMessage(L_DEBUG2, "%s", buffer + 8);
    else
      LogMessage(L_DEBUG, "%s", buffer);

   /*
    * Copy over the buffer data we've used up...
    */

    cups_strcpy(buffer, lineptr);
    bufused -= lineptr - buffer;

    if (bufused < 0)
      bufused = 0;

    lineptr = strchr(buffer, '\n');
  }

  if (bytes <= 0)
  {
   /*
    * All polling processes have died; stop polling...
    */

    LogMessage(L_ERROR, "UpdatePolling: all polling processes have exited!");
    StopPolling();
  }
}


/***********************************************************************
 **** SLP Support Code *************************************************
 ***********************************************************************/

#ifdef HAVE_LIBSLP 
/*
 * SLP service name for CUPS...
 */

#  define SLP_CUPS_SRVTYPE	"service:printer"
#  define SLP_CUPS_SRVLEN	15


/* 
 * Printer service URL structure
 */

typedef struct _slpsrvurl
{
  struct _slpsrvurl	*next;
  char			url[HTTP_MAX_URI];
} slpsrvurl_t;


/*
 * 'RegReportCallback()' - Empty SLPRegReport.
 */

void
RegReportCallback(SLPHandle hslp,
                  SLPError  errcode,
		  void      *cookie)
{
  (void)hslp;
  (void)errcode;
  (void)cookie;

  return;
}


/*
 * 'SendSLPBrowse()' - Register the specified printer with SLP.
 */

static void 
SendSLPBrowse(printer_t *p)		/* I - Printer to register */
{
  char		srvurl[HTTP_MAX_URI],	/* Printer service URI */
		attrs[8192],		/* Printer attributes */
		finishings[1024],	/* Finishings to support */
		make_model[IPP_MAX_NAME * 2],
					/* Make and model, quoted */
		location[IPP_MAX_NAME * 2],
					/* Location, quoted */
		info[IPP_MAX_NAME * 2],
					/* Info, quoted */
		*src,			/* Pointer to original string */
		*dst;			/* Pointer to destination string */
  ipp_attribute_t *authentication;	/* uri-authentication-supported value */
  SLPError	error;			/* SLP error, if any */


  LogMessage(L_DEBUG, "SendSLPBrowse(%p = \"%s\")", p, p->name);

 /*
  * Make the SLP service URL that conforms to the IANA 
  * 'printer:' template.
  */

  snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

  LogMessage(L_DEBUG2, "Service URL = \"%s\"", srvurl);

 /*
  * Figure out the finishings string...
  */

  if (p->type & CUPS_PRINTER_STAPLE)
    strcpy(finishings, "staple");
  else
    finishings[0] = '\0';

  if (p->type & CUPS_PRINTER_BIND)
  {
    if (finishings[0])
      strlcat(finishings, ",bind", sizeof(finishings));
    else
      strcpy(finishings, "bind");
  }

  if (p->type & CUPS_PRINTER_PUNCH)
  {
    if (finishings[0])
      strlcat(finishings, ",punch", sizeof(finishings));
    else
      strcpy(finishings, "punch");
  }

  if (p->type & CUPS_PRINTER_COVER)
  {
    if (finishings[0])
      strlcat(finishings, ",cover", sizeof(finishings));
    else
      strcpy(finishings, "cover");
  }

  if (p->type & CUPS_PRINTER_SORT)
  {
    if (finishings[0])
      strlcat(finishings, ",sort", sizeof(finishings));
    else
      strcpy(finishings, "sort");
  }

  if (!finishings[0])
    strcpy(finishings, "none");

 /*
  * Quote any commas in the make and model, location, and info strings...
  */

  for (src = p->make_model, dst = make_model;
       src && *src && dst < (make_model + sizeof(make_model) - 2);)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!make_model[0])
    strcpy(make_model, "Unknown");

  for (src = p->location, dst = location;
       src && *src && dst < (location + sizeof(location) - 2);)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!location[0])
    strcpy(location, "Unknown");

  for (src = p->info, dst = info;
       src && *src && dst < (info + sizeof(info) - 2);)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!info[0])
    strcpy(info, "Unknown");

 /*
  * Get the authentication value...
  */

  authentication = ippFindAttribute(p->attrs, "uri-authentication-supported",
                                    IPP_TAG_KEYWORD);

 /*
  * Make the SLP attribute string list that conforms to
  * the IANA 'printer:' template.
  */

  snprintf(attrs, sizeof(attrs),
           "(printer-uri-supported=%s),"
           "(uri-authentication-supported=%s>),"
#ifdef HAVE_SSL
           "(uri-security-supported=tls>),"
#else
           "(uri-security-supported=none>),"
#endif /* HAVE_SSL */
           "(printer-name=%s),"
           "(printer-location=%s),"
           "(printer-info=%s),"
           "(printer-more-info=%s),"
           "(printer-make-and-model=%s),"
	   "(charset-supported=utf-8),"
	   "(natural-language-configured=%s),"
	   "(natural-language-supported=de,en,es,fr,it),"
           "(color-supported=%s),"
           "(finishings-supported=%s),"
           "(sides-supported=one-sided%s),"
	   "(multiple-document-jobs-supported=true)"
	   "(ipp-versions-supported=1.0,1.1)",
	   p->uri, authentication->values[0].string.text, p->name, location,
	   info, p->uri, make_model, DefaultLanguage,
           p->type & CUPS_PRINTER_COLOR ? "true" : "false",
           finishings,
           p->type & CUPS_PRINTER_DUPLEX ?
	       ",two-sided-long-edge,two-sided-short-edge" : "");

  LogMessage(L_DEBUG2, "Attributes = \"%s\"", attrs);

 /*
  * Register the printer with the SLP server...
  */

  error = SLPReg(BrowseSLPHandle, srvurl, BrowseTimeout,
	         SLP_CUPS_SRVTYPE, attrs, SLP_TRUE, RegReportCallback, 0);

  if (error != SLP_OK)
    LogMessage(L_ERROR, "SLPReg of \"%s\" failed with status %d!", p->name,
               error);
}


/*
 * 'SLPDeregPrinter()' - SLPDereg() the specified printer
 */

void 
SLPDeregPrinter(printer_t *p)
{
  char	srvurl[HTTP_MAX_URI];	/* Printer service URI */


  if((p->type & CUPS_PRINTER_REMOTE) == 0)
  {
   /*
    * Make the SLP service URL that conforms to the IANA 
    * 'printer:' template.
    */

    snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

   /*
    * Deregister the printer...
    */

    SLPDereg(BrowseSLPHandle, srvurl, RegReportCallback, 0);
  }
}


/*
 * 'GetSlpAttrVal()' - Get an attribute from an SLP registration.
 */

int 					/* O - 0 on success */
GetSlpAttrVal(const char *attrlist,	/* I - Attribute list string */
              const char *tag,		/* I - Name of attribute */
              char       **valbuf)	/* O - Value */
{
  char	*ptr1,				/* Pointer into string */
	*ptr2;				/* ... */


  ClearString(valbuf);

  if ((ptr1 = strstr(attrlist, tag)) != NULL)
  {
    ptr1 += strlen(tag);

    if ((ptr2 = strchr(ptr1,')')) != NULL)
    {
     /*
      * Copy the value...
      */

      *valbuf = calloc(ptr2 - ptr1 + 1, 1);
      strncpy(*valbuf, ptr1, ptr2 - ptr1);

     /*
      * Dequote the value...
      */

      for (ptr1 = *valbuf; *ptr1; ptr1 ++)
	if (*ptr1 == '\\' && ptr1[1])
	  cups_strcpy(ptr1, ptr1 + 1);

      return (0);
    }
  }

  return (-1);
}


/*
 * 'AttrCallback()' - SLP attribute callback 
 */

SLPBoolean				/* O - SLP_TRUE for success */
AttrCallback(SLPHandle  hslp,		/* I - SLP handle */
             const char *attrlist,	/* I - Attribute list */
             SLPError   errcode,	/* I - Parsing status for this attr */
             void       *cookie)	/* I - Current printer */
{
  char         *tmp = 0;
  printer_t    *p = (printer_t*)cookie;


 /*
  * Let the compiler know we won't be using these...
  */

  (void)hslp;

 /*
  * Bail if there was an error
  */

  if (errcode != SLP_OK)
    return (SLP_TRUE);

 /*
  * Parse the attrlist to obtain things needed to build CUPS browse packet
  */

  memset(p, 0, sizeof(printer_t));

  p->type = CUPS_PRINTER_REMOTE;

  if (GetSlpAttrVal(attrlist, "(printer-location=", &(p->location)))
    return (SLP_FALSE);
  if (GetSlpAttrVal(attrlist, "(printer-info=", &(p->info)))
    return (SLP_FALSE);
  if (GetSlpAttrVal(attrlist, "(printer-make-and-model=", &(p->make_model)))
    return (SLP_FALSE);

  if (GetSlpAttrVal(attrlist, "(color-supported=", &tmp))
    return (SLP_FALSE);
  if (strcasecmp(tmp, "true") == 0)
    p->type |= CUPS_PRINTER_COLOR;

  if (GetSlpAttrVal(attrlist, "(finishings-supported=", &tmp))
    return (SLP_FALSE);
  if (strstr(tmp, "staple"))
    p->type |= CUPS_PRINTER_STAPLE;
  if (strstr(tmp, "bind"))
    p->type |= CUPS_PRINTER_BIND;
  if (strstr(tmp, "punch"))
    p->type |= CUPS_PRINTER_PUNCH;

  if (GetSlpAttrVal(attrlist, "(sides-supported=", &tmp))
    return (SLP_FALSE);
  if (strstr(tmp,"two-sided"))
    p->type |= CUPS_PRINTER_DUPLEX;

  ClearString(&tmp);

  return (SLP_TRUE);
}


/*
 * 'SrvUrlCallback()' - SLP service url callback
 */

SLPBoolean				/* O - TRUE = OK, FALSE = error */
SrvUrlCallback(SLPHandle      hslp, 	/* I - SLP handle */
               const char     *srvurl, 	/* I - URL of service */
               unsigned short lifetime,	/* I - Life of service */
               SLPError       errcode, 	/* I - Existing error code */
               void           *cookie)	/* I - Pointer to service list */
{
  slpsrvurl_t	*s,			/* New service entry */
		**head;			/* Pointer to head of entry */


 /*
  * Let the compiler know we won't be using these vars...
  */

  (void)hslp;
  (void)lifetime;

 /*
  * Bail if there was an error
  */

  if (errcode != SLP_OK)
    return (SLP_TRUE);

 /*
  * Grab the head of the list...
  */

  head = (slpsrvurl_t**)cookie;

 /*
  * Allocate a *temporary* slpsrvurl_t to hold this entry.
  */

  if ((s = (slpsrvurl_t *)calloc(1, sizeof(slpsrvurl_t))) == NULL)
    return (SLP_FALSE);

 /*
  * Copy the SLP service URL...
  */

  strlcpy(s->url, srvurl, sizeof(s->url));

 /* 
  * Link the SLP service URL into the head of the list
  */

  if (*head)
    s->next = *head;

  *head = s;

  return (SLP_TRUE);
}


/*
 * 'UpdateSLPBrowse()' - Get browsing information via SLP.
 */

void
UpdateSLPBrowse(void)
{
  slpsrvurl_t	*s,			/* Temporary list of service URLs */
		*next;			/* Next service in list */
  printer_t	p;			/* Printer information */
  const char	*uri;			/* Pointer to printer URI */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */


  LogMessage(L_DEBUG, "UpdateSLPBrowse() Start...");

 /*
  * Reset the refresh time...
  */

  BrowseSLPRefresh = time(NULL) + BrowseInterval;

 /* 
  * Poll for remote printers using SLP...
  */

  s = NULL;

  SLPFindSrvs(BrowseSLPHandle, SLP_CUPS_SRVTYPE, "", "",
	      SrvUrlCallback, &s);

 /*
  * Loop through the list of available printers...
  */

  for (; s; s = next)
  {
   /*
    * Save the "next" pointer...
    */

    next = s->next;

   /* 
    * Load a printer_t structure with the SLP service attributes...
    */

    SLPFindAttrs(BrowseSLPHandle, s->url, "", "", AttrCallback, &p);

   /*
    * Process this printer entry...
    */

    uri = s->url + SLP_CUPS_SRVLEN + 1;

    if (strncmp(uri, "http://", 7) == 0 ||
        strncmp(uri, "ipp://", 6) == 0)
    {
     /*
      * Pull the URI apart to see if this is a local or remote printer...
      */

      httpSeparate(uri, method, username, host, &port, resource);

      if (strcasecmp(host, ServerName) == 0)
	continue;

     /*
      * OK, at least an IPP printer, see if it is a CUPS printer or
      * class...
      */

      if (strstr(uri, "/printers/") != NULL)
        ProcessBrowseData(uri, p.type, IPP_PRINTER_IDLE, p.location,
	                  p.info, p.make_model, BROWSE_SLP);
      else if (strstr(uri, "/classes/") != NULL)
        ProcessBrowseData(uri, p.type | CUPS_PRINTER_CLASS, IPP_PRINTER_IDLE,
	                  p.location, p.info, p.make_model, BROWSE_SLP);
    }

   /*
    * Free this listing...
    */

    free(s);
  }       

  LogMessage(L_DEBUG, "UpdateSLPBrowse() End...");
}
#endif /* HAVE_LIBSLP */


/*
 * 'BrowseRegisterPrinter()' - Start sending broadcast information for a printer
 *				or update the broadcast contents.
 */

void BrowseRegisterPrinter(printer_t *p)
{
  if (!Browsing || !BrowseLocalProtocols || !BrowseInterval || !Browsers ||
      (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    return;

#ifdef HAVE_DNSSD
    if ((BrowseLocalProtocols & BROWSE_DNSSD))
      dnssdRegisterPrinter(p);    
#endif /* HAVE_DNSSD */
}


/*
 * 'BrowseDeregisterPrinter()' - Stop sending broadcast information for a 
 *				 local printer and remove any pending references
 *				 to remote printers.
 */

void BrowseDeregisterPrinter(printer_t *p, 	/* I - Printer to register */
			     int delete)	/* I - Printer being permanently removed */
{
  cups_ptype_t savedtype = p->type;

 /*
  * Only deregister local printers...
  */

  if (!(p->type & CUPS_PRINTER_REMOTE))
  {
    /*
     * Mark the printer for deletion...
     */

    p->type |= CUPS_PRINTER_DELETE;

    /*
     * Announce the deletion...
     */

    if ((BrowseLocalProtocols & BROWSE_CUPS))
      SendCUPSBrowse(p);

    /*
     * Restore it's type...
     */

      p->type = savedtype;

#ifdef HAVE_LIBSLP
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
    if (delete && (BrowseLocalProtocols & BROWSE_DNSSD))
      dnssdDeregisterPrinter(p);
#endif /* HAVE_DNSSD */
  }
}


#ifdef HAVE_DNSSD

/* =============================== HAVE_DNSSD ============================== */

/*
 * 'dnssdRegisterPrinter()' - Start sending broadcast information for a printer
 *				or update the broadcast contents.
 */

static void 
dnssdRegisterPrinter(printer_t *p)
{
  DNSServiceErrorType	se;			/* dnssd errors */
  char			*txt_record,		/* TXT record buffer */
			*name;			/* Service name */
  int			txt_len,		/* TXT record length */
			i,			/* Looping var */
			port;			/* IPP port number */
  CFStringRef		computerNameRef;	/* Computer name CFString */
  CFStringEncoding	nameEncoding;		/* Computer name encoding */
  CFMutableStringRef	shortNameRef;		/* Mutable name string */
  CFIndex		nameLength;		/* Name string length */
  char			str_buffer[1024];	/* C-string buffer */
  const char		*computerName;  	/* Computer name c-string ptr */


  LogMessage(L_DEBUG, "dnssdRegisterPrinter(%s) %s\n", p->name, !p->dnssd_ipp_ref ? "new" : "update");

 /*
  * If per-printer sharing is disabled make sure we're not registered before returning
  */

  if (!p->shared)
  {
    dnssdDeregisterPrinter(p);
    return;
  }

 /*
  * Get the computer name as a c-string...
  */

  computerName = NULL;
  if ((computerNameRef = SCDynamicStoreCopyComputerName(mSysConfigStore, &nameEncoding)))
    if ((computerName = CFStringGetCStringPtr(computerNameRef, kCFStringEncodingUTF8)) == NULL)
      if (CFStringGetCString(computerNameRef, str_buffer, sizeof(str_buffer), kCFStringEncodingUTF8))
	computerName = str_buffer;

 /*
  * The registered name takes the form of "<printer-info> @ <computer name>"...
  */

  name = NULL;
  if (computerName)
    SetStringf(&name, "%s @ %s", (p->info && strlen(p->info)) ? p->info : p->name, computerName);
  else
    SetString(&name, (p->info && strlen(p->info)) ? p->info : p->name);

  if (computerNameRef)
    CFRelease(computerNameRef);

 /*
  * If an existing printer was renamed unregister it and start over...
  */

  if (p->reg_name && strcmp(p->reg_name, name))
    dnssdDeregisterPrinter(p);

  txt_record = dnssdBuildTxtRecord(&txt_len, p);

  if (p->dnssd_ipp_ref == NULL)
  {
   /*
    * Initial registration...
    */

    SetString(&p->reg_name, name);

    port = ippPort();
    for (i = 0; i < NumListeners; i++)
      if (Listeners[i].address.sin_family == AF_INET)
      {
	port = ntohs(Listeners[i].address.sin_port);
	break;
      }

    se = DNSServiceRegister(&p->dnssd_ipp_ref, 0, 0, name, dnssdIPPRegType, 
			    NULL, NULL, htons(port), txt_len, txt_record,
			    dnssdRegisterCallback, p);

   /*
    * In case the name is too long try again shortening the string one character at a time...
    */

    if (se == kDNSServiceErr_BadParam)
    {
      if ((shortNameRef = CFStringCreateMutable(NULL, 0)) != NULL)
      {
	CFStringAppendCString(shortNameRef, name, kCFStringEncodingUTF8);
        nameLength = CFStringGetLength(shortNameRef);

	while (se == kDNSServiceErr_BadParam && nameLength > 1)
	{
	  CFStringDelete(shortNameRef, CFRangeMake(--nameLength, 1));
	  if (CFStringGetCString(shortNameRef, str_buffer, sizeof(str_buffer), kCFStringEncodingUTF8))
	  {
	    se = DNSServiceRegister(&p->dnssd_ipp_ref, 0, 0, str_buffer, dnssdIPPRegType, 
				    NULL, NULL, htons(port), txt_len, txt_record,
				    dnssdRegisterCallback, p);
	  }
	}
	CFRelease(shortNameRef);
      }
    }

    if (se == kDNSServiceErr_NoError)
    {
      p->dnssd_ipp_fd = DNSServiceRefSockFD(p->dnssd_ipp_ref);
      p->txt_record = txt_record;
      p->txt_len = txt_len;
      txt_record = NULL;

      LogMessage(L_DEBUG2, "dnssdRegisterCallback: Adding fd %d to InputSet...",
               p->dnssd_ipp_fd);

      FD_SET(p->dnssd_ipp_fd, InputSet);
    }
    else
      LogMessage(L_WARN, "dnssd registration of \"%s\" failed with %d", p->name, se);

  }
  else if (txt_len != p->txt_len || memcmp(txt_record, p->txt_record, txt_len) != 0)
  {
   /*
    * if the txt record changed update the existing registration...
    */

    /* A TTL of 0 means use record's original value (Radar 3176248) */
    se = DNSServiceUpdateRecord(p->dnssd_ipp_ref, NULL, 0,
				    txt_len, txt_record, 0);

    if (p->txt_record)
      free(p->txt_record);
    p->txt_record = txt_record;
    p->txt_len = txt_len;
    txt_record = NULL;
  }

  if (txt_record)
    free(txt_record);

  if (name)
    free(name);
}


/*
 * 'dnssdDeregisterPrinter()' - Stop sending broadcast information for a printer
 */

static void dnssdDeregisterPrinter(printer_t *p)
{
  LogMessage(L_DEBUG, "dnssdDeregisterPrinter(%s)", p->name);

 /*
  * Closing the socket deregisters the service
  */

  if (p->dnssd_ipp_ref)
  {
    LogMessage(L_DEBUG2, "dnssdDeregisterPrinter: Removing fd %d from InputSet...",
	       p->dnssd_ipp_fd);

    FD_CLR(p->dnssd_ipp_fd, InputFds);
    FD_CLR(p->dnssd_ipp_fd, InputSet);
    DNSServiceRefDeallocate(p->dnssd_ipp_ref);
    p->dnssd_ipp_ref = NULL;
    p->dnssd_ipp_fd = -1;
  }

  if (p->dnssd_query_ref)
  {
    LogMessage(L_DEBUG2, "dnssdDeregisterPrinter: Removing fd %d from InputSet...",
	       p->dnssd_query_fd);

    FD_CLR(p->dnssd_query_fd, InputFds);
    FD_CLR(p->dnssd_query_fd, InputSet);
    DNSServiceRefDeallocate(p->dnssd_query_ref);
    p->dnssd_query_ref = NULL;
    p->dnssd_query_fd = -1;
  }

  ClearString(&p->reg_name);
  ClearString(&p->service_name);
  ClearString(&p->host_target);
  ClearString(&p->txt_record);

  p->browse_protocol &= ~BROWSE_DNSSD;
}


/*
 * 'dnssdRegisterCallback()' - DNSServiceRegister callback
 */

static void dnssdRegisterCallback(
    DNSServiceRef	sdRef,		/* I - DNS Service reference	*/
    DNSServiceFlags	flags,		/* I - Reserved for future use	*/
    DNSServiceErrorType	errorCode,	/* I - Error code		*/
    const char		*name,     	/* I - Service name		*/
    const char		*regtype,  	/* I - Service type		*/
    const char		*domain,   	/* I - Domain. ".local" for now */
    void		*context)	/* I - User-defined context	*/
{
  printer_t		*p;		/* Printer information */

  LogMessage(L_DEBUG, "dnssdRegisterCallback(%s, %s)\n", name, regtype);

  if (errorCode != 0)
  {
    LogMessage(L_ERROR, "DNSServiceRegister failed with error %d", (int)errorCode);
    return;
  }

  /*
  *  Remember the registered service name...
  */

  p = (printer_t*)context;
  SetString(&p->service_name, name);
}


/*
 * 'dnssdBrowseCallback()' - DNSServiceBrowse callback
 */

static void dnssdBrowseCallback(
    DNSServiceRef		sdRef,		/* I - DNS Service reference	*/
    DNSServiceFlags		flags,		/* I - Reserved for future use	*/
    uint32_t			interfaceIndex,	/* I - 0 for all interfaces	*/
    DNSServiceErrorType		errorCode,	/* I - Error code		*/
    const char			*service_name,	/* I - Service name		*/
    const char			*regtype,  	/* I - Service type		*/
    const char			*replyDomain,   /* I - Domain. ".local" for now */
    void			*context)	/* I - User-defined context	*/
{
  DNSServiceErrorType	se;			/* dnssd errors		*/
  printer_t		*p;			/* Printer information	*/
  dnssd_resolve_t	*dnssd_resolve,		/* Resolve request */
    			*prev;			/* Previous resolve request */

  LogMessage(L_DEBUG, "dnssdBrowseCallback(%s) 0x%X %s\n", service_name, (unsigned int)flags,
		(flags & kDNSServiceFlagsAdd) ? "Add" : "Remove" );

  if (errorCode != 0)
  {
    LogMessage(L_ERROR, "DNSServiceBrowse failed with error %d", (int)errorCode);
    return;
  }

  /*
   * Try to match this service name to an existing printer's service name
   */

  for (p = Printers; p != NULL; p = p->next)
    if (p->service_name && strcmp(service_name, p->service_name) == 0)
      break;

  if ( (flags & kDNSServiceFlagsAdd) )
  {
    if (p)
    {
      LogMessage(L_DEBUG, "dnssdBrowseCallback() ignoring existing printer");
      return;
    }

   /*
    * Resolve this service to get the host name
    */

    dnssd_resolve = calloc(1, sizeof(dnssd_resolve_t));
    if (dnssd_resolve != NULL)
    {
      if ((se = DNSServiceResolve(&dnssd_resolve->sdRef, flags, interfaceIndex, service_name,
			   regtype, replyDomain, dnssdResolveCallback, NULL)) == 0)
      {
	dnssd_resolve->fd = DNSServiceRefSockFD(dnssd_resolve->sdRef);

        LogMessage(L_DEBUG2, "dnssdBrowseCallback: Adding fd %d to InputSet...",
		   dnssd_resolve->fd);

        FD_SET(dnssd_resolve->fd, InputSet);

       /*
        * We'll need the service name in the resolve callback
        */

	SetString(&dnssd_resolve->service_name, service_name);

       /*
        * Insert it into the pending resolve list
        */

	dnssd_resolve->next = DNSSDPendingResolves;
	DNSSDPendingResolves = dnssd_resolve;
      }
      else
        free(dnssd_resolve);
    }
  }
  else
  {
    if (p && (p->type & CUPS_PRINTER_REMOTE))
    {
      LogMessage(L_INFO, "Remote destination \"%s\" unregistered; deleting it...",
			p->name);
      DeletePrinter(p, 1);
    }
    else
    {
     /*
      * Remove any pending resolve requests for this service
      */

      for (prev = NULL, dnssd_resolve = DNSSDPendingResolves; dnssd_resolve; prev = dnssd_resolve, dnssd_resolve = dnssd_resolve->next)
	if (!strcmp(service_name, dnssd_resolve->service_name))
	{
	  if (prev == NULL)
	    DNSSDPendingResolves = dnssd_resolve->next;
	  else
	    prev->next = dnssd_resolve->next;

	  LogMessage(L_DEBUG2, "dnssdBrowseCallback: Removing fd %d from InputSet...",
		     dnssd_resolve->fd);

	  FD_CLR(dnssd_resolve->fd, InputFds);
	  FD_CLR(dnssd_resolve->fd, InputSet);
	  DNSServiceRefDeallocate(dnssd_resolve->sdRef);

	  free(dnssd_resolve);
	  break;
	}
    }
  }
}


/*
 * 'dnssdResolveCallback()' - DNSServiceResolve callback
 */

static void dnssdResolveCallback(
    DNSServiceRef		sdRef,		/* I - DNS Service reference	*/
    DNSServiceFlags		flags,		/* I - Reserved for future use	*/
    uint32_t			interfaceIndex,	/* I - 0 for all interfaces	*/
    DNSServiceErrorType		errorCode,	/* I - Error code		*/
    const char			*fullname,	/* I - Service name		*/   
    const char			*host_target,	/* I - Hostname			*/
    uint16_t			port,		/* I - Port			*/
    uint16_t			txt_len,		/* I - TXT record length	*/
    const char			*txtRecord,	/* I - TXT reocrd		*/
    void			*context)	/* I - User-defined context	*/
{
  DNSServiceErrorType	se;			/* dnssd errors */
  char			uri[1024];		/* IPP URI */
  char			*txtvers,		/* TXT record version */
  			*rp,			/* Name */
  			*make_model,		/* Make and model */
  			*location,		/* Location */
  			*state_str,		/* State buffer */
			*type_str,		/* Type buffer */
  			*info;			/* Printer information */
  ipp_pstate_t		state;			/* State */
  unsigned int		type;			/* Type */
  printer_t		*p;			/* Printer information */
  dnssd_resolve_t	*dnssd_resolve,		/* Current resolve request */
    			*prev;			/* Previous resolve request */

  LogMessage(L_DEBUG, "dnssdResolveCallback(%s)\n", fullname);

  /*
   * Find the matching request
   */

  for (prev = NULL, dnssd_resolve = DNSSDPendingResolves; dnssd_resolve; prev = dnssd_resolve, dnssd_resolve = dnssd_resolve->next)
    if (sdRef == dnssd_resolve->sdRef)
      break;

  if (!dnssd_resolve)
  {
    LogMessage(L_ERROR, "dnssdResolveCallback missing request!");
    return;
  }

 /*
  * Remove it from the pending list
  */

  if (prev == NULL)
    DNSSDPendingResolves = dnssd_resolve->next;
  else
    prev->next = dnssd_resolve->next;

  LogMessage(L_DEBUG2, "dnssdResolveCallback: Removing fd %d from InputSet...",
	     dnssd_resolve->fd);

  FD_CLR(dnssd_resolve->fd, InputFds);
  FD_CLR(dnssd_resolve->fd, InputSet);
  DNSServiceRefDeallocate(dnssd_resolve->sdRef);

  if (errorCode != 0)
  {
    LogMessage(L_ERROR, "DNSServiceResolve failed with error %d", (int)errorCode);
    ClearString(&dnssd_resolve->service_name);
    free(dnssd_resolve);
    return;
  }

 /*
  * Search the TXT record for the keys we're interested in
  */

  dnssdFindAttr(txtRecord, txt_len, "txtvers", &txtvers);
  dnssdFindAttr(txtRecord, txt_len, "rp", &rp);
  dnssdFindAttr(txtRecord, txt_len, "ty", &make_model);
  dnssdFindAttr(txtRecord, txt_len, "note", &location);
  dnssdFindAttr(txtRecord, txt_len, "printer-state", &state_str);
  dnssdFindAttr(txtRecord, txt_len, "printer-type", &type_str);

  if (type_str && state_str && rp)
  {
    type = strtod(type_str, NULL);
    state = strtod(state_str, NULL);

    snprintf(uri, sizeof(uri), "ipp://%s/%s/%s", host_target,
	   (type & CUPS_PRINTER_CLASS) ? "classes" : "printers", rp);

   /*
    * If the service name matches the queue name (rp) then set info to NULL,
    * otherwise set it to the service name.
    */

    info = !strcmp(dnssd_resolve->service_name, rp) ? NULL : dnssd_resolve->service_name;

    p = ProcessBrowseData(uri, type, state, location, info, make_model, BROWSE_DNSSD);

    /*
     * If we matched an existing printer or created a new one set it's service name.
     */

    if (p)
    {
      SetString(&p->service_name, dnssd_resolve->service_name);
      SetString(&p->host_target, host_target);

      if ((se = DNSServiceQueryRecord(&p->dnssd_query_ref, 0, 0, fullname, ns_t_txt, ns_c_in, 
    				dnssdQueryRecordCallback, p)) == 0)
      {
        p->dnssd_query_fd = DNSServiceRefSockFD(p->dnssd_query_ref);

        LogMessage(L_DEBUG2, "dnssdResolveCallback: Adding fd %d to InputSet...",
		   p->dnssd_query_fd);

        FD_SET(p->dnssd_query_fd, InputSet);
      }
    }
  }
  else
    LogMessage(L_DEBUG, "dnssdResolveCallback missing TXT record keys");

  ClearString(&txtvers);
  ClearString(&rp);
  ClearString(&make_model);
  ClearString(&location);
  ClearString(&state_str);
  ClearString(&type_str);

  ClearString(&dnssd_resolve->service_name);
  free(dnssd_resolve);
}


/*
 * 'dnssdQueryRecordCallback()' - DNSServiceQueryRecord callback
 */

static void dnssdQueryRecordCallback(
    DNSServiceRef		sdRef,		/* I - DNS Service reference */
    DNSServiceFlags		flags,		/* I - Reserved for future use	*/
    uint32_t			interfaceIndex,	/* I - 0 for all interfaces	*/
    DNSServiceErrorType		errorCode,	/* I - Error code		*/
    const char			*fullname,	/* I - Service name		*/   
    uint16_t			rrtype,		/* I - */
    uint16_t			rrclass,	/* I - */
    uint16_t			rdlen,		/* I - */
    const void			*rdata,		/* I - */
    uint32_t			ttl,		/* I - */
    void 			*context)	/* I - */
{
  char		uri[1024];		/* IPP URI */
  char		*txtvers,		/* TXT record version */
  		*rp,			/* Name */
  		*make_model,		/* Make and model */
  		*location,		/* Location */
  		*state_str,		/* State buffer */
		*type_str;		/* Type buffer */
  ipp_pstate_t	state;			/* State */
  unsigned int	type;			/* Type */
  printer_t	*p;			/* Printer information */

  LogMessage(L_DEBUG, "dnssdQueryRecordCallback(%s) %s\n", fullname, (flags & kDNSServiceFlagsAdd) ? "Add" : "Remove");

  if (errorCode != 0)
  {
    LogMessage(L_ERROR, "DNSServiceQueryRecord failed with error %d", (int)errorCode);
    return;
  }


  if ((flags & kDNSServiceFlagsAdd))
  {
    p = (printer_t *)context;

    /*
     * Search the TXT record for the keys we're interested in
     */

    dnssdFindAttr(rdata, rdlen, "txtvers", &txtvers);
    dnssdFindAttr(rdata, rdlen, "rp", &rp);
    dnssdFindAttr(rdata, rdlen, "ty", &make_model);
    dnssdFindAttr(rdata, rdlen, "note", &location);
    dnssdFindAttr(rdata, rdlen, "printer-state", &state_str);
    dnssdFindAttr(rdata, rdlen, "printer-type", &type_str);

    if (type_str && state_str && rp)
    {
      type = strtod(type_str, NULL);
      state = strtod(state_str, NULL);

      snprintf(uri, sizeof(uri), "ipp://%s/%s/%s", p->host_target,
		   (type & CUPS_PRINTER_CLASS) ? "classes" : "printers", rp);

      ProcessBrowseData(uri, type, state, location, p->info, make_model, BROWSE_DNSSD);
    }
    else
      LogMessage(L_DEBUG, "dnssdQueryRecordCallback missing TXT record keys");

    ClearString(&txtvers);
    ClearString(&rp);
    ClearString(&make_model);
    ClearString(&location);
    ClearString(&state_str);
    ClearString(&type_str);
  }
}


/*
 * 'dnssdBuildTxtRecord()' - Build a TXT record from printer info
 */

static char *dnssdBuildTxtRecord(int *txt_len,		/* O - TXT record length */
				printer_t *p)		/* I - Printer information */
{
  int		i;			/* Looping var */
  char		type_str[32],		/* Type to string buffer */
		state_str[32],		/* State to string buffer */
		rp_str[1024],		/* Queue name string buffer */
		*keyvalue[32][2];	/* Table of key/value pairs */

  i = 0;

  /*
  *  Load up the key value pairs...
  */

  keyvalue[i  ][0] = "txtvers";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "qtotal";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "rp";
  keyvalue[i++][1] = rp_str;
  snprintf(rp_str, sizeof(rp_str), "%s/%s", 
	   (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers", p->name);

  keyvalue[i  ][0] = "ty";
  keyvalue[i++][1] = p->make_model;

  if (p->location && *p->location != '\0')
  {
    keyvalue[i  ][0] = "note";
    keyvalue[i++][1] = p->location;
  }

  keyvalue[i  ][0] = "product";
  keyvalue[i++][1] = p->product ? p->product : "Unknown";

  snprintf(type_str,  sizeof(type_str),  "0x%X", p->type | CUPS_PRINTER_REMOTE);
  snprintf(state_str, sizeof(state_str), "%d", p->state);

  keyvalue[i  ][0] = "printer-state";
  keyvalue[i++][1] = state_str;

  keyvalue[i  ][0] = "printer-type";
  keyvalue[i++][1] = type_str;

  keyvalue[i  ][0] = "Transparent";
  keyvalue[i++][1] = "T";

  keyvalue[i  ][0] = "Binary";
  keyvalue[i++][1] = "T";

  if ((p->type & CUPS_PRINTER_FAX))
  {
    keyvalue[i  ][0] = "Fax";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COLOR))
  {
    keyvalue[i  ][0] = "Color";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_DUPLEX))
  {
    keyvalue[i  ][0] = "Duplex";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_STAPLE))
  {
    keyvalue[i  ][0] = "Staple";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COPIES))
  {
    keyvalue[i  ][0] = "Copies";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COLLATE))
  {
    keyvalue[i  ][0] = "Collate";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_PUNCH))
  {
    keyvalue[i  ][0] = "Punch";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_BIND))
  {
    keyvalue[i  ][0] = "Bind";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_SORT))
  {
    keyvalue[i  ][0] = "Sort";
    keyvalue[i++][1] = "T";
  }

  keyvalue[i  ][0] = "pdl";
  keyvalue[i++][1] = p->pdl ? p->pdl : "application/postscript";

 /*
  * Then pack them into a proper txt record...
  */

  return dnssdPackTxtRecord(txt_len, keyvalue, i);
}


/*
 * 'dnssdPackTxtRecord()' - Pack an array of key/value pairs into the TXT record format
 */

static char *dnssdPackTxtRecord(int *txt_len,		/* O - TXT record length	*/
			      char *keyvalue[][2],	/* I - Table of key value pairs	*/
			      int count)		/* I - Items in table		*/
{
  int  index;			/* Looping var */
  int  length;			/* Length of TXT record */
  char *txtRecord;		/* TXT record buffer */
  char *cursor;			/* Looping pointer */

  /*
   * Calculate the buffer size
   */

  for (length = index = 0; index < count; index++)
    length += 1 + strlen(keyvalue[index][0]) + 
	      (keyvalue[index][1] ? 1 + strlen(keyvalue[index][1]) : 0);

  /*
   * Allocate and fill it
   */

  txtRecord = malloc(length);
  if (txtRecord)
  {
    *txt_len = length;

    for (cursor = txtRecord, index = 0; index < count; index++)
    {
      /*
       * Drop in the p-string style length byte followed by the data
       */
      *cursor++ = (unsigned char)(strlen(keyvalue[index][0]) + 
				(keyvalue[index][1] ? 1 + strlen(keyvalue[index][1]) : 0));

      length = strlen(keyvalue[index][0]);      
      memcpy(cursor, keyvalue[index][0], length);
      cursor += length;

      if (keyvalue[index][1])
      {
	*cursor++ = '=';
	length = strlen(keyvalue[index][1]);
	memcpy(cursor, keyvalue[index][1], length);
	cursor += length;
      }
    }
  }

  return txtRecord;
}


/*
 * dnssdFindAttr()' - Find a TXT record attribute value
 */

static int dnssdFindAttr(const unsigned char *txtRecord,	/* I - TXT record to search */
			int txt_len, 			/* I - Length of TXT record */
			const char *key,		/* I - Key to match */
			char **value)			/* O - Value string */
{
  int 	result = -1;			/* Return result */
  int 	keyLen;				/* Key length */
  int 	valueLen;			/* Value length */
  const unsigned char *txtRecordEnd;	/* End of TXT record */

  *value = NULL;

  /*
   * Walks the list of p-strings to find the matching key /value
   */

  keyLen = strlen(key);
  txtRecordEnd = txtRecord + txt_len;

  for (txtRecordEnd = txtRecord + txt_len; txtRecord < txtRecordEnd; txtRecord = txtRecord + *txtRecord + 1)
  {
    if (*txtRecord >= keyLen && memcmp(key, txtRecord+1, keyLen) == 0 && (*txtRecord == keyLen || *(txtRecord + keyLen + 1) == '='))
    {
      result = 0;						/* Found the key we're looking for */
      valueLen = *txtRecord - keyLen - 1;

      if (valueLen < 0)
        result = -2;						/* Malformed TXT record */
      else
      {
        if ((*value = malloc(valueLen + 1)) == NULL)
          result = -3;
        else
        {
	  memcpy(*value, txtRecord + keyLen + 2, valueLen);	/* Copy the results as a C-string */
	  (*value)[valueLen] = '\0';
	}
      }
      break;
    }
  }

  return result;
}

#endif /* HAVE_DNSSD */

/*
 * End of "$Id: dirsvc.c,v 1.39.2.2 2005/07/27 21:58:45 jlovell Exp $".
 */
