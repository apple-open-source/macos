/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Title:	lap_init.c
 *
 * Facility:	Generic AppleTalk Link Access Protocol Interface
 *		(ALAP, ELAP, etc...)
 *
 * Author:	Gregory Burns, Creation Date: April-1988
 *
 ******************************************************************************
 *                                                                            *
 *        Copyright (c) 1988, 1998 Apple Computer, Inc.                       *
 *                                                                            *
 *        The information contained herein is subject to change without       *
 *        notice and  should not be  construed as a commitment by Apple       *
 *        Computer, Inc. Apple Computer, Inc. assumes no responsibility       *
 *        for any errors that may appear.                                     *
 *                                                                            *
 *        Confidential and Proprietary to Apple Computer, Inc.                *
 *                                                                            *
 ******************************************************************************
 */

/* "@(#)lap_init.c: 2.0, 1.19; 2/26/93; Copyright 1988-92, Apple Computer, Inc." */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <mach/boolean.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/lap.h>
#include <netat/ddp.h>
#include <netat/nbp.h>
#include <netat/at_var.h>
#include <netat/routing_tables.h>

#include <AppleTalk/at_proto.h>
#include <AppleTalk/at_paths.h>

#define	SET_ERRNO(e) errno = e

typedef struct if_cfg {
	at_ifnames_t    used;  
  	char		home_if[IFNAMESIZ];
} if_cfg_t;

/*#define MSGSTR(num,str)		catgets(catd, MS_LAP_INIT, num,str) */
#define MSGSTR(num,str)		str 

	/* multi-port setup defines */
#define MAX_ZONES		50	/* max cfg file zone count */
#define MAX_LINE_LENGTH		240	/* max size cfg file line */
#define INVALID_ZIP_CHARS	"=@*:\377"
#define MAX_NET_NO		0xFEFF	/* max legal net number */

#define COMMENT_CHAR 		'#'	/* version checking */

	/* printout format defines */
#define ZONE_IFS_PER_LINE	6	/* # of if's to list per line for
				           each zone for showCfg() */
#define Z_MAX_PRINT		15	/* maximum # zones to print */

at_ifnames_t seed_names;
int seed_ind;

#define IF_NO(c)    (atoi(&c[2]))       /* return i/f number from h/w name
					   (e.g. 'et2' returns 2) */

char 	homePort[5];			/* name of home port */

extern int in_list(char *, at_ifnames_t *);

/* prototypes */

void showCfg();

static FILE *STDOUT = stdout;

/* These functions set/get the defaults in/from persistent storage */

/* PRAM state information */
#define PRAM_FILE             NVRAM
#define NVRAMSIZE		256

#define INTERFACE_OFFSET	1
#define ADDRESS_OFFSET		(INTERFACE_OFFSET+IFNAMESIZ)
#define ZONENAME_OFFSET		(ADDRESS_OFFSET+sizeof(struct at_addr))

char pram_file[80];

static int readxpram(if_name, buf, count, offset)
	char *if_name, *buf;
	int count;
	int offset;
{
	int fd;

	sprintf(pram_file, "%s.%s", PRAM_FILE, if_name);

	if ((fd = open(pram_file, O_RDONLY)) == -1) {
		return(-1);
	}

	if ((int)lseek(fd, (off_t)offset, SEEK_SET) != offset) {
		close(fd);
		return(-1);
	}

	if (read(fd, buf, count) != count) {
		close(fd);
		return(-1);
	}
#ifdef COMMENT_OUT
	printf("readxpram: read %s %d bytes offset %d: %s\n", 
	       pram_file, count, offset,
	       (offset==ZONENAME_OFFSET)? ((at_nvestr_t *)buf)->str: "" );
#endif
	close(fd);
	return(0);
}

