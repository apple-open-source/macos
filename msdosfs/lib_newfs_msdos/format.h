//
//  format.h
//  newfs_msdos
//  This file contains the functions that were used to format a disk copied from newfs_msdos.c file.
//  This file allows us to encapsulate the format logic into this file, and use the "format" function to format a device
//  from newfs_msdos CLI tool, or from FSKit.
//
//  Created by Kujan Lauz on 06/09/2022.
//

#ifndef format_h
#define format_h

#include "newfs_data_types.h"
#include "lib_newfs_msdos.h"

typedef struct {
    const char *bootStrapFromFile;  // Get bootstrap from file.
    const char *volumeName;         // Volume name (filesystem name), up to 11 characters.  The name should consist of only those characters permitted in regular DOS (8+3) filenames.
    const char *OEMString;          // OEM string (up to 8 characters).  The default is "BSD  4.4" (with two spaces).
    const char *standardFormat;     // Specify a standard (floppy disk) format.  The eight standard formats are (capacities in kilobytes): 160, 180, 320, 360, 640, 720, 1200, 1232, 1440, 2880.
    u_int FATType;                  // FAT type (one of 12, 16, or 32).
    u_int volumeID;                 // Volume ID.
    u_int sectorSize;               // Number of bytes per sector.  Acceptable values are powers of 2 in the range 128 through 32768.
    u_int numOfSectorsPerFAT;       // Number of sectors per FAT.
    u_int blockSize;                // File system block size (bytes per cluster).  This should resolve to an acceptable number of sectors per cluster (see clusterSize).
    u_int clusterSize;              // Sectors per cluster.  Acceptable values are powers of 2 in the range 1 through 128.
    u_int numOfRootDirEnts;         // Number of root directory entries (FAT12 and FAT16 only).
    u_int numDriveHeads;            // Number of drive heads.
    u_int systemSectorLocation;     // Location of the file system info sector (FAT32 only).  A value of 0xffff signifies no info sector.
    u_int backupSectorLocation;     // Location of the backup boot sector (FAT32 only).  A value of 0xffff signifies no backup sector.
    u_int mediaDescriptor;          // Media descriptor (acceptable range 0xf0 to 0xff).
    u_int numbOfFATs;               // Number of FATs.  Acceptable values are 1 to 16 inclusive.  The default is 2.
    u_int numOfHiddenSectors;       // Number of hidden sectors.
    u_int numOfReservedSectors;     // Number of reserved sectors.
    u_int fsSizeInSectors;          // File system size, in sectors.
    u_int numOfSectorsPerTrack;     // Number of sectors per track.
    u_int physicalBytes;            // Physical bytes/sector
    int dryRun;                     // Don't create file system, just print out parameters
    int volumeIDFlag;               // Volume ID flag
    int mediaDescriptorFlag;        // Media descriptor flag
    int hiddenSectorsFlag;          // Hidden sectors flag
} NewfsOptions;

typedef struct format_context_s {
    void *updater;
    void (*startPhase)(char *description, int64_t pendingUnits, int64_t totalCount, unsigned int *completedCount, void *updater);
    void (*endPhase)(char *description, void *updater);
    void *resource;
    size_t (*readHelper)(void *resource, void *buffer, size_t nbytes, off_t offset);
    size_t (*writeHelper)(void *resource, void *buffer, size_t nbytes, off_t offset);

} *format_context;

int format(NewfsOptions sopts, NewfsProperties newfsProps, format_context context);
int getstdfmt(const char *, struct bpb *);
int getdiskinfo(NewfsProperties newfsProps, int oflag, struct bpb *bpb);
enum SDCardType sd_card_type_for_path(const char *path);
void sd_card_set_defaults(const char *path, u_int *fat, struct bpb *bpb);
void print_bpb(struct bpb *);
u_int argtou(const char *, u_int, u_int, const char *);
int oklabel(const char *);
void mklabel(u_int8_t *, const char *);
void setstr(u_int8_t *, const char *, size_t);

#endif /* format_h */
