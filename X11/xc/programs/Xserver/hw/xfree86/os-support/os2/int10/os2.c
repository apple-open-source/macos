/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/int10/os2.c,v 1.4 2002/01/25 21:56:20 tsi Exp $ */
/*
 *                   XFree86 int10 module
 *   execute BIOS int 10h calls in x86 real mode environment
 *                 Copyright 1999 Egbert Eich
 */
#include "xf86.h"
#include "xf86str.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "compiler.h"
#define _INT10_PRIVATE
#include "xf86int10.h"
#include "int10Defines.h"

static CARD8 read_b(xf86Int10InfoPtr pInt,int addr);
static CARD16 read_w(xf86Int10InfoPtr pInt,int addr);
static CARD32 read_l(xf86Int10InfoPtr pInt,int addr);
static void write_b(xf86Int10InfoPtr pInt,int addr, CARD8 val);
static void write_w(xf86Int10InfoPtr pInt,int addr, CARD16 val);
static void write_l(xf86Int10InfoPtr pInt,int addr, CARD32 val);

/*
 * the emulator cannot pass a pointer to the current xf86Int10InfoRec
 * to the memory access functions therefore store it here.
 */

typedef struct {
    int shift;
    int pagesize_1;
    int entries;
    void* vRam;
    memType *alloc_rec;
} genericInt10Priv;

#define INTPriv(x) ((genericInt10Priv*)x->private)

int10MemRec genericMem = {
    read_b,
    read_w,
    read_l,
    write_b,
    write_w,
    write_l
};

static void MapVRam(xf86Int10InfoPtr pInt);
static void UnmapVRam(xf86Int10InfoPtr pInt);
static void setupTable(xf86Int10InfoPtr pInt, memType address,
		       int loc,int size);

static void *sysMem = NULL;

