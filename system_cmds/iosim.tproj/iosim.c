#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <libkern/OSAtomic.h>
#include <limits.h>
#include <errno.h>

#define IO_MODE_SEQ 		0
#define IO_MODE_RANDOM 		1

#define WORKLOAD_TYPE_RO 	0
#define WORKLOAD_TYPE_WO 	1
#define WORKLOAD_TYPE_RW 	2

#define MAX_THREADS 		1000
#define MAX_FILENAME 		64
#define MAX_ITERATIONS 		10000
#define LATENCY_BIN_SIZE 	500
#define LATENCY_BINS 		11 	
#define LOW_LATENCY_BIN_SIZE 	50
#define LOW_LATENCY_BINS 	11
#define THROUGHPUT_INTERVAL	5000
#define DEFAULT_FILE_SIZE 	(262144)
#define BLOCKSIZE 		1024
#define MAX_CMD_SIZE 		256
#define PG_MASK 		~(0xFFF)

int burst_count = 10;                	/* Unit: Number ; Desc.: I/O Burst Count */
int inter_burst_duration = 0; 	 	/* Unit: msecs  ; Desc.: I/O Inter-Burst Duration (-1: Random value [0,100]) */
int inter_io_delay_ms = 0; 		/* Unit: msecs  ; Desc.: Inter I/O Delay */
int thread_count = 1;                	/* Unit: Number ; Desc.: Thread Count */
int workload_type = WORKLOAD_TYPE_RO;	/* Unit: 0/1/2  ; Desc.: Workload Type */
int io_size = 4096;  	                /* Unit: Bytes  ; Desc.: I/O Unit Size */
int sync_frequency_ms = 0; 		/* Unit: msecs  ; Desc.: Sync thread frequency (0: Indicates no sync) */
int io_mode = 0;                     	/* Unit: 0/1	; Desc.: I/O Mode (Seq./Rand.) */
int test_duration = 0;                  /* Unit: secs   ; Desc.: Total Test Duration (0 indicates wait for Ctrl+C signal) */
int io_tier = 0; 			/* Unit: 0/1/2/3; Desc.: I/O Tier */
int file_size = DEFAULT_FILE_SIZE; 	/* Unit: pages  ; Desc.: File Size in 4096 byte blocks */
int cached_io_flag = 0; 		/* Unit: 0/1 	; Desc.: I/O Caching behavior (no-cached/cached) */
char *user_fname; 
int user_specified_file = 0;

int64_t total_io_count;
int64_t total_io_size;
int64_t total_io_time;
int64_t total_burst_count;
int64_t latency_histogram[LATENCY_BINS];
int64_t burst_latency_histogram[LATENCY_BINS];
int64_t low_latency_histogram[LOW_LATENCY_BINS];
int64_t throughput_histogram[MAX_ITERATIONS];
int64_t throughput_index;

void print_usage(void);
void print_data_percentage(int percent);
void print_stats(void);
unsigned int find_io_bin(int64_t latency, int latency_bin_size, int latency_bins);
void signalHandler(int sig);
void perform_io(int fd, char *buf, int size, int type);
void *sync_routine(void *arg);
void *calculate_throughput(void *arg);
void *io_routine(void *arg);
void validate_option(int value, int min, int max, char *option, char *units);
void print_test_setup(int value, char *option, char *units, char *comment);
void setup_process_io_policy(int io_tier);
void print_latency_histogram(int64_t *data, int latency_bins, int latency_bin_size);

void print_usage()
{
	printf("Usage: ./iosim [options]\n");
	printf("Options:\n");
	printf("-c: (number)  Burst Count. No. of I/Os performed in an I/O burst\n");
	printf("-i: (msecs)   Inter Burst Duration. Amount of time the thread sleeps between bursts (-1 indicates random durations between 0-100 msecs)\n");
	printf("-d: (msecs)   Inter I/O delay. Amount of time between issuing I/Os\n");
	printf("-t: (number)  Thread count\n");
	printf("-f: (0/1/2 :  Read-Only/Write-Only/Mixed RW) Workload Type\n");
	printf("-m: (0/1   :  Sequential/Random) I/O pattern\n");
	printf("-j: (number)  Size of I/O in bytes\n");
	printf("-s: (msecs)   Frequency of sync() calls\n");
	printf("-x: (secs)    Test duration (0 indicates that the tool would wait for a Ctrl-C)\n");
	printf("-l: (0/1/2/3) I/O Tier\n");
	printf("-z: (number)  File Size in pages (1 page = 4096 bytes) \n");
	printf("-n: (string)  File name used for tests (the tool would create files if this option is not specified)\n");
	printf("-a: (0/1   :  Non-cached/Cached) I/O Caching behavior\n");
}

