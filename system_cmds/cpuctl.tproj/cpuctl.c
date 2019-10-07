//
//  cpuctl.c
//  system_cmds
//
//  Copyright (c) 2019 Apple Inc. All rights reserved.
//

#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <mach/mach.h>

static void usage()
{
    printf("usage: cpuctl [ list ]\n");
    printf("       cpuctl { offline | online } <cpu> [ <cpu>... ]\n");
    exit(EX_USAGE);
}

static void fetch_cpu_info(host_t *priv_port,
                           processor_port_array_t proc_ports,
                           mach_msg_type_number_t proc_count,
                           processor_basic_info_data_t *cpus)
{
    for (int i = 0; i < proc_count; i++) {
        mach_msg_type_number_t info_count = PROCESSOR_BASIC_INFO_COUNT;

        if (processor_info(proc_ports[i], PROCESSOR_BASIC_INFO, priv_port,
                           (processor_info_t)&cpus[i], &info_count) != KERN_SUCCESS) {
            errx(EX_OSERR, "processor_info(%d) failed", i);
        }
    }
}

static int do_cmd_list(mach_msg_type_number_t proc_count, processor_basic_info_data_t *cpus)
{
    int prev_lowest = -1;
    for (int i = 0; i < proc_count; i++) {
        int lowest_slot = INT_MAX;
        int lowest_idx = -1;
        for (int j = 0; j < proc_count; j++) {
            int slot = cpus[j].slot_num;
            if (slot > prev_lowest && slot < lowest_slot) {
                lowest_slot = slot;
                lowest_idx = j;
            }
        }
        if (lowest_idx == -1)
            errx(EX_OSERR, "slot numbers are out of range");

        processor_basic_info_data_t *cpu = &cpus[lowest_idx];
        printf("CPU%d: %-7s type=%x,%x master=%d\n",
               cpu->slot_num,
               cpu->running ? "online" : "offline",
               cpu->cpu_type,
               cpu->cpu_subtype,
               cpu->is_master);

        prev_lowest = lowest_slot;
    }
    return 0;
}

static int find_cpu_by_slot(mach_msg_type_number_t proc_count,
                            processor_basic_info_data_t *cpus,
                            int slot)
{
    for (int i = 0; i < proc_count; i++) {
        if (cpus[i].slot_num == slot)
            return i;
    }
    return -1;
}

int main(int argc, char **argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                usage();
        }
    }

    host_t priv_port;
    if (host_get_host_priv_port(mach_host_self(), &priv_port) != KERN_SUCCESS)
        errx(EX_OSERR, "host_get_host_priv_port() failed");

    processor_port_array_t proc_ports;
    mach_msg_type_number_t proc_count;
    if (host_processors(priv_port, &proc_ports, &proc_count) != KERN_SUCCESS)
        errx(EX_OSERR, "host_processors() failed");

    processor_basic_info_data_t *cpus = calloc(proc_count, sizeof(*cpus));
    if (!cpus)
        errx(EX_OSERR, "calloc() failed");
    fetch_cpu_info(&priv_port, proc_ports, proc_count, cpus);

    if (optind == argc)
        return do_cmd_list(proc_count, cpus);

    const char *cmd = argv[optind];
    optind++;

    if (!strcmp(cmd, "list"))
        return do_cmd_list(proc_count, cpus);

    bool up = true;
    if (!strncmp(cmd, "off", 3))
        up = false;
    else if (strncmp(cmd, "on", 2))
        usage();

    if (optind == argc)
        usage();

    int ret = 0;
    for (; optind < argc; optind++) {
        char *endp = NULL;
        int slot = (int)strtoul(argv[optind], &endp, 0);
        if (*endp != 0)
            usage();

        int cpu = find_cpu_by_slot(proc_count, cpus, slot);
        if (cpu == -1)
            errx(EX_USAGE, "Invalid CPU ID %d", slot);

        if (up) {
            if (processor_start(proc_ports[cpu]) != KERN_SUCCESS)
                errx(EX_OSERR, "processor_start(%u) failed", cpu);
        } else {
            if (processor_exit(proc_ports[cpu]) != KERN_SUCCESS)
                errx(EX_OSERR, "processor_exit(%u) failed", cpu);
        }
    }

    return ret;
}
