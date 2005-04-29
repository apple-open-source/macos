/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_vbe.c,v 1.14 2003/06/18 16:17:40 eich Exp $ */

#include "savage_driver.h"
#include "savage_vbe.h"

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
#define B_O16(x)  (x) 
#define B_O32(x)  (x)
#else
#define B_O16(x)  ((((x) & 0xff) << 8) | (((x) & 0xff) >> 8))
#define B_O32(x)  ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) \
                  | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))
#endif
#define L_ADD(x)  (B_O32(x) & 0xffff) + ((B_O32(x) >> 12) & 0xffff00)

Bool vbeModeInit( vbeInfoPtr, int );
static int SavageGetDevice( SavagePtr psav );
/*static int SavageGetTVType( SavagePtr psav );*/

static void
SavageClearVM86Regs( xf86Int10InfoPtr pInt )
{
    pInt->ax = 0;
    pInt->bx = 0;
    pInt->cx = 0;
    pInt->dx = 0;
    pInt->si = 0;
    pInt->di = 0;
    pInt->es = 0xc000;
    pInt->num = 0x10;
}

void
SavageSetTextMode( SavagePtr psav )
{
    /* Restore display device if changed. */
    if( psav->iDevInfo != psav->iDevInfoPrim ) {
	SavageClearVM86Regs( psav->pInt10 );
	psav->pInt10->ax = 0x4f14;
	psav->pInt10->bx = 0x0003;
	psav->pInt10->cx = psav->iDevInfoPrim;
	xf86ExecX86int10( psav->pInt10 );
    }

    SavageClearVM86Regs( psav->pInt10 );

    psav->pInt10->ax = 0x83;

    xf86ExecX86int10( psav->pInt10 );
}


void
SavageSetVESAMode( SavagePtr psav, int n, int Refresh )
{
    int iDevInfo;
    static int iCount = 0;

    /* Get current display device status. */

    iDevInfo = SavageGetDevice(psav);
    psav->iDevInfo = iDevInfo;
    if( !iCount++ )
	psav->iDevInfoPrim = psav->iDevInfo;
    if( psav->CrtOnly )
	psav->iDevInfo = CRT_ACTIVE;
    if( psav->TvOn )
	psav->iDevInfo = TV_ACTIVE;

    /* Establish the refresh rate for this mode. */

    SavageClearVM86Regs( psav->pInt10 );
    psav->pInt10->ax = 0x4f14;	/* S3 extensions */
    psav->pInt10->bx = 0x0001;	/* Set default refresh rate */
    psav->pInt10->cx = n & 0x3fff;
    psav->pInt10->di = Refresh & 0xffff;

    xf86ExecX86int10( psav->pInt10 );

    /* Set TV type if TV is on. */
    if( psav->TvOn ) {
	SavageClearVM86Regs( psav->pInt10 );
	psav->pInt10->ax = 0x4f14;	/* S3 extensions */
	psav->pInt10->bx = 0x0007;	/* TV extensions */
	psav->pInt10->cx = psav->PAL ? 0x08 : 0x04;
	psav->pInt10->dx = 0x0c;
	xf86ExecX86int10( psav->pInt10 );
    }

    /* Manipulate output device set. */
    if( psav->iDevInfo != iDevInfo ) {
	SavageClearVM86Regs( psav->pInt10 );
	psav->pInt10->ax = 0x4f14;	/* S3 extensions */
	psav->pInt10->bx = 0x0003;	/* set active devices */
	psav->pInt10->cx = psav->PAL ? 0x08 : 0x04;
	xf86ExecX86int10( psav->pInt10 );

	/* Re-fetch actual device set. */
	psav->iDevInfo = SavageGetDevice( psav );
	iDevInfo = psav->iDevInfo;
	psav->CrtOnly = (iDevInfo == 1);
	psav->TvOn = !!(iDevInfo & 4);
    }

    /* Now, make this mode current. */

    if( xf86LoaderCheckSymbol( "VBESetVBEMode" ) )
    {
	if( !VBESetVBEMode( psav->pVbe, n, NULL ) )
	{
	    ErrorF("Set video mode failed\n");
	}
    }
#ifdef XFree86LOADER
    else
    {
	if( !vbeModeInit( psav->pVbe, n ) )
	{
	    ErrorF("Set video mode failed\n");
	}
    }
#endif
}


/* Function to get supported device list. */

static int SavageGetDevice( SavagePtr psav )
{
    SavageClearVM86Regs( psav->pInt10 );
    psav->pInt10->ax = 0x4f14;	/* S3 extensions */
    psav->pInt10->bx = 0x0103;	/* get active devices */

    xf86ExecX86int10( psav->pInt10 );

    return ((psav->pInt10->cx) & 0xf);
}