xf86Int10InfoPtr
xf86InitInt10(int entityIndex)
{
    xf86Int10InfoPtr pInt;
    int screen;
    void* intMem;
    void* vbiosMem;
    int pagesize;
    int entries;
    int shift;
    legacyVGARec vga;
    
    screen = (xf86FindScreenForEntity(entityIndex))->scrnIndex;
    
    if (int10skip(xf86Screens[screen],entityIndex))
	return NULL;

    pInt = (xf86Int10InfoPtr)xnfcalloc(1,sizeof(xf86Int10InfoRec));
    pInt->entityIndex = entityIndex;
    if (!xf86Int10ExecSetup(pInt))
	goto error0;
    pInt->mem = &genericMem;
    pagesize = xf86getpagesize();
    pInt->private = (pointer)xnfcalloc(1,sizeof(genericInt10Priv));
    entries = SYS_SIZE / pagesize;

    pInt->scrnIndex = screen;
    INTPriv(pInt)->pagesize_1 = pagesize - 1;
    INTPriv(pInt)->entries = entries;
    INTPriv(pInt)->alloc_rec =
	xnfcalloc(1,sizeof(memType) * entries);
    for (shift = 0 ; (pagesize >> shift) ; shift++) {};
    shift -= 1;
    INTPriv(pInt)->shift = shift;

    /*
     * we need to map video RAM MMIO as some chipsets map mmio
     * registers into this range.
     */

    MapVRam(pInt);
    intMem = xnfalloc(pagesize);
    setupTable(pInt,(memType)intMem,0,pagesize);
    vbiosMem = xnfalloc(V_BIOS_SIZE);

#ifdef _PC
    if (!sysMem)
	sysMem = xf86MapVidMem(screen,VIDMEM_FRAMEBUFFER,SYS_BIOS,BIOS_SIZE);
    setupTable(pInt,(memType)sysMem,SYS_BIOS,BIOS_SIZE);
    if (xf86ReadBIOS(0,0,(unsigned char *)intMem,LOW_PAGE_SIZE) < 0) {
	xf86DrvMsg(screen,X_ERROR,"Cannot read int vect\n");
	goto error1;
    }
    if (xf86IsEntityPrimary(entityIndex)) {
	int size;
	int cs = MEM_RW(pInt,((0x10<<2)+2));

int i,k,m;
char buf[100], hx[10];
for (i=0; i<0x100; i+=16) {
sprintf(buf,"%04x: ",i);
for (k=0; k<16; k++) {
  m = MEM_RB(pInt,i+k);
  sprintf(hx,"%02x ",((unsigned)m)&0xff);
  strcat(buf,hx);
}
xf86DrvMsg(screen,X_INFO,"%s\n",buf);
}



	xf86DrvMsg(screen,X_INFO,"Primary V_BIOS segmant is: 0x%x\n",cs);
	if (xf86ReadBIOS(cs << 4,0,(unsigned char *)vbiosMem,
			 0x10) < 0) {
	    xf86DrvMsg(screen,X_ERROR,"Cannot read V_BIOS (1)\n");
	    goto error1;
	}
	if (!((*(CARD8*)vbiosMem == 0x55)
	      && (*((CARD8*)vbiosMem + 1) == 0xAA))) {
	    xf86DrvMsg(screen,X_ERROR,"No V_BIOS found\n");
	    goto error1;
	}
	
	size = *((CARD8*)vbiosMem + 2) * 512;
	if (xf86ReadBIOS(cs << 4,0,vbiosMem, size) < 0) {
	    xf86DrvMsg(screen,X_ERROR,"Cannot read V_BIOS (2)\n");
	    goto error1;
	}

	setupTable(pInt,(memType)vbiosMem,cs<<4,size);
	set_return_trap(pInt);
	pInt->BIOSseg = cs;
    } else {
	reset_int_vect(pInt);
	set_return_trap(pInt);
	if (!mapPciRom(pInt,(unsigned char *)(vbiosMem))) {
	    xf86DrvMsg(screen,X_ERROR,"Cannot read V_BIOS (3)\n");
	    goto error1;
	}
	setupTable(pInt,(memType)vbiosMem,V_BIOS,V_BIOS_SIZE);
	pInt->BIOSseg = V_BIOS >> 4;
	pInt->num = 0xe6;
	LockLegacyVGA(pInt, &vga); 
	xf86ExecX86int10(pInt);
	UnlockLegacyVGA(pInt, &vga);
    }
#else
    if (!sysMem) {
	sysMem = xnfalloc(BIOS_SIZE);
	setup_system_bios((memType)sysMem);
    }
    setupTable(pInt,(memType)sysMem,SYS_BIOS,BIOS_SIZE);
    setup_int_vect(pInt);
    set_return_trap(pInt);
    if (!mapPciRom(pInt,(unsigned char *)(vbiosMem))) {
	xf86DrvMsg(screen,X_ERROR,"Cannot read V_BIOS (4)\n");
	goto error1;
    }
    setupTable(pInt,(memType)vbiosMem,V_BIOS,V_BIOS_SIZE);
    pInt->BIOSseg = V_BIOS >> 4;
    pInt->num = 0xe6;
    LockLegacyVGA(pInt, &vga);	   
    xf86ExecX86int10(pInt);
    UnlockLegacyVGA(pInt, &vga);
#endif
    return pInt;

 error1:
    xfree(vbiosMem);
    xfree(intMem);
    UnmapVRam(pInt);
    xfree(INTPriv(pInt)->alloc_rec);
    xfree(pInt->private);
 error0:
    xfree(pInt);

    return NULL;
}

static void
MapVRam(xf86Int10InfoPtr pInt)
{
    int screen = pInt->scrnIndex;
    int pagesize = INTPriv(pInt)->pagesize_1 + 1;
    int size = ((VRAM_SIZE + pagesize - 1)/pagesize) * pagesize;
    INTPriv(pInt)->vRam = xf86MapVidMem(screen,VIDMEM_MMIO,V_RAM,size);
}

static void 
UnmapVRam(xf86Int10InfoPtr pInt)
{
    int screen = pInt->scrnIndex;
    int pagesize = INTPriv(pInt)->pagesize_1 + 1;
    int size = ((VRAM_SIZE + pagesize - 1)/pagesize) * pagesize;

    xf86UnMapVidMem(screen,INTPriv(pInt)->vRam,size);
}

Bool
MapCurrentInt10(xf86Int10InfoPtr pInt)
{
    /* nothing to do here */
    return TRUE;
}

