#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pwd.h>
#include <grp.h>

#include <libbsm.h>

#define READ_TOKEN_BYTES(buf, len, dest, size, bytesread, err)	\
	do {\
		if(bytesread + size > len) {\
			err = 1;\
		}\
		else {\
			memcpy(dest, buf + bytesread, size);\
			bytesread += size;\
		}\
	} while(0)


#define	SET_PTR(buf, len, ptr, size, bytesread, err) \
	do {\
		if(bytesread + size > len) {\
			err = 1;\
		}\
		else {\
			ptr = buf + bytesread;\
			bytesread += size;\
		}\
	} while(0)

/*
 * Prints the delimiter string 
 */
static void print_delim(FILE *fp, char *del)
{
	fprintf(fp, "%s", del);           
}

/*
 * Prints a single byte in the given format
 */
static void print_1_byte(FILE *fp, u_char val, const char *format)
{
	fprintf(fp, format, val);           
}

/*
 * Print 2 bytes in the given format
 */
static void print_2_bytes(FILE *fp, u_int16_t val, const char *format)
{
	fprintf(fp, format, val);
}


/*
 * Prints 4 bytes in the given format 
 */
static void print_4_bytes(FILE *fp, u_int32_t val, const char *format)
{
	fprintf(fp, format, val);
}

/*
 * Prints 8 bytes in the given format 
 */
static void print_8_bytes(FILE *fp, u_int64_t val, const char *format)
{ 
	fprintf(fp, format, val);
}


/*
 * Prints the given size of data bytes in hex
 */
static void print_mem(FILE *fp, u_char *data, size_t len)
{
	int i;

	if(len > 0) {
		fprintf(fp, "0x");
			for(i = 0; i < len; i++) {
				fprintf(fp, "%x", data[i]);
			}
	}
}


/*
 * Prints the given data bytes as a string
 */
static void print_string(FILE *fp, u_char *str, size_t len)
{
	int i;
	if(len > 0) {
		for(i = 0; i < len; i++) {
			if(str[i] != '\0') 
				fprintf(fp, "%c", str[i]);
		}
	}
}



/* Prints the token type in either the raw or the default form */
static void print_tok_type(FILE *fp, u_char type, const char *tokname, char raw)
{
	if(raw) {
		fprintf(fp, "%u", type);
	}
	else {
		fprintf(fp, "%s", tokname);
	}
}


/*
 * Prints a user value
 */
static void print_user(FILE *fp, u_int32_t usr, char raw)
{
	struct passwd *pwent;
	if(raw) {
		fprintf(fp, "%d", usr);
	}
	else {
		pwent = getpwuid(usr);
		if(pwent != NULL) {
			fprintf(fp, "%s", pwent->pw_name);
		}
		else {
			fprintf(fp, "%d", usr);
		}
	}
}

/*
 * Prints a group value
 */
static void print_group(FILE *fp, u_int32_t grp, char raw)
{
	struct group *grpent;

	if(raw) {
		fprintf(fp, "%d", grp);
	}
	else {
		grpent = getgrgid(grp);
		if(grpent != NULL) {
			fprintf(fp, "%s", grpent->gr_name);
		}
		else {
			fprintf(fp, "%d", grp);
		}
	}
}


/*
 * Prints the event from the header token in either
 * the short, default or raw form
 */
static void print_event(FILE *fp, u_int16_t ev, char raw, char sfrm)
{
	struct au_event_ent *e;

	e = getauevnum(ev);
	if(e == NULL) {
		fprintf(fp, "%u", ev);
		return;
	}

	if(raw) {
		fprintf(fp, "%u", ev);
	}
	else if(sfrm) {
		fprintf(fp, "%s", e->ae_name);
	}
	else {
		fprintf(fp, "%s", e->ae_desc);
	}

	free_au_event_ent(e);
}


/*
 * Prints the event modifier from the header token in either
 * the default or raw form
 */
static void print_evmod(FILE *fp, u_int16_t evmod, char raw)
{
	if(raw) {
		fprintf(fp, "%u", evmod);
	}
	else {
		fprintf(fp, "%u", evmod);
	}
}


/*
 * Prints seconds in the ctime format
 */
