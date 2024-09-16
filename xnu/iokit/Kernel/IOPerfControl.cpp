/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
 */

#include <IOKit/perfcontrol/IOPerfControl.h>

#include <stdatomic.h>

#include <kern/thread_group.h>
#include <kern/task.h>
#include <sys/coalition.h>
#include <kern/coalition.h>

#undef super
#define super OSObject
OSDefineMetaClassAndStructors(IOPerfControlClient, OSObject);

static IOPerfControlClient::IOPerfControlClientShared *_Atomic gIOPerfControlClientShared;

bool
IOPerfControlClient::init(IOService *driver, uint64_t maxWorkCapacity)
{
	// TODO: Remove this limit and implement dynamic table growth if workloads are found that exceed this
	if (maxWorkCapacity > kMaxWorkTableNumEntries) {
		maxWorkCapacity = kMaxWorkTableNumEntries;
	}

	if (!super::init()) {
		return false;
	}

	shared = atomic_load_explicit(&gIOPerfControlClientShared, memory_order_acquire);
	if (shared == nullptr) {
		IOPerfControlClient::IOPerfControlClientShared *expected = shared;
		shared = kalloc_type(IOPerfControlClientShared, Z_WAITOK);
		if (!shared) {
			return false;
		}

		atomic_init(&shared->maxDriverIndex, 0);

		shared->interface = PerfControllerInterface{
			.version = PERFCONTROL_INTERFACE_VERSION_NONE,
			.registerDevice =
		    [](IOService *device) {
			    return kIOReturnSuccess;
		    },
			.unregisterDevice =
			    [](IOService *device) {
				    return kIOReturnSuccess;
			    },
			.workCanSubmit =
			    [](IOService *device, PerfControllerInterface::WorkState *state, WorkSubmitArgs *args) {
				    return false;
			    },
			.workSubmit =
			    [](IOService *device, uint64_t token, PerfControllerInterface::WorkState *state, WorkSubmitArgs *args) {
			    },
			.workBegin =
			    [](IOService *device, uint64_t token, PerfControllerInterface::WorkState *state, WorkBeginArgs *args) {
			    },
			.workEnd =
			    [](IOService *device, uint64_t token, PerfControllerInterface::WorkState *state, WorkEndArgs *args, bool done) {
			    },
			.workUpdate =
			    [](IOService *device, uint64_t token, PerfControllerInterface::WorkState *state, WorkUpdateArgs *args) {
			    },
		};

		shared->interfaceLock = IOLockAlloc();
		if (!shared->interfaceLock) {
			goto shared_init_error;
		}

		shared->deviceRegistrationList = OSSet::withCapacity(4);
		if (!shared->deviceRegistrationList) {
			goto shared_init_error;
		}

		if (!atomic_compare_exchange_strong_explicit(&gIOPerfControlClientShared, &expected, shared, memory_order_acq_rel,
		    memory_order_acquire)) {
			IOLockFree(shared->interfaceLock);
			shared->deviceRegistrationList->release();
			kfree_type(IOPerfControlClientShared, shared);
			shared = expected;
		}
	}
	workTable = NULL;
	workTableLock = NULL;

	// Note: driverIndex is not guaranteed to be unique if maxDriverIndex wraps around. It is intended for debugging only.
	driverIndex = atomic_fetch_add_explicit(&shared->maxDriverIndex, 1, memory_order_relaxed) + 1;

	// + 1 since index 0 is unused for kIOPerfControlClientWorkUntracked
	workTableLength = maxWorkCapacity + 1;
	assertf(workTableLength <= kWorkTableMaxSize, "%zu exceeds max allowed capacity of %zu", workTableLength, kWorkTableMaxSize);
	if (maxWorkCapacity > 0) {
		workTable = kalloc_type(WorkTableEntry, workTableLength, Z_WAITOK_ZERO);
		if (!workTable) {
			goto error;
		}
		workTableNextIndex = 1;

		workTableLock = IOSimpleLockAlloc();
		if (!workTableLock) {
			goto error;
		}
	}

	bzero(&clientData, sizeof(clientData));

	return true;

error:
	if (workTable) {
		kfree_type(WorkTableEntry, workTableLength, workTable);
		workTable = NULL;
	}
	if (workTableLock) {
		IOSimpleLockFree(workTableLock);
		workTableLock = NULL;
	}
	return false;
shared_init_error:
	if (shared) {
		if (shared->interfaceLock) {
			IOLockFree(shared->interfaceLock);
		}
		if (shared->deviceRegistrationList) {
			shared->deviceRegistrationList->release();
		}
		kfree_type(IOPerfControlClientShared, shared);
		shared = nullptr;
	}
	return false;
}

