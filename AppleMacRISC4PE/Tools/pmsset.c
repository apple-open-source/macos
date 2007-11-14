/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: pmsset.c,v 1.3 2005/08/17 18:33:35 raddog Exp $
 *
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <nlist.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <mach/mach_types.h>
#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/mach_error.h>

extern int errno;
extern int optind;
extern char *optarg;
extern int optopt;
extern int opterr;
extern int optreset;


#define pmsMaxStates 64
#define HalfwayToForever 0x7FFFFFFFFFFFFFFFULL
#define century 790560000000000ULL

typedef void (*pmsSetFunc_t)(uint32_t, uint32_t);	/* Function used to set hardware power state */

typedef struct pmsStat {
	uint64_t	stTime[2];			/* Total time until switch to next step */
	uint32_t	stCnt[2];			/* Number of times switched to next step */
} pmsStat;

typedef struct pmsDef {
	uint64_t	pmsLimit;			/* Max time in this state in microseconds */
	uint32_t	pmsStepID;			/* Unique ID for this step */
	uint32_t	pmsSetCmd;			/* Command to select power state */
#define pmsCngXClk  0x80000000		/* Change external clock */
#define pmsXUnk   	0x7F			/* External clock unknown  */
#define pmsXClk     0x7F000000		/* External clock frequency */
#define pmsCngCPU   0x00800000		/* Change CPU parameters */
#define pmsSync     0x00400000		/* Make changes synchronously, i.e., spin until delay finished */
#define pmsMustCmp  0x00200000		/* Delay must complete before next change */
#define pmsCPU      0x001F0000		/* CPU frequency */
#define pmsCPUUnk	0x1F			/* CPU frequency unknown */
#define pmsCngVolt  0x00008000		/* Change voltage */
#define pmsVoltage  0x00007F00		/* Voltage */
#define pmsVoltUnk	0x7F			/* Voltage unknown */
#define pmsPowerID  0x000000FF		/* Identify power state to HW */

/*	Special commands - various things */
#define pmsDelay    0xFFFFFFFD		/* Delayed step, no processor or platform changes.  Timer expiration causes transition to pmsTDelay */
#define pmsParkIt	0xFFFFFFFF		/* Enters the parked state.  No processor or platform changes.  Timers cancelled */
#define pmsCInit	((pmsXUnk << 24) | (pmsCPUUnk << 16) | (pmsVoltUnk << 8))	/* Initial current set command value */

/*	Note:  pmsSetFuncInd is an index into a table of function pointers and pmsSetFunc is the address
 *	of a function.  Initially, when you create a step table, this field is set as an index into
 *	a table of function addresses that gets passed as a parameter to pmsBuild.  When pmsBuild
 *	internalizes the step and function tables, it converts the index to the function address.
 */
	union sf {
		pmsSetFunc_t	pmsSetFunc;	/* Function used to set platform power state */
		uint32_t	pmsSetFuncInd;	/* Index to function in function table */
	} sf;

	uint32_t	pmsDown;			/* Next state if going lower */
	uint32_t	pmsNext;			/* Normal next state */
	uint32_t	pmsTDelay;			/* State if command was pmsDelay and timer expired */
} pmsDef;

typedef struct pmsCtl {
	pmsStat		(*pmsStats)[pmsMaxStates];	/* Pointer to statistics information, 0 if not enabled */
	pmsDef		*pmsDefs[pmsMaxStates];	/* Indexed pointers to steps */
} pmsCtl;

/*
 *	Note that this block is in the middle of the per_proc and the size (32 bytes)
 *	can't be changed without moving it.
 */

typedef struct pmsd {
	uint32_t	pmsState;			/* Current power management state */
	uint32_t	pmsCSetCmd;			/* Current select command */
	uint64_t	pmsPop;				/* Time of next step */
	uint64_t	pmsStamp;			/* Time of transition to current state */
	uint64_t	pmsTime;			/* Total time in this state */
} pmsd;

/*
 *	Required power management scripts
 */
 