static void print_sec(FILE *fp, u_int32_t sec, char raw)
{
	time_t time;
	char timestr[26];

	if(raw) {
		fprintf(fp, "%u", sec);
	}
	else {
		time = (time_t)sec;
		ctime_r(&time, timestr);
		timestr[24] = '\0'; /* No new line */
		fprintf(fp, "%s", timestr);
	}
}

/*
 * Prints the excess milliseconds
 */
static void print_msec(FILE *fp, u_int32_t msec, char raw)
{
	if(raw) {
		fprintf(fp, "%u", msec);
	}
	else {
		fprintf(fp, " + %u msec", msec);
	}
}


/* prints a dotted form for the IP addres */
static void print_ip_address(FILE *fp, u_int32_t ip)
{
	struct in_addr ipaddr;

	ipaddr.s_addr = ip;
	fprintf(fp, "%s", inet_ntoa(ipaddr));
}

/* prints a string value for the given ip address */
static void print_ip_ex_address(FILE *fp, u_int32_t type, u_int32_t *ipaddr)
{
	struct in_addr ipv4;
	struct in6_addr ipv6;
	char dst[INET6_ADDRSTRLEN];
	const char *ret = NULL;

	if(type == AF_INET) {
		ipv4.s_addr = (in_addr_t)(ipaddr[0]);
		ret = inet_ntop(type, &ipv4, dst, INET6_ADDRSTRLEN);
	}
	else if(type == AF_INET6) {
		ipv6.__u6_addr.__u6_addr32[0] = ipaddr[0];	
		ipv6.__u6_addr.__u6_addr32[1] = ipaddr[1];	
		ipv6.__u6_addr.__u6_addr32[2] = ipaddr[2];	
		ipv6.__u6_addr.__u6_addr32[3] = ipaddr[3];	
		ret = inet_ntop(type, &ipv6, dst, INET6_ADDRSTRLEN);
	}

	if(ret != NULL) {
		fprintf(fp, "%s", ret);
		/* XXX  Is ret heap memory?  Leaked if so */
	}
}

/* Prints return value as success or failure */
static void print_retval(FILE *fp, u_char status, char raw)
{
	if(raw) {
		fprintf(fp, "%u", status);
	}
	else {
		if(status == 0) {
			fprintf(fp, "success");
		}
		else {
			fprintf(fp, "failure : %s", strerror(status));
		}
	}
}

/* Prints the exit value */
static void print_errval(FILE *fp, u_int32_t val)
{
	fprintf(fp, "Error %u", val);
}

/*prints IPC type */
static void print_ipctype(FILE *fp, u_char type, char raw)
{
	if(raw) {
		fprintf(fp, "%u", type);
	}
	else {
		if(type == AT_IPC_MSG) {
			fprintf(fp, "Message IPC");
		}
		else if(type == AT_IPC_SEM) {
			fprintf(fp, "Semaphore IPC");
		}
		else if(type == AT_IPC_SHM) {
			fprintf(fp, "Shared Memory IPC");
		}
		else {
			fprintf(fp, "%u", type);
		}
	}
}

/*
 * record byte count       4 bytes
 * version #               1 byte    [2]
 * event type              2 bytes
 * event modifier          2 bytes
 * seconds of time         4 bytes/8 bytes (32-bit/64-bit value)
 * milliseconds of time    4 bytes/8 bytes (32-bit/64-bit value)    
 */
static int fetch_header32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &(tok->tt.hdr32.size), 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32.version, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32.e_type, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32.e_mod, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32.s, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32.ms, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_header32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "header", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.hdr32.size, "%u");
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.hdr32.version, "%u");
	print_delim(fp, del);
	print_event(fp, tok->tt.hdr32.e_type, raw, sfrm); 	
	print_delim(fp, del);
	print_evmod(fp, tok->tt.hdr32.e_mod, raw);
	print_delim(fp, del);
	print_sec(fp, tok->tt.hdr32.s, raw);
	print_delim(fp, del);
	print_msec(fp, tok->tt.hdr32.ms, raw);
}

/*       
 * trailer magic                        2 bytes
 * record size                          4 bytes
 */
static int fetch_trailer_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.trail.magic, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.trail.count, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_trailer_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "trailer", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.trail.count, "%u");
}

/*       
 * argument #              1 byte
 * argument value          4 bytes/8 bytes (32-bit/64-bit value)
 * text length             2 bytes
 * text                    N bytes + 1 terminating NULL byte
 */
