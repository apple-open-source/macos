/*
 * "$Id:$"
 *
 *   Simple tool to showing how to send the answer command to the efax deamon.
 *
 *   Copyright 2006 by Apple Computer Inc., all rights reserved.
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void usage();

int main(int argc, char * argv[])
{
  int	c,
	fd;
  char command[3] = "?\n";
  struct sockaddr_un saddr;

  while ((c = getopt(argc, argv, "ac?")) != -1)
  {
    switch (c)
    {
    case 'a':
    case 'c':
      command[0] = (char)c;
      break;
    }
  }

  if (command[0] == '?')
    usage();

  bzero(&saddr, sizeof(saddr));
  saddr.sun_family = AF_LOCAL;
  strlcpy(saddr.sun_path, "/var/run/efax", sizeof(saddr.sun_path));

  fd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (connect(fd, (struct sockaddr *)&saddr, SUN_LEN(&saddr)) < 0) {
    perror("connect");
    exit(1);
  }

  write(fd, command, strlen(command));
  close(fd);

  return 0;
}

static void usage()
{
  static char usage_msg[] = 
  "Usage: efaxtest [-a] [-c]\n" \
  "    -a     Send answer command to daemon\n" \
  "    -c     Send cancel command to daemon\n";
  
  fputs(usage_msg, stderr);
  exit(1);
}
