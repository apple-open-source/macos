#include <iomanip>
#include <iostream>
#include <map>

#include "../src/trace.h"
#include <ktrace/ktrace.h>
#include <sys/kdebug.h>

typedef struct {
	// allocation size -> number of allocations
	std::map<size_t, size_t> allocs;
	size_t null_allocs;
	size_t total_allocs;
	// allocation size -> number of frees
	std::map<size_t, size_t> frees;
	size_t null_frees;
	size_t total_frees;
} analyze_ktrace_thread_t;

typedef struct {
	// tid -> analyze_ktrace_thread_t
	std::map<uint64_t, analyze_ktrace_thread_t> thread;
	// ptr -> allocation size
	std::unordered_map<uintptr_t, size_t> ptr_size;
} analyze_ktrace_t;

static inline size_t
roundup_log2(size_t sz)
{
	if (!(sz & (sz - 1u))) {
		return sz;
	} else {
		return (1ul << (CHAR_BIT * sizeof(sz) - (unsigned)__builtin_clzl(sz)));
	}
}

static inline void
track_alloc(analyze_ktrace_t &ak, uint64_t tid, uintptr_t ptr, size_t sz)
{
	analyze_ktrace_thread_t &thread = ak.thread[tid];

	if (ptr) {
		++thread.allocs[roundup_log2(sz)];
		ak.ptr_size[ptr] = sz;

		++thread.total_allocs;
	} else {
		++thread.null_allocs;
	}
}

static inline void
track_free(analyze_ktrace_t &ak, uint64_t tid, uintptr_t ptr)
{
	analyze_ktrace_thread_t &thread = ak.thread[tid];

	if (ptr) {
		auto it = ak.ptr_size.find(ptr);
		if (it != ak.ptr_size.end()) {
			++thread.frees[roundup_log2(it->second)];
			ak.ptr_size.erase(it);
		}

		++thread.total_frees;
	} else {
		++thread.null_frees;
	}
}

static inline void
track_thread(analyze_ktrace_t &ak, uint64_t tid)
{
	ak.thread.emplace(tid, analyze_ktrace_thread_t{});
}

static void
add_empty_log2(std::map<size_t, size_t> &sizes)
{
	// add zero entries, starting from largest allocation size
	auto rit = sizes.rbegin();
	if (rit != sizes.rend()) {
		size_t max_pow2 = rit->first;
		do {
			max_pow2 /= 2;
			sizes.emplace(max_pow2, 0);
		} while (max_pow2);
	}
}

static bool
analyze_ktrace(ktrace_session_t ks, analyze_ktrace_t &ak)
{
	if (ktrace_events_subclass(ks, DBG_UMALLOC, DBG_UMALLOC_EXTERNAL,
				(^(ktrace_event_t event) {
				  switch (event->debugid) {
				  case TRACE_DATA_NEWTHREAD:
					  // args: child_tid, pid, exec, uniqueid
					  track_thread(ak, event->arg1);
					  break;

				  case TRACE_malloc | DBG_FUNC_START:
					  // args: zone, size, type_id, 0
					  break;
				  case TRACE_malloc | DBG_FUNC_END:
					  // args: zone, size, ptr, type_id
					  track_alloc(
							  ak, event->threadid, event->arg3, event->arg2);
					  break;

				  case TRACE_malloc_options | DBG_FUNC_START:
					  // args: zone, align, size, 0
					  break;
				  case TRACE_malloc_options | DBG_FUNC_END:
					  // args: zone, align, size, ptr
					  track_alloc(
							  ak, event->threadid, event->arg4, event->arg3);
					  break;

				  case TRACE_calloc | DBG_FUNC_START:
					  // args: zone, num_items, size, type_id
					  break;
				  case TRACE_calloc | DBG_FUNC_END:
					  // args: zone, num_items, size, ptr
					  track_alloc(ak, event->threadid, event->arg4,
							  event->arg2 * event->arg3);
					  break;

				  case TRACE_valloc | DBG_FUNC_START:
					  // args: zone, size, type_id, 0
					  break;
				  case TRACE_valloc | DBG_FUNC_END:
					  // args: zone, size, ptr, type_id
					  track_alloc(
							  ak, event->threadid, event->arg3, event->arg2);
					  break;

				  case TRACE_memalign | DBG_FUNC_START:
					  // args: zone, alignment, size, type_id
					  break;
				  case TRACE_memalign | DBG_FUNC_END:
					  // args: zone, alignment, size, ptr
					  track_alloc(
							  ak, event->threadid, event->arg4, event->arg3);
					  break;

				  case TRACE_realloc | DBG_FUNC_START:
					  // args: zone, ptr, size, type_id
					  break;
				  case TRACE_realloc | DBG_FUNC_END:
					  // args: zone, ptr, size, new_ptr
					  track_free(ak, event->threadid, event->arg2);
					  track_alloc(
							  ak, event->threadid, event->arg4, event->arg3);
					  break;

				  case TRACE_free:
					  // args: zone, ptr, (ptr) ? *(uintptr_t*)ptr : 0, 0
					  track_free(ak, event->threadid, event->arg2);
					  break;
				  }
				}))) {
		return false;
	}

	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	if (!done) {
		return false;
	}

	ktrace_set_completion_handler(ks, ^{
	  ktrace_end(ks, false);

	  dispatch_semaphore_signal(done);
	});

	dispatch_queue_t mq =
			dispatch_queue_create("analyze_ktrace", DISPATCH_QUEUE_SERIAL);
	if (ktrace_start(ks, mq)) {
		return false;
	}

	dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);

	for (auto &t : ak.thread) {
		add_empty_log2(t.second.allocs);
		add_empty_log2(t.second.frees);
	}

	return true;
}

