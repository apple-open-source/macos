//
//  main.c
//  tlsnketest
//
//  Created by Fabrice Gautier on 12/7/11.
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/kext_net.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>

#include <stdbool.h>

#include <AssertMacros.h>
#include "tlssocket.h"
#include "tlsnke.h"


static void print_data(const char *s, size_t l, const unsigned char *p)
{
    printf("%s, %zu:",s, l);
    for(int i=0; i<l; i++)
        printf(" %02x", p[i]);
    printf("\n");
}

static void *server_thread_func(void *arg)
{
    int sock;
    struct sockaddr_in server_addr;
    int err;
    
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("server socket");
        exit(1);
    }
    
    // Dont use TLSSocket_Attach for the server:
    // TLSSocket_Attach can only open one TLS socket at a time.
    {
        struct so_nke so_tlsnke;

        memset(&so_tlsnke, 0, sizeof(so_tlsnke));
        so_tlsnke.nke_handle = TLS_HANDLE_IP4;
        err=setsockopt(sock, SOL_SOCKET, SO_NKE, &so_tlsnke, sizeof(so_tlsnke));
        if(err<0) {
            perror("attach (server)");
            exit(err);
        }
    }
    
    server_addr.sin_family = AF_INET;         
    server_addr.sin_port = htons(23232);
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    bzero(&(server_addr.sin_zero),8); 
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))
        == -1) {
        perror("Unable to bind");
        exit(1);
    }
    
    printf("\nBound - Server Waiting for client on port 23232\n");
    fflush(stdout);
    
    while (1)
    {
        int rc;
        SSLRecord rec;
        rc=TLSSocket_Funcs.read((intptr_t)sock, &rec);
        if(!rc) {
            print_data("recvd", rec.contents.length, rec.contents.data);
            rec.contents.data[rec.contents.length-1]=0;
            printf("recvd: %ld, %s\n", rec.contents.length, rec.contents.data);
            free(rec.contents.data);
        } else {
            printf("read failed: %d\n", rc);
        }
    }
    
    close(sock);
    return NULL;
}

static int create_client_socket(const char *hostname)
{
    int sock;
    int err;
    
    
    printf("Create client socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock<0) {
        perror("client socket");
        return sock;
    }

    
#if 1
    err=TLSSocket_Attach(sock);
    if(err<0) {
        perror("TLSSocket_Attach (server)");
        exit(err);
    }
#endif 
    

    struct hostent *host;
    struct sockaddr_in server_addr;  
    
    //host = gethostbyname("kruk.apple.com");
    //host = gethostbyname("localhost");
    host= gethostbyname(hostname);
    if(!host) {
        herror("host");
        return -1;
    }
    server_addr.sin_family = AF_INET;     
    server_addr.sin_port = htons(23232);   
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(server_addr.sin_zero),8); 
    
    err = connect(sock, (struct sockaddr *)&server_addr,
                  sizeof(struct sockaddr));
    if(err)
    {
        perror("connect");
        return err;
    }

    return sock;
}

/* simple test */
static int kext_test(const char *hostname, int bypass)
{ 
    int sock, i;
    char send_data[1024];
    int tlsfd;
    pthread_t server_thread;
    
    if(strcmp(hostname, "localhost")==0) {
        pthread_create(&server_thread, NULL, server_thread_func, NULL);
        // Just wait for the server to be setup
        sleep(1);
    }
    
    
    sock = create_client_socket(hostname);

    if(bypass) {
        /* Have to open this after we attached the filter to the client socket */
        tlsfd=open("/dev/tlsnke", O_RDWR);
        if(tlsfd<0) {
            perror("open tlsnke");
            exit(1);
        }
    }
    

    for(i=0; i<20;i++) {
        int n;
        ssize_t err;
        n=sprintf(send_data, "Message #%d\n", i);
        if(n<0) {
            perror("sprintf");
            exit(1);
        }

        printf("Client(1) sending %d bytes (\"%s\")\n", n, send_data);
        
        if(bypass) {
            err = write(tlsfd, send_data, n);
            if(err<0) {
                perror("write to tlsnke");
                exit(1);
            }
        } else {
            SSLRecord rec;

            rec.contentType = SSL_RecordTypeAppData;
            rec.protocolVersion = DTLS_Version_1_0;
            rec.contents.data = (uint8_t *)send_data;
            rec.contents.length = n;

            err = TLSSocket_Funcs.write((intptr_t)sock, rec);
            if(err<0) {
                perror("write to socket");
                exit(1);
            }

            /* serviceWriteQueue every 2 writes, this will trigger rdar://11348395 */
            if(i&1) {
                int err;
                err = TLSSocket_Funcs.serviceWriteQueue((intptr_t)sock);
                if(err<0) {
                    perror("service write queue");
                    exit(1);
                }
            }
        }

        sleep(1);
    }

    return 0;
}


/* handshake test */
int st_test();

/* echo test */
int dtls_client(const char *hostname, int bypass);

static
int usage(const char *argv0)
{
    printf("Usage: %s <test> <hostname> <bypass>\n", argv0);
    printf("     <test>: type of test: 's'imple, 'h'andshake or 'e'cho] (see below)\n");
    printf("     <hostname>: hostname of server\n");
    printf("     <bypass>: use /dev/tlsnke bypass test\n");

    printf("\n    'S'imple test:\n"           
           "\tVery basic test with no handshake. DTLS packets are sent through the socket filter, non encrypted.\n"
           "\tIf hostname is 'localhost', a local simple server will be created that will also use the tls filter,\n"
           "\tsuch that the input path is tested.\n" 
           "\tOtherwise, a server on the other side is not required only the output path is tested. If there is no server replying\n"
           "\tonly the ouput path will be tested. If a server is replying, input packet will be processed but are never read to userspace\n"
           "\tif bypass=1, also send the same packet through the /dev/tlsnke interface, as if they were coming from utun\n");

    printf("\n    'H'andshake:\n");
    printf("\tTest SSL Handshake with various ciphers, between a local client going through the tlsnke\n"
           "\tfilter, and a local server using only the userland SecureTransport.\n"
           "\thostname and bypass are ignored.\n");
   
    printf("\n    'E'cho:\n");
    printf("\tTest to connect to an udp echo server indicated by hostname, on port 23232.\n"
           "\tSet bypass=1 to use the /dev/tlsnke bsd device to send/recv the app data (emulate utun behaviour)\n");
    
    printf("\n\tbypass=1 require the tlsnke kext to be compiled with TLS_TEST=1 (not the default in the build)\n");
    
    return -1;
}

int main (int argc, const char * argv[])
{
    
    printf("argv0=%s argc=%d\n", argv[0], argc);
    if(argc<2)
        return usage(argv[0]);
    
    switch (argv[1][0]) {
    case 's':
    case 'S':
            if(argc<3) return usage(argv[0]);
            return kext_test(argv[2], atoi(argv[3])?1:0);
    case 'h':
    case 'H':
            return st_test();
    case 'e':
    case 'E':            
            if(argc<3) return usage(argv[0]);
            return dtls_client(argv[2], atoi(argv[3])?1:0);
    default:
            return usage(argv[0]);
    }
}

