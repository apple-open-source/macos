/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*
 * Copyright (c) 1980, 1986, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fstab.h>
#include <err.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <TargetConditionals.h>

#include "fsck.h"

/* Local Static Functions */
static int argtoi(int flag, char *req, char *str, int base);
static void usage();
static int startdiskcheck(disk_t* disk);

/* Global Variables */
int preen = 0;				/* We're checking all fs'es in 'preen' mode */
int returntosingle = 0;		/* Return a non-zero status to prevent multi-user start up */
int hotroot= 0;				/* We're checking / (slash or root) */
int	fscks_running = 0;		/* Number of currently running fs checks */
int ndisks = 0;				/* Total number of disks observed */
int debug = 0;				/* Output Debugging info */
int force_fsck = 0;			/* Force an fsck even if the underlying FS is clean */
int maximum_running = 0;	/* Maximum number of sub-processes we'll allow to be spawned */
int quick_check = 0;		/* Do a quick check.  Quick check returns clean, dirty, or fail */

/*
 * The two following globals are mutually exclusive; you cannot assume "yes" and "no."
 * The last one observed will be the one that is honored.  e.g. fsck -fdnyny will result 
 * in assume_yes == 1, and assume_no == 0;
 */
int assume_no = 0;			/* If set, assume a "no" response to repair requests */
int assume_yes = 0;			/* If set, assume a "yes" response to repair requests. */

disk_t *disklist = NULL;	/* Disk struct with embedded list to enum. all disks */
part_t *badlist = NULL;		/* List of partitions which may have had errors */

static int argtoi(int flag, char *req, char *str, int base) {
    char *cp;
    int ret;
	
    ret = (int)strtol(str, &cp, base);
    if (cp == str || *cp)
        errx(EEXIT, "-%c flag requires a %s", flag, req);
    return (ret);
}

static void usage(void) {
	fprintf(stderr, "fsck usage: fsck [-fdnypq] [-l number]\n");
}

#if DEBUG
void debug_args (void);
void dump_part (part_t* part);
void dump_disk (disk_t* disk);
void dump_fsp (struct fstab *fsp);

void debug_args (void) {
	if (debug) {
		printf("debug %d\n", debug);
	}
	
	if (force_fsck) {
		printf("force_fsck %d\n", force_fsck);
	}
	
	if (assume_no) {
		printf("assume_no: %d\n", assume_no);
	}
	
	if (assume_yes) {
		printf("assume_yes: %d\n", assume_yes);
	}
	
	if (preen) {
		printf("preen: %d\n", preen);
	}
	
	if (quick_check) {
		printf("quick check %d\n", quick_check);
	}
	
	printf("maximum_running %d\n", maximum_running);
}

void dump_fsp (struct fstab *fsp) {
	fprintf (stderr, "**********dumping fstab entry %p**********\n", fsp);
	fprintf (stderr, "fstab->fs_spec: %s\n", fsp->fs_spec);
	fprintf (stderr, "fstab->fs_file: %s\n", fsp->fs_file);
	fprintf (stderr, "fstab->fs_vfstype: %s\n", fsp->fs_vfstype);
	fprintf (stderr, "fstab->fs_mntops: %s\n", fsp->fs_mntops);
	fprintf (stderr, "fstab->fs_type: %s\n", fsp->fs_type);
	fprintf (stderr, "fstab->fs_freq: %d\n", fsp->fs_freq);
	fprintf (stderr, "fstab->fs_passno: %d\n", fsp->fs_passno);
	fprintf (stderr, "********** finished dumping fstab entry %p**********\n\n\n", fsp);

}

void dump_disk (disk_t* disk) {
	part_t *part;
	fprintf (stderr, "**********dumping disk entry %p**********\n", disk);
	fprintf (stderr, "disk->name: %s\n", disk->name);
	fprintf (stderr, "disk->next: %p\n", disk->next);
	fprintf (stderr, "disk->part: %p\n", disk->part);
	fprintf (stderr, "disk->pid: %d\n\n", disk->pid);
	
	part = disk->part;
	if (part) {
		fprintf(stderr, "dumping partition entries now... \n");
	}
	while (part) {
		dump_part (part);
		part = part->next;
	}
	fprintf (stderr, "**********done dumping disk entry %p**********\n\n\n", disk);

}

