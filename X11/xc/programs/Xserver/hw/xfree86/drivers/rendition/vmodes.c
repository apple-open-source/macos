/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vmodes.c,v 1.14 2004/02/24 01:13:10 dawes Exp $ */
/*
 * file vmodes.c
 *
 * Routines that handle mode setting.
 */

/*
 * includes
 */

#include "rendition.h"
#include "vmodes.h"
#include "vos.h"
#include "vramdac.h"
#include "v1krisc.h"
#include "v1kregs.h"
#include "v2kregs.h"


#undef DEBUG


/*
 * defines
 */

#define combineNMP(N, M, P) \
    (((vu32)(M-2)<<10) | ((vu32)P<<8) | (vu32)(N-2))

#define v2kcombineNMP(N, M, P) (((vu32)N<<13) | ((vu32)P<<9) | (vu32)M)

/* defines for doubled clock */
#define CLK_DOUBLE		1
#define NO_CLK_DOUBLE	0

/* video FIFO is set to 64 or 128 entries based on req.d bandwidth in bytes/s */
#define FIFOSIZE_THRESH 	100000000 	 /* set thresh to 100MB/s */

/* compute desired video FIFO size given total bandwidth in bytes/s. */
#define FIFOSIZE(vclk, Bpp) (((vclk * Bpp) > FIFOSIZE_THRESH)  ? 128 : 64)

/* Bt485A RAMDAC control bits */
#define PALETTEDISABLE		0x10

/* Hold memory refresh cycles until video blank */
#define HOLD_MEMREFRESHCYCLE 0x2000

/* memCtl bits [16..23] */
#define DEFAULT_WREFRESH	0x330000

/* Disable memory refresh cycles */
#define DISABLE_MEMREFRESHCYCLE		0x8000

#define CTL(ldbl, hsynchi, vsynchi) \
  (((ldbl) ? CRTCCTL_LINEDOUBLE : 0) \
  |((hsynchi) ? CRTCCTL_HSYNCHI : 0) \
  |((vsynchi) ? CRTCCTL_VSYNCHI : 0) \
  |(CRTCCTL_VSYNCENABLE | CRTCCTL_HSYNCENABLE))

#define HORZ(fp, sy, bp, ac) \
  (((((vu32)(((fp)>>3)-1))&7)<<21)|((((vu32)(((sy)>>3)-1))&0x1F)<<16)|((((vu32)(((bp)>>3)-1))&0x3f)<<9)|((((vu32)(((ac)>>3)-1))&0xff)))

#define VERT(fp, sy, bp, ac) \
  (((((vu32)(fp-1))&0x3f)<<20)|((((vu32)(sy-1))&0x7)<<17)|((((vu32)(bp-1))&0x3f)<<11)|((((vu32)(ac-1))&0x7ff)))

#define HORZAC(crtchorz) \
  (((((vu32)crtchorz)&CRTCHORZ_ACTIVE_MASK)+1)<<3)
 
#define HORZBP(crtchorz) \
  ((((((vu32)crtchorz)&CRTCHORZ_BACKPORCH_MASK)>>9)+1)<<3)
 
#define HORZSY(crtchorz) \
  ((((((vu32)crtchorz)&CRTCHORZ_SYNC_MASK)>>16)+1)<<3)
 
#define HORZFP(crtchorz) \
  ((((((vu32)crtchorz)&CRTCHORZ_FRONTPORCH_MASK)>>21)+1)<<3)

#define VERTAC(crtcvert) \
  ((((vu32)crtcvert)&CRTCVERT_ACTIVE_MASK)+1)
 
#define VERTBP(crtcvert) \
  (((((vu32)crtcvert)&CRTCVERT_BACKPORCH_MASK)>>11)+1)
 
#define VERTSY(crtcvert) \
  (((((vu32)crtcvert)&CRTCVERT_SYNC_MASK)>>17)+1)
 
#define VERTFP(crtcvert) \
  (((((vu32)crtcvert)&CRTCVERT_FRONTPORCH_MASK)>>20)+1)

