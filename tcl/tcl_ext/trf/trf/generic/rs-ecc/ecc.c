/*
    ecc Version 1.2  by Paul Flaherty (paulf@stanford.edu)
    Copyright (C) 1993 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


/* ecc.c
	Basic Software Tool for Encoding and Decoding Files.

	This is a simple stream encoder which uses the rslib routines to
	do something practical.  It reads data from stdin in 248(encode) or
	256(decode) blocks, and writes the corresponding encoded/decoded
	block onto stdout.  An encoded block contains 248 data bytes, one
	length byte, six redundancy bytes, and a capital G byte as a sync
	marker to round it out to 256 bytes.
*/

#include <stdio.h>
#include <sys/types.h>

main (argc, argv)

     int argc;
     char *argv[];

{

  int ec;

  if (argc != 2)
    {
      fprintf (stderr, "usage: ecc [-edv] \n");
      exit (1);
    }

  if (strcmp ("-e", argv[1]) == 0)
    encode ();
  if (strcmp ("-d", argv[1]) == 0)
    decode ();
  if (strcmp ("-v", argv[1]) == 0)
    ver ();

  fprintf (stderr, "usage: ecc [-edv] \n");
  exit (1);

}

ver ()

{
  extern char version[];

  fprintf (stderr, "%s\n", version);
  exit (0);

}

encode ()

{

  unsigned char msg[249], coded[255];
  int i, readme;

  for (i = 0; i < 249; i++)
    msg[i] = 0;

  readme = 248;

  while (readme == 248)
    {
      readme = fread (msg, 1, 248, stdin);
      msg[248] = readme;

      rsencode (msg, coded);

      fprintf (stdout, "G");
      for (i = 254; i > -1; i--)
	fprintf (stdout, "%c", coded[i]);
    }

  exit (0);

}



decode ()

{

  int i, j, readme, bo, len;
  unsigned char msgs[249], cod[255];

  j = 0;

  while (getc (stdin) == 71)
    {
      j++;
      for (i = 254; i > -1; i--)
	cod[i] = getc (stdin);

      rsdecode (cod, msgs, &bo);

      if (bo > 0 && bo < 4)
	fprintf (stderr, "ecc: %d byte error in block %d.\n", bo, j);

      if (bo == 4)
	fprintf (stderr, "ecc: unrecoverable error in block %d.\n", j);
      len = msgs[248];
      for (i = 0; i < len; i++)
	putc (msgs[i], stdout);
    }

  if (getc (stdin) != EOF)
    {
      fprintf (stderr, "ecc: sync error in block %d.\n", j);
      exit (1);
    }

  exit (0);

}
