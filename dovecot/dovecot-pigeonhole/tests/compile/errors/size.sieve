/*
 * Size test errors
 * 
 * Total errors: 6 (+1 = 7)
 */

# Used as command (1)
size :under 23;

# Missing argument (2)
if size {
}

# Missing :over/:under (3)
if size 45 {
	discard;
}

# No error
if size :over 34K {
	stop;
}

# No error
if size :under 34M {
	stop;
}

# Conflicting tags (4)
if size :under :over 34 {
	keep;
}

# Duplicate tags (5)
if size :over :over 45M {
	stop;
}

# Wrong argument order (6)
if size 34M :over {
	stop;
}

# No error; but worthy of a warning
if size :under 0 {
	stop;
}