void dump_part (part_t* part) {
	fprintf (stderr, "**********dumping partition entry %p**********\n", part);
	fprintf (stderr, "part->next: %p\n", part->next);
	fprintf (stderr, "part->name: %s\n", part->name);
	fprintf (stderr, "part->fsname: %s\n", part->fsname);
	fprintf (stderr, "part->vfstype: %s\n\n", part->vfstype);
	fprintf (stderr, "**********done dumping partition entry %p**********\n\n\n", part);
}

#endif



int main (int argc, char** argv) {
	/* for getopt */
	extern char *optarg;
	extern int optind;
	
	int ch;
	int ret;
	
	sync();
	while ((ch = getopt(argc, argv, "dfpqnNyYl:")) != EOF) {
		switch (ch) {
			case 'd':
				debug++;
				break;
				
			case 'l':
				maximum_running = argtoi('l', "number", optarg, 10);
				break;	
				
			case 'f':
				force_fsck++;
				break;
								
			case 'N':
			case 'n':
				assume_no = 1;
				assume_yes = 0;
				break;
			
			case 'p':
				preen++;
				break;
			
			case 'q':
				quick_check = 1;
				break;
			
			case 'Y':
			case 'y':
				assume_yes = 1;
				assume_no = 0;
				break;
				
			default:
				errx(EEXIT, "%c option?", ch);
				break;
		}		
	}
	argc -= optind;
	argv += optind;
	
	/* Install our signal handlers */
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		(void)signal(SIGINT, catchsig);
	}
	
	if (preen) {
		(void)signal(SIGQUIT, catchquit);
	}
	
	if (argc) {
		/* We do not support any extra arguments at this time */
		ret = EINVAL;
		usage();
		exit(ret);
	}
	
	/* 
	 * checkfstab does the bulk of work for fsck.  It will scan through the
	 * fstab and iterate through the devices, as needed	
	 */
	ret = checkfstab();
	/* Return a non-zero return status so that we'll stay in single-user */
	if (returntosingle) {
		exit(2);
	}
	/* Return the error value obtained from checking all filesystems in checkfstab */
	exit(ret);
	
}

/*
 * This is now the guts of fsck. 
 *
 * This function will iterate over all of the elements in the fstab and run 
 * fsck-like binaries on all of the filesystems in question if able.  The root filesystem
 * is checked first, and then non-root filesystems are checked in order.
 */
int checkfstab(void) {
	int running_status = 0;
	int ret;
	
	/* Open the fstab file (or rewind it) */
	if (setfsent() == 0) {
		fprintf(stderr, "Can't open checklist file: %s\n", _PATH_FSTAB);
		return EEXIT;
	}
	
	ret = build_disklist ();	
	/* 
	 * If we encountered any errors or if 'preen' was off,
	 * then we must have scanned everything. Either way, return. 
	 */
	if ((ret) || (preen == 0)) {
		return ret;
	}
	
	if (preen) {
		/* Otherwise, see if we need to do a cursory fsck against the FS. */
		ret = do_diskchecks();
		running_status |= ret;
	}

	
	if (running_status) {
		part_t *part = NULL;
		
		if (badlist == NULL) {
			/* If there were no disk problems, then return the status */
			return (running_status);
		}
		fprintf(stderr, "THE FOLLOWING FILE SYSTEM%s HAD AN %s\n\t",
				badlist->next ? "S" : "", "UNEXPECTED INCONSISTENCY:");
		for (part = badlist; part; part = part->next) {
			fprintf(stderr, "%s (%s)%s", part->name, part->fsname, part->next ? ", " : "\n");
		}
		return (running_status);
	}
	
	(void)endfsent();
	return (0);
	
}

