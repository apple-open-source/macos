/*
 *  xjob.c
 *  CUPS_Apple
 *
 *  Created by rich on Fri Jun 14 2002.
 *  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
 *
 */

#include <ctype.h>
#include <errno.h>
#include <libc.h>
#include <unistd.h>

#include <sys/fcntl.h>

#include "../scheduler/mime.h"

#define HAVE_TM_GMTOFF		1

#define ErrorFile		stderr

/*
 * Log levels...
 */

#define L_PAGE		-1	/* Used internally for page logging */
#define L_NONE		0
#define L_EMERG		1	/* Emergency issues */
#define L_ALERT		2	/* Something bad happened that needs attention */
#define L_CRIT		3	/* Critical error but server continues */
#define L_ERROR		4	/* Error condition */
#define L_WARN		5	/* Warning */
#define L_NOTICE	6	/* Normal condition that needs logging */
#define L_INFO		7	/* General information */
#define L_DEBUG		8	/* General debugging */
#define L_DEBUG2	9	/* Detailed debugging */

/*** Variable Types ***/



/*** Prototypes ***/

static void ConvertJob(const char *filename, mime_type_t *inMimeType, const char *options, const char *ppdPath, mime_type_t *outMimeType, mime_t *mimeDatabase, const char *out, const char *userName, const char *jobName, const char *numCopies);
int LogMessage(int level, const char *message,	...);
char *getDateTime(time_t t);
static int start_process(const char *command, const char *const argv[], const char *const envp[], int infd, int outfd, int errfd, int root);
static mime_type_t *getMimeType(mime_t *mimeDatabase, const char *mimeStr);

/* Constants */

char ServerBin[1024] = "/usr/libexec/cups";

static int FilterLevel = 		0;
static const char *RIPCache = 		"8m";
static const char *ServerRoot = 	"/etc/cups";
static const char *TempDir = 		"/tmp";
static const char *DataDir = 		"/usr/share/cups";
static const char *FontPath = 		"/usr/share/cups/fonts";
static const int LogLevel =		L_INFO;

/*** Globals ***/

static int MaxFDs = 0;

/*!
 *
 * convert excercises the cups file conversion facilities outside of the cups daemon.
 * It can string together registered filters to convert a file from one MIME type to
 * another.
 *
 * Usage: %s [-f <input filename>] [-o <output filename>] [-i <input mimetype>] [-j <output mimetype>] [-P <PPD filename>] [-a <attribute string>] [-u] [-U <username>] [-J <jobname] [-c <copies>] [-D]
 * -f <filename>		The file to be converted.
 *				If not specified read from stdin
 *
 * -o <filename>		The file to be generated.
 *				If not specified then the output is written to stdout.
 *
 * -i <mimetype>		The mime type of the input file.
 *				If not specified auto type the file.
 *
 * -j <mimetype>		The mime type for the generated output.
 *				If not specified then PDF is generated.
 *
 * -P <PPD file>		The PPD file to be used for the conversion.
 *				In not specified the generic PPD is used.
 *
 * -a <options>			The string of options to use for the conversion.
 *				If not specified no options are used.
 *
 * -U <username>		A string corresponding to the username submitting the job
 *
 * -J <jobname>			A string corresponding to the name of the print job
 *
 * -c <copies>			A string correspoinding to the number of copies requested.
 *
 * -u				Unlink the PPD file when the conversion is finished.
 *
 * -D				Unlink the input file when the conversion is finished.
 */
