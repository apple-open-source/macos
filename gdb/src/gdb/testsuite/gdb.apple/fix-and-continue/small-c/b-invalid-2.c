#ifndef VERSION
#define VERSION 1
#endif

int global_var = (VERSION * 10);
static int static_var = 5;
static int static_bss_int;
int bss_int;

int foo;   /* ERROR: This was a function in the original file */
