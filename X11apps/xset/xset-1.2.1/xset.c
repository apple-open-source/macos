/*

Copyright 1985, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* Modified by Stephen so keyboard rate is set using XKB extensions */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_X11_EXTENSIONS_DPMS_H
# define DPMSExtension
#endif

#ifdef HAVE_X11_EXTENSIONS_MITMISC_H
# define MITMISC
#endif

#ifdef HAVE_X11_XKBLIB_H
# define XKB
#endif

#if defined(HAVE_X11_EXTENSIONS_XF86MISC_H) && defined(HAVE_X11_EXTENSIONS_XF86MSCSTR_H)
# define XF86MISC
#endif

#if defined(HAVE_X11_EXTENSIONS_FONTCACHE_H) && defined(HAVE_X11_EXTENSIONS_FONTCACHEP_H)
# define FONTCACHE
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xmu/Error.h>
#ifdef MITMISC
# include <X11/extensions/MITMisc.h>
#endif
#ifdef DPMSExtension
# include <X11/extensions/dpms.h>
# ifdef WIN32
#  define BOOL wBOOL
#  ifdef Status
#   undef Status
#   define Status wStatus
#  endif
#  include <windows.h>
#  ifdef Status
#   undef Status
#   define Status int
#  endif
#  undef BOOL
# endif
# ifndef HAVE_USLEEP
#  if defined(SVR4) && defined(sun)
#   include <sys/syscall.h>
#  endif
# endif
#endif /* DPMSExtension */

#ifdef XF86MISC
# include <X11/extensions/xf86misc.h>
# include <X11/extensions/xf86mscstr.h>
#endif
#ifdef XKB
# include <X11/XKBlib.h>
#endif
#ifdef FONTCACHE
# include <X11/extensions/fontcache.h>
# include <X11/extensions/fontcacheP.h>

static Status set_font_cache(Display *, long, long, long);
static void query_cache_status(Display *dpy);
#endif

#define ON 1
#define OFF 0

#define SERVER_DEFAULT (-1)
#define DONT_CHANGE -2

#define DEFAULT_ON (-50)
#define DEFAULT_TIMEOUT (-600)

#define ALL -1
#define TIMEOUT 1
#define INTERVAL 2
#define PREFER_BLANK 3
#define ALLOW_EXP 4

#ifdef XF86MISC
# define KBDDELAY_DEFAULT 500
# define KBDRATE_DEFAULT 30
#endif
#ifdef XKB
# define XKBDDELAY_DEFAULT 660
# define XKBDRATE_DEFAULT (1000/40)
#endif

#define	nextarg(i, argv) \
	argv[i]; \
	if (i >= argc) \
		break; \

static char *progName;

static int error_status = 0;

static int is_number(char *arg, int maximum);
static void set_click(Display *dpy, int percent);
static void set_bell_vol(Display *dpy, int percent);
static void set_bell_pitch(Display *dpy, int pitch);
static void set_bell_dur(Display *dpy, int duration);
static void set_font_path(Display *dpy, char *path, int special,
			  int before, int after);
static void set_led(Display *dpy, int led, int led_mode);
static void xkbset_led(Display *dpy, const char *led, int led_mode);
static void set_mouse(Display *dpy, int acc_num, int acc_denom, int threshold);
static void set_saver(Display *dpy, int mask, int value);
static void set_repeat(Display *dpy, int key, int auto_repeat_mode);
static void set_pixels(Display *dpy, unsigned long *pixels, caddr_t *colors,
		       int numpixels);
static void set_lock(Display *dpy, Bool onoff);
static char *on_or_off(int val, int onval, char *onstr,
		       int offval, char *offstr, char buf[]);
static void query(Display *dpy);
static void usage(char *fmt, ...);
static void error(char *message);
static int local_xerror(Display *dpy, XErrorEvent *rep);

#ifdef XF86MISC
static void set_repeatrate(Display *dpy, int delay, int rate);
#endif
#ifdef XKB
static void xkbset_repeatrate(Display *dpy, int delay, int rate);
#endif

