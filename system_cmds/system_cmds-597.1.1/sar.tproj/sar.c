/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 *  Reserved.
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
*/

/*
  cc -Wall -I. -I ../sadc.tproj -O -o  sar sar.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <mach/mach.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <sadc.h>
#include <sar.h>


#define IFNET_32_BIT_COUNTERS 1

/* Options used only for launching sadc */
int t_interval = 5; 	        /* in seconds              */
char * t_intervalp = "5";
int n_samples  = 1;	        /* number of sample loops  */
char * n_samplesp = "1";

/* Used only for storing the binary output after launching sadc */
char *outfile  = NULL;          /* output file             */
int ofd        = 0;		/* output file descriptor  */

/*
 * When launching sadc, this file descriptor reads sadc's stdout
 *    via pipe.
 * When not launching sadc, this file descriptor will be either
 *    the input file passed in with the -f flag
 *    or the standard input file /var/log/sa/saXX
 */
int ifd        = 0;		/* input file descriptor   */   
char *infile   = NULL;          /* input file              */



/* Used when we have to luanch sadc */
pid_t pid;
int fd[2];  /* read from fd[0], write to fd[1] */

char *optionstring1 =  "Adgn:puo:";
char *optionstring1_usage = "/usr/bin/sar [-Adgpu] [-n { DEV | EDEV | PPP } ] [-o filename] t [n]";
char *optionstring2 = "Adgn:pue:f:i:s:";
char *optionstring2_usage = "/usr/bin/sar [-Adgpu] [-n { DEV | EDEV | PPP }] [-e time] [-f filename] [-i sec] [-s time]";


/* option flags */
int aflag = 0;
int Aflag = 0;
int bflag = 0;
int cflag = 0;
int dflag = 0;  /* drive statistics */
int gflag = 0;  /* page-out activity */
int kflag = 0;
int mflag = 0;

int nflag = 0;  /* network statistics */
int network_mode = 0;
char *sadc_mflagp = "-m";
char *sadc_ppp_modep = "PPP";

int pflag = 0;  /* page-in activity */
int qflag = 0;
int rflag = 0;
int uflag = 0;   /* cpu utilization - this is the only default */
int vflag = 0;
int wflag = 0;
int yflag = 0;
int set_default_flag = 1;
int flag_count = 0;

/*
 *  To get the current time of day in seconds
 *  based on a 24 hour clock, pass in the time_t from time()
 *  the remainder is the current time in seconds
*/
#define HOURS_PER_DAY 24
#define MINS_PER_HOUR 60
#define SECS_PER_MIN 60
#define SECS_PER_DAY (SECS_PER_MIN * MINS_PER_HOUR * HOURS_PER_DAY)

/* end time delimiter -- converted from hh:mm:ss to seconds */
time_t end_time = 0;

int iflag = 0;
int iseconds = 0;  /* interval seconds, default = 0 implies all samples are
		    * printed */

/* start time delimiter -- converted from hh:mm:ss to seconds */
time_t start_time = 0;

int oflag = 0;
int fflag = 0;

/* stat records average and previous */
struct vm_statistics       prev_vmstat,  avg_vmstat, cur_vmstat;
host_cpu_load_info_data_t  prev_cpuload, avg_cpuload, cur_cpuload;
struct drivestats_report   *dr_head = NULL;

/* internal table of drive path mappings */
struct drivepath *dp_table = NULL;
int dp_count = 0;

/* internal table of network interface statistics */
struct netstats_report *nr_table = NULL;
int nr_count;
struct netstats *netstat_readbuf = NULL;
size_t netstat_readbuf_size = 0;

int avg_counter = 0;
int avg_interval = 0;

extern int errno;

/* Forward function declarations */
static void exit_usage();
static void open_output_file(char *path);
static void open_input_file(char *path);
static void read_record_hdr(struct record_hdr *hdr, int writeflag);
static void read_record_data(char *buf, size_t size, int writeflag);
static void write_record_hdr(struct record_hdr *hdr);
static void write_record_data(char *buf, size_t size);
static time_t convert_hms(char *string);
static char *get_hms_string(time_t, char *);
static int find_restart_header(struct record_hdr *);
static void print_all_column_headings (time_t timestamp);
static void print_column_heading (int type, char *timebufptr, int mode);
static void read_sample_set(int, time_t, struct record_hdr *);
static void do_main_workloop();
static int bypass_sample_set(struct record_hdr *, time_t);
static void skip_data(int);
static int get_cpu_sample(int flag, struct record_hdr *hdr);
static void print_cpu_sample(char *timebufptr);
static int get_vmstat_sample(int flag, struct record_hdr *hdr);
static void print_vmstat_sample(char *timebufptr);

static int get_drivestats_sample(int flag, struct record_hdr *hdr);
static void init_drivestats(struct drivestats_report *dr);
static void print_drivestats_sample(char *timebufptr);
static int get_drivepath_sample(int flag, struct record_hdr *hdr);

static void set_cur_netstats(struct netstats_report *nr, struct netstats *ns);
static void init_prev_netstats(struct netstats_report *nr);
static int get_netstats_sample(int flag, struct record_hdr *hdr);
static void print_netstats_sample(char *timebufptr);
    
static void exit_average();