void
IOPerfControlClient::free()
{
	if (workTable) {
		kfree_type(WorkTableEntry, workTableLength, workTable);
	}
	if (workTableLock) {
		IOSimpleLockFree(workTableLock);
	}
	super::free();
}

IOPerfControlClient *
IOPerfControlClient::copyClient(IOService *driver, uint64_t maxWorkCapacity)
{
	IOPerfControlClient *client = new IOPerfControlClient;
	if (!client || !client->init(driver, maxWorkCapacity)) {
		panic("could not create IOPerfControlClient");
	}
	return client;
}

/* Convert the per driver token into a globally unique token for the performance
 * controller's consumption. This is achieved by setting the driver's unique
 * index onto the high order bits. The performance controller is shared between
 * all drivers and must track all instances separately, while each driver has
 * its own token table, so this step is needed to avoid token collisions between
 * drivers.
 */
inline uint64_t
IOPerfControlClient::tokenToGlobalUniqueToken(uint64_t token)
{
	return token | (static_cast<uint64_t>(driverIndex) << kWorkTableIndexBits);
}

/* Accounting resources used in a work item to the containing coalition.
 * Contigent upon the PerfController signaling that it wants resource accounting
 * in the registerDevice()/registerDriverDevice calls. More device types can
 * be added here in the future.
 */
void
IOPerfControlClient::accountResources(coalition_t coal, PerfControllerInterface::PerfDeviceID device_type, PerfControllerInterface::ResourceAccounting *resources)
{
	switch (device_type) {
	case PerfControllerInterface::PerfDeviceID::kANE:
		if (coal != nullptr) {
			coalition_update_ane_stats(coal, resources->mach_time_delta, resources->energy_nj_delta);
		}
		break;

	default:
		assertf(false, "Unexpected device type for IOPerfControlClient::accountResources: %llu", static_cast<uint64_t>(device_type));
	}
}

/* With this implementation, tokens returned to the driver differ from tokens
 * passed to the performance controller. This implementation has the nice
 * property that tokens returns to the driver will aways be between 1 and
 * the value of maxWorkCapacity passed by the driver to copyClient. The tokens
 * the performance controller sees will match on the lower order bits and have
 * the driver index set on the high order bits.
 */