static int fetch_arg32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.arg32.no, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}
	
	READ_TOKEN_BYTES(buf, len, &tok->tt.arg32.val, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.arg32.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	
	SET_PTR(buf, len, tok->tt.arg32.text, tok->tt.arg32.len, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}


static void print_arg32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "argument", raw);
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.arg32.no, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.arg32.val, "%#x");
	print_delim(fp, del);
	print_string(fp, tok->tt.arg32.text, tok->tt.arg32.len);
}

static int fetch_arg64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.arg64.no, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}
	
	READ_TOKEN_BYTES(buf, len, &tok->tt.arg64.val, 
			sizeof(u_int64_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.arg64.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	
	SET_PTR(buf, len, tok->tt.arg64.text, tok->tt.arg64.len, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}


static void print_arg64_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "argument", raw);
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.arg64.no, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.arg64.val, "%lld");
	print_delim(fp, del);
	print_string(fp, tok->tt.arg64.text, tok->tt.arg64.len);
}

/*
 * how to print            1 byte
 * basic unit              1 byte
 * unit count              1 byte
 * data items              (depends on basic unit)
 */
static int fetch_arb_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int datasize;

	READ_TOKEN_BYTES(buf, len, &tok->tt.arb.howtopr, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.arb.bu, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.arb.uc, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	/* Determine the size of the basic unit */
	switch(tok->tt.arb.bu) {
		case AUR_BYTE:  datasize = AUR_BYTE_SIZE;
						break;

		case AUR_SHORT: datasize = AUR_SHORT_SIZE;
						break;

		case AUR_LONG:  datasize = AUR_LONG_SIZE;
						break;

		default: return -1;
	}

	SET_PTR(buf, len, tok->tt.arb.data, 
			datasize * tok->tt.arb.uc , tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_arb_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	char *str;
	char *format;
	size_t size;
	int i;

	print_tok_type(fp, tok->id, "arbitrary", raw);
	print_delim(fp, del);
	switch(tok->tt.arb.howtopr) {
		case AUP_BINARY:
			str = "binary";
			format = " %c";
			break;

		case AUP_OCTAL:	
			str = "octal";
			format = " %o";
			break;

		case AUP_DECIMAL:
			str = "decimal";
			format = " %d";
			break;

		case AUP_HEX:
			str = "hex";
			format = " %x";
			break;

		case AUP_STRING:
			str = "string";
			format = "%c";
			break;
	
		default:
			return;
	}
	print_string(fp, str, strlen(str));
	print_delim(fp, del);
	switch(tok->tt.arb.bu) {
		case AUR_BYTE:
			str = "byte";
			size = AUR_BYTE_SIZE;
			print_string(fp, str, strlen(str));
			print_delim(fp, del);
			print_1_byte(fp, tok->tt.arb.uc, "%u");	
			print_delim(fp, del);
			for(i = 0; i<tok->tt.arb.uc; i++) {
				fprintf(fp, format, *(tok->tt.arb.data + (size * i))); 
			}
			break;

		case AUR_SHORT:	
			str = "short";
			size = AUR_SHORT_SIZE;
			print_string(fp, str, strlen(str));
			print_delim(fp, del);
			print_1_byte(fp, tok->tt.arb.uc, "%u");	
			print_delim(fp, del);
			for(i = 0; i<tok->tt.arb.uc; i++) {
				fprintf(fp, format, *((u_int16_t *)(tok->tt.arb.data + (size * i)))); 
			}
			break;

		case AUR_LONG:
			str = "int";
			size = AUR_LONG_SIZE;
			print_string(fp, str, strlen(str));
			print_delim(fp, del);
			print_1_byte(fp, tok->tt.arb.uc, "%u");	
			print_delim(fp, del);
			for(i = 0; i<tok->tt.arb.uc; i++) {
				fprintf(fp, format, *((u_int32_t *)(tok->tt.arb.data + (size * i)))); 
			}
			break;

		default:
			return;
	}

}

/*       
 * file access mode        4 bytes
 * owner user ID           4 bytes
 * owner group ID          4 bytes
 * file system ID          4 bytes
 * node ID                 8 bytes
 * device                  4 bytes/8 bytes (32-bit/64-bit)
 */
static int fetch_attr32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.mode, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.uid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.gid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.fsid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.nid, 
			sizeof(u_int64_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.attr32.dev, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_attr32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "attribute", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.mode, "%o");
	print_delim(fp, del);
	print_user(fp, tok->tt.attr32.uid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.attr32.gid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.fsid, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.attr32.nid, "%lld");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.dev, "%u");
}

