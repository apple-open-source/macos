#include <NetInfo/dns.h>

#define SDNS_NEGATIVE 0x00000001

/*
 * Information we keep for a single DNS client.
 */
typedef struct
{
	char *name;
	dns_handle_t *dns;
	unsigned int flags;
	unsigned int modtime;
	unsigned int stattime;
} sdns_client_t;

/*
 * Handle
 */
typedef struct 
{
	sdns_client_t *dns_default;
	unsigned int client_count;
	sdns_client_t **client;
	unsigned int modtime;
	unsigned int stattime;
	unsigned int stat_latency;
	char *log_title;
	u_int32_t log_dest;
	u_int32_t log_flags;
	u_int32_t log_facility;
	FILE *log_file;
	int (*log_callback)(int, char *);
} sdns_handle_t;

sdns_handle_t *sdns_open();
void sdns_free(sdns_handle_t *sdns);
dns_reply_t *sdns_query(sdns_handle_t *sdns, dns_question_t *q);
void sdns_open_log(sdns_handle_t *sdns, char *title, int dest, FILE *file, int flags, int facility, int (*callback)(int, char *));
