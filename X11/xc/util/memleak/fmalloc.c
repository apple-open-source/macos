/*
 * $Xorg: fmalloc.c,v 1.5 2001/02/09 02:06:19 xorgcvs Exp $
 *
Copyright 1992, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

/* $XFree86: xc/util/memleak/fmalloc.c,v 3.14 2002/04/04 14:06:00 eich Exp $ */


/*
 * Leak tracing allocator -- using C lib malloc/free, tracks
 * all allocations.  When requested, performs a garbage-collection
 * style mark/sweep on static memory (data and stack), locating
 * objects referenced therein.  Recursively marks objects.
 * Sweeps through all allocations, warning of possible violations
 * (unreferenced allocated, referenced freed etc).
 */

#include    <stdio.h>

extern char **environ;
extern xf86DriverList;
extern etext;
extern _etext;
extern __data_start;
extern _end;

#ifndef TOP_OF_DATA
#define TOP_OF_DATA 0
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifdef X_NOT_POSIX
#define NO_ATEXIT
#endif

typedef unsigned long mem;
typedef unsigned int  magic;

#ifdef HAS_GET_RETURN_ADDRESS
#define MAX_RETURN_STACK    16
#endif

#define MAX_FREED_MEMORY    (1*1024*1024)

#define ACTIVE_HEAD_MAGIC   0xff1111ff
#define ACTIVE_TAIL_MAGIC   0xee2222ee
#define ACTIVE_DATA_MAGIC   0xdd3333dd
#define FREED_HEAD_MAGIC    0xcc4444cc
#define FREED_TAIL_MAGIC    0xbb5555bb
#define FREED_DATA_MAGIC    0xcc6666cc

/*
 * the marked fields in each head have two bits - one indicating
 * references to the head of the block, and one indicating references
 * to the middle of the block
 */
#define UNREFERENCED	    0
#define REFERENCED_HEAD	    1
#define REFERENCED_MIDDLE   2

typedef struct _head {
    struct _head    *left, *right;
    struct _head    *next;
    int		    balance;
#ifdef HAS_GET_RETURN_ADDRESS
    mem		    returnStack[MAX_RETURN_STACK];
#endif
    mem		    *from;
    mem             *fromReturnStack;
    unsigned long   allocTime;
    unsigned long   freeTime;
    int		    size;
    int		    desiredsize;
    int		    actualSize;
    int		    marked;
    magic    	    headMagic;
} HeadRec, *HeadPtr;

typedef struct _tail {
    magic    	    tailMagic;
    magic    	    tailPad;
} TailRec, *TailPtr;

#define Header(p)	((HeadPtr) (((char *) (p)) - sizeof (HeadRec)))
#define DataForHead(h)	((mem *) ((h) + 1))
#define Tailer(p)	((TailPtr) (((char *) (p)) + Header(p)->size))
#define TailForHead(h)	(Tailer(DataForHead(h)))
#define RoundSize	(sizeof (mem))
#define RoundUp(s)	(((s) + RoundSize - 1) & ~(RoundSize - 1))
#define TotalSize(s)	((s) + sizeof (HeadRec) + sizeof (TailRec))
#define CheckInit()	if (!endOfStaticMemory) endOfStaticMemory = sbrk(0)
#define BlockContains(h,p)  (DataForHead(h) <= (p) && (p) < (mem *) TailForHead(h))

typedef HeadRec		tree;
typedef mem		*tree_data;

#define COMPARE_ADDR(a,b,op)	(((mem) (a)) op ((mem) (b)))
#define COMPARE(a,b,op,s)	((!s) ? \
			 COMPARE_ADDR(a,b,op) :\
			(((a)->actualSize op (b)->actualSize) || \
			 ((a)->actualSize == (b)->actualSize && \
			  COMPARE_ADDR(a,b,op))))
				

#define LESS_THAN(a,b,s)    COMPARE(a,b,<,s)
#define GREATER_THAN(a,b,s) COMPARE(a,b,>,s)

#define SEARCH(top,result,p) for (result = top; result;) {\
    if ((mem *) (p) < DataForHead(result)) \
	result = result->left; \
    else if ((mem *) TailForHead(result) < (mem *) (p)) \
	result = result->right; \
    else \
	break; \
}
    

static tree	*activeMemory, *freedMemory, *deadMemory;