void print_data_percentage(int percent)
{
	int count = (int)(round(percent / 5.0));
	int spaces = 20 - count;
	printf("| ");
	for(; count > 0; count--)
		printf("*");
	for(; spaces > 0; spaces--)
		printf(" ");
	printf("|");
}

void print_latency_histogram(int64_t *data, int latency_bins, int latency_bin_size)
{
	double percentage;
        char label[MAX_FILENAME];
	int i;

        for (i = 0; i < latency_bins; i++) {
                if (i == (latency_bins - 1))
                        snprintf(label, MAX_FILENAME, "> %d usecs", i * latency_bin_size);
                else
                        snprintf(label, MAX_FILENAME, "%d - %d usecs", i * latency_bin_size, (i+1) * latency_bin_size);
                printf("%25s ", label);
                percentage = ((double)data[i] * 100.0) / (double)total_io_count;
                print_data_percentage((int)percentage);
                printf(" %.2lf%%\n", percentage);
        }
	printf("\n");
}

void print_stats()
{
	int i;
	double percentage;
        char label[MAX_FILENAME];

	printf("I/O Statistics:\n");

	printf("Total I/Os      : %lld\n", total_io_count);
	printf("Avg. Latency    : %.2lf usecs\n", ((double)total_io_time) / ((double)total_io_count));	

	printf("Low Latency Histogram: \n");
	print_latency_histogram(low_latency_histogram, LOW_LATENCY_BINS, LOW_LATENCY_BIN_SIZE);
	printf("Latency Histogram: \n");
	print_latency_histogram(latency_histogram, LATENCY_BINS, LATENCY_BIN_SIZE);
	printf("Burst Avg. Latency Histogram: \n");
	print_latency_histogram(burst_latency_histogram, LATENCY_BINS, LATENCY_BIN_SIZE);
	
	printf("Throughput Timeline: \n");

	int64_t max_throughput = 0;
	for (i = 0; i < throughput_index; i++) {
		if (max_throughput < throughput_histogram[i])
			max_throughput = throughput_histogram[i];
	}

	for (i = 0; i < throughput_index; i++) {
		snprintf(label, MAX_FILENAME, "T=%d msecs", (i+1) * THROUGHPUT_INTERVAL);
		printf("%25s ", label);
		percentage = ((double)throughput_histogram[i] * 100) / (double)max_throughput;
		print_data_percentage((int)percentage);
		printf("%.2lf MBps\n", ((double)throughput_histogram[i] / 1048576.0) / ((double)THROUGHPUT_INTERVAL / 1000.0));
	}
	printf("\n");
		
}

unsigned int find_io_bin(int64_t latency, int latency_bin_size, int latency_bins)
{
	int bin = (int) (latency / latency_bin_size);
	if (bin >= latency_bins)
		bin = latency_bins - 1;
	return bin;
}

void signalHandler(int sig)
{
	printf("\n");
	print_stats();
	exit(0);
}


void perform_io(int fd, char *buf, int size, int type)
{
	long ret;

	if (type == WORKLOAD_TYPE_RW)
		type = (rand() % 2) ? WORKLOAD_TYPE_WO : WORKLOAD_TYPE_RO;

	while(size > 0) {

		if (type == WORKLOAD_TYPE_RO)
			ret = read(fd, buf, size);
		else
			ret = write(fd, buf, size);
	
		if (ret == 0) {
			if (lseek(fd, 0, SEEK_SET) < 0) {
				perror("lseek() to reset file offset to zero failed!\n");
				goto error;
			}
		}
		
		if (ret < 0) {
			perror("read/write syscall failed!\n");
			goto error;
		}
		size -= ret;
	}

	return;

error:
	print_stats();
	exit(1);
}

void *sync_routine(void *arg)
{
	while(1) {	
		usleep(sync_frequency_ms * 1000);
		sync();
	}
	pthread_exit(NULL);
}

void *calculate_throughput(void *arg)
{
	int64_t prev_total_io_size = 0;
	int64_t size;

	while(1) {
		usleep(THROUGHPUT_INTERVAL * 1000);
		size = total_io_size - prev_total_io_size;
		throughput_histogram[throughput_index] = size;
		prev_total_io_size = total_io_size;
		throughput_index++;	
	}
	pthread_exit(NULL);
}	

