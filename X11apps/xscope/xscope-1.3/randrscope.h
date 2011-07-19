#ifndef _RANDRSCOPE_H_
#define _RANDRSCOPE_H_

#define RANDRREQUESTHEADER  "RANDRREQUEST"
#define RANDRREPLYHEADER    "RANDRREPLY"

/*
  To aid in making the choice between level 1 and level 2, we
  define the following define, which does not print relatively
  unimportant fields.
*/

#define printfield(a,b,c,d,e) if (Verbose > 1) PrintField(a,b,c,d,e)

extern void RandrQueryVersion (FD fd, const unsigned char *buf);
extern void RandrQueryVersionReply (FD fd, const unsigned char *buf);
extern void RandrGetScreenInfo (FD fd, const unsigned char *buf);
extern void RandrGetScreenInfoReply (FD fd, const unsigned char *buf);
extern void RandrSetScreenConfig (FD fd, const unsigned char *buf);
extern void RandrSetScreenConfigReply (FD fd, const unsigned char *buf);
extern void RandrScreenChangeSelectInput (FD fd, const unsigned char *buf);
extern void RandrScreenSizes (const unsigned char *buf);
extern void RandrScreenChangeNotifyEvent (const unsigned char *buf);

#endif

