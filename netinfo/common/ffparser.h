#include <NetInfo/dsrecord.h>

char **ff_tokens_from_line(const char *data, const char *sep, int skip_comments);
char **ff_netgroup_tokens_from_line(const char *data);
dsrecord *ff_parse_user(char *data);
dsrecord *ff_parse_user_A(char *data);
dsrecord *ff_parse_group(char *data);
dsrecord *ff_parse_host(char *data);
dsrecord *ff_parse_network(char *data);
dsrecord *ff_parse_service(char *data);
dsrecord *ff_parse_protocol(char *data);
dsrecord *ff_parse_rpc(char *data);
dsrecord *ff_parse_mount(char *data);
dsrecord *ff_parse_printer(char *data);
dsrecord *ff_parse_bootparam(char *data);
dsrecord *ff_parse_bootp(char *data);
dsrecord *ff_parse_alias(char *data);
dsrecord *ff_parse_ethernet(char *data);
dsrecord *ff_parse_netgroup(char *data);
