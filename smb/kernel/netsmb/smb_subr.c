/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2013 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/kpi_mbuf.h>
#include <sys/vnode.h>

#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_converter.h>
#include <smbclient/ntstatus.h>


static const struct {
	uint32_t nterr;
	uint32_t errno;
} nt2errno[] = {
	{STATUS_LOGON_TYPE_NOT_GRANTED,	EAUTH},
	{STATUS_NO_LOGON_SERVERS,		EAUTH},	/* Server can't talk to domain, just return eauth */
	{STATUS_ACCESS_DENIED,			EACCES},
	{STATUS_ACCESS_VIOLATION,		EACCES},
	{STATUS_ACCOUNT_DISABLED,		SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */
	{STATUS_ACCOUNT_RESTRICTION,	SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */	
	{STATUS_LOGIN_TIME_RESTRICTION,	SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */	
	{STATUS_ACCOUNT_LOCKED_OUT,		SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */	
	{STATUS_INVALID_ACCOUNT_NAME,	SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */ 
	{STATUS_PWD_TOO_SHORT,			SMB_ENETFSPWDPOLICY},			/* violates password policy */	
	{STATUS_PWD_TOO_RECENT,			SMB_ENETFSPWDPOLICY},			/* violates password policy */
	{STATUS_PWD_HISTORY_CONFLICT,	SMB_ENETFSPWDPOLICY},			/* violates password policy */	
	{STATUS_ACCOUNT_EXPIRED,		SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */	
	{STATUS_PASSWORD_EXPIRED,		SMB_ENETFSPWDNEEDSCHANGE},		/* change your password */
	{STATUS_PASSWORD_RESTRICTION,	SMB_ENETFSPWDPOLICY},			/* violates password policy */
	{STATUS_PASSWORD_MUST_CHANGE,	SMB_ENETFSPWDNEEDSCHANGE},		/* change your password */
	{STATUS_INVALID_LOGON_HOURS,	SMB_ENETFSACCOUNTRESTRICTED},	/* ask the admin for help */
	{STATUS_ADDRESS_ALREADY_EXISTS,	EADDRINUSE},
	{STATUS_BAD_NETWORK_NAME,		ENOENT},
	{STATUS_NETWORK_NAME_DELETED,	ENOENT},
	{STATUS_DUPLICATE_NAME,			ENOENT},
	{STATUS_BAD_NETWORK_PATH,		ENOENT},
	{STATUS_BUFFER_TOO_SMALL,		E2BIG},
	{STATUS_INVALID_BUFFER_SIZE,	EIO},
	{STATUS_CONFLICTING_ADDRESSES,	EADDRINUSE},
	{STATUS_CONNECTION_ABORTED,		ECONNABORTED},
	{STATUS_CONNECTION_DISCONNECTED,ECONNABORTED},
	{STATUS_CONNECTION_REFUSED,		ECONNREFUSED},
	{STATUS_CONNECTION_RESET,		ENETRESET},
	{STATUS_DEVICE_DOES_NOT_EXIST,	ENODEV},
	{STATUS_DEVICE_PROTOCOL_ERROR,	EPROTO},
	{STATUS_DIRECTORY_NOT_EMPTY,	ENOTEMPTY},
	{STATUS_DISK_FULL,				ENOSPC},
	{STATUS_DLL_NOT_FOUND,			ENOENT},
	{STATUS_END_OF_FILE,			ENODATA},
	{STATUS_FILE_IS_A_DIRECTORY,	EISDIR},
	{STATUS_FILE_LOCK_CONFLICT,		EIO}, /* Return the IO error (AFP Like) and let the lock code reset it */ 
	{STATUS_LOCK_NOT_GRANTED,		EACCES},
	{STATUS_FLOAT_INEXACT_RESULT,	ERANGE},
	{STATUS_FLOAT_OVERFLOW,			ERANGE},
	{STATUS_FLOAT_UNDERFLOW,		ERANGE},
	{STATUS_HOST_UNREACHABLE,		EHOSTUNREACH},
	{STATUS_ILL_FORMED_PASSWORD,	EAUTH},
	{STATUS_INTEGER_OVERFLOW,		ERANGE},
	{STATUS_FILE_CLOSED,			EBADF},
	{STATUS_INVALID_HANDLE,			EBADF},
	{STATUS_INVALID_PARAMETER,		EINVAL},
	{STATUS_INVALID_PIPE_STATE,		EPIPE},
	{STATUS_INVALID_WORKSTATION,	EACCES},
	{STATUS_IN_PAGE_ERROR,			EFAULT},
	{STATUS_IO_TIMEOUT,				ETIMEDOUT},
	{STATUS_IP_ADDRESS_CONFLICT1,	EADDRINUSE},
	{STATUS_IP_ADDRESS_CONFLICT2,	EADDRINUSE},
	{STATUS_LICENSE_QUOTA_EXCEEDED,	EDQUOT},
	{STATUS_LOGON_FAILURE,			EAUTH},
	{STATUS_MEDIA_WRITE_PROTECTED,	EROFS},
	{STATUS_MEMORY_NOT_ALLOCATED,	EFAULT},
	{STATUS_NAME_TOO_LONG,			ENAMETOOLONG},
	{STATUS_NETWORK_ACCESS_DENIED,	EACCES},
	{STATUS_NETWORK_BUSY,			EBUSY},
	{STATUS_NETWORK_UNREACHABLE,	ENETUNREACH},
	{STATUS_NET_WRITE_FAULT,		EIO},
	{STATUS_NONEXISTENT_SECTOR,		ESPIPE},
	{STATUS_NOT_A_DIRECTORY,		ENOTDIR},
	{STATUS_NOT_IMPLEMENTED,		ENOTSUP},
	{STATUS_NOT_MAPPED_VIEW,		EINVAL},
	{STATUS_NOT_SUPPORTED,			ENOTSUP},
	{STATUS_NO_MEDIA,				EIO},
	{STATUS_IO_DEVICE_ERROR,		EIO},
	{STATUS_NO_MEDIA_IN_DEVICE,		EIO},
	{STATUS_NO_MEMORY,				ENOMEM},
	{STATUS_NO_SUCH_DEVICE,			ENODEV},
	{STATUS_NO_SUCH_FILE,			ENOENT},
	{STATUS_OBJECT_NAME_COLLISION,	EEXIST},
	{STATUS_OBJECT_NAME_NOT_FOUND,	ENOENT},
	{STATUS_OBJECT_PATH_INVALID,	ENOTDIR},
	{STATUS_OBJECT_PATH_NOT_FOUND,	ENOENT},
	{STATUS_PAGEFILE_QUOTA,			EDQUOT},
	{STATUS_PATH_NOT_COVERED,		ENOENT},
	{STATUS_NOT_A_REPARSE_POINT,	ENOENT},
	{STATUS_IO_REPARSE_TAG_MISMATCH,EIO},
	{STATUS_IO_REPARSE_DATA_INVALID,EIO},
	{STATUS_IO_REPARSE_TAG_NOT_HANDLED,	EIO},
	{STATUS_REPARSE_POINT_NOT_RESOLVED,	ENOENT},
	{STATUS_PIPE_BROKEN,			EPIPE},
	{STATUS_PIPE_BUSY,				EPIPE},
	{STATUS_PIPE_CONNECTED,			EISCONN},
	{STATUS_PIPE_DISCONNECTED,		EPIPE},
	{STATUS_SMB_BAD_TID,			ENOENT},
	{STATUS_INSUFFICIENT_RESOURCES, EAGAIN},
	{STATUS_INSUFF_SERVER_RESOURCES, EAGAIN},
	{STATUS_PIPE_NOT_AVAILABLE,		ENOSYS},
	{STATUS_PORT_CONNECTION_REFUSED,ECONNREFUSED},
	{STATUS_PORT_MESSAGE_TOO_LONG,	EMSGSIZE},
	{STATUS_PORT_UNREACHABLE,		EHOSTUNREACH},
	{STATUS_PROTOCOL_UNREACHABLE,	ENOPROTOOPT},
	{STATUS_QUOTA_EXCEEDED,			EDQUOT},
	{STATUS_REGISTRY_QUOTA_LIMIT,	EDQUOT},
	{STATUS_REMOTE_DISCONNECT,		ESHUTDOWN},
	{STATUS_REMOTE_NOT_LISTENING,	ECONNREFUSED},
	{STATUS_REQUEST_NOT_ACCEPTED,	EUSERS},
	{STATUS_RETRY,					EAGAIN},
	{STATUS_SHARING_VIOLATION,		EBUSY},
	{STATUS_TIMER_NOT_CANCELED,		ETIME},
	{STATUS_TOO_MANY_LINKS,			EMLINK},
	{STATUS_TOO_MANY_OPENED_FILES,	EMFILE},
	{STATUS_UNABLE_TO_FREE_VM,		EADDRINUSE},
	{STATUS_UNSUCCESSFUL,			EINVAL},
	{STATUS_WRONG_PASSWORD,			EAUTH},
	{STATUS_DELETE_PENDING,			EACCES},
	{STATUS_OBJECT_NAME_INVALID,	ENAMETOOLONG},
	{STATUS_CANNOT_DELETE,			EPERM},
	{STATUS_RANGE_NOT_LOCKED,		EAGAIN},	/* Setting to match F_SETLK, see AFP  */
	{STATUS_INVALID_LEVEL,			ENOTSUP},
	{STATUS_MORE_PROCESSING_REQUIRED,  EAGAIN},	/* SetupX message requires more processing */
	{STATUS_CANCELLED,				ECANCELED},
	{STATUS_INVALID_INFO_CLASS,		EINVAL},
	{STATUS_INFO_LENGTH_MISMATCH,	EINVAL},
	{STATUS_INVALID_DEVICE_REQUEST, EINVAL},
	{STATUS_WRONG_VOLUME,			ENOENT},
	{STATUS_UNRECOGNIZED_MEDIA,		EIO},
	{STATUS_INVALID_SYSTEM_SERVICE,	EINVAL},
	{STATUS_INVALID_LOCK_SEQUENCE,	EACCES},
	{STATUS_ALREADY_COMMITTED,		EACCES},
	{STATUS_OBJECT_TYPE_MISMATCH,	EBADF},
	{STATUS_NOT_LOCKED,				ENOLCK},
	{STATUS_INVALID_PARAMETER_MIX,	EINVAL},
	{STATUS_PORT_DISCONNECTED,		EBADF},
	{STATUS_INVALID_PORT_HANDLE,	EBADF},
	{STATUS_OBJECT_PATH_SYNTAX_BAD,	ENOENT},
	{STATUS_EAS_NOT_SUPPORTED,		ENOTSUP},
	{STATUS_EA_TOO_LARGE,			ENOATTR},
	{STATUS_NONEXISTENT_EA_ENTRY,	ENOATTR},
	{STATUS_NO_EAS_ON_FILE,			ENOATTR},
	{STATUS_NO_SUCH_LOGON_SESSION,	SMB_ENETFSACCOUNTRESTRICTED},
	{STATUS_USER_EXISTS,			SMB_ENETFSACCOUNTRESTRICTED},
	{STATUS_NO_SUCH_USER,			SMB_ENETFSACCOUNTRESTRICTED},
	{STATUS_USER_SESSION_DELETED,	SMB_ENETFSACCOUNTRESTRICTED},
	{STATUS_FILE_INVALID,			EIO},
	{STATUS_DFS_EXIT_PATH_FOUND,	ENOENT},
	{STATUS_DEVICE_DATA_ERROR,		EIO},
	{STATUS_DEVICE_NOT_READY,		EAGAIN},
	{STATUS_ILLEGAL_FUNCTION,		EINVAL},
	{STATUS_FILE_RENAMED,			ENOENT},
	{STATUS_FILE_DELETED,			ENOENT},
	{STATUS_NO_TRUST_LSA_SECRET,	EAUTH},
	{STATUS_NO_TRUST_SAM_ACCOUNT,	EAUTH},
	{STATUS_TRUSTED_DOMAIN_FAILURE,	EAUTH},
	{STATUS_TRUSTED_RELATIONSHIP_FAILURE,	EAUTH},
	{0,	0}
};

#ifdef SMB_DEBUG
void smb_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen)
{
    int32_t addr;
    int32_t i;
	int32_t len = (int32_t)inlen;
	
	printf("%s: hexdump: %s %p length %d inlen %ld\n", func, s, buf, len, inlen);
    addr = 0;
    while( addr < len )
    {
        printf("%6.6x - " , addr );
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
                printf("%2.2x ", buf[addr+i] );
            else
                printf("   " );
        }
        printf(" \"");
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
            {
                if(( buf[addr+i] > 0x19 ) && ( buf[addr+i] < 0x7e ) )
                    printf("%c", buf[addr+i] );
                else
                    printf(".");
            }
        }
        printf("\" \n");
        addr += 16;
    }
    printf("\" \n");
}
#endif // SMB_DEBUG

char * 
smb_strndup(const char * string, size_t maxlen)
{
    char * result = NULL;
    size_t size;
	
    if (!string) {
        goto finish;
    }
	
    size = strnlen(string, maxlen);
	SMB_MALLOC(result, char *, size + 1, M_SMBSTR, M_WAITOK | M_ZERO);
    if (!result) {
        goto finish;
    }
	
    memcpy(result, string, size);
	
finish:
    return result;
}

/* Same as the kernel defualt */
#define SMB_KALLOC_MAP_SIZE_MAX  (128 * 1024 * 1024)
#define SMB_WAIT_SIZE_MAX		(32 * 1024)

/*
 * duplicate memory block from a user space.
 */
void * smb_memdupin(user_addr_t umem, int len)
{
	int error;
	char *p;
	int flags = M_WAITOK;
	
	/* Never let them allocate something unreasonable */
	if ((len < 0) || (len > SMB_KALLOC_MAP_SIZE_MAX)) {
		SMBERROR("Bad size : %d\n", len);
		return NULL;
	}
	/* Requesting large amount of memory don't wait */
	if (len > SMB_WAIT_SIZE_MAX) {
		flags = M_NOWAIT;
	}
    SMB_MALLOC(p, char *, len, M_SMBSTR, flags);
	if (!p) {
		SMBDEBUG("malloc failed : %d\n", ENOMEM);
		return NULL;
	}
	error = copyin(umem, p, len);
	if (error == 0)
		return p;

	SMBDEBUG("copyin failed :  %d\n", error);
	SMB_FREE(p, M_SMBDATA);
	return NULL;
}

/*
 * duplicate memory block in the kernel space.
 */
void *
smb_memdup(const void *umem, int len)
{
	char *p;

	if (len > 32 * 1024)
		return NULL;
    SMB_MALLOC(p, char *, len, M_SMBSTR, M_WAITOK);
	if (p == NULL)
		return NULL;
	bcopy(umem, p, len);
	return p;
}

#ifdef SMB_SOCKETDATA_DEBUG
void
m_dumpm(mbuf_t m) {
	char *p;
	int len;
	SMBDEBUG("d=");
	while(m) {
		p = mbuf_data(m);
		len = mbuf_len(m);
		SMBDEBUG("(%d)",len);
		while(len--){
			SMBDEBUG("%02x ",((int)*(p++)) & 0xff);
		}
		m = mbuf_next(m);
	};
	SMBDEBUG("\n");
}
#endif

/* Convert the DOS Class Error to a NTSTATUS error */
static uint32_t
smb_dos_class_err_to_ntstatus(uint16_t dosErr)
{
	switch (dosErr) {
		case ERRbadfunc:
			return STATUS_NOT_IMPLEMENTED;
		case ERRbadfile:
			return STATUS_NO_SUCH_FILE;
		case ERRbadpath:
			return STATUS_OBJECT_PATH_NOT_FOUND;
		case ERRnofids:
			return STATUS_TOO_MANY_OPENED_FILES;
		case ERRnoaccess:
			return STATUS_ACCESS_DENIED;
		case ERRbadfid:
			return STATUS_INVALID_HANDLE;
		case ERRbadmcb:
			return STATUS_INSUFF_SERVER_RESOURCES;
		case ERRnomem:
			return STATUS_NO_MEMORY;
		case ERRbadmem:
			return STATUS_NO_MEMORY;
		case ERRbadenv:
			return STATUS_INVALID_PARAMETER;
		case ERRbadformat:
			return STATUS_INVALID_PARAMETER;
		case ERRbadaccess:
			return STATUS_ACCESS_DENIED;
		case ERRbaddata:
			return STATUS_DATA_ERROR;
		case ERRoutofmem:
			return STATUS_NO_MEMORY;
		case ERRbaddrive:
			return STATUS_INSUFF_SERVER_RESOURCES;
		case ERRremcd:
			return STATUS_DIRECTORY_NOT_EMPTY;
		case ERRdiffdevice:
			return STATUS_NOT_SAME_DEVICE;
		case ERRnofiles:
			return STATUS_NO_MORE_FILES;
		case ERRwriteprotect:
			return STATUS_MEDIA_WRITE_PROTECTED;
		case ERRnotready:
			return STATUS_DEVICE_NOT_READY;
		case ERRbadcmd:
			return STATUS_SMB_BAD_COMMAND;
		case ERRcrc:
			return STATUS_DATA_ERROR;
		case ERRbadlength:
			return STATUS_INFO_LENGTH_MISMATCH;
		case ERRsectornotfound:
			return STATUS_NONEXISTENT_SECTOR;
		case ERRgeneral:
			return STATUS_UNSUCCESSFUL;
		case ERRbadshare:
			return STATUS_SHARING_VIOLATION;
		case ERRlock:
			return STATUS_FILE_LOCK_CONFLICT;
		case ERRwrongdisk:
			return STATUS_WRONG_VOLUME;
		case ERReof:
			return STATUS_END_OF_FILE;
		case ERRunsup:
			return STATUS_NOT_SUPPORTED;
		case ERRnoipc:
			return STATUS_BAD_NETWORK_NAME;
		case ERRnosuchshare:
			return STATUS_BAD_NETWORK_NAME;
		case ERRtoomanynames:
			return STATUS_TOO_MANY_NAMES;
		case ERRfilexists:
			return STATUS_OBJECT_NAME_COLLISION;
		case ERRinvalidparam:
			return STATUS_INVALID_PARAMETER;
		case ERRinvalidname:
			return STATUS_OBJECT_NAME_INVALID;
		case ERRunknownlevel:
			return STATUS_INVALID_LEVEL;
		case ERRdirnotempty:
			return STATUS_DIRECTORY_NOT_EMPTY;
		case ERRnotlocked:
			return STATUS_RANGE_NOT_LOCKED;
		case ERRrename:
			return STATUS_OBJECT_NAME_COLLISION;
		case ERRbadpipe:
			return STATUS_INVALID_PIPE_STATE;
		case ERRpipebusy:
			return STATUS_PIPE_BUSY;
		case ERRpipeclosing:
			return STATUS_PIPE_CLOSING;
		case ERRnotconnected:
			return STATUS_PIPE_DISCONNECTED;
		case ERRmoredata:
			return STATUS_MORE_PROCESSING_REQUIRED;
		case ERRbadealist:
			return STATUS_EA_TOO_LARGE;
		case ERReasunsupported:
			return STATUS_EAS_NOT_SUPPORTED;
		case ERRnotifyenumdir:
			return STATUS_NOTIFY_ENUM_DIR;
		case ERRinvgroup:
			return STATUS_NETLOGON_NOT_STARTED;
		default:
			break;
	}
	return STATUS_UNSUCCESSFUL;
}

/* Convert the Server Class Error to a NTSTATUS error */
static uint32_t
smb_srv_class_err_to_ntstatus(uint16_t srvErr)
{
	switch (srvErr) {
		case ERRerror:
            /* 
             * Non-specific error: resource other than disk space exhausted 
             * (for example, TIDs); or first command was not negotiate; or 
             * multiple negotiates attempted; or internal server error.
             */
			return STATUS_INSUFFICIENT_RESOURCES;
		case ERRbadpw:
			return STATUS_WRONG_PASSWORD;
		case ERRbadpath:
			return STATUS_PATH_NOT_COVERED;
		case ERRaccess:
			return STATUS_NETWORK_ACCESS_DENIED;
		case ERRinvtid:
			return STATUS_SMB_BAD_TID;
		case ERRinvnetname:
			return STATUS_BAD_NETWORK_NAME;
		case ERRinvdevice:
			return STATUS_BAD_DEVICE_TYPE;
		case ERRinvsess:
			return STATUS_UNSUCCESSFUL;
		case ERRworking:
			return STATUS_UNSUCCESSFUL;
		case ERRnotme:
			return STATUS_UNSUCCESSFUL;
		case ERRbadcmd:
			return STATUS_SMB_BAD_COMMAND;
		case ERRqfull:
			return STATUS_PRINT_QUEUE_FULL;
		case ERRqtoobig:
			return STATUS_NO_SPOOL_SPACE;
		case ERRqeof:
			return STATUS_UNSUCCESSFUL;
		case ERRinvpfid:
			return STATUS_PRINT_CANCELLED;
		case ERRsmbcmd:
			return STATUS_NOT_IMPLEMENTED;
		case ERRsrverror:
			return STATUS_UNEXPECTED_NETWORK_ERROR;
		case ERRfilespecs:
			return STATUS_INVALID_HANDLE;
		case ERRbadpermits:
			return STATUS_NETWORK_ACCESS_DENIED;
		case ERRsetattrmode:
			return STATUS_INVALID_PARAMETER;
		case ERRtimeout:
			return STATUS_IO_TIMEOUT;
		case ERRnoresource:
			return STATUS_REQUEST_NOT_ACCEPTED;
		case ERRtoomanyuids:
			return STATUS_TOO_MANY_SESSIONS;
		case ERRbaduid:
			return STATUS_SMB_BAD_UID;
		case ERRnotconnected:
			return STATUS_PIPE_DISCONNECTED;
		case ERRusempx:
			return STATUS_NOT_IMPLEMENTED;
		case ERRusestd:
			return STATUS_SMB_USE_STANDARD;
		case ERRcontmpx:
			return STATUS_NOT_IMPLEMENTED;
		case ERRaccountExpired:
			return STATUS_ACCOUNT_EXPIRED;
		case ERRbadClient:
			return STATUS_INVALID_WORKSTATION;
		case ERRbadLogonTime:
			return STATUS_INVALID_LOGON_HOURS;
		case ERRpasswordExpired:
			return STATUS_PASSWORD_EXPIRED;
		case ERRnosupport:
			return STATUS_NOT_IMPLEMENTED;
		default:
			break;
	}
	return STATUS_UNSUCCESSFUL;
}

/* Convert the Hardware Class Error to a NTSTATUS error */
static uint32_t
smb_hrd_class_err_to_ntstatus(uint16_t hrdErr)
{
	switch (hrdErr) {
		case ERRnowrite:
			return STATUS_MEDIA_WRITE_PROTECTED;
		case ERRbadunit:
			return STATUS_UNSUCCESSFUL;
		case ERRnotready:
			return STATUS_NO_MEDIA_IN_DEVICE;
		case ERRbadcmd:
			return STATUS_INVALID_DEVICE_STATE;
		case ERRdata:
			return STATUS_DATA_ERROR;
		case ERRbadreq:
			return STATUS_DATA_ERROR;
		case ERRseek:
			return STATUS_UNSUCCESSFUL;
		case ERRbadmedia:
			return STATUS_DISK_CORRUPT_ERROR;
		case ERRbadsector:
			return STATUS_NONEXISTENT_SECTOR;
		case ERRnopaper:
			return STATUS_DEVICE_PAPER_EMPTY;
		case ERRwrite:
			return STATUS_IO_DEVICE_ERROR;
		case ERRread:
			return STATUS_IO_DEVICE_ERROR;
		case ERRgeneral:
			return STATUS_UNSUCCESSFUL;
		case ERRbadshare:
			return STATUS_SHARING_VIOLATION;
		case ERRlock:
			return STATUS_FILE_LOCK_CONFLICT;
		case ERRwrongdisk:
			return STATUS_WRONG_VOLUME;
		case ERRFCBunavail:
			return STATUS_UNSUCCESSFUL;
		case ERRsharebufexc:
			return STATUS_UNSUCCESSFUL;
		case ERRdiskfull:
			return STATUS_DISK_FULL;
		default:
			break;
	}
	return STATUS_UNSUCCESSFUL;
}	

uint32_t
smb_errClassCodes_to_ntstatus(uint8_t errClass, uint16_t errCode)
{
	switch (errClass) {
		case ERRDOS_Class:
			return smb_dos_class_err_to_ntstatus(errCode);
			break;
		case ERRSRV_Class:
			return smb_srv_class_err_to_ntstatus(errCode);
			break;
		case ERRHRD_Class:
			return smb_hrd_class_err_to_ntstatus(errCode);
			break;
		case SUCCESS_Class:
			if (!errCode) {
				return 0;
			}
			/* Fall through have no idea what to do here */
		default:
			break;
	}
	return STATUS_UNSUCCESSFUL;
}

static uint32_t
smb_ntstatus_error_to_errno(uint32_t ntstatus)
{
	int	ii;

	for (ii = 0; nt2errno[ii].errno; ii++)
		if (nt2errno[ii].nterr == ntstatus)
			return (nt2errno[ii].errno);
	
	SMBERROR("Couldn't map ntstatus (0x%x) to errno returning EIO\n", ntstatus);
	return EIO;
}

static uint32_t
smb_ntstatus_warning_to_errno(uint32_t ntstatus)
{
	switch (ntstatus) {
		case STATUS_BUFFER_OVERFLOW:			
			/*  Do a special check for STATUS_BUFFER_OVERFLOW; it's not an error. */
			return 0;
			break;
		case STATUS_NO_MORE_FILES:
			/*  Do a special check for STATUS_NO_MORE_FILES; it's not an error. */
			return ENOENT;
			break;
		case STATUS_STOPPED_ON_SYMLINK:
			return EIO; /* May want to change this to access denied, but leave the same error for now */
			break;
		default:
			break;
	}
	/* XXX - How should we treat ntstatus warnings? */
	SMBERROR("Couldn't map ntstatus (0x%x) to errno returning EIO\n", ntstatus);
	return EIO;
}

uint32_t
smb_ntstatus_to_errno(uint32_t ntstatus)
{
	switch (ntstatus & STATUS_SEVERITY_MASK) {
		case STATUS_SEVERITY_SUCCESS:
            /*
             * Lion Servers returns STATUS_NOTIFY_ENUM_DIR, which just tells
             * the notify code that something has changed. Just skip printing
             * the warning since we know why this is being returned.
             */
			if ((ntstatus == STATUS_SUCCESS) || 
                (ntstatus == STATUS_NOTIFY_ENUM_DIR) ||
                (ntstatus == STATUS_NOTIFY_CLEANUP)) {
				return 0;
			}
			SMBWARNING("STATUS_SEVERITY_SUCCESS ntstatus = 0x%x\n", ntstatus);
			return 0;
			break;
		case STATUS_SEVERITY_INFORMATIONAL:
			SMBWARNING("STATUS_SEVERITY_INFORMATIONAL ntstatus = 0x%x\n", ntstatus);
			return 0;
			break;
		case STATUS_SEVERITY_WARNING:
			return smb_ntstatus_warning_to_errno(ntstatus);
			break;
		case STATUS_SEVERITY_ERROR:
			return smb_ntstatus_error_to_errno(ntstatus);
			break;
		default:
			break;
	}
	return EIO;
}

int 
smb_put_dmem(struct mbchain *mbp, const char *src, size_t srcSize, 
				 int flags, int usingUnicode, size_t *lenp)
{
	char convbuf[512];
	char *utf16Str;
	char *dst;
	size_t utf16InLen, utf16OutLen;
	int error;

	if (srcSize == 0)
		return 0;
	/* Just to be safe make sure we have room for the null bytes */
	utf16InLen = (srcSize * 2) + 2;
	/* We need a bigger buffer */
	if (utf16InLen > sizeof(convbuf)) {
		SMB_MALLOC(utf16Str, void *, utf16InLen, M_TEMP, M_WAITOK);
		if (!utf16Str)
			return ENOMEM;
		
	} else {
		/* Just to be safe behave the same as before */
		utf16InLen = sizeof(convbuf);
		utf16Str = convbuf;
	}

	utf16OutLen = utf16InLen;
	dst = utf16Str;

	error = smb_convert_to_network(&src, &srcSize, &dst, &utf16OutLen, flags, 
								   usingUnicode);
	if (error)
		goto done;
	
	utf16OutLen = utf16InLen - utf16OutLen;
	if (usingUnicode)
		mb_put_padbyte(mbp);
	error = mb_put_mem(mbp, utf16Str, utf16OutLen, MB_MSYSTEM);
	if (!error && lenp)
		*lenp += utf16OutLen;
done:
	/* We allocated it so free it */
	if (utf16Str != convbuf) {
		SMB_FREE(utf16Str, M_TEMP);
	}
	
	return error;
}

int smb_put_dstring(struct mbchain *mbp, int usingUnicode, const char *src, 
					size_t maxlen, int flags)
{
	int error;

	error = smb_put_dmem(mbp, src, strnlen(src, maxlen), flags, usingUnicode, NULL);
	if (error)
		return error;
	if (usingUnicode)
		return mb_put_uint16le(mbp, 0);
	return mb_put_uint8(mbp, 0);
}
