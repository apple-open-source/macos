#include "LinkedList.h"



LinkedListHeader::LinkedListHeader()
{
	mBackward = mForward = this;
}



LinkedListHeader::~LinkedListHeader()
{
	// unlink the item
	mBackward->mForward = mForward;
	mForward->mBackward = mBackward;
}



void LinkedListHeader::InsertAfter(LinkedListHeader* header)
{
	// add to the chain
	header->mForward = mForward;
	mForward->mBackward = header;
	header->mBackward = this;
	mForward = header;
}

