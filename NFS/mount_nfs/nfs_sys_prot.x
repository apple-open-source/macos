/*
 * Copyright (c) 2010-2013 Apple Inc. All rights reserved.
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
 * nfs_sys_prot.x
 */

/*
 * Basic typedefs for RFC 1832 data type definitions
 */
typedef int		int32_t;
typedef unsigned int	uint32_t;

/*
 * Basic data types
 */
typedef uint32_t	bitmap<>;
typedef opaque		utf8string<>;
typedef utf8string	utf8str_cis;
typedef utf8string	utf8str_cs;
typedef utf8str_cs	component;
typedef	component	pathname<>;
typedef	opaque		attrlist<>;

/* timespec */
struct nfstime32 {
	int32_t		seconds;
	uint32_t	nseconds;
};

/* a set of flags: which ones are set and what they are set to */
struct nfs_flag_set {
	bitmap		mask;	/* which flags are valid */
	bitmap		value;	/* what each flag is set to */
};

/* values for MATTR_LOCK_MODE */
enum nfs_lock_mode {
	NFS_LOCK_MODE_ENABLED = 0,
	NFS_LOCK_MODE_DISABLED = 1,
	NFS_LOCK_MODE_LOCAL = 2
};

/*
 * Filesystem locations attributes.
 *
 * This structure closely resembles the fs_locations and
 * fs_locations_info structures in NFSv4.
 *
 * An NFS file system can have multiple locations.
 * Each location consists of one or more servers that have
 * the file system available at a given server-side pathname.
 * Each server has a name and a list of addresses.
 *
 * The *_info elements are optional.  If they are included the
 * length of the element will be non-zero and the contents will
 * be described by the corresponding _info structure.
 */
struct nfs_fs_server_info {
	int32_t		nfssi_currency;
	opaque		nfssi_info<>;
};
struct nfs_fs_server {
	utf8str_cis	nfss_name;
	utf8str_cis	nfss_address<>;		/* universal addresses */
	opaque		nfss_server_info<>;	/* optional, contents described by nfs_fs_server_info */
};
struct nfs_fs_location {
	nfs_fs_server	nfsl_server<>;
	pathname	nfsl_rootpath;
};
struct nfs_fs_locations_info {
	uint32_t	nfsli_flags;
	int32_t		nfsli_valid_for;
	pathname	nfsli_root;
};
struct nfs_fs_locations {
	nfs_fs_location	nfsl_location<>;
	opaque		nfsl_locations_info<>;	/* optional, contents described by nfs_fs_locations_info */
};

/* NFS mount attribute container */
struct nfs_mattr {
	bitmap		attrmask;
	attrlist	attr_vals;
};

/* miscellaneous constants */
const NFS_XDRARGS_VERSION_0 = 0;		/* nfs_mount_args version */
const NFS_MATTR_BITMAP_LEN = 1;			/* # XDR words in mount attributes bitmap */
const NFS_MFLAG_BITMAP_LEN = 1;			/* # XDR words in mount flags bitmap */

/*
 * Mount attributes
 *
 * Additional mount attribute notes:
 *
 * Time value attributes are specified in second.nanosecond format but
 * mount arguments may be rounded to a more appropriate unit/increment.
 *
 * The supported string values for NFS_MATTR_SOCKET_TYPE:
 *     tcp    - use TCP over IPv4 or IPv6
 *     udp    - use UDP over IPv4 or IPv6
 *     tcp6   - use TCP over IPv6 only
 *     udp6   - use UDP over IPv6 only
 *     tcp4   - use TCP over IPv4 only
 *     udp4   - use UDP over IPv4 only
 *     inet   - use TCP or UDP over IPv4 or IPv6
 *     inet4  - use TCP or UDP over IPv4 only
 *     inet6  - use TCP or UDP over IPv6 only
 */

