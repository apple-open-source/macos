/* Stuff here */

struct share_info {
	u_int16_t	type;
	char		*netname;
	char		*remark;
};

int  smb_netshareenum(struct smb_ctx *, int *, int *, struct share_info **);
