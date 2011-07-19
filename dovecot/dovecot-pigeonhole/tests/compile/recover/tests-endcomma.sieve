if true {
	if true {
		# Spurious comma
		if anyof(true,true,true,) {
		}
	}
}

if true {
	if anyof(true,true) {
		# Spurious comma
		if anyof(true,true,true,) {
			if anyof(true,true,true) {
			}
		}
	}
}