static mem	*endOfStaticMemory = (mem *) TOP_OF_DATA;
static mem	*highestAllocatedMemory;

static int	freedMemoryTotal;
static int	freedMemoryCount;
static int	activeMemoryTotal;
static int	activeMemoryCount;
static int	deadMemoryTotal;
static int	unreferencedAllocatedTotal;
static int	unreferencedAllocatedCount;

int		FindLeakWarnMiddlePointers = 0;
unsigned long	FindLeakAllocBreakpoint = ~0;
unsigned long	FindLeakFreeBreakpoint = ~0;
unsigned long	FindLeakTime;
int		FindLeakCheckAlways = 0;
int		FindLeakValidateAlways = 0;
int		FindPrintAllocations = 0;

static void MarkActiveBlock ();
static int  tree_insert (), tree_delete ();
void	    CheckMemory ();
char	    *malloc (), *realloc (), *calloc ();
void	    free ();
extern char *sbrk ();

#ifdef HAS_GET_RETURN_ADDRESS
static void
PrintReturnStack (m, ra)
    char    *m;
    mem	    *ra;
{
    int	    i;

    fprintf (stderr, "   %s:", m);
    for (i = 0; i < MAX_RETURN_STACK && ra[i]; i++)
	fprintf (stderr, " 0x%lx", ra[i]);
    fprintf (stderr, "\n");
}
#endif

static void
MemError (s, h, ourRet)
    char	*s;
    HeadPtr	h;
    int		ourRet;
{
    mem *ra;
    int	i;

    if (h)
    {
	fprintf (stderr, "%s 0x%08lx (size %d) (from 0x%lx) ",
	     s, DataForHead(h), h->desiredsize, h->from);
#ifdef HAS_GET_RETURN_ADDRESS
	if (h->fromReturnStack)
	    PrintReturnStack ("\nallocated at", h->fromReturnStack);
	else
	  fprintf(stderr,"\n");
	PrintReturnStack ("Saved return stack", h->returnStack);
#else
	fprintf(stderr,"\n");
#endif
    }
    else 
	fprintf (stderr, "%s\n", s);
#ifdef HAS_GET_RETURN_ADDRESS
    if (ourRet)
    {
	mem	returnStack[MAX_RETURN_STACK];

	getStackTrace (returnStack, MAX_RETURN_STACK);
	PrintReturnStack ("Current return stack", returnStack);
    }
#endif
}

static void
MarkMemoryRegion (low, high)
    mem	*low, *high;
{
    mem	**start = (mem **) low, **end = (mem **) high;
    mem	*p;

    while (start < end) {
	p = *start;
	if (endOfStaticMemory <= p && p < highestAllocatedMemory)
	    MarkActiveBlock (p, (mem *) start);
	start++;
    }
}

static void
MarkActiveBlock (p, from)
    mem   *p, *from;
{
    HeadPtr	h, hh;
    int		marked;
    int		oldMarked;

    SEARCH(activeMemory, h, p)
    if (h) {
	marked = REFERENCED_HEAD;
        if (p != DataForHead(h))
	    marked = REFERENCED_MIDDLE;
	oldMarked = h->marked;
	if (!(oldMarked & marked))
	{
	    h->marked |= marked;
	    h->from = from;
#ifdef HAS_GET_RETURN_ADDRESS
	    SEARCH(activeMemory, hh, h->from)
	      if (hh) 
		h->fromReturnStack = hh->returnStack;
#endif
	    if (!oldMarked)
		MarkMemoryRegion (DataForHead(h), (mem *) TailForHead(h));
	}
	return;
    }
    SEARCH(freedMemory, h, p)
    if (h)
    {
	marked = REFERENCED_HEAD;
        if (p != DataForHead(h))
	    marked = REFERENCED_MIDDLE;
	if (!(h->marked & marked))
	{
	    h->marked |= marked;
	    h->from = from;
#ifdef HAS_GET_RETURN_ADDRESS
	    SEARCH(activeMemory, hh, h->from)
            if (hh) 
		h->fromReturnStack = hh->returnStack;
#endif
	}
	return;
    }
}

static void
ClearTree (t)
    tree    *t;
{
    if (!t)
	return;
    ClearTree (t->left);
    t->marked = 0;
    t->from = 0;
    ClearTree (t->right);
}

