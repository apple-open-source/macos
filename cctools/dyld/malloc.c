/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef __OPENSTEP__ /* Rhapsody is not indr'ed yet */
#define NO_INDR_LIBC
#endif
#ifdef SHLIB
#undef NXCreateZone
#undef NXCreateChildZone
#undef NXDefaultMallocZone
#undef NXMergeZone
#undef NXZoneCalloc
#undef NXZoneFromPtr
#undef NXZonePtrInfo
#undef NXNameZone
#undef NXMallocCheck
#undef _NXMallocDumpFrees
#undef _NXMallocDumpZones
#undef vfree
#undef NXAddRegion
#undef NXRemoveRegion
#undef malloc
#undef calloc
#undef realloc
#undef valloc
#undef mstats
#undef free
#undef malloc_size
#undef malloc_debug
#undef malloc_error
#undef malloc_freezedry
#undef malloc_jumpstart
#undef malloc_good_size
#undef malloc_singlethreaded
#undef _malloc_fork_child
#undef _malloc_fork_parent
#undef _malloc_fork_prepare
#endif SHLIB

#ifdef TESTING
#define valloc _new_valloc
#define calloc _new_calloc
#define realloc Realloc
#define malloc Malloc
#define free Free
#define malloc_freezedry _new_malloc_freezedry
#define malloc_jumpstart _new_malloc_jumpstart
#define malloc_size _new_malloc_size
#define malloc_good_size _new_malloc_good_size
#define malloc_error _new_malloc_error
#define mstats	_new_malloc_mstats
#define malloc_debug _new_malloc_debug
#define _malloc_fork_prepare  _new_malloc_fork_prepare
#define _malloc_fork_child  _new_malloc_fork_child
#define _malloc_fork_parent _new_malloc_fork_parent
#define _NXMallocDumpFrees  _new_NXMallocDumpFrees
#define _NXMallocDumpZones  _new_NXMallocDumpZones
#define NXNameZone		_new_NXNameZone
#endif

#import <stdlib.h>
#ifndef __OPENSTEP__
extern size_t malloc_size(void *ptr);
#endif
#import <stdio.h>
#import <mach/mach.h>
#ifndef __MACH30__
#import "stuff/openstep_mach.h"
#import <errno.h>
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
#else
#import <mach/cthreads.h>
#endif
#import <string.h>

#import <objc/zone.h>
#import <zoneprotect.h>
#import "errors.h"

/*
 * MALLOC_ALIGN defines the alignment point for malloc'd data.  This must be a power of two
 * and should be no less than sizeof(header_t).
 */
#if defined(MAIN)
#  define	MALLOC_ALIGN	16
#else	/* !defined(MAIN) */
/* Architecture-specific definitions */
#if m68k || __i386__
#  define	MALLOC_ALIGN	4
#elif m88k
#  define	MALLOC_ALIGN	16
#elif __ppc__
#  define	MALLOC_ALIGN	8
#elif hppa
#  define	MALLOC_ALIGN	8
#elif sparc
#  define	MALLOC_ALIGN	8
#else
#  error	MALLOC_ALIGN has not been defined for this architecture.
#endif
#endif	/* !defined(MAIN) */

/* Other useful alignment macros */
#define	ALIGN_OFFSET	(MALLOC_ALIGN - 1)
#define	ALIGN_MASK	(~(MALLOC_ALIGN - 1))

/*
 * ROUND_SIZE rounds the size of the request up to an alignment that allows one header_t
 * before the MALLOC_ALIGN.  We call this the header_t alignment
 *
 *	ASSERT(header_addr % MALLOC_ALIGN == MALLOC_ALIGN - sizeof(header_t))
 */
#define	ROUND_SIZE(x)	( ( ((x) + sizeof(header_t) + ALIGN_OFFSET) & ALIGN_MASK ) \
			  - sizeof(header_t) )

/*
 * Invariants (as I understand them :-)).
 *
 *	1. Malloc'd data (pieces) will always be aligned to MALLOC_ALIGN.  This includes
 *	   the free array (which is initially put at the beginning of a zone.
 *
 *	2. Every header_t will be header_t aligned EXCEPT:
 *
 *		a. The dummy header at the beginning of a child (INTERNAL) zone will
 *		   be MALLOC_ALIGNED.  This is OK since we never really treat is as
 *		   a header.  "Lastguyfree" in the header in front of it will always
 *		   be FALSE, "free" in the header before it will be FALSE until
 *		   the zone memory is freed.
 *
 *	3. Region->beginadd will always point to the memory that was allocated for
 *	   the region.
 *
 *	4. The first header in a region can be found by aligning up to the next header_t
 *	   alignment.
 *
 *	5. Region->endaddr will always point to the first byte AFTER the region.
 *
 */

/************************************************************

The malloc algorithm is based on a better fit algorithm as described in 
"in search of a better malloc"    usenix summer of 85.

It is not a best fit algorithm in that it does not guarantee that the smallest 
possible piece is selected which could satisfy a request. It picks one of the
smallest pieces which could satisfy the request. The free pieces are stored in a 
tree. The tree is a heap as described in Sedgewick in the heapsort section. Instead
of a heap, another tree structure could be used. The nice thing about a heap is 
that there is no memory overhead for left & right pointers and it is always balanced.
For very large trees a self adjusting tree would be more efficient. 
I'm not sure what the breakpoint is. In starting up Mail and running for around
10 minutes there are 54 element on the free heap. In starting up Frame opening and
then closing the demo document, the free heap has 181 elemements. All heap changes
max out at log2(N) operations. One semi-expensive operation which needs to occurr
when items are exchanged in the heap is updating the heap index at the end of a 
free piece. (Look for the MARKFREESPOT define)

Here is an example heap.

             100
	   /    \
	  50     75
	/  \    /
      30   40  20

Say a malloc request is received of size 10. The search pattern is as follows. 
Keep picking the smallest child until none of the children are big enough. The pattern
would be  100, 50, 30.  Part of the 30 piece would be returned. The heap would be
rearranged if the old 30 piece became smaller that any of its children.

A header is placed on each free or used piece. It is 4 bytes and contains a size field
and 2 booleans. The booleans tell whether a piece is freed, and if the last piece is free. Whenever a piece is free it sets the lastguyfree bit of the guy in front. 
It knows this is safe because a dummy block is placed at the end of each memory piece
received from the OS.

When a piece is free it looks at its lastguyfree flag. If the lastguy is free it joins
this piece with the one behind. It finds this piece in the free heap as follows. Whenever a piece  is free the last 4 bytes of a piece will be an index into the free
heap array. This will always be 4 bytes behind the next guys header, making it easy to join with pieces behind.

Since a piece knows its own size it is easy to find the header of the guy in front. Using his size you can find his place in the free heap array if neccessary for joining.

Here is how memory looks.
  
		    header
		    data
		    header
		    data
		    header
		    data
		    deadwood

The deadwood piece is placed at the end of memory when received from the OS. In the 
case of external zones it is 4 bytes. For child zones it is 8 bytes.  

There can be multiple zones. External zones use vm_allocate to get storage.
Child zones act like any other zone but get their storage by mallocing in the 
parent zone.
Here is how they look. The indented part is the child. Part to the left the parent.

              In  Use                    After a destroy        If Merged
	-----------------------------------------------------------------------
		    header                   header             header
		    	 dummy               data		  data
		         header              			  header
			 data					  data
			 header 				  header		
			 data					  data	 
			 deadwood				  header
			 					  data
		    header                   header		header
		    data		     data		data
		    header		     header		header
		    data		     data		data	
		    deadwood		     deadwood		deadwood


 The data part of one of the parents pieces is used to create a new zone for the child.
 This zone has a dummy piece at the beginning and a piece of deadwood at the end. The
 piece at the end is necessary to keep the free code from joining pieces in the parent
 and the child together. The piece at the front is needed when merging to become 
 data for the old parent header. Otherwise you would have 2 headers in a row.
 
 A contigous address range in a zone is called a region. A structure array ordered
 by address is used to keep track of all regions. It looks like this.
 
 beginadd endadd zone
 beginadd endadd zone
 beginadd endadd zone
 
If continous addresses are returned from vm_allocate, existing regions in the array
will be extended. This keeps the number of elements small. The zone for any piece
malloced can be found by searching in this array.

Memory is returned to the OS in 2 cases. 
  1. A complete memory region is free.
  2. The end of a region is free which is larger than the granularity specified in
    NXCreateZone()
    

If a malloc request is received which is close to an integral number of pages a 
call to valloc is automatically generated. This will cut down the number of pages
kept hot to access these pages.

The code calls itself recursively to get memory for internal data structures.
This is where the majority of bugs have occurred. To bootstrap memory on the stack
is used. After that all operations must only allocate at safe times, and always
guarantee that enough room is left in the internal data structures to process
another operation. 
***********************************************************/


/*
 * Start of External interface for programmers writing new allocators.
 */
 void NXAddRegion(int start, int size,NXZone *zonep);
 void NXRemoveRegion(int start);
/*
 * End of External interface for programmers writing new allocators.
 */
 
#define MALLOC_ERROR_VM_ALLOC 0
#define MALLOC_ERROR_VM_DEALLOC 1
#define MALLOC_ERROR_VM_COPY 2
#define MALLOC_ERROR_FREE_FREE 3
#define MALLOC_ERROR_HEAP_CHECK 4
#define MALLOC_ERROR_FREE_NOT_IN_HEAP 5
#define MALLOC_ERROR_BAD_ZONE 6

#define MALLOC_DEBUG_1DEEPFREE 1
#define MALLOC_DEBUG_WASTEFREE 2
#define MALLOC_DEBUG_CATCHFREE 4
#define MALLOC_DEBUG_BEFORE 8
#define MALLOC_DEBUG_AFTER 16

#define MALLOC_ENTER 	"Malloc corrupted entering malloc\n"
#define MALLOC_EXIT  	"Malloc corrupted leaving malloc\n"
#define REALLOC_ENTER 	"Malloc corrupted entering realloc\n"
#define REALLOC_EXIT 	"Malloc corrupted leaving realloc\n"
#define FREE_ENTER   	"Malloc corrupted entering free\n"
#define FREE_EXIT	"Malloc corrupted leaving free\n"
#define MALLOC_ERROR	"memory allocation error: "


#ifdef _POSIX_THREADS
#define LOCK pthread_mutex_lock(malloc_static.lock);
#define UNLOCK pthread_mutex_unlock(malloc_static.lock);
#else
#define LOCK mutex_lock(malloc_static.lock);
#define UNLOCK mutex_unlock(malloc_static.lock);
#endif

#define MALLOC(a) nxzonemallocnolock(malloc_static.defaultzone,a)
#define FREE(a) nxzonefreenolock(malloc_static.defaultzone,a)
#define REALLOC(a,b) nxzonereallocnolock(malloc_static.defaultzone,a,b)
#define Z(a)  ((zone_t *)a)

  /*
   * Because a zones freearray is itself in the heap, we always need to have
   * enough free slots ready which will equal the maximum needed durring
   * a resize of the freearray.
   *
   * 1 for the piece to free
   * 1 for free in morememnolock
   * 1 for free in checkregionsallocation()
   * 1 for ONEDEEPFREE when called from *malloc called from nxzonefreenolock
   *   when it is not called from free().
   */
#define FREESLOTSNEEDED 4
  /*
   * Malloc gets bootstraped by using the stack for parts which are normally
   * allocated in the heap. This is the starting size of the freearray on the 
   * stack. The 0th entry of the array is never used.
   */
#define NUMFREES (FREESLOTSNEEDED+1)

/*
 * These are the elements that are in the free heap.
 */
typedef struct {
    unsigned int size;
    char *data;
} felem_t;	

/*
 * The per zone data structure.
 */
