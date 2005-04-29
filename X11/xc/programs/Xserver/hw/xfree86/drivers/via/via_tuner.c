/*
 * Copyright 2003 Red Hat, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_tuner.c,v 1.2 2004/01/02 18:23:36 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "via_compose.h"
#include "via_capture.h"
#include "via.h"
#include "ddmpeg.h"
#include "xf86drm.h"

#include "via_overlay.h"
#include "via_driver.h"
#include "via_regrec.h"
#include "via_priv.h"
#include "via_swov.h"
#include "via_common.h"

/*
 *	Architecture independant implementation of the TV tuner interfaces
 *	on the VIA chipsets. VIA have a video4linux kernel module for Linux
 *	but that gives us i2c ownership clashes and lack of portability. 
 *	Doing it in X means it should work cross platform
 *
 *	The Overlay/TV input engines on the VIA are SAA7108/7113 or 7114 based.
 */
 
 
/*
 *	Wrap the ugly I2C functions from xf86
 */
 
static void WriteI2C(I2CDevPtr i2c, int sa, int v)
{
	unsigned char buf[2];
	buf[0] = sa;
	buf[1] = v;
	xf86I2CWriteRead(i2c, buf, 2, NULL, 0);
}

static void WriteTuner(ViaTunerPtr tuner, int sa, int v)
{
	WriteI2C(tuner->I2C, sa, v);
}

static void WriteTunerList(ViaTunerPtr tuner, unsigned char *p, int len)
{
	while(len >= 2)
	{
		WriteI2C(tuner->I2C, p[0], p[1]);
		p += 2;
		len -= 2;
	}
}

static int ReadI2C(I2CDevPtr i2c, int sa)
{
	unsigned char buf[2];
	buf[0] = sa;
	xf86I2CWriteRead(i2c, buf, 1, buf+1, 1);
	return buf[1];
}

#ifdef UNUSED
static int ReadTuner(ViaTunerPtr tuner, int sa)
{
	return ReadI2C(tuner->I2C, sa);
}
#endif

/*
 *	I2C register tables to switch TV mode
 */

static unsigned char sa7113_standard[3][4] = {
	 { 0xA8, 0x01, 0x0A, 0x07 },	/* PAL */
	 { 0xE8, 0x01, 0x02, 0x0A },	/* NTSC */
	 { 0xA8, 0x51, 0x0A, 0x07 }	/* SECAM */
};

static unsigned char sa7114_standard[3][3] = {
	{ 0x08, 0x30, 0x01 },	/* PAL - CHECK ME */
	{ 0x0B, 0xFF, 0x00 },	/* NTSC */
	{ 0x08, 0x30, 0x01 }	/* SECAM*/
};

void ViaTunerStandard(ViaTunerPtr pTuner, int mode)
{
	switch(pTuner->decoderType)
	{
		case SAA7113H:
		{
			unsigned char *ptr = sa7113_standard[mode];
			WriteTuner(pTuner, 0x08, ptr[0]);
			WriteTuner(pTuner, 0x0E, ptr[1]);
			WriteTuner(pTuner, 0x40, ptr[2]);
			WriteTuner(pTuner, 0x5A, ptr[3]);
			break;
		}
		case SAA7108H:
		case SAA7114H:
		{
			unsigned char *ptr = sa7114_standard[mode];
			WriteTuner(pTuner, 0x8F, ptr[0]);
			WriteTuner(pTuner, 0x9A, ptr[1]);
			WriteTuner(pTuner, 0x9B, ptr[2]);
			WriteTuner(pTuner, 0x9E, ptr[1]);
			WriteTuner(pTuner, 0x9F, ptr[2]);
			WriteTuner(pTuner, 0x88, 0xD0);
			WriteTuner(pTuner, 0x88, 0xF0);
			break;
		}
	}
}

/*
 *	Set TV properties (0-255). Abstracted in case a non SAA tuner
 *	is added with different registers.
 */
 
void ViaTunerBrightness(ViaTunerPtr pTuner, int value)
{
	WriteTuner(pTuner, 0x0A, value);
}