static int writexpram(if_name, buf, count, offset)
	char *if_name, *buf;
	int count;
	int offset;
{
	int fd;
	char buffer[NVRAMSIZE];

	sprintf(pram_file, "%s.%s", PRAM_FILE, if_name);

	if ((fd = open(pram_file, O_RDWR)) == -1) {
		if (errno == ENOENT) {
			fd = open(pram_file, O_RDWR|O_CREAT, 0644);
			if (fd == -1)
				return(-1);
			memset(buffer, 0, sizeof(buffer));
			if (write(fd, buffer, sizeof(buffer)) 
			    != sizeof(buffer)) {
				close(fd);
 				unlink(pram_file);
				return(-1);
			}
			(void) lseek(fd, (off_t)0, 0);
		}
		else
			return(0);
	}

	if ((int)lseek(fd, (off_t)offset, SEEK_SET) != offset) {
		close(fd);
		return(-1);
	}
#ifdef COMMENT_OUT
	printf("writexpram: wrote %s %d bytes offset %d\n", pram_file, 
	       count, offset);
#endif
	if (write(fd, buf, count) != count) {
		close(fd);
		return(-1);
	}

	close(fd);
	return(0);
}

/* save the current configuration in pram */
int at_savecfgdefaults(fd, ifName)
     int fd;
     char *ifName;
{
	int tmp_fd = 0;  
	at_if_cfg_t cfg;	/* used to read cfg */

	if (!fd) {
	  if ((tmp_fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);
	  fd = tmp_fd;
	}		

	if (ifName && strlen(ifName))
		strncpy(cfg.ifr_name, ifName, sizeof(cfg.ifr_name));
	else
		cfg.ifr_name[0] = '\0';
	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
		fprintf(stderr, "AIOCGETIFCFG failed for %s (%d)\n", 
			cfg.ifr_name, errno);
		if (tmp_fd)
			(void)close(tmp_fd);
		return(-1);
	} else {
		(void)at_setdefaultaddr(cfg.ifr_name, &cfg.node);
		(void)at_setdefaultzone(cfg.ifr_name, &cfg.zonename);
		(void)writexpram(cfg.ifr_name, ifName, IFNAMESIZ, 
				 INTERFACE_OFFSET);
	}
	if (tmp_fd)
		(void)close(tmp_fd);
	return(0);
}

int at_getdefaultaddr(ifName, init_address)
     char *ifName;
     struct at_addr *init_address;
{
	if (!ifName || !strlen(ifName))
        	return(-1);

	if (!readxpram(ifName, init_address, sizeof(struct at_addr), 
		       ADDRESS_OFFSET)) {
/*		printf("at_getdefaultaddr: readx net number %d, node %d\n", 
		init_address->s_net, init_address->s_node);
*/
		return(0);
	}
	else
		return (-1);
}

int at_setdefaultaddr(ifName, init_address)
     char *ifName;
     struct at_addr *init_address;
{
	if (!ifName || !strlen(ifName))
        	return(-1);

	return(writexpram(ifName, init_address, sizeof(struct at_addr), 
			  ADDRESS_OFFSET));
}

int at_getdefaultzone(ifName, zone_name)
     char *ifName;
     at_nvestr_t *zone_name;
{
	if (!ifName || !strlen(ifName))
        	return(-1);

	return(readxpram(ifName, zone_name, sizeof(at_nvestr_t), 
			 ZONENAME_OFFSET));
}

int at_setdefaultzone(ifName, zone_name)
     char *ifName;
     at_nvestr_t *zone_name;
{
	if (!ifName || !strlen(ifName))
        	return(-1);

	if (DEFAULT_ZONE(zone_name)) {	/* If we don't have a router  */
		return(0);		/* return w/o writing to PRAM */
	}

	return(writexpram(ifName, zone_name, sizeof(at_nvestr_t), 
			  ZONENAME_OFFSET));
}

int getConfigInfo(elapcfgp, zonep, cfgFileName, checkCfg, displayCfg, mh)
     at_if_cfg_t elapcfgp[];
     zone_usage_t zonep[];
     char  *cfgFileName;      /* optional alternate router.cfg file */
     short checkCfg;       /* if true, just check config file */
     short displayCfg;       /* display configuration only */
     short mh;			/* multihoming mode of router */