void
xf86FreeInt10(xf86Int10InfoPtr pInt)
{
    int pagesize; 

    if (!pInt)
      return;
    pagesize = INTPriv(pInt)->pagesize_1 + 1;
    if (Int10Current == pInt) 
	Int10Current = NULL;
    xfree(INTPriv(pInt)->alloc_rec[V_BIOS/pagesize]);
    xfree(INTPriv(pInt)->alloc_rec[0]);
    UnmapVRam(pInt);
    xfree(INTPriv(pInt)->alloc_rec);
    xfree(pInt->private);
    xfree(pInt);
}

void *
xf86Int10AllocPages(xf86Int10InfoPtr pInt,int num, int *off)
{
    void* addr;
    int pagesize = INTPriv(pInt)->pagesize_1 + 1;
    int num_pages = INTPriv(pInt)->entries;
    int i,j;
    
    for (i=0;i<num_pages - num;i++) {
	if (INTPriv(pInt)->alloc_rec[i] == 0) {
	    for (j=i;j < num + i;j++)
		if ((INTPriv(pInt)->alloc_rec[j] != 0))
		    break;
	    if (j == num + i)
		break;
	    else
		i = i + num;
	}
    }
    if (i == num_pages - num)
	return NULL;

    *off = i * pagesize;
    addr = xnfalloc(pagesize * num);
    setupTable(pInt,(memType)addr,*off,pagesize * num);
    
    return addr;
}

void
xf86Int10FreePages(xf86Int10InfoPtr pInt, void *pbase, int num)
{
    int num_pages = INTPriv(pInt)->entries;
    int i,j;
    for (i = 0;i<num_pages - num; i++)
	if (INTPriv(pInt)->alloc_rec[i]==(memType)pbase) {
	    for (j = 0; j < num; j++)
		INTPriv(pInt)->alloc_rec[i] = 0;
	    break;
	}
    xfree(pbase);
    return;
}

static void
setupTable(xf86Int10InfoPtr pInt, memType address,int loc,int size)
{
    int pagesize = INTPriv(pInt)->pagesize_1 + 1;
    int i,j,num;

    i = loc / pagesize;
    num = (size + pagesize - 1)/ pagesize; /* round up to the nearest page */
                                           /* boudary if size is not       */
                                           /* multiple of pagesize         */
    for (j = 0; j<num; j++) {
	INTPriv(pInt)->alloc_rec[i+j] = address;
	address += pagesize;
    }
}

#define OFF(addr) \
          ((addr) & (INTPriv(pInt)->pagesize_1))
#define SHIFT \
          (INTPriv(pInt)->shift)
#define BASE(addr,shift) \
          (INTPriv(pInt)->alloc_rec[addr >> shift])
#define V_ADDR(addr,shift,off) \
          (BASE(addr,shift) + (off))
#define VRAM_ADDR(addr) (addr - 0xA0000)
#define VRAM_BASE (INTPriv(pInt)->vRam)

#define VRAM(addr) ((addr >= 0xA0000) && (addr <= 0xBFFFF))
#define V_ADDR_RB(addr,shift,off) \
        (VRAM(addr)) ? MMIO_IN8((CARD8*)VRAM_BASE,VRAM_ADDR(addr)) \
           : *(CARD8*) V_ADDR(addr,shift,off)
#define V_ADDR_RW(addr,shift,off) \
        (VRAM(addr)) ? MMIO_IN16((CARD16*)VRAM_BASE,VRAM_ADDR(addr)) \
           : ldw_u((pointer)V_ADDR(addr,shift,off))
#define V_ADDR_RL(addr,shift,off) \
        (VRAM(addr)) ? MMIO_IN32((CARD32*)VRAM_BASE,VRAM_ADDR(addr)) \
           : ldl_u((pointer)V_ADDR(addr,shift,off))

#define V_ADDR_WB(addr,shift,off,val) \
        if(VRAM(addr)) \
            MMIO_OUT8((CARD8*)VRAM_BASE,VRAM_ADDR(addr),val); \
        else \
            *(CARD8*) V_ADDR(addr,shift,off) = val;
#define V_ADDR_WW(addr,shift,off,val) \
        if(VRAM(addr)) \
            MMIO_OUT16((CARD16*)VRAM_BASE,VRAM_ADDR(addr),val); \
        else \
            stw_u((val),(pointer)(V_ADDR(addr,shift,off)));