#define PCLK(N, M, P) (N),(M),(P)

#define TIMING_MASK	(CRTCCTL_VIDEOFIFOSIZE128|CRTCCTL_LINEDOUBLE|\
			         CRTCCTL_VSYNCHI|CRTCCTL_HSYNCHI|CRTCCTL_VSYNCENABLE|\
                     CRTCCTL_HSYNCENABLE)

/* addr&7 == 5, 6, 7 are bad */
#define BADADDR(x)	(((x)&(((x)<<1)|((x)<<2)))&0x4)

#define V1_MIN_VCO_FREQ  25
#define V1_MAX_VCO_FREQ  135
#define V1_REF_FREQ      14.318
#define V1_MIN_PCF_FREQ  0.2
#define V1_MAX_PCF_FREQ  5

#define V2_MIN_VCO_FREQ  125
#define V2_MAX_VCO_FREQ  250
#define V2_REF_FREQ      14.31818 /* Eh, is this right? */
#define V2_MIN_PCF_FREQ  1
#define V2_MAX_PCF_FREQ  3



/* 
 * global data
 */

static struct width_to_stride_t {
    vu32 width8bpp;
    vu8 stride0;
    vu8 stride1;
    vu16 chip;
} width_to_stride_table[]={
/*  {    0, 0, 0, V1000_DEVICE }, */
    {    4, 4, 0, V1000_DEVICE },
    {   16, 0, 1, V1000_DEVICE },
    {   20, 4, 1, V1000_DEVICE },
    {   32, 0, 2, V1000_DEVICE },
    {   36, 4, 2, V1000_DEVICE },
    {   64, 0, 3, V1000_DEVICE },
    {   68, 4, 3, V1000_DEVICE },
    {  128, 0, 4, V1000_DEVICE },
    {  132, 4, 4, V1000_DEVICE },
    {  256, 1, 0, V1000_DEVICE },
    {  272, 1, 1, V1000_DEVICE },
    {  288, 1, 2, V1000_DEVICE },
    {  320, 1, 3, V1000_DEVICE },
    {  384, 1, 4, V1000_DEVICE },
    {  512, 2, 0, V1000_DEVICE },
    {  528, 2, 1, V1000_DEVICE },
    {  544, 2, 2, V1000_DEVICE },
    {  576, 2, 3, V1000_DEVICE },
    {  592, 6, 1, V2000_DEVICE },
    {  608, 6, 2, V2000_DEVICE },
    {  640, 2, 4, V1000_DEVICE },
    {  704, 6, 4, V2000_DEVICE },
    {  768, 5, 0, V2000_DEVICE },
    {  784, 5, 1, V2000_DEVICE },
    {  800, 5, 2, V2000_DEVICE },
    {  832, 5, 3, V2000_DEVICE },
    {  896, 5, 4, V2000_DEVICE },
    { 1024, 3, 0, V1000_DEVICE },
    { 1028, 4, 5, V1000_DEVICE },
    { 1040, 3, 1, V1000_DEVICE },
    { 1056, 3, 2, V1000_DEVICE },
    { 1088, 3, 3, V1000_DEVICE },
    { 1152, 3, 4, V1000_DEVICE },
    { 1168, 7, 1, V2000_DEVICE },
    { 1184, 7, 2, V2000_DEVICE },
    { 1216, 7, 3, V2000_DEVICE },
    { 1280, 1, 5, V1000_DEVICE },
    { 1536, 2, 5, V1000_DEVICE },
    { 1600, 6, 5, V2000_DEVICE },
    { 1792, 5, 5, V2000_DEVICE },
    { 2048, 0, 6, V1000_DEVICE },
    { 2052, 4, 6, V1000_DEVICE },
    { 2176, 7, 5, V2000_DEVICE },
    { 2304, 1, 6, V1000_DEVICE },
    { 2560, 2, 6, V1000_DEVICE },
    { 2624, 6, 6, V2000_DEVICE },
    { 2816, 5, 6, V2000_DEVICE },
    { 3072, 3, 6, V1000_DEVICE },
    { 3200, 7, 6, V2000_DEVICE },
    { 4096, 0, 7, V1000_DEVICE },
    { 4100, 4, 7, V1000_DEVICE },
    { 4352, 1, 7, V1000_DEVICE },
    { 4608, 2, 7, V1000_DEVICE },
    { 4672, 6, 7, V2000_DEVICE },
    { 4864, 5, 7, V2000_DEVICE },
    { 5120, 3, 7, V1000_DEVICE },
    { 5248, 7, 7, V2000_DEVICE },
    {    0, 0, 0, 0 }
};

