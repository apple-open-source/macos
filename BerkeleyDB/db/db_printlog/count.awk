# $Id: count.awk,v 1.1.1.1 2003/02/15 04:55:41 zarzycki Exp $
#
# Print out the number of log records for transactions that we
# encountered.

/^\[/{
	if ($5 != 0)
		print $5
}
