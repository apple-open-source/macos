/*
    cc -I/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders -arch x86_64 -arch i386  -O -o trace trace.c
*/


#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/ucred.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>
#include <stdarg.h>
#include <inttypes.h>

#include <libutil.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/
#include <sys/param.h>

#include <mach/mach.h>
#include <mach/mach_time.h>


int nbufs = 0;
int enable_flag=0;
int execute_flag=0;
int logRAW_flag=0;
int LogRAW_flag=0;
int readRAW_flag = 0;
int disable_flag=0;
int init_flag=0;
int kval_flag=0;
int remove_flag=0;
int bufset_flag=0;
int bufget_flag=0;
int filter_flag=0;
int filter_file_flag=0;
int filter_alloced=0;
int trace_flag=0;
int nowrap_flag=0;
int freerun_flag=0;
int verbose_flag=0;
int usage_flag=0;
int pid_flag=0;
int pid_exflag=0;
int ppt_flag=0;
int done_with_args=0;
int no_default_codes_flag=0;

unsigned int value1=0;
unsigned int value2=0;
unsigned int value3=0;
unsigned int value4=0;

pid_t pid=0;
int reenable=0;

int force_32bit_exec = 0;
int frequency = 0;

int mib[6];
size_t needed;

char *logfile = (char *)0;      /* This file is trace format */
char *RAW_file = (char *)0;
FILE *output_file;
int   output_fd;

extern char **environ;

uint8_t* type_filter_bitmap;


#define DBG_FUNC_ALL		(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc
#define SHORT_HELP 1
#define LONG_HELP 0

#define CSC_MASK		0xffff0000

#define VFS_LOOKUP		0x03010090
#define BSC_exit		0x040c0004
#define BSC_thread_terminate	0x040c05a4
#define TRACE_DATA_NEWTHREAD	0x07000004
#define TRACE_STRING_NEWTHREAD	0x07010004
#define TRACE_STRING_EXEC	0x07010008
#define TRACE_LOST_EVENTS	0x07020008
#define MACH_SCHEDULED		0x01400000
#define MACH_MAKERUNNABLE	0x01400018
#define MACH_STKHANDOFF		0x01400008

#define EMPTYSTRING ""
#define UNKNOWN "unknown"

char tmpcommand[MAXCOMLEN];

int total_threads = 0;
int nthreads = 0;
kd_threadmap *mapptr = 0;

kd_cpumap_header* cpumap_header = NULL;
kd_cpumap* cpumap = NULL;

/* 
   If NUMPARMS changes from the kernel, 
   then PATHLENGTH will also reflect the change
   This is for the vfslookup entries that
   return pathnames
*/
#define NUMPARMS 23
#define PATHLENGTH (NUMPARMS*sizeof(long))


#define US_TO_SLEEP	50000
#define BASE_EVENTS	500000


double divisor;

typedef struct {
	uint32_t debugid;
	char	*debug_string;
} code_type_t;

code_type_t*	codesc = 0;
size_t			codesc_idx = 0; // Index into first empty codesc entry



typedef struct event *event_t;

struct event {
	event_t	  ev_next;

	uintptr_t ev_thread;
	uint32_t  ev_debugid;
	uint64_t  ev_timestamp;
};

typedef struct lookup *lookup_t;

struct lookup {
	lookup_t  lk_next;
	
	uintptr_t lk_thread;
	uintptr_t lk_dvp;
	long	 *lk_pathptr;
	long	  lk_pathname[NUMPARMS + 1];
};

typedef struct threadmap *threadmap_t;

struct threadmap {
	threadmap_t	tm_next;
	
	uintptr_t	tm_thread;
	uintptr_t	tm_pthread;
	boolean_t	tm_deleteme;
	char		tm_command[MAXCOMLEN + 1];
};
	

#define HASH_SIZE	1024
#define HASH_MASK	1023

event_t		event_hash[HASH_SIZE];
lookup_t	lookup_hash[HASH_SIZE];
threadmap_t	threadmap_hash[HASH_SIZE];

event_t		event_freelist;
lookup_t	lookup_freelist;
threadmap_t	threadmap_freelist;
threadmap_t	threadmap_temp;


#define		SBUFFER_SIZE	(128 * 4096)
char		sbuffer[SBUFFER_SIZE];

int secs_to_run = 0;
int use_current_buf = 0;


kbufinfo_t bufinfo = {0, 0, 0, 0};

int   codenum = 0;
int   codeindx_cache = 0;

static void quit(char *);
static int match_debugid(unsigned int, char *, int *);
static void usage(int short_help);
static int argtoi(int flag, char *req, char *str, int base);
static int parse_codefile(const char *filename);
static void codesc_find_dupes(void);
static int read_command_map(int, uint32_t);
static void read_cpu_map(int);
static void find_thread_command(kd_buf *, char **);
static void create_map_entry(uintptr_t, char *);
static void getdivisor();
static unsigned long argtoul();

static void set_enable(int);
static void set_remove();
static void set_nowrap();
static void set_pidcheck(int, int);
static void set_pidexclude(int, int);
static void set_numbufs(int);
static void set_freerun();
static void get_bufinfo(kbufinfo_t *);
static void set_init();
static void set_kval_list();
static void readtrace(char *);
static void log_trace();
static void Log_trace();
static void read_trace();
static void signal_handler(int);
static void signal_handler_RAW(int);
static void delete_thread_entry(uintptr_t);
static void find_and_insert_tmp_map_entry(uintptr_t, char *);
static void create_tmp_map_entry(uintptr_t, uintptr_t);
static void find_thread_name(uintptr_t, char **, boolean_t);

static int  writetrace(int);
static int  write_command_map(int);
static int  debugid_compar(code_type_t *, code_type_t *);

static threadmap_t find_thread_entry(uintptr_t);

static void saw_filter_class(uint8_t class);
static void saw_filter_end_range(uint8_t end_class);
static void saw_filter_subclass(uint8_t subclass);
static void filter_done_parsing(void);

static void set_filter(void);
static void set_filter_class(uint8_t class);
static void set_filter_range(uint8_t class, uint8_t end);
static void set_filter_subclass(uint8_t class, uint8_t subclass);

static void parse_filter_file(char *filename);

static void quit_args(const char *fmt, ...)  __printflike(1, 2);

#ifndef	KERN_KDWRITETR
#define KERN_KDWRITETR	17
#endif

#ifndef	KERN_KDWRITEMAP
#define KERN_KDWRITEMAP	18
#endif

#ifndef F_FLUSH_DATA
#define F_FLUSH_DATA	40
#endif

#ifndef RAW_VERSION1
typedef struct {
        int             version_no;
        int             thread_count;
        uint64_t        TOD_secs;
        uint32_t        TOD_usecs;
} RAW_header;

#define RAW_VERSION0	0x55aa0000
#define RAW_VERSION1    0x55aa0101
#endif

#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(*x)))

#define EXTRACT_CLASS_LOW(debugid)     ( (uint8_t) ( ((debugid) & 0xFF00   ) >> 8 ) )
#define EXTRACT_SUBCLASS_LOW(debugid)  ( (uint8_t) ( ((debugid) & 0xFF     )      ) )

#define ENCODE_CSC_LOW(class, subclass) \
  ( (uint16_t) ( ((class) & 0xff) << 8 ) | ((subclass) & 0xff) )

RAW_header	raw_header;



void set_enable(int val)
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;
#ifdef	KDEBUG_ENABLE_PPT
	if (ppt_flag && val) {
		mib[3] = KDEBUG_ENABLE_PPT;
	} else {
		mib[3] = val;
	}
#else
	mib[3] = val;
#endif
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KERN_KDENABLE: %s\n", strerror(errno));
}

void set_remove()
{
	extern int errno;
    
	errno = 0;
    
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	{
		if (errno == EBUSY)
			quit("the trace facility is currently in use...\n          fs_usage, sc_usage, trace, and latency use this feature.\n\n");
		else
			quit_args("trace facility failure, KERN_KDREMOVE: %s\n", strerror(errno));
	}
}

void set_numbufs(int nbufs)
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KERN_KDSETBUF: %s\n", strerror(errno));
    
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KERN_KDSETUP: %s\n", strerror(errno));
}

void set_nowrap()
{
        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDEFLAGS;
        mib[3] = KDBG_NOWRAP;
        mib[4] = 0;
        mib[5] = 0;		/* no flags */
        if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KDBG_NOWRAP: %s\n", strerror(errno));

}

void set_pidcheck(int pid, int on_off_flag)
{
	kd_regtype kr;
    
	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off_flag;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDTR;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
	{
		if (on_off_flag == 1)
		{
			printf("trace facility failure, KERN_KDPIDTR,\n\tpid %d does not exist\n", pid);
			set_remove();
			exit(2);
		}
	}
}

void set_pidexclude(int pid, int on_off_flag)
{
	kd_regtype kr;
    
	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off_flag;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDEX;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
	{
		if (on_off_flag == 1)
		{
			printf ("pid %d does not exist\n", pid);
			set_remove();
			exit(2);
		}
	}
}

void set_freerun()
{
        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDEFLAGS;
        mib[3] = KDBG_FREERUN;
        mib[4] = 0;
        mib[5] = 0;
        if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KDBG_FREERUN: %s\n", strerror(errno));
}

void get_bufinfo(kbufinfo_t *val)
{
	needed = sizeof (*val);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, val, &needed, 0, 0) < 0)
		quit_args("trace facility failure, KERN_KDGETBUF: %s\n", strerror(errno));
}

void set_init()
{
        kd_regtype kr;
    
        kr.type = KDBG_RANGETYPE;
        kr.value1 = 0;
        kr.value2 = -1;
        needed = sizeof(kd_regtype);
        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDSETREG;
        mib[3] = 0;
        mib[4] = 0;
	mib[5] = 0;
        if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
            quit_args("trace facility failure, KERN_KDSETREG (rangetype): %s\n", strerror(errno));
    
        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDSETUP;
        mib[3] = 0;
        mib[4] = 0;
        mib[5] = 0;
        if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
            quit_args("trace facility failure, KERN_KDSETUP: %s\n", strerror(errno));
}


