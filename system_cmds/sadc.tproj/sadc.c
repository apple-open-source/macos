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

#define IOKIT   1       /* to get io_name_t in device_types.h */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/param.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>
#include <ifaddrs.h>

#include <sadc.h>

extern int errno;

FILE *data_fp = (FILE *)0;	/* raw data output file pointer */


#define REVISION_HISTORY_DATE 20030718

struct record_hdr restart_record = { SAR_RESTART, REVISION_HISTORY_DATE, 0, 0 };
struct record_hdr timestamp_record = { SAR_TIMESTAMP, 1, 0, 0 };
struct record_hdr vmstat_record = {SAR_VMSTAT, 1, 1, 0 };
struct record_hdr cpu_record = {SAR_CPU, 1, 1, 0 };
struct record_hdr drivestats_record = {SAR_DRIVESTATS, 1, 0, 0 };
struct record_hdr drivepath_record = {SAR_DRIVEPATH, 1, 1, 0 };
struct record_hdr netstats_record = {SAR_NETSTATS, 1, 0, 0};

/* Compile for verbose output */

int t_interval = 0; 	/* in seconds */
int n_samples  = 1;	/* number of sample loops */
char *ofile = NULL;     /* output file */
int ofd;		/* output file descriptor  */
static mach_port_t myHost;
static mach_port_t masterPort;

/* internal table of drive path mappings */
struct drivepath *dp_table = NULL;

/* number of entries in the dp_table */
int dp_count = 0;

/* internal table of network interface statistics */
struct netstats *ns_table = NULL;
int ns_count = 0;

static uid_t realuid;

int network_mode = 0;

/* Forward fuction declarations */
static void exit_usage();
static void open_datafile(char *);
static void write_record_hdr(struct record_hdr *);
static void write_record_data(char *, int);
static void get_all_stats();
static void get_vmstat_sample();
static void get_drivestat_sample();
static int get_ndrives();
static int record_device(io_registry_entry_t, struct drivestats *, int ndrives);
static int check_device_path (char *name, char *path, int ndrives);
static void get_netstat_sample(int pppflag);

int
main(argc, argv)
    int argc;
    char *argv[];
{

    char        *p;
    char	ch;
    
    /*
     * Stop being root ASAP.
     */
    if (geteuid() != 0)
    {
	fprintf(stderr, "sadc: must be setuid root or root");
	exit(1);
    }
    
    realuid = getuid();
    seteuid(realuid);

    setvbuf(stdout, (char *)NULL, _IONBF, 0);

    while ((ch=getopt(argc, argv, "m:")) != EOF) {
	switch(ch) {
	case 'm':
	    /* Only the PPP mode matters on this collector side   */
	    /* The reporter side deals with the DEV or EDEV modes */
	    if (!strncmp(optarg, "PPP", 3))
		network_mode |= NET_PPP_MODE;
	    break;
	default:
	    exit_usage();
	    break;
	}
    }

    argc -= optind;
    if (argc > 0)
    {
	if (isdigit(*argv[optind]))
	{
	    /* we expect to have both an interval and a sample count */
	    errno=0;
	    t_interval = strtol(argv[optind], &p, 0);
	    if (errno || (*p !='\0') || t_interval <= 0)
	    {
		exit_usage();
	    }

	    optind++;
	    if ((argc < 2) || (!isdigit(*argv[optind]))) {
		exit_usage();
	    }	
	    
	    errno=0;
	    n_samples = strtol(argv[optind], &p, 0);
	    if (errno || (*p != '\0') || n_samples <= 0)
	    {
		exit_usage();
	    }

	    optind++;
	    if (argc == 3)
	    {
		/* we have an output file */
		ofile = argv[optind];
	    }
	}
	else
	{
	    /* all we have is an output file */
	    ofile = argv[optind];
	}
    }

    
    /* open the output file */
    (void)open_datafile(ofile);

    /*
     * Get the Mach private port.
     */ 
    myHost = mach_host_self();
    
    /*
     * Get the I/O Kit communication handle.
     */
    IOMasterPort(bootstrap_port, &masterPort);
    

    restart_record.rec_timestamp = time((time_t *)0);
    write_record_hdr(&restart_record);
    get_all_stats();   /* this is the initial stat collection */
    sleep(t_interval);

    if (n_samples > 0)
    {
	/* this init sample is not counted */
	timestamp_record.rec_data = time((time_t *)0); /* returns time in
						       * seconds */
#if 0
	    struct tm *tm;
	    tm = gmtime(&(timestamp_record.rec_data));
	    fprintf(stderr, "timestamp=%ld\n", timestamp_record.rec_data);
	    fprintf(stderr, "GMTIME offset from UTC in seconds = %ld\n", tm->tm_gmtoff);
	    fprintf(stderr, "GMTIME secnds=%d, min=%d, hour=%d\n", tm->tm_sec, tm->tm_min, tm->tm_hour);
	    fprintf(stderr, "asctime = %s\n", asctime(tm));

	    tm=localtime(&(timestamp_record.rec_data));
	    fprintf(stderr, "LOCTIME offset from UTC in seconds = %ld\n",tm->tm_gmtoff);
	    fprintf(stderr, "LOCTIME secnds=%d, min=%d, hour=%d\n", tm->tm_sec, tm->tm_min, tm->tm_hour);
	    fprintf(stderr, "asctime = %s\n", asctime(tm));
#endif

	write_record_hdr(&timestamp_record);
	get_all_stats();
    }

    while (n_samples)
    {
	sleep(t_interval);
	timestamp_record.rec_timestamp = time((time_t *)0); /* returns time in
							     * seconds */
	write_record_hdr(&timestamp_record);
	get_all_stats();
	n_samples--;
    }
    exit(EXIT_SUCCESS);
}