/*
 * This function builds up the list of disks that fsck will need to 
 * process and check.  
 *
 * If we're not in 'preen' mode, then we'll go ahead and do the full 
 * check on all of them now.  
 * 
 * If we ARE in 'preen' mode, then we'll just check the root fs, and log
 * all of the other ones that we encounter by scanning through the fstab
 * for checking a bit later on.  See notes below for checking '/' at boot. 
 */
int build_disklist(void) {
	
	struct fstab *fsp = NULL;
	int passno = 0;
	char *name;
	int retval;
	int running_status = 0;

	 
	/*
	 * We may need to iterate over the elements in the fstab in non-sequential order.
	 * Thus, we take up to two passes to check all fstab fsck-eligible FSes.  The first
	 * pass should focus on the root filesystem, which can be inferred from the fsp->fs_passno 
	 * field.  The library code used to fill in the fsp structure will specify an 
	 * fsp->fs_passno == 1 for the root. All other filesystems should get fsp->fs_passno == 2.
	 * (See fstab manpage for more info.)
	 */
	for (passno = 1; passno <= 2; passno++) {
		/* Open or reset the fstab entry */
		if (setfsent() == 0) {
			fprintf(stderr, "Can't open checklist file: %s\n", _PATH_FSTAB);
			return EEXIT;
		}
		/* Iterate through the fs entries returned from fstab */
		while ((fsp = getfsent()) != 0) {			
			/* 
			 * Determine if the filesystem is worth checking. Ignore it if it
			 * is not checkable. 
			 */
			if (fs_checkable(fsp) == 0) {
				continue;
			}
			/*
			 * If preen is off, then we will wind up checking everything in order, so 
			 * go ahead and just check this item now.
			 *
			 * Otherwise, only work on the root filesystem in the first pass.  We can
			 * tell that the fsp represents the root filesystem if fsp->fs_passno == 1.
			 *
			 * NOTE: On Mac OSX, LibInfo, part of Libsystem is in charge of vending us a valid
			 * fstab entry when we're running 'fsck -p' in early boot to ensure the validity of the 
			 * boot disk.  Since it's before the volume is mounted read-write, getfsent() will probe
			 * the Mach port for directory services.  Since it's not up yet, it will determine the
			 * underlying /dev/disk entry for '/' and mechanically construct a fstab entry for / here.
			 * It correctly fills in the passno field below, which will allow us to fork/exec in order
			 * to call fsck_XXX as necessary.
			 *
			 * Once we're booted to multi-user, this block of code shouldn't ever really check anything
			 * unless it's a valid fstab entry because the synthesized fstab entries don't supply a passno
			 * field.  Also, they would have to be valid /dev/disk fstab entries as opposed to 
			 * UUID or LABEL ones. 
			 */
			if (preen == 0 || (passno == 1 && fsp->fs_passno == 1)) {
				/* Take the special device name, and do some cursory checks. */
				if ((name = blockcheck(fsp->fs_spec)) != 0) {
					/* Construct a temporary disk_t for checkfilesys */
					disk_t disk;
					part_t part;
					
					disk.name = NULL;
					disk.next = NULL;
					disk.part = &part;
					disk.pid = 0;
					
					part.next = NULL;
					part.name = name;
					part.vfstype = fsp->fs_vfstype;
					
					/* Run the filesystem check against the filesystem in question */
					if ((retval = checkfilesys(&disk, 0)) != 0) {
						return (retval);
					}
				} 
				else {
					fprintf(stderr, "BAD DISK NAME %s\n", fsp->fs_spec);
					/* 
					 * If we get here, then blockcheck failed (returned NULL).  
					 * Presumably, we couldn't stat the disk device.  In this case,
					 * just bail out because we couldn't even find all of the
					 * entries in the fstab. 
					 */
					return EEXIT;
				}
			} 
			/*
			 * If we get here, then preen must be ON and we're checking a 
			 * non-root filesystem. So we must be on the 2nd pass, and 
			 * the passno of the element returned from fstab will be > 1.
			 */
			else if (passno == 2 && fsp->fs_passno > 1) {
				/* 
				 * If the block device checks tell us the name is bad, mark it in the status 
				 * field and continue 
				 */
				if ((name = blockcheck(fsp->fs_spec)) == NULL) {
					fprintf(stderr, "BAD DISK NAME %s\n", fsp->fs_spec);
					running_status |= 8;
					continue;
				}
				/* 
				 * Since we haven't actually done anything yet, add this partition 
				 * to the list of devices to check later on.
				 */
				addpart(name, fsp->fs_file, fsp->fs_vfstype);
			}
		}
		/* If we're not in preen mode, then we scanned everything already. Just bail out */
		if (preen == 0) {
			break;
		}
	}
	
	return running_status;
	
}