uint64_t
IOPerfControlClient::allocateToken(thread_group *thread_group)
{
	uint64_t token = kIOPerfControlClientWorkUntracked;

#if CONFIG_THREAD_GROUPS
	auto s = IOSimpleLockLockDisableInterrupt(workTableLock);

	uint64_t num_tries = 0;
	size_t index = workTableNextIndex;
	// - 1 since entry 0 is for kIOPerfControlClientWorkUntracked
	while (num_tries < workTableLength - 1) {
		if (workTable[index].thread_group == nullptr) {
			thread_group_retain(thread_group);
			workTable[index].thread_group = thread_group;
			if (clientData.driverState.resource_accounting) {
				auto *coalition = task_get_coalition(current_task(), COALITION_TYPE_RESOURCE);
				assert(coalition != nullptr);
				coalition_retain(coalition);
				workTable[index].coal = coalition;
			}
			token = index;
			// next integer between 1 and workTableLength - 1
			workTableNextIndex = (index % (workTableLength - 1)) + 1;
			break;
		}
		// next integer between 1 and workTableLength - 1
		index = (index % (workTableLength - 1)) + 1;
		num_tries += 1;
	}
#if (DEVELOPMENT || DEBUG)
	if (token == kIOPerfControlClientWorkUntracked) {
		/* When investigating a panic here, first check that the driver is not leaking tokens.
		 * If the driver is not leaking tokens and maximum is less than kMaxWorkTableNumEntries,
		 * the driver should be modified to pass a larger value to copyClient.
		 * If the driver is not leaking tokens and maximum is equal to kMaxWorkTableNumEntries,
		 * this code will have to be modified to support dynamic table growth to support larger
		 * numbers of tokens.
		 */
		panic("Tokens allocated for this device exceeded maximum of %zu.",
		    workTableLength - 1); // - 1 since entry 0 is for kIOPerfControlClientWorkUntracked
	}
#endif

	IOSimpleLockUnlockEnableInterrupt(workTableLock, s);
#endif

	return token;
}

void
IOPerfControlClient::deallocateToken(uint64_t token)
{
#if CONFIG_THREAD_GROUPS
	assertf(token != kIOPerfControlClientWorkUntracked, "Attempt to deallocate token kIOPerfControlClientWorkUntracked\n");
	assertf(token <= workTableLength, "Attempt to deallocate token %llu which is greater than the table size of %zu\n", token, workTableLength);
	auto s = IOSimpleLockLockDisableInterrupt(workTableLock);

	auto &entry = workTable[token];
	auto *thread_group = entry.thread_group;
	auto *coal = entry.coal;
	bzero(&entry, sizeof(entry));
	workTableNextIndex = token;

	IOSimpleLockUnlockEnableInterrupt(workTableLock, s);

	// This can call into the performance controller if the last reference is dropped here. Are we sure
	// the driver isn't holding any locks? If not, we may want to async this to another context.
	thread_group_release(thread_group);
	if (coal != nullptr) {
		coalition_release(coal);
	}
#endif
}

IOPerfControlClient::WorkTableEntry *
IOPerfControlClient::getEntryForToken(uint64_t token)
{
	if (token == kIOPerfControlClientWorkUntracked) {
		return nullptr;
	}

	if (token >= workTableLength) {
		panic("Invalid work token (%llu): index out of bounds.", token);
	}

	WorkTableEntry *entry = &workTable[token];
	assertf(entry->thread_group, "Invalid work token: %llu", token);
	return entry;
}

void
IOPerfControlClient::markEntryStarted(uint64_t token, bool started)
{
	if (token == kIOPerfControlClientWorkUntracked) {
		return;
	}

	if (token >= workTableLength) {
		panic("Invalid work token (%llu): index out of bounds.", token);
	}

	workTable[token].started = started;
}

#if CONFIG_THREAD_GROUPS

static struct thread_group *
threadGroupForDextService(IOService *device)
{
	assert(device);

	if (!device->hasUserServer()) {
		return NULL;
	}

	// Devices associated with a dext driver, must be called from dext
	// context to ensure that thread_group reference is valid.
	thread_t thread = current_thread();
	assert(get_threadtask(thread) != kernel_task);
	struct thread_group * thread_group = thread_group_get(thread);
	assert(thread_group != nullptr);
	return thread_group;
}

#endif /* CONFIG_THREAD_GROUPS */

