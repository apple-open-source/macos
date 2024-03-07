# Provide Futex style APIs on Darwin

* Proposal: libplatform-0002
* Author(s): Priya Avhad <pavhad@apple.com>
* Reviewed by: darwin-runtime team <darwin-runtime@group.apple.com>
* Status: **Review**
* Intended Release: DawnburstE

##### Related radars

* rdar://94759935 (SEED: Web: Consider classifying __ulock API family as public)
* rdar://109129195 (j475d: Cyberpunk 2077 poor performance, averaging 17 FPS in the benchmark due to negative scaling).
* Unity rdar://79681283 (Support for API similar to WaitOnAddress/WakeByAddress)
* rdar://89422129 (Unity feature request: API for ulock)
* rdar://105475404 (SEED: Provide official C/C++ API for low-level futex functionality)
* rdar://94840444 (Intel TBB should adopt ulock_wake/ulock_wait for waiting/waking threads)
* rdar://58432500 (Publicize ulock API?)

##### Revision history

* **v1** Initial version

## Introduction

There has historically been a lot of interest in having futex-style APIs on Darwin for developers to use them as building blocks for constructing their own high level synchronization primitives. Game developers for Windows who want to port their games to Darwin are among the interested parties.
For others, this ask is also largely motivated by the need to preserve platform consistency, same userspace implementation, and thereby achieve predictable behaviour and results. Developers can use the new APIs to implement synchronization primitives catered to their unique needs e.g. size or not so commonly found usecases. We dive into some interesting case studies in the following motivation section.

## Motivation

### WINE
A lot of Windows based third party games use game porting toolkit. This toolkit includes WINE which is a compatibility layer that makes it possible to run Windows apps on Unix like OS. WINE emulates Window's synchronization primitives using the primitives provided by the underlying OS. Recently, a lot of external outlets have reported about the poor performance of these gaming apps on macOS when scaling the number of cores e.g. M2 Max → M2 Ultra. The performance investigation revealed WINE's emulation of Window's synchronization primitives for macOS to be the bottleneck.

#### Case Study : CyberPunk

Here is a case study of CyberPunk to explain those bottlenecks : rdar://109129195 (j475d: Cyberpunk 2077 poor performance, averaging 17 FPS in the benchmark due to negative scaling).

CyberPunk creates a thread pool of size equal to the number of cpu cores. Their thread usage model involves worker threads parked in the pool waiting for incoming work using NtWaitForMultipleObjects; but with a single object passed which is a global semaphore. When a new work is added to the pool, a thread issues ReleaseSemaphore(global_semaphore, 1) for each worker thread it thinks is waiting. Their implementation
also requires an ability to synchronize across processes.

WINE emulates these primitives in following modes on macOS -

* **Regular** :
	- Serialize all synchronization traffic to external wine server process.
	- This mode is inherently not very scalable due to orchestration by a single threaded external wine server.