static void
set_filter(void)
{
	errno = 0;
	int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDSET_TYPEFILTER };
	size_t needed = KDBG_TYPEFILTER_BITMAP_SIZE;

	if(sysctl(mib, ARRAYSIZE(mib), type_filter_bitmap, &needed, NULL, 0)) {
		quit_args("trace facility failure, KERN_KDSET_TYPEFILTER: %s\n", strerror(errno));
	}
}

void set_kval_list()
{
        kd_regtype kr;
    
        kr.type = KDBG_VALCHECK;
        kr.value1 = value1;
        kr.value2 = value2;
        kr.value3 = value3;
        kr.value4 = value4;
        needed = sizeof(kd_regtype);
        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDSETREG;
        mib[3] = 0;
        mib[4] = 0;
        mib[5] = 0;
        if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
            quit_args("trace facility failure, KERN_KDSETREG (valcheck): %s\n", strerror(errno));
}


void readtrace(char *buffer)
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 3, buffer, &needed, NULL, 0) < 0)
		quit_args("trace facility failure, KERN_KDREADTR: %s\n", strerror(errno));
}


int writetrace(int fd)
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDWRITETR;
	mib[3] = fd;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		return 1;

	return 0;
}


int write_command_map(int fd)
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDWRITEMAP;
	mib[3] = fd;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		return 1;

	return 0;
}


static
lookup_t handle_lookup_event(uintptr_t thread, int debugid, kd_buf *kdp)
{
	lookup_t	lkp;
	int		hashid;
	boolean_t	first_record = FALSE;

	hashid = thread & HASH_MASK;

	if (debugid & DBG_FUNC_START)
		first_record = TRUE;

	for (lkp = lookup_hash[hashid]; lkp; lkp = lkp->lk_next) {
		if (lkp->lk_thread == thread)
			break;
	}
	if (lkp == NULL) {
		if (first_record == FALSE)
			return (0);

		if ((lkp = lookup_freelist))
			lookup_freelist = lkp->lk_next;
		else
			lkp = (lookup_t)malloc(sizeof(struct lookup));

		lkp->lk_thread = thread;

		lkp->lk_next = lookup_hash[hashid];
		lookup_hash[hashid] = lkp;
	}

	if (first_record == TRUE) {
		lkp->lk_pathptr = lkp->lk_pathname;
		lkp->lk_dvp = kdp->arg1;
	} else {
                if (lkp->lk_pathptr > &lkp->lk_pathname[NUMPARMS-4])
			return (lkp);

		*lkp->lk_pathptr++ = kdp->arg1;
	}
	*lkp->lk_pathptr++ = kdp->arg2;
	*lkp->lk_pathptr++ = kdp->arg3;
	*lkp->lk_pathptr++ = kdp->arg4;
	*lkp->lk_pathptr = 0;

	return (lkp);
}


static
void delete_lookup_event(uintptr_t thread, lookup_t lkp_to_delete)
{
	lookup_t	lkp;
	lookup_t	lkp_prev;
	int		hashid;

	hashid = thread & HASH_MASK;

	if ((lkp = lookup_hash[hashid])) {
		if (lkp == lkp_to_delete)
			lookup_hash[hashid] = lkp->lk_next;
		else {
			lkp_prev = lkp;

			for (lkp = lkp->lk_next; lkp; lkp = lkp->lk_next) {
				if (lkp == lkp_to_delete) {
					lkp_prev->lk_next = lkp->lk_next;
					break;
				}
				lkp_prev = lkp;
			}
		}
		if (lkp) {
			lkp->lk_next = lookup_freelist;
			lookup_freelist = lkp;
		}
	}
}


static
void insert_start_event(uintptr_t thread, int debugid, uint64_t now)
{
	event_t		evp;
	int		hashid;

	hashid = thread & HASH_MASK;

	for (evp = event_hash[hashid]; evp; evp = evp->ev_next) {
		if (evp->ev_thread == thread && evp->ev_debugid == debugid)
			break;
	}
	if (evp == NULL) {
		if ((evp = event_freelist))
			event_freelist = evp->ev_next;
		else
			evp = (event_t)malloc(sizeof(struct event));

		evp->ev_thread = thread;
		evp->ev_debugid = debugid;

		evp->ev_next = event_hash[hashid];
		event_hash[hashid] = evp;
	}
	evp->ev_timestamp = now;
}


static
uint64_t consume_start_event(uintptr_t thread, int debugid, uint64_t now)
{
	event_t		evp;
	event_t		evp_prev;
	int		hashid;
	uint64_t	elapsed = 0;

	hashid = thread & HASH_MASK;

	if ((evp = event_hash[hashid])) {
		if (evp->ev_thread == thread && evp->ev_debugid == debugid)
			event_hash[hashid] = evp->ev_next;
		else {
			evp_prev = evp;

			for (evp = evp->ev_next; evp; evp = evp->ev_next) {
				
				if (evp->ev_thread == thread && evp->ev_debugid == debugid) {
					evp_prev->ev_next = evp->ev_next;
					break;
				}
				evp_prev = evp;
			}
		}
		if (evp) {
			elapsed = now - evp->ev_timestamp;
			
			evp->ev_next = event_freelist;
			event_freelist = evp;
		}
	}
	return (elapsed);
}


void
log_trace()
{
        char		*buffer;
	uint32_t	buffer_size;
	int  		fd;
	int		size;
	int		pad_size;
	char		pad_buf[4096];


	if ((fd = open(logfile, O_TRUNC|O_WRONLY|O_CREAT, 0777)) == -1) {
		perror("Can't open logfile");
		exit(1);
	}
	get_bufinfo(&bufinfo);

	if (bufinfo.nolog != 1) {
		reenable = 1;
		set_enable(0);  /* disable logging*/
	}
	get_bufinfo(&bufinfo);

	if (verbose_flag) {
		if (bufinfo.flags & KDBG_WRAPPED)
			printf("Buffer has wrapped\n");
		else
			printf("Buffer has not wrapped\n");
	}
	buffer_size = 1000000 * sizeof(kd_buf);
	buffer = malloc(buffer_size);

	if (buffer == (char *) 0)
		quit("can't allocate memory for tracing info\n");

	read_command_map(0, 0);
	read_cpu_map(0);

	raw_header.version_no = RAW_VERSION1;
	raw_header.thread_count = total_threads;
	raw_header.TOD_secs = time((long *)0);
	raw_header.TOD_usecs = 0;

	write(fd, &raw_header, sizeof(RAW_header));

	size = total_threads * sizeof(kd_threadmap);
	write(fd, (char *)mapptr, size);

	pad_size = 4096 - ((sizeof(RAW_header) + size) & 4095);

	if (cpumap_header) {
		size_t cpumap_size = sizeof(kd_cpumap_header) + cpumap_header->cpu_count * sizeof(kd_cpumap);
		if (pad_size >= cpumap_size) {
			write(fd, (char *)cpumap_header, cpumap_size);
			pad_size -= cpumap_size;
		}
	}

	memset(pad_buf, 0, pad_size);
	write(fd, pad_buf, pad_size);

	for (;;) {
		needed = buffer_size;

		readtrace(buffer);

		if (needed == 0)
			break;
		write(fd, buffer, needed * sizeof(kd_buf));
	}
	close(fd);
}


void
Log_trace()
{
	int	size;
	kd_buf  kd_tmp;
	size_t	len;
	int	num_cpus;
	int	try_writetrace = 1;
	int	fd;
        char		*buffer;
        kd_buf		*kd;
	uint64_t sample_window_abs;
	uint64_t next_window_begins;
	uint64_t current_abs;
	uint64_t ending_abstime;
	uint64_t last_time_written;
	uint32_t us_to_sleep;
	uint32_t us_to_adjust;
	uint32_t ms_to_run;

	memset(&kd_tmp, 0, sizeof(kd_tmp));

	if ((fd = open(logfile, O_TRUNC|O_WRONLY|O_CREAT, 0777)) == -1) {
		perror("Can't open logfile");
		exit(1);
	}
	if (use_current_buf == 0) {
		/*
		 * grab the number of cpus and scale the buffer size
		 */
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		mib[2] = 0;
		len = sizeof(num_cpus);

		sysctl(mib, 2, &num_cpus, &len, NULL, 0);

		if (!bufset_flag)
			nbufs = BASE_EVENTS * num_cpus;

		set_remove();
		set_numbufs(nbufs);
		set_init();

		if (filter_flag)
			set_filter();

		if (kval_flag)
			set_kval_list();
	}
	/* Get kernel buffer information */
	get_bufinfo(&bufinfo);

	buffer = malloc(bufinfo.nkdbufs * sizeof(kd_buf));
	if (buffer == (char *) 0)
		quit("can't allocate memory for tracing info\n");
	memset(buffer, 0, bufinfo.nkdbufs * sizeof(kd_buf));

	if (use_current_buf == 0)
		set_enable(1);

	if (write_command_map(fd)) {
		int  pad_size;
		char pad_buf[4096];

		read_command_map(0, 0);
		read_cpu_map(0);

		raw_header.version_no = RAW_VERSION1;
		raw_header.thread_count = total_threads;
		raw_header.TOD_secs = time((long *)0);
		raw_header.TOD_usecs = 0;

		write(fd, &raw_header, sizeof(RAW_header));

		size = total_threads * sizeof(kd_threadmap);
		write(fd, (char *)mapptr, size);

		pad_size = 4096 - ((sizeof(RAW_header) + size) & 4095);

		if (cpumap_header) {
			size_t cpumap_size = sizeof(kd_cpumap_header) + cpumap_header->cpu_count * sizeof(kd_cpumap);
			if (pad_size >= cpumap_size) {
				write(fd, (char *)cpumap_header, cpumap_size);
				pad_size -= cpumap_size;
			}
		}

		memset(pad_buf, 0, pad_size);			
		write(fd, pad_buf, pad_size);
	}
	sample_window_abs = (uint64_t)((double)US_TO_SLEEP * divisor);

	next_window_begins = mach_absolute_time() + sample_window_abs;

	if (secs_to_run) {
		ending_abstime = mach_absolute_time() + (uint64_t)((double)secs_to_run * (double)1000000 * divisor);
		ms_to_run = secs_to_run * 1000;
	} else
		ms_to_run = 0;
	last_time_written = mach_absolute_time();

	while (LogRAW_flag) {
		current_abs = mach_absolute_time();

		if (try_writetrace) {
			needed = ms_to_run;
				
			if (writetrace(fd))
				try_writetrace = 0;
			else {
				if (needed) {
					current_abs = mach_absolute_time();

					printf("wrote %d events - elapsed time = %.1f secs\n",
					       (int)needed, ((double)(current_abs - last_time_written) / divisor) / 1000000);

					last_time_written = current_abs;
				}
			}
		}
		if (try_writetrace == 0) {

			if (next_window_begins > current_abs)
				us_to_adjust = US_TO_SLEEP - (uint32_t)((double)(next_window_begins - current_abs) / divisor);
			else
				us_to_adjust = US_TO_SLEEP;
			
			next_window_begins = current_abs + sample_window_abs;

			us_to_sleep = US_TO_SLEEP - us_to_adjust;

			next_window_begins = current_abs + (uint64_t)((double)(us_to_sleep + US_TO_SLEEP) * divisor);

			if (us_to_sleep)
				usleep(us_to_sleep);

			get_bufinfo(&bufinfo);

			if (bufinfo.flags & KDBG_WRAPPED)
				printf("lost events\n");

			needed = bufinfo.nkdbufs * sizeof(kd_buf);

			readtrace(buffer);

			if (bufinfo.flags & KDBG_WRAPPED) {

				kd = (kd_buf *) buffer;

				kd_tmp.timestamp = kd[0].timestamp;
				kd_tmp.debugid = TRACE_LOST_EVENTS;

				write(fd, &kd_tmp, sizeof(kd_tmp));
			}
			write(fd, buffer, needed * sizeof(kd_buf));

			if (verbose_flag && needed > nbufs)
				printf("needed = %ld\n", needed);
		}
		if (secs_to_run) {
			current_abs = mach_absolute_time();

			if (current_abs > ending_abstime)
				break;
			ms_to_run = (ending_abstime - current_abs) / (1000 * 1000);
			
			if (ms_to_run == 0)
				break;
		}
	}
	set_enable(0);
	set_numbufs(0);
	set_remove();

	close(fd);
}


