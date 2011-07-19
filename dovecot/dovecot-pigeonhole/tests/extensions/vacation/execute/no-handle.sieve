require "vacation";
require "variables";

set "reason" "I have a conference in Seattle";

vacation 
	:subject "I am not in: ${reason}" 
	:from "stephan@example.org" 
	"I am gone for today: ${reason}.";