static void
SweepActiveTree (t)
    tree    *t;
{
    if (!t)
	return;
    SweepActiveTree (t->left);
    if (!t->marked) {
	unreferencedAllocatedTotal += t->desiredsize;
	unreferencedAllocatedCount++;
	MemError ("Unreferenced allocated", t, FALSE);
    }
    else if (!(t->marked & REFERENCED_HEAD))
	MemError ("Referenced allocated middle", t, FALSE);
    SweepActiveTree (t->right);
}

/*
 * run a thread through the tree at the same time
 * - the thread runs 
 *
 * root -> left_child ... -> right_child ... -> null
 */

static tree *
SweepFreedTree (t)
    tree    *t;
{
    tree    *left_last, *right_last;

    if (!t)
	return 0;

    left_last = SweepFreedTree (t->left);
    if (t->marked)
    {
	if (t->marked & REFERENCED_HEAD)
	    MemError ("Referenced freed base", t, FALSE);
	else if (FindLeakWarnMiddlePointers)
	    MemError ("Referenced freed middle", t, FALSE);
    }
    right_last = SweepFreedTree (t->right);

    if (t->left)
	t->next = t->left;
    else
	t->next = t->right;
    if (left_last)
	left_last->next = t->right;
    if (!right_last)
	right_last = left_last;
    if (!right_last)
	right_last = t;
    return right_last;
}

static void
SweepFreedMemory ()
{
    tree    *t, *n;
    int	    count, shouldCount;

    (void) SweepFreedTree (freedMemory);
    count = 0;
    shouldCount = freedMemoryCount;
    for (t = freedMemory; t; t = n) {
	n = t->next;
	count++;
	if (!t->marked) 
	{
	    (void) tree_delete (&freedMemory, t, FALSE);
	    freedMemoryTotal -= t->desiredsize;
	    freedMemoryCount--;
	    tree_insert (&deadMemory, t, TRUE);
	}
    }
    if (count != shouldCount)
	abort ();
}

static void
ValidateTree (head, headMagic, tailMagic, bodyMagic, mesg)
    tree    *head;
    mem	    headMagic, tailMagic, bodyMagic;
    char    *mesg;
{
    TailPtr	tail;
    magic    	*p;
    int		i;

    if (!head)
	return;
    ValidateTree (head->left, headMagic, tailMagic, bodyMagic, mesg);
    tail = TailForHead (head);
    if (head->headMagic != headMagic)
	MemError (mesg, head, FALSE);
    if (tail->tailMagic != tailMagic)
	MemError (mesg, head, FALSE);
    if (bodyMagic) {
	i = head->size / sizeof (magic);
	p = (magic *) DataForHead(head);
	while (i--) {
	    if (*p++ != bodyMagic)
	    {
		MemError (mesg, head, FALSE);
		break;
	    }
	}
    }
    ValidateTree (head->right, headMagic, tailMagic, bodyMagic, mesg);
}

static void
ValidateActiveMemory ()
{
    ValidateTree (activeMemory, ACTIVE_HEAD_MAGIC, ACTIVE_TAIL_MAGIC,
		  0, "Store outside of active memory");
}

static void
ValidateFreedMemory ()
{
    ValidateTree (freedMemory, FREED_HEAD_MAGIC, FREED_TAIL_MAGIC,
		  FREED_DATA_MAGIC, "Store into freed memory");
}

static void
AddActiveBlock (h)
    HeadPtr	h;
{
    TailPtr	t = TailForHead(h);
    magic    	*p;
    int		i;

    tree_insert (&activeMemory, h, FALSE);
    if ((mem *) t > highestAllocatedMemory)
	highestAllocatedMemory = (mem *) t;

    /*
     * Breakpoint position - assign FindLeakAllocBreakpoint with
     * debugger and set a breakpoint in the conditional clause below
     */
    if (FindLeakTime == FindLeakAllocBreakpoint)
	h->headMagic = ACTIVE_HEAD_MAGIC;	    /* set breakpoint here */

    h->allocTime = FindLeakTime++;

    h->headMagic = ACTIVE_HEAD_MAGIC;
    t->tailMagic = ACTIVE_TAIL_MAGIC;
    i = h->size / sizeof (magic);
    p = (magic *) DataForHead(h);
    while (i--)
	*p++ = ACTIVE_DATA_MAGIC;
    activeMemoryTotal += h->desiredsize;
    activeMemoryCount++;
}

