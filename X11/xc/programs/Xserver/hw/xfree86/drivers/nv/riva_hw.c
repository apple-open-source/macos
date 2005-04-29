 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_hw.c,v 1.52 2003/11/03 05:11:26 tsi Exp $ */

#include "riva_local.h"
#include "compiler.h"
#include "riva_include.h"
#include "riva_hw.h"
#include "riva_tbl.h"

/*
 * This file is an OS-agnostic file used to make RIVA 128 and RIVA TNT
 * operate identically (except TNT has more memory and better 3D quality.
 */
static int nv3Busy
(
    RIVA_HW_INST *chip
)
{
    return ((chip->Rop->FifoFree < chip->FifoEmptyCount) || (chip->PGRAPH[0x000006B0/4] & 0x01));
}
static void vgaLockUnlock
(
    RIVA_HW_INST *chip,
    Bool           Lock
)
{
    CARD8 cr11;
    VGA_WR08(chip->PCIO, 0x3D4, 0x11);
    cr11 = VGA_RD08(chip->PCIO, 0x3D5);
    if(Lock) cr11 |= 0x80;
    else cr11 &= ~0x80;
    VGA_WR08(chip->PCIO, 0x3D5, cr11);
}

static void nv3LockUnlock
(
    RIVA_HW_INST *chip,
    Bool           Lock
)
{
    VGA_WR08(chip->PVIO, 0x3C4, 0x06);
    VGA_WR08(chip->PVIO, 0x3C5, Lock ? 0x99 : 0x57);
    vgaLockUnlock(chip, Lock);
}
static int ShowHideCursor
(
    RIVA_HW_INST *chip,
    int           ShowHide
)
{
    int current;
    current                     =  chip->CurrentState->cursor1;
    chip->CurrentState->cursor1 = (chip->CurrentState->cursor1 & 0xFE) |
	                          (ShowHide & 0x01);
    VGA_WR08(chip->PCIO, 0x3D4, 0x31);
    VGA_WR08(chip->PCIO, 0x3D5, chip->CurrentState->cursor1);
    return (current & 0x01);
}

/****************************************************************************\
*                                                                            *
* The video arbitration routines calculate some "magic" numbers.  Fixes      *
* the snow seen when accessing the framebuffer without it.                   *
* It just works (I hope).                                                    *
*                                                                            *
\****************************************************************************/

