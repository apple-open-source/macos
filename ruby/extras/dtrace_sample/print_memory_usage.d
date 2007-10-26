#!/usr/sbin/dtrace -s

ruby*:::gc-begin
{
  printf("Garbage collection...\n");
}

ruby*:::object-create-start
{
  printf("Creating object of type `%s'\n", copyinstr(arg0));
}

ruby*:::object-free
{
  printf("Freeing object of type `%s'\n", copyinstr(arg0));
}

ruby*:::gc-end
{
  printf("Garbage collection done.\n");
}