static void
exit_usage()
{
    fprintf(stderr, "/usr/lib/sa/sadc [-m {PPP}] [t n] [ofile]\n");
    exit(EXIT_FAILURE);
}

static void
open_datafile(char *path)
{
    if (path == NULL)
    {
	data_fp = stdout;
	return;
    }
    else
	data_fp = fopen(path, "w+");

    if (!data_fp)
    {
	/* failed to open path */
	fprintf(stderr, "sadc: failed to open data file [%s]\n", path?path:"stdout");
	exit_usage();
    }
}

static void
write_record_hdr(hdr)
    struct record_hdr *hdr;
{
    errno = 0;
    
    if (fwrite(hdr, sizeof(struct record_hdr), 1, data_fp) != 1)
    {
	fprintf(stderr, "sadc: write_record_hdr failed, errno=%d\n", errno);
	exit(EXIT_FAILURE);
    }

    fflush(data_fp);
    return;
}

static void
write_record_data(data, size)
    char *data;
    int  size;
{
    errno = 0;

    if (fwrite(data, size, 1, data_fp) != 1)
    {
	fprintf(stderr, "sadc: write_record_data failed, errno=%d\n", errno);
	exit(EXIT_FAILURE);
    }

    fflush(data_fp);    
    return;	
}


static void
get_vmstat_sample()
{    
    struct vm_statistics    stat;
    kern_return_t           error;    
    mach_msg_type_number_t  count;

    count = HOST_VM_INFO_COUNT;
    error = host_statistics(myHost, HOST_VM_INFO, (host_info_t)&stat, &count);
    if (error != KERN_SUCCESS) {
	fprintf(stderr, "sadc: Error in vm host_statistics(): %s\n",
	  mach_error_string(error));
	exit(2);
    }
    
    vmstat_record.rec_count = 1;
    vmstat_record.rec_size = sizeof(vm_statistics_data_t);
    write_record_hdr(&vmstat_record);
    write_record_data((char *)&stat, sizeof(vm_statistics_data_t));    
}

