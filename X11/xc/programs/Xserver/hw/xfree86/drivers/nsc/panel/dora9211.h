/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/dora9211.h,v 1.1 2002/12/10 15:12:28 alanh Exp $ */
/*
 * $Workfile: dora9211.h $
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
 *     Geode graphics driver for panel support
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
 * National Semiconductor Corporation Open Source License for Durango
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
 * National Semiconductor Corporation Gnu General Public License for Durango
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

#include "panel.h"
#include "92xx.h"

#ifndef _DORA9211_h
#define _DORA9211_h

/* 9211 Rev.C3 Dorado */

/* GPIO Pin Configuration Registers */

#define DRD_GEODE_GPPIN_SEL	0x20	/* GPIO Pin Configuration Select */
#define DRD_GEODE_GPPIN_CFG	0x24	/* GPIO Pin Configuration Access */
#define DRD_GEODE_GPPIN_RESET	0x28	/* GPIO Pin Reset */

#define DRD_GEODE_GPIO_BASE	0x6400	/* F0 GPIO, IO mapped */
#define DRD_GEODE_GPDI0		0x04	/* GPIO Data In 0 */
#define DRD_GEODE_GPDO0		0x00	/* GPIO Data Out 0 */

/* Data Ports in */
#define	DRD_CLOCKP9211IN	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDI0
#define	DRD_DATAINP9211IN	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDI0
#define	DRD_DATAOUTP9211IN	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDI0
#define	DRD_CSP9211IN		DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDI0

/* Data Ports out */
#define	DRD_CLOCKP9211OUT	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDO0
#define	DRD_DATAINP9211OUT	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDO0
#define	DRD_DATAOUTP9211OUT	DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDO0
#define DRD_CSP9211OUT		DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDO0

/* Pin MASKS */
#define	DRD_CLOCK9211	0x00000080	/*;gpio 7, clock output to 9211 */
#define	DRD_DATAIN9211	0x00040000	/*;gpio 18, data output to 9211 */
#define	DRD_DATAOUT9211	0x00000800	/*;gpio 11, data input from 9211 */
#define	DRD_CS9211	0x00000200	/*;gpio 9, chip select output to 9211 */

/* Gpio CFG values to select in  */
#define	DRD_CLOCK9211CFG	0x00000007	/* ;gpio 7 */
#define	DRD_DATAIN9211CFG	0x00000012	/* ;gpio 18 */
#define	DRD_DATAOUT9211CFG	0x0000000B	/* ;gpio 11 */
#define	DRD_CS9211CFG		0x00000009	/* ;gpio 9 */

#define DRD_LCDRESGPIO1		0x00
#define DRD_LCDRESGPIO2		0x01
#define DRD_LCDRESGPIO3		0x02
#define DRD_LCDRESGPIO4		0x04

void Dorado9211SetCS(void);
void Dorado9211ClearCS(void);
void Dorado9211SetDataOut(void);
void Dorado9211ClearDataOut(void);
unsigned char Dorado9211ReadDataIn(void);
void Dorado9211ToggleClock(void);
void Dorado9211SetClock(void);
void Dorado9211ClearClock(void);
void Dorado9211GpioInit(void);
unsigned long Dorado9211ReadReg(unsigned short index);
void Dorado9211WriteReg(unsigned short index, unsigned long data);
void DoradoProgramFRMload(void);
void Dorado_Get_9211_Details(unsigned long flags, PPnl_PanelParams pParam);
void Dorado_Power_Up(void);
void Dorado_Power_Down(void);
void Dorado_Save_Panel_State(void);
void Dorado_Restore_Panel_State(void);
void Dorado9211Init(Pnl_PanelStat * pstat);

#endif /* !_DORA9211_h */
