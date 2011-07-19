/* 
 * Handling of unsupported language features.
 *
 *   Total errors: 3 (+1 = 4)
 */

require "variables";
require "include";
require "regex";

/* 
 * Unsupported use of variables
 */

/* Comparator argument */ 

set "comp" "i;ascii-numeric";

if address :comparator "${comp}" "from" "stephan@example.org" {
	stop;
}

/* Included script */

set "script" "blacklist";

include "${blacklist}";