typedef struct zt {
	NXZone	z;
	felem_t	*freearray;	/* the free heap */
 	int	freen;		/* the size of the free heap */
	int	freemaxn;	/* the allocated size of the free heap */
	int	granularity;    /* size expansion granularity */
	struct zt	*parentzonep;
	char	*name;
#define INTERNAL 1
#define EXTERNAL 2	
	unsigned int	type:2;
	unsigned int	canfree:1;
	unsigned int 	newmem:1; /* fresh memory */
	unsigned int 	noshrink:1; /* don't shrink freearray size */
	int		spare:19;
	unsigned int 	debug:8;
	} zone_t;

/*
 * The header which proceeds each piece of storage which is free or busy.
 */
typedef struct {
    unsigned int	lastguyfree:1;
    unsigned int	free:1;
    unsigned int	size:30;
} header_t;


/*
 * The per region data structure.
 * One of these will exist for each valloc, and for each discontinous piece of a 
 * zone. 
 */
typedef struct {
	char	*beginadd;
	char	*endadd;
	zone_t	*zonep;
	} region_t;

/*
 * Data that malloc needs to have around between calls
 */	
struct malloc_static_t {
    int	max_zone;
    int	max_allocedzone;
    zone_t  **zones;
    
    int	max_region;
    int	max_allocedregion;
    region_t  *regions;
    
    int lasthitregion;       /* cache used in findregionnolock */
    NXZone  *defaultzone;
    void    *freeme;
#ifdef _POSIX_THREADS
    pthread_mutex_t *lock;
#else
    mutex_t lock;
#endif
    char inited;
    char safesingle; 	/* No threads and no malloc_debugging */
};

static struct malloc_static_t malloc_static = { 0 };

static void *_valloczonenolock(size_t *size);
static region_t *findregionnolock(void *ptr);
static region_t *addregionnolock(char *start, int size, zone_t *zonep);
static void downheap(felem_t *freearray, int n, int freen);
static void heapremove(zone_t *zonep, int n);
static void upheap(felem_t *freearray, int freen);
static void heapadd(zone_t *zonep, felem_t *new);
static int  newregionnolock();
static void  break_region(char *start, int size, zone_t  *newzonep);
static void *internal_zone_extend(zone_t *parentzone,zone_t *zonep, int  nsize);
static void *NXZoneRealloc_canyou(zone_t *zonep, void *ptr, size_t size);
static void  keepzonenolock(zone_t *zonep);
static void delregionnolock(region_t *regionp);
static void delzonenolock(zone_t *zonep);
static void joinzonesnolock();
static void checkregionsallocation();
static void checkfreearrayallocation(zone_t *zonep, int musthave);
static int malloc_checkfreeheaps();
static int malloc_checkzones(void);
static void newzonedatanolock(zone_t *zonep, void **ptr, int size);

static void DefaultMallocError(int x);
static void (*MallocErrorFunc)(int) = DefaultMallocError;

static void *nxzonemallocnolock(NXZone *zonep, size_t size);
static void *nxzonereallocnolock(NXZone *zonep, void *ptr, size_t size);
static void nxzonefreenolock(NXZone *z, void *ptr);
static void *mallocnofree(NXZone *z, size_t size);
static void *mallocnofreenolock(NXZone *z, size_t size);

static void *nxzonemalloc(NXZone *zonep, size_t size);	// Can LOCK
static void *nxzonerealloc(NXZone *zonep, void *ptr, size_t size);	// Can LOCK
static void nxzonefree(NXZone *z, void *ptr);	// Can LOCK
static void nxdestroyzone(NXZone *z);	// Can LOCK
static void *morememnolock(zone_t *zonep, size_t nsize);
static void *vallocnolock(size_t size);
static void vfreenolock(void *ptr);
static void malloc_init(void);
static void dolockordebug(int fast);
static int isMallocData(void *ptr);
static void malloc_check(char *str);
static void errmessage(char *str);
static void stdmessage(char *str);


#define MARKFREESPOT(f,n) ((int *)(f)->data)[(f)->size / sizeof(int) - 1] = n
#define NOTPAGEALIGNED(a) (((int)a & (vm_page_size - 1)) != 0)
#define ISFREEMEMEMORY(ptr) (isMallocData(ptr) && ((header_t *)ptr)[-1].free)


#define FREEONEDEEP \
{\
	if(malloc_static.freeme) {\
	    void *freeme = malloc_static.freeme;\
	    malloc_static.freeme = 0;\
	    if(malloc_static.max_zone == 1 && NOTPAGEALIGNED(freeme)) {\
		nxzonefreenolock(malloc_static.defaultzone,freeme);\
	    }\
	    else {\
		region_t  *regionp;\
		regionp = findregionnolock(freeme);\
		if(!regionp) \
		    MallocErrorFunc(MALLOC_ERROR_FREE_NOT_IN_HEAP);\
		else if(regionp->zonep)\
		    nxzonefreenolock((NXZone*)regionp->zonep,freeme);\
		else\
		    vfreenolock(freeme);\
	    }\
	}\
}	

static void malloc_check(char *str)
{
    if(NXMallocCheck()) {
	errmessage(str);
	abort();
    }
}
#ifdef __OPENSTEP__
extern void write(int, char *, int);
#endif
static void errmessage(char *str)
{
    write(2,str,strlen(str));
}

static void stdmessage(char *str)
{
    write(1,str,strlen(str));
}



/*
 * Return the default zone.
 */
NXZone *NXDefaultMallocZone()
{
    if(!malloc_static.inited)
	malloc_init();
    return(malloc_static.defaultzone);
}


/*
 * Add an element to the free heap.
 */
static void heapadd(zone_t *zonep, felem_t *new)
{
 int	freen;
felem_t	*freearray;

    freearray = zonep->freearray;
    
    freen = ++zonep->freen;
    freearray[freen]= *new;
    upheap(freearray, freen);
}

/*
 * remove an element from the free heap.
 */
static void heapremove(zone_t *zonep, int n)
{
felem_t	*freearray;
  
    freearray = zonep->freearray;
    
    if(freearray[zonep->freen].size > freearray[n].size) {
	freearray[n] = freearray[zonep->freen--];
	MARKFREESPOT(&freearray[n],n);
	upheap(freearray, n);
    }
    else {
	freearray[n] = freearray[zonep->freen--];
	if(n > zonep->freen)
	    return;
	MARKFREESPOT(&freearray[n],n);
	downheap(freearray, n, zonep->freen);
    }
}


/*
 * Move a piece up the free heap until it is in the appropriate place
 * for its size.
 */
static void upheap(felem_t *freearray, int freen)
{
felem_t	temp,*fp;
int	v;

    /* freearray[0].size always has a large number */
    temp = freearray[freen];
    
    v = temp.size;
    while(freearray[freen >> 1].size < v) {
	fp = &freearray[freen];
	*fp = freearray[freen >> 1];
	MARKFREESPOT(fp,freen);
	freen >>= 1;
    }
    fp = &freearray[freen];
    *fp = temp;
    MARKFREESPOT(fp,freen);
}

/*
 * Move a piece down in the free heap until it is in the appropriate place
 * for its size.
 */
static void downheap(felem_t *freearray, int n, int freen)
{
felem_t	*fp,temp;
int 	v,j,halffreen;
  
    v = freearray[n].size;
    temp = freearray[n];
    
    halffreen = freen >> 1;
    while(n <= halffreen) {
	j = n*2;
	if(j < freen)
	    if(freearray[j].size < freearray[j+1].size)
		j++;
	if(v >= freearray[j].size)
	    break;
	fp = &freearray[n];
	*fp = freearray[j];
	MARKFREESPOT(fp,n);
	n = j;
    }
    fp = &freearray[n];
    *fp = temp;
    MARKFREESPOT(&freearray[n],n);
}


/*
 * A one time bootstrap call.
 */	
static void malloc_init(void)
{
region_t	fakeregions[1];
zone_t	*fakezones[1];
zone_t  firstzone;
#ifdef _POSIX_THREADS
pthread_mutex_t  stacklock;
#else
struct mutex  stacklock;
#endif

    malloc_static.inited = 1;
    malloc_static.lock = &stacklock; 
#ifdef _POSIX_THREADS
    bzero(malloc_static.lock,sizeof(pthread_mutex_t)); 
    pthread_mutex_init(malloc_static.lock, NULL);
#else
    bzero(malloc_static.lock,sizeof(struct mutex)); 
    mutex_init(malloc_static.lock);
#endif
    
    malloc_static.max_allocedregion = 4;
    malloc_static.regions = fakeregions;
    malloc_static.max_allocedzone = 1;
    malloc_static.zones = fakezones;
    malloc_static.defaultzone = (NXZone *)&firstzone;
    NXCreateZone(20*vm_page_size, 10*vm_page_size, 1);
    
    /* Place temp stack storage into heap */
    malloc_static.zones = (zone_t **)MALLOC(sizeof(zone_t *));
    malloc_static.zones[0] = (zone_t *)MALLOC(sizeof(zone_t));
    malloc_static.defaultzone = (NXZone *)malloc_static.zones[0];
    bcopy(&firstzone, malloc_static.zones[0], sizeof(zone_t));
    malloc_static.regions = (region_t *)MALLOC(4*sizeof(region_t));
    bcopy(fakeregions,malloc_static.regions,sizeof(region_t));
    malloc_static.regions[0].zonep = malloc_static.zones[0];
#ifdef _POSIX_THREADS
    malloc_static.lock = (pthread_mutex_t *) MALLOC(sizeof(pthread_mutex_t)); 
    bzero(malloc_static.lock,sizeof(pthread_mutex_t)); 
    pthread_mutex_init(malloc_static.lock, NULL);
#else
    malloc_static.lock = (mutex_t) MALLOC(sizeof(struct mutex)); 
    bzero(malloc_static.lock,sizeof(struct mutex)); 
    mutex_init(malloc_static.lock);
#endif
    ((zone_t *)malloc_static.defaultzone)->debug = MALLOC_DEBUG_1DEEPFREE;
}    

/*
 * Blowup a zone releasing all memory to the OS or the parent zone.
 */
static void nxdestroyzone(NXZone *z)
{
zone_t	*zonep = (zone_t *)z;
int	i;
region_t *regionp;
zone_t *parentzonep;
	
LOCK {
    FREEONEDEEP	
    
    if(zonep->type == EXTERNAL) {
	for(i = 0; i < malloc_static.max_region; i++) {
	    if(malloc_static.regions[i].zonep == zonep) {
		regionp = &malloc_static.regions[i];
		if(vm_deallocate(mach_task_self(),
		    (vm_address_t) regionp->beginadd,
		    (int)(regionp->endadd -regionp->beginadd)) != KERN_SUCCESS)
			MallocErrorFunc(MALLOC_ERROR_VM_DEALLOC);
		delregionnolock(regionp);
		i--;  /* all regions slid down by 1 */
		}
	}
    }
    else {
	parentzonep = zonep->parentzonep;
	for(i = 0; i < malloc_static.max_region; i++) {
	    if(malloc_static.regions[i].zonep == zonep) {
		regionp = &malloc_static.regions[i];
		nxzonefreenolock((NXZone*)zonep->parentzonep, regionp->beginadd);
		if(i > 0) 
		    malloc_static.regions[i-1].endadd = 
		    		malloc_static.regions[i].endadd;
		delregionnolock(regionp);
		i--;  /* all regions slid down by 1 */
		}
	    }
	joinzonesnolock();
    }
    delzonenolock(zonep);
} UNLOCK
}

/*
 * Combine a zone back into its parent zone.
 * All pointers into the original zone will still be valid.
 * The zone needs to be removed from the region array.
 * The dummy piece at start of each piece of the zone is expanded backwards to take in 
 * the header of parent zone and freed.
 * The free heap of the zone is merged with the parent, and the free heap of the 
 * zone freed.
 */ 