enum {
	pmsIdle      = 0,				/* Power state in idle loop */
	pmsNorm      = 1,				/* Normal step - usually low power */
	pmsNormHigh  = 2,				/* Highest power in normal step */
	pmsBoost     = 3,				/* Boost/overdrive step */
	pmsLow       = 4,				/* Lowest non-idle power state, no transitions */
	pmsHigh      = 5,				/* Power step for full on, no transitions */
	pmsPrepCng   = 6,				/* Prepare for step table change */
	pmsPrepSleep = 7,				/* Prepare for sleep */
	pmsOverTemp  = 8,				/* Machine is too hot */
	pmsEnterNorm = 9,				/* Enter into the normal step program */
	pmsFree      = 10,				/* First available empty step */
	pmsStartUp   = 0xFFFFFFFE,		/* Start stepping */
	pmsParked    = 0xFFFFFFFF		/* Power parked - used when changing stepping table */
};

/*
 *	Power Management Stepper Control requests
 */
 
enum {
	pmsCPark = 0,					/* Parks the stepper */
	pmsCStart = 1,					/* Starts normal steppping */
	pmsCFLow = 2,					/* Forces low power */
	pmsCFHigh = 3,					/* Forces high power */
	pmsCCnfg = 4,					/* Loads new stepper program */
	pmsCQuery = 5,					/* Query current step and state */
	pmsCExperimental = 6,			/* Enter experimental mode */
	pmsCFree = 7					/* Next control command to be assigned */
};

#define pmsSetFuncMax 32

char stepfile[512];
FILE *sfile;

pmsDef pmsDefs[pmsMaxStates];					/* Make some room */
int defd[pmsMaxStates];							/* Make sure that we have what's correct defined */
char *cspot, *otoken;

char mimesteps[((sizeof(pmsDefs) / 3) * 4) + 4];	/* Get memory for the mime translated table */

extern kern_return_t pmsCall(uint32_t rqst, pmsDef *pd, uint32_t psize);
extern kern_return_t stepclientsend (void *pmsTable, uint32_t pmsTableLength, void *pmsAuxTable, uint32_t pmsAuxTableLength);
extern kern_return_t stepclientcontrol (uint32_t newStepLevel);

int cb2b64(unsigned char *in, unsigned char *out, int len);

#define century 790560000000000ULL