/*
 * status                  4 bytes
 * return value            4 bytes
 */
static int fetch_exit_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.exit.status, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.exit.ret, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_exit_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "exit", raw);
	print_delim(fp, del);
	print_errval(fp, tok->tt.exit.status);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.exit.ret, "%u");
}

/*
 * count                   4 bytes
 * text                    count null-terminated string(s)
 */
static int fetch_execarg_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;
	char *bptr;

	READ_TOKEN_BYTES(buf, len, &tok->tt.execarg.count, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	for(i = 0; i< tok->tt.execarg.count; i++) {
		bptr = buf + tok->len;
		tok->tt.execarg.text[i] = bptr;
		/* look for a null terminated string */
		while(bptr && (*bptr != '\0')) {
			if(++tok->len >=len)
				return -1;
			bptr = buf + tok->len;
		}	
		if(!bptr)
			return -1;
		tok->len++; /* \0 character */	
	}

	return 0;
}

static void print_execarg_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "exec arg", raw);
	for(i = 0; i< tok->tt.execarg.count; i++) {
		print_delim(fp, del);
		print_string(fp, tok->tt.execarg.text[i], 
			strlen(tok->tt.execarg.text[i]));
	}
}

/*
 * count                   4 bytes
 * text                    count null-terminated string(s)
 */
static int fetch_execenv_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;
	char *bptr;

	READ_TOKEN_BYTES(buf, len, &tok->tt.execenv.count, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	for(i = 0; i< tok->tt.execenv.count; i++) {
		bptr = buf + tok->len;
		tok->tt.execenv.text[i] = bptr;
		/* look for a null terminated string */
		while(bptr && (*bptr != '\0')) {
			if(++tok->len >=len)
				return -1;
			bptr = buf + tok->len;
		}	
		if(!bptr)
			return -1;
		tok->len++; /* \0 character */	
	}

	return 0;
}

static void print_execenv_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "exec arg", raw);
	for(i = 0; i< tok->tt.execenv.count; i++) {
		print_delim(fp, del);
		print_string(fp, tok->tt.execenv.text[i], strlen(tok->tt.execenv.text[i]));
	}
}
/*
 * seconds of time          4 bytes
 * milliseconds of time     4 bytes
 * file name len            2 bytes
 * file pathname            N bytes + 1 terminating NULL byte
 */
static int fetch_file_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.file.s, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.file.ms, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.file.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	SET_PTR(buf, len, tok->tt.file.name, tok->tt.file.len, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_file_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "file", raw);
	print_delim(fp, del);
	print_sec(fp, tok->tt.file.s, raw);
	print_delim(fp, del);
	print_msec(fp, tok->tt.file.ms, raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.file.name, tok->tt.file.len);
}

/*
 * number groups           2 bytes
 * group list              count * 4 bytes
 */
static int fetch_newgroups_tok(tokenstr_t *tok, char *buf, int len)
{
	int i;
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.grps.no, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	for(i = 0; i<tok->tt.grps.no; i++) {

		READ_TOKEN_BYTES(buf, len, &tok->tt.grps.list[i], 
		sizeof(u_int32_t), tok->len, err);
    	if(err) {
    		return -1;
    	}
	}

	return 0;
}

static void print_newgroups_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "group", raw);
	for (i = 0; i < tok->tt.grps.no; i++) {
		print_delim(fp, del);
		print_group(fp, tok->tt.grps.list[i], raw);
	}
}

/*
 * internet addr 4 bytes
 */
static int fetch_inaddr_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr.addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;

}

static void print_inaddr_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "ip addr", raw);
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.inaddr.addr);
}

/*
 * type 	4 bytes
 * address 16 bytes
 */
static int fetch_inaddr_ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;

	READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr_ex.type, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	
	if(tok->tt.inaddr_ex.type == AF_INET) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr_ex.addr[0], 
			sizeof(u_int32_t), tok->len, err);
		if(err) {
			return -1;
		}
	}
	else if (tok->tt.inaddr_ex.type == AF_INET6) {
		for(i = 0; i < 4; i++) {
			READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr_ex.addr[i], 
				sizeof(u_int32_t), tok->len, err);
			if(err) {
				return -1;
			}
		}                                              
    }
	else {
		return -1;
	}

	return 0;
}