static void
RemoveActiveBlock (h)
    HeadPtr	h;
{
    activeMemoryTotal -= h->desiredsize;
    activeMemoryCount--;
    tree_delete (&activeMemory, h, FALSE);
}

static void
AddFreedBlock (h)
    HeadPtr	h;
{
    TailPtr	t = TailForHead(h);
    int		i;
    magic    	*p;

    tree_insert (&freedMemory, h, FALSE);

    /*
     * Breakpoint position - assign FindLeakFreeBreakpoint with
     * debugger and set a breakpoint in the conditional clause below
     */
    if (FindLeakTime == FindLeakFreeBreakpoint)
	h->headMagic = FREED_HEAD_MAGIC;	    /* set breakpoint here */

    h->freeTime = FindLeakTime++;

    h->headMagic = FREED_HEAD_MAGIC;
    t->tailMagic = FREED_TAIL_MAGIC;
    i = h->size / sizeof (magic);
    p = (magic *) DataForHead(h);
    while (i--)
	*p++ = FREED_DATA_MAGIC;
    freedMemoryTotal += h->desiredsize;
    freedMemoryCount++;
    /* GC if we've got piles of unused memory */
    if (freedMemoryTotal - deadMemoryTotal >= MAX_FREED_MEMORY)
	CheckMemory ();
}
#if 0
static void
WarnReferencedRange(rangeStart,rangeEnd,from,to)
     mem *rangeStart;
     mem *rangeEnd;
     mem *from;
     mem *to;
{
  mem *range = rangeStart;

  while ( range < rangeEnd) {
    if ((mem *)*range >= from && (mem *)*range <= to)
      fprintf(stderr, "0x%lx still points into newly allocated range\n",
	      (unsigned long) range);
    range++;
  }
}

static void
WarnReferencedTree(head, from, to)
     tree *head;
     char *from;
     char *to;
{
  if (!head) return;
  WarnReferencedTree(head->right,from,to);
  WarnReferencedRange(DataForHead(head),TailForHead(head),from,to);
  WarnReferencedTree(head->left,from,to);
}

static void
WarnReferenced(from, to)
     char *from;
     char *to;
{
    mem	foo;

    foo = 1;
    WarnReferencedTree(activeMemory,from,to);
    WarnReferencedRange(BOTTOM_OF_DATA, endOfStaticMemory,from,to);
    WarnReferencedRange(&foo, TOP_OF_STACK,from,to);
}  
#endif
/*
 * Entry points:
 *
 *  CheckMemory ()		--	Verifies heap
 *  malloc (size)		--	Allocates memory
 *  free (old)			--	Deallocates memory
 *  realloc (old, size)		--	Allocate, copy, free
 *  calloc (num, size_per)	--	Allocate and zero
 */

void
CheckMemory ()
{
#if 0
    mem	foo;

    unreferencedAllocatedTotal = 0;
    unreferencedAllocatedCount = 0;
    foo = 1;
    fprintf (stderr, "\nCheckMemory\n");
    fprintf (stderr, "Static Memory Area: 0x%lx to 0x%lx\n",
               BOTTOM_OF_DATA, endOfStaticMemory);
    fprintf (stderr, "%d bytes active memory in %d allocations\n",
	     activeMemoryTotal, activeMemoryCount);
    fprintf (stderr, "%d bytes freed memory held from %d allocations\n",
	    freedMemoryTotal, freedMemoryCount);
    ValidateActiveMemory ();
    ValidateFreedMemory ();
    ClearTree (activeMemory);
    ClearTree (freedMemory);
    MarkMemoryRegion (BOTTOM_OF_DATA, endOfStaticMemory);
    MarkMemoryRegion (&foo, TOP_OF_STACK);
    SweepActiveTree (activeMemory);
    SweepFreedMemory ();
    fprintf (stderr, "%d bytes freed memory still held from %d allocations\n",
	     freedMemoryTotal, freedMemoryCount);
    fprintf (stderr, 
	   "%d bytes of allocated memory not referenced from %d allocations\n",
	     unreferencedAllocatedTotal,unreferencedAllocatedCount);
    deadMemoryTotal = freedMemoryTotal;
    fprintf (stderr, "CheckMemory done\n");
#endif
}

