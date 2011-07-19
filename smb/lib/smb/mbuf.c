/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netsmb/upi_mbuf.h>
#include <netsmb/smb_lib.h>


/* mbuf flags */
#define M_EXT           0x0001  /* has associated external storage */
#define M_PKTHDR        0x0002  /* start of record */
#define M_EOR           0x0004  /* end of record */


struct smb_mbuf {
	uint32_t			m_type;
	uint32_t			m_flags;
	size_t				m_maxlen;
	size_t				m_len;
	size_t				m_pkthdr_len;
	char				*m_data;
	struct smb_mbuf		*m_next;
	void (*m_extfree)(caddr_t , size_t, caddr_t);
	caddr_t				m_extarg;
};

/* 
 * mbuf_free
 *
 * Frees a single mbuf. Not commonly used outside of the file because it
 * doesn't touch the rest of the mbufs on the chain.
 * params: 
 *		mbuf - The mbuf to free.
 * result:
 *		The next mbuf in the chain.
 */
mbuf_t mbuf_free(mbuf_t mbuf)
{
	mbuf_t next = NULL;
	
	if (mbuf == NULL)
		return next; /* nothing to free */
	
	next = mbuf->m_next;
	if (mbuf->m_type == MBUF_TYPE_FREE) {
		smb_log_info("%s: Double FREE", ASL_LEVEL_DEBUG, __FUNCTION__);
	}
	if (mbuf->m_flags & M_EXT) {
		if (mbuf->m_extfree)
			mbuf->m_extfree(mbuf->m_extarg, mbuf->m_maxlen, (caddr_t)mbuf->m_data);
	} else if (mbuf->m_data) {
		free(mbuf->m_data);
	}
	mbuf->m_next  = NULL;
	mbuf->m_type = MBUF_TYPE_FREE;
	mbuf->m_data = NULL;
	free(mbuf);
	return next;
}

/* 
 * smb_mbuf_get
 *
 * Internal routine that allocates all userland mbufs. Only creates
 * data storage if maxlen is not zero.
 * params: 
 *		how		- How to create the mbuf, always MBUF_WAITOK in userland.
 *		type	- Type of mbuf to create, always MBUF_TYPE_DATA in userland.
 *		mbuf	- Return location for the mbuf we created.
 *		maxlen	- The data size that should be allocated for this mbuf
 * result:
 *		Error	- Either zero or the appropriate errno
 */
static 
int smb_mbuf_get(uint32_t how, uint32_t type, mbuf_t *mbuf, size_t maxlen)
{
	struct smb_mbuf *m;
	
	if ((type != MBUF_TYPE_DATA) || (how != MBUF_WAITOK))
		return (EINVAL);
	
	m = malloc(sizeof(struct smb_mbuf));
	if (m == NULL)
		return ENOMEM;
	
	bzero(m, sizeof(struct smb_mbuf));
	m->m_type = type;
	if (maxlen) {
		m->m_data = malloc(maxlen);
		if (m->m_data == NULL) {
			(void)mbuf_free(m);
			return ENOMEM;
		}
		m->m_maxlen = maxlen;
	}
	*mbuf = m;
	return 0;
}

/*
 * mbuf_freem
 *
 * Frees a chain of mbufs link through mnext. 
 * params: 
 *		mbuf - The first mbuf in the chain to free.
 */
void mbuf_freem(mbuf_t mbuf)
{
	struct smb_mbuf *m;
	
	while (mbuf) {
		m = mbuf_free(mbuf);
		mbuf = m;
	}
}

