// natd_matches checks if the natd_record matches either the
// source address and port or the destination address and port
// if natd_record matches source, returns 1.
// if natd_record matches desination, returns 2.
// if natd_record doesn't match any entries, returns 0.
typedef enum
{
	natd_match_none		= 0,
	natd_match_local	= 1,
	natd_match_remote	= 2
} natd_match_t;

natd_match_t	natd_matches(struct ph1handle* iph1, struct isakmp_gen *natd_record);
int				natd_create(struct ph1handle* iph1);
int				natd_hasnat(const struct ph1handle* iph1);