void ViaTunerContrast(ViaTunerPtr pTuner, int value)
{
	WriteTuner(pTuner, 0x0B, value);
}

void ViaTunerHue(ViaTunerPtr pTuner, int value)
{
	WriteTuner(pTuner, 0x0D, value);
}

void ViaTunerLuminance(ViaTunerPtr pTuner, int value)
{
	WriteTuner(pTuner, 0x09, value);
}

void ViaTunerSaturation(ViaTunerPtr pTuner, int value)
{
	WriteTuner(pTuner, 0x0C, value);
}

/*
 *	Input Selection
 */
 
void ViaTunerInput(ViaTunerPtr pTuner, int mode)
{
	switch(pTuner->decoderType)
	{
		case SAA7113H:
			if(mode == MODE_TV)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC0);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x01);
				break;
			}
			if(mode == MODE_SVIDEO)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC9);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x81);
				break;
			}
			if(mode == MODE_COMPOSITE)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC2);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x01);
				break;
			}
			break;
		case SAA7108H:
			if(mode == MODE_TV)
				break;
			if(mode == MODE_SVIDEO)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC6);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x80);
			}
			else if(mode == MODE_COMPOSITE)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC0);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x40);
			}
			WriteTuner(pTuner, 0x08, 0x58);
			WriteTuner(pTuner, 0x08, 0xF8);
			WriteTuner(pTuner, 0x88, 0xD0);
			WriteTuner(pTuner, 0x88, 0xF0);
			break;
		case SAA7114H:
			if(mode == MODE_TV)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC0);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x40);
			}
			else if(mode == MODE_SVIDEO)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC7);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x80);
			}
			else if(mode == MODE_COMPOSITE)
			{
				if(pTuner->autoDetect)
					WriteTuner(pTuner, 0x02, 0xC2);
				else
					WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
				WriteTuner(pTuner, 0x09, 0x40);
			}
			WriteTuner(pTuner, 0x08, 0x58);
			WriteTuner(pTuner, 0x08, 0xF8);
			WriteTuner(pTuner, 0x88, 0xD0);
			WriteTuner(pTuner, 0x88, 0xF0);
			break;
	}
}

/*
 *	Switch Channel
 */
 
void ViaTunerChannel(ViaTunerPtr pTuner, int divider, int control)
{
	unsigned char buf[4];
	buf[0] = divider >> 8;
	buf[1] = divider & 0xFF;
	buf[2] = control >> 8;
	buf[3] = control & 0xFF;
	xf86I2CWriteRead(pTuner->FMI2C, buf, 4, NULL, 0);
}

/*
 *	Set up
 */
 