/* mount attribute types */
typedef nfs_flag_set		nfs_mattr_flags;
typedef uint32_t		nfs_mattr_nfs_version;
typedef uint32_t		nfs_mattr_nfs_minor_version;
typedef uint32_t		nfs_mattr_rsize;
typedef uint32_t		nfs_mattr_wsize;
typedef uint32_t		nfs_mattr_readdirsize;
typedef uint32_t		nfs_mattr_readahead;
typedef nfstime32		nfs_mattr_acregmin;
typedef nfstime32		nfs_mattr_acregmax;
typedef nfstime32		nfs_mattr_acdirmin;
typedef nfstime32		nfs_mattr_acdirmax;
typedef nfs_lock_mode		nfs_mattr_lock_mode;
typedef uint32_t		nfs_mattr_security<>;
typedef uint32_t		nfs_mattr_maxgrouplist;
typedef string			nfs_mattr_socket_type<>;
typedef uint32_t		nfs_mattr_nfs_port;
typedef uint32_t		nfs_mattr_mount_port;
typedef nfstime32		nfs_mattr_request_timeout;
typedef uint32_t		nfs_mattr_soft_retry_count;
typedef nfstime32		nfs_mattr_dead_timeout;
typedef	opaque			nfs_mattr_fh<NFS4_FHSIZE>;
typedef nfs_fs_locations	nfs_mattr_fs_locations;
typedef uint32_t		nfs_mattr_mntflags;
typedef string			nfs_mattr_mntfrom<MAXPATHLEN-1>;
typedef string			nfs_mattr_realm<MAXPATHLEN-1>;
typedef string			nfs_mattr_principal<MAXPATHLEN-1>;
typedef string			nfs_mattr_svcpinc<MAXPATHLEN-1>;

/* mount attribute bitmap indices */
const NFS_MATTR_FLAGS			= 0;	/* mount flags bitmap (MFLAG_*) */
const NFS_MATTR_NFS_VERSION		= 1;	/* NFS protocol version */
const NFS_MATTR_NFS_MINOR_VERSION	= 2;	/* NFS protocol minor version */
const NFS_MATTR_READ_SIZE		= 3;	/* READ RPC size */
const NFS_MATTR_WRITE_SIZE		= 4;	/* WRITE RPC size */
const NFS_MATTR_READDIR_SIZE		= 5;	/* READDIR RPC size */
const NFS_MATTR_READAHEAD		= 6;	/* block readahead count */
const NFS_MATTR_ATTRCACHE_REG_MIN	= 7;	/* minimum attribute cache time */
const NFS_MATTR_ATTRCACHE_REG_MAX	= 8;	/* maximum attribute cache time */
const NFS_MATTR_ATTRCACHE_DIR_MIN	= 9;	/* minimum attribute cache time for directories */
const NFS_MATTR_ATTRCACHE_DIR_MAX	= 10;	/* maximum attribute cache time for directories */
const NFS_MATTR_LOCK_MODE		= 11;	/* advisory file locking mode (nfs_lock_mode) */
const NFS_MATTR_SECURITY		= 12;	/* RPC security flavors to use */
const NFS_MATTR_MAX_GROUP_LIST		= 13;	/* max # of RPC AUTH_SYS groups */
const NFS_MATTR_SOCKET_TYPE		= 14;	/* socket transport type as a netid-like string */
const NFS_MATTR_NFS_PORT		= 15;	/* port # to use for NFS protocol */
const NFS_MATTR_MOUNT_PORT		= 16;	/* port # to use for MOUNT protocol */
const NFS_MATTR_REQUEST_TIMEOUT		= 17;	/* initial RPC request timeout value */
const NFS_MATTR_SOFT_RETRY_COUNT	= 18;	/* max RPC retransmissions for soft mounts */
const NFS_MATTR_DEAD_TIMEOUT		= 19;	/* how long until unresponsive mount is considered dead */
const NFS_MATTR_FH			= 20;	/* file handle for mount directory */
const NFS_MATTR_FS_LOCATIONS		= 21;	/* list of locations for the file system */
const NFS_MATTR_MNTFLAGS		= 22;	/* VFS mount flags (MNT_*) */
const NFS_MATTR_MNTFROM			= 23;	/* fixed string to use for "f_mntfromname" */
const NFS_MATTR_REALM			= 24;	/* Kerberos realm to use for authentication */
const NFS_MATTR_PRINCIPAL		= 25;	/* Principal to use for the mount */
const NFS_MATTR_SVCPRINCIPAL		= 26;	/* Kerberos principal of the server */

