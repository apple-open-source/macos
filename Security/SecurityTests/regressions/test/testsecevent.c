#include "testsecevent.h"

#include <assert.h>
#include <CoreFoundation/CFRunLoop.h>
#include <sys/time.h>

#include "testmore.h"
#include "testeventqueue.h"

static OSStatus test_sec_event_callback(SecKeychainEvent keychainEvent,
        SecKeychainCallbackInfo *info, void *inContext)
{
	if (keychainEvent == kSecLockEvent || keychainEvent == kSecUnlockEvent)
	{
		return 0;
	}
	
	CallbackData cd;
	cd.event = keychainEvent;
	cd.version = info->version;
	cd.itemRef = info->item;
	cd.keychain = info->keychain;
	cd.pid = info->pid;
	
	if (cd.itemRef)
	{
		CFRetain (cd.itemRef);
	}
	
	if (cd.keychain)
	{
		CFRetain (cd.keychain);
	}
	
	
	TEQ_Enqueue (&cd);

	return 0;
}

OSStatus test_sec_event_register(SecKeychainEventMask mask)
{
	return SecKeychainAddCallback(test_sec_event_callback, mask, NULL);
}

OSStatus test_sec_event_deregister()
{
	return SecKeychainRemoveCallback(test_sec_event_callback);
}

double GetCurrentTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	double d = tv.tv_sec + tv.tv_usec / 1000000.0;
	return d;
}



/* Get the next keychain event, and optionally return the events
   keychain, item and pid.  */
int test_is_sec_event(SecKeychainEvent event, SecKeychainRef *keychain,
	SecKeychainItemRef *item, pid_t *pid, const char *description,
	const char *directive, const char *reason, const char *file, unsigned line)
{
	int expected = event == 0 ? 0 : 1;

	if (event == 0) // looking for no event?
	{
		if (TEQ_ItemsInQueue() == 0)
		{
				return test_ok(TRUE,
					    description, directive, reason, file, line,
					    "",	0, 0);
		}
	}
	
	
	double startTime = GetCurrentTime();
	double nextTime = startTime + 2.0;
	double currentTime;

	while ((currentTime = GetCurrentTime()) < nextTime)
	{
		/* Run the runloop until we get an event.  Don't hang for
		   more than 0.15 seconds though. */
		SInt32 result = kCFRunLoopRunHandledSource;
		
		if (TEQ_ItemsInQueue () == 0) // are there events left over?
			result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE);
		
		switch (result)
		{
		case kCFRunLoopRunFinished:
			return test_ok(0, description, directive, reason, file, line,
				"# no sources registered in runloop\n");
		case kCFRunLoopRunStopped:
			return test_ok(0, description, directive, reason, file, line,
				"# runloop was stopped\n");
		case kCFRunLoopRunTimedOut:
			continue;

		case kCFRunLoopRunHandledSource:
		{
			CallbackData cd;
			bool dataInQueue;
			
			if (expected)
			{
				dataInQueue = TEQ_Dequeue (&cd);
				if (dataInQueue)
				{
				    if (keychain != NULL)
					*keychain = cd.keychain;
				    else if (cd.keychain) 
					CFRelease(cd.keychain);

				    if (item != NULL)
					*item = cd.itemRef;
				    else if (cd.itemRef)
					CFRelease(cd.itemRef);
				
				    return test_ok(cd.event == event,
					    description, directive, reason, file, line,
					    "#          got: '%d'\n"
					    "#     expected: '%d'\n",
					    cd.event, event);
				}
				else
				{
					
					// oops, we didn't get an event, even though we were looking for one.
					return test_ok(0, description, directive, reason, file, line,
								   "#          event expected but not received\n");
				}
			}
			
			/* We didn't expect anything and we got one event or more. Report them */
			dataInQueue = TEQ_Dequeue (&cd);
			int unexpected_events = 0;
			while (dataInQueue)
			{
				test_diag(directive, reason, file, line, 
					"         got unexpected event: '%d'", cd.event);
				unexpected_events++;
				TEQ_Release (&cd);
				dataInQueue = TEQ_Dequeue (&cd);
			}
			return test_ok(unexpected_events == 0, description, directive, reason, file, line,
					"#          got %d unexpected events\n", unexpected_events);
		}

		default:
			return test_ok(0, description, directive, reason, file, line,
				"# runloop returned: '%d'\n"
				"#         expected: 'kCFRunLoopRunHandledSource'\n",
				result);
		}
	}

	if (expected)
		return test_ok(0, description, directive, reason,
			file, line, "# runloop timed out waiting for event : %d\n",
			event);
	else
		return test_ok(TRUE, description, directive, reason, file, line,
					"#          got %d unexpected events\n", 0);

}