int
main(argc, argv)
    int argc;
    char *argv[];
{

    char    ch;

    time_t curr_time;		/* current time in seconds */
    char timebuf[26];
    char filenamebuf[20];
    char *optstring = NULL;
    int optstringval;
    int i;
    
    /*
     * Detirmine which option string to use
     */

    optreset=0;
    optstringval=0;
    
    while((ch=getopt(argc, argv, "aAbcdgkmn:pqruvwyo:e:f:i:s:")) != EOF) {
	switch(ch) {
	case 'o':
	    if (optstringval == 2)
		exit_usage();
	    optstring=optionstring1;
	    optstringval=1;
	    break;
	case 'e':
	case 'f':
	case 'i':
	case 's':
	    if (optstringval == 1)
		exit_usage();
	    optstring=optionstring2;
	    optstringval=2;
	    break;
	default:
	    /* ignore for now */
	    break;
	}
    }

    if (!optstring)
    {
	/* still trying to determine which option string to use */
	if (argc - optind > 0)
	{
	    optstring=optionstring1;  /* we should have a t_second value */
	    optstringval=1;
	}
	else
	{
	    optstring=optionstring2;
	    optstringval=2;
	}
    }

    optreset = optind = 1;
    while ((ch=getopt(argc, argv, optstring)) != EOF) {
	switch (ch) {
	case 'a':
	    aflag = 1;
	    set_default_flag = 0;
	    flag_count++;
	    break;
	case 'A':
	    Aflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'b':
	    bflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'c':
	    cflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'd':
	    dflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'g':
	    gflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'k':
	    kflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'm':
	    mflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'n':
	    nflag= 1;
	    if (!strncmp(optarg, "PPP", 3))
		network_mode |= NET_PPP_MODE;
	    else if (!strncmp(optarg, "DEV", 3))
		network_mode |= NET_DEV_MODE;
	    else if (!strncmp(optarg, "EDEV", 4))
		network_mode |= NET_EDEV_MODE;
	    else
		exit_usage();
	    set_default_flag = 0;
	    flag_count++;
	    break;	    
	case 'p':
	    pflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'q':
	    qflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'r':
	    rflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'u':
	    uflag= 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'v':
	    vflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'w':
	    wflag = 1;
	    set_default_flag = 0;
	    flag_count++;
	    break;
	case 'y':
	    yflag = 1;
	    set_default_flag = 0;
	    flag_count++;	    
	    break;
	case 'o':
	    /* open the output file */
	    oflag = 1;
	    outfile=optarg;
	    (void)open_output_file(outfile);	    
	    break;
	case 'e':  /* eflag */
	    end_time = convert_hms(optarg);
	    break;
	case 'f':
	    fflag = 1;
	    infile=optarg;	    
	    break;
	case 'i':
	    iflag = 1;
	    iseconds=atoi(optarg);
	    break;
	case 's':
	    start_time = convert_hms(optarg);
	    break;
	default:
	    exit_usage();
	    break;
	}
    }

    /* setup default uflag option */
    if (Aflag)
    {
	dflag = gflag = pflag = uflag = 1;
	if (!nflag)
	{
	    /*
	     * Add network stats to the load
	     * but avoid PPP data by default.
	     */
	    nflag = 1;
	    network_mode = NET_DEV_MODE | NET_EDEV_MODE;;
	}
	flag_count = 2;	  /* triggers column headings */
    }
    else if (set_default_flag)
    {
	uflag=1;
	flag_count++;
    }

    if (nflag)
    {
	if (network_mode & NET_PPP_MODE)
	{
	    if (!(network_mode & NET_DEV_MODE) &&
	      !(network_mode & NET_EDEV_MODE))
	    {
		/* set defaults */
		network_mode |= NET_DEV_MODE;
		network_mode |= NET_EDEV_MODE;
		flag_count++;
	    }
	}
    }

    argc -= optind;
    argv += optind;

    /* set up signal handlers */
    signal(SIGINT,  exit_average);
    signal(SIGQUIT, exit_average);
    signal(SIGHUP,  exit_average);
    signal(SIGTERM, exit_average); 

    if (optstringval == 1)
    {
	/* expecting a time interval */
	
	char *p;

	if (argc >= 1)
	{
	    errno = 0;
	    t_interval = strtol(argv[0], &p, 0);
	    t_intervalp = argv[0];
	    if (errno || (*p != '\0') || t_interval <= 0 )
		exit_usage();
	    if (argc >= 2)
	    {
		errno=0;
		n_samples = strtol(argv[1], &p, 0);
		n_samplesp = argv[1];
		if (errno || (*p != '\0') || n_samples <= 0)
		    exit_usage();
	    }
	}
    }

    /* where does the input come from */
    if (fflag)
    {
	(void)open_input_file(infile);
    }
    else if (optstringval == 2)
    {
	/*
	 * Create a filename of the form /var/log/sa/sadd
	 * where "dd" is the date of the month
	 */
	curr_time = time((time_t *)0);        /* returns time in seconds */

	/*
	  timebuf will be a 26-character string of the form:
	  Thu Nov 24 18:22:48 1986\n\0
	*/

	ctime_r(&curr_time, timebuf);
	strncpy(filenamebuf, "/var/log/sa/sa", 14);
	strncpy(&filenamebuf[14], &timebuf[8], 2);
	if (filenamebuf[14] == ' ')
	    filenamebuf[14] = '0';
	filenamebuf[16]='\0';
	infile = filenamebuf;
	(void)open_input_file(infile);
    }
    else if (optstringval == 1)
    {
	/* launch sadc */
	if (pipe(fd) == -1)
	{
	    fprintf(stderr, "sar: pipe(2) failed, errno = (%d)\n",errno);
	    exit(1);
	}

	if ((pid=fork()) == 0)
	{
#if 0
	    int efd;
#endif
	    int fdlimit = getdtablesize();
	    
            /* This is the child */
	    /* Close all file descriptors except the one we need */
	    
	    for (i=0; i < fdlimit; i++) {
		if ((i != fd[0]) && (i != fd[1]))
		    (void)close(i);
	    }
#if 0
	    efd = open("/tmp/errlog", O_CREAT|O_APPEND|O_RDWR, 0666);
	    if (dup2(efd,2) == -1) {
		exit(1);
	    }
#endif
	    /* Dup the two file descriptors to stdin and stdout */
	    if (dup2(fd[0],0) == -1) {
		exit(1);
	    }
	    if (dup2(fd[1],1) == -1) {
		exit(1);
	    }
	    /* Exec the child process */
	    if (network_mode & NET_PPP_MODE)
		execl("/usr/lib/sa/sadc", "sadc", sadc_mflagp, sadc_ppp_modep, t_intervalp, n_samplesp, NULL);
	    else
		execl("/usr/lib/sa/sadc", "sadc", t_intervalp, n_samplesp, NULL);		    

	    perror("execlp sadc");
	    exit(2); /* This call of exit(2) should never be reached... */
	}
	else
	{	 /* This is the parent */
	    if (pid == -1) {
		fprintf(stderr, "sar: fork(2) failed, errno = (%d)\n",errno);
		exit(1);
	    }
	    close (fd[1]);  /* parent does not write to the pipe */
	    ifd = fd[0];    /* parent will read from the pipe */
	}	
    }
    else
    {
	/* we're confused about source of input data - bail out */
	fprintf(stderr, "sar: no input file recognized\n");
	exit_usage();
    }

    /* start reading input data and format the output */
    (void)do_main_workloop();
    (void)exit_average();
    exit(0);
}

static void
exit_usage()
{
    fprintf(stderr, "\n%s\n\n", optionstring1_usage);
    fprintf(stderr, "%s\n",   optionstring2_usage);
    exit(EXIT_FAILURE);
}

static void
open_output_file(char *path)
{
    if ((ofd = open(path, O_CREAT|O_APPEND|O_TRUNC|O_WRONLY, 0664)) == -1 )
    {
	/* failed to open path */
	fprintf(stderr, "sar: failed to open output file [%s]\n", path);
	exit_usage();
    }
}


static void
open_input_file(char *path)
{
    if ((ifd = open(path, O_RDONLY, 0)) == -1)
    {
	/* failed to open path */
	fprintf(stderr, "sar: failed to open input file [%d][%s]\n", ifd, path);
	exit_usage();
    }
}

static void
read_record_hdr(hdr, writeflag)
    struct record_hdr *hdr;
    int writeflag; 
{
    errno = 0;
    int num = 0;
    int n = 0;
    size_t size = 0;

    size = sizeof(struct record_hdr);

    while (size)
    {
	num = read(ifd, &hdr[n], size);	
	if (num > 0)
	{
	    n += num;
	    size -= num;
	}
	else if (num == 0)
	    exit_average();
	else
	{
	    fprintf(stderr, "sar: read_record_data failed, errno=%d num=%d, size=%d\n", (int)errno, (int)num, (int)size);
	    exit(EXIT_FAILURE);
	}
    }
    
    if (oflag && writeflag)
	write_record_hdr(hdr);
    
    return;	
}

static void
read_record_data(buf, size, writeflag)
    char *  buf;
    size_t  size;
    int     writeflag;
{
    errno = 0;
    size_t num = 0;
    size_t n = 0;

    while (size)
    {
	num = read(ifd, &buf[n], size);	
	if (num > 0)
	{
	    n += num;
	    size -= num;
	}
	else if (num == 0)   /* EOF */
	    exit_average();	
	else
	{
	    fprintf(stderr, "sar: read_record_data failed, errno=%d num=%d, size=%d\n", (int)errno, (int)num, (int)size);
	    exit(EXIT_FAILURE);
	}
    }

    if (oflag && writeflag)
	write_record_data(buf, n);
    
    return;
}

static void
write_record_hdr(hdr)
    struct record_hdr *hdr;
{
    errno = 0;
    int num;
    
    if ((num = write(ofd, hdr, sizeof(struct record_hdr))) == -1)
    {
	fprintf(stderr, "sar: write_record_hdr failed, errno=%d\n", errno);	
	exit(EXIT_FAILURE);
    }
    return;
}

static void
write_record_data(char *buf, size_t nbytes)
{
    errno = 0;
    int num;
    if ((num = write(ofd, buf, nbytes)) == -1)
    {
	fprintf(stderr, "sar: write_record_data failed, errno=%d\n", errno);
	exit(EXIT_FAILURE);
    }
    return;	
}

/*
 * Convert a string of one of the forms
 *      hh   hh:mm     hh:mm:ss
 * into the number of seconds.
 * exit on error
*/

static time_t
convert_hms(string)
    char *string;
{
    int hh = 0;   /* hours */
    int mm = 0;   /* minutes */
    int ss = 0;   /* seconds */
    time_t seconds;
    time_t timestamp;
    struct tm *tm;
    int i;

    if (string == NULL || *string == '\0')
	goto convert_err;

    for (i=0; string[i] != '\0'; i++)
    {
	if ((!isdigit(string[i])) && (string[i] != ':'))
	{
	    goto convert_err;
	}
    }

    if (sscanf(string, "%d:%d:%d", &hh, &mm, &ss) != 3)
    {
	if (sscanf(string, "%d:%d", &hh, &mm) != 2)
	{
	    if (sscanf(string, "%d", &hh) != 1)
	    {
		goto convert_err;
	    }
	}
    }

    if (hh < 0 || hh >= HOURS_PER_DAY ||
      mm < 0 || mm >= MINS_PER_HOUR ||
      ss < 0 || ss > SECS_PER_MIN)
    {
	goto convert_err;
    }

    seconds = ((((hh * MINS_PER_HOUR) + mm) * SECS_PER_MIN) + ss);
    timestamp = time((time_t *)0);
    tm=localtime(&timestamp);
    seconds -= tm->tm_gmtoff;
    
    return(seconds);   
    
    convert_err:
    fprintf(stderr, "sar: time format usage is hh[:mm[:ss]]\n");
    exit_usage();
    return(0);
}


/*
 * Use ctime_r to convert a time value into
 * a 26-character string of the form:
 *
 * Thu Nov 24 18:22:48 1986\n\0
 */

static char *
get_hms_string(tdata, tbuf)
    time_t tdata;
    char *tbuf;
{
    time_t t;
    char *p;

    t = tdata;
    ctime_r(&t, tbuf);
    p=&tbuf[11];
    tbuf[19] = 0;

    return(p);
}
    

/* sample set flags */
#define INIT_SET   0
#define PRINT_SET  1
#define PRINT_AVG  2