void NXMergeZone(NXZone *z)
{
zone_t	*zonep,*parentzonep;
int	i;
region_t *regionp;
header_t  *headerp;
felem_t	  *fp;

LOCK {
    FREEONEDEEP	
    zonep = (zone_t *)z;
    if(zonep->type != INTERNAL)
	MallocErrorFunc(MALLOC_ERROR_BAD_ZONE);

    parentzonep = zonep->parentzonep;
    /* 
     * Join the free heaps.
     * Do this first so we can free the edges of the child in the parent.
     */
     {
    int  d  = parentzonep->debug;
    parentzonep->debug = 0;    /* Free heap is currently under construction */
    checkfreearrayallocation(parentzonep, zonep->freen + FREESLOTSNEEDED);
    parentzonep->debug = d;
    }

    fp = &zonep->freearray[1];
    for(i = 1; i <= zonep->freen; i++) 
	heapadd(parentzonep, fp++);
    
    
    nxzonefreenolock((NXZone*)parentzonep, zonep->freearray);
    for(i = 0; i < malloc_static.max_region; i++) {
	if(malloc_static.regions[i].zonep == zonep) {
	    regionp = &malloc_static.regions[i];
	    /* have the dummy take over the parents piece */

	    /*
	     * The dummy is a fake header from the beginning of the region
	     * up to the first header_t alignment.  We add it's size to the previous
	     * header, effectively splicing over the zones.
	     *
	     * Since this is a child zone, we know that the region end is
	     * header_t aligned.  The deadwood header_t must be back one
	     * MALLOC_ALIGN's worth.
	     * 
	     */
	    headerp = (header_t *)regionp->beginadd;
	    headerp[-1].size = headerp->size + sizeof(header_t);
	    nxzonefreenolock((NXZone *)parentzonep,headerp);
	      /* free end piece in parent zone */
	    headerp = (header_t *)((int)regionp->endadd -
				   ROUND_SIZE(sizeof(header_t)));
	    nxzonefreenolock((NXZone *)parentzonep,headerp);

	    if(i > 0) 
		malloc_static.regions[i-1].endadd = 
			    malloc_static.regions[i].endadd;
	    delregionnolock(regionp);
	    i--;  /* all regions slid down by 1 */
	    }
	}
    joinzonesnolock();
    delzonenolock(zonep);
} UNLOCK
}

/*
 * Join areas in the region array which are continuous and in the 
 * same malloc zone.
 */
static void joinzonesnolock()
{
int	i;
region_t *regionp;

	   /* rejoin zones */
    for(i = 0; i < (malloc_static.max_region - 1); i++) {
	regionp = &malloc_static.regions[i];
	if((regionp->endadd == (regionp+1)->beginadd) && regionp->zonep != 0 &&
		(regionp->zonep == (regionp+1)->zonep)) {
	    regionp->endadd = (regionp+1)->endadd;
	    delregionnolock(regionp+1);
	    i--;  /* all regions slid down by 1 */
	    }
	}
}

/*
 * Create a new malloc zone out of vm_allocate storage.
 */
NXZone *NXCreateZone(size_t startsize, size_t granularity, int canfree)
{
zone_t	*zonep;
felem_t	*freearray;
header_t *datap;

    if(!malloc_static.inited)
	malloc_init();
LOCK {
    if(malloc_static.max_zone == 0)
	zonep = (zone_t *)malloc_static.defaultzone;
    else
	zonep = (zone_t *)MALLOC(sizeof(zone_t));

    zonep->granularity = round_page(granularity);
    zonep->type = EXTERNAL;
    zonep->z.destroy = nxdestroyzone;
    zonep->debug = ((zone_t *)malloc_static.defaultzone)->debug;
    zonep->noshrink = 0;
    zonep->name = 0;
    datap = _valloczonenolock(&startsize);
    newzonedatanolock(zonep, (void **)&datap, startsize);
    checkregionsallocation();

#define STARTHUNK (ROUND_SIZE(sizeof(felem_t)*NUMFREES) + sizeof(header_t))
	/*
	 * datap might have been aligned in newzonedatanolock(), so get the new size
	 * (which includes room for the deadwood).
	 */
    	startsize = datap->size;

	bzero(datap, STARTHUNK);
	datap->size = STARTHUNK;
	freearray = (felem_t *)(datap + 1);
	freearray[0].size = 1 << 31;
	zonep->freearray = freearray;
	zonep->freen = 0;
	
	zonep->freemaxn = NUMFREES;
	datap = (header_t *) ((char *)datap + datap->size);
	datap->size = startsize - STARTHUNK;

	datap->lastguyfree = 0;
	datap->free = 0;
	zonep->newmem = 1;
	zonep->canfree = 1;
	nxzonefreenolock((NXZone*)zonep, datap + 1);
	zonep->canfree = canfree;
	zonep->newmem = 0;
	keepzonenolock(zonep);
    	dolockordebug(malloc_static.safesingle);
} UNLOCK 
	return((NXZone *)zonep);
}

extern BOOL NXProtectZone(NXZone *zone, NXZoneProtection protection) {
    zone_t *zonep = (zone_t *) zone;
    region_t *regionp = 0;
    BOOL hadError = NO;
    kern_return_t ret;
    vm_prot_t protect = VM_PROT_NONE;
    int i;
    
    switch (protection) {
	case NXZoneReadWrite:
	    protect = VM_PROT_ALL;
	    break;
	case NXZoneReadOnly:
	    protect = (VM_PROT_READ|VM_PROT_EXECUTE);
	    break;
	case NXZoneNoAccess:
	    protect = VM_PROT_NONE;
	    break;
	default:
	    print("Invalid argument %d passed to NXProtectZone\n", protection);
	    return NO;
    }
    
    for (i = 0; i < malloc_static.max_region; i++) {
	if (malloc_static.regions[i].zonep == zonep) {
	    regionp = &malloc_static.regions[i];
	    ret = vm_protect(mach_task_self(),
			     (vm_address_t)regionp->beginadd,
			     (vm_size_t)(regionp->endadd - regionp->beginadd),
			     0, protect);
	    if (ret != KERN_SUCCESS) 
		hadError = YES;
	}
    }
    return hadError ? NO : YES;
}

/*
 * Add this piece of memory to regions. Put deadwood where necessary.
 * Merge regions when neccessary.
 */
static void newzonedatanolock(zone_t *zonep, void **ptr, int size)
{
header_t *datap;
int	i,regionnum;
region_t *newregionp;
	
    datap = *ptr;
    regionnum = newregionnolock();
    for(i = 0 ; i < regionnum; i++) 
	if((int)datap < (int)malloc_static.regions[i].beginadd)
	    break;
	    
	    /* Try adding to existing regions */
	    
	        /* after */
    if(i > 0 && malloc_static.regions[i-1].zonep == zonep &&
	    (int)malloc_static.regions[i-1].endadd == (int)datap) {
	if(i < (regionnum - 1) && malloc_static.regions[i+1].zonep == zonep &&
		    (int)malloc_static.regions[i+1].beginadd == ((int)datap+size)) {
    		/* and before */
	    datap--;  /* backup over prevous dead wood */
	    			/* we gain lastguyfree flag here */
	    /* FIXME: Is the following line a noop? */
	    *datap = *((header_t*)(malloc_static.regions[i-1].endadd - sizeof(header_t)));

	    /*
	     * We need to pick up the deadwood, the new memory, and the memory between the
	     * beginning of the next region and its first header.  This algebras out to
	     * just the MALLOC_ALIGN'ment:
	     */
	    datap->size = size + MALLOC_ALIGN;
	    malloc_static.regions[i-1].endadd = malloc_static.regions[i+1].endadd;
	    malloc_static.max_region--;
	    delregionnolock(&malloc_static.regions[i+1]);
	    *ptr = datap;
	    return;
	}
	else {
	    datap--;  /* backup over prevous dead wood */
	    			/* we gain lastguyfree flag here */
	    /* FIXME: Is this a noop? */
	    *datap = *((header_t*)(malloc_static.regions[i-1].endadd - sizeof(header_t)));
	    datap->size = size;
	    *ptr = datap;
	    	/* add new dead wood */
	    datap = (header_t *)((int)datap + size);
	    datap->free = 0;
	    datap->lastguyfree = 1;
	    datap->size = sizeof(header_t);
	    malloc_static.regions[i-1].endadd += size;
	    malloc_static.max_region--;
	    return;
	}
    }
    		/* before */
    if(i < (regionnum - 1) && 
    		malloc_static.regions[i+1].zonep == zonep &&
	    	(int)malloc_static.regions[i+1].beginadd == ((int)datap+size)) {
	malloc_static.regions[i+1].beginadd = (char *)datap;

	/*
	 * We need to align datap to header_t alignment.  We don't need to worry
	 * about adjusting the size because the first header in the next region
	 * has been adjusted up in just the same way.
	 */
	datap = (header_t *)(ROUND_SIZE((unsigned)datap));
	*ptr = datap;

	datap->size = size;
	datap->free = 0;
	datap->lastguyfree = 0;
	malloc_static.max_region--;
	return;
    }
    
    if(i != regionnum) 
	bcopy(&malloc_static.regions[i],&malloc_static.regions[i+1],
			(regionnum - i)*sizeof(region_t));
		    
    newregionp = &malloc_static.regions[i];
    newregionp->beginadd = (char *)datap;
    newregionp->endadd = (char *)((int)datap + size);
    newregionp->zonep = zonep;   
    
    /*
     * Move first header up to header_t alignment and adjust the size of it appropriately.
     */
    size -= ROUND_SIZE((unsigned)datap) - (unsigned)datap;
    datap = (header_t *)(ROUND_SIZE((unsigned)datap));
    *ptr = datap;

    datap->size = size - sizeof(header_t);
    datap->free = 0;
    datap->lastguyfree = 0;
	/* add new dead wood */
    datap = (header_t *)((int)datap + size - sizeof(header_t));
    datap->free = 0;
    datap->lastguyfree = 1;
    datap->size = sizeof(header_t);
}


/*
 * Store away zone pointer.
 */
static void keepzonenolock(zone_t *zonep)
{
    if((malloc_static.max_zone + 1) > malloc_static.max_allocedzone) {
	malloc_static.max_allocedzone *= 2;
	malloc_static.zones = REALLOC(malloc_static.zones,
		malloc_static.max_allocedzone*sizeof(zone_t));
    }
    malloc_static.zones[malloc_static.max_zone++] = zonep;
}

/*
 * Create a new zone which uses memory from a parent zone.
 */
