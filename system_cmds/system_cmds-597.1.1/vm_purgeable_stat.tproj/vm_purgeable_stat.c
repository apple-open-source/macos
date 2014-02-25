#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/task.h>
#include <libproc.h>
#include <mach/vm_purgable.h>

#define USAGE "Usage: vm_purgeable_stat [-a | -p <pid> | -s <interval>]\n"
#define PRIV_ERR_MSG "The option specified needs root priveleges."
#define PROC_NAME_LEN 256
#define KB 1024
#define PURGEABLE_PRIO_LEVELS VM_VOLATILE_GROUP_SHIFT

static inline int purge_info_size_adjust(uint64_t size);
static inline char purge_info_unit(uint64_t size);
void print_header(int summary_view);
int get_system_tasks(task_array_t *tasks, mach_msg_type_number_t *count);
int get_task_from_pid(int pid, task_t *task);
void print_purge_info_task(task_t task, int pid);
void print_purge_info_task_array(task_array_t tasks, mach_msg_type_number_t count);
void print_purge_info_summary(int sleep_duration);

static inline int purge_info_size_adjust(uint64_t size)
{
	while(size > KB)
		size /= KB;
	return (int)size;
}

static inline char purge_info_unit(uint64_t size)
{
	char sizes[] = {'B', 'K', 'M', 'G', 'T'};
	int index = 0;

	while(size > KB) {
		index++;
		size /= KB;
	}
	return sizes[index];
}

void print_header(int summary_view)
{
	if (!summary_view)
		printf("%20s ", "Process-Name");

	printf("%9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s %9s\n",
		"FIFO-P0", "FIFO-P1", "FIFO-P2", "FIFO-P3",
		"FIFO-P4", "FIFO-P5", "FIFO-P6", "FIFO-P7",
		"OBSOLETE",
		"LIFO-P0", "LIFO-P1", "LIFO-P2", "LIFO-P3",
		"LIFO-P4", "LIFO-P5", "LIFO-P6", "LIFO-P7"
	);
}

int get_task_from_pid(int pid, task_t *task)
{
	kern_return_t kr;
	if (geteuid() != 0) {
		fprintf(stderr, "%s\n", PRIV_ERR_MSG);
		return -1;
	}
	kr = task_for_pid(mach_task_self(), pid, task);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "Failed to get task port for pid: %d\n", pid);
		return -1;
	}
	return 0;
}

int get_system_tasks(task_array_t *tasks, mach_msg_type_number_t *count)
{
	processor_set_name_array_t psets;
	mach_msg_type_number_t psetCount;
	mach_port_t pset_priv;
	kern_return_t ret;

	if (geteuid() != 0) {
		fprintf(stderr, "%s\n", PRIV_ERR_MSG);
		return -1;
	}
	
	ret = host_processor_sets(mach_host_self(), &psets, &psetCount);
	if (ret != KERN_SUCCESS) {
		fprintf(stderr, "host_processor_sets() failed: %s\n", mach_error_string(ret));
		return -1;
	}
	if (psetCount != 1) {
		fprintf(stderr, "Assertion Failure: pset count greater than one (%d)\n", psetCount);
		return -1;
	}

	/* convert the processor-set-name port to a privileged port */
	ret = host_processor_set_priv(mach_host_self(), psets[0], &pset_priv);
	if (ret != KERN_SUCCESS) {
		fprintf(stderr, "host_processor_set_priv() failed: %s\n", mach_error_string(ret));
		return -1;
	}
	mach_port_deallocate(mach_task_self(), psets[0]);
	vm_deallocate(mach_task_self(), (vm_address_t)psets, (vm_size_t)psetCount * sizeof(mach_port_t));

	/* convert the processor-set-priv to a list of tasks for the processor set */
	ret = processor_set_tasks(pset_priv, tasks, count);
	if (ret != KERN_SUCCESS) {
		fprintf(stderr, "processor_set_tasks() failed: %s\n", mach_error_string(ret));
		return -1;
	}
	mach_port_deallocate(mach_task_self(), pset_priv);
	return 0;
}

