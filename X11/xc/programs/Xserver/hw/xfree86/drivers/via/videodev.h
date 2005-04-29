#ifndef __LINUX_VIDEODEV_H
#define __LINUX_VIDEODEV_H

/* Linux V4L API, Version 1
 * videodev.h from v4l driver in Linux 2.2.3 
 *
 * Used here with the explicit permission of the original author, Alan Cox.
 *				<alan@lxorguk.ukuu.org.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/videodev.h,v 1.1 2003/08/04 10:32:28 eich Exp $ */

#include "Xmd.h"

#define VID_TYPE_CAPTURE	1	/* Can capture */
#define VID_TYPE_TUNER		2	/* Can tune */
#define VID_TYPE_TELETEXT	4	/* Does teletext */
#define VID_TYPE_OVERLAY	8	/* Overlay onto frame buffer */
#define VID_TYPE_CHROMAKEY	16	/* Overlay by chromakey */
#define VID_TYPE_CLIPPING	32	/* Can clip */
#define VID_TYPE_FRAMERAM	64	/* Uses the frame buffer memory */
#define VID_TYPE_SCALES		128	/* Scalable */
#define VID_TYPE_MONOCHROME	256	/* Monochrome only */
#define VID_TYPE_SUBCAPTURE	512	/* Can capture subareas of the image */

struct video_capability
{
	char name[32];
	int type;
	int channels;	/* Num channels */
	int audios;	/* Num audio devices */
	int maxwidth;	/* Supported width */
	int maxheight;	/* And height */
	int minwidth;	/* Supported width */
	int minheight;	/* And height */
};


struct video_picture
{
	CARD16	brightness;
	CARD16	hue;
	CARD16	colour;
	CARD16	contrast;
	CARD16	whiteness;	/* Black and white only */
	CARD16	depth;		/* Capture depth */
	CARD16   palette;	/* Palette in use */
#define VIDEO_PALETTE_GREY	1	/* Linear greyscale */
#define VIDEO_PALETTE_HI240	2	/* High 240 cube (BT848) */
#define VIDEO_PALETTE_RGB565	3	/* 565 16 bit RGB */
#define VIDEO_PALETTE_RGB24	4	/* 24bit RGB */
#define VIDEO_PALETTE_RGB32	5	/* 32bit RGB */	
#define VIDEO_PALETTE_RGB555	6	/* 555 15bit RGB */
#define VIDEO_PALETTE_YUV422	7	/* YUV422 capture */
#define VIDEO_PALETTE_YUYV	8
#define VIDEO_PALETTE_UYVY	9	/* The great thing about standards is ... */
#define VIDEO_PALETTE_YUV420	10
#define VIDEO_PALETTE_YUV411	11	/* YUV411 capture */
#define VIDEO_PALETTE_RAW	12	/* RAW capture (BT848) */
#define VIDEO_PALETTE_YUV422P	13	/* YUV 4:2:2 Planar */
#define VIDEO_PALETTE_YUV411P	14	/* YUV 4:1:1 Planar */
#define VIDEO_PALETTE_YUV420P	15	/* YUV 4:2:0 Planar */
#define VIDEO_PALETTE_YUV410P	16	/* YUV 4:1:0 Planar */
#define VIDEO_PALETTE_PLANAR	13	/* start of planar entries */
#define VIDEO_PALETTE_COMPONENT 7	/* start of component entries */
};

struct video_audio
{
	int	audio;		/* Audio channel */
	CARD16	volume;		/* If settable */
	CARD16	bass, treble;
	CARD32	flags;
#define VIDEO_AUDIO_MUTE	1
#define VIDEO_AUDIO_MUTABLE	2
#define VIDEO_AUDIO_VOLUME	4
#define VIDEO_AUDIO_BASS	8
#define VIDEO_AUDIO_TREBLE	16	
	char    name[16];
#define VIDEO_SOUND_MONO	1
#define VIDEO_SOUND_STEREO	2
#define VIDEO_SOUND_LANG1	4
#define VIDEO_SOUND_LANG2	8
        CARD16   mode;
        CARD16	balance;	/* Stereo balance */
        CARD16	step;		/* Step actual volume uses */
};

struct video_clip
{
	INT32	x,y;
	INT32	width, height;
	struct	video_clip *next;	/* For user use/driver use only */
};

struct video_window
{
	CARD32	x,y;			/* Position of window */
	CARD32	width,height;		/* Its size */
	CARD32	chromakey;
	CARD32	flags;
	struct	video_clip *clips;	/* Set only */
	int	clipcount;
#define VIDEO_WINDOW_INTERLACE	1
#define VIDEO_CLIP_BITMAP	-1
/* bitmap is 1024x625, a '1' bit represents a clipped pixel */
#define VIDEO_CLIPMAP_SIZE	(128 * 625)
};

struct video_channel
{
	int channel;
	char name[32];
	int tuners;
	CARD32  flags;
#define VIDEO_VC_TUNER		1	/* Channel has a tuner */
#define VIDEO_VC_AUDIO		2	/* Channel has audio */
	CARD16  type;
#define VIDEO_TYPE_TV		1
#define VIDEO_TYPE_CAMERA	2	
	CARD16 norm;			/* Norm set by channel */
};

#define VIDEO_MODE_PAL		0
#define VIDEO_MODE_NTSC		1
#define VIDEO_MODE_SECAM	2
#define VIDEO_MODE_AUTO		3

#endif