struct V1000ClocksStr {
    int h_size;
    int pll_val;
} V1000Clocks [] = {
    { 72, 0x40000 }, /* For Textmode */
    { 640, 0x2408 },   /*  31500  MHz */
    { 800, 0x1583D },  /*  40000  MHz */
    { 1024, 0x14823 }, /*  65000  MHz */
    { 1280, 0x14414 }, /*  108000 MHz */
    { 0, 0 }
};

/*
 * local function prototypes
 */

static void set_PLL(IOADDRESS iob, vu32 value);
static double V1000CalcClock(double target, int *M, int *N, int *P);
static double V2200CalcClock(double target, int *m, int *n, int *p);



/*
 * functions
 */

int
verite_setmode(ScrnInfoPtr pScreenInfo, struct verite_modeinfo_t *mode)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

    int tmp;
    int doubleclock=0;
    int M, N, P;
    int iob=pRendition->board.io_base;

#ifdef DEBUG
    ErrorF ("Rendition: Debug verite_setmode called\n");
#endif
 
    /* switching to native mode */
    verite_out8(iob+MODEREG, NATIVE_MODE|VESA_MODE);

    /* flipping some bytes */
    /* Must be something better to do than this -- FIX */
    switch (mode->bitsperpixel) {
    case 32:
      verite_out8(iob+MEMENDIAN, MEMENDIAN_NO);
      break;
    case 16:
      verite_out8(iob+MEMENDIAN, MEMENDIAN_HW);
      break;
    case 8:
      verite_out8(iob+MEMENDIAN, MEMENDIAN_END);
      break;
    }

    if (pRendition->board.chip != V1000_DEVICE) {
      if(!pRendition->board.overclock_mem){
	verite_out32(iob+SCLKPLL, 0xa484d);  /* mclk=110 sclk=50 */
                                        /* M/N/P/P = 77/5/2/4 */
      }
      else{
	xf86DrvMsg(pScreenInfo->scrnIndex, X_CONFIG,
		   (" *** OVERCLOCKING MEM/CLK mclk=125 sclk=60 ***\n"));
	/* increase Mem/Sys clock on request */
	verite_out32(iob+SCLKPLL, 0xa4854);  /* mclk=125 sclk=60 */
                                        /* M/N/P/P = 84/5/2/4 */
      }
      usleep(500);
    }

    /* this has something to do with memory */
    tmp=verite_in32(iob+DRAMCTL)&0xdfff;              /* reset bit 13 */
    verite_out32(iob+DRAMCTL, tmp|0x330000);
 
    /* program pixel clock */
    if (pRendition->board.chip == V1000_DEVICE) {
        if (110.0 < V1000CalcClock(mode->clock/1000.0, &M, &N, &P)) {
            P++;
            doubleclock=1;
        }
        set_PLL(iob, combineNMP(N, M, P));
    } 
    else {
	tmp = (~0x1800) & verite_in32(iob+DRAMCTL);
        verite_out32(iob+DRAMCTL, tmp);
        V2200CalcClock(mode->clock/1000.0, &M, &N, &P);
        verite_out32(iob+PCLKPLL, v2kcombineNMP(N, M, P));
    }
    usleep(500);

    /* init the ramdac */
    verite_initdac(pScreenInfo, mode->bitsperpixel, doubleclock);

    verite_out32(iob+CRTCHORZ, HORZ(mode->hsyncstart - mode->hdisplay, 
                               mode->hsyncend - mode->hsyncstart,
                               mode->htotal - mode->hsyncend,
                               mode->hdisplay));
    verite_out32(iob+CRTCVERT, VERT(mode->vsyncstart-mode->vdisplay, 
                               mode->vsyncend-mode->vsyncstart,
                               mode->vtotal-mode->vsyncend,
                               mode->vdisplay));

    /* fill in the mode parameters */
    memcpy(&(pRendition->board.mode), mode, sizeof(struct verite_modeinfo_t));
    pRendition->board.mode.fifosize=128;
    pRendition->board.mode.pll_m=M;
    pRendition->board.mode.pll_n=N;
    pRendition->board.mode.pll_p=P;
    pRendition->board.mode.doubleclock=doubleclock;
    if (0 == pRendition->board.mode.virtualwidth)
        pRendition->board.mode.virtualwidth=pRendition->board.mode.screenwidth;

    pRendition->board.init=1;
    (*pScreenInfo->AdjustFrame)(pScreenInfo->scrnIndex,
        pScreenInfo->frameX0, pScreenInfo->frameY0, 0);

    /* Need to fix up syncs */

    /* enable the display */
    verite_out32(iob+CRTCCTL, CTL(0, mode->hsynchi, mode->vsynchi)
                        |mode->pixelformat
                        |CRTCCTL_VIDEOFIFOSIZE128
                        |CRTCCTL_HSYNCENABLE
                        |CRTCCTL_VSYNCENABLE
                        |CRTCCTL_VIDEOENABLE);

