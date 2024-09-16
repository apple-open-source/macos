/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
 */

#pragma once

#ifdef KERNEL_PRIVATE
#ifdef __cplusplus

#include <IOKit/IOService.h>
#include <stdatomic.h>
#include <kern/bits.h>
#include <libkern/c++/OSPtr.h>

struct thread_group;

enum{
	kIOPerfControlClientWorkUntracked = 0,
};

/*!
 * @class IOPerfControlClient : public OSObject
 * @abstract Class which implements an interface allowing device drivers to participate in performance control.
 * @discussion TODO
 */
class IOPerfControlClient final : public OSObject
{
	OSDeclareDefaultStructors(IOPerfControlClient);

protected:
	virtual bool init(IOService *driver, uint64_t maxWorkCapacity);
	virtual void free() APPLE_KEXT_OVERRIDE;

public:
/*!
 * @function copyClient
 * @abstract Return a retained reference to a client object, to be released by the driver. It may be
 * shared with other drivers in the system.
 * @param driver The device driver that will be using this interface.
 * @param maxWorkCapacity The maximum number of concurrent work items supported by the device driver.
 * @returns An instance of IOPerfControlClient.
 */
	static IOPerfControlClient *copyClient(IOService *driver, uint64_t maxWorkCapacity);

/*!
 * @function registerDevice
 * @abstract Inform the system that work will be dispatched to a device in the future.
 * @discussion The system will do some one-time setup work associated with the device, and may block the
 * current thread during the setup. Devices should not be passed to work workSubmit, workSubmitAndBegin,
 * workBegin, or workEnd until they have been successfully registered. The unregistration process happens
 * automatically when the device object is deallocated.
 * @param device The device object. Some platforms require device to be a specific subclass of IOService.
 * @returns kIOReturnSuccess or an IOReturn error code
 */
	virtual IOReturn registerDevice(IOService *driver, IOService *device);

/*!
 * @function unregisterDevice
 * @abstract Inform the system that work will be no longer be dispatched to a device in the future.
 * @discussion This call is optional as the unregistration process happens automatically when the device
 * object is deallocated. This call may block the current thread and/or acquire locks. It should not be
 * called until after all submitted work has been ended using workEnd.
 * @param device The device object. Some platforms require device to be a specific subclass of IOService.
 */
	virtual void unregisterDevice(IOService *driver, IOService *device);

/*!
 * @struct WorkSubmitArgs
 * @discussion Drivers may submit additional device-specific arguments related to the submission of a work item
 * by passing a struct with WorkSubmitArgs as its first member. Note: Drivers are responsible for publishing
 * a header file describing these arguments.
 */
	struct WorkSubmitArgs {
		uint32_t version;
		uint32_t size;
		uint64_t submit_time;
		uint64_t reserved[4];
		void *driver_data;
	};

/*!
 * @function workSubmit
 * @abstract Tell the performance controller that work was submitted.
 * @param device The device that will execute the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param args Optional device-specific arguments related to the submission of this work item.
 * @returns A token representing this work item, which must be passed to workEnd when the work is finished
 * unless the token equals kIOPerfControlClientWorkUntracked. Failure to do this will result in memory leaks
 * and a degradation of system performance.
 */
	virtual uint64_t workSubmit(IOService *device, WorkSubmitArgs *args = nullptr);

/*!
 * @struct WorkBeginArgs
 * @discussion Drivers may submit additional device-specific arguments related to the start of a work item
 * by passing a struct with WorkBeginArgs as its first member. Note: Drivers are responsible for publishing
 * a header file describing these arguments.
 */
	struct WorkBeginArgs {
		uint32_t version;
		uint32_t size;
		uint64_t begin_time;
		uint64_t reserved[4];
		void *driver_data;
	};

/*!
 * @function workSubmitAndBegin
 * @abstract Tell the performance controller that work was submitted and immediately began executing.
 * @param device The device that is executing the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param submitArgs Optional device-specific arguments related to the submission of this work item.
 * @param beginArgs Optional device-specific arguments related to the start of this work item.
 * @returns A token representing this work item, which must be passed to workEnd when the work is finished
 * unless the token equals kIOPerfControlClientWorkUntracked. Failure to do this will result in memory leaks
 * and a degradation of system performance.
 */
	virtual uint64_t workSubmitAndBegin(IOService *device, WorkSubmitArgs *submitArgs = nullptr,
	    WorkBeginArgs *beginArgs = nullptr);

/*!
 * @function workBegin
 * @abstract Tell the performance controller that previously submitted work began executing.
 * @param device The device that is executing the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param args Optional device-specific arguments related to the start of this work item.
 */
	virtual void workBegin(IOService *device, uint64_t token, WorkBeginArgs *args = nullptr);

/*!
 * @struct WorkEndArgs
 * @discussion Drivers may submit additional device-specific arguments related to the end of a work item
 * by passing a struct with WorkEndArgs as its first member. Note: Drivers are responsible for publishing
 * a header file describing these arguments.
 */
	struct WorkEndArgs {
		uint32_t version;
		uint32_t size;
		uint64_t end_time;
		uint64_t reserved[4];
		void *driver_data;
	};

/*!
 * @function workEnd
 * @abstract Tell the performance controller that previously started work finished executing.
 * @param device The device that executed the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param args Optional device-specific arguments related to the end of this work item.
 * @param done Optional Set to false if the work has not yet completed. Drivers are then responsible for
 * calling workBegin when the work resumes and workEnd with done set to True when it has completed. A workEnd() call
 * without a corresponding workBegin() call is a way to cancel a work item and return token to IOPerfControl.
 */
	virtual void workEnd(IOService *device, uint64_t token, WorkEndArgs *args = nullptr, bool done = true);

/*!
 * @function copyWorkContext
 * @abstract Return a retained reference to an opaque OSObject, to be released by the driver. This object can
 * be used by IOPerfControl to track a work item. This may perform dynamic memory allocation.
 * @returns A pointer to an OSObject
 */
	OSPtr<OSObject> copyWorkContext();

/*!
 * @function workSubmitAndBeginWithContext
 * @abstract Tell the performance controller that work was submitted and immediately began executing
 * @param device The device that is executing the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param context An OSObject returned by copyWorkContext(). The context object will be used by IOPerfControl to track
 * this work item.
 * @param submitArgs Optional device-specific arguments related to the submission of this work item.
 * @param beginArgs Optional device-specific arguments related to the start of this work item.
 * @returns true if IOPerfControl is tracking this work item, else false.
 * @note The workEndWithContext() call is optional if the corresponding workSubmitWithContext() call returned false.
 */
	bool workSubmitAndBeginWithContext(IOService *device, OSObject *context, WorkSubmitArgs *submitArgs = nullptr,
	    WorkBeginArgs *beginArgs = nullptr);

/*!
 * @function workSubmitWithContext
 * @abstract Tell the performance controller that work was submitted.
 * @param device The device that will execute the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param context An OSObject returned by copyWorkContext(). The context object will be used by IOPerfControl to track
 * this work item.
 * @param args Optional device-specific arguments related to the submission of this work item.
 * @returns true if IOPerfControl is tracking this work item, else false.
 */
	bool workSubmitWithContext(IOService *device, OSObject *context, WorkSubmitArgs *args = nullptr);

/*!
 * @function workBeginWithContext
 * @abstract Tell the performance controller that previously submitted work began executing.
 * @param device The device that is executing the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param context An OSObject returned by copyWorkContext() and provided to the previous call to workSubmitWithContext().
 * @param args Optional device-specific arguments related to the start of this work item.
 * @note The workBeginWithContext() and workEndWithContext() calls are optional if the corresponding workSubmitWithContext() call returned false.
 */
	void workBeginWithContext(IOService *device, OSObject *context, WorkBeginArgs *args = nullptr);

/*!
 * @function workEndWithContext
 * @abstract Tell the performance controller that previously started work finished executing.
 * @param device The device that executed the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param context An OSObject returned by copyWorkContext() and provided to the previous call to workSubmitWithContext().
 * @param args Optional device-specific arguments related to the end of this work item.
 * @param done Optional Set to false if the work has not yet completed. Drivers are then responsible for
 * calling workBegin when the work resumes and workEnd with done set to True when it has completed.
 * @note The workEndWithContext() call is optional if the corresponding workSubmitWithContext() call returned false. A workEndWithContext()
 * call without a corresponding workBeginWithContext() call is a way to cancel a work item.
 */
	void workEndWithContext(IOService *device, OSObject *context, WorkEndArgs *args = nullptr, bool done = true);

/*!
 * @struct WorkUpdateArgs
 * @discussion Drivers may submit additional device-specific arguments related to a work item by passing a
 * struct with WorkUpdateArgs as its first member. Note: Drivers are responsible for publishing
 * a header file describing these arguments.
 */
	struct WorkUpdateArgs {
		uint32_t version;
		uint32_t size;
		uint64_t update_time;
		uint64_t reserved[4];
		void *driver_data;
	};

/*!
 * @function workUpdateWithContext
 * @abstract Provide and receive additional information from the performance controller. If this call is
 * made at all, it should be between workSubmit and workEnd. The purpose and implementation of this call are
 * device specific, and may do nothing on some devices.
 * @param device The device that submitted the work. Some platforms require device to be a
 * specific subclass of IOService.
 * @param context An OSObject returned by copyWorkContext() and provided to the previous call to workSubmitWithContext().
 * @param args Optional device-specific arguments.
 */
	void workUpdateWithContext(IOService *device, OSObject *context, WorkUpdateArgs *args = nullptr);

/*
 * Callers should always use the CURRENT version so that the kernel can detect both older
 * and newer structure layouts. New callbacks should always be added at the end of the
 * structure, and xnu should expect existing source recompiled against newer headers
 * to pass NULL for unimplemented callbacks.
 */

#define PERFCONTROL_INTERFACE_VERSION_NONE (0) /* no interface */
#define PERFCONTROL_INTERFACE_VERSION_1 (1) /* up-to workEnd */
#define PERFCONTROL_INTERFACE_VERSION_2 (2) /* up-to workUpdate */
#define PERFCONTROL_INTERFACE_VERSION_3 (3) /* up-to (un)registerDriverDevice */
#define PERFCONTROL_INTERFACE_VERSION_4 (4) /* up-to workEndWithResources */
#define PERFCONTROL_INTERFACE_VERSION_CURRENT PERFCONTROL_INTERFACE_VERSION_4

/*!
 * @struct PerfControllerInterface
 * @discussion Function pointers necessary to register a performance controller. Not for general driver use.
 */
	struct PerfControllerInterface {
		enum struct PerfDeviceID : uint32_t{
			kInvalid = 0,
			kCPU = 0,
			kANE = 0x4,
			kGPU,
			kMSR,
			kStorage,
		};

