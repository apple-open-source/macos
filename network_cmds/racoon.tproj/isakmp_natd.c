
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#include "vmbuf.h"
#include "plog.h"
#include "misc.h"
#include "isakmp_var.h"
#include "oakley.h"
#include "isakmp.h"

#include "handler.h"
#include "isakmp_natd.h"

natd_match_t
natd_matches(
	struct ph1handle* iph1,
	struct isakmp_gen *natd_record)
{
	natd_match_t matches = 0;
#ifdef IKE_NAT_T
	int	dataLen = ntohs(natd_record->len) - sizeof(*natd_record);
	char* dataPtr = ((char*)natd_record) + sizeof(*natd_record);
	
	/* Always recreate the natd records in case the ports change */
	natd_create(iph1);
	
	if (iph1->local_natd != NULL && dataLen == iph1->local_natd->l &&
		memcmp(dataPtr, iph1->local_natd->v, dataLen) == 0)
	{
		plog(LLV_DEBUG, LOCATION, iph1->remote,
			"natd payload matches local address\n");
		matches |=  natd_match_local;
	}
	
	if (iph1->remote_natd != NULL && dataLen == iph1->remote_natd->l &&
		memcmp(dataPtr, iph1->remote_natd->v, dataLen) == 0)
	{
		plog(LLV_DEBUG, LOCATION, iph1->remote,
			"natd payload matches remote address\n");
		matches |=  natd_match_remote;
	}
#else
	matches = natd_match_local | natd_match_remote;
#endif

	if (matches == 0)
	{
		plog(LLV_DEBUG, LOCATION, iph1->remote,
			"natd payload matches no address\n");
	}
		
	return matches;
}

/*
 * NAT detection record contains a hash of the initiator cookie,
 * responder cookie, address, and port.
 */
typedef struct {
	cookie_t		initiator_cookie;
	cookie_t		responder_cookie;
	struct in_addr	address;
	u_short			port;
} __attribute__((__packed__)) natd_hash_contents;

int
natd_create(
	struct ph1handle* iph1)
{
#ifdef IKE_NAT_T
	natd_hash_contents	hash_this;
	vchar_t				data_to_hash;
	
	if (iph1->remote->sa_family != AF_INET ||
		iph1->local->sa_family != AF_INET)
	{
		/*
		 * NAT traversal is intentionally unsupported on IPv6.
		 * 
		 * The only reason for using NAT is a lack of addresses.
		 * IPv6 does not suffer from a lack of addresses. If I
		 * find that anyone is using NAT with IPv6, I would break
		 * their kneecaps and poke their eyes out with sharp
		 * sticks. I don't really want to go to jail though, so
		 * instead, I'll spare myself the effort of support nat
		 * traversal magic when IPv6 is involved in hopes this will
		 * cause boneheads who would try to use IPv6 NAT to give up
		 * since so many things will stop working. I'm sick of
		 * working around all of the problems that NATs introduce.
		 *
		 * IPv6 is our opportunity to move to a NAT free world.
		 */
		return -1;
	}
	
	data_to_hash.l = sizeof(hash_this);
	data_to_hash.v = (char*)&hash_this;
	
	memcpy(hash_this.initiator_cookie, iph1->index.i_ck,
		sizeof(hash_this.initiator_cookie));
	memcpy(hash_this.responder_cookie, iph1->index.r_ck,
		sizeof(hash_this.responder_cookie));
	
	/* Local address */
	if (iph1->local_natd != NULL)
		vfree(iph1->local_natd);
	iph1->local_natd = NULL;
	hash_this.address = ((struct sockaddr_in*)(iph1->local))->sin_addr;
	hash_this.port = ((struct sockaddr_in*)(iph1->local))->sin_port;
	plog(LLV_DEBUG, LOCATION, iph1->remote,
		"creating local %.8X%.8X:%.8X%.8X %s:%d\n",
		*(u_long*)&hash_this.initiator_cookie[0],
		*(u_long*)&hash_this.initiator_cookie[4],
		*(u_long*)&hash_this.responder_cookie[0],
		*(u_long*)&hash_this.responder_cookie[4],
		inet_ntoa(hash_this.address), hash_this.port);
	iph1->local_natd = oakley_hash(&data_to_hash, iph1);
	plogdump(LLV_DEBUG, iph1->local_natd->v, iph1->local_natd->l);
	
	/* Remote address */
	if (iph1->remote_natd != NULL)
		vfree(iph1->remote_natd);
	iph1->remote_natd = NULL;
	hash_this.address = ((struct sockaddr_in*)(iph1->remote))->sin_addr;
	hash_this.port = ((struct sockaddr_in*)(iph1->remote))->sin_port;
	plog(LLV_DEBUG, LOCATION, iph1->remote,
		"creating remote %.8X%.8X:%.8X%.8X %s:%d\n",
		*(u_long*)&hash_this.initiator_cookie[0],
		*(u_long*)&hash_this.initiator_cookie[4],
		*(u_long*)&hash_this.responder_cookie[0],
		*(u_long*)&hash_this.responder_cookie[4],
		inet_ntoa(hash_this.address), hash_this.port);
	iph1->remote_natd = oakley_hash(&data_to_hash, iph1);
	plogdump(LLV_DEBUG, iph1->remote_natd->v, iph1->remote_natd->l);
	
	return (iph1->local_natd != NULL) && (iph1->remote_natd != NULL);
#else
	return 0;
#endif
}

int
natd_hasnat(
	const struct ph1handle* iph1)
{
#if IKE_NAT_T
	return (iph1->natt_flags & natt_natd_received) &&
		(iph1->natt_flags & (natt_no_remote_nat | natt_no_local_nat)) != 
		(natt_no_remote_nat | natt_no_local_nat);
#else
	return 0;
#endif
}