int main (int argc, char *argv[]) 
{
    mime_t *mimeDatabase = NULL;
    mime_type_t *inMimeType;
    mime_type_t *outMimeType;
    const char *options = "";
    const char *ppd = "/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/PrintCore.framework/Versions/A/Resources/English.lproj/Generic.ppd";
    const char *outMimeStr = "application/pdf";
    const char *inMimeStr = NULL;		// NULL means auto-detect.
    const char *filename = NULL;		// NULL means read from stdin.
    const char *outFilename = NULL;
    const char *userName = "unknown";		// the username originating the job. This is the default.
    const char *jobName = "unknown";		// the originating job name. This is the default.
    const char *numCopies = "1";		// the number of copies to generate
    int unlinkPPD = FALSE;
    int unlinkInputFile = FALSE;
    char c = 0;
    int status = 0;
    int waitStatus = 0;
    int waitErr = 0;
    
    while ((c = getopt(argc, argv, "f:o:i:j:P:a:uU:J:c:D")) != -1) {
	switch(c) {
	
	 /* The input file to convert.
	  */
	 case 'f':
	    filename = optarg;
	    break;
	 
	 /* The new file to generate.
	  */
	 case 'o':
	    outFilename = optarg;
	    break;
	 
	 /* The mime type of the input file.
	  */
	 case 'i':
	    inMimeStr = optarg;
	    break;
	 
	 /* The mime type of the output file.
	  */
	 case 'j':
	    outMimeStr = optarg;
	    break;
	 
	 /* The PPD file to use during the conversion.
	  */
	 case 'P':
	    ppd = optarg;
	    break;
	 
	 /* The option string of attributes for the conversion.
	  */
	 case 'a':
	    options = optarg;
	    break;
	
	 case 'u':
	    unlinkPPD = TRUE;
	    break;

	 case 'U':
	    userName = optarg;
	    break;

	 case 'J':
	    jobName = optarg;
	    break;

	 case 'c':
	    numCopies = optarg;
	    break;

	 case 'D':
	    unlinkInputFile = TRUE;
	    break;
	
	 case '?':
	 default:
	    fprintf(stderr, "Ignoring unexpected parameter: %s\n", argv[optind - 1]);
	}
	
    }
    
    if (argc != optind) {
	fprintf(stderr, "Usage: %s [-f <input filename>] [-o <output filename>] [-i <input mimetype>] [-j <output mimetype>] [-P <PPD filename>] [-u] [-a <attribute string>] [-U <username>] [-J <jobname] [-c <copies>] [-D]\n", argv[0]);
	status = 1;
     } else {
	char directory[1024]; /* Configuration directory */

	/*
	* Read the MIME type and conversion database...
	*/
	snprintf(directory, sizeof(directory), "%s/filter", ServerBin);

	mimeDatabase = mimeNew();
	mimeMerge(mimeDatabase, ServerRoot, directory);
    
	if (inMimeStr == NULL) {
	    inMimeType = mimeFileType(mimeDatabase, filename);
	} else {
	    inMimeType = getMimeType(mimeDatabase, inMimeStr);
	}
	
	outMimeType = getMimeType(mimeDatabase, outMimeStr);
	
	ConvertJob(filename, inMimeType, options, ppd, outMimeType,  mimeDatabase, outFilename, userName, jobName, numCopies);
	
	/* Wait until all of the children have finished.
	 */
	do {
	    waitErr = wait(&waitStatus);
	} while (waitErr != -1);
    
    }
    
    if (unlinkInputFile && filename && filename[0]){
	unlink(filename);
    }
    
    if (unlinkPPD && ppd != NULL) {
	unlink(ppd);
    }
    
    return status;
}

/*!
 * @function	ConvertJob
 * @abstract	Convert the a file the specified MIME type. The
 *		MIME output is written to stdout.
 */