static void
get_cpu_sample()
{
    host_cpu_load_info_data_t    cpuload;
    kern_return_t           error;
    mach_msg_type_number_t  count;

    count = HOST_CPU_LOAD_INFO_COUNT;
    error = host_statistics(myHost, HOST_CPU_LOAD_INFO,(host_info_t)&cpuload, &count);
    if (error != KERN_SUCCESS) {
	fprintf(stderr, "sadc: Error in cpu host_statistics(): %s",
	  mach_error_string(error));
	exit(2);
    }

    cpu_record.rec_count = 1;
    cpu_record.rec_size = sizeof(host_cpu_load_info_data_t);
    write_record_hdr(&cpu_record);
    write_record_data((char *)&cpuload, sizeof(host_cpu_load_info_data_t));
}

static void
get_drivestat_sample()
{
    io_registry_entry_t     drive;
    io_iterator_t           drivelist;
    CFMutableDictionaryRef match;
    int			    ndrives;
    int			    i = 0;
    long 		    bufsize = 0;
    char 	 	    *buf;
    struct drivestats       *dbuf;
    kern_return_t           status;
    int error;

    if ((ndrives = get_ndrives()) <= 0)
	return;

    /* allocate space to collect stats for all the drives */
    bufsize = ndrives * sizeof(struct drivestats);
    buf  = (char *) malloc (bufsize);
    dbuf = (struct drivestats *)buf;    
    if (buf)
	bzero((char *)buf, bufsize);	
    else
	return;
	
    /*
     * Get an iterator for IOMedia objects.
     */
    match = IOServiceMatching("IOMedia");
    
    /* Get whole disk info */
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    
    status = IOServiceGetMatchingServices(masterPort, match, &drivelist);
    if (status != KERN_SUCCESS)
	goto RETURN;

    /*
     * Scan all of the IOMedia objects, and for each
     * object that has a parent IOBlockStorageDriver,
     * record the statistics
     *
     * XXX What about RAID devices?
     */
    error = 1;
    i = 0;
    while ((drive = IOIteratorNext(drivelist)))
    {
	if (i < ndrives)
	{
	    if (record_device(drive, &dbuf[i], ndrives))
	    {
		error = 0;
		i++;
	    }
	}
	else
	{
	    IOObjectRelease(drive);
	    break;
	}
	IOObjectRelease(drive);
    }
    IOObjectRelease(drivelist);

    if (! error)
    {
	drivestats_record.rec_count = i;
	drivestats_record.rec_size = sizeof (struct drivestats);
	write_record_hdr(&drivestats_record);
	write_record_data((char *)buf, (i * sizeof(struct drivestats)));
    }

    RETURN:
    if (buf)
	free(buf);
    return;
}

/*
 * Determine whether an IORegistryEntry refers to a valid
 * I/O device, and if so, record it.
 * Return zero: no device recorded
 * Return non-zero: device stats recorded
 */
