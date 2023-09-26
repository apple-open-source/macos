/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "ext.h"
#include "lib_fsck_msdos.h"

int checkfilesys(const char *fname, check_context context)
{
	char *phasesNamesFails[6] = {"Check device: Checking parameters: Failed", "Check device: open file: Failed", "Check device: read boot sector: Failed",
                                  "Check device: Quick check: Failed", "Check device: Perform check: Failed", "Check device: Finish: Failed"};
	char *phasesNames[6] = {"Check device: Checking parameters", "Check device: open file", "Check device: read boot sector",
							"Check device: Quick check", "Check device: Perform check", "Check device: Finish"};
	int reportProgress = (context != NULL && context->updater != NULL &&
                          context->startPhase != NULL && context->endPhase != NULL);
	unsigned int progressTracker = 0;
	int finish_dosdirsection = 0;
	bool close_dosfs = true;
	int tryOthersAgain = 3;
	struct bootblock boot;
	int tryFatalAgain = 1;
	int tryErrorAgain = 3;
	int ret = FSERROR;
	int currPhase = 0;
	char raw_path[64];
	int dosfs = -1;
	int mod = 0;

	if (reportProgress) {
		context->startPhase(phasesNames[currPhase], 1, 3, &progressTracker,
                            context->updater);
	}

	/*
	 * We accept device paths of the form "/dev/disk6s1" or "disk6s1"
	 * and convert them to the raw path "/dev/rdisk6s1".
	 *
	 * On iOS, we want to be able to run fsck_msdos as user "mobile"
	 * so that it can be spawned from userfsd, which also runs as "mobile".
	 * But since /dev/disk entries are owned by root, we can't open them
	 * directly.  userfsd has already used an SPI to open the disk device,
	 * so it will spawn fsck_msdos with that open file descriptor, and use
	 * a path of the form "/dev/fd/NUMBER". Just parse NUMBER from the
	 * fname and use it as FD for the device to check.
	 */
	if (fsck_fd() >= 0)
	{
		dosfs = fsck_fd();
		close_dosfs = false;
	}
	else if (!strncmp(fname, "/dev/disk", 9))
	{
		if (snprintf(raw_path, sizeof(raw_path), "/dev/r%s", &fname[5]) < sizeof(raw_path))
			fname = raw_path;
		/* Else we just use the non-raw disk */
	}
	else if (!strncmp(fname, "disk", 4))
	{
		if (snprintf(raw_path, sizeof(raw_path), "/dev/r%s", fname) < sizeof(raw_path))
			fname = raw_path;
		/* Else the open below is likely to fail */
	}
	else if (!strncmp(fname, "/dev/fd/", 8))
	{
		char *end_ptr;
		dosfs = (int)strtol(&fname[8], &end_ptr, 10);
		if (*end_ptr)
		{
			// TODO: is errx or err the right way to report the errors here?
			fsck_print(fsck_ctx, LOG_CRIT, "Invalid file descriptor path: %s", fname);
			ret = 8;
			goto out;
		}
	}
	progressTracker++;

	if (dosfs >= 0)
	{
		struct stat info;
		if (fstat(dosfs, &info))
		{
			fsck_print(fsck_ctx, LOG_CRIT, "%s (%s)\n", "Cannot stat", strerror(errno));
			ret = 8;
			goto out;
		}

		/* Note: Passed in fd can be seeked to arbitrary location, bellow code
		 *       assumes that fd is seeked to 0, and uses read instead of pread.
		 *       To make everything work, just reset fd to 0.
		 */
		if (lseek(dosfs, 0, SEEK_SET) == -1)
		{
			fsck_print(fsck_ctx, LOG_CRIT, "%s (%s)\n", "Cannot seek", strerror(errno));
			ret = 8;
			goto out;
		}
	}
	progressTracker++;

	fsck_set_rdonly(fsck_alwaysno() || fsck_quick());
	if (!fsck_preen()) {
		fsck_print(fsck_ctx, LOG_INFO, "** %s", fname);
	}
	progressTracker++;

    if (reportProgress) {
        context->endPhase(phasesNames[currPhase++], context->updater);
        progressTracker = 0;
        context->startPhase(phasesNames[currPhase], 1, 3, &progressTracker,
                            context->updater);
    }

	if (dosfs < 0) {
		dosfs = open(fname, fsck_rdonly() ? O_RDONLY : O_RDWR | O_EXLOCK, 0);
		close_dosfs = true;
	}
	progressTracker++;

	if (dosfs < 0 && !fsck_rdonly()) {
		dosfs = open(fname, O_RDONLY, 0);
		close_dosfs = true;
		if (dosfs >= 0)
			fsck_print(fsck_ctx, LOG_INFO, "Warning:  (NO WRITE)\n");
		else if (!fsck_preen())
			fsck_print(fsck_ctx, LOG_INFO, "\n");
		fsck_set_rdonly(true);
	} else if (!fsck_preen())
		fsck_print(fsck_ctx, LOG_INFO, "\n");
	progressTracker++;

	if (dosfs < 0) {
		fsck_print(fsck_ctx, LOG_CRIT, "%s (%s)\n", "Can't open", strerror(errno));
		ret = 8;
		goto out;
	}
	progressTracker++;

	if (reportProgress) {
		context->endPhase(phasesNames[currPhase++], context->updater);
		progressTracker = 0;
		context->startPhase(phasesNames[currPhase], 2, 1, &progressTracker,
                            context->updater);
    }

	mod = readboot(dosfs, &boot);
	if (mod & FSFATAL) {
		if (close_dosfs) {
			close(dosfs);
		}
		ret = 8;
		goto out;
	}
	progressTracker++;
	if (reportProgress) {
		context->endPhase(phasesNames[currPhase++], context->updater);
		progressTracker = 0;
	}

	if (fsck_quick()) {
		if (reportProgress) {
		 context->startPhase(phasesNames[currPhase], 19, 1, &progressTracker,
							 context->updater);
		}
		/*
		 * FAT12 volumes don't have a dirty bit.  If we were asked for
		 * a quick check, then actually do a full scan without repairs.
		 */
		if (boot.ClustMask == CLUST12_MASK) {
			/* Don't attempt to do any repairs */
			fsck_set_rdonly(true);
			fsck_set_alwaysno(1);
			fsck_set_alwaysyes(0);
			fsck_set_quick(1);

			/* Finish the quick check phase */
			if (reportProgress) {
				context->endPhase(phasesNames[currPhase++], context->updater);
				progressTracker = 0;
			}
			/* Go verify the volume */
			goto Again;
		}
		else if (isdirty(dosfs, &boot, boot.ValidFat >= 0 ? boot.ValidFat : 0)) {
			fsck_print(fsck_ctx, LOG_INFO, "Warning: FILESYSTEM DIRTY; SKIPPING CHECKS\n");
			ret = FSDIRTY;
		}
		else {
			fsck_print(fsck_ctx, LOG_INFO, "Warning: FILESYSTEM CLEAN; SKIPPING CHECKS\n");
			ret = 0;
		}
		
		if (close_dosfs) {
			close(dosfs);
		}

		/* Finish the quick check phase */
		if (reportProgress) {
			context->endPhase(phasesNames[currPhase++], context->updater);
			currPhase++; // Skip over perform check phase
			progressTracker = 0;
		}
		goto out;
	} else {
		currPhase++; // Skip over quick check phase
	}

Again:
	if (reportProgress) {
		uint64_t units = fsck_quick() ? 58: 77;
		context->startPhase(phasesNames[currPhase], units, 6, &progressTracker, context->updater);
	}
	boot.NumFiles = 0;	/* Reset file count in case we loop back here */
	boot.NumBad = 0;

	/*
	 * [2771127] When there was no active FAT, this code used to
	 * compare FAT #0 with all the other FATs.  That doubled the
	 * already large memory usage, and doesn't seem very useful.
	 * Microsoft's specification says the purpose of the alternate
	 * FATs is in case a sector goes bad in the main FAT.  In fact,
	 * a Windows 2000 system never even notices the FATs have
	 * different values.  Besides, the filesystem itself only ever
	 * uses FAT #0.
	 */
	if (!fsck_preen() && !fsck_quiet()) {
		fsck_print(fsck_ctx, LOG_INFO, "** Phase 1 - Preparing FAT\n");
	}
	progressTracker++;

	mod |= fat_init(dosfs, &boot);
	
	if (mod & FSFATAL) {
		if (close_dosfs) {
			close(dosfs);
		}
		ret = FSERROR;
        goto out;
	}
	progressTracker++;

	if (!fsck_preen() && !fsck_quiet())
		fsck_print(fsck_ctx, LOG_INFO, "** Phase 2 - Checking Directories\n");

	mod |= resetDosDirSection(&boot);
	finish_dosdirsection = 1;
	if (mod & FSFATAL)
		goto out;
	/* delay writing FATs */

	progressTracker++;

	mod |= handleDirTree(dosfs, &boot, fsck_rdonly());
	if (mod & FSFATAL)
		goto out;

	if (!fsck_preen() && !fsck_quiet())
		fsck_print(fsck_ctx, LOG_INFO, "** Phase 3 - Checking for Orphan Clusters\n");

	progressTracker++;

	/*
	 * Should we skip this if (mod & FSERROR)?  It would be bad to free clusters
	 * that are actually referenced by some directory entry, even if they are
	 * beyond the logical file size (and the file size was not repaired).
	 */
	mod |= fat_free_unused();
	if (mod & FSFATAL)
		goto out;

	if (fsck_quick()) {
		if (mod) {
			fsck_print(fsck_ctx, LOG_INFO, "FILESYSTEM DIRTY\n");
			ret = FSDIRTY;
		}
		else {
			fsck_print(fsck_ctx, LOG_INFO, "FILESYSTEM CLEAN\n");
			ret = 0;
		}
	}

	if (boot.NumBad)
		fsck_print(fsck_ctx, LOG_INFO, "Warning: %d files, %lld KiB free (%d clusters), %lld KiB bad (%d clusters)\n",
			  boot.NumFiles,
			  (long long) boot.NumFree * (long long) boot.ClusterSize / 1024LL, boot.NumFree,
			  (long long) boot.NumBad * (long long) boot.ClusterSize / 1024LL, boot.NumBad);
	else
		fsck_print(fsck_ctx, LOG_INFO, "Warning: %d files, %lld KiB free (%d clusters)\n",
			  boot.NumFiles,
			  (long long) boot.NumFree * (long long) boot.ClusterSize / 1024LL, boot.NumFree);

	/*
	 * If we repaired everything, offer to mark the file system clean.
	 */
	if (mod && (mod & FSERROR) == 0) {
		if (mod & FSDIRTY) {
			if (fsck_ask(fsck_ctx, 1, "MARK FILE SYSTEM CLEAN") == 0)
				mod &= ~FSDIRTY;

			if (mod & FSDIRTY) {
				fsck_print(fsck_ctx, LOG_INFO, "Warning: MARKING FILE SYSTEM CLEAN\n");
				mod |= fat_mark_clean();
			} else {
				fsck_print(fsck_ctx, LOG_INFO, "Warning: \n***** FILE SYSTEM IS LEFT MARKED AS DIRTY *****\n");
			}
		}
	}
	progressTracker++;

	/*
	 * We're done changing the FAT, so flush any changes.
	 */
	mod |= fat_flush();
	progressTracker++;

	/* Don't bother trying multiple times if we're not doing repairs */
	if (mod && fsck_rdonly())
		goto out;

	if (((mod & FSFATAL) && (--tryFatalAgain > 0)) ||
	    ((mod & FSERROR) && (--tryErrorAgain > 0)) ||
	    ((mod & FSFIXFAT) && (--tryOthersAgain > 0))) {
		mod = 0;

		goto Again;
	}
	if ((mod & (FSFATAL | FSERROR)) == 0)
		ret = 0;

out:
	if (reportProgress) {
		if (ret) {
			context->endPhase(phasesNamesFails[currPhase++], context->updater);
		} else {
			context->endPhase(phasesNames[currPhase++], context->updater);
		}
		progressTracker = 0;
		context->startPhase(phasesNames[currPhase], 19, 1, &progressTracker,
							context->updater);
	}

	if (finish_dosdirsection)
		finishDosDirSection();

	progressTracker++;

	fat_uninit();
	freeUseMap();
	if (close_dosfs) {
		close(dosfs);
	}

	if (mod & (FSFATMOD|FSDIRMOD))
		fsck_print(fsck_ctx, LOG_INFO, "Warning: \n***** FILE SYSTEM WAS MODIFIED *****\n");
	if (reportProgress) {
		context->endPhase(phasesNames[currPhase], context->updater);
	}

	return ret;
}
