//
//  lib_newfs_msdos.c
//  newfs_msdos
//
//  Created by Kujan Lauz on 04/09/2022.
//

#include <stdio.h>
#include "lib_newfs_msdos.h"


lib_newfs_ctx_t newfs_ctx;

void newfs_set_context_properties(newfs_msdos_print_funct_t print,
                                  newfs_msdos_wipefs_func_t wipefs,
                                  newfs_client_ctx_t client)
{
    newfs_ctx.wipefs = wipefs;
    newfs_ctx.print = print;
    newfs_ctx.client_ctx = client;
}

void newfs_print(lib_newfs_ctx_t c, int level, const char *fmt, ...)
{
    if (c.print) {
        va_list ap;
        va_start(ap, fmt);
        c.print(c.client_ctx, level, fmt, ap);
        va_end(ap);
    }
}

newfs_msdos_wipefs_func_t newfs_get_wipefs_function_callback(void) {
    return newfs_ctx.wipefs;
}

void newfs_set_wipefs_function_callback(newfs_msdos_wipefs_func_t func) {
    newfs_ctx.wipefs = func;
}

newfs_msdos_print_funct_t newfs_get_print_function_callback(void) {
    return newfs_ctx.print;
}

void newfs_set_print_function_callback(newfs_msdos_print_funct_t func) {
    newfs_ctx.print = func;
}

newfs_client_ctx_t newfs_get_client(void) {
    return newfs_ctx.client_ctx;
}

void newfs_set_client (newfs_client_ctx_t c) {
    newfs_ctx.client_ctx = c;
}
