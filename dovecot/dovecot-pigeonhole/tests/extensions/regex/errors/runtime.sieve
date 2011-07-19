require "regex";
require "variables";
require "fileinto";

set "regex" "[";

if header :regex "to" "${regex}" {
	fileinto "frop";
}
