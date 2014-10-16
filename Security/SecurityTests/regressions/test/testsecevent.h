/*
 * testsecevent.h
 */

#ifndef _TESTSECEVENT_H_
#define _TESTSECEVENT_H_  1
#include <Security/SecKeychain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define is_sec_event(EVENT, KEYCHAIN, ITEM, PID, TESTNAME) \
( \
	test_is_sec_event((EVENT), (KEYCHAIN), (ITEM), (PID), (TESTNAME), \
		test_directive, test_reason, __FILE__, __LINE__) \
)

#define no_sec_event(TESTNAME) \
( \
	test_is_sec_event(0, NULL, NULL, NULL, (TESTNAME), \
		test_directive, test_reason, __FILE__, __LINE__) \
)

OSStatus test_sec_event_register(SecKeychainEventMask mask);

OSStatus test_sec_event_deregister();

int test_is_sec_event(SecKeychainEvent event, SecKeychainRef *keychain,
	SecKeychainItemRef *item, pid_t *pid, const char *description,
	const char *directive, const char *reason, const char *file,
	unsigned line); 

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_TESTSECEVENT_H_ */
