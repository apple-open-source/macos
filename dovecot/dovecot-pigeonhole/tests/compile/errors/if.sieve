/*
 * If command errors 
 *
 * Total errors: 11 (+1 = 12)
 */

# Spurious argument
if "frop" true {}

# Spurious argument
elsif "frop" true {}

# Spurious string list
if [ "false", "false", "false" ] false {
	stop;
}

# No block
if true;

# No test
if {
	keep;
}

# Spurious test list
if ( false, false, true ) {
	keep;
}

stop;

# If-less else
else {
	keep;
}

# Not an error
if true {
	keep;
}

stop;

# If-less if structure (should produce only one error)
elsif true {
	keep;
}
elsif true {
	keep;
}
else {
}

# Elsif after else
if true {
	keep;
} else {
	stop;
} elsif true {
	stop;
}

# If used as test
if if true {
} 

# Else if in stead of elsif

if true {
	stop;
} else if false {
	keep;
}