int
main(int argc, char *argv[])
{
    register char *arg;
    register int i;
    int percent;
    int acc_num, acc_denom, threshold;

#ifdef DPMSExtension
    CARD16 standby_timeout, suspend_timeout, off_timeout;
#endif
    int key, auto_repeat_mode;
    XKeyboardControl values;

#define MAX_PIXEL_COUNT 512
    unsigned long pixels[MAX_PIXEL_COUNT];
    caddr_t colors[MAX_PIXEL_COUNT];
    int numpixels = 0;
    char *disp = NULL;
    Display *dpy;
    Bool hasargs = False;

    int miscpresent = 0;
    int xkbpresent = 0;

#ifdef XKB
    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
#else
#endif
#ifdef FONTCACHE
    long himark, lowmark, balance;
#endif

    progName = argv[0];
    for (i = 1; i < argc; i++) {
	arg = argv[i];
	if (strcmp(arg, "-display") == 0 || strcmp(arg, "-d") == 0) {
	    if (++i >= argc)
		usage("missing argument to -display", NULL);
	    disp = argv[i];
	} else {
	    hasargs = True;
	}
    }
    if (!hasargs) {
	usage(NULL, NULL);	       /* replace with window interface */
    }

    dpy = XOpenDisplay(disp);    /*  Open display and check for success */
    if (dpy == NULL) {
	fprintf(stderr, "%s:  unable to open display \"%s\"\n",
		argv[0], XDisplayName(disp));
	exit(EXIT_FAILURE);
    }
    XSetErrorHandler(local_xerror);
    for (i = 1; i < argc;) {
	arg = argv[i++];
	if (strcmp(arg, "-display") == 0 || strcmp(arg, "-d") == 0) {
	    ++i;		       		/* already dealt with */
	    continue;
	} else if (*arg == '-' && *(arg + 1) == 'c') {	/* Does arg start
							   with "-c"? */
	    set_click(dpy, 0);	       		/* If so, turn click off */
	} else if (*arg == 'c') {      		/* Well, does it start
						   with "c", then? */
	    percent = SERVER_DEFAULT;  		/* Default click volume. */
	    if (i >= argc) {
		set_click(dpy, percent);	/* set click to default */
		break;
	    }
	    arg = nextarg(i, argv);
	    if (strcmp(arg, "on") == 0) {	/* Let click be default. */
		percent = DEFAULT_ON;
		i++;
	    } else if (strcmp(arg, "off") == 0) {
		percent = 0;	       		/* Turn it off.          */
		i++;
	    } else if (is_number(arg, 100)) {
		percent = atoi(arg);		/* Set to spec. volume */
		i++;
	    }
	    set_click(dpy, percent);
	} else if (strcmp(arg, "-b") == 0) {
	    set_bell_vol(dpy, 0);      		/* Then turn off bell.    */
	} else if (strcmp(arg, "b") == 0) {
	    percent = SERVER_DEFAULT;  		/* Set bell to default. */
	    if (i >= argc) {
		set_bell_vol(dpy, percent);	/* set bell to default */
		set_bell_pitch(dpy, percent);	/* set pitch to default */
		set_bell_dur(dpy, percent);	/* set duration to default */
		break;
	    }
	    arg = nextarg(i, argv);
	    if (strcmp(arg, "on") == 0) {	/* Let it stay that way.  */
		set_bell_vol(dpy, DEFAULT_ON);	/* set bell on */
		set_bell_pitch(dpy, percent);	/* set pitch to default */
		set_bell_dur(dpy, percent);	/* set duration to default */
		i++;
	    } else if (strcmp(arg, "off") == 0) {
		percent = 0;	       		/* Turn the bell off.     */
		set_bell_vol(dpy, percent);
		i++;
	    } else if (is_number(arg, 100)) {	/* If volume is given:    */
		percent = atoi(arg);   	       	/* set bell appropriately. */
		set_bell_vol(dpy, percent);
		i++;
		arg = nextarg(i, argv);

		if (is_number(arg, 32767)) {	/* If pitch is given:     */
		    set_bell_pitch(dpy, atoi(arg));	/* set the bell.  */
		    i++;

		    arg = nextarg(i, argv);
		    if (is_number(arg, 32767)) { /* If duration is given:  */
			set_bell_dur(dpy, atoi(arg));	/*  set the bell.  */
			i++;
		    }
		}
	    } else
		set_bell_vol(dpy, percent);	/* set bell to default */
	}
#ifdef MITMISC
	else if (strcmp(arg, "bc") == 0) {
	    int dummy;

	    if (XMITMiscQueryExtension(dpy, &dummy, &dummy))
		XMITMiscSetBugMode(dpy, True);
	    else
		fprintf(stderr,
			"server does not have extension for bc option\n");
	} else if (strcmp(arg, "-bc") == 0) {
	    int dummy;

	    if (XMITMiscQueryExtension(dpy, &dummy, &dummy))
		XMITMiscSetBugMode(dpy, False);
	    else
		fprintf(stderr,
			"server does not have extension for -bc option\n");
	}
#endif
#ifdef FONTCACHE
	else if (strcmp(arg, "fc") == 0) {
	    int dummy;
	    FontCacheSettings cs;

	    if (FontCacheQueryExtension(dpy, &dummy, &dummy)) {
		FontCacheGetCacheSettings(dpy, &cs);
		himark = cs.himark / 1024;
		lowmark = cs.lowmark / 1024;
		balance = cs.balance;
		if (i >= argc) {
		    /* Set to server's values, and clear all cache
		       in side effect */
		    set_font_cache(dpy, himark, lowmark, balance);
		    break;
		}
		arg = nextarg(i, argv);
		if (is_number(arg, 32767)) {	/* If hi-mark is given: */
		    himark = atoi(arg);
		    i++;
		    if (himark <= 0) {
			usage("hi-mark must be greater than 0", NULL);
		    }
		    if (i >= argc) {
			lowmark = (himark * 70) / 100;
			set_font_cache(dpy, himark, lowmark, balance);
			break;
		    }
		    arg = nextarg(i, argv);
		    if (is_number(arg, 32767)) { /* If low-mark is given: */
			lowmark = atoi(arg);
			i++;
			if (lowmark <= 0) {
			    usage("low-mark must be greater than 0", NULL);
			}
			if (himark <= lowmark) {
			    usage("hi-mark must be greater than low-mark",
				  NULL);
			}
			if (i >= argc) {
			    set_font_cache(dpy, himark, lowmark, balance);
			    break;
			}
			arg = nextarg(i, argv);
			if (is_number(arg, 90)) {
			    balance = atoi(arg);
			    i++;
			    if (!(10 <= balance && balance <= 90)) {
				usage("balance must be 10 to 90\n");
			    }
			    set_font_cache(dpy, himark, lowmark, balance);
			}
		    }
		} else if (strcmp(arg, "s") == 0
		    || strcmp(arg, "status") == 0) {
		    /* display cache status */
		    query_cache_status(dpy);
		}
	    } else {
		fprintf(stderr,
			"server does not have extension for fc option\n");
	    }
	}
#endif
	else if (strcmp(arg, "fp") == 0) {	/* set font path */
	    if (i >= argc) {
		arg = "default";
	    } else {
		arg = nextarg(i, argv);
	    }
	    set_font_path(dpy, arg, 1, 0, 0);	/* special argument */
	    i++;
	} else if (strcmp(arg, "fp=") == 0) {	/* unconditionally set */
	    if (i >= argc) {
		usage("missing fp= argument", NULL);
	    } else {
		arg = nextarg(i, argv);
	    }
	    set_font_path(dpy, arg, 0, 0, 0);	/* not special, set */
	    i++;
	} else if (strcmp(arg, "+fp") == 0) {	/* set font path */
	    if (i >= argc)
		usage("missing +fp argument", NULL);
	    arg = nextarg(i, argv);
	    set_font_path(dpy, arg, 0, 1, 0);	/* not special, prepend */
	    i++;
	} else if (strcmp(arg, "fp+") == 0) {	/* set font path */
	    if (i >= argc)
		usage("missing fp+ argument", NULL);
	    arg = nextarg(i, argv);
	    set_font_path(dpy, arg, 0, 0, 1);	/* not special, append */
	    i++;
	} else if (strcmp(arg, "-fp") == 0) {	/* set font path */
	    if (i >= argc)
		usage("missing -fp argument", NULL);
	    arg = nextarg(i, argv);
	    set_font_path(dpy, arg, 0, -1, 0);	/* not special, preremove */
	    i++;
	} else if (strcmp(arg, "fp-") == 0) {	/* set font path */
	    if (i >= argc)
		usage("missing fp- argument", NULL);
	    arg = nextarg(i, argv);
	    set_font_path(dpy, arg, 0, 0, -1);	/* not special, postremove */
	    i++;
	} else if (strcmp(arg, "-led") == 0) {	/* Turn off one or all LEDs */
	    values.led_mode = OFF;
	    values.led = ALL;	       		/* None specified */
	    if (i >= argc) {
		set_led(dpy, values.led, values.led_mode);
		break;
	    }
	    arg = nextarg(i, argv);
	    if (strcmp(arg, "named") == 0) {
		if (++i >= argc) {
		    usage("missing argument to led named", NULL);
		} else {
		    arg = nextarg(i, argv);
		    xkbset_led(dpy, arg, values.led_mode);
		}
		break;
	    }
	    if (is_number(arg, 32) && atoi(arg) > 0) {
		values.led = atoi(arg);
		i++;
	    }
	    set_led(dpy, values.led, values.led_mode);
	} else if (strcmp(arg, "led") == 0) {	/* Turn on one or all LEDs  */
	    values.led_mode = ON;
	    values.led = ALL;
	    if (i >= argc) {
		set_led(dpy, values.led,
			values.led_mode);	/* set led to def */
		break;
	    }
	    arg = nextarg(i, argv);
	    if (strcmp(arg, "named") == 0) {
		if (++i >= argc) {
		    usage("missing argument to -led named", NULL);
		} else {
		    arg = nextarg(i, argv);
		    xkbset_led(dpy, arg, values.led_mode);
		}
		break;
	    }
	    if (strcmp(arg, "on") == 0) {
		i++;
	    } else if (strcmp(arg, "off") == 0) { /* ...except in this case. */
		values.led_mode = OFF;
		i++;
	    } else if (is_number(arg, 32) && atoi(arg) > 0) {
		values.led = atoi(arg);
		i++;
	    }
	    set_led(dpy, values.led, values.led_mode);
	}
/*  Set pointer (mouse) settings:  Acceleration and Threshold. */
	else if (strcmp(arg, "m") == 0 || strcmp(arg, "mouse") == 0) {
	    acc_num = SERVER_DEFAULT;		/* restore server defaults */
	    acc_denom = SERVER_DEFAULT;
	    threshold = SERVER_DEFAULT;
	    if (i >= argc) {
		set_mouse(dpy, acc_num, acc_denom, threshold);
		break;
	    }
	    arg = argv[i];
	    if (strcmp(arg, "default") == 0) {
		i++;
	    } else if (*arg >= '0' && *arg <= '9') {
		acc_denom = 1;
		sscanf(arg, "%d/%d", &acc_num, &acc_denom);
		i++;
		if (i >= argc) {
		    set_mouse(dpy, acc_num, acc_denom, threshold);
		    break;
		}
		arg = argv[i];
		if (*arg >= '0' && *arg <= '9') {
		    threshold = atoi(arg); /* Set threshold as user specified. */
		    i++;
		}
	    }
	    set_mouse(dpy, acc_num, acc_denom, threshold);
	}
#ifdef DPMSExtension
	else if (strcmp(arg, "+dpms") == 0) {	/* turn on DPMS */
	    int dummy;

	    if (DPMSQueryExtension(dpy, &dummy, &dummy))
		DPMSEnable(dpy);
	    else
		fprintf(stderr,
		    "server does not have extension for +dpms option\n");
	} else if (strcmp(arg, "-dpms") == 0) {	/* shut off DPMS */
	    int dummy;

	    if (DPMSQueryExtension(dpy, &dummy, &dummy))
		DPMSDisable(dpy);
	    else
		fprintf(stderr,
		    "server does not have extension for -dpms option\n");

	} else if (strcmp(arg, "dpms") == 0) {	/* tune DPMS */
	    int dummy;

	    if (DPMSQueryExtension(dpy, &dummy, &dummy)) {
		DPMSGetTimeouts(dpy, &standby_timeout, &suspend_timeout,
		    &off_timeout);
		if (i >= argc) {
		    DPMSEnable(dpy);
		    break;
		}
		arg = argv[i];
		if (*arg >= '0' && *arg <= '9') {
		    sscanf(arg, "%hu", &standby_timeout);
		    i++;
		    arg = argv[i];
		    if ((arg) && (*arg >= '0' && *arg <= '9')) {
			sscanf(arg, "%hu", &suspend_timeout);
			i++;
			arg = argv[i];
			if ((arg) && (*arg >= '0' && *arg <= '9')) {
			    sscanf(arg, "%hu", &off_timeout);
			    i++;
			    arg = argv[i];
			}
		    }
		    if ((suspend_timeout != 0)
			&& (standby_timeout > suspend_timeout)) {
			fprintf(stderr, "illegal combination of values\n");
			fprintf(stderr,
			    "  standby time of %d is greater than suspend time of %d\n",
			    standby_timeout, suspend_timeout);
			exit(EXIT_FAILURE);
		    }
		    if ((off_timeout != 0) && (suspend_timeout > off_timeout)) {
			fprintf(stderr, "illegal combination of values\n");
			fprintf(stderr,
			    "  suspend time of %d is greater than off time of %d\n",
			    suspend_timeout, off_timeout);
			exit(EXIT_FAILURE);
		    }
		    if ((suspend_timeout == 0) && (off_timeout != 0) &&
			(standby_timeout > off_timeout)) {
			fprintf(stderr, "illegal combination of values\n");
			fprintf(stderr,
			    "  standby time of %d is greater than off time of %d\n",
			    standby_timeout, off_timeout);
			exit(EXIT_FAILURE);
		    }
		    DPMSEnable(dpy);
		    DPMSSetTimeouts(dpy, standby_timeout, suspend_timeout,
			off_timeout);
		} else if (i + 1 < argc && strcmp(arg, "force") == 0) {
		    i++;
		    arg = argv[i];
		    /*
		     * The calls to usleep below are necessary to
		     * delay the actual DPMS mode setting briefly.
		     * Without them, it's likely that the mode will be
		     * set between the Down and Up key transitions, in
		     * which case the Up transition may immediately
		     * turn the display back on.
		     *
		     * On OS/2, use _sleep2()
		     */

#ifdef HAVE_USLEEP
# define Usleep(us) usleep((us))
#else
#ifdef SVR4
# ifdef sun
/* Anything to avoid linking with -lposix4 */
#  define Usleep(us) { \
		struct ts { \
			long	tv_sec; \
			long	tv_nsec; \
		} req; \
		req.tv_sec = 0; \
		req.tv_nsec = (us) * 1000;\
		syscall(SYS_nanosleep, &req, NULL); \
	}
# endif
# ifdef sgi
#  define Usleep(us) sginap((us) / 1000)
# endif
#endif
#ifdef hpux
# ifdef _XPG4_EXTENDED
#  define Usleep(us) usleep((us))
# endif
#endif
#ifdef __UNIXOS2__
# define Usleep(us) _sleep2((us / 1000 > 0) ? us / 1000 : 1)
#endif
#ifdef WIN32
# define Usleep(us) Sleep(us)
#endif
#ifndef Usleep
# if defined(SYSV) || defined(SVR4)
#  define Usleep(us) sleep((us / 1000000 > 0) ? us / 1000000 : 1)
# else
#  define Usleep(us) usleep((us))
# endif
#endif
#endif /* HAVE_USLEEP */

		    if (strcmp(arg, "on") == 0) {
			DPMSEnable(dpy);
			DPMSForceLevel(dpy, DPMSModeOn);
			i++;
		    } else if (strcmp(arg, "standby") == 0) {
			DPMSEnable(dpy);
			Usleep(100000);
			DPMSForceLevel(dpy, DPMSModeStandby);
			i++;
		    } else if (strcmp(arg, "suspend") == 0) {
			DPMSEnable(dpy);
			Usleep(100000);
			DPMSForceLevel(dpy, DPMSModeSuspend);
			i++;
		    } else if (strcmp(arg, "off") == 0) {
			DPMSEnable(dpy);
			Usleep(100000);
			DPMSForceLevel(dpy, DPMSModeOff);
			i++;
		    } else {
			fprintf(stderr, "bad parameter %s\n", arg);
			i++;
		    }
		}
	    } else {
		fprintf(stderr,
		    "server does not have extension for dpms option\n");
	    }
	}
#endif /* DPMSExtension */
	else if (strcmp(arg, "s") == 0) {
	    if (i >= argc) {
		set_saver(dpy, ALL, 0);	/* Set everything to default  */
		break;
	    }
	    arg = argv[i];
	    if (strcmp(arg, "blank") == 0) { /* Alter blanking preference. */
		set_saver(dpy, PREFER_BLANK, PreferBlanking);
		i++;
	    } else if (strcmp(arg, "noblank") == 0) {	/*  Ditto.  */
		set_saver(dpy, PREFER_BLANK, DontPreferBlanking);
		i++;
	    } else if (strcmp(arg, "expose") == 0) {	/* Alter exposure preference. */
		set_saver(dpy, ALLOW_EXP, AllowExposures);
		i++;
	    } else if (strcmp(arg, "noexpose") == 0) {	/*  Ditto.  */
		set_saver(dpy, ALLOW_EXP, DontAllowExposures);
		i++;
	    } else if (strcmp(arg, "off") == 0) {
		set_saver(dpy, TIMEOUT, 0);	/*  Turn off screen saver.  */
		i++;
		if (i >= argc)
		    break;
		arg = argv[i];
		if (strcmp(arg, "off") == 0) {
		    set_saver(dpy, INTERVAL, 0);
		    i++;
		}
	    } else if (strcmp(arg, "default") == 0) {	/* Leave as default. */
		set_saver(dpy, ALL, SERVER_DEFAULT);
		i++;
	    } else if (strcmp(arg, "on") == 0) {	/* Turn on.       */
		set_saver(dpy, ALL, DEFAULT_TIMEOUT);
		i++;
	    } else if (strcmp(arg, "activate") == 0) {	/* force it active */
		XActivateScreenSaver(dpy);
		i++;
	    } else if (strcmp(arg, "reset") == 0) {	/* force it inactive */
		XResetScreenSaver(dpy);
		i++;
	    } else if (*arg >= '0' && *arg <= '9') {	/* Set as user wishes. */
		set_saver(dpy, TIMEOUT, atoi(arg));
		i++;
		if (i >= argc)
		    break;
		arg = argv[i];
		if (*arg >= '0' && *arg <= '9') {
		    set_saver(dpy, INTERVAL, atoi(arg));
		    i++;
		}
	    }
	} else if (strcmp(arg, "-r") == 0) {		/* Turn off one or
							   all autorepeats */
	    auto_repeat_mode = OFF;
	    key = ALL;		       			/* None specified */
	    arg = argv[i];
	    if (i < argc)
		if (is_number(arg, 255)) {
		    key = atoi(arg);
		    i++;
		}
	    set_repeat(dpy, key, auto_repeat_mode);
	} else if (strcmp(arg, "r") == 0) {		/* Turn on one or
							   all autorepeats */
	    auto_repeat_mode = ON;
	    key = ALL;		       			/* None specified */
	    arg = argv[i];
	    if (i < argc) {
		if (strcmp(arg, "on") == 0) {
		    i++;
		} else if (strcmp(arg, "off") == 0) {	/*  ...except in
							    this case */
		    auto_repeat_mode = OFF;
		    i++;
		}
#if defined(XF86MISC) || defined(XKB)
		else if (strcmp(arg, "rate") == 0) {	/*  ...or this one. */
		    int delay = 0, rate = 0;

#ifdef XF86MISC
		    int rate_set = 0;
#endif

#ifdef XKB
		    if (XkbQueryExtension(dpy, &xkbopcode, &xkbevent,
					  &xkberror, &xkbmajor, &xkbminor)) {
			delay = XKBDDELAY_DEFAULT;
			rate = XKBDRATE_DEFAULT;
			xkbpresent = 1;
		    }
#endif
#ifdef XF86MISC
		    if (!xkbpresent) {
			int dummy;

			if (XF86MiscQueryExtension(dpy, &dummy, &dummy)) {
			    delay = KBDDELAY_DEFAULT;
			    rate = KBDRATE_DEFAULT;
			    miscpresent = 1;
			}
		    }
#endif
		    if (!xkbpresent && !miscpresent)
			fprintf(stderr,
				"server does not have extension for \"r rate\" option\n");
		    i++;
		    arg = argv[i];
		    if (i < argc) {
			if (is_number(arg, 10000) && atoi(arg) > 0) {
			    delay = atoi(arg);
			    i++;
			    arg = argv[i];
			    if (i < argc) {
				if (is_number(arg, 255) && atoi(arg) > 0) {
				    rate = atoi(arg);
				    i++;
				}
			    }
			}
		    }
#ifdef XKB
		    if (xkbpresent) {
			xkbset_repeatrate(dpy, delay, 1000 / rate);
#ifdef XF86MISC
			rate_set = 1;
#endif
		    }
#endif
#ifdef XF86MISC
		    if (miscpresent && !rate_set) {
			set_repeatrate(dpy, delay, rate);
		    }
#endif
		}
#endif
		else if (is_number(arg, 255)) {
		    key = atoi(arg);
		    i++;
		}
	    }
	    set_repeat(dpy, key, auto_repeat_mode);
	} else if (strcmp(arg, "p") == 0) {
	    if (i + 1 >= argc)
		usage("missing argument to p", NULL);
	    arg = argv[i];
	    if (numpixels >= MAX_PIXEL_COUNT)
		usage("more than %d pixels specified", MAX_PIXEL_COUNT);
	    if (*arg >= '0' && *arg <= '9')
		pixels[numpixels] = atoi(arg);
	    else
		usage("invalid pixel number %s", arg);
	    i++;
	    colors[numpixels] = argv[i];
	    i++;
	    numpixels++;
	} else if (strcmp(arg, "-k") == 0) {
	    set_lock(dpy, OFF);
	} else if (strcmp(arg, "k") == 0) {
	    set_lock(dpy, ON);
	} else if (strcmp(arg, "q") == 0 || strcmp(arg, "-q") == 0) {
	    query(dpy);
	} else
	    usage("unknown option %s", arg);
    }

    if (numpixels)
	set_pixels(dpy, pixels, colors, numpixels);

    XCloseDisplay(dpy);

    exit(error_status);		       /*  Done.  We can go home now.  */
}

