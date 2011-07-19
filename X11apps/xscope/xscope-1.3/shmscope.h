#ifndef _MITSHMSCOPE_H_
#define _MITSHMSCOPE_H_

#define MITSHMREQUESTHEADER  "MITSHMREQUEST"
#define MITSHMREPLYHEADER    "MITSHMREPLY"

/*
  To aid in making the choice between level 1 and level 2, we
  define the following define, which does not print relatively
  unimportant fields.
*/

#define printfield(a,b,c,d,e) if (Verbose > 1) PrintField(a,b,c,d,e)

extern void MitshmQueryVersion		(FD fd, const unsigned char *buf);
extern void MitshmQueryVersionReply	(FD fd, const unsigned char *buf);
extern void MitshmAttach		(FD fd, const unsigned char *buf);
extern void MitshmDetach		(FD fd, const unsigned char *buf);
extern void MitshmPutImage		(FD fd, const unsigned char *buf);
extern void MitshmGetImage		(FD fd, const unsigned char *buf);
extern void MitshmGetImageReply		(FD fd, const unsigned char *buf);
extern void MitshmCreatePixmap		(FD fd, const unsigned char *buf);
extern void MitshmShmSegError		(FD fd, const unsigned char *buf);

#endif

