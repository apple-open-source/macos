/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_vbe.h,v 1.3 2002/05/14 20:19:52 alanh Exp $ */

/*
This file was originally part of the Linux Real-Mode Interface, or LRMI.
There is nothing LRMI-specific here; this is all good, general VBE info.

Copyright (C) 1996 by Josh Vanderhoof

You are free to distribute and modify this file, as long as you
do not remove this copyright notice and clearly label modified
versions as being modified.

This software has NO WARRANTY.  Use it at your own risk.
*/

#ifndef _SAVAGEVBE_H
#define _SAVAGEVBE_H

/* structures for vbe 2.0 */

#ifndef __GNUC__
#define __attribute__(a)
#endif

struct vbe_info_block
	{
	char vbe_signature[4];
	short vbe_version;
	unsigned short oem_string_off;
	unsigned short oem_string_seg;
	int capabilities;
	unsigned short video_mode_list_off;
	unsigned short video_mode_list_seg;
	short total_memory;
	short oem_software_rev;
	unsigned short oem_vendor_name_off;
	unsigned short oem_vendor_name_seg;
	unsigned short oem_product_name_off;
	unsigned short oem_product_name_seg;
	unsigned short oem_product_rev_off;
	unsigned short oem_product_rev_seg;
	char reserved[222];
	char oem_data[256];
	} __attribute__ ((packed));

#define VBE_ATTR_MODE_SUPPORTED 	(1 << 0)
#define VBE_ATTR_TTY 	(1 << 2)
#define VBE_ATTR_COLOR 	(1 << 3)
#define VBE_ATTR_GRAPHICS 	(1 << 4)
#define VBE_ATTR_NOT_VGA 	(1 << 5)
#define VBE_ATTR_NOT_WINDOWED 	(1 << 6)
#define VBE_ATTR_LINEAR 	(1 << 7)

#define VBE_WIN_RELOCATABLE 	(1 << 0)
#define VBE_WIN_READABLE 	(1 << 1)
#define VBE_WIN_WRITEABLE 	(1 << 2)

#define VBE_MODEL_TEXT 	0
#define VBE_MODEL_CGA 	1
#define VBE_MODEL_HERCULES 	2
#define VBE_MODEL_PLANAR 	3
#define VBE_MODEL_PACKED 	4
#define VBE_MODEL_256 	5
#define VBE_MODEL_RGB 	6
#define VBE_MODEL_YUV 	7

struct vbe_mode_info_block
	{
	unsigned short mode_attributes;
	unsigned char win_a_attributes;
	unsigned char win_b_attributes;
	unsigned short win_granularity;
	unsigned short win_size;
	unsigned short win_a_segment;
	unsigned short win_b_segment;
	unsigned short win_func_ptr_off;
	unsigned short win_func_ptr_seg;
	unsigned short bytes_per_scanline;
	unsigned short x_resolution;
	unsigned short y_resolution;
	unsigned char x_char_size;
	unsigned char y_char_size;
	unsigned char number_of_planes;
	unsigned char bits_per_pixel;
	unsigned char number_of_banks;
	unsigned char memory_model;
	unsigned char bank_size;
	unsigned char number_of_image_pages;
	unsigned char res1;
	unsigned char red_mask_size;
	unsigned char red_field_position;
	unsigned char green_mask_size;
	unsigned char green_field_position;
	unsigned char blue_mask_size;
	unsigned char blue_field_position;
	unsigned char rsvd_mask_size;
	unsigned char rsvd_field_position;
	unsigned char direct_color_mode_info;
	unsigned int phys_base_ptr;
	unsigned int offscreen_mem_offset;
	unsigned short offscreen_mem_size;
	unsigned char res2[206];
	} __attribute__ ((packed));

struct vbe_palette_entry
	{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
	unsigned char align;
	} __attribute__ ((packed));

#endif
