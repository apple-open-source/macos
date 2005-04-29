/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/platform.c,v 1.4 2003/11/03 05:11:24 tsi Exp $ */
/*
 * $Workfile: platform.c $
 * $Revision: 1.1.1.2 $
 *
 * File Contents: This file contains platform dependent functions
 *                which provide interface to that platform.
 *
 *
 * SubModule:     Geode FlatPanel library
 *
 */

/* 
 * NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * Panel Library
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * Panel Library
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * Panel Library
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */

#define LINUX_ROM_SEGMENT 0x000F
#define SEGMENT_LENGTH  0xFFFF
#define PAGE_LENGTH     0x1000
#define SYS_BOARD_NAME_LEN 24

#define PLT_READ         0
#define PLT_WRITE        1
#define PLT_WRITE_BYTES  2
#define PLT_READ_BYTES   3
#define PLT_WRITE_WORDS  4
#define PLT_READ_WORDS   5
#define PLT_WRITE_DWORDS 6
#define PLT_READ_DWORDS  7
#define PLT_UNKNOWN	 ((SYS_BOARD) 0xFFFF)

typedef struct
{
   char sys_board_name[SYS_BOARD_NAME_LEN];
   SYS_BOARD sys_board;
}
SYS_BOARD_INFO;

static SYS_BOARD_INFO Sys_info;

/*
 * The names in the sys_board_name string must exactly match the names in the
 * BIOS header. These names are used by FindStringInSeg() to find the names 
 * in the BIOS header space. The BIOS does not use OTHER; it is a dummy value 
 * for program useonly. 
 *
 */

SYS_BOARD_INFO Sys_board_info_array[] = {
   {"Marmot", MARMOT_PLATFORM},
   {"Unicorn", UNICORN_PLATFORM},
   {"Centaurus", CENTAURUS_PLATFORM},
   {"Aries", ARIES_PLATFORM},
   {"Carmel", CARMEL_PLATFORM},
   {"Hyrda", HYDRA_PLATFORM},
   {"Dorado", DORADO_PLATFORM},
   {"Redcloud", REDCLOUD_PLATFORM},
   {"Other", OTHER_PLATFORM}
};

#define NUM_SYS_BOARD_TYPES sizeof(Sys_board_info_array)/sizeof(SYS_BOARD_INFO)

static int Num_sys_board_type = NUM_SYS_BOARD_TYPES;
SYS_BOARD_INFO *Sys_board_array_base = Sys_board_info_array;
int FindStringInSeg(unsigned int, char *);
static unsigned char get_sys_board_type(SYS_BOARD_INFO *, SYS_BOARD_INFO *);

#if defined(linux) && !defined(__KERNEL__)
#if !defined(XFree86Server)
static void protected_mode_access(unsigned int, unsigned int,
				  unsigned long, unsigned char *);
static void setup_pma();
static void close_pma();
static int fd;
#endif /* IN_MODULE */
#endif /* __KERNEL__ */

/* Detect the Platform */
int
Detect_Platform(void)
{
#if defined(linux) && !defined(__KERNEL__)
#if !defined(XFree86Server)
   setup_pma();
#endif /* IN_MODULE */
#endif /* __KERNEL__ */

   /* See if we can find the board name using Xpressrom */
   if (get_sys_board_type(&Sys_info, Sys_board_array_base) == TRUE) {
#if 0
      if (Sys_info.sys_board == CENTAURUS_PLATFORM) {
	 printk("CENTAURUS Platform Found\n");
      } else if (Sys_info.sys_board == DORADO_PLATFORM) {
	 printk("DORADO Platform Found \n");
      } else {
	 printk("UNKNOWN Platform Found \n");
      }
#endif
   }
#if defined(linux) && !defined(__KERNEL__)
#if !defined(XFree86Server)
   close_pma();
#endif /* IN_MODULE */
#endif /* __KERNEL__ */

   return (Sys_info.sys_board);
}

static int
Strncmp(char *str1, char *str2, int len)
{
   int i;

   if ((str1 == 0x0) || (str2 == 0x0) || (len == 0))
      return (1);
   for (i = 0; i < len; i++) {
      if (*(str1 + i) > *(str2 + i)) {
	 return 1;
      } else if (*(str1 + i) < *(str2 + i)) {
	 return -1;
      }
   }
   return 0;
}

static char *
Strcpy(char *dst, char *src)
{
   int i;

   if ((dst == 0x0) || (src == 0x0))
      return (0);
   for (i = 0; src[i] != 0x0; i++) {
      dst[i] = src[i];
   }
   dst[i] = 0x0;			/* NULL termination */
   return dst;
}