NXZone *NXCreateChildZone(NXZone *parentzonep, size_t startsize,
	    size_t granularity, int canfree)
{
zone_t	*zonep;
felem_t	*freearray;
header_t *datap,*deadwoodp;

    if(!malloc_static.inited)
	malloc_init();

    if(Z(parentzonep)->type != EXTERNAL)
	return(NX_NOZONE);
	
LOCK {	
    if(startsize < 128)
	startsize = 128;	
    if(granularity < 128)
	granularity = 128;

    zonep = MALLOC(sizeof(zone_t));
    zonep->granularity = granularity;
    zonep->type = INTERNAL;
    zonep->parentzonep = (zone_t *)parentzonep;
    zonep->debug = ((zone_t *)malloc_static.defaultzone)->debug;
    zonep->noshrink = 0;
    zonep->name = 0;
    zonep->z.destroy = nxdestroyzone;
    
    datap = nxzonemallocnolock((NXZone*)parentzonep, startsize);
    startsize = malloc_size(datap); /* could round up */
    	/* Break up old region and insert internal */
    break_region((char*)datap, startsize, zonep);
    checkregionsallocation();

#define DUMMYSIZE ROUND_SIZE(2*sizeof(header_t))
      /*
       * Put in a dummy used area which will be used when vaporizing zone.
       */
        datap->size = DUMMYSIZE;
	datap->free = 0;
	datap->lastguyfree = 0;

        startsize -= datap->size;
	datap = (header_t *)((char *)datap + datap->size);

	bzero(datap, STARTHUNK);
	datap->size = STARTHUNK;
	freearray = (felem_t *)(datap + 1);
	freearray[0].size = 1 << 31;
	zonep->freearray = freearray;
	zonep->freen = 0;
	zonep->freemaxn = NUMFREES;

	startsize -= datap->size;
	datap = (header_t *) ((char *)datap + datap->size);
	/*
	 * We are assuming here that the end of the memory allocated is followed
	 * by a header_t, so by backing of by a well-aligned amount, we will
	 * remain aligned to a reasonable header_t location.  We make a larger deadwood
	 * than in an external zone so that we can easily merge with our parent zone
	 * if this zone is freed.
	 */
	datap->size = startsize -  ROUND_SIZE(sizeof(header_t)) - sizeof(header_t);

	datap->lastguyfree = 0;
	datap->free = 0;
		    /* add dead wood at end */
	deadwoodp = (header_t *)((int)datap + datap->size);
	deadwoodp->free = 0;
	deadwoodp->lastguyfree = 1;
	deadwoodp->size = ROUND_SIZE(sizeof(header_t)) + sizeof(header_t);
	zonep->canfree = 1;
	nxzonefreenolock((NXZone*)zonep, datap + 1);
	zonep->canfree = canfree;
	keepzonenolock(zonep);
   	dolockordebug(malloc_static.safesingle);
} UNLOCK 	
	return((NXZone *)zonep);
}

static void dolockordebug(int fast)
{
int   i;

    malloc_static.safesingle = fast;
    for(i = 0;i < malloc_static.max_zone; i++) {
	if(malloc_static.zones[i]->canfree)
	    malloc_static.zones[i]->z.malloc = 
	    		(fast ? nxzonemallocnolock : nxzonemalloc);
	else
	    malloc_static.zones[i]->z.malloc = 
	    		(fast ? mallocnofreenolock : mallocnofree);
	malloc_static.zones[i]->z.realloc = 
			(fast ? nxzonereallocnolock : nxzonerealloc);
	malloc_static.zones[i]->z.free = 
			(fast ? nxzonefreenolock : nxzonefree);
    }
}

/*
 * Get page aligned data and mark it as used by a specific zone.
 */
static void *_valloczonenolock(size_t *size)
{
char	*data;

    *size = round_page(*size);
    data = 0;
#ifdef TESTING    
    if (MYvm_allocate(mach_task_self(),(vm_address_t *) &data, *size, 1) != KERN_SUCCESS)
#else
    if (vm_allocate(mach_task_self(),(vm_address_t *) &data, *size, 1) != KERN_SUCCESS)
#endif    
	MallocErrorFunc(MALLOC_ERROR_VM_ALLOC);

    return(data);
}


/*
 * Rip out part of a region. Possibly breaking into 2 pieces. 
 * If newzone is non-zero create a new region out of the piece.
 */
static	 void  break_region(char *start, int size, zone_t  *newzonep)
{
region_t   *regionp;
int	end;

    regionp = findregionnolock(start);
    
    if(start == regionp->beginadd) {
         /* cutoff beginning */
	 regionp->beginadd = start + size;
	 if(regionp->beginadd == regionp->endadd) {
		/* waste it its empty */
	    delregionnolock(regionp);
	 }
    } else if((start + size) == regionp->endadd) {
	    /* cutoff end */
	regionp->endadd = start;
    } else {
	    /* split in 2 */
	    /* trim top */
	end = (int) regionp->endadd;
	regionp->endadd = start;
	    /* add bottom */
	addregionnolock(start+size,end - ((int)start + size),regionp->zonep);
    }
    
    if(newzonep) 
	addregionnolock(start,size,newzonep);
}

static int newregionnolock()
{
int    regionnum = malloc_static.max_region++;
    if(malloc_static.max_region > malloc_static.max_allocedregion) 
	abort();   /* Should never happen */ 
    return(regionnum);
}


static void checkregionsallocation()
{
char *freelater;
	/*
	 * We must always keep 3 spare regions around (rather than 2).
	 * break_region() can require 2 new regions, but the MALLOC() here
	 * may require 1 more if nxzonemallocnolock() calls morememnolock().
	 */
    if((malloc_static.max_region+3) > malloc_static.max_allocedregion) {
	malloc_static.max_allocedregion *= 2;
        /*
         * REALLOC cannot be used, since the freeing of the old
         * malloc_static.regions memory occurrs at the end of realloc. 
 	 * This free needs to access malloc_static.regions.
  	 */
	freelater = (char *)malloc_static.regions;
	malloc_static.regions = 
		MALLOC(malloc_static.max_allocedregion*sizeof(region_t));
          bcopy(freelater,malloc_static.regions,
		(malloc_static.max_allocedregion/2)*sizeof(region_t));
	FREE(freelater);
    }
}

static void delregionnolock(region_t *regionp)
{
int n;

    n = (int)(&malloc_static.regions[malloc_static.max_region-1] - regionp);
    if(n > 0) 
	bcopy(regionp+1,regionp,n*sizeof(region_t));
     malloc_static.max_region--;
}

static void delzonenolock(zone_t *zonep)
{
int	i;

    for(i = 0; i < malloc_static.max_zone; i++)
	if(malloc_static.zones[i] == zonep)
	    break;

    bcopy(&malloc_static.zones[i+1],&malloc_static.zones[i],
			(malloc_static.max_zone-i-1)*sizeof(region_t));
     malloc_static.max_zone--;
     if(zonep->name)
	FREE(zonep->name);
     FREE(zonep);
}


/*
 * Insert a new region into the regions array.
 * Keep it sorted.
 */
static region_t *addregionnolock(char *start, int size, zone_t *zonep)
{
int	i,regionnum;
region_t *newregionp;
	
    regionnum = newregionnolock();
    for(i = 0 ; i < regionnum; i++) 
	if(start < malloc_static.regions[i].beginadd)
	    break;
	    
	    /* Add to existing region */
	        /* after */
    if(zonep != 0 && i > 0 && malloc_static.regions[i-1].zonep == zonep &&
	    malloc_static.regions[i-1].endadd == start) {
	malloc_static.regions[i-1].endadd += size;
	malloc_static.max_region--;
	return((region_t*)0);
    }
    		/* before */
    if(zonep != 0 && i < (regionnum - 1) && 
    		malloc_static.regions[i+1].zonep == zonep &&
	    	(int)malloc_static.regions[i+1].beginadd == ((int)start+size)) {
	malloc_static.regions[i+1].beginadd = start;
	malloc_static.max_region--;
	return((region_t*)0);
    }
    
    if(i != regionnum) 
	bcopy(&malloc_static.regions[i],&malloc_static.regions[i+1],
			(regionnum - i)*sizeof(region_t));
		    
    newregionp = &malloc_static.regions[i];
    newregionp->beginadd = start;
    newregionp->endadd = start + size;
    newregionp->zonep = zonep;   
    return(newregionp); 
}

/*
 * Find the region that this pointer belongs to.
 */
static region_t *findregionnolock(void *ptr)
{
int	low,high,mid,lastmid;
region_t *regionp,*regions;

    high = malloc_static.max_region - 1;
    regions = malloc_static.regions;
    
    if(malloc_static.lasthitregion <= high) {
    	regionp = &regions[malloc_static.lasthitregion];
	if((char *)ptr < regionp->endadd && (char *)ptr >= regionp->beginadd)
	    return(regionp);
	}
		
    
    if(high < 10) {
	for(;high >= 0; high--) {
	    if((char *)ptr < regions->endadd && (char *)ptr >= regions->beginadd) {
		malloc_static.lasthitregion = regions - malloc_static.regions;
		return(regions);
		}
	    regions++;
	}
	return(0);
    }
    else {
    low = 0;
    
    mid = (high + low) / 2;  
    regionp = &regions[mid];
    
    if((char *)ptr >= regions[high].beginadd && (char *)ptr < regions[high].endadd)
	return(&regions[high]);

    while(1) {  
	if((char *)ptr < regionp->beginadd)
	    high = mid;
	else if((char *)ptr >= regionp->endadd) 
	    low = mid;
	else {
	    malloc_static.lasthitregion = regionp - malloc_static.regions;
	    return(regionp);
	    }
	    
	lastmid = mid;
	mid = (high + low) / 2;
	if(mid == lastmid)
	    return(0);
	regionp = &regions[mid];
    }
    
    }
    
    
}
 
static void *nxzonemalloc(NXZone *z, size_t size)
{
void *new;
    LOCK {
	if(Z(z)->debug & MALLOC_DEBUG_BEFORE) 
	    malloc_check(MALLOC_ENTER);
	
	new = nxzonemallocnolock(z, size);
	if(Z(z)->debug & MALLOC_DEBUG_AFTER) 
	    malloc_check(MALLOC_EXIT);
		
    } UNLOCK 
    return(new);
}

static void *mallocnofree(NXZone *z, size_t size)
{
void *new;
    LOCK {
	new = mallocnofreenolock(z, size);
    } UNLOCK 
    return(new);
}


static void *nxzonemallocnolock(NXZone *z, size_t size)
{
zone_t	*zonep = (zone_t *)z;
felem_t	*freearray;
int freen;
unsigned int sizeleft, sizeright;
int	n;
header_t	*new,*old;

    FREEONEDEEP
    
    if(size <= 0)
	size = 1;
    size = ROUND_SIZE(size) + sizeof(header_t);
	    /*
	     * Look for a piece in the free list this big.
	     */
    freearray = zonep->freearray;
    
    if(zonep->freen == 0 || freearray[1].size < size)
	return(morememnolock(zonep, size));
	
    freen = zonep->freen - 1;
    for(n = 2;;) {	
	if(n < freen) {
	    sizeleft = freearray[n].size;
	    sizeright = freearray[n+1].size;
	}
	else if(n == freen) {
	    sizeleft = freearray[n].size;
	    sizeright = 0;
	}
	else
	    break;
		    
	if(sizeright >= size) {
	    if(sizeright < sizeleft || sizeleft < size)
	    	n++;   /* go right */
	}
	else if(sizeleft < size)
	    break;
	    
	n <<= 1;
	}
	n >>= 1;
   if(n > zonep->freen)
	abort();
    freearray += n;
/*
 * We have found an item in the free heap that 
 * is big enough. We either return the whole thing, or rip of the top of
 * the item and leave the rest.
 */

    new = (header_t *)freearray->data;
    new->free = 0;

#define MINMEMORYSIZE  (sizeof(header_t) + ROUND_SIZE(sizeof(int)))	
    if((freearray->size - size) > MINMEMORYSIZE) {
	/* just rip off the top */
	new->size = size;
	freearray->data += size;
	freearray->size -= size;
	old = (header_t *)freearray->data;
	old->size = freearray->size;
	old->free = 1;
	old->lastguyfree = 0;
	downheap(zonep->freearray, n, zonep->freen);
    }
    else {
	/* take the whole thing */
	heapremove(zonep,n);
	    /* Mark that the next guys lastguy is not free */
	old = (header_t *)((int)new + new->size);
	old->lastguyfree = 0;
    }
    
    return(new + 1);
}

/*
 * Used when we know we will never free.
 */
static void *mallocnofreenolock(NXZone *z, size_t size)
{
zone_t	*zonep = (zone_t *)z;
felem_t	*freearray;
header_t	*new,*old;

    if(size <= 0)
	size = 1;
    size = ROUND_SIZE(size) + sizeof(header_t);
	    /*
	     * Look for a piece in the free list this big.
	     */
    freearray = zonep->freearray;
    
    if(zonep->freen == 0 || freearray[1].size < size)
	return(morememnolock(zonep, size));
	
    freearray ++;
/*
 * We have found an item in the free heap that 
 * is big enough. We either return the whole thing, or rip of the top of
 * the item and leave the rest.
 */

    new = (header_t *)freearray->data;
    new->free = 0;

    if((freearray->size - size) > MINMEMORYSIZE) {
	/* just rip off the top */
	new->size = size;
	freearray->data += size;
	freearray->size -= size;
	old = (header_t *)freearray->data;
	old->size = freearray->size;
	old->free = 1;
	old->lastguyfree = 0;
    }
    else {
	/* take the whole thing */
	heapremove(zonep,1);
	    /* Mark that the next guys lastguy is not free */
	old = (header_t *)((int)new + new->size);
	old->lastguyfree = 0;
    }
    
    return(new + 1);
}