static void
do_main_workloop()
{
    struct record_hdr hdr;
    time_t cur_timestamp = 0;	/* seconds - Coordinated Universal Time */
    time_t next_timestamp = 0;  /* seconds - Coordinated Universal Time */

    if (!find_restart_header(&hdr))
	exit(1);

    cur_timestamp = hdr.rec_timestamp;

    /* convert sflag's start_time from 24 hour clock time to UTC seconds */
    if (start_time  < (cur_timestamp % SECS_PER_DAY))
	start_time = cur_timestamp;
    else
	start_time += cur_timestamp - (cur_timestamp % SECS_PER_DAY);

    /* convert end_time, from 24 hour clock time to UTC seconds */
    if (end_time != 0)
	end_time += cur_timestamp - (cur_timestamp % SECS_PER_DAY);

#if 0
	fprintf(stderr, "start = %ld, end = %ld, cur=%ld, [24hour - %ld]\n",
	  start_time, end_time, cur_timestamp,(cur_timestamp % SECS_PER_DAY));
#endif

    while (cur_timestamp < start_time)
    {
	bypass_sample_set(&hdr, cur_timestamp);
	cur_timestamp = hdr.rec_timestamp;
    }

    next_timestamp = cur_timestamp + iseconds;
    print_all_column_headings(cur_timestamp);
    read_sample_set(INIT_SET, cur_timestamp, &hdr);
    cur_timestamp = hdr.rec_timestamp;    

    while ((end_time == 0) || (next_timestamp < end_time))
    {
	if (cur_timestamp < next_timestamp)
	{
	    bypass_sample_set (&hdr, cur_timestamp);
	    cur_timestamp = hdr.rec_timestamp;
	}
	else
	{
	    /* need to know the seconds interval when printing averages */
	    if (avg_interval == 0)
	    {
		if (iseconds)
		    avg_interval = iseconds;
		else
		    avg_interval = cur_timestamp - next_timestamp;
	    }
	    next_timestamp = cur_timestamp + iseconds;	    
	    read_sample_set(PRINT_SET, cur_timestamp, &hdr);
	    cur_timestamp = hdr.rec_timestamp;	    
	}
    }
    exit_average();
}


/*
 * Find and fill in a restart header.  We don't write
 * the binary data when looking for SAR_RESTART.
 * Return:  1 on success
 *          0 on failure
 */
static int
find_restart_header (ret_hdr)
    struct record_hdr *ret_hdr;
{
    struct record_hdr hdr;
    int bufsize = 0;
    char *buf = NULL;

    errno = 0;
    
    restart_loop:
    read_record_hdr(&hdr, FALSE);   /* exits on error */
    
    if (hdr.rec_type == SAR_RESTART)
    {
	*ret_hdr = hdr;
	if (oflag)
	    write_record_hdr(&hdr);   /* writes the RESTART record */
	if (buf)
	    free(buf);
	return(1);
    }

    /*
     * not the record we want...
     * read past data and try again
     */
    if (hdr.rec_count)
    {
	if (fflag)
	{ /* seek past data in the file */
	    if ((lseek(ifd, (hdr.rec_count * hdr.rec_size), SEEK_CUR)) == -1)
	    {
		/*exit on error */
		fprintf(stderr, "sar: lseek failed, errno=%d\n", errno);
		exit(EXIT_FAILURE);
	    }
	    
	}	    
	/* compute data size - malloc a new buf if it's not big enough */
	else 
	{
	    /* have to read from the pipe */
	    if (bufsize < (hdr.rec_count * hdr.rec_size))
	    {
		if (buf)
		    free(buf);
		bufsize = hdr.rec_count * hdr.rec_size;
		if((buf = (char *)malloc(bufsize)) == NULL)
		{
		    fprintf(stderr, "sar: malloc failed\n");
		    return(0);
		}
	    }
	    /* exits on error */
	    read_record_data(buf, (hdr.rec_count * hdr.rec_size), FALSE);
	}
    }
    goto restart_loop;
}

static void
print_all_column_headings(timestamp)
    time_t timestamp;
{
    char timebuf[26];
    char *timebufp;

    timebufp = get_hms_string (timestamp, timebuf);

    if (uflag) /* print cpu headers */
	print_column_heading(SAR_CPU, timebufp, 0);

    if (gflag) 	/* print page-out activity */
	print_column_heading(SAR_VMSTAT, timebufp, 0);

    if (pflag ) /* print page-in activity */	
	print_column_heading(SAR_VMSTAT, timebufp, 1);

    if (dflag) /* print drive stats */
	print_column_heading(SAR_DRIVESTATS, timebufp, 0);

    if (nflag) /* print network stats */
    {
	if (network_mode & NET_DEV_MODE)
	    print_column_heading(SAR_NETSTATS, timebufp, NET_DEV_MODE);	    

	if (network_mode & NET_EDEV_MODE)
	    print_column_heading(SAR_NETSTATS, timebufp, NET_EDEV_MODE);	    	    
    }
}	


/*
 * Find and fill in a timestamp header.
 * Write the binary data when looking for SAR_TIMESTAMP
 * Don't do anything with the data, just read past it.
 * Return:  1 on success
 *          0 on failure
 */
static int
bypass_sample_set (ret_hdr, timestamp)
    struct record_hdr *ret_hdr;
    time_t timestamp;
{
    struct record_hdr hdr;
    int bufsize = 0;
    char *buf = NULL;

    bypass_loop:
    read_record_hdr(&hdr, TRUE);   /* exits on error */
    
    if (hdr.rec_type == SAR_TIMESTAMP)
    {
	*ret_hdr = hdr;
	if (buf)
	    free(buf);
	return(1);
    }

    /*
     * not the record we want...
     * read past data and try again
     */
    if (hdr.rec_count)
    {
	if (fflag && !oflag)
	{
	    /*
	     * we're reading from a file and we don't have to write the
	     * binary data so seek past data in the file
	     */
	    errno = 0;
	    if ((lseek(ifd, (hdr.rec_count * hdr.rec_size), SEEK_CUR)) == -1)
	    {
		/*exit on error */
		fprintf(stderr, "sar: lseek failed, errno=%d\n", errno);
		exit(EXIT_FAILURE);
	    }	    
	}
	else
	{
	    /*
	     * We end up here when reading from pipe.
	     * malloc a new buffer if current is not big enough
	     */
	    if (bufsize < (hdr.rec_count * hdr.rec_size))
	    {
		if (buf)
		    free(buf);
		bufsize = hdr.rec_count * hdr.rec_size;
		if((buf = (char *)malloc(bufsize)) == NULL)
		{
		    fprintf(stderr, "sar: malloc failed\n");
		    exit(EXIT_FAILURE);
		}
	    }

	    /* exits on error */
	    read_record_data(buf, (hdr.rec_count * hdr.rec_size), TRUE);
	}
    } /* end if hdr.rec_count */
    goto bypass_loop;
}


/*
 * INIT_SET: This initializes the first sample for each type.
 * PRINT_SET: This read, compute and print out sample data.
 */
static void
read_sample_set(flag, timestamp, ret_hdr)
    int flag;
    time_t timestamp;
    struct record_hdr *ret_hdr;
{
    struct record_hdr hdr;
    char timebuf[26];
    char *timebufp;
    char *indent_string;
    char *indent_string_wide;
    char *indent_string_narrow;
    int sar_cpu = 0;
    int sar_vmstat=0;
    int sar_drivestats=0;
    int sar_drivepath=0;
    int sar_netstats = 0;

    indent_string_wide = "          ";
    indent_string_narrow = "  ";    
    indent_string = indent_string_narrow;
    
    read_record_hdr(&hdr, TRUE);

    while (hdr.rec_type != SAR_TIMESTAMP)
    {
	switch (hdr.rec_type)
	{
	case SAR_CPU:
	    sar_cpu = get_cpu_sample(flag, &hdr);
	    break;
	case SAR_VMSTAT:
	    sar_vmstat=get_vmstat_sample(flag, &hdr);
	    break;
	case SAR_DRIVEPATH:
	  sar_drivepath = get_drivepath_sample(flag, &hdr);
	  if (sar_drivepath < 0)
	      fprintf(stderr, "sar: drivepath sync code error %d\n", sar_drivepath);
	  break;
	case SAR_DRIVESTATS:
	    sar_drivestats = get_drivestats_sample(flag, &hdr);
	    break;
	case SAR_NETSTATS:
	    sar_netstats = get_netstats_sample(flag, &hdr);
	    break;
	default:
	    break;
	}

	read_record_hdr(&hdr, TRUE);
    }

    /* return the timestamp header */
    *ret_hdr = hdr;