int main(int argc, char **argv) {

	int i, j, k, opt, maxstep, cline, activate, quiet, userclient, raw, mime, mlen, form;
	char line[512], *xx;
	int chrs, lsize, ltoken, mtoken;
	char *badchar;
	int mult, errors;
	uint32_t pStepID, pHWSel, pSetFunc, pDown, pNext, stepLevel, pDelay;
	uint64_t pLimit;
	kern_return_t ret;
	int cstp, cstpx, seen[pmsMaxStates], lchkd[pmsMaxStates];	

	activate = 0;
	quiet = 0;
	userclient = 0;
	raw = 0;
	mime = 0;

	stepfile[0] = 0;							/* Clear the file name */

	while(1) {
		
		if(optind >= argc) break;				/* Leave if no more options */		
		if(*argv[optind] != '-') {				/* This should be the step file name */
			strncpy(stepfile, argv[optind], 511);	/* Get it */
			optind++;							/* Point past it for getopt */
		}
		
		opt = getopt(argc, argv, "aqhrsum");	/* Parse the arguments */
		if(opt == -1) break;
		switch (opt) {
					
			case 'a':							/* Activate the steps */
				activate = 1;
				break;
					
			case 'm':							/* Dump mime 64 encoded table */
				mime = 1;
				break;
					
			case 'q':							/* Dump source? */
				quiet = 1;						/* No */
				break;
					
			case 'r':							/* Dump raw data? */
				raw = 1;						/* No */
				break;
					
			case 'u':							/* Activate with userclient? */
				userclient = 1;					/* No */
				break;
					
			case 's':							/* set step level */
				if(*argv[optind] != '-') {				/* This should be the numerical step level */
					stepLevel = strtoul(argv[optind], 0, 0);	/* Get the step ID */
					optind++;							/* Point past it for getopt */
					
					if (stepLevel > 3) {
						printf ("Invalid step level %d\n", stepLevel);
						exit (0);
					}
					
					if (geteuid() != 0 ) {						/* We need to be root to activate */
						printf("\nroot is required for controlling step levels\n");
						exit(1);
					}

					ret = stepclientcontrol (stepLevel);
					if(ret == KERN_SUCCESS) {					/* I don't believe it, it worked! */
						printf("\nStep controlled successfully\n");
					} else
						printf("\nStep control failed\n");
					exit (0);
				}
		
				break;
					
			default:
				printf("usage: pmsset - read the code dude...\n");
				exit(1);
		}
		
	}

	if (stepfile[0] == 0) {
		printf("Step file name missing\n");
		exit(1);
	}

	if(!quiet) printf("\nCompiling step file: \"%s\"\n\n", stepfile);
	sfile = fopen(stepfile, "r");				/* Open the step definition */
	if(sfile == 0) {
		printf("Step definition file error, errno = %d\n", errno);
		exit(1);
	}
	
	bzero((void *)pmsDefs, pmsMaxStates * sizeof(pmsDef));	/* Make it tidy */
	for(i = 0; i < pmsMaxStates; i++) defd[i] = 0;	/* Clear sanity check */
	
	errors = 0;
	cline = 0;
	maxstep = -1;
	
	while(1) {									/* Read in the file */
	
		line[511] = 0;							/* Make sure we have at least one null */
		xx = fgets(line, 511, sfile);			/* Read a line of the control file */
		if(!(uint32_t)xx) {						/* Could we have gotten an end of file? */ 
			if(feof(sfile)) break;				/* We hit end of file... */
			printf("Error reading definition file\n");	/* Say error */
			errors = 1;
			break;
		}
	
		cline++;								/* Bump line count */
	
		lsize = strlen(line);					/* Get the number of bytes read in */
		if(line[lsize - 1] == '\n') lsize = lsize - 1;	/* Strip off a newline */
		line[lsize] = 0;						/* Make sure we have a trailing null */
	
		if(!quiet) printf("%3d) %s\n",cline, line);	/* Print the input line */
		
		chrs = strspn(line, " \t");				/* Find first non-white space */
		cspot = &line[chrs];					/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') continue;	/* Skip comments and blank lines */
		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert first token to string */
	
/*		Get the step ID */

		if(cspot[0] == 0 | cspot[0] == ';') continue;	/* Skip comments and blank lines */
		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert first token to string */
		
		pStepID = strtoul(cspot, &badchar, 0);	/* Get the step ID */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid step ID\n");	
			errors = 1;
			continue;
		}
		if(pStepID >= pmsMaxStates) {			/* Out of range? */
			if(!quiet) printf("     Step ID out of range\n");
			errors = 1;
			continue;
		}
		if(defd[pStepID]) {						/* Previously defined? */
			if(!quiet) printf("     Step previously defined, line = %d\n", defd[pStepID]);
			errors = 1;
			continue;
		}
		pmsDefs[pStepID].pmsStepID = pStepID;	/* Start building the entry */
	
/*		Get the down step */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			if(!quiet) printf("     Missing down step\n");
			errors = 1;
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pDown = strtoul(cspot, &badchar, 0);	/* Get the down step */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid down step\n");	
			errors = 1;
			continue;
		}
		if((pDown != 0xFFFFFFFF) && (pDown >= pmsMaxStates)) {	/* Out of range? */
			if(!quiet) printf("     Down step out of range\n");
			errors = 1;
			continue;
		}
		pmsDefs[pStepID].pmsDown = pDown;		/* Start building the entry */
	
	
/*		Get the next step */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			if(!quiet) printf("     Missing next step\n");
			errors = 1;
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pNext = strtoul(cspot, &badchar, 0);	/* Get the next step */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid next step\n");	
			errors = 1;
			continue;
		}
		if((pNext != 0xFFFFFFFF) && (pNext >= pmsMaxStates)) {	/* Out of range? */
			if(!quiet) printf("     Next step out of range\n");
			errors = 1;
			continue;
		}
		pmsDefs[pStepID].pmsNext = pNext;		/* Start building the entry */
	
	
/*		Get the hardware step selector */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			if(!quiet) printf("     Missing hardware selector\n");
			errors = 1;
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pHWSel = strtoul(cspot, &badchar, 16);	/* Get the set command - this one is in hex.  FFFFFFFF is park. */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid set command\n");	
			errors = 1;
			continue;
		}
		pmsDefs[pStepID].pmsSetCmd = pHWSel;	/* Start building the entry */
	
	
