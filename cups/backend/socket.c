/*
 * "$Id: socket.c,v 1.9.2.2 2002/12/05 22:56:05 jlovell Exp $"
 *
 *   AppSocket backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()		- Send a file to the printer or server.
 *   printFile()	- Print a file to a socket.
 *   getSocketOptions()	- Parse command line options.
 *   sendUrgentReset()	- Send a byte of urgent data to reset device.
 *   sighup_handler()	- Signal handler.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 */

static int printFile(int fdin, int fileOut, int socket);
static void getSocketOptions(const char *uri, int *bidiP);
static int sendUrgentReset(int socket);
static void sighup_handler(int sig);

/* This global holds the socket used to send data to the printer.
 * The global is read by the SIGHUP handler in order to send
 * a reset signal to the printer.
 */
static int gSocketOut = -1;

/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  char		method[255],	/* Method in URI */
		hostname[1024],	/* Hostname */
		username[255],	/* Username info (not used) */
		resource[1024];	/* Resource info (not used) */
  int		fp;		/* Print file */
  int		copies;		/* Number of copies to print */
  int		port;		/* Port number */
  int		delay;		/* Delay for retries... */
  int		fd;		/* AppSocket */
  int		error;		/* Error code (if any) */
  struct sockaddr_in addr;	/* Socket address */
  struct hostent *hostaddr;	/* Host address */
  int           bidi;		/* If true send the backchannel data to stdout else stderr. */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGHUP, sighup_handler);

#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);
  action.sa_handler = sighup_handler;
  sigaction(SIGHUP, &action, NULL);
  
#else
  signal(SIGHUP, sighup_handler);
#endif /* HAVE_SIGSET */


 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    puts("network socket \"Unknown\" \"AppSocket/HP JetDirect\"");
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fp     = 0;
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: unable to open print file");
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the hostname and port number from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

  if (port == 0)
    port = 9100;	/* Default to HP JetDirect/Tektronix PhaserShare */

 /*
  * Then try to connect to the remote host...
  */

  if ((hostaddr = httpGetHostByName(hostname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s\n",
            hostname, strerror(errno));
    return (1);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s on port %d\n",
          hostname, port);

  memset(&addr, 0, sizeof(addr));
  memcpy(&(addr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
  addr.sin_family = hostaddr->h_addrtype;
  addr.sin_port   = htons(port);

  while (copies > 0)
  {
    for (delay = 5;;)
    {
      if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
	perror("ERROR: Unable to create socket");
	return (1);
      }

      if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
      {
	error = errno;
	close(fd);
	fd = -1;

	if (error == ECONNREFUSED || error == EHOSTDOWN ||
            error == EHOSTUNREACH)
	{
	  fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in %d seconds...\n",
                  hostname, delay);
	  sleep(delay);

	  if (delay < 30)
	    delay += 5;
	}
	else
	{
	  perror("ERROR: Unable to connect to printer (retrying in 30 seconds)");
	  sleep(30);
	}
      }
      else
	break;
    }

   /*
    * Now that we are "connected" to the port, ignore SIGTERM so that we
    * can finish out any page data the driver sends (e.g. to eject the
    * current page...  Only ignore SIGTERM if we are printing data from
    * stdin (otherwise you can't cancel raw jobs...)
    */

    if (argc < 7)
    {
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
    }

   /*
    * Finally, send the print file...
    */

    copies --;

    if (fp != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fp, 0, SEEK_SET);
    }

    fputs("INFO: Connected to host, sending print job...\n", stderr);

    /* Remember the socket file descriptor in a global so that our
     * HUP signal handler can get to it.
     */
    gSocketOut = fd;
    
    getSocketOptions(argv[0], &bidi);

    if (bidi) fprintf(stderr, "DEBUG: socket bidirectional enabled.\n");

    (void) printFile(fp, bidi ? STDOUT_FILENO : -1, fd);
    
    /* We're done with the socket so we'll clear the global
     * used by the HUP signal handler.
     */
    gSocketOut = -1;
    
   /*
    * Close the socket connection...
    */

    close(fd);
  }

 /*
  * Close the input file and return...
  */

  if (fp != 0)
    close(fp);

  fputs("INFO: Ready to print.\n", stderr);

  return (0);
}