static void ViaTunerSetup(ViaTunerPtr pTuner)
{
	static unsigned char sa7108_boot[] = {
		0x03, 0x10, 0x04, 0x90, 0x05, 0x90, 0x06, 0xEB,
		0x07, 0xE0, 0x08, 0x98, 0x09, 0x80, 0x0A, 0x80,
		0x0B, 0x44, 0x0C, 0x40, 0x0D, 0x00, 0x0E, 0x89,
		0x0F, 0x2A, 0x10, 0x0E, 0x11, 0x00, 0x12, 0x00,
		0x13, 0x01, 0x14, 0x00, 0x15, 0x11, 0x16, 0xFE,
		0x17, 0x40, 0x18, 0x40, 0x19, 0x80, 
		
		0x80, 0x1C, 0x81, 0x00, 0x82, 0x00, 0x83, 0x00,	/* Scaler present */
		0x84, 0x00, 0x85, 0x00, 0x86, 0x45, 0x87, 0x01,
		0x88, 0xF0, 
		
		0x8F, 0x0B, 0x90, 0x00, 0x91, 0x08, 0x92, 0x09, 
		0x93, 0x80, 0x94, 0x02, 0x95, 0x00, 0x96, 0xD0,
		0x97, 0x02, 0x98, 0x12, 0x99, 0x00, 0x9A, 0x00,
		0x9B, 0x00, 0x9C, 0xD0, 0x9D, 0x02, 0x9E, 0xFF,
		0x9F, 0x00, 0xA0, 0x01, 0xA1, 0x00, 0xA2, 0x00,

		0xA4, 0x80, 0xA5, 0x40, 0xA6, 0x40,
		
		0xA8, 0x00, 0xA9, 0x04, 0xAA, 0x00, 
		
		0xAC, 0x00, 0xAD, 0x02, 0xAE, 0x00,
		
		0xB0, 0x00, 0xB1, 0x04, 0xB2, 0x00, 0xB3, 0x04,
		0xB4, 0x00,
		
		0xB8, 0x00, 0xB9, 0x00, 0xBA, 0x00, 0xBB, 0x00,
		0xBC, 0x00, 0xBD, 0x00, 0xBE, 0x00, 0xBF, 0x00,

		0x88, 0xD0, 0x88, 0xF0
	};
	
	static unsigned char sa7113_boot[] = {
		0x03, 0x33, 0x04, 0x00, 0x05, 0x00, 0x06, 0xE9,
		0x07, 0x0D, 0x08, 0xF8, 0x09, 0x01, 0x0A, 0x80,
		0x0B, 0x47, 0x0C, 0x40, 0x0D, 0x00, 0x0E, 0x01,
		0x0F, 0x2A, 0x10, 0x40, 0x11, 0x08, 0x12, 0xB7,
		0x13, 0x00, 0x14, 0x00, 0x15, 0x00, 0x16, 0x00,
		0x17, 0x00,
		0x40, 0x02, 0x41, 0xFF, 0x42, 0xFF, 0x43, 0xFF,
		0x44, 0xFF, 0x45, 0xFF,	0x46, 0xFF, 0x47, 0xFF,
		0x48, 0xFF, 0x49, 0xFF, 0x4A, 0xFF, 0x4B, 0xFF,
		0x4C, 0xFF, 0x4D, 0xFF, 0x4E, 0xFF, 0x4F, 0xFF,
		0x50, 0xFF, 0x51, 0xFF, 0x52, 0xFF, 0x53, 0xFF,
		0x54, 0xFF, 0x55, 0xFF, 0x56, 0xFF, 0x57, 0xFF,
		0x58, 0x00, 0x59, 0x54,0x5A, 0x0A, 0x5B, 0x83
	};
	
	static unsigned char sa7114_boot[] = {
		0x03, 0x10, 0x04, 0x90, 0x05, 0x90, 0x06, 0xEB,
		0x07, 0xE0, 0x08, 0x98, 0x09, 0x80, 0x0A, 0x80,
		0x0B, 0x44, 0x0C, 0x40, 0x0D, 0x00, 0x0E, 0x89,
		0x0F, 0x2A, 0x10, 0x0E, 0x11, 0x00, 0x12, 0x00,
		0x13, 0x01, 0x14, 0x00, 0x15, 0x11, 0x16, 0xFE,
		0x17, 0x40, 0x18, 0x40, 0x19, 0x80,
		
		0x80, 0x1C, 0x81, 0x00, 0x82, 0x00, 0x83, 0x01,
		0x84, 0x00, 0x85, 0x00, 0x86, 0x45, 0x87, 0x01,
		0x88, 0xF0, 0x8F, 0x0B,
		
		0x90, 0x00, 0x91, 0x08, 0x92, 0x09, 0x93, 0x80,
		0x94, 0x02, 0x95, 0x00, 0x96, 0xD0, 0x97, 0x02,
		0x98, 0x12, 0x99, 0x00, 0x9A, 0xFF, 0x9B, 0x00,
		0x9C, 0xD0, 0x9D, 0x02, 0x9E, 0xFF, 0xA0, 0x01,
		0xA1, 0x00, 0xA2, 0x00, 0xA4, 0x80, 0xA5, 0x40,
		0xA6, 0x40,
		
		0xA8, 0x00, 0xA9, 0x04, 0xAA, 0x00, 
		
		0xAC, 0x00, 0xAD, 0x02, 0xAE, 0x00,
		
		0xB0, 0x00, 0xB1, 0x04, 0xB2, 0x00, 0xB3, 0x04,
		0xB4, 0x00, 0xB8, 0x00, 0xB9, 0x00, 0xBA, 0x00,
		0xBB, 0x00, 0xBC, 0x00, 0xBD, 0x00, 0xBE, 0x00,
		0xBF, 0x00,
		
		0x88, 0xD0, 0x88, 0xF0
	};
	switch(pTuner->decoderType)
	{
		case SAA7113H:
			if(pTuner->autoDetect)
				WriteTuner(pTuner, 0x02, 0xC9);
			else
				WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
			WriteTunerList(pTuner, sa7113_boot, sizeof(sa7113_boot));
			break;
		case SAA7108H:
			if(pTuner->autoDetect)
				WriteTuner(pTuner, 0x02, 0xC0);
			else
				WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
			WriteTunerList(pTuner, sa7108_boot, sizeof(sa7108_boot));
			break;
		case SAA7114H:
			if(pTuner->autoDetect)
				WriteTuner(pTuner, 0x02, 0xC2);
			else
				WriteTuner(pTuner, 0x02, 0xC0|pTuner->tunerMode);
			WriteTunerList(pTuner, sa7114_boot, sizeof(sa7114_boot));
			break;
	}
}