/*		Get the platform set function */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			if(!quiet) printf("     Missing platform set function\n");
			errors = 1;
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pSetFunc = strtoul(cspot, &badchar, 0);	/* Get the hardware selector */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid platform set function\n");	
			errors = 1;
			continue;
		}
		pmsDefs[pStepID].sf.pmsSetFuncInd = pSetFunc;	/* Start building the entry */
	
/*		Get the time limit */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			pmsDefs[pStepID].pmsLimit = century;	/* Default to 100 years */
			defd[pStepID] = cline;				/* Remember where we defined it */
			if((int)pStepID > maxstep) maxstep = pStepID;	/* Remember the highest one we did */
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pLimit = strtoull(cspot, &badchar, 0);	/* Get the time limit */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid time limit\n");	
			errors = 1;
			continue;
		}
		
		if(pLimit > century) {					/* Is it too big? */
			if(!quiet) printf("     Time limit greater than one century\n");
			errors = 1;
			continue;
		}
		
		if(pLimit == 0xFFFFFFFFFFFFFFFFULL) pLimit = century;	/* If -1, default to a century */
		
		if((pLimit != 0) && (pLimit < 100ULL)) {	/* Is it too small but not 0? */
			if(!quiet) printf("     Time limit smaller than 100 microseconds\n");
			errors = 1;
			continue;
		}
		
		pmsDefs[pStepID].pmsLimit = pLimit;		/* Start building the entry */

