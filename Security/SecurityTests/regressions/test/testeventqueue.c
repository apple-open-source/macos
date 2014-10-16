#include <stdlib.h>
#include <string.h>

#include "testeventqueue.h"



struct CallbackDataQueueElement;
typedef struct CallbackDataQueueElement CallbackDataQueueElement;

struct CallbackDataQueueElement
{
	CallbackData callbackData;
	CallbackDataQueueElement *forward;
	CallbackDataQueueElement *back;
};

// allocate static storage for the queue header, which is a circularly linked list
static CallbackDataQueueElement gCallbackQueue = {{0, 0, NULL, NULL, 0}, &gCallbackQueue, &gCallbackQueue};
static int gNumItemsInQueue = 0;


void TEQ_Enqueue (CallbackData *cd)
{
	// allocate storage for the queue element and copy it.
	CallbackDataQueueElement* element = (CallbackDataQueueElement*) malloc (sizeof (CallbackDataQueueElement));
	memcpy (&element->callbackData, cd, sizeof (CallbackData));
	
	// enqueue the new element -- always at the end
	CallbackDataQueueElement* tail = gCallbackQueue.back;
	element->forward = tail->forward;
	element->forward->back = element;
	element->back = tail;
	tail->forward = element;
	
	gNumItemsInQueue += 1;
}



bool TEQ_Dequeue (CallbackData *cd)
{
	if (TEQ_ItemsInQueue () == 0)
	{
		return false;
	}
	
	// pull the element out of the queue and copy the data
	CallbackDataQueueElement* element = gCallbackQueue.forward;
	element->forward->back = element->back;
	element->back->forward = element->forward;
	memcpy (cd, &element->callbackData, sizeof (CallbackData));
	
	free (element);
	
	gNumItemsInQueue -= 1;
	return true;
}



int TEQ_ItemsInQueue ()
{
	return gNumItemsInQueue;
}



void TEQ_FlushQueue ()
{
	CallbackDataQueueElement* element = gCallbackQueue.forward;
	while (element != &gCallbackQueue)
	{
		CallbackDataQueueElement* forward = element->forward;
		free (element);
		element = forward;
	}
	
	gNumItemsInQueue = 0;
}



void TEQ_Release (CallbackData *cd)
{
	if (cd->itemRef != NULL)
	{
		CFRelease (cd->itemRef);
	}
	
	if (cd->keychain != NULL)
	{
		CFRelease (cd->keychain);
	}
}