static int
Strlen(char *str)
{
   int i;

   if (str == 0x0)
      return 0;
   for (i = 0; str[i] != 0x0; i++) ;
   return i;
}

/***********************************************************************/

/* Platform Detection Code */

/***********************************************************************/

/************************************************************************
 * int FindStringInSeg( unsigned int segment_address, char *string_ptr )
 * Returns the offset where the NULL terminated string pointed to by 
 * string_ptr is located in the segment passed in segment_address.
 * Segment_address must be of the form 0xXXXX (i.e 0xf000 for segment f).
 * Returns NULL if the string is not found.
 ************************************************************************
 */
int
FindStringInSeg(unsigned int segment_address, char *string_ptr)
{
   int string_length = Strlen(string_ptr);
   char *psegment_buf;
   unsigned long mem_ptr = (unsigned long)segment_address << 16;
   char segment_buffer[SEGMENT_LENGTH + 1];
   int i, cursor = 0;

   /* silence compiler */
   (void)cursor;
   (void)mem_ptr;
   (void)segment_buffer;

#if defined(linux) && !defined(XFree86Server)
#ifdef __KERNEL__
   XpressROMPtr = (unsigned char *)ioremap(mem_ptr, SEGMENT_LENGTH + 1);
   psegment_buf = (char *)XpressROMPtr;
#else
   /* Fill the segment_buffer with 16 page accesses */

   for (cursor = 0; (cursor * PAGE_LENGTH) < SEGMENT_LENGTH; cursor++) {
      protected_mode_access(PLT_READ_BYTES, PAGE_LENGTH, mem_ptr +
			    (cursor * PAGE_LENGTH),
			    &(segment_buffer[(cursor * PAGE_LENGTH)]));
   }
   psegment_buf = segment_buffer;
#endif /* __KERNEL__ */
#elif defined (XFree86Server)
   psegment_buf = (char *)XpressROMPtr;
#elif defined(_WIN32)			/* Windows */
   psegment_buf = XpressROMPtr;
#endif

   /* Now search for the first character of the string_ptr */
   for (i = 0; i < SEGMENT_LENGTH + 1; i++) {
      if (*(psegment_buf + i) == *string_ptr) {

	 /* If we match the first character, do a
	  * string compare.
	  */

	 if (!Strncmp(string_ptr, (psegment_buf + i), string_length)) {
	    /* They match! */
	    return (1);
	 }
      }
   }
   /* if we got this far we didn't find anything.  Return NULL. */
   return (0);

}					/* end FindStringInSeg() */

/**********************************************************************

 * TRUE_FALSE get_sys_board_type( SYS_BOARD_INFO *sys_info,
 * SYS_BOARD_INFO *sys_board_array_base)  Checks the system
 * BIOS area for Xpressrom information. If found, searches the  BIOS 
 * area for one of names in the array pointed to by sys_board_array_ptr.
 * If a match is found, sets the SYS_INFO system_board_name string 
 * and the system_board variable to the board name and returns TRUE. 
 * If Xpressrom or a board is not found, sets the variables to 
 * their default values and returns FALSE.
 * Uses the global string pointer *xpress_rom_string_ptr.
 *
 ***********************************************************************
 */

static unsigned char
get_sys_board_type(SYS_BOARD_INFO * sys_info,
		   SYS_BOARD_INFO * sys_board_array_base)
{
   int index;
   char *xpress_rom_string_ptr = "XpressStart";
   unsigned int segment = LINUX_ROM_SEGMENT;

   /* See if XpressStart is present in the BIOS area. 
    * If it is, search for a board string.  If not, Xpressrom is 
    * not present, set system_board information to UNKNOWN and 
    * return FALSE. 
    */

   if (!FindStringInSeg(segment, xpress_rom_string_ptr)) {
      sys_info->sys_board = PLT_UNKNOWN;
      Strcpy(sys_info->sys_board_name, "Unknown");
      return (FALSE);
   } else {

      /* we have Xpressrom, so look for a board */
      for (index = 0; index < Num_sys_board_type; index++) {
	 if (!FindStringInSeg(segment, (sys_board_array_base +
					index)->sys_board_name)) {
	    continue;
	 } else {

	    /* a match!! */
	    sys_info->sys_board = (sys_board_array_base + index)->sys_board;
	    Strcpy(sys_info->sys_board_name,
		   (sys_board_array_base + index)->sys_board_name);
	    return (TRUE);
	 }
      }					/* end for() */
   }					/* end else */

   /* if we are here we have failed */
   sys_info->sys_board = PLT_UNKNOWN;
   Strcpy(sys_info->sys_board_name, "Unknown");
   return (FALSE);
}					/* end get_sys_board_type() */