static int printFile(int fdin, int fileOut, int socket)
{
  char fileBuffer[16 * 1024];
  char socketReadBuffer[1 * 1024];
  char *inFileBuffer = NULL;
  fd_set readSet;
  fd_set writeSet;
  ssize_t bytesToSend = 0;
  int val;
  int fileEOFRead = false;
  int maxfdp1 = 0;
  struct timeval timeout90;
  struct timeval *timeout = NULL;
  int numReady = 0;
  int jobFinished = false;
  int err = 0;
        
  maxfdp1 = MAX(fdin, fileOut);
  maxfdp1 = MAX(maxfdp1, socket);
  ++maxfdp1;

  /* Set non-blocking mode on both the file descriptor
   * we will read the print file from and on the
   * socket we will write to and read from.
   */
  val = fcntl(fdin, F_GETFL, 0);
  fcntl(fdin, F_SETFL, val | O_NONBLOCK);
        
  val = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, val | O_NONBLOCK);

  while (jobFinished == false && err == 0) {
  
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    
    /* We're always interested if there are incoming bytes from
     * the socket.
     */
    FD_SET(socket, &readSet);

    /* If we still have bytes to send over the socket then
     * we want to know when we can send more.
     */
    if (bytesToSend > 0)
    {
      FD_SET(socket, &writeSet);
      timeout = NULL;
    }
    
    /* If we have no bytes to send and there is still data coming
     * to us on the file descriptor, then we'll want to know
     * when more data can be read.
     */
    else if (!fileEOFRead)
    {
      FD_SET(fdin, &readSet);
      timeout = NULL;
    }
    
    /* If we're done reading from the file descriptor, then we've shutdown
     * our half of the connection and want to set a timeout while waiting
     * for the other end to shutdown.
     */
    else
    {
      timeout90.tv_sec  = 90;
      timeout90.tv_usec = 0;
      timeout = &timeout90;
    }
   
    /* Wait here for something interesting to happen
     */
#ifdef __hpux
    numReady = select(maxfdp1, (int *) &readSet, &writeSet, 0, timeout);
#else
    numReady = select(maxfdp1, &readSet, &writeSet, 0, timeout);
#endif /* __hpux */
    
    if (numReady > 0)
    {
    
      /* We have data to read from the file input stream.
       */
      if (FD_ISSET(fdin, &readSet))
      {  
        bytesToSend = read(fdin, fileBuffer, sizeof(fileBuffer));

        if (bytesToSend > 0) {
          inFileBuffer = fileBuffer;
        } else if (bytesToSend == 0) {
          fileEOFRead = true;
          /*
           * If we've sent the entire file then
           * shutdown the socket and wait for the other end to finish...
           */
          fputs("INFO: Print file sent, waiting for printer to finish...\n", stderr);
          shutdown(socket, 1);
        } else {
          perror("ERROR: socket reading from input steam");
          err = errno;
          break;
        }
      }
    
      /* The socket output stream is ready for more data.
       */
      else if (FD_ISSET(socket, &writeSet))
      {
          ssize_t bytesSent = write(socket, inFileBuffer, (size_t) bytesToSend);
        
          if (bytesSent >= 0) {
            bytesToSend -= bytesSent;
            inFileBuffer += bytesSent;
         
          } else if (bytesSent == EAGAIN) {
            /* Do nothing, we'll try again later. */
          } else {
            perror("ERROR: socket failed socket write");
            err = errno;
            break;
          }
      }
    
      /* The socket has received data we need to handle.
       */
      if (FD_ISSET(socket, &readSet))
      {
        ssize_t receivedBytes = read(socket, socketReadBuffer, sizeof(socketReadBuffer));

        /* Received data goes to the provided pipe or, if a valid pipe
         * is not supplied, we simply log the data's arrival.
         */
        if (receivedBytes > 0)
        {
          if (fileOut >= 0) {
            write(fileOut, socketReadBuffer, (size_t) receivedBytes);
          } else {
            fprintf(stderr, "DEBUG: Received (and ignored) %lu bytes of back-channel data!\n", (unsigned long) receivedBytes);
          }
        }
        
        /* The remote end shutdown their end of the socket so we're done.
         */
        else if (receivedBytes == 0)
        {
          jobFinished = true;
          
          /* The remote end shutdown before we were done sending.
           */
          if (!fileEOFRead) err = -1;
          
        /* There was an error reading the socket.
         */
        }
        else
        {
          perror("DEBUG: failed to read socket back-channel");
        }
      }
    }
    
    /* If the select timed out, then the remote did not shutdown the connection
     * in a timely manner - but we're done so we'll exit this function.
     */
    else if (numReady == 0)
    {
      jobFinished = true;
    }
    
    /* Otherwise select had an error.
     */
    else
    {
      /* We want to ignore an interrupt error, but
       * we want to quit the loop with any other error.
       */
      if (errno != EINTR)
      {
        perror("ERROR: select");
        err = errno;
      }
    }
      
  }

  fprintf(stderr, "DEBUG: socket finished sending file, err = %d.\n", err);

  return err;
}