#define V_ADDR_WL(addr,shift,off,val) \
        if (VRAM(addr)) \
            MMIO_OUT32((CARD32*)VRAM_BASE,VRAM_ADDR(addr),val); \
        else \
            stl_u(val,(pointer)(V_ADDR(addr,shift,off)));

static CARD8
read_b(xf86Int10InfoPtr pInt, int addr)
{
    if (!BASE(addr,SHIFT)) return 0xff;
    
    return V_ADDR_RB(addr,SHIFT,OFF(addr));
}

static CARD16
read_w(xf86Int10InfoPtr pInt, int addr)
{
    int shift = SHIFT;
    int off = OFF(addr);

    if (!BASE(addr,shift)) return 0xffff;
    
#if X_BYTE_ORDER == X_BIG_ENDIAN
    return ((V_ADDR_RB(addr,shift,off))
	    || ((V_ADDR_RB(addr,shift,off + 1)) << 8));
#else
    if (OFF(addr + 1) > 0) {
	return V_ADDR_RW(addr,SHIFT,OFF(addr));
    } else {
	return ((V_ADDR_RB(addr,shift,off + 1))
	    || ((V_ADDR_RB(addr,shift,off)) << 8));
    }
#endif
}

static CARD32
read_l(xf86Int10InfoPtr pInt, int addr)
{
    int shift = SHIFT;
    int off = OFF(addr);
	
    if (!BASE(addr,shift)) return 0xffffffff;
    
#if X_BYTE_ORDER == X_BIG_ENDIAN
    return ((V_ADDR_RB(addr,shift,off))
	    || ((V_ADDR_RB(addr,shift,off + 1)) << 8)
	    || ((V_ADDR_RB(addr,shift,off + 2)) << 16)
	    || ((V_ADDR_RB(addr,shift,off + 3)) << 24));
#else
    if (OFF(addr + 3) > 2) {
	return V_ADDR_RL(addr,SHIFT,OFF(addr));
    } else {
	return ((V_ADDR_RB(addr,shift,off + 3))
		|| ((V_ADDR_RB(addr,shift,off + 2)) << 8)
		|| ((V_ADDR_RB(addr,shift,off + 1)) << 16)
		|| ((V_ADDR_RB(addr,shift,off)) << 24));
    }
#endif
}

static void
write_b(xf86Int10InfoPtr pInt, int addr, CARD8 val)
{
    if (!BASE(addr,SHIFT)) return;
    
    V_ADDR_WB(addr,SHIFT,OFF(addr),val);
}

static void
write_w(xf86Int10InfoPtr pInt, int addr, CARD16 val)
{
    int shift = SHIFT;
    int off = OFF(addr);
    
    if (!BASE(addr,shift)) return;
    
#if X_BYTE_ORDER == X_BIG_ENDIAN
    V_ADDR_WB(addr,shift,off,val);
    V_ADDR_WB(addr,shift,off + 1,val >> 8);
#else
    if (OFF(addr + 1) > 0) {
	V_ADDR_WW(addr,shift,OFF(addr),val);
    } else {
	V_ADDR_WB(addr,shift,off + 1,val);
	V_ADDR_WB(addr,shift,off,val >> 8);
    }
#endif
}

static void
write_l(xf86Int10InfoPtr pInt, int addr, CARD32 val)
{
    int shift = SHIFT;
    int off = OFF(addr);
    if (!BASE(addr,shift)) return;
    
#if X_BYTE_ORDER == X_BIG_ENDIAN
    V_ADDR_WB(addr,shift,off,val);
    V_ADDR_WB(addr,shift,off + 1, val >> 8);
    V_ADDR_WB(addr,shift,off + 2, val >> 16);
    V_ADDR_WB(addr,shift,off + 3, val >> 24);
#else
    if (OFF(addr + 3) > 2) {
	V_ADDR_WL(addr,shift,OFF(addr),val);
    } else {
	V_ADDR_WB(addr,shift,off + 3, val);
	V_ADDR_WB(addr,shift,off + 2, val >> 8);
	V_ADDR_WB(addr,shift,off + 1, val >> 16);
	V_ADDR_WB(addr,shift,off, val >> 24);
    }
#endif
}

pointer
xf86int10Addr(xf86Int10InfoPtr pInt, CARD32 addr)
{
    return (pointer) V_ADDR(addr,SHIFT,OFF(addr));
}
