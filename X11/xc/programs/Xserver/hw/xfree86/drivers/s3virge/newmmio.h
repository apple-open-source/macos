/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/newmmio.h,v 1.5 1999/03/21 07:35:15 dawes Exp $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

/***************************************************************************
 *
 * typedefs and macros for MMIO mode, S3 ViRGE
 *
 *	who	when	vers
 *	HK	9609126	0.0
 *
 * based on:
 *
 * typedefs and macros for old and new MMIO mode, Trio64V+ and 868/968
 *
 *	who	when	vers
 *	BL	0300296	0.1
 *      SM      200497  0.2   Added Caching version of register macros.
 *	KJB	9/98	0.3   Added S3V_MMIO_REGSIZE
 ***************************************************************************/

#ifndef _NEWMMIO_H
#define _NEWMMIO_H

/* base for S3_OUTW macro  */
#define S3_NEWMMIO_REGBASE	0x1000000  /* 16MB */
#define S3_NEWMMIO_REGSIZE	  0x10000  /* 64KB */
#define S3V_MMIO_REGSIZE	   0x8000  /* 32KB */


/* #include <Xmd.h> */


#define int16	CARD16
#define int32	CARD32

#define S3_NEWMMIO_VGABASE	(S3_NEWMMIO_REGBASE + 0x8000)

#if 0
typedef struct { int16 vendor_ID; int16 device_ID; } pci_id;
typedef struct { int16 cmd; int16 devsel; } cmd_devsel;

typedef struct  {
        pci_id  pci_ident;
        cmd_devsel cmd_device_sel;
        int32 class_code;
        char  dummy_0c;
	char  latnecy_timer;
	int16 dummy_0e;
        int32 base0;
        char  dummy_14[0x20-sizeof(int32)];
        int32 bios_base;
	int32 dummy_34;
	int32 dummy_38;
        char  int_line;
        char  int_pin;
        int16  latency_grant;
} pci_conf_regs;


typedef struct  {
        int32  prim_stream_cntl;
        int32  col_chroma_key_cntl;
        char    dummy1[0x8190 - 0x8184-sizeof(int32)];
        int32  second_stream_cntl;
        int32  chroma_key_upper_bound;
        int32  second_stream_stretch;
        char dummy2[0x81a0 - 0x8198-sizeof(int32)];
        int32  blend_cntl;
        char    dummy3[0x81c0 - 0x81a0-sizeof(int32)];
        int32  prim_fbaddr0;
        int32  prim_fbaddr1;
        int32  prim_stream_stride;
        int32  double_buffer;
        int32  second_fbaddr0;
        int32  second_fbaddr1;
        int32  second_stream_stride;
        int32  opaq_overlay_cntl;
        int32  k1;
        int32  k2;
        int32  dda_vert;
        int32  streams_fifo;
        int32  prim_start_coord;
        int32  prim_window_size;
        int32  second_start_coord;
        int32  second_window_size;
} streams_proc_regs;

typedef struct {
   int32  fifo_control;
   int32  miu_control;
   int32  streams_timeout;
   int32  misc_timeout;
   int32  dummy_8210, dummy_8214, dummy_8218, dummy_821c;
   int32  dma_read_base_addr;
   int32  dma_read_stride_width;
} memport_proc_regs;

typedef struct {
   int32  subsystem_csr;
   int32  dummy_8508;
   int32  adv_func_cntl;
} subsys_regs;

typedef struct {
   int32  start_sysmem_addr;
   int32  transfer_length;
   int32  transfer_enable;
} video_dma_regs;

typedef struct {
   int32  base_addr;
   int32  write_pointer;
   int32  read_pointer;
   int32  dma_enable;
} cmd_dma_regs;

typedef struct {
   video_dma_regs video;
   int32  dummy_858c;
   cmd_dma_regs cmd;
} dma_regs;

typedef struct {
	int32	lpb_mode;
	int32	lpb_fifostat;
	int32	lpb_intflags;
	int32	lpb_fb0addr;
	int32	lpb_fb1addr;
	int32	lpb_direct_addr;
	int32	lpb_direct_data;
	int32	lpb_gpio;
	int32	lpb_serial_port;
	int32	lpb_input_winsize;
	int32	lpb_data_offsets;
	int32	lpb_hor_decimctl;
	int32	lpb_vert_decimctl;
	int32	lpb_line_stride;
	int32	lpb_output_fifo;
} lpbus_regs;