/*
 * This function only runs if we're operating in 'preen' mode.  If so,
 * then iterate over our list of non-root filesystems and fork/exec 'fsck_XXX'
 * on them to actually do the checking.  Spawn up to 'maximum_running' processes.
 * If 'maximum_running' was not set, then default to the number of disk devices
 * that we encounter. 
 */ 
int do_diskchecks(void) {
	
	int fsckno = 0;
	int pid = 0;
	int exitstatus = 0;
	int retval = 0;
	int running_status = 0;
	disk_t *disk = NULL;
	disk_t *nextdisk = NULL;
		
	/* 
	 * If we were not specified a maximum number of FS's to check at once, 
	 * or the max exceeded the number of disks we observed, then clip it to 
	 * the maximum number of disks.
	 */
	if ((maximum_running == 0) || (maximum_running > ndisks)) {
		maximum_running = ndisks;
	}		
	nextdisk = disklist;
	
	/* Start as many fscks as we will allow */
	for (fsckno = 0; fsckno < maximum_running; ++fsckno) {
		/*
		 * Run the disk check against the disk devices we have seen.
		 * 'fscks_running' is increased for each disk we actually visit.
		 * If we hit an error then sleep for 10 seconds and just try again. 
		 */ 		
		while ((retval = startdiskcheck(nextdisk)) && fscks_running > 0) {
			sleep(10);
		}
		if (retval) {
			return (retval);
		}
		nextdisk = nextdisk->next;
	}
	
	/* 
	 * Once we get limited by 'maximum_running' as to the maximum 
	 * number of processes we can spawn at a time, wait until one of our 
	 * child processes exits before spawning another one.
	 */
	while ((pid = wait(&exitstatus)) != -1) {		
		for (disk = disklist; disk; disk = disk->next) {
			if (disk->pid == pid) {
				break;
			}
		}
		if (disk == 0) {
			/* Couldn't find a new disk to check */
			printf("Unknown pid %d\n", pid);
			continue;
		}
		/* Check the WIFEXITED macros */
		if (WIFEXITED(exitstatus)) {
			retval = WEXITSTATUS(exitstatus);
		}
		else {
			retval = 0;
		}
		if (WIFSIGNALED(exitstatus)) {
			printf("%s (%s): EXITED WITH SIGNAL %d\n",
				   disk->part->name, disk->part->fsname,
				   WTERMSIG(exitstatus));
			retval = 8;
		}
		/* If it hit an error, OR in the value into our running total */
		if (retval != 0) {
			part_t *temp_part = badlist;
			
			/* Add the bad partition to the bad partition list */
			running_status |= retval;
			badlist = disk->part;
			disk->part = disk->part->next;
			if (temp_part) {
				badlist->next = temp_part;
			}
		} else {
			/* Just move to the next partition */
			part_t *temp_part = disk->part;
			disk->part = disk->part->next;
			destroy_part (temp_part);
		}
		/* 
		 * Reset the pid to 0 since this partition was checked.
		 * Decrease the number of running processes. Decrease the 
		 * number of disks if we finish one off.
		 */
		disk->pid = 0;
		fscks_running--;
		
		if (disk->part == NULL) {
			ndisks--;
		}
		
		if (nextdisk == NULL) {
			if (disk->part) {
				/* Start the next partition up */
				while ((retval = startdiskcheck(disk)) && fscks_running > 0) {
					sleep(10);
				}
				if (retval) {
					return (retval);
				}
			}
		} 
		else if (fscks_running < maximum_running && fscks_running < ndisks) {
			/* If there's more room to go, then find the next valid disk */
			for ( ;; ) {
				if ((nextdisk = nextdisk->next) == NULL) {
					nextdisk = disklist;
				}
				if (nextdisk->part != NULL && nextdisk->pid == 0) {
					break;
				}
			}
			
			while ((retval = startdiskcheck(nextdisk)) && fscks_running > 0) {
				sleep(10);
			}
			if (retval) {
				return (retval);
			}
		}
	}
	return running_status;
	
}

