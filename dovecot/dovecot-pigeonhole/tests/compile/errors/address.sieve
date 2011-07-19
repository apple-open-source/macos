require "comparator-i;ascii-numeric";

/* 
 * Address test errors
 *
 * Total count: 8 (+1 = 9)
 */

/*
 * Command structure
 */ 

# Invalid tag
if address :nonsense :comparator "i;ascii-casemap" :localpart "From" "nico" {
	discard;
}

# Invalid first argument
if address :is :comparator "i;ascii-numeric" :localpart 45 "nico" {
	discard;
}

# Invalid second argument
if address :is :comparator "i;ascii-numeric" :localpart "From" 45 {
	discard;
}

# Invalid second argument
if address :comparator "i;ascii-numeric" :localpart "From" :is {
	discard;
}

# Missing second argument
if address :is :comparator "i;ascii-numeric" :localpart "From" {
	discard;
}

# Missing arguments
if address :is :comparator "i;ascii-numeric" :localpart {
	discard;
}

# Not an error
if address :localpart :is :comparator "i;ascii-casemap" "from" ["frop", "frop"] {
	discard;
}

/*
 * Specified headers must contain addresses
 */

# Invalid header 
if address :is "frop" "frml" {
	keep;
}

# Not an error
if address :is "reply-to" "frml" {
	keep;
}

# Invalid header (#2)
if address :is ["to", "frop"] "frml" {
	keep;
}

# Not an error
if address :is ["to", "reply-to"] "frml" {
	keep;
}

