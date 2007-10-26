#!/usr/sbin/dtrace -s
/* Prints the file, line, class and function of every function entry and function return. */

ruby*:::function-entry
{
  printf("-> %s:%d `%s#%s'\n", copyinstr(arg2), arg3, copyinstr(arg0), copyinstr(arg1));
}

ruby*:::function-return
{
  printf("<- %s:%d `%s#%s'\n", copyinstr(arg2), arg3, copyinstr(arg0), copyinstr(arg1));
}
