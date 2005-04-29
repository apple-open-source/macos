/*
 * vfs.r --
 *
 *	This file creates resources used by the vfs package.
 *
 */

#include <Types.r>
#include <SysTypes.r>

#define VFS_LIBRARY_RESOURCES 5000


#define VFS_PATCHLEVEL     0

#define VFS_VERSION        "1.0"
#define VFS_FULL_VERSION   "1.0.0"

#define	VFS_MAJOR			1				// BCD (0Ñ99)
#define	VFS_MINOR			0				// BCD (0Ñ9)
#define	VFS_PATCH			VFS_PATCHLEVEL	// BCD (0Ñ9)
#define	VFS_STAGE			finalStage		// {developStage, alphaStage, betaStage, finalStage}
#define VFS_PRERELEASE	0				// unsigned binary (0Ñ255)



// TclX version
// The appropriate resources are created automatically, below.
// Just set the values above appropriately

// Construct a version string
// 	Final (release) versions don't get stage or pre-release code
//	Include patch level only if non-zero
#if	(VFS_STAGE == finalStage)
#	if	VFS_PATCH != 0
#		define	VFS_VERSION_STRING	\
			$$Format("%d.%d.%d", VFS_MAJOR, VFS_MINOR, VFS_PATCH)
#	else
#		define	VFS_VERSION_STRING	\
			$$Format("%d.%d", VFS_MAJOR, VFS_MINOR)
#	endif
#else
#	if	(VFS_STAGE == developStage)
#		define	VFS_STAGE_CODE	'd'
#	elif	(VFS_STAGE == alphaStage)
#		define	VFS_STAGE_CODE	'a'
#	elif	(VFS_STAGE == betaStage)
#	 	define	VFS_STAGE_CODE	'b'
#	endif
#	if	VFS_PATCH != 0
#		define	VFS_VERSION_STRING	\
			$$Format("%d.%d.%d%c%d", VFS_MAJOR, VFS_MINOR, VFS_PATCH, \
						VFS_STAGE_CODE, VFS_PRERELEASE)
#	else
#		define	VFS_VERSION_STRING	\
			$$Format("%d.%d%c%d", VFS_MAJOR, VFS_MINOR, \
						VFS_STAGE_CODE, VFS_PRERELEASE)
#	endif
#endif

#define	VFS_MAJOR_BCD	((VFS_MAJOR / 10) * 16) + (VFS_MAJOR % 10)
#define	VFS_MINOR_BCD	(VFS_MINOR * 16) + VFS_PATCH

resource 'vers' (1) {
	VFS_MAJOR_BCD, VFS_MINOR_BCD, VFS_STAGE, VFS_PRERELEASE,
	verUS, VFS_VERSION_STRING,	
	$$Format("%s %s © %d\nby Vince Darley", 
					"vfs", VFS_VERSION_STRING, $$YEAR)
};

resource 'vers' (2) {
	VFS_MAJOR_BCD, VFS_MINOR_BCD, VFS_STAGE, VFS_PRERELEASE,
	verUS, VFS_VERSION_STRING,	
	$$Format("%s %s © %d", "vfs", VFS_VERSION_STRING, $$YEAR)
};

/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"vfs Library\n\n"
	"This is an implementation of a 'vfs' extension. "
	"To work properly, it "
	"should be placed in one of the $tcl_library paths, such as "
	"in the :Tool Command Language: folder "
	"within the :Extensions: folder."
};


/* 
 * We now load the vfs library into the resource fork of the library.
 * (generated from [glob library/*.tcl] via 
 * regsub {(([^\.\r]+)\.tcl)} {read 'TEXT' (VFS_LIBRARY_RESOURCES + $i, "\2", purgeable)   "\1";}
 */

read 'TEXT' (VFS_LIBRARY_RESOURCES    , "pkgIndex", purgeable, preload) "pkgIndex_mac.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 1, "vfs:tclIndex", purgeable) "tclIndex.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 2, "ftpvfs", purgeable)   "ftpvfs.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 3, "httpvfs", purgeable)   "httpvfs.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 4, "mk4vfs", purgeable)   "mk4vfs.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 6, "tclprocvfs", purgeable)   "tclprocvfs.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 7, "testvfs", purgeable)   "testvfs.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 9, "vfsUrl", purgeable)   "vfsUrl.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 10, "vfsUtils", purgeable)   "vfsUtils.tcl";
read 'TEXT' (VFS_LIBRARY_RESOURCES + 11, "zipvfs", purgeable)   "zipvfs.tcl";

