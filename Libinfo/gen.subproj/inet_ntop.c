
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

void __res_close()
{
}

const char *inet_ntop6(const struct in6_addr *addr,char *buf,size_t len)
{
	const u_int16_t *ap=addr->__u6_addr.__u6_addr16;
	int colon=2;
	int i;
	char *bp=buf;
	
	for(i=0;i<8;i++,ap++)
	{
		if(bp>=buf+len-1)
		{
			buf[len-1]=0;
			return buf;
		}
		if(*ap || colon==-1)
		{
			if(colon==2)
				colon=0;
			if(colon)
				colon=-1;
			sprintf(bp,"%x",*ap);
			bp+=strlen(bp);
			if(i!=7)
				*bp++=':';
		}
		else
		{
			if(colon==2)
			{
				*bp++=':';
				*bp++=':';
			}
			else if(!colon && i!=7)
				*bp++=':';
			colon=1;
		}
	}
	*bp=0;
	return buf;
}

const char *inet_ntop4(const struct in_addr *addr,char *buf,size_t len)
{
	const u_int8_t *ap=(u_int8_t*)&addr->s_addr;
	int i;
	char *bp=buf;
	
	for(i=0;i<4;i++,ap++)
	{
		if(bp>=buf+len-1)
		{
			buf[len-1]=0;
			return buf;
		}
		sprintf(bp,"%d",*ap);
		bp+=strlen(bp);
		if(i!=3)
			*bp++='.';
	}
	*bp=0;
	return buf;
}

const char *inet_ntop(int af,const void *addr,char *buf,size_t len)
{
	if(af==AF_INET6)
		return inet_ntop6(addr,buf,len);
	if(af==AF_INET)
		return inet_ntop4(addr,buf,len);
	return NULL;
}
