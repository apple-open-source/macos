/*
 *   CANON backend for the Common UNIX Printing System.
 *
 *
 * Contents:
 *
 *   main()         - Send a file to the specified parallel port.
 *   list_devices() - List all parallel devices.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

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
		error,		/* Last error */
		backchannel;	/* Read backchannel data? */
  struct sockaddr_in addr;	/* Socket address */
  struct hostent *hostaddr;	/* Host address */
  int		wbytes;		/* Number of bytes written */
  int		nbytes,		/* Number of bytes read */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192],	/* Output buffer */
		*bufptr;	/* Pointer into buffer */
  struct timeval timeout;	/* Timeout for select() */
  fd_set	input;		/* Input set for select() */
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
    fputs("Usage: canon job-id user title copies options [file]\n", stderr);
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
      port = 9100;	/* Default for CANON NIC */

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
      if ((fd_out = open(resource, O_RDWR | O_EXCL)) == -1)
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

  backchannel = 1;

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
	if ((wbytes = write(fd_out, bufptr, nbytes)) < 0)
	{
	  perror("ERROR: Unable to send print file to printer");
	  break;
	}

	nbytes -= wbytes;
	bufptr += wbytes;
      }

      if (nbytes > 0)
	break;

     /*
      * Check for possible data coming back from the printer...
      */

      if (!backchannel)
        continue;

      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      FD_ZERO(&input);
      FD_SET(fd_out, &input);
      if (select(fd_out + 1, &input, NULL, NULL, &timeout) > 0)
      {
       /*
	* Grab the data coming back and spit it out to stderr...
	*/

	if ((nbytes = read(fd_out, buffer, sizeof(buffer) - 1)) < 0)
	{
	  fprintf(stderr, "ERROR: Back-channel read error - %s!\n",
	          strerror(errno));
          backchannel = 0;
          continue;
	}

       /*
	* Some devices report themselves permanently ready to read...
	*/

	if (nbytes == 0)
	  continue;

        buffer[nbytes] = '\0';
	if (strncmp(buffer, "@BDC ", 5) != 0)
	  fprintf(stderr, "WARNING: Received %d bytes of unknown back-channel data!\n",
	          nbytes);
	else
	{
	 /*
	  * Skip initial report line...
	  */

	  for (bufptr = buffer; *bufptr && *bufptr != '\n'; bufptr ++);

	  if (*bufptr == '\n')
	    bufptr ++;

         /*
	  * Get status data...
	  */

          strcpy(buffer, bufptr);
	  for (bufptr = buffer; *bufptr && *bufptr != ';'; bufptr ++);
	  *bufptr = '\0';

	  if (strncmp(buffer, "IQ:", 3) == 0)
	  {
	   /*
	    * Report ink level...
	    */

            int i;
            int levels[6];

            buffer[12] = '\0'; /* Limit to 6 inks */
	    for (i = 0, bufptr = buffer; i < 6; i ++, bufptr += 2)
	    {
	      if (isalpha(bufptr[0]))
	        levels[i] = (tolower(bufptr[0]) - 'a' + 10) << 16;
	      else
	        levels[i] = (bufptr[0] - '0') << 16;

	      if (isalpha(bufptr[1]))
	        levels[i] |= tolower(bufptr[1]) - 'a' + 10;
	      else
	        levels[i] |= bufptr[1] - '0';
            }

            switch (i)
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
            }
	  }
	  else
	    fprintf(stderr, "INFO: %s\n", buffer);
        }
      }
      else if (argc > 6)
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
      strcpy(model, "CANON");

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
	    strncmp(line, "MODEL:CANON", 13) != 0)
	  strncpy(model, line + 6, sizeof(model) - 1);
	else if (strncmp(line, "MANUFACTURER:", 13) == 0 &&
	         strncmp(line, "MANUFACTURER:CANON", 20) != 0)
	  strncpy(make, line + 13, sizeof(make) - 1);
      }

      fclose(probe);

      if (strcmp(make, "CANON") == 0)
	printf("direct canon:/dev/lp%d \"%s %s\" \"Parallel Port #%d\"\n",
	       i, make, model, i + 1);
    }
    else
    {
      sprintf(device, "/dev/lp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct canon:%s \"CANON\" \"Parallel Port #%d\"\n", device, i + 1);
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

        if (strcmp(make, "CANON") == 0)
	{
          sprintf(device, "/dev/usb/lp%d", i);
	  if (access(device, 0))
	  {
	    sprintf(device, "/dev/usb/usblp%d", i);

	    if (access(device, 0))
	      sprintf(device, "/dev/usblp%d", i);
	  }

	  printf("direct canon:%s \"%s %s\" \"USB Printer #%d\"\n",
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
	printf("direct canon:%s \"CANON\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usb/usblp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct canon:%s \"CANON\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usblp%d", i);
      if ((fd = open(device, O_RDWR)) >= 0)
      {
	close(fd);
	printf("direct canon:%s \"CANON\" \"USB Printer #%d\"\n", device, i + 1);
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

      puts("direct canon:/dev/plpbi \"CANON\" \"Onboard Parallel Port\"");
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
      printf("direct canon:%s \"CANON\" \"Sun IEEE-1284 Parallel Port #%d\"\n",
             device, i + 1);
  }

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lp%d", i);

    if (access(device, 0) == 0)
      printf("direct canon:%s \"CANON\" \"PC Parallel Port #%d\"\n",
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
      printf("direct canon:%s \"CANON\" \"Parallel Port #%d (interrupt-driven)\"\n", device, i + 1);
    }

    sprintf(device, "/dev/lpa%d", i);
    if ((fd = open(device, O_RDWR)) >= 0)
    {
      close(fd);
      printf("direct canon:%s \"CANON\" \"Parallel Port #%d (polled)\"\n", device, i + 1);
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
      printf("direct canon:%s \"CANON\" \"USB Port #%d\"\n", device, i + 1);
    }
  }
#endif
}


/*
 */