#ifdef DEBUG
    ErrorF ("Interlace mode -> %d\n", mode->flags);
    xf86sleep(10);ErrorF ("...Exit SetMode...\n");
#endif

   return 0;
}

static double
verite_findtextclock(int width)
{
    int i = 0, val;
    while ((val = V1000Clocks[i].h_size) > 0) {
	if (val > width) 
	    break;
	else
	    i++;
    }
    if (i > 0) i--;
    return V1000Clocks[i].pll_val;
}

void
verite_save(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int iob=pRendition->board.io_base;
    RenditionRegPtr reg = &pRendition->saveRegs;

    reg->memendian = verite_in8(iob+MEMENDIAN);
    reg->mode = verite_in8(iob+MODEREG);
    reg->sclkpll = verite_in32(iob+SCLKPLL);
    reg->dramctl = verite_in32(iob+DRAMCTL);
    reg->crtch = verite_in32(iob+CRTCHORZ);
    reg->crtcv = verite_in32(iob+CRTCVERT);
    /* clock */
    if (V1000_DEVICE ==pRendition->board.chip) {
	int width;
	/*
	 * I have no idea how to read back the clock from
	 * V1000. Therefore we pick a VESA Mode clock from
	 * a list for the width found.
	 */
	width = ((reg->crtch & 0xff) + 1) << 3;
        reg->pclkpll = verite_findtextclock(width);
	reg->plldev= verite_in8(iob+PLLDEV);
    } else {
	reg->pclkpll = verite_in32(iob+PCLKPLL);
    }
    /* initdac */
    verite_out8(iob+MODEREG,NATIVE_MODE);
    verite_savedac(pScreenInfo);
    verite_out8(iob+MODEREG,reg->mode);

    reg->vbasea = verite_in32(iob+FRAMEBASEA);
    reg->crtcoff = verite_in32(iob+CRTCOFFSET);
    reg->crtcctl = verite_in32(iob+CRTCCTL);
}

