#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <bsm/audit_kevents.h>

#include <libbsm.h>

#define WRITE_TOKEN(fd, tok, type)	\
	do {	\
		if((tok == NULL) || (-1 == au_write(fd, tok))) { \
			fprintf(stderr, "Error writing token : %s\n", type);	\
		}	\
	} while(0);
					

void write_arb_token(int fd)
{
	token_t *tok1, *tok2, *tok3, *tok4, *tok5;
	char bin[10] = "1010101010";
	short oct[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }; 
	char hex[10] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa }; 
	int dec[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }; 
	char str[10] = "abcdefghij"; 

	tok1 = au_to_data(AUP_BINARY, AUR_BYTE, 10, (char *)bin);
	WRITE_TOKEN(fd, tok1, "arbitrary");
	tok2 = au_to_data(AUP_OCTAL, AUR_SHORT, 10, (char *)oct);
	WRITE_TOKEN(fd, tok2, "arbitrary");
	tok3 = au_to_data(AUP_HEX, AUR_BYTE, 10, (char *)hex);
	WRITE_TOKEN(fd, tok3, "arbitrary");
	tok4 = au_to_data(AUP_DECIMAL, AUR_LONG, 10, (char *)dec);
	WRITE_TOKEN(fd, tok4, "arbitrary");
	tok5 = au_to_data(AUP_STRING, AUR_BYTE, 10, (char *)str);
	WRITE_TOKEN(fd, tok5, "arbitrary");
}


void write_arg32token(int fd)
{
	token_t *tok;

	tok = au_to_arg32(1, "addr", 10);
	WRITE_TOKEN(fd, tok, "arg32");
} 

void write_arg64token(int fd)
{
	token_t *tok;

	tok = au_to_arg64(1, "addr", 10);
	WRITE_TOKEN(fd, tok, "arg64");
} 

void write_attr32token(int fd)
{
	token_t *tok;
	struct vattr attr;
		
	attr.va_mode = 1;
	attr.va_uid = 2;
	attr.va_gid = 3;
	attr.va_fsid = 4;
	attr.va_fileid = 5;
	attr.va_rdev = 6;

	tok = au_to_attr32(&attr);
	WRITE_TOKEN(fd, tok, "attr32");
} 

void write_execargstoken(int fd)
{
	const char *args[] = { "arg1", "arg2", "arg3", NULL }; 

	token_t *tok = au_to_exec_args(args);
	WRITE_TOKEN(fd, tok, "execargs");

}

void write_execenvtoken(int fd)
{
	const char *env[] = { "env1", "env2", "env3", NULL }; 

	token_t *tok = au_to_exec_env(env);
	WRITE_TOKEN(fd, tok, "execenv")
}

void write_exittoken(int fd)
{
	token_t *tok;

	tok = au_to_exit(1, 0);
	WRITE_TOKEN(fd, tok, "exit");
}

void write_filetoken(int fd)
{
	token_t *tok;

	tok = au_to_file("some/dummy/file");
	WRITE_TOKEN(fd, tok, "file");
}

void write_newgroupstoken(int fd)
{
	token_t *tok;

	int groups[] = { 1, 2, 3, 4, 5};

	tok = au_to_newgroups(5, groups);
	WRITE_TOKEN(fd, tok, "newgroups");
}

void write_inaddrtoken(int fd)
{
	token_t *tok;
	struct in_addr addr;

	if(0 == inet_aton("1.2.3.4", &addr))
	{
		printf("Incorrect Ip address in in_addr");
		exit(1);
	}

	tok = au_to_in_addr(&addr);
	WRITE_TOKEN(fd, tok, "inaddr");
}

void write_inaddrextoken(int fd)
{
	token_t *tok;
	struct in6_addr addr;

	addr.__u6_addr.__u6_addr32[0] = 1;
	addr.__u6_addr.__u6_addr32[0] = 2;
	addr.__u6_addr.__u6_addr32[0] = 3;
	addr.__u6_addr.__u6_addr32[0] = 4;

	tok = au_to_in_addr_ex(&addr);
	WRITE_TOKEN(fd, tok, "inaddr6");
}

void write_iptoken(int fd)
{
	token_t *tok;

	u_char iphdr[20] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5 ,6, 7, 8, 9};

	tok = au_to_ip((struct ip *)iphdr);
	WRITE_TOKEN(fd, tok, "ip");
}

void write_ipctoken(int fd)
{
	token_t *tok;

	tok = au_to_ipc(1, 2);
	WRITE_TOKEN(fd, tok, "ipc");
}

void write_ipcpermtoken(int fd)
{
	token_t *tok;

	struct ipc_perm perm;

	perm.uid = 0;
	perm.gid = 1;
	perm.cuid = 2;
	perm.cgid = 3;
	perm.mode = 4;
	perm.seq = 5;
	perm.key = 6;

	tok = au_to_ipc_perm(&perm);
	WRITE_TOKEN(fd, tok, "ipcperm");
}