/*
 * mbuf_gethdr
 *
 * Allocates an mbuf without a cluster for external data. Sets a flag to 
 * indicate there is a packet header and initializes the packet header.
 * params: 
 *		how		- How to create the mbuf, always MBUF_WAITOK in userland.
 *		type	- Type of mbuf to create, always MBUF_TYPE_DATA in userland.
 *		mbuf	- Return location for the mbuf we created.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_gethdr(uint32_t how, uint32_t type, mbuf_t *mbuf)
{
	int error = smb_mbuf_get(how, type, mbuf, getpagesize());
	if (error)
		return error;
	
	(*mbuf)->m_flags |= M_PKTHDR | M_EOR;
	return 0;
}

/*
 * mbuf_get
 *
 * Allocates an mbuf without a cluster for external data.
 * params: 
 *		how		- How to create the mbuf, always MBUF_WAITOK in userland.
 *		type	- Type of mbuf to create, always MBUF_TYPE_DATA in userland.
 *		mbuf	- Return location for the mbuf we created.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_get(uint32_t how, uint32_t type, mbuf_t *mbuf)
{
	return smb_mbuf_get(how, type, mbuf, getpagesize());
}

/*
 * mbuf_getcluster
 *
 * Allocate a cluster of the requested size and attach it to an mbuf for use as 
 * external data. If mbuf points to a NULL mbuf_t, an mbuf will be allocated for 
 * you. If mbuf points to  a non-NULL mbuf_t, mbuf_getcluster may return a different
 * mbuf_t than the one you passed in.
 * params: 
 *		how		- How to create the mbuf, always MBUF_WAITOK in userland.
 *		type	- Type of mbuf to create, always MBUF_TYPE_DATA in userland.
 *		size	- The size of the cluster to be allocated. This routine allows
 *					any size, unlike the kernel kpi mbuf code.
 *		mbuf	- The mbuf the cluster will be attached to.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_getcluster(uint32_t how, uint32_t type, size_t size, mbuf_t *mbuf)
{
	int error;
	/* We currently relocate the mbuf always */
	if (*mbuf) {
		mbuf_freem( *mbuf);
		*mbuf = NULL;
	}
	error = smb_mbuf_get(how, type, mbuf, size);
	if (!error && *mbuf)
		(*mbuf)->m_flags |= M_PKTHDR | M_EOR;
	return error;
}

/*
 * mbuf_attachcluster
 *
 * Attach an external buffer as a cluster for an mbuf.  If mbuf points to a NULL 
 * mbuf_t, an mbuf will be allocated for you.  If mbuf points to a non-NULL mbuf_t, 
 * the user-supplied mbuf will be used instead.  
 * params: 
 *		how		- How to create the mbuf, always MBUF_WAITOK in userland.
 *		type	- Type of mbuf to create, always MBUF_TYPE_DATA in userland.
 *		mbuf	- Pointer to the address of the mbuf; if NULL, an mbuf will be
 *					allocated, otherwise, it must point to a valid mbuf address.
 *		extbuf	- Address of the external buffer.
 *		extfree	- Free routine for the external buffer; the caller is required 
 *					to defined a routine that will be invoked when the mbuf is 
 *					freed. Userland code allows this to be null.
 *		extsize	- Size of the external buffer.
 *		extarg	- Private value that will be passed to the free routine when it 
 *					is called at the time the mbuf is freed.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_attachcluster(uint32_t how, uint32_t type,
					   mbuf_t *mbuf, void * extbuf, 
					   void (*extfree)(caddr_t , size_t, caddr_t),
					   size_t extsize, caddr_t extarg)
{
	int error = 0;
	
	if ((extbuf == NULL) || (extsize == 0)) {
		error = EINVAL;
	} else if (*mbuf == NULL) {
		error = smb_mbuf_get(how, type, mbuf, 0);
	} else if ((*mbuf)->m_data) {
		free((*mbuf)->m_data);
		(*mbuf)->m_data = NULL;
	}
	if (error)
		return error;
	
	(*mbuf)->m_flags = M_EXT | M_PKTHDR | M_EOR;
	(*mbuf)->m_maxlen = extsize;
	(*mbuf)->m_data = extbuf;
	(*mbuf)->m_extfree = extfree;
	(*mbuf)->m_extarg = extarg;
	return 0;
}

/*
 * mbuf_len
 *
 * Gets the length of data in this mbuf.
 * params: 
 *		mbuf	- The mbuf.
 * result:
 *		size_t	- The  length of data in this mbuf.
 */
size_t mbuf_len(const mbuf_t mbuf)
{
	if (mbuf) {
		return mbuf->m_len;
	} else {
		return 0;
	}
}

/*
 * mbuf_maxlen
 *
 * Retrieves the maximum length of data that may be stored in this mbuf.  
 * params: 
 *		mbuf	- The mbuf.
 * result:
 *		size_t	- The maximum length of data for this mbuf.
 */
size_t mbuf_maxlen(const mbuf_t mbuf)
{
	if (mbuf) {
		return mbuf->m_maxlen;
	} else {
		return 0;
	}
}

/*
 * mbuf_setlen
 * 
 * Sets the length of data in this packet. Be careful to not set the length over 
 * the space available in the mbuf.
 * param:
 *		mbuf	- The mbuf.
 *		len		- The new length.
 */
void mbuf_setlen(mbuf_t mbuf, size_t len)
{
	if (mbuf) {
		mbuf->m_len = len;
	}
}

/*
 * mbuf_pkthdr_len
 *
 * Returns the length as reported by the packet header.
 * params: 
 *		mbuf	- The mbuf containing the packet header with the length to
 *					be changed.
 * result:
 *		size_t	- The length, in bytes, of the packet.
 */