static int
record_device(io_registry_entry_t drive, struct drivestats* drivestat, int ndrives)
{
    io_registry_entry_t parent;
    CFDictionaryRef properties, statistics;
    CFStringRef name;
    CFNumberRef number;
    UInt64                  value;
    kern_return_t status;
    int retval = 0;
    int drive_id;
    io_string_t path;
    char        BSDName[MAXDRIVENAME + 1];

    status = IORegistryEntryGetParentEntry(drive, kIOServicePlane, &parent);
    if (status != KERN_SUCCESS)
    {
	/* device has no parent */
	return(retval);
    }

    if (IOObjectConformsTo(parent, "IOBlockStorageDriver"))
    {
	/*
	 * Get a unique device path identifier.
	 * Devices available at boot have an Open Firmware Device Tree path.
	 * The OF path is short and concise and should be first choice.
	 * Devices that show up after boot, are guaranteed to have
	 * a Service Plane, hardware unique path.
	 */

        bzero(path, sizeof(io_string_t));
	if (IORegistryEntryGetPath(drive, kIODeviceTreePlane, path) != KERN_SUCCESS)
	{
	    if(IORegistryEntryGetPath(drive, kIOServicePlane, path) != KERN_SUCCESS)
		/* device has no unique path identifier */
		goto RETURN;
	}
	retval++;

	/* get drive properties */
	status = IORegistryEntryCreateCFProperties(drive,
	  (CFMutableDictionaryRef *)&properties,
	  kCFAllocatorDefault,
	  kNilOptions);
	if (status != KERN_SUCCESS)
	{
            /* device has no properties */
	    goto RETURN;
	}

	bzero(BSDName, MAXDRIVENAME+1);
	/* get name from properties */
	name = (CFStringRef)CFDictionaryGetValue(properties,
	  CFSTR(kIOBSDNameKey));
	if (name) {
	    CFStringGetCString(name, BSDName,
	      MAXDRIVENAME, CFStringGetSystemEncoding());
	    retval++;
	}

	/* get blocksize from properties */
	number = (CFNumberRef)CFDictionaryGetValue(properties,
	  CFSTR(kIOMediaPreferredBlockSizeKey));
	if (number != 0) {
	    CFNumberGetValue(number,
	      kCFNumberSInt64Type, &value);
	    drivestat->blocksize = value;
	    retval++;	    
	}
	CFRelease(properties);
    }
    else
	goto RETURN;

    /* we should have a name and blocksize at a minimum */
    if (retval != 3)
    {
	retval = FALSE;
	goto RETURN;
    }

    drive_id = check_device_path (BSDName, path, ndrives);
    if (drive_id == -1)
    {
	retval = FALSE;
	goto RETURN;
    }
    else
	drivestat->drivepath_id = drive_id;


    /* get parent drive properties */
    status = IORegistryEntryCreateCFProperties(parent,
      (CFMutableDictionaryRef *)&properties,
      kCFAllocatorDefault,
      kNilOptions);
    if (status != KERN_SUCCESS)
    {
        /* device has no properties */
	goto RETURN;
    }
    
    /* Obtain the statistics from the parent drive properties. */
    
    statistics
      = (CFDictionaryRef)CFDictionaryGetValue(properties,
      CFSTR(kIOBlockStorageDriverStatisticsKey));

    if (statistics != 0)
    {
	/* Get number of reads. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsReadsKey));
	if (number != 0) {
	    CFNumberGetValue(number,
	      kCFNumberSInt64Type, &value);
	    drivestat->Reads = value;
	}

	/* Get bytes read. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->BytesRead = value;
	}
		
	/* Get number of writes. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsWritesKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->Writes = value;
	}

	/* Get bytes written. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->BytesWritten = value;
	}

	/* Get LatentReadTime. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsLatentReadTimeKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->LatentReadTime = value;
	}

	/* Get LatentWriteTime. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsLatentWriteTimeKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->LatentWriteTime = value;
	}

	/* Get ReadErrors. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsReadErrorsKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->ReadErrors = value;
	}

	/* Get WriteErrors. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsWriteErrorsKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->WriteErrors = value;
	}

	/* Get ReadRetries. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsReadRetriesKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->ReadRetries = value;
	}

	/* Get WriteRetries. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsWriteRetriesKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->WriteRetries = value;
	}

	/* Get TotalReadTime. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsTotalReadTimeKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->TotalReadTime = value;
	}

	/* Get WriteRetries. */
	number =
	  (CFNumberRef)CFDictionaryGetValue(statistics,
	  CFSTR(kIOBlockStorageDriverStatisticsTotalWriteTimeKey));
	if (number != 0) {
	    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
	    drivestat->TotalWriteTime = value;
	}
	    
	CFRelease(properties);
    } /* end if statistics != 0 */

    RETURN:
    IOObjectRelease(parent);		    
    return(retval);
}


/*
 * find IOMedia objects
 * This routine always gives me a lower count on the number
 * of disks.  I don't know which one to use.
 */