IOReturn
IOPerfControlClient::registerDevice(IOService *driver, IOService *device)
{
	IOReturn ret = kIOReturnSuccess;
#if CONFIG_THREAD_GROUPS
	IOLockLock(shared->interfaceLock);

	clientData.device = device;

	if (device) {
		struct thread_group *dext_thread_group = threadGroupForDextService(device);
		if (dext_thread_group) {
			if (clientData.driverState.has_target_thread_group) {
				panic("driverState has already been initialized");
			}
			clientData.driverState.has_target_thread_group = true;
			clientData.driverState.target_thread_group_id = thread_group_get_id(dext_thread_group);
			clientData.driverState.target_thread_group_data = thread_group_get_machine_data(dext_thread_group);

			clientData.target_thread_group = dext_thread_group;
			thread_group_retain(dext_thread_group);
		}
	}

	if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_3) {
		ret = shared->interface.registerDriverDevice(driver, device, &clientData.driverState);
	} else if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_1) {
		ret = shared->interface.registerDevice(device);
	} else {
		shared->deviceRegistrationList->setObject(this);
	}

	IOLockUnlock(shared->interfaceLock);
#endif
	return ret;
}

void
IOPerfControlClient::unregisterDevice(IOService *driver, IOService *device)
{
#if CONFIG_THREAD_GROUPS
	IOLockLock(shared->interfaceLock);

	if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_3) {
		shared->interface.unregisterDriverDevice(driver, device, &clientData.driverState);
	} else if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_1) {
		shared->interface.unregisterDevice(device);
	} else {
		shared->deviceRegistrationList->removeObject(this);
	}

	if (clientData.driverState.has_target_thread_group) {
		thread_group_release(clientData.target_thread_group);
		clientData.target_thread_group = nullptr;

		clientData.driverState.has_target_thread_group = false;
		clientData.driverState.target_thread_group_id = ~0ull;
		clientData.driverState.target_thread_group_data = nullptr;
	}

	clientData.device = nullptr;

	IOLockUnlock(shared->interfaceLock);
#endif
}

uint64_t
IOPerfControlClient::workSubmit(IOService *device, WorkSubmitArgs *args)
{
#if CONFIG_THREAD_GROUPS
	auto *thread_group = thread_group_get(current_thread());
	if (!thread_group) {
		return kIOPerfControlClientWorkUntracked;
	}

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(thread_group),
		.thread_group_data = thread_group_get_machine_data(thread_group),
		.work_data = nullptr,
		.work_data_size = 0,
		.started = false,
		.driver_state = &clientData.driverState
	};
	if (!shared->interface.workCanSubmit(device, &state, args)) {
		return kIOPerfControlClientWorkUntracked;
	}

	uint64_t token = allocateToken(thread_group);
	if (token != kIOPerfControlClientWorkUntracked) {
		state.work_data = &workTable[token].perfcontrol_data;
		state.work_data_size = sizeof(workTable[token].perfcontrol_data);
		shared->interface.workSubmit(device, tokenToGlobalUniqueToken(token), &state, args);
	}
	return token;
#else
	return kIOPerfControlClientWorkUntracked;
#endif
}

uint64_t
IOPerfControlClient::workSubmitAndBegin(IOService *device, WorkSubmitArgs *submitArgs, WorkBeginArgs *beginArgs)
{
#if CONFIG_THREAD_GROUPS
	auto *thread_group = thread_group_get(current_thread());
	if (!thread_group) {
		return kIOPerfControlClientWorkUntracked;
	}

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(thread_group),
		.thread_group_data = thread_group_get_machine_data(thread_group),
		.work_data = nullptr,
		.work_data_size = 0,
		.started = false,
		.driver_state = &clientData.driverState
	};
	if (!shared->interface.workCanSubmit(device, &state, submitArgs)) {
		return kIOPerfControlClientWorkUntracked;
	}

	uint64_t token = allocateToken(thread_group);
	if (token != kIOPerfControlClientWorkUntracked) {
		auto &entry = workTable[token];
		state.work_data = &entry.perfcontrol_data;
		state.work_data_size = sizeof(workTable[token].perfcontrol_data);
		shared->interface.workSubmit(device, tokenToGlobalUniqueToken(token), &state, submitArgs);
		state.started = true;
		shared->interface.workBegin(device, tokenToGlobalUniqueToken(token), &state, beginArgs);
		markEntryStarted(token, true);
	}
	return token;