static void ConvertJob(const char *filename, mime_type_t *inMimeType, const char *options, const char *ppdPath, mime_type_t *outMimeType, mime_t *mimeDatabase, const char *out, const char *userName, const char *jobName, const char *numCopies)
{
  int		i;		/* Looping var */
  int		slot;		/* Pipe slot */
  int		num_filters;	/* Number of filters for job */
  mime_filter_t	*filters;	/* Filters for job */
  char		method[255];	/* Method for output */
  int		pid;		/* Process ID of new filter process */
  int		statusfds[2],	/* Pipes used between the filters and scheduler */
		filterfds[2][2];/* Pipes used between the filters */
  const char	*argv[8];		/* Filter command-line arguments */
  const char  *envp[20];	/* Environment variables */

  char		command[1024],	/* Full path to filter/backend command */
		path[1024],	/* PATH environment variable */
		language[255],	/* LANG environment variable */
		charset[255],	/* CHARSET environment variable */
		classification[1024],	/* CLASSIFICATION environment variable */
		content_type[255],/* CONTENT_TYPE environment variable */
		out_url[1024],
		device_uri[1024],/* DEVICE_URI environment variable */
		ppd[1024],	/* PPD environment variable */
		printer_name[255],/* PRINTER environment variable */
		root[1024],	/* CUPS_SERVERROOT environment variable */
		cache[255],	/* RIP_MAX_CACHE environment variable */
		tmpdir[1024],	/* TMPDIR environment variable */
		ldpath[1024],	/* LD_LIBRARY_PATH environment variable */
		datadir[1024],	/* CUPS_DATADIR environment variable */
		fontpath[1050];	/* CUPS_FONTPATH environment variable */
    int currentCost = 0;
    int id = 1;
    static const char *outputURL = "file://dev/stdout";
    struct rlimit limit;
    static const int filterLimit = 0;	// unlimited;
    
    if (out != NULL) {
	snprintf(out_url, sizeof(out_url), "file:/%s", out);
	outputURL = out_url;
    }
    
 /*
  * Set the maximum number of files...
  */

  getrlimit(RLIMIT_NOFILE, &limit);
  if (limit.rlim_max > FD_SETSIZE)	/* Can't exceed size of FD set! */
    MaxFDs = FD_SETSIZE;
  else
    MaxFDs = limit.rlim_max;

  limit.rlim_cur = MaxFDs;
  setrlimit(RLIMIT_NOFILE, &limit);
      
 /*
  * Figure out what filters are required to convert from
  * the source to the destination type...
  */

  num_filters   = 0;
  currentCost = 0;

  {
   /*
    * Local jobs get filtered...
    */

    filters = mimeFilter(mimeDatabase, inMimeType, outMimeType, &num_filters);

    if (num_filters == 0)
    {
      LogMessage(L_ERROR, "Unable to convert file!");

      return;
    }

   /*
    * Remove NULL ("-") filters...
    */

    for (i = 0; i < num_filters;)
      if (strcmp(filters[i].filter, "-") == 0)
      {
        num_filters --;
	if (i < num_filters)
	  memcpy(filters + i, filters + i + 1,
	         (num_filters - i) * sizeof(mime_filter_t));
      }
      else
        i ++;

    if (num_filters == 0)
    {
      free(filters);
      filters = NULL;
    }
    else
    {
     /*
      * Compute filter cost...
      */

      for (i = 0; i < num_filters; i ++)
	currentCost += filters[i].cost;
    }
  }

 /*
  * See if the filter cost is too high...
  */

  if ((FilterLevel + currentCost) > filterLimit && FilterLevel > 0 &&
      filterLimit > 0)
  {
   /*
    * Don't print this job quite yet...
    */

    if (filters != NULL)
      free(filters);

    LogMessage(L_INFO, "Holding job %d because filter limit has been reached.", id);

    return;
  }

  FilterLevel += currentCost;

 /*
  * Build the command-line arguments for the filters.  Each filter
  * has 6 or 7 arguments:
  *
  *     argv[0] = printer
  *     argv[1] = job ID
  *     argv[2] = username
  *     argv[3] = title
  *     argv[4] = # copies
  *     argv[5] = options
  *     argv[6] = filename (optional; normally stdin)
  *
  * This allows legacy printer drivers that use the old System V
  * printing interface to be used by CUPS.
  */

  argv[0] = "tofile";
  argv[1] = "1";
  argv[2] = userName;				
  argv[3] = jobName;
  argv[4] = numCopies;
  argv[5] = options;
  argv[6] = filename;
  argv[7] = NULL;

  LogMessage(L_DEBUG, "StartJob: argv = \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
             argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);

 /*
  * Create environment variable strings for the filters...
  */
    strcpy(language, "LANG=C");
    //File: should get a language code from user's prefs.
    
    strcpy(charset, "utf-8");
    

  snprintf(path, sizeof(path), "PATH=%s/filter:/bin:/usr/bin", ServerBin);
  snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s/%s",
           inMimeType->super,
           inMimeType->type);
  snprintf(device_uri, sizeof(device_uri), "DEVICE_URI=%s", outputURL);
  snprintf(ppd, sizeof(ppd), "PPD=%s", ppdPath);
  snprintf(printer_name, sizeof(printer_name), "PRINTER=%s", ".tofile");
  snprintf(cache, sizeof(cache), "RIP_MAX_CACHE=%s", RIPCache);
  snprintf(root, sizeof(root), "CUPS_SERVERROOT=%s", ServerRoot);
  snprintf(tmpdir, sizeof(tmpdir), "TMPDIR=%s", TempDir);
  snprintf(datadir, sizeof(datadir), "CUPS_DATADIR=%s", DataDir);
  snprintf(fontpath, sizeof(fontpath), "CUPS_FONTPATH=%s", FontPath);

    classification[0] = '\0';
    
  if (getenv("LD_LIBRARY_PATH") != NULL)
    snprintf(ldpath, sizeof(ldpath), "LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH"));
  else
    ldpath[0] = '\0';

  envp[0]  = path;
  envp[1]  = "SOFTWARE=CUPS/1.1";
  envp[2]  = "USER=root";
  envp[3]  = charset;
  envp[4]  = language;
  envp[5]  = "GMT";	//FIX: should getenv("TZ") is possible
  envp[6]  = ppd;
  envp[7]  = root;
  envp[8]  = cache;
  envp[9]  = tmpdir;
  envp[10] = content_type;
  envp[11] = device_uri;
  envp[12] = printer_name;
  envp[13] = datadir;
  envp[14] = fontpath;
  envp[15] = ldpath;
  envp[16] = classification;
  envp[17] = NULL;

  LogMessage(L_DEBUG, "StartJob: envp = \"%s\",\"%s\",\"%s\",\"%s\","
                      "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\","
		      "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
	     envp[0], envp[1], envp[2], envp[3], envp[4],
	     envp[5], envp[6], envp[7], envp[8], envp[9],
	     envp[10], envp[11], envp[12], envp[13], envp[14],
	     envp[15], envp[16]);



 /*
  * Now create processes for all of the filters...
  */
