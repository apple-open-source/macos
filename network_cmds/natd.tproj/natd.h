/*
 * natd - Network Address Translation Daemon for FreeBSD.
 *
 * This software is provided free of charge, with no 
 * warranty of any kind, either expressed or implied.
 * Use at your own risk.
 * 
 * You may copy, modify and distribute this software (natd.h) freely.
 *
 * Ari Suutari <suutari@iki.fi>
 *
 *	$Id: natd.h,v 1.1.1.1 2000/01/11 01:48:51 wsanchez Exp $
 */

#define PIDFILE	"/var/run/natd.pid"
#define	INPUT		1
#define	OUTPUT		2
#define	DONT_KNOW	3

extern void Quit (const char* msg);
extern void Warn (const char* msg);
extern int SendNeedFragIcmp (int sock, struct ip* failedDgram, int mtu);