/*
 *	Bit 7 of register 50 enables GPIO control for the audio
 *	Bit 5 then selects which tuner audio input is used
 */
 
#define AUDIO_GPIO1_ENABLE		0x80
#define AUDIO_GPIO_SELECT_TUNER1	0x20
 
void ViaAudioSelect(VIAPtr pVia, int tuner)
{
	int index = VGAIN8(0x3C4);
	int data;
	
	if(!pVia->CXA2104S)
		return;
	
	VGAOUT8(0x3C4, 0x50);	/* Select audio control bit */
	
	data = VGAIN8(0x3C5);
	data |= AUDIO_GPIO1_ENABLE;
	if(tuner == 0)
		data &= ~AUDIO_GPIO_SELECT_TUNER1;
	else
		data |= AUDIO_GPIO_SELECT_TUNER1;
	VGAOUT8(0x3C5, data);
	
	VGAOUT8(0x3C4, index);
}

void ViaAudioInit(VIAPtr pVia)
{
	if(!pVia->CXA2104S)
		return;
	
	WriteI2C(pVia->CXA2104S, 0, 0x01);
	WriteI2C(pVia->CXA2104S, 1, 0x1F);
	WriteI2C(pVia->CXA2104S, 2, 0x1F);
	WriteI2C(pVia->CXA2104S, 3, 0x00);
	WriteI2C(pVia->CXA2104S, 4, 0x00);
}

void ViaAudioMode(VIAPtr pVia, int mode)
{
	if(!pVia->CXA2104S)
		return;
	
	pVia->AudioMode = mode;
	pVia->AudioMute = 0;
	
	switch(mode)
	{
		case AUDIO_STEREO:
			WriteI2C(pVia->CXA2104S, 0, 0x01);
			WriteI2C(pVia->CXA2104S, 1, 0x1F);
			WriteI2C(pVia->CXA2104S, 2, 0x1F);
			WriteI2C(pVia->CXA2104S, 3, 0x01);
			WriteI2C(pVia->CXA2104S, 4, 0x00);
			break;
		case AUDIO_SAP:
			WriteI2C(pVia->CXA2104S, 0, 0x0F);
			WriteI2C(pVia->CXA2104S, 1, 0x1F);
			WriteI2C(pVia->CXA2104S, 2, 0x1F);
			WriteI2C(pVia->CXA2104S, 3, 0x0B);
			WriteI2C(pVia->CXA2104S, 4, 0x20);
			break;
		case AUDIO_DUAL:
			WriteI2C(pVia->CXA2104S, 0, 0x08);
			WriteI2C(pVia->CXA2104S, 1, 0x1F);
			WriteI2C(pVia->CXA2104S, 2, 0x1F);
			WriteI2C(pVia->CXA2104S, 3, 0x0F);
			WriteI2C(pVia->CXA2104S, 4, 0x20);
			break;
	}
}