static int
get_ndrives(void)
{
    io_iterator_t drivelist;
    io_registry_entry_t drive;
    io_registry_entry_t parent;    
    CFMutableDictionaryRef match;
    int error, ndrives;
    kern_return_t status;

    /*
     * Get an iterator for IOMedia objects.
     */
    match = IOServiceMatching("IOMedia");
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    status = IOServiceGetMatchingServices(masterPort, match, &drivelist);
    if (status != KERN_SUCCESS)
	return(0);

    /*
     * Scan all of the IOMedia objects, and count each
     * object that has a parent IOBlockStorageDriver
     *
     * XXX What about RAID devices?
     */
    error = 1;
    ndrives = 0;
    while ((drive = IOIteratorNext(drivelist)))
    {
	/* get drive's parent */
	status = IORegistryEntryGetParentEntry(drive,
	  kIOServicePlane, &parent);
	if (status != KERN_SUCCESS)
	{
	    IOObjectRelease(drive);
	    continue;
	}

	if (IOObjectConformsTo(parent, "IOBlockStorageDriver"))
	{
	    error = 0;
	    ndrives++;
	}
	IOObjectRelease(parent);
	IOObjectRelease(drive);
    }
    
    IOObjectRelease(drivelist);

    return(ndrives);
}


/*
 * When getting the stats, do it in the order
 * of their type.  The types that have the most
 * data come first in the list if possible.
 * This makes the sar reporter tool more efficient,
 * because in some cases, it will allocate a buffer
 * and keep reusing it as long as the sample data fits.
 * When a sample data doesn't fit, it reallocates the buffer
 * to a bigger size etc.
 */
void
get_all_stats()
{

    get_drivestat_sample();
    get_netstat_sample(network_mode);
    get_vmstat_sample();
    get_cpu_sample();
}


/*
 * An internal table maps the BSDName to a unique ioregistry path.
 * The table's index is then used as a unique compressed path, and
 * helps track disks that come and go during the sampling intervals.
 * This routine finds an entry that maps both the BSDName and the
 * IOKit registry path.  If no mapping is discovered, a new entry
 * is created.  An entry is never removed, this maintaining the
 * unique index throughout the data collection.
 * Success returns the map index. Failure returns -1.
 */
static int 
check_device_path (char *name, char *path, int ndrives)
{
    int i;
    int index;
    int n;

    if (dp_table == NULL)
    {
	/* First setup of internal drivepath table */
	dp_table = (struct drivepath *)malloc (ndrives * sizeof(struct drivepath));
	if (dp_table == NULL)
	    return(-1);
	else
	{
	    bzero(dp_table, (ndrives * sizeof(struct drivepath)));
	    dp_count = ndrives;
	    drivepath_record.rec_size = sizeof(struct drivepath);
	}
    }

    for (i=0; i < dp_count; i++)
    {
	if (dp_table[i].state == DPSTATE_UNINITIALIZED)
	{
	    /* This is a new drive entry that should be recorded */
	    index = i;
	    goto NEW_ENTRY;
	}
	else if (!strcmp (dp_table[i].ioreg_path, path))
	{
	    /* Found a matching hardware path */
	    if (!strcmp(dp_table[i].BSDName, name))
	    {
		/* The BSDName matches the entry in the table
		 * so there is no need to record this data.
		 */
		return(i);
	    }
	    else
	    {
		/* The BSDName is different ... implies a change,
		 * like the drive was removed and now is back
		 */
		bzero((char *)dp_table[i].BSDName, MAXDRIVENAME+1);
		dp_table[i].drivepath_id = i;
		dp_table[i].state = DPSTATE_CHANGED;
		strcpy(dp_table[i].BSDName, name);
		write_record_hdr(&drivepath_record);
		write_record_data((char *)&dp_table[i], sizeof(struct drivepath));
		return(i);
	    }
	}
    } /* end for loop */
    
    /* 
     * If we reach this point, then we've run out of
     * table entries. Double the size of the table.
     */
    n = dp_count * 2;
    dp_table = (struct drivepath *)realloc(dp_table, n * sizeof(struct drivepath));
    bzero(&dp_table[dp_count], dp_count * sizeof(struct drivepath));
    index = dp_count;
    dp_count = n;

    /* This is a new drive entry that should be recorded */
    NEW_ENTRY:
    dp_table[index].drivepath_id = index;
    dp_table[index].state = DPSTATE_NEW;
    strcpy(dp_table[index].BSDName, name);
    strcpy(dp_table[index].ioreg_path, path);
    write_record_hdr(&drivepath_record);
    write_record_data((char *)&dp_table[index], sizeof(struct drivepath));  
    return(index);
}