/*
 * Get a new hunk of memory big enough to fullfill this request.
 */
static void *morememnolock(zone_t *zonep, size_t size)
{
header_t *datap,*deadwoodp;
void	*newptr;
int	nsize;
int	canfree;

    /* get more memory */
    nsize = size + ROUND_SIZE(2*sizeof(header_t)) /* dummy block */
	    + ROUND_SIZE(sizeof(header_t)) + sizeof(header_t); /* dead wood piece at end */
    if(nsize < zonep->granularity)
	nsize = zonep->granularity;
	
    if(zonep->type == EXTERNAL) {
	datap = (header_t *)_valloczonenolock((size_t *)&nsize);
	if(!datap)
	    return(0);
	newzonedatanolock(zonep,(void **) &datap, nsize);
    }
    else {
	if(!(datap = internal_zone_extend(zonep->parentzonep, zonep, nsize))) {
	    datap = nxzonemallocnolock((NXZone*)zonep->parentzonep, nsize);
	    /* I think this was a bug */
	    nsize = malloc_size(datap);
		/* Break up old region and insert internal */
	    break_region((char*)datap, nsize, zonep);
	    checkregionsallocation();
	       /* 
		* Add dummy used block needed for vaporizing.
		*/
		datap->size = DUMMYSIZE;
		datap->free = 0;
		datap->lastguyfree = 0;
		nsize -= datap->size;
		datap = (header_t *)((char *)datap + datap->size);
			/* add dead wood at end */
	        /*
		 * Since we allocated this space from our parent zone, the end of the
		 * data should be header_t aligned.  We back off by a
		 * (ROUND_SIZE(sizeof(header_t)) + sizeof(header_t)) in order to remain
		 * header_t aligned.
		 */
		nsize -= ROUND_SIZE(sizeof(header_t)) + sizeof(header_t);
		deadwoodp = (header_t *)((int)datap + nsize);
		deadwoodp->free = 0;
		deadwoodp->lastguyfree = 1;
		deadwoodp->size = ROUND_SIZE(sizeof(header_t)) + sizeof(header_t);
		    /* get first header ready */
		datap->size = nsize;
		datap->lastguyfree = 0;
		datap->free = 0;
	}
    }


    canfree = zonep->canfree;
    zonep->newmem = 1;
    zonep->canfree = 1;
    nxzonefreenolock((NXZone*)zonep, datap + 1);
    zonep->canfree = canfree;
    zonep->newmem = 0;
    newptr = nxzonemallocnolock((NXZone*)zonep, size - sizeof(header_t));
    checkregionsallocation(); /* Its safe to allocate now */
    return(newptr);
}


/*
 * Try to extend an internal zone by reallocing one of its pieces.
 * If this is not possible return null.
 */
static void *
internal_zone_extend(zone_t *parentzonep, zone_t *zonep, int  nsize)
{
 int	i,oldsize;
char	*add,*newadd;
header_t *deadwoodp,*datap;

    for(i = 0; i < malloc_static.max_region; i++) {
	if(malloc_static.regions[i].zonep == zonep) {
	    add = malloc_static.regions[i].beginadd;
	    oldsize = malloc_size(add);
	    newadd = NXZoneRealloc_canyou(parentzonep, add, oldsize + nsize);
	    if(newadd) {
	    		/* Get exact size. Realloc may have rounded up slightly */
		nsize = malloc_size(newadd) - oldsize; 
		malloc_static.regions[i].endadd = malloc_static.regions[i].beginadd +
				malloc_size(newadd);
		malloc_static.regions[i+1].beginadd = malloc_static.regions[i].endadd;
			/* add dead wood at end */
		deadwoodp = (header_t *)
			((int)malloc_static.regions[i].endadd
			 - ROUND_SIZE(sizeof(header_t)) - sizeof(header_t));
		deadwoodp->free = 0;
		deadwoodp->lastguyfree = 1;
		deadwoodp->size = ROUND_SIZE(sizeof(header_t)) + sizeof(header_t);

			/* backup over last deadwood on return */
		datap = (header_t*)((int)add + oldsize
				    - ROUND_SIZE(sizeof(header_t)) - sizeof(header_t));

		datap->free = 0;
		/* we inherit the lastguyfree flag */
		datap->size = nsize;
		return((void*)datap);
		}
	    }
	}
	return(0);
}   

NXZone *NXZoneFromPtr(void *ptr)
{
region_t *regionp;
NXZone *zonep;

    LOCK {
	regionp = findregionnolock(ptr);
	if(regionp)
	    zonep = (NXZone*)regionp->zonep;
	else
	    zonep = NX_NOZONE;
    } UNLOCK;
    return zonep;
}

/*
 * Given a pointer. Check if it is in the heap.
 * If so print out the info for the piece it belongs to.
 */
void NXZonePtrInfo(void *ptr)
{
region_t *regionp;
header_t *headerp;

    regionp = findregionnolock(ptr);
    if(regionp) {
	if(regionp->zonep) {
	    headerp = (header_t *)ROUND_SIZE((unsigned)regionp->beginadd);
	    while((int)headerp < (int)regionp->endadd) {
		if((int)ptr >= (int)headerp && 
				(int)ptr < ((int)headerp + headerp->size)) {
		    print("start add 0x%x size %d free %d\n",(unsigned int)(headerp+1),
		    	(unsigned int)(headerp->size - sizeof(header_t)),headerp->free);
		    print("zone 0x%x ",(unsigned int)regionp->zonep);
		    if(regionp->zonep->name)
			print("%s\n",regionp->zonep->name);
		    else
			print("\n");
		    return;
		}
		headerp = (header_t *)((int)headerp + headerp->size);
	    }
	}
	else {
	    print("valloced -- start add 0x%x size %ld free 0\n",
	    	(unsigned int)regionp->beginadd,
(long int)(regionp->endadd - regionp->beginadd));
	    return;
	   }
	    		
	
    }
    print("ptr not in heap\n");
}

void           *
NXZoneCalloc(NXZone *zonep, size_t numElems, size_t byteSize)
{
    char           *new;

    byteSize *= numElems;
    new = NXZoneMalloc(zonep, byteSize);
    /* Return NULL rather than crashing. */
    if (new) {
	bzero(new, byteSize);
    }
    return (new);
}

static void *nxzonerealloc(NXZone *zonep, void *ptr, size_t size)
{
void	*new;
    LOCK {
	if(Z(zonep)->debug & MALLOC_DEBUG_BEFORE) 
	    malloc_check(REALLOC_ENTER);
	new = nxzonereallocnolock(zonep,ptr, size);
	if(Z(zonep)->debug & MALLOC_DEBUG_AFTER) 
	    malloc_check(REALLOC_EXIT);
    } UNLOCK 
    return(new);
}

static void *nxzonereallocnolock(NXZone *zonep, void *ptr, size_t size)
{
char	*new;
int	oldsize;

    FREEONEDEEP
	
    if(!ptr)
	return(nxzonemallocnolock(zonep, size));
	
    if(NOTPAGEALIGNED(ptr) && (new = NXZoneRealloc_canyou((zone_t *)zonep, ptr, size)))
    ;
    else { 
	oldsize = malloc_size(ptr);
        new = nxzonemallocnolock(zonep, size);
	if(size < oldsize)
	    oldsize = size;
	bcopy(ptr, new, oldsize);
		/* We hitch a ride on the 1 deep freeing stuff here, we can't just 
		   call nxzonefreenolock, because it could be a valloc ptr.
		   */
	malloc_static.freeme = ptr; 
	FREEONEDEEP
    }
    return(new);
}

/*
 * Try to reallocate this piece of memory to this size without moving the 
 * piece. Return the old pointer if successfull.
 */
static void *NXZoneRealloc_canyou(zone_t *zonep, void *ptr, size_t size)
{
int	nextspot,delta;
felem_t *nextfreearray;
header_t	*headerp,*next,*newp;

    nextspot = 0;
    if(size <= 0)
	size = 1;
    size = ROUND_SIZE(size) + sizeof(header_t);
    
    headerp = (header_t *)ptr;
    headerp--; /* backup to get at header */
    
    if(headerp->free) {
	if(zonep->debug & MALLOC_DEBUG_CATCHFREE) 
	    MallocErrorFunc(MALLOC_ERROR_FREE_FREE); 
	return(0);
    }
    
    
    if(size <= headerp->size) {
	delta = headerp->size - size;
	if(delta > 20) { /* is it worth trimming?? */
	    headerp->size = size;
	    newp = (header_t *) ((int)headerp + size);
	    newp->size = delta;
	    newp->lastguyfree = 0;
	    newp->free = 0;
	    nxzonefreenolock((NXZone*)zonep, newp + 1);
	}
	return(ptr);
    }
    
    nextfreearray = 0;
    next = (header_t *) ((int)headerp + headerp->size);
    if(next->free) {
	nextspot = ((int *)next)[next->size / sizeof(int) - 1];
	nextfreearray = &zonep->freearray[nextspot];
	}
    
    
    delta = size - headerp->size;
    if(nextfreearray && nextfreearray->size >= delta) {
	if((nextfreearray->size - delta) > 8) {
	    /* just rip off the top */
	    headerp->size += delta;
	    nextfreearray->data += delta;
	    nextfreearray->size -= delta;
	    headerp = (header_t *)nextfreearray->data;
	    headerp->free = 1;
	    headerp->size = nextfreearray->size;
	    headerp->lastguyfree = 0;
	    downheap(zonep->freearray, nextspot, zonep->freen);
	}
	else {
	    /* take the whole thing */
	    headerp->size += nextfreearray->size;
	    heapremove(zonep,nextspot);

		/* Mark that the next guys lastguy is not free */
	    headerp = (header_t *)((int)headerp + headerp->size);
	    headerp->lastguyfree = 0;
	}
	return(ptr);
    }
    return(0);
}

static void nxzonefree(NXZone *z, void *ptr)
{
    LOCK {
	if(ptr) {
	    if(Z(z)->debug & MALLOC_DEBUG_BEFORE)
	    	malloc_check(FREE_ENTER); 
	    nxzonefreenolock(z, ptr);
	    if(Z(z)->debug & MALLOC_DEBUG_WASTEFREE) {
		if(ISFREEMEMEMORY(ptr))
		    bzero(ptr, malloc_size(ptr) - sizeof(int));
		}
	    if(Z(z)->debug & MALLOC_DEBUG_AFTER) 
	    	malloc_check(FREE_EXIT);
	}
    } UNLOCK 
}