/* obtains and checks zone and port configuration information from the 
   configuration file. fills in the 2 arrays passed

   returns   0 if no problems were encountered.
			-1 if any error occurred
*/

{
	if_cfg_t filecfg;

	if (getIFInfo(elapcfgp, &filecfg, cfgFileName, checkCfg, displayCfg, mh))
		return(-1);
	if (!elapcfgp->ifr_name[0]) {
		fprintf(stderr, 
			MSGSTR(M_NO_IF_CFG, 
			"There are no interfaces configured\n"));
		return(-1);
	}
	
	if (!mh)
	  if (getZoneInfo(zonep, &filecfg, cfgFileName, checkCfg) ||
	      checkSeeds(elapcfgp, zonep))
	      return(-1);

	if (displayCfg)
		showCfg(elapcfgp, zonep);

	return(0);
} /* getConfigInfo */


int getIFInfo(elapcfgp, filecfgp, cfgFileName, checkCfg, displayCfg, mh)
     at_if_cfg_t elapcfgp[];
     if_cfg_t	*filecfgp;
     char  *cfgFileName;	/* optional alternate router.cfg file */
     short checkCfg;		/* if true, just check config file */
     short displayCfg;		/* display configuration only */
     short mh;			/* multihoming mode of router */

/* reads interface configuration information from source file and
   places valid entries into cfgp 

   returns  0 on success
            -1 if any errors were encountered
*/
{
	FILE 	*pFin;			/* input cfg file */
	char	linein[MAX_LINE_LENGTH], 
	  	buf[MAX_LINE_LENGTH], 
	  	buf1[MAX_LINE_LENGTH], 
	  	*pc1,
		errbuf[100];
	int	i,x;
	int	ifno=0;
	int	parmno;
	int	lineno=0;
	int	used_ind = 0;
	int	home = FALSE;
	int	bad = TRUE;
	int	gotNetStart;		/* true if entry had starting network # */
	int	done;
	int	error = FALSE;

	if (!(pFin = fopen(cfgFileName, "r"))) {
		fprintf(stderr,  MSGSTR(M_OPEN_CFG,
			"error opening configuration file: %s\n"), cfgFileName);
		return(-1);
	}
	filecfgp->home_if[0] = '\0';
	bzero(&seed_names, sizeof(seed_names));
	seed_ind = 0;
	
	while (!feof(pFin)) {
	
		if (!fgets(linein, sizeof(linein)-1, pFin)) {
			continue;
		}
		lineno++;
		if (linein[0] == COMMENT_CHAR)  	/* if comment line */
			continue;
		if (linein[0] == ':')			/* if zone entry, skip */
			continue;

		strcpy(buf,linein);

		pc1 = strtok(buf,":");			/* pc1 -> first parm */
		parmno=0;
		bad = FALSE;
		gotNetStart = FALSE;
		done = FALSE;

		if (pc1) do {				/* do loop start for current line */
			if (sscanf(pc1," %s",buf1) != 1) /* skip whitespace */
				break;
			switch(parmno) {
			case 0:		/* I/F name */
				/* used is a list of interfaces to keep track
				   of whether the interface has been used */
				if (in_list(buf1, &filecfgp->used)) {
					sprintf(errbuf, MSGSTR(M_IF_USED,
						"interface %s already used"), buf1);
					bad = TRUE;
				}
				if (!bad)
					strncpy(elapcfgp[ifno].ifr_name, buf1, 4);
				break;
			case 1:		/* net range or home desig */
			case 2:
				if (sscanf(buf1,"%d",&x) != 1){	
				    if (buf1[0] == '*') {		/* must be home then */
				      	if (home++) {
					    sprintf(errbuf, 
						    MSGSTR(M_MULT_HOME,
							   "multiple home designations (%d) "), parmno);
					    bad = TRUE;
					}
					elapcfgp[ifno].flags = ELAP_CFG_HOME;
					done = TRUE;			/* '*' must be last */
					sprintf(homePort,elapcfgp[ifno].ifr_name);
				    }
				    else {
				        sprintf(errbuf, MSGSTR(M_BAD_FMT, "invalid format "));
					bad = TRUE;
				    }
				} else	{	/* this is an actual net number */
				    if (x <= 0 || x >= MAX_NET_NO) {
				        sprintf(errbuf, MSGSTR(M_BAD_NET, 
							       "invalid net:%d "),x);
					bad = TRUE;
				    }
				    if (parmno == 1) {
					elapcfgp[ifno].netStart = x;
					gotNetStart = TRUE;
				    }
				    else {
				        if (elapcfgp[ifno].netStart > x) {
					    sprintf(errbuf, 
						    MSGSTR(M_END_LT_START,
							   "ending net less than start"));
					    bad = TRUE;
					}
					else	
					    elapcfgp[ifno].netEnd = x;
				    }
				}
				break;
			case 3:
				if (buf1[0] == '*') {	/* only valid entry for parm 3 */
				    if (home++) {
				        sprintf(errbuf, 
						MSGSTR(M_MULT_HOME,
						       "multiple home designations (%d) "), parmno);
					bad = TRUE;
				    }
				    elapcfgp[ifno].flags = ELAP_CFG_HOME;
				    sprintf(homePort,elapcfgp[ifno].ifr_name);
				    done = TRUE;
				    break;
				}
				/* fall through */
			default:
				fprintf(stderr,
					"extra input ignored:(%s)\n", buf1);
				done = TRUE;


			} /* end switch */
			if (bad)
				break;
			parmno++;
		} while(!done && pc1 && (pc1  = strtok(NULL,":")));
		if (!bad) {
			if (elapcfgp[ifno].ifr_name[0]) {	/* if entry used, check it */
			    if (gotNetStart) {
			        if (!elapcfgp[ifno].netEnd)	/* if no end, end = start */
				    elapcfgp[ifno].netEnd = elapcfgp[ifno].netStart;

				/* check for range conflicts */
				for (i=0; i<ifno; i++) {
				    
				    if ((elapcfgp[ifno].netEnd >= elapcfgp[i].netStart &&
					 elapcfgp[ifno].netEnd <= elapcfgp[i].netEnd)  ||
					(elapcfgp[ifno].netStart >= elapcfgp[i].netStart &&
					 elapcfgp[ifno].netStart <= elapcfgp[i].netEnd) ||
					(elapcfgp[i].netEnd >= elapcfgp[ifno].netStart &&
					 elapcfgp[i].netEnd <= elapcfgp[ifno].netEnd)  ||
					(elapcfgp[i].netStart >= elapcfgp[ifno].netStart &&
					 elapcfgp[i].netStart <= elapcfgp[ifno].netEnd))  {
				        sprintf(errbuf,  
						MSGSTR(M_RANGE_CONFLICT,
						       "%s net range conflict with %s"),
						elapcfgp[ifno].ifr_name,
						elapcfgp[i].ifr_name);
					bad = TRUE;
					break;
				    }
				}
			    }
			    if (!bad) {		
			        /* set used here */
			        strcpy(filecfgp->used.at_if[used_ind++], 
				       elapcfgp[ifno].ifr_name);
				if (elapcfgp[ifno].flags & ELAP_CFG_HOME) {
				    strcpy(filecfgp->home_if, 
					   elapcfgp[ifno].ifr_name);
				}
				if (gotNetStart) {   /* identify as seed */
				    strcpy(seed_names.at_if[seed_ind++],
					   elapcfgp[ifno].ifr_name);
				}
				ifno++;		
			    }
			} /* end if if_name */
		} 
		if (bad ) {
 			/* reset this entry */
			memset(&elapcfgp[ifno], NULL,sizeof(elapcfgp[ifno]));
			if (checkCfg)
				/* finish error msg */
				fprintf(stderr,  MSGSTR(M_LINE1,
					"error,  %s\n"),errbuf);
			else
				fprintf(stderr,  MSGSTR(M_LINE,
				"error, line %d. %s\n%s\n"),lineno,errbuf,linein);
			error = TRUE;
			break;
		}	/* end if bad */
	}
	if (!mh && !home && !error && ifno) {
		if (checkCfg || displayCfg) 
			fprintf(stderr, MSGSTR(M_NO_HOME_PT_W, 
				"Warning, no home port specified.\n"\
				"(You must designate one interface as the home port before\n"\
				"starting Appletalk in router mode)\n\n"));
		else {
			error = TRUE;
			fprintf(stderr,  MSGSTR(M_NO_HOME_PT,
				"error, no home port specified\n"));
		}
	}
	if (mh && !home)
		elapcfgp[0].flags = ELAP_CFG_HOME;
	fclose(pFin);
	return(error ? -1 : 0);
} /* getIFInfo */