static int
is_number(char *arg, int maximum)
{
    register char *p;

    if (arg[0] == '-' && arg[1] == '1' && arg[2] == '\0')
	return (1);
    for (p = arg; isdigit(*p); p++) ;
    if (*p || atoi(arg) > maximum)
	return (0);
    return (1);
}

/*  These next few functions do the real work (xsetting things).
 */
static void
set_click(Display *dpy, int percent)
{
    XKeyboardControl values;
    XKeyboardState kbstate;

    values.key_click_percent = percent;
    if (percent == DEFAULT_ON)
	values.key_click_percent = SERVER_DEFAULT;
    XChangeKeyboardControl(dpy, KBKeyClickPercent, &values);
    if (percent == DEFAULT_ON) {
	XGetKeyboardControl(dpy, &kbstate);
	if (!kbstate.key_click_percent) {
	    values.key_click_percent = -percent;
	    XChangeKeyboardControl(dpy, KBKeyClickPercent, &values);
	}
    }
    return;
}

static void
set_bell_vol(Display *dpy, int percent)
{
    XKeyboardControl values;
    XKeyboardState kbstate;

    values.bell_percent = percent;
    if (percent == DEFAULT_ON)
	values.bell_percent = SERVER_DEFAULT;
    XChangeKeyboardControl(dpy, KBBellPercent, &values);
    if (percent == DEFAULT_ON) {
	XGetKeyboardControl(dpy, &kbstate);
	if (!kbstate.bell_percent) {
	    values.bell_percent = -percent;
	    XChangeKeyboardControl(dpy, KBBellPercent, &values);
	}
    }
    return;
}