static void print_inaddr_ex_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "ip addr ex", raw);
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.inaddr_ex.type, tok->tt.inaddr_ex.addr);
}

/*
 * ip header     20 bytes
 */
static int fetch_ip_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.version, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.tos, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.id, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.offset, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.ttl, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.prot, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.chksm, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.src, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.dest, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_ip_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "ip", raw);
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.version), sizeof(u_char));
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.tos), sizeof(u_char));
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.ip.len, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.ip.id, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.ip.offset, "%u");
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.ttl), sizeof(u_char));
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.prot), sizeof(u_char));
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.ip.chksm, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ip.src, "%#x");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ip.dest, "%#x");
}

/*
 * object ID type       1 byte
 * Object ID            4 bytes
 */
static int fetch_ipc_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipc.type, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipc.id, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_ipc_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "IPC", raw);
	print_delim(fp, del);
	print_ipctype(fp, tok->tt.ipc.type, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipc.id, "%u");
}

/*
 * owner user id        4 bytes
 * owner group id       4 bytes
 * creator user id      4 bytes
 * creator group id     4 bytes
 * access mode          4 bytes
 * slot seq                     4 bytes
 * key                          4 bytes
 */
static int fetch_ipcperm_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.uid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.gid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.puid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.pgid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.mode, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.seq, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ipcperm.key, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_ipcperm_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "IPC perm", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.ipcperm.uid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.ipcperm.gid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.ipcperm.puid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.ipcperm.pgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.mode, "%o");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.seq, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.key, "%u");
}

/*
 * port Ip address  2 bytes
 */
static int fetch_iport_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.iport.port, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_iport_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "ip port", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.iport.port, "%#x");
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int fetch_opaque_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.opaque.size, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	SET_PTR(buf, len, tok->tt.opaque.data, tok->tt.opaque.size, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_opaque_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "opaque", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.opaque.size, "%u");
	print_delim(fp, del);
	print_mem(fp, tok->tt.opaque.data, tok->tt.opaque.size);
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int fetch_path_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.path.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	SET_PTR(buf, len, tok->tt.path.path, tok->tt.path.len, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_path_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "path", raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.path.path, tok->tt.path.len);
}

/*
 * token ID                     1 byte
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes
 *   machine id         4 bytes
 */
static int fetch_process32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.auid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.euid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.egid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.ruid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.rgid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.pid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.sid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.tid.port, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.tid.addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_process32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "process", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.tid.port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.proc32.tid.addr);
}

static int fetch_process32ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.auid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.euid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.egid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.ruid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.rgid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.pid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.sid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.tid.port, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.tid.type, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	if(tok->tt.proc32_ex.tid.type == AF_INET) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.tid.addr[0], 
			sizeof(u_int32_t), tok->len, err);
		if(err) {
			return -1;
		}
	}
	else if (tok->tt.proc32_ex.tid.type == AF_INET6) {
		for(i = 0; i < 4; i++) {
			READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.tid.addr[i], 
				sizeof(u_int32_t), tok->len, err);
			if(err) {
				return -1;
			}
		}                                              
    }
	else {
		return -1;
	}

	return 0;
}

static void print_process32ex_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "process_ex", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32_ex.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32_ex.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.tid.port, "%u");
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.proc32_ex.tid.type, tok->tt.proc32_ex.tid.addr);
}

/*
 * errno                        1 byte
 * return value         4 bytes
 */
static int fetch_return32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.ret32.status, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ret32.ret, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_return32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "return", raw);
	print_delim(fp, del);
	print_retval(fp, tok->tt.ret32.status, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ret32.ret, "%u");
}

static int fetch_return64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.ret64.err, 
			sizeof(u_char), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.ret64.val, 
			sizeof(u_int64_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_return64_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "return", raw);
	print_delim(fp, del);
	print_retval(fp, tok->tt.ret64.err, raw);
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.ret64.val, "%lld");
}

/*
 * seq                          4 bytes
 */
static int fetch_seq_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.seq.seqno, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}


static void print_seq_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "sequence", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.seq.seqno, "%u");
}


/*
 * socket family           2 bytes
 * local port              2 bytes
 * socket address          4 bytes
 */
