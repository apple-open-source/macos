/*
 * "$Id: epson.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   EPSON backend for the Common UNIX Printing System.
 *
 *   Copyright 1993-2000 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License,
 *   version 2, as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()         - Send a file to the specified parallel port.
 *   list_devices() - List all parallel devices.
 */

/*
 * Include necessary headers.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <cups/cups.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_TIME_H
#  include <time.h>
#endif

#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#endif /* WIN32 || __EMX__ */

#if defined(WIN32) || defined(__EMX__)
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 || __EMX__ */

#ifdef __sgi
#  include <invent.h>
#  ifndef INV_EPP_ECP_PLP
#    define INV_EPP_ECP_PLP	6	/* From 6.3/6.4/6.5 sys/invent.h */
#    define INV_ASO_SERIAL	14	/* serial portion of SGI ASO board */
#    define INV_IOC3_DMA	16	/* DMA mode IOC3 serial */
#    define INV_IOC3_PIO	17	/* PIO mode IOC3 serial */
#    define INV_ISA_DMA		19	/* DMA mode ISA serial -- O2 */
#  endif /* !INV_EPP_ECP_PLP */
#endif /* __sgi */


/*
 * Local functions...
 */

void	list_devices(void);
void    read_backchannel(int fd_out);

/*
 * 'main()' - Send a file to the specified parallel port.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  char		method[1024],	/* Method in URI */
		hostname[1024],	/* Hostname */
		username[1024],	/* Username info (not used) */
		resource[1024],	/* Resource info (device and options) */
		*options;	/* Pointer to options */
  int		port;		/* Port number (not used) */
  int		copies;		/* Number of copies to print */
  int		fd_out,		/* Parallel/USB device or socket */
  		fd_in,		/* Print file */
		error;		/* Last error */
  struct sockaddr_in addr;	/* Socket address */
  struct hostent *hostaddr;	/* Host address */
  int		wbytes;		/* Number of bytes written */
  int		nbytes,		/* Number of bytes read */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192],	/* Output buffer */
		*bufptr;	/* Pointer into buffer */
  struct termios opts;		/* Parallel port options */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  if (argc == 1)
  {
    list_devices();
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fputs("Usage: epson job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fd_in  = fileno(stdin);
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fd_in = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: unable to open print file");
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the device name and options from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

 /*
  * See if there are any options...
  */

  if ((options = strchr(resource, '?')) != NULL)
  {
   /*
    * Yup, terminate the device name string and move to the first
    * character of the options...
    */

    *options++ = '\0';
  }

  if (hostname[0])
  {
   /*
    * Lookup the IP address...
    */

    if ((hostaddr = gethostbyname(hostname)) == NULL)
    {
      fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s",
              hostname, strerror(errno));
      return (1);
    }

    if (port == 0)
      port = 9100;	/* Default for EPSON NIC */

    fprintf(stderr, "INFO: Attempting to connect to printer %s on port %d\n",
            hostname, port);

    memset(&addr, 0, sizeof(addr));
    memcpy(&(addr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
    addr.sin_family = hostaddr->h_addrtype;
    addr.sin_port   = htons(port);

   /*
    * Try to connect...
    */

    for (;;)
    {
      if ((fd_out = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
	perror("ERROR: Unable to create socket");
	return (1);
      }

      if (connect(fd_out, (struct sockaddr *)&addr, sizeof(addr)) < 0)
      {
        error = errno;
	close(fd_out);
	fd_out = -1;

	if (error == ECONNREFUSED)
	{
	  fprintf(stderr, "INFO: Network printer \'%s\' is busy; will retry in 30 seconds...\n",
                  hostname);
	  sleep(30);
	}
	else
	{
	  perror("ERROR: Unable to connect to printer");
	  sleep(30);
	}
      }
      else
	break;
    }

    fputs("INFO: Connected to printer, sending print job...\n", stderr);
  }
  else
  {
   /*
    * Open the parallel or USB port device...
    */

    do
    {
      if ((fd_out = open(resource, O_RDWR | O_EXCL | O_NONBLOCK)) == -1)
      {
	if (errno == EBUSY)
	{
          fputs("INFO: Parallel port busy; will retry in 30 seconds...\n", stderr);
	  sleep(30);
	}
	else
	{
	  perror("ERROR: Unable to open parallel port device file");
	  return (1);
	}
      }
    }
    while (fd_out < 0);

   /*
    * Set any options provided...
    */

    tcgetattr(fd_out, &opts);

    opts.c_cflag |= CREAD;			/* Enable reading */
    opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */

    /**** No options supported yet ****/

    tcsetattr(fd_out, TCSANOW, &opts);
  }

  /*
   * Set nonblocking I/O, if possible (might already be set).
   */
  fcntl(fd_out, F_SETFL,
	O_NONBLOCK | fcntl(fd_out, F_GETFL));

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Finally, send the print file...
  */

  while (copies > 0)
  {
    copies --;

    if (fd_in != fileno(stdin))
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fd_in, 0, SEEK_SET);
    }

    tbytes = 0;
    while ((nbytes = read(fd_in, buffer, sizeof(buffer))) > 0)
    {
     /*
      * Write the print data to the printer...
      */

      tbytes += nbytes;
      bufptr = buffer;

      while (nbytes > 0)
      {
	if ((wbytes = write(fd_out, bufptr, nbytes)) < 0 && errno == EAGAIN)
	  {
	    /* Write would block, so sleep 0.2s and retry... */

	    /*
	     * Check for possible data coming back from the printer...
	     */
#if defined(HAVE_TIME_H) && defined(HAVE_NANOSLEEP)
	    struct timespec sleeptime;
#elif defined(HAVE_UNISTD_H) && defined(HAVE_USLEEP)
#else
	    struct timeval timeout;
#endif
	    read_backchannel(fd_out);
#if defined(HAVE_TIME_H) && defined(HAVE_NANOSLEEP)
	    sleeptime.tv_sec = 0;
	    sleeptime.tv_nsec = 200000000;
	    nanosleep(&sleeptime, &sleeptime);
#elif defined(HAVE_UNISTD_H) && defined(HAVE_USLEEP)
	    usleep(200000);
#else
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 200000;
	    select(1, NULL, NULL, NULL, &timeout);
#endif
	    continue;
	  }
	else if (wbytes < 0) /* write error */
	  break;

	nbytes -= wbytes;
	bufptr += wbytes;
      }

      if (nbytes > 0)
	{
	  perror("ERROR: Unable to send print file to printer");
	  break;
	}

    if (argc > 6)
      fprintf(stderr, "INFO: Sending print file, %u bytes...\n", tbytes);

    }
  }

 /*
  * Close the socket connection or parallel/USB device and input file and
  * return...
  */

  close(fd_out);
  if (fd_in != fileno(stdin))
    close(fd_in);

  return (0);
}


/*
 * 'read_backchannel()' - read data sent back from printer.
 */
void
read_backchannel(int fd_out)
{
  static int    backchannel = 1; /* Read backchannel data? */
  int		nbytes;		 /* Number of bytes read */
  int           ninks = 0;       /* Number of inks */
  char		buffer[8192],	 /* Output buffer */
		*bufptr;	 /* Pointer into buffer */


  if (!backchannel)
    return;

  if ((nbytes = read(fd_out, buffer, sizeof(buffer) - 1)) < 0 && errno != EAGAIN)
    {
      fprintf(stderr, "ERROR: Back-channel read error - %s!\n",
	      strerror(errno));
      backchannel = 0;
      return;
    }

  /*
   * Some devices report themselves permanently ready to read...
   */

  /*fprintf(stderr, "Backchannel read: %d bytes\n", nbytes);*/

  if (nbytes <= 0)
    return;

  buffer[nbytes] = '\0';
  if (strncmp(buffer, "@BDC ", 5) != 0)
    fprintf(stderr, "WARNING: Received %d bytes of unknown back-channel data!\n",
	    nbytes);
  else
    {

      /*
       * Get status data...
       */

      bufptr = strstr(&buffer[0], "IQ:");
      /*fprintf(stderr, "READBACK: %20s\n", bufptr);*/

      if (bufptr)
	{
	  /*
	   * Report ink level...
	   */

	  int i;
	  int levels[7];

	  bufptr += 3;

	  for (i = 0; i < 7 && bufptr < &buffer[sizeof(buffer)-1];
	       i ++, bufptr += 2)
	    {
	      int j, inkend = 0;

	      if (!bufptr[0] || bufptr[0] == ';')
		break;

	      for (j = 0; j < 2; j++)
		{
		  if (bufptr[j] >= '0' && bufptr[j] <= '9')
		    bufptr[j] -= '0';
		  else if (bufptr[j] >= 'A' && bufptr[j] <= 'F')
		    bufptr[j] = bufptr[j] - 'A' + 10;
		  else if (bufptr[j] >= 'a' && bufptr[j] <= 'f')
		    bufptr[j] = bufptr[j] - 'a' + 10;
		  else
		    {
		      inkend = 1;
		      break;
		    }
		}
	      levels[i] = (bufptr[0] << 4) + bufptr[1];

	      if (inkend)
		break;

	      ninks++;
            }

	  switch (ninks)
	    {
	    case 1 :
	    case 2 :
	      fprintf(stderr, "K=%d\n", levels[0]);
	      break;
	    case 3 :
	      fprintf(stderr, "C=%d M=%d Y=%d\n", levels[0], levels[1],
		      levels[2]);
	      break;
	    case 4 :
	    case 5 :
	      fprintf(stderr, "K=%d C=%d M=%d Y=%d\n", levels[0],
		      levels[1], levels[2], levels[3]);
	      break;
	    case 6 :
	      fprintf(stderr, "K=%d C=%d M=%d Y=%d LC=%d LM=%d\n",
		      levels[0], levels[1], levels[2], levels[3],
		      levels[4], levels[5]);
	      break;
	    case 7 :
	      fprintf(stderr, "K=%d C=%d M=%d Y=%d LC=%d LM=%d GY=%d\n",
		      levels[0], levels[1], levels[2], levels[3],
		      levels[4], levels[5], levels[6]);
            }
	}
      else
	fprintf(stderr, "INFO: %s\n", buffer);
    }
}

/*
 * 'list_devices()' - List all parallel devices.
 */

void
list_devices(void)
{
#ifdef __linux
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */
  FILE	*probe;			/* /proc/parport/n/autoprobe file */
  char	line[1024],		/* Line from file */
	*delim,			/* Delimiter in file */
	make[IPP_MAX_NAME],	/* Make from file */
	model[IPP_MAX_NAME];	/* Model from file */


 /*
  * Probe for parallel devices...
  */

  for (i = 0; i < 4; i ++)
  {
    sprintf(device, "/proc/parport/%d/autoprobe", i);
    if ((probe = fopen(device, "r")) != NULL)
    {
      memset(make, 0, sizeof(make));
      memset(model, 0, sizeof(model));
      strcpy(model, "EPSON");

      while (fgets(line, sizeof(line), probe) != NULL)
      {
       /*
        * Strip trailing ; and/or newline.
	*/

        if ((delim = strrchr(line, ';')) != NULL)
	  *delim = '\0';
	else if ((delim = strrchr(line, '\n')) != NULL)
	  *delim = '\0';

       /*
        * Look for MODEL and MANUFACTURER lines...
	*/

        if (strncmp(line, "MODEL:", 6) == 0 &&
	    strncmp(line, "MODEL:EPSON", 13) != 0)
	  strncpy(model, line + 6, sizeof(model) - 1);
	else if (strncmp(line, "MANUFACTURER:", 13) == 0 &&
	         strncmp(line, "MANUFACTURER:EPSON", 20) != 0)
	  strncpy(make, line + 13, sizeof(make) - 1);
      }

      fclose(probe);

      if (strcmp(make, "EPSON") == 0)
	printf("direct epson:/dev/lp%d \"%s %s\" \"Parallel Port #%d\"\n",
	       i, make, model, i + 1);
    }
    else
    {
      sprintf(device, "/dev/lp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct epson:%s \"EPSON\" \"Parallel Port #%d\"\n", device, i + 1);
      }
    }
  }

 /*
  * Probe for USB devices...
  */

  if ((probe = fopen("/proc/bus/usb/devices", "r")) != NULL)
  {
    i = 0;

    memset(make, 0, sizeof(make));
    memset(model, 0, sizeof(model));

    while (fgets(line, sizeof(line), probe) != NULL)
    {
     /*
      * Strip trailing newline.
      */

      if ((delim = strrchr(line, '\n')) != NULL)
	*delim = '\0';

     /*
      * See if it is a printer device ("P: ...")
      */

      if (strncmp(line, "S:", 2) == 0)
      {
       /*
        * String attribute...
	*/

        if (strncmp(line, "S:  Manufacturer=", 17) == 0)
	{
	  strncpy(make, line + 17, sizeof(make) - 1);
	  if (strcmp(make, "Hewlett-Packard") == 0)
	    strcpy(make, "HP");
	}
        else if (strncmp(line, "S:  Product=", 12) == 0)
	  strncpy(model, line + 12, sizeof(model) - 1);
      }
      else if (strncmp(line, "I:", 2) == 0 &&
               strstr(line, "Driver=printer") != NULL &&
	       make[0] && model[0])
      {
       /*
        * We were processing a printer device; send the info out...
	*/

        if (strcmp(make, "EPSON") == 0)
	{
          sprintf(device, "/dev/usb/lp%d", i);
	  if (access(device, 0))
	  {
	    sprintf(device, "/dev/usb/usblp%d", i);

	    if (access(device, 0))
	      sprintf(device, "/dev/usblp%d", i);
	  }

	  printf("direct epson:%s \"%s %s\" \"USB Printer #%d\"\n",
		 device, make, model, i + 1);
        }

	i ++;

	memset(make, 0, sizeof(make));
	memset(model, 0, sizeof(model));
      }
    }

    fclose(probe);
  }
  else
  {
    for (i = 0; i < 8; i ++)
    {
      sprintf(device, "/dev/usb/lp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct epson:%s \"EPSON\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usb/usblp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct epson:%s \"EPSON\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usblp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct epson:%s \"EPSON\" \"USB Printer #%d\"\n", device, i + 1);
      }
    }
  }