/*
 * Allocator interface -- malloc and free (others in separate files)
 */

#define CORE_CHUNK  16384

static char	*core;
static unsigned	core_left;
static unsigned	total_core_used;

static char *
morecore (size)
    unsigned	size;
{
    unsigned	alloc_size;
    char	*alloc, *newcore;

    if (core_left < size)
    {
	alloc_size = (size + CORE_CHUNK - 1) & ~(CORE_CHUNK-1);
    	newcore = sbrk (alloc_size);
    	if (((long) newcore) == -1)
	    return 0;
	core = newcore;
	core_left = alloc_size;
	total_core_used += alloc_size;
    }
    alloc = core;
    core += size;
    core_left -= size;
    return alloc;
}

char *
malloc (desiredsize)
    unsigned	desiredsize;
{
    char	*ret;
    unsigned	size;
    unsigned	totalsize;
    HeadPtr	h;

    if (!endOfStaticMemory)
	endOfStaticMemory = (mem *) sbrk(0);
    if (FindLeakCheckAlways)
	CheckMemory ();
    else if (FindLeakValidateAlways)
    {
	ValidateActiveMemory ();
	ValidateFreedMemory ();
    }
    size = RoundUp(desiredsize);
    totalsize = TotalSize (size);

    h = deadMemory;
    while (h)
    {
	if (h->actualSize == size)
	    break;
	else if (h->actualSize < size)
	    h = h->right;
	else {
	    if (!h->left)
		break;
	    h = h->left;
	}
    }
    if (h) 
    {
	tree_delete (&deadMemory, h, TRUE);
    }
    else
    {
	h = (HeadPtr) morecore (totalsize);
	if (!h)
	    return NULL;
	h->actualSize = size;
    }
    h->desiredsize = desiredsize;
    h->size = size;
#ifdef HAS_GET_RETURN_ADDRESS
    getStackTrace (h->returnStack, MAX_RETURN_STACK);
#endif
    AddActiveBlock (h);
    ret =  (char *) DataForHead(h);
    if (FindPrintAllocations) {
	fprintf(stderr,"Allocated %i bytes at 0x%lx\n",desiredsize,ret);
#ifdef HAS_GET_RETURN_ADDRESS
	PrintReturnStack ("at",h->returnStack);
#endif
    }
    return ret;
}

void
free (p)
    char    *p;
{
    HeadPtr	h;
    static int	beenHere;
    
#ifndef NO_ATEXIT
    /* do it at free instead of malloc to avoid recursion? */
    if (!beenHere)
    {
	beenHere = TRUE;
	atexit (CheckMemory);
    }
#endif
    if (!p)
    {
	MemError ("Freeing NULL", (HeadPtr) 0, TRUE);
	return;
    }
    SEARCH (activeMemory, h, p);
    if (!h)
    {
	SEARCH(freedMemory, h, p);
	if (h)
	    MemError ("Freeing something twice", h, TRUE);
	else
	    MemError ("Freeing something never allocated", h, TRUE);
	return;
    }
    if (DataForHead(h) != (mem *) p)
    {
	MemError ("Freeing pointer to middle of allocated block", h, TRUE);
	return;
    }
    if (h->headMagic != ACTIVE_HEAD_MAGIC ||
	TailForHead(h)->tailMagic != ACTIVE_TAIL_MAGIC)
	MemError ("Freeing corrupted data", h, TRUE);
    RemoveActiveBlock (h);
#ifdef HAS_GET_RETURN_ADDRESS
    getStackTrace (h->returnStack, MAX_RETURN_STACK);
#endif
    AddFreedBlock (h);
    if (FindLeakCheckAlways)
	CheckMemory ();
    else if (FindLeakValidateAlways)
    {
	ValidateActiveMemory ();
	ValidateFreedMemory ();
    }
    if (FindPrintAllocations) {
	fprintf(stderr,"Freed at:  0x%lx\n",p);
	PrintReturnStack ("at",h->returnStack);
    }

}

