/*

	user-level program to display and change debug variables in an 
	active AIX kernel running appletalk. Is is assumed that there
	are 2 u_long bit-mapped variables and that defines in DEBUG_H
	assign the bit values. Bits are set in one of 2 variables based
	on the define prefix of either D_M (for modules) or D_L
	(for types). The defines are displayed in an abbreviated format
	of L_xxx for D_L_xxx defines and M_xxx for D_M_xxx 
	defines.

	revision history
	----------------
	09-08-94	0.01 jjs Created
	09-12-94	0.02 jjs minor
	04-03-96	0.03 jjs allow running & updating of bits in file
						even if stack down

*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <c.h>
#include <fcntl.h>
#include <sys/sysctl.h>

#include <netat/appletalk.h>
#include <netat/debug.h>

#define DEBUG_H	"debug.h"	
#define AT_BASE_PATH  "/netat"
#define DELIMS		"	, "
#define COLUMNS		2
#define	MOD		0		/* array indices */
#define LEV		1

#define HELP \
"	Enter list of numbers to toggle the state of.\n\
	An asterisk (*) marks bits which are active.\n\
	Numbers can be separated by spaces,tabs or commas.\n\
	Ranges of numbers are allowed (e.g. 5,9-15,20).\n\
	Path to debug.h is required and is searched for in\n\
	the following order:\n\
		1. BASE env variable is set (e.g. to '/usr/include')\n\
		2. AT_DEBUG_DIR points to the dir where it is\n\
		3. debug.h is in the current directory\n\
	Other commands:\n\
	q - quit without updating kernel\n\
	x - eXit and update kernel\n\
	t - toggle state of all bits\n\
	u - update kernel immediately\n\
	l - list status\n\
	s - set all bits to ON\n\
	c - clear all bits\n"

dbgBits_t dbgBits;

main(argc, argv)
	int argc;
	char **argv;
{
	char buf[80], instr[80];
	char defStr[40], *t, *s;
	u_long	defBytes[64];
	u_char	type[64];
	int i = 0,j,c,set;
	int found = 0;		/* set to TRUE if any lines found */
	int redisplay;
	int quit = 0;
	int lastRange, range=FALSE;
	size_t size = (size_t)sizeof(dbgBits);

	FILE *fin;

	/* read the value of dbgBits from the kernel */
	if (sysctlbyname("net.appletalk.debug",
			 (void *)&dbgBits, &size, 0, 0) < 0) {
		printf("error retreiving kernel dbg bit status\n");
		exit(1);
	}

	/* find a debug header file */
	if (argc > 1) {
		if (!(fin = fopen(argv[1],"r"))) {
			printf("error opening input file %s, aborting\n",argv[1]);
			exit(1);
		}
	} else {
		if ((t = getenv("BASE")) != NULL) {
			sprintf(buf,"%s%s/%s",t,AT_BASE_PATH, DEBUG_H);
			t = buf;
		} else {
			printf("env variable BASE not found, trying variable AT_DEBUG_DIR\n");

			if (t = getenv("AT_DEBUG_DIR")) {
				sprintf(buf,"%s/%s",t,DEBUG_H);
				t = buf;
			} else {
				printf("env variable AT_DEBUG_DIR not found, trying current dir\n");
				t = DEBUG_H;
			}
		}
		if (access(t,R_OK)) {
			printf("%s not accessable, aborting\n",t);
			exit(1);
		}
		if (!(fin = fopen(t,"r"))) {
			printf("error opening input file %s, aborting\n",t);
			exit(1);
		}
	}

	/* a debug header file has been found */
	do {	/* user input loop */
		i=0;
		while (!feof(fin)) {	/* loop to read & display file */
			if (!fgets(buf, sizeof(buf)-1, fin))
				break;
			if (sscanf(buf," #define D_L_%s 0x%lx", 
				defStr, &defBytes[i]) == 2){
				type[i] = LEV;
			}else {
				if (sscanf(buf," #define D_M_%s 0x%lx", 
					defStr, &defBytes[i]) == 2){
					type[i] = MOD;
				} else
					continue;
			}
			found = 1;
			set = (type[i] == MOD)? dbgBits.dbgMod & defBytes[i] : 
			  		    	dbgBits.dbgLev & defBytes[i];
			printf("%2d %c%c_%-20s",
			       i+1, set ? '*' : ' ', 
			       (type[i] == MOD) ? 'M' : 'L', defStr);
			i++;
			if (!(i%COLUMNS))
				printf("\n");
		}
		if (!found) {
			printf("no defines found in %s. Aborting\n",DEBUG_H);
			exit(1);
		}
		if ((i%COLUMNS))
			printf("\n");
		rewind(fin);
		do {
			redisplay = 1;
			printf("cmd>");
			gets(instr);
			if (strlen(instr) == 0) {
				printf("enter ? for help\n");
				continue;
			}
			s = strtok(instr, DELIMS);
			switch ( s[0] = toupper(s[0])) {
				case 'X':	/* eXit and update kernel */
				case 'Q':	/* quit, don't update kernel */
					quit = 1;
					break;
				case 'S':	/* set all bits */
					dbgBits.dbgMod = 0xffffffff;
					dbgBits.dbgLev = 0xffffffff;
					break;
				case 'C':	/* clear all bits */
					dbgBits.dbgMod = 0;
					dbgBits.dbgLev = 0;
					break;
				case 'T':	/* toggle all bits */
					dbgBits.dbgMod ^= 0xffffffff;
					dbgBits.dbgLev ^= 0xffffffff;
					break;
				case 'U':	/* update kernel imediately */
					redisplay = 0;
					/* set the dbgBits in the kernel */
					if (sysctlbyname("net.appletalk.debug", 
							 0, 0,
							 (void *)&dbgBits, 
							 size) < 0) {
					  printf("error setting DBG info in kernel\n");
					}
					break;
				case 'L':	/* list status */
					redisplay = 1;
					break;
				case '?':	/* help */
					redisplay = 0;
					printf(HELP);
					break;
				default:
					if (isdigit(s[0]))
						break;
					printf("unknown command, enter '?' for help\n");
					redisplay = 0;

					break;
			}
			if (quit || isdigit(s[0]) ||redisplay)
				break;
		} while (1);
		if (quit)
			break;
		if (!isdigit(s[0]))
			continue;
		do {
			c = atoi(s);
			if (c>=1 && c < i) {
				c--;
				if (t=strchr(s,'-')) {
					lastRange=c-1;
					t++;
					c = atoi(t);
					c--;
					range=TRUE;
				}
				if (range && lastRange <= c) {
					for(j=lastRange+1; j<= c; j++)
						if (type[j] == MOD)
							dbgBits.dbgMod ^= defBytes[j];
						else
							dbgBits.dbgLev ^= defBytes[j];
					range=FALSE;
				}
				else {
					lastRange = c;
					if (type[c] == MOD)
						dbgBits.dbgMod ^= defBytes[c];
					else
						dbgBits.dbgLev ^= defBytes[c];
				}
			}
			else if (c == 0)
				if (s[0] == '-') 
					range=TRUE;
		} while (s = strtok(NULL, DELIMS));

	} while (1);

	if (s[0] == 'X') 
		/* set the dbgBits in the kernel */
		if (sysctlbyname("net.appletalk.debug", 0, 0,
				 (void *)&dbgBits, size) < 0)
			printf("error updating kernel\n");
		else
			printf("kernel updated\n");
	else 
		printf("leaving kernel unchanged\n");


	fclose(fin);
	exit(0);
}
