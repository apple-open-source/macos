/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/cen9211.h,v 1.1 2002/12/10 15:12:28 alanh Exp $ */
/*
 * $Workfile: cen9211.h $
 * $Revision: 1.1.1.1 $
 *
 * File Contents: This header file defines the Durango routines and 
 *                variables used to access the memory mapped regions.
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

#include "92xx.h"
#include "panel.h"

#ifndef _CEN9211_h
#define _CEN9211_h

/* Centaurus/97317 GPIO PORT defines  */

#define CENT_97317_CLOCK_PORT		0xE0
#define CENT_97317_DATA_OUTPORT		0xE0
#define CENT_97317_CHIP_SELECT		0xE4
#define CENT_97317_DATA_INPORT		0xE4
#define CENT_PORT1_DIRECTION		0xE1
#define CENT_PORT2_DIRECTION		0xE5
#define CENT_PORT1_OUTPUT_TYPE		0xE2
#define CENT_PORT2_OUTPUT_TYPE		0xE6

/*
 * Centaurus/97317 GPIO bit masks. 
 * In and out are with respect to the 97317. 
 */

#define CENT_97317_CLOCK_MASK		0x10
#define CENT_97317_CHIP_SEL_MASK	0x01
#define CENT_97317_DATA_IN_MASK		0x02
#define CENT_97317_DATA_OUT_MASK	0x80

#define CENT_PASS    1
#define CENT_FAIL    0
#define CENT_READ    0x0000
#define CENT_WRITE   0x0001
#define CENT_NO_DATA 0

#define CENT_CONFIG_INDEX  0x2E
#define CENT_SIOC2         0x22
#define CENT_CONFIG_DATA   0x2F
#define CENT_GPIO_BANK_SELECT	0x80

#define CENT_NUM_READ_CLOCK_TOGGLES	1
#define CENT_NUM_WRITE_CLOCK_TOGGLES	4

/* local functions */
void set_Centaurus_92xx_mode_params(int mode);
void enable_Centaurus_9211_chip_select(void);
void disable_Centaurus_9211_chip_select(void);
void toggle_Centaurus_9211_clock(void);
void write_Centaurus_CX9211_GPIO(unsigned char databit);
void write_Centaurus_CX9211_DWdata(unsigned long data);
void Centaurus_write_gpio(int width, unsigned long address,
			  unsigned long data);
void Centaurus_Power_Up(void);
void Centaurus_Power_Down(void);
unsigned long Centaurus_read_gpio(int width, unsigned long address);
unsigned char read_Centaurus_CX9211_GPIO(void);
unsigned long read_Centaurus_CX9211_DWdata(void);
unsigned char restore_Centaurus_97317_SIOC2(void);
unsigned char init_Centaurus_GPIO(void);
unsigned char init_Centaurus_9211(void);
unsigned char set_Centaurus_92xx_mode(Pnl_PanelStat * pstat);
void CentaurusProgramFRMload(void);
void Centaurus_Get_9211_Details(unsigned long flags, PPnl_PanelParams pParam);
void Centaurus_Save_Panel_State(void);
void Centaurus_Restore_Panel_State(void);
void Centaurus_9211init(Pnl_PanelStat * pstat);

#endif /* !_CEN9211_h */

/* END OF FILE */