static void
set_bell_pitch(Display *dpy, int pitch)
{
    XKeyboardControl values;

    values.bell_pitch = pitch;
    XChangeKeyboardControl(dpy, KBBellPitch, &values);
    return;
}

static void
set_bell_dur(Display *dpy, int duration)
{
    XKeyboardControl values;

    values.bell_duration = duration;
    XChangeKeyboardControl(dpy, KBBellDuration, &values);
    return;
}

/*
 * Set, add, or subtract the path according to before and after flags:
 *
 *	before	after	action
 *
 *	   0      0	FontPath := path
 *	  -1      0	FontPath := current - path
 *	   0     -1	FontPath := current - path
 *	   1      0	FontPath := path + current
 *	   0      1	FontPath := current + path
 */
static void
set_font_path(Display *dpy, char *path, int special, int before, int after)
{
    char **directoryList = NULL;
    int ndirs = 0;
    char **currentList = NULL;
    int ncurrent = 0;

    if (special) {
	if (strcmp(path, "default") == 0) {
	    XSetFontPath(dpy, NULL, 0);
	    return;
	}
	if (strcmp(path, "rehash") == 0) {
	    currentList = XGetFontPath(dpy, &ncurrent);
	    if (!currentList) {
		fprintf(stderr, "%s:  unable to get current font path.\n",
			progName);
		return;
	    }
	    XSetFontPath(dpy, currentList, ncurrent);
	    XFreeFontPath(currentList);
	    return;
	}
	/*
	 * for now, fall though and process keyword and directory list for
	 * compatibility with previous versions.
	 */
    }

    /*
     * parse the path list.  If before or after is non-zero, we'll need
     * the current value.
     */

    if (before != 0 || after != 0) {
	currentList = XGetFontPath(dpy, &ncurrent);
	if (!currentList) {
	    fprintf(stderr, "%s:  unable to get old font path.\n", progName);
	    before = after = 0;
	}
    }

    {
	/* count the number of directories in path */
	register char *cp = path;

	ndirs = 1;
	while ((cp = strchr(cp, ',')) != NULL) {
	    ndirs++;
	    cp++;
	}
    }

    directoryList = (char **)malloc(ndirs * sizeof(char *));
    if (!directoryList)
	error("out of memory for font path directory list");

    {
	/* mung the path and set directoryList pointers */
	int i = 0;
	char *cp = path;

	directoryList[i++] = cp;
	while ((cp = strchr(cp, ',')) != NULL) {
	    directoryList[i++] = cp + 1;
	    *cp++ = '\0';
	}
	if (i != ndirs) {
	    fprintf(stderr,
		"%s: internal error, only parsed %d of %d directories.\n",
		progName, i, ndirs);
	    exit(EXIT_FAILURE);
	}
    }

    /*
     * now we have have parsed the input path, so we can set it
     */

    if (before == 0 && after == 0) {
	XSetFontPath(dpy, directoryList, ndirs);
    }

    /* if adding to list, build a superset */
    if (before > 0 || after > 0) {
	int nnew = ndirs + ncurrent;
	char **newList = (char **)malloc(nnew * sizeof(char *));

	if (!newList)
	    error("out of memory");
	if (before > 0) {	       /* new + current */
	    memmove((char *)newList, (char *)directoryList,
		    (unsigned)(ndirs * sizeof(char *)));
	    memmove((char *)(newList + ndirs), (char *)currentList,
		    (unsigned)(ncurrent * sizeof(char *)));
	    XSetFontPath(dpy, newList, nnew);
	} else if (after > 0) {
	    memmove((char *)newList, (char *)currentList,
		    (unsigned)(ncurrent * sizeof(char *)));
	    memmove((char *)(newList + ncurrent), (char *)directoryList,
		    (unsigned)(ndirs * sizeof(char *)));
	    XSetFontPath(dpy, newList, nnew);
	}
	free((char *)newList);
    }

    /* if deleting from list, build one the same size */
    if (before < 0 || after < 0) {
	int i, j;
	int nnew = 0;
	char **newList = (char **)malloc(ncurrent * sizeof(char *));

	if (!newList)
	    error("out of memory");
	for (i = 0; i < ncurrent; i++) {
	    for (j = 0; j < ndirs; j++) {
		if (strcmp(currentList[i], directoryList[j]) == 0)
		    break;
	    }
	    /* if we ran out, then insert into new list */
	    if (j == ndirs)
		newList[nnew++] = currentList[i];
	}
	if (nnew == ncurrent) {
	    fprintf(stderr,
		    "%s:  warning, no entries deleted from font path.\n",
		    progName);
	}
	XSetFontPath(dpy, newList, nnew);
	free((char *)newList);
    }

    if (directoryList)
	free((char *)directoryList);
    if (currentList)
	XFreeFontPath(currentList);

    return;
}