void write_iporttoken(int fd)
{
	token_t *tok;

	tok = au_to_iport(1);
	WRITE_TOKEN(fd, tok, "iport");
}

void write_metoken(int fd)
{
	token_t *tok;

	tok = au_to_me();
	WRITE_TOKEN(fd, tok, "me");

}
void write_opaquetoken(int fd)
{
	token_t *tok;

	u_char data[] = { 1, 2, 3, 4, 5}; 

	tok = au_to_opaque(data, 5);
	WRITE_TOKEN(fd, tok, "opaque");
}

void write_pathtoken(int fd)
{
	token_t *tok;

	tok = au_to_path("some/dummy/path");
	WRITE_TOKEN(fd, tok, "path");
}

void write_process32token(int fd)
{
	token_t *tok;
	
	au_tid_t tid;
	tid.port = 1;
	tid.machine = inet_addr("1.2.3.4");  

	tok = au_to_process32(1, 2, 3 ,4, 5, 6, 7, &tid);
	WRITE_TOKEN(fd, tok, "process32");
}

void write_process32extoken(int fd)
{
	token_t *tok;
	
	au_tid_addr_t tid;
	tid.at_port = 1;
	tid.at_type = AF_INET6;
	tid.at_addr[0] = 1;
	tid.at_addr[1] = 2;
	tid.at_addr[2] = 3;
	tid.at_addr[3] = 4;

	tok = au_to_process32_ex(1, 2, 3 ,4, 5, 6, 7, &tid);
	WRITE_TOKEN(fd, tok, "process32ex");
}

void write_return32token(int fd)
{
	token_t *tok;

	tok = au_to_return32(1, 10);
	WRITE_TOKEN(fd, tok, "return32");
}

void write_return64token(int fd)
{
	token_t *tok;

	tok = au_to_return64(1, 10);
	WRITE_TOKEN(fd, tok, "return64");
}

void write_seqtoken(int fd)
{
	token_t *tok;

	tok = au_to_seq(100);
	WRITE_TOKEN(fd, tok, "seq");
}

void write_subject32token(int fd)
{
	token_t *tok;
	
	au_tid_t tid;
	tid.port = 1;
	tid.machine = inet_addr("1.2.3.4");  

	tok = au_to_subject32(1, 2, 3 ,4, 5, 6, 7, &tid);
	WRITE_TOKEN(fd, tok, "subject32");
}

void write_subject32extoken(int fd)
{
	token_t *tok;
	
	au_tid_addr_t tid;
	tid.at_port = 1;
	tid.at_type = AF_INET6;
	tid.at_addr[0] = 1;
	tid.at_addr[1] = 2;
	tid.at_addr[2] = 3;
	tid.at_addr[3] = 4;

	tok = au_to_subject32_ex(1, 2, 3 ,4, 5, 6, 7, &tid);
	WRITE_TOKEN(fd, tok, "subject32ex");
}

void write_texttoken(int fd)
{
	token_t *tok;

	tok = au_to_text("some-sample-text");
	WRITE_TOKEN(fd, tok, "text");

}


int writerec()
{
	int fd = au_open();

	write_arb_token(fd);
	write_arg32token(fd);
	write_arg64token(fd);
	write_attr32token(fd);
	write_execargstoken(fd);
	write_execenvtoken(fd);
	write_exittoken(fd);	
	write_filetoken(fd);
	write_newgroupstoken(fd);
	write_inaddrtoken(fd);
	write_inaddrextoken(fd);
	write_iptoken(fd);
	write_ipctoken(fd);
	write_ipcpermtoken(fd);
	write_iporttoken(fd);
	write_metoken(fd);
	write_opaquetoken(fd);
	write_pathtoken(fd);
	write_process32token(fd);
	write_process32extoken(fd);
	write_return32token(fd);
	write_return64token(fd);
	write_seqtoken(fd);
	write_subject32token(fd);
	write_subject32extoken(fd);
	write_texttoken(fd);

	if(-1 == au_close(fd, 1, AUE_FORK))
	{
		printf("Could not commit token");
		exit(1);
	}

	return 1;
}


int readrec(char *file)
{
	u_char c;

	int fd = open(file, O_RDONLY);
	if(fd == -1) {
		printf("Cannot open audit trail file");
		return -1;
	}

	while (read(fd, &c, 1) != 0) {
		printf("Next byte read = %x \n", c);
	}

	close(fd);

	return 0;
}


void usage()
{
	printf("Arguments read [file] / write\n");
	exit(1);
}


int main(int argc, char **argv)
{
	char *file;

	if(argc == 1) {
		writerec();
		return 1;
	}

	if(argc == 3) {
		file = *(argv + 2);
	}
	else {
		file = "audit.trail";
	}	
	
	if(argc > 3) { 
		usage();
	}
	else {
		if(strcmp(*(argv + 1), "write") == 0)
			writerec();
		else if(strcmp(*(argv + 1), "read") == 0)
			readrec(file);
		else 
			usage();
	} 	

	return 1;
}
