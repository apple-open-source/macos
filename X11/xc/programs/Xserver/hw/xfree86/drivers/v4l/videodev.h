#ifndef __LINUX_VIDEODEV_H
#define __LINUX_VIDEODEV_H

/* Linux V4L API, Version 1
 * videodev.h from v4l driver in Linux 2.2.3 
 *
 * Used here with the explicit permission of the original author, Alan Cox.
 *				<alan@lxorguk.ukuu.org.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/v4l/videodev.h,v 1.8 2001/03/03 22:46:31 tsi Exp $ */

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

struct video_tuner
{
	int tuner;
	char name[32];
	unsigned long rangelow, rangehigh;	/* Tuner range */
	CARD32 flags;
#define VIDEO_TUNER_PAL		1
#define VIDEO_TUNER_NTSC	2
#define VIDEO_TUNER_SECAM	4
#define VIDEO_TUNER_LOW		8	/* Uses KHz not MHz */
#define VIDEO_TUNER_NORM	16	/* Tuner can set norm */
#define VIDEO_TUNER_STEREO_ON	128	/* Tuner is seeing stereo */
	CARD16 mode;			/* PAL/NTSC/SECAM/OTHER */
#define VIDEO_MODE_PAL		0
#define VIDEO_MODE_NTSC		1
#define VIDEO_MODE_SECAM	2
#define VIDEO_MODE_AUTO		3
	CARD16 signal;			/* Signal strength 16bit scale */
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

struct video_capture
{
	CARD32 	x,y;			/* Offsets into image */
	CARD32	width, height;		/* Area to capture */
	CARD16	decimation;		/* Decimation divder */
	CARD16	flags;			/* Flags for capture */
#define VIDEO_CAPTURE_ODD		0	/* Temporal */
#define VIDEO_CAPTURE_EVEN		1
};

struct video_buffer
{
	void	*base;
	int	height,width;
	int	depth;
	int	bytesperline;
};

struct video_mmap
{
	unsigned	int frame;		/* Frame (0 - n) for double buffer */
	int		height,width;
	unsigned	int format;		/* should be VIDEO_PALETTE_* */
};

struct video_key
{
	CARD8	key[8];
	CARD32	flags;
};


#define VIDEO_MAX_FRAME		32

struct video_mbuf
{
	int	size;		/* Total memory to map */
	int	frames;		/* Frames */
	int	offsets[VIDEO_MAX_FRAME];
};
	

#define 	VIDEO_NO_UNIT	(-1)

	
struct video_unit
{
	int 	video;		/* Video minor */
	int	vbi;		/* VBI minor */
	int	radio;		/* Radio minor */
	int	audio;		/* Audio minor */
	int	teletext;	/* Teletext minor */
};

#define VIDIOCGCAP		_IOR('v',1,struct video_capability)	/* Get capabilities */
#define VIDIOCGCHAN		_IOWR('v',2,struct video_channel)	/* Get channel info (sources) */
#define VIDIOCSCHAN		_IOW('v',3,struct video_channel)	/* Set channel 	*/
#define VIDIOCGTUNER		_IOWR('v',4,struct video_tuner)		/* Get tuner abilities */
#define VIDIOCSTUNER		_IOW('v',5,struct video_tuner)		/* Tune the tuner for the current channel */
#define VIDIOCGPICT		_IOR('v',6,struct video_picture)	/* Get picture properties */
#define VIDIOCSPICT		_IOW('v',7,struct video_picture)	/* Set picture properties */
#define VIDIOCCAPTURE		_IOW('v',8,int)				/* Start, end capture */
#define VIDIOCGWIN		_IOR('v',9, struct video_window)	/* Set the video overlay window */
#define VIDIOCSWIN		_IOW('v',10, struct video_window)	/* Set the video overlay window - passes clip list for hardware smarts , chromakey etc */
#define VIDIOCGFBUF		_IOR('v',11, struct video_buffer)	/* Get frame buffer */
#define VIDIOCSFBUF		_IOW('v',12, struct video_buffer)	/* Set frame buffer - root only */
#define VIDIOCKEY		_IOR('v',13, struct video_key)		/* Video key event - to dev 255 is to all - cuts capture on all DMA windows with this key (0xFFFFFFFF == all) */
#define VIDIOCGFREQ		_IOR('v',14, unsigned long)		/* Set tuner */
#define VIDIOCSFREQ		_IOW('v',15, unsigned long)		/* Set tuner */
#define VIDIOCGAUDIO		_IOR('v',16, struct video_audio)	/* Get audio info */
#define VIDIOCSAUDIO		_IOW('v',17, struct video_audio)	/* Audio source, mute etc */
#define VIDIOCSYNC		_IOW('v',18, int)			/* Sync with mmap grabbing */
#define VIDIOCMCAPTURE		_IOW('v',19, struct video_mmap)		/* Grab frames */
#define VIDIOCGMBUF		_IOR('v', 20, struct video_mbuf)	/* Memory map buffer info */
#define VIDIOCGUNIT		_IOR('v', 21, struct video_unit)	/* Get attached units */
#define VIDIOCGCAPTURE		_IOR('v',22, struct video_capture)	/* Get frame buffer */
#define VIDIOCSCAPTURE		_IOW('v',23, struct video_capture)	/* Set frame buffer - root only */

#define BASE_VIDIOCPRIVATE	192		/* 192-255 are private */


#define VID_HARDWARE_BT848	1
#define VID_HARDWARE_QCAM_BW	2
#define VID_HARDWARE_PMS	3
#define VID_HARDWARE_QCAM_C	4
#define VID_HARDWARE_PSEUDO	5
#define VID_HARDWARE_SAA5249	6
#define VID_HARDWARE_AZTECH	7
#define VID_HARDWARE_SF16MI	8
#define VID_HARDWARE_RTRACK	9
#define VID_HARDWARE_ZOLTRIX	10
#define VID_HARDWARE_SAA7146    11
#define VID_HARDWARE_VIDEUM	12	/* Reserved for Winnov videum */
#define VID_HARDWARE_RTRACK2	13
#define VID_HARDWARE_PERMEDIA2	14	/* Reserved for Permedia2 */
#define VID_HARDWARE_RIVA128	15	/* Reserved for RIVA 128 */
#define VID_HARDWARE_PLANB	16	/* PowerMac motherboard video-in */
#define VID_HARDWARE_BROADWAY	17	/* Broadway project */
#define VID_HARDWARE_GEMTEK	18
#define VID_HARDWARE_TYPHOON	19
#define VID_HARDWARE_VINO	20	/* Reserved for SGI Indy Vino */

/*
 *	Initialiser list
 */
 
struct video_init
{
	char *name;
	int (*init)(struct video_init *);
};

#endif
