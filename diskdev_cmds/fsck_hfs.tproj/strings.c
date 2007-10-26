
/* status messages */
char * stat_msg[] =
{
	/* 1 - 5 */
	"Checking HFS volume.",
	"Checking Non-journaled HFS Plus volume.",
	"Checking Extents Overflow file.",
	"Checking Catalog file.",
	"Checking Catalog hierarchy.",

	/* 6 - 10 */
	"Checking Extended Attributes file.",
	"Checking volume bitmap.",
	"Checking volume information.",
	"Checking multi-linked files.",
	"Looking for orphaned extents.",

	/* 11 - 15 */
	"Rebuilding Extents Overflow B-tree.",
	"Rebuilding Catalog B-tree.",
	"Rebuilding Extended Attributes B-tree.",
	"Repairing volume.",
	"The volume %s needs to be repaired.",

	/* 16 - 20 */
	"The volume %s appears to be OK.",
	"The volume %s was repaired successfully.",
	"The volume %s could not be repaired.",
	"Volume check failed.",
	"Rechecking volume.",

	/* 21 - 25 */
	"Look for missing items in lost+found directory.",
	"Cannot repair volume when it is mounted with write access.",
	"Detected a case-sensitive catalog.",
	"The volume %s could not be repaired after 3 attempts.",
	"Look for links to corrupt files in DamagedFiles directory.",

	/* 26 - 29 */
	"Checking multi-linked directories.",
	"Checking Journaled HFS Plus volume.",
	"Performing live verification.",
	"Verifying volume when it is mounted with write access."
};


/* error messages */
char * err_msg[] =
{
	/* 500 - 509 */
	"Incorrect block count for file %s",
	"Incorrect size for file %s",
	"Invalid directory item count",
	"Invalid length for file name",
	"Invalid node height",
	"Missing file record for file thread",
	"Invalid allocation block size",
	"Invalid number of allocation blocks",
	"Invalid VBM start block",
	"Invalid allocation block start",

	/* 510 - 519 */
	"Invalid extent entry",
	"Overlapped extent allocation (file %s)",
	"Invalid BTH length",
	"BT map too short during repair",
	"Invalid root node number",
	"Invalid node type",
	"Invalid record count",
	"Invalid index key",
	"Invalid index link",
	"Invalid sibling link",

	/* 520 - 529 */
	"Invalid node structure",
	"Overlapped node allocation",
	"Invalid map node linkage",
	"Invalid key length",
	"Keys out of order",
	"Invalid map node",
	"Invalid header node",
	"Exceeded maximum B-tree depth",
	"Invalid catalog record type",
	"Invalid directory record length",

	/* 530 - 539 */
	"Invalid thread record length",
	"Invalid file record length",
	"Missing thread record for root dir",
	"Missing thread record (id = %s)",
	"Missing directory record (id = %s)",
	"Invalid key for thread record",
	"Invalid parent CName in thread record",
	"Invalid catalog record length",
	"Loop in directory hierarchy",
	"Invalid root directory count",

	/* 540 - 549 */
	"Invalid root file count",
	"Invalid volume directory count",
	"Invalid volume file count",
	"Invalid catalog PEOF",
	"Invalid extent file PEOF",
	"Nesting of folders has exceeded the recommended limit of 100",
	"File thread flag not set in file rec",
	"Reserved fields in the catalog record have incorrect data",
	"Invalid file name",
	"Invalid file clump size",

	/* 550 - 559 */
	"Invalid B-tree Header",
	"Directory name locked",
	"Catalog file entry not found for extent",
	"Invalid volume free block count",
	"Master Directory Block needs minor repair",
	"Volume Header needs minor repair",
	"Volume Bit Map needs minor repair",
	"Invalid B-tree node size",
	"Invalid leaf record count",
	"(It should be %s instead of %s)",

	/* 560 - 569 */
	"Invalid file or directory ID found",
	"I can't understand this version of HFS Plus",
	"Disk full error",
	"Internal files overlap (file %s)",
	"Invalid Volume Header",
	"HFS Wrapper volume needs repair",
	"Wrapper catalog file location needs repair",
	"Indirect node %s needs link count adjustment",
	"Orphaned open unlinked file %s",
	"Invalid BSD file type",

	/* 570 - 579 */
	"Invalid BSD User ID",
	"Illegal name",
	"Incorrect number of thread records",
	"Cannot create links to all corrupt files",
	"Invalid content in Journal",
	"Incorrect number of Extended Attributes",
	"Incorrect number of Access Control Lists",
	"Incorrect block count for attribute %s of file %s",
	"Incorrect size for attribute %s of file %s",
	"Invalid attribute record",

	/* 580 - 589 */
	"Incorrect folder count in a directory (id = %s)",
	"HasFolderCount flag needs to be set (id = %s)",  
	"Incorrect permissions for private directory", 
	"Incorrect flags for directory inode (id = %s)", 
	"Invalid parent for directory inode (id = %s)",
	"Invalid name for directory inode (id = %s)",
	"Incorrect number of directory hard links", 
	"Incorrect owner flags for directory hard link (id = %s)", 
	"Invalid finder info for directory hard link (id = %s)",
	"Incorrect flags for directory hard link ancestor (id = %s)",

	/* 590 - 599 */
	"Bad parent directory hierarchy (id = %s)",
	"Maximum nesting of folders and directory hard links reached",
	"Missing private directory for directory hard links",
	"Previous ID in a hard link chain is incorrect (id = %s)",
	"Next ID in a hard link chain is incorrect (id = %s)",
	"Incorrect flags for file inode (id = %s)", 
	"Invalid parent for file inode (id = %s)",
	"Invalid name for file inode (id = %s)",
	"Incorrect number of file hard links", 
	"Invalid finder info for file hard link (id = %s)",

	/* 600 - 606 */
	"Invalid first link in hard link chain (id = %s)",
	"Incorrect flags for file hard link (id = %s)",
	"Incorrect flags for directory hard link (id = %s)",
	"Orphaned file hard link (id = %s)",
	"Orphaned directory hard link (id = %s)",
	"Orphaned file inode (id = %s)", 
	"Orphaned directory inode (id = %s)"
};