#define DEFAULT_GR_LWM 100
#define DEFAULT_VID_LWM 100
#define DEFAULT_GR_BURST_SIZE 256
#define DEFAULT_VID_BURST_SIZE 128
#define VIDEO		0
#define GRAPHICS	1
#define MPORT		2
#define ENGINE		3
#define GFIFO_SIZE	320
#define GFIFO_SIZE_128	256
#define MFIFO_SIZE	120
#define VFIFO_SIZE	256
#define	ABS(a)	(a>0?a:-a)
typedef struct {
  int gdrain_rate;
  int vdrain_rate;
  int mdrain_rate;
  int gburst_size;
  int vburst_size;
  char vid_en;
  char gr_en;
  int wcmocc, wcgocc, wcvocc, wcvlwm, wcglwm;
  int by_gfacc;
  char vid_only_once;
  char gr_only_once;
  char first_vacc;
  char first_gacc;
  char first_macc;
  int vocc;
  int gocc;
  int mocc;
  char cur;
  char engine_en;
  char converged;
  int priority;
} nv3_arb_info;
typedef struct {
  int graphics_lwm;
  int video_lwm;
  int graphics_burst_size;
  int video_burst_size;
  int graphics_hi_priority;
  int media_hi_priority;
  int rtl_values;
  int valid;
} nv3_fifo_info;
typedef struct {
  char pix_bpp;
  char enable_video;
  char gr_during_vid;
  char enable_mp;
  int memory_width;
  int video_scale;
  int pclk_khz;
  int mclk_khz;
  int mem_page_miss;
  int mem_latency;
  char mem_aligned;
} nv3_sim_state;
static int nv3_iterate(nv3_fifo_info *res_info, nv3_sim_state * state, nv3_arb_info *ainfo)
{
    int iter = 0;
    int tmp;
    int vfsize, mfsize, gfsize;
    int mburst_size = 32;
    int mmisses, gmisses, vmisses;
    int misses;
    int vlwm, glwm;
    int last, next, cur;
    int max_gfsize ;
    long ns;

    vlwm = 0;
    glwm = 0;
    vfsize = 0;
    gfsize = 0;
    cur = ainfo->cur;
    mmisses = 2;
    gmisses = 2;
    vmisses = 2;
    if (ainfo->gburst_size == 128) max_gfsize = GFIFO_SIZE_128;
    else  max_gfsize = GFIFO_SIZE;
    max_gfsize = GFIFO_SIZE;
    while (1)
    {
        if (ainfo->vid_en)
        {
            if (ainfo->wcvocc > ainfo->vocc) ainfo->wcvocc = ainfo->vocc;
            if (ainfo->wcvlwm > vlwm) ainfo->wcvlwm = vlwm ;
            ns = 1000000 * ainfo->vburst_size/(state->memory_width/8)/state->mclk_khz;
            vfsize = ns * ainfo->vdrain_rate / 1000000;
            vfsize =  ainfo->wcvlwm - ainfo->vburst_size + vfsize;
        }
        if (state->enable_mp)
        {
            if (ainfo->wcmocc > ainfo->mocc) ainfo->wcmocc = ainfo->mocc;
        }
        if (ainfo->gr_en)
        {
            if (ainfo->wcglwm > glwm) ainfo->wcglwm = glwm ;
            if (ainfo->wcgocc > ainfo->gocc) ainfo->wcgocc = ainfo->gocc;
            ns = 1000000 * (ainfo->gburst_size/(state->memory_width/8))/state->mclk_khz;
            gfsize = (ns * (long) ainfo->gdrain_rate)/1000000;
            gfsize = ainfo->wcglwm - ainfo->gburst_size + gfsize;
        }
        mfsize = 0;
        if (!state->gr_during_vid && ainfo->vid_en)
            if (ainfo->vid_en && (ainfo->vocc < 0) && !ainfo->vid_only_once)
                next = VIDEO;
            else if (ainfo->mocc < 0)
                next = MPORT;
            else if (ainfo->gocc< ainfo->by_gfacc)
                next = GRAPHICS;
            else return (0);
        else switch (ainfo->priority)
            {
                case VIDEO:
                    if (ainfo->vid_en && ainfo->vocc<0 && !ainfo->vid_only_once)
                        next = VIDEO;
                    else if (ainfo->gr_en && ainfo->gocc<0 && !ainfo->gr_only_once)
                        next = GRAPHICS;
                    else if (ainfo->mocc<0)
                        next = MPORT;
                    else    return (0);
                    break;
                case GRAPHICS:
                    if (ainfo->gr_en && ainfo->gocc<0 && !ainfo->gr_only_once)
                        next = GRAPHICS;
                    else if (ainfo->vid_en && ainfo->vocc<0 && !ainfo->vid_only_once)
                        next = VIDEO;
                    else if (ainfo->mocc<0)
                        next = MPORT;
                    else    return (0);
                    break;
                default:
                    if (ainfo->mocc<0)
                        next = MPORT;
                    else if (ainfo->gr_en && ainfo->gocc<0 && !ainfo->gr_only_once)
                        next = GRAPHICS;
                    else if (ainfo->vid_en && ainfo->vocc<0 && !ainfo->vid_only_once)
                        next = VIDEO;
                    else    return (0);
                    break;
            }
        last = cur;
        cur = next;
        iter++;
        switch (cur)
        {
            case VIDEO:
                if (last==cur)    misses = 0;
                else if (ainfo->first_vacc)   misses = vmisses;
                else    misses = 1;
                ainfo->first_vacc = 0;
                if (last!=cur)
                {
                    ns =  1000000 * (vmisses*state->mem_page_miss + state->mem_latency)/state->mclk_khz; 
                    vlwm = ns * ainfo->vdrain_rate/ 1000000;
                    vlwm = ainfo->vocc - vlwm;
                }
                ns = 1000000*(misses*state->mem_page_miss + ainfo->vburst_size)/(state->memory_width/8)/state->mclk_khz;
                ainfo->vocc = ainfo->vocc + ainfo->vburst_size - ns*ainfo->vdrain_rate/1000000;
                ainfo->gocc = ainfo->gocc - ns*ainfo->gdrain_rate/1000000;
                ainfo->mocc = ainfo->mocc - ns*ainfo->mdrain_rate/1000000;
                break;
            case GRAPHICS:
                if (last==cur)    misses = 0;
                else if (ainfo->first_gacc)   misses = gmisses;
                else    misses = 1;
                ainfo->first_gacc = 0;
                if (last!=cur)
                {
                    ns = 1000000*(gmisses*state->mem_page_miss + state->mem_latency)/state->mclk_khz ;
                    glwm = ns * ainfo->gdrain_rate/1000000;
                    glwm = ainfo->gocc - glwm;
                }
                ns = 1000000*(misses*state->mem_page_miss + ainfo->gburst_size/(state->memory_width/8))/state->mclk_khz;
                ainfo->vocc = ainfo->vocc + 0 - ns*ainfo->vdrain_rate/1000000;
                ainfo->gocc = ainfo->gocc + ainfo->gburst_size - ns*ainfo->gdrain_rate/1000000;
                ainfo->mocc = ainfo->mocc + 0 - ns*ainfo->mdrain_rate/1000000;
                break;
            default:
                if (last==cur)    misses = 0;
                else if (ainfo->first_macc)   misses = mmisses;
                else    misses = 1;
                ainfo->first_macc = 0;
                ns = 1000000*(misses*state->mem_page_miss + mburst_size/(state->memory_width/8))/state->mclk_khz;
                ainfo->vocc = ainfo->vocc + 0 - ns*ainfo->vdrain_rate/1000000;
                ainfo->gocc = ainfo->gocc + 0 - ns*ainfo->gdrain_rate/1000000;
                ainfo->mocc = ainfo->mocc + mburst_size - ns*ainfo->mdrain_rate/1000000;
                break;
        }
        if (iter>100)
        {
            ainfo->converged = 0;
            return (1);
        }
        ns = 1000000*ainfo->gburst_size/(state->memory_width/8)/state->mclk_khz;
        tmp = ns * ainfo->gdrain_rate/1000000;
        if (ABS(ainfo->gburst_size) + ((ABS(ainfo->wcglwm) + 16 ) & ~0x7) - tmp > max_gfsize)
        {
            ainfo->converged = 0;
            return (1);
        }
        ns = 1000000*ainfo->vburst_size/(state->memory_width/8)/state->mclk_khz;
        tmp = ns * ainfo->vdrain_rate/1000000;
        if (ABS(ainfo->vburst_size) + (ABS(ainfo->wcvlwm + 32) & ~0xf)  - tmp> VFIFO_SIZE)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(ainfo->gocc) > max_gfsize)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(ainfo->vocc) > VFIFO_SIZE)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(ainfo->mocc) > MFIFO_SIZE)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(vfsize) > VFIFO_SIZE)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(gfsize) > max_gfsize)
        {
            ainfo->converged = 0;
            return (1);
        }
        if (ABS(mfsize) > MFIFO_SIZE)
        {
            ainfo->converged = 0;
            return (1);
        }
    }
}
static char nv3_arb(nv3_fifo_info * res_info, nv3_sim_state * state,  nv3_arb_info *ainfo) 
{
    long ens, vns, mns, gns;
    int mmisses, gmisses, vmisses, eburst_size, mburst_size;
    int refresh_cycle;

    refresh_cycle = 0;
    refresh_cycle = 2*(state->mclk_khz/state->pclk_khz) + 5;
    mmisses = 2;
    if (state->mem_aligned) gmisses = 2;
    else    gmisses = 3;
    vmisses = 2;
    eburst_size = state->memory_width * 1;
    mburst_size = 32;
    gns = 1000000 * (gmisses*state->mem_page_miss + state->mem_latency)/state->mclk_khz;
    ainfo->by_gfacc = gns*ainfo->gdrain_rate/1000000;
    ainfo->wcmocc = 0;
    ainfo->wcgocc = 0;
    ainfo->wcvocc = 0;
    ainfo->wcvlwm = 0;
    ainfo->wcglwm = 0;
    ainfo->engine_en = 1;
    ainfo->converged = 1;
    if (ainfo->engine_en)
    {
        ens =  1000000*(state->mem_page_miss + eburst_size/(state->memory_width/8) +refresh_cycle)/state->mclk_khz;
        ainfo->mocc = state->enable_mp ? 0-ens*ainfo->mdrain_rate/1000000 : 0;
        ainfo->vocc = ainfo->vid_en ? 0-ens*ainfo->vdrain_rate/1000000 : 0;
        ainfo->gocc = ainfo->gr_en ? 0-ens*ainfo->gdrain_rate/1000000 : 0;
        ainfo->cur = ENGINE;
        ainfo->first_vacc = 1;
        ainfo->first_gacc = 1;
        ainfo->first_macc = 1;
        nv3_iterate(res_info, state,ainfo);
    }
    if (state->enable_mp)
    {
        mns = 1000000 * (mmisses*state->mem_page_miss + mburst_size/(state->memory_width/8) + refresh_cycle)/state->mclk_khz;
        ainfo->mocc = state->enable_mp ? 0 : mburst_size - mns*ainfo->mdrain_rate/1000000;
        ainfo->vocc = ainfo->vid_en ? 0 : 0- mns*ainfo->vdrain_rate/1000000;
        ainfo->gocc = ainfo->gr_en ? 0: 0- mns*ainfo->gdrain_rate/1000000;
        ainfo->cur = MPORT;
        ainfo->first_vacc = 1;
        ainfo->first_gacc = 1;
        ainfo->first_macc = 0;
        nv3_iterate(res_info, state,ainfo);
    }
    if (ainfo->gr_en)
    {
        ainfo->first_vacc = 1;
        ainfo->first_gacc = 0;
        ainfo->first_macc = 1;
        gns = 1000000*(gmisses*state->mem_page_miss + ainfo->gburst_size/(state->memory_width/8) + refresh_cycle)/state->mclk_khz;
        ainfo->gocc = ainfo->gburst_size - gns*ainfo->gdrain_rate/1000000;
        ainfo->vocc = ainfo->vid_en? 0-gns*ainfo->vdrain_rate/1000000 : 0;
        ainfo->mocc = state->enable_mp ?  0-gns*ainfo->mdrain_rate/1000000: 0;
        ainfo->cur = GRAPHICS;
        nv3_iterate(res_info, state,ainfo);
    }
    if (ainfo->vid_en)
    {
        ainfo->first_vacc = 0;
        ainfo->first_gacc = 1;
        ainfo->first_macc = 1;
        vns = 1000000*(vmisses*state->mem_page_miss + ainfo->vburst_size/(state->memory_width/8) + refresh_cycle)/state->mclk_khz;
        ainfo->vocc = ainfo->vburst_size - vns*ainfo->vdrain_rate/1000000;
        ainfo->gocc = ainfo->gr_en? (0-vns*ainfo->gdrain_rate/1000000) : 0;
        ainfo->mocc = state->enable_mp? 0-vns*ainfo->mdrain_rate/1000000 :0 ;
        ainfo->cur = VIDEO;
        nv3_iterate(res_info, state, ainfo);
    }
    if (ainfo->converged)
    {
        res_info->graphics_lwm = (int)ABS(ainfo->wcglwm) + 16;
        res_info->video_lwm = (int)ABS(ainfo->wcvlwm) + 32;
        res_info->graphics_burst_size = ainfo->gburst_size;
        res_info->video_burst_size = ainfo->vburst_size;
        res_info->graphics_hi_priority = (ainfo->priority == GRAPHICS);
        res_info->media_hi_priority = (ainfo->priority == MPORT);
        if (res_info->video_lwm > 160)
        {
            res_info->graphics_lwm = 256;
            res_info->video_lwm = 128;
            res_info->graphics_burst_size = 64;
            res_info->video_burst_size = 64;
            res_info->graphics_hi_priority = 0;
            res_info->media_hi_priority = 0;
            ainfo->converged = 0;
            return (0);
        }
        if (res_info->video_lwm > 128)
        {
            res_info->video_lwm = 128;
        }
        return (1);
    }
    else
    {
        res_info->graphics_lwm = 256;
        res_info->video_lwm = 128;
        res_info->graphics_burst_size = 64;
        res_info->video_burst_size = 64;
        res_info->graphics_hi_priority = 0;
        res_info->media_hi_priority = 0;
        return (0);
    }
}
static char nv3_get_param(nv3_fifo_info *res_info, nv3_sim_state * state, nv3_arb_info *ainfo)
{
    int done, g,v, p;
    
    done = 0;
    for (p=0; p < 2; p++)
    {
        for (g=128 ; g > 32; g= g>> 1)
        {
            for (v=128; v >=32; v = v>> 1)
            {
                ainfo->priority = p;
                ainfo->gburst_size = g;     
                ainfo->vburst_size = v;
                done = nv3_arb(res_info, state,ainfo);
                if (done && (g==128))
                    if ((res_info->graphics_lwm + g) > 256)
                        done = 0;
                if (done)
                    goto Done;
            }
        }
    }

 Done:
    return done;
}
static void nv3CalcArbitration 
(
    nv3_fifo_info * res_info,
    nv3_sim_state * state
)
{
    nv3_fifo_info save_info;
    nv3_arb_info ainfo;
    char   res_gr, res_vid;

    ainfo.gr_en = 1;
    ainfo.vid_en = state->enable_video;
    ainfo.vid_only_once = 0;
    ainfo.gr_only_once = 0;
    ainfo.gdrain_rate = (int) state->pclk_khz * (state->pix_bpp/8);
    ainfo.vdrain_rate = (int) state->pclk_khz * 2;
    if (state->video_scale != 0)
        ainfo.vdrain_rate = ainfo.vdrain_rate/state->video_scale;
    ainfo.mdrain_rate = 33000;
    res_info->rtl_values = 0;
    if (!state->gr_during_vid && state->enable_video)
    {
        ainfo.gr_only_once = 1;
        ainfo.gr_en = 1;
        ainfo.gdrain_rate = 0;
        res_vid = nv3_get_param(res_info, state,  &ainfo);
        res_vid = ainfo.converged;
        save_info.video_lwm = res_info->video_lwm;
        save_info.video_burst_size = res_info->video_burst_size;
        ainfo.vid_en = 1;
        ainfo.vid_only_once = 1;
        ainfo.gr_en = 1;
        ainfo.gdrain_rate = (int) state->pclk_khz * (state->pix_bpp/8);
        ainfo.vdrain_rate = 0;
        res_gr = nv3_get_param(res_info, state,  &ainfo);
        res_gr = ainfo.converged;
        res_info->video_lwm = save_info.video_lwm;
        res_info->video_burst_size = save_info.video_burst_size;
        res_info->valid = res_gr & res_vid;
    }
    else
    {
        if (!ainfo.gr_en) ainfo.gdrain_rate = 0;
        if (!ainfo.vid_en) ainfo.vdrain_rate = 0;
        res_gr = nv3_get_param(res_info, state,  &ainfo);
        res_info->valid = ainfo.converged;
    }
}
static void nv3UpdateArbitrationSettings
(
    unsigned      VClk, 
    unsigned      pixelDepth, 
    unsigned     *burst,
    unsigned     *lwm,
    RIVA_HW_INST *chip
)
{
    nv3_fifo_info fifo_data;
    nv3_sim_state sim_data;
    unsigned int M, N, P, pll, MClk;
    
    pll = chip->PRAMDAC[0x00000504/4];
    M = (pll >> 0) & 0xFF; N = (pll >> 8) & 0xFF; P = (pll >> 16) & 0x0F;
    MClk = (N * chip->CrystalFreqKHz / M) >> P;
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 0;
    sim_data.enable_mp      = 0;
    sim_data.video_scale    = 1;
    sim_data.memory_width   = (chip->PEXTDEV[0x00000000/4] & 0x10) ? 128 : 64;
    sim_data.memory_width   = 128;

    sim_data.mem_latency    = 9;
    sim_data.mem_aligned    = 1;
    sim_data.mem_page_miss  = 11;
    sim_data.gr_during_vid  = 0;
    sim_data.pclk_khz       = VClk;
    sim_data.mclk_khz       = MClk;
    nv3CalcArbitration(&fifo_data, &sim_data);
    if (fifo_data.valid)
    {
        int  b = fifo_data.graphics_burst_size >> 4;
        *burst = 0;
        while (b >>= 1) (*burst)++;
        *lwm   = fifo_data.graphics_lwm >> 3;
    }
    else
    {
        *lwm   = 0x24;
        *burst = 0x2;
    }
}

