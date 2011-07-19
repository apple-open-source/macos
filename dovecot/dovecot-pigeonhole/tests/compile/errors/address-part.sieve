/*
 * Address part errors
 *
 * Total errors: 2 (+1 = 3)
 */

# Duplicate address part (1)
if address :all :comparator "i;octet" :domain "from" "STEPHAN" {

	# Duplicate address part (2)
	if address :domain :localpart :comparator "i;octet" "from" "friep.example.com" {
		keep;
	}

	stop;
}