char *
realloc (old, desiredsize)
    char	*old;
    unsigned	desiredsize;
{
    char    *new;
    HeadPtr  h, fh;
    int	    copysize;

    new = malloc (desiredsize);
    if (!new)
	return NULL;
    if (!old)
	return new;
    SEARCH(activeMemory, h, old);
    if (!h)
    {
	SEARCH(freedMemory, fh, old);
	if (fh)
	    MemError ("Reallocing from freed data", fh, TRUE);
	else
	    MemError ("Reallocing from something not allocated", h, TRUE);
    }
    else
    {
	if (DataForHead(h) != (mem *) old)
	{
	    MemError ("Reallocing from pointer to middle of allocated block", h, TRUE);
	}
	else
	{
	    if (h->headMagic != ACTIVE_HEAD_MAGIC ||
		TailForHead(h)->tailMagic != ACTIVE_TAIL_MAGIC)
		MemError ("Reallocing corrupted data", h, TRUE);
	    copysize = desiredsize;
	    if (h->desiredsize < desiredsize)
		copysize = h->desiredsize;
#ifdef SVR4
	    memmove (new, old, copysize);
#else
	    bcopy (old, new, copysize);
#endif
	    RemoveActiveBlock (h);
#ifdef HAS_GET_RETURN_ADDRESS
	    getStackTrace (h->returnStack, MAX_RETURN_STACK);
#endif
	    AddFreedBlock (h);
	}
    }
    if (FindPrintAllocations) {
	fprintf(stderr,"Freed at: 0x%lx\n",old);
	fprintf(stderr,"Reallocated: %i bytes at:  0x%lx\n",desiredsize,new);
#ifdef HAS_GET_RETURN_ADDRESS
        PrintReturnStack ("at", h->returnStack);
#endif
    }
    return new;
}

char *
calloc (num, size)
    unsigned	num, size;
{
    char *ret;

    size *= num;
    ret = malloc (size);
    if (!ret)
	return NULL;
#ifdef SVR4
    memset (ret, 0, size);
#else
    bzero (ret, size);
#endif
    return ret;
}

/*
 * Semi-Balanced trees (avl).  This only contains two
 * routines - insert and delete.  Searching is
 * reserved for the client to write.
 */

static	rebalance_right (), rebalance_left ();

/*
 * insert a new node
 *
 * this routine returns non-zero if the tree has grown
 * taller
 */

static int
tree_insert (treep, new, bySize)
tree	**treep;
tree	*new;
int	bySize;
{
	if (!(*treep)) {
		(*treep) = new;
		(*treep)->left = 0;
		(*treep)->right = 0;
		(*treep)->balance = 0;
		return 1;
	} else {
		if (LESS_THAN (*treep, new, bySize)) {
			if (tree_insert (&((*treep)->right), new, bySize))
				switch (++(*treep)->balance) {
				case 0:
					return 0;
				case 1:
					return 1;
				case 2:
					(void) rebalance_right (treep);
				}
			return 0;
		} else if (GREATER_THAN(*treep, new, bySize)) {
			if (tree_insert (&((*treep)->left), new, bySize))
				switch (--(*treep)->balance) {
				case 0:
					return 0;
				case -1:
					return 1;
				case -2:
					(void) rebalance_left (treep);
				}
			return 0;
		} else {
			return 0;
		}
	}
	/*NOTREACHED*/
}
					
/*
 * delete a node from a tree
 *
 * this routine return non-zero if the tree has been shortened
 */