		struct DriverState {
			uint32_t has_target_thread_group : 1;
			uint32_t has_device_info : 1;
			uint32_t reserved : 30;

			uint64_t target_thread_group_id;
			void *target_thread_group_data;

			PerfDeviceID device_type;
			uint32_t instance_id;
			bool resource_accounting;
		};

		struct WorkState {
			uint64_t thread_group_id;
			void *thread_group_data;
			void *work_data;
			uint32_t work_data_size;
			uint32_t started : 1;
			uint32_t reserved : 31;
			const DriverState* driver_state;
		};

		struct ResourceAccounting {
			uint64_t mach_time_delta;
			uint64_t energy_nj_delta;
		};

		using RegisterDeviceFunction = IOReturn (*)(IOService *);
		using RegisterDriverDeviceFunction = IOReturn (*)(IOService *, IOService *, DriverState *);
		using WorkCanSubmitFunction = bool (*)(IOService *, WorkState *, WorkSubmitArgs *);
		using WorkSubmitFunction = void (*)(IOService *, uint64_t, WorkState *, WorkSubmitArgs *);
		using WorkBeginFunction = void (*)(IOService *, uint64_t, WorkState *, WorkBeginArgs *);
		using WorkEndFunction = void (*)(IOService *, uint64_t, WorkState *, WorkEndArgs *, bool);
		using WorkEndWithResourcesFunction = void (*)(IOService *, uint64_t, WorkState *, WorkEndArgs *, ResourceAccounting *, bool);
		using WorkUpdateFunction = void (*)(IOService *, uint64_t, WorkState *, WorkUpdateArgs *);