static void
set_led(Display *dpy, int led, int led_mode)
{
    XKeyboardControl values;

    values.led_mode = led_mode;
    if (led != ALL) {
	values.led = led;
	XChangeKeyboardControl(dpy, KBLed | KBLedMode, &values);
    } else {
	XChangeKeyboardControl(dpy, KBLedMode, &values);
    }
    return;
}

static void
xkbset_led(Display *dpy, const char *led, int led_mode)
{
#ifndef XKB
    error("  xset was not built with XKB Extension support\n");
#else
    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
    Atom ledatom;

    if (XkbQueryExtension(dpy, &xkbopcode, &xkbevent, &xkberror,
			  &xkbmajor, &xkbminor)) {
	ledatom = XInternAtom(dpy, led, True);
	if ((ledatom != None) &&
	    XkbGetNamedIndicator(dpy, ledatom, NULL, NULL, NULL, NULL)) {
	    if (XkbSetNamedIndicator(dpy, ledatom, True,
				     led_mode, False, NULL) == False) {
		printf("Failed to set led named %s %s\n",
		       led, led_mode ? "on" : "off");
	    }
	} else {
	    fprintf(stderr,"%s: Invalid led name: %s\n", progName, led);
	}
    } else {
	printf("  Server does not have the XKB Extension\n");
    }
#endif
    return;
}

static void
set_mouse(Display *dpy, int acc_num, int acc_denom, int threshold)
{
    int do_accel = True, do_threshold = True;

    if (acc_num == DONT_CHANGE)	       /* what an incredible crock... */
	do_accel = False;
    if (threshold == DONT_CHANGE)
	do_threshold = False;
    if (acc_num < 0)		       /* shouldn't happen */
	acc_num = SERVER_DEFAULT;
    if (acc_denom <= 0)		       /* prevent divide by zero */
	acc_denom = SERVER_DEFAULT;
    if (threshold < 0)
	threshold = SERVER_DEFAULT;
    XChangePointerControl(dpy, do_accel, do_threshold, acc_num,
			  acc_denom, threshold);
    return;
}