static int
tree_delete (treep, old, bySize)
tree	**treep;
tree	*old;
int	bySize;
{
	tree	*to_be_deleted;
	tree	*replacement;
	tree	*replacement_parent;
	int	replacement_direction;
	int	delete_direction;
	tree	*swap_temp;
	int	balance_temp;

	if (!*treep)
		/* node not found */
		return 0;
	if (LESS_THAN(*treep, old, bySize)) {
		if (tree_delete (&(*treep)->right, old, bySize))
			/*
			 * check the balance factors
			 * Note that the conditions are
			 * inverted from the insertion case
			 */
			switch (--(*treep)->balance) {
			case 0:
				return 1;
			case -1:
				return 0;
			case -2:
				return rebalance_left (treep);
			}
		return 0;
	} else if (GREATER_THAN(*treep, old, bySize)) {
		if (tree_delete (&(*treep)->left, old, bySize))
			switch (++(*treep)->balance) {
			case 0:
				return 1;
			case 1:
				return 0;
			case 2:
				return rebalance_right (treep);
			}
		return 0;
	} else {
		to_be_deleted = *treep;
		/*
		 * find an empty down pointer (if any)
		 * and rehook the tree
		 */
		if (!to_be_deleted->right) {
			(*treep) = to_be_deleted->left;
			return 1;
		} else if (!to_be_deleted->left) {
			(*treep) = to_be_deleted->right;
			return 1;
		} else {
		/* 
		 * if both down pointers are full, then
		 * move a node from the bottom of the tree up here.
		 *
		 * This builds an incorrect tree -- the replacement
		 * node and the to_be_deleted node will not
		 * be in correct order.  This doesn't matter as
		 * the to_be_deleted node will obviously not leave
		 * this routine alive.
		 */
			/*
			 * if the tree is left heavy, then go left
			 * else go right
			 */
			replacement_parent = to_be_deleted;
			if (to_be_deleted->balance == -1) {
				delete_direction = -1;
				replacement_direction = -1;
				replacement = to_be_deleted->left;
				while (replacement->right) {
					replacement_parent = replacement;
					replacement_direction = 1;
					replacement = replacement->right;
				}
			} else {
				delete_direction = 1;
				replacement_direction = 1;
				replacement = to_be_deleted->right;
				while (replacement->left) {
					replacement_parent = replacement;
					replacement_direction = -1;
					replacement = replacement->left;
				}
			}
			/*
			 * swap the replacement node into
			 * the tree where the node is to be removed
			 * 
			 * this would be faster if only the data
			 * element was swapped -- but that
			 * won't work for findleak.  The alternate
			 * code would be:
			   data_temp = to_be_deleted->data;
			   to _be_deleted->data = replacement->data;
			   replacement->data = data_temp;
			 */
			swap_temp = to_be_deleted->left;
			to_be_deleted->left = replacement->left;
			replacement->left = swap_temp;
			swap_temp = to_be_deleted->right;
			to_be_deleted->right = replacement->right;
			replacement->right = swap_temp;
			balance_temp = to_be_deleted->balance;
			to_be_deleted->balance = replacement->balance;
			replacement->balance = balance_temp;
			/*
			 * if the replacement node is directly below
			 * the to-be-removed node, hook the to_be_deleted
			 * node below it (instead of below itself!)
			 */
			if (replacement_parent == to_be_deleted)
				replacement_parent = replacement;
			if (replacement_direction == -1)
				replacement_parent->left = to_be_deleted;
			else
				replacement_parent->right = to_be_deleted;
			(*treep) = replacement;
			/*
			 * delete the node from the sub-tree
			 */
			if (delete_direction == -1) {
				if (tree_delete (&(*treep)->left, old, bySize)) {
					switch (++(*treep)->balance) {
					case 2:
						abort ();
					case 1:
						return 0;
					case 0:
						return 1;
					}
				}
				return 0;
			} else {
				if (tree_delete (&(*treep)->right, old, bySize)) {
					switch (--(*treep)->balance) {
					case -2:
						abort ();
					case -1:
						return 0;
					case 0:
						return 1;
					}
				}
				return 0;
			}
		}
	}
	/*NOTREACHED*/
}

/*
 * two routines to rebalance the tree.
 *
 * rebalance_right -- the right sub-tree is too long
 * rebalance_left --  the left sub-tree is too long
 *
 * These routines are the heart of avl trees, I've tried
 * to make their operation reasonably clear with comments,
 * but some study will be necessary to understand the
 * algorithm.
 *
 * these routines return non-zero if the resultant
 * tree is shorter than the un-balanced version.  This
 * is only of interest to the delete routine as the
 * balance after insertion can never actually shorten
 * the tree.
 */
 