#if defined(linux) && !defined(__KERNEL__)
#if !defined(XFree86Server)

/******************************************************************
 *
 * protected_mode_access( unsigned int mode, unsigned int width,
 * unsigned long addr, unsigned char* pdata )
 * This function provides access to physical memory
 * at the requested address.
 * mode is: PLT_READ or PLT_WRITE (accesses a single byte, word 
 * or double word depending on the value of "width". 
 * Only 1, 2 or 4 supported).
 * PLT_READ_BYTES, PLT_WRITE_BYTES accesses "width" number 
 * of bytes (8 bits)
 * PLT_READ_WORDS, PLT_WRITE_WORDS accesses "width" number
 * of words (16 bits) PLT_READ_DWORDS, PLT_WRITE_DWORDS accesses 
 * "width" number of dwords (32 bits)
 * width is: The size of the access.  
 * For PLT_READ or PLT_WRITE, only 1, 2 and 4 are
 * supported.  For other modes, width is not limited but 
 * will cause paging if the block traverses page boundaries.  
 * addr is: The physical address being accessed
 * pdata is: A pointer to the data to be read or written into.
 * NOTE! WORD or DWORD accesses can only be made on 
 * WORD or DWORD boundaries!
 *
 ******************************************************************
 */

static void
protected_mode_access(unsigned int mode, unsigned int width,
		      unsigned long addr, unsigned char *pdata)
{

#define PMTRASH 0x12345678L

   unsigned long base;			/* The physical page address */
   int length = 0x1000;			/* the page size is 4k */
   unsigned int offset = 0;		/* The physical addr offset into page */
   unsigned int index = 0;		/* Used to read/write from/to a block */
   unsigned int chunk = 0;		/* The amount to read/wr from THIS block */
   unsigned int size = 0;		/* Data size shift value (to avoid math) */
   static void *ptr;			/* pointer to real memory location. */

   static unsigned long lastbase = PMTRASH;

   /* temp storage of previous base used. */
   /* type specific buffer pointers */
   unsigned char *byte_data = (unsigned char *)pdata;
   unsigned int *word_data = (unsigned int *)pdata;
   unsigned long *dword_data = (unsigned long *)pdata;

   switch (mode) {

   case PLT_READ_WORDS:
   case PLT_WRITE_WORDS:

      size = 1;
      break;

   case PLT_READ_DWORDS:
   case PLT_WRITE_DWORDS:

      size = 2;
   }

   /* Check if we're in the user accessable range */
   if (addr < 0xFF000000L) {

      /* We get physical memory in "pages", defined by the 
       * following "base" address and the "offset" into it.
       * "base" will be used with mmap to get "ptr", which 
       * points to the memory mapped actual physical memory at 
       * the address pointed-to by "base". 
       * "width" and "chunk" are in units of whatever data 
       * type we're reading.  
       * "length" and "offset" are in units of bytes.  
       * "width" and "chunk" must be adjusted with "<<size" 
       * to use with "length" or "offset".  Similarly, the 
       * result must be adjusted with ">>size" to make into the 
       * proper type units when done.
       */
      base = addr & 0xFFFFF000L;
      offset = addr & 0x00000FFFL;
      do {
	 if ((offset + (width << size)) > length) {

	    /* Block being read extends beyond the 
	     * page boundary. Adjust things. 
	     */
	    chunk = (length - offset) >> size;

	    /* Figure the chunk size */
	    width -= chunk;

	    /* Reduce width by the current chunk */
	 } else {

	    /* Block being read is within the 
	     * page boundary.
	     */
	    chunk = width;
	    width = 0;

	    /* set to zero so we'll exit at the end */

	 }
	 /* We keep the page around in case we need to
	  * access it again.
	  * This saves us some time if we have consecutive 
	  * accesses.
	  */

	 if (base != lastbase) {

	    /* we haven't mmap'd this address 
	     * Have to get a new page.  Free the
	     * previous page, if it's valid  (ie, not 
	     * PMTRASH). If not, unmap it and get the 
	     * new page. 
	     */
	    if (lastbase != PMTRASH)
	       munmap(ptr, length);
	    ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		       base);
	    if ((int)ptr == -1) {
	       lastbase = PMTRASH;
	       return;			/* error */
	    }
	 }

	 /* Now we're ready to get the data.  
	  * It's pure memory access now, no funny 
	  * function calls, however we do cast things to get 
	  * the right size data.
	  */

	 /* Scale the offset for the data type size */
	 index = offset >> size;

	 /* Note that the above line and below lines, 
	  * which shift "offset", discard address information 
	  * if you happen to be trying to write, for example,
	  * dwords on non-dword boundaries.
	  */
	 /* Note that cases PLT_READ and PLT_WRITE don't 
	  * use "index". They shift "offset"  on their own. 
	  * This is because in PLT_READ and PLT_WRITE modes, 
	  * the information on the size of the data 
	  * transaction is in the "width" variable not "size".
	  * We also need separate cases to cast the values 
	  * right. 
	  */
	 switch (mode) {

	 case PLT_READ:{

	       switch (chunk) {

	       case FOUR_BYTES:

		  *(dword_data) = (unsigned long)
			(*(((unsigned long *)ptr) + (offset >> 2)));
		  break;

	       case TWO_BYTES:

		  *(word_data) = (unsigned int)
			(*(((unsigned int *)ptr) + (offset >> 1)));
		  break;

	       default:

		  *(byte_data) = (unsigned char)
			(*(((unsigned char *)ptr) + (offset)));
		  break;

	       }			/* end switch() */
	       break;

	    }				/* end case PLT_READ */

	 case PLT_WRITE:{

	       switch (chunk) {

	       case FOUR_BYTES:

		  *(((unsigned long *)ptr) + (offset >> 2)) = *dword_data;
		  break;

	       case TWO_BYTES:

		  *(((unsigned int *)ptr) + (offset >> 1)) = *word_data;
		  break;

	       default:

		  *(((unsigned char *)ptr) + (offset)) = *byte_data;
		  break;
	       }			/* end switch() */
	       break;

	    }				/* end case PLT_WRITE */

	 case PLT_READ_BYTES:{

	       for (; chunk > 0; chunk--) {

		  *(byte_data++) = (unsigned char)(*(((unsigned char *)ptr) +
						     (index++)));
	       }
	       break;
	    }				/* end case PLT_READ_BYTES */

	 case PLT_WRITE_BYTES:{

	       for (; chunk > 0; chunk--) {
		  *(((unsigned char *)ptr) + (index++)) = *(byte_data++);
	       }
	       break;

	    }				/* end case PLT_WRITE_BYTES */

	 case PLT_READ_WORDS:{

	       for (; chunk > 0; chunk--) {

		  *(word_data++) = (unsigned int)
			(*(((unsigned int *)ptr) + (index++)));
	       }
	       break;

	    }				/* end case PLT_READ_WORDS */

	 case PLT_WRITE_WORDS:{

	       for (; chunk > 0; chunk--) {

		  *(((unsigned int *)ptr) + (index++)) = *(word_data++);
	       }
	       break;

	    }				/* end case PLT_WRITE_WORDS */

	 case PLT_READ_DWORDS:{

	       for (; chunk > 0; chunk--) {

		  *(dword_data++) = (*(((unsigned long *)ptr) + (index++)));
	       }
	       break;

	    }				/* end case PLT_READ_DWORDS */

	 case PLT_WRITE_DWORDS:{

	       for (; chunk > 0; chunk--) {

		  *(((unsigned long *)ptr) + (index++))
			= *(dword_data++);
	       }
	       break;

	    }				/* end case PLT_WRITE_DWORDS */

	 }				/* end switch(mode) */

	 lastbase = base;

	 /* Save the page we've just processed. */

	 if (width) {

	    /* If there's still width left to get. */

	    base += length;
	    /* Increment to the next page. */

	    offset = 0;
	    /* Set the offset to zero. */
	 }

      } while (width);			/* While there's still data to get. */
      return;

   } /* end for if addr */
   else {

      printf("PMA error: Unable to read ROM address space\n");
      exit(1);
   }
   return;
}

/************************************************************************
 * setup_pma() loads the ROM memory access module and initializes 
 * memory access file descriptor (access is handled through a file-like 
 * interface).
 ************************************************************************
 */
static void
setup_pma()
{
   fd = open("/dev/mem", 2);		/*  O_RDWR */
   if (fd == -1) {

      printf("Error: Unable to open /dev/mem !\a\n");
      exit(1);
   }
   return;
}

/**********************************************************************
 * close_pma() cleans up the open memory access devices and file 
 * descriptors.
 **********************************************************************
 */
static void
close_pma()
{
   close(fd);
   return;
}
#endif /* IN_MODULE */
#endif /* linux && !__KERNEL__ */