/*		Get the TDelay */

		chrs = strspn(&cspot[ltoken + 1], " \t");	/* Find start of next operand */
		cspot = &cspot[chrs + ltoken + 1];		/* Point to the first character */
		
		if(cspot[0] == 0 | cspot[0] == ';') {	/* Is this one missing? */
			pmsDefs[pStepID].pmsTDelay = pmsIdle;	/* Default to pmsIdle */
			defd[pStepID] = cline;				/* Remember where we defined it */
			if((int)pStepID > maxstep) maxstep = pStepID;	/* Remember the highest one we did */
			continue;
		}

		ltoken = strcspn(cspot, " \t");			/* Find next white space */
		cspot[ltoken] = 0;						/* Convert next token to string */
		
		pDelay = strtoul(cspot, &badchar, 0);	/* Get the time limit */
		if((*badchar != 0) && (*badchar != ' ') && (*badchar != ';')) {	/* Any error? */
			if(!quiet) printf("     Invalid TDelay\n");	
			errors = 1;
			continue;
		}
		
		if(pDelay >= pmsMaxStates) {			/* Is it too big? */
			if(!quiet) printf("     TDelay step out of range\n");
			errors = 1;
			continue;
		}
		
		pmsDefs[pStepID].pmsTDelay = pDelay;	/* Start building the entry */

		defd[pStepID] = cline;					/* Remember where we defined it */
		if((int)pStepID > maxstep) maxstep = pStepID;	/* Remember the highest one we did */
	
	}
	

	if(!quiet) printf("\n\nPost-compile validation\n");
	
	if(!quiet) printf("\n Stmt  StepID   Down   Next       SetCMD  SetFunc                Limit  TDelay\n");
	for(i = 0; i <= maxstep; i++) {
		if(!defd[i]) continue;					/* Skip if not defined... */
		if(!quiet) printf("%4d)  %6d %6d %6d     %08X   %6d %20lld  %6d\n", defd[i], pmsDefs[i].pmsStepID, pmsDefs[i].pmsDown, 
			pmsDefs[i].pmsNext, pmsDefs[i].pmsSetCmd,
			pmsDefs[i].sf.pmsSetFuncInd, pmsDefs[i].pmsLimit, pmsDefs[i].pmsTDelay);
		
		if((pmsDefs[i].pmsSetCmd != 0xFFFFFFFF) && (pmsDefs[i].pmsDown != 0xFFFFFFFF) && !defd[pmsDefs[i].pmsDown]) {	/* Has the down been defined or are we parking? */
			if(!quiet) printf("       Down step has not been defined\n");
			errors = 1;
		}
		
		if((pmsDefs[i].pmsSetCmd != 0xFFFFFFFF) && (pmsDefs[i].pmsNext != 0xFFFFFFFF) && !defd[pmsDefs[i].pmsNext]) {	/* Has the next been defined or are we parking? */
			if(!quiet) printf("       Next step has not been defined\n");
			errors = 1;
		}
		
		if((pmsDefs[i].pmsSetCmd != pmsDelay) && (pmsDefs[i].pmsTDelay != pmsIdle)) {	/* Has TDelay be specified for a non-delay command? */
			if(!quiet) printf("       TDelay specfied for a non-delay command\n");
			errors = 1;
		}
		
		if((pmsDefs[i].pmsSetCmd == pmsDelay) && !defd[pmsDefs[i].pmsTDelay]) {	/* Has TDelay step been defined? */
			if(!quiet) printf("       TDelay step has not been defined\n");
			errors = 1;
		}
	}
	
	if(!quiet) printf("\n\nChecking for infinite non-wait loops\n");
	
	for(i = 0; i < pmsMaxStates; i++) lchkd[i] = 0;	/* Clear loop checked flags */
	
	for(i = 0; i <= maxstep; i++) {				/* Check all included steps for infinite non-wait loops */
		
		if(lchkd[i]) continue;					/* We already checked this one... */

		for(j = 0; j < pmsMaxStates; j++) seen[j] = 0;	/* Clear the "seen this one" flags */
		
		cstp = i;								/* Start here */
		while(1) {								/* Do until we hit the end */
			
			if(seen[cstp]) {					/* Have we chained through this step already? If so, infinite loop... */
				if(!quiet) printf("%4d)  Infinite non-wait loop detected:  %d", defd[cstp], cstp);	/* Say so */
				cstpx = cstp;					/* Remember the first */
				while(1) {						/* Chase the chain... */
					cstp = pmsDefs[cstp].pmsNext;	/* Chain on */
					if(!quiet) printf(" --> %d", cstp);	/* Print it */
					if(cstp = cstpx) break;		/* We've wrapped */
				}
				if(!quiet) printf("\n");		/* Return the carriage */
				errors = 1;
				break;							/* Out of this here loop... */
			}
				
			lchkd[cstp] = 1;					/* Remember we've seen this step */
			seen[cstp] = 1;						/* Remember we've chained through this step */
			
			if(pmsDefs[cstp].pmsSetCmd == pmsParkIt) break;	/* Parking always terminates a chain so no endless loop here */
			if(pmsDefs[cstp].pmsSetCmd == pmsDelay) break;	/* Delay always terminates a chain so no endless loop here */
			if((pmsDefs[cstp].pmsLimit != 0) && ((pmsDefs[cstp].pmsSetCmd & pmsSync) != pmsSync)) break;	/* If time limit is not 0 and not synchronous, no endless loop */
			if(pmsDefs[cstp].pmsNext == pmsParked) break;	/* If the next step is parked, no endless loop */
			
 			cstp = pmsDefs[cstp].pmsNext;		/* Chain to the next */
 		}	
	}
		
	if (raw) {
		for(i = 0; i <= maxstep; i++) {
			printf ("%08x %08x %08x %08x %08x %08x %08x %08x ", (unsigned int)((pmsDefs[i].pmsLimit >> 32) & 0xFFFFFFFF),
			 (unsigned int)(pmsDefs[i].pmsLimit & 0xFFFFFFFF), pmsDefs[i].pmsStepID, pmsDefs[i].pmsSetCmd, pmsDefs[i].sf.pmsSetFuncInd, 
			 pmsDefs[i].pmsDown, pmsDefs[i].pmsNext, pmsDefs[i].pmsTDelay); 
		}
		printf ("\n");
	}
		
	if (mime) {									/* Dump mime.  I hate mimes.  Whoever decided to use this should be locked in an invisible box */
		
		mlen = cb2b64((char *)pmsDefs, mimesteps, (maxstep + 1) * sizeof(pmsDef));	/* Convert the table to mime 64 */
		
		for(i = 0; i < (mlen / 4); i++) {		/* Dump it in words */
			form = i % 5;						/* Why do we dump this out in 5 word chunks? */
			if(form == 0) printf("                            ");	/* Space out */
			printf("%c%c%c%c", mimesteps[i * 4], mimesteps[(i * 4) + 1], mimesteps[(i * 4) + 2], /* Print a word's worth */
				mimesteps[(i * 4) + 3]);
			if(form == 4) printf("\n");
		}
		
		printf ("\n");
	}

	if(errors) {								/* Any errors detected? */
		printf("\nErrors detected, update abandoned\n");
		exit(1);
	}

	if(!activate) {								/* Any errors detected? */
		printf("\nNo activation requested (-a), update abandoned\n");
		exit(0);
	}

	if (geteuid() != 0 ) {						/* We need to be root to activate */
		printf("\nNo activation: root is required\n");
		exit(1);
	}

	printf("\nAttempting step activation\n");