static
rebalance_right (treep)
tree	**treep;
{
	tree	*temp;
	/*
	 * rebalance the tree
	 */
	if ((*treep)->right->balance == -1) {
		/* 
		 * double whammy -- the inner sub-sub tree
		 * is longer than the outer sub-sub tree
		 *
		 * this is the "double rotation" from
		 * knuth.  Scheme:  replace the tree top node
		 * with the inner sub-tree top node and
		 * adjust the maze of pointers and balance
		 * factors accordingly.
		 */
		temp = (*treep)->right->left;
		(*treep)->right->left = temp->right;
		temp->right = (*treep)->right;
		switch (temp->balance) {
		case -1:
			temp->right->balance = 1;
			(*treep)->balance = 0;
			break;
		case 0:
			temp->right->balance = 0;
			(*treep)->balance = 0;
			break;
		case 1:
			temp->right->balance = 0;
			(*treep)->balance = -1;
			break;
		}
		temp->balance = 0;
		(*treep)->right = temp->left;
		temp->left = (*treep);
		(*treep) = temp;
		return 1;
	} else {
		/*
		 * a simple single rotation
		 *
		 * Scheme:  replace the tree top node
		 * with the sub-tree top node 
		 */
		temp = (*treep)->right->left;
		(*treep)->right->left = (*treep);
		(*treep) = (*treep)->right;
		(*treep)->left->right = temp;
		/*
		 * only two possible configurations --
		 * if the right sub-tree was balanced, then
		 * *both* sides of it were longer than the
		 * left side, so the resultant tree will
		 * have a long leg (the left inner leg being
		 * the same length as the right leg)
		 */
		if ((*treep)->balance == 0) {
			(*treep)->balance = -1;
			(*treep)->left->balance = 1;
			return 0;
		} else {
			(*treep)->balance = 0;
			(*treep)->left->balance = 0;
			return 1;
		}
	}
}

static
rebalance_left (treep)
tree	**treep;
{
	tree	*temp;
	/*
	 * rebalance the tree
	 */
	if ((*treep)->left->balance == 1) {
		/* 
		 * double whammy -- the inner sub-sub tree
		 * is longer than the outer sub-sub tree
		 *
		 * this is the "double rotation" from
		 * knuth.  Scheme:  replace the tree top node
		 * with the inner sub-tree top node and
		 * adjust the maze of pointers and balance
		 * factors accordingly.
		 */
		temp = (*treep)->left->right;
		(*treep)->left->right = temp->left;
		temp->left = (*treep)->left;
		switch (temp->balance) {
		case 1:
			temp->left->balance = -1;
			(*treep)->balance = 0;
			break;
		case 0:
			temp->left->balance = 0;
			(*treep)->balance = 0;
			break;
		case -1:
			temp->left->balance = 0;
			(*treep)->balance = 1;
			break;
		}
		temp->balance = 0;
		(*treep)->left = temp->right;
		temp->right = (*treep);
		(*treep) = temp;
		return 1;
	} else {
		/*
		 * a simple single rotation
		 *
		 * Scheme:  replace the tree top node
		 * with the sub-tree top node 
		 */
		temp = (*treep)->left->right;
		(*treep)->left->right = (*treep);
		(*treep) = (*treep)->left;
		(*treep)->right->left = temp;
		/*
		 * only two possible configurations --
		 * if the left sub-tree was balanced, then
		 * *both* sides of it were longer than the
		 * right side, so the resultant tree will
		 * have a long leg (the right inner leg being
		 * the same length as the left leg)
		 */
		if ((*treep)->balance == 0) {
			(*treep)->balance = 1;
			(*treep)->right->balance = -1;
			return 0;
		} else {
			(*treep)->balance = 0;
			(*treep)->right->balance = 0;
			return 1;
		}
	}
}

#ifdef DEBUG

static 
depth (treep)
tree	*treep;
{
	int	ldepth, rdepth;

	if (!treep)
		return 0;
	ldepth = depth (treep->left);
	rdepth = depth (treep->right);
	if (ldepth > rdepth)
		return ldepth + 1;
	return rdepth + 1;
}

static tree *
left_most (treep)
tree	*treep;
{
	while (treep && treep->left)
		treep = treep->left;
	return treep;
}

static tree *
right_most (treep)
tree	*treep;
{
	while (treep && treep->right)
		treep = treep->right;
	return treep;
}

tree_verify (treep)
tree	*treep;
{
	tree_data	left_data, right_data;

	if (!treep)
		return 1;
	if (treep->left)
		left_data = right_most (treep->left)->data;
	else
		left_data = treep->data - 1;
	if (treep->right)
		right_data = left_most (treep->right)->data;
	else
		right_data = treep->data + 1;
	if (treep->data < left_data || treep->data > right_data) {
		abort ();
		return 0;
	}
	if (treep->balance != depth (treep->right) - depth (treep->left)) {
		abort ();
		return 0;
	}
	return tree_verify (treep->left) && tree_verify (treep->right);
}

#endif
