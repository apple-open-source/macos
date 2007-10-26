#!/bin/sh
#
# uuspeed - a script to parse a Taylor UUCP Stats file into pretty results.
# Zacharias J. Beckman.
#
# Modified 20/12/96 by Mark Horsburgh (markh@kcbbs.gen.nz) to calculate
# correct average transmission rate and to give bytes/s rather than baud
# (since baud is not really the correct name for it and bits/s actually
# depends on the number of start and stop bits etc)

grep bytes /usr/spool/uucp/Stats | grep -v 'bytes 0.00 secs' | grep -v 'failed after' | tail -80 | \
gawk '
  BEGIN {
    printf("          UUCP transmission history:\n");
    format=" %8d bytes %8s(%8s) in %7.2f sec = %5.0f bytes/s, %5.1fK / min\n";
    bytes = 0.0
    seconds = 0.0
  }

  {
  if ($6 > 100) {
      printf (format, $6, $5, $2, $9, $6/$9, ($6/$9*60)/1024);

      bytes += $6
      seconds += $9
    }
  }

  END {
    printf ("          average speed %5.0f bytes/s, %4.1fK/min\n",
	bytes/seconds,bytes/seconds*60/1024);
  }
'