/*
 *	Ok, we're gonna poke the kernel now...
 */

	if (userclient)
		ret = stepclientsend (&pmsDefs[0], (maxstep + 1) * sizeof(pmsDef), NULL, 0);
	else
		ret = pmsCall(pmsCCnfg, &pmsDefs[0], (maxstep + 1) * sizeof(pmsDef));	/* Call the kernel to install step table */

	if(ret == KERN_SUCCESS) {					/* I don't believe it, it worked! */
		printf("\nStep table successfully installed\n");
		exit(0);
	}
	
	printf("Step table install failed, ret = %08X\n", ret);
	exit(1);
}

/*		Base64 mime

         0 A            17 R            34 i            51 z
         1 B            18 S            35 j            52 0
         2 C            19 T            36 k            53 1
         3 D            20 U            37 l            54 2
         4 E            21 V            38 m            55 3
         5 F            22 W            39 n            56 4
         6 G            23 X            40 o            57 5
         7 H            24 Y            41 p            58 6
         8 I            25 Z            42 q            59 7
         9 J            26 a            43 r            60 8
        10 K            27 b            44 s            61 9
        11 L            28 c            45 t            62 +
        12 M            29 d            46 u            63 /
        13 N            30 e            47 v
        14 O            31 f            48 w         (pad) =
        15 P            32 g            49 x
        16 Q            33 h            50 y
*/

unsigned char *b2b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int cb2b64(unsigned char *in, unsigned char *out, int len) {

	int outpos, extra, chunks, i;
	uint32_t d24;
	
	outpos = 0;
	chunks = len / 3;							/* Get number of 3 byte chunks */
	for(i = 0; i < chunks; i++) {				/* Step through input 3 bytes at a crack */
		d24 = (in[0] << 16) | (in[1] << 8) | in[2];	/* Create a 24 bit number */
		out[outpos] = b2b64[(d24 >> 18) & 0x3F];	/* Convert first 6 bits */
		out[outpos + 1] = b2b64[(d24 >> 12) & 0x3F];	/* Convert second 6 bits */
		out[outpos + 2] = b2b64[(d24 >> 6) & 0x3F];	/* Convert third 6 bits */
		out[outpos + 3] = b2b64[d24 & 0x3F];	/* Convert fourth 6 bits */
		outpos = outpos + 4;					/* Next output location */
		in = in + 3;							/* Next input position */
	}
	
	extra = len % 3;							/* Get extra bytes */
	if(extra == 0) return outpos;				/* Finished */
	
	out[outpos + 2] = '=';						/* Fill extra */
	out[outpos + 3] = '=';						/* Fill extra */

	out[outpos] = b2b64[(in[0] >> 2) & 0x3F];		/* Convert first 6 bits */
	
	if(extra == 1) {							/* If there is just one left */
		out[outpos + 1] = b2b64[(in[0] << 4) & 0x30];	/* Convert second 6 bits */
		return (outpos + 4);					/* Return length */
	}
			
	out[outpos + 1] = b2b64[((in[0] << 4) & 0x30) | ((in[1] >> 4) & 15)];	/* Convert second 6 bits */
	out[outpos + 2] = b2b64[(in[1] << 2) & 0x3C];	/* Convert third 6 bits */
	return (outpos + 4);
}
			