typedef struct { char atr_cntl_ind; char attr_cntl_dat; char misc_out;
                 char viseo_enable; } v3c0;
typedef struct { char seq_index; char seq_data; char dac_mask;
                      char dac_rd_index; } v3c4;
typedef struct { char dac_wr_index; char dac_data; char feature_cntl;
                      char filler; } v3c8;
typedef struct v3cc { char misc_out; char filler; char graph_cntl_index;
                      char graph_cntl_data; } v3cc;
typedef struct {
        v3c0    v3c0_regs;
        v3c4    v3c4_regs;
        v3c8    v3c8_regs;
        v3cc    v3cc_regs;
} vga_3c_regs;

typedef struct { char crt_index; char crt_data; int16 filler; } v3d4;
typedef struct { int16 filler1; char feature_cntl; char filler2;} v3d8;

typedef struct  {
        int32   filler;
        v3d4    v3d4_regs;
        v3d8    v3d8_regs;
} vga_3bd_regs ;



typedef struct  {
   char   filler1[-(0xa000-0xa100)];
   int32  patt[-(0xa100-0xa1bc) / sizeof(int32) + 1];
} colpatt_regs;

typedef struct  {
   char   filler1[-(0xa400-0xa4d4)];
   int32  src_base;
   int32  dest_base;
   int32  clip_l_r;
   int32  clip_t_b;
   int32  dest_src_str;
   int32  mono_pat0;
   int32  mono_pat1;
   int32  pat_bg_clr;
   int32  pat_fg_clr;
   int32  src_bg_clr;
   int32  src_fg_clr;
   int32  cmd_set;
   int32  rwidth_height;
   int32  rsrc_xy;
   int32  rdest_xy;
} bltfill_regs;

typedef struct  {
   char   filler1[-(0xa800-0xa8d4)];
   int32  src_base;
   int32  dest_base;
   int32  clip_l_r;
   int32  clip_t_b;
   int32  dest_src_str;
   int32  dummy1;
   int32  dummy2;
   int32  dummy3;
   int32  pat_fg_clr;
   int32  dummy4;
   int32  dummy5;
   int32  cmd_set;
   char   filler2[-(0xa904-0xa96c)];
   int32  lxend0_end1;
   int32  ldx;
   int32  lxstart;
   int32  lystart;
   int32  lycnt;
} line_regs;

typedef struct  {
   char   filler1[-(0xac00-0xacd4)];
   int32  src_base;
   int32  dest_base;
   int32  clip_l_r;
   int32  clip_t_b;
   int32  dest_src_str;
   int32  mono_pat0;
   int32  mono_pat1;
   int32  pat_bg_clr;
   int32  pat_fg_clr;
   int32  dummy1;
   int32  dummy2;
   int32  cmd_set;
   char   filler2[-(0xad04-0xad68)];
   int32  prdx;
   int32  prxstart;
   int32  pldx;
   int32  plxstart;
   int32  pystart;
   int32  pycnt;
} polyfill_regs;

typedef struct  {
   char   filler1[-(0xb000-0xb0d4)];
   int32  z_base;
   int32  dest_base;
   int32  clip_l_r;
   int32  clip_t_b;
   int32  dest_src_str;
   int32  z_stride;
   int32  dummy1;
   int32  dummy2;
   int32  fog_clr;
   int32  dummy3;
   int32  dummy4;
   int32  cmd_set;
   char   filler2[-(0xb104-0xb144)];
   int32  dgdy_dbdy;
   int32  dady_drdy;
   int32  gs_bs;
   int32  as_rs;
   int32  dummy5;
   int32  dz;
   int32  zstart;
   int32  dummy6;
   int32  dummy7;
   int32  dummy8;
   int32  xend0_end1;
   int32  dx;
   int32  xstart;
   int32  ystart;
   int32  ycnt;
} line3d_regs;