static void
set_saver(Display *dpy, int mask, int value)
{
    int timeout, interval, prefer_blank, allow_exp;

    XGetScreenSaver(dpy, &timeout, &interval, &prefer_blank, &allow_exp);
    if (mask == TIMEOUT)
	timeout = value;
    if (mask == INTERVAL)
	interval = value;
    if (mask == PREFER_BLANK)
	prefer_blank = value;
    if (mask == ALLOW_EXP)
	allow_exp = value;
    if (mask == ALL) {
	timeout = SERVER_DEFAULT;
	interval = SERVER_DEFAULT;
	prefer_blank = DefaultBlanking;
	allow_exp = DefaultExposures;
    }
    XSetScreenSaver(dpy, timeout, interval, prefer_blank, allow_exp);
    if (mask == ALL && value == DEFAULT_TIMEOUT) {
	XGetScreenSaver(dpy, &timeout, &interval, &prefer_blank, &allow_exp);
	if (!timeout)
	    XSetScreenSaver(dpy, -DEFAULT_TIMEOUT, interval, prefer_blank,
			    allow_exp);
    }
    return;
}

static void
set_repeat(Display *dpy, int key, int auto_repeat_mode)
{
    XKeyboardControl values;

    values.auto_repeat_mode = auto_repeat_mode;
    if (key != ALL) {
	values.key = key;
	XChangeKeyboardControl(dpy, KBKey | KBAutoRepeatMode, &values);
    } else {
	XChangeKeyboardControl(dpy, KBAutoRepeatMode, &values);
    }
    return;
}

#ifdef XF86MISC
static void
set_repeatrate(Display *dpy, int delay, int rate)
{
    XF86MiscKbdSettings values;

    XF86MiscGetKbdSettings(dpy, &values);
    values.delay = delay;
    values.rate = rate;
    XF86MiscSetKbdSettings(dpy, &values);
    return;
}
#endif

#ifdef XKB
static void
xkbset_repeatrate(Display *dpy, int delay, int interval)
{
    XkbDescPtr xkb = XkbAllocKeyboard();

    if (!xkb)
	return;
    XkbGetControls(dpy, XkbRepeatKeysMask, xkb);
    xkb->ctrls->repeat_delay = delay;
    xkb->ctrls->repeat_interval = interval;
    XkbSetControls(dpy, XkbRepeatKeysMask, xkb);
    XkbFreeKeyboard(xkb, 0, True);
}
#endif

static void
set_pixels(Display *dpy, unsigned long *pixels, caddr_t * colors,
    int numpixels)
{
    XColor def;
    int scr = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, scr);
    Colormap cmap = DefaultColormap(dpy, scr);
    unsigned long max_cells = DisplayCells(dpy, scr);
    XVisualInfo viproto, *vip;
    int nvisuals = 0;
    char *visual_type = NULL;
    int i;

    viproto.visualid = XVisualIDFromVisual(visual);
    vip = XGetVisualInfo(dpy, VisualIDMask, &viproto, &nvisuals);
    if (!vip) {
	fprintf(stderr, "%s: Can't get visual for visualID 0x%x\n",
		progName, (unsigned int)viproto.visualid);
	return;
    }

    switch (vip->class) {
    case GrayScale:
    case PseudoColor:
	break;
    case TrueColor:
	visual_type = "TrueColor";
	/* fall through */
    case DirectColor:
	max_cells *= max_cells * max_cells;
	break;
    case StaticGray:
	visual_type = "StaticGray";
	break;
    case StaticColor:
	visual_type = "StaticColor";
	break;
    default:
	fprintf(stderr, "%s:  unknown visual class type %d\n",
		progName, vip->class);
	numpixels = 0;
    }

    if (visual_type) {
	fprintf(stderr,
		"%s:  cannot set pixel values in read-only %s visuals\n",
		progName, visual_type);
    } else {
	for (i = 0; i < numpixels; i++) {
	    def.pixel = pixels[i];
	    if (def.pixel >= max_cells)
		fprintf(stderr,
			"%s:  pixel value %ld out of colormap range 0 through %ld\n",
			progName, def.pixel, max_cells - 1);
	    else {
		if (XParseColor(dpy, cmap, colors[i], &def))
		    XStoreColor(dpy, cmap, &def);
		else
		    fprintf(stderr, "%s:  invalid color \"%s\"\n", progName,
			    colors[i]);
	    }
	}
    }

    XFree((char *)vip);

    return;
}

static void
set_lock(Display *dpy, Bool onoff)
{
    XModifierKeymap *mods;

    mods = XGetModifierMapping(dpy);

    if (onoff)
	mods =
	    XInsertModifiermapEntry(mods, (KeyCode) XK_Caps_Lock,
				    LockMapIndex);
    else
	mods =
	    XDeleteModifiermapEntry(mods, (KeyCode) XK_Caps_Lock,
				    LockMapIndex);
    XSetModifierMapping(dpy, mods);
    XFreeModifiermap(mods);
    return;
}

#ifdef FONTCACHE
static Status
set_font_cache(Display *dpy, long himark, long lowmark, long balance)
{
    FontCacheSettings cs;
    Status status;

    cs.himark = himark * 1024;
    cs.lowmark = lowmark * 1024;
    cs.balance = balance;
    status = FontCacheChangeCacheSettings(dpy, &cs);

    return status;
}
#endif

static char *
on_or_off(int val, int onval, char *onstr,
    int offval, char *offstr, char buf[])
{
    if (val == onval)
	return onstr;
    else if (val == offval)
	return offstr;

    buf[0] = '\0';
    sprintf(buf, "<%d>", val);
    return buf;
}

/*  This is the information-getting function for telling the user what the
 *  current "xsettings" are.
 */
