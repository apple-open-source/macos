# $Id: count.awk,v 1.2 2004/03/30 01:21:27 jtownsen Exp $
#
# Print out the number of log records for transactions that we
# encountered.

/^\[/{
	if ($5 != 0)
		print $5
}
