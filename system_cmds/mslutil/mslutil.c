//
//  mslutil.c
//  mslutil
//
//  Created by Christopher Deppe on 3/31/17.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/sysctl.h>
#include <stack_logging.h>

#define    BSD_PID_MAX    99999        /* Copy of PID_MAX from sys/proc_internal.h. */

static void print_usage()
{
    printf("usage: mslutil pid [--disable] | [--enable malloc | vm | full | lite | vmlite]\n");
}

static int send_msl_command(uint64_t pid, uint64_t flavor)
{
    uint64_t flags = flavor;
    flags <<= 32;
    
    flags |= (pid & 0xFFFFFFFF);
    
    int ret = sysctlbyname("kern.memorystatus_vm_pressure_send", 0, 0, &flags, sizeof(flags));
    
    if (ret) {
        printf("send_msl_command - sysctl: kern.memorystatus_vm_pressure_send failed %s\n", strerror(errno));
    } else {
        printf("send_msl_command - success!\n");
    }
    
    return ret;
}

int main(int argc, const char * argv[])
{
    if (argc < 3) {
        print_usage();
        exit(1);
    }
    
    int ret = -1;
    
    pid_t pid = atoi(argv[1]);
    
    if (pid <= 0 || pid > BSD_PID_MAX) {
        printf("Invalid pid\n");
        exit(1);
    }
    
    if (strcmp(argv[2], "--enable") == 0) {
        if (argc < 4) {
            print_usage();
            exit(1);
        }
        
        uint64_t flavor = 0;
        
        if (strcmp(argv[3], "full") == 0) {
            flavor = MEMORYSTATUS_ENABLE_MSL_MALLOC | MEMORYSTATUS_ENABLE_MSL_VM;
        } else if (strcmp(argv[3], "malloc") == 0) {
            flavor = MEMORYSTATUS_ENABLE_MSL_MALLOC;
        } else if (strcmp(argv[3], "vm") == 0) {
            flavor = MEMORYSTATUS_ENABLE_MSL_VM;
        } else if (strcmp(argv[3], "lite") == 0) {
            flavor = MEMORYSTATUS_ENABLE_MSL_LITE_FULL;
        } else if (strcmp(argv[3], "vmlite") == 0) {
            flavor = MEMORYSTATUS_ENABLE_MSL_LITE_VM;
        }
        
        if (flavor == 0) {
            print_usage();
            exit(1);
        }
        
        ret = send_msl_command(pid, flavor);
    } else if (strcmp(argv[2], "--disable") == 0) {
        ret = send_msl_command(pid, MEMORYSTATUS_DISABLE_MSL);
    } else {
        print_usage();
        exit(1);
    }
    
    if (ret != 0) {
        exit(1);
    } else {
        exit(0);
    }
}