int getZoneInfo(zonep, filecfgp, cfgFileName, checkCfg)
     zone_usage_t zonep[];
     if_cfg_t	*filecfgp;
     char  *cfgFileName;   /* optional alternate router.cfg file */
     short checkCfg;    /* if true, just check config file */

/* reads zone information from configuration file and places valid entries 
   into zonep 

   returns  0 on success
	   -1 if any errors were encountered
*/
{
	FILE 	*pFin;			/* input cfg file */
	char	linein[MAX_LINE_LENGTH], buf[MAX_LINE_LENGTH], 
			buf1[MAX_LINE_LENGTH], *pc1, *pc2;
	char	tbuf1[NBP_NVE_STR_SIZE+20];
	char	tbuf2[NBP_NVE_STR_SIZE+20];
	char	curzone[NBP_NVE_STR_SIZE+20];
	char	errbuf[100];
	int	i,j;
	int	parmno;
	int	lineno=0, homeLine=0;
	int	home  = FALSE;
	int	bad   = TRUE;
	int	done  = FALSE;
	int	error = FALSE;
	int	zoneno = 0;
	int	z_ind = 0;
	int 	len;
	int	gotHomePort =  (filecfgp->home_if[0])?1:0;

	if (!(pFin = fopen(cfgFileName, "r"))) {
		fprintf(stderr,  MSGSTR(M_OPEN_CFG,
			"error opening configuration file: %s\n"), cfgFileName);
		return(-1);
	}

	while (!feof(pFin)) {
		if (!fgets(linein, sizeof(linein)-1, pFin)) {
			continue;
		}
		lineno++;
		if (linein[0] == COMMENT_CHAR)  /* if comment line */
			continue;
		if (linein[0] != ':')		/* zone entries start with ':' */
			continue;						
		strcpy(buf,linein);

		pc1 = strtok(&buf[1],":");
		parmno=0;
		if (!bad) {
			zoneno++;
			z_ind = 0;
		}
		bad = FALSE;
		done = FALSE;

		if (pc1) do {			/* do loop start for current line */
			if (sscanf(pc1," %[^:\n]",buf1) != 1)	/* skip whitespace */
				break;
			switch(parmno) {
			case 0:			/* zone name */
				strcpy(curzone,buf1);
				/* chk length */
				if ((len = strlen(buf1)) > NBP_NVE_STR_SIZE) {
					sprintf(errbuf,  MSGSTR(M_ZONE_TOO_LONG,
						"zone name too long"));
					bad = TRUE;
					break;
				}
				/* chk for bad chars */
				if (pc2 = strpbrk(buf1, INVALID_ZIP_CHARS)) {
					sprintf(errbuf,  MSGSTR(M_IVAL_CHAR,
						"invalid character in zone name (%c)"),*pc2);
					bad = TRUE;
					break;
				}	
				/* check for dupe zones */
				for (i=0; i < zoneno; i++) {
					strcpy(tbuf1, buf1);
					strcpy(tbuf2, zonep[i].zone_name.str);
					/* compare same case */
					for (j=0; j<NBP_NVE_STR_SIZE; j++) {
						if (!tbuf1[j]) break;
						tbuf1[j] = toupper(tbuf1[j] );
						tbuf2[j] = toupper(tbuf2[j] );
					}
					if (!strcmp(tbuf1,tbuf2)) {				
						sprintf(errbuf,  MSGSTR(M_DUPE_ZONE,
							"duplicate zone entry"));
						bad = TRUE;
						break;
					}
				}
				/* save zone name */
				strcpy(zonep[zoneno].zone_name.str,buf1);
				zonep[zoneno].zone_name.len = len;
				break;
			default:	/* I/F's or home designation */
				if (buf1[0] == '*') {
					if (home++) {
						sprintf(errbuf, MSGSTR(M_DUPE_HOME,
							"home already designated on line %d"),homeLine);
						bad = TRUE;
						break;
					}
					homeLine = lineno;

					/* home zone must exist on home I/F */
					if (gotHomePort && 
					    !in_list(filecfgp->home_if,
						     &zonep[zoneno].zone_iflist)) {
						sprintf(errbuf, MSGSTR(M_HOME_MUST,
							"Zone designated as 'home' must contain\n"\
						        "the home I/F (%s)"),
							homePort);
						bad = TRUE;
						break;
					}
					done = TRUE;
					zonep[zoneno].zone_home = TRUE;
				}
				else {		/* not home, must be another I/F */
				    /* check for valid type and make
				       sure it has been defined too */
				    if (in_list(buf1, &filecfgp->used)) {
				        if (in_list(buf1, &seed_names)) {
					  strcpy(zonep[zoneno].zone_iflist.at_if[z_ind++], buf1);
					} else {
					  sprintf(errbuf, 
						MSGSTR(M_ALL_IFS_ASSOC,
						    "all interfaces associated with a "\
						    "zone must be seed type\n"\
						    "interface for this zone is non-seed (%s)"),
						buf1);
					  bad = TRUE;
					}
				    } else {
					sprintf(errbuf,  
						MSGSTR(M_INVAL_IF,
						       "invalid interface (%s)"), buf1);
					bad = TRUE;
				    }
				    break;
				}
			} /* end switch */
			parmno++;

			if (bad) {
				/* finish error msg */
				if(checkCfg) {
					sscanf(pc1,":%[^:\n]",linein);
					fprintf(stderr, MSGSTR(M_EZONE,
						"error, %s\nzone:%s\n"),errbuf,curzone);	
				}
				else
					fprintf(stderr, MSGSTR(M_LINE,
						"error, line %d. %s\n%s\n"),lineno,errbuf,linein);	
				error = TRUE;
				break;
			}
	
		} while(!done && pc1 && (pc1  = strtok(NULL,":")));
	}
	/* if no home zone set and home port is seed type, check to
	   see if there is only one zone assigned to that i/f. If
	   so, then make it the home zone
	*/
	if (gotHomePort && !home && 
	    (in_list(filecfgp->home_if, &seed_names))) {
		j = -1; 	/* j will be set to home zone */
		for (i=0; i<MAX_ZONES; i++) {
			if (!zonep[i].zone_name.len)
				break;
			if (in_list(filecfgp->home_if,&zonep[i].zone_iflist))
				if (j >=0 )	{
				    home = 0;	/* more than one zone assigned to home
						   port, we can't assume correct one */
				    break;
				}
				else {
				    home = 1;
				    j = i;	/* save 1st zone found */
				}
		}
		if (home) 
			zonep[j].zone_home = TRUE;
		else {
			error = TRUE; 
			fprintf(stderr, MSGSTR(M_NO_HOME_ZN,
				"error, no home zone designated\n")); 
		}
	}
	fclose(pFin);
	return(error? -1:0);
} /* getZoneInfo */