    if (flag == PRINT_SET)
    {
	avg_counter++;
	timebufp = get_hms_string(timestamp, timebuf);

	if (uflag && sar_cpu)
	    print_cpu_sample(timebufp);

	if((gflag || pflag) && sar_vmstat)
	    print_vmstat_sample(timebufp);

	if (dflag && sar_drivestats)
	    print_drivestats_sample(timebufp);

	if (nflag && sar_netstats)
	    print_netstats_sample(timebufp);
    }
}

static void
skip_data(bufsize)
    int bufsize;
{
    char *buf = NULL;
    
    if (fflag)
    {
        /* seek past data in the file */
	if ((lseek(ifd, bufsize, SEEK_CUR) == -1))
	{
	    /*exit on error */
	    fprintf(stderr, "sar: lseek failed, errno=%d\n", errno);
	    exit(EXIT_FAILURE);
	}
    }	    
    else 
    {
	/* have to read from the pipe */
	if((buf = (char *)malloc(bufsize)) == NULL)
	{
	    fprintf(stderr, "sar: malloc failed\n");
	    exit(EXIT_FAILURE);
	}
	/* even though we skip this data, we still write it if necessary */
	read_record_data(buf, bufsize, TRUE);
    }
    if (buf)
	free(buf);
    
    return;
}

static int
get_cpu_sample(flag, hdr)
    int flag;
    struct record_hdr *hdr;
{
    int  datasize;

    datasize = hdr->rec_count * hdr->rec_size;
    
    if (datasize != sizeof(host_cpu_load_info_data_t))
    {
	/* read past the data but don't do anything with it */
	skip_data(datasize);
	return(0);
    }

    read_record_data ((char *)&cur_cpuload, (int)sizeof(host_cpu_load_info_data_t), TRUE );

    if (flag == INIT_SET)
    {
	prev_cpuload = cur_cpuload;
	bzero(&avg_cpuload, sizeof(avg_cpuload));
    }
    return(1);
}

static void
print_cpu_sample(timebufptr)
    char * timebufptr;
{

    double time;

    time = 0.0;
    cur_cpuload.cpu_ticks[CPU_STATE_USER]
      -= prev_cpuload.cpu_ticks[CPU_STATE_USER];
    
    prev_cpuload.cpu_ticks[CPU_STATE_USER]
      += cur_cpuload.cpu_ticks[CPU_STATE_USER];
	
    time += cur_cpuload.cpu_ticks[CPU_STATE_USER];

    cur_cpuload.cpu_ticks[CPU_STATE_NICE]
      -= prev_cpuload.cpu_ticks[CPU_STATE_NICE];
    
    prev_cpuload.cpu_ticks[CPU_STATE_NICE]
      += cur_cpuload.cpu_ticks[CPU_STATE_NICE];
	
    time += cur_cpuload.cpu_ticks[CPU_STATE_NICE];
	
    cur_cpuload.cpu_ticks[CPU_STATE_SYSTEM]
      -= prev_cpuload.cpu_ticks[CPU_STATE_SYSTEM];
	
    prev_cpuload.cpu_ticks[CPU_STATE_SYSTEM]
      += cur_cpuload.cpu_ticks[CPU_STATE_SYSTEM];
	
    time += cur_cpuload.cpu_ticks[CPU_STATE_SYSTEM];
	
    cur_cpuload.cpu_ticks[CPU_STATE_IDLE]
      -= prev_cpuload.cpu_ticks[CPU_STATE_IDLE];
	
    prev_cpuload.cpu_ticks[CPU_STATE_IDLE]
      += cur_cpuload.cpu_ticks[CPU_STATE_IDLE];
	
    time += cur_cpuload.cpu_ticks[CPU_STATE_IDLE];

    avg_cpuload.cpu_ticks[CPU_STATE_USER] += rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_USER]
      / (time ? time : 1));

    avg_cpuload.cpu_ticks[CPU_STATE_NICE] += rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_NICE]
      / (time ? time : 1));

    avg_cpuload.cpu_ticks[CPU_STATE_SYSTEM] += rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_SYSTEM]
      / (time ? time : 1));
    
    avg_cpuload.cpu_ticks[CPU_STATE_IDLE] += rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_IDLE]
      / (time ? time : 1));

    if(flag_count > 1)
	print_column_heading(SAR_CPU, timebufptr, 0);

    fprintf(stdout, "%s%5.0f   ", timebufptr,
      rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_USER]
      / (time ? time : 1)));
	
    fprintf(stdout, "%4.0f   ",
      rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_NICE]
      / (time ? time : 1)));

    fprintf(stdout, "%4.0f   ",
      rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_SYSTEM]
      / (time ? time : 1)));
	
    fprintf(stdout, "%4.0f\n",
      rint(100. * cur_cpuload.cpu_ticks[CPU_STATE_IDLE]
      / (time ? time : 1)));
}

static int
get_vmstat_sample(flag, hdr)
    int flag;
    struct record_hdr *hdr;
{
    int  datasize;

    datasize = hdr->rec_count * hdr->rec_size;
    
    if (datasize != sizeof(struct vm_statistics))
    {
	/* read past the data but don't do anything with it */
	skip_data(datasize);
	return(0);
    }

    read_record_data ((char *)&cur_vmstat, (int)sizeof(struct vm_statistics), TRUE );

    if (flag == INIT_SET)
    {
	prev_vmstat = cur_vmstat;
	bzero(&avg_vmstat, sizeof(avg_vmstat));
    }
    return(1);
}


static void
print_vmstat_sample(char *timebufptr)
{

    cur_vmstat.faults -= prev_vmstat.faults;
    prev_vmstat.faults += cur_vmstat.faults;
    avg_vmstat.faults += cur_vmstat.faults;	

    cur_vmstat.cow_faults -= prev_vmstat.cow_faults;
    prev_vmstat.cow_faults += cur_vmstat.cow_faults;
    avg_vmstat.cow_faults += cur_vmstat.cow_faults;	

    cur_vmstat.zero_fill_count -= prev_vmstat.zero_fill_count;
    prev_vmstat.zero_fill_count += cur_vmstat.zero_fill_count;
    avg_vmstat.zero_fill_count += cur_vmstat.zero_fill_count;

    cur_vmstat.reactivations -= prev_vmstat.reactivations;
    prev_vmstat.reactivations += cur_vmstat.reactivations;
    avg_vmstat.reactivations += cur_vmstat.reactivations;	
	
    cur_vmstat.pageins -= prev_vmstat.pageins;
    prev_vmstat.pageins += cur_vmstat.pageins;
    avg_vmstat.pageins += cur_vmstat.pageins;	
	
    cur_vmstat.pageouts -= prev_vmstat.pageouts;
    prev_vmstat.pageouts += cur_vmstat.pageouts;
    avg_vmstat.pageouts += cur_vmstat.pageouts;


    if (gflag)
    {
	if (flag_count > 1)
	    print_column_heading(SAR_VMSTAT, timebufptr, 0);
	fprintf(stdout, "%s   %8.1f   \n", timebufptr, (float)((float)cur_vmstat.pageouts/avg_interval));
    }
	
    if (pflag)
    {
	if (flag_count > 1)
	    print_column_heading(SAR_VMSTAT, timebufptr, 1);
	fprintf(stdout, "%s   %8.1f      %8.1f      %8.1f\n", timebufptr,
	  (float)((float)cur_vmstat.pageins / avg_interval),
	  (float)((float)cur_vmstat.cow_faults/avg_interval),
	  (float)((float)cur_vmstat.faults/avg_interval));
    }
    fflush(stdout);
}

static int
get_drivestats_sample(flag, hdr)
    int flag;
    struct record_hdr *hdr;
{
    struct drivestats *databuf;
    struct drivestats_report *dr;
    size_t  datasize;
    int     datacount;
    int     index;    
    int     i;
    
    datasize = hdr->rec_count * hdr->rec_size;
    datacount = hdr->rec_count;
    
    if (hdr->rec_size != sizeof(struct drivestats))
    {
	/* something isn't right... read past the data but don't analyze it */
	skip_data(datasize);
	return(0);
    }

    /* malloc read buffer */
    if ((databuf = (struct drivestats *)malloc(datasize)) == NULL)
    {
	fprintf(stderr, "sar: malloc failed\n");
	exit (EXIT_FAILURE);
    }

    bzero(databuf, datasize);

    read_record_data ((char *)databuf, datasize, TRUE );
    
    /* clear all global current fields */
    for(dr = dr_head; dr; dr=(struct drivestats_report *)dr->next)
    {
	dr->present = 0;
	dr->cur_Reads = 0;
	dr->cur_BytesRead = 0;
	dr->cur_Writes = 0;
	dr->cur_BytesWritten = 0;
	dr->cur_LatentReadTime = 0;
	dr->cur_LatentWriteTime = 0;
	dr->cur_ReadErrors = 0;
	dr->cur_WriteErrors = 0;
	dr->cur_ReadRetries = 0;
	dr->cur_WriteRetries = 0;
	dr->cur_TotalReadTime = 0;
	dr->cur_TotalWriteTime=0;
    }