#else
	return kIOPerfControlClientWorkUntracked;
#endif
}

void
IOPerfControlClient::workBegin(IOService *device, uint64_t token, WorkBeginArgs *args)
{
#if CONFIG_THREAD_GROUPS
	WorkTableEntry *entry = getEntryForToken(token);
	if (entry == nullptr) {
		return;
	}

	assertf(!entry->started, "Work for token %llu was already started", token);

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(entry->thread_group),
		.thread_group_data = thread_group_get_machine_data(entry->thread_group),
		.work_data = &entry->perfcontrol_data,
		.work_data_size = sizeof(entry->perfcontrol_data),
		.started = true,
		.driver_state = &clientData.driverState
	};
	shared->interface.workBegin(device, tokenToGlobalUniqueToken(token), &state, args);
	markEntryStarted(token, true);
#endif
}

void
IOPerfControlClient::workEnd(IOService *device, uint64_t token, WorkEndArgs *args, bool done)
{
#if CONFIG_THREAD_GROUPS
	WorkTableEntry *entry = getEntryForToken(token);
	if (entry == nullptr) {
		return;
	}

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(entry->thread_group),
		.thread_group_data = thread_group_get_machine_data(entry->thread_group),
		.work_data = &entry->perfcontrol_data,
		.work_data_size = sizeof(entry->perfcontrol_data),
		.started = entry->started,
		.driver_state = &clientData.driverState
	};

	if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_4) {
		PerfControllerInterface::ResourceAccounting resources;
		shared->interface.workEndWithResources(device, tokenToGlobalUniqueToken(token), &state, args, &resources, done);
		if (clientData.driverState.resource_accounting) {
			accountResources(workTable[token].coal, clientData.driverState.device_type, &resources);
		}
	} else {
		shared->interface.workEnd(device, tokenToGlobalUniqueToken(token), &state, args, done);
	}

	if (done) {
		deallocateToken(token);
	} else {
		markEntryStarted(token, false);
	}
#endif
}

static _Atomic uint64_t unique_work_context_id = 1ull;

class IOPerfControlWorkContext : public OSObject
{
	OSDeclareDefaultStructors(IOPerfControlWorkContext);

public:
	uint64_t id;
	struct thread_group *thread_group;
	coalition_t coal;
	bool started;
	uint8_t perfcontrol_data[32];

	bool init() override;
	void reset();
	void free() override;
};

OSDefineMetaClassAndStructors(IOPerfControlWorkContext, OSObject);

bool
IOPerfControlWorkContext::init()
{
	if (!super::init()) {
		return false;
	}
	id = atomic_fetch_add_explicit(&unique_work_context_id, 1, memory_order_relaxed) + 1;
	reset();
	return true;
}

void
IOPerfControlWorkContext::reset()
{
	thread_group = nullptr;
	coal = nullptr;
	started = false;
	bzero(perfcontrol_data, sizeof(perfcontrol_data));
}

void
IOPerfControlWorkContext::free()
{
	assertf(thread_group == nullptr, "IOPerfControlWorkContext ID %llu being released without calling workEnd!\n", id);
	assertf(coal == nullptr, "IOPerfControlWorkContext ID %llu being released without calling workEnd!\n", id);
	super::free();
}

OSObject *
IOPerfControlClient::copyWorkContext()
{
	IOPerfControlWorkContext *context = new IOPerfControlWorkContext;

	if (context == nullptr) {
		return nullptr;
	}

	if (!context->init()) {
		context->free();
		return nullptr;
	}

	return context;
}

