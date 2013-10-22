
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <strings.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <IOKit/IOHibernatePrivate.h>


#define ptoa_64(p)	  (((uint64_t)p) << PAGE_SHIFT)
#define ptoa_32(p)	  ptoa_64(p)
#define atop_64(p)	  ((p) >> PAGE_SHIFT)
#define pal_hib_map(a, p) (p)

enum { kIOHibernateTagLength    = 0x00001fff };

static IOHibernateImageHeader _hibernateHeader;
IOHibernateImageHeader * gIOHibernateCurrentHeader = &_hibernateHeader;

ppnum_t gIOHibernateHandoffPages[64];
uint32_t gIOHibernateHandoffPageCount = sizeof(gIOHibernateHandoffPages) 
					/ sizeof(gIOHibernateHandoffPages[0]);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static uint32_t
store_one_page(uint32_t * src, uint32_t compressedSize, 
		uint32_t * buffer, uint32_t ppnum)
{
//    printf("ppnum 0x%x\n", ppnum);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint32_t
hibernate_sum_page(uint8_t *buf, uint32_t ppnum)
{
    return (((uint32_t *)buf)[((PAGE_SIZE >> 2) - 1) & ppnum]);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static hibernate_bitmap_t *
hibernate_page_bitmap(hibernate_page_list_t * list, uint32_t page)
{
    uint32_t             bank;
    hibernate_bitmap_t * bitmap = &list->bank_bitmap[0];

    for (bank = 0; bank < list->bank_count; bank++)
    {
	if ((page >= bitmap->first_page) && (page <= bitmap->last_page))
	    break;
	bitmap = (hibernate_bitmap_t *) &bitmap->bitmap[bitmap->bitmapwords];
    }
    if (bank == list->bank_count)
	bitmap = NULL;
	
    return (bitmap);
}

hibernate_bitmap_t *
hibernate_page_bitmap_pin(hibernate_page_list_t * list, uint32_t * pPage)
{
    uint32_t             bank, page = *pPage;
    hibernate_bitmap_t * bitmap = &list->bank_bitmap[0];

    for (bank = 0; bank < list->bank_count; bank++)
    {
	if (page <= bitmap->first_page)
	{
	    *pPage = bitmap->first_page;
	    break;
	}
	if (page <= bitmap->last_page)
	    break;
	bitmap = (hibernate_bitmap_t *) &bitmap->bitmap[bitmap->bitmapwords];
    }
    if (bank == list->bank_count)
	bitmap = NULL;
	
    return (bitmap);
}

void 
hibernate_page_bitset(hibernate_page_list_t * list, boolean_t set, uint32_t page)
{
    hibernate_bitmap_t * bitmap;

    bitmap = hibernate_page_bitmap(list, page);
    if (bitmap)
    {
	page -= bitmap->first_page;
	if (set)
	    bitmap->bitmap[page >> 5] |= (0x80000000 >> (page & 31));
	    //setbit(page - bitmap->first_page, (int *) &bitmap->bitmap[0]);
	else
	    bitmap->bitmap[page >> 5] &= ~(0x80000000 >> (page & 31));
	    //clrbit(page - bitmap->first_page, (int *) &bitmap->bitmap[0]);
    }
}

boolean_t 
hibernate_page_bittst(hibernate_page_list_t * list, uint32_t page)
{
    boolean_t		 result = TRUE;
    hibernate_bitmap_t * bitmap;

    bitmap = hibernate_page_bitmap(list, page);
    if (bitmap)
    {
	page -= bitmap->first_page;
	result = (0 != (bitmap->bitmap[page >> 5] & (0x80000000 >> (page & 31))));
    }
    return (result);
}

// count bits clear or set (set == TRUE) starting at page.
uint32_t
hibernate_page_bitmap_count(hibernate_bitmap_t * bitmap, uint32_t set, uint32_t page)
{
    uint32_t index, bit, bits;
    uint32_t count;

    count = 0;

    index = (page - bitmap->first_page) >> 5;
    bit = (page - bitmap->first_page) & 31;

    bits = bitmap->bitmap[index];
    if (set)
	bits = ~bits;
    bits = (bits << bit);
    if (bits)
	count += __builtin_clz(bits);
    else
    {
	count += 32 - bit;
	while (++index < bitmap->bitmapwords)
	{
	    bits = bitmap->bitmap[index];
	    if (set)
		bits = ~bits;
	    if (bits)
	    {
		count += __builtin_clz(bits);
		break;
	    }
	    count += 32;
	}
    }

    if ((page + count) > (bitmap->last_page + 1)) count = (bitmap->last_page + 1) - page;

    return (count);
}

static ppnum_t
hibernate_page_list_grab(hibernate_page_list_t * list, uint32_t * pNextFree)
{
    uint32_t		 nextFree = *pNextFree;
    uint32_t		 nextFreeInBank;
    hibernate_bitmap_t * bitmap;

    nextFreeInBank = nextFree + 1;
    while ((bitmap = hibernate_page_bitmap_pin(list, &nextFreeInBank)))
    {
	nextFreeInBank += hibernate_page_bitmap_count(bitmap, FALSE, nextFreeInBank);
	if (nextFreeInBank <= bitmap->last_page)
	{
	    *pNextFree = nextFreeInBank;
	    break;
	}
    }

    if (!bitmap) 
    {
	exit(1);
	nextFree = 0;
    }

    return (nextFree);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void process_image(const void * image)
{
    uint64_t headerPhys;
    uint64_t mapPhys;
    uint64_t srcPhys;
    uint64_t imageReadPhys;
    uint64_t pageIndexPhys;
    uint32_t idx;
    uint32_t * pageIndexSource;
    hibernate_page_list_t * map;
    uint32_t stage;
    uint32_t count;
    uint32_t ppnum;
    uint32_t page;
    uint32_t conflictCount;
    uint32_t compressedSize;
    uint32_t uncompressedPages;
    uint32_t copyPageListHeadPage;
    uint32_t pageListPage;
    uint32_t * copyPageList;
    uint32_t * src;
    uint32_t copyPageIndex;
    uint32_t sum;
    uint32_t pageSum;
    uint32_t nextFree;
    uint32_t lastImagePage;
    uint32_t lastMapPage;
    uint32_t lastPageIndexPage;
    uint32_t handoffPages;
    uint32_t handoffPageCount;


    headerPhys = (uint64_t) image;

    bcopy(image, gIOHibernateCurrentHeader, sizeof(IOHibernateImageHeader));

    mapPhys = headerPhys
             + (offsetof(IOHibernateImageHeader, fileExtentMap)
	     + gIOHibernateCurrentHeader->fileExtentMapSize 
	     + ptoa_32(gIOHibernateCurrentHeader->restore1PageCount)
	     + gIOHibernateCurrentHeader->previewSize);

    map = (hibernate_page_list_t *) pal_hib_map(BITMAP_AREA, mapPhys);

    lastImagePage = atop_64(headerPhys + gIOHibernateCurrentHeader->image1Size);
    lastMapPage = atop_64(mapPhys + gIOHibernateCurrentHeader->bitmapSize);

    handoffPages     = gIOHibernateCurrentHeader->handoffPages;
    handoffPageCount = gIOHibernateCurrentHeader->handoffPageCount;

    nextFree = 0;
    hibernate_page_list_grab(map, &nextFree);

    sum = gIOHibernateCurrentHeader->actualRestore1Sum;
    gIOHibernateCurrentHeader->diag[0] = atop_64(headerPhys);
    gIOHibernateCurrentHeader->diag[1] = sum;

    uncompressedPages    = 0;
    conflictCount        = 0;
    copyPageListHeadPage = 0;
    copyPageList         = 0;
    copyPageIndex        = PAGE_SIZE >> 2;

    compressedSize       = PAGE_SIZE;
    stage                = 2;
    count                = 0;
    srcPhys              = 0;


    if (gIOHibernateCurrentHeader->previewSize)
    {
	pageIndexPhys     = headerPhys
	                   + (offsetof(IOHibernateImageHeader, fileExtentMap)
			   + gIOHibernateCurrentHeader->fileExtentMapSize 
			   + ptoa_32(gIOHibernateCurrentHeader->restore1PageCount));
	imageReadPhys     = (pageIndexPhys + gIOHibernateCurrentHeader->previewPageListSize);
	lastPageIndexPage = atop_64(imageReadPhys);
	pageIndexSource   = (uint32_t *) pal_hib_map(IMAGE2_AREA, pageIndexPhys);
    }
    else
    {
	pageIndexPhys     = 0;
	lastPageIndexPage = 0;
	imageReadPhys     = (mapPhys + gIOHibernateCurrentHeader->bitmapSize);
    }


    while (1)
    {
	switch (stage)
	{
	    case 2:
		// copy handoff data
		count = srcPhys ? 0 : handoffPageCount;
		if (!count)
		    break;
		if (count > gIOHibernateHandoffPageCount) count = gIOHibernateHandoffPageCount;
		srcPhys = ptoa_64(handoffPages);
		break;
	
	    case 1:
		// copy pageIndexSource pages == preview image data
		if (!srcPhys)
		{
		    if (!pageIndexPhys) break;
		    srcPhys = imageReadPhys;
		}
		ppnum = pageIndexSource[0];
		count = pageIndexSource[1];
		pageIndexSource += 2;
		pageIndexPhys   += 2 * sizeof(pageIndexSource[0]);
		imageReadPhys = srcPhys;
		break;

	    case 0:
		// copy pages
		if (!srcPhys) srcPhys = (mapPhys + gIOHibernateCurrentHeader->bitmapSize);
		src = (uint32_t *) pal_hib_map(IMAGE_AREA, srcPhys);
		ppnum = src[0];
		count = src[1];
		srcPhys += 2 * sizeof(*src);
		imageReadPhys = srcPhys;
		break;
	}


	if (!count)
	{
	    if (!stage)
	        break;
	    stage--;
	    srcPhys = 0;
	    continue;
	}

if (!stage) printf("phys 0x%x, 0x%x\n", ppnum, count);

	for (page = 0; page < count; page++, ppnum++)
	{
	    uint32_t tag;
	    int conflicts;

	    src = (uint32_t *) pal_hib_map(IMAGE_AREA, srcPhys);

	    if (2 == stage) ppnum = gIOHibernateHandoffPages[page];
	    else if (!stage)
	    {
		tag = *src++;
		srcPhys += sizeof(*src);
		compressedSize = kIOHibernateTagLength & tag;
	    }

	    conflicts = (ppnum >= atop_64(mapPhys)) && (ppnum <= lastMapPage);

	    conflicts |= ((ppnum >= atop_64(imageReadPhys)) && (ppnum <= lastImagePage));

	    if (stage >= 2)
 		conflicts |= ((ppnum >= atop_64(srcPhys)) && (ppnum <= (handoffPages + handoffPageCount - 1)));

	    if (stage >= 1)
 		conflicts |= ((ppnum >= atop_64(pageIndexPhys)) && (ppnum <= lastPageIndexPage));

//	    if (!conflicts)
	    {
//              if (compressedSize)
		pageSum = store_one_page(src, compressedSize, 0, ppnum);
		if (stage != 2)
		    sum += pageSum;
		uncompressedPages++;
	    }

	    srcPhys += ((compressedSize + 3) & ~3);
	    src     += ((compressedSize + 3) >> 2);

	}
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main(int argc, char * argv[])
{
    int fd;
    void * map;

    fd = open("/var/vm/sleepimage", O_RDONLY);
    map = mmap(NULL, 1*1024*1024*1024, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);

    printf("map %p\n", map);
    process_image(map);
}

