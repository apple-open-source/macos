int gdb_self_backtrace (void **buffer, int bufsize);

char **gdb_self_backtrace_symbols (void **addrbuf, int num_of_addrs);

void gdb_self_backtrace_symbols_fd (void **addrbuf, int num_of_addrs, int fd,
                           int skip, int maxdepth);