typedef struct  {
   char   filler1[-(0xb400-0xb4d4)];
   int32  z_base;
   int32  dest_base;
   int32  clip_l_r;
   int32  clip_t_b;
   int32  dest_src_str;
   int32  z_stride;
   int32  tex_base;
   int32  tex_bdr_clr;
   int32  fog_clr;
   int32  color0;
   int32  color1;
   int32  cmd_set;
   int32  bv;
   int32  bu;
   int32  dwdx;
   int32  dwdy;
   int32  ws;
   int32  dddx;
   int32  dvdx;
   int32  dudx;
   int32  dddy;
   int32  dvdy;
   int32  dudy;
   int32  ds;
   int32  vs;
   int32  us;
   int32  dgdx_dbdx;
   int32  dadx_drdx;
   int32  dgdy_dbdy;
   int32  dady_drdy;
   int32  gs_bs;
   int32  as_rs;
   int32  dzdx;
   int32  dzdy;
   int32  zs;
   int32  dxdy12;
   int32  xend12;
   int32  dxdy01;
   int32  xend01;
   int32  dxdy02;
   int32  xstart02;
   int32  ystart;
   int32  y01_y12;
} triangle3d_regs;


typedef struct {
        int32 img[0x8000/4];
        union { pci_conf_regs regs;
                char dummy[-(0x8000 - 0x8180)];
        } pci_regs;
        union { streams_proc_regs regs;
                char dummy[-(0x8180 - 0x8200)];
        } streams_regs;
        union { memport_proc_regs regs;
                char dummy[-(0x8200 - 0x83b0)];
        } memport_regs;
        union { vga_3bd_regs    regs;
                char dummy[-(0x83b0 - 0x83c0)];
        } v3b_regs;
        union { vga_3c_regs     regs;
                char dummy[-(0x83c0 - 0x83d0)];
        } v3c_regs;
        union { vga_3bd_regs    regs;
                char dummy[-(0x83d0 - 0x8504)];
        } v3d_regs;
        union { subsys_regs     regs;
                char dummy[-(0x8504 - 0x8580)];
        } subsys_regs;
        union { dma_regs     regs;
                char dummy[-(0x8580 - 0xa000)];
        } dma_regs;
        union { colpatt_regs     regs;
                char dummy[-(0xa000 - 0xa400)];
        } colpatt_regs;
        union { bltfill_regs     regs;
                char dummy[-(0xa400 - 0xa800)];
        } bltfill_regs;
        union { line_regs     regs;
                char dummy[-(0xa800 - 0xac00)];
        } line_regs;
        union { polyfill_regs   regs;
                char dummy[-(0xac00 - 0xb000)];
        } polyfill_regs;
        union { line3d_regs   regs;
                char dummy[-(0xb000 - 0xb400)];
        } line3d_regs;
        union { triangle3d_regs   regs;
                char dummy[-(0xb400 - 0xff00)];
        } triangle3d_regs;
        union { lpbus_regs   regs;
                char dummy[-(0xff00 - 0xff5c)];
        } lbp_regs;
} mm_virge_regs ;



#define mmtr	volatile mm_virge_regs *

#define SET_WRT_MASK(msk)	/*  */
#define SET_RD_MASK(msk)	/*  */
#define SET_FRGD_COLOR(col)	/*  */
#define SET_BKGD_COLOR(col)	/*  */
#define SET_FRGD_MIX(fmix)	/*  */
#define SET_BKGD_MIX(bmix)	/*  */
#define SET_PIX_CNTL(val)	/*  */
#define SET_MIN_AXIS_PCNT(min)	/*  */
#define SET_MAJ_AXIS_PCNT(maj)	/*  */
#define SET_CURPT(c_x, c_y)	/*  */
#define SET_CUR_X(c_x)		/*  */
#define SET_CUR_Y(c_y)		/*  */
#define SET_DESTSTP(x,y)	/*  */
#define SET_AXIS_PCNT(maj, min)	/*  */
#define SET_CMD(c_d) 		/*  */
#define SET_ERR_TERM(e)		/*  */
#define SET_SCISSORS(x1,y1,x2,y2) /*  */
#define SET_SCISSORS_RB(x,y)	/*  */
#define SET_MULT_MISC(val)	/*  */

#define SET_PIX_TRANS_W(val)	/*  */
#define SET_PIX_TRANS_L(val)	/*  */
#define SET_MIX(fmix,bmix) 	/*  */