* **ESYNC** :
	- Implement the synchronization primitives in process by using pipe, read/poll/write.
	- In this mode, [a wait on a semaphore is implemented using read/poll on a pipe](https://github.com/GloriousEggroll/proton-wine/blob/proton_7.0/dlls/ntdll/unix/esync.c#L801) and [release on a semaphore is implemented by writing to a pipe](https://github.com/GloriousEggroll/proton-wine/blob/proton_7.0/dlls/ntdll/unix/esync.c#L387).
	- With CyperPunk's thread usage model, WINEESYNC has been particularly seen to scale very poorly with increasing number of cores.
	- Writing 1 byte to a pipe wakes up all potential workers doing poll() to fight to read that 1 byte i.e. to grab that one work block just submitted. Additionally, our pipe implementation on macOS involves taking pipe's internal mutex several times during an operation. Therefore, with a larger worker thread pool size, a significant time is spent contending on this lock.
* **FSYNC** :
	- Implements the synchronization primitives by using futex style APIs and shared memory. 
	- We prototyped FSYNC on Darwin using __ulock_wait2/__ulock_wake SPIs which are futex style primitives.
	- CyberPunk workload on macOS shows this mode performs significantly better as opposed to other two modes specially when scaling the number of cores. See perf data at : https://quip-apple.com/t1k9A5FnEnmt

### NATIVE

Many native gaming apps (including Unity) use a set of synchronization primitives across their engine and have indicated a need for futex-style APIs on Darwin to implement such primitives on Apple devices. Some of these use cases call for certain size specifications or the support of shared memory primitives. Due to the lack of these APIs today, they are forced to apply various workarounds on Darwin, which can be sub-optimal in terms of performance compared to the platforms.

## Proposed Design

The design proposes Futex style APIs on Darwin in the form of syscall wrappers around existing futex alike kernel functionality that will reside in libplatform.

## Proposed API

### C APIs

```C
/*!
 * @function os_sync_wait_on_address
 *
 * This function provides an atomic compare-and-wait functionality that
 * can be used to implement other higher level synchronization primitives.
 *
 * It reads a value from @addr, compares it to expected @value and blocks
 * the calling thread if they are equal. This sequence of operations is
 * done atomically with respect to other concurrent operations that can
 * be performed on this @addr by other threads using this same function
 * or os_sync_wake_by_addr variants. At this point, the blocked calling
 * thread is considered to be a waiter on this @addr, waiting to be woken
 * up by a call to os_sync_wake_by_addr variants. If the value at @addr
 * turns out to be different than expected, the calling thread returns
 * immediately without blocking.
 *
 * This function is expected to be used for implementing synchronization
 * primitives that do not have a sense of ownership (e.g. condition
 * variables, semaphores) as it does not provide priority inversion avoidance.
 * For locking primitives, it is recommended that you use existing OS
 * primitives such as os_unfair_lock API family / pthread mutex or
 * std::mutex.
 *
 * @param addr
 * The userspace address to be used for atomic compare-and-wait.
 * This address must be aligned to @size.
 *
 * @param value
 * The value expected at @addr.
 *
 * @param size
 * The size of @value, in bytes. This can be either 4 or 8 today.
 * For 4-bytes @value, the @addr should to be aligned to a 4-byte boundary.
 *
 * @param flags
 * Flags to alter behavior of os_sync_wait_on_address.
 * See os_sync_wait_on_address_flags_t.
 *
 * @return
 * If the calling thread is woken up by a call to os_sync_wake_by_addr
 * variants or the value at @addr is different than expected, this function
 * returns successfully and the return value indicates the number
 * of outstanding waiters blocked on this address.
 * In the event of an error,
 * returns -1 with errno set to indicate the error.
 *
 * EINVAL	:	Invalid flags or size.
 * EINVAL	:	The @addr passed is NULL or misaligned.
 * EINVAL	:	The operation associated with existing kernel state
 *				at this @addr is inconsistent with what the caller
 *				has requested.
 *				It is important to make sure consistent values are
 *				passed across wait and wake APIs for @addr, @size
 *				and the shared memory specification
 *				(See os_sync_wait_on_address_flags_t).
 * ENOMEM	:	Unable to allocate memory for kernel internal data
 *				structures.
 * EFAULT	:	Unable to read value from the @addr.
 *				Kernel copyin failed. It is important to make sure
 *				@addr is valid and re-try.
 * EINTR	:	The syscall was interrupted / spurious wake up.
 */
int
os_sync_wait_on_address(void *addr,
						uint64_t value,
						size_t size,
						os_sync_wait_on_address_flags_t flags);

/*!
 * @function os_sync_wait_on_address_with_deadline
 *
 * This function is a variant of os_sync_wait_on_address that
 * allows the calling thread to specify a deadline
 * until which it is willing to block.
 *
 * @param addr
 * The userspace address to be used for atomic compare-and-wait.
 * This address must be aligned to @size.
 *
 * @param value
 * The value expected at @addr.
 *
 * @param size
 * The size of @value, in bytes. This can be either 4 or 8 today.
 * For 4-bytes @value, the @addr should to be aligned to a 4-byte boundary.
 *
 * @param flags
 * Flags to alter behavior of os_sync_wait_on_address_with_deadline.
 * See os_sync_wait_on_address_flags_t.
 *
 * @param clockid
 * This value anchors @deadline argument to a specific clock id.
 * See os_clockid_t.
 *
 * @param deadline
 * This value is used to specify a deadline until which the calling
 * thread is willing to block.
 * Passing zero for the @deadline results in an error being returned.
 * It is recommended to use os_sync_wait_on_address API to block
 * indefinitely until woken up by a call to os_sync_wake_by_address_any
 * or os_sync_wake_by_address_all APIs.
 *
 * @return
 * If the calling thread is woken up by a call to os_sync_wake_by_addr
 * variants or the value at @addr is different than expected, this function
 * returns successfully and the return value indicates the number
 * of outstanding waiters blocked on this address.
 * In the event of an error,
 * returns -1 with errno set to indicate the error.
 *
 * In addition to errors returned by os_sync_wait_on_address, this function
 * can return the following additional error codes.
 *
 * EINVAL		:	Invalid clock id.
 * EINVAL		:	The @deadline passed is 0.
 * ETIMEDOUT	:	Deadline expired.
 */
int
os_sync_wait_on_address_with_deadline(void *addr,
									uint64_t value,
									size_t size,
									os_sync_wait_on_address_flags_t flags,
									os_clockid_t clockid,
									uint64_t deadline);

/*!
 * @function os_sync_wait_on_address_with_timeout
 *
 * This function is a variant of os_sync_wait_on_address that
 * allows the calling thread to specify a timeout
 * until which it is willing to block.
 *
 * @param addr
 * The userspace address to be used for atomic compare-and-wait.
 * This address must be aligned to @size.
 *
 * @param value
 * The value expected at @addr.
 *
 * @param size
 * The size of @value, in bytes. This can be either 4 or 8 today.
 * For 4-bytes @value, the @addr should to be aligned to a 4-byte boundary.
 *
 * @param flags
 * Flags to alter behavior of os_sync_wait_on_address_with_timeout.
 * See os_sync_wait_on_address_flags_t.
 *
 * @param clockid
 * This value anchors @timeout_ns argument to a specific clock id.
 * See os_clockid_t.
 *
 * @param timeout_ns
 * This value is used to specify a timeout in nanoseconds until which
 * the calling thread is willing to block.
 * Passing zero for the @timeout_ns results in an error being returned.
 * It is recommended to use os_sync_wait_on_address API to block
 * indefinitely until woken up by a call to os_sync_wake_by_address_any
 * or os_sync_wake_by_address_all APIs.
 *
 * @return
 * If the calling thread is woken up by a call to os_sync_wake_by_address
 * variants or the value at @addr is different than expected, this function
 * returns successfully and the return value indicates the number
 * of outstanding waiters blocked on this address.
 * In the event of an error,
 * returns -1 with errno set to indicate the error.
 *
 * In addition to errors returned by os_sync_wait_on_address, this function
 * can return the following additional error codes.
 *
 * EINVAL		:	Invalid clock id.
 * EINVAL		:	The @timeout_ns passed is 0.
 * ETIMEDOUT	:	Timeout expired.
 */
int
os_sync_wait_on_address_with_timeout(void *addr,
									uint64_t value,
									size_t size,
									os_sync_wait_on_address_flags_t flags,
									os_clockid_t clockid,
									uint64_t timeout_ns);

/*!
 * @typedef os_sync_wait_on_address_flags_t
 *
 * @const OS_SYNC_WAIT_ON_ADDRESS_SHARED
 * This flag informs the kernel that the address passed to os_sync_wait_on_address
 * and its variants is expected to be placed in a shared memory region.
 */
OS_OPTIONS(os_sync_wait_on_address_flags, uint32_t,
	OS_SYNC_WAIT_ON_ADDRESS_NONE,
	OS_SYNC_WAIT_ON_ADDRESS_SHARED,
);

/*!
 * @function os_sync_wake_by_address_any
 *
 * This function wake up one waiter out of all those blocked in os_sync_wait_on_address
 * or its variants on the @addr. No guarantee is provided about which
 * specific waiter is woken up e.g. highest priority waiter.
 *
 * @param addr
 * The userspace address to be used for waking up the blocked waiter.
 * It should be same as what is passed to os_sync_wait_on_address or its variants.
 *
 * @param size
 * The size of lock value, in bytes. This can be either 4 or 8 today.
 * It should be same as what is passed to os_sync_wait_on_address or its variants.
 *
 * @param flags
 * Flags to alter behavior of os_sync_wake_by_address_any.
 * See os_sync_wake_by_address_flags_t.
 *
 * @return
 * Returns 0 on success.
 * In the event of an error,
 * returns -1 with errno set to indicate the error.
 *
 * EINVAL	:	Invalid flags or size.
 * EINVAL	:	The @addr passed is NULL.
 * EINVAL	:	The operation associated with existing kernel state
 *				at this @addr is inconsistent with what caller
 *				has requested.
 *				It is important to make sure consistent values are
 *				passed across wait and wake APIs for @addr, @size
 *				and the shared memory specification
 *				(See os_sync_wake_by_address_flags_t).
 * ENOENT	:	Unable to find a kernel internal data structure
 *				for the @addr. It is important to make sure there
 *				exist waiter(s) on this @addr using os_sync_wait_on_address
 *				API variants.
 */
int
os_sync_wake_by_address_any(void *addr,
						size_t size,
						os_sync_wake_by_address_flags_t flags)

/*!
 * @function os_sync_wake_by_address_all
 *
 * This function is a variant of os_sync_wake_by_address_any that wakes up all waiters
 * blocked in os_sync_wait_on_address or its variants.
 *
 * @param addr
 * The userspace address to be used for waking up the blocked waiters.
 * It should be same as what is passed to os_sync_wait_on_address or its variants.
 *
 * @param size
 * The size of lock value, in bytes. This can be either 4 or 8 today.
 * It should be same as what is passed to os_sync_wait_on_address or its variants.
 *
 * @param flags
 * Flags to alter behavior of os_sync_wake_by_address_all.
 * See os_sync_wake_by_address_flags_t.
 *
 * @return
 * Returns 0 on success.
 * In the event of an error,
 * returns -1 with errno set to indicate the error.
 *
 * This function returns same error codes as returned by os_sync_wait_on_address.
 */
int
os_sync_wake_by_address_all(void *addr,
						size_t size,
						os_sync_wake_by_address_flags_t flags)

/*!
 * @typedef os_sync_wake_by_address_flags_t
 *
 * @const OS_SYNC_WAKE_BY_ADDRESS_SHARED
 * This flag informs the kernel that the address passed to os_sync_wake_by_address_any
 *  or its variants is expected to be placed in a shared memory region.
 */
OS_OPTIONS(os_sync_wake_by_address_flags, uint32_t,
	OS_SYNC_WAKE_BY_ADDRESS_NONE,
	OS_SYNC_WAKE_BY_ADDRESS_SHARED,
);
```

## Locking based primitives

For implementing locks or locking based primitives with a sense of ownership, we strongly encourage our developers to use existing OS primitives such as os_unfair_lock API family or pthread_mutex or std::mutex. To restate, the new os_sync_wait_on_address and os_sync_wake_by_addr APIs do not provide priority inversion
avoidance and should only be used for implementing synchronization primitives that do not have a sense of ownership such as condition variables, semaphores.

### Future Scope
There are a few improvements that can be done to existing locking based OS primitives that are worth mentioning here; but, are out of scope for this proposal.

- Expose OS_UNFAIR_LOCK_ADAPTIVE_SPIN SPI flag. (rdar://93994704)
- Expose os_unfair_recursive_lock SPIs.

## SWIFT APIs

Swift APIs will be provided as an extension of Swift's low level atomics operations proposal described at https://gist.github.com/Azoy/89167e0ab3990ab1ed22d2aa8e628295. We are working with engineers from Swift Standard Library to flesh out the details.

## Future Direction

This API surface can be futher extended by adding following incremental functionalities.

1. Ability to wake up a specific thread  
This functionality will provide an ability to wake up a specific waiter blocked on an address and whose identity will need be specified in some pre-defined format.

2. Ability to wake up a specific number of threads  

3. Ability to work with two addresses at the same time e.g. for condition variables.  
Thundering hurd wake-ups, similar to condition variables, can cause waiters from one address to be woken up at the same time; however, all waiters except one immediately block on the second address. To avoid this, a new functionality that wakes up a single waiter and reenqueues the remaining ones into waitq for the second address can be introduced.