static int fetch_sock_inet32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockinet32.family, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockinet32.port, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockinet32.addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_sock_inet32_tok(FILE *fp, tokenstr_t *tok, char *del,
		char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "socket-inet", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.sockinet32.family, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.sockinet32.port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.sockinet32.addr);
}

/*
 * socket family           2 bytes
 * path                    104 bytes
 */
static int fetch_sock_unix_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockunix.family, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.sockunix.path, 
			104, tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_sock_unix_tok(FILE *fp, tokenstr_t *tok, char *del,
		char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "socket-unix", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.sockunix.family, "%u");
	print_delim(fp, del);
	print_string(fp, tok->tt.sockunix.path, strlen(tok->tt.sockunix.path));
}

/*
 * socket type             2 bytes
 * local port              2 bytes
 * local address           4 bytes
 * remote port             2 bytes
 * remote address          4 bytes
 */
static int fetch_socket_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.type, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.l_port, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.l_addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.r_port, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.r_addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	return 0;
}

static void print_socket_tok(FILE *fp, tokenstr_t *tok, char *del,
		char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "socket", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket.type, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket.l_port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket.l_addr);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket.r_port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket.r_addr);
}

/*
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes
 *   machine id         4 bytes
 */
static int fetch_subject32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.auid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.euid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.egid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.ruid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.rgid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.pid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.sid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.tid.port, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.tid.addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	return 0;
}

static void print_subject32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "subject", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.tid.port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.subj32.tid.addr);
}

/*
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes
 *	 type				4 bytes
 *   machine id         16 bytes
 */
static int fetch_subject32ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.auid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.euid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.egid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.ruid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.rgid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.pid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.sid, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.tid.port, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.tid.type, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	if(tok->tt.subj32_ex.tid.type == AF_INET) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.tid.addr[0], 
			sizeof(u_int32_t), tok->len, err);
		if(err) {
			return -1;
		}
	}
	else if (tok->tt.subj32_ex.tid.type == AF_INET6) {
		for(i = 0; i < 4; i++) {
			READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.tid.addr[i], 
				sizeof(u_int32_t), tok->len, err);
			if(err) {
				return -1;
			}
		}                                              
    }
	else {
		return -1;
	}

	return 0;
}

static void print_subject32ex_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "subject_ex", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32_ex.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32_ex.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.tid.port, "%u");
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.subj32_ex.tid.type, tok->tt.subj32_ex.tid.addr);
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int fetch_text_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.text.len, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}

	SET_PTR(buf, len, tok->tt.text.text, tok->tt.text.len, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_text_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "text", raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.text.text, tok->tt.text.len);
}

/*
 * socket type             2 bytes
 * local port              2 bytes
 * address type/length     4 bytes
 * local Internet address  4 bytes 
 * remote port             4 bytes      
 * address type/length     4 bytes
 * remote Internet address 4 bytes
 */
static int fetch_socketex32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.type, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.l_port, 
			sizeof(u_int16_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.l_ad_type, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.l_addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.r_port, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.r_ad_type, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.r_addr, 
			sizeof(u_int32_t), tok->len, err);
	if(err) {
		return -1;
	}
	return 0;
}

static void print_socketex32_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "socket", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket_ex32.type, "%#x");
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket_ex32.l_port, "%#x");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket_ex32.l_addr);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.socket_ex32.r_port, "%#x");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket_ex32.r_addr);
}

static int fetch_invalid_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int recoversize;

	recoversize = len - tok->len - TRAILER_SIZE;
	if(recoversize <= 0) {
		return -1;
	}

	tok->tt.invalid.length = recoversize;

	SET_PTR(buf, len, tok->tt.invalid.data, recoversize, tok->len, err);		
	if(err) {
		return -1;
	}

	return 0;
}

static void print_invalid_tok(FILE *fp, tokenstr_t *tok, char *del,
                char raw, char sfrm)
{
	print_tok_type(fp, tok->id, "unknown", raw);
	print_delim(fp, del);
	print_mem(fp, tok->tt.invalid.data, tok->tt.invalid.length);
}


/*
 * Reads the token beginning at buf into tok
 */