#endif /* 0 */

/*
 * reads from SUBSYS_STAT
 */
#define IN_SUBSYS_STAT() (INREG(SUBSYS_STAT_REG))
#define SET_SUBSYS_CRTL(val) do { write_mem_barrier();\
OUTREG((val), SUBSYS_STAT_REG);\
write_mem_barrier(); } while (0)


#if 0
#define SET_DAC_W_INDEX(index)  OUTREG8(DAC_W_INDEX, index)
#define SET_DAC_DATA(val) 	OUTREG8(DAC_DATA,val)
#endif

#if 0

#define IMG_TRANS		(((mmtr)s3vMmioMem)->img)
#define SET_PIXTRANS(a,v)	IMG_TRANS[a] = (v)
#define COLOR_PATTERN           (((mmtr)s3vMmioMem)->colpatt_regs.regs)

#define CMD_DMA_BASE(val)       (((mmtr)s3vMmioMem)->dma_regs.regs.cmd.base_addr) = (val)
#define CMD_DMA_WRITEP(val)     (((mmtr)s3vMmioMem)->dma_regs.regs.cmd.write_pointer) = (val)
#define CMD_DMA_READP(val)      (((mmtr)s3vMmioMem)->dma_regs.regs.cmd.read_pointer) = (val)
#define CMD_DMA_ENABLE(val)     (((mmtr)s3vMmioMem)->dma_regs.regs.cmd.dma_enable) = (val)

#define SETB_SRC_BASE(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.src_base = (val)
#define SETB_DEST_BASE(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.dest_base = (val)
#define SETB_CLIP_L_R(l,r)	((mmtr)s3vMmioMem)->bltfill_regs.regs.clip_l_r = ((l)<<16 | (r))
#define SETB_CLIP_T_B(t,b)	((mmtr)s3vMmioMem)->bltfill_regs.regs.clip_t_b = ((t)<<16 | (b))
/*  #define SETB_DEST_SRC_STR(d,s)	((mmtr)s3vMmioMem)->bltfill_regs.regs.dest_src_str = ((d)<<16 | (s)) */

#define SETB_DEST_SRC_STR(d, s) (OUTREG(DEST_SRC_STR, ((d) << 16 | (s))))

#define SETB_MONO_PAT0(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.mono_pat0 = (val)
#define SETB_MONO_PAT1(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.mono_pat1 = (val)
#define SETB_PAT_BG_CLR(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.pat_bg_clr = (val)
#define SETB_PAT_FG_CLR(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.pat_fg_clr = (val)
#define SETB_SRC_BG_CLR(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.src_bg_clr = (val)
#define SETB_SRC_FG_CLR(val)	((mmtr)s3vMmioMem)->bltfill_regs.regs.src_fg_clr = (val)
#define SETB_CMD_SET(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->bltfill_regs.regs.cmd_set = (val); write_mem_barrier(); } while (0)
#define SETB_RWIDTH_HEIGHT(w,h)	((mmtr)s3vMmioMem)->bltfill_regs.regs.rwidth_height = ((w)<<16 | (h))
#define SETB_RSRC_XY(x,y)	((mmtr)s3vMmioMem)->bltfill_regs.regs.rsrc_xy = ((x)<<16 | (y))
#define SETB_RDEST_XY(x,y)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->bltfill_regs.regs.rdest_xy = ((x)<<16 | (y)); write_mem_barrier(); } while (0)

/* Caching version of the same MACROs */