static void
query(Display *dpy)
{
    int scr = DefaultScreen(dpy);
    XKeyboardState values;
    int acc_num, acc_denom, threshold;
    int timeout, interval, prefer_blank, allow_exp;

#ifdef XF86MISC
    XF86MiscKbdSettings kbdinfo;
#endif
#ifdef XKB
    XkbDescPtr xkb;
    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
#endif
    char **font_path;
    int npaths;
    int i, j;
    char buf[20];		       /* big enough for 16 bit number */

    XGetKeyboardControl(dpy, &values);
    XGetPointerControl(dpy, &acc_num, &acc_denom, &threshold);
    XGetScreenSaver(dpy, &timeout, &interval, &prefer_blank, &allow_exp);
    font_path = XGetFontPath(dpy, &npaths);

    printf("Keyboard Control:\n");
    printf
	("  auto repeat:  %s    key click percent:  %d    LED mask:  %08lx\n",
	on_or_off(values.global_auto_repeat, AutoRepeatModeOn, "on",
	    AutoRepeatModeOff, "off", buf), values.key_click_percent,
	values.led_mask);
#ifdef XKB
    if (XkbQueryExtension(dpy, &xkbopcode, &xkbevent, &xkberror, &xkbmajor,
			  &xkbminor)
	&& (xkb = XkbAllocKeyboard()) != NULL) {
	if (XkbGetNames(dpy, XkbIndicatorNamesMask, xkb) == Success) {
	    Atom iatoms[XkbNumIndicators];
	    char *iatomnames[XkbNumIndicators];
	    Bool istates[XkbNumIndicators];
	    int inds[XkbNumIndicators];
	    int activecount = 0;
	    int maxnamelen = 0;
	    int columnwidth;
	    int linewidth;

	    printf("  XKB indicators:\n");

	    for (i = 0, j = 0; i < XkbNumIndicators; i++) {
		if (xkb->names->indicators[i] != None) {
		    iatoms[j++] =  xkb->names->indicators[i];
		}
	    }

	    if (XGetAtomNames(dpy, iatoms, j, iatomnames)) {
		for (i = 0; i < j; i++) {
		    if (XkbGetNamedIndicator(dpy, iatoms[i], &inds[i],
					     &istates[i], NULL, NULL)) {
			int namelen = strlen(iatomnames[i]);
			if (namelen > maxnamelen) {
			    maxnamelen = namelen;
			}
			activecount++;
		    } else {
			inds[i] = -1;
		    }
		}
	    }

	    if (activecount == 0) {
		printf("    None\n");
	    } else {

#define XKB_IND_FORMAT_CHARS 13 /* size of other chars in '    DD: X: off' */
#define MAX_LINE_WIDTH	     76

		columnwidth = maxnamelen + XKB_IND_FORMAT_CHARS;
		if (columnwidth > MAX_LINE_WIDTH) {
		    columnwidth = MAX_LINE_WIDTH;
		}

		for (i = 0, linewidth = 0; i < activecount ; i++) {
		    if (inds[i] != -1) {
			int spaces = columnwidth - XKB_IND_FORMAT_CHARS
			    - strlen(iatomnames[i]);

			if (spaces < 0)
			    spaces = 0;

			linewidth += printf("    %02d: %s: %*s",
					    inds[i], iatomnames[i],
					    spaces + 3,
					    on_or_off(istates[i],
						      True,  "on ",
						      False, "off", buf));
		    }
		    if (linewidth > (MAX_LINE_WIDTH - columnwidth)) {
			printf("\n");
			linewidth = 0;
		    }
		}
		if (linewidth > 0) {
		    printf("\n");
		}
	    }
	}
	if (XkbGetControls(dpy, XkbRepeatKeysMask, xkb) == Success) {
	    printf("  auto repeat delay:  %d    repeat rate:  %d\n",
		   xkb->ctrls->repeat_delay,
		   1000 / xkb->ctrls->repeat_interval);
	}
    }
#ifdef XF86MISC
    else
#endif
#endif
#ifdef XF86MISC
    {
	int dummy;

	if (XF86MiscQueryExtension(dpy, &dummy, &dummy) &&
	    XF86MiscGetKbdSettings(dpy, &kbdinfo))
	    printf("  auto repeat delay:  %d    repeat rate:  %d\n",
	           kbdinfo.delay, kbdinfo.rate);
    }
#endif
    printf("  auto repeating keys:  ");
    for (i = 0; i < 4; i++) {
	if (i)
	    printf("                        ");
	for (j = 0; j < 8; j++) {
	    printf("%02x", (unsigned char)values.auto_repeats[i * 8 + j]);
	}
	printf("\n");
    }
    printf("  bell percent:  %d    bell pitch:  %d    bell duration:  %d\n",
	   values.bell_percent, values.bell_pitch, values.bell_duration);

    printf("Pointer Control:\n");
    printf("  acceleration:  %d/%d    threshold:  %d\n",
	   acc_num, acc_denom, threshold);

    printf("Screen Saver:\n");
    printf("  prefer blanking:  %s    ",
	   on_or_off(prefer_blank, PreferBlanking, "yes",
		     DontPreferBlanking, "no", buf));
    printf("allow exposures:  %s\n",
	   on_or_off(allow_exp, AllowExposures, "yes",
		     DontAllowExposures, "no", buf));
    printf("  timeout:  %d    cycle:  %d\n", timeout, interval);

    printf("Colors:\n");
    printf("  default colormap:  0x%lx    BlackPixel:  %ld    WhitePixel:  %ld\n",
	   DefaultColormap(dpy, scr), BlackPixel(dpy, scr), WhitePixel(dpy,
								       scr));

    printf("Font Path:\n");
    if (npaths) {
	printf("  %s", *font_path++);
	for (--npaths; npaths; npaths--)
	    printf(",%s", *font_path++);
	printf("\n");
    } else {
	printf("  (empty)\n");
    }

#ifdef MITMISC
    {
	int dummy;

	if (XMITMiscQueryExtension(dpy, &dummy, &dummy)) {
	    if (XMITMiscGetBugMode(dpy))
		printf("Bug Mode: compatibility mode is enabled\n");
	    else
		printf("Bug Mode: compatibility mode is disabled\n");
	}
    }
#endif
#ifdef DPMSExtension
    {

	int dummy;
	CARD16 standby, suspend, off;
	BOOL onoff;
	CARD16 state;

	printf("DPMS (Energy Star):\n");
	if (DPMSQueryExtension(dpy, &dummy, &dummy)) {
	    if (DPMSCapable(dpy)) {
		DPMSGetTimeouts(dpy, &standby, &suspend, &off);
		printf("  Standby: %d    Suspend: %d    Off: %d\n",
		       standby, suspend, off);
		DPMSInfo(dpy, &state, &onoff);
		if (onoff) {
		    printf("  DPMS is Enabled\n");
		    switch (state) {
		    case DPMSModeOn:
			printf("  Monitor is On\n");
			break;
		    case DPMSModeStandby:
			printf("  Monitor is in Standby\n");
			break;
		    case DPMSModeSuspend:
			printf("  Monitor is in Suspend\n");
			break;
		    case DPMSModeOff:
			printf("  Monitor is Off\n");
			break;
		    default:
			printf("  Unrecognized response from server\n");
		    }
		} else
		    printf("  DPMS is Disabled\n");
	    } else
		printf("  Display is not capable of DPMS\n");
	} else {
	    printf("  Server does not have the DPMS Extension\n");
	}
    }
#endif
#ifdef FONTCACHE
    {
	int dummy;
	FontCacheSettings cs;
	int himark, lowmark, balance;

	printf("Font cache:\n");
	if (FontCacheQueryExtension(dpy, &dummy, &dummy)) {
	    if (FontCacheGetCacheSettings(dpy, &cs)) {
		himark = cs.himark / 1024;
		lowmark = cs.lowmark / 1024;
		balance = cs.balance;
		printf("  hi-mark (KB): %d  low-mark (KB): %d  balance (%%): %d\n",
		       himark, lowmark, balance);
	    }
	} else {
	    printf("  Server does not have the FontCache Extension\n");
	}
    }
#endif
#ifdef XF86MISC
    {
	int dummy;
	int maj, min;
	XF86MiscFilePaths paths;

	if (XF86MiscQueryExtension(dpy, &dummy, &dummy) &&
	    XF86MiscQueryVersion(dpy, &maj, &min) &&
	    ((maj > 0) || (maj == 0 && min >= 7)) &&
	    XF86MiscGetFilePaths(dpy, &paths)) {
	    printf("File paths:\n");
	    printf("  Config file:  %s\n", paths.configfile);
	    printf("  Modules path: %s\n", paths.modulepath);
	    printf("  Log file:     %s\n", paths.logfile);
	}
    }
#endif

    return;
}

#ifdef FONTCACHE
/*
 *  query_cache_status()
 *
 *  This is the information-getting function for telling the user what the
 *  current settings and statistics are.
 */
