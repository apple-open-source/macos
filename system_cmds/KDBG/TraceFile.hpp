//
//  TraceFile.hpp
//  system_cmds
//
//  Created by James McIlree on 4/1/14.
//
//

#ifndef __system_cmds__TraceFile__
#define __system_cmds__TraceFile__

// These are not (yet) defined in debug.h
// Remove and use kdebug.h ASAP.
#define RAW_VERSION2	0x55aa0200 // RAW_VERSION2 is from Instruments/kperf
#define RAW_VERSION3	0x55aa0300 // RAW_VERSION3 is the new hotness from kperf

enum class kTraceFileVersion : uint32_t {
	V0 = 0,
	V1 = 1,
	V1Plus = 2, // A 1+ is a 1 with a cpumap
	V2 = 3, // Can type 2 contain a cpumap? Looks like no.
	V3 = 4,
	Unknown = UINT32_MAX
};

class TraceFile {
    protected:
	MappedFile			_file;
	kTraceFileVersion		_version;
	bool				_is_64_bit;
	bool				_is_valid;
	void*				_threadmap;
	uint32_t			_threadmap_count;
	KDCPUMapEntry*			_cpumap;
	uint32_t			_cpumap_count;
	void*				_events;
	uintptr_t			_event_count;
	std::vector<uint8_t>		_time_sorted_events; // This is empty unless event sorting is requested.
	std::vector<KDCPUMapEntry>	_default_cpumap; // If the file does not contain a cpumap, this will be used instead

	template <typename SIZE>
	void sanity_check_event_data();
	
	template <typename SIZE>
	void parse(bool, uint32_t, uint32_t);

    public:
	TraceFile(const char* path, bool sort_events = false, uint32_t default_ap_count = 24, uint32_t default_iop_count = 0);

	// Returns true if a Machine state can be created.
	bool is_valid()	const				{ return _is_valid; }
	bool is_64_bit() const				{ return _is_64_bit; }
	kTraceFileVersion version() const		{ return _version; }

	// Exposed so iOS devices can report over sized trace
	bool mmap_failed() const			{ return _file.mmap_failed(); }

	const KDCPUMapEntry* cpumap() const		{ return _cpumap; }
	uint32_t cpumap_count() const			{ return _cpumap_count; }

	template <typename SIZE>
	const KDThreadMapEntry<SIZE>* threadmap() const	{ return reinterpret_cast<KDThreadMapEntry<SIZE>*>(_threadmap); }
	uint32_t threadmap_count() const		{ return _threadmap_count; }

	template <typename SIZE>
	const KDEvent<SIZE>* events() const		{ return reinterpret_cast<KDEvent<SIZE>*>(_events); }
	uintptr_t event_count() const			{ return _event_count; }
};

//
// This is a very simple attempt to sanity check the event data and prevent
// crashes when reading 32b vs 64b trace data.
//
template <typename SIZE>
void TraceFile::sanity_check_event_data() {
	uintptr_t event_check_count = std::min((uintptr_t)10, _event_count);

	AbsTime last_timestamp;

	for (uintptr_t i=0; i<event_check_count; i++) {
		KDEvent<SIZE>& event = reinterpret_cast<KDEvent<SIZE>*>(_events)[i];

		if (event.cpu() < 0) {
			THROW("Event cpu id is less than 0");
		}

		if (event.cpu() >= _cpumap_count) {
			THROW("Event cpu id is greater than the number of configured cpus");
		}

		if (event.timestamp() < last_timestamp) {
			THROW("Event Data sanity check found out of order timestamps");
		}

		if (SIZE::is_64_bit) {
			if (event.unused() != 0) {
				THROW("Event has value set in unknown field");
			}
		}

		last_timestamp = event.timestamp();
	}
}

