#ifndef __LINKED_LIST__
#define __LINKED_LIST__


class LinkedListHeader
{
protected:
	LinkedListHeader *mBackward;
	LinkedListHeader *mForward;
	
public:
	LinkedListHeader();
	virtual ~LinkedListHeader();
	
	void InsertAfter(LinkedListHeader* header);
	LinkedListHeader* Forward() {return mForward;}
	LinkedListHeader* Backward() {return mBackward;}
};



#endif