static void nxzonefreenolock(NXZone *z, void *ptr)
{
zone_t	*zonep = (zone_t *)z;
int	lastspot,nextspot,newsize;
felem_t *lastfreearray,*nextfreearray,myfree;
header_t	*headerp,*next;


    if(!zonep->canfree)
	return;
    
    lastspot = 0;
    nextspot = 0;
    headerp = (header_t *)ptr;
    headerp--; /* backup to get at header */
    
    if(headerp->free) {
	if(Z(z)->debug & MALLOC_DEBUG_CATCHFREE) 
	    MallocErrorFunc(MALLOC_ERROR_FREE_FREE);
	return;
    }
    
    nextfreearray = lastfreearray = 0;
    if(headerp->lastguyfree) {
	lastspot = ((int *)headerp)[-1];
	lastfreearray = &zonep->freearray[lastspot];
    }
    
    next = (header_t *) ((int)headerp + headerp->size);
    if(next->free) {
	nextspot = ((int *)next)[next->size / sizeof(int) - 1];
	nextfreearray = &zonep->freearray[nextspot];
	}
    
    if(lastfreearray) {
    	if(nextfreearray) {
	    /* combine all 3 spaces waste the lastone */
	    newsize = lastfreearray->size + nextfreearray->size + headerp->size;
	    heapremove(zonep, nextspot);
		/* must revalidate the position since remove can move it */
	    lastspot = ((int *)headerp)[-1];
	    lastfreearray = &zonep->freearray[lastspot];
	    lastfreearray->size = newsize;
	    headerp = (header_t *) lastfreearray->data;
	    headerp->size = newsize;
	    upheap(zonep->freearray, lastspot);
	}
	else {
	    /* combine the 2 */
	    lastfreearray->size += headerp->size;
	    headerp = (header_t *) lastfreearray->data;    
	    headerp->size = lastfreearray->size;
	    upheap(zonep->freearray, lastspot);
	    next->lastguyfree = 1;
	}
    }
    else {
	if(nextfreearray) {
	    /* combine the 2 */
	    nextfreearray->size += headerp->size;
	    nextfreearray->data -= headerp->size;
	    headerp = (header_t *) nextfreearray->data;    
	    headerp->size = nextfreearray->size;
	    headerp->free = 1;
	    upheap(zonep->freearray, nextspot);
	    headerp->lastguyfree = 0;
	}
	else {
	    /* make a new free element */
	    myfree.data = (char *)headerp;
	    myfree.size = headerp->size;
	    heapadd(zonep, &myfree);
	    next->lastguyfree = 1;
	    headerp->free = 1;
	    headerp->lastguyfree = 0;
	}
    }
    
       
#define GIVEBACK
#ifdef GIVEBACK
    /* Try to give back memory to OS */
    /* Give back entire regions, or end of regions that are greater than the 
    * zone granularity.
    * Make sure we are not just at the end of a external part of a region followed
    * by an internal piece.
    */
    if((headerp->size + sizeof(header_t)) >= vm_page_size && 
    			zonep->type == EXTERNAL && !zonep->newmem) {
	void	*startpage;
	int	heapspot,leftover,relsize;
	region_t *regionp,*nextregionp,*lastregionp;
	felem_t	*freearray;
	
		/* See if this whole piece is a region */
	    /*
	     * This is a little weird, but we truncate the headerp down to the
	     * MALLOC_ALIGN when comparing it to page and region boundries in case it is
	     * the very first headerp in the region.  Remember, when we allocate new
	     * regions, we push the first header up to header_t alignment which might
	     * leave a little unused space at the beginning.
	     */
	    startpage = (void *)round_page((unsigned)headerp & ALIGN_MASK);
	    if(((unsigned)headerp & ALIGN_MASK) == (unsigned)startpage) {
		regionp = findregionnolock(headerp);
		if(regionp < &malloc_static.regions[malloc_static.max_region-1])
		    nextregionp = regionp+1;
		else
		    nextregionp = 0;
		if(regionp > malloc_static.regions)
		    lastregionp = regionp-1;
		else
		    lastregionp = 0;
		/* See comment above */
		if(((int)regionp->beginadd == ((int)headerp & ALIGN_MASK) && 
			 ((int)regionp->endadd - sizeof(header_t)) ==
			    		 ((int)headerp+headerp->size)) &&
		!(nextregionp && nextregionp->zonep && 
			nextregionp->zonep->type == INTERNAL) &&
		!(lastregionp && lastregionp->zonep && 
			lastregionp->zonep->type == INTERNAL) 
		) {
		    heapspot = ((int *)headerp)[headerp->size / sizeof(int) - 1];
		    if(vm_deallocate(mach_task_self(),((int)headerp & ALIGN_MASK), 
			    headerp->size + sizeof(header_t)) != KERN_SUCCESS)
			MallocErrorFunc(MALLOC_ERROR_VM_DEALLOC);
		    delregionnolock(regionp);
		    heapremove(zonep,heapspot);
		    }
	    }
	else if(headerp->size  >= zonep->granularity) {
		leftover = headerp->size - zonep->granularity;
		if(leftover == 0 || leftover > MINMEMORYSIZE) {
		    regionp = findregionnolock(headerp);
		    if(regionp < &malloc_static.regions[malloc_static.max_region-1])
			nextregionp = regionp+1;
		    else
			nextregionp = 0;
		    if(!(nextregionp && nextregionp->zonep &&
		    		nextregionp->zonep->type == INTERNAL) &&
		    		((int)regionp->endadd - sizeof(header_t)) ==
						((int)headerp+headerp->size)) {
				/* Deallocate end of region */
		heapspot = ((int *)headerp)[headerp->size / sizeof(int) - 1];
		
		leftover = headerp->size % zonep->granularity;
				/* 
				 * We know we can do this because of leftover
				 * test above.
				 */
		if(leftover < MINMEMORYSIZE) leftover += zonep->granularity;

		relsize = headerp->size - leftover;
		regionp->endadd	-= relsize;
		
		if(leftover == 0) {
		    headerp->size = sizeof(header_t); /* make into dead wood */
		    headerp->free = 0;
		    heapremove(zonep,heapspot);
		}
		else {
		    headerp->size -= relsize;
		    freearray = &zonep->freearray[heapspot];
		    freearray->size = headerp->size;
		    downheap(zonep->freearray, heapspot, zonep->freen);
		    	/* put in dead wood */
		    headerp = (header_t *)((int)headerp + headerp->size);
		    headerp->size = sizeof(header_t);
		    headerp->free = 0;
		    headerp->lastguyfree = 1;
		}
		if(vm_deallocate(mach_task_self(),(int)(headerp + 1), 
			relsize) != KERN_SUCCESS)
		    MallocErrorFunc(MALLOC_ERROR_VM_DEALLOC);
	    }
	}
	}
    }
#endif	
 /* 
 * Shrink or expand the size of the freearray.
 * Must be called at a safe point any time entries are used.
 */
{
void	*freethis;
felem_t *freearray;
#define SHRINKFREELIST
#ifdef SHRINKFREELIST
    if(zonep->freen < zonep->freemaxn / 4 && !zonep->noshrink && 
    		zonep->freemaxn > 2*NUMFREES && zonep->freemaxn > 16*FREESLOTSNEEDED) {
	zonep->freemaxn /= 2;
	freearray = nxzonemallocnolock((NXZone*)zonep,zonep->freemaxn*sizeof(felem_t));
	bcopy(zonep->freearray, freearray,(zonep->freen+1) *sizeof(felem_t));
	freethis = zonep->freearray;
	zonep->freearray = freearray;
	nxzonefreenolock((NXZone*)zonep,freethis);    
    } 
    else 
#endif    
    	{
    	/* We need loop because the calls used could possibly use up entries */
	while((zonep->freen + FREESLOTSNEEDED) >= zonep->freemaxn) {
	    /*zonep->freemaxn = zonep->freemaxn*2 + 1;*/
	    zonep->freemaxn += zonep->freemaxn / 2 + 1;
	    freearray = nxzonemallocnolock((NXZone*)zonep,
	    		zonep->freemaxn*sizeof(felem_t));
	    bcopy(zonep->freearray, freearray,(zonep->freen+1) *sizeof(felem_t));
	    freethis = zonep->freearray;
	    zonep->freearray = freearray;
	    nxzonefreenolock((NXZone*)zonep,freethis);    
	}
    }
}

}

/* 
 * Shrink or expand the size of the freearray of a zone.
 * Must be called at a safe point any time entries are used.
 */
static void checkfreearrayallocation(zone_t *zonep, int musthave)
{
void	*freethis;
felem_t *freearray;

    	/* We need loop because the calls used could possibly use up entries */
	while((zonep->freen + musthave) >= zonep->freemaxn) {
	    zonep->freemaxn = zonep->freemaxn*2 + 1;
	    freearray = nxzonemallocnolock((NXZone*)zonep,
	    		zonep->freemaxn*sizeof(felem_t));
	    bcopy(zonep->freearray, freearray,(zonep->freen+1) *sizeof(felem_t));
	    freethis = zonep->freearray;
	    zonep->freearray = freearray;
	    zonep->noshrink = 1;
	    nxzonefreenolock((NXZone*)zonep,freethis);    
	    zonep->noshrink = 0;
	}
}

/*
 * print out a particular memory heap.
 */
static void dumpheap(felem_t *freearray, int maxn)
{
int	n,i;
header_t *headerp;

    for(n = 1; n <= maxn; n++) {
	print("n %d size 0x%x data %x \n",n,freearray[n].size,(unsigned int)freearray[n].data);
	if((2*n) <= maxn)
	    if(freearray[2*n].size > freearray[n].size)
		print("**Bad heap: left child to big 0x%x\n",freearray[2*n].size);
	if((2*n+1) <= maxn)
	    if(freearray[2*n+1].size > freearray[n].size)
		print("**Bad heap: right child to big 0x%x\n",freearray[2*n+1].size);
	headerp = (header_t *)freearray[n].data;
	if(!headerp->free)
	    print("**Bad free area not marked free\n");
	if(headerp->size != freearray[n].size)
	    print("**Bad heap: free area size is 0x%x\n",headerp->size);
	i = ((int *)headerp)[headerp->size / sizeof(int) - 1];
	if(i != n)
	    print("**Bad heap: back pointer to free heap is bad\n");
	}

}

/*
 * verify a free heap is good.
 */
static int checkheap(int canfree, felem_t *freearray, int maxn)
{
int	n,i;
header_t *headerp;

    for(n = 1; n <= maxn; n++) {
	headerp = (header_t *)freearray[n].data;
	if(!headerp->free) {
	    stdmessage("**Bad free area not marked free\n");
	    return(1);
	    }
	if(headerp->size != freearray[n].size) {
	    stdmessage("**Bad heap: bad free area size\n");
	    return(1);
	    }
	i = ((int *)headerp)[headerp->size / sizeof(int) - 1];
	if(i != n) {
	    stdmessage("**Bad heap: back pointer to free heap is bad\n");
	    return(1);
	    }
	}
    return(0);
}

/*
 * Check the free heaps and the all the zones.
 *
 */
int NXMallocCheck(void)
{
    if(malloc_checkfreeheaps())
	return(1);
    if(malloc_checkzones())
	return(1);
    return(0);
}

static int malloc_checkfreeheaps()
{
int i;
    for(i = 0; i < malloc_static.max_zone; i++) {
	if(checkheap(malloc_static.zones[i]->canfree,
		malloc_static.zones[i]->freearray, malloc_static.zones[i]->freen))
	    return(1);
    }
    return(0);
}


void _NXMallocDumpFrees(void)
{
int	i;
char 	*type;
zone_t  *zonep;
    print("number of regions used %d allocated %d\n",malloc_static.max_region,
    		malloc_static.max_allocedregion);
    for(i = 0; i < malloc_static.max_region; i++) {
	zonep = malloc_static.regions[i].zonep;
	if(zonep != 0) {
	    if(zonep->type == INTERNAL)
		type = "INTERNAL";
	    else
		type = "EXTERNAL";
	}
	else
	    type = "VALLOC";
	print("beginadd %x endadd %x zone 0x%x type %s ",
		(unsigned int)malloc_static.regions[i].beginadd,
		(unsigned int)malloc_static.regions[i].endadd,(unsigned int)zonep,type);
	if(zonep && zonep->name)
	    print("%s\n",zonep->name);
	else
	    print("\n");
    }
    print("number of zones used %d allocated %d\n",malloc_static.max_zone,
    		malloc_static.max_allocedzone);
    for(i = 0; i < malloc_static.max_zone; i++) {
	print("freen %d freemaxn %d \n",malloc_static.zones[i]->freen,
		malloc_static.zones[i]->freemaxn);
	print("The free heap ---------------\n");
	dumpheap(malloc_static.zones[i]->freearray, malloc_static.zones[i]->freen);
    }
}

