/*
 * Tag errors
 * 
 * Total errors: 2 (+1 = 3)
 */

# Unknown tag (1)
if envelope :isnot :comparator "i;ascii-casemap" :localpart "From" "nico" {
	discard;
}

# Spurious tag (1)
if true :comparator "i;ascii-numeric" {
  	keep;
}

