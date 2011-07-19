require "comparator-i;ascii-numeric";

/*
 * Compile errors for the header test
 *
 * Total errors: 9 (+1 validation failed msg = 10)
 */

# Unknown tagged argument
if header :all :comparator "i;ascii-casemap" "From" "nico" {
	keep;
}

# Wrong first argument
if header :is :comparator "i;ascii-numeric" 45 "nico" {
	keep;
}

# Wrong second argument
if header :is :comparator "i;ascii-numeric" "From" 45 {
	discard;
}

# Wrong second argument
if header :is :comparator "i;ascii-numeric" "From" :tag {
	stop;
}

# Missing second argument
if header :is :comparator "i;ascii-numeric" "From" {
	stop;
}

# Missing arguments
if header :is :comparator "i;ascii-numeric" {
	keep;
}

# Not an error
if header :is :comparator "i;ascii-casemap" "frop" ["frop", "frop"] {
	discard;
}

# Spurious sub-test
if header "frop" "frop" true {
	discard;
}

# Test used as command with block
header "frop" "frop" {
    discard;
}

# Test used as command
header "frop" "frop";