void ViaAudioMute(VIAPtr pVia, int mute)
{
	if(!pVia->CXA2104S)
		return;
	
	switch(pVia->AudioMode)
	{
		case AUDIO_STEREO:
			WriteI2C(pVia->CXA2104S, 3, 0x01 - mute);
			break;
		case AUDIO_SAP:
			WriteI2C(pVia->CXA2104S, 3, 0x0B - mute);
			break;
		case AUDIO_DUAL:
			WriteI2C(pVia->CXA2104S, 3, 0x0F - mute);
			break;
	}
	pVia->AudioMute = mute;
}
				
/*
 *	Check for philips tuners to go with the I2C devices
 */
 	
static void ViaProbeFMTuner(ViaTunerPtr pTuner, int slave)
{
	if(!xf86I2CProbeAddress(pTuner->I2C->pI2CBus, slave))
		return;
	pTuner->FMI2C = xf86CreateI2CDevRec();
	pTuner->FMI2C->DevName = "FI1236";
	pTuner->FMI2C->SlaveAddr = slave;
	pTuner->FMI2C->pI2CBus = pTuner->I2C->pI2CBus;
	if(!xf86I2CDevInit(pTuner->FMI2C))
	{
		xf86DestroyI2CDevRec(pTuner->FMI2C, TRUE);
		pTuner->FMI2C = NULL;
	}
}

/*
 *	Helper for tuner creation
 */

static ViaTunerPtr CreateTuner(int type, I2CDevPtr pI2C)
{
	ViaTunerPtr v = xnfcalloc(sizeof(ViaTunerRec), 1);
	v->FMI2C = NULL;
	v->I2C = pI2C;
	v->decoderType = type;
	return v;
}

/*
 *	Probe and configure VIA tuner devices on the second I2C bus
 */
 