int ifcompare(v1,v2)
void *v1, *v2;
{
	return(strcmp(((at_if_cfg_t *)v1)->ifr_name, ((at_if_cfg_t *)v2)->ifr_name));
}

void showCfg(cfgp, if_zones)
     at_if_cfg_t *cfgp;
     zone_usage_t if_zones[];         /* zone info from cfg file;
					   not used in multihoming mode	*/
{
	int i,k, cnt=0, seed=FALSE;
	char range[40];
	qsort((void*)cfgp, IF_TOTAL_MAX, sizeof(*cfgp), ifcompare);
	for (i=0; i<IF_TOTAL_MAX; i++) {
		if (cfgp[i].ifr_name[0]) {
			if (!cnt++) {
				fprintf(STDOUT, 
					MSGSTR(M_RTR_CFG,"Router mode configuration:\n\n"));
				fprintf(STDOUT,
					MSGSTR(M_RTR_CFG_HDR,"H I/F  Network Range\n"));
				fprintf(STDOUT,"- ---  -------------\n");
			}
			range[0] = '\0';
			if (cfgp[i].netStart || cfgp[i].netEnd) {
				sprintf(range,"%5d - %d", cfgp[i].netStart, cfgp[i].netEnd); 
				seed=TRUE;
			}
			fprintf(STDOUT,"%c %-3s  %s\n", cfgp[i].flags & ELAP_CFG_HOME ? '*' : ' ',
				cfgp[i].ifr_name,
				range[0] ? range : MSGSTR(M_NONSEED,"(non-seed)"));
		}
	}
	if (!cnt) {
		fprintf(STDOUT, 
			MSGSTR(M_NO_IF_CFG, 
			"There are no interfaces configured\n"));
		return;
	}
	if (seed) {
		fprintf(STDOUT, "\n\n  %-32s  %s\n", 
			MSGSTR(M_ZONE_DEF,"Defined zones"),
			MSGSTR(M_IF_DEF_ZONE,"Interfaces Defining Zone"));
			fprintf(STDOUT,"  %-32s  %s\n",
    					 	   "-------------",
					   "------------------------");
	}

	for (i=0; i<MAX_ZONES && seed; i++)  {
		int ifcnt=0;
		if (!if_zones[i].zone_name.str[0])
			break;
		fprintf(STDOUT, "%c %-32s  ", if_zones[i].zone_home ? '*' : ' ',
			if_zones[i].zone_name.str);
		for (k=0; k<IF_TOTAL_MAX; k++)
			if (if_zones[i].zone_iflist.at_if[k][0]) {
				if (ifcnt && !((ifcnt)%ZONE_IFS_PER_LINE))
					fprintf(STDOUT,"\n%36s","");
				ifcnt++;
				fprintf(STDOUT, "%s ",
					if_zones[i].zone_iflist.at_if[k]);
			}
		fprintf(STDOUT, "\n");
	}
	fprintf(STDOUT, 
		MSGSTR(M_HOME_Z_IND,
		"\n* indicates home port and home zone\n"\
  	      "  (if home port is a seed port)\n"));
		
}


