#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <Kernel/kern/ledger.h>
#include <mach/mach_types.h>

extern int ledger(int cmd, caddr_t arg1, caddr_t arg2, caddr_t arg3);

int pid = -1;
char *group_print = NULL;
char *resource_print = NULL;

struct proc_list {
	int pid;
	int seen;
	char command[32];
	struct ledger *ledger;
	struct proc_list *next;
};

struct proc_list *procs = NULL;
struct ledger_template_info *template = NULL;
int entry_cnt = 0;

struct ledger {
	int64_t id;
	int seen;
	int64_t entries;
	struct ledger_entry_info *info;
	struct ledger_entry_info *old_info;
	struct ledger *next;
};

struct ledger *ledgers = NULL;

static void
get_template_info()
{

	void *buf;
	int cnt;

top:
	/* Allocate enough space to accomodate a few new entries */
	cnt = entry_cnt + 5;
	buf = malloc(cnt * sizeof (struct ledger_template_info));
	if (buf == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit (1);
	}

	if (ledger(LEDGER_TEMPLATE_INFO, (caddr_t)buf, (caddr_t)&cnt, NULL) < 0) {
        perror("ledger() system call failed");
		exit(1);
	}

	/* We underestimated how many entries we needed.  Let's try again */
	if (cnt == entry_cnt + 5) {
		entry_cnt += 5;
		free(buf);
		goto top;
	}
	entry_cnt = cnt;
	template = buf;
}

/*
 * Note - this is a destructive operation.  Unless we're about to exit, this
 * needs to be followed by another call to get_template_info().
 */
static void
dump_template_info()
{
	int i, j;
	const char *group = NULL;
	
	printf("Resources being tracked:\n");
	printf("\t%10s  %15s  %8s\n", "GROUP", "RESOURCE", "UNITS");
	for (i = 0; i < entry_cnt; i++) {
		if (strlen(template[i].lti_name) == 0)
			continue;
		
		group = template[i].lti_group;
		for (j = i; j < entry_cnt; j++) {
			if (strcmp(template[j].lti_group, group))
				continue;
			printf("\t%10s  %15s  %8s\n", template[j].lti_group,
			    template[j].lti_name, template[j].lti_units);
			template[j].lti_name[0] = '\0';
		}
	}
}

static void
validate_group()
{
	int i;

	if (template == NULL)
		get_template_info();

	for (i = 0; i < entry_cnt; i++)
		if (!strcmp(group_print, template[i].lti_group))
			return;

	fprintf(stderr, "No such group: %s\n", group_print);
	exit (1);
}

static void
validate_resource()
{
	int i;

	if (template == NULL)
		get_template_info();

	for (i = 0; i < entry_cnt; i++)
		if (!strcmp(resource_print, template[i].lti_name))
			return;

	fprintf(stderr, "No such resource: %s\n", resource_print);
	exit (1);
}

static size_t
get_kern_max_proc(void)
{
	int mib[] = { CTL_KERN, KERN_MAXPROC };
	int max;
	size_t max_sz = sizeof (max);

	if (sysctl(mib, 2, &max, &max_sz, NULL, 0) < 0) {
		perror("Failed to get max proc count");
		exit (1);
	}

	return (max);
}

static int
get_proc_kinfo(pid_t pid, struct kinfo_proc *kinfo)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
	size_t len;

	len = sizeof(struct kinfo_proc);
	return (sysctl(mib, 4, kinfo, &len, NULL, 0) < 0);
}

static struct ledger *
ledger_find(struct ledger_info *li)
{
	struct ledger *l;

	for (l = ledgers; l && (li->li_id != l->id); l = l->next)
		;

	if (l == NULL) {
		l = (struct ledger *)malloc(sizeof (*l));
		if (l == NULL) {
			fprintf(stderr, "Out of memory");
			exit (1);
		}
		l->id = li->li_id;
		l->entries = li->li_entries;
		l->next = ledgers;
		l->seen = 0;
		l->info = NULL;
		l->old_info = NULL;
		ledgers = l;
	}		
	return (l);

}

static void
ledger_update(pid_t pid, struct ledger *l)
{
	void *arg;
	struct ledger_entry_info *lei;
	int64_t cnt;

	cnt = l->entries;
	if (cnt > entry_cnt)
		cnt = entry_cnt;
	arg = (void *)(long)pid;
    lei = (struct ledger_entry_info *)malloc((size_t)(cnt * sizeof (*lei)));
	if (ledger(LEDGER_ENTRY_INFO, arg, (caddr_t)lei, (caddr_t)&cnt) < 0) {
    	perror("ledger_info() failed: ");
		exit (1);
	}
	l->info = lei;
}

static void
get_proc_info(int pid)
{
	struct ledger_info li;
	struct ledger *ledgerp;
	struct proc_list *proc;
	struct kinfo_proc kinfo;
	void *arg;

	if (pid == 0)
		return;

	arg = (void *)(long)pid;
    errno = 0;
    if (ledger(LEDGER_INFO, arg, (caddr_t)&li, NULL) < 0) {

		if (errno == ENOENT || errno == ESRCH)
			return;

		perror("ledger_info() failed: ");
		exit (1);
	}

	ledgerp = ledger_find(&li);
	ledger_update(pid, ledgerp);
	ledgerp->seen = 1;
	
	for (proc = procs; proc; proc = proc->next)
		if (proc->pid == pid)
			break;
	if (proc == NULL) {
		proc = (struct proc_list *)malloc(sizeof (*proc));
		if (proc == NULL) {
			fprintf(stderr, "Out of memory\n");
			exit (1);
		}

		if (get_proc_kinfo(pid, &kinfo))
			strlcpy(proc->command, "Error", sizeof (proc->command));
		else
			strlcpy(proc->command, kinfo.kp_proc.p_comm,
			    sizeof (proc->command));

		proc->pid = pid;
		proc->ledger = ledgerp;
		proc->next = procs;
		procs = proc;
	}
	proc->seen = 1;
}