template <typename SIZE>
void TraceFile::parse(bool should_presort_events, uint32_t default_ap_count, uint32_t default_iop_count) {
	if (TraceDataHeader<SIZE>* header = reinterpret_cast<TraceDataHeader<SIZE>*>(_file.address())) {
		KDThreadMapEntry<SIZE>* threadmap = NULL;
		uint32_t threadmap_count = 0;
		KDCPUMapEntry* cpumap = NULL;
		uint32_t cpumap_count = 0;
		KDEvent<SIZE>* events = NULL;
		kTraceFileVersion version;

		switch (header->version()) {
			case RAW_VERSION0:
				// Should never happen!
				ASSERT(false, "File is RAW_VERSION0");
				THROW("RAW_VERSION0 is ILLEGAL");
				break;

			case RAW_VERSION1:
				// Could be either v1 or v1+
				break;

			case RAW_VERSION2:
				_version = kTraceFileVersion::V2;
				// We do not know how to parse a V2 file
				THROW("RAW_VERSION2 is unhandled");
				break;

			case RAW_VERSION3:
				_version = kTraceFileVersion::V3;
				// We do not know how to parse a V3 file
				THROW("RAW_VERSION3 is unhandled");
				break;

			default:
				// Could be a v0
				break;
		}

		if (header->version() != RAW_VERSION1) {
			// If the header is not a RAW_VERSION1, we must assume it is a
			// RAW_VERSION0. The difficulty here is that RAW_VERSION0 consists
			// of 4 bytes, which are the thread_count. We can't do much
			// sanity checking. The first four bytes are already read into
			// the existing header, reuse them. We must also reset the file
			// offset.

			threadmap_count = header->version();
			threadmap = reinterpret_cast<KDThreadMapEntry<SIZE>*>(_file.address() + 4);

			// Event data starts immediately following the threadmap
			size_t offset = 4 + threadmap_count * sizeof(KDThreadMapEntry<SIZE>);
			events = reinterpret_cast<KDEvent<SIZE>*>(_file.address() + offset);

			version = kTraceFileVersion::V0;
		} else {
			//
			// RAW_VERSION1
			//
			threadmap_count = header->thread_count();
			threadmap = reinterpret_cast<KDThreadMapEntry<SIZE>*>(_file.address() + sizeof(TraceDataHeader<SIZE>));

			size_t threadmap_size_in_bytes = threadmap_count * sizeof(KDThreadMapEntry<SIZE>);
			size_t offset_to_event_data = (sizeof(TraceDataHeader<SIZE>) + threadmap_size_in_bytes + 4095) & ~4095;
			size_t offset_to_cpumap_data = sizeof(TraceDataHeader<SIZE>) + threadmap_size_in_bytes;
			size_t cpumap_bytes = offset_to_event_data - offset_to_cpumap_data;

			//
			// In a RAW_VERSION1, there *may* be a cpumap.
			// If it exists, it will be between the header and the page aligned offset
			// that event data begins at.
			//
			if (cpumap_bytes > sizeof(kd_cpumap_header) + sizeof(kd_cpumap)) {
				kd_cpumap_header* cpumap_header = reinterpret_cast<kd_cpumap_header*>(_file.address() + offset_to_cpumap_data);
				if (cpumap_header->version_no == RAW_VERSION1) {
					cpumap = (KDCPUMapEntry*)&cpumap_header[1];
					cpumap_count = cpumap_header->cpu_count;
				}
			}

			// Event data starts at the next PAGE alignment boundary.
			//
			// Hmm, this could be pretty awful in iOS...
			//
			// Kernel page size is 4k. Userspace page size is 16kb in 64b.
			// Kernel writes the data. Unless the kernel call fails, then userspace writes the data. Blech.
			events = reinterpret_cast<KDEvent<SIZE>*>(_file.address() + offset_to_event_data);
		}

		uintptr_t event_count = (uintptr_t)_file.size() - (reinterpret_cast<uintptr_t>(events) - reinterpret_cast<uintptr_t>(_file.address()));
		if (event_count % sizeof(KDEvent<SIZE>) != 0) {
			// We're probably looking at the wrong k32/k64. Throw and try the other size.
			THROW("Bytes in file does not match an even multiple of Event struct");
		}
		event_count /= sizeof(KDEvent<SIZE>);

		if (cpumap == NULL || cpumap_count == 0) {
			// No cpumap found, we need to fake one up using the default values.
			//
			// It would be nice if we could just read the events and derive the
			// AP/IOP count, but the IOP events do not have valid tid(s), and
			// must be ignored.

			for (uint32_t i=0; i<default_ap_count; ++i) {
				_default_cpumap.emplace_back(i, 0, "AP-???");
			}
			uint32_t iop_limit = default_ap_count + default_iop_count;
			for (uint32_t i=default_ap_count; i<iop_limit; ++i) {
				_default_cpumap.emplace_back(i, KDBG_CPUMAP_IS_IOP, "IOP-???");
			}

			cpumap = _default_cpumap.data();
			cpumap_count = (uint32_t)_default_cpumap.size();

			version = kTraceFileVersion::V1;
		} else {
			version = kTraceFileVersion::V1Plus;
		}

		
		// IOP's have been producing .trace files with out of order events.
		// This is a hack fix to work around that. It costs a full copy of the data!
		MemoryBuffer<KDEvent<SIZE>> presorted_events;
		if (should_presort_events && event_count) {
			_time_sorted_events.reserve(event_count * sizeof(KDEvent<SIZE>));
			memcpy(_time_sorted_events.data(), events, event_count * sizeof(KDEvent<SIZE>));
			events = reinterpret_cast<KDEvent<SIZE>*>(_time_sorted_events.data());
			std::sort(events, events + event_count, [](KDEvent<SIZE> const& p0, KDEvent<SIZE> const& p1) -> bool {
				return p0.timestamp() < p1.timestamp();
			});
		}

		_threadmap = threadmap;
		_threadmap_count = threadmap_count;

		_cpumap = cpumap;
		_cpumap_count = cpumap_count;

		_events = events;
		_event_count = event_count;

		_version = version;
		_is_64_bit = SIZE::is_64_bit;

		sanity_check_event_data<SIZE>();

		//
		// Okay, success if we made it this far.
		//
		_is_valid = true;
	}
}

#endif /* defined(__system_cmds__TraceFile__) */