/*
 * fork/exec in order to spawn a process that will eventually
 * wait4() on the fsck_XXX process.
 * 
 * Note: The number of forks/execs here is slightly complicated.
 * We call fork twice, and exec once.  The reason we need three total
 * processes is that the first will continue on as the main line of execution.
 * This first fork() will create the second process which calls checkfilesys().  
 * In checkfilesys() we will call fork again, followed by an exec.  Observe that
 * the second process created here will *immediately* call wait4 on the third
 * process, fsck_XXX. This is so that we can track error dialogs and exit statuses 
 * and tie them to the specific instance of fsck_XXX that created them. Otherwise, 
 * if we just called fork a bunch of times and waited on the first one to finish, 
 * it would be difficult to tell which process exited first, and whether or not the
 * exit status is meaningful.  
 *
 * Also note that after we get our result from checkfilesys(), we immediately exit,
 * so that this process doesn't linger and accidentally continue on. 
 */
static 
int startdiskcheck(disk_t* disk) {
	
	/* 
	 * Split this process into the one that will go 
	 * call fsck_XX and the one that won't 
	 */
	disk->pid = fork();
	if (disk->pid < 0) {
		perror("fork");
		return (8);
	}
	if (disk->pid == 0) {
		/* 
		 * Call checkfilesys.  Note the exit() call.  Also note that
		 * we pass 1 to checkfilesys since we are a child process 
		 */
		exit(checkfilesys(disk, 1));
	}
	else {
		fscks_running++;
	}
	return (0);
}




/*
 * Call fork/exec in order to spawn instance of fsck_XXX for the filesystem
 * of the specified vfstype. This will actually spawn the process that does the
 * checking of the filesystem in question. 
 */