void
verite_restore(ScrnInfoPtr pScreenInfo, RenditionRegPtr reg)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int iob=pRendition->board.io_base;

    verite_restoredac (pScreenInfo, reg);
    /*
     * If this is a Verite 1000, restore the MODEREG
     * register now.  The MODEREG gets restored later
     * for the Verite 2x00 because restoring it here
     * has been confirmed to cause intermittent
     * system locks.
     */
    if (pRendition->board.chip == V1000_DEVICE) {
	verite_out32(iob+MODEREG,reg->mode);
    }
    verite_out8(iob+MEMENDIAN,reg->memendian);
    verite_out32(iob+DRAMCTL,reg->dramctl);
    verite_out32(iob+SCLKPLL,reg->sclkpll);
    if (pRendition->board.chip == V1000_DEVICE) {
	/* fixme */
        set_PLL(iob, reg->pclkpll);
	xf86UDelay(10000);
    } else { 
	verite_out32(iob+PCLKPLL,reg->pclkpll);
	/* 
	 * Need to wait 200uS for PLL to stabilize --
	 * let's play it safe with 500 
	 */
	xf86UDelay(10000);
	/* wait until VBLANK */
	while ((verite_in32(iob+CRTCSTATUS)&CRTCSTATUS_VERT_MASK) !=
	       CRTCSTATUS_VERT_ACTIVE);
	while ((verite_in32(iob+CRTCSTATUS)&CRTCSTATUS_VERT_MASK) ==
	       CRTCSTATUS_VERT_ACTIVE);
    }

    /*
     * If this is a Verite 2x00, restore the MODEREG
     * register now.  The MODEREG register is restored
     * earlier for the Verite 1000, but is restored
     * here for the Verite 2x00 to prevent system
     * locks.
     */
    if (pRendition->board.chip != V1000_DEVICE) {
	verite_out32(iob+MODEREG,reg->mode);
    }

    verite_out32(iob+CRTCHORZ,reg->crtch);
    verite_out32(iob+CRTCVERT,reg->crtcv);
    verite_out32(iob+FRAMEBASEA, reg->vbasea);
    verite_out32(iob+CRTCOFFSET,reg->crtcoff);
    verite_out32(iob+CRTCCTL,reg->crtcctl);
}

void
verite_setframebase(ScrnInfoPtr pScreenInfo, vu32 framebase)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

    vu32 offset;
    
    int iob=pRendition->board.io_base;
    int swidth=pRendition->board.mode.screenwidth;
    int vwidth=  pRendition->board.mode.virtualwidth;
    int bytespp=pRendition->board.mode.bitsperpixel>>3;
    int fifo_size=pRendition->board.mode.fifosize;

#ifdef DEBUG
    ErrorF( "Rendition: Debug verite_setframebase w=%d v=%d b=%d f=%d\n", 
        swidth, vwidth, bytespp, fifo_size);
#endif

    /* CRTCOFFSET */
    offset=vwidth*bytespp               /* virtual width in bytes */
          -swidth*bytespp               /* screen width in bytes */
          +((swidth*bytespp)%fifo_size) /* width in bytes modulo fifo size */
        ;

    if (!(   framebase&7            /* framebase multiple of 8? */
          || (swidth*bytespp)%128)) /* screenwidth multiple of fifo size */
      offset+=fifo_size;            /* increment offset by fifosize */

    /* wait for vertical retrace */
#ifndef DEBUG
    if (!pRendition->board.init) {
        while ((verite_in32(iob+CRTCSTATUS) & CRTCSTATUS_VERT_MASK) !=
               CRTCSTATUS_VERT_ACTIVE) ;
        while ((verite_in32(iob+CRTCSTATUS) & CRTCSTATUS_VERT_MASK) ==
               CRTCSTATUS_VERT_ACTIVE) ;
    }
    else
        pRendition->board.init=0;
#endif

    /* framebase */
    verite_out32(iob+FRAMEBASEA, framebase);

    /* crtc offset */
    verite_out32(iob+CRTCOFFSET, offset&0xffff);
}

int
verite_getstride(ScrnInfoPtr pScreenInfo, int *width, 
		 vu16 *stride0, vu16 *stride1)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int bytesperline;
    int c = 0;

    bytesperline
	= pRendition->board.mode.virtualwidth
	* (pRendition->board.mode.bitsperpixel >> 3);
#ifdef DEBUG
    ErrorF("RENDITION: %d bytes per line\n", bytesperline);