void
SavageFreeBIOSModeTable( SavagePtr psav, SavageModeTablePtr* ppTable )
{
    int i;
    SavageModeEntryPtr pMode = (*ppTable)->Modes;

    for( i = (*ppTable)->NumModes; i--; )
    {
	if( pMode->RefreshRate )
	{
	    xfree( pMode->RefreshRate );
	    pMode->RefreshRate = NULL;
	}
	pMode++;
    }

    xfree( *ppTable );
}


SavageModeTablePtr
SavageGetBIOSModeTable( SavagePtr psav, int iDepth )
{
    int nModes = SavageGetBIOSModes( psav, iDepth, NULL );
    SavageModeTablePtr pTable;

    pTable = (SavageModeTablePtr) 
	xcalloc( 1, sizeof(SavageModeTableRec) + 
		    (nModes-1) * sizeof(SavageModeEntry) );
    if( pTable ) {
	pTable->NumModes = nModes;
	SavageGetBIOSModes( psav, iDepth, pTable->Modes );
    }

    return pTable;
}


unsigned short
SavageGetBIOSModes( 
    SavagePtr psav,
    int iDepth,
    SavageModeEntryPtr s3vModeTable )
{
    unsigned short iModeCount = 0;
    unsigned short int *mode_list;
    pointer vbeLinear = NULL;
    vbeControllerInfoPtr vbe = NULL;
    int vbeReal;
    struct vbe_mode_info_block * vmib;

    if( !psav->pVbe )
	return 0;

    vbe = (vbeControllerInfoPtr) psav->pVbe->memory;
    vbeLinear = xf86Int10AllocPages( psav->pInt10, 1, &vbeReal );
    if( !vbeLinear )
    {
	ErrorF( "Cannot allocate scratch page in real mode memory." );
	return 0;
    }
    vmib = (struct vbe_mode_info_block *) vbeLinear;
    
    for (
	mode_list = xf86int10Addr( psav->pInt10, L_ADD(vbe->VideoModePtr) );
	*mode_list != 0xffff;
	mode_list++
    )
    {
	/*
	 * This is a HACK to work around what I believe is a BUG in the
	 * Toshiba Satellite BIOSes in 08/2000 and 09/2000.  The BIOS
	 * table for 1024x600 says it has six refresh rates, when in fact
	 * it only has 3.  When I ask for rate #4, the BIOS goes into an 
	 * infinite loop until the user interrupts it, usually by pressing
	 * Ctrl-Alt-F1.  For now, we'll just punt everything with a VESA
	 * number greater than or equal to 0200.
	 *
	 * This also prevents some strange and unusual results seen with
	 * the later ProSavage/PM133 BIOSes directly from S3/VIA.
	 */
	if( *mode_list >= 0x0200 )
	    continue;

	SavageClearVM86Regs( psav->pInt10 );

	psav->pInt10->ax = 0x4f01;
	psav->pInt10->cx = *mode_list;
	psav->pInt10->es = SEG_ADDR(vbeReal);
	psav->pInt10->di = SEG_OFF(vbeReal);
	psav->pInt10->num = 0x10;

	xf86ExecX86int10( psav->pInt10 );

	if( 
	   (vmib->bits_per_pixel == iDepth) &&
	   (
	      (vmib->memory_model == VBE_MODEL_256) ||
	      (vmib->memory_model == VBE_MODEL_PACKED) ||
	      (vmib->memory_model == VBE_MODEL_RGB)
	   )
	)
	{
	    /* This mode is a match. */

	    iModeCount++;

	    /* If we're supposed to fetch information, do it now. */

	    if( s3vModeTable )
	    {
	        int iRefresh = 0;

		s3vModeTable->Width = vmib->x_resolution;
		s3vModeTable->Height = vmib->y_resolution;
		s3vModeTable->VesaMode = *mode_list;
		
		/* Query the refresh rates at this mode. */

		psav->pInt10->cx = *mode_list;
		psav->pInt10->dx = 0;

		do
		{
		    if( (iRefresh % 8) == 0 )
		    {
			if( s3vModeTable->RefreshRate )
			{
			    s3vModeTable->RefreshRate = (unsigned char *)
				xrealloc( 
				    s3vModeTable->RefreshRate,
				    (iRefresh+8) * sizeof(unsigned char)
				);
			}
			else
			{
			    s3vModeTable->RefreshRate = (unsigned char *)
				xcalloc( 
				    sizeof(unsigned char),
				    (iRefresh+8)
				);
			}
		    }

		    psav->pInt10->ax = 0x4f14;	/* S3 extended functions */
		    psav->pInt10->bx = 0x0201;	/* query refresh rates */
		    psav->pInt10->num = 0x10;
		    xf86ExecX86int10( psav->pInt10 );

		    s3vModeTable->RefreshRate[iRefresh++] = psav->pInt10->di;
		}
		while( psav->pInt10->dx );

		s3vModeTable->RefreshCount = iRefresh;

	    	s3vModeTable++;
	    }
	}
    }

    xf86Int10FreePages( psav->pInt10, vbeLinear, 1 );

    return iModeCount;
}