    /* By this point, we have read in a complete set of diskstats from the sadc
     * data collector.
     * The order of the drives in not guaranteed.
     * The global report structure is a linked list, but may need initialization
     * We need to traverse this list  and transfer the current
     * read data.  If a disk entry isn't found, then we need to allocate one
     * initilize it.
    */
    for (i=0; i< datacount; i++)
    {
	struct drivestats_report *dr_last = NULL;

	index = databuf[i].drivepath_id;   /* use this as index into dp_table */
	
	/* find disk entry or allocate new one*/
	for(dr = dr_head; dr; dr=(struct drivestats_report *)dr->next)
	{
	    dr_last = dr;
	    if(index == dr->drivepath_id)
		break;
	} 
	
	if (dr == NULL)
	{
	    /* allocate new entry */
	    if((dr = (struct drivestats_report *)malloc(sizeof(struct drivestats_report))) == NULL)
	    {
		fprintf(stderr, "sar: malloc error\n");
		exit(EXIT_FAILURE);
	    }
	    bzero((char *)dr, sizeof(struct drivestats_report));
	    dr->blocksize = databuf[i].blocksize;
	    dr->drivepath_id = index;
	    dr->next = NULL;
	    dr->avg_count = 0;

	    /* get the BSDName which should be in the table by now */
	    if ((index < dp_count) && (dp_table[index].state != DPSTATE_UNINITIALIZED))
		strncpy(dr->name, dp_table[index].BSDName, MAXDRIVENAME+1);
	    else
		strcpy(dr->name, "disk??");

	    if (dr_head == NULL)
	    {
		dr_head = dr;
		dr_head->next = NULL;
	    }
	    else
	    {
		dr_last->next = (char *)dr;
	    }
	} /* end if dr == NULL */
	
	dr->present = TRUE;
	dr->cur_Reads = databuf[i].Reads;
	dr->cur_BytesRead = databuf[i].BytesRead;
	dr->cur_Writes = databuf[i].Writes;
	dr->cur_BytesWritten = databuf[i].BytesWritten;
	dr->cur_LatentReadTime = databuf[i].LatentReadTime;
	dr->cur_LatentWriteTime = databuf[i].LatentWriteTime;
	dr->cur_ReadErrors = databuf[i].ReadErrors;
	dr->cur_WriteErrors = databuf[i].WriteErrors;
	dr->cur_ReadRetries = databuf[i].ReadRetries;
	dr->cur_WriteRetries = databuf[i].WriteRetries;
	dr->cur_TotalReadTime = databuf[i].TotalReadTime;
	dr->cur_TotalWriteTime=databuf[i].TotalWriteTime;
    } /* end for loop */
	
    /* Reinitialize the prev and avg fields when
     * This is a new disk
     * This is a changed disk - name change implies disk swapping
     * This disk is not present in this sample
     */
    for(dr = dr_head; dr; dr=(struct drivestats_report *)dr->next)
    {
	if (dr->drivepath_id >= dp_count)
	{
	    /* something is amiss */
	    continue;
	}
	else
	{
	    index = dr->drivepath_id;   /* use this as index into dp_table */
	}
	
	if ((flag == INIT_SET) ||
	  (dp_table[index].state == DPSTATE_NEW) ||
	  (dp_table[index].state == DPSTATE_CHANGED) ||
	  (!dr->present))
	{
	    /*
	     * prev will be set to cur
	     * activate the state in dp_table
	     */
	    if (dr->present)
		dp_table[index].state = DPSTATE_ACTIVE;
	    
	    init_drivestats(dr);
	}
    }
    return(1);
}

static void
init_drivestats(struct drivestats_report *dr)
{
    dr->avg_count = 0;
    dr->prev_Reads = dr->cur_Reads;
    dr->avg_Reads = 0;
    dr->prev_BytesRead = dr->cur_BytesRead;
    dr->avg_BytesRead = 0;
    dr->prev_Writes = dr->cur_Writes;
    dr->avg_Writes = 0;
    dr->prev_BytesWritten = dr->cur_BytesWritten;
    dr->avg_BytesWritten = 0;
    dr->prev_LatentReadTime = dr->cur_LatentReadTime;
    dr->avg_LatentReadTime = 0;
    dr->prev_LatentWriteTime = dr->cur_LatentWriteTime ;
    dr->avg_LatentWriteTime = 0;
    dr->prev_ReadErrors = dr->cur_ReadErrors ;
    dr->avg_ReadErrors = 0;
    dr->prev_WriteErrors = dr->cur_WriteErrors ;
    dr->avg_WriteErrors = 0;
    dr->prev_ReadRetries = dr->cur_ReadRetries ;
    dr->avg_ReadRetries = 0;
    dr->prev_WriteRetries = dr->cur_WriteRetries ;
    dr->avg_WriteRetries = 0;
    dr->prev_TotalReadTime = dr->cur_TotalReadTime ;
    dr->avg_TotalReadTime = 0;
    dr->prev_TotalWriteTime = dr->cur_TotalWriteTime ;
    dr->avg_TotalWriteTime = 0;    
}


static void
print_drivestats_sample(char *timebufptr)
{
    struct drivestats_report *dr;
    long double transfers_per_second;
    long double kb_per_transfer, mb_per_second;
    u_int64_t interval_bytes, interval_transfers, interval_blocks;
    u_int64_t interval_time;
    long double blocks_per_second, ms_per_transaction;

    if (flag_count > 1)
	print_column_heading(SAR_DRIVESTATS, timebufptr, 0);
	
    for (dr=dr_head; dr; dr=(struct drivestats_report *)dr->next)
    {
	if(!dr->present)
	    continue;

	/*
	 * This sanity check is for drives that get removed and then
	 * returned during the sampling sleep interval.  If anything
	 * looks out of sync, reinit and skip this entry.  There is
	 * no way to guard against this entirely.
	 */
	if ((dr->cur_Reads < dr->prev_Reads) ||
	  (dr->cur_BytesRead < dr->prev_BytesRead) ||
	  (dr->cur_Writes < dr->prev_Writes) ||
	  (dr->cur_BytesWritten < dr->prev_BytesWritten))
	{
	    init_drivestats(dr);
	    continue;
	}

	dr->avg_count++;

	dr->cur_Reads -= dr->prev_Reads;
	dr->prev_Reads += dr->cur_Reads;
	dr->avg_Reads += dr->cur_Reads;
	  
        dr->cur_BytesRead -= dr->prev_BytesRead;
	dr->prev_BytesRead += dr->cur_BytesRead;
	dr->avg_BytesRead += dr->cur_BytesRead;
	
	dr->cur_Writes -= dr->prev_Writes ;
	dr->prev_Writes += dr->cur_Writes ;
	dr->avg_Writes += dr->cur_Writes ;

	dr->cur_BytesWritten -= dr->prev_BytesWritten ;
	dr->prev_BytesWritten += dr->cur_BytesWritten ;
	dr->avg_BytesWritten += dr->cur_BytesWritten ;

	dr->cur_LatentReadTime -= dr->prev_LatentReadTime ;
	dr->prev_LatentReadTime += dr->cur_LatentReadTime ;
	dr->avg_LatentReadTime += dr->cur_LatentReadTime ;

	dr->cur_LatentWriteTime -= dr->prev_LatentWriteTime ;
	dr->prev_LatentWriteTime += dr->cur_LatentWriteTime ;
	dr->avg_LatentWriteTime += dr->cur_LatentWriteTime ;	

	dr->cur_ReadErrors -= dr->prev_ReadErrors ;
	dr->prev_ReadErrors += dr->cur_ReadErrors ;
	dr->avg_ReadErrors += dr->cur_ReadErrors ;

	dr->cur_WriteErrors -= dr->prev_WriteErrors ;
	dr->prev_WriteErrors += dr->cur_WriteErrors ;
	dr->avg_WriteErrors += dr->cur_WriteErrors ;

	dr->cur_ReadRetries -= dr->prev_ReadRetries ;
	dr->prev_ReadRetries += dr->cur_ReadRetries ;
	dr->avg_ReadRetries += dr->cur_ReadRetries ;

	dr->cur_WriteRetries -= dr->prev_WriteRetries ;
	dr->prev_WriteRetries += dr->cur_WriteRetries;
	dr->avg_WriteRetries += dr->cur_WriteRetries;

	dr->cur_TotalReadTime -= dr->prev_TotalReadTime ;
	dr->prev_TotalReadTime += dr->cur_TotalReadTime ;
	dr->avg_TotalReadTime += dr->cur_TotalReadTime ;

	dr->cur_TotalWriteTime -= dr->prev_TotalWriteTime ;
	dr->prev_TotalWriteTime += dr->cur_TotalWriteTime ;
	dr->avg_TotalWriteTime += dr->cur_TotalWriteTime ;

	/* I/O volume */
	interval_bytes = dr->cur_BytesRead + dr->cur_BytesWritten;

	/* I/O counts */
	interval_transfers = dr->cur_Reads + dr->cur_Writes;

	/* I/O time */
	interval_time = dr->cur_LatentReadTime + dr->cur_LatentWriteTime;

	interval_blocks = interval_bytes / dr->blocksize;
	blocks_per_second = interval_blocks / avg_interval;
	transfers_per_second = interval_transfers / avg_interval;
	mb_per_second = (interval_bytes / avg_interval) / (1024 *1024);

	kb_per_transfer = (interval_transfers > 0) ?
	  ((long double)interval_bytes / interval_transfers)
	  / 1024 : 0;

	/* times are in nanoseconds, convert to milliseconds */
	ms_per_transaction = (interval_transfers > 0) ?
	  ((long double)interval_time / interval_transfers)
	  / 1000 : 0;

	/* print device name */
	fprintf(stdout, "%s   %-10s", timebufptr, dr->name);
	  
	/* print transfers per second */
	fprintf(stdout, "%4.0Lf       ", transfers_per_second);
	
	/* print blocks per second - in device blocksize */
	fprintf(stdout, "%4.0Lf\n", blocks_per_second);
    }
}

