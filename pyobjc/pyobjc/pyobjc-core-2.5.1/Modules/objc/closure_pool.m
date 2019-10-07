/*
 * A simple allocator for closure. This assumes that most closures are kept
 * alive forever and we therefore don't have to return storage to the OS.
 */
#include "pyobjc.h"

#include <stdlib.h>
#include <sys/queue.h>
#include <os/lock.h>

SLIST_HEAD(closurelist, closureelem);

struct closurelist freelist = SLIST_HEAD_INITIALIZER(freelist);
struct closurelist usedlist = SLIST_HEAD_INITIALIZER(usedlist);
os_unfair_lock listlock;

typedef struct closureelem {
	ffi_closure_wrapper wrapper;
	SLIST_ENTRY(closureelem) entries;
} closureelem;

ffi_closure_wrapper*
PyObjC_malloc_closure(void)
{
	closureelem* entry;
	os_unfair_lock_lock(&listlock);
	entry = SLIST_FIRST(&freelist);
	if (entry)
	{
		SLIST_REMOVE_HEAD(&freelist, entries);
	}
	else
	{
		entry = calloc(1, sizeof(*entry));
		entry->wrapper.closure = ffi_closure_alloc(sizeof(ffi_closure), &entry->wrapper.code_addr);
	}
	SLIST_INSERT_HEAD(&usedlist, entry, entries);
	os_unfair_lock_unlock(&listlock);
	return &entry->wrapper;
}

int
PyObjC_free_closure(ffi_closure_wrapper* cl)
{
	if (cl)
	{
		closureelem *entry = (closureelem *)cl;
		os_unfair_lock_lock(&listlock);
		SLIST_REMOVE(&usedlist, entry, closureelem, entries);
		SLIST_INSERT_HEAD(&freelist, entry, entries);
		os_unfair_lock_unlock(&listlock);
	}
	return 0;
}

ffi_closure_wrapper*
PyObjC_closure_from_code(void* code)
{
	ffi_closure_wrapper* result = NULL;
	if (code)
	{
		closureelem *entry;
		os_unfair_lock_lock(&listlock);
		SLIST_FOREACH(entry, &usedlist, entries)
		if (entry->wrapper.code_addr == code)
			result = &entry->wrapper;
		os_unfair_lock_unlock(&listlock);
	}
	return result;
}