int checkfilesys(disk_t *disk, int child) {
#define ARGC_MAX 4	/* cmd-name, options, device, NULL-termination */
	part_t *part = disk->part;
	const char *argv[ARGC_MAX];
	int argc;
	int error = 0;
	struct stat buf;
	pid_t pid;
	int status = 0;
	char options[] = "-pdfnyq";  /* constant strings are not on the stack */
	char progname[NAME_MAX];
	char execname[MAXPATHLEN + 1];
	char* filesys = part->name;
	char* vfstype = part->vfstype;
	
	if (preen && child) {
		(void)signal(SIGQUIT, ignore_single_quit);
	}
	/* 
	 * If there was a vfstype specified, then we can go ahead and fork/exec
	 * the child fsck process if the fsck_XXX binary exists.
	 */
	if (vfstype) {
		int exitstatus;

		bzero(options, sizeof(options));
		snprintf(options, sizeof(options), "-%s%s%s%s%s%s",
				 (preen) ? "p" : "",
				 (debug) ? "d" : "",
				 (force_fsck) ? "f" : "",
				 (assume_no) ? "n" : "",
				 (assume_yes) ? "y" : "",
				 (quick_check) ? "q" : ""
				 );
		
		argc = 0;
		snprintf(progname, sizeof(progname), "fsck_%s", vfstype);
		argv[argc++] = progname;
		if (strlen(options) > 1) {
			argv[argc++] = options;
		}
		argv[argc++] = filesys;
		argv[argc] = NULL;
		
		/* Create the string to the fsck binary */
		(void)snprintf(execname, sizeof(execname), "%s/fsck_%s", _PATH_SBIN, vfstype);
		
		/* Check that the binary exists */
		error = stat (execname, &buf);
		if (error != 0) {
			fprintf(stderr, "Filesystem cannot be checked \n");
			return EEXIT;
		}

		pid = fork();
		switch (pid) {
			case -1:
				/* The fork failed. */			
				fprintf(stderr, "fork failed for %s \n", filesys);
				if (preen) {
					fprintf(stderr, "\n%s: UNEXPECTED INCONSISTENCY; RUN fsck MANUALLY.\n",
							filesys);
					exit(EEXIT);
				}
				
				status = EEXIT;
				break;
				
			case 0:
				/* The child */
				if (preen) {
					(void)signal(SIGQUIT, ignore_single_quit);
				}
				execv(execname, (char * const *)argv);
				fprintf(stderr, "error attempting to exec %s\n", execname);
				_exit(8);
				break;
				
			default:
				/* The parent; child is process "pid" */
				waitpid(pid, &exitstatus, 0);
				if (WIFEXITED(exitstatus)) {
					status = WEXITSTATUS(exitstatus);
				}
				else {
					status = 0;
				}
				if (WIFSIGNALED(exitstatus)) {
					printf("%s (%s) EXITED WITH SIGNAL %d\n", filesys, vfstype, WTERMSIG(exitstatus));
					status = 8;
				}
				break;
		}
		
		return status;
	}
	else {
		fprintf(stderr, "Filesystem cannot be checked \n");
		return EEXIT;
	}
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void catchquit(int sig) {
	extern int returntosingle;
	
	printf("returning to single-user after filesystem check\n");
	returntosingle = 1;
	(void)signal(SIGQUIT, SIG_DFL);
}

/* Quit if we catch a signal here. Emit 12 */
void catchsig(int sig) {
	exit (12);
}


/*
 * Determine whether a filesystem should be checked.
 *
 * Zero indicates that no check should be performed.
 */
int fs_checkable(struct fstab *fsp) {
	
	/*
	 * HFS, MSDOS, exfat, and UDF are allowed for now. 
	 */
	if (strcmp(fsp->fs_vfstype, "hfs")	&&
		strcmp(fsp->fs_vfstype, "msdos") &&
		strcmp(fsp->fs_vfstype, "exfat") && 
	    strcmp(fsp->fs_vfstype, "udf"))	{
		return 0;
	}
	
	/* if not RW and not RO (SW or XX?), ignore it */
	if ((strcmp(fsp->fs_type, FSTAB_RW) && strcmp(fsp->fs_type, FSTAB_RO)) ||
	    fsp->fs_passno == 0) {
		return 0;
	}
	
#define	DISKARB_LABEL	"LABEL="
#define	DISKARB_UUID	"UUID="
	/* If LABEL  or UUID specified, ignore it */
	if ((strncmp(fsp->fs_spec, DISKARB_LABEL, strlen(DISKARB_LABEL)) == 0)
		|| (strncmp(fsp->fs_spec, DISKARB_UUID, strlen(DISKARB_UUID)) == 0)) {
		return 0;
	}
	
	/* Otherwise, it looks fine. Go ahead and check! */
	return 1;
}

/*
 * Do some cursory checks on the pathname provided to ensure that it's really a block
 * device. If it is, then generate the raw device name and vend it out. 
 */
char *blockcheck (char *origname) {
	struct stat stslash;
	struct stat stblock;
	struct stat stchar;
	
	char *newname;
	char *raw;
	int retried = 0;
	int error = 0;
	
#if TARGET_OS_EMBEDDED
	/* Variables for setting up the kqueue listener*/
#define TIMEOUT_SEC 30l	
	struct kevent kev;
	struct kevent results;
	struct timespec ts;
	int slashdev_fd;
	int kq = -1;
	int ct;
	time_t end;
	time_t now;
#endif
	
	hotroot = 0;
	/* Try to get device info for '/' */
	if (stat("/", &stslash) < 0) {
		perror("/");
		/* If we can't get any info on root, then bail out */
		printf("Can't stat root\n");
		return (origname);
	}
	newname = origname;
	
retry:
	/* Poke the block device argument */
	error = stat(newname, &stblock);
	if (error < 0) {
#if TARGET_OS_EMBEDDED
		/* 
		 * If the device node is not present, set up 
		 * a kqueue and wait for up to 30 seconds for it to be
		 * published.
		 */
		kq = kqueue();
		if (kq < 0) {
			printf("kqueue: could not create kqueue: %d\n", errno);
			printf("Can't stat %s\n", newname);
			return NULL;
		}
		slashdev_fd = open(_PATH_DEV, O_RDONLY);
		
		EV_SET(&kev, slashdev_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_WRITE, 0, NULL);
		ct = kevent(kq, &kev, 1, NULL, 0, NULL);
		if (ct != 0) {
			printf("kevent() failed to register: %d\n", errno);
			printf("Can't stat %s\n", newname);
			/* If we can't register the kqueue, bail out */
			close (kq);
			kq = -1;
			return NULL;
		}
		now = time(NULL);
		end = now + TIMEOUT_SEC;
		
		ts.tv_nsec = 0;
		while ((now = time(NULL)) < end) {
			ts.tv_sec = end - now;
			ct = kevent(kq, NULL, 0, &results, 1, &ts);
			if (results.flags & EV_ERROR) {
				/* If we register any errors, bail out */
				printf("kevent: registered errors.\n");
				error = -1;
				close (kq);
				kq = -1;
				break;
			}
			error = stat (newname, &stblock);
			if (error == 0) {
				/* found the item. continue on */
				if (kq >= 0) {
					close (kq);
				}
				break;
			}
		}
		if (error != 0) {
			/* Time out.  bail out */
			if (kq >= 0) {
				close(kq);
			}
			printf("fsck timed out. Can't stat %s\n", newname);
			return NULL;
		}
		
#else 
		perror(newname);
		printf("Can't stat %s\n", newname);
		return (NULL);
#endif
	}

	if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
		/* 
		 * If the block device we're checking is the same as '/' then
		 * update hotroot global for debugging.
		 */
		if (stslash.st_dev == stblock.st_rdev) {
			hotroot++;
		}
		raw = rawname(newname);
		if (stat(raw, &stchar) < 0) {
			perror(raw);
			printf("Can't stat %s\n", raw);
			return (origname);
		}
		if ((stchar.st_mode & S_IFMT) == S_IFCHR) {
			return (raw);
		} else {
			printf("%s is not a character device\n", raw);
			return (origname);
		}
	} else if ((stblock.st_mode & S_IFMT) == S_IFCHR && !retried) {
		newname = unrawname(newname);
		retried++;
		goto retry;
	}
	/*
	 * Not a block or character device, return NULL and
	 * let the user decide what to do.
	 */
	return (NULL);
}