static int malloc_checkzones(void)
{
int	i,lastspot;
header_t *headerp,*lastheaderp;
region_t *regionp;
zone_t	*zonep;
felem_t *lastfreearray;
int	max_size,size;

	/*
	 * Find largest possible malloc piece size.
	 */
    max_size = 0;
    for(i = 0; i < malloc_static.max_region; i++)  {
	size = malloc_static.regions[i].endadd -
		    malloc_static.regions[i].beginadd;
    	if(size > max_size)
	   max_size = size;
    }
    for(i = 0; i < malloc_static.max_region; i++) {
	if(malloc_static.regions[i].zonep != 0) {
	    lastheaderp = 0;
	    zonep = malloc_static.regions[i].zonep;
	    /*
	     * The first header in an EXTERNAL region is aligned up from the
	     * base of the region.
	     */
            if (zonep->type == INTERNAL)
	      headerp = (header_t *)malloc_static.regions[i].beginadd;
	    else
	      headerp = (header_t *)
	               ROUND_SIZE((unsigned)malloc_static.regions[i].beginadd);
	    while((int)headerp < (int)malloc_static.regions[i].endadd) {
		if(headerp->size == 0) {
		    stdmessage("bad zone piece size of zero\n");
		    return(1);
		}
		/*
		 * If we create a child zone that is larger than
		 * all the other regions, it's header will sit right at
		 * the end of the previous region.  When we check for
		 * headers with bogus sizes, we need to make sure we
		 * ignore headers associated with child zones.
		 */
		if(headerp->size > max_size &&
		   (int)(headerp + 1) < (int)malloc_static.regions[i].endadd) {
		    stdmessage("Smashed zone. Header size invalid\n");
		    return(1);
		}
		if(headerp->lastguyfree) {
		    lastspot = ((int *)headerp)[-1];
		    if(lastspot < 1 || lastspot > zonep->freen) {
			stdmessage("back pointer number to the lastfree is bad\n");
			return(1);
		    }
		    lastfreearray = &zonep->freearray[lastspot];
		    if((int)(lastfreearray->data) != (int)lastheaderp) {
			stdmessage("back pointer to last free is bad\n");
			return(1);
		    }
		    if(!lastheaderp->free) {
			stdmessage("lastfree is not really free\n");
			return(1);
		    }
		}	
		lastheaderp = headerp;		
		headerp = (header_t *)((int)headerp + headerp->size);
	    }
	    if((int)headerp != (int)malloc_static.regions[i].endadd) {
		regionp = findregionnolock(headerp); /* check for internal */
		if(!regionp || regionp->zonep != malloc_static.regions[i].zonep) {
		    stdmessage("end of last piece did not match end of zone\n");
		    return(1);
		    }
	    }
	    if(lastheaderp->size > (sizeof(header_t) + ROUND_SIZE(sizeof(header_t))) &&
	       !((i < (malloc_static.max_region - 1) &&
	    	malloc_static.regions[i+1].zonep->type == INTERNAL))) {
		stdmessage("last piece in region bad\n");
		return(1);
	    }
	}
    }
    return(0);
}



void _NXMallocDumpZones(void)
{
int	i,lastspot;
header_t *headerp,*lastheaderp;
char *type;
zone_t	*zonep;
felem_t *lastfreearray;

    for(i = 0; i < malloc_static.max_region; i++) {
	zonep = malloc_static.regions[i].zonep;
	if(zonep != 0) {
	    if(zonep->type == INTERNAL)
		type = "INTERNAL";
	    else
		type = "EXTERNAL";
	}
	else
	    type = "VALLOC";
	print("beginadd %x endadd %x zonep 0x%x type %s ",
		(unsigned int)malloc_static.regions[i].beginadd,
		(unsigned int)malloc_static.regions[i].endadd, (unsigned int)zonep, type);
		
	if(zonep && zonep->name)
	    print("%s\n",zonep->name);
	else
	    print("\n");
	    
	if(zonep != 0) {
	    lastheaderp = 0;		
/* For child zones, initial header (dummy) isn't header_t aligned */
            if (zonep->type == INTERNAL)
	      headerp = (header_t *)malloc_static.regions[i].beginadd;
	    else
	      headerp = (header_t *)
	         (ROUND_SIZE((unsigned)malloc_static.regions[i].beginadd));

	    while((int)headerp < (int)malloc_static.regions[i].endadd) {
		print("add 0x%x size 0x%x free %d lastguyfree %d\n",
		    (unsigned int)headerp,headerp->size,headerp->free,
		    headerp->lastguyfree);
		if(headerp->size == 0) {
		    print("***bad piece at %x size of zero\n",(unsigned int)headerp);
		    return;
		}
		if(headerp->size == 4) {
		    print("-------- extend point %x --------\n",(unsigned int)headerp);
		}
		if(headerp->lastguyfree) {
		    lastspot = ((int *)headerp)[-1];
		    if(lastspot < 1 || lastspot > zonep->freen) {
			print("back pointer number to the lastfree is bad\n");
			lastspot = 1;
		    }
		    lastfreearray = &zonep->freearray[lastspot];
		    if((int)(lastfreearray->data) != (int)lastheaderp) {
			print("back pointer to last free is bad\n");
		    }
		    if(!lastheaderp->free) {
			print("lastfree is not really free\n");
		    }
		}
		lastheaderp = headerp;		
		headerp = (header_t *)((int)headerp + headerp->size);
	    }
	    if(lastheaderp->size > (ROUND_SIZE(sizeof(header_t)) + sizeof(header_t)) &&
	       !((i < (malloc_static.max_region - 1) &&
	    	malloc_static.regions[i+1].zonep->type == INTERNAL))) {
		print("last piece in region is bad\n");
	    }

	}
    }

}

int malloc_debug (int level)
{
int old;
int   i;

    if(!malloc_static.inited)
	malloc_init();
	
    old = malloc_static.zones[0]->debug;

    for(i = 0;i < malloc_static.max_zone; i++) {
	malloc_static.zones[i]->debug = level;
    }
    if(level && level != MALLOC_DEBUG_1DEEPFREE)
	dolockordebug(0);
    return old;
}

void (*malloc_error(void (*func)(int)))(int) {
    void (*oldfunc)(int);

    oldfunc = MallocErrorFunc;
    MallocErrorFunc = func;
    return oldfunc;
}

size_t malloc_size (void *ptr)
{
header_t *headerp;
region_t *regionp;

  if(!NOTPAGEALIGNED(ptr)) {
    regionp = findregionnolock(ptr);
    if(!regionp->zonep)
	return((int)(regionp->endadd - regionp->beginadd));  
  }
  
    headerp = (header_t *) ptr;
    headerp--;
    return (headerp->size - sizeof(header_t));
}

size_t malloc_good_size (size_t byteSize)
{
  return byteSize;
}

size_t mstats (void)
{
  return 1;
}


void           *
calloc(size_t numElems, size_t byteSize)
{
void	*new;
    new = NXZoneCalloc(malloc_static.defaultzone, numElems, byteSize);
    return(new);
}

void *valloc(size_t size)
{
void	*new;
LOCK {
    new = vallocnolock(size); 
} UNLOCK
    return(new);
}

static void *vallocnolock(size_t size)
{
void	*new;
    new = _valloczonenolock(&size); 
    if(!new)
	return(0);
    addregionnolock(new, size, 0);
    checkregionsallocation();
    return(new);
}


void           *
realloc(void *oldptr, size_t newsize)
{
    region_t	*regionp;

    regionp = findregionnolock(oldptr);
    if (!regionp || !regionp->zonep)
	return NXZoneRealloc(malloc_static.defaultzone, oldptr, newsize);
    else
	return NXZoneRealloc((NXZone *) regionp->zonep, oldptr, newsize);
}

void *malloc(size_t size)
{
    if(!malloc_static.inited)
	malloc_init();
	
    if(size > (vm_page_size - 32)) {
	if((vm_page_size - (size % vm_page_size)) <= 32)
	    return(valloc(size));
    }
    
    return(NXZoneMalloc(malloc_static.defaultzone, size));
}


extern void free(void *ptr)
{
 region_t   *regionp;
 void	*oldfree;
 int	debug;
 
   if(malloc_static.safesingle) {
	if(Z(malloc_static.defaultzone)->debug & MALLOC_DEBUG_1DEEPFREE) {
		oldfree = malloc_static.freeme;
		malloc_static.freeme = 0;
	}
	else {
	    oldfree = ptr;
	    ptr = 0;
	    }
	if(oldfree) { 
	    if(malloc_static.max_zone == 1 && NOTPAGEALIGNED(oldfree)) {
		NXZoneFree(malloc_static.defaultzone, oldfree);
	    }
	    else {
		regionp = findregionnolock(oldfree);
		if(!regionp) 
		    MallocErrorFunc(MALLOC_ERROR_FREE_NOT_IN_HEAP);
		else if(regionp->zonep)
		    NXZoneFree((NXZone*)regionp->zonep, oldfree);
		else
		    vfreenolock(oldfree);
	    }
	}
	malloc_static.freeme = ptr;
    }
    else {
	LOCK {
	debug = Z(malloc_static.defaultzone)->debug;
	if(debug & MALLOC_DEBUG_1DEEPFREE) {
		oldfree = malloc_static.freeme;
		malloc_static.freeme = 0;
	}
	else  {
	    oldfree = ptr;
	    ptr = 0;
	    }
	
	if(oldfree) {
	    if(debug & MALLOC_DEBUG_BEFORE) 
		malloc_check(FREE_ENTER);
	    regionp = findregionnolock(oldfree);
	    if(!regionp) 
		MallocErrorFunc(MALLOC_ERROR_FREE_NOT_IN_HEAP);
	    else if(regionp->zonep)
		nxzonefreenolock((NXZone*)regionp->zonep, oldfree);
	    else
		vfreenolock(oldfree);
		
	    if(debug & MALLOC_DEBUG_WASTEFREE) { 
		if(ISFREEMEMEMORY(oldfree))
		    bzero(oldfree, malloc_size(oldfree) - sizeof(int));
		}
	    if(debug & MALLOC_DEBUG_AFTER) 
		malloc_check(FREE_EXIT);
	}
	malloc_static.freeme = ptr;
	} UNLOCK 
    }
}

/*
 * Return true if this address is the start of a malloc area in the heap.
 */ 
static int isMallocData(void *ptr)
{
region_t *regionp;
header_t *headerp,*winner;

    regionp = findregionnolock(ptr);
    winner = (header_t *)((int)ptr - sizeof(header_t));
    if(regionp && regionp->zonep) {
	headerp = (header_t *) (ROUND_SIZE((unsigned)regionp->beginadd));
	while((int)headerp < (int)regionp->endadd) {
	    if(headerp == winner)
		return(1);
	    headerp = (header_t *)((int)headerp + headerp->size);	
	}
    }
    return(0);
}

extern void vfree(void *ptr)
{
LOCK {
    vfreenolock(ptr);
} UNLOCK
}

static void vfreenolock(void *ptr)
{
region_t   *regionp;

    if(ptr) {
	regionp = findregionnolock(ptr);
	if(!regionp) 
	    MallocErrorFunc(MALLOC_ERROR_FREE_NOT_IN_HEAP);
	
	if(regionp->beginadd == ptr) {
	    if(vm_deallocate(mach_task_self(),(int)regionp->beginadd, 
		    (int)(regionp->endadd - regionp->beginadd)) != KERN_SUCCESS)
		MallocErrorFunc(MALLOC_ERROR_VM_DEALLOC);
	    delregionnolock(regionp);
	}
    }
}

/*
 * Give a zone a name.
 * The string will be copied.
 */