/*
 * Print averages before exiting.
 */
static void
exit_average()
{
    int i;
    
    if (avg_counter <= 0 )
	exit(0);

    if (oflag)
      {
	if (ofd)
	  close (ofd);
	ofd = 0;
      }

    if (uflag) /* print cpu averages */
    {
	if(flag_count > 1)
	    print_column_heading(SAR_CPU, 0, 0);
    
        fprintf(stdout, "Average:  %5d   ",
	   (int)avg_cpuload.cpu_ticks[CPU_STATE_USER]
	  / (avg_counter ? avg_counter : 1));

	fprintf(stdout, "%4d   ", 
	   (int)avg_cpuload.cpu_ticks[CPU_STATE_NICE]
	  / (avg_counter ? avg_counter : 1));

	fprintf(stdout, "%4d   ", 
	   (int)avg_cpuload.cpu_ticks[CPU_STATE_SYSTEM]
	  / (avg_counter ? avg_counter : 1));

	fprintf(stdout, "%4d   \n",
	   (int)avg_cpuload.cpu_ticks[CPU_STATE_IDLE]
	  / (avg_counter ? avg_counter : 1));
	
	fflush(stdout);	
    }    


    if (gflag) /* print page-out averages */
    {
	if (flag_count > 1)
	    print_column_heading(SAR_VMSTAT, 0, 0);
	
	fprintf(stdout, "Average:   %8.1f\n",
	(float)((avg_vmstat.pageouts / (avg_counter ? avg_counter : 1)) / avg_interval));
	fflush(stdout);	
    }

    if (pflag) /* print page-in averages */
    {
	if (flag_count > 1)
	    print_column_heading(SAR_VMSTAT, 0, 1);	    
	
	fprintf(stdout, "Average:   %8.1f      %8.1f      %8.1f\n",
	  (float)(((float)avg_vmstat.pageins / (avg_counter ? avg_counter : 1)) / avg_interval),
	  (float)(((float)avg_vmstat.cow_faults / (avg_counter ? avg_counter : 1)) / avg_interval),
	  (float)(((float)avg_vmstat.faults / (avg_counter ? avg_counter : 1)) / avg_interval));
	fflush(stdout);
    }

    if (dflag) /* print drivestats averages */
    {
	struct drivestats_report *dr;
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	u_int64_t total_bytes, total_transfers, total_blocks;
	u_int64_t total_time;
	long double blocks_per_second, ms_per_transaction;
	int msdig;

	if (flag_count > 1)
	    print_column_heading(SAR_DRIVESTATS, 0, 0);

	for (dr=dr_head; dr; dr=(struct drivestats_report *)dr->next)
	{
	    /* don't bother to print out averages for disks that were removed */
	    if (!dr->present)
		continue;

	    fprintf(stdout, "           %s    %s\n",
	      dp_table[dr->drivepath_id].BSDName, dp_table[dr->drivepath_id].ioreg_path);	    
	    
	    /* I/O volume */
	    total_bytes = dr->avg_BytesRead + dr->avg_BytesWritten;

	    /* I/O counts */
	    total_transfers = dr->avg_Reads + dr->avg_Writes;

	    /* I/O time */
	    total_time = dr->avg_LatentReadTime + dr->avg_LatentWriteTime;

	    total_blocks = total_bytes / dr->blocksize;
	    blocks_per_second = total_blocks / avg_interval;
	    transfers_per_second = total_transfers / avg_interval;
	    mb_per_second = (total_bytes / avg_interval) / (1024 *1024);

	    kb_per_transfer = (total_transfers > 0) ?
	      ((long double)total_bytes / total_transfers)
	      / 1024 : 0;

	    /* times are in nanoseconds, convert to milliseconds */
	    ms_per_transaction = (total_transfers > 0) ?
	      ((long double)total_time / total_transfers)
	      / 1000 : 0;
	    msdig = (ms_per_transaction < 100.0) ? 1 : 0;
	    fprintf(stdout, "Average:   %-10s %4.0Lf      %4.0Lf\n",
	      dr->name,
	      (transfers_per_second / dr->avg_count),
	      (blocks_per_second / dr->avg_count));
	    
	    fflush(stdout);	
	}
    } /* end if dflag */

    if (nflag)
    {
	int avg_count;
	
	if (network_mode & NET_DEV_MODE)	    
	{
	    if (flag_count > 1)
		print_column_heading(SAR_NETSTATS, 0, NET_DEV_MODE);
	    for (i = 0; i < nr_count; i++)
	    {
		if (!nr_table[i].valid)
		    continue;

		if(nr_table[i].avg_count == 0)
		    avg_count = 1;
		else
		    avg_count = nr_table[i].avg_count;

		fprintf(stdout, "Average:   %-8.8s", nr_table[i].tname_unit);
	    
		fprintf (stdout, "%8llu    ",
		  ((nr_table[i].avg_ipackets / avg_count) / avg_interval));

		fprintf (stdout, "%10llu    ",
		  ((nr_table[i].avg_ibytes / avg_count) / avg_interval));

		fprintf (stdout, "%8llu    ",
		  ((nr_table[i].avg_opackets / avg_count) / avg_interval));
		
		fprintf (stdout, "%10llu\n",
		  ((nr_table[i].avg_obytes / avg_count) / avg_interval));
		
		fflush(stdout);
	    }
	}

	if (network_mode & NET_EDEV_MODE)	    
	{

	    if(flag_count > 1)
		print_column_heading(SAR_NETSTATS, 0, NET_EDEV_MODE);

	    for (i = 0; i < nr_count; i++)
	    {
		if (!nr_table[i].valid)
		    continue;

		if(nr_table[i].avg_count == 0)
		    avg_count = 1;
		else
		    avg_count = nr_table[i].avg_count;

		fprintf(stdout, "Average:   %-8.8s  ", nr_table[i].tname_unit);
	    
		fprintf (stdout, "%7llu    ",		  
		  ((nr_table[i].avg_ierrors / avg_count) / avg_interval));
		
		fprintf (stdout, "%7llu    ",
		  ((nr_table[i].avg_oerrors / avg_count) / avg_interval));

		fprintf (stdout, "%5llu    ",
		  ((nr_table[i].avg_collisions / avg_count) / avg_interval));

		fprintf (stdout, "   %5llu\n",
		  ((nr_table[i].avg_drops / avg_count) / avg_interval));
		
		fflush(stdout);
	    }
	}	

    } /* end if nflag */
    exit(0);
}


/*
 * Return < 0 failure, debugging purposes only
 * Return = 0 data skipped
 * Return > 0 success
 */
  
static int
get_drivepath_sample(flag, hdr)
    int flag;
    struct record_hdr *hdr;
{
    size_t datasize;
    struct drivepath dp;
    struct drivestats_report *dr;
    int i, n;

    datasize = hdr->rec_count * hdr->rec_size;

    if (datasize != sizeof(struct drivepath))
    {
	/* read past the data but don't do anything with it */
	skip_data(datasize);
	return(0);
    }

    read_record_data ((char *)&dp, (int)sizeof(struct drivepath), TRUE );

