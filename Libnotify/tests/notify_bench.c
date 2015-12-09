#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <notify.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <assert.h>

extern uint32_t notify_register_plain(const char *name, int *out_token);
#ifdef NO_OP_TESTS
extern uint32_t notify_no_op_str_sync(const char *name, size_t len);
extern uint32_t notify_no_op_str_async(const char *name, size_t len);
extern uint32_t notify_no_op_int_sync(int n);
extern uint32_t notify_no_op_int_async(int n);
#endif

#define MAX_CNT 100
#define MAX_SPL 10000

#define DEFAULT_CNT 50
#if defined(__arm__)
#define DEFAULT_SPL 501
#else
#define DEFAULT_SPL 1001
#endif

uint32_t cnt = DEFAULT_CNT;
uint32_t spl = DEFAULT_SPL;

static long double loop_cost;
static mach_timebase_info_data_t tbi;
static uint64_t dmy[MAX_SPL], reg_plain[MAX_SPL], cancel_plain[MAX_SPL], reg_port[MAX_SPL], cancel_port[MAX_SPL];
static uint64_t post_plain1[MAX_SPL], post_plain2[MAX_SPL], post_plain3[MAX_SPL];
static uint64_t post_port[MAX_SPL], set_state1[MAX_SPL], set_state2[MAX_SPL], get_state[MAX_SPL];
#ifdef NO_OP_TESTS
static uint64_t nss[MAX_SPL], nsa[MAX_SPL], nis[MAX_SPL], nia[MAX_SPL];
#endif

static void __attribute__((noinline))
print_result(uint64_t *s, const char *str)
{
	unsigned j;
	uint64_t m;
	long double dd = 0, mm, sd = 0;

	for (j = 0 ; j < spl; j++) {
		dd += (long double)s[j]/(long double)cnt;
	}
	dd = dd/(long double)spl;

	for (j = 0 ; j < spl; j++) {
		long double tmp = (long double)s[j]/(long double)cnt - dd;
		sd += tmp * tmp;
	}
	sd = sqrtl(sd/(long double)spl);

	qsort_b(s, spl, sizeof(uint64_t),
			^(const void *a, const void *b){
				const uint64_t l = *(const uint64_t*)a;
				const uint64_t r = *(const uint64_t*)b;
				return l == r ? 0 : (l < r ? -1 : 1);
			});
	m = s[spl/2];
	mm = (long double)m / (long double)cnt;

	if (tbi.numer != tbi.denom) {
		dd *= tbi.numer;
		dd /= tbi.denom;
		mm *= tbi.numer;
		mm /= tbi.denom;
		sd *= tbi.numer;
		sd /= tbi.denom;
	}

	if (str) {
		dd -= loop_cost;
		mm -= loop_cost;	
	} else {
		loop_cost = mm;
	}

	dd /= NSEC_PER_USEC;
	dd = roundl(dd * 200.0)/200.0;
	mm /= NSEC_PER_USEC;
	mm = roundl(mm * 200.0)/200.0;
	sd /= NSEC_PER_USEC;
	sd = roundl(sd * 200.0)/200.0;

	if (!str) {
		printf("%-28s %-8s    %-8s    %-8s\n",
				"Symbol", "Median", "Average", "StdDev");
	}
	printf("%-28s%8.3Lf us %8.3Lf us %8.3Lf us\n",
			str ? str : "Empty loop:", mm, dd, sd);
}

int
main(int argc, char *argv[])
{
	uint32_t r;
	kern_return_t kr;
	unsigned i, j;

	kr = mach_timebase_info(&tbi);
	assert(!kr);

	int tok;
	r = notify_register_check("dummy.test", &tok);
	assert(r == 0);
	r = notify_cancel(tok);
	assert(r == 0);

	int t[MAX_CNT];
	mach_port_t p[MAX_CNT];
	char *n[MAX_CNT];
	size_t l[MAX_CNT];
	uint64_t s;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-c")) cnt = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-s")) spl = atoi(argv[++i]) + 1;
	}

	if (cnt > MAX_CNT) cnt = MAX_CNT;
	if (spl > MAX_SPL) spl = MAX_SPL + 1;

	for (j = 0 ; j < spl; j++)
	{
		for (i = 0; i < cnt; i++)
		{
			r = asprintf(&n[i], "dummy.test.%d", i);
			assert(r != -1);
			l[i] = strlen(n[i]);
			kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p[i]);
			assert(kr == 0);
		}

		/* Empty Loop */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
