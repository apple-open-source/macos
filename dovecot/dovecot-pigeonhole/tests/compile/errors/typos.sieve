/*
 * This test is primarily meant to check the compiler's handling of typos
 * at various locations.
 */

require "fileinto";

/* 
 * Missing semicolon 
 */

fileinto "frop"
keep;

/* Other situations */

fileinto "frup" 
true;

fileinto "friep" 
snot;

/*
 * Forgot tag colon
 */ 

if address matches "from" "*frop*" {
	stop;
}