bool
IOPerfControlClient::workSubmitAndBeginWithContext(IOService *device, OSObject *context, WorkSubmitArgs *submitArgs, WorkBeginArgs *beginArgs)
{
#if CONFIG_THREAD_GROUPS

	if (workSubmitWithContext(device, context, submitArgs) == false) {
		return false;
	}

	IOPerfControlWorkContext *work_context = OSDynamicCast(IOPerfControlWorkContext, context);

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(work_context->thread_group),
		.thread_group_data = thread_group_get_machine_data(work_context->thread_group),
		.work_data = &work_context->perfcontrol_data,
		.work_data_size = sizeof(work_context->perfcontrol_data),
		.started = true,
		.driver_state = &clientData.driverState
	};

	shared->interface.workBegin(device, work_context->id, &state, beginArgs);

	work_context->started = true;

	return true;
#else
	return false;
#endif
}

bool
IOPerfControlClient::workSubmitWithContext(IOService *device, OSObject *context, WorkSubmitArgs *args)
{
#if CONFIG_THREAD_GROUPS
	IOPerfControlWorkContext *work_context = OSDynamicCast(IOPerfControlWorkContext, context);

	if (work_context == nullptr) {
		return false;
	}

	auto *thread_group = thread_group_get(current_thread());
	assert(thread_group != nullptr);

	assertf(!work_context->started, "IOPerfControlWorkContext ID %llu was already started", work_context->id);
	assertf(work_context->thread_group == nullptr, "IOPerfControlWorkContext ID %llu has already taken a refcount on TG 0x%p \n", work_context->id, (void *)(work_context->thread_group));

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(thread_group),
		.thread_group_data = thread_group_get_machine_data(thread_group),
		.work_data = nullptr,
		.work_data_size = 0,
		.started = false,
		.driver_state = &clientData.driverState
	};
	if (!shared->interface.workCanSubmit(device, &state, args)) {
		return false;
	}

	work_context->thread_group = thread_group_retain(thread_group);
	if (clientData.driverState.resource_accounting) {
		auto *coalition = task_get_coalition(current_task(), COALITION_TYPE_RESOURCE);
		assert(coalition != nullptr);
		work_context->coal = coalition;
		coalition_retain(coalition);
	}

	state.work_data = &work_context->perfcontrol_data;
	state.work_data_size = sizeof(work_context->perfcontrol_data);

	shared->interface.workSubmit(device, work_context->id, &state, args);

	return true;
#else
	return false;
#endif
}

void
IOPerfControlClient::workUpdateWithContext(IOService *device, OSObject *context, WorkUpdateArgs *args)
{
#if CONFIG_THREAD_GROUPS
	IOPerfControlWorkContext *work_context = OSDynamicCast(IOPerfControlWorkContext, context);

	if (work_context == nullptr) {
		return;
	}

	if (work_context->thread_group == nullptr) {
		// This Work Context has not taken a refcount on a TG
		return;
	}

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(work_context->thread_group),
		.thread_group_data = thread_group_get_machine_data(work_context->thread_group),
		.work_data = &work_context->perfcontrol_data,
		.work_data_size = sizeof(work_context->perfcontrol_data),
		.driver_state = &clientData.driverState
	};
	shared->interface.workUpdate(device, work_context->id, &state, args);
#endif
}

void
IOPerfControlClient::workBeginWithContext(IOService *device, OSObject *context, WorkBeginArgs *args)
{
#if CONFIG_THREAD_GROUPS
	IOPerfControlWorkContext *work_context = OSDynamicCast(IOPerfControlWorkContext, context);

	if (work_context == nullptr) {
		return;
	}

	if (work_context->thread_group == nullptr) {
		// This Work Context has not taken a refcount on a TG
		return;
	}

	assertf(!work_context->started, "IOPerfControlWorkContext %llu was already started", work_context->id);

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(work_context->thread_group),
		.thread_group_data = thread_group_get_machine_data(work_context->thread_group),
		.work_data = &work_context->perfcontrol_data,
		.work_data_size = sizeof(work_context->perfcontrol_data),
		.started = true,
		.driver_state = &clientData.driverState
	};
	shared->interface.workBegin(device, work_context->id, &state, args);

	work_context->started = true;
#endif
}