/*
 * Generate a raw disk device pathname from a normal one.
 * 
 * For input /dev/disk1s2, generate /dev/rdisk1s2
 */
char *rawname(char *name) {
	static char rawbuf[32];
	char *dp;
	
	/* 
	 * Search for the last '/' in the pathname.
	 * If it's not there, then bail out 
	 */
	if ((dp = strrchr(name, '/')) == 0) {
		return (0);
	}
	/* 
	 * Insert a NULL in the place of the final '/' so that we can 
	 * copy everything BEFORE that last '/' into a separate buffer.
	 */
	*dp = 0;
	(void)strlcpy(rawbuf, name, sizeof(rawbuf));
	*dp = '/';
	/* Now add an /r to our buffer, then copy everything after the final / */
	(void)strlcat(rawbuf, "/r", sizeof(rawbuf));
	(void)strlcat(rawbuf, &dp[1], sizeof(rawbuf));
	return (rawbuf);
}

/*
 * Generate a regular disk device name from a raw one. 
 *
 * For input /dev/rdisk1s2, generate /dev/disk1s2
 */
char *unrawname(char *name) {
	char *dp;
	struct stat stb;
	int length;
	
	/* Find the last '/' in the pathname */
	if ((dp = strrchr(name, '/')) == 0) {
		return (name);
	}
	
	/* Stat the disk device argument */
	if (stat(name, &stb) < 0) {
		return (name);
	}
	
	/* If it's not a character device, error out */
	if ((stb.st_mode & S_IFMT) != S_IFCHR) {
		return (name);
	}
	
	/* If it's not a real raw name, then error out */
	if (dp[1] != 'r') {
		return (name);
	}
	length = strlen(&dp[2]);
	length++; /* to account for trailing NULL */
	
	memmove(&dp[1], &dp[2], length);
	return (name);
}

