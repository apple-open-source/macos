#ifndef __TEST_EVENT_QUEUE
#define __TEST_EVENT_QUEUE



#include <Security/Security.h>

typedef struct CallbackData
{
	UInt32 version;
	SecKeychainEvent event;
	SecKeychainItemRef itemRef;
	SecKeychainRef keychain;
	pid_t pid;
} CallbackData;



void TEQ_Enqueue(CallbackData* cd);
bool TEQ_Dequeue(CallbackData* cd);
void TEQ_FlushQueue();
int TEQ_ItemsInQueue();
void TEQ_Release(CallbackData* cd);

#endif