#if defined(__arm__)
			asm volatile("movs %0, #0 @ %1" : "=r" (r) : "m" (n[i]) );
#elif defined(__x86_64)
			asm volatile("movl $0, %0 # %1" : "=r" (r) : "m" (n[i]) );
#else
			asm volatile("" : : "m" (n[i]) );
			r = 0;
#endif
			assert(r == 0);
		}
		dmy[j] = mach_absolute_time() - s;

#ifdef NO_OP_TESTS
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_no_op_str_sync(n[i], l[i]);
			assert(r == 0);
		}
		nss[j] = mach_absolute_time() - s;

		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_no_op_str_async(n[i], l[i]);
			assert(r == 0);
		}
		nsa[j] = mach_absolute_time() - s;

		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_no_op_int_sync(i);
			assert(r == 0);
		}
		nis[j] = mach_absolute_time() - s;

		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_no_op_int_async(i);
			assert(r == 0);
		}
		nia[j] = mach_absolute_time() - s;
#endif

		/* Register Plain */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_register_plain(n[i], &t[i]);
			assert(r == 0);
		}
		reg_plain[j] = mach_absolute_time() - s;

		/* Post 1 */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_post(n[i]);
			assert(r == 0);
		}
		post_plain1[j] = mach_absolute_time() - s;
		
		/* Post 2 */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_post(n[i]);
			assert(r == 0);
		}
		post_plain2[j] = mach_absolute_time() - s;
		
		/* Post 3 */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_post(n[i]);
			assert(r == 0);
		}
		post_plain3[j] = mach_absolute_time() - s;
		
		/* Cancel Plain */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_cancel(t[i]);
			assert(r == 0);
		}
		cancel_plain[j] = mach_absolute_time() - s;

		/* Register Mach Port */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_register_mach_port(n[i], &p[i], NOTIFY_REUSE, &t[i]);
			assert(r == 0);
		}
		reg_port[j] = mach_absolute_time() - s;

#ifdef NOTDEF
		/* Post */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_post(n[i]);
			assert(r == 0);
		}
		post_port[j] = mach_absolute_time() - s;
		for (i = 0; i < cnt; i++)
		{
			mach_msg_empty_rcv_t msg = { .header = {
				.msgh_size = sizeof(mach_msg_empty_rcv_t),
				.msgh_local_port = p[i],
			}};
			kr = mach_msg_receive(&msg.header);
			assert(kr == 0);
			mach_msg_destroy(&msg.header);
		}
#endif
		/* Set State 1 */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_set_state(t[i], 1);
			assert(r == 0);
		}
		set_state1[j] = mach_absolute_time() - s;

		/* Get State */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			uint64_t dummy;
			r = notify_get_state(t[i], &dummy);
			assert(r == 0);
		}
		get_state[j] = mach_absolute_time() - s;
		
		/* Set State 2 */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_set_state(t[i], 2);
			assert(r == 0);
		}
		set_state2[j] = mach_absolute_time() - s;
		
		/* Cancel Port */
		s = mach_absolute_time();
		for (i = 0; i < cnt; i++)
		{
			r = notify_cancel(t[i]);
			assert(r == 0);
		}
		cancel_port[j] = mach_absolute_time() - s;

		for (i = 0; i < cnt; i++)
		{
			free(n[i]);
			kr = mach_port_mod_refs(mach_task_self(), p[i], MACH_PORT_RIGHT_RECEIVE, -1);
			assert(kr == 0);
		}
	}

	print_result(dmy, NULL);
#ifdef NO_OP_TESTS
	print_result(nss,  "notify_no_op_str_sync:");
	print_result(nsa,  "notify_no_op_str_async:");
	print_result(nis,  "notify_no_op_int_sync:");
	print_result(nia,  "notify_no_op_int_async:");
#endif
	print_result(reg_plain, "notify_register_plain:");
	print_result(post_plain1,  "notify_post [plain 1]:");
	print_result(post_plain2,  "notify_post [plain 2]:");
	print_result(post_plain3,  "notify_post [plain 3]:");
	print_result(cancel_plain, "notify_cancel [plain]:");
	print_result(reg_port,  "notify_register_mach_port:");
	print_result(set_state1,  "notify_set_state [1]:");
	print_result(set_state2,  "notify_set_state [2]:");
	print_result(get_state,  "notify_get_state:");
	print_result(cancel_port,  "notify_cancel [port]");

	return 0;
}
