# $Id: commit.awk,v 1.1.1.1 2003/02/15 04:55:41 zarzycki Exp $
#
# Output tid of committed transactions.

/txn_regop/ {
	print $5
}