/*
 * Mount flags
 */
const NFS_MFLAG_SOFT			= 0;	/* soft mount (requests fail if unresponsive) */
const NFS_MFLAG_INTR			= 1;	/* allow operations to be interrupted */
const NFS_MFLAG_RESVPORT		= 2;	/* use a reserved port */
const NFS_MFLAG_NOCONNECT		= 3;	/* don't connect the socket (UDP) */
const NFS_MFLAG_DUMBTIMER		= 4;	/* don't estimate RTT dynamically */
const NFS_MFLAG_CALLUMNT		= 5;	/* call MOUNTPROC_UMNT on unmount */
const NFS_MFLAG_RDIRPLUS		= 6;	/* request additional info when reading directories */
const NFS_MFLAG_NONEGNAMECACHE		= 7;	/* don't do negative name caching */
const NFS_MFLAG_MUTEJUKEBOX		= 8;	/* don't treat jukebox errors as unresponsive */
const NFS_MFLAG_EPHEMERAL		= 9;	/* ephemeral (mirror) mount */
const NFS_MFLAG_NOCALLBACK		= 10;	/* don't provide callback RPC service */
const NFS_MFLAG_NONAMEDATTR		= 11;	/* don't use named attributes */
const NFS_MFLAG_NOACL			= 12;	/* don't support ACLs */
const NFS_MFLAG_ACLONLY			= 13;	/* only support ACLs - not mode */
const NFS_MFLAG_NFC			= 14;	/* send NFC strings */
const NFS_MFLAG_NOQUOTA			= 15;	/* don't support QUOTA requests */
const NFS_MFLAG_MNTUDP			= 16;	/* MOUNT protocol should use UDP */
const NFS_MFLAG_MNTQUICK		= 17;	/* use short timeouts while mounting */


/*
 * Arguments to mount an NFS file system
 *
 * Format of the buffer passed to NFS in the mount(2) system call.
 */
struct nfs_mount_args {
	uint32_t	args_version;		/* NFS_ARGSVERSION_XDR = 88 */
	uint32_t	args_length;		/* length of the entire nfs_mount_args structure */
	uint32_t	xdr_args_version;	/* version of nfs_mount_args structure */
	nfs_mattr	nfs_mount_attrs;	/* mount information */
};



/*
 * Mount Info attributes
 */

/* mount info attribute types */
typedef nfs_flag_set		nfs_miattr_flags;
typedef uint32_t		nfs_miattr_cur_loc_index[4];

/* mount info attribute bitmap indices */
const NFS_MIATTR_FLAGS			= 0;	/* mount info flags bitmap (MIFLAG_*) */
const NFS_MIATTR_ORIG_ARGS		= 1;	/* original mount args passed into mount call */
const NFS_MIATTR_CUR_ARGS		= 2;	/* current mount args values */
const NFS_MIATTR_CUR_LOC_INDEX		= 3;	/* current fs location index */

/*
 * Mount Info flags
 */
const NFS_MIFLAG_DEAD			= 0;	/* mount is dead */
const NFS_MIFLAG_NOTRESP		= 1;	/* server is unresponsive */
const NFS_MIFLAG_RECOVERY		= 2;	/* mount in recovery */

/* NFS mount info attribute container */
struct nfs_miattr {
	bitmap		attrmask;
	attrlist	attr_vals;
};

/* miscellaneous constants */
const NFS_MOUNT_INFO_VERSION = 0;		/* nfs_mount_info version */
const NFS_MIATTR_BITMAP_LEN = 1;		/* # XDR words in mount info attributes bitmap */
const NFS_MIFLAG_BITMAP_LEN = 1;		/* # XDR words in mount info flags bitmap */

/*
 * NFS mount information as returned by NFS_MOUNTINFO sysctl.
 */
struct nfs_mount_info {
	uint32_t	info_version;		/* NFS_MOUNT_INFO_VERSION = 0 */
	uint32_t	info_length;		/* length of the entire nfs_mount_info structure */
	nfs_miattr	nfs_mountinfo_attrs;	/* mount information attributes */
};