    /*
     * If state is new -- put a new entry in the dp_table.
     * If state is changed -- traverse the drivestats_report table
     * and copy new name.
     */
    if (dp.state == DPSTATE_NEW)
    {

	if (dp_table == NULL)
	{
	    if (dp.drivepath_id != 0)
		return(-1);
	    /* First setup of internal drivepath table */
	    dp_table = (struct drivepath *)malloc(sizeof(struct drivepath));
	    if (dp_table == NULL)
		return(-2);
	    dp_count = 1;
	}

	if (dflag)
	    fprintf(stdout, "New Disk: [%s] %s\n", dp.BSDName, dp.ioreg_path);

	/* traverse table and find next uninitialized entry */
	for (i = 0; i< dp_count; i++)
	{
	    if (dp_table[i].state == DPSTATE_UNINITIALIZED)
	    {
		if (dp.drivepath_id != i)
		{
		    /* the table is out of sync - this should not happen */
		    return (-3);
		}
		dp_table[i] = dp;
		return(1);
	    }
	}
	/*
	 * If we get here, we've run out of table entries.
	 * Double the size of the table, then assign the next entry.
	 */
	if (dp.drivepath_id != i)
	{
	    /* the table is out of sync - this should not happen */
	    return (-4);
	}
	n = dp_count * 2;
	dp_table = (struct drivepath *)realloc(dp_table, n * sizeof(struct drivepath));
	bzero(&dp_table[dp_count], dp_count * sizeof(struct drivepath));
	dp_table[dp_count] = dp;
	dp_count = n;
	return(1);

    }
    else if (dp.state == DPSTATE_CHANGED)
    {

	  /* Update the name in the table */
	if ((dp.drivepath_id < dp_count) && (dp_table[dp.drivepath_id].state != DPSTATE_UNINITIALIZED))
	{
	    if (strcmp(dp_table[dp.drivepath_id].ioreg_path, dp.ioreg_path) != 0)
	    {
		/* something is amiss */
		return (-5);
	    }
	    else
	    {
		if (dflag)
		{
		    fprintf(stdout, "Change: [%s] %s\n", dp.BSDName,
		      dp_table[dp.drivepath_id].ioreg_path);
		}
		strcpy(dp_table[dp.drivepath_id].BSDName, dp.BSDName);

		for(dr = dr_head; dr; dr=(struct drivestats_report *)dr->next)
		  {
		    if (dr->drivepath_id == dp.drivepath_id)
		      strcpy(dr->name, dp.BSDName);
		  }
		return(1);
	    }
	}
	else
	    return(-6);
    }
    return(-7);
}

/*
 * Bytes and packet counts are used to track
 * counter wraps.  So, don't enforce the
 * NET_DEV_MODE or NET_EDEV_MODE  in here.
 * Maintain all the stats.
 */
static void
set_cur_netstats(struct netstats_report *nr, struct netstats *ns)
{

    nr->cur_ipackets   = ns->net_ipackets;
    nr->cur_ibytes     = ns->net_ibytes;
    nr->cur_opackets   = ns->net_opackets;
    nr->cur_obytes     = ns->net_obytes;

    nr->cur_ierrors    = ns->net_ierrors;
    nr->cur_oerrors    = ns->net_oerrors;
    nr->cur_collisions = ns->net_collisions;
    nr->cur_drops      = ns->net_drops;

    nr->cur_imcasts    = ns->net_imcasts;
    nr->cur_omcasts    = ns->net_omcasts;

}

static void
init_prev_netstats(struct netstats_report *nr)
{
    nr->avg_count = 0;
    nr->valid = 1;
    nr->present = 1;

    nr->prev_ipackets = nr->cur_ipackets;
    nr->avg_ipackets  = 0;
    nr->prev_ibytes   = nr->cur_ibytes;
    nr->avg_ibytes    = 0;	
    nr->prev_opackets = nr->cur_opackets;
    nr->avg_opackets  = 0;
    nr->prev_obytes   = nr->cur_obytes;
    nr->avg_obytes    = 0;

    nr->prev_ierrors  = nr->cur_ierrors;
    nr->avg_ierrors   = 0;
    nr->prev_oerrors  = nr->cur_oerrors ;
    nr->avg_oerrors   = 0;
    nr->prev_collisions = nr->cur_collisions ;
    nr->avg_collisions  = 0;
    nr->prev_drops  = nr->cur_drops ;
    nr->avg_drops   = 0;

    /* track these, but never displayed */    
    nr->prev_imcasts  = nr->cur_imcasts;
    nr->avg_imcasts = 0;
    nr->prev_omcasts  = nr->cur_omcasts;
    nr->avg_omcasts = 0;    
}

/*
 * Success : 1
 * Failure : 0
 */
static int
get_netstats_sample(flag, hdr)
    int flag;
    struct record_hdr *hdr;
{
    struct netstats *databuf = NULL;
    size_t datasize;
    int    datacount;
    int    i, j;

    datasize = hdr->rec_count * hdr->rec_size;
    datacount = hdr->rec_count;

    if (hdr->rec_size != sizeof(struct netstats))
    {
	/* something isn't right... read past the data but don't analyze it */
	skip_data(datasize);
	return(0);
    }

    /* malloc new or bigger read buffer */
    if((netstat_readbuf == NULL) || (netstat_readbuf_size < datasize))
    {
	if (netstat_readbuf)
	    free (netstat_readbuf);
	
	if ((netstat_readbuf = (struct netstats *)malloc(datasize)) == NULL)
	{
	    fprintf(stderr, "sar: malloc failed\n");
	    exit (EXIT_FAILURE);
	}
	netstat_readbuf_size = datasize;
    }

    bzero(netstat_readbuf, netstat_readbuf_size);
    databuf = netstat_readbuf;

    read_record_data ((char *)databuf, datasize, TRUE );    

    if (nr_table == NULL)
    {
	/* initial internal table setup */
	nr_table = (struct netstats_report *)malloc(datacount * sizeof(struct netstats_report));
	nr_count = datacount;
	bzero(nr_table, (datacount * sizeof(struct netstats_report)));

	/* on first init, this is faster than finding our way to NEW_ENTRY */
	for (i = 0; i < datacount; i++)
	{
	    if (!(network_mode & NET_PPP_MODE))
	    {
		if (!strncmp(databuf[i].tname_unit, "ppp", 3))
		    continue;   /*
				 * Skip ppp interfaces.
				 * ie don't even put them in this internal table.
				 */
	    }
	    strncpy(nr_table[i].tname_unit, databuf[i].tname_unit, MAX_TNAME_UNIT_SIZE);
	    nr_table[i].tname_unit[MAX_TNAME_UNIT_SIZE] = '\0';
	    set_cur_netstats(&nr_table[i], &databuf[i]);
	    init_prev_netstats(&nr_table[i]);
	}
	return(1);
    }

    /*
     * clear all the present flags.
     * As we traverse the current sample set
     * and update the internal table, the flag
     * is reset.
     */
    for (i = 0; i < nr_count; i++)
    {
	nr_table[i].present = 0;
    }

    /*
     * Find and update table entries.
     * Init new entries.
     */
    for (i=0; i<datacount; i++)
    {
	int found;
	char *name;
	int nr_index;
	int n;

	name = databuf[i].tname_unit;
	found = 0;

	if (!(network_mode & NET_PPP_MODE))
	{
	    if (!strncmp(name, "ppp", 3))
		continue;   /* skip ppp interfaces */
	}

	/* Find the matching entry using the interface name */
	for (j=0; j < nr_count && !found; j++)
	{
	    if (nr_table[j].valid)
	    {
		if(!strcmp(nr_table[j].tname_unit, name))
		{
		    found = 1;
		    nr_table[j].present = 1;
		    set_cur_netstats(&nr_table[j], &databuf[i]);
		}
	    }
	} /* end for */

	if (!found)  /* this is a new entry */
	{
	    /* Find an invalid entry in the table and init it */
	    for (j=0; j < nr_count; j++)
	    {
		if (!nr_table[j].valid)
		{
		    nr_index = j;
		    goto NEW_ENTRY;
		}
	    }

	    /* we ran out of entries... grow the table */
	    n = nr_count * 2;
	    nr_table = (struct netstats_report *)realloc(nr_table, n * sizeof(struct netstats_report));
	    bzero(&nr_table[nr_count], nr_count * sizeof (struct netstats_report));
	    nr_index = nr_count;
	    nr_count = n;
	    
	    NEW_ENTRY:
	    strncpy(nr_table[nr_index].tname_unit, databuf[i].tname_unit, MAX_TNAME_UNIT_SIZE);
	    nr_table[nr_index].tname_unit[MAX_TNAME_UNIT_SIZE] = '\0';
	    set_cur_netstats(&nr_table[nr_index], &databuf[i]);
	    init_prev_netstats(&nr_table[nr_index]);
	}
	
    } /* end for */

    /*
     * Traverse the internal table.  Any valid entry that wasn't
     * present in this sample is cleared for reuse.
     */
    for (i = 0; i < nr_count; i++)
    {
	if (nr_table[i].valid)
	{
	    if (nr_table[i].present == 0)
		bzero(&nr_table[i], sizeof(struct netstats_report));
	}
    }
    return (1);
}