void *io_routine(void *arg)
{
	struct timeval start_tv;
	struct timeval end_tv;
	int64_t elapsed;
	int64_t burst_elapsed;
	char *data;
	char test_filename[MAX_FILENAME];
	struct stat filestat;
	int i, fd, io_thread_id;

	io_thread_id = (int)arg;
	if (user_specified_file)
		strncpy(test_filename, user_fname, MAX_FILENAME);
	else
		snprintf(test_filename, MAX_FILENAME, "iosim-%d-%d", (int)getpid(), io_thread_id);

	if (0 > (fd = open(test_filename, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
		printf("Error opening file %s!\n", test_filename);
		exit(1);
	}

	if (fstat(fd, &filestat) < 0) {
		printf("Error stat()ing file %s!\n", test_filename);
		exit(1);
	}	

	if (filestat.st_size < io_size) {
		printf("%s: File size (%lld) smaller than I/O size (%d)!\n", test_filename, filestat.st_size, io_size);
		exit(1);
	}

	if (!cached_io_flag)
		fcntl(fd, F_NOCACHE, 1);

	fcntl(fd, F_RDAHEAD, 0);
	
	if(!(data = (char *)calloc(io_size, 1))) {
		perror("Error allocating buffers for I/O!\n");
		exit(1);
	}
	memset(data, '\0', io_size);

	while(1) {
		
		burst_elapsed = 0;

		for(i = 0; i < burst_count; i++) {
			if (io_mode == IO_MODE_RANDOM) {
				if (lseek(fd, (rand() % (filestat.st_size - io_size)) & PG_MASK, SEEK_SET) < 0) {
					perror("Error lseek()ing to random location in file!\n");
					exit(1);
				}
			}
				

			gettimeofday(&start_tv, NULL);
			perform_io(fd, data, io_size, workload_type);
			gettimeofday(&end_tv, NULL);

			OSAtomicIncrement64(&total_io_count);
			OSAtomicAdd64(io_size, &total_io_size);
			elapsed = ((end_tv.tv_sec - start_tv.tv_sec) * 1000000)  + (end_tv.tv_usec - start_tv.tv_usec);
			OSAtomicAdd64(elapsed, &total_io_time);
			OSAtomicIncrement64(&(latency_histogram[find_io_bin(elapsed, LATENCY_BIN_SIZE, LATENCY_BINS)]));
			OSAtomicIncrement64(&(low_latency_histogram[find_io_bin(elapsed, LOW_LATENCY_BIN_SIZE, LOW_LATENCY_BINS)]));
			burst_elapsed += elapsed;
			
			if (inter_io_delay_ms)
				usleep(inter_io_delay_ms * 1000);
		}

		burst_elapsed /= burst_count;
		OSAtomicIncrement64(&(burst_latency_histogram[find_io_bin(burst_elapsed, LATENCY_BIN_SIZE, LATENCY_BINS)]));
		OSAtomicIncrement64(&total_burst_count);

		if(inter_burst_duration == -1)
			usleep((rand() % 100) * 1000);
		else
			usleep(inter_burst_duration * 1000);
	}

	free(data);
	close(fd);
	pthread_exit(NULL);
}

void validate_option(int value, int min, int max, char *option, char *units)
{
	if (value < min || value > max) {
		printf("Illegal option value %d for %s (Min value: %d %s, Max value: %d %s).\n", value, option, min, units, max, units);
		exit(1);
	}
}

void print_test_setup(int value, char *option, char *units, char *comment)
{
	if (comment == NULL)
		printf("%32s: %16d %-16s\n", option, value, units);
	else
		printf("%32s: %16d %-16s (%s)\n", option, value, units, comment);
}

void setup_process_io_policy(int io_tier)
{
	switch(io_tier)
	{
		case 0:
			if (setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_IMPORTANT))
				goto iopol_error;
			break;
		case 1:
			if (setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_STANDARD))
                                goto iopol_error;
                        break;
		case 2:
			if (setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_UTILITY))
                                goto iopol_error;
                        break;
		case 3:
			if (setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE))
                                goto iopol_error;
                        break;
	}
	return;

iopol_error:
	printf("Error setting process-wide I/O policy to %d\n", io_tier);
        exit(1);
}