		uint64_t version;
		RegisterDeviceFunction registerDevice;
		RegisterDeviceFunction unregisterDevice;
		WorkCanSubmitFunction workCanSubmit;
		WorkSubmitFunction workSubmit;
		WorkBeginFunction workBegin;
		WorkEndFunction workEnd;
		WorkUpdateFunction workUpdate;
		RegisterDriverDeviceFunction registerDriverDevice;
		RegisterDriverDeviceFunction unregisterDriverDevice;
		WorkEndWithResourcesFunction workEndWithResources;
	};

	struct IOPerfControlClientShared {
		atomic_uint_fast8_t maxDriverIndex;
		PerfControllerInterface interface;
		IOLock *interfaceLock;
		OSSet *deviceRegistrationList;
	};

	struct IOPerfControlClientData {
		struct thread_group *target_thread_group;
		PerfControllerInterface::DriverState driverState;
		IOService* device;
	};
/*!
 * @function registerPerformanceController
 * @abstract Register a performance controller to receive callbacks. Not for general driver use.
 * @param interface Struct containing callback functions implemented by the performance controller.
 * @returns kIOReturnSuccess or kIOReturnError if the interface was already registered.
 */
	virtual IOReturn registerPerformanceController(PerfControllerInterface *interface);

/*!
 * @function getClientData
 * @abstract Not for general driver use. Only used by registerPerformanceController(). Allows performanceController to register existing IOPerfControlClient.
 * @returns IOPerfControlData associated with a IOPerfControlClient
 */
	IOPerfControlClientData *
	getClientData()
	{
		return &clientData;
	}

private:
	struct WorkTableEntry {
		struct thread_group *thread_group;
		coalition_t coal;
		bool started;
		uint8_t perfcontrol_data[32];
	};

	static constexpr size_t kMaxWorkTableNumEntries = 1024;
	static constexpr size_t kWorkTableIndexBits = 24;
	static constexpr size_t kWorkTableMaxSize = (1 << kWorkTableIndexBits) - 1; // - 1 since
	// kIOPerfControlClientWorkUntracked takes number 0
	static constexpr size_t kWorkTableIndexMask = (const size_t)mask(kWorkTableIndexBits);

	uint64_t allocateToken(thread_group *thread_group);
	void deallocateToken(uint64_t token);
	WorkTableEntry *getEntryForToken(uint64_t token);
	void markEntryStarted(uint64_t token, bool started);
	inline uint64_t tokenToGlobalUniqueToken(uint64_t token);
	void accountResources(coalition_t coal, PerfControllerInterface::PerfDeviceID device_type, PerfControllerInterface::ResourceAccounting *resources);

	uint8_t driverIndex;
	IOPerfControlClientShared *shared;
	WorkTableEntry *workTable;
	size_t workTableLength;
	size_t workTableNextIndex;
	IOSimpleLock *workTableLock;

	IOPerfControlClientData clientData;
};

#endif /* __cplusplus */
#endif /* KERNEL_PRIVATE */