/*
 * Thus far, only the networking stats take an optional flag
 * to modify the collection of data.  The number of ppp
 * interfaces can be very high, causing the raw data file to
 * grow very large.  We want this option to include ppp
 * statistics to be off by default.  When we see the -m PPP
 * mode passed in, ppp collection will be turned on.
 */
static void
get_netstat_sample(int mode)
{

    int n;
    int ns_index     = 0;
    char                    tname[MAX_TNAME_SIZE + 1];
    char                    name[MAX_TNAME_UNIT_SIZE + 1];
    struct ifaddrs *ifa_list, *ifa;


	/*
	 * Set the starting table size to 100 entries
	 * That should be big enough for most cases,
	 * even with a lot of ppp connections.
	 */
	ns_count = 100;
	ns_table = (struct netstats *) malloc(ns_count * sizeof (struct netstats));
	if (ns_table == NULL)
	{
	    fprintf(stderr, "sadc: malloc netstat table failed\n");
	    return;
	}

    bzero(ns_table, ns_count * sizeof(struct netstats));
    if (getifaddrs(&ifa_list) == -1)
   	    return;

	for (ifa = ifa_list; ifa; ifa = ifa->ifa_next)
	{
        struct if_data *if_data = (struct if_data *)ifa->ifa_data;
        
		if (AF_LINK != ifa->ifa_addr->sa_family)
			continue;
		if (ifa->ifa_data == 0)
			continue;
	    tname[MAX_TNAME_SIZE] = '\0';
	    if (!(network_mode & NET_PPP_MODE))
	    {
		/*
		 * If the flag is set, include PPP connections.
		 * By default this collection is turned off
		 */
		if(!strncmp(ifa->ifa_name, "ppp", 3))
		    continue;
	    }
	    snprintf(name, MAX_TNAME_UNIT_SIZE, "%s", ifa->ifa_name);
	    name[MAX_TNAME_UNIT_SIZE] = '\0';

	    if (ns_index == ns_count)
	    {
		/* the stat table needs to grow */
		n = ns_count * 2;
		ns_table = (struct netstats *)realloc(ns_table, n * sizeof(struct netstats));
		bzero(&ns_table[ns_count], ns_count * sizeof(struct netstats));
		ns_count = n;
	    }

	    /*
	     * As a means of helping to identify when interface unit numbers
	     * are reused, a generation counter may eventually be implemented.
	     * This will be especially helpful with ppp-x connections.
	     * In anticipation, we will reserve a space for it, but always
	     * set it to zero for now.
	     */
	    ns_table[ns_index].gen_counter = 0;
	    
	    strncpy(ns_table[ns_index].tname_unit, name, MAX_TNAME_UNIT_SIZE);
	    ns_table[ns_index].tname_unit[MAX_TNAME_UNIT_SIZE] = '\0';
	    ns_table[ns_index].net_ipackets = if_data->ifi_ipackets;
	    ns_table[ns_index].net_ierrors  = if_data->ifi_ierrors;	    	    
	    ns_table[ns_index].net_opackets = if_data->ifi_opackets;
	    ns_table[ns_index].net_oerrors  = if_data->ifi_oerrors;
	    ns_table[ns_index].net_collisions = if_data->ifi_collisions;	    
	    ns_table[ns_index].net_ibytes   = if_data->ifi_ibytes;
	    ns_table[ns_index].net_obytes   = if_data->ifi_obytes;	    
	    ns_table[ns_index].net_imcasts   = if_data->ifi_imcasts;
	    ns_table[ns_index].net_omcasts   = if_data->ifi_omcasts;
	    ns_table[ns_index].net_drops      = if_data->ifi_iqdrops;
	    ns_index++;
	}  /* end for */

	netstats_record.rec_count = ns_index;
	netstats_record.rec_size = sizeof(struct netstats);
	write_record_hdr(&netstats_record);
	write_record_data((char *)ns_table, (ns_index * sizeof(struct netstats)));
    return;
}