int au_fetch_tok(tokenstr_t *tok, u_char *buf, int len)
{
	if(len <= 0) {
		return -1;
	}

	tok->len = 1;
	tok->data = buf;
	tok->id = *buf;

	switch(tok->id) {

		case AU_HEADER_32_TOKEN :
				return fetch_header32_tok(tok, buf, len);

		case AU_TRAILER_TOKEN :
				return fetch_trailer_tok(tok, buf, len);

		case AU_ARG32_TOKEN :
				return fetch_arg32_tok(tok, buf, len);

		case AU_ARG64_TOKEN :
				return fetch_arg64_tok(tok, buf, len);

		case AU_ATTR32_TOKEN :
				return fetch_attr32_tok(tok, buf, len);

		case AU_EXIT_TOKEN :
				return fetch_exit_tok(tok, buf, len);

		case AU_EXEC_ARG_TOKEN :
				return fetch_execarg_tok(tok, buf, len);

		case AU_EXEC_ENV_TOKEN :
				return fetch_execenv_tok(tok, buf, len);

		case AU_FILE_TOKEN :
				return fetch_file_tok(tok, buf, len);

		case AU_NEWGROUPS_TOKEN :
				return fetch_newgroups_tok(tok, buf, len);

		case AU_IN_ADDR_TOKEN :
				return fetch_inaddr_tok(tok, buf, len);

		case AU_IN_ADDR_EX_TOKEN :
				return fetch_inaddr_ex_tok(tok, buf, len);

		case AU_IP_TOKEN :
				return fetch_ip_tok(tok, buf, len);

		case AU_IPC_TOKEN :
				return fetch_ipc_tok(tok, buf, len);

		case AU_IPCPERM_TOKEN :
				return fetch_ipcperm_tok(tok, buf, len);

		case AU_IPORT_TOKEN :
				return fetch_iport_tok(tok, buf, len);

		case AU_OPAQUE_TOKEN :
				return fetch_opaque_tok(tok, buf, len);

		case AU_PATH_TOKEN :
				return fetch_path_tok(tok, buf, len);

		case AU_PROCESS_32_TOKEN :
				return fetch_process32_tok(tok, buf, len);

		case AU_PROCESS_32_EX_TOKEN :
				return fetch_process32ex_tok(tok, buf, len);

		case AU_RETURN_32_TOKEN :
				return fetch_return32_tok(tok, buf, len);

		case AU_RETURN_64_TOKEN :
				return fetch_return64_tok(tok, buf, len);

		case AU_SEQ_TOKEN :
				return fetch_seq_tok(tok, buf, len);

		case AU_SOCK_TOKEN :
				return fetch_socket_tok(tok, buf, len);

		case AU_SOCK_INET_32_TOKEN :
				return fetch_sock_inet32_tok(tok, buf, len);

		case AU_SOCK_UNIX_TOKEN :
				return fetch_sock_unix_tok(tok, buf, len);

		case AU_SUBJECT_32_TOKEN :
				return fetch_subject32_tok(tok, buf, len);

		case AU_SUBJECT_32_EX_TOKEN :
				return fetch_subject32ex_tok(tok, buf, len);

		case AU_TEXT_TOKEN :
				return fetch_text_tok(tok, buf, len);

		case AU_SOCK_EX32_TOKEN :
				return fetch_socketex32_tok(tok, buf, len);

		case AU_ARB_TOKEN :
				return fetch_arb_tok(tok, buf, len);

		default:
				return fetch_invalid_tok(tok, buf, len);
	}
}

/*
 * 'prints' the token out to outfp 
 */