#if 0
  if (pipe(statusfds))
  {
    LogMessage(L_ERROR, "Unable to create job status pipes - %s.",
	       strerror(errno));
    //snprintf(printer->state_message, sizeof(printer->state_message),
    //         "Unable to create status pipes - %s.", strerror(errno));
    return;
  }

  LogMessage(L_DEBUG, "StartJob: statusfds = %d, %d",
             statusfds[0], statusfds[1]);

  *statusFD   = statusfds[0];
#else
    statusfds[0] = -1;
    statusfds[1] = STDERR_FILENO;
#endif

  filterfds[1][0] = open("/dev/null", O_RDONLY);
  filterfds[1][1] = -1;

  LogMessage(L_DEBUG, "StartJob: filterfds[%d] = %d, %d", 1, filterfds[1][0],
             filterfds[1][1]);

  for (i = 0, slot = 0; i < num_filters; i ++)
  {
    if (filters[i].filter[0] != '/')
      snprintf(command, sizeof(command), "%s/filter/%s", ServerBin,
               filters[i].filter);
    else
    {
      strncpy(command, filters[i].filter, sizeof(command) - 1);
      command[sizeof(command) - 1] = '\0';
    }


    if (i < (num_filters - 1) ||
	strncmp(outputURL, "file:", 5) != 0)
      pipe(filterfds[slot]);
    else
    {
      filterfds[slot][0] = -1;
      if (strncmp(outputURL, "file:/dev/", 10) == 0)
	filterfds[slot][1] = open(outputURL + 5,
	                          O_WRONLY | O_EXCL);
      else
	filterfds[slot][1] = open(outputURL + 5,
	                          O_WRONLY | O_CREAT | O_TRUNC, 0600);
    }


    LogMessage(L_DEBUG, "StartJob: filter = \"%s\"", command);
    LogMessage(L_DEBUG, "StartJob: filterfds[%d] = %d, %d",
               slot, filterfds[slot][0], filterfds[slot][1]);

    pid = start_process(command, argv, envp, filterfds[!slot][0],
                        filterfds[slot][1], statusfds[1], 0);

    close(filterfds[!slot][0]);
    close(filterfds[!slot][1]);

    if (pid == 0)
    {
      LogMessage(L_ERROR, "Unable to start filter \"%s\" - %s.",
                 filters[i].filter, strerror(errno));
      //snprintf(printer->state_message, sizeof(printer->state_message),
      //         "Unable to start filter \"%s\" - %s.",
      //         filters[i].filter, strerror(errno));
      return;
    }


    LogMessage(L_INFO, "Started filter %s (PID %d).",
               command, pid);

    argv[6] = NULL;
    slot    = !slot;
  }

  if (filters != NULL)
    free(filters);

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(outputURL, "file:", 5) != 0)
  {
    sscanf(outputURL, "%254[^:]", method);
    snprintf(command, sizeof(command), "%s/backend/%s", ServerBin, method);

    argv[0] = outputURL;

    filterfds[slot][0] = -1;
    filterfds[slot][1] = open("/dev/null", O_WRONLY);

    LogMessage(L_DEBUG, "StartJob: backend = \"%s\"", command);
    LogMessage(L_DEBUG, "StartJob: filterfds[%d] = %d, %d",
               slot, filterfds[slot][0], filterfds[slot][1]);

    pid = start_process(command, argv, envp, filterfds[!slot][0],
			filterfds[slot][1], statusfds[1], 1);

    close(filterfds[!slot][0]);
    close(filterfds[!slot][1]);

    if (pid == 0)
    {
      LogMessage(L_ERROR, "Unable to start backend \"%s\" - %s.",
                 method, strerror(errno));
      //snprintf(printer->state_message, sizeof(printer->state_message),
      //         "Unable to start backend \"%s\" - %s.", method, strerror(errno));
      return;
    }
    else
    {

      LogMessage(L_INFO, "Started backend %s (PID %d).", command, pid);
    }
  }
  else
  {
    filterfds[slot][0] = -1;
    filterfds[slot][1] = -1;

    close(filterfds[!slot][0]);
    close(filterfds[!slot][1]);
  }

  close(filterfds[slot][0]);
  close(filterfds[slot][1]);

  close(statusfds[1]);

  //LogMessage(L_DEBUG2, "StartJob: Adding fd %d to InputSet...", current->pipe);

  //FD_SET(current->pipe, &InputSet);


}