void
IOPerfControlClient::workEndWithContext(IOService *device, OSObject *context, WorkEndArgs *args, bool done)
{
#if CONFIG_THREAD_GROUPS
	IOPerfControlWorkContext *work_context = OSDynamicCast(IOPerfControlWorkContext, context);

	if (work_context == nullptr) {
		return;
	}

	if (work_context->thread_group == nullptr) {
		return;
	}

	PerfControllerInterface::WorkState state{
		.thread_group_id = thread_group_get_id(work_context->thread_group),
		.thread_group_data = thread_group_get_machine_data(work_context->thread_group),
		.work_data = &work_context->perfcontrol_data,
		.work_data_size = sizeof(work_context->perfcontrol_data),
		.started = work_context->started,
		.driver_state = &clientData.driverState
	};

	if (shared->interface.version >= PERFCONTROL_INTERFACE_VERSION_4) {
		PerfControllerInterface::ResourceAccounting resources;
		shared->interface.workEndWithResources(device, work_context->id, &state, args, &resources, done);
		if (clientData.driverState.resource_accounting) {
			accountResources(work_context->coal, clientData.driverState.device_type, &resources);
		}
	} else {
		shared->interface.workEnd(device, work_context->id, &state, args, done);
	}

	if (done) {
		thread_group_release(work_context->thread_group);
		if (work_context->coal != nullptr) {
			coalition_release(work_context->coal);
		}
		work_context->reset();
	} else {
		work_context->started = false;
	}

	return;
#else
	return;
#endif
}

IOReturn
IOPerfControlClient::registerPerformanceController(PerfControllerInterface *pci)
{
	IOReturn result = kIOReturnError;

	IOLockLock(shared->interfaceLock);

	if (shared->interface.version == PERFCONTROL_INTERFACE_VERSION_NONE) {
		shared->interface.version = pci->version;

		if (pci->version >= PERFCONTROL_INTERFACE_VERSION_1) {
			assert(pci->registerDevice && pci->unregisterDevice && pci->workCanSubmit && pci->workSubmit && pci->workBegin && pci->workEnd);
			shared->interface.registerDevice = pci->registerDevice;
			shared->interface.unregisterDevice = pci->unregisterDevice;
			shared->interface.workCanSubmit = pci->workCanSubmit;
			shared->interface.workSubmit = pci->workSubmit;
			shared->interface.workBegin = pci->workBegin;
			shared->interface.workEnd = pci->workEnd;
		}

		if (pci->version >= PERFCONTROL_INTERFACE_VERSION_2) {
			if (pci->workUpdate != nullptr) {
				shared->interface.workUpdate = pci->workUpdate;
			}
		}

		if (pci->version >= PERFCONTROL_INTERFACE_VERSION_3) {
			assert(pci->registerDriverDevice && pci->unregisterDriverDevice);
			shared->interface.registerDriverDevice = pci->registerDriverDevice;
			shared->interface.unregisterDriverDevice = pci->unregisterDriverDevice;
		}

		if (pci->version >= PERFCONTROL_INTERFACE_VERSION_4) {
			assert(pci->workEndWithResources);
			shared->interface.workEndWithResources = pci->workEndWithResources;
		}

		result = kIOReturnSuccess;

		OSObject *obj;
		while ((obj = shared->deviceRegistrationList->getAnyObject())) {
			IOPerfControlClient *client = OSDynamicCast(IOPerfControlClient, obj);
			IOPerfControlClientData *clientData = client->getClientData();
			if (clientData && clientData->device) {
				if (pci->version >= PERFCONTROL_INTERFACE_VERSION_3) {
					pci->registerDriverDevice(clientData->device->getProvider(), clientData->device, &(clientData->driverState));
				} else if (pci->version >= PERFCONTROL_INTERFACE_VERSION_1) {
					pci->registerDevice(clientData->device);
				}
			}
			shared->deviceRegistrationList->removeObject(obj);
		}
	}

	IOLockUnlock(shared->interfaceLock);

	return result;
}
