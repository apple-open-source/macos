if address :contains "to" "frop.example" {
	/* #1 */
	redirect "stephan@example.com";
	
	/* #2 */
	keep;
}

/* #3 */
redirect "stephan@example.org";

/* #4 */
redirect "nico@example.nl";

/* Duplicates */
redirect "Stephan Bosch <stephan@example.com>";
keep;