void read_trace()
{
        char		*buffer;
	uint32_t	buffer_size;
        kd_buf		*kd;
	int		fd;
	int 		firsttime = 1;
	int		lines = 0;
	int		io_lines = 0;
	uint64_t	bias = 0;
	uint32_t	count_of_names;
	double		last_event_time = 0.0;
	time_t		trace_time;

	if (!readRAW_flag) {
		get_bufinfo(&bufinfo);

		if (bufinfo.nolog != 1) {
			reenable = 1;
			set_enable(0);  /* disable logging*/
		}
		if (verbose_flag) {
			if (bufinfo.flags & KDBG_WRAPPED)
				printf("Buffer has wrapped\n");
			else
				printf("Buffer has not wrapped\n");
		}
		fd = 0;
		count_of_names = 0;

	} else {
		fd = open(RAW_file, O_RDONLY);

		if (fd < 0) {
			perror("Can't open file");
			exit(1);
		}
		if (read(fd, &raw_header, sizeof(RAW_header)) != sizeof(RAW_header)) {
			perror("read failed");
			exit(2);
		}
		if (raw_header.version_no != RAW_VERSION1) {
			raw_header.version_no = RAW_VERSION0;
			raw_header.TOD_secs = time((long *)0);
			raw_header.TOD_usecs = 0;

			lseek(fd, (off_t)0, SEEK_SET);

			if (read(fd, &raw_header.thread_count, sizeof(int)) != sizeof(int)) {
				perror("read failed");
				exit(2);
			}
		}
		count_of_names = raw_header.thread_count;
		trace_time = (time_t) (raw_header.TOD_secs);

		printf("%s\n", ctime(&trace_time));
	}
	buffer_size = 1000000 * sizeof(kd_buf);
	buffer = malloc(buffer_size);

	if (buffer == (char *) 0)
		quit("can't allocate memory for tracing info\n");

	kd = (kd_buf *)buffer;

	read_command_map(fd, count_of_names);
	read_cpu_map(fd);

	for (;;) {
		uint32_t count;
		uint64_t now = 0;
		uint64_t prev;
		uint64_t prevdelta;
		uint32_t cpunum;
		uintptr_t thread;
		double	x = 0.0;
		double	y = 0.0;
		double  event_elapsed_time;
		kd_buf *kdp;
		lookup_t  lkp;
		boolean_t ending_event;
		int	i;
		int	debugid;
		int	debugid_base;
		int	dmsgindex;
		char	dbgmessge[80];
		char	outbuf[32];
		char	*command;

		if (!readRAW_flag) {
			needed = buffer_size;

			mib[0] = CTL_KERN;
			mib[1] = KERN_KDEBUG;
			mib[2] = KERN_KDREADTR;		
			mib[3] = 0;
			mib[4] = 0;
			mib[5] = 0;
			if (sysctl(mib, 3, buffer, &needed, NULL, 0) < 0)
				quit_args("trace facility failure, KERN_KDREADTR: %s\n", strerror(errno));

			if (needed == 0)
				break;
			count = needed;

		} else {
			uint32_t bytes_read;

			bytes_read = read(fd, buffer, buffer_size);

			if (bytes_read == -1) {
				perror("read failed");
				exit(2);
			}
			count = bytes_read / sizeof(kd_buf);

			if (count == 0)
				break;
		}
		for (kdp = &kd[0], i = 0; i < count; i++, kdp++) {

			prev = now;
			debugid = kdp->debugid;
			debugid_base = debugid & DBG_FUNC_MASK;
			now = kdp->timestamp & KDBG_TIMESTAMP_MASK;

			if (firsttime)
				bias = now;
			now -= bias;

			cpunum = kdbg_get_cpu(kdp); 
			thread = kdp->arg5;

			if (lines == 64 || firsttime)
			{
				prevdelta = now - prevdelta;

				if (firsttime)
					firsttime = 0;
				else {
					x = (double)prevdelta;
					x /= divisor;

					fprintf(output_file, "\n\nNumber of microsecs since in last page %8.1f\n", x);
				}
				prevdelta = now;
           
				/* 
				 * Output description row to output file (make sure to format correctly for 32-bit and 64-bit)
				 */
				fprintf(output_file,
#ifdef __LP64__
					"   AbsTime(Us)      Delta            debugid                       arg1             arg2             arg3             arg4              thread         cpu#  command\n\n"
#else
					"   AbsTime(Us)      Delta            debugid                       arg1           arg2           arg3           arg4                thread   cpu#  command\n\n"
#endif
					);
	    
				lines = 0;
				
				if (io_lines > 15000) {
					fcntl(output_fd, F_FLUSH_DATA, 0);

					io_lines = 0;
				}
			}
			lkp = 0;

			if (debugid_base == VFS_LOOKUP) {
				lkp = handle_lookup_event(thread, debugid, kdp);

				if ( !lkp || !(debugid & DBG_FUNC_END))
					continue;
			}
			x = (double)now;
			x /= divisor;

			if (last_event_time)
				y = x - last_event_time;
			else
				y = x;
			last_event_time = x;
			ending_event = FALSE;

			/*
			 * Is this event from an IOP? If so, there will be no
			 * thread command, label it with the symbolic IOP name
			 */
			if (cpumap && (cpunum < cpumap_header->cpu_count) && (cpumap[cpunum].flags & KDBG_CPUMAP_IS_IOP)) {
				command = cpumap[cpunum].name;
			} else {
				find_thread_command(kdp, &command);
			}

			/*
			 * The internal use TRACE points clutter the output.
			 * Print them only if in verbose mode.
			 */
			if (!verbose_flag)
			{
				/* Is this entry of Class DBG_TRACE */
				if ((debugid >> 24) == DBG_TRACE) {
					if (((debugid >> 16) & 0xff) != DBG_TRACE_INFO)
						continue;
				}
			}
			if ( !lkp) {
				int t_debugid;
				int t_thread;

				if ((debugid & DBG_FUNC_START) || debugid == MACH_MAKERUNNABLE) {

					if (debugid_base != BSC_thread_terminate && debugid_base != BSC_exit) {

						if (debugid == MACH_MAKERUNNABLE)
							t_thread = kdp->arg1;
						else
							t_thread = thread;

						insert_start_event(t_thread, debugid_base, now);
					}

				} else if ((debugid & DBG_FUNC_END) || debugid == MACH_STKHANDOFF || debugid == MACH_SCHEDULED) {
					
					if (debugid == MACH_STKHANDOFF || debugid == MACH_SCHEDULED) {
						t_debugid = MACH_MAKERUNNABLE;
						t_thread = kdp->arg2;
					} else {
						t_debugid = debugid_base;
						t_thread = thread;
					}
					event_elapsed_time = (double)consume_start_event(t_thread, t_debugid, now);
					event_elapsed_time /= divisor;
					ending_event = TRUE;

					if (event_elapsed_time == 0 && (debugid == MACH_STKHANDOFF || debugid == MACH_SCHEDULED))
						ending_event = FALSE;
				}
			}
			if (ending_event) {
				char *ch;

				sprintf(&outbuf[0], "(%-10.1f)", event_elapsed_time);
				/*
				 * fix that right paren
				 */
				ch = &outbuf[11];

				if (*ch != ')') {
					ch = strchr (&outbuf[0], ')');
				}
				if (ch)
				{
					*ch = ' ';
					--ch;

					while (ch != &outbuf[0])
					{
						if (*ch == ' ')
							--ch;
						else
						{
							*(++ch) = ')';
							break;
						}
					}
				}
			}
			if (match_debugid(debugid_base, dbgmessge, &dmsgindex)) {
				if (ending_event)
					fprintf(output_file, "%13.1f %10.1f%s %-28x  ", x, y, outbuf, debugid_base);
				else
					fprintf(output_file, "%13.1f %10.1f             %-28x  ", x, y, debugid_base);
			} else {
				if (ending_event)
					fprintf(output_file, "%13.1f %10.1f%s %-28.28s  ", x, y, outbuf, dbgmessge);
				else
					fprintf(output_file, "%13.1f %10.1f             %-28.28s  ", x, y, dbgmessge);
			}
			if (lkp) {
				char *strptr;
				int	len;

				strptr = (char *)lkp->lk_pathname;

				/*
				 * print the tail end of the pathname
				 */
				len = strlen(strptr);
				if (len > 51)
					len -= 51;
				else
					len = 0;
#ifdef __LP64__

				fprintf(output_file, "%-16lx %-51s %-16lx  %-2d %s\n", lkp->lk_dvp, &strptr[len], thread, cpunum, command);
#else
				fprintf(output_file, "%-8x   %-51s   %-8lx   %-2d  %s\n", (unsigned int)lkp->lk_dvp, &strptr[len], thread, cpunum, command);
#endif
				delete_lookup_event(thread, lkp);
			} else {
#ifdef __LP64__
				fprintf(output_file, "%-16lx %-16lx %-16lx %-16lx  %-16lx  %-2d %s\n", kdp->arg1, kdp->arg2, kdp->arg3, kdp->arg4, thread, cpunum, command);
#else
				fprintf(output_file, "%-8lx       %-8lx       %-8lx       %-8lx            %-8lx   %-2d  %s\n", kdp->arg1, kdp->arg2, kdp->arg3, kdp->arg4, thread, cpunum, command);
#endif
			}
			lines++;
			io_lines++;
		}
	}
	if (reenable == 1)
		set_enable(1);  /* re-enable kernel logging */
}



