/* $XConsortium: bbox.h,v 2.12 91/07/10 19:34:09 converse Exp $ */
/*
 *			  COPYRIGHT 1987
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting documen-
 * tation, and that the name of Digital Equipment Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.
 */
/* $XFree86: xc/programs/xmh/bbox.h,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

#ifndef _bbox_h
#define _bbox_h

	/* for radio button boxes only */

extern ButtonBox RadioBBoxCreate      (Scrn, char *);
extern void	 RadioBBoxAddButton   (ButtonBox, char *, Boolean);
extern void	 RadioBBoxSet	      (Button);
extern char *	 RadioBBoxGetCurrent  (ButtonBox);
extern void	 RadioBBoxDeleteButton(Button);

	/* for other kinds of button boxes */

extern ButtonBox BBoxCreate	      (Scrn, char *);
extern void	 BBoxAddButton (ButtonBox, char *, WidgetClass, Boolean);
extern void	 BBoxDeleteButton     (Button);

	/* for all kinds of button boxes */

extern void	 BBoxInit             (void);
extern void	 BBoxEnable	      (Button);
extern void	 BBoxDisable	      (Button);
extern Button	 BBoxFindButtonNamed  (ButtonBox, char *);
extern Button	 BBoxFindButton	      (ButtonBox, Widget);
extern Button	 BBoxButtonNumber     (ButtonBox, int);
extern int	 BBoxNumButtons	      (ButtonBox);
extern char *	 BBoxNameOfButton     (Button);
extern Widget	 BBoxMenuOfButton     (Button);
extern void	 BBoxLockSize	      (ButtonBox);
extern Boolean	 BBoxIsGrandparent    (ButtonBox, Widget);

	/* operations upon folder buttons */

extern void	BBoxMailFlag	      (ButtonBox, char*, int);

#endif /* _bbox_h */