#define CACHE_SETB_CLIP_L_R(l,r)	do { unsigned int clip = ((l)<<16 | (r)); if (s3vCached_CLIP_LR != clip) { ((mmtr)s3vMmioMem)->bltfill_regs.regs.clip_l_r = clip; s3vCached_CLIP_LR = clip; s3vCacheMiss++;} else s3vCacheHit++;} while(0)
#define CACHE_SETB_CLIP_T_B(t,b)	do { unsigned int clip = ((t)<<16 | (b)); if (s3vCached_CLIP_TB != clip) { ((mmtr)s3vMmioMem)->bltfill_regs.regs.clip_t_b = clip; s3vCached_CLIP_TB = clip; s3vCacheMiss++;} else s3vCacheHit++;} while(0)
#define CACHE_SETB_RSRC_XY(x,y)		do { unsigned int src = ((x)<<16 | (y)); if (s3vCached_RSRC_XY != src) { ((mmtr)s3vMmioMem)->bltfill_regs.regs.rsrc_xy = src; s3vCached_RSRC_XY = src; s3vCacheMiss++;} else s3vCacheHit++;} while(0)
#define CACHE_SETB_RWIDTH_HEIGHT(w,h)	do { unsigned int rwh = ((w)<<16 | (h)); if (s3vCached_RWIDTH_HEIGHT != rwh) { ((mmtr)s3vMmioMem)->bltfill_regs.regs.rwidth_height = rwh; s3vCached_RWIDTH_HEIGHT = rwh; s3vCacheMiss++;} else s3vCacheHit++;} while(0)
#define CACHE_SETB_MONO_PAT0(val) do { \
	if (s3vCached_MONO_PATTERN0 != (val)) { \
		((mmtr)s3vMmioMem)->bltfill_regs.regs.mono_pat0 = (val); \
		s3vCached_MONO_PATTERN0 = (val); \
		s3vCacheMiss++; \
	} else s3vCacheHit++; \
} while(0)
#define CACHE_SETB_MONO_PAT1(val) do { \
	if (s3vCached_MONO_PATTERN1 != (val)) { \
		((mmtr)s3vMmioMem)->bltfill_regs.regs.mono_pat1 = (val); \
		s3vCached_MONO_PATTERN1 = (val); \
		s3vCacheMiss++; \
	} else s3vCacheHit++;\
} while(0)
#define CACHE_SETB_PAT_FG_CLR(val) do { \
	if (s3vCached_PAT_FGCLR != (val)) { \
		((mmtr)s3vMmioMem)->bltfill_regs.regs.pat_fg_clr = (val); \
		s3vCached_PAT_FGCLR = (val); \
		s3vCacheMiss++; \
	} else s3vCacheHit++; \
} while(0)
#define CACHE_SETB_PAT_BG_CLR(val) do { \
	if (s3vCached_PAT_BGCLR != (val)) { \
		((mmtr)s3vMmioMem)->bltfill_regs.regs.pat_bg_clr = (val); \
		s3vCached_PAT_BGCLR = (val); \
		s3vCacheMiss++; \
	} else s3vCacheHit++; \
} while(0)
#define CACHE_SETB_CMD_SET(val) do { \
	if (s3vCached_CMD_SET != (val)) { \
		write_mem_barrier(); \
		((mmtr)s3vMmioMem)->bltfill_regs.regs.cmd_set = (val); \
		s3vCached_CMD_SET = (val); \
		s3vCacheMiss++; \
		write_mem_barrier(); \
	} else s3vCacheHit++; \
} while(0)
#define SETL_SRC_BASE(val)	((mmtr)s3vMmioMem)->line_regs.regs.src_base = (val)
#define SETL_DEST_BASE(val)	((mmtr)s3vMmioMem)->line_regs.regs.dest_base = (val)
#define SETL_CLIP_L_R(l,r)	((mmtr)s3vMmioMem)->line_regs.regs.clip_l_r = ((l)<<16 | (r))
#define SETL_CLIP_T_B(t,b)	((mmtr)s3vMmioMem)->line_regs.regs.clip_t_b = ((t)<<16 | (b))
#define SETL_DEST_SRC_STR(d,s)	((mmtr)s3vMmioMem)->line_regs.regs.dest_src_str = ((d)<<16 | (s))
#define SETL_PAT_FG_CLR(val)	((mmtr)s3vMmioMem)->line_regs.regs.pat_fg_clr = (val)
#define SETL_CMD_SET(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->line_regs.regs.cmd_set = (val); write_mem_barrier(); } while (0)
#define SETL_LXEND0_END1(e0,e1)	((mmtr)s3vMmioMem)->line_regs.regs.lxend0_end1 = ((e0)<<16 | (e1))
#define SETL_LDX(val)	((mmtr)s3vMmioMem)->line_regs.regs.ldx = (val)
#define SETL_LXSTART(val)	((mmtr)s3vMmioMem)->line_regs.regs.lxstart = (val)
#define SETL_LYSTART(val)	((mmtr)s3vMmioMem)->line_regs.regs.lystart = (val)
#define SETL_LYCNT(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->line_regs.regs.lycnt = (val); write_mem_barrier(); } while (0)