int checkSeeds(elapcfgp,zonep)
     at_if_cfg_t elapcfgp[];
     zone_usage_t zonep[];
{
	int i,j;
	int	ok;
	int	error=FALSE;

	for (i=0; i<IF_TOTAL_MAX; i++) {
		if (!elapcfgp[i].ifr_name[0])	
			break;
		if (!elapcfgp[i].netStart)		/* if no seed, skip */
			continue;
		ok = FALSE;
		for (j=0;j<MAX_ZONES; j++) {
			if (!zonep[j].zone_name.str[0])
				break;
			if (in_list(elapcfgp[i].ifr_name, 
				    &zonep[j].zone_iflist)) {
				ok = TRUE;
				break;
			}
		} /* for each zone */
		if (!ok) {
			fprintf(stderr, MSGSTR(M_SEED_WO_ZONE,
				"error, seed I/F without any zones: %s\n"),elapcfgp[i].ifr_name);
			error = -1;
		}
	}     /* for I/F type */
	return(error);
} /* checkSeeds */

int
showZones()
{
	int status;
	int size;
	int if_id;
	int done = FALSE;
	ZT_entryno zte;
	int did_header=0;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	*(int *)&zte = 1;
	while (1) {
		size = sizeof(ZT_entryno);
		status = at_send_to_dev(if_id, LAP_IOC_GET_ZONE, &zte, &size);
		if (status <0)
			switch (errno) {
				case EINVAL:
					done = TRUE;
					break;
				case 0:
					break;
				default:
					fprintf(stderr, MSGSTR(M_RET_ZONES,
						"showZones: error retrieving zone list\n"));
					goto error;
			}
		if (done)
			break;
		
		if (!did_header++) {
			fprintf(STDOUT, MSGSTR(M_ZONES,"..... Zones ......\n"));
			fprintf(STDOUT, MSGSTR(M_ZONE_HDR,"zno zcnt zone\n"));
		}
		zte.zt.Zone.str[zte.zt.Zone.len] = '\0';
		fprintf(STDOUT, "%3d  %3d %s\n", zte.entryno+1,zte.zt.ZoneCount, zte.zt.Zone.str);
		*(int *)&zte = 0;
	}
	if (*(int *)&zte == 1)
		fprintf(STDOUT, MSGSTR(M_NO_ZONES,"no zones found\n"));
	(void) close(if_id);
	return(0);
error:
	(void) close(if_id);
	return(-1);
}