static void
print_netstats_sample(char *timebufptr)
{
    int i;

    for (i=0; i < nr_count; i++)
    {
	if (!nr_table[i].valid)
	    continue;

	/*
	 * This is where we attempt to handle counters that
	 * might wrap ... the kernel netstats are only 32 bits.
	 *
	 * Interfaces may go away and then return within the
	 * sampling period.  This can't be detected and it
	 * may look like a counter wrap.  An interface generation
	 * counter will help... but isn't implemented at this time.
	 */

	/* 
	 * The ppp interfaces are very likely to come and go during
	 * a sampling period.  During the normal life of a ppp interface,
	 * it's less likely that the packet counter will wrap, so if
	 * it appears to have done so, is probably because the
	 * interface unit number has been reused. 
	 * We reinitialize that interface in that case.
	 */
	if (network_mode & NET_PPP_MODE)
	{
	    /*
	     * ppp interfaces won't even make it into this table
	     * when NET_PPP_MODE isn't set
	    */
	    if (!strncmp(nr_table[i].tname_unit, "ppp", 3))
	    {
		/*
		 * Both ipackets and opackets have to be less
		 * than the previous counter to cause us to reinit.
		 */

		if ((nr_table[i].cur_ipackets < nr_table[i].prev_ipackets)
		  && (nr_table[i].cur_opackets < nr_table[i].prev_opackets))
		{
		    init_prev_netstats(&nr_table[i]);
		    continue;
		}
	    }
	}

	nr_table[i].avg_count ++;

#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_ipackets < nr_table[i].prev_ipackets)
	    nr_table[i].cur_ipackets += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_ipackets -= nr_table[i].prev_ipackets;
	nr_table[i].prev_ipackets += nr_table[i].cur_ipackets;
	nr_table[i].avg_ipackets += nr_table[i].cur_ipackets;
	

#ifdef IFNET_32_BIT_COUNTERS	
	while (nr_table[i].cur_ibytes < nr_table[i].prev_ibytes)
	    nr_table[i].cur_ibytes += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_ibytes -= nr_table[i].prev_ibytes;
	nr_table[i].prev_ibytes += nr_table[i].cur_ibytes;
	nr_table[i].avg_ibytes += nr_table[i].cur_ibytes;


#ifdef IFNET_32_BIT_COUNTERS	
	while (nr_table[i].cur_opackets < nr_table[i].prev_opackets)
	    nr_table[i].cur_opackets += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_opackets -= nr_table[i].prev_opackets;
	nr_table[i].prev_opackets += nr_table[i].cur_opackets;
	nr_table[i].avg_opackets += nr_table[i].cur_opackets;

#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_obytes < nr_table[i].prev_obytes)
	    nr_table[i].cur_obytes += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_obytes -= nr_table[i].prev_obytes;
	nr_table[i].prev_obytes += nr_table[i].cur_obytes;
	nr_table[i].avg_obytes += nr_table[i].cur_obytes;


#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_ierrors < nr_table[i].prev_ierrors)
	    nr_table[i].cur_ierrors += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_ierrors -= nr_table[i].prev_ierrors;
	nr_table[i].prev_ierrors += nr_table[i].cur_ierrors;
	nr_table[i].avg_ierrors += nr_table[i].cur_ierrors;

#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_oerrors < nr_table[i].prev_oerrors)
	    nr_table[i].cur_oerrors += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_oerrors -= nr_table[i].prev_oerrors;
	nr_table[i].prev_oerrors += nr_table[i].cur_oerrors;
	nr_table[i].avg_oerrors += nr_table[i].cur_oerrors;

#ifdef IFNET_32_BIT_COUNTERS	
	while (nr_table[i].cur_collisions < nr_table[i].prev_collisions)
	    nr_table[i].cur_collisions += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_collisions -= nr_table[i].prev_collisions;
	nr_table[i].prev_collisions += nr_table[i].cur_collisions;
	nr_table[i].avg_collisions += nr_table[i].cur_collisions;

#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_drops < nr_table[i].prev_drops)
	    nr_table[i].cur_drops += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_drops -= nr_table[i].prev_drops;
	nr_table[i].prev_drops += nr_table[i].cur_drops;
	nr_table[i].avg_drops += nr_table[i].cur_drops;

	
#ifdef IFNET_32_BIT_COUNTERS	
	while (nr_table[i].cur_imcasts < nr_table[i].prev_imcasts)
	    nr_table[i].cur_imcasts += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_imcasts -= nr_table[i].prev_imcasts;
	nr_table[i].prev_imcasts += nr_table[i].cur_imcasts;
	nr_table[i].avg_imcasts += nr_table[i].cur_imcasts;

#ifdef IFNET_32_BIT_COUNTERS
	while (nr_table[i].cur_omcasts < nr_table[i].prev_omcasts)
	    nr_table[i].cur_omcasts += 0x100000000LL;
#endif /* IFNET_32_BIT_COUNTERS */
	nr_table[i].cur_omcasts -= nr_table[i].prev_omcasts;
	nr_table[i].prev_omcasts += nr_table[i].cur_omcasts;
	nr_table[i].avg_omcasts += nr_table[i].cur_omcasts;
    }


    if (!(flag_count > 1))
	fprintf(stdout, "\n");
    
    if (network_mode & NET_DEV_MODE)
    {
	if (flag_count > 1)
	    print_column_heading(SAR_NETSTATS, timebufptr, NET_DEV_MODE);
	
	for (i=0; i < nr_count; i++)
	{
	    if (!nr_table[i].valid)
		continue;

	    if (!(network_mode & NET_PPP_MODE))
	    {
		if (!strncmp(nr_table[i].tname_unit, "ppp", 3))
		{
		    continue;  /* skip any ppp interfaces */
		}
	    }
	
	    /* print the interface name */
	    fprintf(stdout, "%s    %-8.8s", timebufptr, nr_table[i].tname_unit);

	    fprintf (stdout, "%8llu    ",
	      (nr_table[i].cur_ipackets / avg_interval));

	    fprintf (stdout, "%10llu    ",
	      (nr_table[i].cur_ibytes / avg_interval));

	    fprintf (stdout, "%8llu    ",
	      (nr_table[i].cur_opackets / avg_interval));

	    fprintf (stdout, "%10llu\n",
	      (nr_table[i].cur_obytes / avg_interval));
	}
    }

    
    if (network_mode & NET_EDEV_MODE)
    {
	if(flag_count > 1)
	{
	    print_column_heading(SAR_NETSTATS, timebufptr, NET_EDEV_MODE);
	}
    
	for (i=0; i < nr_count; i++)
	{
	    if (!nr_table[i].valid)
		continue;
	    
	    if (!(network_mode & NET_PPP_MODE))
	    {
		if (!strncmp(nr_table[i].tname_unit, "ppp", 3))
		{
		    continue;  /* skip any ppp interfaces */
		}
	    }	

	    /* print the interface name */
	    fprintf(stdout, "%s    %-8.8s  ", timebufptr, nr_table[i].tname_unit);
	    
	    fprintf (stdout, "%7llu    ",
	      (nr_table[i].cur_ierrors / avg_interval));
	    
	    fprintf (stdout, "%7llu    ",
	      (nr_table[i].cur_oerrors / avg_interval));
		
	    fprintf (stdout, "%5llu    ",
	      (nr_table[i].cur_collisions / avg_interval));
	    
	    fprintf (stdout, "   %5llu\n",
	      (nr_table[i].cur_drops / avg_interval));
	}
	fflush(stdout);
    }
}

static void
print_column_heading(int type, char *timebufptr, int mode)
{
    char *p;

    p = timebufptr;
    
    if (p == NULL)
	p = "Average:";

    if (!(flag_count > 1))
      fprintf(stdout, "\n");

    switch (type)
    {
    case SAR_CPU:
    	fprintf (stdout, "\n%s  %%usr  %%nice   %%sys   %%idle\n", p);
	break;
	
    case SAR_VMSTAT:
	if (mode == 0)  /* gflag */
	    fprintf(stdout, "\n%s    pgout/s\n", p);
	else if (mode == 1)  /* pflag */
	    fprintf(stdout, "\n%s     pgin/s        pflt/s        vflt/s\n", p);	    
	break;	
    case SAR_DRIVESTATS:
	fprintf(stdout, "\n%s   device    r+w/s    blks/s\n", p);	
	break;
    case SAR_NETSTATS:
	if (mode == NET_DEV_MODE)
	{	    
	    fprintf(stdout, "\n%s %-8.8s   %8.8s    %10.10s    %8.8s    %10.10s\n", p,
	      "   IFACE", "Ipkts/s", "Ibytes/s", "Opkts/s", "Obytes/s");
	}
	else if (mode == NET_EDEV_MODE)
	{
	    fprintf(stdout, "\n%s %-8.8s     %7.7s     %7.7s    %5s      %s\n", p,
	      "   IFACE", "Ierrs/s", "Oerrs/s", "Coll/s", "Drop/s");
	}
	break;	
    default:
	break;
    }
}