int				/* O - 1 on success, 0 on error */
LogMessage(int        level,	/* I - Log level */
           const char *message,	/* I - printf-style message string */
	   ...)			/* I - Additional args as needed */
{
  int		len;		/* Length of message */
  char		line[1024];	/* Line for output file */
  va_list	ap;		/* Argument pointer */
  static char	levels[] =	/* Log levels... */
		{
		  ' ',
		  'X',
		  'A',
		  'C',
		  'E',
		  'W',
		  'N',
		  'I',
		  'D',
		  'd'
		};
#ifdef HAVE_VSYSLOG
  static int	syslevels[] =	/* SYSLOG levels... */
		{
		  0,
		  LOG_EMERG,
		  LOG_ALERT,
		  LOG_CRIT,
		  LOG_ERR,
		  LOG_WARNING,
		  LOG_NOTICE,
		  LOG_INFO,
		  LOG_DEBUG,
		  LOG_DEBUG
		};
#endif /* HAVE_VSYSLOG */


 /*
  * See if we want to log this message...
  */

  if (level > LogLevel)
    return (1);

 /*
  * Print the log level and date/time...
  */

  fprintf(ErrorFile, "%c %s ", levels[level], getDateTime(time(NULL)));

 /*
  * Then the log message...
  */

  va_start(ap, message);
  len = vsnprintf(line, sizeof(line), message, ap);
  va_end(ap);

 /*
  * Then a newline...
  */

  fputs(line, ErrorFile);
  if (len > 0 && line[len - 1] != '\n')
    putc('\n', ErrorFile);

  fflush(ErrorFile);

  return (1);
}

/*
 * 'getDateTime()' - Returns a pointer to a date/time string.
 */