void signal_handler(int sig) 
{
	ptrace(PT_KILL, pid, (caddr_t)0, 0);
	/*
	 * child is gone; no need to disable the pid
	 */
	exit(2);
}


void signal_handler_RAW(int sig)
{
	LogRAW_flag = 0;
}



int main(argc, argv, env)
int argc;
char **argv;
char **env;
{
	extern char *optarg;
	extern int optind;
	int status;
	int ch;
	int i;
	char *output_filename = NULL;
	char *filter_filename = NULL;
	unsigned int parsed_arg;

	for (i = 1; i < argc; i++) {
		if (strcmp("-X", argv[i]) == 0) {
			force_32bit_exec = 1;
			break;
		}
	}
	if (force_32bit_exec) {
		if (0 != reexec_to_match_lp64ness(FALSE)) {
			fprintf(stderr, "Could not re-execute: %d\n", errno);
			exit(1);
		}
	} else {
		if (0 != reexec_to_match_kernel()) {
			fprintf(stderr, "Could not re-execute: %d\n", errno);
			exit(1);
		}
	}
        if (setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_PASSIVE) < 0) {
                printf("setiopolicy failed\n");
                exit(1);
        }
	output_file = stdout;
	output_fd = 1;

	while ((ch = getopt(argc, argv, "hedEk:irb:gc:p:s:tR:L:l:S:F:a:x:Xnfvo:PT:N")) != EOF)
	{
		switch(ch)
		{
		case 'h': /* help */
			usage_flag=1;
			break;
		case 'S':
			secs_to_run = argtoi('S', "decimal number", optarg, 10);
			break;
		case 'a': /* set tracing on a pid */
			pid_flag=1;
			pid = argtoi('a', "decimal number", optarg, 10);
			break;
		case 'x': /* exclude a pid from tracing */
			pid_exflag=1;
			pid = argtoi('x', "decimal number", optarg, 10);
			break;
		case 'v':
			verbose_flag=1;
			break;
		case 'l':
			logRAW_flag = 1;
			logfile = optarg;
			break;
		case 'L':
			LogRAW_flag = 1;
			logfile = optarg;
			signal(SIGINT, signal_handler_RAW);
			break;
		case 'e':
			enable_flag = 1;
			break;
		case 'i':
			init_flag = 1;
			break;
		case 'E':
			execute_flag = 1;
			break;
		case 'd':
			disable_flag = 1;
			break;
		case 'k':
			if (kval_flag == 0)
				value1 = (unsigned int) argtoul('k', "hex number", optarg, 16);
			else if (kval_flag == 1)
				value2 = (unsigned int) argtoul('k', "hex number", optarg, 16);
			else if (kval_flag == 2)
				value3 = (unsigned int) argtoul('k', "hex number", optarg, 16);
			else if (kval_flag == 3)
				value4 = (unsigned int) argtoul('k', "hex number", optarg, 16);
			else
			{
				fprintf(stderr, "A maximum of four values can be specified with -k\n");
				usage(SHORT_HELP);
			}
			kval_flag++;
			break;
		case 'r':
			remove_flag = 1;
			break;
		case 'g':
			bufget_flag = 1;
			break;
		case 't':
			trace_flag = 1;
			break;
		case 'R':
			readRAW_flag = 1;
			RAW_file = optarg;
			break;
		case 'n':
			nowrap_flag = 1;
			break;
		case 'f':
			freerun_flag = 1;
			break;
		case 'b':
			bufset_flag = 1;
			nbufs = argtoi('b', "decimal number", optarg, 10);
			break;
		case 'c':
			filter_flag = 1;
			parsed_arg = argtoi('c', "decimal, hex, or octal number", optarg, 0);
			if (parsed_arg > 0xFF)
				quit_args("argument '-c %s' parsed as %u, "
				          "class value must be 0-255\n", optarg, parsed_arg);
			saw_filter_class(parsed_arg);
			break;
		case 's':
			filter_flag = 1;
			parsed_arg = argtoi('s', "decimal, hex, or octal number", optarg, 0);
			if (parsed_arg > 0xFF)
				quit_args("argument '-s %s' parsed as %u, "
				          "subclass value must be 0-255\n", optarg, parsed_arg);
			saw_filter_subclass(parsed_arg);
			break;
		case 'p':
			filter_flag = 1;
			parsed_arg = argtoi('p', "decimal, hex, or octal number", optarg, 0);
			if (parsed_arg > 0xFF) 
				quit_args("argument '-p %s' parsed as %u, "
				          "end range value must be 0-255\n", optarg, parsed_arg);
			saw_filter_end_range(parsed_arg);
			break;
		case 'P':
			ppt_flag = 1;
			break;
		case 'o':
			output_filename = optarg;
			break;
		case 'F':
			frequency = argtoi('F', "decimal number", optarg, 10);
			break;
		case 'X':
			break;
		case 'N':
			no_default_codes_flag = 1;
			break;
		case 'T':
			filter_flag = 1;

			// Flush out any unclosed -c argument
			filter_done_parsing();

			parse_filter_file(optarg);
			break;
		default:
			usage(SHORT_HELP);
		}
	}
	argc -= optind;

	if (!no_default_codes_flag)
	{
		if (verbose_flag)
			printf("Adding default code file /usr/share/misc/trace.codes. Use '-N' to skip this.\n");
		parse_codefile("/usr/share/misc/trace.codes");
	}

	if (argc)
	{
		if (!execute_flag)
		{
			while (argc--)
			{
				const char *cfile = argv[optind++];
				if (verbose_flag) printf("Adding code file %s \n", cfile);
				parse_codefile(cfile);
			}
		}
	}
	else
	{
		if (execute_flag)
			quit_args("-E flag needs an executable to launch\n");
	}

	if (usage_flag)
		usage(LONG_HELP);

	getdivisor();

	if (pid_flag && pid_exflag)
		quit_args("Can't use both -a and -x flag together\n");

	if (kval_flag && filter_flag)
		quit_args("Cannot use -k flag with -c, -s, or -p\n");

	if (output_filename && !trace_flag && !readRAW_flag)
		quit_args("When using 'o' option, must use the 't' or 'R' option too\n");

	filter_done_parsing();

	done_with_args = 1;

	if (LogRAW_flag) {
		get_bufinfo(&bufinfo);

		if (bufinfo.nolog == 0)
			use_current_buf = 1;
	}

	if (disable_flag)
	{
		if (pid_flag)
		{
			set_pidcheck(pid, 0);   /* disable pid check for given pid */
			exit(1);
		}
		else if (pid_exflag)
		{
			set_pidexclude(pid, 0);  /* disable pid exclusion for given pid */
			exit(1);
		}
		set_enable(0);
		exit(1);
	}

	if (remove_flag)
	{
		set_remove();
		exit(1);
	}
    
	if (bufset_flag )
	{
		if (!init_flag && !LogRAW_flag)
		{
			fprintf(stderr,"The -b flag must be used with the -i flag\n");
			exit(1);
		}
		set_numbufs(nbufs);
	}
    
	if (nowrap_flag)
		set_nowrap();

	if (freerun_flag)
		set_freerun();
 
	if (bufget_flag)
	{
		get_bufinfo(&bufinfo);

		printf("The kernel buffer settings are:\n");

		if (bufinfo.flags & KDBG_BUFINIT)
			printf("\tKernel buffer is initialized\n");
		else
			printf("\tKernel buffer is not initialized\n");

		printf("\t  number of buf entries  = %d\n", bufinfo.nkdbufs);

		if (verbose_flag)
		{
			if (bufinfo.flags & KDBG_MAPINIT)
				printf("\tKernel thread map is initialized\n");
			else
				printf("\tKernel thread map is not initialized\n");
			printf("\t  number of thread entries  = %d\n", bufinfo.nkdthreads);
		}

		if (bufinfo.nolog)
			printf("\tBuffer logging is disabled\n");
		else
			printf("\tBuffer logging is enabled\n");

		if (verbose_flag)
			printf("\tkernel flags = 0x%x\n", bufinfo.flags);

		if (bufinfo.flags & KDBG_NOWRAP)
			printf("\tKernel buffer wrap is disabled\n");
		else
			printf("\tKernel buffer wrap is enabled\n");

		if (bufinfo.flags & KDBG_RANGECHECK)
			printf("\tCollection within a range is enabled\n");
		else
			printf("\tCollection within a range is disabled\n");

		if (bufinfo.flags & KDBG_VALCHECK)
			printf("\tCollecting specific code values is enabled\n");
		else
			printf("\tCollecting specific code values is disabled\n");
        
		if (bufinfo.flags & KDBG_TYPEFILTER_CHECK)
			printf("\tCollection based on a filter is enabled\n");
		else
			printf("\tCollection based on a filter is disabled\n");

		if (bufinfo.flags & KDBG_PIDCHECK)
			printf("\tCollection based on pid is enabled\n");
		else
			printf("\tCollection based on pid is disabled\n");

		if (bufinfo.flags & KDBG_PIDEXCLUDE)
			printf("\tCollection based on pid exclusion is enabled\n");
		else
			printf("\tCollection based on pid exclusion is disabled\n");

		if (bufinfo.bufid == -1)
			printf("\tKernel buffer is not controlled by any process.\n");
		else
			printf("\tKernel buffer is controlled by proc id [%d]\n", bufinfo.bufid);
	}

	if (init_flag)
		set_init();

	if (filter_flag)
		set_filter();

	if (kval_flag)
		set_kval_list();

	if (execute_flag)
	{
		fprintf(stderr, "Starting program: %s\n", argv[optind]);
		fflush(stdout);
		fflush(stderr);

		switch ((pid = vfork()))
		{
		case -1:
			perror("vfork: ");
			exit(1);
		case 0: /* child */
			setsid();
			ptrace(PT_TRACE_ME, 0, (caddr_t)0, 0);
			execve(argv[optind], &argv[optind], environ);
			perror("execve:");
			exit(1);
		}
		sleep(1);

		signal(SIGINT, signal_handler);
		set_pidcheck(pid, 1);
		set_enable(1);
		ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);
		waitpid(pid, &status, 0);
		/* child is gone; no need to disable the pid */
		exit(0);
	}
	else if (enable_flag)
	{
		if (pid_flag)
			set_pidcheck(pid, 1);
		else if (pid_exflag)
			set_pidexclude(pid, 1);
		set_enable(1);
	}

	if (output_filename)
	{
		if (((output_fd = open(output_filename, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0644)) < 0 ) ||
		    !(output_file = fdopen(output_fd, "w")))
		{
			fprintf(stderr, "Cannot open file \"%s\" for writing.\n", output_filename);
			usage(SHORT_HELP);
		}
		setbuffer(output_file, &sbuffer[0], SBUFFER_SIZE);
	
		if (fcntl(output_fd, F_NOCACHE, 1) < 0)
		{
			/* Not fatal */
			fprintf(stderr, "Warning: setting F_NOCACHE on %s, failed\n", output_filename);
		}
	}
	if (!LogRAW_flag && !logRAW_flag)
		setbuffer(output_file, &sbuffer[0], SBUFFER_SIZE);

	if (trace_flag || readRAW_flag)
		read_trace();
	else if (LogRAW_flag)
		Log_trace();
	else if (logRAW_flag)
		log_trace();

	exit(0);

} /* end main */

