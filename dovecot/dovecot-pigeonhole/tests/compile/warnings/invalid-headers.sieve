# Header test
if header "from:" "frop@example.org" {
	stop;
}

# Address test
if address "from:" "frop@example.org" {
	stop;
}

# Exists test
if exists "from:" {
	stop;
}