/*
 * Given a pathname to a disk device, generate the relevant disk_t for that 
 * disk device.  It is assumed that this function will be called for each item in the
 * fstab that needs to get checked. 
 */
disk_t *finddisk (char *pathname) {
	disk_t *disk;
	disk_t **dkp;
	char *tmp;
	size_t len;
	
	/*
	 * Find the disk name.  It is assumed that the disk name ends with the
	 * first run of digit(s) in the last component of the path.
	 */
	tmp = strrchr(pathname, '/');		/* Find the last component of the path */
	if (tmp == NULL) {
		tmp = pathname;
	}
	else {
		tmp++;
	}
	for (; *tmp && !isdigit(*tmp); tmp++) {	/* Skip non-digits */
		continue;
	}
	
	for (; *tmp && isdigit(*tmp); tmp++){	/* Skip to end of consecutive digits */
		continue;
	}
	
	len = tmp - pathname;
	if (len == 0) {
		len = strlen(pathname);
	}
	
	/* Iterate through all known disks to see if this item was already seen before */
	for (disk = disklist, dkp = &disklist; disk; dkp = &disk->next, disk = disk->next) {
		if ((strncmp(disk->name, pathname, len) == 0) && 
			(disk->name[len] == 0)) {
			return (disk);
		}
	}
	/* If not, then allocate a new structure and add it to the end of the list */
	if ((*dkp = (disk_t*)malloc(sizeof(disk_t))) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	/* Make 'disk' point to the newly allocated structure */
	disk = *dkp;
	if ((disk->name = malloc(len + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	/* copy the name into place */
	(void)strncpy(disk->name, pathname, len);
	disk->name[len] = '\0';
	/* Initialize 'part' and 'next' to NULL for now */
	disk->part = NULL;
	disk->next = NULL;
	disk->pid = 0;
	/* Increase total number of disks observed */
	ndisks++;
	
	/* Bubble out either the newly created disk_t or the one we found */
	return (disk);
}


/*
 * Add this partition to the list of devices to check. 
 */ 
void addpart(char *name, char *fsname, char *vfstype) {
	disk_t *disk;
	part_t *part;
	part_t **ppt;
	
	/* Find the disk_t that corresponds to our element */
	disk = finddisk(name);
	ppt = &(disk->part);
	
	/* 
	 * Now iterate through all of the partitions of that disk.
	 * If we see our partition name already in there, then it means the entry
	 * was in the fstab more than once, which is bad. 
	 */
	for (part = disk->part; part; ppt = &part->next, part = part->next) {
		if (strcmp(part->name, name) == 0) {
			printf("%s in fstab more than once!\n", name);
			return;
		}
	}
	
	/* Hopefully we get here. Allocate a new partition structure for the disk */
	if ((*ppt = (part_t*)malloc(sizeof(part_t))) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	part = *ppt;
	if ((part->name = malloc(strlen(name) + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	
	/* Add the name & vfs info to the partition struct */
	(void)strcpy(part->name, name);
	if ((part->fsname = malloc(strlen(fsname) + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	(void)strcpy(part->fsname, fsname);
	part->next = NULL;
	part->vfstype = strdup(vfstype);
	if (part->vfstype == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
}

/*
 * Free the partition and its fields. 
 */
void destroy_part (part_t *part) {
	if (part->name) {
		free (part->name);
	}
	
	if (part->fsname) {
		free (part->fsname);
	}
	
	if (part->vfstype) {
		free (part->vfstype);
	}
	
	free (part);
}


/*
 * Ignore a single quit signal; wait and flush just in case.
 * Used by child processes in preen mode.
 */
void
ignore_single_quit(int sig) {
	
    sleep(1);
    (void)signal(SIGQUIT, SIG_IGN);
    (void)signal(SIGQUIT, SIG_DFL);
}