void au_print_tok(FILE *outfp, tokenstr_t *tok, char *del, char raw, char sfrm)
{
	switch(tok->id) {

		case AU_HEADER_32_TOKEN :
				return print_header32_tok(outfp, tok, del, raw, sfrm);

		case AU_TRAILER_TOKEN :
				return print_trailer_tok(outfp, tok, del, raw, sfrm);

		case AU_ARG32_TOKEN :
				return print_arg32_tok(outfp, tok, del, raw, sfrm);

		case AU_ARG64_TOKEN :
				return print_arg64_tok(outfp, tok, del, raw, sfrm);

		case AU_ARB_TOKEN :
				return print_arb_tok(outfp, tok, del, raw, sfrm);

		case AU_ATTR32_TOKEN :
				return print_attr32_tok(outfp, tok, del, raw, sfrm);

		case AU_EXIT_TOKEN :
				return print_exit_tok(outfp, tok, del, raw, sfrm);

		case AU_EXEC_ARG_TOKEN :
				return print_execarg_tok(outfp, tok, del, raw, sfrm);

		case AU_EXEC_ENV_TOKEN :
				return print_execenv_tok(outfp, tok, del, raw, sfrm);

		case AU_FILE_TOKEN :
				return print_file_tok(outfp, tok, del, raw, sfrm);

		case AU_NEWGROUPS_TOKEN :
				return print_newgroups_tok(outfp, tok, del, raw, sfrm);

		case AU_IN_ADDR_TOKEN :
				return print_inaddr_tok(outfp, tok, del, raw, sfrm);

		case AU_IN_ADDR_EX_TOKEN :
				return print_inaddr_ex_tok(outfp, tok, del, raw, sfrm);

		case AU_IP_TOKEN :
				return print_ip_tok(outfp, tok, del, raw, sfrm);

		case AU_IPC_TOKEN :
				return print_ipc_tok(outfp, tok, del, raw, sfrm);

		case AU_IPCPERM_TOKEN :
				return print_ipcperm_tok(outfp, tok, del, raw, sfrm);

		case AU_IPORT_TOKEN :
				return print_iport_tok(outfp, tok, del, raw, sfrm);

		case AU_OPAQUE_TOKEN :
				return print_opaque_tok(outfp, tok, del, raw, sfrm);

		case AU_PATH_TOKEN :
				return print_path_tok(outfp, tok, del, raw, sfrm);

		case AU_PROCESS_32_TOKEN :
				return print_process32_tok(outfp, tok, del, raw, sfrm);

		case AU_PROCESS_32_EX_TOKEN :
				return print_process32ex_tok(outfp, tok, del, raw, sfrm);

		case AU_RETURN_32_TOKEN :
				return print_return32_tok(outfp, tok, del, raw, sfrm);

		case AU_RETURN_64_TOKEN :
				return print_return64_tok(outfp, tok, del, raw, sfrm);

		case AU_SEQ_TOKEN :
				return print_seq_tok(outfp, tok, del, raw, sfrm);

		case AU_SOCK_TOKEN :
				return print_socket_tok(outfp, tok, del, raw, sfrm);

		case AU_SOCK_INET_32_TOKEN :
				return print_sock_inet32_tok(outfp, tok, del, raw, sfrm);

		case AU_SOCK_UNIX_TOKEN :
				return print_sock_unix_tok(outfp, tok, del, raw, sfrm);

		case AU_SUBJECT_32_TOKEN :
				return print_subject32_tok(outfp, tok, del, raw, sfrm);

		case AU_SUBJECT_32_EX_TOKEN :
				return print_subject32ex_tok(outfp, tok, del, raw, sfrm);

		case AU_TEXT_TOKEN :
				return print_text_tok(outfp, tok, del, raw, sfrm);

		case AU_SOCK_EX32_TOKEN :
				return print_socketex32_tok(outfp, tok, del, raw, sfrm);

		default:
				return print_invalid_tok(outfp, tok, del, raw, sfrm);
	}
}

/* 
 * Rread a record from the file pointer, store data in buf 
 * memory for buf is also allocated in this function 
 * and has to be free'd outside this call
 */
int au_read_rec(FILE *fp, u_char **buf)
{
	u_char *bptr;
	u_int32_t recsize;
	u_int32_t bytestoread;
	u_char type;

	type = fgetc(fp);
	/* record must begin with a header token */
	if(type != AU_HEADER_32_TOKEN) {
		return -1;
	}

	/* read the record size from the token */
	if(fread(&recsize, 1, sizeof(u_int32_t), fp) < sizeof(u_int32_t)) {
		return -1;
	}

	/* Check for recsize sanity */
	if(recsize < (sizeof(u_int32_t) + sizeof(u_char))) {
		return -1;
	}

	*buf = (u_char *)malloc(recsize * sizeof(u_char));
	if(*buf == NULL) {
		return -1;
	}
	bptr = *buf;
	memset(bptr, 0, recsize);

	/* store the token contents already read, back to the buffer*/
	*bptr = type;
	bptr++;
	memcpy(bptr, &recsize, sizeof(u_int32_t));
	bptr += sizeof(u_int32_t);

	/* now read remaining record bytes */
	bytestoread = recsize - sizeof(u_int32_t) - sizeof(u_char);

	if(fread(bptr, 1, bytestoread, fp) < bytestoread) {
		free(*buf);
		return -1;
	}

	return recsize;
}