int
showRoutes()
{
	int status;
	int size;
	int if_id;
	int i,j;
	int zcnt,gap;
	int done = FALSE;
	int did_header = FALSE;
	RT_entry rt;
	char state[10];

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	*(int *)&rt = 1;
	state[9] = '\0';
	while (1) {
		size = sizeof(RT_entry);
		status = at_send_to_dev(if_id, LAP_IOC_GET_ROUTE, &rt, &size);
		if (status < 0)
			switch (errno) {
				case EINVAL:
					done = TRUE;
					break;
				case 0:
					break;
				default:
					fprintf(STDOUT, MSGSTR(M_RET_ROUTES,
						"showRoutes: error retrieving route table\n"));
					goto error;
			}
		if (done)
			break;
		if (!did_header++)	{
			fprintf(STDOUT, MSGSTR(M_ROUTES,
				"............ Routes ................\n"));
			fprintf(STDOUT, MSGSTR(M_NXT_STATE,
				"                next             state\n"));
			fprintf(STDOUT, MSGSTR(M_RTR_HDR,
				"start-stop    net:node   d  p  PBTZ GSBU zones\n"));
		}
		gap = 0;
		for (i=0; i<8; i++) {
			if (i==4) {
				gap =1; 
				state[4] = ' ';
			}
			state[i+gap] =  (rt.EntryState & 1<<(7-i)) ? '1' : '0';
		}
		fprintf(STDOUT, "%5d-%-5d %5d:%-05d %2d %2d  %s ", 
			rt.NetStart, rt.NetStop, rt.NextIRNet, rt.NextIRNode, 
			rt.NetDist, rt.NetPort, state);
		zcnt = 0;
		for (i=0; i<ZT_BYTES; i++) {
			for (j=0; j<8; j++)
				if ((rt.ZoneBitMap[i] <<j) & 0x80) {
					if (zcnt >= Z_MAX_PRINT) { 
						fprintf(STDOUT, MSGSTR(M_MORE,",more..."));
						i = ZT_BYTES;
						break;
					}
					fprintf(STDOUT, zcnt ? ",%d" : " %d",i*8+j+1);	/* 1st zone is 1 not 0 */
					zcnt++;
				}
		}
		fprintf(STDOUT, "\n");
		*(int *)&rt = 0;
	}
	if (*(int *)&rt == 1)
		fprintf(STDOUT, MSGSTR(M_NO_ROUTES,"no routes found\n"));
	(void) close(if_id);
	return(0);
error:
	(void) close(if_id);
	return(-1);
}

void aurpd(fd)
	int fd;
{
	ATgetmsg(fd, 0, 0, 0);
}