/* Cache version */
#define CACHE_SETL_CMD_SET(val)		do { if (s3vCached_CMD_SET != val) { write_mem_barrier(); ((mmtr)s3vMmioMem)->line_regs.regs.cmd_set = val; s3vCached_CMD_SET = val; s3vCacheMiss++; write_mem_barrier(); } else s3vCacheHit++;} while(0)



#define SETP_SRC_BASE(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.src_base = (val)
#define SETP_DEST_BASE(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.dest_base = (val)
#define SETP_CLIP_L_R(l,r)	((mmtr)s3vMmioMem)->polyfill_regs.regs.clip_l_r = ((l)<<16 | (r))
#define SETP_CLIP_T_B(t,b)	((mmtr)s3vMmioMem)->polyfill_regs.regs.clip_t_b = ((t)<<16 | (b))
#define SETP_DEST_SRC_STR(d,s)	((mmtr)s3vMmioMem)->polyfill_regs.regs.dest_src_str = ((d)<<16 | (s))
#define SETP_MONO_PAT0(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.mono_pat0 = (val)
#define SETP_MONO_PAT1(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.mono_pat1 = (val)
#define SETP_PAT_BG_CLR(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.pat_bg_clr = (val)
#define SETP_PAT_FG_CLR(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.pat_fg_clr = (val)
#define SETP_CMD_SET(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->polyfill_regs.regs.cmd_set = (val); write_mem_barrier(); } while (0)
#define SETP_RWIDTH_HEIGHT(w,h)	((mmtr)s3vMmioMem)->polyfill_regs.regs.rwidth_height = ((w)<<16 | (h))
#define SETP_PRDX(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.prdx = (val)
#define SETP_PRXSTART(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.prxstart = (val)
#define SETP_PLDX(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.pldx = (val)
#define SETP_PLXSTART(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.plxstart = (val)
#define SETP_PYSTART(val)	((mmtr)s3vMmioMem)->polyfill_regs.regs.pystart = (val)
#define SETP_PYCNT(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->polyfill_regs.regs.pycnt = (val); write_mem_barrier(); } while (0)

/* Cache version */
#define CACHE_SETP_CMD_SET(val)		do { if (s3vCached_CMD_SET != val) { write_mem_barrier(); ((mmtr)s3vMmioMem)->polyfill_regs.regs.cmd_set = val; s3vCached_CMD_SET = val; s3vCacheMiss++; write_mem_barrier(); } else s3vCacheHit++;} while(0)


#define SETL3_Z_BASE(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.z_base = (val)
#define SETL3_DEST_BASE(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.dest_base = (val)
#define SETL3_CLIP_L_R(l,r)	((mmtr)s3vMmioMem)->line3d_regs.regs.clip_l_r = ((l)<<16 | (r))
#define SETL3_CLIP_T_B(t,b)	((mmtr)s3vMmioMem)->line3d_regs.regs.clip_t_b = ((t)<<16 | (b))
#define SETL3_DEST_SRC_STR(d,s)	((mmtr)s3vMmioMem)->line3d_regs.regs.dest_src_str = ((d)<<16 | (s))
#define SETL3_Z_STRIDE(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.z_stride = (val)
#define SETL3_FOG_CLR(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.fog_clr = (val)
#define SETL3_CMD_SET(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->line3d_regs.regs.cmd_set = (val); write_mem_barrier(); } while (0)
#define SETL3_DGDY_DBDY(dg,db)	((mmtr)s3vMmioMem)->line3d_regs.regs.dgdy_dbdy = ((dg)<<16 | (db))
#define SETL3_DADY_DRDY(da,dr)	((mmtr)s3vMmioMem)->line3d_regs.regs.dady_drdy = ((da)<<16 | (dr))
#define SETL3_GS_BS(gs,bs)	((mmtr)s3vMmioMem)->line3d_regs.regs.gs_bs = ((gs)<<16 | (bs))
#define SETL3_AS_RS(as,rs)	((mmtr)s3vMmioMem)->line3d_regs.regs.as_rs = ((as)<<16 | (rs))
#define SETL3_DZ(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.dz = (val)
#define SETL3_ZSTART(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.zstart = (val)
#define SETL3_XEND0_END1(e0,e1)	((mmtr)s3vMmioMem)->line3d_regs.regs.xend0_end1 = ((e0)<<16 | (e1))
#define SETL3_DX(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.dx = (val)
#define SETL3_XSTART(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.xstart = (val)
#define SETL3_YSTART(val)	((mmtr)s3vMmioMem)->line3d_regs.regs.ystart = (val)
#define SETL3_YCNT(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->line3d_regs.regs.ycnt = (val); write_mem_barrier(); } while (0)