size_t mbuf_pkthdr_len(const mbuf_t mbuf)
{
	return mbuf->m_pkthdr_len;
}

/*
 * mbuf_pkthdr_setlen
 *
 * Sets the length of the packet in the packet header.
 * params: 
 *		mbuf	- The mbuf containing the packet header.
 */
void mbuf_pkthdr_setlen(mbuf_t mbuf, size_t len)
{
	mbuf->m_pkthdr_len = len;
}

/*
 * mbuf_pkthdr_adjustlen
 *
 * Adjusts the length of the packet in the packet header.
 * params: 
 *		mbuf	- The mbuf containing the packet header.
 *		amount	- The number of bytes to adjust the packet header length field.
 */
void mbuf_pkthdr_adjustlen(mbuf_t mbuf, int amount)
{
	mbuf->m_pkthdr_len += amount;
}

/*
 * mbuf_next
 *
 * Returns the next mbuf in the chain.
 * params: 
 *		mbuf	- The mbuf
 * result:
 *		mbuf_t	- The next mbuf in the chain.
 */
mbuf_t mbuf_next(const mbuf_t mbuf)
{
	if (mbuf) {
		return mbuf->m_next;
	} else {
		return NULL;
	}
}

/*
 * mbuf_setnext
 *
 * Sets the next mbuf in the chain.
 * params: 
 *		mbuf	- The mbuf
 *		next	- The new next mbuf.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_setnext(mbuf_t mbuf, mbuf_t next)
{
	if (!next || (next->m_type == MBUF_TYPE_FREE)) 
		return EINVAL;
	if (mbuf->m_flags & M_EOR) {
		mbuf->m_flags &= ~M_EOR;
		next->m_flags |=M_EOR;
	}
	mbuf->m_next = next;
	return 0;
}

/*
 * mbuf_data
 *
 * Returns a pointer to the start of data in this mbuf. There may be additional 
 * data on chained mbufs. The data you're looking for may not be virtually 
 * contiguous if it spans more than one mbuf.  In addition, data that is virtually 
 * contiguous might not be represented by physically contiguous pages; see
 * further comments in mbuf_data_to_physical.  Use mbuf_len to determine the length 
 * of data available in this mbuf. If a data structure you want to access stradles 
 * two mbufs in a chain, either use mbuf_pullup to get the data contiguous in one mbuf
 * or copy the pieces of data from each mbuf in to a contiguous buffer. Using 
 * mbuf_pullup has the advantage of not having to copy the data. On the other hand, 
 * if you don't make sure there is space in the mbuf, mbuf_pullup may fail and 
 * free the mbuf.
 * params: 
 *		mbuf	- The mbuf
 *		next	- The new next mbuf.
 * result:
 *		void *	- A pointer to the data in the mbuf.
 */
void *mbuf_data(const mbuf_t mbuf)
{
	if (mbuf) {
		return (void *)mbuf->m_data;
	} else {
		return NULL;
	}
}

/*
 * mbuf_trailingspace
 * 
 * Determines the space available in the mbuf following the current data.
 * params: 
 *		mbuf	- The mbuf
 * result:
 *		size_t	- The number of unused bytes following the current data.
 */
size_t mbuf_trailingspace(const mbuf_t mbuf)
{
	return mbuf->m_maxlen - mbuf->m_len;
}

/*
 * mbuf_copydata
 *
 * Copies data out of an mbuf in to a specified buffer. If the data is stored in 
 * a chain of mbufs, the data will be copied from each mbuf in the chain until 
 * length bytes have been copied.
 * params: 
 *		mbuf	- The mbuf chain to copy data out of.
 *		offset	- The offset in to the mbuf to start copying.
 *		length	- The number of bytes to copy.
 *		out_data - A pointer to the location where the data will be copied.
 * result:
 *		Error	- Either zero or the appropriate errno
 */
int mbuf_copydata(const mbuf_t mbuf, size_t offset, size_t length, void *out_data)
{
	/* Copied m_copydata, added error handling (don't just panic) */
	size_t count;
	mbuf_t  m = mbuf;
	
	while (offset > 0) {
		if (m == 0)
			return EINVAL;
		if (offset < (size_t)m->m_len)
			break;
		offset -= m->m_len;
		m = m->m_next;
	}
	while (length > 0) {
		if (m == 0)
			return EINVAL;
		count = m->m_len - offset > length ? length : m->m_len - offset;
		bcopy((caddr_t)mbuf_data(m) + offset, out_data, count);
		length -= count;
		out_data = ((char*)out_data) + count;
		offset = 0;
		m = m->m_next;
	}
	
	return 0;
}
