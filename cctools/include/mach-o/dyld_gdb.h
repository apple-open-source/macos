#ifndef _DYLD_GDB_
#define _DYLD_GDB_
/*
 * This file describes the interface between gdb and dyld created for
 * MacOS X GM.  Prior to MacOS X GM gdb used the dyld_debug interfaces
 * described in <mach-o/dyld_debug.h>.
 */

/*
 * gdb_dyld_version is the version of gdb interface that dyld is currently
 * exporting.  For the interface described in this header file gdb_dyld_version
 * is 1.  As the gdb/dyld interface changes this number will be incremented and
 * comments will be added as to what are the are changes for the various
 * versions.
 */
extern unsigned int gdb_dyld_version;

/* 
 * gdb_dyld_state_changed is the internal dyld routine called by dyld to notify
 * gdb that the state of the data structures has changed.  gdb is expected to
 * put a break point on this routine and re-read the internal dyld data
 * structures below when this break point is hit.
 */
extern void gdb_dyld_state_changed(void);

/*
 * gdb looks directly at parts of two of dyld's internal data structures.  The
 * list of object file images and the list of library images.  The parts of
 * these structures that gdb looks at will not change unless the value of
 * gdb_dyld_version changes.  The size of these structures and the other fields
 * that gdb does not look at may change.
 *
 *  struct object_images {
 *      struct object_image images[NOBJECT_IMAGES];
 *      unsigned long nimages; 
 *      struct object_images *next_images; 
 *      ...
 *  };
 *
 *  struct library_images { 
 *      struct library_image images[NLIBRARY_IMAGES];
 *      unsigned long nimages;
 *      struct library_images *next_images;
 *      ...
 *  };
 *
 * Both the object_image structure and the library_image structure
 * start with a structure containing the following fields:
 *
 *  struct image {   
 *      char *name;                 image name for reporting errors
 *      unsigned long vmaddr_slide; the slide from the staticly linked address
 *      struct mach_header *mh;     address of the mach header of the image
 *	unsigned long valid;        TRUE if this is struct is valid
 *      ...
 *  };
 */

/* object_images is the global object_images structure */

/* the number of gdb_object_image structures present per bucket */
extern unsigned int gdb_nobject_images;

/* the size of each gdb_object_image structure */
extern unsigned int gdb_object_image_size;

/* library_images is the global library_images structure */

/* the number of gdb_library_image structures present per bucket */
extern unsigned int gdb_nlibrary_images;

/* the size of each gdb_library_image structure */
extern unsigned int gdb_library_image_size;

#endif /* _DYLD_GDB_ */