#define SETT3_Z_BASE(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.z_base = (val)
#define SETT3_DEST_BASE(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dest_base = (val)
#define SETT3_CLIP_L_R(l,r)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.clip_l_r = ((l)<<16 | (r))
#define SETT3_CLIP_T_B(t,b)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.clip_t_b = ((t)<<16 | (b))
#define SETT3_DEST_SRC_STR(d,s)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dest_src_str = ((d)<<16 | (s))
#define SETT3_Z_STRIDE(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.z_stride = (val)
#define SETT3_TEX_BASE(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.tex_base = (val)
#define SETT3_TEX_BDR_CLR(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.tex_bdr_clr = (val)
#define SETT3_FOG_CLR(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.fog_clr = (val)
#define SETT3_COLOR0(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.color0 = (val)
#define SETT3_COLOR1(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.color1 = (val)
#define SETT3_CMD_SET(val)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->triangle3d_regs.regs.cmd_set = (val); write_mem_barrier(); } while (0)
#define SETT3_BV(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.bv = (val)
#define SETT3_BU(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.bu = (val)
#define SETT3_DWDX(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dwdx = (val)
#define SETT3_DWDY(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dwdy = (val)
#define SETT3_WS(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.ws = (val)
#define SETT3_DDDX(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dddx = (val)
#define SETT3_DVDX(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dvdx = (val)
#define SETT3_DUDX(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dudx = (val)
#define SETT3_DDDY(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dddy = (val)
#define SETT3_DVDY(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dvdy = (val)
#define SETT3_DUDY(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dudy = (val)
#define SETT3_DS(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.ds = (val)
#define SETT3_VS(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.vs = (val)
#define SETT3_US(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.us = (val)
#define SETT3_DGDX_DBDX(gx,bx)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dgdx_dbdx = ((gx)<<16 | (bx))
#define SETT3_DADX_DRDX(ax,rx)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dadx_drdx = ((ax)<<16 | (rx))
#define SETT3_DGDY_DBDY(gy,by)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dgdy_dbdy = ((gy)<<16 | (by))
#define SETT3_DADY_DRDY(ay,ry)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dady_drdy = ((ay)<<16 | (ry))
#define SETT3_GS_BS(gs,bs)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.gs_bs = ((gs)<<16 | (bs))
#define SETT3_AS_RS(as,rs)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.as_rs = ((as)<<16 | (rs))
#define SETT3_DZDX(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dzdx = (val)
#define SETT3_DZDY(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dzdy = (val)
#define SETT3_ZS(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.zs = (val)
#define SETT3_DXDY12(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dxdy12 = (val)
#define SETT3_XEND12(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.xend12 = (val)
#define SETT3_DXDY01(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dxdy01 = (val)
#define SETT3_XEND01(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.xend01 = (val)
#define SETT3_DXDY02(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.dxdy02 = (val)
#define SETT3_XSTART02(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.xstart02 = (val)
#define SETT3_YSTART(val)	((mmtr)s3vMmioMem)->triangle3d_regs.regs.ystart = (val)
#define SETT3_Y01_Y12(y01,y12)	do { write_mem_barrier(); ((mmtr)s3vMmioMem)->triangle3d_regs.regs.y01_y12 = ((y01)<<16 | (y12)); write_mem_barrier(); } while (0)



#define DBGOUT(p) /* OUTREG8(0x3bc,p) */

#endif /* 0 */

#endif /* _NEWMMIO_H */
