/* Put.c */

#include "Sys.h"

#include <signal.h>

#include "Util.h"
#include "RCmd.h"
#include "Xfer.h"
#include "Cmds.h"
#include "Get.h"
#include "Getopt.h"
#include "Glob.h"
#include "Put.h"

extern int gTransferType;
extern int gOptInd, gXferAbortFlag;

int
BinaryPut(char *remoteName, int infile, char *localName, long size)
{
	int result;
	XferSpecPtr xp;

	/* We don't do a SETBINARY here. Instead, we set it to gTransferType,
	 * which we know is not ascii.  Most often this will mean binary mode,
	 * but perhaps we're dealing with a tenex machine.
	 */
	SetType(gTransferType);

	/* Setup the parameter block to give to RDataCmd. */
	xp = InitXferSpec();
	xp->netMode = kNetWriting;
	xp->xProc = StdFileSend;
	xp->inStream = infile;
	/* xp->outStream = gDataSocket;  RDataCmd fills this in when it gets it. */
	
	/* This group is needed for the progress reporting and logging stuff.
	 * Otherwise, it isn't that important.
	 */
	xp->doReports = (size > 0) ? 1 : 0;
	xp->localFileName = localName;
	xp->remoteFileName = remoteName;
	xp->expectedSize = size;
	
	result = RDataCmd(xp, "STOR %s", remoteName);
	DoneWithXferSpec(xp);

	return (result);
}	/* BinaryPut */




int AsciiPut(char *remoteName, int infile, char *localName, long size)
{
	int result;
	XferSpecPtr xp;

	SETASCII;
	
	/* Setup the parameter block to give to RDataCmd. */
	xp = InitXferSpec();
	xp->netMode = kNetWriting;
	xp->xProc = StdAsciiFileSend;
	xp->inStream = infile;
	/* xp->outStream = gDataSocket;  RDataCmd fills this in when it gets it. */
	
	/* This group is needed for the progress reporting and logging stuff.
	 * Otherwise, it isn't that important.
	 */
	xp->doReports = (size > 0) ? 1 : 0;
	xp->localFileName = localName;
	xp->remoteFileName = remoteName;
	xp->expectedSize = size;

	result = RDataCmd(xp, "STOR %s", remoteName);
	DoneWithXferSpec(xp);

	return (result);
}	/* AsciiPut */




void GetLocalSendFileName(char *localName, char *remoteName, size_t siz)
{
	char *cp;
	
	/* Create a remote file name.  We want this to be just the same
	 * as the local file name, but with the directory path stripped off.
	 */ 
	cp = strrchr(localName, '/');
	if (cp == NULL)
		cp = localName;
	else
		cp++;
	
	Strncpy(remoteName, cp, siz);
}	/* GetLocalSendFileName */




int OpenLocalSendFile(char *localName, long *size)
{
	int fd;
	struct stat st;
	
	*size = kSizeUnknown;

	if ((fd = open(localName, O_RDONLY)) < 0) {
		Error(kDoPerror, "Can't open local file %s.\n", localName);
	} else {
		/* While we're here, get the size of the file.  This is useful
		 * for the progress reporting functions.
		 */
		if (stat(localName, &st) == 0)
			*size = st.st_size;
	}
	return fd;
}	/* OpenLocalSendFile */




int PutCmd(int argc, char **argv)
{
	int fd;
	int i, result, errs, opt;
	string remote;
	long fileSize;
	LineList globFiles;
	LinePtr globFile;
	int renameMode;

	errs = 0;
	renameMode = 0;
	GetoptReset();
	while ((opt = Getopt(argc, argv, "Rrz")) >= 0) {
		switch (opt) {
			case 'R':
			case 'r':
				PrintF("Recursive put not implemented yet.\n");
				break;
			case 'z':
				renameMode = 1;
				break;
			default:
				return (kUsageErr);
		}
	}
	argv += gOptInd;
	argc -= gOptInd;

	if (renameMode) {
		/* User wanted to transfer a local file, but name it different. */
		if (argc < 2)
			return (kUsageErr);
		fd = OpenLocalSendFile(argv[0], &fileSize);
		if (fd < 0) {
			--errs;
		} else {
			if (gTransferType == 'A') {		
				result = AsciiPut(
					argv[1],	/* Remote */
					fd,
					argv[0],	/* Local */
					fileSize
				);
			} else {
				result = BinaryPut(
					argv[1],	/* Remote */
					fd,
					argv[0],	/* Local */
					fileSize
				);
			}
			(void) close(fd);
			if (result < 0) {
				--errs;
				/* Maybe remove the remote file. */
			}
		}
	} else for (i=0; i<argc; i++) {
		InitLineList(&globFiles);
		LocalGlob(&globFiles, argv[i]);
		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			GetLocalSendFileName(globFile->line, remote, sizeof(remote));
			fd = OpenLocalSendFile(globFile->line, &fileSize);
			if (fd < 0)
				continue;
			if (gTransferType == 'A') {		
				result = AsciiPut(
					remote,
					fd,
					globFile->line,
					fileSize
				);
			} else {
				result = BinaryPut(
					remote,
					fd,
					globFile->line,
					fileSize
				);
			}
			(void) close(fd);
			if (result < 0) {
				--errs;
				/* Maybe remove the remote file. */
			}
			if (gXferAbortFlag == SIGINT)
				break;
		}
		DisposeLineListContents(&globFiles);
	}

	return (errs);
}	/* PutCmd */




int CreateCmd(int argcUNUSED, char **argv)
{
	int fd;
	string remote;
	int result;

	/* Good ol' /dev/null always returns EOF on reads. */
	fd = open("/dev/null", O_RDONLY);
	if (fd < 0)
		return (kCmdErr);
	
	STRNCPY(remote, argv[1]);
	result = BinaryPut(
		remote,
		fd,
		"/dev/null",
		0L
	);

	(void) close(fd);
	return (result);
}	/* CreateCmd */