/****************************************************************************\
*                                                                            *
*                          RIVA Mode State Routines                          *
*                                                                            *
\****************************************************************************/

/*
 * Calculate the Video Clock parameters for the PLL.
 */
static int CalcVClock
(
    int           clockIn,
    int          *clockOut,
    int          *mOut,
    int          *nOut,
    int          *pOut,
    RIVA_HW_INST *chip
)
{
    unsigned lowM, highM, highP;
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;
    
    DeltaOld = 0xFFFFFFFF;

    VClk     = (unsigned)clockIn;
    
    if (chip->CrystalFreqKHz == 13500)
    {
        lowM  = 7;
        highM = 12;
    }                      
    else
    {
        lowM  = 8;
        highM = 13;
    }

    highP = 3;
    for (P = 0; P <= highP; P ++)
    {
        Freq = VClk << P;
        if ((Freq >= 128000) && (Freq <= chip->MaxVClockFreqKHz))
        {
            for (M = lowM; M <= highM; M++)
            {
                N    = (VClk << P) * M / chip->CrystalFreqKHz;
                if(N <= 255) {
                    Freq = (chip->CrystalFreqKHz * N / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
                    if (DeltaNew < DeltaOld)
                    {
                        *mOut     = M;
                        *nOut     = N;
                        *pOut     = P;
                        *clockOut = Freq;
                        DeltaOld  = DeltaNew;
                    }
                }
            }
        }
    }
    return (DeltaOld != 0xFFFFFFFF);
}
/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 */
static void CalcStateExt
(
    RIVA_HW_INST  *chip,
    RIVA_HW_STATE *state,
    int            bpp,
    int            width,
    int            hDisplaySize,
    int            height,
    int            dotClock,
    int		   flags 
)
{
    int pixelDepth, VClk, m, n, p;
    /*
     * Save mode parameters.
     */
    state->bpp    = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */
    state->width  = width;
    state->height = height;
    /*
     * Extended RIVA registers.
     */
    pixelDepth = (bpp + 1)/8;
    CalcVClock(dotClock, &VClk, &m, &n, &p, chip);

    nv3UpdateArbitrationSettings(VClk, 
                                 pixelDepth * 8, 
                                 &(state->arbitration0),
                                 &(state->arbitration1),
                                 chip);
    state->cursor0  = 0x00;
    state->cursor1  = 0x78;
    if (flags & V_DBLSCAN)
       state->cursor1 |= 2;
    state->cursor2  = 0x00000000;
    state->pllsel   = 0x10010100;
    state->config   = ((width + 31)/32)
                      | (((pixelDepth > 2) ? 3 : pixelDepth) << 8)
                      | 0x1000;
    state->general  = 0x00100100;
    state->repaint1 = hDisplaySize < 1280 ? 0x06 : 0x02;


    state->vpll     = (p << 16) | (n << 8) | m;
    state->repaint0 = (((width/8)*pixelDepth) & 0x700) >> 3;
    state->pixel    = pixelDepth > 2   ? 3    : pixelDepth;
    state->offset   = 0;
    state->pitch    = pixelDepth * width;
}
/*
 * Load fixed function state and pre-calculated/stored state.
 */
#define LOAD_FIXED_STATE(tbl,dev)                                       \
    for (i = 0; i < sizeof(tbl##Table##dev)/8; i++)                 \
        chip->dev[tbl##Table##dev[i][0]] = tbl##Table##dev[i][1]
#define LOAD_FIXED_STATE_8BPP(tbl,dev)                                  \
    for (i = 0; i < sizeof(tbl##Table##dev##_8BPP)/8; i++)            \
        chip->dev[tbl##Table##dev##_8BPP[i][0]] = tbl##Table##dev##_8BPP[i][1]
#define LOAD_FIXED_STATE_15BPP(tbl,dev)                                 \
    for (i = 0; i < sizeof(tbl##Table##dev##_15BPP)/8; i++)           \
        chip->dev[tbl##Table##dev##_15BPP[i][0]] = tbl##Table##dev##_15BPP[i][1]
#define LOAD_FIXED_STATE_16BPP(tbl,dev)                                 \
    for (i = 0; i < sizeof(tbl##Table##dev##_16BPP)/8; i++)           \
        chip->dev[tbl##Table##dev##_16BPP[i][0]] = tbl##Table##dev##_16BPP[i][1]
#define LOAD_FIXED_STATE_32BPP(tbl,dev)                                 \
    for (i = 0; i < sizeof(tbl##Table##dev##_32BPP)/8; i++)           \
        chip->dev[tbl##Table##dev##_32BPP[i][0]] = tbl##Table##dev##_32BPP[i][1]
static void UpdateFifoState
(
    RIVA_HW_INST  *chip
)
{
}
static void LoadStateExt
(
    RIVA_HW_INST  *chip,
    RIVA_HW_STATE *state
)
{
    int i;

    /*
     * Load HW fixed function state.
     */
    LOAD_FIXED_STATE(Riva,PMC);
    LOAD_FIXED_STATE(Riva,PTIMER);
     /*
      * Make sure frame buffer config gets set before loading PRAMIN.
      */
    chip->PFB[0x00000200/4] = state->config;
    LOAD_FIXED_STATE(nv3,PFIFO);
    LOAD_FIXED_STATE(nv3,PRAMIN);
    LOAD_FIXED_STATE(nv3,PGRAPH);
    switch (state->bpp)
    {
     case 15:
     case 16:
         LOAD_FIXED_STATE_15BPP(nv3,PRAMIN);
         LOAD_FIXED_STATE_15BPP(nv3,PGRAPH);
         break;
     case 24:
     case 32:
         LOAD_FIXED_STATE_32BPP(nv3,PRAMIN);
         LOAD_FIXED_STATE_32BPP(nv3,PGRAPH);
         break;
     case 8:
     default:
         LOAD_FIXED_STATE_8BPP(nv3,PRAMIN);
        LOAD_FIXED_STATE_8BPP(nv3,PGRAPH);
        break;
    }
    for (i = 0x00000; i < 0x00800; i++)
        chip->PRAMIN[0x00000502 + i] = (i << 12) | 0x03;
    chip->PGRAPH[0x00000630/4] = state->offset;
    chip->PGRAPH[0x00000634/4] = state->offset;
    chip->PGRAPH[0x00000638/4] = state->offset;
    chip->PGRAPH[0x0000063C/4] = state->offset;
    chip->PGRAPH[0x00000650/4] = state->pitch;
    chip->PGRAPH[0x00000654/4] = state->pitch;
    chip->PGRAPH[0x00000658/4] = state->pitch;
    chip->PGRAPH[0x0000065C/4] = state->pitch;

    LOAD_FIXED_STATE(Riva,FIFO);
    UpdateFifoState(chip);

    /*
     * Load HW mode state.
     */
    VGA_WR08(chip->PCIO, 0x03D4, 0x19);
    VGA_WR08(chip->PCIO, 0x03D5, state->repaint0);
    VGA_WR08(chip->PCIO, 0x03D4, 0x1A);
    VGA_WR08(chip->PCIO, 0x03D5, state->repaint1);
    VGA_WR08(chip->PCIO, 0x03D4, 0x25);
    VGA_WR08(chip->PCIO, 0x03D5, state->screen);
    VGA_WR08(chip->PCIO, 0x03D4, 0x28);
    VGA_WR08(chip->PCIO, 0x03D5, state->pixel);
    VGA_WR08(chip->PCIO, 0x03D4, 0x2D);
    VGA_WR08(chip->PCIO, 0x03D5, state->horiz);
    VGA_WR08(chip->PCIO, 0x03D4, 0x1B);
    VGA_WR08(chip->PCIO, 0x03D5, state->arbitration0);
    VGA_WR08(chip->PCIO, 0x03D4, 0x20);
    VGA_WR08(chip->PCIO, 0x03D5, state->arbitration1);
    VGA_WR08(chip->PCIO, 0x03D4, 0x30);
    VGA_WR08(chip->PCIO, 0x03D5, state->cursor0);
    VGA_WR08(chip->PCIO, 0x03D4, 0x31);
    VGA_WR08(chip->PCIO, 0x03D5, state->cursor1);
    VGA_WR08(chip->PCIO, 0x03D4, 0x2F);
    VGA_WR08(chip->PCIO, 0x03D5, state->cursor2);
    VGA_WR08(chip->PCIO, 0x03D4, 0x39);
    VGA_WR08(chip->PCIO, 0x03D5, state->interlace);

    chip->PRAMDAC[0x00000508/4] = state->vpll;
    chip->PRAMDAC[0x0000050C/4] = state->pllsel;
    chip->PRAMDAC[0x00000600/4]  = state->general;

    /*
     * Turn off VBlank enable and reset.
     */
    chip->PCRTC[0x00000140/4] = 0;
    chip->PCRTC[0x00000100/4] = chip->VBlankBit;
    /*
     * Set interrupt enable.
     */    
    chip->PMC[0x00000140/4]  = chip->EnableIRQ & 0x01;
    /*
     * Set current state pointer.
     */
    chip->CurrentState = state;
    /*
     * Reset FIFO free and empty counts.
     */
    chip->FifoFreeCount  = 0;
    /* Free count from first subchannel */
    chip->FifoEmptyCount = chip->Rop->FifoFree; 
}

static void UnloadStateExt
(
    RIVA_HW_INST  *chip,
    RIVA_HW_STATE *state
)
{
    /*
     * Save current HW state.
     */
    VGA_WR08(chip->PCIO, 0x03D4, 0x19);
    state->repaint0     = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x1A);
    state->repaint1     = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x25);
    state->screen       = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x28);
    state->pixel        = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x2D);
    state->horiz        = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x1B);
    state->arbitration0 = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x20);
    state->arbitration1 = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x30);
    state->cursor0      = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x31);
    state->cursor1      = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x2F);
    state->cursor2      = VGA_RD08(chip->PCIO, 0x03D5);
    VGA_WR08(chip->PCIO, 0x03D4, 0x39);
    state->interlace    = VGA_RD08(chip->PCIO, 0x03D5);
    state->vpll         = chip->PRAMDAC[0x00000508/4];
    state->pllsel       = chip->PRAMDAC[0x0000050C/4];
    state->general      = chip->PRAMDAC[0x00000600/4];
    state->config       = chip->PFB[0x00000200/4];
    state->offset       = chip->PGRAPH[0x00000630/4];
    state->pitch        = chip->PGRAPH[0x00000650/4];
}

static void SetStartAddress
(
    RIVA_HW_INST *chip,
    unsigned      start
)
{
    int offset = start >> 2;
    int pan    = (start & 3) << 1;
    unsigned char tmp;

    /*
     * Unlock extended registers.
     */
    chip->LockUnlock(chip, 0);
    /*
     * Set start address.
     */
    VGA_WR08(chip->PCIO, 0x3D4, 0x0D); VGA_WR08(chip->PCIO, 0x3D5, offset);
    offset >>= 8;
    VGA_WR08(chip->PCIO, 0x3D4, 0x0C); VGA_WR08(chip->PCIO, 0x3D5, offset);
    offset >>= 8;
    VGA_WR08(chip->PCIO, 0x3D4, 0x19); tmp = VGA_RD08(chip->PCIO, 0x3D5);
    VGA_WR08(chip->PCIO, 0x3D5, (offset & 0x01F) | (tmp & ~0x1F));
    VGA_WR08(chip->PCIO, 0x3D4, 0x2D); tmp = VGA_RD08(chip->PCIO, 0x3D5);
    VGA_WR08(chip->PCIO, 0x3D5, (offset & 0x60) | (tmp & ~0x60));
    /*
     * 4 pixel pan register.
     */
    offset = VGA_RD08(chip->PCIO, chip->IO + 0x0A);
    VGA_WR08(chip->PCIO, 0x3C0, 0x13);
    VGA_WR08(chip->PCIO, 0x3C0, pan);
}
/****************************************************************************\
*                                                                            *
*                      Probe RIVA Chip Configuration                         *
*                                                                            *
\****************************************************************************/

static void nv3GetConfig
(
    RIVA_HW_INST *chip
)
{
    /*
     * Fill in chip configuration.
     */
    if (chip->PFB[0x00000000/4] & 0x00000020)
    {
        if (((chip->PMC[0x00000000/4] & 0xF0) == 0x20)
         && ((chip->PMC[0x00000000/4] & 0x0F) >= 0x02))
        {        
            /*
             * SDRAM 128 ZX.
             */
            chip->RamBandwidthKBytesPerSec = 800000;
            switch (chip->PFB[0x00000000/4] & 0x03)
            {
                case 2:
                    chip->RamAmountKBytes = 1024 * 4;
                    break;
                case 1:
                    chip->RamAmountKBytes = 1024 * 2;
                    break;
                default:
                    chip->RamAmountKBytes = 1024 * 8;
                    break;
            }
        }            
        else            
        {
            chip->RamBandwidthKBytesPerSec = 1000000;
            chip->RamAmountKBytes          = 1024 * 8;
        }            
    }
    else
    {
        /*
         * SGRAM 128.
         */
        chip->RamBandwidthKBytesPerSec = 1000000;
        switch (chip->PFB[0x00000000/4] & 0x00000003)
        {
            case 0:
                chip->RamAmountKBytes = 1024 * 8;
                break;
            case 2:
                chip->RamAmountKBytes = 1024 * 4;
                break;
            default:
                chip->RamAmountKBytes = 1024 * 2;
                break;
        }
    }        
    chip->CrystalFreqKHz   = (chip->PEXTDEV[0x00000000/4] & 0x00000040) ? 14318 : 13500;
    chip->CURSOR           = &(chip->PRAMIN[0x00008000/4 - 0x0800/4]);
    chip->VBlankBit        = 0x00000100;
    chip->MaxVClockFreqKHz = 256000;
    /*
     * Set chip functions.
     */
    chip->Busy            = nv3Busy;
    chip->ShowHideCursor  = ShowHideCursor;
    chip->CalcStateExt    = CalcStateExt;
    chip->LoadStateExt    = LoadStateExt;
    chip->UnloadStateExt  = UnloadStateExt;
    chip->SetStartAddress = SetStartAddress;
    chip->LockUnlock      = nv3LockUnlock;
}
int RivaGetConfig
(
    RivaPtr pRiva
)
{
    RIVA_HW_INST *chip = &pRiva->riva;

    nv3GetConfig(chip);
    /*
     * Fill in FIFO pointers.
     */
    chip->Rop    = (RivaRop                 *)&(chip->FIFO[0x00000000/4]);
    chip->Clip   = (RivaClip                *)&(chip->FIFO[0x00002000/4]);
    chip->Patt   = (RivaPattern             *)&(chip->FIFO[0x00004000/4]);
    chip->Pixmap = (RivaPixmap              *)&(chip->FIFO[0x00006000/4]);
    chip->Blt    = (RivaScreenBlt           *)&(chip->FIFO[0x00008000/4]);
    chip->Bitmap = (RivaBitmap              *)&(chip->FIFO[0x0000A000/4]);
    chip->Line   = (RivaLine                *)&(chip->FIFO[0x0000C000/4]);
    return (0);
}

