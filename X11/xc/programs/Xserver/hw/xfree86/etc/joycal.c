/* $XFree86: xc/programs/Xserver/hw/xfree86/etc/joycal.c,v 3.4 1997/11/22 00:00:18 hohndel Exp $ */

/* A simple program to get the Joystick calibration coordinates */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#if defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <machine/joystick.h>
#define JS_RETURN sizeof(struct joystick)
#define JS_DATA_TYPE joystick
#define button_down(j) (j.b1 | j.b2)
#endif
#ifdef linux
#define inline __inline__
#include <linux/joystick.h>
#define button_down(j) (j.buttons)
#if defined(JSIOCGTIMELIMIT)
/* make 2.1.x joystick.h backward compatable */
#define JS_DATA_TYPE		js_status
#endif

#endif
#include <fcntl.h>
#include <stdio.h>

extern int errno;

#define TIMEOUT_JS 50000

main(int argc, char *argv[])
{
  int fd;
  int cx, cy, minx, miny, maxx, maxy;
  int xdiff, ydiff;
  struct JS_DATA_TYPE js;
  char l[8];

  if (argc < 2) {
    fprintf(stderr, "Usage: %s device\n", argv[0]);
    exit(1);
  }

  if ((fd = open(argv[1], O_RDWR | O_NDELAY)) < 0) {
    fprintf(stderr, "%s: cannot open joystick device %s\n", argv[0], argv[1]);
    exit(1);
  }

  fprintf(stderr, "Center the joystick, then press a button\n");
  do {
    read(fd, &js, JS_RETURN);
    usleep(TIMEOUT_JS);
  } while (!button_down(js));
  cx = js.x; cy = js.y;
  printf("Center coordinates: %d, %d\n", js.x, js.y);
  sleep(1);
  fprintf(stderr, "Position the joystick at upper left, then press a button\n");
  do {
    read(fd, &js, JS_RETURN);
    usleep(TIMEOUT_JS);
  } while (!button_down(js));
  minx = js.x; miny = js.y;
  printf("Upper left coordinates: %d, %d\n", js.x, js.y);
  sleep(1);
  fprintf(stderr, "Position the joystick at lower right, then press a"
		  " button\n");
  do {
    read(fd, &js, JS_RETURN);
    usleep(TIMEOUT_JS);
  } while (!button_down(js));
  printf("Lower right coordinates: %d, %d\n", js.x, js.y);
  maxx = js.x; maxy = js.y;
  xdiff = (maxx - minx) / 2;
  ydiff = (maxy - miny) / 2;
  maxx = cx + xdiff;
  minx = cx - xdiff;
  maxy = cy + ydiff;
  miny = cy - ydiff;
  printf("\n");
  printf("CenterX %d\n", cx);
  printf("CenterY %d\n", cy);
  printf("MinimumXPosition %d\n", minx);
  printf("MaximumXPosition %d\n", maxx);
  printf("MinimumYPosition %d\n", miny);
  printf("MaximumYPosition %d\n", maxy);
  close(fd);
  exit(0);
}