static int
pid_compare(const void *a, const void *b)
{
	pid_t *pid_a = (pid_t *)a;
	pid_t *pid_b = (pid_t *)b;

	return (*pid_b - *pid_a);
}

static void
get_all_info()
{
	pid_t *pids;
	int sz, cnt, i;

	if (pid < 0)
		cnt = (int) get_kern_max_proc();
	else
		cnt = 1;

	sz = cnt * sizeof(pid_t);
	pids = (pid_t *)malloc(sz);
	if (pids == NULL) {
		perror("can't allocate memory for proc buffer\n");
		exit (1);
	}

	if (pid < 0) {
		cnt = proc_listallpids(pids, sz);
		if (cnt < 0) {
			perror("failed to get list of active pids");
			exit (1);
		}
		qsort(pids, cnt, sizeof (pid_t), pid_compare);
	} else {
		pids[0] = pid;
	}

	for (i = 0; i < cnt; i++)
		get_proc_info(pids[i]);
	free(pids);
}

static void
print_num(int64_t num, int64_t delta)
{
	char suf = ' ';
	char posneg = ' ';

	if (num == LEDGER_LIMIT_INFINITY) {
		printf("%10s ", "-  ");
		return;
	}

	if (llabs(num) > 10000000000) {
		num /= 1000000000;
		suf = 'G';
	} else if (llabs(num) > 10000000) {
		num /= 1000000;
		suf = 'M';
	} else if (llabs(num) > 100000) {
		num /= 1000;
		suf = 'K';
	}
	posneg = (delta < 0) ? '-' : ((delta > 0) ? '+' : ' ');

	if (suf == ' ') {
		suf = posneg;
		posneg = ' ';
	}
	printf("%8lld%c%c ", num, suf, posneg);
}

static void
dump_all_info()
{
	struct ledger_entry_info *info, *old;
	struct proc_list *p;
	int line, i;
	int64_t d;

	printf("\n%5s %10s %15s %10s %10s %10s %10s %10s\n", "PID", "COMMAND",
	    "RESOURCE", "CREDITS", "DEBITS", "BALANCE", "LIMIT", "PERIOD");

	for (p = procs; p; p = p->next) {
		if (p->seen == 0)
			continue;
		
		printf("%5d %10.10s ", p->pid, p->command);
		line = 0;

		info = p->ledger->info;
		old = p->ledger->old_info;
		for (i = 0; i < p->ledger->entries; i++) {
			if (group_print &&
			    strcmp(group_print, template[i].lti_group))
				continue;

			if (resource_print &&
			    strcmp(resource_print, template[i].lti_name))
				continue;

			if (line++)
				printf("                 ");
			printf("%15s ", template[i].lti_name);
			
			d = old ? info[i].lei_credit - old[i].lei_credit : 0;
			print_num(info[i].lei_credit, d);

			d = old ? info[i].lei_debit - old[i].lei_debit : 0;
			print_num(info[i].lei_debit, d);

			d = old ? info[i].lei_balance - old[i].lei_balance : 0;
			print_num(info[i].lei_balance, d);

			if (info[i].lei_limit == LEDGER_LIMIT_INFINITY) {
				printf("%10s %10s", "none", "-  ");
			} else {
				print_num(info[i].lei_limit, 0);
				print_num(info[i].lei_refill_period, 0);
			}
			printf("\n");
		}
	}
	
	if (line == 0) 
		exit (0);
}

static void
cleanup()
{	
	struct proc_list *p, *pnext, *plast;
	struct ledger *l, *lnext, *llast;

	plast = NULL;
	for (p = procs; p; p = pnext) {
		pnext = p->next;
		if (p->seen == 0) {
			if (plast)
				plast->next = pnext;
			else
				procs = pnext;
			
			free(p);
		} else {
			p->seen = 0;
		}
	}

	llast = NULL;
	for (l = ledgers; l; l = lnext) {
		lnext = l->next;
		if (l->seen == 0) {
			if (llast)
				llast->next = lnext;
			else
				ledgers = lnext;
			free(l->info);
			if (l->old_info)
				free(l->old_info);
			free(l);	
		} else {
			l->seen = 0;
			free(l->old_info);
			l->old_info = l->info;
			l->info = NULL;
		}
	}

	free(template);
	template = NULL;
}

static void
usage()
{
	printf("lprint [-hL] [-g group] [-p pid] [-r resource] [interval]\n");
}

int
main(int argc, char **argv)
{
	int c;
	int interval = 0;
    
	while ((c = getopt(argc, argv, "g:hLp:r:")) != -1) {
		switch (c) {
		case 'g':
			group_print = optarg;
			break;

		case 'h':
			usage();
			exit(0);
	
		case 'L':
			get_template_info();
			dump_template_info();
			exit(0);
			
		case 'p':
			pid = atoi(optarg);
			break;

		case 'r':
			resource_print = optarg;
			break;

		default:
			usage();
			exit(1);
		}

	}
	argc -= optind;
	argv += optind;

	if (argc)
		interval = atoi(argv[0]);

	if (group_print && resource_print) {
		fprintf(stderr, "Cannot specify both a resource and a group\n");
		exit (1);
	}

	if (group_print)
		validate_group();
	if (resource_print)
		validate_resource();

	do {
		get_template_info();
		get_all_info();
		dump_all_info();
		cleanup();
		sleep(interval);
	} while (interval);
}