void ViaTunerProbe(ScrnInfoPtr pScrn)
{
	VIAPtr pVia = VIAPTR(pScrn);
	I2CDevPtr dev;
	I2CDevPtr tdev;
	
	pVia->Tuner[0] = NULL;
	pVia->Tuner[1] = NULL;
	pVia->CXA2104S = NULL;
	
	/* The TV tuners if present live on I2C bus 2. There will be an
	   encoder/decoder chip or two and one or two tuner ICs. An additional
	   sound IC may also be present */
	   
	dev = xf86CreateI2CDevRec();
	dev->DevName = "TV Probe";
	dev->SlaveAddr = 0x88;
	dev->pI2CBus = pVia->I2C_Port2;
	if (!xf86I2CDevInit(dev))
	{
		xf86DestroyI2CDevRec(dev, TRUE);
		return;
	}
		
	/* Ok so we have a TV processor for TV0 .. but what is it ?
	   Probe 0x88 register 0x1C */
	
	/* Check for an SAA7108H on tuner 1 */
	if(ReadI2C(dev, 0x1C) == 0x04)
	{
		tdev = xf86CreateI2CDevRec();
		tdev->DevName = "SAA7108H";
		tdev->SlaveAddr = 0x40;
		tdev->pI2CBus = pVia->I2C_Port2;
		if (xf86I2CDevInit(dev) && (ReadI2C(tdev, 0x00) >> 4) == 0x00) 	/* 7108H */
			pVia->Tuner[0] = CreateTuner(SAA7108H, tdev);
		else
			xf86DestroyI2CDevRec(tdev, TRUE);
	}
	else
	{
		/* Check for an SAA7113H on tuner 0 */
		tdev = xf86CreateI2CDevRec();
		tdev->DevName = "SAA7113H";
		tdev->SlaveAddr = 0x48;
		tdev->pI2CBus = pVia->I2C_Port2;
		if (xf86I2CDevInit(dev) && (ReadI2C(tdev, 0x00)  & 0xE0) == 0x00) 	/* 7113H */
			pVia->Tuner[0] = CreateTuner(SAA7113H, tdev);
		else
			xf86DestroyI2CDevRec(tdev, TRUE);
	}

	/* Tuner 0 probe done. Look for tuner 1 */
	
	/* Check for an SAA7108H on tuner 1 */
	if(ReadI2C(dev, 0x1C) == 0x04)
	{
		tdev = xf86CreateI2CDevRec();
		tdev->DevName = "SAA7108H";
		tdev->SlaveAddr = 0x42;
		tdev->pI2CBus = pVia->I2C_Port2;
		if (xf86I2CDevInit(dev) && (ReadI2C(tdev, 0x00) >> 4) == 0x00) 	/* 7108H */
			pVia->Tuner[1] = CreateTuner(SAA7108H, tdev);
		else
			xf86DestroyI2CDevRec(tdev, TRUE);
	}
	else
	{
		/* Check for an SAA7113H on tuner 1 */
		tdev = xf86CreateI2CDevRec();
		tdev->DevName = "SAA7113H";
		tdev->SlaveAddr = 0x4A;
		tdev->pI2CBus = pVia->I2C_Port2;
		if (xf86I2CDevInit(dev) && (ReadI2C(tdev, 0x00)  & 0xE0) == 0x00) 	/* 7113H */
			pVia->Tuner[1] = CreateTuner(SAA7113H, tdev);
		else
		{
			xf86DestroyI2CDevRec(tdev, TRUE);
			/* Check for an SAA7114H on tuner 1 */
			tdev = xf86CreateI2CDevRec();
			tdev->DevName = "SAA7114H";
			tdev->SlaveAddr = 0x40;
			tdev->pI2CBus = pVia->I2C_Port2;
			if (xf86I2CDevInit(dev) && (ReadI2C(tdev, 0x00)  & 0xE0) == 0x00) 	/* 7113H */
				pVia->Tuner[1] = CreateTuner(SAA7114H, tdev);
			else
				xf86DestroyI2CDevRec(tdev, TRUE);
		}
	}	
	xf86DestroyI2CDevRec(dev, TRUE);
	
	if(pVia->Tuner[0])
	{
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Video decoder 0: %s.\n",
			pVia->Tuner[0]->I2C->DevName);
		ViaTunerSetup(pVia->Tuner[0]);
		ViaProbeFMTuner(pVia->Tuner[0], 0xC6);
	}	
	if(pVia->Tuner[1])
	{
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Video decoder 1: %s.\n",
			pVia->Tuner[1]->I2C->DevName);
		ViaTunerSetup(pVia->Tuner[1]);
		ViaProbeFMTuner(pVia->Tuner[1], 0xC0);
	}
	
	/* Check for a CXA2104S audio controller */
	
	if((pVia->Tuner[0] || pVia->Tuner[1]) && xf86I2CProbeAddress(pVia->I2C_Port2, 0x84))
	{
		dev = xf86CreateI2CDevRec();
		dev->DevName = "CXA2104S";
		dev->SlaveAddr = 0x84;
		dev->pI2CBus = pVia->I2C_Port2;
		if(xf86I2CDevInit(dev))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Video decoder 1: %s.\n",
				pVia->Tuner[1]->I2C->DevName);
			pVia->CXA2104S = dev;
		}
		else
			xf86DestroyI2CDevRec(dev, TRUE);
	}	
}

void ViaTunerDestroy(ScrnInfoPtr pScrn)
{
	VIAPtr pVia = VIAPTR(pScrn);
	if(pVia->Tuner[0])
	{
		if(pVia->Tuner[0]->FMI2C)
			xf86DestroyI2CDevRec(pVia->Tuner[0]->FMI2C, TRUE);
		xf86DestroyI2CDevRec(pVia->Tuner[0]->I2C, TRUE);
		xfree(pVia->Tuner[0]);
		pVia->Tuner[0] = NULL;
	}
	if(pVia->Tuner[1])
	{
		if(pVia->Tuner[1]->FMI2C)
			xf86DestroyI2CDevRec(pVia->Tuner[1]->FMI2C, TRUE);
		xf86DestroyI2CDevRec(pVia->Tuner[1]->I2C, TRUE);
		xfree(pVia->Tuner[1]);
		pVia->Tuner[1] = NULL;
	}
	if(pVia->CXA2104S)
		xf86DestroyI2CDevRec(pVia->CXA2104S, TRUE);
}