#elif defined(__sgi)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */
  inventory_t	*inv;		/* Hardware inventory info */


 /*
  * IRIX maintains a hardware inventory of most devices...
  */

  setinvent();

  while ((inv = getinvent()) != NULL)
  {
    if (inv->inv_class == INV_PARALLEL && inv->inv_type == INV_EPP_ECP_PLP)
    {
     /*
      * Standard parallel port...
      */

      puts("direct epson:/dev/plpbi \"EPSON\" \"Onboard Parallel Port\"");
    }
  }

  endinvent();
#elif defined(__sun)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */


 /*
  * Standard parallel ports...
  */

  for (i = 0; i < 10; i ++)
  {
    sprintf(device, "/dev/ecpp%d", i);
    if (access(device, 0) == 0)
      printf("direct epson:%s \"EPSON\" \"Sun IEEE-1284 Parallel Port #%d\"\n",
             device, i + 1);
  }

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lp%d", i);

    if (access(device, 0) == 0)
      printf("direct epson:%s \"EPSON\" \"PC Parallel Port #%d\"\n",
             device, i + 1);
  }
#elif defined(FreeBSD) || defined(OpenBSD) || defined(NetBSD)
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */


 /*
  * Probe for parallel devices...
  */

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lpt%d", i);
    if ((fd = open(device, O_RDWR)) >= 0)
    {
      close(fd);
      printf("direct epson:%s \"EPSON\" \"Parallel Port #%d (interrupt-driven)\"\n", device, i + 1);
    }

    sprintf(device, "/dev/lpa%d", i);
    if ((fd = open(device, O_RDWR)) >= 0)
    {
      close(fd);
      printf("direct epson:%s \"EPSON\" \"Parallel Port #%d (polled)\"\n", device, i + 1);
    }
  }

 /*
  * Probe for USB devices...
  */

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/ulpt%d", i);
    if ((fd = open(device, O_RDWR)) >= 0)
    {
      close(fd);
      printf("direct epson:%s \"EPSON\" \"USB Port #%d\"\n", device, i + 1);
    }
  }
#endif
}


/*
 * End of "$Id: epson.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $".
 */
