if true {
	if true {
		# Missing semicolon
		keep
	}
}

if true {
	# Erroneous syntax
	keep,
	keep
}

if true {
	if anyof(true,true,false) {
		keep;
	}
}

if true {
	if anyof(true,true,false) {
		keep;
		# Missing semicolon
		discard
	}
}