void NXNameZone(NXZone *z, const char *name)
{
zone_t	*zonep = (zone_t *)z;

  LOCK {
    if(zonep->name)
	FREE(zonep->name);
    zonep->name = MALLOC(strlen(name) + 1);
    strcpy(zonep->name,name);
  } UNLOCK 
}

static const char * const MallocErrorMessages[] = {
    "vm_allocate failed\n",
    "vm_deallocate failed\n",
    "vm_copy failed\n",
    "attempt to free or realloc space already freed\n",
    "internal verification of memory heap failed\n",
    "attempt to free or realloc space not in heap\n",
    "bad zone\n"
};

static void DefaultMallocError(int x) {

    errmessage("dyld: ");
    errmessage(MALLOC_ERROR);
    errmessage((char *)MallocErrorMessages[x]);
    if ((x == MALLOC_ERROR_VM_ALLOC) || (x == MALLOC_ERROR_FREE_FREE)) {
        errno = ENOMEM;
    } else {
        abort();
    }
}

/*
 * Optimization which tells us that we are single threaded, and do not care about
 * malloc_debugging.
 */
#ifdef NO_INDR_LIBC
void set_malloc_singlethreaded(int single)
#else
void _set_malloc_singlethreaded(int single)
#endif
{
    if(!malloc_static.inited)
        malloc_init();
    dolockordebug(single);
}

void malloc_singlethreaded()
{
#ifdef NO_INDR_LIBC
    set_malloc_singlethreaded(1);
#else
    _set_malloc_singlethreaded(1);
#endif
}

void _malloc_fork_prepare()
/*
 * Prepare the malloc module for a fork by insuring that no thread is in a
 * malloc critical section.
 */
{
	LOCK
}
void _malloc_fork_parent()
/*
 * Called in the parent process after a fork() to resume normal operation.
 */
{
	UNLOCK
}
void _malloc_fork_child()
/*
 * Called in the child process after a fork() to resume normal operation.
 * In the MTASK case we also have to change memory inheritance so that the
 * child does not share memory with the parent.
 */
{
	UNLOCK
}



/* 
 * Interface to allow other allocator writers to register zones.
 */
void NXAddRegion(int start, int size,NXZone *zonep)
{
    LOCK
    addregionnolock((char *)start, size, (zone_t *)zonep);
    checkregionsallocation();
    UNLOCK
}

void NXRemoveRegion(int start)
{
region_t   *regionp;

    LOCK
    regionp = findregionnolock((char *)start);
    delregionnolock(regionp); 
    UNLOCK
}

#define MALLOCVERSION  1
/*
 * The next 2 functions exist for application which want to dump the heap
 * of an application to disk and restart the application later and have
 * malloc still work. 
 *
 * All malloc will show up in the default zone when restored.  
 *
 * Have Malloc take all important information and save it in the heap.
 * An int is returned which needs to be given to the malloc_jumpstart call
 * to restore malloc state.
 */
#define ADDINT(i) (*((int *)cp) = i, cp += sizeof(int))
#define ADDDATA(ptr,ptrsize) (bcopy((char *)ptr,cp,ptrsize), cp += ptrsize)

int	malloc_freezedry()
{
char	*cp,*startcp;
int		size,regionsize,zonesize;

	regionsize = malloc_static.max_region * sizeof(region_t);
	zonesize = malloc_static.max_zone   * sizeof(zone_t *);
	size = sizeof(int) + sizeof(int) + regionsize + sizeof(int) + zonesize;
	startcp = cp = (char *)_valloczonenolock((size_t *)&size);
	if(!cp)
	    return(0);
	ADDINT(MALLOCVERSION);
	ADDINT(malloc_static.max_region);
	ADDDATA(malloc_static.regions, regionsize);
	ADDINT(malloc_static.max_zone);
	ADDDATA(malloc_static.zones, zonesize);
	return((int)startcp);
}

/*
 * If fed an int returned prevously from malloc_freezedry(), will restore malloc
 * state. 
 * Returns 
 * 	1 if there is a version mismatch.
 * 	0 on success.
 */
#define FETCHINT(i) (i = *((int *)cp), cp += sizeof(int))

int malloc_jumpstart(int cookie)
{
char *cp = (char *)cookie;
char *startcp;
int	 version,regions,zones;
region_t	region;
zone_t		*zonep;

    startcp = cp;
    FETCHINT(version); 
    if(version != MALLOCVERSION) 
	return(1);
    FETCHINT(regions);
    while(--regions >= 0) {
	region = *((region_t *)cp);
	cp += sizeof(region_t);
	NXAddRegion((int)region.beginadd,(int)(region.endadd-region.beginadd),
				    (NXZone *) region.zonep);
    }
    FETCHINT(zones);
    while(--zones >= 0) {
	zonep = *((zone_t **)cp);
	cp += sizeof(zone_t *);
	keepzonenolock(zonep);
    }
    vm_deallocate(mach_task_self(), (vm_address_t) startcp, (int)(cp - startcp));
	return(1);
}

#ifdef MAIN
   /*
    * This program is used for testing malloc
    *
    * Do
    * cc -DMAIN -DTESTING  -g malloc.c
    * a.out bignumber
    */
struct uf  {
  char *a;
  NXZone   *z;
  int  size;
  };

int count;
NXZone *z2;

void malloctest(char *);

int main(argc, argv)
int	argc;
char	*argv[];
{
	NXZone *z2;
   int seed;
	cthread_t t1, t2;

    z2 = NXCreateZone(vm_page_size, vm_page_size/4, 1);
    NXNameZone(z2,"The second zone");
    NXNameZone(z2,"The second zone");
   NXNameZone(NXDefaultMallocZone(),"The default zone");
    if(argc < 2) {
	print("give count\n");
	exit(1);
    }
    count = atoi(argv[1]);
    print("count is %d\n", count);
    if(argc > 2)
	seed = atoi(argv[2]);
    else
	seed = 1;

    srandom(seed);
    malloc_debug(0);

    malloctest("main zone");
    t1 = cthread_fork(malloctest, "t1 zone");
    t2 = cthread_fork(malloctest, "t2 zone");
    cthread_join(t1);
    cthread_join(t2);

   print("\n\neverything freed\n");
   _NXMallocDumpFrees();
   _NXMallocDumpZones();
   
   NXDestroyZone(z2);
   _NXMallocDumpFrees();
   _NXMallocDumpZones();
}

void malloctest(char *name)
{
int i,p,z,j;
int size,r,what,canfree;
int un;
char *v;
NXZone *z1;
struct uf unfreed[10000];

    z1 = NXCreateChildZone(malloc_static.defaultzone, vm_page_size, vm_page_size/4, 1);
    canfree = 1;
    NXNameZone(z1,name);
    
    v = 0;
   un = 0;
   for(i = 0; i < count; i++) {
       r = random();
       size = r % (vm_page_size + 2);
       if(size < (vm_page_size - 50))
	    size /= 8;
       z = 0;
       what = r % 4;

       switch(what) {
	    case 0:  /* malloc */
	    case 1:  /* malloc */
		switch(z) {
		    case 0:
			if((r % 100) == 100)
			    v = valloc(1);
			unfreed[un].a = malloc(size);
			if ((unsigned)unfreed[un].a & ALIGN_OFFSET) {
				errmessage("ALIGNMENT: malloc()");
				abort();
			}
			if((r % 3000) == 0)
			    NXZonePtrInfo(unfreed[un].a);
			bfill(unfreed[un].a,size);
			unfreed[un].size = size;
			unfreed[un++].z =  malloc_static.defaultzone;
			if(v)
			    free(v);
			v = 0;
			break;
		    case 1:
			size /= 8;
			unfreed[un].size = size;
			unfreed[un].a = NXZoneMalloc(z1,size);	
		    if ((unsigned)unfreed[un].a & ALIGN_OFFSET) {
			    errmessage("ALIGNMENT: NXZoneMalloc(z1)");
			    abort();
		    }
			bfill(unfreed[un].a,size);
			unfreed[un++].z = z1;
			break;
		    case 2:
			unfreed[un].size = size;
			unfreed[un].a = NXZoneMalloc(z2,size);
		    if ((unsigned)unfreed[un].a & ALIGN_OFFSET) {
			    errmessage("ALIGNMENT: NXZoneMalloc(z2)");
			    abort();
		    }
			bfill(unfreed[un].a,size);
			unfreed[un++].z = z2;
			break;
		    }
		break;
	    case 2:   /* realloc */
		if(!un)
		    break;
		p = r % un;
		if(unfreed[p].a) {
		    checkmem(unfreed[p].a,unfreed[p].size);
		    unfreed[p].a = NXZoneRealloc(unfreed[p].z, unfreed[p].a, size);
		    if ((unsigned)unfreed[p].a & ALIGN_OFFSET) {
			    errmessage("ALIGNMENT: NXZoneRealloc");
			    abort();
		    }
		    unfreed[p].size = size;
		    bfill(unfreed[p].a,size);
		}
		break;
	    case 3: /* free */
		if(!un)
		    break;
		p = r % un;
		if(unfreed[p].z == z1 || !canfree)
		    break;
		checkmem(unfreed[p].a,unfreed[p].size);
		free(unfreed[p].a);
		unfreed[p].a = 0;
		break;
	    }
	if (un >= 10000) {
		print("un (%d) exceeded!\n", un);
		abort();
	}
	if((i % 1000) == 0) {
	    print("%d\n",i);
           switch(r & 0x3F) {
               case 0:      
		   malloc_debug(MALLOC_DEBUG_BEFORE + MALLOC_DEBUG_AFTER);
                   break;
               case 1:
                   malloc_debug(0);
                   break;
               case 2:
                   malloc_debug(MALLOC_DEBUG_WASTEFREE);
                   break;
		default:
                   malloc_debug(MALLOC_DEBUG_1DEEPFREE);
		   break;
               }
	}
	if((r % 1000) == 0 || un > 9000) {
	    if(r & 1) {	    /* merge */
		NXMergeZone(z1);
		for(j = 0; j < un; j++) /* now belong to parent */
		    if(unfreed[j].z == z1)
			unfreed[j].z = NXDefaultMallocZone();
	    }
	    else {	/* destroy */
		NXDestroyZone(z1);
		for(j = 0; j < un; j++) /* no longer allocated */
		    if(unfreed[j].z == z1) {
			unfreed[j].a = 0;
			unfreed[j].z = 0;
		    }
	    }
	    if((r % 20) == 0) {
		while(un > 500) { 
		    free(unfreed[--un].a);
		    }
		canfree = 0;
	    }
	    else
		canfree = 1;
	z1 = NXCreateChildZone(malloc_static.defaultzone, vm_page_size, 
			vm_page_size/4, canfree);
	}	
	if(((r % 5000) == 0 || un > 9000) && canfree) {
	    while(--un >= 0) {
		free(unfreed[un].a);
		}
	un = 0;
	}
	}

   NXDestroyZone(z1);
    for(j = 0; j < un; j++) /* no longer allocated */
	if(unfreed[j].z == z1) {
	    unfreed[j].a = 0;
	    unfreed[j].z = 0;
	}
  
   while(--un >= 0) {
	free(unfreed[un].a);
	}
}

checkmem(char *ptr, int n)
{
int   i;
    if(!ptr)
	return;
    for(i = 0; i < n ; i++)
	if(ptr[i] != 1) {
	    print("mem not zero ptr 0x%x n %d i %d\n",ptr,n,i);
	    abort();
	}
}

bfill(char *ptr, int n)
{
    while(n-- > 0)
	*ptr++ = 1;
}

kern_return_t MYvm_allocate(vm_task_t target_task, vm_address_t *address, vm_size_t size, boolean_t anywhere)
{
   return(vm_allocate(target_task,address,size,anywhere));
}


#endif /* MAIN */

#endif /* __MACH30__ */