static constexpr unsigned FIELD_WIDTH = 10;

static void
print_thread(const analyze_ktrace_thread_t &at)
{
	std::cout << "====== Allocation Sizes ======" << std::endl;
	for (const auto &kv : at.allocs) {
		const double alloc_percent =
				(100. * ((double)kv.second / (double)at.total_allocs));
		std::cout << "≤ " << std::setw(FIELD_WIDTH) << kv.first << ": "
				  << std::setw(FIELD_WIDTH) << kv.second << " ("
				  << alloc_percent << "%)" << std::endl;
	}
	std::cout << std::setw(FIELD_WIDTH + 4)
			  << "Total: " << std::setw(FIELD_WIDTH) << at.total_allocs << "\n"
			  << std::setw(FIELD_WIDTH + 4)
			  << "NULL: " << std::setw(FIELD_WIDTH) << at.null_allocs << "\n"
			  << std::endl;

	std::cout << "========= Free Sizes =========" << std::endl;
	for (const auto &kv : at.frees) {
		const double free_percent =
				(100. * ((double)kv.second / (double)at.total_frees));
		std::cout << "≤ " << std::setw(FIELD_WIDTH) << kv.first << ": "
				  << std::setw(FIELD_WIDTH) << kv.second << " (" << free_percent
				  << "%)" << std::endl;
	}
	std::cout << std::setw(FIELD_WIDTH + 4)
			  << "Total: " << std::setw(FIELD_WIDTH) << at.total_frees << "\n"
			  << std::setw(FIELD_WIDTH + 4)
			  << "NULL: " << std::setw(FIELD_WIDTH) << at.null_frees
			  << std::endl;
}

static void
print_analysis(const analyze_ktrace_t &ak)
{
	analyze_ktrace_thread_t aggregate{};
	bool should_aggregate = (ak.thread.size() > 1);

	std::cout << std::fixed << std::setprecision(2);

	for (const auto &t : ak.thread) {
		if (should_aggregate) {
			for (const auto &kv : t.second.allocs) {
				aggregate.allocs[kv.first] += kv.second;
			}
			aggregate.null_allocs += t.second.null_allocs;
			aggregate.total_allocs += t.second.total_allocs;

			for (const auto &kv : t.second.frees) {
				aggregate.frees[kv.first] += kv.second;
			}
			aggregate.null_frees += t.second.null_frees;
			aggregate.total_frees += t.second.total_frees;
		}

		std::cout << "\n<<<<<< Thread 0x" << std::setw(8) << std::setfill('0')
				  << std::hex << t.first << " >>>>>>" << std::endl;

		std::cout << std::dec << std::setfill(' ');
		print_thread(t.second);
	}

	if (should_aggregate) {
		std::cout << "\n<<<<<<<<<< Aggregate >>>>>>>>>>" << std::endl;
		print_thread(aggregate);
	}
}

int
main(int argc, char **argv)
{
	int ret = 0;

	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <input ktrace> " << std::endl;
		return -1;
	}

	std::cout << "Parsing '" << argv[1] << "'..." << std::endl;

	ktrace_file_t kf = ktrace_file_open(argv[1], false);
	if (!kf) {
		std::cerr << "Error: Cannot open ktrace file '" << argv[1] << "'"
				  << std::endl;
		return -1;
	}

	analyze_ktrace_t ak{};
	ktrace_session_t ks = ktrace_session_create_with_flags(KTSF_NONE);
	// takes ownership of the ktrace_file_t
	if (ktrace_set_ktrace_file(ks, kf, true)) {
		ktrace_file_close(kf);
		std::cerr << "Error: Cannot create ktrace session" << std::endl;
		ret = -1;
		goto out;
	}

	if (!analyze_ktrace(ks, ak)) {
		std::cerr << "Error: Cannot analyze '" << argv[1] << "'" << std::endl;
		ret = -1;
	}

	print_analysis(ak);

out:
	ktrace_session_destroy(ks);
	return ret;
}
