#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define _PATH_ASL_OUT "/var/log/asl.log"

#define ASL_QUERY_OP_NULL          0x00000

#define NOTIFY_SYSTEM_MASTER "com.apple.system.syslog.master"
#define NOTIFY_SYSTEM_ASL_FILTER "com.apple.system.syslog.asl_filter"
#define NOTIFY_PREFIX_SYSTEM "com.apple.system.syslog"
#define NOTIFY_PREFIX_USER "user.syslog"

typedef struct __aslclient
{
	uint32_t options;
	struct sockaddr_un server;
	int sock;
	pid_t pid;
	uid_t uid;
	gid_t gid;
	char *name;
	char *facility;
	uint32_t filter;
	int notify_token;
	int notify_master_token;
	uint32_t fd_count;
	int *fd_list;
	uint32_t reserved1;
	void *reserved2;
} asl_client_t;

typedef struct __aslmsg
{
	uint32_t type;
	uint32_t count;
	char **key;
	char **val;
	uint32_t *op;
} asl_msg_t;

typedef struct __aslresponse
{
	uint32_t count;
	uint32_t curr;
	asl_msg_t **msg;
} asl_search_result_t;
