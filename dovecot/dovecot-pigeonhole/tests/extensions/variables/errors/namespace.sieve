require "variables";
require "fileinto";

set "namespace.frop" "value";
set "complex.struct.frop" "value";

fileinto "${namespace.frop}";
fileinto "${complex.struct.frop}";