static void
query_cache_status(Display *dpy)
{
    int dummy;
    FontCacheSettings cs;
    FontCacheStatistics cstats;
    int himark, lowmark, balance;

    if (FontCacheQueryExtension(dpy, &dummy, &dummy)) {
	if (FontCacheGetCacheSettings(dpy, &cs)) {
	    printf("font cache settings:\n");
	    himark = cs.himark / 1024;
	    lowmark = cs.lowmark / 1024;
	    balance = cs.balance;
	    printf("  hi-mark (KB): %d  low-mark (KB): %d  balance (%%): %d\n",
		   himark, lowmark, balance);
	}
	if (FontCacheGetCacheStatistics(dpy, &cstats)) {
	    printf("font cache statistics:\n");
	    printf("   cache purged: %ld\n", cstats.purge_runs);
	    printf("   cache status: %ld\n", cstats.purge_stat);
	    printf("  cache balance: %ld\n", cstats.balance);
	    printf("font cache entry statistics:\n");
	    printf("      hits: %ld\n", cstats.f.hits);
	    printf("  misshits: %ld\n", cstats.f.misshits);
	    printf("    purged: %ld\n", cstats.f.purged);
	    printf("     usage: %ld\n", cstats.f.usage);
	    printf("large bitmap cache entry statistics:\n");
	    printf("      hits: %ld\n", cstats.v.hits);
	    printf("  misshits: %ld\n", cstats.v.misshits);
	    printf("    purged: %ld\n", cstats.v.purged);
	    printf("     usage: %ld\n", cstats.v.usage);
	}
    } else {
	printf("Server does not have the FontCache Extension\n");
    }
}
#endif

/*  This is the usage function */

static void
usage(char *fmt, ...)
{
    va_list ap;

    if (fmt) {
	fprintf(stderr, "%s:  ", progName);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n\n");

    }

    fprintf(stderr, "usage:  %s [-display host:dpy] option ...\n", progName);
    fprintf(stderr, "    To turn bell off:\n");
    fprintf(stderr, "\t-b                b off               b 0\n");
    fprintf(stderr, "    To set bell volume, pitch and duration:\n");
    fprintf(stderr, "\t b [vol [pitch [dur]]]          b on\n");
#ifdef MITMISC
    fprintf(stderr, "    To disable bug compatibility mode:\n");
    fprintf(stderr, "\t-bc\n");
    fprintf(stderr, "    To enable bug compatibility mode:\n");
    fprintf(stderr, "\tbc\n");
#endif
    fprintf(stderr, "    To turn keyclick off:\n");
    fprintf(stderr, "\t-c                c off               c 0\n");
    fprintf(stderr, "    To set keyclick volume:\n");
    fprintf(stderr, "\t c [0-100]        c on\n");
#ifdef DPMSExtension
    fprintf(stderr, "    To control Energy Star (DPMS) features:\n");
    fprintf(stderr, "\t-dpms      Energy Star features off\n");
    fprintf(stderr, "\t+dpms      Energy Star features on\n");
    fprintf(stderr, "\t dpms [standby [suspend [off]]]     \n");
    fprintf(stderr, "\t      force standby \n");
    fprintf(stderr, "\t      force suspend \n");
    fprintf(stderr, "\t      force off \n");
    fprintf(stderr, "\t      force on \n");
    fprintf(stderr, "\t      (also implicitly enables DPMS features) \n");
    fprintf(stderr, "\t      a timeout value of zero disables the mode \n");
#endif
#ifdef FONTCACHE
    fprintf(stderr, "    To control font cache:\n");
    fprintf(stderr, "\t fc [hi-mark [low-mark [balance]]]\n");
    fprintf(stderr, "\t    both mark values specified in KB\n");
    fprintf(stderr, "\t    balance value specified in percent (10 - 90)\n");
    fprintf(stderr, "    Show font cache statistics:\n");
    fprintf(stderr, "\t fc s\n");
#endif
    fprintf(stderr, "    To set the font path:\n");
    fprintf(stderr, "\t fp= path[,path...]\n");
    fprintf(stderr, "    To restore the default font path:\n");
    fprintf(stderr, "\t fp default\n");
    fprintf(stderr, "    To have the server reread font databases:\n");
    fprintf(stderr, "\t fp rehash\n");
    fprintf(stderr, "    To remove elements from font path:\n");
    fprintf(stderr, "\t-fp path[,path...]  fp- path[,path...]\n");
    fprintf(stderr, "    To prepend or append elements to font path:\n");
    fprintf(stderr, "\t+fp path[,path...]  fp+ path[,path...]\n");
    fprintf(stderr, "    To set LED states off or on:\n");
    fprintf(stderr, "\t-led [1-32]         led off\n");
    fprintf(stderr, "\t led [1-32]         led on\n");
#ifdef XKB
    fprintf(stderr, "\t-led named 'name'   led off\n");
    fprintf(stderr, "\t led named 'name'   led on\n");
#endif
    fprintf(stderr, "    To set mouse acceleration and threshold:\n");
    fprintf(stderr, "\t m [acc_mult[/acc_div] [thr]]    m default\n");
    fprintf(stderr, "    To set pixel colors:\n");
    fprintf(stderr, "\t p pixel_value color_name\n");
    fprintf(stderr, "    To turn auto-repeat off or on:\n");
    fprintf(stderr, "\t-r [keycode]        r off\n");
    fprintf(stderr, "\t r [keycode]        r on\n");
#if defined(XF86MISC) || defined(XKB)
    fprintf(stderr, "\t r rate [delay [rate]]\n");
#endif
    fprintf(stderr, "    For screen-saver control:\n");
    fprintf(stderr, "\t s [timeout [cycle]]  s default    s on\n");
    fprintf(stderr, "\t s blank              s noblank    s off\n");
    fprintf(stderr, "\t s expose             s noexpose\n");
    fprintf(stderr, "\t s activate           s reset\n");
    fprintf(stderr, "    For status information:  q\n");
    exit(EXIT_SUCCESS);
}

static void
error(char *message)
{
    fprintf(stderr, "%s: %s\n", progName, message);
    exit(EXIT_FAILURE);
}

static int
local_xerror(Display *dpy, XErrorEvent *rep)
{
    if (rep->request_code == X_SetFontPath && rep->error_code == BadValue) {
	fprintf(stderr,
		"%s:  bad font path element (#%ld), possible causes are:\n",
		progName, rep->resourceid);
	fprintf(stderr,
		"    Directory does not exist or has wrong permissions\n");
	fprintf(stderr, "    Directory missing fonts.dir\n");
	fprintf(stderr, "    Incorrect font server address or syntax\n");
    } else if (rep->request_code == X_StoreColors) {
	switch (rep->error_code) {
	  case BadAccess:
	    fprintf(stderr,
		    "%s:  pixel not allocated read/write\n", progName);
	    break;
	  case BadValue:
	    fprintf(stderr,
		    "%s:  cannot store in pixel 0x%lx, invalid pixel number\n",
		    progName, rep->resourceid);
	    break;
	  default:
	    XmuPrintDefaultErrorMessage(dpy, rep, stderr);
	}
    } else
	XmuPrintDefaultErrorMessage(dpy, rep, stderr);

    error_status = -1;

    return (0);
}
