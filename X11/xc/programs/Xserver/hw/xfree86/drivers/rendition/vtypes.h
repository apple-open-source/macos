/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vtypes.h,v 1.10 2002/04/04 14:05:45 eich Exp $ */
#ifndef _VTYPES_H_
#define _VTYPES_H_



/*
 * includes
 */

#include "Xmd.h"
#include "xf86Pci.h"


/*
 * defines
 */

/* chip types */
#define V1000_DEVICE            0x0001
#define V2000_DEVICE            0x2000

/* Size of microcode */
#define MC_SIZE                 0x10000


#define XSERVER



/*
 * typedefs
 */

/* generic type definitions for central services */
typedef CARD32 vu32;
typedef CARD16 vu16;
typedef CARD8  vu8;
typedef INT32 vs32;
typedef INT16 vs16;
typedef INT8  vs8;

typedef enum {
    V_PIXFMT_DSTFMT=0,
    V_PIXFMT_332=1,       /**/
#define V_PIXFMT_233 V_PIXFMT_332
    V_PIXFMT_8I=2,        /**/
    V_PIXFMT_8A=3,
    V_PIXFMT_565=4,       /**/
    V_PIXFMT_4444=5,      /**/
    V_PIXFMT_1555=6,      /**/
    /* 7 reserved */
    V_PIXFMT_4I_565=8,
    V_PIXFMT_4I_4444=9,
    V_PIXFMT_4I_1555=10,
    /* 11 reserved */
    V_PIXFMT_8888=12,     /**/
    V_PIXFMT_Y0CRY1CB=13
#define V_PIXFMT_Y0CBY1CR V_PIXFMT_Y0CRY1CB
    /* 14 reserved */
    /* 15 reserved */
} vpixfmt;



/*
 * structs
 */

struct verite_modeinfo_t {
    int clock;              /* pixel clock */
    int hdisplay;           /* horizontal timing */
    int hsyncstart;
    int hsyncend;
    int htotal;
    int hskew;
    int vdisplay;           /* vertical timing */
    int vsyncstart;
    int vsyncend;
    int vtotal;
    int screenwidth;        /* further mode information */
    int virtualwidth;
    int screenheight;
    int virtualheight;
    int bitsperpixel;
    int hsynchi;
    int vsynchi;
    int pixelformat;        /* set by the mode init routines */
    int fifosize;
    int flags;              /* additional information like interlace etc */
    vu8 pll_n;
    vu8 pll_m;
    vu8 pll_p;
    vu8 refresh;
    vu8 doubleclock;
    vu16 stride0;
    vu16 stride1;
};

typedef struct _renditionRegs {
    CARD8 mode;
    CARD8 memendian;
    CARD32 sclkpll;
    CARD32 dramctl;
    CARD8 plldev;
    CARD32 pclkpll;
    CARD8 daccmd0;
    CARD8 daccmd1;
    CARD8 daccmd2;
    CARD8 daccmd3;
    CARD32 crtch;
    CARD32 crtcv;
    CARD32 vbasea;
    CARD32 crtcoff;
    CARD32 crtcctl;
} RenditionRegRec, *RenditionRegPtr;

/* structure describing the Verite board and its functionality */
struct verite_board_t {
    /* type of chip */
    vu16  chip;

    /* */
    IOADDRESS io_base, vgaio_base;
    vu32 mmio_base;
    vu32 vmmio_base;
    vu32 mem_size;
    vu8 *mem_base;
    vu8 *vmem_base;
    Bool init;

    /* */
    Bool accel;

    /* */
    vu32 csucode_base;
    vu32 ucode_base;
    vu32 ucode_entry;
    vu32 cursor_base;

    int Rop;
    int Color;

    /* mode information */
    struct verite_modeinfo_t mode;
    
    /* saved text mode settings */
    vu8 cursor_hi;
    vu8 cursor_low;
    vu8 offset_hi;
    vu8 offset_low;
    vu8 *scr_contents;

    Bool hwcursor_used;     /* Is HW-cursor used? */
    vu16 hwcursor_vmemsize; /* How much videomem does it use */
    vu32 hwcursor_membase;  /* videomem adress for V2K-cursor */
                            /* Has to be on 1024-byte boundry */

    vu32 fbOffset;          /* Currently busy fb-memory marker */
    Bool overclock_mem;     /* Memory overclock ? */

    Bool shadowfb;          /* Use ShadowFB ? */
    vu8 *shadowPtr;
    vu32 shadowPitch;
    void (*PointerMoved)(int index, int x, int y);

    int rotate;             /* Rotate clockwise or counterclockwise */

    vu8 ucode_buffer[MC_SIZE]; /* Space for microcode, when not needed */
};
    

/*
 * This structure is used to wrap the screen's CloseScreen vector.
 */
typedef struct _renditionRec
{
    struct verite_board_t board;        /* information on the board */
    struct verite_modeinfo_t mode;      /* information on the mode */
    PCITAG pcitag;                      /* tag for the PCI config space */
    pciVideoPtr PciInfo;                /* PCI config data */
    EntityInfoPtr pEnt;                 /* entity information */
    CloseScreenProcPtr CloseScreen;     /* wrap CloseScreen */
    xf86CursorInfoPtr CursorInfoRec;    /* Cursor data */
    XAAInfoRecPtr AccelInfoRec;         /* Needed for XAA */
    OptionInfoPtr Options;
    RenditionRegRec saveRegs;
} renditionRec, *renditionPtr;

#define RENDITIONPTR(p)     ((renditionPtr)((p)->driverPrivate))

#endif /* #ifndef _VTYPES_H_ */

/*
 * end of file vtypes.h
 */