void print_purge_info_task(task_t task, int pid)
{
	task_purgable_info_t info;
	kern_return_t kr;
	int i;
	char pname[PROC_NAME_LEN];

	kr = task_purgable_info(task, &info);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "(pid: %d) task_purgable_info() failed: %s\n", pid, mach_error_string(kr));
		return;
	}
	if (0 == proc_name(pid, pname, PROC_NAME_LEN))
		strncpy(pname, "Unknown", 7);
	pname[20] = 0;
	printf("%20s ", pname);
	for (i=0; i<PURGEABLE_PRIO_LEVELS; i++)
		printf("%4u/%3d%c ", (unsigned)info.fifo_data[i].count, purge_info_size_adjust(info.fifo_data[i].size), purge_info_unit(info.fifo_data[i].size));
        printf("%4u/%3d%c ", (unsigned)info.obsolete_data.count, purge_info_size_adjust(info.obsolete_data.size), purge_info_unit(info.obsolete_data.size));
	for (i=0; i<PURGEABLE_PRIO_LEVELS; i++)
		printf("%4u/%3d%c ", (unsigned)info.lifo_data[i].count, purge_info_size_adjust(info.lifo_data[i].size), purge_info_unit(info.lifo_data[i].size));
	printf("\n");
	return;
}

void print_purge_info_task_array(task_array_t tasks, mach_msg_type_number_t count)
{
	int i;
	int pid;

	for (i=0; i<count; i++) {
		if (KERN_SUCCESS != pid_for_task(tasks[i], &pid))
			continue;
		print_purge_info_task(tasks[i], pid);
	}
	return;
}

void print_purge_info_summary(int sleep_duration)
{
	host_purgable_info_data_t       info;
        mach_msg_type_number_t          count;
        kern_return_t                   result;
        int                             i;

	while(1) {
		count = HOST_VM_PURGABLE_COUNT;
		result = host_info(mach_host_self(), HOST_VM_PURGABLE, (host_info_t)&info, &count);
		if (result != KERN_SUCCESS)
			break;
		for (i=0; i<PURGEABLE_PRIO_LEVELS; i++)
			printf("%4u/%3d%c ", (unsigned)info.fifo_data[i].count, purge_info_size_adjust(info.fifo_data[i].size), purge_info_unit(info.fifo_data[i].size));
		printf("%4u/%3d%c ", (unsigned)info.obsolete_data.count, purge_info_size_adjust(info.obsolete_data.size), purge_info_unit(info.obsolete_data.size));
		for (i=0; i<PURGEABLE_PRIO_LEVELS; i++)
			printf("%4u/%3d%c ", (unsigned)info.lifo_data[i].count, purge_info_size_adjust(info.lifo_data[i].size), purge_info_unit(info.lifo_data[i].size));
		printf("\n");
		sleep(sleep_duration);
	}
        return;
}

int main(int argc, char *argv[])
{
	
	char ch;
	int pid;
	int sleep_duration;
	task_array_t tasks;
	task_t task;
	mach_msg_type_number_t taskCount;
	int noargs = 1;

	while(1) {
		ch = getopt(argc, argv, "ahp:s:");
		if (ch == -1)
			break;
		noargs = 0;
		switch(ch) {
			case 'a':
				if (get_system_tasks(&tasks, &taskCount) < 0)
					break;
				print_header(0);
				print_purge_info_task_array(tasks, taskCount);
				break;

			case 'p':
				pid = (int)strtol(optarg, NULL, 10);
				if (pid < 0)
					break;
				if (get_task_from_pid(pid, &task) < 0)
					break;
				print_header(0);
				print_purge_info_task(task, pid);
				break;
			case 's':
				sleep_duration = (int)strtol(optarg, NULL, 10);
				if (sleep_duration < 0)
					break;
				print_header(1);
				print_purge_info_summary(sleep_duration);
				break;
			case '?':
			case 'h':
			default:
				printf("%s", USAGE);		
		}
		break;
	}
	if (noargs)
		printf("%s", USAGE);
	return 0;
}