static void getSocketOptions(const char *uri, int *bidiP)
{
  char method[255];	/* Method in URI */
  char hostname[1024];	/* Hostname */
  char username[255];	/* Username info (not used) */
  char resource[1024];	/* Resource info (device and options) */
  int port = 0;		/* Port number (not used) */
  char *resourcePtr = NULL;
  char *options = NULL;	/* Pointer to options */
  char optionName[255];	/* Name of option */
  char value[255];	/* Value of option */
  char *ptr = NULL;     /* Pointer into name or value */
  int bidi = false;

  /* Extract the device name and options from the URI...
   */
  method[0] = username[0] = hostname[0] = resource[0] = '\0';
        
  httpSeparate(uri, method, username, hostname, &port, resource);

  /*
   * See if there are any parameters...
   */
  if ((options = strchr(resource, '?')) != NULL)
  {

    /*
     * Yup, terminate the device name string and move to the first
     * character of the options...
     */
    *options++ = '\0';        

    while (*options != '\0')
    {
      /*
      * Get the name...
      */
      for (ptr = optionName; *options && *options != '=';)
        *ptr++ = *options++;

      *ptr = '\0';
                
      if (*options == '=')
      {
        /*
         * Get the value...
         */
        options ++;
                        
        for (ptr = value; *options && *options != '+';)
          *ptr++ = *options++;

        *ptr = '\0';
                        
        if (*options == '+')
          options ++;
          
      }
      else
      {
        value[0] = '\0';
      }
      
      if (strcasecmp(optionName, "bidi") == 0)
      {
        bidi = true;
      }
    } 
  }
  
  if (bidiP) *bidiP = bidi;
  
}

static int sendUrgentReset(int socket)
{
    char resetByte = 0;
    int err = 0;
    
    if (send(socket, &resetByte, sizeof(resetByte), MSG_OOB) < 0) {
        err = errno;
        perror("Failed to send socket reset");
    }
    
    return err;
}

/*
 * 'sighup_handler()' - Handle 'hangup' signals to send a reset.
 */

static void sighup_handler(int sig)
{
  (void)sig;

  /* If it looks like we have a valid socket then send the urgent data.
   */
  if (gSocketOut >= 0)
  {
    sendUrgentReset(gSocketOut);
  }
  
#ifdef HAVE_SIGSET
  sigset(SIGHUP, sighup_handler);
#elif !defined(HAVE_SIGACTION)
  signal(SIGHUP, sighup_handler);
#endif /* HAVE_SIGSET */
}


/*
 * End of "$Id: socket.c,v 1.9.2.2 2002/12/05 22:56:05 jlovell Exp $".
 */