char *				/* O - Date/time string */
getDateTime(time_t t)		/* I - Time value */
{
  struct tm	*date;		/* Date/time value */
  static char	s[1024];	/* Date/time string */
  static const char *months[12] =/* Months */
		{
		  "Jan",
		  "Feb",
		  "Mar",
		  "Apr",
		  "May",
		  "Jun",
		  "Jul",
		  "Aug",
		  "Sep",
		  "Oct",
		  "Nov",
		  "Dec"
		};


 /*
  * Get the date and time from the UNIX time value, and then format it
  * into a string.  Note that we *can't* use the strftime() function since
  * it is localized and will seriously confuse automatic programs if the
  * month names are in the wrong language!
  *
  * Also, we use the "timezone" variable that contains the current timezone
  * offset from GMT in seconds so that we are reporting local time in the
  * log files.  If you want GMT, set the TZ environment variable accordingly
  * before starting the scheduler.
  *
  * (*BSD and Darwin store the timezone offset in the tm structure)
  */

  date = localtime(&t);

  snprintf(s, sizeof(s), "[%02d/%s/%04d:%02d:%02d:%02d %+03ld%02ld]",
	   date->tm_mday, months[date->tm_mon], 1900 + date->tm_year,
	   date->tm_hour, date->tm_min, date->tm_sec,
#ifdef HAVE_TM_GMTOFF
           date->tm_gmtoff / 3600, (date->tm_gmtoff / 60) % 60);
#else
           timezone / 3600, (timezone / 60) % 60);
#endif /* HAVE_TM_GMTOFF */
 
  return (s);
}

/*
 * 'start_process()' - Start a background process.
 */

static int				/* O - Process ID or 0 */
start_process(const char *command,	/* I - Full path to command */
              const char       *const argv[],	/* I - Command-line arguments */
	      const char       *const envp[],	/* I - Environment */
              int        infd,		/* I - Standard input file descriptor */
	      int        outfd,		/* I - Standard output file descriptor */
	      int        errfd,		/* I - Standard error file descriptor */
	      int        root)		/* I - Run as root? */
{
  int	fd;				/* Looping var */
  int	pid;				/* Process ID */


  LogMessage(L_DEBUG, "start_process(\"%s\", %08x, %08x, %d, %d, %d)",
             command, argv, envp, infd, outfd, errfd);

  if ((pid = fork()) == 0)
  {
   /*
    * Child process goes here...
    *
    * Update stdin/stdout/stderr as needed...
    */

    close(0);
    dup(infd);
    close(1);
    dup(outfd);
    if (errfd > 2)
    {
      close(2);
      dup(errfd);
    }

   /*
    * Close extra file descriptors...
    */

    for (fd = 3; fd < MaxFDs; fd ++)
      close(fd);

#if 0
   /*
    * Change user to something "safe"...
    */

    if (!root && getuid() == 0)
    {
     /*
      * Running as root, so change to non-priviledged user...
      */

      if (setgid(Group))
        exit(errno);

      if (setuid(User))
        exit(errno);
    }
#endif

#if CHANGE_PERM
   /*
    * Reset group membership to just the main one we belong to.
    */

    setgroups(0, NULL);

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);
#endif

   /*
    * Execute the command; if for some reason this doesn't work,
    * return the error code...
    */

    execve(command, (char *const*) argv, (char *const*) envp);

    perror(command);

    exit(errno);
  }
  else if (pid < 0)
  {
   /*
    * Error - couldn't fork a new process!
    */

    LogMessage(L_ERROR, "Unable to fork %s - %s.", command, strerror(errno));

    return (0);
  }

  return (pid);
}

static mime_type_t *getMimeType(mime_t *mimeDatabase, const char *mimeStr)
{
    char super[MIME_MAX_SUPER];
    char type[MIME_MAX_TYPE];
    const char *inStr = NULL;
    char *inSuper = super;
    char *inType = type;
    
    inStr = mimeStr;
    while (*inStr != '/' && *inStr != '\0' && (inSuper - super + 1) < sizeof(super)) {
	*inSuper++ = tolower(*inStr++);
    }
    *inSuper = '\0';
    
    if (*inStr != '/') {
	fprintf(stderr, "Invalid format for mime type: %s\n", mimeStr);
    } else {
	++inStr;
	while(*inStr != '\0' && (type - type + 1) < sizeof(type)) {
	    *inType++ = tolower(*inStr++);
	}
	*inType++ = '\0';
    }
    
    return mimeType(mimeDatabase, super, type);

}

