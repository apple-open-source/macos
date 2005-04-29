# $Id: commit.awk,v 1.2 2004/03/30 01:21:27 jtownsen Exp $
#
# Output tid of committed transactions.

/txn_regop/ {
	print $5
}