static void
quit_args(const char *fmt, ...) 
{
	char buffer[1024];

	if (reenable == 1)
	{
		reenable = 0;
		set_enable(1);  /* re-enable kernel logging */
	}

	va_list args;

	va_start (args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	fprintf(stderr, "trace error: %s", buffer);

	va_end(args);

	if (!done_with_args)
		usage(SHORT_HELP);

	exit(1);
}


void
quit(char *s)
{
	if (reenable == 1)
	{
		reenable = 0;
		set_enable(1);  /* re-enable kernel logging */
	}

	printf("trace: ");
	if (s)
		printf("%s ", s);
	exit(1);
}

static void
usage(int short_help)
{

	if (short_help)
	{
		(void)fprintf(stderr, "  usage: trace -h [-v]\n");
		(void)fprintf(stderr, "  usage: trace -i [-b numbufs]\n");
		(void)fprintf(stderr, "  usage: trace -g\n");
		(void)fprintf(stderr, "  usage: trace -d [-a pid | -x pid ]\n");
		(void)fprintf(stderr, "  usage: trace -r\n");
		(void)fprintf(stderr, "  usage: trace -n\n");

		(void)fprintf(stderr,
			      "  usage: trace -e [ -c class [[-s subclass]... | -p class ]]... | \n");
		(void)fprintf(stderr,
			      "                  [-k code | -k code | -k code | -k code] [-P] [-T tracefilter] \n");
		(void)fprintf(stderr,
			      "                  [-a pid | -x pid] \n\n");

		(void)fprintf(stderr,
			      "  usage: trace -E [ -c class [[-s subclass]... | -p class ]]... | \n");
		(void)fprintf(stderr,
			      "                  [-k code | -k code | -k code | -k code] [-P] [-T tracefilter] \n");
		(void)fprintf(stderr,
			      "                  executable_path [optional args to executable] \n\n");

		(void)fprintf(stderr,
			      "  usage: trace -L RawFilename [-S SecsToRun]\n");
		(void)fprintf(stderr,
			      "  usage: trace -l RawFilename\n");
		(void)fprintf(stderr,
			      "  usage: trace -R RawFilename [-X] [-F frequency] [-o OutputFilename] [-N] [ExtraCodeFilename1 ExtraCodeFilename2 ...]\n");
		(void)fprintf(stderr,
			      "  usage: trace -t [-o OutputFilename] [-N] [ExtraCodeFilename1 ExtraCodeFilename2 ...]\n");
		(void)fprintf(stderr,
				  "  Trace will import /usr/share/misc/trace.codes as a default codefile unless -N is specified. Extra codefiles specified are used in addition to the default codefile.\n");
		exit(1);
	}


	/* Only get here if printing long usage info */
	(void)fprintf(stderr, "usage: trace -h [-v]\n");
	(void)fprintf(stderr, "\tPrint this long command help.\n\n");
	(void)fprintf(stderr, "\t -v Print extra information about tracefilter and code files.\n\n");

	(void)fprintf(stderr, "usage: trace -i [-b numbufs]\n");
	(void)fprintf(stderr, "\tInitialize the kernel trace buffer.\n\n");
	(void)fprintf(stderr, "\t-b numbufs    The number of trace elements the kernel buffer\n");
	(void)fprintf(stderr, "\t              can hold is set to numbufs.  Use with the -i flag.\n");
	(void)fprintf(stderr, "\t              Enter a decimal value.\n\n");

	(void)fprintf(stderr, "usage: trace -g\n");
	(void)fprintf(stderr, "\tGet the kernel buffer settings.\n\n");

	(void)fprintf(stderr, "usage: trace -d [-a pid | -x pid]\n");
	(void)fprintf(stderr, "\tDisable/stop collection of kernel trace elements.\n\n");
	(void)fprintf(stderr, "\t -a pid     Disable/stop collection for this process only.\n\n");
	(void)fprintf(stderr, "\t -x pid     Disable/stop exclusion of this process only.\n\n");

	(void)fprintf(stderr, "usage: trace -r\n");
	(void)fprintf(stderr, "\tRemove the kernel trace buffer. Set controls to default.\n\n");

	(void)fprintf(stderr, "usage: trace -n\n");
	(void)fprintf(stderr, "\tDisables kernel buffer wrap around.\n\n");

	(void)fprintf(stderr,
	              "usage: trace -e [ -c class [[-s subclass]... | -p class ]]...  |\n");
	(void)fprintf(stderr,
	              "             [-k code | -k code | -k code | -k code] [-P] [-T tracefilter]\n");
	(void) fprintf(stderr,
	              "             [-a pid | -x pid]\n\n");
	(void)fprintf(stderr, "\t Enable/start collection of kernel trace elements. \n\n");
	(void)fprintf(stderr, "\t By default, trace collects all tracepoints. \n");
	(void)fprintf(stderr, "\t The following arguments may be used to restrict collection \n");
	(void)fprintf(stderr, "\t to a limited set of tracepoints. \n\n");
	(void)fprintf(stderr, "\t Multiple classes can be specified by repeating -c. \n");
	(void)fprintf(stderr, "\t Multiple subclasses can be specified by repeating -s after -c. \n");
	(void)fprintf(stderr, "\t Classes, subclasses, and class ranges can be entered \n");
	(void)fprintf(stderr, "\t in hex (0xXX), decimal (XX), or octal (0XX). \n\n");
	(void)fprintf(stderr, "\t -c class    Restrict trace collection to given class. \n\n");
	(void)fprintf(stderr, "\t -p class    Restrict trace collection to given class range. \n");
	(void)fprintf(stderr, "\t             Must provide class with -c first. \n\n");
	(void)fprintf(stderr, "\t -s subclass    Restrict trace collection to given subclass. \n");
	(void)fprintf(stderr, "\t                Must provide class with -c first. \n\n");
	(void)fprintf(stderr, "\t -a pid     Restrict trace collection to the given process.\n\n");
	(void)fprintf(stderr, "\t -x pid     Exclude the given process from trace collection.\n\n");
	(void)fprintf(stderr, "\t -k code    Restrict trace collection up to four specific codes.\n");
	(void)fprintf(stderr, "\t            Enter codes in hex (0xXXXXXXXX). \n\n");
	(void)fprintf(stderr, "\t -P         Enable restricted PPT trace points only.\n\n");
	(void)fprintf(stderr, "\t -T tracefilter     Read class and subclass restrictions from a \n");
	(void)fprintf(stderr, "\t                    tracefilter description file. \n");
	(void)fprintf(stderr, "\t                    Run trace -h -v for more info on this file. \n\n");

	(void)fprintf(stderr,
		      "usage: trace -E [ -c class [[-s subclass]... | -p class ]]... |\n");
	(void)fprintf(stderr,
		      "             [-k code | -k code | -k code | -k code] [-P] [-T tracefilter]\n");
	(void)fprintf(stderr,
		      "             executable_path [optional args to executable] \n\n");
	(void)fprintf(stderr, "\tLaunch the given executable and enable/start\n");
	(void)fprintf(stderr, "\tcollection of kernel trace elements for that process.\n");
	(void)fprintf(stderr, "\tSee -e(enable) flag for option descriptions.\n\n");

    (void)fprintf(stderr, "usage: trace -t [-o OutputFilename] [-N] [ExtraCodeFilename1 ExtraCodeFilename2 ...] \n"); 
	(void)fprintf(stderr, "\tCollect the kernel buffer trace data and print it.\n\n");
	(void)fprintf(stderr, "\t -N                 Do not import /usr/share/misc/trace.codes (for raw hex tracing or supplying an alternate set of codefiles)\n"); 
	(void)fprintf(stderr, "\t -o OutputFilename  Print trace output to OutputFilename. Default is stdout.\n\n");

	(void)fprintf(stderr,
		      "usage: trace -R RawFilename [-X] [-F frequency] [-o OutputFilename] [-N] [ExtraCodeFilename1 ExtraCodeFilename2 ...] \n");
	(void)fprintf(stderr, "\tRead raw trace file and print it.\n\n");
	(void)fprintf(stderr, "\t -X                 Force trace to interpret trace data as 32 bit. \n");
	(void)fprintf(stderr, "\t                          Default is to match the bit width of the current system. \n");
	(void)fprintf(stderr, "\t -N                 Do not import /usr/share/misc/trace.codes (for raw hex tracing or supplying an alternate set of codefiles)\n"); 
	(void)fprintf(stderr, "\t -F frequency       Specify the frequency of the clock used to timestamp entries in RawFilename.\n\t                    Use command \"sysctl hw.tbfrequency\" on the target device, to get target frequency.\n");
	(void)fprintf(stderr, "\t -o OutputFilename  Print trace output to OutputFilename. Default is stdout.\n\n");

	(void)fprintf(stderr,
		      "usage: trace -L RawFilename [-S SecsToRun]\n");
	(void)fprintf(stderr, "\tContinuously collect the kernel buffer trace data in the raw format \n");
	(void)fprintf(stderr, "\tand write it to RawFilename. \n");

	(void)fprintf(stderr, "\t-L implies -r -i if tracing isn't currently enabled.\n");
	(void)fprintf(stderr, "\tOptions passed to -e(enable) are also accepted by -L. (except -a -x -P)\n\n");
	(void)fprintf(stderr, "\t -S SecsToRun       Specify the number of seconds to collect trace data.\n\n");

	(void)fprintf(stderr,
		      "usage: trace -l RawFilename\n");
	(void)fprintf(stderr, "\tCollect the existing kernel buffer trace data in the raw format.\n\n");

	if (verbose_flag) {
		(void)fprintf(stderr,
		              "Code file: \n"
		              "\t A code file consists of a list of tracepoints, one per line, \n"
		              "\t with one tracepoint code in hex, followed by a tab, \n"
		              "\t followed by the tracepoint's name. \n\n"

		              "\t Example tracepoint: \n"
		              "\t 0x010c007c\tMSC_mach_msg_trap \n"
		              "\t This describes the tracepoint with the following info: \n"
		              "\t Name:          MSC_mach_msg_trap \n"
		              "\t Class:         0x01   (Mach events) \n"
		              "\t Subclass:      0x0c   (Mach system calls) \n"
		              "\t Code:          0x007c (Mach syscall number 31) \n\n"

		              "\t See /usr/include/sys/kdebug.h for the currently defined \n"
		              "\t class and subclass values. \n"
		              "\t See /usr/share/misc/trace.codes for the currently allocated \n"
		              "\t system tracepoints in trace code file format. \n"
		              "\t This codefile is useful with the -R argument to trace. \n"
		              "\n");

		(void)fprintf(stderr,
		              "Tracefilter description file: \n"
		              "\t A tracefilter description file consists of a list of \n"
		              "\t class and subclass filters in hex, one per line,  \n"
		              "\t which are applied as if they were passed with -c and -s. \n"
		              "\t Pass -v to see what classes and subclasses are being set. \n\n"

		              "\t File syntax: \n"
		              "\t  Class filter: \n"
		              "\t  C 0xXX \n"
		              "\t  Subclass filter (includes class): \n"
		              "\t  S 0xXXXX \n"
		              "\t  Comment: \n"
		              "\t  # This is a comment \n\n"

		              "\t For example, to trace Mach events (class 1):\n"
		              "\t C 0x01 \n"
		              "\t or to trace Mach system calls (class 1 subclass 13): \n"
		              "\t S 0x010C \n"
		              "\n");
	}

	exit(1);
}


static int
argtoi(flag, req, str, base)
int flag;
char *req, *str;
int base;
{
	char *cp;
	int ret;

	ret = (int)strtol(str, &cp, base);
	if (cp == str || *cp)
		errx(EINVAL, "-%c flag requires a %s", flag, req);
	return (ret);
}


static unsigned long
argtoul(flag, req, str, base)
int flag;
char *req, *str;
int base;
{
	char *cp;
	unsigned long ret;

	ret = (int)strtoul(str, &cp, base);
	if (cp == str || *cp)
		errx(EINVAL, "-%c flag requires a %s", flag, req);
	return (ret);
}


/*
 * comparison function for qsort
 * sort by debugid
 */
int debugid_compar(p1, p2)
	code_type_t *p1;
	code_type_t *p2;
{
	if (p1->debugid > p2->debugid)
		return(1);
	else if (p1->debugid == p2->debugid)
		return(0);
	else
		return(-1);
}


/*
 * Filter args parsing state machine:
 *
 * Allowed args:
 * -c -p
 * -c -s (-s)*
 * -c (-c)*
 * every -c goes back to start
 *
 * Valid transitions:
 * start -> class (first -c)
 * class -> range (-c -p)
 * class -> sub   (-c -s)
 * class -> class (-c -c)
 * range -> class (-c -p -c)
 * sub   -> class (-c -s -c)
 * *     -> start (on filter_done_parsing)
 *
 * Need to call filter_done_parsing after
 * calling saw_filter_*
 * to flush out any class flag waiting to see if
 * there is a -s flag coming up
 */


// What type of flag did I last see?
enum {
	FILTER_MODE_START,
	FILTER_MODE_CLASS,
	FILTER_MODE_CLASS_RANGE,
	FILTER_MODE_SUBCLASS
} filter_mode = FILTER_MODE_START;

uint8_t filter_current_class        = 0;
uint8_t filter_current_subclass     = 0;
uint8_t filter_current_class_range  = 0;

static void
saw_filter_class(uint8_t class)
{
	switch(filter_mode) {
	case FILTER_MODE_START:
	case FILTER_MODE_CLASS_RANGE:
	case FILTER_MODE_SUBCLASS:
		filter_mode = FILTER_MODE_CLASS;
		filter_current_class       = class;
		filter_current_subclass    = 0;
		filter_current_class_range = 0;
		// the case of a lone -c is taken care of
		// by filter_done_parsing
		break;
	case FILTER_MODE_CLASS:
		filter_mode = FILTER_MODE_CLASS;
		// set old class, remember new one 
		set_filter_class(filter_current_class);
		filter_current_class       = class;
		filter_current_subclass    = 0;
		filter_current_class_range = 0;
		break;
	default:
		quit_args("invalid case in saw_filter_class\n");
	}
}

static void
saw_filter_end_range(uint8_t end_class)
{
	switch(filter_mode) {
	case FILTER_MODE_CLASS:
		filter_mode = FILTER_MODE_CLASS_RANGE;
		filter_current_class_range = end_class;
		set_filter_range(filter_current_class, filter_current_class_range);
		break;
	case FILTER_MODE_START:
		quit_args("must provide '-c class' before '-p 0x%x'\n",
		          end_class);
	case FILTER_MODE_CLASS_RANGE:
		quit_args("extra range end '-p 0x%x'"
		          " for class '-c 0x%x'\n",
		          end_class, filter_current_class);
	case FILTER_MODE_SUBCLASS:
		quit_args("cannot provide both range end '-p 0x%x'"
		          " and subclass '-s 0x%x'"
		          " for class '-c 0x%x'\n",
		          end_class, filter_current_subclass,
		          filter_current_class);
	default:
		quit_args("invalid case in saw_filter_end_range\n");
	}
}

static void
saw_filter_subclass(uint8_t subclass)
{
	switch(filter_mode) {
	case FILTER_MODE_CLASS:
	case FILTER_MODE_SUBCLASS:
		filter_mode = FILTER_MODE_SUBCLASS;
		filter_current_subclass = subclass;
		set_filter_subclass(filter_current_class, filter_current_subclass);
		break;
	case FILTER_MODE_START:
		quit_args("must provide '-c class'"
		          " before subclass '-s 0x%x'\n", subclass);
	case FILTER_MODE_CLASS_RANGE:
		quit_args("cannot provide both range end '-p 0x%x'"
		          " and subclass '-s 0x%x'"
		          " for the same class '-c 0x%x'\n",
		          filter_current_class_range,
		          subclass, filter_current_class);
	default:
		quit_args("invalid case in saw_filter_subclass\n");
	}
}

static void
filter_done_parsing(void)
{
	switch(filter_mode) {
	case FILTER_MODE_CLASS:
		// flush out the current class
		set_filter_class(filter_current_class);
		filter_mode = FILTER_MODE_START;
		filter_current_class       = 0;
		filter_current_subclass    = 0;
		filter_current_class_range = 0;
		break;
	case FILTER_MODE_SUBCLASS:
	case FILTER_MODE_START:
	case FILTER_MODE_CLASS_RANGE:
		filter_mode = FILTER_MODE_START;
		filter_current_class       = 0;
		filter_current_subclass    = 0;
		filter_current_class_range = 0;
		break;
	default:
		quit_args("invalid case in filter_done_parsing\n");
	}
}

/* Tell set_filter_subclass not to print every. single. subclass. */
static boolean_t setting_class = FALSE;
static boolean_t setting_range = FALSE;

static void
set_filter_subclass(uint8_t class, uint8_t subclass)
{
	if (!filter_alloced) {
		type_filter_bitmap = (uint8_t *) calloc(1, KDBG_TYPEFILTER_BITMAP_SIZE);
		if (type_filter_bitmap == NULL)
			quit_args("Could not allocate type_filter_bitmap.\n");
		filter_alloced = 1;
	}

	uint16_t csc = ENCODE_CSC_LOW(class, subclass);

	if (verbose_flag && !setting_class) 
		printf("tracing subclass: 0x%4.4x\n", csc);

	if (verbose_flag && isset(type_filter_bitmap, csc))
		printf("class %u (0x%2.2x), subclass %u (0x%2.2x) set twice.\n",
		       class, class, subclass, subclass);

	setbit(type_filter_bitmap, csc);
}

static void
set_filter_class(uint8_t class)
{
	if (verbose_flag && !setting_range)
		printf("tracing class:    0x%2.2x\n", class);

	setting_class = TRUE;

	for (int i = 0; i < 256; i++)
		set_filter_subclass(class, i);

	setting_class = FALSE;
}

static void
set_filter_range(uint8_t class, uint8_t end)
{
	if (verbose_flag)
		printf("tracing range:    0x%2.2x - 0x%2.2x\n", class, end);

	setting_range = TRUE;

	for (int i = class; i <= end; i++)
		set_filter_class(i);

	setting_range = FALSE;
}

/*
 * Syntax of filter file:
 * Hexadecimal numbers only
 * Class:
 * C 0xXX
 * Subclass (includes class):
 * S 0xXXXX
 * Comment:
 * # <string>
 * TBD: Class ranges?
 * TBD: K for -k flag?
 */

static void
parse_filter_file(char *filename) {
	FILE* file;
	uint32_t current_line = 0;
	uint32_t parsed_arg   = 0;
	int rval;

	char line[256];

	if ( (file = fopen(filename, "r")) == NULL ) {
		quit_args("Failed to open filter description file %s: %s\n",
		          filename, strerror(errno));
	}

	if (verbose_flag)
		printf("Parsing typefilter file: %s\n", filename);

	while( fgets(line, sizeof(line), file) != NULL ) {
		current_line++;
		
		switch (line[0]) {
		case 'C':
			rval = sscanf(line, "C 0x%x\n", &parsed_arg);
			if (rval != 1)
				quit_args("invalid line %d of file %s: %s\n",
				         current_line, filename, line);
			if (parsed_arg > 0xFF)
				quit_args("line %d of file %s: %s\n"
				          "parsed as 0x%x, "
				          "class value must be 0x0-0xFF\n",
				          current_line, filename, line, parsed_arg);
			set_filter_class((uint8_t)parsed_arg);
			break;
		case 'S':
			rval = sscanf(line, "S 0x%x\n", &parsed_arg);
			if (rval != 1)
				quit_args("invalid line %d of file %s: %s\n",
				          current_line, filename, line);
			if (parsed_arg > 0xFFFF)
				quit_args("line %d of file %s: %s\n"
				          "parsed as 0x%x, "
				          "value must be 0x0-0xFFFF\n",
				          current_line, filename, line, parsed_arg);
			set_filter_subclass(EXTRACT_CLASS_LOW(parsed_arg),
			                    EXTRACT_SUBCLASS_LOW(parsed_arg));
			break;
		case '#':
			// comment
			break;
		case '\n':
			// empty line
			break;
		case '\0':
			// end of file
			break;
		default:
			quit_args("Invalid filter description file: %s\n"
			          "could not parse line %d: %s\n",
			          filename, current_line, line);
		}
	}

	fclose(file);
}


/*
 *  Find the debugid code in the list and return its index
 */
static int binary_search(list, lowbound, highbound, code)
	code_type_t *list;
	int lowbound, highbound;
	unsigned int code;
{
	int low, high, mid;
	int tries = 0;

	low = lowbound;
	high = highbound;

	while (1)
	{
		mid = (low + high) / 2;

		tries++;

		if (low > high)
			return (-1); /* failed */
		else if ( low + 1 >= high)
		{
			/* We have a match */
			if (list[high].debugid == code)
				return(high);
			else if (list[low].debugid == code)
				return(low);
			else
				return(-1);  /* search failed */
		}
		else if (code < list[mid].debugid)
			high = mid;
		else
			low = mid;
	} 
}


static int
parse_codefile(const char *filename)
{
	int fd;
	int i, j, line;
	size_t count;
	struct stat stat_buf;
	unsigned long file_size;
	char *file_addr, *endp; 

	if ((fd = open(filename, O_RDONLY, 0)) == -1)
	{
		printf("Failed to open code description file %s\n",filename);	
		return(-1);
	}

	if (fstat(fd, &stat_buf) == -1)
	{
		printf("Error: Can't fstat file: %s\n", filename);
		return(-1);
	}

	/*
	 * For some reason mapping files with zero size fails
	 * so it has to be handled specially.
	 */
	file_size = stat_buf.st_size;

	if (stat_buf.st_size != 0)
	{
		if ((file_addr = mmap(0, stat_buf.st_size, PROT_READ|PROT_WRITE, 
				      MAP_PRIVATE|MAP_FILE, fd, 0)) == (char*) -1)
		{
			printf("Error: Can't map file: %s\n", filename);
			close(fd);
			return(-1);
		}
	}
	else
	{
		// Skip empty files
		close(fd);
		return(0);
	}
	close(fd);


	/*
	 * If we get here, we have mapped the file
	 * and we are ready to parse it.  Count
	 * the newlines to get total number of codes.
	 */

	for (count = 0, j=1; j < file_size; j++)
	{
		if (file_addr[j] == '\n')
			count++;
	}

	if (count == 0)
	{
		printf("Error: No codes in %s\n", filename);
		return(-1);
	}

	/*
	 * Fudge the count to accomodate the last line in the file -
	 * in case it doesn't end in a newline.
	 */
	count++;

	// Grow the size of codesc to store new entries.
	size_t total_count = codesc_idx + count;
	code_type_t *new_codesc = (code_type_t *)realloc(codesc, (total_count) * sizeof(code_type_t));

	if (new_codesc == NULL) {
		printf("Failed to grow/allocate buffer. Skipping file %s\n", filename);
		return (-1);
	}
	codesc = new_codesc;
	bzero((char *)(codesc + codesc_idx), count * sizeof(code_type_t));

	for (line = 1, j = 0; j < file_size && codesc_idx < total_count; codesc_idx++)
	{
		/* Skip blank lines */
		while (file_addr[j] == '\n')
		{
			j++;
			line++;
		}
	
		/* Skip leading whitespace */
		while (file_addr[j] == ' ' || file_addr[j] == '\t')
			j++;

		/* Get the debugid code */
		codesc[codesc_idx].debugid = strtoul(file_addr + j, &endp, 16); 
		j = endp - file_addr;

		if (codesc[codesc_idx].debugid == 0) 
		{
			/* We didn't find a debugid code - skip this line */
			if (verbose_flag)
				printf("Error: while parsing line %d, skip\n", line);
			while (file_addr[j] != '\n' && j < file_size)
				j++;
			codesc_idx--; 
			line++;
			continue;
		}

		/* Skip whitespace */
		while (file_addr[j] == ' ' || file_addr[j] == '\t')
			j++;

		/* Get around old file that had count at the beginning */
		if (file_addr[j] == '\n')
		{
			/* missing debugid string - skip */
			if (verbose_flag)
				printf("Error: while parsing line %d, (0x%x) skip\n", line, codesc[codesc_idx].debugid);

			j++;
			codesc_idx--; 
			line++;
			continue;
		}

		/* Next is the debugid string terminated by a newline */
		codesc[codesc_idx].debug_string = &file_addr[j]; 

		/* Null out the newline terminator */
		while ((j < file_size) && (file_addr[j] != '\n'))
			j++;
		file_addr[j] = '\0'; /* File must be read-write */
		j++;
		line++;
		codenum++;	/*Index into codesc is 0 to codenum-1 */
	}

	if (verbose_flag)
	{
		printf("Parsed %d codes in %s\n", codenum, filename);
		printf("[%6d] 0x%8x  %s\n", 0, codesc[0].debugid, codesc[0].debug_string);
		printf("[%6d] 0x%8x %s\n\n", codenum-1, codesc[codenum-1].debugid, codesc[codenum-1].debug_string);
	}

	/* sort */
	qsort((void *)codesc, codesc_idx, sizeof(code_type_t), debugid_compar); 

	if (verbose_flag)
	{
		printf("Sorted %zd codes in %s\n", codesc_idx, filename); 
		printf("lowbound  [%6d]: 0x%8x %s\n", 0, codesc[0].debugid, codesc[0].debug_string);
		printf("highbound [%6zd]: 0x%8x %s\n\n", codesc_idx - 1, codesc[codesc_idx - 1].debugid, codesc[codesc_idx - 1].debug_string); 
	}
	codesc_find_dupes();

#if 0
	/* Dump the codefile */
	for (i = 0; i < codesc_idx; i++)
		printf("[%d]  0x%x   %s\n",i+1, codesc[i].debugid, codesc[i].debug_string);
#endif
	return(0);
}

static void codesc_find_dupes(void)
{
	boolean_t found_dupes = FALSE;
	if (codesc_idx == 0)
	{
		return;
	}
	uint32_t last_debugid = codesc[0].debugid;
	for(int i = 1; i < codesc_idx; i++)
	{
		if(codesc[i].debugid == last_debugid)
		{
			found_dupes = TRUE;
			if (verbose_flag) {
				fprintf(stderr, "WARNING: The debugid 0x%"PRIx32" (%s) has already been defined as '%s'.\n", codesc[i].debugid, codesc[i].debug_string, codesc[i - 1].debug_string);
			}
		}
		last_debugid = codesc[i].debugid;
	}
	if (found_dupes)
	{
		fprintf(stderr, "WARNING: One or more duplicate entries found in your codefiles, which will lead to unpredictable decoding behavior. Re-run with -v for more info\n");
	}
}


int match_debugid(unsigned int xx, char * debugstr, int * yy)
{
	int indx;

	if (codenum == 0)
		return(-1);

	if (codesc[codeindx_cache].debugid != xx)
		indx = binary_search(codesc, 0, (codenum-1), xx);
	else
		indx = codeindx_cache;

	if (indx == -1)
		return(indx);  /* match failed */
	else {
		bcopy(&codesc[indx].debug_string[0], debugstr,80);
		*yy = indx;
		codeindx_cache = indx;
		return(0);   /* match success */
	}
}

void
read_cpu_map(int fd)
{
	if (cpumap_header) {
		free(cpumap_header);
		cpumap_header = NULL;
		cpumap = NULL;
	}

	/*
	 * To fit in the padding space of a VERSION1 file, the max possible
	 * cpumap size is one page.
	 */
	cpumap_header = malloc(PAGE_SIZE);
	
	if (readRAW_flag) {
		/*
		 * cpu maps exist in a RAW_VERSION1+ header only
		 */
		if (raw_header.version_no == RAW_VERSION1) {
			off_t base_offset = lseek(fd, (off_t)0, SEEK_CUR);
			off_t aligned_offset = (base_offset + (4095)) & ~4095; /* <rdar://problem/13500105> */
			
			size_t padding_bytes = (size_t)(aligned_offset - base_offset);
			
			if (read(fd, cpumap_header, padding_bytes) == padding_bytes) {
				if (cpumap_header->version_no == RAW_VERSION1) {
					cpumap = (kd_cpumap*)&cpumap_header[1];
				}
			}
		}
	} else {
		int mib[3];
		
		mib[0] = CTL_KERN;
		mib[1] = KERN_KDEBUG;
		mib[2] = KERN_KDCPUMAP;
		
		size_t temp = PAGE_SIZE;
		if (sysctl(mib, 3, cpumap_header, &temp, NULL, 0) == 0) {
			if (PAGE_SIZE >= temp) {
				if (cpumap_header->version_no == RAW_VERSION1) {
					cpumap = (kd_cpumap*)&cpumap_header[1];
				}
			}
		}
	}

	if (!cpumap) {
		printf("Can't read the cpu map -- this is not fatal\n");
		free(cpumap_header);
		cpumap_header = NULL;
	} else if (verbose_flag) {
		/* Dump the initial cpumap */
		printf("\nCPU\tName\n");
		for (int i = 0; i < cpumap_header->cpu_count; i++) {
			printf ("%2d\t%s\n", cpumap[i].cpu_id, cpumap[i].name);
		}
		printf("\n");
	}
}

int
read_command_map(int fd, uint32_t count)
{
	int i;
	size_t size;
	int mib[6];

	if (readRAW_flag) {
		total_threads = count;
		size = count * sizeof(kd_threadmap);
	} else {
		get_bufinfo(&bufinfo);

		total_threads = bufinfo.nkdthreads;
		size = bufinfo.nkdthreads * sizeof(kd_threadmap);
	}
	mapptr = 0;
	nthreads = total_threads * 2;

	if (verbose_flag)
		printf("Size of map table is %d, thus %d entries\n", (int)size, total_threads);

	if (size) {
		if ((mapptr = (kd_threadmap *) malloc(size)))
			bzero (mapptr, size);
		else
		{
			if (verbose_flag)
				printf("Thread map is not initialized -- this is not fatal\n");
			return(0);
		}
	}
	if (readRAW_flag) {
		if (read(fd, mapptr, size) != size) {
			if (verbose_flag)
				printf("Can't read the thread map -- this is not fatal\n");
			free(mapptr);
			mapptr = 0;

			return (int)size;
		}
	} else {
		/* Now read the threadmap */
		mib[0] = CTL_KERN;
		mib[1] = KERN_KDEBUG;
		mib[2] = KERN_KDTHRMAP;
		mib[3] = 0;
		mib[4] = 0;
		mib[5] = 0;		/* no flags */
		if (sysctl(mib, 3, mapptr, &size, NULL, 0) < 0)
		{
			/* This is not fatal -- just means I cant map command strings */
			if (verbose_flag)
				printf("Can't read the thread map -- this is not fatal\n");
			free(mapptr);
			mapptr = 0;
			return(0);
		}
	}
	for (i = 0; i < total_threads; i++) {
		if (mapptr[i].thread)
			create_map_entry(mapptr[i].thread, &mapptr[i].command[0]);
	}

	if (verbose_flag) {
		/* Dump the initial map */
		
		printf("Size of maptable returned is %ld, thus %ld entries\n", size, (size/sizeof(kd_threadmap)));

		printf("Thread    Command\n");
		for (i = 0; i < total_threads; i++) {
			printf ("0x%lx    %s\n",
				mapptr[i].thread,
				mapptr[i].command);
		}
	}
	return (int)size;
}


void create_map_entry(uintptr_t thread, char *command)
{
	threadmap_t	tme;
	int		hashid;

	if ((tme = threadmap_freelist))
		threadmap_freelist = tme->tm_next;
	else
		tme = (threadmap_t)malloc(sizeof(struct threadmap));

	tme->tm_thread = thread;
	tme->tm_deleteme = FALSE;

	(void)strncpy (tme->tm_command, command, MAXCOMLEN);
	tme->tm_command[MAXCOMLEN] = '\0';

	hashid = thread & HASH_MASK;

	tme->tm_next = threadmap_hash[hashid];
	threadmap_hash[hashid] = tme;
}


void delete_thread_entry(uintptr_t thread)
{
	threadmap_t	tme = 0;
	threadmap_t	tme_prev;
	int		hashid;

	hashid = thread & HASH_MASK;

	if ((tme = threadmap_hash[hashid])) {
		if (tme->tm_thread == thread)
			threadmap_hash[hashid] = tme->tm_next;
		else {
			tme_prev = tme;

			for (tme = tme->tm_next; tme; tme = tme->tm_next) {
				if (tme->tm_thread == thread) {
					tme_prev->tm_next = tme->tm_next;
					break;
				}
				tme_prev = tme;
			}
		}
		if (tme) {
			tme->tm_next = threadmap_freelist;
			threadmap_freelist = tme;
		}
	}
}


void find_and_insert_tmp_map_entry(uintptr_t pthread, char *command)
{
	threadmap_t	tme = 0;
	threadmap_t	tme_prev;
	int		hashid;

	if ((tme = threadmap_temp)) {
		if (tme->tm_pthread == pthread)
			threadmap_temp = tme->tm_next;
		else {
			tme_prev = tme;

			for (tme = tme->tm_next; tme; tme = tme->tm_next) {
				if (tme->tm_pthread == pthread) {
					tme_prev->tm_next = tme->tm_next;
					break;
				}
				tme_prev = tme;
			}
		}
		if (tme) {
			(void)strncpy (tme->tm_command, command, MAXCOMLEN);
			tme->tm_command[MAXCOMLEN] = '\0';

			delete_thread_entry(tme->tm_thread);

			hashid = tme->tm_thread & HASH_MASK;

			tme->tm_next = threadmap_hash[hashid];
			threadmap_hash[hashid] = tme;
		}
	}
}


void create_tmp_map_entry(uintptr_t thread, uintptr_t pthread)
{
	threadmap_t	tme;

	if ((tme = threadmap_freelist))
		threadmap_freelist = tme->tm_next;
	else
		tme = (threadmap_t)malloc(sizeof(struct threadmap));

	tme->tm_thread = thread;
	tme->tm_pthread = pthread;
	tme->tm_deleteme = FALSE;
	tme->tm_command[0] = '\0';

	tme->tm_next = threadmap_temp;
	threadmap_temp = tme;
}


threadmap_t
find_thread_entry(uintptr_t thread)
{
	threadmap_t	tme;
	int	hashid;

	hashid = thread & HASH_MASK;

	for (tme = threadmap_hash[hashid]; tme; tme = tme->tm_next) {
		if (tme->tm_thread == thread)
			return (tme);
	}
	return (0);
}


void find_thread_name(uintptr_t thread, char **command, boolean_t deleteme)
{
	threadmap_t	tme;

	if ((tme = find_thread_entry(thread))) {
		*command = tme->tm_command;

		if (deleteme == TRUE)
			tme->tm_deleteme = deleteme;
	} else
		*command = EMPTYSTRING;
}


void find_thread_command(kd_buf *kbufp, char **command)
{
	uintptr_t	thread;
	threadmap_t	tme;
	int		debugid_base;

	thread = kbufp->arg5;
	debugid_base = kbufp->debugid & DBG_FUNC_MASK;

	if (debugid_base == BSC_exit || debugid_base == MACH_STKHANDOFF) {
		/*
		 * Mark entry as invalid and return temp command pointer
		 */
		if ((tme = find_thread_entry(thread))) {

			strncpy(tmpcommand, tme->tm_command, MAXCOMLEN);
			*command = tmpcommand;

			if (debugid_base == BSC_exit || tme->tm_deleteme == TRUE)
				delete_thread_entry(thread);
		} else
			*command = EMPTYSTRING;
	}
	else if (debugid_base == TRACE_DATA_NEWTHREAD) {
		/*
		 * Save the create thread data
		 */
		create_tmp_map_entry(kbufp->arg1, kbufp->arg5);
	}
	else if (debugid_base == TRACE_STRING_NEWTHREAD) {
		/*
		 * process new map entry
		 */
		find_and_insert_tmp_map_entry(kbufp->arg5, (char *)&kbufp->arg1);
	}
	else if (debugid_base == TRACE_STRING_EXEC) {

		delete_thread_entry(kbufp->arg5);

		create_map_entry(kbufp->arg5, (char *)&kbufp->arg1);
	}
	else
		find_thread_name(thread, command, (debugid_base == BSC_thread_terminate));
}


static
void getdivisor()
{
	mach_timebase_info_data_t info;

	if (frequency == 0) {
		(void) mach_timebase_info (&info);

		divisor = ( (double)info.denom / (double)info.numer) * 1000;
	} else
		divisor = (double)frequency / 1000000;

	if (verbose_flag)
		printf("divisor = %g\n", divisor);
}