#endif

    /* for now, I implemented a linear search only, should be fixed <ml> */
    while (0 != width_to_stride_table[c].width8bpp) {
	if (width_to_stride_table[c].width8bpp == bytesperline 
	    && ((width_to_stride_table[c].chip == pRendition->board.chip) 
		|| (V2000_DEVICE == pRendition->board.chip))) {
            *stride0 = width_to_stride_table[c].stride0;
            *stride1 = width_to_stride_table[c].stride1;
            return 1;
        }
        c++;
    }
    return 0;
}

/*
 * local functions
 */

/*
 * void set_PLL(IOADDRESS iob, vu32 value)
 *
 * Set PLL clock to desired frequency for the V1000.
 */

void
set_PLL(IOADDRESS iob, vu32 value)
{
    vu32 ulD;
    int b;
    
    /* shift out the 20 serial bits */
    for (b=19; b>=0; b--) {
        ulD=(value>>b)&1;
        verite_out8(iob+PLLDEV, (vu8)ulD);
    }
  
    /* read PLL device so the latch is filled with the previously 
     * written value */
    (void)verite_in8(iob+PLLDEV);
}



/* Vxx00CalcClock -- finds PLL parameters to match target
 *                   frequency (in megahertz)
 *
 *                   Brute force, but takes less than a tenth
 *                   of a second and the function is only called
 *                   O(1) times during program execution.
 */
static double
V1000CalcClock(double target, int *M, int *N, int *P)
{
    double mindiff = 1e10;
    double vco, pcf, diff, freq;
    int mm, nn, pp;

    for (pp=0; pp<4; pp++)
        for (nn=1; nn<=129; nn++)
            for (mm=1; mm<=129; mm++) {
                vco=V1_REF_FREQ*2.0*mm/nn;
                if ((vco<V1_MIN_VCO_FREQ) || (vco>V1_MAX_VCO_FREQ))
                    continue;
            	pcf = V1_REF_FREQ/nn;
            	if ((pcf<V1_MIN_PCF_FREQ) || (pcf>V1_MAX_PCF_FREQ))
                    continue;
            	freq=vco/(1<<pp);
            	diff=fabs(target-freq);
            	if (diff < mindiff) {
                    *M=mm; 
                    *N=nn; 
                    *P=pp;
                    mindiff=diff;
                }
            }
 
    vco=V1_REF_FREQ*2*(*M)/(*N);
    pcf=V1_REF_FREQ/(*N);
    freq=vco/(1<<(*P));

#ifdef DEBUG
    ErrorF(
        "RENDITION: target=%f freq=%f vco=%f pcf=%f n=%d m=%d p=%d\n",
        target, freq, vco, pcf, *N, *M, *P);
#endif

    return freq;
}



static double
V2200CalcClock(double target, int *m, int *n, int *p)
{
    double mindiff = 1e10;
    double vco, pcf, diff, freq;
    int mm, nn, pp;

    for (pp=1; pp<=0x0f; pp++)
        for (nn=1; nn<=0x3f; nn++)
            for (mm=1; mm<=0xff; mm++) {
                vco = V2_REF_FREQ*mm/nn;
                if ((vco < V2_MIN_VCO_FREQ) || (vco > V2_MAX_VCO_FREQ))
                    continue;
            	pcf = V2_REF_FREQ*mm/nn;
                if ((vco < V2_MIN_VCO_FREQ) || (vco > V2_MAX_VCO_FREQ))
                    continue;
            	pcf = V2_REF_FREQ/nn;
            	if ((pcf < V2_MIN_PCF_FREQ) || (pcf > V2_MAX_PCF_FREQ))
                    continue;
            	freq = vco/pp;
            	diff = fabs(target-freq);
            	if (diff < mindiff) {
                    *m = mm; *n = nn; *p = pp;
                    mindiff = diff;
                }
            }
 
    vco = V2_REF_FREQ * *m / *n;
    pcf = V2_REF_FREQ / *n;
    freq = vco / *p;

#ifdef DEBUG
    ErrorF(
        "RENDITION: target=%f freq=%f vco=%f pcf=%f n=%d m=%d p=%d\n",
        target, freq, vco, pcf, *n, *m, *p);
#endif

    return freq;
}
 

/*
 * end of file vmodes.c
 */