int main(int argc, char *argv[])
{
	int i, option = 0;
	pthread_t thread_list[MAX_THREADS];
	pthread_t sync_thread;
	pthread_t throughput_thread;
	char fname[MAX_FILENAME];

	while((option = getopt(argc, argv,"hc:i:d:t:f:m:j:s:x:l:z:n:a:")) != -1) {
		switch(option) {
			case 'c':
				burst_count = atoi(optarg);
				validate_option(burst_count, 0, INT_MAX, "Burst Count", "I/Os");
				break;
			case 'i':
				inter_burst_duration = atoi(optarg);
				validate_option(inter_burst_duration, -1, INT_MAX, "Inter Burst duration", "msecs");
				break;
			case 'd':
				inter_io_delay_ms = atoi(optarg);
				validate_option(inter_io_delay_ms, 0, INT_MAX, "Inter I/O Delay", "msecs");
				break;
			case 't':
				thread_count = atoi(optarg);
				validate_option(thread_count, 0, MAX_THREADS, "Thread Count", "Threads");
				break;
			case 'f':
				workload_type = atoi(optarg);
				validate_option(workload_type, 0, 2, "Workload Type", "");
				break;
			case 'm':
				io_mode = atoi(optarg);
				validate_option(io_mode, 0, 1, "I/O Mode", "");
				break;
			case 'j':
				io_size = atoi(optarg);
				validate_option(io_size, 0, INT_MAX, "I/O Size", "Bytes");
				break;
			case 'h':
				print_usage();
				exit(1);
			case 's':
				sync_frequency_ms = atoi(optarg);
				validate_option(sync_frequency_ms, 0, INT_MAX, "Sync. Frequency", "msecs");
				break;
			case 'x':
				test_duration = atoi(optarg);
				validate_option(test_duration, 0, INT_MAX, "Test duration", "secs");
				break;
			case 'l':
				io_tier = atoi(optarg);
				validate_option(io_tier, 0, 3, "I/O Tier", "");
				break;
			case 'z':
				file_size = atoi(optarg);
				validate_option(file_size, 0, INT_MAX, "File Size", "bytes");
				break; 
			case 'n':
				user_fname = optarg;
				user_specified_file = 1;
				break;
			case 'a':
				cached_io_flag = atoi(optarg);
				validate_option(cached_io_flag, 0, 1, "I/Os cached/no-cached", "");
				break;
			default:
				printf("Unknown option %c\n", option);
				print_usage();
				exit(1);
		}
	}

	printf("***********************TEST SETUP*************************\n");

	print_test_setup(burst_count, "Burst Count", "I/Os", 0);
	print_test_setup(inter_burst_duration, "Inter Burst duration", "msecs", "-1 indicates random burst duration");
	print_test_setup(inter_io_delay_ms, "Inter I/O Delay", "msecs", 0);
	print_test_setup(thread_count, "Thread Count", "Threads", 0);
	print_test_setup(workload_type, "Workload Type", "", "0:R 1:W 2:RW");
	print_test_setup(io_mode, "I/O Mode", "", "0:Seq. 1:Rnd");
	print_test_setup(io_size, "I/O Size", "Bytes", 0);
	print_test_setup(sync_frequency_ms, "Sync. Frequency", "msecs", "0 indicates no sync. thread");
	print_test_setup(test_duration, "Test duration", "secs", "0 indicates tool waits for Ctrl+C");
	print_test_setup(io_tier, "I/O Tier", "", 0);
	print_test_setup(cached_io_flag, "I/O Caching", "", "0 indicates non-cached I/Os");
	print_test_setup(0, "File read-aheads", "", "0 indicates read-aheads disabled");
	
	printf("**********************************************************\n");

	if (user_specified_file == 0) {
		char dd_command[MAX_CMD_SIZE];
		for (i=0; i < thread_count; i++) {
			snprintf(fname, MAX_FILENAME, "iosim-%d-%d", (int)getpid(), i);
			snprintf(dd_command, MAX_CMD_SIZE, "dd if=/dev/urandom of=%s bs=4096 count=%d", fname, file_size);  
			printf("Creating file %s of size %lld...\n", fname, ((int64_t)file_size * 4096));
			system(dd_command);
		}
	} else {
		printf("Using user specified file %s for all threads...\n", user_fname);
	}
	system("purge");
	setup_process_io_policy(io_tier);

	printf("**********************************************************\n");		
	printf("Creating threads and generating workload...\n");	

	signal(SIGINT, signalHandler);
	signal(SIGALRM, signalHandler);

	for(i=0; i < thread_count; i++) {
		if (pthread_create(&thread_list[i], NULL, io_routine, i) < 0) {
			perror("Could not create I/O thread!\n");
			exit(1);
		}
	}

	if (sync_frequency_ms) {
		if (pthread_create(&sync_thread, NULL, sync_routine, NULL) < 0) {
			perror("Could not create sync thread!\n");
			exit(1);
		}
	}

	if (pthread_create(&throughput_thread, NULL, calculate_throughput, NULL) < 0) {
		perror("Could not throughput calculation thread!\n");
		exit(1);
	}

	/* All threads are now initialized */
	if (test_duration)
		alarm(test_duration);	

	for(i=0; i < thread_count; i++)
		pthread_join(thread_list[i], NULL);
	
	if (sync_frequency_ms)
		pthread_join(sync_thread, NULL);

	pthread_join(throughput_thread, NULL);

	pthread_exit(0);

}
