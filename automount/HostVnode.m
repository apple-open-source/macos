#import "HostVnode.h"
#import "Controller.h"
#import "AMString.h"
#import "AMMap.h"
#import "automount.h"
#import "log.h"
#import <syslog.h>
#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import <netdb.h>
#import "nfs_prot.h"
#import "Server.h"
#import <sys/types.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <netdb.h>
#import <unistd.h>
#import <stdlib.h>
#import <string.h>
#import <sys/socket.h>
#import <net/if.h>
#import <sys/ioctl.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/svc.h>
#import <errno.h>
#import "nfs_prot.h"
#import "automount.h"
#import "log.h"
#import "mount.h"
#import "AMString.h"

extern u_short getport(struct sockaddr_in *, u_long, u_long, u_int);

@implementation HostVnode

- (void)newMount:(String *)hname server:(Server *)serv volume:(char *)dir parent:(Vnode *)p
{
	String *serversrc, *type, *x;
	Vnode *v;
	char *s;
	int len;

	type = [String uniqueString:"nfs"];
	serversrc = [String uniqueString:dir];

	v = [map createVnodePath:serversrc from:p withType:nil];

	if ([v type] == NFLNK)
	{
		/* mount already exists - do not override! */
		[type release];
		[serversrc release];
		return;
	}

	[v setType:NFLNK];
	[v setServer:serv];
	[v setSource:serversrc];
	[v setVfsType:type];
	
	/* Information derived dynamically this way is not necessarily trustworthy: */
	[v addMntArg:MNT_NOSUID];
	[v addMntArg:MNT_NODEV];

	[type release];
	[serversrc release];

	len = [[map mountPoint] length] + [[v path] length] + 1;
	s = malloc(len);
	sprintf(s, "%s%s", [[map mountPoint] value], [[v path] value]);

	x = [String uniqueString:s];
	free(s);
	[v setLink:x];
	[x release];
}

- (Vnode *)vnodeForHost:(String *)hname
{
	int status, s, len;
	struct timeval tv;
	CLIENT *cl;
	exports server_exports, next;
	struct sockaddr_in sin;
	Vnode *v;
	String *x;
	char *str;
	Server *serv;
	u_short port;

	serv = [controller serverWithName:hname];
	if (serv == nil) return nil;

	if ([serv isLocalHost])
	{
		v = [[Vnode alloc] init];
		[v setName:hname];
		[v setMap:map];
		[v setServer:serv];
		[v setType:NFLNK];
		[v setMode:00755 | NFSMODE_LNK];
		x = [String uniqueString:"/"];
		[v setLink:x];
		[v setSource:x];
		[x release];
		[v setMounted:YES];
		[v setFakeMount:YES];
		[controller registerVnode:v];
		[self addChild:v];
		return v;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = [serv address];

	port = getport(&sin, MOUNTPROG, 3, IPPROTO_UDP);
	if (port == 0) return nil;

	sys_msg(debug, LOG_DEBUG, "%s(%d):  port = %d\n", __FILE__, __LINE__, ntohs(port));
	
	sin.sin_port = port;

	s = RPC_ANYSOCK;
	cl = clntudp_create(&sin, MOUNTPROG, 3, tv, &s);
	if (cl == NULL) return nil;

	memset(&server_exports, 0, sizeof(exports));

	status = clnt_call(cl, MOUNTPROC_EXPORT, (xdrproc_t)xdr_void, NULL, (xdrproc_t)xdr_exports, (caddr_t)&server_exports, tv);
	clnt_destroy(cl);
	if (status != RPC_SUCCESS) return nil;

	if (server_exports == NULL) return nil;

	v = [[HostVnode alloc] init];
	[v setName:hname];
	[v setServerDepth:0];

	x = [String uniqueString:"nfs"];
	[v setVfsType:x];
	[x release];

	[v setMap:map];
	[controller registerVnode:v];
	[self addChild:v];

	[v setType:NFLNK];
	[v setMode:01755 | NFSMODE_LNK];

	[v setServer:serv];

	len = [[map mountPoint] length] + [[v path] length] + 1;
	str = malloc(len);
	sprintf(str, "%s%s", [[map mountPoint] value], [[v path] value]);
	x = [String uniqueString:str];
	free(str);
	[v setLink:x];
	[x release];

	next = server_exports;
	while (next != NULL)
	{
//		sys_msg(debug, LOG_DEBUG, "%s: %s", [hname value], next->ex_dir);
		if (!strcmp(next->ex_dir, "/")) {
			[v setType:NFLNK];
			[v setServer:serv];
			x = [String uniqueString:"/"];
			[v setSource:x];
			[x release];
			x = [String uniqueString:"nfs"];
			[v setVfsType:x];
			[x release];
		} else {
			[self newMount:hname server:serv volume:next->ex_dir parent:v];
		};
		next = next->ex_next;
	}

	return v;
}

- (Vnode *)lookup:(String *)n
{
	int i, count;
	Vnode *sub;
	struct hostent *h;

	if (strcmp([n value], ".") == 0) return self;
	if (strcmp([n value], "..") == 0) return supernode;

	count = [subnodes count];
	for (i = 0; i < count; i++)
	{
		sub = [subnodes objectAtIndex:i];
		if ([n equal:[sub name]]) return sub;
	}

	h = gethostbyname([n value]);
	if (h == NULL) return nil;

	return [self vnodeForHost:n];
}

@end
